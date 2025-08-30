/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PiperTTSProvider implementation inspired by libpiper
 * Original libpiper by Michael Hansen: https://github.com/rhasspy/piper
 * Licensed under GPL-3.0, compatible with crow-translate's GPL-3.0+ license
 */

#include "piperttsprovider.h"

#include "language.h"
#include "playlistplayer.h"
#include "provideroptions.h"
#include "settings/appsettings.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMessageBox>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

// ONNX Runtime includes
#include <onnxruntime_cxx_api.h>

// espeak-ng includes
#include <espeak-ng/speak_lib.h>

// Clause terminator constants from libpiper
#define CLAUSE_INTONATION_FULL_STOP 0x00000000
#define CLAUSE_INTONATION_COMMA 0x00001000
#define CLAUSE_INTONATION_QUESTION 0x00002000
#define CLAUSE_INTONATION_EXCLAMATION 0x00003000

#define CLAUSE_TYPE_CLAUSE 0x00040000
#define CLAUSE_TYPE_SENTENCE 0x00080000

#define CLAUSE_PERIOD (40 | CLAUSE_INTONATION_FULL_STOP | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_COMMA (20 | CLAUSE_INTONATION_COMMA | CLAUSE_TYPE_CLAUSE)
#define CLAUSE_QUESTION (40 | CLAUSE_INTONATION_QUESTION | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_EXCLAMATION (45 | CLAUSE_INTONATION_EXCLAMATION | CLAUSE_TYPE_SENTENCE)
#define CLAUSE_COLON (30 | CLAUSE_INTONATION_FULL_STOP | CLAUSE_TYPE_CLAUSE)
#define CLAUSE_SEMICOLON (30 | CLAUSE_INTONATION_COMMA | CLAUSE_TYPE_CLAUSE)

// Constants from libpiper
const PhonemeId ID_PAD = 0;
const PhonemeId ID_BOS = 1;
const PhonemeId ID_EOS = 2;

const float DEFAULT_LENGTH_SCALE = 1.0f;
const float DEFAULT_NOISE_SCALE = 0.667f;
const float DEFAULT_NOISE_W_SCALE = 0.8f;

PiperTTSProvider::PiperTTSProvider(QObject *parent)
    : ATTSProvider(parent)
    , m_state(QTextToSpeech::Ready)
    , m_errorReason(QTextToSpeech::ErrorReason::NoError)
    , m_language(Language(QLocale::English))
    , m_rate(1.0)
    , m_pitch(1.0)
    , m_volume(1.0)
    , m_player(new PlaylistPlayer(this))
    , m_tempAudioFile(nullptr)
    , m_espeakVoice(QStringLiteral("en-us"))
    , m_sampleRate(22050)
    , m_numSpeakers(1)
    , m_lengthScale(DEFAULT_LENGTH_SCALE)
    , m_noiseScale(DEFAULT_NOISE_SCALE)
    , m_noiseWScale(DEFAULT_NOISE_W_SCALE)
    , m_currentSpeaker("")
    , m_currentSpeakerId(0)
    , m_initialized(false)
{
    // Set up PlaylistPlayer connections
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        switch (state) {
        case QMediaPlayer::PlayingState:
            setState(QTextToSpeech::Speaking);
            break;
        case QMediaPlayer::PausedState:
            setState(QTextToSpeech::Paused);
            break;
        case QMediaPlayer::StoppedState:
            setState(QTextToSpeech::Ready);
            break;
        }
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
        Q_UNUSED(error);
        setError(QTextToSpeech::ErrorReason::Playback, errorString);
    });

    // Set up audio output
    auto *audioOutput = new QAudioOutput(this);
    audioOutput->setVolume(m_volume);
    m_player->setAudioOutput(audioOutput);

    initializeAudio();

    // Try to initialize Piper with better error handling for Windows
    try {
        qDebug() << "PiperTTSProvider::constructor - attempting ONNX initialization";
        if (!initializePiper()) {
            qDebug() << "PiperTTSProvider::constructor - initializePiper() failed normally";
            setError(QTextToSpeech::ErrorReason::Initialization, tr("Failed to initialize Piper TTS"));
        } else {
            qDebug() << "PiperTTSProvider::constructor - initializePiper() succeeded, m_initialized:" << m_initialized;
        }
    } catch (const std::exception &e) {
        qDebug() << "PiperTTSProvider::constructor - caught std::exception:" << e.what();
        setError(QTextToSpeech::ErrorReason::Initialization, tr("ONNX Runtime exception: %1").arg(e.what()));
    } catch (...) {
        qDebug() << "PiperTTSProvider::constructor - caught unknown exception";
        setError(QTextToSpeech::ErrorReason::Initialization, tr("Unknown ONNX Runtime initialization error"));
    }

    // Initialize with default voice
    m_voice = Voice();
}

PiperTTSProvider::~PiperTTSProvider()
{
    cleanupPiper();
    cleanupAudio();
}

QString PiperTTSProvider::getProviderType() const
{
    return QStringLiteral("PiperTTSProvider");
}

