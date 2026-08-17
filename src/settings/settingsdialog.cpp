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
#include "llm/openaiendpoint.h"
#include "llm/visionmodelprobe.h"
#include "ocr/llmocr.h"
#include "ocr/tesseractocr.h"
#include "shortcutsmodel/shortcutitem.h"
#include "shortcutsmodel/shortcutsmodel.h"
#include "translator/atranslationprovider.h"
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

    ui->ocrLanguagesListWidget->addLanguages(parent->tesseractOcr()->availableLanguages());

    // Set all available instances
    ui->mozhiUrlComboBox->addItems(InstancePinger::instances());
    connect(ui->mozhiUrlComboBox, &QComboBox::currentTextChanged, this, &SettingsDialog::mozhiInstanceChanged);

    buildLocalAiTabs();
    buildOcrEngineUi();

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
    settings.setShowStatusBar(ui->showStatusBarCheckBox->isChecked());
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
    saveOcrEngineSettings();
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
            t.text.timeout->setSuffix(tr(" s"));
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
            t.text.promptModel.clear();
        }

        grid->addWidget(t.text.page, 2, 0, 1, 2);

        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 0);
        grid->setRowStretch(2, 1);

        ui->localAiTabWidget->addTab(outer, AppSettings::localProviderDisplayName(id));

        t.url = url;
        t.apiKey = apiKey;
        t.refresh = refresh;
        m_localTabs.insert(id, t);

        connect(refresh, &QPushButton::clicked, this, [this, id]() {
            refreshLocalModels(id);
        });

        // URL and key are written by saveLocalAiSettings() on accept, so
        // Cancel actually cancels them. refreshLocalModels() reads the widget
        // directly, so probing an unsaved URL still works.

        // Text page signal handlers. The model combo is editable, so its
        // currentTextChanged fires per keystroke - binding a settings write to
        // it saved a model (and a prompt) under every prefix of what was being
        // typed. textActivated only fires when a model is actually picked from
        // the list; everything else is written by saveLocalAiSettings() when
        // the dialog is accepted.
        //
        // Deliberately not QLineEdit::editingFinished: it also fires on
        // focus-out, including the focus-out that ~QDialog() triggers while
        // hiding itself - by then SettingsDialog's own members (m_localTabs
        // among them) have already been destroyed, and the handler crashes.
        connect(t.text.model, &QComboBox::textActivated, this, [this, id](const QString &m) {
            showLocalAiPromptFor(id, m);
        });
        connect(t.text.timeout, qOverload<int>(&QSpinBox::valueChanged), this, [this, id](int val) {
            AppSettings().setLocalAiTimeout(id, val);
        });
        if (t.text.disableThinking) {
            connect(t.text.disableThinking, &QCheckBox::toggled, this, [this, id](bool checked) {
                AppSettings().setLocalAiDisableThinking(id, checked);
            });
        }
    }
}

