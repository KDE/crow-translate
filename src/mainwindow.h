/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "language.h"
#include "qhotkey.h"
#include "screenwatcher.h"
#include "trayicon.h"
#include "ocr/aocrprovider.h"
#include "ocr/screengrabbers/abstractscreengrabber.h"
#include "ocr/snippingarea.h"
#include "ocr/tesseractocr.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QComboBox>
#include <QIcon>
#include <QKeySequence>
#include <QLocale>
#include <QMainWindow>
#include <QShortcut>
#include <QTextEdit>
#include <QTextToSpeech>
#include <QToolButton>

class LanguageButtonsWidget;
class PopupWindow;
class SourceTextEdit;
class ProviderOptionsManager;
class ModuleStatus;
class StatusStrip;
class QLabel;
class AOcrProvider;
class LlmOcr;
class TesseractOcr;

namespace Ui
{
class MainWindow;
} // namespace Ui

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", APPLICATION_ID ".MainWindow")
    Q_DISABLE_COPY(MainWindow)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    AOcrProvider *ocr() const;
    TesseractOcr *tesseractOcr() const;
    QComboBox *getEngineComboBox() const;
    QComboBox *sourceVoiceComboBox() const;
    QComboBox *translationVoiceComboBox() const;
    QComboBox *sourceSpeakerComboBox() const;
    QComboBox *translationSpeakerComboBox() const;
    LanguageButtonsWidget *sourceLanguageButtons() const;
    LanguageButtonsWidget *translationLanguageButtons() const;
    QToolButton *copyTranslationButton() const;
    QToolButton *swapButton() const;
    QToolButton *translateButton() const;
    QToolButton *copySourceButton() const;
    QToolButton *copyAllTranslationButton() const;
    QTextEdit *translationEdit() const;
    SourceTextEdit *sourceEdit() const;
    ModuleStatus *moduleStatus() const;
    QToolButton *sourcePlayPauseButton() const;
    QToolButton *sourceStopButton() const;
    QToolButton *translationPlayPauseButton() const;
    QToolButton *translationStopButton() const;
    QKeySequence closeWindowShortcut() const;
    void sourcePlayPauseClicked();
    void sourceStopClicked();
    void translationPlayPauseClicked();
    void translationStopClicked();
    void showTranslationWindow();
public slots:
    void ttsStateChanged(QTextToSpeech::State newState);
    void onTTSError(QTextToSpeech::ErrorReason reason, const QString &errorString);
    void translatorStateChanged(ATranslationProvider::State newState);
    Q_SCRIPTABLE void open();
    Q_SCRIPTABLE static void quit();
    Q_SCRIPTABLE void openSettings();
    Q_SCRIPTABLE void toggleOcrNegate();

