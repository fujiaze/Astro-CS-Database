# Phase3 模块架构 (HiPS reader → WCS → resampler → FITS writer)

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> ID: ARCH-P3-001  状态: FROZEN  上游: ALG-007(PHASE3_RESAMPLE.md)/ARCH-002/ARCH-004  下游: API-005/CODE-P3/SYN-007
> 原则: **科学选择全部落在 ALG-007 冻结公式(G1–G5),本架构只定义模块边界/数据结构/并发与内存上界——不把科学决策藏进 cache/loader。**

## 1 模块与数据结构(lib/phase3_session + lib/algorithms/{resample,fits_output,projection},四单元单向流)

```text
[HiPSReader]  →  [TileCache]  →  [Resampler]  →  [FitsWriter]
   ALG-P3-001        (纯缓存)       ALG-P3-002/003      ALG-P3-004
```

| 单元 | 输入→输出 | 关键数据结构 | 权威公式(不在此重复定义) |
|---|---|---|---|
| HiPSReader | hips_dir+params→`HiPSContext`(properties 校验后不可变快照) | `HiPSProperties{hips_order,tile_width,frame,dataproduct}`; 拒绝码表 | §9a-1/2/8 显式拒绝清单 |
| TileCache | (tile_ipix)→`TileRef{ipix, W×W float32/64, source_path}` | LRU(容量=配置 max_tiles), 命中/未命中同一 `load_tile` 路径 | ALG-P3-003 G4 |
| Resampler | (HiPSContext,TileCache,WCS,out_params)→子块 `OutBlock{S,C}` | `OutBlock{S: span<float>, C: span<uint8>, x0,y0,w,h}`(单子块驻留) | G2/G3/G4(order_needed/反向映射/采样) |
| FitsWriter | (子块流,WCS,provenance,path)→原子 FITS | `FitsDesc{BITPIX,BUNIT,WCS keys,HISTORY}`; 子集写数据区; tmp+rename | G5 |

- 依赖: `lib/algorithms/shared/healpix`(ang2pix/pix2ang 唯一实现, round-trip ≤1e-12 deg)、`astro_image_io`(FITS 底层写)。不引入第二 HEALPix/第二 FITS 写路径。

> ⚠ **设计态符号未落码（目标态）**：本表「关键数据结构」列的 `HiPSProperties`/`TileRef`/`OutBlock`
> 及 §3 编排键 `sub_block_px`/`queue_depth` 在 lib/ 生产面**零命中**（未落码，目标态命名，登记不删除）；
> 实现态符号 = `P3Sampler`（`lib/algorithms/resample/p3_resample.h`）/
> `SharedTileCache`（`lib/algorithms/resample/p3_resample.cpp:129`）/
> `p3_sampler_set_max_tiles`（`p3_resample.cpp:228` 定义；`lib/phase3_session/p3_session.cpp:194` 调用）/
> `max_tiles`（配置键，守卫见 §3）。
> 目录锚：设计态目录名 `lib/phase3` 不存在；生产实况 = `lib/phase3_session`
> （p3_session/p3_wcs/p3_resample/p3_output 四源文件）+ `lib/algorithms/{resample,fits_output,projection}`。
> 设计-实现映射表待建（P3 文档整改项）。注：`p3_resample.cpp` 文件头注释仍自述旧路径
> `lib/phase3_session/p3_resample.cpp`，实际位于 `lib/algorithms/resample/`（代码注释未随目录迁移更新，范围外已登记）。

## 2 跨 tile 访问(科学语义在 ALG,缓存只管取放)

- bilinear 需要 4 leaf 时由 Resampler 计算 leaf 邻域(NESTED 父子公式, ALG-P3-003), TileCache 仅按 (tile_ipix) 提供原子引用;**cache 不做任何插值/加权/order 决策**(数据未命中→同步阻塞加载, 返回只读 span)。
- tile 边界读: 邻居 tile 未命中→按需加载;文件缺失→`TileRef.missing=true`(C=0,S=NaN 语义由 Resampler 依 ALG 决定)+provenance 记 missing;**cache 永不伪造数据**(无零填充兜底)。

## 3 并发与内存上界(ARCH-004 合同实例化)