bool PiperTTSProvider::initializePiper()
{
    try {
        // Handle Windows telemetry before ONNX Runtime initialization
#ifdef _WIN32
        handleOnnxTelemetryOnWindows();
#endif

        // Initialize ONNX Runtime environment
        m_ortEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "piper");

#ifdef _WIN32
        // Disable telemetry at runtime (in addition to environment variable)
        try {
            m_ortEnv->DisableTelemetryEvents();
        } catch (const std::exception &e) {
            qWarning() << "Failed to disable ONNX Runtime telemetry at runtime:" << e.what();
        }
#endif

        m_sessionOptions = std::make_unique<Ort::SessionOptions>();
        m_allocator = std::make_unique<Ort::AllocatorWithDefaultOptions>();

        // Set ONNX Runtime options
        m_sessionOptions->SetIntraOpNumThreads(1);
        m_sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        // Initialize espeak-ng with correct data path
        QByteArray dataPathBytes;
        const char *dataPath = nullptr;

        // Try application-bundled espeak-ng data first (for NixOS and other bundled builds)
        const QStringList candidatePaths = {
            QCoreApplication::applicationDirPath() + "/espeak-ng-data", // Application bundled location
            QCoreApplication::applicationDirPath() + "/../espeak-ng-data", // Build directory location (Windows)
            QCoreApplication::applicationDirPath() + "/../share/crow-translate/espeak-ng-data", // Installed location
            "/usr/share/crow-translate/espeak-ng-data", // System installed location
            "/usr/local/share/crow-translate/espeak-ng-data", // Local system installed location
        };

        qDebug() << "PiperTTSProvider: Searching for espeak-ng data in paths:";
        QString validDataPath;
        for (const QString &path : candidatePaths) {
            qDebug() << "  - Checking:" << path;
            qDebug() << "    Directory exists:" << QDir(path).exists();
            qDebug() << "    phontab exists:" << QFile::exists(path + "/phontab");

            if (QDir(path).exists() && QFile::exists(path + "/phontab")) {
                validDataPath = path;
                qDebug() << "  ✅ Found valid espeak-ng data at:" << path;
                break;
            }
        }

        if (!validDataPath.isEmpty()) {
            qDebug() << "PiperTTSProvider: Using espeak-ng data from:" << validDataPath;
            dataPathBytes = validDataPath.toLocal8Bit();
            dataPath = dataPathBytes.constData();
        } else {
            qDebug() << "PiperTTSProvider: No bundled espeak-ng data found, using system data (nullptr)";
        }

        if (espeak_Initialize(AUDIO_OUTPUT_SYNCH_PLAYBACK, 0, dataPath, 0) < 1) {
            setError(QTextToSpeech::ErrorReason::Initialization, tr("Failed to initialize espeak-ng"));
            return false;
        }

        // Set default voice
        const QByteArray voiceBytes = m_espeakVoice.toLocal8Bit();
        espeak_SetVoiceByName(voiceBytes.constData());

        // Discover all available models dynamically
        const QStringList modelPaths = discoverAvailableModels();
        qDebug() << "PiperTTSProvider::initializePiper - discovered models:" << modelPaths.size();

        // Show warning dialog if no models found
        if (modelPaths.isEmpty()) {
            showPiperVoicesWarning();
        }

        bool modelLoaded = false;
        for (const QString &modelPath : modelPaths) {
            const QString configPath = modelPath + ".json";
            qDebug() << "  - trying model:" << modelPath;
            qDebug() << "  - config exists:" << QFile::exists(configPath);
            if (QFile::exists(modelPath) && QFile::exists(configPath)) {
                if (loadModel(modelPath, configPath)) {
                    qDebug() << "  - model loaded successfully:" << modelPath;
                    modelLoaded = true;
                    break;
                } else {
                    qDebug() << "  - model load failed:" << modelPath;
                }
            }
        }

        if (!modelLoaded) {
            qDebug() << "No Piper models found, using basic synthesis";
            // Continue without model for basic functionality
        }

        m_initialized = true;
        return true;

    } catch (const std::exception &e) {
        setError(QTextToSpeech::ErrorReason::Initialization, tr("ONNX Runtime initialization error: %1").arg(e.what()));
        return false;
    }
}

void PiperTTSProvider::cleanupPiper()
{
    if (m_initialized) {
        espeak_Terminate();
        m_initialized = false;
    }

    m_ortSession.reset();
    m_sessionOptions.reset();
    m_allocator.reset();
    m_ortEnv.reset();
}

#ifdef _WIN32
void PiperTTSProvider::handleOnnxTelemetryOnWindows()
{
    // Set environment variable to disable telemetry before ONNX Runtime initialization
    qputenv("ORT_DISABLE_TELEMETRY", "1");

#ifdef WITH_ONNX_RUNTIME_DYNAMIC
    // Check if user has been notified about telemetry (only for dynamic linking)
    AppSettings settings;
    const bool hasBeenNotified = settings.isPiperTelemetryNotificationShown();

    if (!hasBeenNotified) {
        // Show privacy notice about ONNX Runtime telemetry
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("Privacy Notice - Neural TTS"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("ONNX Runtime Telemetry Information"));
        msgBox.setInformativeText(
            tr("Crow Translate uses ONNX Runtime for neural text-to-speech. "
               "The Windows binary includes Microsoft telemetry that collects usage information.\n\n"
               "Data Collection:\n"
               "• Device and usage data, error reports, performance information\n"
               "• Used by Microsoft to improve product quality and features\n"
               "• Subject to Microsoft Privacy Statement\n\n"
               "Crow Translate Actions:\n"
               "• Automatically disables telemetry on startup\n"
               "• Uses both environment variables and runtime API calls\n"
               "• Note: Some data may be transmitted during ONNX initialization\n\n"
               "On Unix systems, telemetry is inactive by default.\n"
               "This notice is shown once per installation."));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();

        // Mark as notified so we don't show this again
        settings.setPiperTelemetryNotificationShown(true);
    }
#endif
}
#endif

