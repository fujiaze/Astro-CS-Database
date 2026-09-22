# Phase1 目标态详细设计

> 上游：ASTROCS_DESIGN.md §4（normalize：单帧标准化）

文档 ID：`DESIGN-P1-001`  
状态：`TARGET_NORMATIVE`  
上位：`ASTROCS_DESIGN.md`（§0 权威链，最高设计）、`docs/owner/PROJECT_SPEC.md`  
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

**节点顺序以 `ASTROCS_DESIGN.md` §3.2 为唯一权威**：

```text
ingest → calibration → cosmetic/validity → background/noise
       → star_detection（星表引导检测）→ psf
       → platesolve（星表匹配 + 稳健迭代精化 WCS，此结果即权威 WCS）
       → photometry（测光拟合 + 归一化施加到像素，**同一步**；见下）
       → noise_snr → drizzle → 产品验证 → 原子发布 HiPS+JSON
```

节点可由调度器安排，但科学依赖不可改变；每节点只执行声明 operation，不得通过多个 facade 重复运行整段 Phase1。
**photometry 为什么是一步（不是两步）**：拟合出的归一化标度 `k_photo` 必须真正落到像素，但**施加不需要独立的节点**——
同一节点内「读 calibrated 面 → `I_photo = k_photo·I_cal`（in-place）→ 写 photoapplied 面」一次走完，
省掉一次中间产物落盘，即**省一次写 + 一次读的 IO 往返**（生产 IR 的 normalize 阶段因此是 8 个节点：
`calibrate / cosmetic_correct / detect_sources / plate_solve / measure_flux / estimate_snr / drizzle_stack / write_hips`，
其中 `measure_flux` 即 photometry，施加是它的**内部步骤**而不是第 9 个节点）。
**强制语义（不得放宽，细化见 §3.6）**：① 星表引导检测（检测定义域 = 星表位置，不是整幅图像）；
② WCS 解算只有**一个节点、一个权威解**：近似指向由 `wcs.init_source` 给出（不是解算节点），`platesolve` 在该指向下匹配星表并稳健迭代精化，输出即权威 WCS（轮次数是求解器实现细节，不是流程语义）；
③ **一次检测、一次通量积分、三处复用**（`star_detection` → `psf` → `photometry` → `noise_snr` 共用同一份
检测结果与同一 `flux` 口径）；④ **测光归一化必须真正落到像素**（`I_photo = k_photo·m(x,y)·I_cal`；
未启用时产品显式记 `degraded_reason` 并 fail-closed，元数据 `photappl`/`photscal` 如实落盘）；
⑤ **施加的可核对性不因合并而降低**：provenance `p1_phot.json`（`DATA-P1-PHOTPROV-001`）必须记 `photometry_applied` /
`photscal` / `photscales`（逐帧 `k_photo`）/ `photoapplied_artifacts`（施加后产物路径），使「k 确实乘进了像素」
可由独立读者用「calibrated 面 × k」逐像素复算核对（判据与实测见 `run/RULING-DOC-01/REPORT.md` 裁决 B）。

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
- 检测阈值的**冻结定义**为全局背景噪声倍数 `median(img)+5.0·bgnoise`（`docs/science/STAR_DETECTION.md:18-19`）；以逐像素 variance/ivar 做**局部噪声自适应**为目标态、当前未实现（`DISP-STAR-002`）；输出 selection function 和 completeness 相关参数；检测目录不是图像灵敏度本身；
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

## 8. SNR、点源信息量与 Phase2 输入

### 8.1 三个不同对象

1. **逐源 SNR**：`SNR_s = F_hat_s / sigma_F,s`，用于源测量诊断；依赖真实源亮度。
2. **参考通量深度**：`m_5 = ZP - 2.5 log10[5 sigma_F(ref)]`，用于帧/位置深度表达。
3. **点源信息量**：

```text
W_psf,k(x,y) = a_k(x,y)^2 P_kᵀ C_k⁻¹ P_k = 1 / Var(F_hat_k)
```

用于 Phase2 点源最优合并（Phase1 产出的是该信息量，不产出叠加权重）。白噪声时：

```text
W_psf,k = a_k² Σ_p P_k,p² / sigma_pix,k² = a_k² / (sigma_pix,k² A_NEA,k)
```

固定参考通量下 `SNR_k²(F_ref) = F_ref² W_psf,k`。所以 Phase2 消费的是点源信息量或等价充分统计量，不是未平方 SNR，也不是实际星表的 median SNR。

### 8.2 Phase1 的 SNR 与信息量产品边界

Phase1 **只**产出帧级 SNR、稀疏控制点上的**绝对** SNR（`F_ref/σ_F(x,y)`，与帧级同口径、同参考通量 `F_ref`），以及 `W_psf = PᵀC⁻¹P` 作为点源充分统计量（`point_source_information`）；
**不产生、不消费**任何叠加权重。叠加权重由**阶段二**按该天球像素对应的输入帧集合**现场算出**（派生量）。
PSF 拟合质量代理（FWHM、残差尺度等）**只作诊断**，**禁止**计入阶段二科学叠加权重（`ASTROCS_DESIGN.md` §2、§3.1）。

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
- `point_source_information`（map/model + summary）；`depth_m5`（map/model + summary）；
- source catalog（逐源 flux、variance、SNR、flags）；
- drizzle correlation/transfer 描述；
- product manifest：schema、算法/模块/provider、完整 SHA、输入/配置哈希、单位、参考尺度、近似和降级。

产品的**落盘形态**由配置选定，默认归档形态 `<name>.hips.zst`（整包 tar + 逐成员 zstd 帧），可显式切裸形态 `<name>.hips/`；两形态都必须写出产品级索引 `<name>.hips.index.json`（不压缩：叶块覆盖集合 + 归档定位表），一次运行还写出数据集级覆盖索引 `coverage.index.json`（不压缩：块 → 帧集合）。归档内 `properties` 与裸形态逐字节一致，解压后必须通过既有 HiPS 校验（`docs/design/PRODUCT_STORAGE_FORM.md`、`docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md`）。

禁止用一个 `snr` 字段同时承载上述对象。

## 11. 验收

- 校准解析/Monte Carlo 方差一致；共同 master 相关性不被误当独立；
- 注入点源：理论 `sigma_F = 1/sqrt(W_psf)` 与实测散度一致；
- 改变源亮度分布不改变同一图像的 `W_psf`，但会改变 median source SNR；
- seeing、背景、透明度按理论改变点源信息量；
- 帧级标量门失败时必须升级为空间模型；
- Drizzle 常量面亮度、积分通量、variance 与 correlation oracle 全过；
- 产品从磁盘独立重开后足以执行 Phase2，不依赖进程内状态。
