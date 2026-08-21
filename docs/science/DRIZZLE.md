# Drizzle Science

## 目的

将抖动帧重投影到公共天球网格，保持通量守恒并传播方差。

## 科学定义

Fruchter & Hook 线性重建：

```text
源像素 j → 目标像素 p:
  w_jp = a_jp / A_drop,j
  F_p  = Σ_j x_j w_jp
  D_p  = Σ_j a_jp
  S_p  = F_p / D_p
```

a_jp：源像素在目标像素 p 内的面积；A_drop：drop size 内面积。

## 方差传播（SCI-DRZ-014 / ALG-DRZ-VAR）

```text
sumVarNum += v_j × w_jp²
variance_p = sumVarNum / D_p²
ivar_p     = 1 / variance_p
```

缩放律：x'=αx → var'=α²var，ivar'=ivar/α²（SNR-002）。

> sumVarNum 为 `TileLeafAccumulator.sumVarNum` 分子中间量，归一在 sink/writer finalize（`astro_sphere_sink.cpp:100` / `aio_hips_writer::finalize_tile`）。

## 变量/单位

- S：信号（ADU/e⁻）；D：drop 覆盖面积（px²）；v：方差（信号²）；
- 坐标：HEALPix NESTED（target_order + leaf 9 阶）。

## 假设

- 线性叠加；源像素独立；几何（WCS）已解。

## 有效域

- target_order 由 MOC union 决定；极区无奇点（NESTED 球面）。

## 不保证

- 不保证相邻输出像素独立（协方差文档化，不存完整矩阵）。

## 失效条件

- 几何退化/无覆盖 → 显式 NO_DATA；candidate 保守测试 false negative=0
  （ALG-DRZ-CAND oracle）。

> 缓冲分层：overlap quick-reject 1.25×hp_res / candidate 圆盘 3.0× / fast 1.25×+1.15 畸变+极冠回退（见 `spherical_overlap.cpp:40` 与 A2-03 证据）。

## 数值精度

FP64 累积；candidate/overlap/geometry 计数作为 METRIC（P1-DRZ-*）。

## 参考文献

Fruchter & Hook (2002)；Górski et al. (2005) HEALPix。

## ID

SCI-DRZ-001, 014, 015, 016；ALG-DRZ-CAND-* / ALG-DRZ-OVERLAP-* /
ALG-DRZ-VAR-*。
