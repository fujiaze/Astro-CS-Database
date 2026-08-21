# Plate Solve（IPV）

关联：SCI-AST-001；模块：lib/plate_solve/cpp/ipv。

## 输入

星点表 + 参考星表（Gaia）。

## 输出

WCS（CD + SIP）或失败状态。

## Preconditions

≥最小星点数；几何非退化。

## Postconditions

前向投影与星点一致（残差报告）；WCS 头可写。

## Invariants

坐标约定 J2000 + TAN/SIP；内部 0-based `x,y` ↔ FITS 1-based `CRPIX=width/2+0.5`（`x0=CRPIX-1`，`lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:154,276`）；SIP 前向 `A/B` 解析 / 逆向 `AP/BP`（`NB_GRID=7` 网格拟合，`AP[1,0]/BP[0,1]-=1`，`ipv_wcs.cpp:463-464`）与 Y-down 输出 `cd12/cd22` 取反、`A' = A·(-1)^j`、`B' = −B·(-1)^j`（`AP/BP` 同规则，`ipv_wcs.cpp:542-570`）；CD 与 SIP 一致（V18R3 审计）。

## 复杂度

匹配 O(n log n)；退化降级路径显式。

## 并行模型

单线程求解（阶段内串行）。

## 数值风险

极区/大畸变；SIP 高阶振荡 → 阶数上限（`order 2–3`）；极区 `|dec|>45°` 分支、`|dec|>85°` 仍保守（Lipschitz `C=π/2`/`C45=π/(2√2)`，`lib/gaia_xpsd_client/src/gaia_client.c:polar_plane_intersects`），RA 环绕 `cos(dec)` 缩放（`bbox_intersects`），跨界保守不剪枝。数值阈值：`iterative_reproject` 收敛 `0.01"`/上限 `5` 次（`lib/plate_solve/cpp/ipv/src/ipv_solver.cpp:CONV_THRESH_ARCSEC/MAX_ITERS`）、`triangle_match` 尺度容差 `0.002`/匹配容差 `5.0"`、SIP `IRLS 15` 次/`ε 1e-6`/`PIVOT_EPS 1e-12`/`Huber 1.345`/`δ<1e-9 guard`（`lib/plate_solve/cpp/ipv/src/ipv_sip.cpp:238-262`，`1.345·median_abs_r`）。

## fast/reference/oracle

合成投影图恢复；与 astrometry.net 语义对照（工程控制 05 spec）。

## ID

ALG-PLATESOLVE-*；TEST-AST-*。
