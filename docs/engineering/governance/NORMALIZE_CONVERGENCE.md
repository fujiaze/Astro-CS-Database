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