# 纯 CPU 并行、ISA 与 Benchmark 合同

## 1. 发布范围

本 Alpha 只发布纯 CPU 生产后端。ACR 保留但默认不构建进产品、不注册、不参与路由。支持架构仅 AMD64/x86-64；不承诺 ARM。

保守路径含义是“最低 ISA 兼容”，不是“单线程”。在可用 CPU≥2 且任务达到重计算阈值时，baseline 也必须通过 Runtime 多线程执行。

## 2. Provider 构建

| Provider | 最低要求 | 编译边界 | 发布要求 |
|---|---|---|---|
| baseline | AMD64 基线（SSE2 为架构基本能力） | 单独 target，不带全局高级 ISA flag | 必须随包，所有 kernel 有可靠实现 |
| avx2 | AVX2；若 kernel 使用 FMA 则另声明 FMA | 单独 DLL/so 或独立 object target | 只包含已验证热点；不支持时绝不加载 |
| avx512 | 每 kernel 声明 AVX512F/DQ/BW/VL 等具体子集 | 单独 DLL/so | 可以随包；benchmark 不快则不选 |

CLI/runtime 主二进制以 baseline 编译。不得让 loader 在完成 feature/ABI 检查前执行 provider 的高级指令初始化代码。

## 3. CPU 能力与配额探测

必须同时考虑：

- CPUID vendor/family/model/stepping 与 feature bits；
- OSXSAVE 与 XGETBV，确认 OS 保存 YMM/ZMM/opmask 状态；
- Linux affinity/cgroup quota/cpuset；Windows process/job/affinity 限制；
- 实际可用逻辑处理器而不是机器标称核心数；
- cache line、L1/L2/L3、NUMA group；
- 当前 provider 和 compiler build id。

禁止 CPU 型号字符串白名单、硬编码核心数、只检查 CPUID 不检查 OS 状态。

## 4. Benchmark 子命令

### 4.1 命令

```text
astrocs benchmark --quick --output astrocs-machine-profile.json
astrocs benchmark --full  --output astrocs-machine-profile.json
astrocs benchmark --verify-profile astrocs-machine-profile.json
```

`quick` 应在数分钟内完成；Linux 2c2g 默认 quick。`full` 在 Fatduck 运行，但仍只用合成 micro/mini workload，不调用真实 32R。

### 4.2 固定步骤

1. 记录机器、OS、配额、温度/频率可用信息和构建 ID。
2. 运行 baseline correctness Oracle；失败立即结束。
3. 对支持的 provider 运行同一 Oracle；错误 provider 标 `REJECTED_CORRECTNESS`。
4. 测内存 copy/read/write/triad，得到可用带宽基线。
5. 对每个代表 kernel 测工作规模 small/medium/large。
6. 对线程候选 `1..available` 测扩展；1 worker 只用于测量，不作为 heavy production 默认。
7. 对 block/tile 候选测吞吐、CPU、带宽和内存峰值。
8. 3 warmup + 7 measured，使用 median 与 MAD；单次有 timeout。
9. 按 kernel 选择最快且稳定、正确的 provider/worker/block。
10. 写 profile；再独立读取并验证一次。

### 4.3 防止错误“最快”

- 任一 correctness 失败的候选不计时排名；
- AVX-512 相比 AVX2 median 提升不足 3% 时选 AVX2，避免频率/能耗波动；
- 结果 MAD/median>5% 时重测一次短组；仍不稳定则选更保守候选；
- worker 增加后吞吐提升<3%且资源/内存更坏时选较少 worker，但 heavy workload 在 available≥2 时不得选 1；
- profile 选择和原始数据都保留，不只写 winner。

## 5. Profile 最低字段

