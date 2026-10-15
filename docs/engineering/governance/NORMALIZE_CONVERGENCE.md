# ACSD 阶段一设计收敛：文档权威、实际源链与已解决冲突

> **文档收敛登记，不是实验运行清单和审批门禁。** 2026-10-14 前台科学架构统一裁决；它只说明哪份资料控制什么、为什么旧叙述不可再用于改码。上游 docs/ACSD_DESIGN.md，科学主链 science/normalize/NORMALIZE_SCIENCE.md，工程主链 engineering/architecture/NORMALIZE_PIPELINE.md。实际代码静态审查版本 6a62ea10eb7f8ad8cfbdee16fa39afc4d27baf0e；之后真正代码状态以源码及独立实验为准。

## 1. 我们不再同时维护六份“完整 Normalize 科学设计”

长期文档权威按**物理问题与软件责任唯一归属**：

| 层级 | 文件 | 唯一责任 | 与别的文件冲突时 |
|---|---|---|---|
| 用户目标 | docs/ACSD_DESIGN.md | 三命令、产品边界、用户自由跨设备合并、总架构 | 用户目标/阶段结果以此为准 |
| 全项目科学 | science/unified/UNIFIED_SCIENCE_MODEL.md、DATA_SEMANTICS.md、SCIENCE_SCOPE.md | 观测模型、物理量纲、整体可辨识性 | 不单独复写每阶段节点图 |
| **阶段一科学横向主链** | **science/normalize/NORMALIZE_SCIENCE.md** | IPV→Gaia sources/PSF→XP→SNR→Drizzle 的唯一估计量/输入输出语义 | 覆盖旧 Normalize 流程宣称 |
| 科学专题 | science/calibration、detection/ASTROMETRY、STAR_DETECTION、psf/PSF、photometry/PHOTOMETRY、noise_snr/NOISE_SNR、drizzle/DRIZZLE | 各自数学公式、误差/适用域 | 不再次独立定义 Normalize 全链 |
| **阶段一工程横向主链** | **engineering/architecture/NORMALIZE_PIPELINE.md** | 当前真实 C++ 节点/数据依赖、Typed 目标、模块责任 | 代码现状必须依真正调用/实验更新 |
| 工程专册 | engineering/architecture/DEV_MODE.md、standards/DIAGNOSTIC_PROBES.md、contracts/*、api/* | 编译调试、I/O/API/ABI/运行边界 | 不修改数学模型 |
| 旧细节/接口 | detail/normalize/pipeline.md、detail/registry/acsd.phase1.*、science/algorithms/normalize/* | 旧接口、源码锚及局部历史推导 | 与上述冲突则作为历史而非目标 |
| 实验记录 | 实验/photometric-magnitude、absolute-snr、healpix-polar、m42-realdata、science-v2-impl | 给定代码/输入下的事实、反例和未运行项 | 不能替代目标设计，也不能被“运行成功”升级为普适结论 |

旧 detail/normalize/pipeline.md 已收敛成单页导航/现状摘要，旧长文仍在 Git 历史；不用为了保留过时定义而再开新文档。旧科学研究论文与原始实验数据保留，不因架构收敛自动删除或篡改。

## 2. 实际科学冲突的裁决表（不是留给 Agent 决定）

| 编号 | 以前矛盾的说法 | 已确定的唯一答案 | 当前代码与科学验证状态 |
|---|---|---|---|
| N01 | “31 帧缺 WCS → 需要外部解算/不能进阶段一” | 原始 FITS 头没有完整 WCS 与 ACSD IPV 解析无直接等价关系；先使用阶段一自带 IPV 实际逐帧解算 | WCS solver 源码和正式节点真实存在；历史 census **没有运行 IPV**，这 31 帧是否会解出来是未知，而不是失败事实 |
| N02 | “星点检测必须在 plate solve 前”与“Gaia 引导检测在 plate solve 后” | IPV 内部盲检 solver_detections 在解算过程中；正式 Gaia-guided science_sources 在解算后 | p1_op_wcs 和 p1_op_star_psf_impl 已分开，旧文件混称两个对象 |
| N03 | “p1_session.cpp 已是完整 Normalize”与“mod adapters 才有 WCS/XP/Drizzle” | 用户唯一全链经 CLI 的 P1NodeModule；p1_session 只为校准/坏点局部底座或兼容 | 源码 tail 已明确后六域 unavailable；代码组织待收敛 |
| N04 | “跨滤镜不得合并”与“用户自由选择” | 用户可以自由在 Mosaic 组合；Normalize 可按母版/响应分组，每帧真实 R_k 定标，不准拿字符串限制合法观测 | Phase2 coverage 的 filter mismatch 已移除；Phase1 PHOT_PASSBAND_IDENTITY_INCONSISTENT 仍拒同组混响应且须按物理模型重构 |
| N05 | “母版方差固定不传播”与“必需完整误差传递” | 科学目标是雅可比及共享母版相关模型；当前未建模项状态 explicit unmodelled，不能冒充零或完整绝对 SNR | science/calibration §5.7 已修订；生产算法未据此修改 |
| N06 | “光度系数 α 已应用”与“所有产品还是原始 ADU/sr” | 线性标度施加一次；根据真实数值单位/response 写 BUNIT/provenance。Relative scale 不等同 SI flux | P1 photoapplied 流存在，实际各分支标度与 writer 标签要对照；旧数值量级检查仅为尺度自洽 |
| N07 | “公共 F0=一批星中位 ADU”与“各帧同 reference SED 信息可比” | 共同参考的是 SED/颜色/光度系统；给每帧匹配其 R_k 与 applied unit 的 F_ref,k，不能仅依内部 star median | 旧 p1_op_noise/snr_frame_science.cpp 的 group-common F0 有真实生产路径；canonical 模型仍待接线 |
| N08 | “SNR Catalogue 已有所以稀疏控制点已完成” | legacy snr_psf=(A−B)/mad 与以局部 PSF/covariance 定义的 F_ref/√V_F 不同；要一个 canonical producer/schema/consumer | AIO 有 legacy Catalogue writer；真正绝对空间控制点及 P2 dense consumer 未闭合 |
| N09 | “PSF FWHM 就代表参考信息量” | 必须使用与实际观测向量同单位的归一响应 h 和噪声/背景 nuisance；FWHM 只是参数 | 既有 PSF 模型可复用，V_F 精度算子的组合尚待证明 |
| N10 | “所有内部 JSON 均是可用开发模式关掉的中间产物” | 同阶段实际依赖必须始终存在，开发 snapshot 只是只读旁路；工程目标 typed ports + 正式科学 spill/缓存与 dev 数据隔离 | typed DAG 边已接多处，尚有 p1_wcs/phot/snr/stack 等文件供实际消费者读 |
| N11 | “三阶段 exit=0 和 tile 一致即证明阶段一科学已通” | E2E 证明有限功能和输入域，不能证明独立 astrometry/photometry/SNR/drizzle 物理准确性 | 8 帧 Hα 等实跑、ON/OFF tile 一致有价值，但 canonical SNR/曲边/Gaia XP 多设备尚无完整独立证明 |
| N12 | “Phase2 单信号所以 Phase1 应删去一切辅助统计数据” | Phase1 可以发布必要 PSF/noise/ivar 与 sparse controls；Phase2 最终仅一个 signal 图像通道，二者并不矛盾 | 当前 Phase1 多层辅助产品存在；是否有科学依据按具体模型/单位判断 |

以上每条中，“科学裁决”已由前台完成，不交 Agent 再重新投票；“实现状态未知/未闭合”是**真实现状**，并非新的第二设计方案。

## 3. 科学可信性应该怎么表述

对任一子模块用这五种**事实**标记，不把它们当审批门：
1. DESIGNED：公式及被估计量定义明确；
2. CODE_EXISTS：项目里确实存在函数；
3. PROD_REACHED：真实 acsd normalize 命令调用到了该函数；
4. PRODUCT_OBSERVED：以真实输入获得可独立读取的产品和元数据；
5. INDEPENDENTLY_SUPPORTED：独立参考/解析/前向随机实验支持定量正确性与适用域。

例如 IPV 目前至少为 CODE_EXISTS + PRODUCER_WIRED；raw-no-WCS 31 帧的 PROD_REACHED/PRODUCT_OBSERVED 仍未在提交证据中体现。头部普查仅是输入资料，不在 IPV 成功与失败这两个状态中。frame_snr HiPS 头和自身 p1_snr.json 逐位相同属于 PRODUCT_OBSERVED，不是 sparse V_F 的 INDEPENDENTLY_SUPPORTED。

历史负例不删除，不让一个真实数据 smoke 的退出码覆盖理论反例。没有必要为了这五种标签再新增一个自动检查“门禁系统”。

### 3.1 已上传证据对 Normalize 各段真正支持到哪里

**这是对当前 GitHub 中 Agent 实跑记录/源码的独立语义复核，不是前台重跑 /testdata 得到的新实验。** 来源：实验/science-v2-impl/docs/E2E_RUN.md、SCIENCE_V2_FACT_REPORT.md、实验/engineering-evidence/DEV-MODE-01/README.md 与实际 module_adapters.cpp。数值只用于明确证据强弱，不构成新的“运行计划”。

| 环节 | 已有事实 | 尚未被这些事实证明 |
|---|---|---|
| 校准与坏点 | 既有节点、真实母版/帧 E2E，选定光帧校准图统计；历史修复真实参与 | 共享 master 误差/相关噪声全传播、任意设备数据域 |
| 天体测量 IPV | 生产 p1_op_wcs 真调用 ipv；历史 DEV-MODE-01 记录一帧 wcs_source=ipv、SIP samples 几何复核 | **原始头无 WCS 的 31 帧实际 IPV 求解结果**；外部独立 Gaia held-out 全域误差及真实成功率 |
| 科学源/PSF | 生产 p1_sources/p1_psf 对象和可调用 guided detector；一些局部源结果存在 | 所有帧 source_id 绑定/PSF 实际积分归一/独立注入回收质量 |
| Gaia XP photometry | 真实节点产出 p1_phot、k_photo 与条件 photometric provenance；历史 P1 M42 对某类帧有残差读数 | 跨设备/通带已做可靠响应同一化、未知 SED 的无偏性、每条路径 BUNIT 与 photo scale 完全一致 |
| noise/frame reference SNR | 8 帧 Hα E2E 中一帧 ACSD_FRAME_SNR 与 p1_snr.json 同为 14.042478467183946 | 这个数的**绝对准确性**；实跑已标记 upper_bound_no_gain，缺源泊松项 |
| canonical sparse controls | 旧 Catalogue writer 存在，现有 HioSnrControlPoint 为 (A−B)/mad | **F_ref,k/√V_F 的真实空间控制点**；P1 canonical producer → P2 consumer，现有产物未闭合 |
| sphere Drizzle | 已观察 8 帧逐帧 HiPS、原始仪器 ADU/球面面积量级自洽；对照 58.46 ADU/px 和 Ω≈2.0799e−11 sr 导出 2.81e12 ADU/sr 的量级 | HEALPix 真曲边/所有 WCS 畸变/相关噪声等独立误差已正确，量级正确不等于完整守恒与 PSF 无损 |
| 产品/诊断 | 双编译构建的 1496 公共科学文件逐字节一致的 Agent 记录；阶段一至阶段三选定两帧 E2E exit=0 | 完整阶段一科学对象在所有帧具备，或多设备/无预置 WCS/缺 gain 的真实适用域普遍成立 |

因此，**阶段一尚未通过完整独立科学链验证**是准确表述；**阶段一没有天测解算器**、**31 帧确定无法求解**则是错误表述。下一轮任何报告都应能把真实 solver 的输出与简单头部 census 分开，且把已定义模型和已得到物理独立证据分开。

## 4. 当前代码收敛的方向（只有设计规格，暂不编写运行任务）

- 恢复统一 FrameIdentity/CalibratedPlane/AstrometrySolution/ScienceSourceCatalogue/EffectivePsfModel/PhotometricSolution/ConditionalNoiseModel/SparseReferencePrecision/DrizzleModel 的物理关系，复用已有实现与稳定数据指纹。
- 保持 IPV 作为 WCS 计算唯一求解器；明确 solved vs explicit_config、solver_detections vs science_sources；p1_session 校准局部身份而非完整产品。
- 同一模型目标只计算一次，逐帧 R_k 的 XP response/scale 必须与 noise F_ref,k 的单位配对；canonical sparse controls 不再用 legacy 数据无解释冒名。
- 类型/数据所有权归对应算法模块；module_adapters.cpp 主要保留 DAG adapters 和生命周期，减少在 16000 行编排代码里逐处定义数学。
- P1 writer 一处声明和发布长期科学 HiPS/模型，阶段内 work cache 显式管理；普通 DEV snapshot 不提供科学数据依赖。
- 阶段二权重算法的最终选择继续暂缓；阶段一先保证提供可解释且有版本的输入科学模型。

这里不提供具体运行命令、测试批次、阈值打卡或 Agent 自主科研项目列表。代码由 Agent 按前台科学和工程正本实施；如果算法数学真的被独立反例否定，返回证据给前台修正唯一主文档。

## 5. 下一次前台更新职责

前台负责以上**三份主文档的数学/代码归属**，同步顶层 docs/ACSD_DESIGN.md、science README、engineering README、DOCUMENT_INDEX.yaml 和 AGENTS.md 的指向，旧 task package/第二轮运行提示暂不拥有设计权限。Agent 可以提交真实代码/源调用变化清单和事实报告，不能编辑上述主文档重新创立定义。

本收敛令仍未修改 C++ 与真实服务器的数据，故不作“阶段一完成”的表述。

## 6. 接口裁决与实施边界

阶段一完整接口已由 NORMALIZE_DATA 与 detail/normalize/contracts 定稿：参考缺失不发明中位 F0；唯一 sidecar 为 frame_science.json；noise 平面生产、Drizzle 后由 noise 库完成球面 precision；已应用面和原生 gain 坐标分开；Runtime typed payload 与正式 cache 逐边迁移；每帧状态明确。

该规格允许执行者推进完整阶段一代码，不再限于只做结构整理。此前第 4、5 节描述的“接口待细化”工作已由这两份合同承接；多阶段执行包保持历史身份。代码、source set、port registry、AIO reader/writer、老产品 adapter 的更新由执行者完成；本次前台修改只定稿文档、schema 和形状示例，没有在 /testdata 重跑科学实验。

新增裁决：Drizzle 的无条件面积平均简式只在 Ω/A 共同常数时成立；数据合同必须保留实际 Ω/A。外部 WCS 的 CRPIX 不强制为图像中心；Gaia 的参考历元与 ICRS 的坐标系身份分开。相关核必须 PSD，球面 R 的秩亏须按可估计子空间处理，不能用任意地板伪造精度。

文档定稿检查：规范索引 157 项无重复/悬空，新增本地 Markdown 链接可解析；新 frame-science schema 自身可加载，available/unavailable 两种夹具通过，9 种形状负例被拒绝。一般面积归一公式、正定相关核与秩亏线性传递做了局部数值核对。以上只验证文档/接口形状及局部数学，不是 production C++、真实 IPV 成功率或阶段一科学端到端验证。

## 7. 实施后的事实复核与继续收敛

上述早期实现状态保留其版本语境，不能替代后续代码事实。`950c261be` 交付已增加生产球面精度调用、Drizzle 几何捕获与 sidecar；其真实单帧记录显示原始头无 WCS 时 IPV 实际求解成功，但该帧没有施加测光。独立复核见 `实验/normalize-precision-p5/INDEPENDENT_REVIEW_950C261.md`，其中异方差反例直接编译生产源得到 V_F=0.896928，而解析真值2.647059，已有等方差单元测试未暴露错误。前台未独立重跑计算节点原始帧。

后续收敛维持既有八节点与 Runtime typed payload 目标。新增正式补充将 GLS 的矩阵次序/可估计子空间、最终 signal 的 R、原生 gain/alpha、PSF 积分与空间求值以及读取/发布完整性写回各自正本；不让实验包创立第二条科学权威链。`p1_session` 保留兼容 facade，不以删兼容文件替代数据链迁移。

完整实施要求由上述正式正本支撑，任务范围见 `实验/normalize-precision-p5/NORMALIZE_COMPLETE_TASK_PACKAGE.md`。一帧信号输出、新对象可写与完整条件精度已验证分别报告；缺真实设备信息的 approximate 与代码仍 not_wired 也分别报告。阶段二算法切换仍暂停。

## 8. 第一轮 A/D 后的三项科学裁决（2026-10-15）

本次复核的分支：审查起点 12204600edbad8e23ba9e8fe4cbc8427efee0bf9，Agent 最新 d0b61a297f140935e63df24caa3a91965ebf44c3，共 5 个新提交。直接审查源代码、真值程序、evidence log 与 ROUND1_FACT_REPORT.md；前台**没有**再次执行用户服务器的真实 FITS，所以两次生产一致性、20–26 分钟的耗时和读数是 Agent 已报告且有仓内证据的观察，不是本轮重新独立测得的结果。

| 裁决 | 单一目标与原因 | 本轮真实状态 |
|---|---|---|
| **N13 允许因子协方差** | 正式科学数据若假设平面条件 covariance 为 D=diag(v)，CSR R+v 是派生 C=RDRᵀ 的精确表示；不要求每窗把 nl×nl JSON 落盘。明确旧 v1 factor 的源方差与球面派生 C 单位曾错位，新 factor v2 需分别声明逻辑/物理 shape、unit、rank 与模型缺项 | 生产 v1 factor 已写 64 控制，reader 可部分识别；语义/单位还未按新合同修正 |
| **N14 秩亏求解不靠缩小真实观测域** | 用 Q=R√D 的行空间/零空间正交投影计算 GLS Fisher 信息；选择验证过的 rank-revealing QR/SVD、保留真正选定球面叶的全部贡献，不能故意削掉噪声来换 O(m³) 的速度；背景基冗余不能自动判 F 不可辨识 | 现行手写 Jacobi 保留子空间约 20–26 分钟/帧（Agent 报告）；数学上可改算法，不意味着已有高性能代码 |
| **N15 PSF 归一与像元积分先于宣称绝对 SNR** | Moffat4 未归一原函数值不等于单位总通量像元积分 P；正确 P_j 先连续归一再像元求积，保留真实子像元相位/有限窗 f_out 和通量估计器身份；F_ref 和条件源 Poisson 要同一口径 | **新发现的生产数值口径缺陷**；d0b61a297 的 F_ref=1000/控制 SNR=141.47/帧 SNR=153.30 只算该代码下历史可复现数据，不是经 PSF 修复的物理真值 |

其它已证事实：GLS full-rank 异方差反例的新数值 V_F=2.6470588235294117 与理论吻合，源码已经把 B 的矩阵次序改正并处理 H^+²；源 R 的完整叶 N_p 和 sidecar read/write 也有实际修复。不能因此自动推断直接真实 signal 对 R 每列的扰动检查已做、source-space 标度/PSF 物理匹配全面正确、31 帧 IPV 独立留出都通过。原用户希望的是正确且简洁的科学链，而不是进一步复制 N13–N15 到十份旧 registry。

**追加需整理的实际源码问题**：sparse_reference.h 和 sparse_reference.cpp 内部分注释仍有 H^+ 与 H^+²、B 顺序互相矛盾的旧口径；normalize_precision.cpp 对 const PrecisionFrameRequest 进行 const_cast 写回窗口半径；surface factor values 的单位与 derived surface covariance 单位混标；帧支持面积跨 patch 未去重。这些由工程主链中唯一记录并交 Agent 真正修，不因此重新定义新的数学路线。

下一步不进入 Mosaic/Export。Agent 依据已更新 science/psf/PSF.md、science/noise_snr/NOISE_SNR.md、engineering/contracts/NORMALIZE_DATA.md、detail/normalize/contracts.md 和 architecture/NORMALIZE_PIPELINE.md 实现正确的 PSF→F_ref/V_F、因子载体、秩揭示求解，再推进 typed Runtime/SED reference/spatial PSF/校准相关误差。源程序依旧不需要在所有平台得到任意数值 PASS 才准研发；但不能把未建模、非物理模板和单位错位的数值称“conditional complete”。