bool PiperTTSProvider::loadModel(const QString &modelPath, const QString &configPath)
{
    try {
        // Load config JSON
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open config file:" << configPath;
            return false;
        }

        const QJsonDocument configDoc = QJsonDocument::fromJson(configFile.readAll());
        QJsonObject config = configDoc.object();

        // Parse config
        if (config.contains("audio")) {
            QJsonObject audio = config["audio"].toObject();
            m_sampleRate = audio["sample_rate"].toInt(22050);
        }

        if (config.contains("espeak")) {
            QJsonObject espeak = config["espeak"].toObject();
            m_espeakVoice = espeak["voice"].toString(QStringLiteral("en-us"));
            const QByteArray voiceBytes = m_espeakVoice.toLocal8Bit();
            espeak_SetVoiceByName(voiceBytes.constData());
        }

        if (config.contains("num_speakers")) {
            m_numSpeakers = config["num_speakers"].toInt(1);
        }

        // Load speaker ID mapping
        if (config.contains("speaker_id_map")) {
            QJsonObject speakerMap = config["speaker_id_map"].toObject();
            m_speakerIdMap.clear();

            for (auto it = speakerMap.begin(); it != speakerMap.end(); ++it) {
                const QString speakerName = it.key();
                const int speakerId = it.value().toInt();
                m_speakerIdMap[speakerName] = speakerId;
            }

            // Set first speaker as default if available
            if (!m_speakerIdMap.isEmpty()) {
                m_currentSpeaker = m_speakerIdMap.keys().first();
                m_currentSpeakerId = m_speakerIdMap.value(m_currentSpeaker);
            }
        } else {
            // Single speaker model
            m_speakerIdMap.clear();
            m_currentSpeaker = "default";
            m_currentSpeakerId = 0;
        }

        // Load phoneme ID mapping
        if (config.contains("phoneme_id_map")) {
            QJsonObject phonemeMap = config["phoneme_id_map"].toObject();
            m_phonemeIdMap.clear();

            qDebug() << "PiperTTSProvider::loadModel - loading phoneme ID map with" << phonemeMap.size() << "entries";

            // Debug: Print first few entries from JSON to see the actual structure
            auto keys = phonemeMap.keys();
            for (int i = 0; i < std::min(5, (int)keys.size()); ++i) {
                const QString key = keys[i];
                const QJsonValue value = phonemeMap[key];
                qDebug() << "  - JSON sample [" << i << "]:" << key << "=" << value;
            }
            for (auto it = phonemeMap.begin(); it != phonemeMap.end(); ++it) {
                const QString phonemeStr = it.key();
                if (!phonemeStr.isEmpty()) {
                    const char32_t phoneme = phonemeStr.toUcs4().at(0);
                    const QJsonArray idArray = it.value().toArray();
                    QList<PhonemeId> ids;
                    for (const QJsonValue &val : idArray) {
                        ids.append(val.toInt());
                    }
                    m_phonemeIdMap[phoneme] = ids;
                    QString idsStr;
                    for (int i = 0; i < qMin(ids.size(), 3); ++i) {
                        if (i > 0)
                            idsStr += ",";
                        idsStr += QString::number(ids[i]);
                    }
                    if (ids.size() > 3)
                        idsStr += "...";
                    qDebug() << "  - phoneme:" << phonemeStr << "-> IDs [" << ids.size() << "]:" << idsStr;
                }
            }
        }

        // Load ONNX model
        const QFile modelFile(modelPath);
        if (!modelFile.exists()) {
            qDebug() << "Model file does not exist:" << modelPath;
            return false;
        }

        const QByteArray modelPathBytes = modelPath.toLocal8Bit();

#ifdef _WIN32
        const std::wstring modelPathWStr = modelPath.toStdWString();
        m_ortSession = std::make_unique<Ort::Session>(*m_ortEnv, modelPathWStr.c_str(), *m_sessionOptions);
#else
        m_ortSession = std::make_unique<Ort::Session>(*m_ortEnv, modelPathBytes.constData(), *m_sessionOptions);
#endif

        // Debug: Print all input names
        const size_t numInputs = m_ortSession->GetInputCount();
        qDebug() << "Model has" << numInputs << "inputs:";
        for (size_t i = 0; i < numInputs; ++i) {
            auto inputName = m_ortSession->GetInputNameAllocated(i, *m_allocator);
            qDebug() << "  Input" << i << ":" << inputName.get();
        }

        qDebug() << "Successfully loaded Piper model:" << modelPath;
        return true;

    } catch (const std::exception &e) {
        qDebug() << "Error loading model:" << e.what();
        return false;
    }
}

QList<PhonemeId> PiperTTSProvider::textToPhonemeIds(const QString &text)
{
    QList<PhonemeId> phonemeIds;

    if (!m_initialized) {
        qDebug() << "PiperTTSProvider::textToPhonemeIds - not initialized, returning empty vector";
        return phonemeIds;
    }

    // Accumulate phonemes per sentence like libpiper does
    QStringList sentence_phonemes{QString()};
    int current_idx = 0;
    const QByteArray textBytes = text.toUtf8();
    const void *text_ptr = textBytes.constData();

    while (text_ptr != nullptr) {
        int terminator = 0;
        QString terminator_str;

        const char *phonemes = espeak_TextToPhonemesWithTerminator(&text_ptr, espeakCHARS_UTF8, espeakPHONEMES_IPA, &terminator);

        if (phonemes != nullptr) {
            sentence_phonemes[current_idx] += QString::fromUtf8(phonemes);
        }

        // Categorize terminator (based on libpiper constants)
        terminator &= 0x000FFFFF;

        if (terminator == CLAUSE_PERIOD) {
            terminator_str = QStringLiteral(".");
        } else if (terminator == CLAUSE_QUESTION) {
            terminator_str = QStringLiteral("?");
        } else if (terminator == CLAUSE_EXCLAMATION) {
            terminator_str = QStringLiteral("!");
        } else if (terminator == CLAUSE_COMMA) {
            terminator_str = QStringLiteral(", ");
        } else if (terminator == CLAUSE_COLON) {
            terminator_str = QStringLiteral(": ");
        } else if (terminator == CLAUSE_SEMICOLON) {
            terminator_str = QStringLiteral("; ");
        }

        sentence_phonemes[current_idx] += terminator_str;

        // Check if this is a sentence terminator (like libpiper)
        if ((terminator & CLAUSE_TYPE_SENTENCE) == CLAUSE_TYPE_SENTENCE) {
            sentence_phonemes.append(QString());
            current_idx = sentence_phonemes.size() - 1;
        }
    }

    // Convert phonemes to ids for each sentence (like libpiper)
    for (const QString &phonemes_str : sentence_phonemes) {
        if (phonemes_str.isEmpty()) {
            continue;
        }

        // Add BOS and PAD for each sentence
        if (!phonemeIds.isEmpty()) {
            // Add extra pause between sentences
            for (int i = 0; i < 8; ++i) {
                phonemeIds.append(ID_PAD);
            }
        }

        phonemeIds.append(ID_BOS);
        phonemeIds.append(ID_PAD);

        qDebug() << "PiperTTSProvider::textToPhonemeIds - processing sentence phonemes:" << phonemes_str;

        // Convert to UTF-32 for proper Unicode handling
        const QVector<uint> utf32 = phonemes_str.toUcs4();

        // Filter out (lang) switch flags like libpiper
        bool inLangFlag = false;
        for (const uint codepoint : utf32) {
            char32_t phoneme = static_cast<char32_t>(codepoint);

            if (inLangFlag) {
                if (phoneme == U')') {
                    inLangFlag = false;
                }
            } else if (phoneme == U'(') {
                inLangFlag = true;
            } else {
                auto it = m_phonemeIdMap.find(phoneme);
                if (it != m_phonemeIdMap.end()) {
                    for (const PhonemeId id : it.value()) {
                        phonemeIds.append(id);
                        phonemeIds.append(ID_PAD);
                    }
                } else {
                    qDebug() << "PiperTTSProvider::textToPhonemeIds - unknown phoneme: U+" << QString::number(phoneme, 16);
                }
            }
        }

        phonemeIds.append(ID_EOS);
    }

    // Debug: Show the complete phoneme ID sequence
    qDebug() << "PiperTTSProvider::textToPhonemeIds - final phoneme ID sequence:";
    QString idSequence;
    for (int i = 0; i < qMin(phonemeIds.size(), 20); ++i) {
        if (i > 0)
            idSequence += ",";
        idSequence += QString::number(phonemeIds[i]);
    }
    if (phonemeIds.size() > 20)
        idSequence += "...";
    qDebug() << "  - IDs:" << idSequence;

    return phonemeIds;
}

