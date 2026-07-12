# 鲁棒拟合模块实现文档 (Gradient Estimator)

## 1. 模块概述

鲁棒拟合模块（`gradient_estimator`）完成天文图像光度定标的核心环节：利用 Gaia 参考星的合成流量 F_syn 与图像实测仪器流量 F_instr 的比值，拟合空间梯度曲面，校正图像的乘性渐晕和加性背景梯度，使输出图像与 Gaia 星表系统一致。

## 2. 目录结构

```
lib/photometric_calib/gradient_estimator/
├── python/
│   ├── estimator.py          # 主入口 GradientEstimator
│   ├── star_matcher.py       # 星-图匹配 + MAD 离群清洗
│   ├── gradient_fitter.py    # IRLS + Tukey biweight 稳健拟合 + LOOCV 选阶
│   ├── image_corrector.py    # 图像校正 + 通量归一化
│   ├── wcs_transform.py      # WCS 像素<->天球坐标转换
│   ├── run_estimator.py      # CLI 入口
│   └── test_synthetic.py     # 合成数据自测
└── memory.md
```

## 3. 算法流程

`GradientEstimator.calibrate(image, gaia_stars, wcs_transform, psf_results=None)` 执行以下 10 步：

```
原图 + Gaia 参考星 + WCS
        │
        ▼
[1] PSF 拟合 (psf_results=None 时内部调用 StarDetector + DynamicPSF)
        │
        ▼
[2] 星-图匹配 (StarMatcher.match_and_clean)
    Gaia 星 (ra,dec) --WCS--> 像素坐标 --> 近邻匹配 PSF 星
    + MAD 离群清洗 (outlier_sigma 阈值)
        │
        ▼
[3] 匹配星数 < 6 ?  --> 退化为恒等曲面 (不拟合)
        │
        ▼
[4] 提取数组: x, y, F_instr, F_syn, B_local
        │
        ▼
[5] 乘性输入: r = log10(F_instr / F_syn) = log10(M_true)
        │
        ▼
[6] 乘性梯度拟合 (GradientFitter.fit_multiplicative)
    IRLS + Tukey biweight 鲁棒回归 + LOOCV 自动选阶
        │
        ▼
[7] 加性梯度拟合 (GradientFitter.fit_additive)
    对 PSF 背景值 B 拟合空间曲面
        │
        ▼
[8] 图像校正 + 通量归一化 (ImageCorrector.correct_and_normalize)
    I_cal = (I - S_map) / M_map * scale
        │
        ▼
[9] 质量报告 (n_matched, LOOCV, 残差统计, scale_factor)
        │
        ▼
[10] 残差 CSV 保存 (诊断用)
```

## 4. 鲁棒拟合核心: IRLS + Tukey biweight

### 4.1 为什么需要鲁棒拟合

匹配的参考星中可能存在：
- Gaia 光谱数据异常（BP/RP 光谱缺失或噪声大）-> F_syn 错误
- PSF 拟合失败（双星、星系误判）-> F_instr 错误
- 匹配错误（邻近星误匹配）

普通最小二乘 (OLS) 对离群点敏感，少量异常值即可严重偏移拟合结果。本模块采用 **IRLS (迭代重加权最小二乘) + Tukey biweight 权函数**，对离群点逐步降权至 0，实现鲁棒回归。

### 4.2 IRLS 迭代流程

```
初始化: weights = 1 (OLS)
       coeffs = solve(X^T X, X^T r)

重复 (最多 50 次):
  1. 计算残差: residuals = r - X @ coeffs
  2. 估计尺度: med = median(residuals)
                mad = median(|residuals - med|)
                sigma = mad / 0.6745  (MAD 标准化)
  3. Tukey biweight 权重:
                c = 4.685 * sigma
                u = residuals / c
                w = (1 - u²)²  if |u| < 1
                w = 0           if |u| >= 1   (离群点完全排除)
  4. 加权最小二乘: coeffs = solve(X^T W X, X^T W r)
  5. 收敛判断: max(|coeffs_new - coeffs_old|) < 1e-8
```

### 4.3 Tukey biweight 特性

- **红降权重**：残差越大权重越低，平滑过渡
- **完全拒绝**：|u| ≥ 1 时权重归零，极端离群点完全排除
- **崩溃点 50%**：可容忍高达 50% 的离群点而不崩溃
- **常数 c=4.685**：在正态分布下达到 95% 渐近效率

### 4.4 设计矩阵

坐标归一化到 [-1, 1]：`x' = 2x/W - 1`, `y' = 2y/H - 1`

2D 多项式基（单项式项）：
```
order=1: [1, x, y]                              (3 项)
order=2: [1, x, y, x², xy, y²]                  (6 项)
order=P: {(x^j)(y^k) : j+k ≤ P}                 ((P+1)(P+2)/2 项)
```

## 5. LOOCV 自动选阶

### 5.1 防过拟合约束

