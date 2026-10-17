# 单信号球面 HiPS 到平面 FITS 的数值重采样

本分册依据[最高设计](../../ACSD_DESIGN.md)与[平面导出合同](../projection/HIPS_TO_FITS.md)，定义已有球面信号的数值重采样及几何、图像质量影响。输入量见[统一科学模型](../unified/UNIFIED_SCIENCE_MODEL.md)，实现状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 重采样是已成像信号的数值表示变更

输入是兼容HiPS的signal通道，典型来源为阶段二马赛克；每个叶是当前混合/参考通带约定下的球面平均线性强度 B_j；输出是给定 WCS 平面网格的平均/插值信号 y_i。并无新光子输入，不会通过重复插值增加天文信息。

所有重采样核需定义：
- 物理输入究竟为 leaf 面积平均值还是叶中心抽样值；
- 核对目标 WCS 面积平均还是中心值进行估计；
- 边缘缺数据与 NaN 如何处理；
- 真实权重与归一因子、独立几何误差；
- 对星点、弥散目标、噪声相关与分辨率的影响。

“插值后看起来噪声更细”只是统计相关性及频谱改变，不是新的 photon SNR 测量。

## 2. 球面到平面几何

设 HEALPix leaf H_j，目标平面像素对应的球面足迹 Q_i，面积分别 Ω_j 和 Ω'_i。RA/Dec 来自符合FITS WCS标准[1,2]的反投影，不在极点/RA 0° 处使用未处理经纬度线性相加。

对连续面积重采样，a_ij=area(Q_i∩H_j)，在有效区域上有：
\[
 y_i=\frac{\sum_{j\in E_i}a_{ij}B_j}{\sum_{j\in E_i}a_{ij}}.
\]
这条公式求的是**已覆盖部分的平均面亮度**；若目标全部覆盖，则分母恰为 Ω'_i（按同一精确几何定义）。如果想要对全目标像素的平均并把缺失部分作为未知，缺失域不能自动设为零。应根据用户输出目的指定部分覆盖政策而非误把两者当同一个算子。

对重建目标像素的**积分通量**，只有在所选面亮度代表正确的目标物理响应且足迹/覆盖一致时才可以再乘目标立体角；在跨通带混合产品上仍需遵守光度约定。

## 3. 近邻与局部线性核

最近邻：经球面反映射找包含输出中心方向的 HEALPix leaf，取 B_j。它是几何上明确的零阶保持算子，但在输出高采样倍率或 WCS 畸变显著时可能产生块状 aliasing；“没有插值”不代表在科学上没有采样误差。

四象限局部线性：在输出天球位置的局部切平面取多个有确定几何位置的叶中心，按两方向的比例构造非负权重，合格节点上归一 Σ_j c_ij=1。它对常量场保持，平滑变化场可有较低数值插值误差，却不能保证保持星点 PSF 总通量，也不等于真正的面积平均。极区/face 边缘需要独立几何连续测试。若四象限候选退化，则明确使用的几何近邻选择策略，避免跨界不连续。

对已选邻域值中的 NaN/Inf 不能赋 0；若部分值可用，则以合格集合 E_i 计算
\[
 y_i=\frac{\sum_{j\in E_i}c^{(0)}_{ij}B_j}
 {\sum_{j\in E_i}c^{(0)}_{ij}},
\]
只有分母为正且有限才定义 y_i。这个选择相当于不同的**有效核**，会改变局部采样形状；空洞边缘需要与保守不外推的替代方法对照。

### 3.1 不规则 HEALPix 邻域的一次再现定理与保守替代核（2026-10-17）

对局部切平面已实际确定的叶中心位置 `q_j=(x_j,y_j)`、目标方向在同一平面的位置 `t=(x_t,y_t)`，任意固定线性插值 `\widehat B(t)=Σ_j c_j B(q_j)` 对**所有**仿射天空 `B(x,y)=b_0+b_xx+b_yy` 精确，当且仅当
\[
\sum_j c_j=1,\qquad
\sum_j c_jx_j=x_t,\qquad
\sum_j c_jy_j=y_t.
\]
必要性由代入三个基函数 `1,x,y` 立即得到；充分性由线性组合得到。标准矩形四角系数仅对**其自身几何约定的矩形四角**自动保证这三个条件，切平面上实际 HEALPix 最近邻四象限不一定构成该矩形。这是 Stage3 #90 的数值精度问题，不涉及 Stage1 curved spherical Drizzle #15。

