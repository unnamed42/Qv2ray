#pragma once
#include <QtGlobal>
#ifdef Q_OS_UNIX

#include "../DNSBase.hpp"

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <netinet/in.h>
#include <vector>

namespace Qv2ray::components::latency::icmping
{
    class ICMPPing : public DNSBase<ICMPPing>
    {
        Q_OBJECT
      public:
        using DNSBase<ICMPPing>::DNSBase;
        ~ICMPPing() override;
        void start(int ttl = 30);

      protected:
        void onHostResolved() override;

      private:
        void ping();
        bool notifyTestHost();
        void deinit();
        void onDataAvailable();
        void onTimeout();

        // number incremented with every echo request packet send
        unsigned short seq = 1;
        // socket
        int socketId = -1;
        struct sockaddr_in storage4;
        QTimer timeoutTimer;
        QSocketNotifier *readNotifier = nullptr;
        std::vector<timeval> startTimevals;
    };
} // namespace Qv2ray::components::latency::icmping
#endif
