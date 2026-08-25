# Drizzle / Spherical Resampling Science (SCI-DRIZZLE)

> ID: SCI-DRZ-001,014,015,016  状态: FROZEN (T105 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-DRZ-001..  模块: healpix_drizzle

## 1 目的与非目标

- **目的**：将多帧抖动观测经球面 Drizzle 线性重建到公共 HEALPix 网格，保持通量守恒并传播方差/协方差语义（不存完整矩阵），为 HiPS 信号/权重/支撑度提供重采样基础。
- **非目标**：不提供完整协方差矩阵产品（仅方差传播，协方差文档化）；不处理超越 Fruchter & Hook 线性模型的非线性探测器效应。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `x_j` | 源像素 j 信号 (ADU) | `drizzle_engine` |
| `v_j` | 源像素方差 (ADU²) | `variance_propagation` |
| `a_jp` | 源像素 j 在目标 p 内面积 | `spherical_overlap` |
| `A_drop,j` | drop 内总面积 | 同上 |
| `w_jp` | `a_jp/A_drop,j` 归一权重 | 重建式 |
| `F_p, D_p, S_p` | 累积通量/覆盖/归一信号 | 同上 |
| `sumVarNum` | `Σ v_j·w_jp²` 方差分子 | `TileLeafAccumulator` |
| `pixfrac` | drop 收缩因子 (0,1] | `drizzle_engine:half=0.5*pixfrac` |
| `hp_res` | HEALPix 像素尺度 `√(π/3)/nside rad` | `spherical_overlap.cpp:40` |
| `C=π/2, C45=π/(2√2)` | 极区 Lipschitz 常数 | 极区 prune |

## 3 物理量和单位

- `S,F,x`: ADU/e⁻；`D,a,A_drop`: px²（球面立体角等价）；`v,variance`: ADU²；`ivar`: ADU⁻²；`w`: 无量纲；`hp_res, max_angle`: rad；`pixfrac`: 无量纲；`nside, order`: 无量纲（`nside=2^order`）。

## 4 输入有效域

- `w>0,h>0`, WCS 有效，`0<pixfrac<=1` 否则 `NO_DATA`（`drizzle:拒绝非法pixfrac`），NESTED 唯一 ordering，`nside=2^order` 为 2 的幂。
- `target_order` 由 MOC union 决定；`source` 像素四角经 `pixelToSky` 映射到球面多边形（`half=0.5·pixfrac` 收缩）。
- 极区 `|dec|>45°` 走保守 prune，`θ_q+radius>90°` 或跨边界走 `boundary_fallback`。

## 5 连续定义

```text
Fruchter & Hook 线性重建 (SCI-DRZ-001):
  源像素 j → 目标像素 p:
    w_jp = a_jp / A_drop,j
    F_p  = Σ_j x_j · w_jp
    D_p  = Σ_j a_jp
    S_p  = F_p / D_p                # 最终信号为 F/D

方差传播 (SCI-DRZ-014):
    sumVarNum += v_j · w_jp²
    variance_p = sumVarNum / D_p²
    ivar_p     = 1 / variance_p
  缩放律: x' = α·x ⇒ var' = α²·var, ivar' = ivar/α² (SNR-002)
  sumVarNum 为 TileLeafAccumulator 中间分子，归一在 sink/writer finalize

球面几何 (ALG-DRZ-GEOM):
  drop 多边形: pixelToSky((x±0.5·pixfrac, y±0.5·pixfrac)) → Vec3 单位向量
  目标边界: NESTED leaf 4 角（赤道菱形/极区退化），nside≥256 用 boundary4，
            低 nside 自适应细分；面积经 Sutherland–Hodgman + Girard 定理

缓冲三层 (spherical_overlap.cpp:40, HP_CIRCUMRADIUS_FACTOR=1.25·hp_res):
  1) overlap quick-reject:  lim = max_angle + 1.25·hp_res
  2) candidate 保守查询圆: query_radius = max_angle + 3.0·hp_res
  3) fast 枚举: buffer = 1.25·hp_res, 赤道 delta×1.15 畸变系数, 极冠回退
```

与 `lib/healpix_db/healpix_drizzle/spherical_overlap.cpp:40,573,773-931` 及 `drizzle_engine.cpp:100,736-762` 一致。

## 6 假设

- 线性叠加且源像素噪声独立；几何 WCS 已解；球面裁剪外接圆半径 `1.25·hp_res` 覆盖赤道对角线 `1.532·res` 与极区最坏 `1.044·res` + 裕量。

## 7 独立不变量

- **通量守恒**：`Σ_p F_p` 在覆盖完全区等于 `Σ_j x_j·(a_jp/A_drop)` 的全域和（重建线性性）。
- **常量场不变量**：常数输入 `x_j=C, v_j=V` 时 `S_p=C`，`variance_p=V·(Σ w_jp²/D_p²)` 量纲一致，无空间调制偏差。
- **零漏选不变量**：`candidate_oracle_test` 9003 例全枚举下 `false_negative=0`（12 face×边/角×RA跨0×极区×4 pixfrac×5 尺度×7 nside）。
- **缩放律**：`x→αx` 时 `var→α²var`，`ivar→ivar/α²` 精确成立（`variance_propagation_test`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `pixfrac` 非法 `<=0/>1` | 拒绝 `NO_DATA` | `drizzle:拒绝非法pixfrac` |
| RING ordering | 拒绝（HISS 统一 NESTED） | `drizzle:拒绝RING` |
| 多通道图像 | 拒绝 | `drizzle:拒绝多通道` |
| WCS 无效/尺度非法 | 拒绝 `compute_auto_nside` 失败 | `drizzle_engine.cpp:631` |
| 无覆盖/几何退化 | `NO_DATA`，不产伪信号 | `drizzleTiled` |
| 微小交集 `max_angle<1e-3 rad` | 切平面面积近似保持 `w=overlap/drop_area` 一致 | `spherical_overlap.cpp:75` |
| RA 跨0/极区/face边界 | `boundary_fallback` 保守 queryDisc，`false_negative=0` | `spherical_overlap` |
| 极区 `θ_q+radius>90°` | 保守不剪枝，遍历极冠树 | `gaia_client.c` 复用 |

## 9 精度策略

- FP64 累积 `F_p/sumVarNum/D_p`（`TileAccumulator sumFlux/sumArea/sumVarNum/nContrib` 线程局部后串行合并）；面积用 `double` 角点计算防 `float` 0.05% 偏差；`arc-chord <1e-6·hp_res ≈3e-6"`。

## 10 不可接受变化

- 改变 `pixfrac` 语义或 `HP_CIRCUMRADIUS_FACTOR=1.25` 保守半径而不重跑 `9003` 例零漏选；
- 将 `S_p` 定义为 `F_p`（漏 `D_p` 归一）；
- 将方差传播写为 `var_p=Σ v_j·w_jp`（漏 `²` 与 `/D²`）；
- 在微小区用 `float` 面积致 0.05% 偏差。

## 11 验证 Oracle

- **零漏选门**：`candidate_oracle_test` 9003 例 `false_negative=0`（`TEST-DRZ-CAND-001`）。
- **方差缩放律**：`α` 缩放输入的 `variance_propagation_test` 逐像素 `variance_p` 按 `α²` 精确（`TEST-DRZ-VAR-001`）。
- **流量守恒**：常数场 `C` 的 `S_p=C` 全像素 `max_abs==0`。
- **Python 参考**：`healpy` 球面多边形面积对同 `drop` 的 `a_jp` 复算（`rtol 1e-9`）。
- **几何缓存等价**：`TargetGeomCache` 命中/未命中结果 `max_abs==0`（`DRIZZLE_TARGETED`）。

## 12 关联 ALG ID

- `ALG-DRZ-CAND` 候选零漏选与包围圆三层缓冲
- `ALG-DRZ-OVERLAP` Sutherland–Hodgman + Girard 球面裁剪
- `ALG-DRZ-VAR` 方差传播 `sumVarNum/D²`
- `ALG-DRZ-GEOM-CACHE` LRU 8192 bounded target-ipix cache

## 13 追溯与测试

- 权威文件: `docs/science/DRIZZLE.md` (SCI-DRZ-001,014,015,016)
- 实现: `lib/healpix_db/healpix_drizzle/spherical_overlap.cpp` (40,573,773-931), `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` (100,736-762, sink finalize), `lib/astro_image_io/src/hips/aio_hips_writer.cpp` (finalize_tile)
- 公开 API: `processPixelSharedTiled, compute_overlap_area_g_ctx, drizzleTiled, compute_auto_nside`
- 测试: `TEST-DRZ-CAND-001` 9003例零漏选、`TEST-DRZ-VAR-001` 缩放律、常数场、`TargetGeomCache` 等价（`candidate_oracle_test.cpp, variance_propagation_test.cpp`）
