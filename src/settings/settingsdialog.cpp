/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-FileCopyrightText: 2026 Oleksandr Mikriukov <ur3ley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include "instancepinger.h"
#include "instancepingerdialog.h"
#include "mainwindow.h"
#include "qhotkey.h"
#include "screenwatcher.h"
#include "trayicon.h"
#include "autostartmanager/abstractautostartmanager.h"
#include "ocr/ocr.h"
#include "shortcutsmodel/shortcutitem.h"
#include "shortcutsmodel/shortcutsmodel.h"
#include "translator/atranslationprovider.h"
#include "translator/localaitranslationprovider.h"
#include "tts/attsprovider.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

void widenComboPopup(QComboBox *combo)
{
    if (!combo)
        return;
    const QFontMetrics fm(combo->font());
    int w = 0;
    for (int i = 0; i < combo->count(); ++i)
        w = qMax(w, fm.horizontalAdvance(combo->itemText(i)));
    if (w > 0) {
        combo->view()->setMinimumWidth(w + 40);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        int cap = 480;
        if (const QWidget *win = combo->window()) {
            if (win->width() > 240)
                cap = win->width() - 120;
        }
        combo->setMaximumWidth(cap);
    }
}

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
    ui->logoLabel->setPixmap(QIcon::fromTheme(QStringLiteral(APPLICATION_ID)).pixmap(512, 512));
    ui->versionLabel->setText(QCoreApplication::applicationVersion());

#ifdef WITH_PORTABLE_MODE
    m_portableCheckbox->setToolTip(tr("Use %1 to store settings").arg(AppSettings::portableConfigName()));
    qobject_cast<QFormLayout *>(ui->generalGroupBox->layout())->addRow(m_portableCheckbox);
#endif
    // Set item data in comboboxes
    ui->localeComboBox->addItem(tr("<System language>"), AppSettings::defaultLocale());
    for (const QString &locale : LOCALES)
        addLocale(QLocale(locale));

    ui->primaryLangComboBox->addItem(tr("<Auto>"), QVariant::fromValue(Language::autoLanguage()));
    ui->secondaryLangComboBox->addItem(tr("<Auto>"), QVariant::fromValue(Language::autoLanguage()));

    const auto availableLanguages = Language::allLanguages();
    for (const Language &language : availableLanguages) {
        if (language.isValid() && language != Language::autoLanguage()) {
            ui->primaryLangComboBox->addItem(language.displayName(), QVariant::fromValue(language));
            ui->secondaryLangComboBox->addItem(language.displayName(), QVariant::fromValue(language));
        }
    }

    ui->ocrLanguagesListWidget->addLanguages(parent->ocr()->availableLanguages());

    // Set all available instances
    ui->mozhiUrlComboBox->addItems(InstancePinger::instances());
    connect(ui->mozhiUrlComboBox, &QComboBox::currentTextChanged, this, &SettingsDialog::mozhiInstanceChanged);

    buildLocalAiTabs();

    // Adjust tab widgets to fit the scroll area viewport (sidebar occupies ~200 px).
    connect(ui->pagesStackedWidget, &QStackedWidget::currentChanged, this, [this](int /*idx*/) {
        QWidget *vp = ui->scrollArea->viewport();
        if (!vp) {
            return;
        }
        int avail = vp->width() - 40;
        if (avail <= 0) {
            return;
        }
        for (int i = 0; i < ui->localAiTabWidget->count(); ++i) {
            QWidget *w = ui->localAiTabWidget->widget(i);
            if (w) {
                w->setMaximumWidth(avail);
            }
        }
        ui->localAiTabWidget->updateGeometry();
    });

    // Language detection (LocalAI) — on the Translation page.
    const QStringList providerIds = AppSettings::localProviderIds();
    for (const QString &id : providerIds) {
        ui->detectProviderComboBox->addItem(AppSettings::localProviderDisplayName(id), id);
    }
    connect(ui->detectProviderComboBox, &QComboBox::currentIndexChanged, this, [this]() {
        populateDetectModels();
    });

    // Populate translation providers dynamically
    ui->translationProviderComboBox->addItem(tr("Copy"), QVariant::fromValue(ATranslationProvider::ProviderBackend::Copy));
    ui->translationProviderComboBox->addItem("Mozhi", QVariant::fromValue(ATranslationProvider::ProviderBackend::Mozhi));
    ui->translationProviderComboBox->addItem("LocalAI", QVariant::fromValue(ATranslationProvider::ProviderBackend::LocalAI));

    // Populate TTS providers dynamically
    ui->ttsProviderComboBox->addItem(tr("None"), QVariant::fromValue(ATTSProvider::ProviderBackend::None));
#ifdef WITH_TTS
    ui->ttsProviderComboBox->addItem("Mozhi", QVariant::fromValue(ATTSProvider::ProviderBackend::Mozhi));
    ui->ttsProviderComboBox->addItem("Qt", QVariant::fromValue(ATTSProvider::ProviderBackend::Qt));
#endif
#ifdef WITH_PIPER_TTS
    ui->ttsProviderComboBox->addItem("Piper", QVariant::fromValue(ATTSProvider::ProviderBackend::Piper));