- 最低样本要求：`n ≥ (P+1)(P+2)/2 * 3`（3 倍参数数）
- 阶数范围：P ∈ [1, 5]

### 5.2 帽子矩阵 LOOCV

留一交叉验证（Leave-One-Out CV）不需要逐次重新拟合，利用帽子矩阵对角元素一次计算：

```
h_ii = W_i * Σ_j X_ij * (X^T W X)^{-1}_jk * X_ik
LOO_residual_i = residual_i / (1 - h_ii)
CV = mean(W_i * LOO_residual_i²)
```

### 5.3 选阶规则

对每个可用阶数 P 计算 CV(P)，选择满足以下条件的最小阶数：
```
CV(P*) ≤ min(CV) * (1 + 10%)
```

即选择在最优 CV 的 10% 容差范围内的最简模型（Occam's razor 原则，避免过拟合）。

## 6. 乘性/加性梯度分离

### 6.1 物理模型

图像观测值分解：
```
I_obs = M_true(x,y) * I_star + S_true(x,y)
```
- `M_true`：乘性渐晕因子（光学渐晕、滤光片不均匀性）
- `S_true`：加性背景梯度（天空背景空间变化、杂散光）

### 6.2 乘性梯度

```
r = log10(F_instr / F_syn) = log10(M_true)
M_map = 10^r_surface
```

对 r 拟合 2D 多项式曲面，取 10 的幂恢复 M_true。

### 6.3 加性梯度

```
S_map = B_surface
```

对 PSF 拟合的局部背景值 B 直接拟合曲面。

### 6.4 校正公式

```
I_cal = (I_obs - S_map) / M_map * scale
```

`scale` 为全局通量归一化因子，使校正后图像的星点流量与 Gaia 系统一致。

## 7. 容错机制

### 7.1 错误光谱星的自动排除

当某颗参考星的 Gaia 光谱数据错误或积分结果异常时：
1. 该星的 `F_syn` 偏离真实值 -> `r = log10(F_instr/F_syn)` 异常
2. 在 IRLS 迭代中，该星残差远大于 sigma
3. Tukey biweight 权重归零，该星被完全排除
4. 不影响梯度曲面拟合质量

**这就是为什么可以容忍少量错误光谱匹配星**：鲁棒拟合天然能过滤它们。

### 7.2 退化处理

- 匹配星数 < 6：返回恒等曲面（M=1, S=0, scale=1.0），不拟合
- F_instr/F_syn ≤ 0：剔除后再拟合
- 所有阶数样本不足：退化到 1 阶

### 7.3 星-图匹配清洗

`StarMatcher.match_and_clean` 在匹配后执行 MAD 离群清洗：
```
med = median(r)
mad = median(|r - med|)
sigma = 1.4826 * mad
剔除: |r - med| > outlier_sigma * sigma
```

## 8. 关键参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| match_radius_px | 3.0 | 星-图匹配半径（像素） |
| outlier_sigma | 3.0 | 离群清洗 sigma 阈值 |
| max_order | 5 | 梯度曲面最高阶数 |
| _TUKEY_C | 4.685 | Tukey biweight 常数 |
| _MIN_SAMPLE_FACTOR | 3 | 每个参数最低样本倍数 |
| _CV_REL_TOL | 0.10 | LOOCV 选阶容差 |
| _MIN_MATCHED_FOR_FIT | 6 | 最小拟合样本数 |

## 9. 输入输出

### 输入

- `image`: 2D numpy 数组（uint16 或 float），原图
- `gaia_stars`: list[dict]，每项含 `ra, dec, mag_g, f_syn, source_id`
- `wcs_transform`: WCSTransform 对象（cd/crval/crpix/sip）
- `psf_results`: 可选，PSF 拟合结果列表；None 时内部调用星检测

### 输出

```python
{
    "image_calibrated": np.ndarray float32,  # 校正后图像
    "mult_surface": GradientSurface,          # 乘性梯度曲面
    "add_surface": GradientSurface,           # 加性梯度曲面
    "scale_factor": float,                    # 全局通量缩放
    "n_matched": int,                         # 匹配星数
    "n_excluded": int,                        # 清洗排除数
    "quality_report": dict,                   # 质量报告
}
```

### 质量报告字段

```
n_matched, n_excluded, n_used
mult_order, mult_loocv_error, mult_residual_median, mult_residual_std
add_order, add_loocv_error, add_residual_median, add_residual_std
scale_factor
```

### 残差 CSV（诊断用）

- `mult_residuals.csv`: x, y, observed_r, fitted_r, weight
- `add_residuals.csv`: x, y, observed_b, fitted_b, weight

## 10. 性能数据（panel1_Red 实测）

| 指标 | 值 |
|------|-----|
| 匹配星数 | 44 |
| 乘性梯度阶数 | 1 |
| 加性梯度阶数 | 5 |
| scale_factor | 1.048 |
| step4 耗时 | 5.74s |
| 总链路耗时 | 18.28s |
