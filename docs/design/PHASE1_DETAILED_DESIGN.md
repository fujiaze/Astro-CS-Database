# Phase1 目标态详细设计

文档 ID：`DESIGN-P1-001`  
状态：`TARGET_NORMATIVE`  
上位：`ASTROCS_PROJECT_CONSTITUTION.md`、`docs/owner/PROJECT_SPEC.md`  
下游：Phase1 SCI/ALG/DATA/API/实现与验收；冲突时本文件描述目标，现有实现不得反向定义目标。

## 1. 使命与科学产品

Phase1 把一帧传感器观测变成一个可由任意后续程序独立消费的、光度与天球坐标已定义、噪声可传播、PSF 可解释的球面单帧产品。它不是简单“生成 HiPS”，而是把原始 CCD/CMOS 数据转换为观测模型：

```text
d_k = A_k x + n_k,    Cov(n_k) = C_k
```

其中 (A_k) 包含光度响应、PSF、像素响应、WCS 和重采样算子。Phase1 必须交付足够信息，使 Phase2 不回读原始 light 也能：

1. 对扩展源作方差正确的面亮度合并；
2. 对点源作 PSF-aware 的最大信噪检测/测光；
3. 判断近似何时失效而不是静默使用一个质量分。

## 2. 输入合同

- 原始/预处理 light；bias、dark、flat、cosmetic map（按相机、增益、温度、滤镜、曝光分组）；
- 曝光、gain、read noise、饱和、非线性状态、时间、滤镜和观测站元数据；
- Gaia/离线星表输入及版本；
- phase_config，只含科学参数；不得含 workers/ISA/block；
- 每个输入有 SHA-256、单位、dtype、shape、所有权和 frame identity。

缺失关键单位、gain/read-noise 口径、WCS 所需元数据或产品身份不一致时 fail-closed；允许的降级必须写 manifest，不能以默认零代替未知。

## 3. 节点与先后关系

`ingest → detector calibration → cosmetic/validity → background/noise → source detection → PSF → astrometry → photometry → sensitivity/information → spherical resampling → product validation → atomic publish`。

节点可由调度器安排，但科学依赖不可改变；每节点只执行声明 operation，不得通过多个 facade 重复运行整段 Phase1。

## 4. 校准与方差传播

### 4.1 信号

典型分离 bias/dark 路径：

```text
y_p = [r_p - b_p - alpha (d_p - b_p)] / f_p,    alpha = t_light / t_dark
```

不得裁切负值或加未声明 pedestal。flat 归一、暗场缩放、非线性和饱和的实际口径必须进入 manifest。

### 4.2 不确定度

目标态不得再用“校准层不传播、后面重新猜一个噪声”替代物理传播。独立近似下至少传播：

```text
V(y_p) = {V(r_p)+V(b_p)+alpha²[V(d_p)+V(b_p)]+y_p²V(f_p)} / f_p²
```

实际共享 master 导致跨像素/跨帧相关时，不存完整巨矩阵，但必须保留低秩/相关核/共同 master ID 和强度参数。gain、Poisson、read noise、量化和校准 master 方差分别标识。

## 5. 背景、有效性与源检测

- 背景模型 (B(x,y)) 与随机噪声 (C) 分开；Phase1 可估计背景但不得把背景校正和 UPM 混成同一层；
- validity 包含 NaN/Inf、坏点、饱和、cosmetic、边界、插值、星轨/严重形变；
- 检测阈值基于局部噪声，输出 selection function 和 completeness 相关参数；检测目录不是图像灵敏度本身；
- 检测、PSF、WCS、测光、SNR 的 source row 都绑定同一 frame_id/source_id。

## 6. PSF 模型

PSF `P_k(u,v;x,y)` 在每个位置归一为 `ΣP=1`，包含像素响应。产品至少提供：

- PSF 家族、参数、FWHM/椭率、有效域、拟合残差；
- 空间变化模型及协方差；若通过均匀性门可降级为帧级 PSF；
- noise-equivalent area：`A_NEA = 1/ΣP_p²`（白噪声）；
- 更一般的信息核 `PᵀC⁻¹P`。

PSF 拟合质量只能作 validity/诊断，不能未经概率模型直接乘入科学权重。

## 7. 天体测量与测光

