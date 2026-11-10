# B4｜统一不确定度、相对背景与传统合成的科学推导和统计验证

## S0 共同真实随机变量只传播一次

定义同一原始观测及共享 Gaia/flat/master/PSF/background latent；以唯一因子、Jacobian或 `C` action 保留跨像元/跨帧相关。`C_{out}=R C_{in}Rᵀ` 只能在真实 `C_in` 已知/条件已知时计算；模型偏差、数值界、噪声方差不是一回事。`gain` 不可识别时不能凭 ADU 推出电子 Poisson 方差。将 `V_F`（点源参考 Fisher）、`V_B`（实际面亮度）及 `V_ctrl`（控制点）分别定义并独立验证，禁止把参考SNR平方除以通量平方冒充 scene IVAR。

**独立例**：小矩阵 2×3 `R`、非对角C、有共享factor及带负号孔径t，用精确有理数计算 `R C Rᵀ` 与 `tᵀ C t`；相同 `V_F` 不同 `V_B` 反例；100帧共享底噪不会随K→∞消失；缺部分方差则完整scene variance必须unknown。

## S1 稀疏 actual-scene 方差
先证明存在什么观测量/误差界，才允许用对数域插值/三角网/球面chart做预测。提供控点支持、holes、外部梯度/曲率上界、插值误差和查询有效域；只有采样点而无曲率约束时 `C∞ bump` 反例说明不可得到全域确定界。参考点源 Fisher 仅在有数学桥接 `V_B/V_F`、相同估计量/噪声条件时才转换；否则保留参考诊断。

## S2 Sky controls与差模
- 模型 `y_k=H_k S+A_kβ_k+ε_k` 只用于可比较响应/PSF下的共同天空；构造 `T` 控制行、`H` 共同天空、`A/E` 背景基、`K` GLS/profile，从图连通分量辨认可估的 β 和 gauge；不把分量公共常数/斜坡当已知绝对天光。
- 证明 `K H=0` 在所声明共同模式上的保护，以及 `P=I-EKT`；联合噪声是 `P C_y Pᵀ`，有**两个负交叉项**。参与拟合的数据不能用“先独立算β方差，再加到原方差”的错误式；无支持/异响应不造伪control。
- 外部估计和同数据估计要区分；fit与eval必须是相同basis/action；β的输出、受支持域、预测域、未识别nullspace与潜在残余 sky shot noise 一并保存。
- 反例：真实星云梯度无差模，拟合应零；差模小平面按gauge移除；断连、空控制、Gaia共有星/共享噪声、Hα/Red不可比、PSF不匹配、负背景和同数据相关。

## S3 选择、权重、合成
- `Selection` 在冻结 sky后基于可信 DQ/support/可比较样本建立，N=1..3默认不做无证据时间排异。小N仅在可信外部Γ和共同均值假设下研究 N≥4 单步最大LOO与 Bonferroni；阈值由显式统计命题推导而不是“看结果调到绿”。
- SPD `Γ` 的 BLUE：`t=Γ^{-1}1/(1ᵀΓ^{-1}1)`；PSD保持精确零空间约束，非负要求用真实KKT/QP证书；Γ未知时自动策略可合法等权但不能叫已知IVAR或精度最优。
- 观察依赖选择/权导致统计分布改变；固定最终mask的 `tᵀΓt` 是**条件公式**，不自动具有无条件68/95%覆盖保证；使用包括fit/selection/sky的完整forward Monte Carlo检验bias/coverage/误拒率并给二项置信区间。
- 异PSF/通带即使用户允许组合，也应输出空间/谱有效混合核 `K_eff(p,ω,λ)` 或条件模型；不得称与单滤镜同一物理天空标量。瞬变/变星不自动成为坏帧。

## S4 唯一工程消费链
`sky.controls → sky.fit → apply_sky → coadd.select → coadd.weights → coadd.integrate`。每个阶段产生不可变对象且保存上游hash/模型/units/shared-root；`coadd.integrate` 仅消费传入已冻结的Selection和WeightSolution，不偷偷重复选择或求权。几何revision变化必须重新检查sky的controls、Γ和选择；不允许原位修改有效产品。

## 交付
逐式推导、原始文献/自研变体卡、可独立求解的精确小矩阵、历史R3–R5成功/失败证据重访、新选择后MC/真实复拍计划，以及生产`src/information/sky/coadd/uncertainty`逐源码的对照报告。统计判据要求明确适用域与容许的误拒概率，不能从单次噪声样本的最大百分比误差推断整个方法失败或成功。
