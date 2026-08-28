# Phase3 模块架构 (HiPS reader → WCS → resampler → FITS writer)

> ID: ARCH-P3-001  状态: FROZEN (V5 ARCH-005, 2026-08-28)  上游: ALG-007(PHASE3_RESAMPLE.md)/ARCH-002/ARCH-004  下游: API-005/CODE-P3/SYN-007
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
| Resampler | (HiPSContext,TileCache,WCS,out_params)→`OutPlane{S,C}` | `OutPlane{S: span<float>, C: span<uint8>, W_out,H_out}` | G2/G3/G4(order_needed/反向映射/采样) |
| FitsWriter | (OutPlane,WCS,provenance,path)→原子 FITS | `FitsDesc{BITPIX,BUNIT,WCS keys,HISTORY}`; tmp+rename | G5 |

- 依赖: `lib/common/healpix`(ang2pix/pix2ang 唯一实现, round-trip ≤1e-12 deg)、`astro_image_io`(FITS 底层写)。不引入第二 HEALPix/第二 FITS 写路径。

## 2 跨 tile 访问(科学语义在 ALG,缓存只管取放)

- bilinear 需要 4 leaf 时由 Resampler 计算 leaf 邻域(NESTED 父子公式, ALG-P3-003), TileCache 仅按 (tile_ipix) 提供原子引用;**cache 不做任何插值/加权/order 决策**(数据未命中→同步阻塞加载, 返回只读 span)。
- tile 边界读: 邻居 tile 未命中→按需加载;文件缺失→`TileRef.missing=true`(C=0,S=NaN 语义由 Resampler 依 ALG 决定)+provenance 记 missing;**cache 永不伪造数据**(无零填充兜底)。

## 3 并发与内存上界(ARCH-004 合同实例化)

- 行带 worker pool(预算经 host budget 派生, 禁硬编码);TileCache 为共享读+互斥加载(未命中加载持锁, 命中读无锁)；单写者: OutPlane 每行带独立区间, 无跨行带写竞争。
- 内存上界(冻结): `M ≤ W_out·H_out·(4|8) + max_tiles·W²·(4|8) + 常数`;`max_tiles` 默认 `min(1024, ceil(W_out·H_out/W²)+16)` 且配置可降不可升超物理内存守卫(07 资源门联动);超出→诊断事件+`rc=MEM_BUDGET`(不静默换页)。
- I/O 线程 1(异步预取深度=1, ARCH-004 §2);取消=行带粒度, 取消时 FitsWriter 不发生(tmp 删除), TileCache 丢弃未引用项。

## 4 错误/回退

- reader 拒绝类(properties 非法/lossy tile/frame≠icrs)=启动前显式拒(ALG-P3-001 拒绝码表)——**不进入半成品 run**;
- 运行中 tile IO 错误(非缺失)=stage 安全中止(ARCH-003 §6-2 同款, 禁静默降级 nearest);
- FitsWriter 落盘失败=tmp 清理+错误码, 目录无残留产物。

## 5 追溯(逐 claim 到 ALG-007)

| 架构声明 | ALG-007 锚 |
|---|---|
| 拒绝清单/properties 校验 | ALG-P3-001(§2 来源声明+§4 表) |
| WCS 构造/反向映射 | ALG-P3-002(G1/G2) |
| order 选择/采样/coverage | ALG-P3-003(G3/G4/G5-coverage) |
| FITS 原子写/provenance | ALG-P3-004(G5) |
| 并发/内存上界 | THREAD_BUDGET_ARCH §2/§3+本文件 §3 |
| 容差/Oracle 独立性 | ALG-P3 §8/§9(Oracle 不调本模块) |
