#pragma once

// Lazy, on-demand loader for the gRPC-backed stats backend.
//
// The concrete worker lives in the separate `backend_api` shared library, which
// is the ONLY component that links grpc/absl. The main executable never lists
// grpc/absl (or even `backend_api`) in its ELF NEEDED entries: the library is
// dlopen'ed only when the API/stats feature is actually enabled. If it cannot
// be loaded (library missing, incompatible grpc, etc.) we degrade gracefully
// (backend disabled) instead of failing startup.

#include "core/kernel/APIBackendInterface.hpp"

#include <QLibrary>
#include <QObject>

namespace Qv2ray::core::kernel
{
    // Returns a usable IAPIWorker for the given stats port, or nullptr if the
    // backend library could not be loaded / its factory failed. Never throws.
    IAPIWorker *LoadAPIWorker(int statsPort, QObject *parent);

    // Human-readable reason for the last failed load (for logging/UI).
    QString LastAPIWorkerLoadError();
} // namespace Qv2ray::core::kernel