#endif

#ifdef WITH_PIPER_TTS
    setupPiperVoicesPathUI();
#endif
#ifndef WITH_TTS
    // Built without TTS support: hide the whole TTS group box
    ui->ttsGroupBox->setVisible(false);
#endif

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
    const ATranslationProvider::ProviderBackend currentBackend = settings.translationProviderBackend();
    const ATranslationProvider::ProviderBackend newBackend = ui->translationProviderComboBox->currentData().value<ATranslationProvider::ProviderBackend>();
    settings.setTranslationProviderBackend(newBackend);

    // Emit signal if backend changed
    if (currentBackend != newBackend) {
        emit translationBackendChanged(newBackend);
    }

    // TTS Settings
    const ATTSProvider::ProviderBackend currentTTSBackend = settings.ttsProviderBackend();
    const ATTSProvider::ProviderBackend newTTSBackend = ui->ttsProviderComboBox->currentData().value<ATTSProvider::ProviderBackend>();
    settings.setTTSProviderBackend(newTTSBackend);

    // Emit signal if TTS backend changed
    if (currentTTSBackend != newTTSBackend) {
        emit ttsBackendChanged(newTTSBackend);
    }
    // Mozhi settings
    settings.setSourceTranslitEnabled(ui->sourceTranslitCheckBox->isChecked());
    settings.setTranslationTranslitEnabled(ui->translationTranslitCheckBox->isChecked());
    settings.setSourceTranscriptionEnabled(ui->sourceTranscriptionCheckBox->isChecked());
    settings.setTranslationOptionsEnabled(ui->translationOptionsCheckBox->isChecked());
    settings.setExamplesEnabled(ui->examplesCheckBox->isChecked());
    settings.setSimplifySource(ui->sourceSimplificationCheckBox->isChecked());
    settings.setPrimaryLanguage(ui->primaryLangComboBox->currentData().value<Language>());
    settings.setSecondaryLanguage(ui->secondaryLangComboBox->currentData().value<Language>());
    settings.setForceSourceAutodetect(ui->forceSourceAutodetectCheckBox->isChecked());
    settings.setForceTranslationAutodetect(ui->forceTranslationAutodetectCheckBox->isChecked());
    // Mozhi instance settings
    settings.setInstance(ui->mozhiUrlComboBox->currentText());
    settings.setLibreTranslateApiKey(ui->mozhiApiKeyEdit->text());
    settings.setLibreTranslateDirect(ui->mozhiDirectCheckBox->isChecked());

    // LocalAI settings
    saveLocalAiSettings();
    settings.setDetectViaLlm(ui->detectViaLlmCheckBox->isChecked());
    settings.setDetectProvider(ui->detectProviderComboBox->currentData().toString());
    settings.setDetectModel(ui->detectModelComboBox->currentText());

    // OCR
    settings.setConvertLineBreaks(ui->convertLineBreaksCheckBox->isChecked());
    settings.setOcrLanguagesPath(ui->ocrLanguagesPathEdit->text().toLocal8Bit());
    settings.setOcrLanguagesString(ui->ocrLanguagesListWidget->checkedLanguagesString());
#ifdef WITH_PIPER_TTS
    if (m_piperVoicesPathEdit != nullptr)
        settings.setPiperVoicesPath(m_piperVoicesPathEdit->text().toLocal8Bit());
