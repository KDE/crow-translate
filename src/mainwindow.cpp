/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "popupwindow.h"
#include "provideroptions.h"
#include "provideroptionsmanager.h"
#include "screenwatcher.h"
#include "selection.h"
#include "singleapplication.h"
#include "ocr/screengrabbers/abstractscreengrabber.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"
#include "tts/voice.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSet>
#include <QTimer>

#include <cassert>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_translateSelectionHotkey(new QHotkey(this))
    , m_speakSelectionHotkey(new QHotkey(this))
    , m_speakTranslatedSelectionHotkey(new QHotkey(this))
    , m_stopSpeakingHotkey(new QHotkey(this))
    , m_playPauseSpeakingHotkey(new QHotkey(this))
    , m_showMainWindowHotkey(new QHotkey(this))
    , m_copyTranslatedSelectionHotkey(new QHotkey(this))
    , m_recognizeScreenAreaHotkey(new QHotkey(this))
    , m_translateScreenAreaHotkey(new QHotkey(this))
    , m_delayedRecognizeScreenAreaHotkey(new QHotkey(this))
    , m_delayedTranslateScreenAreaHotkey(new QHotkey(this))
    , m_toggleOcrNegateHotkey(new QHotkey(this))
    , m_closeWindowsShortcut(new QShortcut(this))
    , ui(new Ui::MainWindow)
    , m_optionsManager(new ProviderOptionsManager(this))
    , m_ocr(new Ocr())
    , m_screenCaptureTimer(new QTimer(this))
    , m_snippingArea(new SnippingArea(this))
    , m_trayIcon(new TrayIcon(this))
    , m_orientationWatcher(new ScreenWatcher(this))
    , m_screenGrabber(AbstractScreenGrabber::createScreenGrabber(this))
{
    ui->setupUi(this);
    // Screen orientation
    connect(m_orientationWatcher, &ScreenWatcher::screenOrientationChanged, this, &MainWindow::setOrientation);

    // Show the main window if a secondary instance has been started
    connect(qobject_cast<SingleApplication *>(QCoreApplication::instance()), &SingleApplication::instanceStarted, this, &MainWindow::open);

    // Selection requests
    connect(&Selection::instance(), &Selection::requestedSelectionAvailable, ui->sourceEdit, &SourceTextEdit::replaceText);
    // Hotkeys
    connect(m_closeWindowsShortcut, &QShortcut::activated, this, &MainWindow::close);
    connect(m_showMainWindowHotkey, &QHotkey::activated, this, &MainWindow::open);
    connect(m_translateSelectionHotkey, &QHotkey::activated, this, &MainWindow::translateSelection);
    connect(m_speakSelectionHotkey, &QHotkey::activated, this, &MainWindow::speakSelection);
    connect(m_speakTranslatedSelectionHotkey, &QHotkey::activated, this, &MainWindow::speakTranslatedSelection);
    connect(m_stopSpeakingHotkey, &QHotkey::activated, this, &MainWindow::stopSpeaking);
    connect(m_playPauseSpeakingHotkey, &QHotkey::activated, this, &MainWindow::playPauseSpeaking);
    connect(m_copyTranslatedSelectionHotkey, &QHotkey::activated, this, &MainWindow::copyTranslatedSelection);
    connect(m_recognizeScreenAreaHotkey, &QHotkey::activated, this, &MainWindow::recognizeScreenArea);
    connect(m_translateScreenAreaHotkey, &QHotkey::activated, this, &MainWindow::translateScreenArea);
    connect(m_delayedRecognizeScreenAreaHotkey, &QHotkey::activated, this, &MainWindow::delayedRecognizeScreenArea);
    connect(m_delayedTranslateScreenAreaHotkey, &QHotkey::activated, this, &MainWindow::delayedTranslateScreenArea);
    connect(m_toggleOcrNegateHotkey, &QHotkey::activated, this, &MainWindow::toggleOcrNegate);

    // OCR logic
    connect(m_screenGrabber, &AbstractScreenGrabber::grabbed, m_snippingArea, &SnippingArea::snip);
    connect(m_snippingArea, &SnippingArea::snipped, m_ocr, &Ocr::recognize);
    connect(m_ocr, &Ocr::recognized, ui->sourceEdit, &SourceTextEdit::replaceText);
    m_screenCaptureTimer->setSingleShot(true);

    loadAppSettings();
    m_tts = ATTSProvider::createTTSProvider(this, m_chosenTTSBackend);
    connect(m_tts, &ATTSProvider::errorOccurred, this, &MainWindow::onTTSError);
    applyTTSProviderSettings();
    m_translator = ATranslationProvider::createTranslationProvider(this, m_chosenTranslationBackend);

    updateProviderUI();

    connect(ui->sourcePlayPauseButton, &QToolButton::clicked, this, &MainWindow::sourcePlayPauseClicked);
    connect(ui->sourceStopButton, &QToolButton::clicked, this, &MainWindow::sourceStopClicked);
    connect(ui->translationPlayPauseButton, &QToolButton::clicked, this, &MainWindow::translationPlayPauseClicked);
    connect(ui->translationStopButton, &QToolButton::clicked, this, &MainWindow::translationStopClicked);
    connect(ui->settingsButton, &QToolButton::clicked, this, &MainWindow::openSettings);
    connect(m_tts, &ATTSProvider::stateChanged, this, &MainWindow::ttsStateChanged);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::updateTTSButtonStates);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::updateAutoLocales);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::handleAutoTranslation);

    ui->sourceEdit->setListenForEdits(true);

    connect(ui->sourceVoiceComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_tts && ui->sourceVoiceComboBox->currentData().isValid()) {
            const ProviderUIRequirements reqs = m_tts->getUIRequirements();
            if (reqs.supportedCapabilities.contains("voiceSelection")) {
                const Voice voice = ui->sourceVoiceComboBox->currentData().value<Voice>();
                const Language sourceLanguage = m_sourceLang != Language::autoLanguage() ? m_sourceLang : Language(QLocale::system());
                m_tts->setLanguage(sourceLanguage);
                m_tts->setVoice(voice);
                updateSpeakerComboBoxes();
            }
        }
    });

    connect(ui->translationVoiceComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_tts && ui->translationVoiceComboBox->currentData().isValid()) {
            const ProviderUIRequirements reqs = m_tts->getUIRequirements();
            if (reqs.supportedCapabilities.contains("voiceSelection")) {
                const Voice voice = ui->translationVoiceComboBox->currentData().value<Voice>();
                const Language translationLanguage = m_destLang != Language::autoLanguage() ? m_destLang : Language(QLocale::system());
                m_tts->setLanguage(translationLanguage);
                m_tts->setVoice(voice);
                updateSpeakerComboBoxes();
            }
        }
    });

    connect(ui->sourceSpeakerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_tts && ui->sourceSpeakerComboBox->count() > 0) {
            const ProviderUIRequirements reqs = m_tts->getUIRequirements();
            if (reqs.supportedCapabilities.contains("speakerSelection")) {
                const QString speaker = ui->sourceSpeakerComboBox->currentText();
                if (!speaker.isEmpty()) {
                    auto options = m_optionsManager->createTTSOptionsFromSettings(m_tts);
                    if (options) {
                        options->setOption("speaker", speaker);
                        m_tts->applyOptions(*options);
                    }
                }
            }
        }
    });

    connect(ui->translationSpeakerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_tts && ui->translationSpeakerComboBox->count() > 0) {
            const ProviderUIRequirements reqs = m_tts->getUIRequirements();
            if (reqs.supportedCapabilities.contains("speakerSelection")) {
                const QString speaker = ui->translationSpeakerComboBox->currentText();
                if (!speaker.isEmpty()) {
                    auto options = m_optionsManager->createTTSOptionsFromSettings(m_tts);
                    if (options) {
                        options->setOption("speaker", speaker);
                        m_tts->applyOptions(*options);
                    }
                }
            }
        }
    });

    setupEngineComboBoxConnection();

    connect(ui->translationEdit, &QTextEdit::textChanged, this, &MainWindow::updateTTSButtonStates);
    connect(m_translator, &ATranslationProvider::stateChanged, this, &MainWindow::translatorStateChanged);
    connect(this, &MainWindow::translationRequested, this, &MainWindow::handleTranslationRequest);
    connect(this, &MainWindow::translationAccepted, m_translator, &ATranslationProvider::finish);
    connect(this, &MainWindow::resetTranslator, m_translator, &ATranslationProvider::reset);

    applyTranslationProviderSettings();

    // Connect to abstract provider signals
    connect(m_translator, &ATranslationProvider::engineChanged, this, [this]() {
        refreshLanguageWidgetsWithSupportedLanguages();
    });
    connect(m_translator, &ATranslationProvider::languageDetected, this, [this](const Language &detectedLanguage, bool isTranslationContext) {
        if (ui->sourceLanguagesWidget->isAutoButtonChecked()) {
            m_sourceLang = detectedLanguage;
            ui->sourceLanguagesWidget->setAutoLanguage(detectedLanguage);
        }

        // If translation auto button is checked, re-evaluate destination language with detected source
        // Only retranslate if: (translation context) OR (auto-translate is enabled)
        if (ui->translationLanguagesWidget->isAutoButtonChecked() && (isTranslationContext || ui->autoTranslateCheckBox->isChecked())) {
            const Language preferredDest = preferredTranslationLanguage(detectedLanguage);
            qDebug() << "Language detected:" << detectedLanguage.name() << "isTranslationContext:" << isTranslationContext
                     << "preferred destination:" << preferredDest.name();

            // Check if the current translation target matches our preferred destination
            // If not, retranslate with the correct destination language
            if (m_translator && m_translator->getState() == ATranslationProvider::State::Processed) {
                const Language currentDestLang = m_translator->translationLanguage;
                if (currentDestLang != preferredDest && !ui->sourceEdit->toPlainText().isEmpty()) {
                    qDebug() << "Retranslating from" << detectedLanguage.name() << "to preferred destination:" << preferredDest.name();

                    // Update the destination language state and UI
                    m_destLang = preferredDest;
                    ui->translationLanguagesWidget->setAutoLanguage(preferredDest);

                    // Update voice comboboxes for the new destination language
                    updateVoiceComboBoxes();

                    m_translator->translate(ui->sourceEdit->toSourceText(), preferredDest, detectedLanguage);
                }
            }
        }
    });

    loadMainWindowSettings();

    refreshLanguageWidgetsWithSupportedLanguages();

    updateVoiceComboBoxes();

    connect(ui->sourceLanguagesWidget, &LanguageButtonsWidget::languagesChanged, this, [](const QVector<Language> &languages) {
        AppSettings().setLanguages(AppSettings::Source, languages);
    });
    connect(ui->translationLanguagesWidget, &LanguageButtonsWidget::languagesChanged, this, [](const QVector<Language> &languages) {
        AppSettings().setLanguages(AppSettings::Translation, languages);
    });

    // Source and translation logic
    connect(ui->sourceLanguagesWidget, &LanguageButtonsWidget::buttonChecked, this, &MainWindow::onSourceLanguageChanged);
    connect(ui->translationLanguagesWidget, &LanguageButtonsWidget::buttonChecked, this, &MainWindow::onDestinationLanguageChanged);

    connect(ui->sourceLanguagesWidget, &LanguageButtonsWidget::autoLanguageChanged, this, [this](const Language &language) {
        if (ui->sourceLanguagesWidget->isAutoButtonChecked()) {
            m_sourceLang = language;
            updateTTSButtonStates();
            updateVoiceComboBoxes();
        }
    });

    updateTTSButtonStates();
}

