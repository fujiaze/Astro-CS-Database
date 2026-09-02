# packaging/windows/.vsconfig — Windows 工具链组件锁 (BLD-001)

## 用途

本文件是 Visual Studio 2022 Build Tools 安装的唯一组件清单
（09_WINDOWS_TOOLCHAIN_LOCK.md §4 固定组件），与 CMakePresets.json 的正式
Windows preset 一起构成 `win-msvc-17.14.39-x64` 发布工具链的配置面。

机器校验：`cmake/toolchain/verify_toolchain.py`（合同
`packaging/schemas/preset-contract.json` 为单一事实源）会对本文件做组件
精确比对——多一个或少一个组件都会 FAIL fast。

## 为什么放 packaging/windows/ 而不是仓库根

AstroCS 05_FIXED_SUBAGENT_BINDINGS.yaml 给 SA-BLD-02 的 write 白名单为
`CMakeLists.txt / CMakePresets.json / cmake/** / packaging/** / DEPENDENCIES.md
/ vcpkg.json / vcpkg-configuration.json`，根目录 `.vsconfig` 不在白名单内。
`packaging/windows/` 是本 owner 可写且语义贴切的位置（Windows 发布包内容）。
Visual Studio 安装器支持以 `--config <path>` 显式传入任意路径的组件清单
文件（文件名不必是 `.vsconfig`），因此本文件的语义与官方 `.vsconfig`
完全等价。

## 组件（冻结，7 项，与 09_WINDOWS_TOOLCHAIN_LOCK.md §4 逐字一致）

- Microsoft.VisualStudio.Workload.VCTools
- Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64
- Microsoft.VisualStudio.Component.Windows11SDK.26100
- Microsoft.VisualStudio.Component.VC.ASAN
- Microsoft.VisualStudio.Component.TestTools.BuildTools
- Microsoft.VisualStudio.Component.VC.Llvm.Clang
- Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset

禁止追加：MFC/ATL/C++CLI/UWP/WinUI/Windows App SDK/ARM/ARM64/VS2026/v144/v145。
禁止使用 `--includeRecommended`（会把未审查组件带入环境）。
