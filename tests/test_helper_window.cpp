/*
 * SPDX-FileCopyrightText: 2025 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // --text-file takes precedence: the launcher's shell-command string is
    // naively whitespace-split (not real shell parsing - confirmed
    // empirically, quoting does not survive), so a multi-word value cannot
    // be passed as a single --text argv token. A file path never contains
    // spaces, sidestepping the problem entirely.
    const QStringList args = app.arguments();
    QString text = QStringLiteral("The quick brown fox jumps over the lazy dog");
    const int textFileArgIndex = args.indexOf(QStringLiteral("--text-file"));
    const int textArgIndex = args.indexOf(QStringLiteral("--text"));
    if (textFileArgIndex >= 0 && textFileArgIndex + 1 < args.size()) {
        QFile file(args.at(textFileArgIndex + 1));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(file.readAll()).trimmed();
    } else if (textArgIndex >= 0 && textArgIndex + 1 < args.size()) {
        text = args.at(textArgIndex + 1);
    }

    QWidget window;
    window.setWindowTitle("Test Helper Window");
    window.resize(400, 200);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    const bool wantsFocus = args.contains(QStringLiteral("--focus"));

    QLabel *label = new QLabel(text, &window);
    // Both mouse-drag selection (for e2e scenarios that click-drag) and
    // keyboard selection (Ctrl+A -> Select All, which works focus-free of a
    // real mouse) need to work here - whether that sets the system PRIMARY
    // selection or just the regular clipboard depends on the compositor
    // (see Selection::getSelection()'s supportsSelection() branch).
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    // setFocusPolicy alone (regardless of an explicit setFocus() call
    // below) makes Qt auto-focus this label on show, since it's the only
    // focusable widget in the window - confirmed empirically: a stray
    // blinking text cursor still appeared with no --focus flag and no
    // setFocus() call at all. Gate the policy itself on --focus, not just
    // the setFocus() call, or OCR staging (which never wants a cursor -
    // see below) gets one anyway.
    label->setFocusPolicy(wantsFocus ? Qt::StrongFocus : Qt::NoFocus);
    label->setStyleSheet("QLabel { font-size: 24px; padding: 20px; }");
    label->setWordWrap(true);
    layout->addWidget(label);

    QLabel *instructionLabel = new QLabel("Select this text or take a screenshot for testing", &window);
    instructionLabel->setStyleSheet("QLabel { font-size: 12px; color: gray; }");
    layout->addWidget(instructionLabel);

    window.show();
    // Only focus (and thus show a blinking text cursor) when a caller
    // actually needs keyboard-driven selection (Ctrl+A) - e.g. OCR staging
    // never focuses this, since it drag-selects by mouse/pixel region and
    // a stray cursor glyph inside the drag rectangle gets OCR'd as a bogus
    // extra character (confirmed empirically: recognized text came back
    // with a trailing "]"/"'" artifact).
    if (wantsFocus)
        label->setFocus();

    QTimer::singleShot(60000, &app, &QApplication::quit);

    return app.exec();
}
