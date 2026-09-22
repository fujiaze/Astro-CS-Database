# Phase3 模块架构 (HiPS reader → WCS → resampler → FITS writer)

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> ID: ARCH-P3-001  状态: FROZEN  上游: ALG-007(PHASE3_RESAMPLE.md)/ARCH-002/ARCH-004  下游: API-005/CODE-P3/SYN-007
> 原则: **科学选择全部落在 ALG-007 冻结公式(G1–G5),本架构只定义模块边界/数据结构/并发与内存上界——不把科学决策藏进 cache/loader。**

## 1 模块与数据结构(lib/phase3,四单元单向流)

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

## 2 跨 tile 访问(科学语义在 ALG,缓存只管取放)

- bilinear 需要 4 leaf 时由 Resampler 计算 leaf 邻域(NESTED 父子公式, ALG-P3-003), TileCache 仅按 (tile_ipix) 提供原子引用;**cache 不做任何插值/加权/order 决策**(数据未命中→同步阻塞加载, 返回只读 span)。
- tile 边界读: 邻居 tile 未命中→按需加载;文件缺失→`TileRef.missing=true`(C=0,S=NaN 语义由 Resampler 依 ALG 决定)+provenance 记 missing;**cache 永不伪造数据**(无零填充兜底)。

## 3 并发与内存上界(ARCH-004 合同实例化)

- 调度单元 = 输出**子块**(边长 `sub_block_px`, 编排参数, 由配置/资源门决定, 禁硬编码):
  「读子块 → 投影重采样 → 写子块」三级**有界**流水线(读/算/写) + 背压, 队列深度 `queue_depth`;
  在途子块总量受 `2·queue_depth` 约束, 队列满即上游阻塞(不无界增长)。
- worker 预算经 host budget 派生(禁硬编码线程数); TileCache 为共享读+互斥加载(未命中加载持锁, 命中读无锁)。
- 写面: 子块经 cfitsio 子集接口写进目标 HDU 的**数据区**(子块索引升序, 单写者, 区间互不重叠
  ⇒ 输出与 worker 数无关); PRIMARY 头(WCS/BUNIT/provenance/HISTORY)在**首像素写出前**组装完成。
- 内存上界(冻结): `M ≤ 2·queue_depth·sub_block_px²·(4|8) + max_tiles·W²·(4|8) + 常数`
  —— **与输出总图大小 W_out·H_out 无关**(ASTROCS_DESIGN §8.3 export 行: 「内存占用与子块大小
  成正比、与总图大小无关」)。
  `sub_block_px` 默认 256、值域 [16,1024](内存守卫); `queue_depth` 默认 4、值域 [1,64];
  `max_tiles` 默认 `min(1024, ceil(W_out·H_out/W²)+16)` 且配置可降不可升超物理内存守卫
  (07 资源门联动); 超出→诊断事件+`rc=MEM_BUDGET`(不静默换页)。
- 中间产物 `p3_resampled.bin` 由子块**位置写**装配(平面 = 行主序连续区, 子块行区间互不重叠
  ⇒ 与写出顺序、worker 数无关), 全部子块成功后才 fsync + 原子 rename(失败不留半成品)。
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
