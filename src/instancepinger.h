/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INSTANCEPINGER_H
#define INSTANCEPINGER_H

#include <QElapsedTimer>
#include <QList>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QElapsedTimer;
class QTimer;

class InstancePinger : public QObject
{
    Q_OBJECT

public:
    explicit InstancePinger(QObject *parent = nullptr);

    void detectFastest();
    const QString &fastestInstance() const;

    static const QStringList &instances();

signals:
    void finished();
    void processingInstance(size_t instanceIndex);

private slots:
    void timeout();
    void reactOnResponse();

private:
    void pingNextUrl();

    static const QStringList s_instances;
    static constexpr int s_maxTimeout = 2000;

    QNetworkAccessManager *m_networkManager;
    QElapsedTimer m_elapsedTimer;
    QTimer *m_timeoutTimer;
    QPointer<QNetworkReply> m_currentReply;
    QString m_fastestUrl;
    int m_bestTime = s_maxTimeout;
    size_t m_currentIndex = 0;
};

#endif // INSTANCEPINGER_H
