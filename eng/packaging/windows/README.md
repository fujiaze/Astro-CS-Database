# eng/packaging/windows/ — Windows 发布工具链落点 (BLD-001)

## 用途

本目录是 Windows x64 发布工具链的**取值落点**：

- `.vsconfig` —— Visual Studio 2022 Build Tools 安装的**唯一组件清单**（机器精确比对面）；
- 本 README —— 组件清单与工具链取值的**登记面**；机器单一事实源是
  `eng/packaging/schemas/preset-contract.json`。

机器校验：`eng/cmake/toolchain/verify_toolchain.py`（合同 `preset-contract.json` 为单一事实源）
对本目录 `.vsconfig` 做组件精确比对——多一个或少一个组件都会 FAIL fast；构建入口是
`CMakePresets.json` 的正式 preset `win-msvc-17.14.39-x64`。

## 工具链取值（Windows 正式面）

下表逐项来自机器源（`preset-contract.json` 与 `CMakePresets.json`）；**漂移一律以机器源为准**，
本表只是登记镜像，不构成第二套取值。

| 项 | 取值 | 机器源 |
|---|---|---|
| 正式 generator | `Visual Studio 17 2022`，`-A x64`，host tool `x64` | `preset-contract.json#windows.formal_generator` / `architecture` |
| platform toolset | `v143`；`toolset_version = 14.44.35207`；compiler family `19.44` | `preset-contract.json#windows.toolset` / `toolset_version` / `compiler_family` |
| VS 2022 Build Tools | `17.14.39`；installationVersion `17.14.37614.0` | `preset-contract.json#windows.vs_buildtools_version` / `vs_installationVersion` |
| Windows SDK | `10.0.26100.0`；servicing bundle `10.0.26100.9169` | `preset-contract.json#windows.sdk_version` / `sdk_servicing_bundle` |
| CMake | `3.31.12` | `preset-contract.json#windows.cmake_version`；`CMakePresets.json` vendor 段 `cmake_pin` |
| C++ 标准 | C++17 | `preset-contract.json#windows.cpp_standard` |
| CRT | Debug `/MDd`；Release/RelWithDebInfo `/MD`；禁 `/MT` | `preset-contract.json#windows.crt_debug` / `crt_release`、`forbidden.static_crt` |
| VS 安装位置 | `C:/AstroCS/toolchains/vs2022-17.14.39` | `preset-contract.json#windows.vs_installation`（由 preset 显式声明，属机器路径白名单例外） |
| 禁面 | `forbidden` 段（evergreen / mingw_msys / vs2026 / future_toolset / ninja_generator / non_x64_arch / static_crt） | `preset-contract.json#forbidden` |

**未决面（仓内无可核依据，故不登记取值）**：Windows 侧浮点开关（`/fp:*`）、优化开关
（`/O2` 与 `/GL`/LTCG/PGO 禁面）、clang-tidy 版本、Windows 验证脚本所用的 Python 版本。
仓内（含 git 全历史）没有这些取值的机器锚或构建锚；需要时先补机器锚，再登记到本表。

## 组件（冻结，7 项；`.vsconfig` 是唯一清单）

- Microsoft.VisualStudio.Workload.VCTools
- Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64
- Microsoft.VisualStudio.Component.Windows11SDK.26100
- Microsoft.VisualStudio.Component.VC.ASAN
- Microsoft.VisualStudio.Component.TestTools.BuildTools
- Microsoft.VisualStudio.Component.VC.Llvm.Clang
- Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset

禁止追加：MFC/ATL/C++CLI/UWP/WinUI/Windows App SDK/ARM/ARM64/VS2026/v144/v145。
禁止使用 `--includeRecommended`（会把未审查组件带入环境）。