QList<float> PiperTTSProvider::synthesizeAudio(const QList<PhonemeId> &phonemeIds)
{
    QList<float> audioSamples;

    if (!m_ortSession) {
        emit errorOccurred(
            QTextToSpeech::ErrorReason::Configuration,
            QStringLiteral("PiperTTS: ONNX Runtime session not initialized. Please check if voice models are installed in the configured path."));
        return audioSamples;
    }

    if (phonemeIds.empty()) {
        emit errorOccurred(QTextToSpeech::ErrorReason::Input,
                           QStringLiteral("PiperTTS: No phonemes generated from text. Please check if espeak-ng is available."));
        return audioSamples;
    }

    try {
        qDebug() << "PiperTTSProvider::synthesizeAudio - using ONNX model";
        qDebug() << "  - phonemeIds.size():" << phonemeIds.size();

        // Prepare ONNX Runtime inputs - convert QList to std::vector for ONNX API
        std::vector<int64_t> phonemeIdsVec(phonemeIds.begin(), phonemeIds.end());
        std::vector<int64_t> inputShape = {1, static_cast<int64_t>(phonemeIds.size())};

        const Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor =
            Ort::Value::CreateTensor<int64_t>(memoryInfo, phonemeIdsVec.data(), phonemeIdsVec.size(), inputShape.data(), inputShape.size());
        qDebug() << "  - created input tensor";

        // Prepare input lengths
        std::vector<int64_t> inputLengths = {static_cast<int64_t>(phonemeIds.size())};
        std::vector<int64_t> lengthsShape = {1};
        Ort::Value lengthsTensor =
            Ort::Value::CreateTensor<int64_t>(memoryInfo, inputLengths.data(), inputLengths.size(), lengthsShape.data(), lengthsShape.size());

        // Prepare scales (noise_scale, length_scale, noise_w_scale)
        std::vector<float> scales = {m_noiseScale, static_cast<float>(m_lengthScale / m_rate), m_noiseWScale};
        std::vector<int64_t> scalesShape = {static_cast<int64_t>(scales.size())};
        Ort::Value scalesTensor = Ort::Value::CreateTensor<float>(memoryInfo, scales.data(), scales.size(), scalesShape.data(), scalesShape.size());

        // Prepare input names and tensors
        std::vector<const char *> inputNames = {"input", "input_lengths", "scales"};
        std::vector<const char *> outputNames = {"output"};
        std::vector<Ort::Value> inputs;
        inputs.push_back(std::move(inputTensor));
        inputs.push_back(std::move(lengthsTensor));
        inputs.push_back(std::move(scalesTensor));

        // Add speaker ID input for multi-speaker models
        if (m_numSpeakers > 1) {
            std::vector<int64_t> speakerShape = {1};
            std::vector<int64_t> speakerData = {static_cast<int64_t>(m_currentSpeakerId)};

            Ort::Value speakerTensor =
                Ort::Value::CreateTensor<int64_t>(memoryInfo, speakerData.data(), speakerData.size(), speakerShape.data(), speakerShape.size());

            inputNames.push_back("sid");
            inputs.push_back(std::move(speakerTensor));
        }

        qDebug() << "  - about to run ONNX session with" << inputs.size() << "inputs";

        auto outputs = m_ortSession->Run(Ort::RunOptions{nullptr}, inputNames.data(), inputs.data(), inputs.size(), outputNames.data(), outputNames.size());

        qDebug() << "  - ONNX session completed, outputs:" << outputs.size();

        // Extract audio data
        if (!outputs.empty()) {
            float *outputData = outputs[0].GetTensorMutableData<float>();
            auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();

            qDebug() << "  - output tensor shape size:" << shape.size();
            for (int i = 0; i < static_cast<int>(shape.size()); ++i) {
                qDebug() << "    - shape[" << i << "]:" << shape[i];
            }

            if (shape.size() >= 2) {
                // For 4D tensors [batch, 1, 1, samples], take the last dimension
                // For 2D tensors [batch, samples], take the second dimension
                const int64_t numSamples = shape[shape.size() - 1];
                audioSamples.reserve(numSamples);
                for (int64_t i = 0; i < numSamples; ++i) {
                    audioSamples.append(outputData[i]);
                }
                qDebug() << "  - extracted" << numSamples << "audio samples";
            }
        }

    } catch (const std::exception &e) {
        emit errorOccurred(QTextToSpeech::ErrorReason::Initialization,
                           QStringLiteral("PiperTTS: ONNX Runtime error: %1").arg(QString::fromStdString(e.what())));
        return audioSamples;
    }

    return audioSamples;
}