MainWindow::~MainWindow()
{
    delete ui;
}

Ocr *MainWindow::ocr() const
{
    return m_ocr;
}

QComboBox *MainWindow::getEngineComboBox() const
{
    return ui->engineComboBox;
}

QComboBox *MainWindow::sourceVoiceComboBox() const
{
    return ui->sourceVoiceComboBox;
}

QComboBox *MainWindow::translationVoiceComboBox() const
{
    return ui->translationVoiceComboBox;
}

QComboBox *MainWindow::sourceSpeakerComboBox() const
{
    return ui->sourceSpeakerComboBox;
}

QComboBox *MainWindow::translationSpeakerComboBox() const
{
    return ui->translationSpeakerComboBox;
}

LanguageButtonsWidget *MainWindow::sourceLanguageButtons() const
{
    return ui->sourceLanguagesWidget;
}

LanguageButtonsWidget *MainWindow::translationLanguageButtons() const
{
    return ui->translationLanguagesWidget;
}

QToolButton *MainWindow::copyTranslationButton() const
{
    return ui->copyTranslationButton;
}

QToolButton *MainWindow::swapButton() const
{
    return ui->swapButton;
}

QToolButton *MainWindow::copySourceButton() const
{
    return ui->copySourceButton;
}

QToolButton *MainWindow::copyAllTranslationButton() const
{
    return ui->copyAllTranslationButton;
}

QTextEdit *MainWindow::translationEdit() const
{
    return ui->translationEdit;
}

SourceTextEdit *MainWindow::sourceEdit() const
{
    return ui->sourceEdit;
}

QToolButton *MainWindow::sourcePlayPauseButton() const
{
    return ui->sourcePlayPauseButton;
}

QToolButton *MainWindow::sourceStopButton() const
{
    return ui->sourceStopButton;
}

QToolButton *MainWindow::translationPlayPauseButton() const
{
    return ui->translationPlayPauseButton;
}

QToolButton *MainWindow::translationStopButton() const
{
    return ui->translationStopButton;
}

Q_SCRIPTABLE void MainWindow::toggleOcrNegate()
{
    constexpr int timeout = 500;
    AppSettings settings;
    const bool negate = settings.toggleOcrNegate();
    m_snippingArea->setNegateOcrImage(negate);
    m_trayIcon->showMessage(QApplication::tr("OCR"),
                            negate ? QApplication::tr("OCR image negated") : QApplication::tr("OCR image is normal"),
                            QSystemTrayIcon::Information,
                            timeout);
}

QKeySequence MainWindow::closeWindowShortcut() const
{
    return m_closeWindowsShortcut->key();
}

void MainWindow::setListenForContentChanges(bool listen)
{
    m_listenForContentChanges = listen;
}

Q_SCRIPTABLE void MainWindow::open()
{
    ui->sourceEdit->setFocus();
    ui->sourceEdit->selectAll();

    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();

    // Required to show the application on some WMs like XFWM
    // if window already opened on different workspace. Doesn't
    // affect KWin.
    raise();

    activateWindow();
}

Q_SCRIPTABLE void MainWindow::translateSelection()
{
    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    auto selectionConnection = std::make_shared<QMetaObject::Connection>();
    *selectionConnection =
        connect(&Selection::instance(), &Selection::requestedSelectionAvailable, this, [this, selectionConnection](const QString &selectedText) {
            if (!selectedText.isEmpty()) {
                ui->sourceEdit->setPlainText(selectedText);
                ui->sourceEdit->stopEditTimer(); // Prevent delayed textEdited signal

                // Use auto-detect for selection text if auto button is checked, otherwise use selected language
                const bool isSourceAutoChecked = ui->sourceLanguagesWidget->isAutoButtonChecked();
                const bool isTranslationAutoChecked = ui->translationLanguagesWidget->isAutoButtonChecked();
                const Language sourceLanguage = isSourceAutoChecked ? Language::autoLanguage() : m_sourceLang;
                const Language destinationLanguage = isTranslationAutoChecked ? Language::autoLanguage() : m_destLang;

                if (m_translator && m_translator->getState() == ATranslationProvider::State::Ready) {
                    emit translationRequested(selectedText, destinationLanguage, sourceLanguage);
                } else {
                    // Wait for translator to be ready
                    auto connection = std::make_shared<QMetaObject::Connection>();
                    *connection = connect(m_translator,
                                          &ATranslationProvider::stateChanged,
                                          this,
                                          [this, selectedText, sourceLanguage, destinationLanguage, connection](ATranslationProvider::State state) {
                                              if (state == ATranslationProvider::State::Ready) {
                                                  emit translationRequested(selectedText, destinationLanguage, sourceLanguage);
                                                  disconnect(*connection);
                                              }
                                          });
                }
            }
            disconnect(*selectionConnection);
        });
    Selection::instance().requestSelection();
}

