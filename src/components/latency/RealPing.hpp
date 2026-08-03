#pragma once
#include "LatencyTest.hpp"
#include "base/Qv2rayBase.hpp"

#include <QObject>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace Qv2ray::components::latency::realping
{
    /// Real (HTTP) latency test: performs a real HTTP request through the local
    /// Qv2ray inbound proxy and measures the round-trip time.
    ///
    /// Uses Qt's QNetworkAccessManager instead of libcurl, so no external HTTP
    /// library is required. Each request gets its own QNetworkAccessManager with a
    /// "Connection: close" header, so every measurement is a fresh connection
    /// through the proxy (matching the old curl-multi behaviour).
    class RealPing : public QObject
    {
        Q_OBJECT
      public:
        RealPing(LatencyTestRequest &req, LatencyTestHost *testHost);
        ~RealPing() override;
        void setOnFinished(std::function<void()> f);
        void start();

      private:
        void checkCompleted();

        LatencyTestRequest req;
        LatencyTestResult data;
        LatencyTestHost *testHost;
        std::function<void()> onFinished;
        int successCount = 0;
        int pending = 0;
    };
} // namespace Qv2ray::components::latency::realping