- **已实例化（现行实况）**: 并行编排 = 每工作线程一个 `P3Sampler` 实例 + 跨 worker **共享有界 LRU**
  tile 缓存(容量 cap = `max_tiles`, 与 worker 数无关 ⇒ 峰值内存不随核数增长) + 缺失 tile **负缓存**
  (每 tile 至多一次真实 open, 结果与无负缓存逐位相同) + 每线程前端热缓存(命中不取共享锁)
  （`lib/algorithms/resample/p3_resample.cpp:7-16` P30 修复注记/`:56-62`/`:129`）;
  worker 预算经 Runtime lease 派生(禁硬编码线程数)。
  ~~原「调度单元 = 输出子块(边长 `sub_block_px`) + 『读子块→投影重采样→写子块』三级有界流水线
  + 背压(队列深度 `queue_depth`, 在途受 2·queue_depth 约束)」~~ —— **未落码（目标态）**:
  `sub_block_px`/`queue_depth` 两编排键在 lib/ 生产面零命中（仅 ACR DORMANT 面
  cost_estimator/mixed_route_planner 同名），生产 P3 无三级流水线与背压承载。
- TileCache 共享读 + 互斥加载（未命中加载持共享缓存锁，命中走每线程热缓存无锁）。
- 写面: 子块经 cfitsio 子集接口写进目标 HDU 的**数据区**(子块索引升序, 单写者, 区间互不重叠
  ⇒ 输出与 worker 数无关); PRIMARY 头(WCS/BUNIT/provenance/HISTORY)在**首像素写出前**组装完成。
- 内存上界(冻结): **现行承载支路** = `M ≤ max_tiles·W²·(4|8) + Σ_per-worker(热缓存 ≤ kHotSlots 槽·W²·(4|8)) + 常数`
  （`p3_resample.cpp:56-62` 注释「峰值内存 = cap MiB + Σ(每线程热缓存 pin ≤ kHotSlots 个), 有界」）;
  原式 `2·queue_depth·sub_block_px²·(4|8)` 支路属**未落码（目标态）**编排（见上），不参与现行内存核算。
  两支路均 **与输出总图大小 W_out·H_out 无关**(ASTROCS_DESIGN §8.3 export 行)。
  `max_tiles` 默认 `min(1024, ceil(W_out·H_out/W²)+16)` 且配置**可降不可升**超内存守卫——**已逐字落码**
  （`lib/phase3_session/p3_session.cpp:179-194`「max_tiles 内存守卫(ARCH-P3 §3): 请求可降不可升,
  默认 min(1024, ceil(W·H/W²)+16)」, 超出 → last_error + `ACS_ERR_BUDGET` fail-closed, 不静默换页）;
  资源门联动见 `docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md`。
- 中间产物 `p3_resampled.bin` 由子块**位置写**装配(平面 = 行主序连续区, 子块行区间互不重叠
  ⇒ 与写出顺序、worker 数无关), 全部子块成功后才 fsync + 原子 rename(失败不留半成品)
  （调度器适配层 artifact 约定记载: `module_adapters.cpp:15508-15509` p3_props.json → p3_wcs.json → p3_resampled.{json,bin} → output_phase3.fits → p3_verify.json）。
- 独立重开 verify 同样按子块读回对拍(尺寸/WCS 关键字/HDU 面/逐像素/NaN 同态/COVERAGE 掩码)。
- I/O 线程 1(异步预取深度=1, ARCH-004 §2); 取消=子块粒度, 取消时 FitsWriter 不发生(tmp 删除),
  TileCache 丢弃未引用项。

## 4 错误/回退

- reader 拒绝类(properties 非法/lossy tile/`hips_frame` ∉ {equatorial, icrs})=启动前显式拒(ALG-P3-001 拒绝码表)——**不进入半成品 run**;
- 运行中 tile IO 错误(非缺失)=stage 安全中止(ARCH-003 §6-2 同款, 禁静默降级 nearest);
- FitsWriter 落盘失败=tmp 清理+错误码, 目录无残留产物。

## 5 追溯(逐条到 ALG-007)

| 架构声明 | ALG-007 锚 |
|---|---|
| 拒绝清单/properties 校验 | ALG-P3-001(§2 来源声明+§4 表) |
| WCS 构造/反向映射 | ALG-P3-002(G1/G2) |
| order 选择/采样/coverage | ALG-P3-003(G3/G4/G5-coverage) |
| FITS 原子写/provenance | ALG-P3-004(G5) |
| 并发/内存上界 | THREAD_BUDGET_ARCH §2/§3+本文件 §3 |
| 容差/Oracle 独立性 | ALG-P3 §8/§9(Oracle 不调本模块) |
