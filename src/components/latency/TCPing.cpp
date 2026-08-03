#include "TCPing.hpp"

#include <QNetworkProxy>
#include <QTcpSocket>
#include <QTimer>

#define QV_MODULE_NAME "TCPingWorker"

namespace Qv2ray::components::latency::tcping
{
    void TCPing::start()
    {
        data.totalCount = req.totalCount;
        data.failedCount = 0;
        data.worst = 0;
        data.avg = 0;
        data.best = 0;
        startResolve();
    }

    TCPing::~TCPing() = default;

    void TCPing::onHostResolved()
    {
        startConnections();
    }

    void TCPing::startConnections()
    {
        pending = req.totalCount;
        for (int i = 0; i < req.totalCount; ++i)
        {
            auto sock = new QTcpSocket(this);
            // Latency test must go directly to the target, bypassing any
            // application/system proxy that QTcpSocket would otherwise honor.
            sock->setProxy(QNetworkProxy::NoProxy);

            const qint64 startMs = QDateTime::currentMSecsSinceEpoch();

            connect(sock, &QTcpSocket::connected, this, [this, sock, startMs]() { onConnected(sock, startMs); });
            connect(sock, &QTcpSocket::errorOccurred, this, [this, sock](QAbstractSocket::SocketError) { onError(sock, sock->errorString()); });

            // Per-connection connect timeout.
            QTimer::singleShot(conn_timeout_ms, sock, [sock]() { sock->abort(); });

            sock->connectToHost(targetAddress, req.port);
        }
    }

    void TCPing::onConnected(QTcpSocket *sock, qint64 startMs)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const long ms = static_cast<long>(now - startMs);
        ++successCount;
        data.avg += ms;
        data.worst = std::max(data.worst, ms);
        data.best = std::min(data.best, ms);
        sock->deleteLater();
        checkCompleted();
    }

    void TCPing::onError(QTcpSocket *sock, const QString &err)
    {
        ++data.failedCount;
        data.errorMessage = err;
        sock->deleteLater();
        checkCompleted();
    }

    void TCPing::checkCompleted()
    {
        if (--pending != 0)
            return;
        if (data.failedCount == req.totalCount)
            data.avg = LATENCY_TEST_VALUE_ERROR;
        else
            data.errorMessage.clear(), data.avg = data.avg / successCount;

        notifyCompleted();
        finish();
    }
} // namespace Qv2ray::components::latency::tcping
