# Qv2ray Development Guide

## Status

**No longer maintained** (2019-2021). Latest release v2.7.0. CI and build system still work, but there is no active development. The default branch is `dev`; match your changes to it.

## Project

- Cross-platform Qt-based V2Ray GUI client (Windows/Linux/macOS) with a plugin system (SSR / Trojan / Trojan-Go / NaiveProxy). C++23, GPLv3.
- Entrypoint: `src/main.cpp` selects the app class at compile time via defines:
  - `QV2RAY_GUI_QWIDGETS` → `Qv2rayWidgetApplication` (default, `src/ui/widgets/`)
  - `QV2RAY_GUI_QML` → `Qv2rayQMLApplication` (`src/ui/qml/`)
  - `QV2RAY_CLI` → `Qv2rayCliApplication` (`src/ui/cli/`)
  - `Qv2rayApplication` is a macro mapping to the built variant.
- Source layout: `src/base` (shared foundational types/JSON), `src/components` (geosite, latency, ntp, proxy, route, update…), `src/core` (connection model, kernel, settings, handler), `src/ui` (app shells), `src/plugins` (builtin protocol + subscription-adapter plugins, not the submodule).
- Submodules (must be initialized before build): `src/plugin-interface` (DHR60/QvPlugin-Interface, unrelated to `src/plugins`), plus `3rdparty/` (SingleApplication, QNodeEditor, QCodeEditor, QJsonStruct, qt-qrcode, puresource) and `cmake/android`.

## Commands

```bash
# Initialize submodules first (build fails without them)
git submodule update --init --recursive

# Configure + build (ninja)
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)

# Build tests and run them (BUILD_TESTING is OFF by default)
cmake .. -GNinja -DBUILD_TESTING=ON
cmake --build .
ctest                       # or: ctest -R parse_ss_url for one test
```

- Build options live in `CMakeLists.txt`: `QV2RAY_UI_TYPE` (`QWidget`/`QML`/`CLI`), `BUILD_TESTING`, `QV2RAY_HAS_BUILTIN_PLUGINS`, `QV2RAY_DISABLE_AUTO_UPDATE`, `QV2RAY_USE_V5_CORE`. Qt6 is the **only** supported Qt major version (Qt5 support has been removed).
- Windows: run `libs/setup-libs.sh <DEPS_OS> <DEPS_CATEGORY>` (downloads prebuilt deps; e.g. `win64 msvc` / `win64 tools`). Requires curl and jq.
- Building just the isolated gRPC backend: `ninja backend_api` (produces `libbackend_api.so`). Expected for a normal full build too.

## gRPC / stats backend (ABI isolation)

