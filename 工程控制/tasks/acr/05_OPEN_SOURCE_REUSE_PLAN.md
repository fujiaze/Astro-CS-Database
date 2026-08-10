# 开源项目详细复用方案

核验日期：2026-08-03。实现时必须再次检查官方 release、许可证和平台矩阵，并将最终版本或 commit 固定到仓库依赖锁定文件。禁止跟踪未固定的 `main`/`develop`。

## 1. 总体原则

ACR 自研范围只保留：

- AstroCS 稳定公共 API；
- 开源库之间的薄适配层；
- 离线硬件画像 Qualification 测试矩阵；
- 硬件画像生成、读取和成本模型；
- 95%资源预算控制；
- CPU/GPU动态工作域拆分与结果合并；
- 错误、日志、配置和诊断语义。

线程池、CPU 拓扑、ISA 探测、GPU 编程抽象、微基准框架、JSON、CLI 和日志不得无理由从零重写。

## 2. alpaka：强制优先验证的异构 kernel 层

官方：

- https://github.com/alpaka-group/alpaka
- https://alpaka.readthedocs.io/

官方定位：header-only C++20 加速器抽象库，支持 CPU 和 NVIDIA/AMD/Intel GPU，可提供 CUDA、HIP、SYCL、OpenMP、std::thread 和串行等后端；同一个 kernel 以函数对象形式实现，并可在运行时选择设备。许可证：MPL-2.0。

ACR 中的职责：

- 定义一份可在 CPU/GPU 编译的经典实验 kernel；
- 统一索引域、work division、队列和设备内存视图；
- 实现 CPU-only、CUDA、HIP、SYCL 等后端适配；
- 支持同一进程中多个设备队列；
- 提供 host/device buffer 和复制操作的基础抽象。

明确不负责：

- 不负责 ACR 的离线性能路由；
- 不自动判断某个业务函数是否可并行；
- 不保证一个 work division 在所有设备上都最优；
- 不负责 95%资源占用控制；
- 不替代专用 FFT/BLAS 库。

集成要求：

- 优先验证稳定主线 alpaka，而不是直接采用正在发生破坏性变化的 alpaka3；
- CMake 中以独立 target 封装，第三方类型不得出现在 `astro::compute` 公共 API；
- CPU-only target 不应因未安装 CUDA/HIP/SYCL 而失败；
- 每个 GPU 后端独立 feature gate；
- 对 Windows/MSVC、Linux/GCC/Clang 和实际 GPU 工具链建立最小编译矩阵；
- 记录模板编译时间和二进制膨胀。

验收实验：AXPY、transpose、convolution、reduction、CPU+GPU mixed split。

## 3. oneTBB：CPU 任务执行层

官方：

- https://github.com/uxlfoundation/oneTBB
- https://uxlfoundation.github.io/oneTBB/

官方定位：基于任务的可组合 C++ 并行库，使用逻辑任务而不是让应用直接管理线程，提供 `parallel_for`、`parallel_reduce`、`task_group`、`task_arena`、`global_control` 等。许可证：Apache-2.0。

ACR 中的职责：

- CPU worker pool 和工作窃取；
- CPU 任务切分和局部归约；
- 用 `task_arena` 隔离 ACR CPU 工作；
- 限制并发任务数量，但不通过永久禁用线程解释 95%；
- GPU 提交线程、CPU 计算任务和合并任务的调度辅助；
- CPU-only fallback 的基础运行时。

明确不负责：

- 不负责 GPU kernel；
- 不负责 CPU ISA 发现；
- 不负责设备拓扑；
- `global_control` 的并发上限不是 CPU 利用率百分比，ACR 仍需自己的占空比和队列控制。

集成要求：

- 只建立一个受控 ACR arena，避免算法各自创建线程池；
- 不把 oneTBB 类型暴露给公共 API；
- 测试与已有线程模型/OpenMP 共存；
- Windows 动态库部署方式必须记录；
- 若仓库已有兼容任务系统，先比较，避免重复运行时。

验收实验：多线程 range、nested submission、取消、异常传播、95%节流、长短任务混排。

## 4. hwloc：拓扑、NUMA 和设备局部性

官方：

- https://github.com/open-mpi/hwloc
- https://hwloc.readthedocs.io/

