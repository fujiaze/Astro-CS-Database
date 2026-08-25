# WCS / PlateSolve Algorithms (ALG-WCS)

> 上游 SCI: SCI-WCS-001  状态: DERIVED (T202 冻结, 2026-08-23)  模块: plate_solve/cpp/ipv

## 1 上游 SCI 与输入输出

- 上游: `SCI-WCS-001` (CRPIX=w/2+0.5, CD deg/pixel, cd_inv pixel/arcsec, SIP A/B 解析 / AP/BP 7×7网格, Y-down)
- 输入: 星点表 `(x,y)` + Gaia 参考星表 (RA/Dec)
- 输出: `WCS` (CD+CRPIX/CRVAL+SIP A/B/AP/BP) 或 `NO_SOLUTION`

## 2 离散公式

```text
F1: CRPIX = w/2+0.5, h/2+0.5 (1-based), 0-based x0=CRPIX−1
F2: CD = trans.linear/3600 (deg/pixel), cd_inv = inv(trans.linear) (pixel/arcsec)
F3: SIP前向 A[i][j]=cd_inv·trans.x_ij, B[i][j]=cd_inv·trans.y_ij (解析)
F4: SIP逆向 AP/BP = argmin ||UV−(u,v)−SIP(u,v)||² on 7×7 grid, AP[1,0]-=1, BP[0,1]-=1
F5: Y-down: cd12,cd22 取反; A'=A·(-1)^j, B'=−B·(-1)^j, AP/BP 同规则
F6: 投影 TAN + SIP畸变 + J2000, 极区 Lipschitz C=π/2 / C45=π/(2√2) conservative prune
```

来源: `ipv_wcs.cpp:153-576` `ipv_select.cpp:695` `gaia_client.c:polar_plane_intersects`

## 3 伪代码

```text
function solve_wcs(detections, gaia):
  if n_detections < min_stars → NO_SOLUTION
  triangles = build_triangles(detections) scale_tol=0.002
  matches = kd_match(triangles, gaia_triangles) tol=5.0"
  for each hypothesis:
    trans = iterative_reproject(matches) conv 0.01" max5 (ipv_solver.cpp)
    sip = build_sip(trans) order 2-3, IRLS 15× ε1e-6 Huber 1.345 (ipv_sip.cpp:238-262)
    wcs = compose(CRPIX,CRVAL,CD, sip, Y-down)
    rms = residual(wcs, matches)
  best = min rms, rank by n_matches + rms
  if rms > threshold → NO_SOLUTION else return wcs

function build_sip(trans):
  cd_inv = inv(trans.linear)
  for (i,j) in order: A[i][j]=cd_inv·trans.x_ij, B[i][j]=cd_inv·trans.y_ij
  grid = 7×7 UV = cd_inv·IWC, fit AP/BP via least squares
  AP[1,0]-=1; BP[0,1]-=1
  apply Y-down sign flips

Polar prune: if |dec|>45° use C/C45 disk B(q,C·radius), false_negative=0
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `n < min_stars` | `NO_SOLUTION` |
| `det(trans.linear)==0` | reject SIP, `NO_SOLUTION` |
| `NB_GRID` 奇异 | fallback linear |
| 极区跨界 `θ+radius>90°` | 保守不剪枝遍历 |
| RA环绕 `dra>180°` | `dra=360−dra` + cos(dec) 缩放 |
| 输入含 NaN | skip/fail per-field |

## 5 确定性与归约

- 单线程求解，三角形匹配 KD-tree 确定性（排序 tie-break by frame_id）；SIP LS 按 grid 索引固定顺序；无跨假设归约。

## 6 时间/空间复杂度

- 匹配 O(n log n)；SIP O(order²·49) LS；空间 O(n_triangles)

## 7 CPU/GPU 划分

- CPU 单线程；GPU 仅 Gaia 查询加速（若有），WCS 求解仍 CPU，容差与确定性门 `≤1e-6 deg` 等价。

## 8 参考实现/Oracle

- 合成投影图已知 WCS 恢复残差 `<0.1"`；astrometry.net 语义对照 (05 spec)；Astropy WCS 前向/逆向 `Δ<1e-4 px`。

## 9 容差来源

- 收敛 0.01" (pixel/3600), 尺度容差 0.002 (各向异性 0.2%), Huber 1.345 (robust 统计), 预冻结。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` 单线程阶段内串行
- API: `ipv_api.h: ipv_solve_from_detections_v1`, `ipv_wcs.h: build_wcs`
- TST: `TST-WCS-001` 合成恢复, `TST-WCS-INV` 极区保守, `TST-WCS-FAIL` 退化
