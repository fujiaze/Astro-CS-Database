# Phase3 模块架构（HiPS reader → WCS → resampler → FITS writer）

> 上游：ASTROCS_DESIGN.md §8（软件架构）、§6.2（export 流程）、§6.3（投影算法）

**职责边界**：科学选择全部落在 `docs/science/algorithms/PHASE3_RESAMPLE.md` 的冻结公式 G1–G5，本架构只定义模块边界、数据结构、并发与内存上界——不把科学决策藏进 cache/loader。

## 1 模块与数据结构

模块与生产目录：`lib/phase3_session` 与 `lib/algorithms/{resample,fits_output,projection}`，四单元单向流。

```text
[HiPSReader]  →  [TileCache]  →  [Resampler]  →  [FitsWriter]
```

| 单元 | 输入→输出 | 关键数据结构 | 实现锚 |
|---|---|---|---|
| HiPSReader | hips_dir+params → `HiPSContext`（properties 校验后不可变快照） | `HiPSProperties{hips_order,tile_width,frame,dataproduct}`；拒绝码表 | `p3_session.cpp` |
| TileCache | (tile_ipix) → 瓦片引用 | 跨 worker 共享有界 LRU（容量 = `max_tiles`）+ 缺失 tile 负缓存 + 每 sampler 热缓存 | `p3_resample.h/.cpp` |
| Resampler | (HiPSContext, TileCache, WCS, out_params) → 输出像素 | `P3Sampler`（每工作线程一实例）；采样核 nearest / bilinear | `p3_resample.cpp`、`p3_wcs.cpp` |
| FitsWriter | (输出面, WCS, provenance, path) → 原子 FITS | `FitsDesc{BITPIX,BUNIT,WCS keys,HISTORY}`；tmp+rename | `p3_output.cpp` |

实现锚（上表末列全名）：

- `p3_session.cpp` = `lib/phase3_session/p3_session.cpp`（properties 校验、TileCache 装配、生产调用点）；拒绝码正本 = `docs/science/algorithms/PHASE3_RESAMPLE.md` §9a-1/2/8。
- `p3_resample.h` / `p3_resample.cpp` = `lib/algorithms/resample/`（`SharedTileCache`、`p3_sampler_attach_cache`、`p3_sampler_set_max_tiles`、`p3_order_select`、采样核、`P3Sampler`）。
- `p3_wcs.cpp` = `lib/algorithms/projection/p3_wcs.cpp`（WCS 与投影面）。
- `p3_output.cpp` = `lib/algorithms/fits_output/p3_output.cpp`（原子 FITS 写出）。

- 依赖：`lib/algorithms/shared/healpix`（ang2pix/pix2ang 唯一实现，round-trip ≤1e-12 deg）、`astrocs_aio`（FITS 底层写）。不引入第二 HEALPix、第二 FITS 写路径。

## 2 跨 tile 访问（科学语义在算法正本，缓存只管取放）

- bilinear 需要 4 leaf 时由 Resampler 计算 leaf 邻域（NESTED 父子公式），TileCache 仅按 tile 身份提供原子引用；**cache 不做任何插值/加权/order 决策**（未命中 → 同步阻塞加载，返回只读 span）。
- tile 边界读：邻居 tile 未命中 → 按需加载；文件缺失 → 记 missing（C=0、S=NaN 语义由 Resampler 依算法正本决定）并在 provenance 逐条登记；**cache 永不伪造数据**（无零填充兜底）。

## 3 并发与内存上界

- **并行编排**：每工作线程一个 `P3Sampler` 实例 + 跨 worker **共享有界 LRU** tile 缓存（容量 cap = `max_tiles`，与 worker 数无关 ⇒ 峰值内存不随核数增长）+ 缺失 tile **负缓存**（每 tile 至多一次真实 open，结果与无负缓存逐位相同）+ 每线程前端热缓存（命中不取共享锁）。worker 预算经 Runtime lease 派生（禁硬编码线程数，见 `THREAD_BUDGET_ARCH.md`）。
- TileCache 共享读 + 互斥加载（未命中加载持共享缓存锁，命中走每线程热缓存无锁）。
- 写面：像素经 cfitsio 子集接口写进目标 HDU 的**数据区**（子块索引升序、单写者、区间互不重叠 ⇒ 输出与 worker 数无关）；PRIMARY 头（WCS/BUNIT/provenance/HISTORY）在**首像素写出前**组装完成。
- 内存上界（冻结）：`M ≤ max_tiles·W²·(4|8) + Σ_per-worker(热缓存 ≤ kHotSlots 槽·W²·(4|8)) + 常数`，**与输出总图大小 W_out·H_out 无关**（`ASTROCS_DESIGN.md` §8.3 export 行）。
- `max_tiles` 默认 `min(1024, ceil(W_out·H_out/W²)+16)`，配置**可降不可升**：请求超出内存守卫上限 → 记 last_error + `ACS_ERR_BUDGET` fail-closed（不静默换页）。资源门联动见 `docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md`。
- 中间产物 `p3_resampled.bin` 由子块**位置写**装配（平面 = 行主序连续区，子块行区间互不重叠 ⇒ 与写出顺序、worker 数无关），全部子块成功后才 fsync + 原子 rename（失败不留半成品）。调度器适配层的 artifact 约定见 `lib/infrastructure/scheduler/src/module_adapters.cpp`（`p3_props.json` → `p3_wcs.json` → `p3_resampled.{json,bin}` → `output_phase3.fits` → `p3_verify.json`）。
- 独立重开 verify 同样按子块读回对拍（尺寸 / WCS 关键字 / HDU 面 / 逐像素 / NaN 同态 / COVERAGE 掩码）。
- I/O 线程 1（异步预取深度 = 1，见 `THREAD_BUDGET_ARCH.md`）；取消 = 子块粒度，取消时 FitsWriter 不发生（tmp 删除），TileCache 丢弃未引用项。

## 4 错误与回退

- reader 拒绝类（properties 非法 / lossy tile / `hips_frame` ∉ {equatorial, icrs}）= 启动前显式拒（拒绝码正本 = `docs/science/algorithms/PHASE3_RESAMPLE.md` §9a），**不进入半成品 run**；
- 运行中 tile IO 错误（非缺失）= stage 安全中止（禁静默降级 nearest；错误码与传播链见 `docs/architecture/ERROR_MODEL.md`）；
- FitsWriter 落盘失败 = tmp 清理 + 错误码，目录无残留产物。

## 5 追溯（逐条到算法正本）

| 架构声明 | 算法正本锚（`docs/science/algorithms/PHASE3_RESAMPLE.md`） |
|---|---|
| 拒绝清单 / properties 校验 | §9a-1/2/8 显式拒绝清单 |
| WCS 构造 / 反向映射 | G1 / G2 |
| order 选择 / 采样 / coverage | G3 / G4 / G5-coverage |
| FITS 原子写 / provenance | G5 |
| 并发 / 内存上界 | `THREAD_BUDGET_ARCH.md` §2/§3 + 本文件 §3 |
| 容差 / Oracle 独立性 | §8/§9（Oracle 不调本模块） |