官方定位：提供处理器 package、core、PU、缓存、NUMA 内存节点及 PCI/I/O 设备的层级拓扑、bitmap 和绑定接口。许可证：BSD-3-Clause。

ACR 中的职责：

- 枚举 package/core/logical PU/cache/NUMA；
- 建立 CPU worker 与 NUMA 节点关系；
- 识别 GPU PCI 位置及其最近 CPU/NUMA 节点（平台可用时）；
- 为 pinned host buffer、GPU 上传线程和 CPU worker 提供亲和性建议；
- 导出可诊断的 topology report。

明确限制：

- 各操作系统能暴露的信息不同；
- GPU OS device 和 PCI 局部性可能依赖可选组件；
- hwloc 只报告拓扑，不报告真实带宽和 kernel 性能；
- 不应因信息缺失阻止运行，应降级为无 NUMA 优化。

验收实验：拓扑序列化、允许 CPU 集、NUMA 本地/远端内存测试、PCI 设备映射、无 hwloc 降级。

## 5. google/cpu_features：CPU ISA 安全门禁

官方：

- https://github.com/google/cpu_features

官方定位：跨平台 C99 运行时 CPU 特性检测库，覆盖 x86、AArch64、ARM、MIPS、POWER、RISC-V、s390x 等。许可证：Apache-2.0。

ACR 中的职责：

- 检测 SSE、AVX、AVX2、FMA、AVX-512 子集等真实可用能力；
- 在加载 ISA 插件或调用特定函数前做安全门禁；
- 生成设备指纹的一部分；
- 防止仅凭编译机器能力在目标机器执行非法指令。

明确限制：

- 只说明“能不能执行”，不说明“是不是最快”；
- 最优 ISA 仍由 Qualification 决定；
- AVX-512 子集必须精确匹配，不能只记录一个布尔值。

验收实验：模拟能力 mask、拒绝非法插件、baseline 永远可用、跨平台编译。

## 6. Google Benchmark：Qualification 测量框架

官方：

- https://github.com/google/benchmark

官方定位：C++ 微基准支持库，提供重复运行、计时、参数化、统计和 reporter。许可证：Apache-2.0。

ACR 中的职责：

- 驱动单设备基础微基准；
- 参数化尺寸、精度、ISA 和线程点；
- 提供 warm-up、重复、计数器和 JSON 输出基础；
- 记录 median、p95、变异和吞吐指标所需原始数据。

ACR 必须额外实现：

- 空载提示；
- GPU event 计时和传输/计算分离；
- CPU+GPU 联合执行测试；
- 温态持续测试；
- 路由生成；
- profile 指纹；
- 中断和损坏恢复。

不得把 Google Benchmark 的单次微基准结果直接等同于最终路由。

## 7. CLI11、nlohmann/json、spdlog/fmt

### CLI11

官方：https://github.com/CLIUtils/CLI11  
用途：独立 `acr-benchmark`、`acr-status`、`acr-report` 工具的子命令、选项、验证和帮助。header-only，BSD-3-Clause。不得为了本支线修改现有 AstroCS CLI。

### nlohmann/json

官方：https://github.com/nlohmann/json  
用途：硬件指纹、Qualification 原始结果、硬件画像、验证报告和 delivery manifest。公共 schema 必须带 `schema_version`，解析时严格校验未知/缺失关键字段。MIT。

### spdlog/fmt

官方：https://github.com/gabime/spdlog  
用途：控制台、文件、结构化诊断和非阻断警告。支持 header-only 或 compiled 模式、同步/异步 sink。MIT。优先复用仓库现有日志系统；只有不存在或不满足时才新增。

## 8. 测试框架

优先使用仓库已有 Catch2 或 GoogleTest。若两者都没有，由 Agent 在 ADR 中选择一个，禁止同时引入两套。

用途：

- 公共 API 单元测试；
- CPU/GPU 容差比较；
- 故障注入；
- schema 和路由测试；
- 构建和动态插件测试。

Benchmark 不能替代单元测试，性能结果也不能替代正确性断言。

## 9. 专用数学库

以下实验用于验证“调用成熟库”的适配能力，不自行实现生产级算法：

