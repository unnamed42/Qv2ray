#pragma once

#include <QtGlobal>
#ifdef Q_OS_WIN

#include "../DNSBase.hpp"

#include <QObject>
#include <QTimer>

namespace Qv2ray::components::latency::icmping
{
    class ICMPPing : public DNSBase<ICMPPing>
    {
        Q_OBJECT
      public:
        using DNSBase<ICMPPing>::DNSBase;
        ~ICMPPing();

      public:
        static const uint64_t DEFAULT_TIMEOUT = 10000U;
        void start();

      protected:
        void onHostResolved() override;

      private:
        void ping();
        void pingImpl();
        bool notifyTestHost(LatencyTestHost *testHost, const ConnectionId &id);

      private:
        uint64_t timeout = DEFAULT_TIMEOUT;
        QTimer waitHandleTimer;
    };
} // namespace Qv2ray::components::latency::icmping
#endif
