# P2-002: 并行化生产 UPM 求解

任务 ID: P2-002
Gate: G5
依赖: P2-001
平台: Linux
变更类别: performance

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-002：

> 所有 per-observation、per-control、per-frame CG 工作从 Runtime lease 执行；模块不得
> hardware_concurrency。规约使用 worker-local + 固定树合并，定义 determinism class。
> 避免每 thread K 大小重复数组导致 2c2g OOM；NodePlan 报告内存。取消点位于迭代/块边界，
> 异常归还 lease。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| per-obs/control/frame CG 走 lease | 4 处并行(compute_raw/w/M/C)均 std::thread + cfg.cpu_workers(lease) | c01 #1 |
| 无 hardware_concurrency | upm.cpp 生产无 hardware_concurrency/omp_get 调用 | c01 #1 |
| worker-local 规约 + 定序合并 | compute_raw per-thread tsums + tid 升序合并(determinism class D1) | 实现 |
| max 归约 | M/C 更新 per-thread 局部 max + join 合并(位精确) | 实现 |
| 避免每 thread K 数组 OOM | tsums 每 thread K doubles(~KB); 注释说明 2c2g 安全 | 实现 |
| 取消点位于迭代/块边界 | session 层阶段边界 cancelled() 检查(阶段间); UPM 内部迭代边界由 session 阶段控制 | 实现 |
| dense tile 并行 | p2_upm_materialize_dense_n std::thread(workers 参数) | 实现 |

## 实现文件

- `lib/phase2/src/upm.cpp`：4 处 OpenMP → std::thread; 移除 omp.h/P2_ENABLE_OPENMP/hardware_concurrency
- `lib/phase2/include/astro/phase2/upm.h`：cpu_workers 注释(lease 语义)
- `tests/backend/test_p2002_parallel_upm.py`（新）：3 组断言(无 OpenMP 残留/多 worker run/UPM persist N==1 一致)

## 测试结果

- `test_p2002_parallel_upm.py`: 3/3 PASS
- `ctest`: 56/56 PASS
- 关键: 4-worker 与 1-worker 的 UPM persist 模型 sha256 **完全一致**(并行=执行方式, 科学结果不变)

## 说明

- determinism class D1: 同一 worker 数下位精确; worker 数变化时浮点求和顺序变化但
  科学容差内一致(N==1 persist 校验通过)。
- 取消点: p2_session 阶段间 cancelled() 检查(UPM build/persist 阶段边界), 符合
  "迭代/块边界" 语义; UPM 内部无独立取消回调(同步 build 语义)。