- FFTW/pocketfft：CPU FFT；
- cuFFT：NVIDIA FFT；
- rocFFT：AMD FFT；
- oneMKL：Intel/CPU/SYCL 路径；
- BLAS/cuBLAS/rocBLAS/oneMKL：GEMM；
- CUB/rocPRIM/oneDPL：backend 内部 reduction/scan primitives。

ACR 公共 API 只暴露抽象 operation，不暴露 vendor handle。专用库不可用时，相关实验标记为 SKIPPED，不得伪报通过。

## 10. StarPU：可选评估，不作为第一版强制依赖

官方：

- https://starpu.gitlabpages.inria.fr/
- https://starpu.gitlabpages.inria.fr/features.html

StarPU 能提供 codelet、异构任务调度、数据管理、多 GPU、性能模型和数据局部性优化。它与 ACR 的目标有重叠，但默认偏向运行时性能模型和动态调度，而 ACR 已冻结为“离线详细标定 + 硬件画像 + 运行时工作保持”。

本支线要求：

- 做独立 PoC/ADR，评估能否用 worker mask、自定义 scheduler 或显式 worker 选择承载硬件画像；
- 评估 Windows、Linux、CUDA、HIP 和部署成本；
- 不允许公共 API 绑定 StarPU；
- 第一版即使不用 StarPU，也必须记录未采用原因；
- 若采用，只能作为可选 runtime adapter，CPU-only 不依赖它；
- 不为了“最大复用”强行引入与硬件画像冲突的复杂运行时。

## 11. 依赖锁定和许可证交付

Agent 必须生成 `third_party/dependency-lock.json` 或仓库等价文件，至少记录：

- 项目名；
- 官方仓库；
- 精确 tag/commit；
- SHA-256；
- SPDX 许可证；
- 用途；
- 是否编译进二进制；
- 是否可选；
- 平台和后端；
- 本地补丁；
- 升级风险。

交付包必须附第三方 NOTICE/许可证清单，但不得把庞大 GPU SDK 或依赖缓存装入 ZIP。


## 12. 经典Benchmark参考项目：复用思想和测试定义，不盲目整包复制

### STREAM

官方参考：

- https://www.cs.virginia.edu/stream/
- https://www.cs.virginia.edu/stream/ref.html

用途：CPU持续主存带宽。采用Copy、Scale、Add、Triad定义和其“数组明显大于末级缓存”的原则。ACR可自行实现同数学操作并用Google Benchmark组织，不必直接复制全部STREAM源码；若复制代码，必须单独核对其许可和运行规则。

### BabelStream

官方：

- https://github.com/UoB-HPC/BabelStream

用途：GPU/加速器全局内存带宽，测试Copy、Mul、Add、Triad、Dot等。BabelStream不包含PCIe传输时间，因此ACR必须另外测H2D/D2H，不能把显存带宽当端到端吞吐。

### PolyBench/C与PolyBench-ACC

官方参考：

- https://www.cs.colostate.edu/~pouchet/software/polybench/
- https://cavazos-lab.github.io/PolyBench-ACC/

用途：提供2D convolution、矩阵、stencil等静态控制经典kernel的尺寸和结构参考。ACR只选择与天文图像处理相关的代表模式，不运行整套并用总分路由。

### HPCG思想

官方参考论文/项目：

- https://www.hpcg-benchmark.org/

用途：提醒模型不能只测密集FLOPS，必须覆盖低计算强度、不规则访存、稀疏和细粒度依赖。ACR不需要运行完整HPCG排名程序，而是实现SpMV风格gather、稀疏scatter和工作量不均实验。

### Roofline模型

用途：将实测计算吞吐和实测内存带宽与任务计算强度结合，作为成本推算框架。ACR采用扩展Roofline思想，但必须加入启动、传输、队列、原子、分支和合并成本；不能只用一条理论Roofline线。

## 13. 依赖选择硬门禁

- 现有分支已实现的oneTBB/hwloc等可继续复用，不因控制包修订而推倒重来；
- alpaka若无法在目标Windows/Linux工具链稳定支持所需后端，允许经ADR选择Kokkos/SYCL或薄原生插件，但公共API不得改变；
- 不为了“用了更多开源库”同时引入多个重叠运行时；
- 任何第三方库只解决其擅长部分，ACR自研层只保留AstroCS语义、画像模型、dispatcher和资源控制；
- 依赖选择必须有可构建PoC和经典实验数据，而不是只引用宣传页。
