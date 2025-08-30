/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PiperTTSProvider implementation inspired by libpiper
 * Original libpiper by Michael Hansen: https://github.com/rhasspy/piper
 * Licensed under GPL-3.0, compatible with crow-translate's GPL-3.0+ license
 */

#ifndef PIPERTTSPROVIDER_H
#define PIPERTTSPROVIDER_H

#include "attsprovider.h"
#include "voice.h"

#include <QAudioOutput>
#include <QBuffer>
#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QQueue>
#include <QTemporaryFile>
#include <QThread>

#include <memory>

class PlaylistPlayer;

// Forward declarations for ONNX Runtime
namespace Ort
{
class Env;
class Session;
class SessionOptions;
class AllocatorWithDefaultOptions;
}

// Forward declarations for espeak-ng - types will be defined in the cpp file

typedef char32_t Phoneme;
typedef int64_t PhonemeId;
typedef int64_t SpeakerId;
typedef QHash<Phoneme, QList<PhonemeId>> PhonemeIdMap;

class PiperTTSProvider : public ATTSProvider
{
    Q_OBJECT

public:
    explicit PiperTTSProvider(QObject *parent = nullptr);
    ~PiperTTSProvider() override;

    QString getProviderType() const override;
    void say(const QString &text) override;
    void stop() override;
    void pause() override;
    void resume() override;

    QTextToSpeech::State state() const override;
    QTextToSpeech::ErrorReason errorReason() const override;
    QString errorString() const override;

    Language language() const override;
    void setLanguage(const Language &language) override;

    Voice voice() const override;
    void setVoice(const Voice &voice) override;
    QList<Voice> availableVoices() const override;
    QList<Voice> findVoices(const Language &language) const override;

    double rate() const override;
    void setRate(double rate) override;

    double pitch() const override;
    void setPitch(double pitch) override;

    double volume() const override;
    void setVolume(double volume) override;

    QList<Language> availableLanguages() const override;

    void applyOptions(const ProviderOptions &options) override;
    std::unique_ptr<ProviderOptions> getDefaultOptions() const override;
    QStringList getAvailableOptions() const override;

    ProviderUIRequirements getUIRequirements() const override;

    // Speaker support (Piper-specific)
    QStringList availableSpeakers() const override;
    QStringList availableSpeakersForVoice(const Voice &voice) const override;
    QString currentSpeaker() const override;
    void setSpeaker(const QString &speakerName) override;
    int speakerId() const;

private slots:
    void onSynthesisFinished();

private:
    void setState(QTextToSpeech::State newState);
    void setError(QTextToSpeech::ErrorReason reason, const QString &errorString);
    void initializeAudio();
    void cleanupAudio();
    bool initializePiper();
    void cleanupPiper();

#ifdef _WIN32
    void handleOnnxTelemetryOnWindows();
#endif

    // Core synthesis methods (like libpiper)
    bool loadModel(const QString &modelPath, const QString &configPath);
    QList<PhonemeId> textToPhonemeIds(const QString &text);
    QList<float> synthesizeAudio(const QList<PhonemeId> &phonemeIds);

    // Model discovery methods
    QStringList discoverAvailableModels();
    QStringList getStandardPiperVoicesPaths();
    void reinitializeModels();
    void showPiperVoicesWarning();

    // Audio file helper
    void writeWavHeader(QIODevice *device, int numSamples, int sampleRate, int numChannels, int bitsPerSample);
    QStringList findModelsInDirectory(const QString &basePath);
    Voice createVoiceFromModelPath(const QString &modelPath) const;

    QTextToSpeech::State m_state;
    QTextToSpeech::ErrorReason m_errorReason;
    QString m_errorString;

    Language m_language;
    Voice m_voice;
    double m_rate;
    double m_pitch;
    double m_volume;

    PlaylistPlayer *m_player;
    QTemporaryFile *m_tempAudioFile;
    QByteArray m_audioData;

    // Piper-specific members
    QString m_espeakVoice;
    int m_sampleRate;
    int m_numSpeakers;
    PhonemeIdMap m_phonemeIdMap;

    // Speaker support
    QHash<QString, int> m_speakerIdMap; // speaker name -> speaker ID
    QString m_currentSpeaker;
    int m_currentSpeakerId;

    // ONNX Runtime session
    std::unique_ptr<Ort::Env> m_ortEnv;
    std::unique_ptr<Ort::Session> m_ortSession;
    std::unique_ptr<Ort::SessionOptions> m_sessionOptions;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> m_allocator;

    // Synthesis parameters
    float m_lengthScale;
    float m_noiseScale;
    float m_noiseWScale;

    // State management
    QQueue<QList<PhonemeId>> m_phonemeIdQueue;
    QList<float> m_chunkSamples;

    mutable QMutex m_stateMutex;
    bool m_initialized;

    // Voice to model path mapping
    QHash<QString, QString> m_voiceModelPaths;
};

#endif // PIPERTTSPROVIDER_H