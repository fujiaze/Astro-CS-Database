# Threading Model

## 分层

- 编排层：orchestrator/stage2 顺序 stage，内部 OpenMP parallel-for。
- 科学模块：内部 OpenMP parallel region；每模块文档化 parallel/shared/
  thread-local/reduction/determinism/float accumulation order。
- ACR：work_pool + device_executor 调度；CPU reference 与 GPU 等价契约。
- 浏览器：Qt 主线程 + 后台 I/O 线程；renderer 只读共享数据。

## 约定

- 禁止库内修改全局 OpenMP 设置；线程数由 run context 配置。
- 计数器：atomic 或 thread-local 聚合（禁止裸 data race counter）。
- 浮点累积顺序固定（确定性输出）；reduction 顺序文档化。
- cache（dense UPM、Gaia 查询缓存）必须线程安全或单线程互斥访问。

## 确定性锚点（ARC-004）

- Phase2 UPM 权重归一：`lib/phase2/src/upm.cpp:495` `compute_raw` — `raw_w = quality_factor * control_ivar` 冻结后按 control `sums[ck]` 归一（`raw_w[i]/sums[ck]*reliability`），遍历顺序为观测索引 `i` 固定顺序；确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Phase2 sampler：`lib/phase2/src/sampler.cpp` **串行（hotfix 后无任何 OpenMP parallel region）**——`af763de` 已移除 `#pragma omp parallel for` / `critical(aio_read)`，`lib/phase2/CMakeLists.txt:18` `option(P2_ENABLE_OPENMP OFF)` 硬禁用；`P2_ENABLE_OPENMP=ON` 仅使构建定义 `-fopenmp` 并链接 `libgomp`（compile/link 接线存在），**代码无并行区，故 ON/OFF 数值恒等价（79 PASS identical）**；`cells` 预分配按 `c*64+off` 确定性索引、串行遍历，确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Drizzle 浮点归约：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1662` `reduction(+:nSourcePixels,prof_geom_s,prof_wcs_s)`；`1751` tile 合并 `sumFlux/sumArea/sumVarNum/nContrib` 经 thread-local `TileAccumulator` 后串行合并（`t=1..num_threads` 固定顺序）；`1834`/`1843` `parallel reduction(+:n_quick,n_fully,n_dropin,n_sh)` 与 `atomic` 计时累加 — 浮点累积顺序固定，reduction 顺序已文档化。

## 契约

ENG-THREAD-001..003（S2 注册）。
