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
class QSizeGrip;
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
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    // Writes the size this pop-up was left at back to the settings, so the next
    // one opens the way the user left this one.
    void saveSizeToSettings();

    static void connectLanguageButtons(LanguageButtonsWidget *popupButtons, const LanguageButtonsWidget *mainWindowButtons);

    Ui::PopupWindow *ui;
    QShortcut *m_closeWindowsShortcut;
    QTimer *m_closeWindowTimer = nullptr;
    // The window is frameless, so nothing else offers an edge to drag.
    QSizeGrip *m_sizeGrip = nullptr;
    // Coalesces a drag's burst of resize events into one write.
    QTimer *m_sizeSaveTimer = nullptr;
    QPointer<MainWindow> m_parent;
    QMetaObject::Connection m_textChangedConnection;
    QTimer *m_syncTimer = nullptr;
    QList<QPair<QComboBox *, QComboBox *>> m_mirroredCombos;
    QList<QPair<QWidget *, QWidget *>> m_mirroredVisibility;
};

#endif // POPUPWINDOW_H
