#pragma once
#include "DNSBase.hpp"
#include "LatencyTest.hpp"

#include <QObject>

class QTcpSocket;

namespace Qv2ray::components::latency::tcping
{
    class TCPing : public DNSBase<TCPing>
    {
        Q_OBJECT
      public:
        using DNSBase<TCPing>::DNSBase;
        void start();
        ~TCPing() override;

      protected:
        void onHostResolved() override;

      private:
        void startConnections();
        void onConnected(QTcpSocket *sock, qint64 startMs);
        void onError(QTcpSocket *sock, const QString &err);
        void checkCompleted();

        int conn_timeout_ms = 5000;
        int pending = 0;
    };
} // namespace Qv2ray::components::latency::tcping
