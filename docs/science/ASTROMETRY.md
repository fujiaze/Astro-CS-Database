# Astrometry Science

## 目的

为每帧建立 WCS（TAN/SIP），使像素→天球坐标可追溯。

## 科学定义

WCS 前向模型：

```text
ra,dec = TAN(CD · (x − x0, y − y0) + SIP 多项式畸变)
```

采用标准 FITS WCS 约定（WCS Paper II），J2000 赤道坐标。

## 变量/单位

- x,y：像素（0 基）；CD：deg/pixel；SIP：多项式系数；RA/Dec：度。

## 假设

- 视场内单次 gnomonic 投影 + 低阶畸变；星表为 Gaia DR3。

## 有效域

- IPV 求解器覆盖常见天文视场（弧分~度级）；极区有 provably-conservative
  prune（V18R3）。

## 不保证

- 不保证畸变高于 SIP 阶数的残差（QA 报告）。

## 失效条件

- 星点不足/几何退化 → PLATESOLVE 显式失败（NO_DATA）。
- 极区近奇点由专用 prune 处理（AST-001 域）。

## 系统/随机误差

- 系统：星表位置误差、SIP 截断；随机：星点质心误差。

## 数值精度

FP64；CD 矩阵与 SIP 系数写入 FITS WCS 头。

## 参考文献

Calabretta & Greisen (2002)；Gaia Collaboration (2021)；
Shupe et al. (2005) SIP。

## ID

SCI-AST-001（WCS 坐标约定闭合）；ALG-PLATESOLVE-*。