Q_SCRIPTABLE void MainWindow::speakSelection()
{
    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }

    auto selectionConnection = std::make_shared<QMetaObject::Connection>();
    *selectionConnection =
        connect(&Selection::instance(), &Selection::requestedSelectionAvailable, this, [this, selectionConnection](const QString &selectedText) {
            if (!selectedText.isEmpty() && m_tts) {
                const Language sourceLanguage = m_sourceLang != Language::autoLanguage() ? m_sourceLang : Language(QLocale::system());
                m_tts->setLanguage(sourceLanguage);
                m_tts->say(selectedText);
            }
            disconnect(*selectionConnection);
        });
    Selection::instance().requestSelection();
}

Q_SCRIPTABLE void MainWindow::speakTranslatedSelection()
{
    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    const QString translationText = ui->translationEdit->toPlainText();
    if (!translationText.isEmpty() && (m_tts != nullptr)) {
        const Language translationLanguage = m_destLang != Language::autoLanguage() ? m_destLang : Language(QLocale::system());
        m_tts->setLanguage(translationLanguage);
        m_tts->say(translationText);
    }
}

Q_SCRIPTABLE void MainWindow::stopSpeaking()
{
    if (m_tts != nullptr) {
        m_tts->stop();
    }
}

Q_SCRIPTABLE void MainWindow::playPauseSpeaking()
{
    if (m_tts == nullptr) {
        return;
    }

    if (m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->pause();
    } else if (m_tts->state() == QTextToSpeech::Paused) {
        m_tts->resume();
    }
}

Q_SCRIPTABLE void MainWindow::copyTranslatedSelection()
{
    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    const QString translationText = ui->translationEdit->toPlainText();
    if (!translationText.isEmpty()) {
        QApplication::clipboard()->setText(translationText);
    }
}

Q_SCRIPTABLE void MainWindow::recognizeScreenArea()
{
    if (m_ocr->languagesString().isEmpty()) {
        QMessageBox::critical(this, Ocr::tr("OCR languages are not loaded"), Ocr::tr("You should set at least one OCR language in the application settings"));
        return;
    }

    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    if (m_screenGrabber != nullptr) {
        m_screenGrabber->grab();
    }
}

Q_SCRIPTABLE void MainWindow::translateScreenArea()
{
    if (m_ocr->languagesString().isEmpty()) {
        QMessageBox::critical(this, Ocr::tr("OCR languages are not loaded"), Ocr::tr("You should set at least one OCR language in the application settings"));
        return;
    }

    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    if (m_screenGrabber != nullptr) {
        auto ocrConnection = std::make_shared<QMetaObject::Connection>();
        *ocrConnection = connect(m_ocr, &Ocr::recognized, this, [this, ocrConnection](const QString &text) {
            ui->sourceEdit->setPlainText(text);
            ui->sourceEdit->stopEditTimer(); // Prevent delayed textEdited signal

            // Use auto-detect for OCR text if auto button is checked, otherwise use selected language
            const bool isSourceAutoChecked = ui->sourceLanguagesWidget->isAutoButtonChecked();
            const bool isTranslationAutoChecked = ui->translationLanguagesWidget->isAutoButtonChecked();
            const Language sourceLanguage = isSourceAutoChecked ? Language::autoLanguage() : m_sourceLang;
            const Language destinationLanguage = isTranslationAutoChecked ? Language::autoLanguage() : m_destLang;
            qDebug() << "OCR: sourceAutoChecked:" << isSourceAutoChecked << "translationAutoChecked:" << isTranslationAutoChecked
                     << "sourceLanguage:" << sourceLanguage.name() << "destinationLanguage:" << destinationLanguage.name();

            if (m_translator && m_translator->getState() == ATranslationProvider::State::Ready) {
                emit translationRequested(text, destinationLanguage, sourceLanguage);
            } else {
                // Wait for translator to be ready
                auto connection = std::make_shared<QMetaObject::Connection>();
                *connection =
                    connect(m_translator, &ATranslationProvider::stateChanged, this, [this, text, sourceLanguage, destinationLanguage, connection](ATranslationProvider::State state) {
                        if (state == ATranslationProvider::State::Ready) {
                            emit translationRequested(text, destinationLanguage, sourceLanguage);
                            disconnect(*connection);
                        }
                    });
            }
            disconnect(*ocrConnection);
        });
        m_screenGrabber->grab();
    }
}

Q_SCRIPTABLE void MainWindow::delayedRecognizeScreenArea()
{
    if (m_ocr->languagesString().isEmpty()) {
        QMessageBox::critical(this, Ocr::tr("OCR languages are not loaded"), Ocr::tr("You should set at least one OCR language in the application settings"));
        return;
    }

    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    if ((m_screenCaptureTimer != nullptr) && (m_screenGrabber != nullptr)) {
        const AppSettings settings;
        m_screenCaptureTimer->start(settings.captureDelay());
        connect(
            m_screenCaptureTimer,
            &QTimer::timeout,
            this,
            [this]() {
                m_screenGrabber->grab();
            },
            Qt::SingleShotConnection);
    }
}

Q_SCRIPTABLE void MainWindow::delayedTranslateScreenArea()
{
    if (m_ocr->languagesString().isEmpty()) {
        QMessageBox::critical(this, Ocr::tr("OCR languages are not loaded"), Ocr::tr("You should set at least one OCR language in the application settings"));
        return;
    }

    AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }

    if ((m_screenCaptureTimer != nullptr) && (m_screenGrabber != nullptr)) {
        const AppSettings settings;
        m_screenCaptureTimer->start(settings.captureDelay());
        connect(
            m_screenCaptureTimer,
            &QTimer::timeout,
            this,
            [this]() {
                auto ocrConnection = std::make_shared<QMetaObject::Connection>();
                *ocrConnection = connect(m_ocr, &Ocr::recognized, this, [this, ocrConnection](const QString &text) {
                    ui->sourceEdit->setPlainText(text);
                    ui->sourceEdit->stopEditTimer(); // Prevent delayed textEdited signal

                    // Use auto-detect for OCR text if auto button is checked, otherwise use selected language
                    const bool isSourceAutoChecked = ui->sourceLanguagesWidget->isAutoButtonChecked();
                    const bool isTranslationAutoChecked = ui->translationLanguagesWidget->isAutoButtonChecked();
                    const Language sourceLanguage = isSourceAutoChecked ? Language::autoLanguage() : m_sourceLang;
                    const Language destinationLanguage = isTranslationAutoChecked ? Language::autoLanguage() : m_destLang;
                    qDebug() << "Delayed OCR: sourceAutoChecked:" << isSourceAutoChecked << "translationAutoChecked:" << isTranslationAutoChecked
                             << "sourceLanguage:" << sourceLanguage.name() << "destinationLanguage:" << destinationLanguage.name();

                    if (m_translator && m_translator->getState() == ATranslationProvider::State::Ready) {
                        emit translationRequested(text, destinationLanguage, sourceLanguage);
                    } else {
                        // Wait for translator to be ready
                        auto connection = std::make_shared<QMetaObject::Connection>();
                        *connection = connect(m_translator,
                                              &ATranslationProvider::stateChanged,
                                              this,
                                              [this, text, sourceLanguage, destinationLanguage, connection](ATranslationProvider::State state) {
                                                  if (state == ATranslationProvider::State::Ready) {
                                                      emit translationRequested(text, destinationLanguage, sourceLanguage);
                                                      disconnect(*connection);
                                                  }
                                              });
                    }
                    disconnect(*ocrConnection);
                });
                m_screenGrabber->grab();
            },
            Qt::SingleShotConnection);
    }
}

Q_SCRIPTABLE void MainWindow::clearText()
{
    ui->sourceEdit->removeText();
    ui->translationEdit->clear();
}

Q_SCRIPTABLE void MainWindow::openSettings()
{
    SettingsDialog config(this);
    connect(&config, &SettingsDialog::translationBackendChanged, this, &MainWindow::swapTranslator);
    connect(&config, &SettingsDialog::ttsBackendChanged, this, &MainWindow::swapTTSProvider);

    // Let ProviderOptionsManager handle all provider-specific settings changes
    m_optionsManager->connectToSettingsDialog(&config, m_tts);
    connect(m_optionsManager, &ProviderOptionsManager::ttsProviderUIUpdateRequired, this, [this]() {
        updateVoiceComboBoxes();
        updateSpeakerComboBoxes();
    });
    if (config.exec() == QDialog::Accepted) {
        loadAppSettings();

        applyTranslationProviderSettings();
    }
}

