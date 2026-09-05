#pragma once

// Pure-Qt ABI-stable facade for the gRPC-backed V2Ray stats API.
//
// The actual gRPC/absl implementation lives in the separate dynamic library
// `backend_api.so` (or `backend_api.dll`), which we load lazily at runtime via
// QLibrary. This header carries NO gRPC / absl / protobuf includes on purpose:
// the main executable must not link or load those libraries at startup, so that
// upgrading them never causes "cannot open shared library" on a fresh boot when
// the API/stats feature is disabled.
//
// Only Qt types cross the boundary: QObject signals + QMap<StatisticsType,QvStatsSpeed>.

#include "base/models/QvConfigIdentifier.hpp"

#include <QObject>
#include <QString>

namespace Qv2ray::core::kernel
{
    // Abstract worker that polls v2ray-core's gRPC stats service.
    //
    // Implemented inside the backend shared library. All out-of-band data flows
    // over Qt signals/slots (meta-object system, ABI-stable), and error messages
    // are strings — no grpc/absl types ever cross this interface.
    class IAPIWorker : public QObject
    {
        Q_OBJECT

      public:
        explicit IAPIWorker(QObject *parent = nullptr) : QObject(parent)
        {
        }
        ~IAPIWorker() override = default;

        virtual void StartAPI(const QMap<bool, QMap<QString, QString>> &tagProtocolPair) = 0;
        virtual void StopAPI() = 0;

      signals:
        void onAPIDataReady(const QMap<StatisticsType, QvStatsSpeed> &data);
        void onAPIErrored(const QString &err);
    };

    // C ABI factory entrypoint, exported by the backend shared library.
    // Returns a heap-allocated IAPIWorker (owned by the caller) or nullptr on
    // failure. `statsPort` is the v2ray-core stats listener port.
    using CreateAPIWorkerFunc = IAPIWorker *(*)(int statsPort, QObject *parent);

    // Reverse lookup: name of the exported factory symbol inside backend library.
    inline const char *APIWorkerFactorySymbolName()
    {
        return "qv2ray_create_api_worker";
    }

    // Platform-neutral short name (without extension / `lib` prefix) of the backend
    // library, matching the CMake target `backend_api` (→ `libbackend_api.so` /
    // `libbackend_api.dll`). Must stay in sync with the add_library target name.
    inline const char *APIWorkerLibraryName()
    {
        return "backend_api";
    }
} // namespace Qv2ray::core::kernel