- WCS 为 ICRS，像素中心、轴向、单位、SIP/PV 域明确；正反变换和独立星表残差验证；
- 光度模型把本帧 signal 映射到统一线性通量尺度：`d = a_k F P + n`；必须给 `a_k` 及其不确定度、颜色项和有效域；
- 相对标度不足时标记不可跨帧合并，不用“median stellar flux”静默代替；
- astrometry/photometry 的系统误差与随机误差分开。

## 8. SNR、PSF Signal Weight 与 Phase2 输入

### 8.1 三个不同对象

1. **逐源 SNR**：`SNR_s = F_hat_s / sigma_F,s`，用于源测量诊断；依赖真实源亮度。
2. **参考通量深度**：`m_5 = ZP - 2.5 log10[5 sigma_F(ref)]`，用于帧/位置深度表达。
3. **点源信息权重**：

```text
W_psf,k(x,y) = a_k(x,y)^2 P_kᵀ C_k⁻¹ P_k = 1 / Var(F_hat_k)
```

用于 Phase2 点源最优合并。白噪声时：

```text
W_psf,k = a_k² Σ_p P_k,p² / sigma_pix,k² = a_k² / (sigma_pix,k² A_NEA,k)
```

固定参考通量下 `SNR_k²(F_ref) = F_ref² W_psf,k`。所以 Phase2 消费的是信息权重或等价充分统计量，不是未平方 SNR，也不是实际星表的 median SNR。

### 8.2 PSF Signal Weight 双产品

AstroCS 正式生产两类 PSF 权重：

- `psf_information_weight`：上述 `W_psf=PᵀC⁻¹P` 信息权重，是点源检测/测光默认科学产品；
- `psfsw_robust_weight`：受 PixInsight PSFSW 启发，综合共同星集的 PSF 总 signal、signal concentration、稳健 noise 和稳健 background，是 Phase2 conventional integration 的可选相对帧权重。

Phase1 必须把 PSFSW 的四个分量、共同星集/selection function、归一和有效性分别输出。该相对无量纲权重不写成 ivar，也不取代 `W_psf`；详细合同见 `docs/science/PSF_SIGNAL_WEIGHT.md`。

### 8.3 标量降级门

只有当帧内 `W_psf(x,y)` 的鲁棒相对离散和系统趋势均低于 SCI 指定阈值，才允许存帧级标量；否则存稀疏控制点/多项式/HEALPix map。摘要必须带 p05/p50/p95、最大系统偏差、取样覆盖和模型误差。

## 9. 球面 Drizzle 与不确定度

源像素积分通量 `x_j` 先转换为源像素面亮度 `B_j = x_j/A_pixel,j`，按球面交叠面积 `a_jp` 估计：

```text
S_p = Σ_j B_j a_jp / Σ_j a_jp
```

同时输出线性算子/足够的方差传播信息、support、coverage、validity 和相关噪声描述。pixfrac、像素面积与单位不可隐含。点源信息权重不能仅用 drizzle 后逐像素 ivar 重建而丢掉 PSF/协方差。

## 10. Phase1 产品

一个原子目录至少包含：

- HiPS signal；pixel variance/ivar；support；coverage/validity；
- PSF 模型/地图；photometric response；WCS；背景/噪声模型；
- `point_source_information`（map/model + summary）；`psfsw_robust`（四分量 + 相对权重 + validity）；`depth_m5`（map/model + summary）；
- source catalog（逐源 flux、variance、SNR、flags）；
- drizzle correlation/transfer 描述；
- product manifest：schema、算法/模块/provider、完整 SHA、输入/配置哈希、单位、参考尺度、近似和降级。

禁止用一个 `snr` 字段同时承载上述对象。

## 11. 验收

- 校准解析/Monte Carlo 方差一致；共同 master 相关性不被误当独立；
- 注入点源：理论 `sigma_F = 1/sqrt(W_psf)` 与实测散度一致；
- 改变源亮度分布不改变同一图像的 `W_psf`，但会改变 median source SNR；
- seeing、背景、透明度按理论改变信息权重；
- 帧级标量门失败时必须升级为空间模型；
- Drizzle 常量面亮度、积分通量、variance 与 correlation oracle 全过；
- 产品从磁盘独立重开后足以执行 Phase2，不依赖进程内状态。
