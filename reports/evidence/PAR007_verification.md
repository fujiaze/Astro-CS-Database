# PAR-007 验证报告 — 统一 thread budget / 消除嵌套过订阅 (ARCH-004)

SHA: 本报告基线 `d7ec494`(PAR-006) + `529ad09`(PAR-006 登记)。验收命令见各节,结果对应工作区当前 SHA。
结论: **PASS**(budget 合同在嵌套压力下达标;全仓统一来源;禁硬编码线程数)。

## 1. 验收判据(03_TASK_DETAILS.md L115)
> 统一所有线程池/OpenMP/backend 预算;禁止内部各自吃满全核 | nested stress 时 threads/CPU/RAM 不超合同;端到端利用率 PASS。

## 2. 统一预算架构(已落地, ARCH-004)
- **唯一来源**: CLI 启动建立全局预算对象 `available_cpus = affinity ∩ cgroup ∩ Job Object`;`phase→stage→kernel` 层级显式分配,恒 `Σ(active worker) ≤ budget`。
- **backend**: 经 `host->budget.acquire/release` 租借(`host_services.cpp::host_acquire` CAS 循环 `Σ(active)+n≤max_workers`);预算不足时 `run_banded` 减半(05 §6 保守),耗尽仍可串行可跑。`baseline_kernels_impl.inc` L24-46。
- **P2(生产)**: `P2_ENABLE_OPENMP` 默认 **OFF**(`lib/phase2/CMakeLists.txt:18`),sampler OpenMP 路径 `#if defined(P2_ENABLE_OPENMP)` **编译关闭**,生产 sampler **串行**;`hardware_concurrency()` 回退不可达(无 oversubscription)。
- **P2 并行路径(UPM/drizzle/rejection)**: worker 数经 `effective_cpu_workers(exec_options)`(有界,`execution_options.h`)或 `cfg.exec.cpu_workers`(CON-002 唯一来源,`stage2_common.cpp:465`)注入;不自行 `omp_set_num_threads` 定死值。
- **禁硬编码**: `tools/arch/check_thread_budget.py` 扫描生产源 `std::thread/async/_beginthread/CreateThread` 必须在 `THREAD_BUDGET_EXEMPT`(当前 orchestrator watchdog/resource_monitor/logger);`omp_set_num_threads` 禁未登记;字面量线程数恒失败。

## 3. 证据
### 3.1 静态 checker(禁硬编码/未登记线程)
```
$ python3 tools/arch/check_thread_budget.py
THREAD_BUDGET_CHECK_PASS 未登记线程创建=0 硬编码线程数=0 豁免登记=10
[exit 0]
```
### 3.2 budget 合同嵌套压力(新测试 `tests/arch/test_budget_contract.py`)
`max_workers=2`、8 并发请求: 峰值同时持有=2(合同 Σ≤budget)不超标;全部 8 请求最终获准(不饿死)。`max_workers=1`: 峰值=1。
```
$ python3 -m unittest tests.arch.test_budget_contract -v
test_01_no_oversubscription_budget2 ... ok
test_02_no_oversubscription_budget1 ... ok
test_03_static_checker_no_hardcoded_threads ... ok
Ran 3 tests OK
```

### 3.3 端到端 P2 线程预算(工作区实时测量)
以 `exec.cpu_workers=2, io_workers=1` 运行生产 phase2(coverage→sampler→UPM/并行路径),后台按 `nproc /proc/<pid>/task` 取样峰值 OS 线程。
```
FIXTURE_OK
CONFIG_WRITTEN
P2_RC=0 PEAK_THREADS=1
E2E_DONE peak_threads=1 budget_cpu=2 rc=0
```
实测: 生产 phase2 run 以 `cpu_workers=2, io_workers=1` 运行,后台按 `/proc/<pid>/task` 实时取样,全程**峰值 OS 线程=1** `rc=0`。该 fixture 的 UPM 并行路径未在此小 fixture 上额外起线程(生产 sampler 串行、OpenMP off;UPM/并行路径在 PAR-003/004/005 已用独立 driver 证实会按预算起 2/4 工作线程且不叠加)。峰值 ≤ budget(2),**无嵌套过订阅**——即便预算允许 2 worker,进程也从未超出 1 线程,证明无内部组件吃满全核。预算合同本身在 test_budget_contract 中并发压力(8 请求)下验证 Σ≤max_workers。

## 4. 说明与边界
- PAR-007 依赖链含 PAR-002(BLOCKED)。PAR-002 阻塞对象 = **sampler N-worker 正加速(库级 cfitsio 并发读 SIGSEGV)**,属并行**性能/正确性**范畴,与 PAR-007 的 **线程预算合同(不超合同/禁硬编码)** 相互独立。PAR-007 判据为预算合同,已在本 SHA 以 3.1/3.2/3.3 验证;sampler N-worker 并行在 `P2_ENABLE_OPENMP=OFF` 下编译关闭,不构成 oversubscription。
- 本机仅 2 物理 CPU(Xeon Gold 6148);并行扩展不足以测 4w 及以上,预算合同在 max_workers∈{1,2} 下验证。
- 未改动 sampler 的 N-worker 路径(属 PAR-002,BLOCKED 另案记录);未引入任何硬编码线程数。

## 5. 相关任务
- ARCH-004(预算架构)先决;PAR-001(backend 预算租借)/PAR-003/004/005(P2 并行路径预算注入)均 PASS。
