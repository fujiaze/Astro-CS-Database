# synthetic —— 合成数据生成代码（可复用真值场景）

## 文件

| 文件 | 作用 |
|---|---|
| `synth_gain.py` | P1-SPATIAL-GAIN 的合成帧生成与实验驱动（真值已知 + 能红能绿的负例） |
| `gainlib.py` | 共享库：低阶多项式基、归一化、加权 Tukey-IRLS 曲面拟合、形状/峰峰度量 |

## 合成模型（P1-SPATIAL-GAIN）

    y_k(p) = a_k · m_k(p) · (T(p) + S0) + g_k(p) + n_k(p)

- `T(p)`：平滑星云场（低阶多项式 + 4 个高斯团块）；
- `m_k(p)`：**已知**空间乘法增益（`log10 m` 为低阶多项式，线性为主，pp 5–10%）；
- `g_k(p)`：**已知**加性梯度（≤2 阶，pp 4% S0）；
- `n_k(p)`：高斯白噪声（σ=5 ADU/px）；
- 星点：高斯 PSF（σ=2 px），流量对数均匀 1e3–1e5 ADU；
- 测光：**在真正渲染的像素上**做固定孔径 + 环形局部天光（自动含加性梯度泄漏）。

## 用法

```bash
python3 synth_gain.py            # 150 MC/配置, 约 7 min (16 核)
python3 synth_gain.py --quick    # 40 MC/配置, 约 100 s
```

输出 `../experiments/p1_spatial_gain/data/synth_results.json`。

## 约定（重要，容易搞反）

- 拟合出的 `m` 是**校正因子**，与被测的仪器空间乘法响应 `m_true` 互为倒数（`m ≈ 1/m_true`）；
- `m` 归一化到**参与拟合的星集合**上 `log10 m` 加权均值为 0（几何均值 1），因此与 `k_photo` 不重复；
- `order=0` 时本库严格退化为现有标量估计 `k_photo = 10^(−location)`。