#endif
    settings.setRegionRememberType(static_cast<AppSettings::RegionRememberType>(ui->rememberRegionComboBox->currentIndex()));
    settings.setCaptureDelay(ui->captureDelaySpinBox->value());
    settings.setShowMagnifier(ui->showMagnifierCheckBox->isChecked());
    settings.setConfirmOnRelease(ui->confirmOnReleaseCheckBox->isChecked());
    settings.setApplyLightMask(ui->applyLightMaskCheckBox->isChecked());
    settings.setTesseractParameters(ui->tesseractParametersTableWidget->parameters());
    settings.setOcrNegate(ui->negateOcrCheckBox->isChecked());

    // Mozhi proxy settings
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
    // Ignore size policy for hidden pages to show scrollbar only for visible page
    // https://wiki.qt.io/Technical_FAQ#How_can_I_get_a_QStackedWidget_to_automatically_switch_size_depending_on_the_content_of_the_page.3F
    ui->pagesStackedWidget->currentWidget()->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui->pagesStackedWidget->setCurrentIndex(index);
    ui->pagesStackedWidget->currentWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void SettingsDialog::buildLocalAiTabs()
{
    const QStringList providerIds = AppSettings::localProviderIds();
    for (const QString &id : providerIds) {
        auto *outer = new QWidget(ui->localAiTabWidget);
        auto *grid = new QGridLayout(outer);
        grid->setContentsMargins(10, 10, 10, 10);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(8);

        auto *urlLabel = new QLabel(tr("URL:"), outer);
        auto *url = new QLineEdit(outer);
        url->setObjectName(QStringLiteral("localAiUrlEdit"));
        url->setPlaceholderText(AppSettings::defaultLocalProviderUrl(id));
        auto *apiKeyLabel = new QLabel(tr("Key:"), outer);
        auto *apiKey = new QLineEdit(outer);
        apiKey->setObjectName(QStringLiteral("localAiApiKeyEdit"));
        apiKey->setEchoMode(QLineEdit::Password);
        apiKey->setPlaceholderText(tr("optional, e.g. for a cloud endpoint"));
        auto *refresh = new QPushButton(tr("Refresh models"), outer);
        refresh->setObjectName(QStringLiteral("localAiRefreshButton"));
        auto *urlLayout = new QHBoxLayout();
        urlLayout->addWidget(urlLabel);
        urlLayout->addWidget(url);
        urlLayout->addWidget(apiKeyLabel);
        urlLayout->addWidget(apiKey);
        grid->addLayout(urlLayout, 0, 0, Qt::AlignVCenter);
        grid->addWidget(refresh, 0, 1, Qt::AlignRight | Qt::AlignVCenter);

        auto *toggleWidget = new QWidget(outer);
        auto *toggleHL = new QHBoxLayout(toggleWidget);
        toggleHL->setContentsMargins(0, 0, 0, 0);
        toggleHL->setSpacing(0);

        auto *visionToggle = new QPushButton(tr("Vision"), outer);
        visionToggle->setCheckable(true);
        visionToggle->setObjectName(QStringLiteral("visionToggle"));
        auto *textToggle = new QPushButton(tr("Text"), outer);
        textToggle->setCheckable(true);
        textToggle->setObjectName(QStringLiteral("textToggle"));
        textToggle->setChecked(true);
        toggleWidget->setStyleSheet(QStringLiteral(
            "QPushButton#visionToggle:checked, QPushButton#textToggle:checked{font-weight:bold;font-style:normal}"
            "QPushButton#visionToggle:not(:checked), QPushButton#textToggle:not(:checked){font-weight:normal;font-style:italic}"));

        auto *modeGroup = new QButtonGroup(outer);
        modeGroup->setExclusive(true);
        modeGroup->addButton(visionToggle, 0);
        modeGroup->addButton(textToggle, 1);

        auto *helpBtn = new QPushButton(tr("?"), outer);
        helpBtn->setFixedWidth(helpBtn->fontMetrics().horizontalAdvance(QStringLiteral(" ?? ")));
        helpBtn->setToolTip(tr("How to use this tab"));

        toggleHL->addWidget(visionToggle);
        toggleHL->addWidget(textToggle);
        toggleHL->addWidget(helpBtn);
        grid->addWidget(toggleWidget, 1, 1, Qt::AlignRight | Qt::AlignVCenter);

        auto *debugCheck = new QCheckBox(tr("Show both"), outer);
        debugCheck->setToolTip(tr("Debug: show Text and Vision blocks simultaneously"));
        debugCheck->setVisible(false);
        grid->addWidget(debugCheck, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);

        auto *stack = new QStackedWidget(outer);
        LocalProviderTab t;

        {
            t.text.page = new QWidget();
            auto *vl = new QVBoxLayout(t.text.page);
            vl->setContentsMargins(0, 0, 0, 0);
            vl->setSpacing(8);
            auto *modelRow = new QHBoxLayout();
            t.text.modelLabel = new QLabel(tr("Translation model:"), t.text.page);
            t.text.model = new QComboBox(t.text.page);
            t.text.model->setObjectName(QStringLiteral("localAiTextModelCombo"));
            t.text.model->setEditable(true);
            t.text.model->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            modelRow->addWidget(t.text.modelLabel);
            modelRow->addWidget(t.text.model);
            modelRow->addSpacing(12);
            auto *textTimeoutLabel = new QLabel(tr("Timeout:"), t.text.page);
            t.text.timeout = new QSpinBox(t.text.page);
            t.text.timeout->setRange(60, 3600);
            t.text.timeout->setSingleStep(30);
            t.text.timeout->setSuffix(QStringLiteral(" s"));
            t.text.timeout->setToolTip(tr("Maximum time to wait for a translation or detection response."));
            modelRow->addWidget(textTimeoutLabel);
            modelRow->addWidget(t.text.timeout);
            vl->addLayout(modelRow);
            if (id == QLatin1String("ollama")) {
                t.text.disableThinking = new QCheckBox(tr("Disable reasoning"), t.text.page);
                vl->addWidget(t.text.disableThinking);
            }
            auto *sep = new QFrame(t.text.page);
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Sunken);
            vl->addWidget(sep);
            auto *hint = new QLabel(tr("Placeholders: %1").arg(QStringLiteral("{source_lang} {source_code} {target_lang} {target_code} {text}")), t.text.page);
            hint->setWordWrap(true);
            vl->addWidget(hint);
            auto *resetBtn = new QPushButton(tr("Reset Text prompt"), t.text.page);
            vl->addWidget(resetBtn);
            t.text.prompt = new QPlainTextEdit(t.text.page);
            vl->addWidget(t.text.prompt, 1);
            connect(resetBtn, &QPushButton::clicked, this, [this, id]() {
                m_localTabs[id].text.prompt->setPlainText(AppSettings::defaultLocalAiPrompt());
            });
        }

        {
            t.vision.page = new QWidget();
            auto *vl = new QVBoxLayout(t.vision.page);
            vl->setContentsMargins(0, 0, 0, 0);
            vl->setSpacing(8);
            auto *modelRow = new QHBoxLayout();
            t.vision.modelLabel = new QLabel(tr("Vision model:"), t.vision.page);
            t.vision.model = new QComboBox(t.vision.page);
            t.vision.model->setObjectName(QStringLiteral("localAiVisionModelCombo"));
            t.vision.model->setEditable(true);
            t.vision.model->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            modelRow->addWidget(t.vision.modelLabel);
            modelRow->addWidget(t.vision.model);
            modelRow->addSpacing(12);
            auto *visionTimeoutLabel = new QLabel(tr("Timeout:"), t.vision.page);
            t.vision.timeout = new QSpinBox(t.vision.page);
            t.vision.timeout->setRange(60, 3600);
            t.vision.timeout->setSingleStep(30);
            t.vision.timeout->setSuffix(QStringLiteral(" s"));
            t.vision.timeout->setToolTip(tr("Maximum time to wait for a translation or detection response."));
            modelRow->addWidget(visionTimeoutLabel);
            modelRow->addWidget(t.vision.timeout);
            vl->addLayout(modelRow);
            if (id == QLatin1String("ollama")) {
                t.vision.disableThinking = new QCheckBox(tr("Disable reasoning"), t.vision.page);
                vl->addWidget(t.vision.disableThinking);
            }
            auto *sep = new QFrame(t.vision.page);
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Sunken);
            vl->addWidget(sep);
            auto *hint = new QLabel(tr("Placeholders: %1").arg(QStringLiteral("{source_lang} {source_code} {target_lang} {target_code}")), t.vision.page);
            hint->setWordWrap(true);
            vl->addWidget(hint);
            auto *resetBtn = new QPushButton(tr("Reset Vision prompt"), t.vision.page);
            vl->addWidget(resetBtn);
            t.vision.prompt = new QPlainTextEdit(t.vision.page);
            vl->addWidget(t.vision.prompt, 1);
            connect(resetBtn, &QPushButton::clicked, this, [this, id]() {
                m_localTabs[id].vision.prompt->setPlainText(AppSettings::defaultVisionPrompt());
            });
        }

        stack->addWidget(t.vision.page);
        stack->addWidget(t.text.page);
        stack->setCurrentIndex(1);
        grid->addWidget(stack, 2, 0, 1, 2);

        auto *debugContainer = new QWidget(outer);
        auto *debugLayout = new QVBoxLayout(debugContainer);
        debugLayout->setContentsMargins(0, 0, 0, 0);
        debugLayout->setSpacing(12);
        debugContainer->hide();
        grid->addWidget(debugContainer, 2, 0, 1, 2);

        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 0);
        grid->setRowStretch(2, 1);

        ui->localAiTabWidget->addTab(outer, AppSettings::localProviderDisplayName(id));

        t.url = url;
        t.apiKey = apiKey;
        t.refresh = refresh;
        t.visionToggle = visionToggle;
        t.textToggle = textToggle;
        t.modeGroup = modeGroup;
        t.stack = stack;
        t.debugContainer = debugContainer;
        t.debugCheckBox = debugCheck;
        m_localTabs.insert(id, t);

        connect(helpBtn, &QPushButton::clicked, this, [this]() {
            const QString helpText = tr(
                                         "<h3>How the LocalAI tab works</h3>"
                                         "<p><b>Text / Vision modes:</b> Each provider has two independent "
                                         "blocks — <i>Text</i> for translating text and <i>Vision</i> for "
                                         "translating text from images (drag &amp; drop or paste an image "
                                         "into the source field). Switch with the [Vision] [Text] buttons.</p>"
                                         "<p><b>Prompts are per-model:</b> <u>Each model has its own prompt.</u> "
                                         "When you change the model, its saved prompt is loaded. "
                                         "Your edits are saved automatically as you type — no need to click Save.</p>"
                                         "<p><b>Default text prompt:</b><br>"
                                         "<code>%1</code></p>"
                                         "<p><b>Default vision prompt:</b><br>"
                                         "<code>%2</code></p>"
                                         "<p><b>Example — describe a photo (vision mode):</b><br>"
                                         "<code>You are a visual analyst. Examine the image and provide "
                                         "a detailed description in {target_lang} ({target_code}). "
                                         "Include: scene type, subjects, colors, lighting, notable details. "
                                         "If there is text, translate it. Output only the description, "
                                         "3–6 sentences.</code></p>"
                                         "<p><b>Placeholders:</b><br>"
                                         "<code>{source_lang}</code> — source language name<br>"
                                         "<code>{source_code}</code> — source language ISO code<br>"
                                         "<code>{target_lang}</code> — target language name<br>"
                                         "<code>{target_code}</code> — target language ISO code<br>"
                                         "<code>{text}</code> — the input text (text mode only)</p>"
                                         "<p><b>Disable reasoning:</b> Skips thinking tokens on supported "
                                         "models. Only shown for Ollama providers.</p>"
                                         "<p><b>Timeout:</b> Maximum time to wait for a response.</p>")
                                         .arg(AppSettings::defaultLocalAiPrompt().toHtmlEscaped(),
                                              AppSettings::defaultVisionPrompt().toHtmlEscaped());
            auto *msg = new QMessageBox(QMessageBox::Information, tr("LocalAI Tab Help"),
                                        helpText, QMessageBox::Ok, this);
            msg->setMinimumWidth(680);
            msg->exec();
        });

        connect(modeGroup, &QButtonGroup::idClicked, this, [this, id](int mode) {
            LocalProviderTab &tab = m_localTabs[id];
            if (tab.debugCheckBox && tab.debugCheckBox->isChecked()) {
                return;
            }
            tab.stack->setCurrentIndex(mode == 0 ? 0 : 1);
        });

        connect(debugCheck, &QCheckBox::toggled, this, [this, id](bool checked) {
            LocalProviderTab &tab = m_localTabs[id];
            if (checked) {
                const int idx = tab.stack->currentIndex();
                tab.stack->removeWidget(tab.text.page);
                tab.stack->removeWidget(tab.vision.page);
                auto *debugLayout = tab.debugContainer->layout();
                debugLayout->addWidget(tab.text.page);
                debugLayout->addWidget(tab.vision.page);
                tab.text.page->show();
                tab.vision.page->show();
                tab.stack->hide();
                tab.debugContainer->show();
                if (idx == 0) {
                    tab.visionToggle->setChecked(true);
                }
            } else {
                auto *debugLayout = tab.debugContainer->layout();
                debugLayout->removeWidget(tab.text.page);
                debugLayout->removeWidget(tab.vision.page);
                tab.stack->addWidget(tab.vision.page);
                tab.stack->addWidget(tab.text.page);
                tab.stack->setCurrentIndex(tab.modeGroup->checkedId() == 0 ? 0 : 1);
                tab.debugContainer->hide();
                tab.stack->show();
            }
        });

        connect(refresh, &QPushButton::clicked, this, [this, id]() {
            refreshLocalModels(id);
        });

        connect(url, &QLineEdit::textChanged, this, [this, id](const QString &text) {
            AppSettings().setLocalProviderUrl(id, text);
        });

        connect(apiKey, &QLineEdit::textChanged, this, [this, id](const QString &text) {
            AppSettings().setLocalProviderApiKey(id, text);
        });

        // Text page signal handlers
        connect(t.text.model, &QComboBox::currentTextChanged, this, [this, id](const QString &m) {
            LocalProviderTab &tab = m_localTabs[id];
            AppSettings().setLocalProviderModel(id, m);
            const QString p = AppSettings().localAiPrompt(m);
            tab.text.prompt->setPlainText(p);
        });
        connect(t.text.timeout, qOverload<int>(&QSpinBox::valueChanged), this, [this, id](int val) {
            AppSettings().setLocalAiTimeout(id, val);
        });
        connect(t.text.prompt, &QPlainTextEdit::textChanged, this, [this, id]() {
            LocalProviderTab &tab = m_localTabs[id];
            const QString curModel = tab.text.model->currentText();
            if (!curModel.isEmpty()) {
                AppSettings().setLocalAiPrompt(curModel, tab.text.prompt->toPlainText());
            }
        });
        if (t.text.disableThinking) {
            connect(t.text.disableThinking, &QCheckBox::toggled, this, [this, id](bool checked) {
                AppSettings().setLocalAiDisableThinking(id, checked);
            });
        }

        // Vision page signal handlers
        connect(t.vision.model, &QComboBox::currentTextChanged, this, [this, id](const QString &m) {
            LocalProviderTab &tab = m_localTabs[id];
            AppSettings().setLocalVisionModel(id, m);
            const QString p = AppSettings().localVisionPrompt(m);
            tab.vision.prompt->setPlainText(p);
        });
        connect(t.vision.timeout, qOverload<int>(&QSpinBox::valueChanged), this, [this, id](int val) {
            AppSettings().setLocalAiVisionTimeout(id, val);
        });
        connect(t.vision.prompt, &QPlainTextEdit::textChanged, this, [this, id]() {
            LocalProviderTab &tab = m_localTabs[id];
            const QString curModel = tab.vision.model->currentText();
            if (!curModel.isEmpty()) {
                AppSettings().setLocalVisionPrompt(curModel, tab.vision.prompt->toPlainText());
            }
        });
        if (t.vision.disableThinking) {
            connect(t.vision.disableThinking, &QCheckBox::toggled, this, [this, id](bool checked) {
                AppSettings().setLocalAiDisableVisionThinking(id, checked);
            });
        }
    }
}