void SettingsDialog::buildOcrEngineUi()
{
    auto *engineRow = new QWidget(ui->ocrPage);
    auto *engineLayout = new QHBoxLayout(engineRow);
    engineLayout->setContentsMargins(0, 0, 0, 0);

    auto *engineLabel = new QLabel(tr("OCR engine:"), engineRow);
    m_ocrEngineCombo = new QComboBox(engineRow);
    engineLabel->setBuddy(m_ocrEngineCombo);
    m_ocrEngineCombo->setObjectName(QStringLiteral("ocrEngineCombo"));
    m_ocrEngineCombo->addItem(tr("Tesseract"), static_cast<int>(AppSettings::OcrEngine::Tesseract));
    m_ocrEngineCombo->addItem(tr("Vision model (LLM)"), static_cast<int>(AppSettings::OcrEngine::Llm));
    engineLayout->addWidget(engineLabel);
    engineLayout->addWidget(m_ocrEngineCombo);
    engineLayout->addStretch(1);

    m_ocrEngineStack = new QStackedWidget(ui->ocrPage);

    auto *tesseractPage = new QWidget(m_ocrEngineStack);
    auto *tesseractLayout = new QVBoxLayout(tesseractPage);
    tesseractLayout->setContentsMargins(0, 0, 0, 0);
    auto *tesseractNote = new QLabel(tr("Tesseract runs locally. Configure languages and parameters below."), tesseractPage);
    tesseractNote->setWordWrap(true);
    tesseractLayout->addWidget(tesseractNote);
    m_ocrEngineStack->addWidget(tesseractPage);

    auto *llmPage = new QWidget(m_ocrEngineStack);
    auto *llmLayout = new QVBoxLayout(llmPage);
    llmLayout->setContentsMargins(0, 0, 0, 0);
    llmLayout->setSpacing(8);

    auto *providerRow = new QHBoxLayout();
    auto *providerLabel = new QLabel(tr("Provider:"), llmPage);
    m_ocrLlmProviderCombo = new QComboBox(llmPage);
    providerLabel->setBuddy(m_ocrLlmProviderCombo);
    m_ocrLlmProviderCombo->setObjectName(QStringLiteral("ocrLlmProviderCombo"));
    for (const QString &id : AppSettings::localProviderIds()) {
        m_ocrLlmProviderCombo->addItem(AppSettings::localProviderDisplayName(id), id);
    }
    providerRow->addWidget(providerLabel);
    providerRow->addWidget(m_ocrLlmProviderCombo);
    providerRow->addSpacing(12);
    auto *timeoutLabel = new QLabel(tr("Timeout:"), llmPage);
    m_ocrLlmTimeoutSpin = new QSpinBox(llmPage);
    timeoutLabel->setBuddy(m_ocrLlmTimeoutSpin);
    m_ocrLlmTimeoutSpin->setRange(60, 3600);
    m_ocrLlmTimeoutSpin->setSingleStep(30);
    m_ocrLlmTimeoutSpin->setSuffix(tr(" s"));
    m_ocrLlmTimeoutSpin->setToolTip(tr("Maximum time to wait for a transcription response."));
    providerRow->addWidget(timeoutLabel);
    providerRow->addWidget(m_ocrLlmTimeoutSpin);
    providerRow->addStretch(1);
    llmLayout->addLayout(providerRow);

    // The OCR endpoint is configured here in full. It is deliberately not the
    // translation backend's endpoint: transcribing a screenshot and
    // translating a sentence are different jobs, often on different hosts and
    // certainly on different models.
    auto *urlRow = new QHBoxLayout();
    auto *urlLabel = new QLabel(tr("URL:"), llmPage);
    m_ocrLlmUrlEdit = new QLineEdit(llmPage);
    urlLabel->setBuddy(m_ocrLlmUrlEdit);
    m_ocrLlmUrlEdit->setObjectName(QStringLiteral("ocrLlmUrlEdit"));
    auto *apiKeyLabel = new QLabel(tr("Key:"), llmPage);
    m_ocrLlmApiKeyEdit = new QLineEdit(llmPage);
    apiKeyLabel->setBuddy(m_ocrLlmApiKeyEdit);
    m_ocrLlmApiKeyEdit->setObjectName(QStringLiteral("ocrLlmApiKeyEdit"));
    m_ocrLlmApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_ocrLlmApiKeyEdit->setPlaceholderText(tr("optional, e.g. for a cloud endpoint"));
    m_ocrLlmRefreshButton = new QPushButton(tr("Refresh models"), llmPage);
    m_ocrLlmRefreshButton->setObjectName(QStringLiteral("ocrLlmRefreshButton"));
    urlRow->addWidget(urlLabel);
    urlRow->addWidget(m_ocrLlmUrlEdit);
    urlRow->addWidget(apiKeyLabel);
    urlRow->addWidget(m_ocrLlmApiKeyEdit);
    urlRow->addWidget(m_ocrLlmRefreshButton);
    llmLayout->addLayout(urlRow);

    auto *modelRow = new QHBoxLayout();
    auto *modelLabel = new QLabel(tr("Model:"), llmPage);
    m_ocrLlmModelCombo = new QComboBox(llmPage);
    modelLabel->setBuddy(m_ocrLlmModelCombo);
    m_ocrLlmModelCombo->setObjectName(QStringLiteral("ocrLlmModelCombo"));
    m_ocrLlmModelCombo->setEditable(true);
    m_ocrLlmModelCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    modelRow->addWidget(modelLabel);
    modelRow->addWidget(m_ocrLlmModelCombo);
    llmLayout->addLayout(modelRow);

    m_ocrLlmCapabilityHint = new QLabel(llmPage);
    m_ocrLlmCapabilityHint->setWordWrap(true);
    llmLayout->addWidget(m_ocrLlmCapabilityHint);

    m_ocrLlmResetPromptButton = new QPushButton(tr("Reset OCR prompt"), llmPage);
    llmLayout->addWidget(m_ocrLlmResetPromptButton);
    m_ocrLlmPromptEdit = new QPlainTextEdit(llmPage);
    m_ocrLlmPromptEdit->setPlaceholderText(LlmOcr::defaultPrompt());
    llmLayout->addWidget(m_ocrLlmPromptEdit, 1);
    m_ocrEngineStack->addWidget(llmPage);

    if (auto *ocrLayout = ui->ocrPage->findChild<QVBoxLayout *>(QStringLiteral("ocrLayout"))) {
        ocrLayout->insertWidget(0, engineRow);
        ocrLayout->insertWidget(1, m_ocrEngineStack);
    }

    connect(m_ocrEngineCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_ocrEngineStack->setCurrentIndex(index);
        updateOcrEngineVisibility(m_ocrEngineCombo->itemData(index).toInt());
    });
    connect(m_ocrLlmProviderCombo, &QComboBox::currentIndexChanged, this, [this]() {
        loadOcrLlmProvider(m_ocrLlmProviderCombo->currentData().toString());
    });
    // Persist on commit, never per keystroke: an editable combo emits
    // currentTextChanged for every character typed, which used to write a
    // settings key (and a prompt) for each prefix of a model name. See
    // buildLocalAiTabs() for why editingFinished is not used either.
    connect(m_ocrLlmModelCombo, &QComboBox::textActivated, this, [this](const QString &model) {
        showOcrPromptFor(model);
    });
    connect(m_ocrLlmRefreshButton, &QPushButton::clicked, this, &SettingsDialog::refreshOcrModels);
    connect(m_ocrLlmResetPromptButton, &QPushButton::clicked, this, [this]() {
        m_ocrLlmPromptEdit->setPlainText(QString());
    });
}

