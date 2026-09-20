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

## M16-SCENE 新增（RELEASE-02 分片 M16-SCENE）

| 文件 | 作用 |
|---|---|
| `m16_mask.py` | **有效域掩膜生成器**：HST M16 三帧（WFC3/UVIS 窄带 drz）逐帧生成 `valid`/`flags`/`meta`，落 `run/reverse_verify/m16_scene/masks/`（**不入库**）。判据写死在 `MaskConfig`（NONFINITE / ZERO / NEGATIVE / EDGE / EXTREME_NEG / SPIKE），带 `--selftest` 红绿例 |
| `m16_scene.py` | **前向渲染接口**（`renderer="m16_scene"`）：真实 M16 结构 → 期望率面；**PHOTFLAM 一手标定** → AB/ST 星等尺度；掩膜传播；噪声一律调用 `noise_model.expose()`。输出 SCI + MASK + TRUTHE 三个 HDU |
| `scenes/m16_*.json` | 4 个 M16 场景配方：亮星云核心（F657N）/ 一般星场（F502N）/ 极暗低 SNR（F673N）/ 三波段同天区 |

对接方式：`data/synthetic/generate.py` 按场景配方的 `renderer` 键派发（`"render"` → `render.py`；`"m16_scene"` → `m16_scene.py`）。
索引与统计：`reverse_verify/data/real/m16_scene_index.json`；报告：`reports/RELEASE-02/m16-scene.md`。

**诚实边界（drz 合成品）**：三帧 `NDRIZIM=32` 的 drizzle 合成品，噪声已被压低并**相关化**，
**不是**独立泊松样本 ⇒ 只能当**结构模板**；本接口生成的是**逐像素独立**噪声，
**噪声功率谱与真实 drz 不同** ⇒ 对相关长度敏感的判据（稀疏 SNR 间距、UPM 平滑尺度）不得用合成帧定标。