一种无需猜测的**替代候选**：在真实候选叶中心集合寻找包含 t 的非退化三角形 `q_0,q_1,q_2`，求 `V c=(1,x_t,y_t)^T`，其中
\[
V=\begin{pmatrix}1&1&1\\x_0&x_1&x_2\\y_0&y_1&y_2\end{pmatrix}.
\]
当 `det V≠0` 且三个重心系数 `c_i≥0`，目标位于三角形闭凸包、权和为1且仿射一次精确；不得把不稳定的近共线 `V` 当有可靠解。若目标缺少可支持三角形，返回 `unsupported` 并按明确的 fallback policy 处理，不得悄悄将多次引用同一 leaf 当独立观测。对连续二阶可微球面场（**将完整观测算子固定后，在局部切平面上**）有 `||H_B||_2≤M` 时，重心插值误差满足
\[
|\widehat B(t)-B(t)|\le \frac M2\sum_i c_i\|q_i-t\|^2,
\]
但输入是 HEALPix **叶面积平均值** 时尚有 cell averaging/source PSF/球面曲率与 WCS 的模型误差，不能把本式用作整个 Stage3 physical brightness error certificate。针对不规则点集的真实影响需真实几何采样与有真值的 forward scene 实验。

同一个输入 leaf 在候选角重复时，对随机变量的权要先按完整 leaf ID 聚合：`a_j=\sum_{k:id(k)=j}c_k`，再在**不同叶独立**条件下得到 `Var(\widehat B)=\sum_j a_j^2V_j`；一般含跨叶相关时为 `a^TCa`。当前 `p3_uncertainty_propagate` 已包含重复叶聚合，不应沿用 #88 旧行号把现版误判为未修。纯数学对照见 [graph_and_affine_oracles.py](../../../实验/innovation-closure/code/graph_and_affine_oracles.py)；真实 Stage3 full-sky 几何和 E2 尚待 #90/#88 分别验证。

## 4. 面积交叠核与相关性

面积交叠核使用真实球面边界。精确的 HEALPix 叶边界一般不是四条大圆弧，所用球面多边形/切平面近似必须登记与真实曲边的独立误差。输出 FOV 边缘像元与输入 tile 面缝需有专门测试；仅依靠“每个权重正且和为 1”无法发现整体 WCS 手性错误。

无论选择什么核，只要固定了数据可用集合就是线性算子 y=R B。若阶段二球面图的完整随机协方差 C_B 在研究时可用，则
\[
 C_y=R C_B R^T,\quad
 {\rm Var}(a^Ty)=a^T C_y a.
\]
由于阶段二最终不发布 C_B，阶段三对这些量**只能作研究世界中的外部评价，不输出伪造的绝对 variance/ivar**。用单层最终图测局部 RMS 无法从中唯一恢复重采样前的独立噪声与它的所有协方差。

## 5. 层级选择与两次重采样的误差分离

HiPS层级遵循[3]。512 像素 tile 的等面积等效尺度：s_K=√(π/3)/(512·2^K)。选择能覆盖请求输出尺度的输入 order，但该数只是面积等效标尺，不保证最坏方向采样间隔。对过采样请求，最大层级 Kmax 不改变，必须在元数据中说明放大不创造物理分辨率。

HiPS 的父层本来是从叶层再次粗化的图像，而阶段三进一步在目标 WCS 上重采样。对“输出变软”“细节晕圈”“条纹”必须测量：
1. CCD → 球面 Drizzle 的一次算子；
2. 最细层 → HiPS 较粗层级的层级派生；
3. 已选 HiPS 层 → 最终平面 WCS 的 export 核。

在有真值的独立模拟中分别替换一段算子作对照，对点源积分通量、FWHM、MTF、扩展源低频、噪声功率谱和边界连续性测量，避免把后期显示拉伸作为科学结论。

## 6. 输出与代码实际限制

返回一个二维 FITS signal 图像，头中记录 FITS WCS、BUNIT、有效光度系统/响应说明、实际层序和采样核。NaN 表示不支持/无数据，零与负数是合法科学信号。不要要求阶段二额外发布 support/variance/ivar 来让现有 export API 通过；应按单信号产品修改阶段三消费合同。

具体可用核和生产消费限制以[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为准。独立评价在同一真值世界中分离Drizzle、层级派生和Export；几何构型与保留证据见[球面实验报告](../../../实验/healpix-polar/REPORT_experiment.md)。

## 7. 参考文献

[1] Greisen, E. W., Calabretta, M. R. Representations of world coordinates in FITS. A&A, 395, 1061–1075. [论文](https://arxiv.org/abs/astro-ph/0207407)。

[2] Calabretta, M. R., Greisen, E. W. Representations of celestial coordinates in FITS. A&A, 395, 1077–1122. [论文](https://arxiv.org/abs/astro-ph/0207413)。

[3] IVOA. HiPS: Hierarchical Progressive Surveys. [推荐标准](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)。