// Lists every model the last probe returned, grouped by whether the server
// vouched for image support. Nothing is filtered out: most OpenAI-compatible
// endpoints report no capabilities at all, so hiding the unvouched ones would
// hide every working model behind such an endpoint.
void SettingsDialog::populateOcrModelCombo(const QString &id)
{
    const AppSettings settings;
    const QStringList all = settings.ocrLlmModels(id);
    const QStringList vision = settings.ocrLlmVisionModels(id);
    const QString current = m_ocrLlmModelCombo->currentText().isEmpty() ? settings.ocrLlmModel(id)
                                                                        : m_ocrLlmModelCombo->currentText();

    QStringList proven;
    QStringList unproven;
    for (const QString &model : all) {
        (vision.contains(model) ? proven : unproven).append(model);
    }

    QSignalBlocker blocker(m_ocrLlmModelCombo);
    m_ocrLlmModelCombo->clear();

    const auto addHeader = [this](const QString &text) {
        m_ocrLlmModelCombo->addItem(text);
        const int row = m_ocrLlmModelCombo->count() - 1;
        m_ocrLlmModelCombo->setItemData(row, QVariant(), Qt::UserRole - 1); // non-selectable
    };

    const bool grouped = !proven.isEmpty() && !unproven.isEmpty();
    if (grouped) {
        addHeader(tr("— Server reports image support —"));
    }
    m_ocrLlmModelCombo->addItems(proven);
    if (grouped) {
        addHeader(tr("— Image support not reported —"));
    }
    m_ocrLlmModelCombo->addItems(unproven);

    if (!current.isEmpty() && m_ocrLlmModelCombo->findText(current) < 0) {
        m_ocrLlmModelCombo->insertItem(0, current);
    }
    m_ocrLlmModelCombo->setCurrentText(current);
    blocker.unblock();

    widenComboPopup(m_ocrLlmModelCombo);

    if (all.isEmpty()) {
        m_ocrLlmCapabilityHint->setText(tr("No models loaded yet — press \"Refresh models\", or type a model name."));
    } else if (!VisionModelProbe::reportsCapabilities(id)) {
        m_ocrLlmCapabilityHint->setText(tr("This endpoint does not report model capabilities, so image support cannot be verified here. "
                                           "Pick a model you know accepts images."));
    } else if (proven.isEmpty()) {
        m_ocrLlmCapabilityHint->setText(tr("The server reported no image-capable models. A model listed here may still work, "
                                           "but recognition will fail if it cannot accept images."));
    } else {
        m_ocrLlmCapabilityHint->setText(tr("%n model(s) reported as image-capable.", nullptr, static_cast<int>(proven.size())));
    }
}

