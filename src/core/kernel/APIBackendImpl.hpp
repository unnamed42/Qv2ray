#pragma once

// Backend-internal declaration of the gRPC-backed IAPIWorker implementation.
//
// This header is ONLY compiled into the `backend_api` shared library. It must
// never be included from the main executable, because it pulls in
// grpc++/protobuf generated headers.
//
// Compile-time coupling to gRPC runtime is isolated here so the main program
// can link and boot without any gRPC/absl shared objects present.

#include "core/kernel/APIBackendInterface.hpp"

#include "v2ray_api.grpc.pb.h"

#include <grpc++/grpc++.h>

#include <QThread>

#include <memory>

namespace Qv2ray::core::kernel
{
    // Concrete gRPC-backed worker. Lives entirely inside `backend_api`.
    class APIWorkerImpl : public IAPIWorker
    {
        Q_OBJECT

      public:
        explicit APIWorkerImpl(int statsPort, QObject *parent = nullptr);
        ~APIWorkerImpl() override;

        void StartAPI(const QMap<bool, QMap<QString, QString>> &tagProtocolPair) override;
        void StopAPI() override;

      private slots:
        void process();

      private:
        qint64 CallStatsAPIByName(const QString &name);
        void StopThread();

        QThread *workThread = nullptr;
        bool started = false;
        bool running = false;
        int statsPort;
        QMap<bool, QMap<QString, QString>> apiTags;
        //
        std::shared_ptr<::grpc::Channel> grpc_channel;
        std::unique_ptr<::v2ray::core::app::stats::command::StatsService::Stub> stats_service_stub;
    };
} // namespace Qv2ray::core::kernel