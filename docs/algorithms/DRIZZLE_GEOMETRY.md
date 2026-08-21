# Drizzle Geometry

关联：SCI-DRZ-001..016；V19R3 DRIZZLE_TARGETED（bounded target-ipix
geometry cache + operation counters）；模块：lib/healpix_db/healpix_drizzle
（spherical_overlap.cpp / drizzle_engine.cpp）。

## 输入

输入帧（像素 + WCS）+ 目标 HEALPix 网格 + pixfrac/drop。

## 输出

目标像素候选集 + 重叠面积权重。

## 球面几何推导

源像素 drop 为像素四角经 WCS/SIP 映射到单位球后的球面多边形：

```text
pixelToSky((x±0.5·pixfrac, y±0.5·pixfrac)) → ra/dec → Vec3 单位向量
```

目标 HEALPix leaf 边界：NESTED 像素 4 角（赤道带菱形 / 极区三角形
退化），nside>=256 生产路径固定 4 角（get_healpix_boundary4，与
get_healpix_boundary 逐位等价）；低 NSIDE 用自适应细分边界
（get_healpix_boundary_sampled，赤道带 4 边采样，极区保持大圆弧）。

重叠面积：球面 Sutherland–Hodgman 裁剪 drop 多边形于像素 4 边大圆
（法向量指向像素内部），交集面积用 Girard 定理
（Area = Σ内角 − (n−2)π）；微小交集（drop max_angle<1e-3 rad）用切平面
面积保持 weight=overlap/drop_area 一致性。drop 面积也由 double 精度角点
源计算，避免 float 存储舍入的 ~0.05% 面积偏差。

## 候选包围圆构造（零漏选）

```text
drop 包围圆：center = normalize(Σ corners)，max_angle = max(∠(center, c_i))
三层缓冲（与 lib/healpix_db/healpix_drizzle/spherical_overlap.cpp:40 一致，
HP_CIRCUMRADIUS_FACTOR=1.25×hp_res 为像素外接半径保守上界，覆盖赤道
1.532×res 对角线与极区三角形最坏情况 1.044×res + 裕量）：
  1) overlap quick-reject：lim = max_angle + 1.25×hp_res
  2) candidate 保守查询圆：query_radius = max_angle + 3.0×hp_res
  3) fast 快速枚举：buffer = 1.25×hp_res；赤道带 delta×1.15 畸变系数，
     极冠/盒触极冠回退保守 candidate（3.0×）路径
```

RA 跨 0 / 南北极 / face 边界的候选查询走 boundary_fallback（保守
queryDisc 语义）；false_negative=0 由 candidate_oracle_test 9003 例
（12 face × 边/角 + RA跨0 + 极区 × pixfrac{0.1,0.25,0.5,1.0} ×
尺度{0.1",1",10",60",3600"} × nside{16..4194304}）全枚举对照保证。

## V19R3 bounded target-ipix geometry cache（DRIZZLE_TARGETED 优先级 1）

一次 run 内同一个 target leaf 被大量 drop 候选重复访问。缓存
{center, boundary4}（TargetGeomCache，LRU，默认容量 8192，线程私有）：

```text
命中   → 复用 center + boundary4，跳过 pix2radec/radec_to_vec/boundary4
未命中 → 构建并缓存（target_boundary_builds++ / target_geometry_builds++）
run 结束 → 下个 drizzleTiled 的 run generation 递增，线程 cache clear
```

科学等价性：compute_overlap_area_g_ctx_cached 与 _g_ctx 共用同一
overlap_area_impl 数值路径，仅 geometry 来源不同；quick-reject 保持在
边界构建之前（保序，避免穷举 oracle 退化）。验证：UPMW-005 MC
k_corr=1.3883 不变、freeze 42/42、candidate oracle 9003/0。

## 操作计数模型（DrizzleStats + [ops] 行）

```text
source_pixels / candidates / true_overlaps / quick_rejects
pix2radec / boundary_builds / geometry_builds
target_boundary_builds / target_geometry_builds
geometry_cache_hits / geometry_cache_misses
sh_calls / tile_lookups / heap_allocations
cand_eff = true_overlaps/candidates；sh_frac = sh/(sh+quick_rejects)
```

小图实测（20×20，nside=512，pixfrac=0.8）：src=400 cand=3463
true_ov=1221（cand_eff=0.353）quick_rej=2242，tgt_b=288
（gcache_hit=3175/3463≈91.7%）。生产单帧优化前后对比以
evidence/drizzle/operation_counts_{before,after}.json 记录。

## Preconditions

WCS 可逆；目标 order 有效。

## Postconditions

候选保守（false negative=0）；面积守恒 Σw 精确。

## Invariants

球面 S-H 重叠 vs 平面近似误差受控；边界/极区无漏。

## 复杂度

候选 O(pixels × 投影)；geometry cache 复用。

## 并行模型

OpenMP 按源帧 tile；只读 cache。

## 数值风险

极区奇点；wcs 数值发散 → 保守 reject。

## fast/reference/oracle

candidate oracle（全枚举对照，false negative=0）；overlap oracle
（逐像素面积核对）——evidence/drizzle/*.json。

reference（无 cache 路径）= 同一 overlap_area_impl 数值路径；fast =
geometry cache 复用；两者必须逐位等价（V19R3 冻结）。

## ID

ALG-DRZ-CAND-001；ALG-DRZ-OVERLAP-001..；ALG-DRZ-GEOM-CACHE-001；
TEST-ALG-DRZ-*；UPMW-005（control estimator 方差 MC 复用 Drizzle 引擎）。
