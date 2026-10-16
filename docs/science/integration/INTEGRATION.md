# 接受样本的数值集成

本分册依据[统一科学模型](../unified/UNIFIED_SCIENCE_MODEL.md)，定义同一球面位置的候选归约、支持摘要与状态。样本资格由[排异](REJECTION.md)决定，权重的统计解释由[噪声分册](../noise_snr/NOISE_SNR.md)定义；实际入口及字段见[Mosaic 模块](../../detail/mosaic/modules.md#integrate)。

## 1. 输入与目标

归约器消费已经作相对背景校正、绑定同一坐标与线性面亮度尺度的候选 x_k，数值权重 w_k、支撑 s_k 和接受掩膜。它只负责聚合，不估计噪声、不作排异或重投影，也不保存完整协方差。

缺省权重表示等权，缺省支撑表示全支撑，缺省掩膜表示全部接受。这些是库 API 的数值约定，不能把尚未说明单位和观测域的输入自动变为可比较科学量。实际 CLI 会构造自己的权重数组；等权本身不证明各帧精度相等。

| 对象 | 语义 |
|---|---|
| x_k | 同一目标网格上的面亮度，允许负值与零 |
| w_k | 有限非负数值权重；解释为逆方差时单位为 [x]⁻² |
| s_k | 有限正的具名样本支撑；不能与精度权重混用 |
| accepted_k | 此次归约的候选资格，绑定原帧身份 |
| signal | 正权参与样本的加权均值 |
| support | 接受样本支撑的最大值，内部摘要 |
| n_used | 实际正权参与数 |

## 2. 资格和归约

先检查接受样本的值、支撑和权重；被接受样本中的非有限信号、非正支撑、非有限或负权重为非法输入，不能静默忽略后返回成功。已拒绝样本不参加本次归约。零权重合法，但不贡献 signal。

令 E 为接受且通过上述输入检查的集合，P={k∈E:w_k>0}。P 非空且归约值可表示时，
\[
 W=\sum_{k\in P}w_k,\qquad
 \widehat B=\frac{\sum_{k\in P}w_kx_k}{W}.
\]
支撑独立归约为 max_{k∈E}s_k，包含接受的零权样本；支撑缺省时为 1。它不作为 signal 的第二次除数。若 s_k 确实表示同一目标像元的有效覆盖分数，其最大值只是覆盖并集分数的下界，不能等同真实并集、Drizzle 分母 N 或统计置信度。

每像元按输入索引固定求和次序。浮点乘法和累加仍可能溢出，必须检查 W、加权分子与商的有限性。固定顺序有助可复现；跨后端等价还依赖 dtype、FMA 与浮点环境。

## 3. 六种互斥结果

| 状态 | 定义 |
|---|---|
| OK | 正权贡献非空，输入合法，归约结果有限 |
| NO_CANDIDATES | 没有候选 |
| ALL_REJECTED | 有候选但没有接受样本 |
| ZERO_VALID_WEIGHT | 有合法接受样本，但没有正权贡献 |
| INVALID_INPUT | 接受样本的信号、支撑或权重非法 |
| NON_FINITE_OUTPUT | 合法有限输入在乘法或归约中产生不可表示结果 |

全拒绝、全零权重、非法输入和算术溢出具有不同原因，不能合并为一个成功零值。无有效信号使用 NaN；零权重集合仍可能有几何支撑。局部 status 与 C API 返回码、CLI 退出码各有归属，接口说明见模块正文。

本归约器没有科学调参阈值。输入缺省及上述支撑策略是项目定义，不是从噪声理论推出的最优估计器。

## 4. 统计解释的条件

对固定资格、固定归一系数 a_k=w_k/Σw 和共同被估计量，若帧间协方差为 Γ，则
\[
 \operatorname{Var}(\widehat B)=a^T\Gamma a.
\]
只有帧随机误差独立且 w_k=1/V_B,k 时才退化为 1/Σw。逆方差组合的经典依据见[1,2]；点源最优组合还需响应和 PSF 模型[3,4]。重采样后的空间相关见[5]。

点源信息 I_F=SNR_ref²/F_ref² 不自动等于面亮度逆方差，转换要有同条件 T。共享母版、测光参数、背景同数据拟合、数据驱动排异与通带/PSF 失配都需要额外解释。reducer 能计算数值均值，不意味着这些物理条件已成立。

输出阶段二产品只有 signal HiPS。support、权重和方差可以服务内部计算与评价，不成为最终第二个科学图像通道。

## 5. 独立核验与实现边界

核验包括解析加权均值、常量场、零/负信号、接受零权样本的支撑、六种状态、有限大值溢出、帧映射和不同 worker。零权样本不改变 signal，但可能改变 support；这两个不变量分别检查。

统计实验另测相同目标量、不同噪声、相关帧、共享参数和排异后的偏差/覆盖。接口通过不替代这些证据。生产 reducer 为 `p2_integrate_pixel`，静态调用与限制见[实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)；本轮没有运行数值检验。

## 参考文献

[1] Bevington P. R., Robinson D. K. Data Reduction and Error Analysis for the Physical Sciences. 3rd ed. McGraw-Hill, 2003. ISBN 0072472278。

[2] Aitken A. C. On least squares and linear combination of observations. Proc. Roy. Soc. Edinburgh, 1935, 55: 42–48. [DOI](https://doi.org/10.1017/S0370164600014346)。

[3] Zackay B., Ofek E. O. How to coadd images. ApJ, 2017, 836: 187. [DOI](https://doi.org/10.3847/1538-4357/836/2/187)。

[4] Naylor T. An optimal extraction algorithm for imaging photometry. MNRAS, 1998, 296: 339–346. [DOI](https://doi.org/10.1046/j.1365-8711.1998.01314.x)。

[5] Fruchter A. S., Hook R. N. Drizzle: A method for the linear reconstruction of undersampled images. PASP, 2002, 114: 144–152. [DOI](https://doi.org/10.1086/338393)。
