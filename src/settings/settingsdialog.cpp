/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include "mainwindow.h"
#include "qhotkey.h"
#include "screenwatcher.h"
#include "trayicon.h"
#include "autostartmanager/abstractautostartmanager.h"
#include "ocr/ocr.h"
#include "shortcutsmodel/shortcutitem.h"
#include "shortcutsmodel/shortcutsmodel.h"

#include <QDate>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QStandardItemModel>

SettingsDialog::SettingsDialog(MainWindow *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_autostartManager(AbstractAutostartManager::createAutostartManager(this))
#ifdef WITH_PORTABLE_MODE
    , m_portableCheckbox(new QCheckBox(tr("Portable mode"), this))
#endif
{
    ui->setupUi(this);
    if (!ScreenWatcher::isWidthFitScreen(this))
        activateCompactMode();

    connect(ui->dialogButtonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    connect(ui->globalShortcutsCheckBox, &QCheckBox::toggled, ui->shortcutsTreeView->model(), &ShortcutsModel::setGlobalShortuctsEnabled);
    ui->logoLabel->setPixmap(QIcon::fromTheme(QStringLiteral("crow-translate")).pixmap(512, 512));
    ui->versionLabel->setText(QCoreApplication::applicationVersion());

#ifdef WITH_PORTABLE_MODE
    m_portableCheckbox->setToolTip(tr("Use %1 to store settings").arg(AppSettings::portableConfigName()));
    qobject_cast<QFormLayout *>(ui->generalGroupBox->layout())->addRow(m_portableCheckbox);
#endif

    // Set item data in comboboxes
    ui->localeComboBox->addItem(tr("<System language>"), AppSettings::defaultLocale());
    for (const QString &locale : LOCALES)
        addLocale(QLocale(locale));

    ui->primaryLangComboBox->addItem(tr("<System language>"), OnlineTranslator::Auto);
    ui->secondaryLangComboBox->addItem(tr("<System language>"), OnlineTranslator::Auto);
    for (int i = 1; i <= OnlineTranslator::Zulu; ++i) {
        const auto lang = static_cast<OnlineTranslator::Language>(i);

        ui->primaryLangComboBox->addItem(OnlineTranslator::languageName(lang), i);
        ui->secondaryLangComboBox->addItem(OnlineTranslator::languageName(lang), i);
    }

    ui->ocrLanguagesListWidget->addLanguages(parent->ocr()->availableLanguages());

    // Set all avaialble instances
    ui->mozhiUrlComboBox->addItems(AppSettings::instanceUrls());

    // Sort languages in comboboxes alphabetically
    ui->primaryLangComboBox->model()->sort(0);
    ui->secondaryLangComboBox->model()->sort(0);

    // Set maximum and minimum values for the size of the popup window
    ui->popupWidthSlider->setMaximum(QGuiApplication::primaryScreen()->availableGeometry().width());
    ui->popupWidthSpinBox->setMaximum(QGuiApplication::primaryScreen()->availableGeometry().width());
    ui->popupHeightSlider->setMaximum(QGuiApplication::primaryScreen()->availableGeometry().height());
    ui->popupHeightSpinBox->setMaximum(QGuiApplication::primaryScreen()->availableGeometry().height());
    ui->popupWidthSlider->setMinimum(200);
    ui->popupWidthSpinBox->setMinimum(200);
    ui->popupHeightSlider->setMinimum(200);
    ui->popupHeightSpinBox->setMinimum(200);

    // Disable (enable) opacity slider if "Window mode" ("Popup mode") selected
    connect(ui->windowModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), ui->popupOpacityLabel, &QSlider::setDisabled);
    connect(ui->windowModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), ui->popupOpacitySlider, &QSlider::setDisabled);

#ifndef Q_OS_UNIX
    // Add information about icons
    auto *iconsTitleLabel = new QLabel(this);
    iconsTitleLabel->setText(tr("Icons:"));

    auto *iconsLabel = new QLabel(this);
    iconsLabel->setText("<a href=\"https://invent.kde.org/frameworks/breeze-icons\">Breeze</a>");
    iconsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    iconsLabel->setOpenExternalLinks(true);

    qobject_cast<QFormLayout *>(ui->aboutGroupBox->layout())->addRow(iconsTitleLabel, iconsLabel);
#endif

    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::accept()
{
    if (!ui->tesseractParametersTableWidget->validateParameters()) {
        QMessageBox msgBox;
        msgBox.setText(tr("The OCR parameter fields can not be empty."));
        msgBox.setInformativeText(tr("Do you want to discard the invalid parameters?"));
        msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::No);
        msgBox.setIcon(QMessageBox::Warning);
        if (msgBox.exec() == QMessageBox::No) {
            // Navigate to OCR
            ui->pagesListWidget->setCurrentRow(ui->pagesStackedWidget->indexOf(ui->ocrPage));
            return;
        }
    }

    // Set settings location first
#ifdef WITH_PORTABLE_MODE
    AppSettings::setPortableModeEnabled(m_portableCheckbox->isChecked());
#endif

    // General settings
    AppSettings settings;
    settings.setLocale(ui->localeComboBox->currentData().value<QLocale>());
    settings.setMainWindowOrientation(static_cast<Qt::ScreenOrientation>(ui->mainWindowOrientationComboBox->currentIndex()));
    settings.setWindowMode(static_cast<AppSettings::WindowMode>(ui->windowModeComboBox->currentIndex()));
    settings.setTranslationNotificationTimeout(ui->translationNotificationTimeoutSpinBox->value());
    settings.setPopupWindowTimeout(ui->popupWindowTimeoutSpinBox->value());
    settings.setShowTrayIcon(ui->showTrayIconCheckBox->isChecked());
    settings.setStartMinimized(ui->startMinimizedCheckBox->isChecked());
    m_autostartManager->setAutostartEnabled(ui->autostartCheckBox->isChecked());

    // Interface settings
    QFont font = ui->fontNameComboBox->currentFont();
    font.setPointSize(ui->fontSizeSpinBox->value());
    settings.setFont(font);

    settings.setPopupOpacity(static_cast<double>(ui->popupOpacitySlider->value()) / 100);
    settings.setPopupWidth(ui->popupWidthSpinBox->value());
    settings.setPopupHeight(ui->popupHeightSpinBox->value());

    settings.setMainWindowLanguageFormat(static_cast<AppSettings::LanguageFormat>(ui->mainWindowLanguageFormatComboBox->currentIndex()));
    settings.setPopupLanguageFormat(static_cast<AppSettings::LanguageFormat>(ui->popupLanguageFormatComboBox->currentIndex()));

    settings.setTrayIconType(static_cast<AppSettings::IconType>(ui->trayIconComboBox->currentIndex()));
    settings.setCustomIconPath(ui->customTrayIconEdit->text());

    // Translation settings
    settings.setSourceTranslitEnabled(ui->sourceTranslitCheckBox->isChecked());
    settings.setTranslationTranslitEnabled(ui->translationTranslitCheckBox->isChecked());
    settings.setSourceTranscriptionEnabled(ui->sourceTranscriptionCheckBox->isChecked());
    settings.setTranslationOptionsEnabled(ui->translationOptionsCheckBox->isChecked());
    settings.setExamplesEnabled(ui->examplesCheckBox->isChecked());
    settings.setSimplifySource(ui->sourceSimplificationCheckBox->isChecked());
    settings.setPrimaryLanguage(ui->primaryLangComboBox->currentData().value<OnlineTranslator::Language>());
    settings.setSecondaryLanguage(ui->secondaryLangComboBox->currentData().value<OnlineTranslator::Language>());
    settings.setForceSourceAutodetect(ui->forceSourceAutodetectCheckBox->isChecked());
    settings.setForceTranslationAutodetect(ui->forceTranslationAutodetectCheckBox->isChecked());

    // Instance settings
    settings.setInstanceUrl(ui->mozhiUrlComboBox->currentText());

    // OCR
    settings.setConvertLineBreaks(ui->convertLineBreaksCheckBox->isChecked());
    settings.setOcrLanguagesPath(ui->ocrLanguagesPathEdit->text().toLocal8Bit());
    settings.setOcrLanguagesString(ui->ocrLanguagesListWidget->checkedLanguagesString());
    settings.setRegionRememberType(static_cast<AppSettings::RegionRememberType>(ui->rememberRegionComboBox->currentIndex()));
    settings.setCaptureDelay(ui->captureDelaySpinBox->value());
    settings.setShowMagnifier(ui->showMagnifierCheckBox->isChecked());
    settings.setConfirmOnRelease(ui->confirmOnReleaseCheckBox->isChecked());
    settings.setApplyLightMask(ui->applyLightMaskCheckBox->isChecked());
    settings.setTesseractParameters(ui->tesseractParametersTableWidget->parameters());

    // Connection settings
    settings.setProxyType(static_cast<QNetworkProxy::ProxyType>(ui->proxyTypeComboBox->currentIndex()));
    settings.setProxyHost(ui->proxyHostEdit->text());
    settings.setProxyPort(static_cast<quint16>(ui->proxyPortSpinbox->value()));
    settings.setProxyAuthEnabled(ui->proxyAuthCheckBox->isChecked());
    settings.setProxyUsername(ui->proxyUsernameEdit->text());
    settings.setProxyPassword(ui->proxyPasswordEdit->text());

    // Shortcuts
    if (QHotkey::isPlatformSupported())
        settings.setGlobalShortcutsEnabled(ui->globalShortcutsCheckBox->isChecked());
    ui->shortcutsTreeView->model()->saveShortcuts(settings);

    QDialog::accept();
}

