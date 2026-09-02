# CPU 拓扑、指令集与自动 benchmark 规范

## 1. 不得硬编码

- 不得写死 2/8/16/32 核；
- 不得全局添加 `-mavx2`、`-mavx512*` 等使基础二进制依赖特定 ISA；
- 不得只凭 CPUID 选择“理论更快”的指令集；
- 不得让 `std::thread::hardware_concurrency()` 成为唯一核心数来源；
- 不得在每次启动重复长 benchmark。

## 2. 可用 CPU 数

Linux 按优先级综合：

1. `sched_getaffinity`；
2. cgroup v2 `cpuset.cpus.effective` 和 CPU quota；
3. 在线 CPU topology；
4. `hardware_concurrency` 只作最后 fallback。

Windows 使用 process affinity、processor groups/job limits 和系统 topology。最终得到 `available_logical_cpus`、物理核/SMT关系、NUMA节点和允许 affinity。

用户显式 `cpu_workers` 永远优先，但必须限制在当前允许 CPU 集内。

## 3. ISA 安全注册

- 基础标量/reference translation unit 使用平台最低安全 ISA。
- 可选 SIMD kernel 分独立 translation unit 编译；构建系统先探测编译器是否支持。
- x86 runtime 同时验证 CPUID feature 和 OSXSAVE/XGETBV 保存状态；ARM 使用系统 feature API。
- 每个 variant 注册 `name / required_features / precision / alignment / kernel_version / function`。
- 未通过硬件或 OS 检测的 variant 绝不调用，避免 illegal instruction。

## 4. 每 kernel benchmark

不能用一个全局 AVX 结论覆盖所有算法。至少分别 benchmark：

- calibration pixel transform；
- noise/SNR statistics；
- drizzle overlap/accumulation；
- UPM SpMV/residual/weight update；
- rejection stack；
- integration reducer；
- HiPS encode/decode/bulk copy（只评估吞吐，不要求占满CPU）。

每个 kernel：

1. 先用 scalar reference 验证所有 variant 数值；失败 variant 禁用并报错。
2. warm-up；自适应重复直到计时稳定，不固定迭代次数。
3. 覆盖小/中/大三类数据规模、实际精度和对齐。
4. worker 候选由 `1..available` 自适应探索；大核数使用逐步扩展并在吞吐平台附近细化，不写死上限。
5. 记录 throughput、latency、CPU time/wall、内存带宽、RSS、频率/降频可得信息。
6. 选择科学正确且实测吞吐最高的 `(variant, workers, block_size)`；差距处于噪声带时选择更窄、更稳定的 variant。

## 5. 内存基准

同机测 copy/read/write/triad 参考带宽。算法低CPU但接近内存带宽上限时可判 memory-bound；否则低CPU视为并行/等待缺陷。

## 6. 缓存

写入 `cpu_profile.json`，key 至少包含：

- CPU vendor/family/model/stepping/microcode；
- affinity/cgroup/job限制和可用核数；
- OS/kernel；
- compiler及版本；
- build commit、binary hash；
- kernel version、precision；
- relevant memory/NUMA topology。

任一 key 变化自动失效。提供 `--benchmark-cpu`、`--cpu-profile`、`--cpu-workers`、`--cpu-variant`；强制 variant 仅用于测试，生产默认 autotune。

## 7. 启动开销

- 首次或 cache 失效时运行完整 benchmark；
- 普通启动只做轻量安全验证和读取 cache；
- benchmark 本身必须由资源监控包装，并有总 timeout。

