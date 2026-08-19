/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef POPUPWINDOW_H
#define POPUPWINDOW_H

#include <QList>
#include <QPointer>
#include <QWidget>

class QComboBox;
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

signals:
    void windowReady();

private:
    void loadSettings();
    // Re-copies the mirrored controls' contents, selection and visibility from
    // the main window. Called on construction and again whenever the original
    // changes, because the pop-up outlives the state it was built from.
    void syncFromMainWindow();
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

    static void connectLanguageButtons(LanguageButtonsWidget *popupButtons, const LanguageButtonsWidget *mainWindowButtons);

    Ui::PopupWindow *ui;
    QShortcut *m_closeWindowsShortcut;
    QTimer *m_closeWindowTimer = nullptr;
    QPointer<MainWindow> m_parent;
    QMetaObject::Connection m_textChangedConnection;
    QTimer *m_syncTimer = nullptr;
    QList<QPair<QComboBox *, QComboBox *>> m_mirroredCombos;
    QList<QPair<QWidget *, QWidget *>> m_mirroredVisibility;
};

#endif // POPUPWINDOW_H
