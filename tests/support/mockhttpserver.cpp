/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mockhttpserver.h"

#include <QHostAddress>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <memory>

MockHttpServer::MockHttpServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &MockHttpServer::onNewConnection);
    m_server.listen(QHostAddress::LocalHost, 0);
}

QString MockHttpServer::baseUrl() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
}

quint16 MockHttpServer::port() const
{
    return m_server.serverPort();
}

void MockHttpServer::queueResponse(const Response &response)
{
    m_pending.append(response);
}

void MockHttpServer::queueJson(int status, const QByteArray &jsonBody)
{
    Response response;
    response.status = status;
    response.body = jsonBody;
    response.headers.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json"));
    queueResponse(response);
}

void MockHttpServer::releaseHeldRequest(int index)
{
    releaseHeldRequest(index, Response());
}

void MockHttpServer::releaseHeldRequest(int index, const Response &response)
{
    if (index < 0 || index >= m_held.size())
        return;

    QTcpSocket *socket = m_held.at(index).socket;
    m_held.removeAt(index);
    if (socket)
        writeResponse(socket, response);
}

bool MockHttpServer::clientDisconnectedEarly() const
{
    return m_clientDisconnectedEarly;
}

int MockHttpServer::requestCount() const
{
    return m_requests.size();
}

QByteArray MockHttpServer::requestBody(int index) const
{
    if (index < 0 || index >= m_requests.size())
        return {};
    return m_requests.at(index).body;
}

QString MockHttpServer::requestPath(int index) const
{
    if (index < 0 || index >= m_requests.size())
        return {};
    return m_requests.at(index).path;
}

void MockHttpServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        m_inProgress.append({socket, QByteArray(), -1, -1});
        connect(socket, &QTcpSocket::readyRead, this, &MockHttpServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void MockHttpServer::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    for (int i = 0; i < m_inProgress.size(); ++i) {
        PendingConnection &conn = m_inProgress[i];
        if (conn.socket != socket)
            continue;

        conn.buffer.append(socket->readAll());

        if (conn.headerEnd < 0)
            conn.headerEnd = conn.buffer.indexOf("\r\n\r\n");
        if (conn.headerEnd < 0)
            return; // headers not complete yet

        if (conn.contentLength < 0) {
            const QByteArray head = conn.buffer.left(conn.headerEnd);
            conn.contentLength = 0;
            for (const QByteArray &line : head.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.startsWith("Content-Length:") || trimmed.startsWith("content-length:")) {
                    conn.contentLength = trimmed.mid(trimmed.indexOf(':') + 1).trimmed().toInt();
                    break;
                }
            }
        }

        const int bodyStart = conn.headerEnd + 4;
        if (conn.buffer.size() - bodyStart < conn.contentLength)
            return; // body not complete yet

        const QByteArray head = conn.buffer.left(conn.headerEnd);
        const QByteArray body = conn.buffer.mid(bodyStart, conn.contentLength);
        m_inProgress.removeAt(i);
        finishRequest(socket, head, body);
        return;
    }
}

void MockHttpServer::finishRequest(QTcpSocket *socket, const QByteArray &head, const QByteArray &body)
{
    const QByteArray requestLine = head.left(head.indexOf("\r\n"));
    const QList<QByteArray> parts = requestLine.split(' ');
    const QString path = parts.size() >= 2 ? QString::fromUtf8(parts.at(1)) : QString();

    const int index = m_requests.size();
    m_requests.append({path, body});
    emit requestReceived(index, path, body);

    if (!m_pending.isEmpty()) {
        const Response response = m_pending.takeFirst();
        if (response.hang) {
            m_held.append({socket});
        } else {
            writeResponse(socket, response);
        }
        return;
    }

    // No canned response queued: respond with an inert empty JSON object so a
    // test that forgot to queue one fails on content mismatch, not on a hang.
    Response fallback;
    fallback.status = 200;
    fallback.body = QByteArrayLiteral("{}");
    fallback.headers.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json"));
    writeResponse(socket, fallback);
}

void MockHttpServer::writeResponse(QTcpSocket *socket, const Response &response)
{
    if (!socket)
        return;

    if (!response.streamChunks.isEmpty()) {
        // Chunked transfer with no Content-Length, each chunk flushed on its
        // own timer so the client sees a genuine trickle.
        QByteArray head = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(response.status) + QByteArrayLiteral(" OK\r\n");
        head += QByteArrayLiteral("Content-Type: text/event-stream\r\n");
        head += QByteArrayLiteral("Transfer-Encoding: chunked\r\n");
        for (auto it = response.headers.constBegin(); it != response.headers.constEnd(); ++it)
            head += it.key().toUtf8() + QByteArrayLiteral(": ") + it.value().toUtf8() + QByteArrayLiteral("\r\n");
        head += QByteArrayLiteral("\r\n");
        socket->write(head);
        socket->flush();

        auto remaining = std::make_shared<QList<QByteArray>>(response.streamChunks);
        // Notice the abort the moment it happens rather than on the next
        // chunk timer, so a test can assert immediately after the client
        // reacts.
        connect(socket, &QTcpSocket::disconnected, this, [this, remaining]() {
            if (!remaining->isEmpty())
                m_clientDisconnectedEarly = true;
        });
        auto writeNext = std::make_shared<std::function<void()>>();
        QPointer<QTcpSocket> guarded(socket);
        MockHttpServer *self = this;
        *writeNext = [self, guarded, remaining, writeNext, delay = response.chunkDelayMs]() {
            if (!guarded || guarded->state() != QAbstractSocket::ConnectedState) {
                // The client hung up before we ran out of chunks: that is the
                // early abort the streaming tests are looking for.
                if (!remaining->isEmpty())
                    self->m_clientDisconnectedEarly = true;
                return;
            }
            if (remaining->isEmpty()) {
                guarded->write(QByteArrayLiteral("0\r\n\r\n"));
                guarded->flush();
                guarded->disconnectFromHost();
                return;
            }
            const QByteArray chunk = remaining->takeFirst();
            guarded->write(QByteArray::number(chunk.size(), 16) + QByteArrayLiteral("\r\n") + chunk + QByteArrayLiteral("\r\n"));
            guarded->flush();
            QTimer::singleShot(delay, guarded, *writeNext);
        };
        QTimer::singleShot(response.delayMs > 0 ? response.delayMs : 0, socket, *writeNext);
        return;
    }

    auto send = [socket, response]() {
        if (!socket)
            return;
        QByteArray out = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(response.status) + QByteArrayLiteral(" OK\r\n");
        out += QByteArrayLiteral("Content-Length: ") + QByteArray::number(response.body.size()) + QByteArrayLiteral("\r\n");
        for (auto it = response.headers.constBegin(); it != response.headers.constEnd(); ++it)
            out += it.key().toUtf8() + QByteArrayLiteral(": ") + it.value().toUtf8() + QByteArrayLiteral("\r\n");
        out += QByteArrayLiteral("Connection: close\r\n\r\n");
        out += response.body;
        socket->write(out);
        socket->flush();
        socket->disconnectFromHost();
    };

    if (response.delayMs > 0)
        QTimer::singleShot(response.delayMs, socket, send);
    else
        send();
}