void PiperTTSProvider::say(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    setState(QTextToSpeech::Speaking);

    // Run synthesis in a separate thread to avoid blocking
    QThread::create([this, text]() {
        qDebug() << "PiperTTSProvider::say - starting synthesis for text:" << text;

        // Convert text to phoneme IDs
        const QList<PhonemeId> phonemeIds = textToPhonemeIds(text);
        qDebug() << "PiperTTSProvider::say - phoneme IDs count:" << phonemeIds.size();

        // Synthesize audio
        QList<float> audioSamples = synthesizeAudio(phonemeIds);
        qDebug() << "PiperTTSProvider::say - audio samples count:" << audioSamples.size();

        // Convert to QByteArray
        QByteArray audioData;
        audioData.resize(audioSamples.size() * sizeof(float));
        if (!audioSamples.isEmpty()) {
            std::memcpy(audioData.data(), audioSamples.constData(), audioData.size());
        }
        qDebug() << "PiperTTSProvider::say - audio data size:" << audioData.size();

        // Store audio data
        m_audioData = audioData;

        // Signal completion
        QMetaObject::invokeMethod(this, "onSynthesisFinished", Qt::QueuedConnection);
    })->start();
}

void PiperTTSProvider::stop()
{
    m_player->stop();
    m_player->clearPlaylist();
    setState(QTextToSpeech::Ready);
}

void PiperTTSProvider::pause()
{
    if (m_state == QTextToSpeech::Speaking) {
        m_player->pause();
    }
}

void PiperTTSProvider::resume()
{
    if (m_state == QTextToSpeech::Paused) {
        m_player->play();
    }
}

QTextToSpeech::State PiperTTSProvider::state() const
{
    const QMutexLocker locker(&m_stateMutex);
    return m_state;
}

QTextToSpeech::ErrorReason PiperTTSProvider::errorReason() const
{
    return m_errorReason;
}

QString PiperTTSProvider::errorString() const
{
    return m_errorString;
}

Language PiperTTSProvider::language() const
{
    return m_language;
}

void PiperTTSProvider::setLanguage(const Language &language)
{
    m_language = language;

    // Map language to espeak voice - comprehensive mapping for all supported languages
    QLocale locale = language.toQLocale();
    const QString languageCode = locale.name().toLower();

    // Map QLocale to espeak voice names
    static const QHash<QString, QString> localeToEspeak = {// English variants
                                                           {"en_us", "en-us"},
                                                           {"en_gb", "en-gb"},
                                                           {"en_au", "en-au"},
                                                           {"en_ca", "en-ca"},
                                                           {"en_in", "en-in"},
                                                           {"en_ie", "en-ie"},
                                                           {"en_za", "en-za"},
                                                           {"en_nz", "en-nz"},

                                                           // Romance languages
                                                           {"es_es", "es"},
                                                           {"es_mx", "es-mx"},
                                                           {"es_ar", "es-ar"},
                                                           {"es_co", "es-co"},
                                                           {"fr_fr", "fr"},
                                                           {"fr_ca", "fr-ca"},
                                                           {"fr_be", "fr-be"},
                                                           {"fr_ch", "fr-ch"},
                                                           {"it_it", "it"},
                                                           {"pt_br", "pt-br"},
                                                           {"pt_pt", "pt"},
                                                           {"ca_es", "ca"},
                                                           {"ro_ro", "ro"},

                                                           // Germanic languages
                                                           {"de_de", "de"},
                                                           {"de_at", "de-at"},
                                                           {"de_ch", "de-ch"},
                                                           {"nl_nl", "nl"},
                                                           {"nl_be", "nl-be"},
                                                           {"da_dk", "da"},
                                                           {"sv_se", "sv"},
                                                           {"nb_no", "no"},
                                                           {"nn_no", "no"},
                                                           {"is_is", "is"},
                                                           {"af_za", "af"},

                                                           // Slavic languages
                                                           {"ru_ru", "ru"},
                                                           {"pl_pl", "pl"},
                                                           {"cs_cz", "cs"},
                                                           {"sk_sk", "sk"},
                                                           {"uk_ua", "uk"},
                                                           {"bg_bg", "bg"},
                                                           {"hr_hr", "hr"},
                                                           {"sr_rs", "sr"},
                                                           {"sl_si", "sl"},
                                                           {"mk_mk", "mk"},
                                                           {"bs_ba", "bs"},

                                                           // Other European
                                                           {"fi_fi", "fi"},
                                                           {"et_ee", "et"},
                                                           {"lv_lv", "lv"},
                                                           {"lt_lt", "lt"},
                                                           {"hu_hu", "hu"},
                                                           {"el_gr", "el"},
                                                           {"tr_tr", "tr"},
                                                           {"mt_mt", "mt"},
                                                           {"ga_ie", "ga"},
                                                           {"cy_gb", "cy"},
                                                           {"eu_es", "eu"},
                                                           {"gl_es", "gl"},

                                                           // Asian languages
                                                           {"zh_cn", "zh"},
                                                           {"zh_tw", "zh-tw"},
                                                           {"zh_hk", "zh-hk"},
                                                           {"ja_jp", "ja"},
                                                           {"ko_kr", "ko"},
                                                           {"hi_in", "hi"},
                                                           {"bn_in", "bn"},
                                                           {"ta_in", "ta"},
                                                           {"te_in", "te"},
                                                           {"mr_in", "mr"},
                                                           {"gu_in", "gu"},
                                                           {"kn_in", "kn"},
                                                           {"ml_in", "ml"},
                                                           {"or_in", "or"},
                                                           {"pa_in", "pa"},
                                                           {"as_in", "as"},
                                                           {"ne_np", "ne"},
                                                           {"si_lk", "si"},
                                                           {"my_mm", "my"},
                                                           {"km_kh", "km"},
                                                           {"lo_la", "lo"},
                                                           {"th_th", "th"},
                                                           {"vi_vn", "vi"},
                                                           {"id_id", "id"},
                                                           {"ms_my", "ms"},
                                                           {"tl_ph", "tl"},

                                                           // Middle Eastern & African
                                                           {"ar_sa", "ar"},
                                                           {"he_il", "he"},
                                                           {"fa_ir", "fa"},
                                                           {"ur_pk", "ur"},
                                                           {"sw_ke", "sw"},
                                                           {"am_et", "am"},
                                                           {"ha_ng", "ha"},
                                                           {"yo_ng", "yo"},
                                                           {"ig_ng", "ig"},
                                                           {"zu_za", "zu"},
                                                           {"xh_za", "xh"},
                                                           {"st_za", "st"},

                                                           // Constructed/other
                                                           {"eo", "eo"},
                                                           {"ia", "ia"}};

    // Try exact match first
    if (localeToEspeak.contains(languageCode)) {
        m_espeakVoice = localeToEspeak[languageCode];
    } else {
        // Try language-only match (e.g., "en" for "en_XX")
        const QString languageOnly = languageCode.split('_').first();
        if (localeToEspeak.contains(languageOnly)) {
            m_espeakVoice = localeToEspeak[languageOnly];
        } else {
            // Default fallback
            m_espeakVoice = QStringLiteral("en-us");
        }
    }

    if (m_initialized) {
        const QByteArray voiceBytes = m_espeakVoice.toLocal8Bit();
        espeak_SetVoiceByName(voiceBytes.constData());
    }
}

