# Astrometry Science

## 目的

为每帧建立 WCS（TAN/SIP），使像素→天球坐标可追溯。

## 科学定义

WCS 前向模型：

```text
ra,dec = TAN(CD · (x − x0, y − y0) + SIP 多项式畸变)
```

采用标准 FITS WCS 约定（WCS Paper II），J2000 赤道坐标。内部计算坐标 `x,y` 为 0-based 像素（`x∈[0,width-1]`），FITS 头 `CRPIX1/2` 为 1-based（`CRPIX = width/2+0.5, height/2+0.5`，cf. `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp:154,276`）；两者换算 `1-based xp = x+1`，WCS 线性项为 `CD·(xp−CRPIX) = CD·(x−(CRPIX−1))`，故 0-based 零点 `x0 = CRPIX1−1 = width/2−0.5` 等价于 1-based `x0=CRPIX1=width/2+0.5`。`CD` 单位 deg/pixel（`CD = trans 线性项/3600`），其逆 `cd_inv` 单位 pixel/arcsec 仅用于 SIP 系数换算（`A/B = cd_inv·trans 高阶项`）。

SIP 区分前向与逆向：前向 `A/B`（`order = trans.order`，2–3 阶）由解析公式 `A[i][j]=cd_inv·trans.x_ij` 求得；逆向 `AP/BP` 由 `NB_GRID=7` 网格反变换最小二乘拟合 `UV→(u,v)` 得到，约定 `AP[1,0]-=1, BP[0,1]-=1`（剔除单位线性，`ipv_wcs.cpp:463-464`）。

Y-down 转换：求解器内部为 Y-up（`U.y = -(py−cy)`，`ipv_select.cpp:695,712`），输出为标准 FITS Y-down 时 `cd12, cd22` 取反，SIP 前向 `A' = A·(−1)^j`、`B' = −B·(−1)^j`，逆向 `AP/BP` 同规则（`ipv_wcs.cpp:542-570`）；`CRVAL/CRPIX` 不变，`|det(CD)|` 不变。

## 变量/单位

- x,y：像素（0 基，FITS 头 CRPIX 1-based = width/2+0.5）；CD：deg/pixel；cd_inv：pixel/arcsec（仅 SIP 换算）；SIP `A/B`（前向）/`AP/BP`（逆向）：1/pixel^(i+j−1)；RA/Dec：度。

## 假设

- 视场内单次 gnomonic 投影 + 低阶畸变；星表为 Gaia DR3。

## 有效域

- IPV 求解器覆盖常见天文视场（弧分~度级）；极区有 provably-conservative prune（V18R3，`lib/gaia_xpsd_client/src/gaia_client.c:polar_plane_intersects`）。AE 极冠阈值 `|dec|>45°` 进入极区投影分支，`|dec|>85°` 仍保守：平面 Lipschitz 常数 `C=π/2≈1.5708`（全局 `θ∈[0,90°]`）/`C45=π/(2√2)≈1.1107`（`θ≤45°` 子冠），查询锥在平面盘 `B(q,C·radius)` 与节点矩形不相交则安全拒绝，false_negative=0。

## 不保证

- 不保证畸变高于 SIP 阶数的残差（QA 报告）。

## 失效条件

- 星点不足/几何退化 → PLATESOLVE 显式失败（NO_DATA）。
- 极区近奇点由专用 prune 处理（AST-001 域）；Gaia 锥形查询 RA 环绕按 `dra>180°→360°−dra` 并以 `cos(dec)` 缩放判相交（`gaia_client.c:bbox_intersects`），未命中返回显式失败而非静默空集。
- 极区查询锥跨 `±45°` 边界或 `θ_q+radius>90°` 时保守不剪枝（遍历极冠树）。

## 系统/随机误差

- 系统：星表位置误差、SIP 截断；随机：星点质心误差。

## 数值精度

FP64；CD 矩阵与 SIP 系数写入 FITS WCS 头。

## 参考文献

Calabretta & Greisen (2002)；Gaia Collaboration (2021)；
Shupe et al. (2005) SIP。

## ID

SCI-AST-001（WCS 坐标约定闭合）；ALG-PLATESOLVE-*。
