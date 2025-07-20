/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ocruninitializedtransition.h"

#include "mainwindow.h"
#include "ocr/ocr.h"

#include <QMessageBox>

OcrUninitializedTransition::OcrUninitializedTransition(MainWindow *mainWindow, QState *sourceState)
    : QAbstractTransition(sourceState)
    , m_mainWindow(mainWindow)
{
}

bool OcrUninitializedTransition::eventTest(QEvent * /*event*/)
{
    return m_mainWindow->ocr()->languagesString().isEmpty();
}

void OcrUninitializedTransition::onTransition(QEvent * /*event*/)
{
    QMessageBox::critical(m_mainWindow, Ocr::tr("OCR languages are not loaded"), Ocr::tr("You should set at least one OCR language in the application settings"));
}