Q_SCRIPTABLE void MainWindow::quit()
{
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
}

void MainWindow::showTranslationWindow()
{
    // Always show main window if it already opened
    if (!isHidden()) {
        open();
        return;
    }

    switch (m_windowMode) {
    case AppSettings::PopupWindow: {
        auto *popup = new PopupWindow(this);
        popup->show();
        popup->activateWindow();

        // Force listening for changes in source field
        if (!m_listenForContentChanges) {
            setListenForContentChanges(true);
            connect(popup, &PopupWindow::destroyed, [this] {
                setListenForContentChanges(ui->autoTranslateCheckBox->isChecked());
            });
        }

        break;
    }
    case AppSettings::MainWindow:
        open();
        break;
    case AppSettings::Notification:
        break;
    }
}

void MainWindow::loadMainWindowSettings()
{
    AppSettings settings;
    ui->autoTranslateCheckBox->setChecked(settings.isAutoTranslateEnabled());
    ui->sourceLanguagesWidget->setLanguages(settings.languages(AppSettings::Source));

    QVector<Language> translationLanguages = settings.languages(AppSettings::Translation);
    translationLanguages.removeAll(Language::autoLanguage());
    ui->translationLanguagesWidget->setLanguages(translationLanguages);
    // Auto button in translation language = use auto-translation settings
    ui->translationLanguagesWidget->setAutoButtonVisible(true);
    ui->translationLanguagesWidget->checkButton(settings.checkedButton(AppSettings::Translation));
    ui->sourceLanguagesWidget->checkButton(settings.checkedButton(AppSettings::Source));

    m_sourceLang = ui->sourceLanguagesWidget->checkedLanguage();
    m_destLang = ui->translationLanguagesWidget->checkedLanguage();

    if (m_destLang == QLocale::c()) {
        m_destLang = QLocale::system();
        ui->translationLanguagesWidget->checkLanguage(m_destLang);
    }

    restoreGeometry(settings.mainWindowGeometry());
    if (!settings.isShowTrayIcon() || !settings.isStartMinimized()) {
        show();
        ui->sourceEdit->setFocus();
    }

    if (settings.isShowPrivacyPopup()) {
        const QString href = QStringLiteral("<a href=\"%1\">%2</a>");
        const QString mozhiLink = href.arg(QStringLiteral("https://codeberg.org/aryak/mozhi"), QStringLiteral("Mozhi"));
        const QString instanceLink = href.arg(settings.instance(), settings.instance());
        QMessageBox messageBox;
        messageBox.setIcon(QMessageBox::Information);
        messageBox.setWindowTitle(APPLICATION_NAME);
        messageBox.setText(tr("This application uses %1 to provide translations.").arg(mozhiLink));
        messageBox.setInformativeText(tr("While Mozhi acts as a proxy to protect your privacy,"
                                         " the third-party services it uses may store and analyze"
                                         " the text you send. Your instance right now is '%1',"
                                         " but you can change it in the translation settings."
                                         " See instance details in the link above.")
                                          .arg(instanceLink));

        QCheckBox dontShowAgainCheckBox(tr("Don't show again"));
        messageBox.setCheckBox(&dontShowAgainCheckBox);
        messageBox.exec();

        settings.setShowPrivacyPopup(!dontShowAgainCheckBox.isChecked());
    }
}

void MainWindow::saveMainWindowSettings()
{
    AppSettings settings;

    settings.setMainWindowGeometry(saveGeometry());

    settings.setAutoTranslateEnabled(ui->autoTranslateCheckBox->isChecked());

    settings.setLanguages(AppSettings::Source, ui->sourceLanguagesWidget->languages());
    settings.setLanguages(AppSettings::Translation, ui->translationLanguagesWidget->languages());
    settings.setCheckedButton(AppSettings::Source, ui->sourceLanguagesWidget->checkedId());
    settings.setCheckedButton(AppSettings::Translation, ui->translationLanguagesWidget->checkedId());
}

void MainWindow::loadAppSettings()
{
    AppSettings settings;

    // General
    m_windowMode = settings.windowMode();
    setOrientation(settings.mainWindowOrientation());
    m_trayIcon->setTranslationNotificationTimeout(settings.translationNotificationTimeout());

    // Interface
    ui->translationEdit->setFont(settings.font());
    ui->sourceEdit->setFont(settings.font());

    ui->sourceLanguagesWidget->setLanguageFormat(settings.mainWindowLanguageFormat());
    ui->translationLanguagesWidget->setLanguageFormat(settings.mainWindowLanguageFormat());

    if (const AppSettings::IconType iconType = settings.trayIconType(); iconType == AppSettings::CustomIcon) {
        const QString customIconPath = settings.customIconPath();
        m_trayIcon->setIcon(TrayIcon::customTrayIcon(customIconPath));
        if (m_trayIcon->icon().isNull()) {
            m_trayIcon->showMessage(TrayIcon::tr("Invalid tray icon"), TrayIcon::tr("The specified icon '%1' is invalid. The default icon will be used.").arg(customIconPath));
            m_trayIcon->setIcon(QIcon::fromTheme(TrayIcon::trayIconName(AppSettings::DefaultIcon)));
            settings.setTrayIconType(AppSettings::DefaultIcon);
        }
    } else {
        m_trayIcon->setIcon(QIcon::fromTheme(TrayIcon::trayIconName(iconType)));
    }
    m_trayIcon->setVisible(settings.isShowTrayIcon());
    QGuiApplication::setQuitOnLastWindowClosed(!m_trayIcon->isVisible());
    m_chosenTTSBackend = settings.ttsProviderBackend();
    m_chosenTranslationBackend = settings.translationProviderBackend();

    // Validate TTS backend availability
    ProviderOptionsManager::validateTTSBackendAvailability();
    m_chosenTTSBackend = settings.ttsProviderBackend(); // Reload in case it was changed by validation

    ui->sourceEdit->setSimplifySource(settings.isSimplifySource());

    if (const QByteArray languages = settings.ocrLanguagesString(), path = settings.ocrLanguagesPath();
        !m_ocr->init(languages, path, settings.tesseractParameters())) {
        if (languages != AppSettings::defaultOcrLanguagesString() || path != AppSettings::defaultOcrLanguagesPath())
            m_trayIcon->showMessage(Ocr::tr("Unable to set OCR languages"), Ocr::tr("Unable to initialize Tesseract with %1").arg(QString(languages)));
    }
    if (const AppSettings::RegionRememberType type = settings.regionRememberType(); m_snippingArea->regionRememberType() != type) {
        m_snippingArea->setRegionRememberType(type);
        if (type == AppSettings::RememberAlways)
            m_snippingArea->setCropRegion(settings.cropRegion());
    }
    m_ocr->setConvertLineBreaks(settings.isConvertLineBreaks());
    m_screenCaptureTimer->setInterval(settings.captureDelay());
    m_snippingArea->setCaptureOnRelese(settings.isConfirmOnRelease());
    m_snippingArea->setShowMagnifier(settings.isShowMagnifier());
    m_snippingArea->setApplyLightMask(settings.isApplyLightMask());
    m_snippingArea->setNegateOcrImage(settings.isOcrNegate());

    if (const QNetworkProxy::ProxyType proxyType = settings.proxyType(); proxyType == QNetworkProxy::DefaultProxy) {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
    } else {
        QNetworkProxy proxy(proxyType);
        if (proxyType == QNetworkProxy::HttpProxy || proxyType == QNetworkProxy::Socks5Proxy) {
            proxy.setHostName(settings.proxyHost());
            proxy.setPort(settings.proxyPort());
            if (settings.isProxyAuthEnabled()) {
                proxy.setUser(settings.proxyUsername());
                proxy.setPassword(settings.proxyPassword());
            }
        }
        QNetworkProxy::setApplicationProxy(proxy);
    }

    // Global shortcuts
    if (QHotkey::isPlatformSupported() && settings.isGlobalShortuctsEnabled()) {
        m_translateSelectionHotkey->setShortcut(settings.translateSelectionShortcut(), true);
        m_speakSelectionHotkey->setShortcut(settings.speakSelectionShortcut(), true);
        m_stopSpeakingHotkey->setShortcut(settings.stopSpeakingShortcut(), true);
        m_playPauseSpeakingHotkey->setShortcut(settings.playPauseSpeakingShortcut(), true);
        m_speakTranslatedSelectionHotkey->setShortcut(settings.speakTranslatedSelectionShortcut(), true);
        m_showMainWindowHotkey->setShortcut(settings.showMainWindowShortcut(), true);
        m_copyTranslatedSelectionHotkey->setShortcut(settings.copyTranslatedSelectionShortcut(), true);
        m_recognizeScreenAreaHotkey->setShortcut(settings.recognizeScreenAreaShortcut(), true);
        m_translateScreenAreaHotkey->setShortcut(settings.translateScreenAreaShortcut(), true);
        m_delayedRecognizeScreenAreaHotkey->setShortcut(settings.delayedRecognizeScreenAreaShortcut(), true);
        m_delayedTranslateScreenAreaHotkey->setShortcut(settings.delayedTranslateScreenAreaShortcut(), true);
        m_toggleOcrNegateHotkey->setShortcut(settings.toggleOcrNegateShortcut(), true);
    } else {
        m_translateSelectionHotkey->setRegistered(false);
        m_speakSelectionHotkey->setRegistered(false);
        m_stopSpeakingHotkey->setRegistered(false);
        m_playPauseSpeakingHotkey->setRegistered(false);
        m_speakTranslatedSelectionHotkey->setRegistered(false);
        m_showMainWindowHotkey->setRegistered(false);
        m_copyTranslatedSelectionHotkey->setRegistered(false);
        m_recognizeScreenAreaHotkey->setRegistered(false);
        m_translateScreenAreaHotkey->setRegistered(false);
        m_delayedRecognizeScreenAreaHotkey->setRegistered(false);
        m_delayedTranslateScreenAreaHotkey->setRegistered(false);
        m_toggleOcrNegateHotkey->setRegistered(false);
    }

    // Window shortcuts
    ui->sourcePlayPauseButton->setShortcut(settings.speakSourceShortcut());
    ui->translationPlayPauseButton->setShortcut(settings.speakTranslationShortcut());
    ui->translateButton->setShortcut(settings.translateShortcut());
    ui->swapButton->setShortcut(settings.swapShortcut());
    ui->copyTranslationButton->setShortcut(settings.copyTranslationShortcut());
    m_closeWindowsShortcut->setKey(settings.closeWindowShortcut());
}

