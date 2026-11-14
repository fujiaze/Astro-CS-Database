# 平面背景噪声与球面参考精度的算法说明

本文说明平面背景估计和参考精度计算的不同职责。被估计量、协方差及可辨识性由噪声正本规定[1]，阶段内对象、球面完成时间和正式序列化由 Normalize 合同规定[2]。这里保留实际算法和检验方法，不以背景模型或兼容 SNR 代替 canonical reference precision。

## 平面背景模型

平面背景接口位于 `lib/algorithms/noise_snr/cpp/include/snr_estimator.h`，实现在同目录的 `src/noise_model.cpp`。`snr_noise_model_v1` 与 `_f64` 分别处理连续行主序浮点数组，消费像素、源位置/通量/FWHM、显式掩膜和配置，产生 `NoiseWeightModelV1`；`snr_noise_model_v1_fill` 在给定位置求值，`snr_noise_model_v1_free` 释放控制数组与登记状态。

当前处理包括：

1. 按配置的 patch 网格收集有限、未掩膜且符合饱和规则的天空样本。
2. 以中位数和 MAD 估计局部尺度，按配置裁剪并重新估计。
3. 收集合格 patch 的位置、背景方差和有效样本量；用有空间支持的平面模型，或退化为具名全局背景摘要。
4. 在有效域求方差及其倒数，单独表达不可用和数值错误。

对独立高斯样本，
\[
 \widehat\sigma=\frac{\operatorname{median}|x-\operatorname{median}x|}{\Phi^{-1}(3/4)}
 \simeq1.482602218505602\,\operatorname{MAD}(x).
\]
这是一致性标定，不保证任意有限样本、相关噪声、裁剪或污染下无偏。PSF 残差列的截尾绝对均值不是 MAD，不能套用相同因子。

配置初值由 `snr_noise_model_v1_default_config` 提供；网格、裁剪倍数、轮数、最小样本量和 `variance_floor` 都是实现参数，不能据初值宣称通用科学误差上界。默认 8×8 网格及 5 倍尺度、最多两轮裁剪仅描述该默认构型。完整配置来源见[3]。

## 掩膜、支持与空间拟合

逐源掩膜使用局部源强、FWHM 和背景尺度计算半径，再受上下限及天空样本预算约束。半径可写成
\[
 r_i=\operatorname{clip}(r_{\rm local}(F_i,\mathrm{FWHM}_i,k\sigma),r_{\min},r_{\max}).
\]
当前实现有两遍背景初估和预算收缩；单星半径公式的单调性不自动传给经过全场预算收缩后的最终掩膜。缺通量或 FWHM 的兼容回退需记录，不能当作已取得相同物理支持。显式 `source_mask` 与自动源掩膜的消费优先级应依接口核对，不能假定两者自动求并。

饱和阈值必须与输入数组单位一致。`saturation_level=0` 表示未提供电平，不证明无饱和；负值及真实零值不因强度符号而失效。NaN/Inf 与掩膜、修复、饱和分别记录。

空间模型用 `v(x,y)=a+b x+c y` 近似背景方差。`plane_geometry_ratio` 以中心化控制点 Gram 的特征值比判断几何支持；当前阈值 `kPlaneGeomRatio=0.0625` 是实现选择。合格点数不足、近共线或模型不可用时，报告对应原因。非负性约束、拟合权、外推域和填充值需连同实际实现说明，不能把无约束 LS、带权约束解及其后处理当作同一算法。

当前 fill 路径对合法正预测施加已绑定 floor 并求倒数；非正预测或产品 dtype 无法表示的数值可采用兼容 `variance=0,ivar=0` 状态。该零对必须由显式可用性解释，不能表示“零随机误差”或无限精度；正式产品未知值遵循数据合同[2]。

## 内存、并发与错误

`g_model_floor` 和 clamp 计数是按模型指针索引的进程级登记表，当前受 `g_model_registry_mutex` 保护，不是模型私有或无全局状态的容器。`free` 擦除登记；调用方仍须保证模型自身不被并发释放或复用。掩膜可能占 O(HW) 内存，不能宣称只有 O(64) 额外工作集。