private:
    Q_SCRIPTABLE void clearText();
    void loadMainWindowSettings();
    void saveMainWindowSettings();
    QHotkey *m_translateSelectionHotkey;
    QHotkey *m_speakSelectionHotkey;
    QHotkey *m_speakTranslatedSelectionHotkey;
    QHotkey *m_stopSpeakingHotkey;
    QHotkey *m_playPauseSpeakingHotkey;
    QHotkey *m_showMainWindowHotkey;
    QHotkey *m_copyTranslatedSelectionHotkey;
    QHotkey *m_recognizeScreenAreaHotkey;
    QHotkey *m_translateScreenAreaHotkey;
    QHotkey *m_delayedRecognizeScreenAreaHotkey;
    QHotkey *m_delayedTranslateScreenAreaHotkey;
    QHotkey *m_toggleOcrNegateHotkey;
    QShortcut *m_closeWindowsShortcut;
    Ui::MainWindow *ui;
    ATTSProvider *m_tts;
    ATTSProvider::ProviderBackend m_chosenTTSBackend;
    ATranslationProvider *m_translator;
    ATranslationProvider::ProviderBackend m_chosenTranslationBackend;
    ProviderOptionsManager *m_optionsManager;
    TesseractOcr *m_tesseractOcr;
    LlmOcr *m_llmOcr;
    QTimer *m_screenCaptureTimer;
    SnippingArea *m_snippingArea;
    ModuleStatus *m_moduleStatus;
    StatusStrip *m_statusStrip = nullptr;
    // Reports the capture start to the status model; grab() itself has no
    // "started" signal and adding one would touch every grabber subclass.
    void startScreenCapture();
    void loadAppSettings();
    void swapTranslator(ATranslationProvider::ProviderBackend newBackend);
    void setupEngineComboBoxConnection();
    void swapTTSProvider(ATTSProvider::ProviderBackend newBackend);
    void validateLanguageSupport();
    void applyTranslationProviderSettings();
    void applyTTSProviderSettings();
    void refreshLanguageWidgetsWithSupportedLanguages();
    void updateTTSButtonStates();
    bool isTTSAvailableForLanguage(const Language &language) const;
    void updateAutoLocales();
    void updateTranslateButtonState();
    void updateProviderUI();
    Language preferredTranslationLanguage(const Language &sourceLang) const;
    void handleTranslationRequest(const QString &text, const Language &destLang, const Language &srcLang);
    AppSettings::WindowMode m_windowMode;
    TrayIcon *m_trayIcon;
    ScreenWatcher *m_orientationWatcher;
    AbstractScreenGrabber *m_screenGrabber;
    Language m_sourceLang = Language::autoLanguage();
    Language m_destLang = Language::autoLanguage();
    bool m_listenForContentChanges = false;
    void setListenForContentChanges(bool listen);
    // Saved Mozhi engine combo items (restored after switching back from LocalAI)
    QStringList m_engineItemsText;
    QStringList m_engineItemsData;
    QList<QIcon> m_engineItemsIcons;
    void clearSourceImage();
    void setSourceImageInternal(const QImage &img);
    bool loadImageFromFile(const QString &path, QImage &out);
    bool m_hasSourceImage = false;
    QPixmap m_originalPixmap;
    QLabel *m_imagePreview = nullptr;
    QImage m_pendingOcrImage;
    AOcrProvider *activeOcr() const;
    void configureLlmOcr();
    // Configures the active engine from settings and reports whether it is
    // usable; every OCR entry point goes through it.
    bool prepareOcr();
    void recognizeSourceImage();
    // Chains a translation onto the next recognition. Armed by the
    // translate-screen-area paths and by an image drop while auto-translate is
    // on; disarmed on cancel so it never fires for an unrelated later OCR.
    void armOcrTranslation();
    void disarmOcrTranslation();
    QMetaObject::Connection m_ocrTranslateConnection;
    QMetaObject::Connection m_ocrTranslatorReadyConnection;
    QMetaObject::Connection m_imageOcrRecognizedConnection;
    QMetaObject::Connection m_imageOcrCanceledConnection;
    QMetaObject::Connection m_imageOcrFailedConnection;
    // Disconnected explicitly in ~MainWindow() before `delete ui` - QWidget's
    // own destructor destroys child widgets (deleteChildren(), which runs
    // the popup's destroyed() lambda below) *before* QObject's destructor
    // gets to auto-sever connections where `this` is the context, so relying
    // on that auto-disconnect alone isn't enough here.
    QMetaObject::Connection m_popupDestroyedConnection;
public slots:
    void on_translateButton_clicked();
    // Global shortcuts
    Q_SCRIPTABLE void translateSelection();
    Q_SCRIPTABLE void speakSelection();
    Q_SCRIPTABLE void speakTranslatedSelection();
    Q_SCRIPTABLE void stopSpeaking();
    Q_SCRIPTABLE void playPauseSpeaking();
    Q_SCRIPTABLE void copyTranslatedSelection();
    Q_SCRIPTABLE void recognizeScreenArea();
    Q_SCRIPTABLE void translateScreenArea();
    Q_SCRIPTABLE void delayedRecognizeScreenArea();
    Q_SCRIPTABLE void delayedTranslateScreenArea();

private slots:
    void setOrientation(Qt::ScreenOrientation orientation);
    void onSourceLanguageChanged(int id);
    void onDestinationLanguageChanged(int id);
    void handleAutoTranslation();
    void updateVoiceComboBoxes();
    void updateSpeakerComboBoxes();
    void on_swapButton_clicked();
    void on_abortButton_clicked();
    void on_copySourceButton_clicked();
    void on_openImageButton_clicked();
    void on_delayedRecognizeScreenAreaButton_clicked();
    void on_clearButton_clicked();
    void on_copyTranslationButton_clicked();
    void on_copyAllTranslationButton_clicked();
    void on_delayedTranslateScreenAreaButton_clicked();

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void translationRequested(const QString &inputText, const Language &translationLang, const Language &sourceLang);
    void translationAccepted();
    void resetTranslator();

#ifdef BUILD_TESTING
    // Test observability: Forward provider state changes as signals for QSignalSpy.
    // The slot versions (ttsStateChanged/translatorStateChanged) handle UI updates but aren't signals.
    void ttsStateChangedSignal(QTextToSpeech::State newState);
    void translatorStateChangedSignal(ATranslationProvider::State newState);
#endif
};

#endif // MAINWINDOW_H