- The traffic-stats API (V2Ray core gRPC stats service) is the **only** consumer of the gRPC/absl libraries. It is compiled into a separate shared library `backend_api` (target `backend_api`, source `src/core/kernel/APIBackendImpl.{cpp,hpp}`), which is the **only** CMake target that links `QV2RAY_BACKEND_LIBRARY` (grpc++/grpc/gpr + absl).
- The main `qv2ray` executable never links or `#include`s gRPC/absl. It loads `libbackend_api.so` **on demand via `QLibrary`/`dlopen`** (`src/core/kernel/APIBackendLoader.{cpp,hpp}`), only when the API/stats feature is actually enabled in `StartConnection`. If the library can't be loaded, traffic statistics degrade gracefully (disabled) instead of failing startup.
- Cross-boundary contract is pure Qt: `src/core/kernel/APIBackendInterface.hpp` declares the abstract `IAPIWorker` (signals `onAPIDataReady`/`onAPIErrored`) and the C factory symbol `qv2ray_create_api_worker(int statsPort, QObject*)`. No gRPC/absl/protobuf type crosses the boundary.
- Do **not** add grpc/absl includes or links back into `qv2ray_baselib`/`qv2ray` — that would reintroduce the startup `cannot open shared library` failures on grpc upgrades. Keep grpc/absl confined to `backend_api`.
- When upgrading gRPC/absl, only `backend_api` needs rebuilding/redeploying; the main program and plugins are unaffected.
- `backend_api` has `install()` rules so it ships in packaged installs, placed in a dedicated **`libs`** dir (NOT `plugins/`): Linux `share/qv2ray/libs/`, Windows `libs/`, macOS `qv2ray.app/Contents/Resources/libs/`. Keep it out of `plugins/` because the plugin host scans that dir and would show a startup error dialog for this non-plugin library. The loader (`APIBackendLoader.cpp`) searches the app dir, `appDir/lib/`, `appDir/libs/`, plus Linux `appDir/../share/qv2ray/libs/` and macOS `appDir/../Resources/libs/` — keep these in sync if you move the install destination.
- **Loader pitfalls (already fixed, keep them fixed):** `APIWorkerLibraryName()` must equal the CMake `add_library(backend_api ...)` target name (`"backend_api"`), and the loader must prepend the platform `lib` prefix for the actual filename (`libbackend_api.so` / `libbackend_api.dll`). Also the loader must only accept candidates that `QFileInfo::exists` (not bare `QLibrary::isLibrary`, which matches non-existent paths and produces a misleading `/usr/bin/lib…so` error). Do not reintroduce a bare-name fallback.
- **Worker deadline:** `APIWorkerImpl::CallStatsAPIByName` sets a 2s gRPC `ClientContext` deadline. Without it, the synchronous `GetStats` RPC blocks forever when v2ray-core isn't reachable on the stats port (appears as "API Worker started." + "gRPC Version: …" then a hang). Keep this deadline; `OnAPIErrored` is currently not connected to any UI handler, so API failures only degrade stats silently.
- **Critical: APIWorkerImpl must have NO parent.** The factory `qv2ray_create_api_worker` intentionally passes `nullptr` as the QObject parent (ignoring the caller's `parent`). If the object has a parent, `QObject::moveToThread(workThread)` fails with "Cannot move objects with a parent", so `process()` (and the blocking gRPC `GetStats`) runs on the **main thread**, freezing the UI and removing the tray icon while the proxy keeps working. Symptom: "QObject::moveToThread: Cannot move objects with a parent" in the log + tray disappears only after enabling the API backend. Do not re-add a parent.

## Tests

- Catch2. Registered via the `ADD_QV2RAY_TEST` function in `test/CMakeLists.txt` — 6 tests: `parse_ss_url`, `parse_vmess_url`, `parse_vless_url`, `generation` (all in `test/src/core/connection/`), `qjsonio` (`test/libs/QJsonStruct/QJsonIO.cpp`), `realping` (`test/src/components/latency/TestRealPing.cpp`).
- Tests link only the base library (`qv2ray_baselib`), so they only cover parsing/JSON/logic, not the GUI or kernel. Adding a new test = add an `ADD_QV2RAY_TEST(...)` line.

## Architecture notes

- `Qv2rayApplication` (via `#ifdef`) drives the event loop; debug builds append `_debug/` to the config dir (`QV2RAY_CONFIG_DIR_SUFFIX` in `src/base/Qv2rayBase.hpp`) — beware running debug and release against different config locations.
- Kernel and connection logic live in `src/core/kernel` and `src/core/connection`; settings in `src/core/settings` (JSON-backed).
- Plugin system: `src/plugins/` holds the builtin protocol + subscription-adapter plugins (built via `QvPlugin-BuiltinProtocolSupport.cmake` / `QvPlugin-BuiltinSubscriptionAdapters.cmake`); the external plugin interface ABI is the `src/plugin-interface` submodule.

## Conventions

- CI commit-message directives: a commit containing `!QT6`, `!DEB`, or `!NSIS` skips the matching workflow job. The `l10n_dev` branch skips all CI (`branches-ignore`).
- Code style (`src/.clang-format`-driven): Microsoft style, Allman braces, column limit 150, 4-space indent. Functions/classes use UpperCamelCase; namespaces lowercase (except `Qv2ray`). Run `clang-format` before committing.
- Header files use `.hpp`. Avoid editing `.ui` / `.qrc` files by hand when a tool generates them.
- `hooks/pre-commit` (standalone bash script, not the pre-commit framework) auto-increments `makespec/BUILDVERSION` (currently 7002) when staged files touch it. It only runs if you install it.

## Pitfalls

- Don't edit generated resources (`*.ui`, `resources.new.qrc` vs `resources.qrc`) manually; keep changes with their generator.
- Qt6 is the **only** supported Qt major version (Qt5 support was removed). `QV2RAY_QT6` is no longer a build toggle; the CMake always uses `Qt6` / `qt6_add_executable`, and the Qt5 CI workflow (`build-qv2ray-cmake.yml`) and `azure-pipelines.yml` have been deleted. The sole build workflow is `build-qv2ray-qt6.yml`. MSVC downgrades the language standard to `/std:c++17`.
- Version is split across `makespec/VERSION` (2.7.1) / `VERSIONSUFFIX` / `BUILDVERSION`; the pre-commit bump only triggers when the working tree is otherwise staged — don't bump it by hand.
- Do not assume the plugin interface twice: `src/plugins` (builtin plugins) and `src/plugin-interface` (submodule ABI) are distinct.
- `QJsonIO::SetValue` (in `3rdparty/QJsonStruct/QJsonIO.hpp`) with a `QJsonIOPath` that contains an array index (e.g. `{ "settings", "vnext", 0, "address" }`) used to crash with `SIGSEGV` in Qt6 when building up from an empty object (indexing an out-of-range array is UB in Qt6's `QJsonValueRef::toValue()`; Qt5 tolerated it). The build-up loop now grows the array to fit the index before taking the reference — keep this behavior if ever touching that file. **Note:** this fix is currently an *uncommitted* change inside the `3rdparty/QJsonStruct` submodule (it will be overwritten by a `git submodule update`).
- **Planned rewrite:** `QJsonStruct` (`3rdparty/QJsonStruct`) is slated to be replaced with a native **C++26 reflection**-based JSON serializer, superseding the current `JSONSTRUCT_REGISTER` / `QJsonIO` machinery. When that rewrite happens, carry over the Qt6 array-index `SetValue` fix above so the array-indexing UB does not regress.
- The main executable no longer links libprotobuf at all. geosite.dat/geoip.dat country-code parsing (`src/components/geosite/QvGeositeReader.cpp`) uses **protozero** (header-only protobuf reader, `3rdparty/protozero`) instead of libprotobuf, so protobuf/absl never enter the main binary's dependency graph. protozero is backward-compatible with geosite-format field additions. Only `backend_api` (the gRPC backend) links protobuf/absl/grpc.

## Maintenance

- Future agents: whenever you discover a new repo-specific command, convention, or pitfall, update this file in place. Keep it accurate and concise; remove stale entries. There is a Chinese mirror at `AGENTS.zh.md` — keep it roughly in sync if it drifts and English is the source of truth.