当前 patch 循环是顺序实现，没有模型级取消回调；帧级并发由宿主调度。不能将计划中的 patch 并行、SIMD 或取消点写成已有能力。跨 DLL 的分配、释放与异常边界按公开 API[4]实现。

当前入口区分成功、完全退化、非法参数/异常以及 `SNR_FLOOR_UNBOUND`。floor 非有限、非正或没有绑定时应报告具体状态。`snr_noise_scale_law`、gain 诊断和 mask 参数的实际校验仍需逐调用核对，不由函数名推断。

## 从平面模型到球面控制点

背景模型只提供部分随机条件。参考精度由 `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp::complete_spherical_reference` 计算；生产适配位于 `lib/infrastructure/scheduler/src/normalize_precision.cpp`，调用边界见[5]。

对完整积分 PSF 模板 P、同次 Drizzle 的最终信号算子 R，建立
\[
 h=RP,\qquad C_{\rm ref}=R C_{\rm planar,ref}R^T,
 \qquad Z_{\rm sphere}=RZ_{\rm planar}.
\]
F 是与完整 PSF 配对的总通量。求解窗截断模板，不重新归一模板以改变同一个 F 的含义；空间标度后的总通量归一也在完整响应域定义。观测叶受到窗外源像元随机贡献时，协方差必须包含这些贡献。

固定参考条件下，背景已知时 `V_F=(hᵀC⁻¹h)⁻¹`；共同拟合背景时求可估计通量的 GLS 方差。秩亏、冗余 nuisance 与精确约束的处理以[1]为准，不任意加对角地板。参考源泊松项、实际场景泊松项、母版和共同参数误差分别表达。

每点保存 F_ref、V_F、SNR_ref、条件与支持。`I_F=SNR_ref²/F_ref²` 是通量精度，转换为面亮度精度需同位置、同条件的 `T=V_B/V_F`；参考源条件不能直接当实际场景条件。乘法组合的 PSF/photometric proxy 与 source-median SNR 均不自动成为 canonical 数据。

## 独立检验与证据

平面检验应覆盖独立高斯场、相关噪声、污染、常量退化、梯度方差、非正预测、几何缺支持、dtype 下溢、非法 floor、掩膜优先级与释放后复用。独立 NumPy/SciPy 实现应消费相同有效样本，不从被测输出抄取期望值。MAD 相对误差及空间模型误差必须随样本量、相关性和污染报告，不能沿用无适用域的 5%/10% 保证。

对确定性标度 alpha，固定输入方差应满足 `v'=alpha²v`、`ivar'=ivar/alpha²`；有限精度下用声明容差，回乘不承诺逐位恒等。gain 诊断式 `mu/g+(RN/g)²` 只适用于对应原生 ADU 条件，经验底噪已含的分量不能再次相加。缺 gain 只有在遗漏项确为 PSD、模板和观测域固定等条件满足时才支持条件 SNR 上界[1]。

已有[局部 GLS 与 Monte Carlo 记录](../../../../实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log)[6]保留异方差、背景、秩亏、标度和 leaf 0 构型。它支持记录中的局部模型，不证明真实逐帧响应、空间积分 PSF、完整 reader 或发布事务已闭合；本页没有重新运行。

方差平面负预测、信号损失及各 oracle 比较目标的来源记录保留在[证据说明](../../../../实验/normalize-precision-p5/DOCUMENTATION_EVIDENCE_NOTES.md)。数值目标、实现自陈与可复跑结果分别判读。

## 参考文献

[1] [噪声与条件参考精度](../../noise_snr/NOISE_SNR.md)。

[2] [Normalize 数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。

[3] [配置合同](../../../engineering/contracts/CONFIG.md)。

[4] [公共 API](../../../engineering/api/PUBLIC_API.md)。

[5] [Normalize 生产链](../../../engineering/architecture/NORMALIZE_PIPELINE.md)。

[6] [局部 GLS 与 Monte Carlo 记录](../../../../实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log)。
