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
#include "core/usernotifier.h"
#include "ocr/aocrprovider.h"
#include "ocr/screengrabbers/abstractscreengrabber.h"
#include "ocr/snippingarea.h"
#include "ocr/tesseractocr.h"
#include "settings/appsettings.h"
#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QComboBox>
#include <QElapsedTimer>
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
class TranslationSession;
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
    // Whether a streamed answer is being painted right now. The pop-up asks so
    // that it holds the reader's place in a stream while a fresh result still
    // opens at the top.
    bool translationIsStreaming() const;
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
    void showNotification(const UserNotifier::Notification &notification);
public slots:
    void ttsStateChanged(QTextToSpeech::State newState);
    void onTTSError(QTextToSpeech::ErrorReason reason, const QString &errorString);
    void translatorStateChanged(ATranslationProvider::State newState);
    Q_SCRIPTABLE void open();
    Q_SCRIPTABLE static void quit();
    Q_SCRIPTABLE void openSettings();
    Q_SCRIPTABLE void toggleOcrNegate();
    Q_SCRIPTABLE void clearText();

private:
    void loadMainWindowSettings();
    void saveMainWindowSettings();
    QHotkey *m_translateSelectionHotkey = nullptr;
    QHotkey *m_speakSelectionHotkey = nullptr;
    QHotkey *m_speakTranslatedSelectionHotkey = nullptr;
    QHotkey *m_stopSpeakingHotkey = nullptr;
    QHotkey *m_playPauseSpeakingHotkey = nullptr;
    QHotkey *m_showMainWindowHotkey = nullptr;
    QHotkey *m_copyTranslatedSelectionHotkey = nullptr;
    QHotkey *m_recognizeScreenAreaHotkey = nullptr;
    QHotkey *m_translateScreenAreaHotkey = nullptr;
    QHotkey *m_delayedRecognizeScreenAreaHotkey = nullptr;
    QHotkey *m_delayedTranslateScreenAreaHotkey = nullptr;
    QHotkey *m_toggleOcrNegateHotkey = nullptr;
    QShortcut *m_closeWindowsShortcut = nullptr;
    Ui::MainWindow *ui = nullptr;
    // Both providers, the options manager, the language model and the status
    // model live here now. The window borrows them; it does not own them, and
    // a backend swap replaces the provider objects underneath it without any
    // of the window's own wiring being touched.
    TranslationSession *m_session = nullptr;
    QTimer *m_screenCaptureTimer = nullptr;
    SnippingArea *m_snippingArea = nullptr;
    StatusStrip *m_statusStrip = nullptr;
    // What the voice combos were last populated for, so a completed
    // translation only rebuilds them when the spoken language actually moved.
    Language m_voiceComboSourceLang = Language::autoLanguage();
    Language m_voiceComboTranslationLang = Language::autoLanguage();
    // Reports the capture start to the status model; grab() itself has no
    // "started" signal and adding one would touch every grabber subclass.
    void startScreenCapture();
    void loadAppSettings();
    void setupEngineComboBoxConnection();
    void validateLanguageSupport();
    // The session rebuilt a provider: re-read it and rebuild what displays it.
    void onTranslatorChanged();
    void onTtsProviderChanged();
    void refreshLanguageWidgetsWithSupportedLanguages();
    void updateTTSButtonStates();
    bool isTTSAvailableForLanguage(const Language &language) const;
    void updateAutoLocales();
    void updateTranslateButtonState();
    void updateProviderUI();
    // Pushes the buttons' checked languages into the session's language
    // model. "Auto" stays auto here - resolving it is the model's job, not
    // the caller's. This is the only door the window's language choice goes
    // through, which is what lets the session answer "what did the user ask
    // for" without owning a widget.
    void publishLanguageSelection();
    // The provider reported a detected source, or finished a translation.
    void onLanguageDetected(const Language &detectedLanguage, bool isTranslationContext);
    // The answer so far, from a streaming provider: shown while the model is
    // still writing it.
    void onPartialTranslation(const QString &text);
    // What a translation of the selection is for: shown as every translation
    // is, or spoken or copied without a window, which is what the speak- and
    // copy-translated-selection shortcuts ask for.
    enum class SelectionUse : uint8_t {
        Show,
        Speak,
        Copy,
    };
    // Reads the selection into the source field and translates it, for `use`.
    void translateSelectionFor(SelectionUse use);
    // Clears the translation field and starts the request; see its definition.
    void requestSelectionTranslation(const QString &text, const Language &destination, const Language &source, SelectionUse use);
    void speakTranslation();
    void copyTranslation();
    // Which text the speech in progress reads: each side's play button pauses
    // and resumes its own speech, and starts its own over the other's.
    enum class SpeechSide : uint8_t {
        None,
        Source,
        Translation,
    };
    void playPauseSide(SpeechSide side, const QString &text);
    SpeechSide m_speechSide = SpeechSide::None;
    // The use of the selection translation in flight; Show for everything else.
    SelectionUse m_selectionUse = SelectionUse::Show;
    // Replaces the translation without moving the reader: every streamed update
    // rebuilds the document, and a rebuild resets the view.
    void setTranslationHtml(const QString &html);
    AppSettings::WindowMode m_windowMode;
    TrayIcon *m_trayIcon;
    ScreenWatcher *m_orientationWatcher;
    AbstractScreenGrabber *m_screenGrabber;
    Language m_sourceLang = Language::autoLanguage();
    Language m_destLang = Language::autoLanguage();
    bool m_listenForContentChanges = false;
    // Whether this request's first streamed piece has already brought the
    // translation window up.
    bool m_partialShown = false;
    // When the field was last repainted from a streamed answer.
    QElapsedTimer m_partialRepaint;
    void setListenForContentChanges(bool listen);
    // Saved Mozhi engine combo items (restored after switching back from LocalAI)
    QStringList m_engineItemsText;
    QStringList m_engineItemsData;
    QList<QIcon> m_engineItemsIcons;
    void clearSourceImage();
    void setSourceImageInternal(const QImage &img);
    bool m_hasSourceImage = false;
    QPixmap m_originalPixmap;
    QLabel *m_imagePreview = nullptr;
    QImage m_pendingOcrImage;
    // Asks the session whether OCR can run, then applies the parts that are
    // this window's own - every OCR entry point goes through it.
    bool prepareOcr();
    void recognizeSourceImage();
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
    void refreshVoicesForSpokenLanguages();
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

private:
    // The one place that answers which voice and speaker the user wants.
    void applySelectedVoiceAndSpeaker(bool forSource);

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

public:
    // The languages speech should actually use. Not the same thing as the
    // buttons' checked languages: with "auto" checked the destination is
    // resolved per translation, and speaking the resolved text in the
    // system locale instead is exactly the bug these exist to prevent.
    Language spokenSourceLanguage() const;
    Language spokenTranslationLanguage() const;
#ifdef BUILD_TESTING
    // Which language the voice combos were last populated for - a stale value
    // here is how an English voice ended up overriding a Russian translation.
    Language voiceComboTranslationLanguage() const
    {
        return m_voiceComboTranslationLang;
    }
#endif
};

#endif // MAINWINDOW_H
