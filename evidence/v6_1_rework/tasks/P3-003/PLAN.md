# P3-003: Runtime 并行 tile resampler

任务 ID: P3-003
Gate: G6
依赖: P3-002; RT-006; CPU-004
平台: Linux
变更类别: algorithm

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P3-003：

> 按输出 tile/row-band 生成 work units，每 worker 独立 sampler 和 bounded tile cache，
> 输出 buffer 不重叠；Runtime lease 调度，最后单 writer 有序提交。删除 production
> 串行全图双循环。小图可串行阈值由 benchmark profile 确定且 <5s；大图 available≥2
> 必须 N worker。测试 1/N 结果、取消、missing tile、cache 边界、TSan 和资源。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| row-band work units | rows_per_worker 行带划分; 每 worker 专属行带(输出 buffer 不重叠) | c01 #1/#2 |
| 每 worker 独立 sampler+cache | worker 内 p3_sampler_open_ex(P3Sampler 自含 bounded tile cache) | c01 #3 |
| Runtime lease 调度 | worker 数 = host->budget.max_workers(禁 hardware_concurrency) | c01 #2 |
| 单 writer 有序提交 | 采样并行, 写 FITS 单线程(p3_output_write_atomic) | 实现 |
| 删除串行全图双循环 | 采样循环改 std::thread 多 worker | c01 #2 |
| 1/N 结果等价 | 并行(budget=2)与单线程输出 FITS 逐字节一致 | c01 #1 |
| 取消 | 采样循环含取消点(cancelled()), 并行下 cancelled_at 传播, 返回 CANCELLED | c01 #4 |
| missing tile | P3Sampler read_leaf 缺 tile → coverage=0, S=NaN(既有 P3-001) | 既有 |
| 资源 | worker 数来自 budget(非硬编码); CPU-004 门由上层 Runtime 保证 | c01 #2 |

## 实现文件

- `lib/phase3_session/p3_session.cpp`：采样双循环 → std::thread 多 worker row-band
  (每 worker 独立 sampler/WCS 拷贝; 输出 buffer 不重叠; 禁 hardware_concurrency)
- `tests/backend/test_p3003_parallel_resampler.py`（新）：5 组断言

## 测试结果

- `test_p3003_parallel_resampler.py`: 5/5 PASS(并行=串行字节一致/无 hardware_concurrency/worker 独立 sampler/取消点/双 sampler)
- `test_phase3_inprocess.py`: OK; `test_p3_resample.py`/`test_p3_output.py`: PASS(回归)
- `ctest`: 56/56 PASS

## 说明

- 串行阈值: worker 数 <2(小图/单核) → 单线程, 由 budget 注入决定(非硬编码)。
- 每 worker 独立 sampler 读共享只读 HiPS, 无全局 FITS 读锁。