void MainWindow::applyTranslationProviderSettings()
{
    if (m_translator == nullptr) {
        return;
    }

    m_optionsManager->applySettingsToTranslationProvider(m_translator);
}

void MainWindow::applyTTSProviderSettings()
{
    if (m_tts == nullptr) {
        return;
    }

    m_optionsManager->applySettingsToTTSProvider(m_tts);

    // Update UI elements that depend on provider state
    updateVoiceComboBoxes();
    updateSpeakerComboBoxes();
}

void MainWindow::refreshLanguageWidgetsWithSupportedLanguages()
{
    if (m_translator == nullptr) {
        return;
    }

    const QVector<Language> supportedSourceLangs = m_translator->supportedSourceLanguages();
    const QVector<Language> supportedDestLangs = m_translator->supportedDestinationLanguages();

    ui->sourceLanguagesWidget->setSupportedLanguages(supportedSourceLangs);
    ui->translationLanguagesWidget->setSupportedLanguages(supportedDestLangs);

    validateLanguageSupport();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveMainWindowSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::ttsStateChanged(QTextToSpeech::State newState)
{
    switch (newState) {
    case QTextToSpeech::Ready:
        ui->sourcePlayPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->translationPlayPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
        break;
    case QTextToSpeech::Speaking:
        ui->sourcePlayPauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
        ui->translationPlayPauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
        break;
    case QTextToSpeech::Paused:
        ui->sourcePlayPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->translationPlayPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
        break;
    case QTextToSpeech::Error:
    case QTextToSpeech::Synthesizing:
        break;
    }

    updateTTSButtonStates();
}

void MainWindow::onTTSError(QTextToSpeech::ErrorReason reason, const QString &errorString)
{
    qDebug() << "MainWindow::onTTSError - reason:" << reason << "error:" << errorString;

    // Show error dialog to user without resetting TTS provider
    QMessageBox::warning(this, tr("TTS Error"), tr("Text-to-Speech error:\n\n%1\n\nPlease check your TTS settings or try a different voice/locale.").arg(errorString));
}

void MainWindow::translatorStateChanged(ATranslationProvider::State newState)
{
    qDebug() << "MainWindow::translatorStateChanged - newState:" << static_cast<int>(newState);
    updateTranslateButtonState();
    ui->abortButton->setEnabled(newState == ATranslationProvider::State::Processing);
    switch (newState) {
    case ATranslationProvider::State::Ready:
    case ATranslationProvider::State::Processing:
        break;
    case ATranslationProvider::State::Processed:
        if (m_translator->error == ATranslationProvider::TranslationError::NoError) {
            if (!m_translator->result.isEmpty()) {
                ui->translationEdit->setHtml(m_translator->result);

                showTranslationWindow();
            }

            emit translationAccepted();
        } else {
            Q_UNREACHABLE();
            ui->translationEdit->setHtml("Error: " + m_translator->getErrorString());
        }

        break;
    case ATranslationProvider::State::Finished:
        if (m_translator->error != ATranslationProvider::TranslationError::NoError) {
            ui->translationEdit->setHtml("Error: " + m_translator->getErrorString());
        }
        emit resetTranslator();
        break;
    }
}

void MainWindow::setOrientation(Qt::ScreenOrientation orientation)
{
    if (orientation == Qt::PrimaryOrientation)
        orientation = screen()->orientation();

    switch (orientation) {
    case Qt::LandscapeOrientation:
    case Qt::InvertedLandscapeOrientation:
        ui->centralLayout->setDirection(QBoxLayout::LeftToRight);
        ui->translationButtonsLayout->setDirection(QBoxLayout::LeftToRight);
        ui->translationLanguagesWidget->setLayoutDirection(Qt::RightToLeft);
        break;
    case Qt::PortraitOrientation:
        ui->centralLayout->setDirection(QBoxLayout::TopToBottom);
        ui->translationButtonsLayout->setDirection(QBoxLayout::RightToLeft);
        ui->translationLanguagesWidget->setLayoutDirection(Qt::LeftToRight);
        break;
    case Qt::InvertedPortraitOrientation:
        ui->centralLayout->setDirection(QBoxLayout::BottomToTop);
        ui->translationButtonsLayout->setDirection(QBoxLayout::RightToLeft);
        ui->translationLanguagesWidget->setLayoutDirection(Qt::LeftToRight);
        break;
    default:
        Q_UNREACHABLE(); // Will never be called with Qt::PrimaryOrientation
    }
}

void MainWindow::onSourceLanguageChanged(int id)
{
    m_sourceLang = ui->sourceLanguagesWidget->language(id);
    updateTTSButtonStates();
    updateVoiceComboBoxes();
}

void MainWindow::onDestinationLanguageChanged(int id)
{
    m_destLang = ui->translationLanguagesWidget->language(id);
    updateTTSButtonStates();
    updateVoiceComboBoxes();
}

void MainWindow::validateLanguageSupport()
{
    if (m_translator == nullptr) {
        return;
    }

    const QVector<Language> supportedSourceLangs = m_translator->supportedSourceLanguages();
    const QVector<Language> supportedDestLangs = m_translator->supportedDestinationLanguages();

    bool sourceSupported = false;
    for (const Language &language : supportedSourceLangs) {
        if (language == m_sourceLang || (m_sourceLang != Language::autoLanguage() && language.hasQLocaleEquivalent() && m_sourceLang.hasQLocaleEquivalent() && language.toQLocale().language() == m_sourceLang.toQLocale().language())) {
            sourceSupported = true;
            break;
        }
    }

    bool destSupported = false;
    for (const Language &language : supportedDestLangs) {
        if (language == m_destLang || (m_destLang != Language::autoLanguage() && language.hasQLocaleEquivalent() && m_destLang.hasQLocaleEquivalent() && language.toQLocale().language() == m_destLang.toQLocale().language())) {
            destSupported = true;
            break;
        }
    }

    if (!sourceSupported) {
        m_sourceLang = QLocale::c();
        ui->sourceLanguagesWidget->checkAutoButton();
    }

    if (!destSupported) {
        if (!supportedDestLangs.isEmpty()) {
            m_destLang = supportedDestLangs.first();
            ui->translationLanguagesWidget->checkLanguage(m_destLang);
        }
    }
}

void MainWindow::swapTranslator(ATranslationProvider::ProviderBackend newBackend)
{
    if (m_chosenTranslationBackend == newBackend) {
        return;
    }

    if (m_translator != nullptr) {
        disconnect(m_translator, &ATranslationProvider::stateChanged, this, &MainWindow::translatorStateChanged);
        disconnect(this, &MainWindow::translationRequested, this, &MainWindow::handleTranslationRequest);
        disconnect(this, &MainWindow::translationAccepted, m_translator, &ATranslationProvider::finish);
        disconnect(this, &MainWindow::resetTranslator, m_translator, &ATranslationProvider::reset);

        m_translator->deleteLater();
    }

    m_chosenTranslationBackend = newBackend;
    m_translator = ATranslationProvider::createTranslationProvider(this, m_chosenTranslationBackend);

    updateProviderUI();

    connect(m_translator, &ATranslationProvider::stateChanged, this, &MainWindow::translatorStateChanged);
    connect(this, &MainWindow::translationRequested, this, &MainWindow::handleTranslationRequest);
    connect(this, &MainWindow::translationRequested, this, [](const QString &text, const Language &destLang, const Language &srcLang) {
        qDebug() << "MainWindow::translationRequested - text:" << text.left(50) << "srcLang:" << srcLang.name() << "destLang:" << destLang.name();
    });
    connect(this, &MainWindow::translationAccepted, m_translator, &ATranslationProvider::finish);
    connect(this, &MainWindow::resetTranslator, m_translator, &ATranslationProvider::reset);

    applyTranslationProviderSettings();

    // Connect to abstract provider signals
    connect(m_translator, &ATranslationProvider::engineChanged, this, [this]() {
        refreshLanguageWidgetsWithSupportedLanguages();
    });
    connect(m_translator, &ATranslationProvider::languageDetected, this, [this](const Language &detectedLanguage, bool isTranslationContext) {
        if (ui->sourceLanguagesWidget->isAutoButtonChecked()) {
            m_sourceLang = detectedLanguage;
            ui->sourceLanguagesWidget->setAutoLanguage(detectedLanguage);
        }

        // If translation auto button is checked, re-evaluate destination language with detected source
        // Only retranslate if: (translation context) OR (auto-translate is enabled)
        if (ui->translationLanguagesWidget->isAutoButtonChecked() && (isTranslationContext || ui->autoTranslateCheckBox->isChecked())) {
            const Language preferredDest = preferredTranslationLanguage(detectedLanguage);
            qDebug() << "Language detected:" << detectedLanguage.name() << "isTranslationContext:" << isTranslationContext
                     << "preferred destination:" << preferredDest.name();

            // Check if the current translation target matches our preferred destination
            // If not, retranslate with the correct destination language
            if (m_translator && m_translator->getState() == ATranslationProvider::State::Processed) {
                const Language currentDestLang = m_translator->translationLanguage;
                if (currentDestLang != preferredDest && !ui->sourceEdit->toPlainText().isEmpty()) {
                    qDebug() << "Retranslating from" << detectedLanguage.name() << "to preferred destination:" << preferredDest.name();

                    // Update the destination language state and UI
                    m_destLang = preferredDest;
                    ui->translationLanguagesWidget->setAutoLanguage(preferredDest);

                    // Update voice comboboxes for the new destination language
                    updateVoiceComboBoxes();

                    m_translator->translate(ui->sourceEdit->toSourceText(), preferredDest, detectedLanguage);
                }
            }
        }
    });

    setupEngineComboBoxConnection();
    refreshLanguageWidgetsWithSupportedLanguages();
}

void MainWindow::swapTTSProvider(ATTSProvider::ProviderBackend newBackend)
{
    if (m_chosenTTSBackend == newBackend) {
        return;
    }

    if (m_tts != nullptr) {
        disconnect(m_tts, &ATTSProvider::stateChanged, this, &MainWindow::ttsStateChanged);

        m_tts->deleteLater();
    }

    m_chosenTTSBackend = newBackend;
    m_tts = ATTSProvider::createTTSProvider(this, m_chosenTTSBackend);

    connect(m_tts, &ATTSProvider::stateChanged, this, &MainWindow::ttsStateChanged);
    connect(m_tts, &ATTSProvider::errorOccurred, this, &MainWindow::onTTSError);

    applyTTSProviderSettings();
    updateProviderUI();
    updateVoiceComboBoxes();

    updateTTSButtonStates();
}

void MainWindow::handleAutoTranslation()
{
    if (ui->autoTranslateCheckBox->isChecked() && (m_translator != nullptr) && m_translator->getState() == ATranslationProvider::State::Ready
        && !ui->sourceEdit->toPlainText().isEmpty()) {
        const bool isSourceAutoChecked = ui->sourceLanguagesWidget->isAutoButtonChecked();
        const bool isTranslationAutoChecked = ui->translationLanguagesWidget->isAutoButtonChecked();
        const Language sourceLanguage = isSourceAutoChecked ? Language::autoLanguage() : m_sourceLang;
        const Language destinationLanguage = isTranslationAutoChecked ? Language::autoLanguage() : m_destLang;

        emit translationRequested(ui->sourceEdit->toSourceText(), destinationLanguage, sourceLanguage);
    }
}

void MainWindow::updateAutoLocales()
{
    if ((m_translator == nullptr) || !m_translator->supportsAutodetection()) {
        return;
    }

    // Only do standalone language detection if auto-translation is disabled
    // If auto-translation is enabled, the translation itself will detect the language
    if (ui->autoTranslateCheckBox->isChecked()) {
        return;
    }

    const QString sourceText = ui->sourceEdit->toPlainText();
    if (sourceText.isEmpty()) {
        return;
    }

    // If translator is ready, detect immediately
    if (m_translator->getState() == ATranslationProvider::State::Ready) {
        m_translator->detectLanguage(sourceText);
        return;
    }

    // If translator is not ready, set up one-time connection to retry when ready
    qDebug() << "MainWindow::updateAutoLocales - translator not ready, setting up retry connection, state:" << static_cast<int>(m_translator->getState());
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_translator, &ATranslationProvider::stateChanged, this, [this, sourceText, connection](ATranslationProvider::State newState) {
        if (newState == ATranslationProvider::State::Ready) {
            qDebug() << "MainWindow::updateAutoLocales - retry connection triggered, detecting language";
            // Check text hasn't changed since we set up the connection
            if (ui->sourceEdit->toPlainText() == sourceText && !ui->autoTranslateCheckBox->isChecked()) {
                m_translator->detectLanguage(sourceText);
            }
            disconnect(*connection);
        }
    });
}