Voice PiperTTSProvider::voice() const
{
    return m_voice;
}

void PiperTTSProvider::setVoice(const Voice &voice)
{
    qDebug() << "PiperTTSProvider::setVoice - voice name:" << voice.name();
    qDebug() << "PiperTTSProvider::setVoice - model path:" << voice.modelPath();

    m_voice = voice;

    // Load the model for the selected voice
    const QString modelPath = voice.modelPath();
    if (!modelPath.isEmpty()) {
        const QString configPath = modelPath + ".json";
        if (QFile::exists(modelPath) && QFile::exists(configPath)) {
            qDebug() << "PiperTTSProvider::setVoice - loading model:" << modelPath;
            loadModel(modelPath, configPath);
        } else {
            qDebug() << "PiperTTSProvider::setVoice - model files not found:" << modelPath << configPath;
        }
    } else {
        qDebug() << "PiperTTSProvider::setVoice - empty model path";
    }
}

QList<Voice> PiperTTSProvider::availableVoices() const
{
    QList<Voice> voices;

    // Get all available models
    const QStringList modelPaths = const_cast<PiperTTSProvider *>(this)->discoverAvailableModels();

    for (const QString &modelPath : modelPaths) {
        const Voice voice = createVoiceFromModelPath(modelPath);
        if (!voice.name().isEmpty()) {
            voices.append(voice);
        }
    }

    return voices;
}

QList<Voice> PiperTTSProvider::findVoices(const Language &language) const
{
    const QList<Voice> allVoices = availableVoices();
    QList<Voice> matchingVoices;
    QLocale locale = language.toQLocale();

    for (const Voice &voice : allVoices) {
        // Match by language, with or without country
        QLocale voiceLocale = voice.language().toQLocale();
        if (voiceLocale.language() == locale.language()) {
            matchingVoices.append(voice);
        }
    }

    return matchingVoices;
}

double PiperTTSProvider::rate() const
{
    return m_rate;
}

void PiperTTSProvider::setRate(double rate)
{
    m_rate = qBound(0.1, rate, 3.0);
}

double PiperTTSProvider::pitch() const
{
    return m_pitch;
}

void PiperTTSProvider::setPitch(double pitch)
{
    m_pitch = qBound(0.1, pitch, 3.0);
}

double PiperTTSProvider::volume() const
{
    return m_volume;
}

void PiperTTSProvider::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
}

QList<Language> PiperTTSProvider::availableLanguages() const
{
    QList<Language> languages;
    const QList<Voice> voices = availableVoices();

    for (const Voice &voice : voices) {
        const Language language = voice.language();
        if (!languages.contains(language)) {
            languages.append(language);
        }
    }

    return languages;
}

void PiperTTSProvider::applyOptions(const ProviderOptions &options)
{
    if (options.hasOption("reinitializeModels") && options.getOption("reinitializeModels").toBool()) {
        reinitializeModels();
    }

    if (options.hasOption("speaker")) {
        const QString speaker = options.getOption("speaker").toString();
        setSpeaker(speaker);
    }
}

std::unique_ptr<ProviderOptions> PiperTTSProvider::getDefaultOptions() const
{
    auto options = std::make_unique<ProviderOptions>();
    options->setOption("speaker", m_currentSpeaker);
    return options;
}

QStringList PiperTTSProvider::getAvailableOptions() const
{
    return {"speaker"};
}

ProviderUIRequirements PiperTTSProvider::getUIRequirements() const
{
    ProviderUIRequirements requirements;
    requirements.supportedCapabilities = {"voiceSelection", "speakerSelection"};
    requirements.requiredUIElements = {"sourceVoiceComboBox", "translationVoiceComboBox", "sourceSpeakerComboBox", "translationSpeakerComboBox"};
    return requirements;
}

void PiperTTSProvider::onSynthesisFinished()
{
    if (m_audioData.isEmpty()) {
        setError(QTextToSpeech::ErrorReason::Initialization, tr("No audio data generated"));
        return;
    }

    // Clean up previous temporary file
    if (m_tempAudioFile != nullptr) {
        m_tempAudioFile->remove();
        delete m_tempAudioFile;
    }

    // Create temporary file for audio data
    m_tempAudioFile = new QTemporaryFile(this);
    m_tempAudioFile->setFileTemplate(QDir::tempPath() + "/piper_audio_XXXXXX.wav");

    if (!m_tempAudioFile->open()) {
        setError(QTextToSpeech::ErrorReason::Initialization, tr("Failed to create temporary audio file"));
        return;
    }

    // Write WAV header for the audio data
    writeWavHeader(m_tempAudioFile, m_audioData.size() / sizeof(float), m_sampleRate, 1, 32);

    // Write audio data
    if (m_tempAudioFile->write(m_audioData) != m_audioData.size()) {
        setError(QTextToSpeech::ErrorReason::Initialization, tr("Failed to write audio data"));
        return;
    }

    m_tempAudioFile->close();

    // Play the temporary file using PlaylistPlayer
    m_player->clearPlaylist();
    m_player->addMedia(QUrl::fromLocalFile(m_tempAudioFile->fileName()));
    m_player->playPlaylist();
}

