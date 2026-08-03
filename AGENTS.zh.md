# Qv2ray 开发指南

## 状态

**已停止维护** (2019-2021)。CI 和构建系统仍可用，但无活跃开发。

## 构建

```bash
git submodule update --init --recursive
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

- C++23 / Qt 5.15（默认）或 Qt 6.5（`-DQV2RAY_QT6=ON`）
- Android/QML 强制 Qt6
- MSVC 回退 `/std:c++17`
- 构建选项见 CMakeLists.txt，关键：`QV2RAY_UI_TYPE`（QWidget/QML/CLI），`BUILD_TESTING`

## 测试

```bash
cmake .. -GNinja -DBUILD_TESTING=ON && cmake --build . && ctest
```

测试框架 Catch2，注册 6 个测试（parse_ss_url、parse_vmess_url、parse_vless_url、generation、qjsonio、realping）。

## CI 特殊约定

提交消息含 `!QT5` / `!QT6` / `!DEB` / `!NSIS` 可跳过对应 CI 任务。`l10n_dev` 分支跳过所有 CI。

## pre-commit hook

`hooks/pre-commit` 自动递增 `makespec/BUILDVERSION`（当前 7002）。已有脚本，非 pre-commit 框架。

## 代码风格

- `.clang-format`: Microsoft 风格，Allman 大括号，列宽 150，4 空格缩进
- 函数/类: UpperCamelCase；命名空间小写（`Qv2ray` 除外）
- 头文件后缀 `.hpp`
- 避免改 `.ui` 文件
- 提交前运行 `clang-format`

## 架构要点

- 入口: `src/main.cpp` → 按编译选项实例化 `Qv2rayWidgetApplication` / `Qv2rayQMLApplication` / `Qv2rayCliApplication`
- `Qv2rayApplication` 是宏，映射到具体实现类
- Debug 构建配置目录后缀 `_debug/`
- 插件通过 `src/plugin-interface` 子模块定义接口

## 平台依赖

- Linux: 系统安装 Qt5/Qt6、gRPC、protobuf、libcurl、OpenSSL
- Windows: 运行 `libs/setup-libs.sh` 下载预编译依赖
- macOS: CI 使用 patched `macdeployqt`
