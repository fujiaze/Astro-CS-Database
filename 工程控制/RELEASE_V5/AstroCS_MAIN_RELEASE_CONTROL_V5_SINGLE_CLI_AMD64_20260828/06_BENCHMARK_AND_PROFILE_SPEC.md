# Benchmark、自动调优与 cpu_profile 合同

## 1. 原则

benchmark 的顺序固定为：正确性筛选 -> 预热 -> 多次计时 -> 稳健统计 -> 选择。速度永远不能使错误路径获胜。

不允许只测一个大循环后给全程序选择同一 ISA。选择键至少为 `kernel + precision + size_class + layout`。

## 2. 硬件画像

`astrocs hardware inspect` 和 benchmark 必须记录：

- CPU vendor/family/model/stepping、brand、microcode（可得时）；
- CPUID 与 XCR0，有效 affinity、逻辑 CPU、物理核、SMT、NUMA；
- 容器/cgroup 或 Windows Job Object 限制；
- RAM、page size、cache 层级；
- OS/kernel/build、编译器、AstroCS commit/build id；
- CLI 与所有 backend hash。

不得把 `hardware_concurrency()`、`nproc` 或注册表总数单独当作可用 worker 数。

## 3. 候选空间

- ISA：仅 manifest 存在、CPU/OS 可用且 Oracle 通过的 backend。
- worker：从有效 affinity/topology 自动产生候选，包含 1、物理核级和全部允许逻辑 CPU；不得写死 2/4/8/16。
- block/chunk：由工作集、cache 和数据布局生成几何候选；不得把某台机器数值写进源码默认。
- 数据：small/medium/large；FP32/FP64；对齐/非对齐；典型与边界分布。

先运行内存 read/write/copy/triad，获得本机带宽基线，供识别 memory-bound 使用。

## 4. 计时与选择

- 使用单调高分辨率时钟；预热不计时。
- 至少 7 个有效样本或直到置信区间满足合同；记录 median、MAD、p05/p95。
- 每个样本同时由资源监控采集；thermal/throttle/后台负载异常样本标记而非删除不报。
- 候选收益小于预设噪声裕量时选更保守 ISA/更少资源的路径。
- AVX-512 必须用端到端 kernel 实测，不能依据指令存在直接选择。
- 选择结果逐 kernel 写出：backend、workers、block、证据、备选、正确性 hash。

## 5. profile 生命周期

`cpu_profile.json` 必须对 `schemas/cpu_profile.schema.json` 有效，并含：

- `schema_version,created_at,mode`；
- hardware fingerprint；
- affinity/NUMA/memory 限制；
- commit/build/backend/kernel hashes；
- 每个 kernel 的选择和量测摘要；
- benchmark 自身资源摘要与 PASS/FAIL。

以下任一变化使相关 profile 失效：CPU identity、OS ISA state、有效 affinity、backend/CLI hash、ABI、kernel version、precision contract。失效不得静默继续。

## 6. 未 benchmark 的行为

无 profile 时：

1. 只加载 baseline；
2. worker 上限取本进程有效 affinity；若可用 CPU >=2，重计算不得退成 1 worker；
3. 使用源码中的与硬件数无关保守 block 规则；
4. 写出 `reason=no_valid_profile`；
5. 运行仍受资源门禁，若 baseline 并行不足则 FAIL 并提示运行 benchmark/修复并行实现。

不得把“保守”解释为“单线程”。

## 7. 命令模式

- `--quick`：首次启动的短筛选，有限 kernel/size，目标是安全优于 baseline；不替代正式性能证据。
- `--full`：Windows 发布机器正式 profile；逐 kernel 全矩阵并生成审计摘要。
- 普通 Phase run 只读取 profile，不自动偷偷跑长 benchmark。

