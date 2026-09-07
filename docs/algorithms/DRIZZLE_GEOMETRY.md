# Drizzle Geometry Algorithms (P1-DRZ)

> ID 覆盖: ALG-DRZ-001  状态: CONTRACT_READY (P1-DRZ-DOC 冻结, 2026-09-07)  上游: SCI-DRZ-001  下游: DATA-P1-DRZ / API-DRZ-001 / TEST-DRZ-DESIGN-001
> 本文档由源码逐函数核对后重写（P1-DRZ-DOC）。实现唯一生产源 =
> `lib/healpix_db/healpix_drizzle/`（CMake 目标 `astrocs_drizzle`，
> CMakeLists.txt:356-366；C ABI 导出 `lib/healpix_db/healpix_drizzle/
> hp_drizzle_api.h:42,62,70,130,139,140`）；迁移目标目录 `lib/drizzle/`
> （落码由 P1-DRZ-IMPL 执行，尚未存在生产符号）。科学定义见
> `docs/science/DRIZZLE.md`（SCI-DRZ-001，FROZEN T105 2026-08-23，
> 集合 SCI-DRZ-001/014/015/016）。本文档只登记离散算法与实现事实，
> 不修改 SCI；源码与 SCI 的差异全部登记于 §10（DISP-DRZ-*），
> 禁止根据代码错误反向修改 SCI。禁止声明 IMPLEMENTED。

## 0 范围界定

本文档将旧版 `ALG-DRZ-001..016` 区间收缩为单一 **ALG-DRZ-001**（drizzle
几何与累加合同），覆盖现行唯一生产 tiled 实现全链路
（`drizzleTiled[_f64]` → `processPixelSharedTiled/processPixelTiled` →
球面几何 → tile 累加器），P1-DRZ-IMPL/TEST 均以本文档为准：

- 候选缓冲/几何合同旧名 `ALG-DRZ-GEOM-CACHE-001`、`ALG-DRZ-VAR`、
  `TEST-ALG-DRZ-*` 为历史登记名（源码注释中仍可见），现行合同一律引用
  ALG-DRZ-001 与 TEST-DRZ-DESIGN-001，不再使用旧名登记新事实。
- S_p=F_p/D_p 面亮度归一与 variance/ivar finalize 属 astro_image_io
  下游（astro_sphere_sink.cpp:100 传出原始累加量后由
  aio_hips_writer finalize_tile 完成，见 DISP-DRZ-007），不在本模块。

## 1 ALG-DRZ-001 核心累加公式（与 SCI-DRZ-001 §5 对照）