void SettingsDialog::loadOcrLlmProvider(const QString &id)
{
    storeOcrPromptEdits();

    const AppSettings settings;
    {
        QSignalBlocker urlBlocker(m_ocrLlmUrlEdit);
        m_ocrLlmUrlEdit->setText(settings.ocrLlmUrl(id));
        m_ocrLlmUrlEdit->setPlaceholderText(AppSettings::defaultLocalProviderUrl(id));
        QSignalBlocker keyBlocker(m_ocrLlmApiKeyEdit);
        m_ocrLlmApiKeyEdit->setText(settings.ocrLlmApiKey(id));
        QSignalBlocker timeoutBlocker(m_ocrLlmTimeoutSpin);
        m_ocrLlmTimeoutSpin->setValue(settings.ocrLlmTimeout(id));
        QSignalBlocker modelBlocker(m_ocrLlmModelCombo);
        m_ocrLlmModelCombo->setCurrentText(settings.ocrLlmModel(id));
    }
    populateOcrModelCombo(id);

    m_ocrLlmPromptModel.clear();
    showOcrPromptFor(m_ocrLlmModelCombo->currentText());
}

// Files whatever is in the prompt editor under the model it was written for,
// before the selection moves on to a different model.
void SettingsDialog::storeOcrPromptEdits()
{
    if (m_ocrLlmPromptModel.isEmpty()) {
        return;
    }
    AppSettings settings;
    const QString edited = m_ocrLlmPromptEdit->toPlainText();
    if (edited != settings.ocrLlmPrompt(m_ocrLlmPromptModel)) {
        settings.setOcrLlmPrompt(m_ocrLlmPromptModel, edited);
    }
}

void SettingsDialog::showOcrPromptFor(const QString &model)
{
    if (model == m_ocrLlmPromptModel) {
        return;
    }
    storeOcrPromptEdits();
    m_ocrLlmPromptModel = model;
    m_ocrLlmPromptEdit->setPlainText(AppSettings().ocrLlmPrompt(model));
}

void SettingsDialog::refreshOcrModels()
{
    const QString id = m_ocrLlmProviderCombo->currentData().toString();
    QString base = m_ocrLlmUrlEdit->text().trimmed();
    if (base.isEmpty()) {
        base = AppSettings::defaultLocalProviderUrl(id);
    }
    if (base.isEmpty()) {
        QMessageBox::warning(this, tr("Vision model"), tr("Enter the endpoint URL first."));
        return;
    }

    if (m_ocrLlmProbe == nullptr) {
        m_ocrLlmProbe = new VisionModelProbe(this);
        connect(m_ocrLlmProbe, &VisionModelProbe::finished, this, [this](const QStringList &all, const QStringList &vision) {
            m_ocrLlmRefreshButton->setEnabled(true);
            const QString probedId = m_ocrLlmProviderCombo->currentData().toString();
            if (all.isEmpty()) {
                QMessageBox::information(this, AppSettings::localProviderDisplayName(probedId), tr("No models found."));
                return;
            }
            AppSettings settings;
            settings.setOcrLlmModels(probedId, all);
            settings.setOcrLlmVisionModels(probedId, vision);
            populateOcrModelCombo(probedId);
        });
        connect(m_ocrLlmProbe, &VisionModelProbe::failed, this, [this](const QString &error) {
            m_ocrLlmRefreshButton->setEnabled(true);
            const QString probedId = m_ocrLlmProviderCombo->currentData().toString();
            QMessageBox::warning(this,
                                 AppSettings::localProviderDisplayName(probedId),
                                 tr("Could not reach %1 at %2:\n%3")
                                     .arg(AppSettings::localProviderDisplayName(probedId), m_ocrLlmUrlEdit->text(), error));
        });
    }

    m_ocrLlmRefreshButton->setEnabled(false);
    m_ocrLlmProbe->probe(id, base, m_ocrLlmApiKeyEdit->text());
}

void SettingsDialog::updateOcrEngineVisibility(int engineValue)
{
    const bool isTesseract = (engineValue == static_cast<int>(AppSettings::OcrEngine::Tesseract));
    ui->languagesGroupBox->setVisible(isTesseract);
    ui->ocrParametersGroupBox->setVisible(isTesseract);
    // screenCaptureGroupBox is engine-agnostic and stays visible always.
}