void MainWindow::updateTTSButtonStates()
{
    if (m_tts == nullptr) {
        ui->sourcePlayPauseButton->setEnabled(false);
        ui->sourceStopButton->setEnabled(false);
        ui->translationPlayPauseButton->setEnabled(false);
        ui->translationStopButton->setEnabled(false);
        return;
    }

    const Language sourceLanguage = m_sourceLang != Language::autoLanguage() ? m_sourceLang : Language(QLocale::system());
    const Language translationLanguage = m_destLang != Language::autoLanguage() ? m_destLang : Language(QLocale::system());

    const bool sourceAvailable = isTTSAvailableForLanguage(sourceLanguage);
    const bool translationAvailable = isTTSAvailableForLanguage(translationLanguage);

    ui->sourcePlayPauseButton->setEnabled(sourceAvailable && !ui->sourceEdit->toPlainText().isEmpty());
    ui->sourceStopButton->setEnabled(sourceAvailable);
    ui->translationPlayPauseButton->setEnabled(translationAvailable && !ui->translationEdit->toPlainText().isEmpty());
    ui->translationStopButton->setEnabled(translationAvailable);
}

void MainWindow::updateTranslateButtonState()
{
    const bool hasText = !ui->sourceEdit->toPlainText().isEmpty();
    const bool translatorReady = (m_translator != nullptr) && m_translator->getState() == ATranslationProvider::State::Ready;
    const bool translatorProcessing = (m_translator != nullptr) && m_translator->getState() == ATranslationProvider::State::Processing;
    ui->translateButton->setEnabled(hasText && translatorReady);
    ui->abortButton->setEnabled(translatorProcessing);
}