- 源锚: `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
  （processPixelSharedTiled/processPixelTiled 累加段 1490-1540）。
- 记号: 源像素 j（值 x_j [ADU]、方差 v_j [ADU²]、权重面）、目标
  HEALPix NESTED leaf p；drop = 源像素按 pixfrac 收缩的球面 footprint；
  A_drop,j = drop 球面总面积 [sr]（S-H 裁剪前，双精度角点累积，
  <1e-20 拒绝，drizzle_engine.cpp:1436-1438）。
- 离散公式（逐条源码锚）:
  - 权重: `w_jp = a_jp / A_drop,j`，a_jp = drop ∩ target p 球面交叠
    面积 [sr]（drizzle_engine.cpp:1505；w≤0 拒绝 :1506-1509）。
  - 通量: `F_p = Σ_j x_j · w_jp`（`acc.sumFlux += Scalar(pixelValue *
    weight)`，:1527）。
  - 支撑面积: `D_p = Σ_j a_jp`（`acc.sumArea += Scalar(overlap_area)`，
    :1528）。
  - 方差分子: `sumVarNum_p = Σ_j v_j · w_jp²`（`(double)v · (double)w²`
    中转再转 Scalar，仅当 v>0 累加，:1531-1534）。
  - 贡献计数: `nContrib_p = Σ_j 1`。
- **S_p = F_p/D_p 归一不在本模块**: sumFlux/sumArea/sumVarNum 原始和
  逐 tile 传出（astro_sphere_sink.cpp:99-104 dense 化），归一在
  aio_hips_writer finalize_tile（variance = var_num_sum/area²，
  lib/astro_image_io/src/hips/aio_hips_writer.cpp:566-631）——与
  drizzle_engine.cpp:2-3 锚注释一致（DISP-DRZ-007 登记旧锚失效）。
- 单位/dtype: 累加器 Scalar = float（precision_mode=0）或 double（=1）
  显式模板双实例（drizzle_engine.cpp:2171-2178）；a_jp/面积几何全程
  double，FP32 仅发生在累加存储层（逐项舍入，容差门见 §9）。

## 2 几何管线（drop footprint → 候选 → 交叠面积）

- drop 四角: `half = 0.5·pixfrac`，`(x±half, y±half)` 经 WCS/SIP
  `pixelToSky` → 单位 Vec3（drizzle_engine.cpp:1303-1308 四角构造；
  pixfrac==1.0 时行级顶点共享缓存，:1663,1686-1710）。
- 收缩语义: 球面 slerp 不做显式插值，四角直接按收缩后平面坐标取
  WCS 映射（pixfrac 收缩在源平面完成）；pixfrac∈(0,1] 严格校验，
  ≤0 或 >1 拒绝不夹逼（:1567-1574；文件通道 API 层接受 0.0 的双轨
  差异见 DISP-DRZ-003）。
- 三层候选缓冲（spherical_overlap.cpp）:
  1. quick-reject: `lim = max_angle + 1.25·hp_res`
     （HP_CIRCUMRADIUS_FACTOR=1.25，:42；极区实测最坏 1.044×hp_res）；
  2. 保守查询圆: `query_radius = max_angle + 3.0·hp_res`
     （queryDisc 回退）；
  3. fast 路径: 面 delta×1.15 面内畸变系数 + 极冠/跨 face 边界回退
     queryDisc（:1664-1699）。
- 交叠面积: 球面 Sutherland–Hodgman 裁剪（clip_normals_d，内部 double）
  + **Eriksson 扇形三角剖分**有向面积 + 半球包含检查
  （max_ang ≥ π/2−1e-12 → NAN，:186-239）。SCI 文本称 "Girard 定理"
  与实际实现命名不符（DISP-DRZ-002）。
- 目标几何缓存: per-thread LRU 8192（TargetGeomCache），hit 复用
  center+boundary4；per-run generation 原子递增清空
  （drizzle_engine.cpp:1659-1660 `s_target_cache_gen.fetch_add`，
  B4-22 修复裸 static data race）。
- HEALPix 地址: 仅 NESTED；`parent = ipix >> 2·d`、
  `local = ipix & (4^d − 1)` 位分解（:1512-1513）；候选枚举 Morton
  spread 位交织（spherical_overlap.cpp:1639-1653）；shim
  healpix_core.h 转发 astrocs::healpix::ang2pix_nest，RING 直接拒绝。

## 3 auto nside 决策（compute_auto_nside）

- 源锚: drizzle_engine.cpp:624-710。
- `HEALPIX_SCALE_PER_NSIDE_ARCSEC = sqrt(π/3)·(180/π)·3600 ≈ 211034.6`
  （:676-677）。
- 决策: 最小 2 次幂 ≥ 211034.6/finest_arcsec；无效输入（finest≤0 等）
  → 0 由调用方兜底（:632-643）；钳位 [16, 2^22]（:681-697）。
- orchestrator 侧策略 `1x_to_2x_drizzle`/`fixed` 最终映射到本函数
  （orchestrator.cpp:3298-3311）。

## 4 反向 drizzle（Sphere→Plane，REV-101..107）

- 源锚: reverse_drizzle.h:26-70、reverse_drizzle.cpp:255-334。
- 语义: 每 source leaf 构造球面 footprint（边界自适应细分），pixfrac
  沿球面向 leaf 中心收缩；目标平面像素经 WCS/SIP 映射为球面
  footprint；重叠面积 = 球面面积（禁止平面 2D 面积作权重）；signal
  按球面面积比例分摊；coverage 以"覆盖在 leaf 内均匀分布"假设输出。
- 严格校验（reverse_drizzle.cpp:255-334）: nside 2 的幂 [1,2^22]；
  仅 NESTED；宽高>0；pixfrac∈(0,1]；CD 行列式有限且 |det|≥1e-30、
  |cd[k]|≤1 deg/px；crval dec∈[-90,90]；f32/f64 signal 二选一严格；
  support∈[0,1]（空=全 1.0）；ipix 越界/重复拒绝。
- 能力/版本: hp_drizzle_reverse_capability 位 0x01|0x02|0x04|0x08|
  0x10|0x20（api.cpp:145-147）；version "1.0.0"（:149-151）。

## 5 输入校验与 NaN/Inf/invalid 边界

| 条件 | 行为 | 锚 |
|---|---|---|
| pixfrac ≤0 或 >1（引擎层） | 拒绝（不夹逼） | drizzle_engine.cpp:1567-1574 |
| pixfrac==0.0（文件通道 API 层） | 放行后进引擎再拒（双轨） | api.cpp:191（DISP-DRZ-003） |
| RING（nested=0） | 硬拒绝 | :1575-1579；shim throw |
| channels≠1 多通道 | 拒绝 | :1580-1588 |
| 缺 WCS（CD 与 CDELT+CROTA2 均无） | 拒绝（帧通道返回 -9） | api.cpp:541-545 |
| 尺寸/空指针非法 | 拒绝 | drizzle_engine.cpp:1594-1603 |
| **值像素 NaN/Inf** | **主循环静默 continue（等效掩膜），不进累加器，无计数暴露** | :1712（DISP-DRZ-004） |
| SNR/权重/variance 面非有限或 ≤0 | 静默跳过该像素 | :1715-1729 |
| 几何 NaN（ra/dec 非有限） | 显式拒绝该像素 | :1319-1320 |
| 半球检查失败（max_ang≥π/2） | 返回 NAN 面积 | spherical_overlap.cpp:196-216 |
| A_drop<1e-20 / w≤0 | 拒绝 | drizzle_engine.cpp:1436-1438,1506-1509 |
| nside 非法（shim 构造） | 容忍不抛，校验责任在调用方（缺陷候选） | healpix_core.h:25-27 |
| reverse 输入非法 | 非 0 返回码，逐项校验 | reverse_drizzle.cpp:255-334 |

## 6 确定性与归约（1/N 合同）

- 并行: `#pragma omp parallel for schedule(static) num_threads(N)
  reduction(+:nSourcePixels,…)` 逐行条带（drizzle_engine.cpp:1670-1671）；
  线程数 = config.threads（>0）否则 omp_get_max_threads（:1639-1644），
  不改全局 omp_set_num_threads；无 _OPENMP 退化串行（tid=0）。
- 累加结构: per-thread `unordered_map<parent, TileAccumulator>` +
  per-thread 计数器（:1645-1646）；行级顶点缓存 thread_local
  （:1686-1688）。
- 合并: 串行按线程序 t=1..N−1 合入 threadTiles[0]，仅合并 touched
  leaf，字段序 sumFlux→sumArea→sumVarNum→nContrib（:1762-1785）。
- **1/N 确定性成立**: 同输入同线程数 bitwise 可复现（schedule(static)
  行→线程映射固定 + 合并序固定）；跨线程数时 leaf 内浮点和顺序不同，
  不保证 bitwise（差异 ≤ 浮点结合律界，测试门 §9 覆盖 1/2/4 线程）。
- ThreadLease: 模块内零命中；omp 为遗留内部通道，生产调度走 Runtime
  lease（CMakeLists.txt:379-382 注释）——ThreadLease 迁移整改点
  （P1-DRZ-IMPL），迁移必须保持本节合并序。

## 7 复杂度与内存

- 时间: O(n_source · avg_candidates)，avg≈3.5（小图实测，9003 例
  oracle 枚举）；fast 路径圆心距预过滤降低 S-H 调用。
- 内存: tile 累加器 leaf 连续数组 O(1) 寻址（禁 per-leaf 全局 map，
  :1522 注释）；target 缓存 per-thread bounded 8192 LRU；单交集
  O(顶点数≤8) 无整帧副本；SNR 控制点 RAII vector（api.cpp:609-613，
  修复 legacy free 泄漏）。

## 8 数据布局

- Tile: `TileAccumulatorT<Scalar>{parent_ipix, touched[], pixels[4^d]}`
  ；`TileLeafAccumulatorT<Scalar>{sumFlux, sumArea, sumVarNum,
  nContrib}`（drizzle_engine.h:60-67；release 注释"3 字段"与实际 4
  字段不符，DISP-DRZ-006）。
- HiPS 直写: tile_depth 必须 =9、nside≥512（astro_sphere_sink.cpp:36-51），
  leaf tile 512×512 1:1，产品 SIGNAL/SUPPORT + V19 variance/ivar
  （有 variance 输入时）；provenance 写 pixfrac/源像素尺度。
- legacy HISS: HissWriter 流式（writeHisTilesT，drizzle_engine.cpp:1262
  finalize），测光 gate :1943-1949；operation_counts.json 剖面
  （api.cpp:1074-1117）。
- 输入通道: PipelineFrame "data"（f32/f64 二选一，bzero=0/bscale=1
  固定，api.cpp:486-503）+ header WCS/SIP KV + 可选 "snr_model" 块
  （KD-tree IDW 重建逐像素 SNR，snr_evaluator.h）。

## 9 TEST-DRZ-DESIGN-001（测试设计冻结，可执行 TEST-P1-DRZ-001 由 P1-DRZ-TEST 建立）

- fixture（全合成、离线、零真实数据）:
  - FIX-DRZ-A 常量场（面亮度语义 oracle：每像素常量 ADU ⇒ S=C/A_drop
    ≠C，检验 D_p 语义）；FIX-DRZ-B 点源高斯（总通量守恒）；
    FIX-DRZ-C 梯度场；FIX-DRZ-D 脉冲单像素；FIX-DRZ-E NaN/Inf 注入面；
    FIX-DRZ-F SIP 畸变边缘 patch（15° 宽场）。
- 冻结容差（预冻结，源自既有测试门，不得放宽）:
  - FP64 通量闭合 <1e-6（主域）；FP32/FP64 逐 leaf <1e-5
    （drizzle_freeze_test.cpp 硬门，:211 附近；l0 小图 FP64<1e-10）；
  - 方差 α² 缩放律逐像素 worst_rel <1e-4（variance_propagation_test，
    SCI-DRZ-014）；
  - 候选零漏选: 9003 例 false_negative=0（candidate_oracle_test，
    4 pixfrac × 5 尺度 × 7 nside 全枚举，RA 跨 0/极区/face 边界）；
  - reverse: false_hole=false_fill=0、coverage∈[0,1]、常数场
    uniform_rel_std<1e-4、半图/全图 total_in 比 <1e-6；
  - 面积误差预算: float 面积 0.05% ≪ 候选保守性（零漏选）≪ 门禁
    容差；arc-chord 1e-6·hp_res（旧 §9/§12 冻结值照抄）。
- 不变量: 常量场均匀性（无接缝/系统性亮斑，drizzle_freeze_test:422）；
  NESTED 地址往返；1/2/4 线程一致性；HISS 写读往返；负值保持；
  multi-FWHM 孔径测光 4σ 相对差 <1e-3。
- 负面矩阵: §5 表逐行断言（pixfrac 0/负/>1、RING、多通道、缺 WCS、
  NaN 面、reverse 二选一/越界/重复 ipix）。

## 10 DISP-DRZ-001..008（SCI/文档 vs 源码差异清单，P1-DRZ-IMPL/INT 消化；修复不得反向改 SCI）

| # | 文档声称 | 源码实际 | 双方锚 |
|---|---|---|---|
| DISP-DRZ-001 | api.h:93 注释 sip_order "0..4" | api.cpp:88-92 校验 [0,5]（6×6 系数组支持 5 阶下标） | hp_drizzle_api.h:93 vs api.cpp:88-92 |
| DISP-DRZ-002 | 面积="S-H + Girard 定理"（DRIZZLE.md:63,:124） | S-H 裁剪 + Eriksson 扇形三角剖分，无 Girard 实现 | DRIZZLE.md:63,124 vs spherical_overlap.cpp:186-239 |
| DISP-DRZ-003 | pixfrac∈(0,1] 单一边界 | 文件通道 API 层接受 0.0（<0 才拒），引擎层拒绝——两层双轨 | api.cpp:191 vs drizzle_engine.cpp:1567 |
| DISP-DRZ-004 | 值像素 NaN 经 F_p 传播、不掩膜（DRIZZLE.md:96） | 主循环 !isfinite→continue 静默跳过（不进累加器），无计数暴露 | DRIZZLE.md:96 vs drizzle_engine.cpp:1712 |
| DISP-DRZ-005 | max_angle<1e-3 rad 切平面近似（DRIZZLE.md:98，证据 spherical_overlap.cpp:75） | 全文无 1e-3 切平面分支，:75 为注释行；现行统一球面 S-H | DRIZZLE.md:98 vs spherical_overlap.cpp:75 |
| DISP-DRZ-006 | TileLeafAccumulatorT release 仅 3 字段（drizzle_engine.h:58-59 注释） | 实际 4 字段（sumVarNum 为正式产品） | drizzle_engine.h:58-59 vs 60-67 |
| DISP-DRZ-007 | SCI §13 方差锚 drizzle_engine.cpp:100/736-762 | 行号漂移：现行方差锚 astro_sphere_sink.cpp:100 + aio_hips_writer finalize_tile | DRIZZLE.md:131 vs drizzle_engine.cpp:2-3 |
| DISP-DRZ-008 | poly_clip.h 自述生产重叠面积用途 | PolyClip（平面 S-H/Shoelace）生产 tiled 路径零调用（legacy） | poly_clip.h:4-15 vs drizzle_engine.cpp 全文 |

无差异项（核对通过）: w=a/A_drop、F/D/sumVarNum 公式、HP_CIRCUMRADIUS
_FACTOR=1.25、三层缓冲语义、NESTED 统一、按线程序合并确定性。

## 11 关联

- SCI: SCI-DRZ-001（docs/science/DRIZZLE.md，FROZEN；§5 公式、§7
  不变量、§15 Acceptance；集合 SCI-DRZ-014 方差传播 / 015 支撑 /
  016 协方差）。
- DATA: DATA-P1-DRZ（docs/contracts/DATA_SEMANTICS.md §11）；上游
  DATA-P1-CAL（§9）；编排现状引用 DATA-P1-STACK（descriptor）。
- API: API-DRZ-001（docs/contracts/PUBLIC_API.md）；API-P1-007
  （docs/api/PHASE1_API_V1.md，区间 API-P1-001..010 编排合同）。
- MOD/SRC: MOD-astrocs-phase1-drizzle（lib/drizzle/module.yaml，
  CONTRACT_READY；lib/drizzle/README.md 实现事实）；SRC-DRZ-001
  （lib/healpix_db/healpix_drizzle/hp_drizzle_api.h 等签名源）。
- TEST: TEST-DRZ-DESIGN-001（本文档 §9）；可执行 TEST-P1-DRZ-001
  由 P1-DRZ-TEST 建立（既有 lib 内 tests/*.cpp 为科学门基线）。