void SettingsDialog::setCurrentPage(int index)
{
    // Ignore size police for hidden pages to show scrollbar only for visible page
    // https://wiki.qt.io/Technical_FAQ#How_can_I_get_a_QStackedWidget_to_automatically_switch_size_depending_on_the_content_of_the_page.3F
    ui->pagesStackedWidget->currentWidget()->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui->pagesStackedWidget->setCurrentIndex(index);
    ui->pagesStackedWidget->currentWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void SettingsDialog::onProxyTypeChanged(int type)
{
    if (type == QNetworkProxy::HttpProxy || type == QNetworkProxy::Socks5Proxy) {
        ui->proxyHostEdit->setEnabled(true);
        ui->proxyHostLabel->setEnabled(true);
        ui->proxyPortLabel->setEnabled(true);
        ui->proxyPortSpinbox->setEnabled(true);
        ui->proxyInfoLabel->setEnabled(true);
        ui->proxyAuthCheckBox->setEnabled(true);
    } else {
        ui->proxyHostEdit->setEnabled(false);
        ui->proxyHostLabel->setEnabled(false);
        ui->proxyPortLabel->setEnabled(false);
        ui->proxyPortSpinbox->setEnabled(false);
        ui->proxyInfoLabel->setEnabled(false);
        ui->proxyAuthCheckBox->setEnabled(false);
    }
}

// Update "Show tray Icon" checkbox state when “Notification" or "Popup" mode selected
void SettingsDialog::onWindowModeChanged(int mode)
{
    if (mode == AppSettings::Notification || mode == AppSettings::PopupWindow) {
        ui->showTrayIconCheckBox->setDisabled(true);
        ui->showTrayIconCheckBox->setChecked(true);
    } else {
        ui->showTrayIconCheckBox->setEnabled(true);
    }
}

// Disable (enable) "Custom icon path" option
void SettingsDialog::onTrayIconTypeChanged(int type)
{
    if (type == AppSettings::CustomIcon) {
        ui->customTrayIconLabel->setEnabled(true);
        ui->customTrayIconEdit->setEnabled(true);
        ui->customTrayIconButton->setEnabled(true);
    } else {
        ui->customTrayIconLabel->setEnabled(false);
        ui->customTrayIconEdit->setEnabled(false);
        ui->customTrayIconButton->setEnabled(false);
    }
}

void SettingsDialog::selectCustomTrayIcon()
{
    const QString path = ui->customTrayIconEdit->text().left(ui->customTrayIconEdit->text().lastIndexOf(QDir::separator()));
    const QString file = QFileDialog::getOpenFileName(this, tr("Select icon"), path, tr("Images (*.png *.ico *.svg *.jpg);;All files()"));
    if (!file.isEmpty())
        ui->customTrayIconEdit->setText(file);
}

void SettingsDialog::setCustomTrayIconPreview(const QString &iconPath)
{
    ui->customTrayIconButton->setIcon(TrayIcon::customTrayIcon(iconPath));
}

void SettingsDialog::selectOcrLanguagesPath()
{
    const QString path = ui->ocrLanguagesPathEdit->text().left(ui->ocrLanguagesPathEdit->text().lastIndexOf(QDir::separator()));
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Select OCR languages path"), path);
    if (!directory.isEmpty())
        ui->ocrLanguagesPathEdit->setText(directory);
}

void SettingsDialog::onOcrLanguagesPathChanged(const QString &path)
{
    ui->ocrLanguagesListWidget->clear();
    ui->ocrLanguagesListWidget->addLanguages(Ocr::availableLanguages(path));
}

void SettingsDialog::onTesseractParametersCurrentItemChanged()
{
    if (ui->tesseractParametersTableWidget->currentRow() == -1)
        ui->tesseractParametersRemoveButton->setEnabled(false);
    else
        ui->tesseractParametersRemoveButton->setEnabled(true);
}

void SettingsDialog::loadShortcut(ShortcutItem *item)
{
    if (item->childCount() == 0) {
        ui->shortcutGroupBox->setEnabled(true);
        ui->shortcutSequenceEdit->setKeySequence(item->shortcut());
    } else {
        ui->shortcutGroupBox->setEnabled(false);
        ui->shortcutSequenceEdit->clear();
    }
}

void SettingsDialog::updateAcceptButton()
{
    if (ui->shortcutsTreeView->currentItem()->shortcut() != ui->shortcutSequenceEdit->keySequence())
        ui->acceptShortcutButton->setEnabled(true);
    else
        ui->acceptShortcutButton->setEnabled(false);
}

void SettingsDialog::acceptCurrentShortcut()
{
    ui->shortcutsTreeView->currentItem()->setShortcut(ui->shortcutSequenceEdit->keySequence());
    ui->acceptShortcutButton->setEnabled(false);
}

void SettingsDialog::clearCurrentShortcut()
{
    ui->shortcutSequenceEdit->clear();
    ui->acceptShortcutButton->setEnabled(true);
}

void SettingsDialog::resetCurrentShortcut()
{
    ui->shortcutsTreeView->currentItem()->resetShortcut();
    ui->shortcutSequenceEdit->setKeySequence(ui->shortcutsTreeView->currentItem()->shortcut());
    ui->acceptShortcutButton->setEnabled(false);
}

void SettingsDialog::resetAllShortcuts()
{
    ui->shortcutsTreeView->model()->resetAllShortcuts();
}

void SettingsDialog::restoreDefaults()
{
    // General settings
    ui->localeComboBox->setCurrentIndex(ui->localeComboBox->findData(AppSettings::defaultLocale()));
    ui->mainWindowOrientationComboBox->setCurrentIndex(AppSettings::defaultMainWindowOrientation());
    ui->windowModeComboBox->setCurrentIndex(AppSettings::defaultWindowMode());
    ui->translationNotificationTimeoutSpinBox->setValue(AppSettings::defaultTranslationNotificationTimeout());
    ui->popupWindowTimeoutSpinBox->setValue(AppSettings::defaultPopupWindowTimeout());
    ui->showTrayIconCheckBox->setChecked(AppSettings::defaultShowTrayIcon());
    ui->startMinimizedCheckBox->setChecked(AppSettings::defaultStartMinimized());
    ui->autostartCheckBox->setChecked(AppSettings::defaultAutostartEnabled());

    // Interface settings
    const QFont defaultFont = QApplication::font();
    ui->fontNameComboBox->setCurrentFont(defaultFont);
    ui->fontSizeSpinBox->setValue(defaultFont.pointSize());

    ui->popupOpacitySlider->setValue(static_cast<int>(AppSettings::defaultPopupOpacity() * 100));
    ui->popupWidthSpinBox->setValue(AppSettings::defaultPopupWidth());
    ui->popupHeightSpinBox->setValue(AppSettings::defaultPopupHeight());

    ui->mainWindowLanguageFormatComboBox->setCurrentIndex(AppSettings::defaultMainWindowLanguageFormat());
    ui->popupLanguageFormatComboBox->setCurrentIndex(AppSettings::defaultPopupLanguageFormat());

    ui->trayIconComboBox->setCurrentIndex(AppSettings::defaultTrayIconType());
    ui->customTrayIconEdit->setText(AppSettings::defaultCustomIconPath());

    // Translation settings
    ui->sourceTranslitCheckBox->setChecked(AppSettings::defaultSourceTranslitEnabled());
    ui->translationTranslitCheckBox->setChecked(AppSettings::defaultTranslationTranslitEnabled());
    ui->sourceTranscriptionCheckBox->setChecked(AppSettings::defaultSourceTranscriptionEnabled());
    ui->translationOptionsCheckBox->setChecked(AppSettings::defaultTranslationOptionsEnabled());
    ui->examplesCheckBox->setChecked(AppSettings::defaultExamplesEnabled());
    ui->sourceSimplificationCheckBox->setChecked(AppSettings::defaultSimplifySource());
    ui->primaryLangComboBox->setCurrentIndex(ui->primaryLangComboBox->findData(AppSettings::defaultPrimaryLanguage()));
    ui->secondaryLangComboBox->setCurrentIndex(ui->secondaryLangComboBox->findData(AppSettings::defaultSecondaryLanguage()));
    ui->forceSourceAutodetectCheckBox->setChecked(AppSettings::defaultForceSourceAutodetect());
    ui->forceTranslationAutodetectCheckBox->setChecked(AppSettings::defaultForceTranslationAutodetect());

    // Instance settings
    ui->mozhiUrlComboBox->setCurrentText(AppSettings::randomInstanceUrl());

    // OCR
    ui->convertLineBreaksCheckBox->setChecked(AppSettings::defaultConvertLineBreaks());
    ui->ocrLanguagesPathEdit->setText(AppSettings::defaultOcrLanguagesPath());
    ui->ocrLanguagesListWidget->setCheckedLanguages(AppSettings::defaultOcrLanguagesString());
    ui->rememberRegionComboBox->setCurrentIndex(AppSettings::defaultRegionRememberType());
    ui->captureDelaySpinBox->setValue(AppSettings::defaultCaptureDelay());
    ui->showMagnifierCheckBox->setChecked(AppSettings::defaultShowMagnifier());
    ui->confirmOnReleaseCheckBox->setChecked(AppSettings::defaultConfirmOnRelease());
    ui->applyLightMaskCheckBox->setChecked(AppSettings::defaultApplyLightMask());
    ui->tesseractParametersTableWidget->setParameters(AppSettings::defaultTesseractParameters());

    // Connection settings
    ui->proxyTypeComboBox->setCurrentIndex(AppSettings::defaultProxyType());
    ui->proxyHostEdit->setText(AppSettings::defaultProxyHost());
    ui->proxyPortSpinbox->setValue(AppSettings::defaultProxyPort());
    ui->proxyAuthCheckBox->setChecked(AppSettings::defaultProxyAuthEnabled());
    ui->proxyUsernameEdit->setText(AppSettings::defaultProxyUsername());
    ui->proxyPasswordEdit->setText(AppSettings::defaultProxyPassword());

    // Shortcuts
    if (QHotkey::isPlatformSupported())
        ui->globalShortcutsCheckBox->setEnabled(AppSettings::defaultGlobalShortcutsEnabled());
    resetAllShortcuts();
}

void SettingsDialog::addLocale(const QLocale &locale)
{
    ui->localeComboBox->addItem(locale.nativeLanguageName(), locale);
}

void SettingsDialog::activateCompactMode()
{
    setWindowState(windowState() | Qt::WindowMaximized);

    ui->pagesListWidget->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->scrollArea->hide();

    auto *backButton = new QPushButton(QIcon::fromTheme("arrow-left"), tr("Back"));
    backButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->settingsDialogLayout->insertWidget(0, backButton);
    backButton->hide();

    connect(backButton, &QPushButton::clicked, backButton, &QPushButton::hide);
    connect(backButton, &QPushButton::clicked, ui->scrollArea, &QScrollArea::hide);
    connect(backButton, &QPushButton::clicked, ui->pagesListWidget, &QListWidget::show);
    connect(ui->pagesListWidget, &QListWidget::itemActivated, ui->pagesListWidget, &QListWidget::hide);
    connect(ui->pagesListWidget, &QListWidget::itemActivated, backButton, &QPushButton::show);
    connect(ui->pagesListWidget, &QListWidget::itemActivated, ui->scrollArea, &QScrollArea::show);
    connect(ui->pagesListWidget, &QListWidget::itemClicked, ui->pagesListWidget, &QListWidget::hide);
    connect(ui->pagesListWidget, &QListWidget::itemClicked, backButton, &QPushButton::show);
    connect(ui->pagesListWidget, &QListWidget::itemClicked, ui->scrollArea, &QScrollArea::show);
}

void SettingsDialog::loadSettings()
{
    // General settings
    const AppSettings settings;
    ui->localeComboBox->setCurrentIndex(ui->localeComboBox->findData(settings.locale()));
    ui->mainWindowOrientationComboBox->setCurrentIndex(settings.mainWindowOrientation());
    ui->translationNotificationTimeoutSpinBox->setValue(settings.translationNotificationTimeout());
    ui->popupWindowTimeoutSpinBox->setValue(settings.popupWindowTimeout());
    ui->windowModeComboBox->setCurrentIndex(settings.windowMode());
    ui->showTrayIconCheckBox->setChecked(settings.isShowTrayIcon());
    ui->startMinimizedCheckBox->setChecked(settings.isStartMinimized());
    ui->autostartCheckBox->setChecked(m_autostartManager->isAutostartEnabled());
#ifdef WITH_PORTABLE_MODE
    m_portableCheckbox->setChecked(settings.isPortableModeEnabled());
#endif

    // Interface settings
    const QFont font = settings.font();
    ui->fontNameComboBox->setCurrentFont(font);
    ui->fontSizeSpinBox->setValue(font.pointSize());

    ui->popupOpacitySlider->setValue(static_cast<int>(settings.popupOpacity() * 100));
    ui->popupWidthSpinBox->setValue(settings.popupWidth());
    ui->popupHeightSpinBox->setValue(settings.popupHeight());

    ui->mainWindowLanguageFormatComboBox->setCurrentIndex(settings.mainWindowLanguageFormat());
    ui->popupLanguageFormatComboBox->setCurrentIndex(settings.popupLanguageFormat());

    ui->trayIconComboBox->setCurrentIndex(settings.trayIconType());
    ui->customTrayIconEdit->setText(settings.customIconPath());

    // Translation settings
    ui->sourceTranslitCheckBox->setChecked(settings.isSourceTranslitEnabled());
    ui->translationTranslitCheckBox->setChecked(settings.isTranslationTranslitEnabled());
    ui->sourceTranscriptionCheckBox->setChecked(settings.isSourceTranscriptionEnabled());
    ui->translationOptionsCheckBox->setChecked(settings.isTranslationOptionsEnabled());
    ui->examplesCheckBox->setChecked(settings.isExamplesEnabled());
    ui->sourceSimplificationCheckBox->setChecked(settings.isSimplifySource());
    ui->primaryLangComboBox->setCurrentIndex(ui->primaryLangComboBox->findData(settings.primaryLanguage()));
    ui->secondaryLangComboBox->setCurrentIndex(ui->secondaryLangComboBox->findData(settings.secondaryLanguage()));
    ui->forceSourceAutodetectCheckBox->setChecked(settings.isForceSourceAutodetect());
    ui->forceTranslationAutodetectCheckBox->setChecked(settings.isForceTranslationAutodetect());

    // Instance settings
    ui->mozhiUrlComboBox->setCurrentText(settings.instanceUrl());

    // OCR
    ui->convertLineBreaksCheckBox->setChecked(settings.isConvertLineBreaks());
    ui->ocrLanguagesPathEdit->setText(settings.ocrLanguagesPath());
    ui->ocrLanguagesListWidget->setCheckedLanguages(settings.ocrLanguagesString());
    ui->rememberRegionComboBox->setCurrentIndex(settings.regionRememberType());
    ui->captureDelaySpinBox->setValue(settings.captureDelay());
    ui->showMagnifierCheckBox->setChecked(settings.isShowMagnifier());
    ui->confirmOnReleaseCheckBox->setChecked(settings.isConfirmOnRelease());
    ui->applyLightMaskCheckBox->setChecked(settings.isApplyLightMask());
    ui->tesseractParametersTableWidget->setParameters(settings.tesseractParameters());

    // Connection settings
    ui->proxyTypeComboBox->setCurrentIndex(settings.proxyType());
    ui->proxyHostEdit->setText(settings.proxyHost());
    ui->proxyPortSpinbox->setValue(settings.proxyPort());
    ui->proxyAuthCheckBox->setChecked(settings.isProxyAuthEnabled());
    ui->proxyUsernameEdit->setText(settings.proxyUsername());
    ui->proxyPasswordEdit->setText(settings.proxyPassword());

    // Shortcuts
    if (QHotkey::isPlatformSupported()) {
        ui->globalShortcutsCheckBox->setChecked(settings.isGlobalShortuctsEnabled());
    } else {
        ui->globalShortcutsCheckBox->setChecked(false);
        ui->globalShortcutsCheckBox->setEnabled(false);
    }
    ui->shortcutsTreeView->model()->loadShortcuts(settings);
}
