/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "modulestatus.h"
#include "popupwindow.h"
#include "provideroptions.h"
#include "provideroptionsmanager.h"
#include "screenwatcher.h"
#include "selection.h"
#include "singleapplication.h"
#include "statusstrip.h"
#include "core/translationsession.h"
#include "core/usernotifier.h"
#include "gui/translationhtmlformatter.h"
#include "ocr/aocrprovider.h"
#include "ocr/llmocr.h"
#include "ocr/screengrabbers/abstractscreengrabber.h"
#include "ocr/tesseractocr.h"
#include "settings/appsettings.h"
#include "settings/settingsdialog.h"
#include "translator/atranslationprovider.h"
#include "translator/languageresolution.h"
#include "tts/attsprovider.h"
#include "tts/voice.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QSet>
#include <QStandardPaths>
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
    , m_session(new TranslationSession(this))
    , m_screenCaptureTimer(new QTimer(this))
    , m_snippingArea(new SnippingArea(this))
    , m_trayIcon(new TrayIcon(this))
    , m_orientationWatcher(new ScreenWatcher(this))
    , m_screenGrabber(AbstractScreenGrabber::createScreenGrabber(this))
{
    // Before anything that can notify is constructed. Delivery is queued, so
    // a provider built further down this constructor can still report a
    // failed start and have it reach the user.
    connect(UserNotifier::instance(), &UserNotifier::notified, this, &MainWindow::showNotification);

    ui->setupUi(this);

    // The status strip lives in the status bar rather than centralLayout:
    // setOrientation() reassigns that layout's direction per screen
    // orientation, which would place the strip beside the columns in
    // landscape and invert its position in InvertedPortraitOrientation.
    m_statusStrip = new StatusStrip;
    m_statusStrip->setModel(m_session->moduleStatus());
    ui->statusbar->addWidget(m_statusStrip, 1);
    // Save original Mozhi engine combo items so they can be restored after
    // switching to LocalAI (which clears the combo).
    for (int i = 0; i < ui->engineComboBox->count(); ++i) {
        m_engineItemsText.append(ui->engineComboBox->itemText(i));
        m_engineItemsData.append(ui->engineComboBox->itemData(i).toString());
        m_engineItemsIcons.append(ui->engineComboBox->itemIcon(i));
    }

    // Screen orientation
    connect(m_orientationWatcher, &ScreenWatcher::screenOrientationChanged, this, &MainWindow::setOrientation);

    // Show the main window if a secondary instance has been started
    connect(qobject_cast<SingleApplication *>(QCoreApplication::instance()), &SingleApplication::instanceStarted, this, &MainWindow::open);

    // Selection requests
    connect(&Selection::instance(), &Selection::requestedSelectionAvailable, ui->sourceEdit, &SourceTextEdit::replaceText);
    connect(&Selection::instance(), &Selection::windowActivationNeeded, this, &MainWindow::showTranslationWindow);
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
    connect(m_snippingArea, &SnippingArea::snipped, this, [this](const QPixmap &pixmap, int dpi) {
        m_session->ocr()->recognize(pixmap.toImage(), dpi);
    });
    // An abandoned snip must not leave a translation armed for whatever gets
    // recognized next.
    connect(m_snippingArea, &SnippingArea::cancelled, m_session, &TranslationSession::disarmOcrTranslation);
    // A failed grab never opens the snipping area, so it emits no SnippingArea
    // signal either - the same armed-connection leak as an abandoned snip.
    connect(m_screenGrabber, &AbstractScreenGrabber::grabbingFailed, m_session, &TranslationSession::disarmOcrTranslation);
    connect(m_session->tesseractOcr(), &AOcrProvider::recognized, ui->sourceEdit, &SourceTextEdit::replaceText);
    connect(m_session->llmOcr(), &AOcrProvider::recognized, ui->sourceEdit, &SourceTextEdit::replaceText);
    // Text inserted through replaceText() deliberately suppresses
    // SourceTextEdit::textEdited (the armed flows chain their own follow-up),
    // so an UNarmed recognition - a plain recognize-screen-area, or an image
    // with auto-translate off - used to leave the translate button disabled
    // and language detection not running until the user typed something:
    // any real edit fires textEdited, which drives all of these. Run the
    // same follow-ups typed text gets. These permanent connections are
    // created before any armed/temporary one, so the gate below sees the
    // current armed state correctly.
    const auto handleRecognizedText = [this]() {
        if (m_session->isOcrTranslationArmed())
            return; // the armed path chains its own translation
        updateTranslateButtonState();
        updateTTSButtonStates();
        updateAutoLocales();
        handleAutoTranslation();
    };
    connect(m_session->tesseractOcr(), &AOcrProvider::recognized, this, handleRecognizedText);
    connect(m_session->llmOcr(), &AOcrProvider::recognized, this, handleRecognizedText);
    m_screenCaptureTimer->setSingleShot(true);

    // The session binds its own two recognition engines; the grabber and the
    // snipping area belong to whatever owns a screen, which is this window.
    m_session->moduleStatus()->bindCapture(m_screenGrabber, m_snippingArea);
    connect(m_session, &TranslationSession::ocrLanguagesUnavailable, this, [this](const QString &languages) {
        m_trayIcon->showMessage(TesseractOcr::tr("Unable to set OCR languages"),
                                TesseractOcr::tr("Unable to initialize Tesseract with %1").arg(languages));
    });

    loadAppSettings();

    // Subscribed before the providers exist: building them below reports
    // through exactly these signals, and none of them has to be re-made when
    // a backend swap replaces the provider objects.
    connect(m_session, &TranslationSession::translatorChanged, this, &MainWindow::onTranslatorChanged);
    connect(m_session, &TranslationSession::ttsProviderChanged, this, &MainWindow::onTtsProviderChanged);
    connect(m_session, &TranslationSession::translationStateChanged, this, &MainWindow::translatorStateChanged);
    connect(m_session, &TranslationSession::ttsStateChanged, this, &MainWindow::ttsStateChanged);
    connect(m_session, &TranslationSession::ttsErrorOccurred, this, &MainWindow::onTTSError);
    connect(m_session, &TranslationSession::languageDetected, this, &MainWindow::onLanguageDetected);
    connect(m_session, &TranslationSession::engineChanged, this, &MainWindow::refreshLanguageWidgetsWithSupportedLanguages);
    connect(m_session, &TranslationSession::translationStarted, this, [](const QString &text, const Language &destLang, const Language &srcLang) {
        qDebug() << "TranslationSession::translationStarted - text:" << text.left(50) << "srcLang:" << srcLang.name() << "destLang:" << destLang.name();
    });
    // An armed recognition chains its own translation; the delayed textEdited
    // this would otherwise fire would make a second one.
    connect(m_session, &TranslationSession::ocrTranslationChaining, this, [this]() {
        ui->sourceEdit->stopEditTimer();
    });
    connect(this, &MainWindow::translationRequested, this, [this](const QString &text, const Language &destLang, const Language &srcLang) {
        m_session->requestTranslation(text, destLang, srcLang);
    });
    connect(this, &MainWindow::translationAccepted, m_session, &TranslationSession::acceptTranslation);
    connect(this, &MainWindow::resetTranslator, m_session, &TranslationSession::resetTranslator);

    m_session->loadBackendsFromSettings();

    connect(ui->sourcePlayPauseButton, &QToolButton::clicked, this, &MainWindow::sourcePlayPauseClicked);
    connect(ui->sourceStopButton, &QToolButton::clicked, this, &MainWindow::sourceStopClicked);
    connect(ui->translationPlayPauseButton, &QToolButton::clicked, this, &MainWindow::translationPlayPauseClicked);
    connect(ui->translationStopButton, &QToolButton::clicked, this, &MainWindow::translationStopClicked);
    connect(ui->settingsButton, &QToolButton::clicked, this, &MainWindow::openSettings);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::updateTranslateButtonState);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::updateTTSButtonStates);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::updateAutoLocales);
    connect(ui->sourceEdit, &SourceTextEdit::textEdited, this, &MainWindow::handleAutoTranslation);

    ui->sourceEdit->setListenForEdits(true);
    ui->sourceEdit->viewport()->setAcceptDrops(true);
    ui->sourceEdit->viewport()->installEventFilter(this);
    ui->sourceEdit->installEventFilter(this);

    // Image preview overlay — child of sourceEdit's parent, positioned manually
    m_imagePreview = new QLabel(ui->sourceEdit->parentWidget());
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setStyleSheet(QStringLiteral("QLabel{background:#1e1e1e;border:1px solid #555;border-radius:4px}"));
    m_imagePreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imagePreview->hide();
    ui->sourceEdit->parentWidget()->installEventFilter(this);

    connect(ui->sourceVoiceComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_session->tts() && ui->sourceVoiceComboBox->currentData().isValid()) {
            const ProviderCapabilities reqs = m_session->tts()->capabilities();
            if (reqs.testFlag(ProviderCapability::VoiceSelection)) {
                const Voice voice = ui->sourceVoiceComboBox->currentData().value<Voice>();
                const Language sourceLanguage = spokenSourceLanguage();
                m_session->tts()->setLanguage(sourceLanguage);
                m_session->tts()->setVoice(voice);
                updateSpeakerComboBoxes();
            }
        }
    });

    connect(ui->translationVoiceComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_session->tts() && ui->translationVoiceComboBox->currentData().isValid()) {
            const ProviderCapabilities reqs = m_session->tts()->capabilities();
            if (reqs.testFlag(ProviderCapability::VoiceSelection)) {
                const Voice voice = ui->translationVoiceComboBox->currentData().value<Voice>();
                const Language translationLanguage = spokenTranslationLanguage();
                m_session->tts()->setLanguage(translationLanguage);
                m_session->tts()->setVoice(voice);
                updateSpeakerComboBoxes();
            }
        }
    });

    connect(ui->sourceSpeakerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_session->tts() && ui->sourceSpeakerComboBox->count() > 0) {
            const ProviderCapabilities reqs = m_session->tts()->capabilities();
            if (reqs.testFlag(ProviderCapability::SpeakerSelection)) {
                const QString speaker = ui->sourceSpeakerComboBox->currentText();
                if (!speaker.isEmpty()) {
                    auto options = m_session->options()->createTTSOptionsFromSettings(m_session->tts());
                    if (options) {
                        options->setOption("speaker", speaker);
                        m_session->tts()->applyOptions(*options);
                    }
                }
            }
        }
    });

    connect(ui->translationSpeakerComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_session->tts() && ui->translationSpeakerComboBox->count() > 0) {
            const ProviderCapabilities reqs = m_session->tts()->capabilities();
            if (reqs.testFlag(ProviderCapability::SpeakerSelection)) {
                const QString speaker = ui->translationSpeakerComboBox->currentText();
                if (!speaker.isEmpty()) {
                    auto options = m_session->options()->createTTSOptionsFromSettings(m_session->tts());
                    if (options) {
                        options->setOption("speaker", speaker);
                        m_session->tts()->applyOptions(*options);
                    }
                }
            }
        }
    });

    setupEngineComboBoxConnection();

    connect(ui->translationEdit, &QTextEdit::textChanged, this, &MainWindow::updateTTSButtonStates);

    loadMainWindowSettings();

    updateTranslateButtonState();

    refreshLanguageWidgetsWithSupportedLanguages();

    updateVoiceComboBoxes();

    connect(ui->sourceLanguagesWidget, &LanguageButtonsWidget::languagesChanged, this, [](const QVector<Language> &languages) {
        AppSettings().setLanguages(AppSettings::Source, languages);
    });
    connect(ui->translationLanguagesWidget, &LanguageButtonsWidget::languagesChanged, this, [](const QVector<Language> &languages) {
        AppSettings().setLanguages(AppSettings::Translation, languages);
    });

    // Everything that displays or speaks a resolved language follows the model
    // rather than working it out again. This is the connection that replaces
    // the hand-written updates each of them used to need.
    connect(m_session->languages(), &LanguageResolution::changed, this, [this]() {
        ui->translationLanguagesWidget->setAutoLanguage(m_session->languages()->effectiveDestination());
        updateVoiceComboBoxes();
        updateTTSButtonStates();
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
    disconnect(m_popupDestroyedConnection);
    delete ui;
}

AOcrProvider *MainWindow::ocr() const
{
    return m_session->ocr();
}

TesseractOcr *MainWindow::tesseractOcr() const
{
    return m_session->tesseractOcr();
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

QToolButton *MainWindow::translateButton() const
{
    return ui->translateButton;
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

ModuleStatus *MainWindow::moduleStatus() const
{
    return m_session->moduleStatus();
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

                if (m_session->translator() && m_session->translator()->getState() == ATranslationProvider::State::Ready) {
                    emit translationRequested(selectedText, destinationLanguage, sourceLanguage);
                } else {
                    // Wait for translator to be ready
                    auto connection = std::make_shared<QMetaObject::Connection>();
                    *connection = connect(m_session->translator(),
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

    // Selection class handles Wayland-specific window activation internally
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
            if (!selectedText.isEmpty() && m_session->tts()) {
                const Language sourceLanguage = spokenSourceLanguage();
                m_session->tts()->setLanguage(sourceLanguage);
                m_session->tts()->say(selectedText);
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
    if (!translationText.isEmpty() && (m_session->tts() != nullptr)) {
        const Language translationLanguage = spokenTranslationLanguage();
        m_session->tts()->setLanguage(translationLanguage);
        m_session->tts()->say(translationText);
    }
}

Q_SCRIPTABLE void MainWindow::stopSpeaking()
{
    if (m_session->tts() != nullptr) {
        m_session->tts()->stop();
    }
}

Q_SCRIPTABLE void MainWindow::playPauseSpeaking()
{
    if (m_session->tts() == nullptr) {
        return;
    }

    if (m_session->tts()->state() == QTextToSpeech::Speaking) {
        m_session->tts()->pause();
    } else if (m_session->tts()->state() == QTextToSpeech::Paused) {
        m_session->tts()->resume();
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

// Whether the engine can run at all is the session's question, and it reports
// its own answer to the user. What is left here is the part that is genuinely
// about this window: forcing the language buttons back to auto.
bool MainWindow::prepareOcr()
{
    if (!m_session->prepareOcr()) {
        return false;
    }

    const AppSettings settings;
    if (settings.isForceSourceAutodetect()) {
        ui->sourceLanguagesWidget->checkAutoButton();
    }
    if (settings.isForceTranslationAutodetect()) {
        ui->translationLanguagesWidget->checkAutoButton();
    }
    return true;
}

Q_SCRIPTABLE void MainWindow::recognizeScreenArea()
{
    if (!prepareOcr()) {
        return;
    }

    if (m_screenGrabber != nullptr) {
        m_session->disarmOcrTranslation();
        startScreenCapture();
    }
}

Q_SCRIPTABLE void MainWindow::translateScreenArea()
{
    if (!prepareOcr()) {
        return;
    }

    if (m_screenGrabber != nullptr) {
        m_session->armOcrTranslation(m_session->ocr());
        startScreenCapture();
    }
}

Q_SCRIPTABLE void MainWindow::delayedRecognizeScreenArea()
{
    if (!prepareOcr()) {
        return;
    }

    if ((m_screenCaptureTimer != nullptr) && (m_screenGrabber != nullptr)) {
        const AppSettings settings;
        m_screenCaptureTimer->start(settings.captureDelay());
        connect(
            m_screenCaptureTimer,
            &QTimer::timeout,
            this,
            [this]() {
                m_session->disarmOcrTranslation();
                startScreenCapture();
            },
            Qt::SingleShotConnection);
    }
}

Q_SCRIPTABLE void MainWindow::delayedTranslateScreenArea()
{
    if (!prepareOcr()) {
        return;
    }

    if ((m_screenCaptureTimer != nullptr) && (m_screenGrabber != nullptr)) {
        const AppSettings settings;
        m_screenCaptureTimer->start(settings.captureDelay());
        connect(
            m_screenCaptureTimer,
            &QTimer::timeout,
            this,
            [this]() {
                m_session->armOcrTranslation(m_session->ocr());
                startScreenCapture();
            },
            Qt::SingleShotConnection);
    }
}

Q_SCRIPTABLE void MainWindow::clearText()
{
    ui->sourceEdit->removeText();
    ui->translationEdit->clear();
}

void MainWindow::startScreenCapture()
{
    if (m_screenGrabber == nullptr)
        return;

    m_session->moduleStatus()->beginScreenCapture();
    m_screenGrabber->grab();
}

Q_SCRIPTABLE void MainWindow::openSettings()
{
    SettingsDialog config(this);
    connect(&config, &SettingsDialog::translationBackendChanged, m_session, &TranslationSession::setTranslationBackend);
    connect(&config, &SettingsDialog::ttsBackendChanged, m_session, &TranslationSession::setTtsBackend);

    // Let ProviderOptionsManager handle all provider-specific settings changes
    m_session->options()->connectToSettingsDialog(&config, m_session->tts());
    connect(m_session->options(), &ProviderOptionsManager::ttsProviderUIUpdateRequired, this, [this]() {
        updateVoiceComboBoxes();
        updateSpeakerComboBoxes();
    });
    if (config.exec() == QDialog::Accepted) {
        loadAppSettings();
        // The availability check that runs in here can move the stored TTS
        // choice - a backend the settings name but this build cannot provide.
        // Re-reading both backends applies that; it is a no-op when neither
        // moved, so the common case still costs nothing.
        m_session->loadBackendsFromSettings();
        m_session->applyTranslationOptions();
    }
}

Q_SCRIPTABLE void MainWindow::quit()
{
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
}

void MainWindow::showNotification(const UserNotifier::Notification &notification)
{
    // The core says what happened; deciding that it looks like a message box
    // is this frontend's business and nobody else's.
    static const QHash<UserNotifier::Severity, QMessageBox::Icon> icons = {
        {UserNotifier::Severity::Information, QMessageBox::Information},
        {UserNotifier::Severity::Warning, QMessageBox::Warning},
        {UserNotifier::Severity::Critical, QMessageBox::Critical},
    };

    auto *box = new QMessageBox(icons.value(notification.severity, QMessageBox::Information),
                                notification.title,
                                notification.text,
                                QMessageBox::Ok,
                                this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    if (!notification.details.isEmpty()) {
        box->setTextFormat(notification.detailsAreRichText ? Qt::RichText : Qt::PlainText);
        box->setInformativeText(notification.details);
    }

    // Only the ones that say they must be acknowledged block. The rest used
    // to call exec() too, which meant a missing voice model could freeze the
    // window from inside a provider's constructor.
    if (notification.requiresAcknowledgement)
        box->exec();
    else
        box->open();
}

void MainWindow::showTranslationWindow()
{
    qDebug() << "showTranslationWindow: isHidden=" << isHidden() << "windowMode=" << m_windowMode;

    // Always show main window if it already opened
    if (!isHidden()) {
        qDebug() << "showTranslationWindow: calling open() because window not hidden";
        open();
        return;
    }

    switch (m_windowMode) {
    case AppSettings::PopupWindow: {
        // Check if a popup window already exists and is visible
        for (QObject *child : children()) {
            if (auto *existingPopup = qobject_cast<PopupWindow *>(child)) {
                if (existingPopup->isVisible()) {
                    qDebug() << "showTranslationWindow: popup already visible, activating it";
                    existingPopup->activateWindow();
                    return;
                }
            }
        }

        qDebug() << "showTranslationWindow: creating popup window";
        auto *popup = new PopupWindow(this);

        // Connect popup ready signal to Selection for Wayland clipboard access
        connect(popup, &PopupWindow::windowReady, &Selection::instance(), &Selection::onWindowReady, Qt::UniqueConnection);

        popup->show();
        popup->activateWindow();

        // Force listening for changes in source field
        if (!m_listenForContentChanges) {
            setListenForContentChanges(true);
            // Stored and explicitly disconnected in ~MainWindow() before
            // `delete ui` - see m_popupDestroyedConnection's declaration for
            // why the `this` context argument alone isn't sufficient here.
            disconnect(m_popupDestroyedConnection);
            m_popupDestroyedConnection = connect(popup, &PopupWindow::destroyed, this, [this] {
                setListenForContentChanges(ui->autoTranslateCheckBox->isChecked());
            });
        }

        break;
    }
    case AppSettings::MainWindow:
        qDebug() << "showTranslationWindow: calling open() for main window mode";
        open();
        break;
    case AppSettings::Notification:
        qDebug() << "showTranslationWindow: notification mode, not showing window";
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
    publishLanguageSelection();

    // "Auto" is represented by the C locale (Language::autoLanguage() ==
    // QLocale::c()), so only fall back to the system locale when no language
    // is actually selected - not when the user has the auto (primary/secondary)
    // button checked. Resetting auto to the system locale here is what left
    // the destination stuck on the primary language.
    if (m_destLang == QLocale::c() && !ui->translationLanguagesWidget->isAutoButtonChecked()) {
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
    // De-identify BEFORE hiding the status bar: the AT-SPI bridge ignores
    // property updates for widgets in an already-hidden hierarchy, so the
    // strip's labels would keep announcing their last name indefinitely.
    m_statusStrip->setShown(settings.isShowStatusBar());
    ui->statusbar->setVisible(settings.isShowStatusBar());

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
    ui->sourceEdit->setSimplifySource(settings.isSimplifySource());

    m_session->initTesseractFromSettings();
    if (const AppSettings::RegionRememberType type = settings.regionRememberType(); m_snippingArea->regionRememberType() != type) {
        m_snippingArea->setRegionRememberType(type);
        if (type == AppSettings::RememberAlways)
            m_snippingArea->setCropRegion(settings.cropRegion());
    }
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

void MainWindow::refreshLanguageWidgetsWithSupportedLanguages()
{
    if (m_session->translator() == nullptr) {
        return;
    }

    const QVector<Language> supportedSourceLangs = m_session->translator()->supportedSourceLanguages();
    const QVector<Language> supportedDestLangs = m_session->translator()->supportedDestinationLanguages();

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
#ifdef BUILD_TESTING
    emit ttsStateChangedSignal(newState);
#endif

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
#ifdef BUILD_TESTING
    emit translatorStateChangedSignal(newState);
#endif
    updateTranslateButtonState();
    ui->abortButton->setEnabled(newState == ATranslationProvider::State::Processing);
    switch (newState) {
    case ATranslationProvider::State::Ready:
    case ATranslationProvider::State::Processing:
        break;
    case ATranslationProvider::State::Processed:
        if (m_session->translator()->error == ATranslationProvider::TranslationError::NoError) {
            if (!m_session->translator()->result.isEmpty()) {
                ui->translationEdit->setHtml(TranslationHtmlFormatter::format(m_session->translator()->result));

                showTranslationWindow();
            }

            // The destination "auto" resolves to is only known once the
            // translation is done. The voice combos were populated for
            // whatever language was current before, and a QVoice carries its
            // own locale - so applying a stale English voice on play would
            // override the correct language and speak Russian in English.
            refreshVoicesForSpokenLanguages();

            emit translationAccepted();
        } else {
            ui->translationEdit->setPlainText(tr("Error: %1").arg(m_session->translator()->getErrorString()));
        }

        break;
    case ATranslationProvider::State::Finished:
        if (m_session->translator()->error != ATranslationProvider::TranslationError::NoError) {
            ui->translationEdit->setPlainText(tr("Error: %1").arg(m_session->translator()->getErrorString()));
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
    publishLanguageSelection();
    updateTTSButtonStates();
}

void MainWindow::onDestinationLanguageChanged(int id)
{
    m_destLang = ui->translationLanguagesWidget->language(id);
    publishLanguageSelection();
    updateTTSButtonStates();
}

void MainWindow::validateLanguageSupport()
{
    if (m_session->translator() == nullptr) {
        return;
    }

    const QVector<Language> supportedSourceLangs = m_session->translator()->supportedSourceLanguages();
    const QVector<Language> supportedDestLangs = m_session->translator()->supportedDestinationLanguages();

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

    if (!destSupported && m_destLang != Language::autoLanguage()) {
        if (!supportedDestLangs.isEmpty()) {
            m_destLang = supportedDestLangs.first();
            ui->translationLanguagesWidget->checkLanguage(m_destLang);
            publishLanguageSelection();
        }
    }
}

// The session rebuilt the translation provider. Everything that displays it
// is rebuilt from scratch here, because none of it can be carried over: the
// old provider object is gone.
void MainWindow::onTranslatorChanged()
{
    updateProviderUI();
    refreshLanguageWidgetsWithSupportedLanguages();
}

void MainWindow::onTtsProviderChanged()
{
    updateProviderUI();
    updateVoiceComboBoxes();
    updateSpeakerComboBoxes();
    updateTTSButtonStates();
}

void MainWindow::handleAutoTranslation()
{
    if (ui->autoTranslateCheckBox->isChecked() && (m_session->translator() != nullptr) && m_session->translator()->getState() == ATranslationProvider::State::Ready
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
    if ((m_session->translator() == nullptr) || !m_session->translator()->supportsAutodetection()) {
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
    if (m_session->translator()->getState() == ATranslationProvider::State::Ready) {
        m_session->translator()->detectLanguage(sourceText);
        return;
    }

    // If translator is not ready, set up one-time connection to retry when ready
    qDebug() << "MainWindow::updateAutoLocales - translator not ready, setting up retry connection, state:" << static_cast<int>(m_session->translator()->getState());
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_session->translator(), &ATranslationProvider::stateChanged, this, [this, sourceText, connection](ATranslationProvider::State newState) {
        if (newState == ATranslationProvider::State::Ready) {
            qDebug() << "MainWindow::updateAutoLocales - retry connection triggered, detecting language";
            // Check text hasn't changed since we set up the connection
            if (ui->sourceEdit->toPlainText() == sourceText && !ui->autoTranslateCheckBox->isChecked()) {
                m_session->translator()->detectLanguage(sourceText);
            }
            disconnect(*connection);
        }
    });
}

void MainWindow::updateTTSButtonStates()
{
    if (m_session->tts() == nullptr) {
        ui->sourcePlayPauseButton->setEnabled(false);
        ui->sourceStopButton->setEnabled(false);
        ui->translationPlayPauseButton->setEnabled(false);
        ui->translationStopButton->setEnabled(false);
        return;
    }

    const Language sourceLanguage = spokenSourceLanguage();
    const Language translationLanguage = spokenTranslationLanguage();

    const bool sourceAvailable = isTTSAvailableForLanguage(sourceLanguage);
    const bool translationAvailable = isTTSAvailableForLanguage(translationLanguage);

    ui->sourcePlayPauseButton->setEnabled(sourceAvailable && !ui->sourceEdit->toPlainText().isEmpty());
    ui->sourceStopButton->setEnabled(sourceAvailable);
    ui->translationPlayPauseButton->setEnabled(translationAvailable && !ui->translationEdit->toPlainText().isEmpty());
    ui->translationStopButton->setEnabled(translationAvailable);
}

void MainWindow::updateTranslateButtonState()
{
    const bool hasContent = !ui->sourceEdit->toPlainText().isEmpty() || m_hasSourceImage;
    const bool translatorReady = (m_session->translator() != nullptr) && m_session->translator()->getState() == ATranslationProvider::State::Ready;
    const bool translatorProcessing = (m_session->translator() != nullptr) && m_session->translator()->getState() == ATranslationProvider::State::Processing;
    ui->translateButton->setEnabled(hasContent && translatorReady);
    ui->abortButton->setEnabled(translatorProcessing);
}

// Made once. It used to be re-made on every backend swap, which left the
// previous lambda attached to the same combo box - the provider is read out
// of the session when the signal fires, so one connection has always been
// enough.
void MainWindow::setupEngineComboBoxConnection()
{
    connect(ui->engineComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_session->translator()) {
            const ProviderCapabilities reqs = m_session->translator()->capabilities();
            if (reqs.testFlag(ProviderCapability::EngineSelection)) {
                // Save to settings using type-safe method
                m_session->translator()->saveOptionToSettings("engine", index);

                // Apply to provider using options system
                auto options = std::make_unique<ProviderOptions>();
                options->setOption("engine", index);
                m_session->translator()->applyOptions(*options);
            } else if (reqs.testFlag(ProviderCapability::ProviderSelection)) {
                // LocalAI: the combo lists local providers (Ollama/FastFlowLM/LM Studio).
                const QString providerId = ui->engineComboBox->itemData(index).toString();
                if (!providerId.isEmpty()) {
                    AppSettings settings;
                    settings.setActiveLocalProvider(providerId);
                    // Re-apply so URL/model/prompt follow the chosen provider.
                    m_session->applyTranslationOptions();
                }
            }
        }
    });
}

void MainWindow::updateProviderUI()
{
    // Update translation provider UI elements
    if (m_session->translator() != nullptr) {
        const ProviderCapabilities translationReqs = m_session->translator()->capabilities();
        // Which control a capability implies is this window's business - the
        // backend used to be asked for widget names, which meant it had to
        // know this layout.
        ui->engineComboBox->setVisible(translationReqs.testFlag(ProviderCapability::EngineSelection)
                                       || translationReqs.testFlag(ProviderCapability::ProviderSelection));

        // Set current engine selection for providers that support it
        if (translationReqs.testFlag(ProviderCapability::EngineSelection)) {
            const AppSettings settings;
            // We're in the Mozhi-style engineSelection branch, so the combo
            // must show Mozhi's engines regardless of what it held before
            // (e.g. LocalAI's provider list, if we just switched back).
            if (!m_engineItemsText.isEmpty()) {
                QSignalBlocker blocker(ui->engineComboBox);
                ui->engineComboBox->clear();
                for (int i = 0; i < m_engineItemsText.size(); ++i) {
                    ui->engineComboBox->addItem(m_engineItemsIcons.at(i),
                                                m_engineItemsText.at(i),
                                                m_engineItemsData.at(i));
                }
            }
            ui->engineComboBox->setCurrentIndex(static_cast<int>(settings.currentEngine()));
        } else if (translationReqs.testFlag(ProviderCapability::ProviderSelection)) {
            // LocalAI: populate the combo with local providers, select the active one.
            const AppSettings settings;
            QSignalBlocker blocker(ui->engineComboBox);
            ui->engineComboBox->clear();
            const QStringList providerIds = AppSettings::localProviderIds();
            for (const QString &id : providerIds) {
                ui->engineComboBox->addItem(AppSettings::localProviderDisplayName(id), id);
            }
            const int idx = ui->engineComboBox->findData(settings.activeLocalProvider());
            if (idx >= 0) {
                ui->engineComboBox->setCurrentIndex(idx);
            }
            widenComboPopup(ui->engineComboBox);
        }
    }

    // Update TTS provider UI elements
    if (m_session->tts() != nullptr) {
        const ProviderCapabilities ttsReqs = m_session->tts()->capabilities();
        const bool hasVoices = ttsReqs.testFlag(ProviderCapability::VoiceSelection);
        const bool hasSpeakers = ttsReqs.testFlag(ProviderCapability::SpeakerSelection);
        ui->sourceVoiceComboBox->setVisible(hasVoices);
        ui->translationVoiceComboBox->setVisible(hasVoices);
        ui->sourceSpeakerComboBox->setVisible(hasSpeakers);
        ui->translationSpeakerComboBox->setVisible(hasSpeakers);

        // Hide playback controls entirely when the provider has no TTS UI (e.g. None)
        const bool hasVoiceUI = hasVoices || hasSpeakers;
        ui->sourcePlayPauseButton->setVisible(hasVoiceUI);
        ui->sourceStopButton->setVisible(hasVoiceUI);
        ui->translationPlayPauseButton->setVisible(hasVoiceUI);
        ui->translationStopButton->setVisible(hasVoiceUI);
    }
#ifndef WITH_TTS
    // Built without TTS support: no provider other than "None" exists, hide all TTS UI
    for (QWidget *w : {static_cast<QWidget *>(ui->sourcePlayPauseButton),
                       static_cast<QWidget *>(ui->sourceStopButton),
                       static_cast<QWidget *>(ui->translationPlayPauseButton),
                       static_cast<QWidget *>(ui->translationStopButton),
                       static_cast<QWidget *>(ui->sourceVoiceComboBox),
                       static_cast<QWidget *>(ui->translationVoiceComboBox),
                       static_cast<QWidget *>(ui->sourceSpeakerComboBox),
                       static_cast<QWidget *>(ui->translationSpeakerComboBox)}) {
        w->setVisible(false);
        w->setEnabled(false);
    }
#endif
}

void MainWindow::refreshVoicesForSpokenLanguages()
{
    if (m_session->tts() == nullptr) {
        return;
    }
    if (spokenSourceLanguage() == m_voiceComboSourceLang && spokenTranslationLanguage() == m_voiceComboTranslationLang) {
        return;
    }
    updateVoiceComboBoxes();
}

void MainWindow::updateVoiceComboBoxes()
{
    // Recorded before any early return: the combos reflect these languages
    // now, whether that meant filling them or clearing them.
    m_voiceComboSourceLang = spokenSourceLanguage();
    m_voiceComboTranslationLang = spokenTranslationLanguage();

    if (m_session->tts() == nullptr) {
        ui->sourceVoiceComboBox->clear();
        ui->translationVoiceComboBox->clear();
        ui->sourceSpeakerComboBox->clear();
        ui->translationSpeakerComboBox->clear();
        return;
    }

    const ProviderCapabilities reqs = m_session->tts()->capabilities();
    if (!reqs.testFlag(ProviderCapability::VoiceSelection)) {
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

    // Offer voices for the language speech will actually use, which for a
    // checked "auto" is whatever detection found rather than the locale.
    const Language sourceLanguage = spokenSourceLanguage();
    const QList<Voice> sourceVoices = m_session->tts()->findVoices(sourceLanguage);

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

    const Language translationLanguage = spokenTranslationLanguage();
    const QList<Voice> translationVoices = m_session->tts()->findVoices(translationLanguage);

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
    if (m_session->tts() == nullptr) {
        ui->sourceSpeakerComboBox->clear();
        ui->translationSpeakerComboBox->clear();
        return;
    }

    const ProviderCapabilities reqs = m_session->tts()->capabilities();
    if (!reqs.testFlag(ProviderCapability::SpeakerSelection)) {
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
        sourceSpeakers = m_session->tts()->availableSpeakersForVoice(sourceVoice);
    } else {
        sourceSpeakers = QStringList() << "default";
    }

    if (ui->translationVoiceComboBox->currentData().isValid()) {
        Voice translationVoice = ui->translationVoiceComboBox->currentData().value<Voice>();
        translationSpeakers = m_session->tts()->availableSpeakersForVoice(translationVoice);
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
    if (m_session->tts() == nullptr) {
        return false;
    }

    const QList<Language> availableLanguages = m_session->tts()->availableLanguages();

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
    if (m_session->tts() == nullptr) {
        return;
    }

    const QString text = ui->sourceEdit->toPlainText();
    if (text.isEmpty()) {
        return;
    }

    const Language sourceLanguage = spokenSourceLanguage();
    m_session->tts()->setLanguage(sourceLanguage);
    const ProviderCapabilities reqs = m_session->tts()->capabilities();
    if (reqs.testFlag(ProviderCapability::VoiceSelection) && ui->sourceVoiceComboBox->currentData().isValid()) {
        const Voice voice = ui->sourceVoiceComboBox->currentData().value<Voice>();
        m_session->tts()->setVoice(voice);
    }

    // Apply speaker selection for source TTS
    if (reqs.testFlag(ProviderCapability::SpeakerSelection) && ui->sourceSpeakerComboBox->count() > 0) {
        const QString speaker = ui->sourceSpeakerComboBox->currentText();
        if (!speaker.isEmpty()) {
            auto options = m_session->options()->createTTSOptionsFromSettings(m_session->tts());
            if (options) {
                options->setOption("speaker", speaker);
                m_session->tts()->applyOptions(*options);
            }
        }
    }

    if (m_session->tts()->state() == QTextToSpeech::Speaking) {
        m_session->tts()->pause();
    } else if (m_session->tts()->state() == QTextToSpeech::Paused) {
        m_session->tts()->resume();
    } else {
        m_session->tts()->say(text);
    }
}

void MainWindow::sourceStopClicked()
{
    if (m_session->tts() != nullptr) {
        m_session->tts()->stop();
    }
}

void MainWindow::translationPlayPauseClicked()
{
    if (m_session->tts() == nullptr) {
        return;
    }

    const QString text = ui->translationEdit->toPlainText();
    if (text.isEmpty()) {
        return;
    }

    const Language translationLanguage = spokenTranslationLanguage();
    m_session->tts()->setLanguage(translationLanguage);
    const ProviderCapabilities reqs = m_session->tts()->capabilities();
    if (reqs.testFlag(ProviderCapability::VoiceSelection) && ui->translationVoiceComboBox->currentData().isValid()) {
        const Voice voice = ui->translationVoiceComboBox->currentData().value<Voice>();
        m_session->tts()->setVoice(voice);
    }

    // Apply speaker selection for translation TTS
    if (reqs.testFlag(ProviderCapability::SpeakerSelection) && ui->translationSpeakerComboBox->count() > 0) {
        const QString speaker = ui->translationSpeakerComboBox->currentText();
        if (!speaker.isEmpty()) {
            auto options = m_session->options()->createTTSOptionsFromSettings(m_session->tts());
            if (options) {
                options->setOption("speaker", speaker);
                m_session->tts()->applyOptions(*options);
            }
        }
    }

    if (m_session->tts()->state() == QTextToSpeech::Speaking) {
        m_session->tts()->pause();
    } else if (m_session->tts()->state() == QTextToSpeech::Paused) {
        m_session->tts()->resume();
    } else {
        m_session->tts()->say(text);
    }
}

void MainWindow::translationStopClicked()
{
    if (m_session->tts() != nullptr) {
        m_session->tts()->stop();
    }
}

void MainWindow::on_translateButton_clicked()
{
    assert(m_session->translator() != nullptr);
    assert(m_session->translator()->getState()
           == ATranslationProvider::State::Ready); // should be initialized and ready or something is wrong with button enabling logic and not here

    const bool isSourceAutoChecked = ui->sourceLanguagesWidget->isAutoButtonChecked();
    const bool isTranslationAutoChecked = ui->translationLanguagesWidget->isAutoButtonChecked();
    const Language sourceLanguage = isSourceAutoChecked ? Language::autoLanguage() : m_sourceLang;
    const Language destinationLanguage = isTranslationAutoChecked ? Language::autoLanguage() : m_destLang;

    if (m_hasSourceImage) {
        // Recognition is still running on the image; translate whatever it
        // produces rather than translating an empty source edit.
        m_session->armOcrTranslation(m_session->ocr());
        return;
    }

    emit translationRequested(ui->sourceEdit->toSourceText(), destinationLanguage, sourceLanguage);
}

void MainWindow::on_swapButton_clicked()
{
    if (m_hasSourceImage) {
        const QString translationText = ui->translationEdit->toPlainText();
        clearSourceImage();
        ui->sourceEdit->setPlainText(translationText);
        ui->translationEdit->clear();
        LanguageButtonsWidget::swapCurrentLanguages(ui->sourceLanguagesWidget, ui->translationLanguagesWidget);
        m_sourceLang = ui->sourceLanguagesWidget->checkedLanguage();
        m_destLang = ui->translationLanguagesWidget->checkedLanguage();
        publishLanguageSelection();
        updateTranslateButtonState();
        updateTTSButtonStates();
        return;
    }

    LanguageButtonsWidget::swapCurrentLanguages(ui->sourceLanguagesWidget, ui->translationLanguagesWidget);

    m_sourceLang = ui->sourceLanguagesWidget->checkedLanguage();
    m_destLang = ui->translationLanguagesWidget->checkedLanguage();
    publishLanguageSelection();

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
    if (m_hasSourceImage) {
        clearSourceImage();
        ui->translationEdit->clear();
        return;
    }
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

void MainWindow::publishLanguageSelection()
{
    // Auto has to be reported AS auto. checkedLanguage() answers with the auto
    // button's label, which is a resolved language once anything has resolved
    // - feeding that back in reads as "the user picked this" and pins the
    // destination, which is the bug this class exists to prevent, arriving
    // through its own front door.
    const Language source = ui->sourceLanguagesWidget->isAutoButtonChecked()
        ? Language::autoLanguage()
        : ui->sourceLanguagesWidget->checkedLanguage();
    const Language destination = ui->translationLanguagesWidget->isAutoButtonChecked()
        ? Language::autoLanguage()
        : ui->translationLanguagesWidget->checkedLanguage();
    m_session->setSelectedLanguages(source, destination);
}

// The session has already told the language model what was detected; this is
// only what the window does about it.
void MainWindow::onLanguageDetected(const Language &detectedLanguage, bool isTranslationContext)
{
    if (ui->sourceLanguagesWidget->isAutoButtonChecked()) {
        m_sourceLang = detectedLanguage;
        ui->sourceLanguagesWidget->setAutoLanguage(detectedLanguage);
    }

    if (!ui->translationLanguagesWidget->isAutoButtonChecked() || !(isTranslationContext || ui->autoTranslateCheckBox->isChecked())) {
        return;
    }

    // Retranslate only when the destination the detection implies is not where
    // the translation already went. The label is not updated here: the model
    // moved above, and whoever shows it is subscribed.
    if (m_session->translator() == nullptr || m_session->translator()->getState() != ATranslationProvider::State::Processed) {
        return;
    }
    const Language preferredDest = m_session->preferredDestination(detectedLanguage);
    if (m_session->translator()->translationLanguage != preferredDest && !ui->sourceEdit->toPlainText().isEmpty()) {
        qDebug() << "Retranslating from" << detectedLanguage.name() << "to preferred destination:" << preferredDest.name();
        m_session->translator()->translate(ui->sourceEdit->toSourceText(), preferredDest, detectedLanguage);
    }
}

Language MainWindow::spokenSourceLanguage() const
{
    return m_session->languages()->effectiveSource();
}

Language MainWindow::spokenTranslationLanguage() const
{
    return m_session->languages()->effectiveDestination();
}

void MainWindow::setSourceImageInternal(const QImage &src)
{
    QImage img = src;

    if (img.width() * img.height() > AppSettings::maxSourceImagePixels) {
        ui->sourceEdit->setPlainText(tr("Image too large (%1 MP). Max %2 MP.")
                                         .arg(img.width() * img.height() / 1000000.0, 0, 'f', 0)
                                         .arg(AppSettings::maxSourceImagePixels / 1000000.0, 0, 'f', 0));
        return;
    }

    if (img.width() > AppSettings::maxSourceImageDimension || img.height() > AppSettings::maxSourceImageDimension) {
        img = img.scaled(AppSettings::maxSourceImageDimension, AppSettings::maxSourceImageDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_pendingOcrImage = img;

    // Show preview over sourceEdit using geometry
    m_originalPixmap = QPixmap::fromImage(img);
    const QRect geo = ui->sourceEdit->geometry();
    m_imagePreview->setGeometry(geo);
    m_imagePreview->setPixmap(m_originalPixmap.scaled(geo.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imagePreview->show();
    m_hasSourceImage = true;
    ui->sourceLanguagesWidget->setEnabled(false);

    updateTranslateButtonState();

    recognizeSourceImage();
}

void MainWindow::clearSourceImage()
{
    if (!m_hasSourceImage) {
        return;
    }
    // Recognition now starts the moment an image arrives, so dropping the
    // image while it is still running has to stop it too - otherwise Clear or
    // Swap leaves a request in flight that lands its text (and possibly a
    // translation) in a source edit the user has already emptied. The
    // recognized/failed/canceled handlers disconnect themselves before calling
    // this, so a still-connected handler is exactly the "aborted early" case.
    if (m_imageOcrRecognizedConnection) {
        disconnect(m_imageOcrRecognizedConnection);
        disconnect(m_imageOcrCanceledConnection);
        disconnect(m_imageOcrFailedConnection);
        m_session->disarmOcrTranslation();
        m_session->ocr()->cancel();
    }

    m_hasSourceImage = false;
    m_pendingOcrImage = QImage();
    m_imagePreview->hide();
    m_originalPixmap = QPixmap();
    ui->sourceLanguagesWidget->setEnabled(true);
}

// An image that arrives by drop, paste or the open button is transcribed
// straight away, the same way a screen-area capture is: the point of dropping
// an image is to see its text. Whether that text then gets translated is the
// auto-translate setting's business, exactly as it is for typed text.
void MainWindow::recognizeSourceImage()
{
    if (!prepareOcr()) {
        clearSourceImage();
        return;
    }

    AOcrProvider *engine = m_session->ocr();

    disconnect(m_imageOcrRecognizedConnection);
    disconnect(m_imageOcrCanceledConnection);
    disconnect(m_imageOcrFailedConnection);

    if (ui->autoTranslateCheckBox->isChecked()) {
        m_session->armOcrTranslation(engine);
    } else {
        m_session->disarmOcrTranslation();
    }

    // The recognized text itself lands in the source edit through the
    // permanent recognized -> replaceText connection; these only manage the
    // preview overlay that the text replaces.
    m_imageOcrRecognizedConnection = connect(engine, &AOcrProvider::recognized, this, [this]() {
        disconnect(m_imageOcrRecognizedConnection);
        disconnect(m_imageOcrCanceledConnection);
        disconnect(m_imageOcrFailedConnection);
        clearSourceImage();
    });
    m_imageOcrCanceledConnection = connect(engine, &AOcrProvider::canceled, this, [this]() {
        disconnect(m_imageOcrRecognizedConnection);
        disconnect(m_imageOcrCanceledConnection);
        disconnect(m_imageOcrFailedConnection);
        m_session->disarmOcrTranslation();
        clearSourceImage();
        updateTranslateButtonState();
    });
    m_imageOcrFailedConnection = connect(engine, &AOcrProvider::failed, this, [this](const QString &error) {
        disconnect(m_imageOcrRecognizedConnection);
        disconnect(m_imageOcrCanceledConnection);
        disconnect(m_imageOcrFailedConnection);
        m_session->disarmOcrTranslation();
        clearSourceImage();
        ui->sourceEdit->replaceText(tr("OCR failed: %1").arg(error));
        updateTranslateButtonState();
    });

    engine->recognize(m_pendingOcrImage, 96);
}

// Loads an image file, retrying through a PNG repair pass for the files Qt
// refuses because of non-standard ancillary chunks (screenshots from some
// tools carry them). Shared by drag-and-drop and the open-image button.
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Resize preview overlay when parent resizes
    if (obj == ui->sourceEdit->parentWidget() && event->type() == QEvent::Resize && m_hasSourceImage) {
        const QRect geo = ui->sourceEdit->geometry();
        m_imagePreview->setGeometry(geo);
        if (!m_originalPixmap.isNull()) {
            m_imagePreview->setPixmap(m_originalPixmap.scaled(geo.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    if (obj == ui->sourceEdit->viewport()) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *de = static_cast<QDropEvent *>(event);
            if (de->mimeData()->hasUrls() || de->mimeData()->hasImage()) {
                de->acceptProposedAction();
                return true;
            }
        }
        if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            QImage img;
            if (drop->mimeData()->hasUrls()) {
                const QString path = drop->mimeData()->urls().constFirst().toLocalFile();
                if (!m_session->loadImage(path, img)) {
                    ui->sourceEdit->replaceText(tr("Cannot open image:\n%1").arg(path));
                    return true;
                }
            }
            if (img.isNull() && drop->mimeData()->hasImage()) {
                img = qvariant_cast<QImage>(drop->mimeData()->imageData());
            }
            if (img.isNull()) {
                return true;
            }
            setSourceImageInternal(img);
            return true;
        }
    }
    if (obj == ui->sourceEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            // Ctrl+V paste image from clipboard
            if (ke->matches(QKeySequence::Paste)) {
                const QClipboard *clip = QApplication::clipboard();
                if (clip->mimeData()->hasImage()) {
                    QImage img = qvariant_cast<QImage>(clip->mimeData()->imageData());
                    if (!img.isNull()) {
                        setSourceImageInternal(img);
                        return true;
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_hasSourceImage) {
        // clearSourceImage() cancels the engine that is actually running.
        // Cancelling both unconditionally used to make LlmOcr::cancel() emit a
        // spurious canceled() for a Tesseract run (and vice versa).
        clearSourceImage();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::on_openImageButton_clicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Open image"),
                                                      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                      tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QImage img;
    if (!m_session->loadImage(path, img)) {
        QMessageBox::warning(this, tr("Open image"), tr("Cannot open image:\n%1").arg(path));
        return;
    }
    setSourceImageInternal(img);
}
