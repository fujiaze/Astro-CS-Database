# ADR-001: alpaka 1.2.0 单源异构 kernel 抽象

| 项目 | 值 |
| --- | --- |
| 状态 | Accepted |
| 日期 | 2026-08-02 |
| 依赖锁 | `lib/acr/docs/dependency-lock.json` |
| 版本 | alpaka 1.2.0（commit `1.2.0`） |
| 许可证 | MPL-2.0 |
| 平台 | Windows / Linux |
| 形态 | header-only，C++20 |

## 状态

Accepted。alpaka 作为 ACR 异构 kernel 抽象的统一入口，所有多后端 kernel 经由 alpaka 编写，禁止在 ACR 公共代码中直接调用 CUDA/HIP/SYCL 原生 API。

## 背景

ACR 必须在 CPU / CUDA / HIP / SYCL 四类后端上运行同一份 kernel（AXPY、transpose、conv、reduction、mixed），并支持离线标定产出的固定路由表。若每个后端手写一份 kernel，将出现 4 份等价代码、4 套编译路径与 4 套错误码，长期维护成本无法承受。同时 ACR 不允许在公共 API 层暴露任何第三方设备类型（Device/Queue/View），以免下游模块被反向耦合。

alpaka 1.2.0 是当前稳定线，1.x 与即将到来的 3.x 之间已预告存在 breaking change，故必须钉死 1.2.0 release tag。

## 决策

1. 引入 alpaka 1.2.0 作为 ACR 唯一的单源异构 kernel 抽象层。
2. 所有 kernel 以 `ALPAKA_FN_ACC` 风格编写，通过 `acc` 参数访问线程索引与共享内存。
3. 设备/队列/内存视图统一通过 alpaka 抽象类型在 ACR 内部模块传递。
4. 编译开关由 ADR-009 的 `ACR_BUILD_CUDA/HIP/SYCL` 控制，并翻译为 alpaka 的 `ALPAKA_ACC_GPU_*_ENABLE` 宏。
5. CPU-only 构建不得因缺少 CUDA/ROCm/oneAPI SDK 而失败。

## 理由

- 单源 kernel 将四后端差异收敛到编译期宏，维护成本最低。
- header-only 形态契合 ACR 的 CMake FetchContent 策略（ADR-008），无需预编译动态库。
- alpaka 在 HEP/ExaCAO 等社区已被大规模验证，CPU 路径稳定，CUDA/HIP/SYCL 路径成熟。
- 与 ACR "离线标定 + 固定路由" 范式契合：kernel 只负责执行，调度决策在外层。

## 集成边界

- **职责内**：单源 kernel 编写、设备/队列/内存视图抽象、多后端编译开关翻译。
- **职责外**：离线路由决策（由 ACR router 模块负责）、95% 占用率目标（由标定阶段决定，非 alpaka 责任）、FFT/BLAS 等高级算法（alpaka 不提供，ACR 不通过 alpaka 调用）。
- **API 边界**：alpaka 类型（`alpaka::Dev`、`alpaka::Queue`、`alpaka::View`）不得出现在 ACR 公共头文件的接口签名中，必须在内部实现文件中封装为 ACR 自有类型。
- **CMake 边界**：alpaka 作为独立 CMake target 链接，不与 oneTBB/hwloc 等共享编译选项。
- **降级边界**：每个 GPU 后端独立 feature gate，缺一个不影响其余后端；CPU-only 构建强制可用。

## 替代方案

1. **手写各后端 kernel**：每后端独立 `.cu/.cpp/.sycl` 文件，预处理器分支。
   - 未采用：维护成本 4 倍，bug 修复需同步 4 份代码，违反 DRY。
2. **cuRAND / rocRAND 等厂商库**：直接调用厂商随机数/数学库。
   - 未采用：仅适用于特定 kernel（如 PRNG），不解决通用 kernel 抽象问题，且绑死单一后端。
3. **Kokkos**：另一单源异构框架。
   - 未采用：Kokkos 编译模型更重，ACR 场景下 alpaka 已足够；切换成本高于收益。

## 未采用原因

手写方案在 ACR 的"四后端等价 kernel"约束下不可持续；厂商库方案仅覆盖局部；Kokkos 与 alpaka 互斥，且 ACR 已选定 alpaka 以匹配 1.x 稳定线承诺。

## 验收实验

| 实验 | 目标 | 通过条件 |
| --- | --- | --- |
| AXPY | 单源 kernel 在 CPU/CUDA 后端运行 | 数值与参考实现一致（容差 1e-6） |
| transpose | 二维转置 kernel 四后端可编译 | 至少 CPU+CUDA 实际运行通过 |
| conv | 卷积 kernel CPU/CUDA 双后端 | 性能数据落盘 JSON，无越界 |
| reduction | 归约 kernel CPU/CUDA 双后端 | 与 std::reduce 数值一致 |
| mixed | 混合 kernel（含 shared mem） | CPU 路径无 illegal instruction（依赖 ADR-004 门禁） |

所有实验由 ADR-005 的 Google Benchmark 驱动，结果输出到 `run/logs/acr/<module>/<date>/` 与 `run/drizzle/` 之外的 ACR 专属产物路径。
