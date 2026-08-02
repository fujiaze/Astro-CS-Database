# AstroCS ACR 底层支线开发控制包

日期：2026-08-02  
支线名称：AstroCompute Runtime（ACR）  
唯一开发分支：`feature/astrocompute-runtime`  
最终归宿：底层全部验收通过后合并到 `main` 备用；现阶段不改造任何 AstroCS 现有算法。

> 本控制包没有发布版本号。后续修订直接覆盖和更新这一份权威控制包，不创建 V2/V3 控制包，不创建 `feature/astrocompute-runtime-v2` 等平行实现。

## 0. 工程启动词

```text
继续在现有AstroCS的feature/astrocompute-runtime分支修改，读取本包00_READ_FIRST；只完善ACR底层、硬件画像和经典实验，不改算法，不建新版分支。
```

同样内容保存在 `00_AGENT_START_PROMPT.txt`。若该分支尚不存在，才允许从最新 `main` 创建；若已经存在，必须在原分支和原实现上增量修正，禁止另起新仓库、新版本目录或重复实现。

## 1. 本次最关键的设计修正

ACR 不再采用以下错误或过度方案：

- 不让用户填写 CPU/GPU 任务比例；
- 不把 `0/25/50/75/100%` 当作正式路由参数；
- 不为每个 AstroCS 业务算法预先穷举 CPU/GPU 切分比例；
- 不生成“某 kernel 固定 CPU 18%、GPU 82%”的路由文件；
- 不把一次 AXPY 或单一 GEMM 总分当作异构路由依据；
- 不在运行中在线学习或改写性能画像。

正确方案是：

```text
离线经典 Benchmark
        ↓
生成 CPU ISA、GPU、内存、总线、算术、归约、卷积、稀疏访问等多维硬件性能画像
        ↓
任务通过极简接口提供任务类别、规模、精度、访存和驻留等少量特征
        ↓
ACR 根据硬件画像推算各设备的预计成本与合适块大小
        ↓
运行时使用工作保持的动态队列，让 CPU 和所有可用 GPU 持续领取工作块
        ↓
在默认约 95%资源占用目标下尽量吃满全部硬件
```

Benchmark 中可以测量多种尺寸、线程数、ISA、精度和设备，但这些是建立能力曲线，不是生成固定设备比例。

## 2. 本支线唯一目标

只开发一个可独立构建、测试、以后供少数密集模块调用的底层计算运行时 ACR。当前阶段严禁修改：

- Drizzle、积分、叠加、校准、测光、HISS、PSF、重采样等算法；
- PipelineFrame 数据语义；
- Stage 1/Stage 2 流程；
- Orchestrator 正常行为；
- 现有 OpenMP 代码；
- 正常 AstroCS CLI 默认行为。

允许新增：

- ACR 公共 API、runtime、backend、buffer、队列、事件和设备管理；
- 独立 `acr-benchmark`、`acr-status`、`acr-report`；
- 硬件画像、性能模型和动态调度器；
- 经典实验、单测、故障注入和 CI；
- 最小、可选、默认安全的 CMake/依赖入口。

## 3. 合并后的备用状态

完成后合并到 `main`，但必须保持 dormant：

- 现有算法完全不调用 ACR；
- 普通 AstroCS 启动不探测 GPU、不创建 ACR 线程、不发未标定警告；
- CPU-only 构建在没有 GPU SDK 时仍可用；
- GPU backend 是可选插件；
- 后续真实算法接入另开独立集成分支。

## 4. 未来算法如何接入

未来只改少量真正密集的算法。算法作者不写线程、CUDA、HIP、SYCL、AVX，也不指定设备比例，只需：

- 写单工作项、单 Tile、局部归约或独立批次的串行逻辑；
- 调用 `parallel_for`、`parallel_tiles`、`parallel_reduce` 或 `parallel_batch`；
- 选择一个任务类别，必要时补充 halo、稀疏度或原子冲突提示。

ACR 自动完成：CPU线程、ISA选择、GPU队列、块大小、数据迁移、CPU+GPU动态并行和结果合并。

任意完整 C++ 程序不能无条件自动 GPU 化；工作项依赖、共享写入、I/O和复杂对象限制见 `04_KERNEL_MODEL_AND_LIMITS.md`。

## 5. 开源复用和经典测试

- 开源项目详细职责、边界、许可证和验收：`05_OPEN_SOURCE_REUSE_PLAN.md`；
- 硬件画像 Benchmark：`06_QUALIFICATION_BENCHMARK_SPEC.md`；
- 画像推算和运行时动态调度：`07_STATIC_ROUTING_AND_MIXED_EXECUTION.md`；
- 经典实验输入、参考答案、尺寸、容差和覆盖能力：`17_CLASSIC_EXPERIMENT_SUITE.md`；
- 现有分支必须纠正的问题：`19_EXISTING_BRANCH_CORRECTION_TASKS.md`。

## 6. 文档阅读顺序

1. `01_FROZEN_REQUIREMENTS.md`
2. `19_EXISTING_BRANCH_CORRECTION_TASKS.md`
3. `02_SYSTEM_ARCHITECTURE.md`
4. `03_PUBLIC_API_SPEC.md`
5. `04_KERNEL_MODEL_AND_LIMITS.md`
6. `05_OPEN_SOURCE_REUSE_PLAN.md`
7. `06_QUALIFICATION_BENCHMARK_SPEC.md`
8. `07_STATIC_ROUTING_AND_MIXED_EXECUTION.md`
9. `08_RESOURCE_CONTROL_SPEC.md`
10. `17_CLASSIC_EXPERIMENT_SUITE.md`
11. `10_PHASES_TASKS_ACCEPTANCE.md`
12. `11_GIT_BRANCH_AND_AGENT_RULES.md`
13. `12_TEST_VALIDATION_MATRIX.md`
14. `18_MAIN_MERGE_AND_DORMANT_INTEGRATION.md`
15. `13_DELIVERY_PACKAGE_RULES.md`
16. `16_AGENT_MASTER_INSTRUCTION.md`

`09_INTEGRATION_MIGRATION_PLAN.md` 只供未来真实算法改造阅读，本分支不得执行其中改造。

## 7. 完成定义

只有同时满足以下条件才算完成：

- 公共 API 真正连接路由器、dispatcher 和 backend，不得忽略任务描述；
- CPU baseline、CPU ISA 多版本、至少一个真实 GPU backend通过；
- Benchmark 生成多维硬件画像，而非固定 CPU/GPU 比例；
- 画像包含算术、内存、传输、归约、卷积、稀疏、原子、分支和启动开销曲线；
- 动态调度能让 CPU、单 GPU、多 GPU领取工作，并保证不遗漏、不重复；
- 95%是资源占用目标，不是任务比例或少开线程；
- 未标定时纯 CPU运行并非阻断警告；
- 经典实验全部通过或对不可用硬件明确 SKIPPED；
- 没有任何现有算法源码改动；
- Evidence 从同一个干净 HEAD 一次生成；
- 合并到 `main` 后保持备用、无副作用。