void PiperTTSProvider::setState(QTextToSpeech::State newState)
{
    QTextToSpeech::State stateToEmit;
    bool shouldEmit = false;

    {
        const QMutexLocker locker(&m_stateMutex);
        if (m_state != newState) {
            m_state = newState;
            stateToEmit = m_state;
            shouldEmit = true;
        }
    }

    // Emit signal outside of mutex lock to avoid reentrancy deadlock
    if (shouldEmit) {
        emit stateChanged(stateToEmit);
    }
}

void PiperTTSProvider::setError(QTextToSpeech::ErrorReason reason, const QString &errorString)
{
    m_errorReason = reason;
    m_errorString = errorString;
    setState(QTextToSpeech::Error);
    emit errorOccurred(reason, errorString);
}

void PiperTTSProvider::initializeAudio()
{
    // Audio initialization is now handled by PlaylistPlayer
}

void PiperTTSProvider::cleanupAudio()
{
    if (m_tempAudioFile != nullptr) {
        m_tempAudioFile->remove();
        delete m_tempAudioFile;
        m_tempAudioFile = nullptr;
    }
}

QStringList PiperTTSProvider::getStandardPiperVoicesPaths()
{
    QStringList paths;

    const QByteArray customPath = AppSettings().piperVoicesPath();
    if (!customPath.isEmpty()) {
        paths << QString::fromLocal8Bit(customPath);
    }

    paths << "/usr/share/piper-voices/"
          << "/usr/local/share/piper-voices/" << QDir::homePath() + "/.local/share/piper-voices/" << QDir::currentPath() + "/piper-voices/";

    return paths;
}

void PiperTTSProvider::writeWavHeader(QIODevice *device, int numSamples, int sampleRate, int numChannels, int bitsPerSample)
{
    // WAV file header structure
    const int bytesPerSample = bitsPerSample / 8;
    const int dataSize = numSamples * numChannels * bytesPerSample;
    const int fileSize = 44 + dataSize - 8; // Total file size minus 8 bytes for RIFF header

    // Write RIFF header
    device->write("RIFF", 4);
    device->write(reinterpret_cast<const char *>(&fileSize), 4);
    device->write("WAVE", 4);

    // Write format chunk
    device->write("fmt ", 4);
    const int formatChunkSize = 16;
    device->write(reinterpret_cast<const char *>(&formatChunkSize), 4);

    const short audioFormat = 3; // IEEE float format
    device->write(reinterpret_cast<const char *>(&audioFormat), 2);
    device->write(reinterpret_cast<const char *>(&numChannels), 2);
    device->write(reinterpret_cast<const char *>(&sampleRate), 4);

    const int byteRate = sampleRate * numChannels * bytesPerSample;
    device->write(reinterpret_cast<const char *>(&byteRate), 4);

    const short blockAlign = numChannels * bytesPerSample;
    device->write(reinterpret_cast<const char *>(&blockAlign), 2);
    device->write(reinterpret_cast<const char *>(&bitsPerSample), 2);

    // Write data chunk
    device->write("data", 4);
    device->write(reinterpret_cast<const char *>(&dataSize), 4);
}

QStringList PiperTTSProvider::findModelsInDirectory(const QString &basePath)
{
    QStringList modelPaths;
    const QDir baseDir(basePath);

    if (!baseDir.exists()) {
        return modelPaths;
    }

    // Iterate through language directories (e.g., en/, de/, fr/)
    const QStringList langDirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &langDir : langDirs) {
        const QDir langPath(baseDir.absoluteFilePath(langDir));

        // Iterate through locale directories (e.g., en_US/, de_DE/, fr_FR/)
        const QStringList localeDirs = langPath.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &localeDir : localeDirs) {
            const QDir localePath(langPath.absoluteFilePath(localeDir));

            // Iterate through voice directories (e.g., lessac/, amy/, thorsten/)
            const QStringList voiceDirs = localePath.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &voiceDir : voiceDirs) {
                const QDir voicePath(localePath.absoluteFilePath(voiceDir));

                // Iterate through quality directories (e.g., low/, medium/, high/)
                const QStringList qualityDirs = voicePath.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &qualityDir : qualityDirs) {
                    const QDir qualityPath(voicePath.absoluteFilePath(qualityDir));

                    // Look for ONNX model files
                    QStringList filters;
                    filters << "*.onnx";
                    const QStringList modelFiles = qualityPath.entryList(filters, QDir::Files);

                    for (const QString &modelFile : modelFiles) {
                        const QString modelPath = qualityPath.absoluteFilePath(modelFile);
                        const QString configPath = modelPath + ".json";

                        // Only add if both model and config exist
                        if (QFile::exists(modelPath) && QFile::exists(configPath)) {
                            modelPaths.append(modelPath);
                        }
                    }
                }
            }
        }
    }

    return modelPaths;
}

QStringList PiperTTSProvider::discoverAvailableModels()
{
    QStringList allModelPaths;

    const QStringList basePaths = getStandardPiperVoicesPaths();
    for (const QString &basePath : basePaths) {
        const QStringList modelPaths = findModelsInDirectory(basePath);
        allModelPaths.append(modelPaths);
    }

    // Remove duplicates but preserve order (earlier paths take priority)
    QStringList uniqueModelPaths;
    for (const QString &modelPath : allModelPaths) {
        if (!uniqueModelPaths.contains(modelPath)) {
            uniqueModelPaths.append(modelPath);
        }
    }

    return uniqueModelPaths;
}

