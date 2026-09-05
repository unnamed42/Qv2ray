#include "core/kernel/APIBackendImpl.hpp"

#include "v2ray_api.grpc.pb.h"
#include "v2ray_api.pb.h"

#include <grpc++/grpc++.h>

#include <QThread>

#include <chrono>
#include <memory>

using namespace v2ray::core::app::stats::command;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

// Check 10 times before telling user that API has failed.
constexpr auto QV2RAY_API_CALL_FAILEDCHECK_THRESHOLD = 30;
constexpr auto Qv2ray_GRPC_ERROR_RETCODE = -1;

namespace Qv2ray::core::kernel
{
    static QMap<StatisticsType, QStringList> DefaultInboundAPIConfig()
    {
        return { { API_INBOUND, { "dokodemo-door", "http", "socks" } } };
    }
    static QMap<StatisticsType, QStringList> DefaultOutboundAPIConfig()
    {
        return { { API_OUTBOUND_PROXY, { "dns", "http", "mtproto", "shadowsocks", "socks", "vmess", "vless", "trojan" } },
                  { API_OUTBOUND_DIRECT, { "freedom" } },
                  { API_OUTBOUND_BLACKHOLE, { "blackhole" } } };
    }

    APIWorkerImpl::APIWorkerImpl(int statsPort_, QObject *parent) : IAPIWorker(parent), statsPort(statsPort_)
    {
        workThread = new QThread(this);
        this->moveToThread(workThread);
        qInfo() << "API Worker initialised.";
        connect(workThread, &QThread::started, this, &APIWorkerImpl::process);
        connect(workThread, &QThread::finished, [] { qInfo() << "API thread stopped"; });
        started = true;
        workThread->start();
    }

    void APIWorkerImpl::StartAPI(const QMap<bool, QMap<QString, QString>> &tagProtocolPair)
    {
        // Discard the static config map; instead store the raw tag/protocol pairs
        // and classify them when polling. Keeps this impl free of any ODR-globals.
        apiTags = tagProtocolPair;
        running = true;
    }

    void APIWorkerImpl::StopAPI()
    {
        running = false;
    }

    APIWorkerImpl::~APIWorkerImpl()
    {
        StopAPI();
        started = false;
        if (workThread)
        {
            workThread->quit();
            workThread->wait();
        }
    }

    void APIWorkerImpl::process()
    {
        qInfo() << "API Worker started.";
        while (started)
        {
            QThread::msleep(1000);
            bool dialed = false;
            int apiFailCounter = 0;

            while (running)
            {
                if (!dialed)
                {
                    const auto channelAddress = "127.0.0.1:" + QString::number(statsPort);
                    qInfo() << "gRPC Version: " << QString::fromStdString(grpc::Version());
                    grpc_channel = grpc::CreateChannel(channelAddress.toStdString(), grpc::InsecureChannelCredentials());
                    v2ray::core::app::stats::command::StatsService service;
                    stats_service_stub = service.NewStub(grpc_channel);
                    dialed = true;
                }
                if (apiFailCounter == QV2RAY_API_CALL_FAILEDCHECK_THRESHOLD)
                {
                    qWarning() << "API call failure threshold reached, cancelling further API calls.";
                    emit onAPIErrored(QStringLiteral("Failed to get statistics data, please check if V2Ray is running properly"));
                    apiFailCounter++;
                    QThread::msleep(1000);
                    continue;
                }
                else if (apiFailCounter > QV2RAY_API_CALL_FAILEDCHECK_THRESHOLD)
                {
                    // Ignored future requests.
                    QThread::msleep(1000);
                    continue;
                }

                QMap<StatisticsType, QvStatsSpeed> statsResult;
                bool hasError = false;
                for (auto it = apiTags.cbegin(); it != apiTags.cend(); ++it)
                {
                    const bool isOutbound = it.key();
                    for (const auto &[tag, protocol] : it.value().toStdMap())
                    {
                        const auto configFor = isOutbound ? DefaultOutboundAPIConfig() : DefaultInboundAPIConfig();
                        StatisticsType type = API_INBOUND;
                        for (auto cfg = configFor.cbegin(); cfg != configFor.cend(); ++cfg)
                        {
                            if (cfg.value().contains(protocol))
                                type = cfg.key();
                        }
                        const QString prefix = isOutbound ? "outbound" : "inbound";
                        const auto value_up = CallStatsAPIByName(prefix % ">>>" % tag % ">>>traffic>>>uplink");
                        const auto value_down = CallStatsAPIByName(prefix % ">>>" % tag % ">>>traffic>>>downlink");
                        hasError = hasError || value_up == Qv2ray_GRPC_ERROR_RETCODE || value_down == Qv2ray_GRPC_ERROR_RETCODE;
                        statsResult[type].first += std::max(value_up, 0LL);
                        statsResult[type].second += std::max(value_down, 0LL);
                    }
                }
                apiFailCounter = hasError ? apiFailCounter + 1 : 0;
                emit onAPIDataReady(statsResult);
                QThread::msleep(1000);
            } // end while running
        }     // end while started

        workThread->quit();
    }

    qint64 APIWorkerImpl::CallStatsAPIByName(const QString &name)
    {
        ClientContext context;
        // Bound the RPC so a missing/unreachable v2ray-core (API disabled, core
        // not listening on the stats port) fails fast instead of hanging this
        // thread forever. Matches the old CURLOPT_TIMEOUT-style behaviour; the
        // caller counts failures and eventually surfaces onAPIErrored.
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
        GetStatsRequest request;
        GetStatsResponse response;
        request.set_name(name.toStdString());
        request.set_reset(true);

        const auto status = stats_service_stub->GetStats(&context, request, &response);
        if (!status.ok())
        {
            qWarning() << "API call returns: " << status.error_code() << " (" << QString::fromStdString(status.error_message()) << ")";
            return Qv2ray_GRPC_ERROR_RETCODE;
        }
        else
        {
            return response.stat().value();
        }
    }
} // namespace Qv2ray::core::kernel

// ---------------------------------------------------------------------------
// C ABI factory entrypoint. This is the ONLY exported symbol that the main
// program resolves via QLibrary/dlopen. Everything gRPC/absl stays inside this
// shared library.
// ---------------------------------------------------------------------------
extern "C" Q_DECL_EXPORT Qv2ray::core::kernel::IAPIWorker *qv2ray_create_api_worker(int statsPort, QObject *)
{
    // The APIWorkerImpl must have NO parent: APIWorkerImpl::moveToThread fails with
    // "Cannot move objects with a parent", which kept the whole worker (incl. the
    // blocking gRPC GetStats) executing on the main thread and froze the UI/tray.
    // Lifttime is managed by the caller (`V2RayKernelInstance` deletes it); the
    // destructor stops its thread before teardown.
    return new Qv2ray::core::kernel::APIWorkerImpl(statsPort, nullptr);
}