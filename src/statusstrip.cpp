/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "statusstrip.h"
#include "ui_statusstrip.h"

#include <QEvent>
#include <QLabel>
#include <QPalette>
#include <QTimer>

using namespace std::chrono_literals;

StatusStrip::StatusStrip(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatusStrip)
    , m_ellipsisTimer(new QTimer(this))
{
    ui->setupUi(this);

    m_readyLabel = new QLabel(tr("Ready"), this);
    m_readyLabel->setObjectName(QStringLiteral("readyLabel"));
    ui->stripLayout->addWidget(m_readyLabel);
    static const char *const segmentNames[] = {"snippingSegmentLabel", "ocrSegmentLabel", "translationSegmentLabel", "ttsSegmentLabel"};
    for (int i = 0; i < ModuleStatus::moduleCount(); ++i) {
        auto *label = new QLabel(this);
        // Named for automated UI testing (findChild) - QLabel reports nothing
        // to AT-SPI without an accessible name either.
        label->setObjectName(QString::fromLatin1(segmentNames[i]));
        // The layout has zero spacing; give every segment after the first a
        // little air so simultaneous segments don't mash together.
        if (i > 0)
            label->setContentsMargins(9, 0, 0, 0);
        m_moduleLabels[static_cast<size_t>(i)] = label;
        ui->stripLayout->addWidget(label);
    }

    // Busy messages are stored dot-free in the model; the view owns the
    // animation, and the timer runs only while something is actually Busy.
    m_ellipsisTimer->setInterval(400ms);
    connect(m_ellipsisTimer, &QTimer::timeout, this, &StatusStrip::renderDots);

    renderModules();
}

StatusStrip::~StatusStrip()
{
    delete ui;
}

void StatusStrip::setModel(ModuleStatus *model)
{
    if (m_model != nullptr)
        disconnect(m_model, &ModuleStatus::changed, this, &StatusStrip::renderModules);
    m_model = model;
    if (model != nullptr)
        connect(model, &ModuleStatus::changed, this, &StatusStrip::renderModules);
    renderModules();
}

void StatusStrip::setHideWhenIdle(bool hide)
{
    m_hideWhenIdle = hide;
    updateVisibility();
}

void StatusStrip::setShown(bool shown)
{
    m_shown = shown;
    renderModules();
}

void StatusStrip::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::LanguageChange:
        m_readyLabel->setText(tr("Ready"));
        renderModules(); // messages are tr()'d on demand by the model
        break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        renderModules(); // the error colour is derived from the palette
        break;
    default:
        QWidget::changeEvent(event);
    }
}

void StatusStrip::renderModules()
{
    if (m_model != nullptr) {
        bool anyActive = false;
        for (int i = 0; i < ModuleStatus::moduleCount(); ++i) {
            const auto module = static_cast<ModuleStatus::Module>(i);
            QLabel *label = m_moduleLabels[static_cast<size_t>(i)];
            const auto activity = m_model->activity(module);

            if (!m_model->isAvailable(module) || activity == ModuleStatus::Activity::Idle) {
                label->hide();
                label->clear();
                label->setAccessibleName({});
                label->setToolTip({});
                continue;
            }

            anyActive = true;
            const bool busy = activity == ModuleStatus::Activity::Busy;
            label->setText(m_model->message(module) + (busy ? dots(m_dotCount) : QString()));
            // The accessible name carries the dot-free message: the animated
            // ellipsis must not rename the widget for AT-SPI/UIA exact-match
            // lookups (and screen readers) four times a second.
            label->setAccessibleName(m_model->message(module));
            label->setToolTip(m_model->detail(module));

            QPalette labelPalette = palette();
            labelPalette.setColor(label->foregroundRole(), busy ? labelPalette.color(QPalette::WindowText) : errorTextColor(this));
            label->setPalette(labelPalette);
            label->show();
        }
        m_readyLabel->setVisible(!anyActive);
        if (!anyActive) {
            // Same identity discipline as the segments below: a label that
            // isn't shown must not keep announcing (or keep a name-matchable
            // identity) through the AT-SPI tree.
            m_readyLabel->setText(tr("Ready"));
            m_readyLabel->setAccessibleName(tr("Ready"));
        } else {
            m_readyLabel->clear();
            m_readyLabel->setAccessibleName({});
        }

        if (m_model->isBusy())
            m_ellipsisTimer->start();
        else {
            m_ellipsisTimer->stop();
            m_dotCount = 0;
        }
    } else {
        for (QLabel *label : m_moduleLabels)
            label->hide();
        m_readyLabel->setText(tr("Ready"));
        m_readyLabel->setAccessibleName(tr("Ready"));
        m_readyLabel->show();
    }

    updateVisibility();
}

void StatusStrip::renderDots()
{
    if (m_model == nullptr)
        return;

    m_dotCount = (m_dotCount + 1) % 4;
    for (int i = 0; i < ModuleStatus::moduleCount(); ++i) {
        const auto module = static_cast<ModuleStatus::Module>(i);
        if (m_model->isAvailable(module) && m_model->activity(module) == ModuleStatus::Activity::Busy)
            m_moduleLabels[static_cast<size_t>(i)]->setText(m_model->message(module) + dots(m_dotCount));
    }
}

bool StatusStrip::hasActiveSegments() const
{
    if (m_model == nullptr)
        return false;

    for (int i = 0; i < ModuleStatus::moduleCount(); ++i) {
        const auto module = static_cast<ModuleStatus::Module>(i);
        if (m_model->isAvailable(module) && m_model->activity(module) != ModuleStatus::Activity::Idle)
            return true;
    }
    return false;
}

void StatusStrip::updateVisibility()
{
    const bool visible = m_shown && (!m_hideWhenIdle || hasActiveSegments());
    if (!visible) {
        // Hide and de-identify the labels themselves, not just this widget:
        // the AT-SPI bridge never prunes interfaces, so a label keeps its
        // name-matchable identity (and keeps announcing to screen readers)
        // unless it is cleared and hidden itself - hiding an ancestor alone
        // does nothing. This covers a strip hidden via its parent
        // (QStatusBar::setVisible(false) from the ShowStatusBar setting)
        // and the hide-while-idle pop-up at rest.
        m_readyLabel->clear();
        m_readyLabel->setAccessibleName({});
        m_readyLabel->hide();
        for (QLabel *label : m_moduleLabels)
            label->hide();
    }
    setVisible(visible);
}

QColor StatusStrip::errorTextColor(const QWidget *widget)
{
    // QPalette has no error role; blend the window text colour toward red so
    // the result stays legible on both light and dark themes.
    const QColor base = widget->palette().color(QPalette::WindowText);
    QColor result;
    result.setRed(static_cast<int>(base.red() * 0.35 + 255 * 0.65));
    result.setGreen(static_cast<int>(base.green() * 0.35));
    result.setBlue(static_cast<int>(base.blue() * 0.35));
    return result;
}

QString StatusStrip::dots(int count)
{
    return QString(count, QLatin1Char('.'));
}
