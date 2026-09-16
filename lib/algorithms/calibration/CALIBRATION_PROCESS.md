# CCD/CMOS 标准校准流程

## 概述

本文档描述天文 CCD/CMOS 图像的标准校准流程，包括主校准帧制作、图像校准、坏点修复三个阶段。

## 一、主校准帧制作

### 1.1 Master Bias（偏置主帧）

- **输入**: 多帧 Bias 帧（零曝光，仅读出噪声）
- **流程**: sigma-clip (3σ) 剔除离群值 → median 合并
- **关键**: **保留坏点**。坏点是传感器的真实特征，校准时需要用含坏点的 Master Dark/Bias 来扣除
- **输出**: float32 FITS，记录 NCOMBINE

### 1.2 Master Dark（暗场主帧）

- **输入**: 多帧 Dark 帧（盖快门，固定曝光时间）
- **流程**: sigma-clip (3σ) 剔除离群值 → median 合并
- **关键**:
  - **Dark 已包含 Bias**，制作 Master Dark 时**不需要减 Bias**
  - **保留坏点**（热像素是真实信号，校准时扣除）
- **输出**: float32 FITS，记录 NCOMBINE / EXPTIME

### 1.3 Master Flat（平场主帧）

- **输入**: 多帧 Flat 帧（均匀光源，短曝光）
- **流程**:
  1. 每帧减 Bias: `Flat_sub = Flat - MasterBias`（Flat 曝光极短，热噪声可忽略，但偏置需要扣除）
  2. 逐帧归一化: `Flat_norm = Flat_sub / median(Flat_sub)`
  3. sigma-clip (3σ) + mean 合并
  4. 再归一化: `MasterFlat = Flat_combined / median(Flat_combined)`
- **关键**: Flat 减 Bias 是因为 Bias 是每帧都有的读出噪声，需要扣除以获得纯净的平场响应
- **输出**: float32 FITS，归一化后 median=1.0，记录 NCOMBINE / FILTER

## 二、图像校准

### 2.1 标准校准（无暗场优化）

适用于 Light 曝光时间与 Dark 曝光时间一致的情况。

```
Calibrated = (Light - MasterDark) / MasterFlat
```

**原理**: Master Dark 已包含 Bias（热噪声 + 读出噪声 + 坏点），直接从 Light 中扣除即可。Master Flat 已归一化（median=1.0），直接做除法校正像素响应不一致。

### 2.2 带暗场优化的校准

适用于 Light 曝光时间与 Dark 曝光时间不一致，需要按比例缩放暗电流的情况。

```
Calibrated = (Light - MasterBias - K × (MasterDark - MasterBias)) / MasterFlat
```

**原理**: 暗场优化需要提取**纯净暗电流** `MasterDark - MasterBias`，乘以缩放系数 K 后从 Light 中扣除。K 的初始值为 `Light曝光时间 / Dark曝光时间`，通过残差最小化（背景区域 MAD 最小）搜索最优 K 值。

**为什么要减 Bias**: 暗场优化场景下，Dark 中的 Bias 成分不能直接乘以 K（Bias 与曝光时间无关），需要先扣除得到纯净暗电流 `Dark - Bias`，再按 K 缩放。

### 2.3 主帧自动匹配

- **Dark 匹配**: 按 EXPTIME 匹配最接近的 Master Dark（容差 ±10s）
- **Flat 匹配**: 按 FILTER 精确匹配 Master Flat（大小写不敏感）

## 三、坏点修复（Cosmetic Correction）

### 3.1 时机

坏点修复在**校准之后**的 Light 上进行。校准过程已通过 `Light - Dark` 扣除了大部分坏点（Dark 中的坏点与 Light 中的坏点位置一致），但残留的单个坏点需要额外修复。

### 3.2 检测策略

- **检测对象**: 校准后的 Light 帧
- **方法**: 局部统计检测（5×5 中值滤波 + 残差 MAD）
- **热像素**: 残差 > hot_sigma × MAD（局部异常高值）
- **冷像素**: 残差 < -cold_sigma × MAD（局部异常低值）
- **孤立性检查**: 候选像素的 8 个邻居中候选像素数 ≤ max_neighbor_candidates（默认 0，严格单像素）
  - 目的：排除星点 PSF（多像素扩展）、宇宙线（线状/簇状）
  - 只修复**孤立的单像素坏点**

### 3.3 修复方法

- **median**: 用 5×5 中值滤波值替换坏点像素
- **bilinear**: 用双线性插值替换坏点像素（NaN 边缘用最近邻回填）

### 3.4 不处理的范围

- **宇宙线**: 校准阶段**不处理**，留给叠加时 3σ rejection 修复
- **星点**: 通过孤立性检查排除，不会被误修复
- **Dark/Bias 主帧的坏点**: **不修复**，保留在主帧中用于校准扣除

## 四、完整管线流程

```
┌─────────────────────────────────────────────────────┐
│  阶段1: 主帧制作                                      │
│  Bias帧  ──→  sigma-clip+median  ──→  MasterBias     │
│  Dark帧  ──→  sigma-clip+median  ──→  MasterDark     │
│  Flat帧  ──→  -Bias+归一化+avg+归一化  ──→  MasterFlat│
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│  阶段2: 图像校准                                      │
│  Light - MasterDark ──→ ÷ MasterFlat ──→ Calibrated  │
│  (或 Light - Bias - K×(Dark-Bias)) / Flat             │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│  阶段3: 坏点修复                                      │
│  Calibrated ──→ 局部统计检测 ──→ 孤立性过滤           │
│             ──→ 中值/双线性插值修复 ──→ Final         │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│  后续: 叠加                                            │
│  多帧 Final ──→ 对齐 ──→ 3σ rejection 合并            │
│  (宇宙线在此阶段去除)                                  │
└─────────────────────────────────────────────────────┘
```

## 五、关键原则总结

1. **Dark 已含 Bias**: 标准校准直接 `Light - Dark`，不需要再减 Bias
2. **Bias 仅用于**: Master Flat 制作（`Flat - Bias`）和暗场优化（`Dark - Bias` 提取纯暗电流）
3. **主帧保留坏点**: Dark/Bias 主帧的坏点是真实信号，校准时扣除，不做修复
4. **坏点修复仅在校准后 Light 上做**: 检测残留的单像素坏点，用临近像素修复
5. **宇宙线留给叠加**: 校准阶段不处理宇宙线，叠加时 3σ rejection 去除
6. **Flat 减 Bias**: Flat 曝光短，热噪声可忽略，但偏置需要扣除
