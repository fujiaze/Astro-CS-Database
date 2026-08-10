# ADR-009: CPU-only 构建门禁

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 开关 | `ACR_BUILD_CUDA` / `ACR_BUILD_HIP` / `ACR_BUILD_SYCL`（默认 OFF） |
| 门禁 | CI 强制无 GPU SDK 构建必须成功 |
| 平台 | Windows / Linux |

## 状态

Accepted。ACR 构建默认 CPU-only，`ACR_BUILD_CUDA/HIP/SYCL` 默认 OFF。CPU-only 构建不得引用任何 GPU SDK 头文件。alpaka 的 `ALPAKA_ACC_GPU_*_ENABLE` 宏由 `ACR_BUILD_*` 控制。CI 强制包含无 GPU SDK 的构建门禁。

## 背景

ACR 的目标用户中，绝大多数普通用户机器无 CUDA / ROCm / oneAPI SDK。若 ACR 主线构建强制要求 GPU SDK，普通用户将无法构建，违反"clean machine 可构建"原则。同时 ACR 必须支持 GPU 后端（CUDA/HIP/SYCL）以服务有 GPU 的用户与 CI。

alpaka 通过 `ALPAKA_ACC_GPU_CUDA_ENABLE` 等宏控制后端启用，若不显式控制，alpaka 可能自动探测 SDK 并在缺失时 configure 失败。ACR 必须显式将这些宏与 `ACR_BUILD_*` 绑定，确保 CPU-only 构建时 alpaka 也仅启用 CPU 后端。

候选方案中，"always fetch all SDK" 会绑死用户必须安装全部 SDK；"autodetect" 在 SDK 缺失时 configure 失败，不可靠。两者均违反普通用户可构建原则。

## 决策

1. `ACR_BUILD_CUDA` / `ACR_BUILD_HIP` / `ACR_BUILD_SYCL` 默认 **OFF**。
2. CPU-only 构建时（三者均 OFF），**禁止引用任何 GPU SDK 头文件**（`cuda_runtime.h`、`hip/hip_runtime.h`、`sycl/sycl.hpp` 等）。
3. alpaka 的 `ALPAKA_ACC_GPU_CUDA_ENABLE` / `ALPAKA_ACC_GPU_HIP_ENABLE` / `ALPAKA_ACC_GPU_SYCL_ENABLE` 宏由 `ACR_BUILD_CUDA` / `ACR_BUILD_HIP` / `ACR_BUILD_SYCL` 控制：
   - `ACR_BUILD_CUDA=ON` → `ALPAKA_ACC_GPU_CUDA_ENABLE=ON`
   - 其余类推；OFF 时对应 alpaka 宏强制 OFF。
4. CI 强制包含一条**无 GPU SDK 构建门禁** job：在无 CUDA/ROCm/oneAPI SDK 的环境执行 `cmake configure + build`，必须成功。
5. CPU-only 构建产物必须能运行 ACR 的 CPU 路径全部功能（baseline + ISA 插件，参见 ADR-004）。
6. `ACR_BUILD_CUDA=ON` 时，CMake 须校验 CUDA Toolkit 存在，缺失则 configure 失败并明确报错（不静默降级，避免用户误以为已启用 CUDA）。

## 理由

- 普通用户无 GPU SDK 是常态，主线构建必须兼容。
- 显式 `ACR_BUILD_*` 开关比 autodetect 可靠，避免 SDK 缺失时 configure 失败的不可控行为。
- alpaka 宏与 ACR 开关绑定，确保 CPU-only 构建时 alpaka 不探测 GPU SDK。
- CI 门禁保证 CPU-only 构建不被意外破坏（如误引入 CUDA 头文件）。
- 与 ADR-008 FetchContent 协同：CPU-only 构建不拉取 GPU 依赖。
- 与 ADR-001 alpaka 集成边界协同：每个 GPU 后端独立 feature gate。

## 集成边界

- **职责内**：`ACR_BUILD_*` 开关定义、alpaka 宏翻译、GPU SDK 头文件隔离、CI 门禁。
- **职责外**：GPU SDK 的安装与配置（用户责任）、GPU kernel 的实际运行验证（由 `ACR_BUILD_*=ON` 的 CI job 负责）。
- **头文件边界**：CPU-only 构建的编译单元中，预处理器不得展开任何 GPU SDK 头文件。GPU 相关代码必须以 `#ifdef ACR_BUILD_CUDA` 等条件编译隔离。
- **CMake 边界**：`ACR_BUILD_CUDA=ON` 时 CMake 校验 `CMAKE_CUDA_COMPILER` 或 CUDA Toolkit 路径，缺失则 `message(FATAL_ERROR ...)`。
- **CI 边界**：CI 矩阵必须包含至少一条 CPU-only job（无任何 GPU SDK），且为必过门禁。
- **降级边界**：CPU-only 构建产物运行时不允许尝试加载 GPU 后端；若路由表引用 GPU 后端而运行时不可用，须明确报错而非崩溃。

## 替代方案

1. **Always fetch all SDK（强制全后端构建）**：
   - 未采用：绑死用户必须安装 CUDA + ROCm + oneAPI，普通用户不可构建。
2. **Autodetect SDK（自动探测）**：
   - 未采用：SDK 缺失时 configure 行为不可控（部分库静默禁用、部分库 FATAL_ERROR），不可靠。
3. **GPU 后端单独仓库 / 子项目**：
   - 未采用：增加仓库管理复杂度；与 ACR 单仓库原则冲突；共享代码需重复。
4. **仅支持 CPU（不引入 GPU 后端）**：
   - 未采用：违反 ACR 异构目标；有 GPU 的用户与 CI 需求无法满足。

## 未采用原因

强制全后端与 autodetect 均违反普通用户可构建原则；单独仓库增加管理复杂度；仅 CPU 方案不满足异构目标。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| clean machine CPU-only 构建 | 无 CUDA/ROCm/oneAPI SDK | `cmake -B build && cmake --build build` 成功 |
| 无 GPU SDK 头引用 | CPU-only 构建产物 | 编译命令无任何 `-I.../cuda` 等包含路径 |
| alpaka 宏绑定 | `ACR_BUILD_CUDA=OFF` | `ALPAKA_ACC_GPU_CUDA_ENABLE` 未定义或 OFF |
| CI 门禁通过 | CI CPU-only job | 无 GPU SDK 环境构建 + 测试全绿 |
| ON 时校验 SDK | `ACR_BUILD_CUDA=ON` 但无 Toolkit | CMake configure FATAL_ERROR，明确提示 |
| CPU 路径全功能 | CPU-only 产物运行 | baseline + ISA 插件全可用（ADR-004） |

构建日志写入 `run/logs/acr/build/<YYYYMMDD>/`。
