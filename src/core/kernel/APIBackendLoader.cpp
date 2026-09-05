#include "core/kernel/APIBackendLoader.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLibrary>
#include <QString>

namespace Qv2ray::core::kernel
{
    // Best-effort path for discovering the backend library next to the
    // main binary / in a dedicated libs dir, then the runtime path.
    QString APIWorkerLibraryFile()
    {
        const auto appDir = QCoreApplication::applicationDirPath();
        // Real shared-library filename for the given base dir: on ELF/mach-O the
        // library is `lib<name>.so`/`lib<name>.dylib` (CMake prepends `lib`); on
        // Windows it is `<name>.dll`.
        const auto trySuffix = [&](const QString &base) {
#ifdef Q_OS_WIN
            return base + "/" + QLatin1String(APIWorkerLibraryName()) + ".dll";
#elif defined(Q_OS_MACOS)
            return base + "/lib" + QLatin1String(APIWorkerLibraryName()) + ".dylib";
#else
            return base + "/lib" + QLatin1String(APIWorkerLibraryName()) + ".so";
#endif
        };
        QStringList candidates = {
            // cwd/app dir first (dev builds + portable installs)
            trySuffix(appDir),
            trySuffix(appDir + "/lib"),
            trySuffix(appDir + "/libs"),
        };
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(Q_OS_ANDROID)
        // Linux system/packaged install: <prefix>/share/qv2ray/libs
        candidates << trySuffix(appDir + "/../share/qv2ray/libs");
#elif defined(Q_OS_MACOS)
        // macOS .app bundle: Contents/Resources/libs
        candidates << trySuffix(appDir + "/../Resources/libs");
#endif
        // Only consider candidates that actually exist on disk. (QLibrary::isLibrary
        // returns true even for non-existing paths, which would make us return a bare
        // name and let QLibrary guess at appDir/loader paths — the source of the
        // "/usr/bin/libqv2ray_backend_api.so" error below.)
        for (const auto &c : candidates)
        {
            if (QFileInfo::exists(c))
                return c;
        }
        // No candidate exists: return empty so LoadAPIWorker reports a clear
        // "backend not found" condition instead of making QLibrary guess a path.
        return QString();
    }

    static QString g_lastLoadError;

    IAPIWorker *LoadAPIWorker(int statsPort, QObject *parent)
    {
        g_lastLoadError.clear();
        const QString libPath = APIWorkerLibraryFile();
        if (libPath.isEmpty())
        {
            g_lastLoadError = QObject::tr("gRPC stats backend library (%1) not found next to the application or in its lib dirs.")
                                  .arg(QLatin1String(APIWorkerLibraryName()));
            return nullptr;
        }
        QLibrary lib(libPath);
        if (!lib.load())
        {
            g_lastLoadError = lib.errorString();
            return nullptr;
        }
        auto factory = reinterpret_cast<CreateAPIWorkerFunc>(lib.resolve(APIWorkerFactorySymbolName()));
        if (!factory)
        {
            g_lastLoadError = QObject::tr("Backend library lacks factory symbol: ") +
                              QLatin1String(APIWorkerFactorySymbolName());
            return nullptr;
        }
        IAPIWorker *worker = factory(statsPort, parent);
        if (!worker)
        {
            g_lastLoadError = QObject::tr("Backend factory returned null.");
            return nullptr;
        }
        return worker;
    }

    QString LastAPIWorkerLoadError()
    {
        return g_lastLoadError;
    }
} // namespace Qv2ray::core::kernel