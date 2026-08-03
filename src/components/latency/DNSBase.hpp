#pragma once
#include "LatencyTest.hpp"
#include "base/Qv2rayBase.hpp"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QObject>
#include <functional>

namespace Qv2ray::components::latency
{
    /// Base class for latency-test worker objects.
    ///
    /// Provides a QObject-based, Qt-native event loop friendly implementation that
    /// replaces the old libuv/uvw based one. Host-name resolution is done
    /// asynchronously through QHostInfo and delivered on the worker thread's event
    /// loop (the thread the object was created in).
    ///
    /// Lifecycle: the owning thread installs a finish callback via setOnFinished().
    /// A worker calls finish() exactly once when the whole test for one ConnectionId
    /// has completed (success or failure); the thread then releases and destroys it.
    template<typename T>
    class DNSBase : public QObject
    {
      public:
        DNSBase(LatencyTestRequest &request, LatencyTestHost *host) : req(std::move(request)), testHost(host)
        {
            data.method = req.method;
        }
        ~DNSBase() override
        {
            if (lookupId >= 0)
                QHostInfo::abortHostLookup(lookupId);
        }
        void setOnFinished(std::function<void()> f)
        {
            onFinished = std::move(f);
        }

      protected:
        /// Called once the target address is known (literal or resolved). Subclasses
        /// start their actual ping here.
        virtual void onHostResolved() = 0;

        /// Resolve req.host into targetAddress (accepting literals directly) and then
        /// invoke onHostResolved() asynchronously. On resolution failure completes the
        /// test with the DNS error.
        void startResolve()
        {
            QHostAddress literal;
            if (literal.setAddress(req.host))
            {
                targetAddress = literal;
                onHostResolved();
                return;
            }
            lookupId = QHostInfo::lookupHost(req.host, this,
                                             [this](const QHostInfo &info)
                                             {
                                                 lookupId = -1;
                                                 if (info.error() != QHostInfo::NoError)
                                                 {
                                                     failWithError(QObject::tr("DNS not resolved"));
                                                     return;
                                                 }
                                                 const auto addrs = info.addresses();
                                                 if (addrs.isEmpty())
                                                 {
                                                     failWithError(QObject::tr("DNS not resolved"));
                                                     return;
                                                 }
                                                 // Prefer IPv4, fall back to whatever the resolver returned.
                                                 targetAddress = addrs.first();
                                                 for (const auto &a : addrs)
                                                 {
                                                     if (a.protocol() == QAbstractSocket::IPv4Protocol)
                                                     {
                                                         targetAddress = a;
                                                         break;
                                                     }
                                                 }
                                                 onHostResolved();
                                             });
        }

        /// Report the (already filled) result to the GUI thread and release this worker.
        void notifyCompleted()
        {
            testHost->OnLatencyTestCompleted(req.id, data);
        }

        /// Emit the DNS-failed result and finish.
        void failWithError(const QString &msg)
        {
            data.errorMessage = msg;
            data.avg = LATENCY_TEST_VALUE_ERROR;
            data.worst = LATENCY_TEST_VALUE_ERROR;
            data.best = LATENCY_TEST_VALUE_ERROR;
            data.totalCount = req.totalCount;
            data.failedCount = req.totalCount;
            notifyCompleted();
            finish();
        }

        /// Let the owning thread know the test is fully done.
        void finish()
        {
            if (onFinished)
                onFinished();
        }

        LatencyTestRequest req;
        LatencyTestResult data;
        LatencyTestHost *testHost = nullptr;
        int successCount = 0;
        QHostAddress targetAddress;
        std::function<void()> onFinished;
        int lookupId = -1;
    };
} // namespace Qv2ray::components::latency
