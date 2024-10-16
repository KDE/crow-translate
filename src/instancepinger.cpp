/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "instancepinger.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <qglobal.h>

const QStringList InstancePinger::s_instanceUrls = {
    QStringLiteral("https://mozhi.aryak.me"),
    QStringLiteral("https://translate.bus-hit.me"),
    QStringLiteral("https://nyc1.mz.ggtyler.dev"),
    QStringLiteral("https://translate.projectsegfau.lt"),
    QStringLiteral("https://translate.nerdvpn.de"),
    QStringLiteral("https://mozhi.ducks.party"),
    QStringLiteral("https://mozhi.frontendfriendly.xyz"),
    QStringLiteral("https://mozhi.pussthecat.org"),
    QStringLiteral("https://mo.zorby.top"),
    QStringLiteral("https://mozhi.adminforge.de"),
    QStringLiteral("https://translate.privacyredirect.com"),
    QStringLiteral("https://mozhi.canine.tools"),
    QStringLiteral("https://mozhi.gitro.xyz"),
};

InstancePinger::InstancePinger(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_elapsedTimer(new QElapsedTimer)
    , m_timeoutTimer(new QTimer)
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &InstancePinger::timeout);

    pingNextUrl();
}

QStringList InstancePinger::instanceUrls()
{
    return s_instanceUrls;
}

void InstancePinger::pingNextUrl()
{
    if (m_currentIndex >= s_instanceUrls.size()) {
        // End of the loop, cleanup and return the obtained results
        const QString bestUrl = m_bestUrl.isEmpty() ? s_instanceUrls.first() : qMove(m_bestUrl);
        qInfo() << tr("Best instance URL is '%1' with time %2 ms").arg(bestUrl).arg(m_bestTime);
        m_currentReply->deleteLater();
        emit finished(bestUrl);
        return;
    }

    QString url = s_instanceUrls.at(m_currentIndex++);

    m_elapsedTimer->restart();
    m_currentReply = m_networkManager->get(QNetworkRequest(url));
    m_timeoutTimer->start(qMin(m_bestTime, s_maxTimeout));

    connect(m_currentReply, &QNetworkReply::finished, this, &InstancePinger::reactOnResponse);
}

void InstancePinger::timeout()
{
    if (m_currentReply != nullptr) {
        qInfo() << tr("Ping to '%1' takes longer then %2 ms").arg(m_currentReply->url().toString()).arg(m_timeoutTimer->interval());
        m_currentReply->abort();
        pingNextUrl();
    }
}

void InstancePinger::reactOnResponse()
{
    m_timeoutTimer->stop();

    const QString url = m_currentReply->url().toString();
    auto responseTime = static_cast<int>(m_elapsedTimer->elapsed());

    switch (m_currentReply->error()) {
    case QNetworkReply::NoError:
        qInfo() << tr("Ping to '%1' successful, response time: %2 ms").arg(url).arg(responseTime);
        if (responseTime < m_bestTime) {
            m_bestTime = responseTime;
            m_bestUrl = url;
        }
        break;
    case QNetworkReply::OperationCanceledError:
        // Do nothing since it's handled in the timeout function.
        // We can't rely solely on cancel here, as sometimes it cancels
        // faster than the operation starts.
        return;
    default:
        qInfo() << tr("Ping to '%1' failed, error: %2").arg(url, m_currentReply->errorString());
        break;
    }

    pingNextUrl();
}