void MainWindow::setupEngineComboBoxConnection()
{
    connect(ui->engineComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_translator) {
            const ProviderUIRequirements reqs = m_translator->getUIRequirements();
            if (reqs.supportedCapabilities.contains("engineSelection")) {
                // Save to settings using type-safe method
                m_translator->saveOptionToSettings("engine", index);

                // Apply to provider using options system
                auto options = std::make_unique<ProviderOptions>();
                options->setOption("engine", index);
                m_translator->applyOptions(*options);
            }
        }
    });
}

void MainWindow::updateProviderUI()
{
    // Update translation provider UI elements
    if (m_translator != nullptr) {
        const ProviderUIRequirements translationReqs = m_translator->getUIRequirements();
        ui->engineComboBox->setVisible(translationReqs.requiredUIElements.contains("engineComboBox"));

        // Set current engine selection for providers that support it
        if (translationReqs.supportedCapabilities.contains("engineSelection")) {
            const AppSettings settings;
            ui->engineComboBox->setCurrentIndex(static_cast<int>(settings.currentEngine()));
        }
    }

    // Update TTS provider UI elements
    if (m_tts != nullptr) {
        const ProviderUIRequirements ttsReqs = m_tts->getUIRequirements();
        ui->sourceVoiceComboBox->setVisible(ttsReqs.requiredUIElements.contains("sourceVoiceComboBox"));
        ui->translationVoiceComboBox->setVisible(ttsReqs.requiredUIElements.contains("translationVoiceComboBox"));
        ui->sourceSpeakerComboBox->setVisible(ttsReqs.requiredUIElements.contains("sourceSpeakerComboBox"));
        ui->translationSpeakerComboBox->setVisible(ttsReqs.requiredUIElements.contains("translationSpeakerComboBox"));
    }
}

void MainWindow::updateVoiceComboBoxes()
{
    if (m_tts == nullptr) {
        ui->sourceVoiceComboBox->clear();
        ui->translationVoiceComboBox->clear();
        ui->sourceSpeakerComboBox->clear();
        ui->translationSpeakerComboBox->clear();
        return;
    }

    const ProviderUIRequirements reqs = m_tts->getUIRequirements();
    if (!reqs.supportedCapabilities.contains("voiceSelection")) {
        ui->sourceVoiceComboBox->clear();
        ui->translationVoiceComboBox->clear();
        return;
    }

    ui->sourceVoiceComboBox->blockSignals(true);
    ui->translationVoiceComboBox->blockSignals(true);

    Voice currentSourceVoice, currentTranslationVoice;
    if (ui->sourceVoiceComboBox->currentData().isValid()) {
        currentSourceVoice = ui->sourceVoiceComboBox->currentData().value<Voice>();
    }
    if (ui->translationVoiceComboBox->currentData().isValid()) {
        currentTranslationVoice = ui->translationVoiceComboBox->currentData().value<Voice>();
    }

    Language sourceLanguage = ui->sourceLanguagesWidget->checkedLanguage();
    if (sourceLanguage == Language::autoLanguage()) {
        const QString sourceText = ui->sourceEdit->toPlainText();
        if (!sourceText.isEmpty() && (m_translator != nullptr) && m_translator->supportsAutodetection()) {
            sourceLanguage = m_translator->detectLanguage(sourceText);
        } else {
            sourceLanguage = Language(QLocale::system());
        }
    }
    const QList<Voice> sourceVoices = m_tts->findVoices(sourceLanguage);

    ui->sourceVoiceComboBox->clear();
    int sourceVoiceIndex = -1;
    for (int i = 0; i < sourceVoices.size(); ++i) {
        const Voice &voice = sourceVoices[i];
        ui->sourceVoiceComboBox->addItem(voice.name(), QVariant::fromValue(voice));
        if (voice == currentSourceVoice) {
            sourceVoiceIndex = i;
        }
    }
    if (sourceVoiceIndex >= 0) {
        ui->sourceVoiceComboBox->setCurrentIndex(sourceVoiceIndex);
    } else if (!sourceVoices.isEmpty()) {
        ui->sourceVoiceComboBox->setCurrentIndex(0);
    }

    Language translationLanguage = ui->translationLanguagesWidget->checkedLanguage();
    if (translationLanguage == Language::autoLanguage()) {
        translationLanguage = Language(QLocale::system());
    }
    const QList<Voice> translationVoices = m_tts->findVoices(translationLanguage);

    ui->translationVoiceComboBox->clear();
    int translationVoiceIndex = -1;
    for (int i = 0; i < translationVoices.size(); ++i) {
        const Voice &voice = translationVoices[i];
        ui->translationVoiceComboBox->addItem(voice.name(), QVariant::fromValue(voice));
        if (voice == currentTranslationVoice) {
            translationVoiceIndex = i;
        }
    }
    if (translationVoiceIndex >= 0) {
        ui->translationVoiceComboBox->setCurrentIndex(translationVoiceIndex);
    } else if (!translationVoices.isEmpty()) {
        ui->translationVoiceComboBox->setCurrentIndex(0);
    }

    ui->sourceVoiceComboBox->blockSignals(false);
    ui->translationVoiceComboBox->blockSignals(false);

    updateSpeakerComboBoxes();
}

void MainWindow::updateSpeakerComboBoxes()
{
    if (m_tts == nullptr) {
        ui->sourceSpeakerComboBox->clear();
        ui->translationSpeakerComboBox->clear();
        return;
    }

    const ProviderUIRequirements reqs = m_tts->getUIRequirements();
    if (!reqs.supportedCapabilities.contains("speakerSelection")) {
        ui->sourceSpeakerComboBox->clear();
        ui->translationSpeakerComboBox->clear();
        return;
    }

    ui->sourceSpeakerComboBox->blockSignals(true);
    ui->translationSpeakerComboBox->blockSignals(true);

    const QString currentSourceSpeaker = ui->sourceSpeakerComboBox->currentText();
    const QString currentTranslationSpeaker = ui->translationSpeakerComboBox->currentText();

    // Get speakers for source and translation voices independently
    QStringList sourceSpeakers;
    QStringList translationSpeakers;

    if (ui->sourceVoiceComboBox->currentData().isValid()) {
        Voice sourceVoice = ui->sourceVoiceComboBox->currentData().value<Voice>();
        sourceSpeakers = m_tts->availableSpeakersForVoice(sourceVoice);
    } else {
        sourceSpeakers = QStringList() << "default";
    }

    if (ui->translationVoiceComboBox->currentData().isValid()) {
        Voice translationVoice = ui->translationVoiceComboBox->currentData().value<Voice>();
        translationSpeakers = m_tts->availableSpeakersForVoice(translationVoice);
    } else {
        translationSpeakers = QStringList() << "default";
    }

    // Populate source speaker combo box
    ui->sourceSpeakerComboBox->clear();
    int sourceIndex = -1;
    for (int i = 0; i < sourceSpeakers.size(); ++i) {
        const QString &speaker = sourceSpeakers[i];
        ui->sourceSpeakerComboBox->addItem(speaker);
        if (speaker == currentSourceSpeaker) {
            sourceIndex = i;
        }
    }
    if (sourceIndex >= 0) {
        ui->sourceSpeakerComboBox->setCurrentIndex(sourceIndex);
    } else if (!sourceSpeakers.isEmpty()) {
        ui->sourceSpeakerComboBox->setCurrentIndex(0);
    }

    // Populate translation speaker combo box
    ui->translationSpeakerComboBox->clear();
    int translationIndex = -1;
    for (int i = 0; i < translationSpeakers.size(); ++i) {
        const QString &speaker = translationSpeakers[i];
        ui->translationSpeakerComboBox->addItem(speaker);
        if (speaker == currentTranslationSpeaker) {
            translationIndex = i;
        }
    }
    if (translationIndex >= 0) {
        ui->translationSpeakerComboBox->setCurrentIndex(translationIndex);
    } else if (!translationSpeakers.isEmpty()) {
        ui->translationSpeakerComboBox->setCurrentIndex(0);
    }

    ui->sourceSpeakerComboBox->blockSignals(false);
    ui->translationSpeakerComboBox->blockSignals(false);
}

