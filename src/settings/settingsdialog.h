/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "translator/atranslationprovider.h"
#include "tts/attsprovider.h"

#include <QDialog>
#include <QMap>
#include <QString>

class MainWindow;
class AbstractAutostartManager;
class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QCheckBox;
class QSpinBox;
class QButtonGroup;
class QPushButton;
class QStackedWidget;
class OnlineTranslator;
class QMediaPlayer;
class QMediaPlaylist;
class QLabel;
class QToolButton;
class ShortcutItem;
class VisionModelProbe;
#ifdef WITH_PORTABLE_MODE
class QCheckBox;
#endif

namespace Ui
{
class SettingsDialog;
} // namespace Ui

// Widens a combo box's popup (and the box itself) to fit its longest item.
// Shared by SettingsDialog's LocalAI provider tabs and MainWindow's engine
// combo, both of which populate the same combo box with provider names.
void widenComboPopup(QComboBox *combo);

class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsDialog)

public:
    explicit SettingsDialog(MainWindow *parent = nullptr);
    ~SettingsDialog() override;

public slots:
    void accept() override;

signals:
    void translationBackendChanged(ATranslationProvider::ProviderBackend newBackend);
    void ttsBackendChanged(ATTSProvider::ProviderBackend newBackend);
    void piperVoicesPathChanged(const QString &newPath);
    void mozhiInstanceChanged(const QString &newInstance);

private slots:
    void setCurrentPage(int index);

    void onProxyTypeChanged(int type);
    void onWindowModeChanged(int mode);

    void onTrayIconTypeChanged(int type);
    void selectCustomTrayIcon();
    void setCustomTrayIconPreview(const QString &iconPath);
    void detectFastestInstance();

    void selectOcrLanguagesPath();
    void onOcrLanguagesPathChanged(const QString &path);
#ifdef WITH_PIPER_TTS
    void setupPiperVoicesPathUI();
    void selectPiperVoicesPath();
    void onPiperVoicesPathChanged(const QString &path);
#endif
    void onTesseractParametersCurrentItemChanged();

    void loadShortcut(ShortcutItem *item);
    void updateAcceptButton();
    void acceptCurrentShortcut();
    void clearCurrentShortcut();
    void resetCurrentShortcut();
    void resetAllShortcuts();

    void restoreDefaults();

private:
    void addLocale(const QLocale &locale);
    void activateCompactMode();
    void loadSettings();

    Ui::SettingsDialog *ui;

    // OCR engine selection (Tesseract / LLM) — dynamic widgets on the OCR page.
    // The LLM engine carries a complete endpoint of its own (URL, key, model
    // list); it shares nothing but the provider *kinds* with the LocalAI
    // translation backend.
    QComboBox *m_ocrEngineCombo = nullptr;
    QStackedWidget *m_ocrEngineStack = nullptr;
    QComboBox *m_ocrLlmProviderCombo = nullptr;
    QLineEdit *m_ocrLlmUrlEdit = nullptr;
    QLineEdit *m_ocrLlmApiKeyEdit = nullptr;
    QPushButton *m_ocrLlmRefreshButton = nullptr;
    QComboBox *m_ocrLlmModelCombo = nullptr;
    QSpinBox *m_ocrLlmTimeoutSpin = nullptr;
    QLabel *m_ocrLlmCapabilityHint = nullptr;
    QPlainTextEdit *m_ocrLlmPromptEdit = nullptr;
    QPushButton *m_ocrLlmResetPromptButton = nullptr;
    VisionModelProbe *m_ocrLlmProbe = nullptr;
    // Which model the prompt editor is currently showing text for, so a
    // model switch can file the edits under the model they were written for
    // instead of under whatever is selected afterwards.
    QString m_ocrLlmPromptModel;

    // LocalAI settings (Ollama / FastFlowLM / LM Studio) — dynamic sub-tabs.
    struct LocalProviderMode {
        QWidget *page = nullptr;
        QLabel *modelLabel = nullptr;
        QComboBox *model = nullptr;
        QSpinBox *timeout = nullptr;
        QCheckBox *disableThinking = nullptr;
        QPlainTextEdit *prompt = nullptr;
        QString promptModel;
    };
    struct LocalProviderTab {
        QLineEdit *url = nullptr;
        QLineEdit *apiKey = nullptr;
        QPushButton *refresh = nullptr;
        LocalProviderMode text;
    };
    void buildLocalAiTabs();
    void buildOcrEngineUi();
    void loadOcrEngineSettings();
    void saveOcrEngineSettings();
    void updateOcrEngineVisibility(int engineValue);
    void loadOcrLlmProvider(const QString &id);
    void populateOcrModelCombo(const QString &id);
    void refreshOcrModels();
    void showOcrPromptFor(const QString &model);
    void storeOcrPromptEdits();
    void loadLocalAiSettings();
    void saveLocalAiSettings();
    void showLocalAiPromptFor(const QString &id, const QString &model);
    void storeLocalAiPromptEdits(const QString &id);
    void refreshLocalModels(const QString &id);
    void populateDetectModels();

    QMap<QString, LocalProviderTab> m_localTabs; // provider id -> widgets

    // Manage platform-dependant autostart
    AbstractAutostartManager *m_autostartManager;

#ifdef WITH_PORTABLE_MODE
    QCheckBox *m_portableCheckbox;
#endif

#ifdef WITH_PIPER_TTS
    QLabel *m_piperVoicesPathLabel;
    QLineEdit *m_piperVoicesPathEdit;
    QToolButton *m_piperVoicesPathButton;
#endif
};

#endif // SETTINGSDIALOG_H
