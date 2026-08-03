# Qv2ray Development Guide

## Status

**No longer maintained** (2019-2021). CI and build system still work, but no active development.

## Build

```bash
git submodule update --init --recursive
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

- C++23 / Qt 6 (default, Qt 6.0+)
- Android/QML force Qt6
- MSVC falls back to `/std:c++17`
- Build options in CMakeLists.txt; key ones: `QV2RAY_UI_TYPE` (QWidget/QML/CLI), `BUILD_TESTING`

## Tests

```bash
cmake .. -GNinja -DBUILD_TESTING=ON && cmake --build . && ctest
```

Catch2 framework, 6 registered tests (parse_ss_url, parse_vmess_url, parse_vless_url, generation, qjsonio, realping).

## CI Conventions

Commit messages containing `!QT5` / `!QT6` / `!DEB` / `!NSIS` skip the corresponding CI job. The `l10n_dev` branch skips all CI.

## pre-commit hook

`hooks/pre-commit` auto-increments `makespec/BUILDVERSION` (currently 7002). Standalone script, not the pre-commit framework.

## Code style

- `.clang-format`: Microsoft style, Allman braces, column limit 150, 4-space indent
- Functions/classes: UpperCamelCase; namespaces lowercase (except `Qv2ray`)
- Header files use `.hpp`
- Avoid editing `.ui` files
- Run `clang-format` before committing

## Architecture notes

- Entrypoint: `src/main.cpp` → instantiates `Qv2rayWidgetApplication` / `Qv2rayQMLApplication` / `Qv2rayCliApplication` per build options
- `Qv2rayApplication` is a macro mapping to the concrete class
- Debug builds use a `_debug/` config directory suffix
- Plugin interface defined via `src/plugin-interface` submodule

## Platform dependencies

- Linux: system Qt5/Qt6, gRPC, protobuf, libcurl, OpenSSL
- Windows: run `libs/setup-libs.sh` to download prebuilt deps
- macOS: CI uses patched `macdeployqt`