```json
{
  "schema": "astrocs.cpu-profile/v1",
  "profile_id": "sha256:...",
  "created_utc": "...",
  "host": {
    "arch": "amd64",
    "vendor": "...",
    "family": 0,
    "model": 0,
    "stepping": 0,
    "os": "...",
    "logical_available": 2,
    "quota_signature": "...",
    "features": []
  },
  "build": {
    "astrocs_version": "0.10.0-alpha.1",
    "source_commit": "...",
    "compiler": "...",
    "runtime_build_id": "...",
    "provider_build_ids": {}
  },
  "memory_bandwidth": {},
  "kernels": {
    "astrocs.drizzle.accumulate/v1": {
      "provider": "avx2",
      "workers": 2,
      "block": 0,
      "correctness_test": "TEST-...",
      "median": 0.0,
      "mad": 0.0
    }
  }
}
```

profile 只对完全相同的 arch/CPU signature/OS ABI/quota class/runtime build/provider build/benchmark schema 有效。输入 workload 不匹配时，模块可用 ALG 声明的 scale class 选择相邻档，但必须记录 reason。

## 6. Runtime 线程合同

- Runtime 计算 `available_cpu` 并拥有唯一 pool。
- 模块 plan 返回 work units、parallel_min_work、memory estimate、splittable axes、determinism policy。
- Runtime 发 `ThreadLease`；模块只向 lease executor 投递 work。
- 不允许模块内 `omp_set_num_threads`、`std::async` 无界启动、私有永久 pool。
- 保留 OpenMP 的 kernel 使用 `num_threads(lease.size)`，nested disabled；调用前后测试 active team size。
- 同时运行多个 heavy nodes 时，总 active workers 不超过 budget；IO executor 不计入 CPU-heavy pool但有小上限。

## 7. 分块与负载均衡

- Drizzle/Phase3 优先空间 tile + 动态 work queue；避免极区/稀疏 coverage 造成静态块严重不均。
- Phase2 sampling 按预估有效 sample 数而非单纯 tile 数分块。
- block 太小的调度开销与太大的尾部效应由 benchmark 决定。
- 每 worker 指标至少有 processed work、busy time、wait time、steal count、bytes read/written；max/min work 比过大自动 finding。
- 归约采用 worker-local accumulator + 固定树合并，避免全局 mutex/atomic 热点。

## 8. 资源监测合同

每个 CPU-heavy node 自动启动监测，不依赖执行者额外记得加脚本。输出：

- `resource_samples.csv`：UTC/elapsed、process CPU、system CPU、active workers、RSS/commit、page fault、read/write bytes、queue depth、lock wait、progress；
- `resource_summary.json`：初始化、active、flush 分段；mean/p50/p95；峰值；瓶颈分类；门禁；
- `worker_balance.csv`：每 worker 工作量/忙等；
- `RESOURCE_FINDING.md`：仅在门禁失败时生成。

Linux 2 核 active window：normalized process CPU 的 100% 表示两个可用逻辑核全部使用。Windows 采用同一定义，避免任务管理器不同百分比口径。

## 9. 资源门判定

CPU-bound heavy node：

- available≥2 时 active worker p50≥2；
- normalized CPU p50≥90%，mean≥85%；
- 长度不足 10s 的任务只记录，不作为持续利用率门；另用扩大合成 workload 达到≥10s；
- 单个瞬时峰值不能通过。

低 CPU 自动分类：

1. lock wait >10%：`LOCK_BOUND_FAIL`；
2. I/O wait >10% 且算法 active queue 空：`IO_BOUND_INVESTIGATE`；
3. worker imbalance >1.5：`IMBALANCE_FAIL`；
4. memory bandwidth ≥80% benchmark：`MEMORY_BOUND_EVIDENCED`，仍需检查数据搬运/布局；
5. serial fraction/active worker<2：`SERIAL_PATH_FAIL`；
6. 无法分类：`LOW_UTILIZATION_UNEXPLAINED_FAIL`。

任何 FAIL 不得通过改报告阈值消失；必须源码修复或按最高约束取得负责人明确批准。

## 10. ACR 未来兼容

科学模块只依赖抽象 kernel ID 与参数合同。未来 ACR provider 可以加入 registry，但不得改变 Pipeline IR、DATA schema、SCI/ALG 或模块入口。当前 profile schema可预留 provider kind，不加载 ACR、不产生 GPU 结论。
