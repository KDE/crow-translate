/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATUSSTRIP_H
#define STATUSSTRIP_H

#include "modulestatus.h"

#include <QWidget>

#include <array>

class QLabel;
class QTimer;

namespace Ui
{
class StatusStrip;
} // namespace Ui

// The strip at the bottom of the windows naming what is currently running.
// One label per module plus a "Ready" label, all created up front and
// shown/hidden per ModuleStatus::activity() - no add/remove churn on state
// changes. Only active modules are shown; "Ready" appears when nothing runs.
class StatusStrip : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(StatusStrip)

public:
    explicit StatusStrip(QWidget *parent = nullptr);
    ~StatusStrip() override;

    void setModel(ModuleStatus *model);
    // The pop-up window hides the whole strip at rest so the resting pop-up
    // is unchanged; the main window keeps showing "Ready".
    void setHideWhenIdle(bool hide);
    // Interface/ShowStatusBar pick-up: a strip hidden by the setting stays
    // hidden whatever the model does.
    void setShown(bool shown);

protected:
    void changeEvent(QEvent *event) override;

private:
    void renderModules();
    void renderDots();
    bool hasActiveSegments() const;
    void updateVisibility();

    static QColor errorTextColor(const QWidget *widget);
    static QString dots(int count);

    Ui::StatusStrip *ui;
    ModuleStatus *m_model = nullptr;
    QTimer *m_ellipsisTimer;
    QLabel *m_readyLabel = nullptr;
    std::array<QLabel *, ModuleStatus::moduleCount()> m_moduleLabels{};
    int m_dotCount = 0;
    bool m_hideWhenIdle = false;
    bool m_shown = true;
};

#endif // STATUSSTRIP_H