void SettingsDialog::loadLocalAiSettings()
{
    const AppSettings settings;
    const QStringList providerIds = AppSettings::localProviderIds();
    for (const QString &id : providerIds) {
        LocalProviderTab &tab = m_localTabs[id];
        QSignalBlocker urlBlocker(tab.url);
        tab.url->setText(settings.localProviderUrl(id));
        QSignalBlocker apiKeyBlocker(tab.apiKey);
        tab.apiKey->setText(settings.localProviderApiKey(id));

        const bool isVision = settings.isVisionEnabled(id);
        {
            QSignalBlocker blocker(tab.modeGroup);
            if (isVision) {
                tab.visionToggle->setChecked(true);
                tab.stack->setCurrentIndex(0);
            } else {
                tab.textToggle->setChecked(true);
                tab.stack->setCurrentIndex(1);
            }
        }

        QStringList models = settings.localProviderModels(id);
        const QString textModel = settings.localProviderModel(id);
        if (!textModel.isEmpty() && !models.contains(textModel)) {
            models.prepend(textModel);
        }
        {
            QSignalBlocker b(tab.text.model);
            tab.text.model->clear();
            tab.text.model->addItems(models);
            tab.text.model->setCurrentText(textModel);
        }
        widenComboPopup(tab.text.model);

        const QString visionModel = settings.localVisionModel(id);
        if (!visionModel.isEmpty() && !models.contains(visionModel)) {
            models.prepend(visionModel);
        }
        {
            QSignalBlocker b(tab.vision.model);
            tab.vision.model->clear();
            tab.vision.model->addItems(models);
            tab.vision.model->setCurrentText(visionModel);
        }
        widenComboPopup(tab.vision.model);

        tab.text.prompt->setPlainText(settings.localAiPrompt(textModel));
        tab.vision.prompt->setPlainText(settings.localVisionPrompt(visionModel));
        tab.text.timeout->setValue(settings.localAiTimeout(id));
        tab.vision.timeout->setValue(settings.localAiVisionTimeout(id));
        if (tab.text.disableThinking) {
            tab.text.disableThinking->setChecked(settings.localAiDisableThinking(id));
        }
        if (tab.vision.disableThinking) {
            tab.vision.disableThinking->setChecked(settings.localAiDisableVisionThinking(id));
        }
    }

    const int di = ui->detectProviderComboBox->findData(settings.detectProvider());
    {
        QSignalBlocker blocker(ui->detectProviderComboBox);
        if (di >= 0) {
            ui->detectProviderComboBox->setCurrentIndex(di);
        }
    }
    ui->detectViaLlmCheckBox->setChecked(settings.detectViaLlm());
    populateDetectModels();
    {
        QSignalBlocker blocker(ui->detectModelComboBox);
        ui->detectModelComboBox->setCurrentText(settings.detectModel());
    }
}

