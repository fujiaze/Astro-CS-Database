# 管线流程概述

> 基于 `流程概述.txt` 整理，详细架构见 `ARCHITECTURE.md`。
> 本文档聚焦流程步骤与模块的对应关系，便于快速理解数据流向。

## 两阶段流水线

### 第一阶段：单帧预处理（`orchestrator stage1`，FITS → .hiss）

| 步骤 | 模块 | 说明 |
|------|------|------|
| 1. 基础校准 | calibration | Bias/Dark/Flat 校准 + 坏点修复 |
| 2. 板解算 | plate_solve | WCS/SIP 解析（向量匹配 + Umeyama SVD） |
| 3. PSF 拟合 | dynamic_psf | 高斯/Moffat PSF 拟合，输出 [N,9] |
| 4. 测光定标 | photometric_calib | CCD QE + 滤镜透过率 + Gaia 积分流量 → IRLS+Tukey 稳健回归求全局 scale + 应用到图像（测光坐标系校准） |
| 5. 帧级基准 SNR | snr_estimator | 流量不确定度/星等残差离散度 → 整帧 SNR 标量 |
| 6. 稀疏 SNR 控制点 | snr_estimator | PSF-SNR 异常剔除 → 稀疏控制点（球面坐标 + snr_psf） |
| 7. Drizzle 重采样 | healpix_drizzle | 图像 + SNR 控制点 → HEALPix（NSIDE 自适应 1-2x 采样率） |
| 8. HISS 落盘 | healpix_drizzle | 输出 .hiss（含 snr 控制点，非稠密 SNR 图） |

> stage1 共 7 节点：READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE（GRADIENT_2D 已于 2026-07-18 归档，曲面拟合和图像亮度修正在 stage2 马赛克阶段处理）。上表把 SNR 拆为 2 步、Drizzle 拆为 2 步展示，实际 orchestrator 中 SNR 与 DRIZZLE 各为 1 个节点。

**SNR 乘法模型**：SNR(ra,dec) = SNR_phot × (IDW_球面插值(控制点) / median_snr)
- SNR_phot = 1/(ln10 × sigma_residual)：帧级全局标量
- 稀疏控制点：避免存储稠密 SNR 图，为第二阶段区域 SNR 拟合提供基础

### 第二阶段：多帧合并（`orchestrator stage2`，.hiss → .hcsd）

| 步骤 | 模块 | 说明 |
|------|------|------|
| 9. 梯度校准 | healpix_stack | 重叠区一致性校正 |
| 10. 区域 SNR 拟合 | healpix_stack | 基于稀疏控制点重新拟合各区域局部 SNR |
| 11. 离群值剔除 | healpix_stack | Winsorized sigma clip |
| 12. SNR² 加权叠加 | healpix_stack | 高 SNR 数据更高权重 → .hcsd 天球数据库 |

## 数据格式

- `.hiss`：单帧 HEALPix 存储（含 snr 控制点通道）
- `.hcsd`：天球数据库（含子叶块索引，nside=64，浏览器按需加载）

## 流程总览

```
原始 FITS
  → 校准 → 板解算 → PSF 拟合
  → 迭代线性测光定标（CCD响应 + 滤镜 + Gaia积分流量）
  → 帧级基准 SNR → PSF-SNR 稀疏控制点提取
  → Drizzle 至 HEALPix（图像 + 控制点）→ HISS 落盘
  → 梯度校准 → 区域 SNR 拟合
  → 离群值剔除 → SNR² 加权马赛克叠加
  → HSCD 天球数据库
```

## 核心设计原则

- **C++ 核心算法 + Python 调试层**：性能敏感算法 C++17 DLL，Python 仅调试
- **数据总线**：data_pipeline（PipelineFrame + PipelineEngine）提供命名块容器在内存中传递
- **稀疏 SNR**：不存储稠密 SNR 图，用稀疏控制点 + IDW 球面插值
- **两段流水线**：stage1 单帧 → .hiss，stage2 多帧 → .hcsd