void SettingsDialog::loadOcrEngineSettings()
{
    const AppSettings settings;
    const int index = m_ocrEngineCombo->findData(static_cast<int>(settings.ocrEngine()));
    if (index >= 0) {
        m_ocrEngineCombo->setCurrentIndex(index);
    }
    const QString providerId = settings.ocrLlmProvider();
    const int providerIndex = m_ocrLlmProviderCombo->findData(providerId);
    if (providerIndex >= 0) {
        QSignalBlocker blocker(m_ocrLlmProviderCombo);
        m_ocrLlmProviderCombo->setCurrentIndex(providerIndex);
    }
    loadOcrLlmProvider(m_ocrLlmProviderCombo->currentData().toString());
    updateOcrEngineVisibility(static_cast<int>(settings.ocrEngine()));
}

void SettingsDialog::saveOcrEngineSettings()
{
    AppSettings settings;
    const QString id = m_ocrLlmProviderCombo->currentData().toString();
    const QString model = m_ocrLlmModelCombo->currentText();
    settings.setOcrEngine(static_cast<AppSettings::OcrEngine>(m_ocrEngineCombo->currentData().toInt()));
    settings.setOcrLlmProvider(id);
    settings.setOcrLlmUrl(id, m_ocrLlmUrlEdit->text().trimmed());
    settings.setOcrLlmApiKey(id, m_ocrLlmApiKeyEdit->text());
    settings.setOcrLlmModel(id, model);
    settings.setOcrLlmTimeout(id, m_ocrLlmTimeoutSpin->value());
    // The editor's contents belong to the model that is about to be used - a
    // model typed by hand and never picked from the list included.
    if (!model.isEmpty()) {
        settings.setOcrLlmPrompt(model, m_ocrLlmPromptEdit->toPlainText());
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

        tab.text.promptModel.clear();
        showLocalAiPromptFor(id, textModel);
        tab.text.timeout->setValue(settings.localAiTimeout(id));
        if (tab.text.disableThinking) {
            tab.text.disableThinking->setChecked(settings.localAiDisableThinking(id));
        }
    }

    const int di = ui->detectProviderComboBox->findData(settings.detectProvider());
    {
        QSignalBlocker blocker(ui->detectProviderComboBox);
        if (di >= 0) {
            ui->detectProviderComboBox->setCurrentIndex(di);
        }
    }
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
        settings.setLocalProviderModel(id, tab.text.model->currentText());
        settings.setLocalAiTimeout(id, tab.text.timeout->value());
        if (tab.text.disableThinking) {
            settings.setLocalAiDisableThinking(id, tab.text.disableThinking->isChecked());
        }
        const QString model = tab.text.model->currentText();
        if (!model.isEmpty()) {
            settings.setLocalAiPrompt(model, tab.text.prompt->toPlainText());
        }
    }
}

// Same commit-on-switch discipline as the OCR prompt: whatever is in the
// editor belongs to the model it was typed for, not to whichever model gets
// selected next.
void SettingsDialog::storeLocalAiPromptEdits(const QString &id)
{
    LocalProviderTab &tab = m_localTabs[id];
    if (tab.text.promptModel.isEmpty()) {
        return;
    }
    AppSettings settings;
    const QString edited = tab.text.prompt->toPlainText();
    if (edited != settings.localAiPrompt(tab.text.promptModel)) {
        settings.setLocalAiPrompt(tab.text.promptModel, edited);
    }
}

void SettingsDialog::showLocalAiPromptFor(const QString &id, const QString &model)
{
    LocalProviderTab &tab = m_localTabs[id];
    if (model == tab.text.promptModel) {
        return;
    }
    storeLocalAiPromptEdits(id);
    tab.text.promptModel = model;
    tab.text.prompt->setPlainText(AppSettings().localAiPrompt(model));
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

    QNetworkRequest request(QUrl(OpenAiEndpoint::modelsUrl(base)));
    OpenAiEndpoint::setAuthHeaders(request, AppSettings::localProviderIsAnthropic(id), m_localTabs[id].apiKey->text());

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
    ui->ocrLanguagesListWidget->addLanguages(TesseractOcr::availableLanguages(path));
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
    ui->showStatusBarCheckBox->setChecked(AppSettings::defaultShowStatusBar());

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
    ui->showStatusBarCheckBox->setChecked(settings.isShowStatusBar());

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
    loadOcrEngineSettings();

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