void SettingsDialog::saveLocalAiSettings()
{
    AppSettings settings;
    const QStringList providerIds = AppSettings::localProviderIds();
    for (const QString &id : providerIds) {
        const LocalProviderTab &tab = m_localTabs[id];
        settings.setLocalProviderUrl(id, tab.url->text());
        settings.setLocalProviderApiKey(id, tab.apiKey->text());
        settings.setVisionEnabled(id, tab.modeGroup->checkedId() == 0);
        settings.setLocalProviderModel(id, tab.text.model->currentText());
        settings.setLocalVisionModel(id, tab.vision.model->currentText());
        settings.setLocalAiTimeout(id, tab.text.timeout->value());
        settings.setLocalAiVisionTimeout(id, tab.vision.timeout->value());
        if (tab.text.disableThinking) {
            settings.setLocalAiDisableThinking(id, tab.text.disableThinking->isChecked());
        }
        if (tab.vision.disableThinking) {
            settings.setLocalAiDisableVisionThinking(id, tab.vision.disableThinking->isChecked());
        }
        const QString tModel = tab.text.model->currentText();
        if (!tModel.isEmpty()) {
            settings.setLocalAiPrompt(tModel, tab.text.prompt->toPlainText());
        }
        const QString vModel = tab.vision.model->currentText();
        if (!vModel.isEmpty()) {
            settings.setLocalVisionPrompt(vModel, tab.vision.prompt->toPlainText());
        }
    }
}

