/*
 *  Copyright © 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 *  Copyright © 2022 Volk Milit <javirrdar@gmail.com>
 *
 *  This file is part of Crow Translate.
 *
 *  Crow Translate is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Crow Translate is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Crow Translate. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SELECTION_H
#define SELECTION_H

#include <QObject>

#ifdef Q_OS_WIN
class QTimer;
class QMimeData;
#endif

class Selection : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Selection)

public:
    ~Selection() override;

    static Selection &instance();

signals:
    void requestedSelectionAvailable(const QString &selection);

public slots:
    void requestSelection();

protected:
    Selection();

private slots:
    void getSelection();

#ifdef Q_OS_WIN
private:
    QScopedPointer<QMimeData> m_originalClipboardData;
    QTimer *m_maxSelectionDelay;
#endif
};

#endif // SELECTION_H
