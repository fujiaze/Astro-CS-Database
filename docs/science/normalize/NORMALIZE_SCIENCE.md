# Normalize 科学正本：阶段一唯一因果链与物理量合同

> **状态：正式目标设计，不是当前全部已实现的证明。** 由 ChatGPT 前台于 2026-10-14 统一整理。上游 docs/ACSD_DESIGN.md §1–§4、docs/science/unified/UNIFIED_SCIENCE_MODEL.md 和 DATA_SEMANTICS.md。本文件唯一决定 Normalize 全链的物理量、阶段先后关系、数据语义；各专题 science/calibration、science/detection、science/psf、science/photometry、science/noise_snr、science/drizzle 只维护自身数学细节。源码与落地状态见 docs/engineering/architecture/NORMALIZE_PIPELINE.md。
>
> **重要纠正**：历史 实验/science-v2-impl/code/wcs_census.py 只检查原始 FITS 头里有没有现成 CRVAL/CRPIX/CD，没有调用项目 IPV 解算器。“31 帧缺原始 WCS”不等于“31 帧 Normalize 无法解析”。历史三阶段 exit=0 也不等于本文件定义的全科学链已成立。

## 1. 单帧 Normalize 究竟估计什么

由 CCD/CMOS 像素观测、仪器母版、Gaia 数据和设备记录，构建**每帧独立的球面已成像线性信号**及其几何、光度、PSF、噪声与空间条件精度说明。Mosaic 无须回读光帧，就能解释该帧的坐标、统一参考光度约定、参考源信息量和相关噪声模型。Normalize 绝不做跨帧接缝/相对天光求解、最终多帧权重与合成。

一条科学模型通过一个用户命令执行；同命令可含多个校准 block，一个 frame 应有独立 HiPS 信号产品。不同设备/滤镜可以由用户在 Mosaic 自由选择，Normalize 内部为了母版与响应模型而分组不是用户数据准入限制。所谓科学精度包含随机条件估计和无法辨识的系统项；没有真实来源的绝对精度不许伪造。

## 2. 唯一逻辑处理顺序

