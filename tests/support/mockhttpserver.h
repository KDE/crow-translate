/*
 * SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOCKHTTPSERVER_H
#define MOCKHTTPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTcpServer>

class QTcpSocket;

// Minimal single-purpose HTTP/1.1 responder for pointing translation providers
// (MozhiTranslationProvider::setInstance(), LocalAiTranslationProvider's "url"
// option) at 127.0.0.1 instead of a live third-party service in tests.
//
// One instance per test. Each accepted connection is served the next queued
// canned Response, in order, unless the response has hang=true, in which case
// the connection is held open (no bytes written) until releaseHeldRequest()
// is called for it - this is what makes provider-side timeout and overlapping
// in-flight request races reproducible deterministically instead of depending
// on real network timing.
class MockHttpServer : public QObject
{
    Q_OBJECT
public:
    struct Response {
        int status = 200;
        QByteArray body;
        QHash<QString, QString> headers;
        int delayMs = 0; // write the response after this delay, once the connection is up
        bool hang = false; // accept the connection, record the request, write nothing until released
        // Server-sent-events mode: write each chunk as its own flush, spaced
        // by chunkDelayMs, with no Content-Length. Needed to exercise a
        // client that reacts to the stream as it arrives - LlmOcr aborts the
        // request the moment the transcription starts repeating, and only a
        // real trickle can reproduce that.
        QList<QByteArray> streamChunks;
        int chunkDelayMs = 10;
    };

    explicit MockHttpServer(QObject *parent = nullptr);

    QString baseUrl() const;
    quint16 port() const;

    void queueResponse(const Response &response);
    void queueJson(int status, const QByteArray &jsonBody);

    // Write the queued (or default 200 empty-JSON) response for the Nth held
    // connection, in the order it was recorded by requestReceived().
    void releaseHeldRequest(int index);
    void releaseHeldRequest(int index, const Response &response);

    int requestCount() const;
    // True when a client closed the connection before the server finished
    // writing its stream - i.e. it aborted mid-response. This is how a test
    // asserts that an early abort actually happened rather than the client
    // simply reading everything and discarding it.
    bool clientDisconnectedEarly() const;
    QByteArray requestBody(int index) const;
    QString requestPath(int index) const;

signals:
    void requestReceived(int index, QString path, QByteArray body);

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    struct PendingConnection {
        QPointer<QTcpSocket> socket;
        QByteArray buffer;
        int contentLength = -1;
        int headerEnd = -1;
    };
    struct HeldConnection {
        // A held connection can legitimately outlive its socket - e.g. the
        // client (a QNetworkReply) is abort()ed while its request is still
        // held, which closes the connection and schedules the server-side
        // QTcpSocket for deleteLater(). QPointer auto-nulls in that case
        // instead of leaving a dangling pointer for a later
        // releaseHeldRequest() call to write through.
        QPointer<QTcpSocket> socket;
    };
    struct RecordedRequest {
        QString path;
        QByteArray body;
    };

    void writeResponse(QTcpSocket *socket, const Response &response);
    void finishRequest(QTcpSocket *socket, const QByteArray &head, const QByteArray &body);

    QTcpServer m_server;
    QList<Response> m_pending;
    QList<PendingConnection> m_inProgress;
    QList<HeldConnection> m_held;
    QList<RecordedRequest> m_requests;
    bool m_clientDisconnectedEarly = false;
};

#endif // MOCKHTTPSERVER_H
