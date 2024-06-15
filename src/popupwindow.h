/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef POPUPWINDOW_H
#define POPUPWINDOW_H

#include <QWidget>

class QShortcut;
class QTimer;
class MainWindow;
class LanguageButtonsWidget;

namespace Ui
{
class PopupWindow;
}

class PopupWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(PopupWindow)

public:
    explicit PopupWindow(MainWindow *parent = nullptr);
    ~PopupWindow() override;

private:
    void loadSettings();
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

    static void connectLanguageButtons(LanguageButtonsWidget *popupButtons, const LanguageButtonsWidget *mainWindowButtons);

    Ui::PopupWindow *ui;
    QShortcut *m_closeWindowsShortcut;
    QTimer *m_closeWindowTimer = nullptr;
};

#endif // POPUPWINDOW_H