\`\`\`text
raw lights / masters / Gaia XP / instrument metadata
  → ingest + 校准观测与数据单位/有效域
  → calibration / cosmetic / 初始背景噪声统计
  → astrometry [IPV 自行盲检 solver_detections + Gaia 匹配/解 TAN+SIP]
  → 已解 WCS / 逐帧 unsolved verdict
  → Gaia WCS-guided science_sources / centroid / flux
  → spatial PSF model
  → Gaia XP per-frame photometric response + scale application ONCE
  → canonical noise conditions and sparse conditional reference-SNR controls
  → sphere Drizzle (+ effective PSF / correlation / geometry provenance)
  → per-frame signal HiPS + sparse controls + scientific manifest
\`\`\`

图是**因果图而不是禁止并行的线性程序**：不依赖 WCS 的初始天空噪声统计可并行；但最终 photometry、science_sources、SNR、Drizzle 的 WCS、标度和质量状态必须来自同一帧确定的版本。下游不得靠“p1_wcs.json 文件刚好已经出现”猜测上游已完成；明确使用 typed port/版本化对象。

必须区分两类星点：
- **solver_detections**：IPV 解算器内部以校准像素盲检测出的几何候选，用来解 WCS，前提中**不需要 WCS**。
- **science_sources**：WCS 求解之后，以完整 WCS+SIP 和 Gaia 投影构建的源检测/质心/通量/PSF/掩膜目录。不能用 solver 候选代替后续经过模型约束的科学星表。

## 3. 天体测量：不要求原始 FITS 已含 WCS

默认权威 WCS 应由 IPV PlateSolve 对**校准后的实际像元**自主求解：从天区/尺度近似提示和本地图像星候选出发，使用 Gaia 的匹配/鲁棒精化，产出 ICRS 下 TAN/CD、SIP 与来源、残差、实际可靠域。合法初始信息可以是 OBJCTRA/OBJCTDEC 或 RA/DEC、FOCALLEN、XPIXSZ、用户配置和 ACSD 先前已解产品的邻帧提示。它们是粗指向/尺度，**不是完成的 WCS**。项目当前配置还支持独立声明的 explicit_config WCS：只是用户提供且被检查的 WCS，不能记成 IPV 解析成功。

必须逐帧区分：
- 输入原始 FITS 是否已有 WCS 头；此项仅是文件属性；
- 解算输入的 hint 来源、Gaia 索引和状态；
- 求解成功/失败及每次尝试的检测数、星表数、匹配内点、RMS、候选星分布与几何条件；
- 真正输出 WCS（CRPIX、CRVAL、CD、CTYPE、TAN/SIP A/B 与 AP/BP 的口径、ICRS）及来源 ipv|explicit_config；
- 用**独立留出的 Gaia 星/外部 WCS 读写器**计算的绝对天球残差、覆盖率、边角畸变、旋转/反射、尺度与 RA wrap。

内部 0-based 的像元数组中心送到 FITS WCS 1-based 坐标时恰好加 1；相同代码的 pix2sky/sky2pix 往返误差小只能说明内部自洽，**不能证明绝对天球坐标正确**。如果 WCS 无解，记逐帧 unsolved 与真实原因，不制造伪 WCS，也不对该帧 Drizzle；别的已解帧照样有可保留的独立产物，但集合是否完整成功应如实报告。

**M42 30 帧与 GalaxyCenter 1 帧的历史“缺 WCS”只来自 FITS 头 census。这些帧是否可以被 IPV 解算，现在尚未有该执行证据，不能写成外部求解任务的既定需求。**

## 4. 校准、坏点与随机/共同不确定性

校准算子先明确偏置 B、暗场 D 是否已含 B、曝光时间系数 γ 与归一平场 f。示例（只适用于暗场还未单独去偏置的约定）：

\[
 y_i=\frac{L_i-B_i-\gamma(D_i-B_i)}{f_i}.
\]

若使用已去偏置 dark，必须采用相应等价式，禁止双扣 Bias。坏点插值/宇宙线/饱和的 validity 与源信号分别传递；负线性信号合法，数值零不是无效像元。

随机输入向量 z→校准输出 y=f(z) 的一阶条件协方差是

\[
 C_y\approx J_z C_z J_z^T,\quad J_z=\partial f/\partial z.
\]

因此 master dark/bias/flat 的有限估计误差、平场除法的信号依赖、共享母版引起的帧内/跨帧共同误差，不能在物理模型里被一个独立像元常数替代。**实现缺口**：当前 science/calibration/CALIBRATION.md §5.7 和真实代码尚未证明母版方差传递已闭合；阶段一必须记录已知的条件方差与 master_uncertainty=unmodelled/approximate 等精度状态，不应在文档中一边宣称全传递一边又把遗漏写成“永远不传播的冻结科学口径”。

## 5. 科学源表、PSF 与单位

IPV 求解成功后，用该帧 WCS（**包含 SIP**）把 Gaia 源投影到图像测量域。每个科学源保留 frame_id/source_id、测量通量与方差、定位、掩膜/饱和/拥挤和质量原因。仅有粗检测源的 SNR 中位数不等于条件参考点源深度。

PSF 模型 P_k(x,y;u) 是在位置 u 下单位点源通量到像元的积分响应；在有效域内保持相应积分归一。FWHM、椭率、拟合残差只描述已成像轮廓，**不能**单凭 FWHM 定义全局匹配滤波 Fisher 信息；source_snr 使用的 h 必须与实际流量估计器/有效 PSF 和同一像元单位匹配。

## 6. Gaia XP：逐帧响应与仅一次的线性标度应用

帧 k 的综合光子响应 R_k(λ) 与 Gaia XP 星光谱 f_λ,s 定义（按选定能量/光子约定）：

\[
 Q_{ks}=(hc)^{-1}\int f_{\lambda,s}(\lambda)R_k(\lambda)\lambda\,d\lambda.
\]

将实际星点积分计数与 Q_ks 拟合得到逐帧系数 a_k 或等价的 k_photo，必要的颜色依赖/空间响应 m_k(u) 与它们的误差、可辨识域一起登记。真实像元在这个节点施加**恰好一次**线性比例，后面 Drizzle 不可再次重复缩放。参考响应 R_0 的设置是共同光谱约定，不能凭 Gaia 恒星色项推出未知 Hα/[N II] 星云逐像元 SED。

校准 ADU、已施加 k_photo 的线性单位、球面 [x]/sr 单位各自准确命名。若 k_photo 仅对齐**相对零点**，FITS BUNIT 和 provenance 不能假装获得了 SI 绝对辐亮度或尚在未经缩放的 ADU；若合成谱通量单位无标准 FITS 表达，使用明确的模型尺度+附带约定，不杜撰物理单位。

**分组界限**：相同母版或同响应光度模型可以内部共享计算，不同 R_k 需要逐帧响应模型或自动分子组；合法异滤镜不能只因为“不同于组内第一帧”就标为坏观测。未知响应属于可辨识性问题，可报告 relative_only/unknown_response，但不能伪造绝对零点，也不能在 Mosaic 拒绝用户自由组合。最终用户是否追求 SNR 或高精度由用户决定。

## 7. 稀疏 SNR 的正式估计量与必要数据

**三个不同估计量**：
1. source_snr(s)=实际某星通量估计/对应 σ_F,s；
2. frame_snr_ref=在同一参考光谱与噪声口径上的帧级深度**摘要**；
3. sparse_snr_controls(u)=在指定天球位置的局部 PSF、噪声、面积和背景条件下，对**固定参考点源**的条件信息量。它是供阶段二插值的空间控制点，不是实际恒星 SNR 中位数的别名。

对同一观测单位的局部面亮度向量 y=F h+Zβ+n，C 为相应协方差：

\[
 V_F=
 \begin{cases}
 (h^T C^{-1}h)^{-1}, & \text{背景已知},\\
 [(A^T C^{-1}A)^{-1}]_{FF},\quad A=[h,Z], & \text{共同估计背景},
 \end{cases}
 \qquad SNR_{\mathrm{ref},k}(u)=F_{\mathrm{ref},k}/\sqrt{V_{F,k}(u)}.
\]

参考基准统一的是**参考星光谱/参考颜色/目标光度系统的定义**，不等于所有未统一的相机原生 ADU 帧使用同一个数值 F_0。各帧 F_ref,k 必须由其响应 R_k、实际归一化和单位得到，允许在**真正同数值单位**下取公共数值 F_ref。将一批图像自身检出星流量中位数作为 F_0，只可用作具名相对/历史近似，不能当独立于星场和样本选择的物理标准。

每个 canonical 控制点最少含 frame_id、ICRS 位置、响应和参考 SED/flux/单位、V_F 和 SNR_ref、PSF/kernel 与 noise model 身份、背景 nuisance 口径、空间支持、有效性与精度状态（measured/approximate/upper_bound_no_gain/unavailable）、provenance。若只有背景项而缺 gain/source Poisson，可声明条件上界或近似，不能称已具备完整绝对 σ_F/5σ 深度。

**实际代码缺口**：aio_snr_model.h 的 HioSnrControlPoint.snr_psf 注释为 (A−B)/mad；snr_phot 是恒星测光残差派生的全局统计量；这两者都**不能自动**代替 F_ref/√V_F。旧 SNR Catalogue 存储此数据的事实 ≠ 已经产生 canonical sparse controls。Phase1 真正的稀疏 producer 和 Phase2 reader 的共同 schema、坐标/支持域是科学接口的一部分，不能只改名字而使公式表面相同。

参考点源通量方差的倒数 I_F=SNR_ref²/F_ref² 与 Mosaic 合并 HEALPix **面亮度**的 w_B=1/V_B 是不同的被估计量。Phase1 留下足够的有效 PSF、几何和噪声相关描述，让 Phase2 使用 T=V_B/V_F；**本阶段不发布所谓最终最优叠加权重**。

## 8. 球面 Drizzle 的信号与随机算子

设线性源像元 j 的积分量 x_j、未收缩球面面积 Ω_j、收缩后 drop 面积 A_j，与目标 HEALPix p 交叠面积 a_jp，定义 w_jp=a_jp/A_j，则

\[
 N_p=\sum_jw_{jp}\Omega_j,\quad
 S_p=\frac{\sum_j w_{jp}x_j}{N_p},\quad
 R_{pj}=w_{jp}/N_p,\quad C_{\mathrm{sphere}}=R C_{\mathrm{planar}}R^T.
\]

S_p 单位为 [x]/sr；N_p 不等于任意覆盖并集面积；Σa_jp、N_p、support fraction、叶面积也不能随意替换。无数据分母不产生有限假亮度；负值和真实零保留。每帧使用**已求出的本帧 WCS+SIP** 定义 footprint，不使用原始头无 CD 时凭空伪造的几何。

必须区分 drop 分割数值闭合与真实 HEALPix 曲边近似误差。输出相邻叶噪声通常相关；对 mosaic 使用的局部有效 PSF/积分面积、相关核和随机模型，Phase1 需要提供足够证据，不强迫长期写整个协方差巨矩阵。

## 9. 阶段一产品：长期科学对象 ≠ 节点工作文件 ≠ 开发快照

| 对象 | 身份和消费 |
|---|---|
| 每帧 signal HiPS | 正式跨阶段图像输入，真实 BUNIT/坐标/响应/层级 |
| WCS、Gaia XP photo、PSF、noise、有效性、响应及来源 | 正式科学模型/manifest；无需都转成第二张 HiPS 图像 |
| canonical sparse reference SNR controls | **若物理可估计**必须输出可解析 Catalogue HiPS 或有版本的唯一 sidecar，并声明参考量/支持域 |
| per-sample variance/ivar、coverage | Phase1 可选**有来源**的辅助科学输入，保留供统计研究/真实基线；不能替代空间参考 SNR |
| p1_wcs.json、p1_phot.json、p1_snr.json、p1_stack.json | 目前 C++ 阶段内的真实数据依赖；目标是被版本化 typed nodes 和单一 writer 统一，不把工作路径当架构接口 |
| Dev snapshots、perf | 只有 ACSD_DEV_MODE=ON 的**旁路观察**，从不供任何科学消费者读取 |
| per-frame product manifest | frame_id、预期/成功/unsolved/未标定、模型状态、SHA、单位、WCS 来源、下游可用性 |

阶段一辅助 variance/PSF 等，与**阶段二最终只发布单 signal HiPS 图像通道**不矛盾。一个 normalize 调用的每个 frame 状态必须单独记录，不能把部分成功集合不加说明地报告为所有成功。

## 10. 独立科学证据究竟要回答什么

这是**模型的可证伪问题清单**，不是要求 Agent 先跑某批任务才能写代码：

- 真实**没有原始 WCS 头**的帧进入 IPV 后，求解能否产生可读 WCS？成功率/失败来源、与留出 Gaia 或外部独立 WCS 的真实残差如何？Census 不是 plate solve。
- 校准式/共享母版随机相关是否在真实条件和理论 Jacobian 下自洽？缺建模项必须标出，不以单个 MAD 修复全部。
- Gaia XP、PSF/质心/光度比例是否在不同 R_k 和已知 SED 下可辨识？标度应用一次，图像 BUNIT 与参考通量一致。
- source_snr、frame_snr_ref、canonical sparse controls 的数值与状态是否由各自正确估计量产生？已有 HiPS 头数值与 p1_snr.json 相同只证明一致性，不能证明绝对稀疏控制已经存在。
- Drizzle 的常值/总光通/真实曲边/相邻协方差与独立球面真值能否解释？读写器共享公式不等于外部独立验证。

历史 实验/photometric-magnitude、absolute-snr、healpix-polar、m42-realdata 的确有有价值数据，但按其实际输入与方法归因，不把历史局部结果改写成“全阶段已经验证”。

## 11. 不再允许并行起草第二条 Normalize 权威链

本文件是**唯一横向 Normalize 科学正本**。旧 docs/detail/normalize/pipeline.md、science/algorithms/normalize/*、detail/registry/acsd.phase1.* 只说明历史算法/API 或实现细节，不能重新定义被估计量、跨通带禁令、公共 F0、WCS 先验要求、或产品发布政策。实现责任集中到 docs/engineering/architecture/NORMALIZE_PIPELINE.md。发现本模型有数学反例须由前台修改此正本，不交给 Agent 自己立另一套新体系。