bool MainWindow::isTTSAvailableForLanguage(const Language &language) const
{
    if (m_tts == nullptr) {
        return false;
    }

    const QList<Language> availableLanguages = m_tts->availableLanguages();

    if (availableLanguages.contains(language)) {
        return true;
    }

    for (const Language &availableLanguage : availableLanguages) {
        if (availableLanguage.hasQLocaleEquivalent() && language.hasQLocaleEquivalent() && availableLanguage.toQLocale().language() == language.toQLocale().language()) {
            return true;
        }
    }

    return false;
}

void MainWindow::sourcePlayPauseClicked()
{
    if (m_tts == nullptr) {
        return;
    }

    const QString text = ui->sourceEdit->toPlainText();
    if (text.isEmpty()) {
        return;
    }

    const Language sourceLanguage = m_sourceLang != Language::autoLanguage() ? m_sourceLang : Language(QLocale::system());
    m_tts->setLanguage(sourceLanguage);
    const ProviderUIRequirements reqs = m_tts->getUIRequirements();
    if (reqs.supportedCapabilities.contains("voiceSelection") && ui->sourceVoiceComboBox->currentData().isValid()) {
        const Voice voice = ui->sourceVoiceComboBox->currentData().value<Voice>();
        m_tts->setVoice(voice);
    }

    // Apply speaker selection for source TTS
    if (reqs.supportedCapabilities.contains("speakerSelection") && ui->sourceSpeakerComboBox->count() > 0) {
        const QString speaker = ui->sourceSpeakerComboBox->currentText();
        if (!speaker.isEmpty()) {
            auto options = m_optionsManager->createTTSOptionsFromSettings(m_tts);
            if (options) {
                options->setOption("speaker", speaker);
                m_tts->applyOptions(*options);
            }
        }
    }

    if (m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->pause();
    } else if (m_tts->state() == QTextToSpeech::Paused) {
        m_tts->resume();
    } else {
        m_tts->say(text);
    }
}

void MainWindow::sourceStopClicked()
{
    if (m_tts != nullptr) {
        m_tts->stop();
    }
}

void MainWindow::translationPlayPauseClicked()
{
    if (m_tts == nullptr) {
        return;
    }

    const QString text = ui->translationEdit->toPlainText();
    if (text.isEmpty()) {
        return;
    }

    const Language translationLanguage = m_destLang != Language::autoLanguage() ? m_destLang : Language(QLocale::system());
    m_tts->setLanguage(translationLanguage);
    const ProviderUIRequirements reqs = m_tts->getUIRequirements();
    if (reqs.supportedCapabilities.contains("voiceSelection") && ui->translationVoiceComboBox->currentData().isValid()) {
        const Voice voice = ui->translationVoiceComboBox->currentData().value<Voice>();
        m_tts->setVoice(voice);
    }

    // Apply speaker selection for translation TTS
    if (reqs.supportedCapabilities.contains("speakerSelection") && ui->translationSpeakerComboBox->count() > 0) {
        const QString speaker = ui->translationSpeakerComboBox->currentText();
        if (!speaker.isEmpty()) {
            auto options = m_optionsManager->createTTSOptionsFromSettings(m_tts);
            if (options) {
                options->setOption("speaker", speaker);
                m_tts->applyOptions(*options);
            }
        }
    }

    if (m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->pause();
    } else if (m_tts->state() == QTextToSpeech::Paused) {
        m_tts->resume();
    } else {
        m_tts->say(text);
    }
}

void MainWindow::translationStopClicked()
{
    if (m_tts != nullptr) {
        m_tts->stop();
    }
}

void MainWindow::on_translateButton_clicked()
{
    assert(m_translator != nullptr);
    assert(m_translator->getState()
           == ATranslationProvider::State::Ready); // should be initialized and ready or something is wrong with button enabling logic and not here

    const bool isSourceAutoChecked = ui->sourceLanguagesWidget->isAutoButtonChecked();
    const bool isTranslationAutoChecked = ui->translationLanguagesWidget->isAutoButtonChecked();
    const Language sourceLanguage = isSourceAutoChecked ? Language::autoLanguage() : m_sourceLang;
    const Language destinationLanguage = isTranslationAutoChecked ? Language::autoLanguage() : m_destLang;

    emit translationRequested(ui->sourceEdit->toSourceText(), destinationLanguage, sourceLanguage);
}

void MainWindow::on_swapButton_clicked()
{
    LanguageButtonsWidget::swapCurrentLanguages(ui->sourceLanguagesWidget, ui->translationLanguagesWidget);

    m_sourceLang = ui->sourceLanguagesWidget->checkedLanguage();
    m_destLang = ui->translationLanguagesWidget->checkedLanguage();

    const QString sourceText = ui->sourceEdit->toPlainText();
    const QString translationText = ui->translationEdit->toPlainText();

    ui->sourceEdit->setPlainText(translationText);
    ui->translationEdit->setPlainText(sourceText);

    updateTTSButtonStates();
}

void MainWindow::on_abortButton_clicked()
{
    emit resetTranslator();
}

void MainWindow::on_copySourceButton_clicked()
{
    const QString sourceText = ui->sourceEdit->toPlainText();
    if (!sourceText.isEmpty()) {
        QApplication::clipboard()->setText(sourceText);
    }
}

void MainWindow::on_copyTranslationButton_clicked()
{
    const QString translationText = ui->translationEdit->toPlainText();
    if (!translationText.isEmpty()) {
        QApplication::clipboard()->setText(translationText);
    }
}

void MainWindow::on_copyAllTranslationButton_clicked()
{
    const QString sourceText = ui->sourceEdit->toPlainText();
    const QString translationText = ui->translationEdit->toPlainText();

    if (!sourceText.isEmpty() || !translationText.isEmpty()) {
        QString allText;
        if (!sourceText.isEmpty()) {
            allText += sourceText + "\n";
        }
        if (!translationText.isEmpty()) {
            allText += translationText;
        }
        QApplication::clipboard()->setText(allText);
    }
}

void MainWindow::on_clearButton_clicked()
{
    ui->sourceEdit->clear();
    ui->translationEdit->clear();
}

void MainWindow::on_delayedRecognizeScreenAreaButton_clicked()
{
    delayedRecognizeScreenArea();
}

void MainWindow::on_delayedTranslateScreenAreaButton_clicked()
{
    delayedTranslateScreenArea();
}

Language MainWindow::preferredTranslationLanguage(const Language &sourceLang) const
{
    const AppSettings settings;
    Language primaryLang = settings.primaryLanguage();
    if (primaryLang == Language::autoLanguage())
        primaryLang = Language(QLocale::system());

    // First choice: use primary if different from source
    if (primaryLang != sourceLang)
        return primaryLang;

    // Primary same as source, try secondary
    Language secondaryLang = settings.secondaryLanguage();
    if (secondaryLang == Language::autoLanguage())
        secondaryLang = Language(QLocale::system());

    if (secondaryLang != sourceLang)
        return secondaryLang;

    // Both primary and secondary same as source, fall back to system language
    return Language(QLocale::system());
}

void MainWindow::handleTranslationRequest(const QString &text, const Language &destLang, const Language &srcLang)
{
    Language actualDestLang = destLang;

    // If destination is auto, determine preferred translation language
    if (destLang == Language::autoLanguage()) {
        if (srcLang != Language::autoLanguage()) {
            // Source is known, use preferred translation logic immediately
            actualDestLang = preferredTranslationLanguage(srcLang);
        } else {
            // Source is auto too - we'll rely on language detection signal to retranslate if needed
            // For now, use system locale as fallback
            actualDestLang = preferredTranslationLanguage(Language(QLocale::system()));
        }
    }

    qDebug() << "MainWindow::handleTranslationRequest - srcLang:" << srcLang.name()
             << "destLang:" << destLang.name() << "actualDest:" << actualDestLang.name();

    m_translator->translate(text, actualDestLang, srcLang);
}
