# Drizzle Geometry Algorithms (ALG-DRIZZLE)

> 上游 SCI: SCI-DRZ-001..016  状态: DERIVED (T205 冻结, 2026-08-23)  模块: healpix_drizzle

## 1 上游 SCI 与输入输出

- 上游: `SCI-DRZ-001` (候选保守) `SCI-DRZ-014` (方差传播) `SCI-DRZ-015/016` (支撑/协方差)
- 输入: 输入帧 (像素+WCS) + 目标 HEALPix 网格 (nside) + pixfrac
- 输出: 目标像素候选集 + 重叠面积权重 w=a/A_drop + 操作计数 DrizzleStats

## 2 离散公式

```text
F1: drop corners: (x±0.5·pixfrac, y±0.5·pixfrac) → pixelToSky → Vec3
F2: center=normalize(Σ corners), max_angle=max ∠(center,c_i)
F3: 三层缓冲: quick-reject 1.25·hp_res, candidate 3.0·hp_res, fast 1.25·hp_res +1.15畸变
F4: Sutherland-Hodgman 裁剪: drop ∩ target pixel, Girard Area=Σ角−(n−2)π
F5: w = overlap / drop_area, drop_area double角点源 (防float 0.05%偏差)
F6: 微小交集 max_angle<1e-3 rad 切平面近似保持 w一致性
F7: TargetGeomCache LRU 8192: hit复用 center+boundary4, miss构建计数, run generation递增clear
```

来源: `spherical_overlap.cpp:40,573,773-931` `drizzle_engine.cpp:100`

## 3 伪代码

```text
function build_drop_corners(x,y,pixfrac,wcs): 4 corners → Vec3[]
function candidate_radius(drop): max_angle+3.0·hp_res (保守查询圆)
function overlap_area(drop, target_pixel): S-H裁剪 + Girard, 切平面fallback if <1e-3 rad

function drizzle_pixel(drop, wcs, target_nside):
  radius = candidate_radius(drop); candidates = queryDisc(center, radius) + boundary_fallback
  for each cand:
    if quick_reject(max_angle+1.25·hp_res) skip
    overlap = overlap_area(drop, target_pixel) // or cached boundary4
    w = overlap / drop_area; D += overlap; F += x_j·w
  S = F/D; variance = sumVarNum/D²

function TargetGeomCache.get(target_ipix):
  if hit: return cached {center,boundary4}
  else: build pix2radec+boundary4, cache, inc target_geometry_builds
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| pixfrac 非法 | 拒绝 |
| WCS 不可逆 | skip pixel |
| 极区/RA跨0/face边界 | boundary_fallback保守 |
| max_angle<1e-3 | 切平面近似 |
| target cache 满 8192 | LRU 淘汰 |

## 5 确定性与归约

- 每源像素独立，无跨像素归约；候选集排序 by ipix 固定；overlap面积 Girard 确定性；cache命中与未命中数值路径同一 overlap_area_impl。

## 6 复杂度

- O(n_source·avg_candidates), avg≈3.5 (小图实测); cache hit 91.7% 降 geometry_builds.

## 7 CPU/GPU

- CPU OpenMP 按源帧tile; GPU 按candidate切分, 候选查询与S-H裁剪kernel化, false_negative=0 等价门。

## 8 参考实现/Oracle

- candidate oracle 9003例 false_negative=0; overlap逐像素面积核对; cache等价逐位验证; 证据 evidence/drizzle/*.json

## 9 容差来源

- arc-chord 1e-6·hp_res, area float 0.05% 双精度修正, 预冻结。

## 10 关联 ARC/API/TST

- API: spherical_overlap.h: compute_overlap_area_g_ctx, drizzle_engine.h: drizzleTiled
- TST: TEST-ALG-DRZ-* candidate/overlap/cache, UPMW-005 MC

## 11 数据布局

- 输入：源帧像素（+WCS）→ 四角 `Vec3` 单位向量（每顶点 16B，就地；`half=0.5·pixfrac` 收缩）。
- 目标：NESTED leaf 4 角（nside≥256 用 `boundary4`，低 nside 自适应细分）→ 球面多边形 `vector<Vec3>`。
- 缓冲三层（LRU geometry cache，容量 8192 target-ipix）：① 快速拒绝 `lim=max_angle+1.25·hp_res`；
  ② 保守查询圆 `query_radius=max_angle+3.0·hp_res`；③ fast buffer `1.25·hp_res`。
- 每 (drop, target) 交集：`a_jp` 面积、`w_jp=a_jp/A_drop,j` 标量；`TargetGeomCache::get_or_build`、
  `run_target_cache` 显式缓存与清理。
- 内存：单交集 O(顶点数≤8)，无整帧副本；缓存按 run generation 清空（`spherical_overlap.cpp:40,573,773-931`）。

## 12 误差预算

- 面积精度：球面 Sutherland–Hodgman + Girard，双精度修正，float 面积 **0.05%**（§9 冻结）；
- arc-chord：`1e-6·hp_res` 近似（§9 冻结）；
- 微小交集 `max_angle<1e-3 rad`：切平面近似保持 `w=overlap/drop_area` 一致（`spherical_overlap.cpp:75`）；
- 候选保守性：查询半径 `3.0·hp_res` 对 `1.25·hp_res` 缓冲 → 零漏选（`candidate_oracle_test` 9003 例
  `false_negative=0`；RA 跨 0 / 极区 / face 边界 / 4 pixfrac × 5 尺度 × 7 nside 全枚举）；
- 缓存：LRU 8192 定点优化，运行期清空，科学等价 + 操作计数（`ALG-DRZ-GEOM-CACHE-001`）；
- 排序：**几何面积误差(0.05%) ≪ 候选查询保守性(零漏选) ≪ 门禁容差**。各 F 步骤映射：
  `F1`→`compute_overlap_area_g_ctx_cached`（`TEST-ALG-DRZ-*`）；`F6`→`spherical_overlap.cpp:75`。
