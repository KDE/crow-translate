/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "cmake.h"
#include "qonlinetranslator.h"
#include "settings/appsettings.h"

#include <QMainWindow>
#include <QMediaPlayer>

class AbstractScreenGrabber;
class LanguageButtonsWidget;
class Ocr;
class SnippingArea;
class ScreenWatcher;
class SpeakButtons;
class TranslationEdit;
class TrayIcon;
class QHotkey;
class QComboBox;
class QShortcut;
class QToolButton;

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", APPLICATION_ID ".MainWindow")
    Q_DISABLE_COPY(MainWindow)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    const QComboBox *engineCombobox() const;
    const QToolButton *swapButton() const;
    const QToolButton *copySourceButton() const;
    const QToolButton *copyTranslationButton() const;
    const QToolButton *copyAllTranslationButton() const;
    const TranslationEdit *translationEdit() const;
    const LanguageButtonsWidget *sourceLanguageButtons() const;
    const LanguageButtonsWidget *translationLanguageButtons() const;
    const SpeakButtons *sourceSpeakButtons() const;
    const SpeakButtons *translationSpeakButtons() const;
    QKeySequence closeWindowShortcut() const;
    Ocr *ocr() const;

public slots:
    // Global shortcuts
    Q_SCRIPTABLE void translateSelection();
    Q_SCRIPTABLE void speakSelection();
    Q_SCRIPTABLE void speakTranslatedSelection();
    Q_SCRIPTABLE void stopSpeaking();
    Q_SCRIPTABLE void playPauseSpeaking();
    Q_SCRIPTABLE void open();
    Q_SCRIPTABLE void copyTranslatedSelection();
    Q_SCRIPTABLE void recognizeScreenArea();
    Q_SCRIPTABLE void translateScreenArea();
    Q_SCRIPTABLE void delayedRecognizeScreenArea();
    Q_SCRIPTABLE void delayedTranslateScreenArea();

    // Main window shortcuts
    Q_SCRIPTABLE void clearText();
    Q_SCRIPTABLE void cancelOperation();
    Q_SCRIPTABLE void swapLanguages();
    Q_SCRIPTABLE void openSettings();
    Q_SCRIPTABLE void setAutoTranslateEnabled(bool enabled);
    Q_SCRIPTABLE void copySourceText();
    Q_SCRIPTABLE void copyTranslation();
    Q_SCRIPTABLE void copyAllTranslationInfo();
    Q_SCRIPTABLE static void quit();

signals:
    void contentChanged();
    void translateSelectionRequested();
    void speakSelectionRequested();
    void speakTranslatedSelectionRequested();
    void copyTranslatedSelectionRequested();
    void recognizeScreenAreaRequested();
    void translateScreenAreaRequested();
    void delayedRecognizeScreenAreaRequested();
    void delayedTranslateScreenAreaRequested();

private slots:
    // State machine's slots
    void requestTranslation();
    void requestRetranslation();
    void displayTranslation();
    void clearTranslation();

    void requestSourceLanguage();
    void parseSourceLanguage();

    void speakSource();
    void speakTranslation();

    void showTranslationWindow();
    void copyTranslationToClipboard();

    void forceSourceAutodetect();
    void forceTranslationAutodetect();

    void minimize();

    // UI
    void markContentAsChanged();
    void setListenForContentChanges(bool listen);
    void resetAutoSourceButtonText();

    // Other
    void setOrientation(Qt::ScreenOrientation orientation);
#ifdef Q_OS_WIN
    void checkForUpdates();
#endif

private slots:
    void onEngineChanged(int index);
    void onSelectImageButtonClicked();

private:
    void changeEvent(QEvent *event) override;

    void buildStateMachine();

    // Top-level states
    void buildTranslationState(QState *state) const;
    void buildSpeakSourceState(QState *state) const;
    void buildTranslateSelectionState(QState *state) const;
    void buildSpeakTranslationState(QState *state) const;
    void buildSpeakSelectionState(QState *state) const;
    void buildSpeakTranslatedSelectionState(QState *state) const;
    void buildCopyTranslatedSelectionState(QState *state) const;
    void buildRecognizeScreenAreaState(QState *state, void (MainWindow::*showFunction)() = &MainWindow::open);
    void buildTranslateScreenAreaState(QState *state);

    template<typename Func, typename... Args>
    void buildDelayedOcrState(QState *state, Func buildState, Args... additionalArgs);

    // State helpers
    void buildSetSelectionAsSourceState(QState *state) const;
    void setupRequestStateButtons(QState *state) const;

    // Other helpers
    void loadMainWindowSettings();
    void loadAppSettings();
    void checkLanguageButton(int checkedId);

    QOnlineTranslator::Language preferredTranslationLanguage(QOnlineTranslator::Language sourceLang) const;
    QOnlineTranslator::Engine currentEngine() const;

    Ui::MainWindow *ui;

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
    QShortcut *m_closeWindowsShortcut;

    QStateMachine *m_stateMachine;
    QOnlineTranslator *m_translator;
    TrayIcon *m_trayIcon;
    Ocr *m_ocr;
    QTimer *m_screenCaptureTimer;
    ScreenWatcher *m_orientationWatcher;
    AbstractScreenGrabber *m_screenGrabber;
    SnippingArea *m_snippingArea;

    QOnlineTranslator::Language m_primaryLanguage;
    QOnlineTranslator::Language m_secondaryLanguage;

    AppSettings::WindowMode m_windowMode;

    bool m_forceSourceAutodetect;
    bool m_forceTranslationAutodetect;
    bool m_listenForContentChanges = false;

    QString m_selectedImagePath; // For Gemini image translation
};

#endif // MAINWINDOW_H