void SettingsDialog::populateDetectModels()
{
    const QString id = ui->detectProviderComboBox->currentData().toString();
    const QStringList models = AppSettings().localProviderModels(id);
    const QString cur = ui->detectModelComboBox->currentText();
    {
        QSignalBlocker blocker(ui->detectModelComboBox);
        ui->detectModelComboBox->clear();
        ui->detectModelComboBox->addItems(models);
        if (!cur.isEmpty()) {
            ui->detectModelComboBox->setCurrentText(cur);
        }
    }
    widenComboPopup(ui->detectModelComboBox);
}

void SettingsDialog::refreshLocalModels(const QString &id)
{
    QString base = m_localTabs[id].url->text().trimmed();
    if (base.isEmpty()) {
        base = AppSettings::defaultLocalProviderUrl(id);
    }
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }

    QPushButton *refreshButton = m_localTabs[id].refresh;
    if (refreshButton) {
        refreshButton->setEnabled(false);
    }

    QNetworkRequest request(QUrl(base + QStringLiteral("/v1/models")));
    LocalAiTranslationProvider::setAuthHeaders(request, AppSettings::localProviderIsAnthropic(id), m_localTabs[id].apiKey->text());

    auto *manager = new QNetworkAccessManager(this);
    manager->setTransferTimeout(5000);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, id, base, reply, manager]() {
        reply->deleteLater();
        manager->deleteLater();

        QPushButton *refreshButton = m_localTabs.contains(id) ? m_localTabs[id].refresh : nullptr;
        if (refreshButton) {
            refreshButton->setEnabled(true);
        }

        const QString display = AppSettings::localProviderDisplayName(id);
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, display, tr("Could not reach %1 at %2:\n%3").arg(display, base, reply->errorString()));
            return;
        }
        const QJsonArray data = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("data")).toArray();

        QStringList names;
        for (const QJsonValue &m : data) {
            const QString name = m.toObject().value(QStringLiteral("id")).toString();
            if (!name.isEmpty()) {
                names.append(name);
            }
        }
        if (names.isEmpty()) {
            QMessageBox::information(this, display, tr("No models found."));
            return;
        }

        AppSettings().setLocalProviderModels(id, names);

        LocalProviderTab &tab = m_localTabs[id];
        const QString curText = tab.text.model->currentText();
        {
            QSignalBlocker blocker(tab.text.model);
            tab.text.model->clear();
            tab.text.model->addItems(names);
            if (!curText.isEmpty()) {
                tab.text.model->setCurrentText(curText);
            }
        }
        widenComboPopup(tab.text.model);

        const QString curVision = tab.vision.model->currentText();
        {
            QSignalBlocker blocker(tab.vision.model);
            tab.vision.model->clear();
            tab.vision.model->addItems(names);
            if (!curVision.isEmpty()) {
                tab.vision.model->setCurrentText(curVision);
            }
        }
        widenComboPopup(tab.vision.model);

        if (ui->detectProviderComboBox->currentData().toString() == id) {
            populateDetectModels();
        }
    });
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