Voice PiperTTSProvider::createVoiceFromModelPath(const QString &modelPath) const
{
    // Parse model path to extract voice information
    // Format: .../lang/locale/voice_name/quality/locale-voice_name-quality.onnx
    // Example: .../en/en_US/lessac/medium/en_US-lessac-medium.onnx

    const QFileInfo fileInfo(modelPath);
    const QString fileName = fileInfo.baseName(); // e.g., "en_US-lessac-medium"

    // Extract components from filename
    QStringList parts = fileName.split('-');
    if (parts.size() < 3) {
        return {}; // Invalid format
    }

    // Parse locale (e.g., "en_US")
    QString localeStr = parts[0];
    if (parts.size() > 3) {
        // Handle cases like "en_US" where locale has underscore
        localeStr = parts[0] + "_" + parts[1];
        parts.removeFirst(); // Remove the first part to adjust indexing
    }

    Language language = Language(localeStr);
    if (!language.isValid()) {
        // Try without country code
        QStringList localeParts = localeStr.split('_');
        if (!localeParts.empty()) {
            language = Language(localeParts[0]);
        }
        if (!language.isValid()) {
            language = Language(QLocale(localeStr));
        }
    }

    // Extract voice name and quality
    QString voiceName;
    QString quality;

    if (parts.size() >= 3) {
        voiceName = parts[1];
        quality = parts[2];
    }

    // Create display name
    const QString displayName = QString("%1 (%2, %3)").arg(voiceName, localeStr, quality);

    // Create custom Voice object with model path as data
    QVariantMap data;
    data["modelPath"] = modelPath;
    return {displayName, language, data};
}

void PiperTTSProvider::reinitializeModels()
{
    qDebug() << "PiperTTSProvider::reinitializeModels - clearing cached models and voices";

    // If we have a currently loaded model, we'll need to reload it if it's still available
    QString currentModelPath;
    if (!m_voice.modelPath().isEmpty()) {
        currentModelPath = m_voice.modelPath();
    }

    // Discover models with new path
    const QStringList modelPaths = discoverAvailableModels();
    qDebug() << "PiperTTSProvider::reinitializeModels - discovered" << modelPaths.size() << "models";

    // Show warning dialog if no models found in the new path
    if (modelPaths.isEmpty()) {
        showPiperVoicesWarning();
    }

    // Try to reload the current model if it still exists
    if (!currentModelPath.isEmpty() && modelPaths.contains(currentModelPath)) {
        qDebug() << "PiperTTSProvider::reinitializeModels - reloading current model:" << currentModelPath;
        const QString configPath = currentModelPath + ".json";
        if (loadModel(currentModelPath, configPath)) {
            // Update current voice with fresh metadata
            m_voice = createVoiceFromModelPath(currentModelPath);
        }
    } else if (!currentModelPath.isEmpty()) {
        qDebug() << "PiperTTSProvider::reinitializeModels - current model no longer available, clearing";
        // Current model is no longer available, clear it
        m_voice = Voice();
        m_speakerIdMap.clear();
    }

    qDebug() << "PiperTTSProvider::reinitializeModels - completed";
}

// Speaker support methods
QStringList PiperTTSProvider::availableSpeakers() const
{
    if (m_speakerIdMap.isEmpty()) {
        return QStringList() << "default";
    }
    return m_speakerIdMap.keys();
}

QStringList PiperTTSProvider::availableSpeakersForVoice(const Voice &voice) const
{
    const QString modelPath = voice.modelPath();
    if (modelPath.isEmpty()) {
        return QStringList() << "default";
    }

    const QString configPath = modelPath + ".json";
    if (!QFile::exists(configPath)) {
        return QStringList() << "default";
    }

    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        return QStringList() << "default";
    }

    const QJsonDocument configDoc = QJsonDocument::fromJson(configFile.readAll());
    QJsonObject config = configDoc.object();

    if (config.contains("speaker_id_map")) {
        QJsonObject speakerMap = config["speaker_id_map"].toObject();
        QStringList speakers;
        for (auto it = speakerMap.begin(); it != speakerMap.end(); ++it) {
            speakers.append(it.key());
        }
        return speakers;
    }

    return QStringList() << "default";
}

QString PiperTTSProvider::currentSpeaker() const
{
    return m_currentSpeaker;
}

void PiperTTSProvider::setSpeaker(const QString &speakerName)
{
    if (m_speakerIdMap.contains(speakerName)) {
        m_currentSpeaker = speakerName;
        m_currentSpeakerId = m_speakerIdMap.value(speakerName);
    } else if (speakerName == "default" && m_speakerIdMap.isEmpty()) {
        m_currentSpeaker = "default";
        m_currentSpeakerId = 0;
    }
}

int PiperTTSProvider::speakerId() const
{
    return m_currentSpeakerId;
}

void PiperTTSProvider::showPiperVoicesWarning()
{
    // Show warning dialog only once per path per session to avoid spam
    static QSet<QString> warningShownForPaths;
    const QString currentPath = QString::fromLocal8Bit(AppSettings().piperVoicesPath());

    if (warningShownForPaths.contains(currentPath)) {
        return;
    }
    warningShownForPaths.insert(currentPath);

    const QString warningText = tr(
        "<h3>Piper Voice Models Not Found</h3>"
        "<p>No Piper TTS voice models were found in the standard locations.</p>"
        "<p><b>To use Piper TTS:</b></p>"
        "<ol>"
        "<li>Download voice models from <a href='https://huggingface.co/rhasspy/piper-voices'>HuggingFace Piper Voices</a></li>"
        "<li>Extract the models to a directory</li>"
        "<li>Set the path in Settings → TTS → Piper Voices Path</li>"
        "</ol>"
        "<p><b>⚠️ License Warning:</b> Some models may have non-free licenses and cannot be used in commercial settings. "
        "Please check the license of each model before commercial use.</p>");

    QMessageBox msgBox(QMessageBox::Warning, tr("Piper Voice Models Required"), warningText);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}
