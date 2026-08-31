# P2-001: 并行化生产 Coverage 与 Sampler

任务 ID: P2-001
Gate: G5
依赖: RT-008; MON-003
平台: Linux
变更类别: performance

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-001：

> 删除 `P2_ENABLE_OPENMP` 默认关闭和 `_MSC_VER` 排除造成的生产串行。优先使用
> Runtime tile/work queue；如保留 OpenMP 必须 `num_threads(lease.size)`、nested off。
> 每 worker 独立 reader/CFITSIO handle，按预计有效 sample 负载分块。1-worker 只作
> reference；Linux/MSVC N-worker 实测调用同生产符号。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 删除 P2_ENABLE_OPENMP 默认关闭 | sampler.cpp 移除 OpenMP 条件; 改用 std::thread(跨平台一致, 不依赖 OpenMP) | c01 #1 |
| 删除 _MSC_VER 排除 | 并行执行不再被 _MSC_VER 排除(无条件 std::thread) | 实现 |
| Runtime lease 多 worker | p2_session 传 `budget.max_workers` → cfg.cpu_workers → sampler workers | c01 #1 |
| 无 hardware_concurrency | sampler.cpp 生产无 `::hardware_concurrency()` 调用(仅注释提及) | c01 #1 |
| 每 worker 独立 reader | SamplerReader.init_own(每 worker 独立 CFITSIO 句柄) 保留 | 实现 |
| 分块 | std::atomic next_c 动态分块(等价 schedule(dynamic)) | 实现 |
| 1-worker reference | cfg.cpu_workers=1 → 串行分支(共享句柄) | c01 #3 |
| N-worker 结果一致 | 4-worker 与 1-worker obs=1536 一致 | c01 #3 |
| Linux/MSVC 同符号 | std::thread 跨平台; 无 OpenMP/_MSC_VER 分支 | 实现 |

## 实现文件

- `lib/phase2/src/sampler.cpp`：并行执行改为 std::thread + atomic 分块; 移除 OpenMP 条件;
  worker 数来自 cfg.cpu_workers(Runtime lease); 移除 omp.h include
- `lib/phase2_session/p2_session.cpp`：sampler `cpu_workers=1` → `budget.max_workers`(lease 绑定)
- `lib/phase2/include/astro/phase2/sampler.h`：cpu_workers 注释更新(lease 语义)
- `tests/backend/test_p2001_parallel_sampler.py`（新）：3 组断言(无 OpenMP 残留/多 worker 成功/N-worker==1-worker)

## 测试结果

- `test_p2001_parallel_sampler.py`: 3/3 PASS(生产无 OpenMP 门 + phase2 run 默认多 worker + N==1 一致性)
- `ctest`: 56/56 PASS
- 实跑 phase2 run: obs=1536(多 worker), 4-worker 与 1-worker 结果一致

## 说明

- 并行只是执行方式: pass1_cell 科学逻辑一字未动(串行/并行共用 body, 杜绝双份漂移)。
- reduction 用 per-thread 累加 + join 后合并, 整型计数无顺序依赖(确定性)。