void SettingsDialog::detectFastestInstance()
{
    ui->detectFastestButton->setEnabled(false);

    auto *dialog = new InstancePingerDialog(this);
    connect(dialog, &InstancePingerDialog::canceled, dialog, [this, dialog]() {
        ui->detectFastestButton->setEnabled(true);
        dialog->deleteLater();
    });
    connect(dialog, &InstancePingerDialog::accepted, dialog, [this, dialog]() {
        ui->mozhiUrlComboBox->setCurrentText(dialog->fastestUrl());
        ui->detectFastestButton->setEnabled(true);
        dialog->deleteLater();
    });
    dialog->show();
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

#ifdef WITH_PIPER_TTS
void SettingsDialog::setupPiperVoicesPathUI()
{
    // Create UI elements
    m_piperVoicesPathLabel = new QLabel(tr("Piper voices path:"), this);
    m_piperVoicesPathEdit = new QLineEdit(this);
    m_piperVoicesPathButton = new QToolButton(this);

    // Create a horizontal layout for the edit + button
    auto *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(m_piperVoicesPathEdit);
    pathLayout->addWidget(m_piperVoicesPathButton);
    pathLayout->setContentsMargins(0, 0, 0, 0);

    auto *pathWidget = new QWidget(this);
    pathWidget->setLayout(pathLayout);

    // Set properties
    m_piperVoicesPathEdit->setPlaceholderText(tr("Directory with voice models"));
    m_piperVoicesPathButton->setToolTip(tr("Select Piper voices path"));
    m_piperVoicesPathButton->setIcon(QIcon::fromTheme(QStringLiteral("folder")));

    // Add to the existing form layout
    auto *formLayout = ui->ttsGroupBox->findChild<QFormLayout *>("ttsFormLayout");
    if (formLayout != nullptr) {
        formLayout->addRow(m_piperVoicesPathLabel, pathWidget);
    }

    // Connect signals
    connect(m_piperVoicesPathButton, &QToolButton::clicked, this, &SettingsDialog::selectPiperVoicesPath);
    connect(m_piperVoicesPathEdit, &QLineEdit::editingFinished, this, [this]() {
        onPiperVoicesPathChanged(m_piperVoicesPathEdit->text());
    });
}

void SettingsDialog::selectPiperVoicesPath()
{
    const QString path = m_piperVoicesPathEdit->text().left(m_piperVoicesPathEdit->text().lastIndexOf(QDir::separator()));
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Select Piper voices path"), path);
    if (!directory.isEmpty())
        m_piperVoicesPathEdit->setText(directory);
}

void SettingsDialog::onPiperVoicesPathChanged(const QString &path)
{
    emit piperVoicesPathChanged(path);
}
#endif

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
    const int defaultTranslationBackendIndex = ui->translationProviderComboBox->findData(QVariant::fromValue(AppSettings().defaultTranslationProviderBackend()));
    if (defaultTranslationBackendIndex != -1) {
        ui->translationProviderComboBox->setCurrentIndex(defaultTranslationBackendIndex);
    }

    // TTS settings
    const int defaultTTSBackendIndex = ui->ttsProviderComboBox->findData(QVariant::fromValue(AppSettings().defaultTTSProviderBackend()));
    if (defaultTTSBackendIndex != -1) {
        ui->ttsProviderComboBox->setCurrentIndex(defaultTTSBackendIndex);
    }

    // Mozhi settings
    ui->sourceTranslitCheckBox->setChecked(AppSettings::defaultSourceTranslitEnabled());
    ui->translationTranslitCheckBox->setChecked(AppSettings::defaultTranslationTranslitEnabled());
    ui->sourceTranscriptionCheckBox->setChecked(AppSettings::defaultSourceTranscriptionEnabled());
    ui->translationOptionsCheckBox->setChecked(AppSettings::defaultTranslationOptionsEnabled());
    ui->examplesCheckBox->setChecked(AppSettings::defaultExamplesEnabled());
    ui->sourceSimplificationCheckBox->setChecked(AppSettings::defaultSimplifySource());
    ui->primaryLangComboBox->setCurrentIndex(ui->primaryLangComboBox->findData(QVariant::fromValue(AppSettings::defaultPrimaryLanguage())));
    ui->secondaryLangComboBox->setCurrentIndex(ui->secondaryLangComboBox->findData(QVariant::fromValue(AppSettings::defaultSecondaryLanguage())));
    ui->forceSourceAutodetectCheckBox->setChecked(AppSettings::defaultForceSourceAutodetect());
    ui->forceTranslationAutodetectCheckBox->setChecked(AppSettings::defaultForceTranslationAutodetect());

    // We don't reset the picked instance

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
    ui->negateOcrCheckBox->setChecked(AppSettings::defaultOcrNegate());

// Piper-voices path
#ifdef WITH_PIPER_TTS
    if (m_piperVoicesPathEdit != nullptr)
        m_piperVoicesPathEdit->setText(AppSettings::defaultPiperVoicesPath());
#endif
    // Mozhi network settings
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
    // Translation provider backend
    const int translationBackendIndex = ui->translationProviderComboBox->findData(QVariant::fromValue(settings.translationProviderBackend()));
    if (translationBackendIndex != -1) {
        ui->translationProviderComboBox->setCurrentIndex(translationBackendIndex);
    }

    // TTS settings
    // TTS provider backend
    const int ttsBackendIndex = ui->ttsProviderComboBox->findData(QVariant::fromValue(settings.ttsProviderBackend()));
    if (ttsBackendIndex != -1) {
        ui->ttsProviderComboBox->setCurrentIndex(ttsBackendIndex);
    }

    // Mozhi settings
    ui->sourceTranslitCheckBox->setChecked(settings.isSourceTranslitEnabled());
    ui->translationTranslitCheckBox->setChecked(settings.isTranslationTranslitEnabled());
    ui->sourceTranscriptionCheckBox->setChecked(settings.isSourceTranscriptionEnabled());
    ui->translationOptionsCheckBox->setChecked(settings.isTranslationOptionsEnabled());
    ui->examplesCheckBox->setChecked(settings.isExamplesEnabled());
    ui->sourceSimplificationCheckBox->setChecked(settings.isSimplifySource());
    ui->primaryLangComboBox->setCurrentIndex(ui->primaryLangComboBox->findData(QVariant::fromValue(settings.primaryLanguage())));
    ui->secondaryLangComboBox->setCurrentIndex(ui->secondaryLangComboBox->findData(QVariant::fromValue(settings.secondaryLanguage())));
    ui->forceSourceAutodetectCheckBox->setChecked(settings.isForceSourceAutodetect());
    ui->forceTranslationAutodetectCheckBox->setChecked(settings.isForceTranslationAutodetect());

    // Temporarily disconnect signal to avoid triggering provider updates while loading settings
    disconnect(ui->mozhiUrlComboBox, &QComboBox::currentTextChanged, this, &SettingsDialog::mozhiInstanceChanged);
    ui->mozhiUrlComboBox->setCurrentText(settings.instance());
    ui->mozhiApiKeyEdit->setText(settings.libreTranslateApiKey());
    ui->mozhiDirectCheckBox->setChecked(settings.libreTranslateDirect());
    connect(ui->mozhiUrlComboBox, &QComboBox::currentTextChanged, this, &SettingsDialog::mozhiInstanceChanged);

    // LocalAI
    loadLocalAiSettings();

    // OCR
    ui->convertLineBreaksCheckBox->setChecked(settings.isConvertLineBreaks());
    ui->ocrLanguagesPathEdit->setText(settings.ocrLanguagesPath());
    ui->ocrLanguagesListWidget->setCheckedLanguages(settings.ocrLanguagesString());
#ifdef WITH_PIPER_TTS
    if (m_piperVoicesPathEdit != nullptr)
        m_piperVoicesPathEdit->setText(settings.piperVoicesPath());
#endif
    ui->rememberRegionComboBox->setCurrentIndex(settings.regionRememberType());
    ui->captureDelaySpinBox->setValue(settings.captureDelay());
    ui->showMagnifierCheckBox->setChecked(settings.isShowMagnifier());
    ui->confirmOnReleaseCheckBox->setChecked(settings.isConfirmOnRelease());
    ui->applyLightMaskCheckBox->setChecked(settings.isApplyLightMask());
    ui->tesseractParametersTableWidget->setParameters(settings.tesseractParameters());
    ui->negateOcrCheckBox->setChecked(settings.isOcrNegate());

    // Mozhi Network/Proxy settings
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
