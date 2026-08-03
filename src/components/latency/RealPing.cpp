#include "RealPing.hpp"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <algorithm>

#define QV_MODULE_NAME "RealPingWorker"

namespace Qv2ray::components::latency::realping
{
    RealPing::RealPing(LatencyTestRequest &request, LatencyTestHost *host) : req(std::move(request)), testHost(host)
    {
        data.method = req.method;
    }

    RealPing::~RealPing() = default;

    void RealPing::setOnFinished(std::function<void()> f)
    {
        onFinished = std::move(f);
    }

    void RealPing::start()
    {
        if (!GlobalConfig.inboundConfig.useSocks && !GlobalConfig.inboundConfig.useHTTP)
        {
            data.avg = LATENCY_TEST_VALUE_ERROR;
            data.totalCount = req.totalCount;
            data.failedCount = req.totalCount;
            testHost->OnLatencyTestCompleted(req.id, data);
            if (onFinished)
                onFinished();
            return;
        }

        // Build the proxy for the local Qv2ray inbound.
        QNetworkProxy proxy;
        QString listenHost = GlobalConfig.inboundConfig.listenip;
        if (GlobalConfig.inboundConfig.useHTTP)
        {
            proxy.setType(QNetworkProxy::HttpProxy);
            if (listenHost == "0.0.0.0" || listenHost == "::")
                listenHost = (listenHost == "::") ? "::1" : "127.0.0.1";
            proxy.setHostName(listenHost);
            proxy.setPort(GlobalConfig.inboundConfig.httpSettings.port);
            if (GlobalConfig.inboundConfig.httpSettings.useAuth)
            {
                proxy.setUser(GlobalConfig.inboundConfig.httpSettings.account.user);
                proxy.setPassword(GlobalConfig.inboundConfig.httpSettings.account.pass);
            }
        }
        else if (GlobalConfig.inboundConfig.useSocks)
        {
            proxy.setType(QNetworkProxy::Socks5Proxy);
            if (listenHost == "0.0.0.0" || listenHost == "::")
                listenHost = (listenHost == "::") ? "::1" : "127.0.0.1";
            proxy.setHostName(listenHost);
            proxy.setPort(GlobalConfig.inboundConfig.socksSettings.port);
            if (GlobalConfig.inboundConfig.socksSettings.useAuth)
            {
                proxy.setUser(GlobalConfig.inboundConfig.socksSettings.account.user);
                proxy.setPassword(GlobalConfig.inboundConfig.socksSettings.account.pass);
            }
        }

        data.totalCount = 0;
        data.failedCount = 0;
        data.worst = 0;
        data.avg = 0;
        data.best = 0;
        pending = req.totalCount;

        const QUrl url{ GlobalConfig.networkConfig.latencyRealPingTestURL };

        for (int i = 0; i < req.totalCount; ++i)
        {
            // One manager per request: no connection pooling across requests, so each
            // measurement is a fresh connection through the proxy (curl-multi parity).
            auto manager = new QNetworkAccessManager(this);
            manager->setProxy(proxy);

            QNetworkRequest request{ url };
            request.setRawHeader("Connection", "close");
            // Total timeout (connect + transfer), mirrors old CURLOPT_TIMEOUT = 5s.
            request.setTransferTimeout(5000);

            QElapsedTimer elapsed;
            elapsed.start();

            auto reply = manager->get(request);
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, elapsed]()
                    {
                        const long ms = static_cast<long>(elapsed.elapsed());
                        if (reply->error() == QNetworkReply::NoError)
                        {
                            ++successCount;
                            data.avg += ms;
                            data.worst = std::max(data.worst, ms);
                            data.best = std::min(data.best, ms);
                        }
                        else
                        {
                            ++data.failedCount;
                            data.errorMessage = reply->errorString();
                        }
                        reply->deleteLater();
                        checkCompleted();
                    });
        }
    }

    void RealPing::checkCompleted()
    {
        if (--pending != 0)
            return;
        if (data.failedCount == req.totalCount)
            data.avg = LATENCY_TEST_VALUE_ERROR;
        else
            data.errorMessage.clear(), data.avg = data.avg / successCount;

        testHost->OnLatencyTestCompleted(req.id, data);
        if (onFinished)
            onFinished();
    }
} // namespace Qv2ray::components::latency::realping
