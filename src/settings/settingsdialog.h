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
#ifdef WITH_PORTABLE_MODE
class QCheckBox;
#endif

namespace Ui
{
class SettingsDialog;
} // namespace Ui

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

    // LocalAI settings (Ollama / FastFlowLM / LM Studio) — dynamic sub-tabs.
    struct LocalProviderMode {
        QWidget *page = nullptr;
        QLabel *modelLabel = nullptr;
        QComboBox *model = nullptr;
        QSpinBox *timeout = nullptr;
        QCheckBox *disableThinking = nullptr;
        QPlainTextEdit *prompt = nullptr;
    };
    struct LocalProviderTab {
        QLineEdit *url = nullptr;
        QPushButton *refresh = nullptr;
        QPushButton *visionToggle = nullptr;
        QPushButton *textToggle = nullptr;
        QButtonGroup *modeGroup = nullptr;
        QStackedWidget *stack = nullptr;
        QWidget *debugContainer = nullptr;
        QCheckBox *debugCheckBox = nullptr;
        LocalProviderMode text;
        LocalProviderMode vision;
    };
    void buildLocalAiTabs();
    void loadLocalAiSettings();
    void saveLocalAiSettings();
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
