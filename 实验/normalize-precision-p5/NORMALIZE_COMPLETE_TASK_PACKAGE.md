# Normalize 完整收敛实施包

本包面向执行 Agent，由前台给出实施范围和收敛要求；科学定义仍以 docs/science、数据/架构合同仍以 docs/engineering 与 docs/detail 为唯一正本。代码起点是 `950c261be` 的交付及其独立复核。本包覆盖阶段一完整实现，不只修复一次数值反例。三阶段工作中的 Mosaic 权重、背景与 Export 算法本轮暂停。

## 1. 阅读顺序与执行方式

先读 AGENTS.md、docs/ACSD_DESIGN.md，再完整阅读：

1. `docs/science/normalize/NORMALIZE_SCIENCE.md`
2. `docs/science/calibration/CALIBRATION.md` 和 `CCD_DEFECT.md`
3. `docs/science/detection/ASTROMETRY.md`、`STAR_DETECTION.md`
4. `docs/science/psf/PSF.md`
5. `docs/science/photometry/PHOTOMETRY.md`
6. `docs/science/noise_snr/NOISE_SNR.md`，特别是球面完成及新增精确恒等式/标度说明
7. `docs/science/drizzle/DRIZZLE.md`
8. `docs/engineering/architecture/NORMALIZE_PIPELINE.md`
9. `docs/engineering/contracts/NORMALIZE_DATA.md`
10. `docs/detail/normalize/contracts.md`，特别是完整实现的数据边界
11. `docs/engineering/governance/NORMALIZE_CONVERGENCE.md`
12. `docs/engineering/architecture/DEV_MODE.md`、`docs/engineering/standards/DIAGNOSTIC_PROBES.md`
13. 对应模块 detail 页、Runtime/AIO/PIPELINE_BLOCK/ATOMIC_PUBLISH/ABI 合同
14. 同目录 `INDEPENDENT_REVIEW_950C261.md` 及 `code/review_950c261.cpp`

然后核对源码、source-set、schemas、IR 与注册表。路径缺失先在文档索引定位同主题正本，不凭旧路径猜规范。历史 Science V2 多阶段包不恢复执行效力。

用户授权直接在 `fujiaze/Astro-CS-Database` 的 `audit/acsd-science-docs-p5` 持续实施、commit、push，不建 PR、不改 main、不 force-push、不覆盖其他方工作。fetch 并确认本包与最新正式补充存在后再改代码。按依赖分批提交，但不要每批停下来要求批准，直到完成全包或存在具体外部阻塞。前台维护科学/架构正本；代码、机器合同和实际 API/实现说明由执行者同步，数学分歧附反例交前台，不自行弱化模型。

## 2. 本轮完成对象

唯一生产 DAG 的八节点：calibration、cosmetic、WCS/IPV、guided source/PSF、photometry、noise、Drizzle、writer。

每个节点具备实际 typed 输入/输出、同帧同版本身份、算法所有权、有效/缺项状态和错误传播。最终发布单帧 HiPS、唯一 frame_science.json、可独立求值的必要模型与完整运行清单。没有 WCS、参考、gain、response 或 PSF 的输入分别按物理能力处理，不能混称执行成功或坏观测。

“完整收敛”要求代码支持正本定义的能力，并用已知输入证明其实现；不要求从不存在的原始信息恢复未知设备参数。某真实帧缺数据而 unavailable 可合法交付；有完整输入却 not_wired、只有字段记录没有实际消费、或者已知数学反例未修复，均属于未完成。

## 3. 依赖批次与具体交付

### A. 修复精度数学与最终信号算子

- 修复 F01 满列秩异方差 GLS 矩阵次序；验证 R 满列秩前提、等价源空间信息、一般保留子空间及通量可辨识性。区别 C 的秩、参数矩阵秩和不可估计的源分量，不盲加 epsilon。
- 修复 F02：从生产最终 signal 的全贡献 N_p 得到 R=w/N，不使用 flux_sum 的中间系数，也不在局部窗口重新归一边缘叶。
- 将 R、h、Z、C 的生成与控制点计算留在对应算法库，scheduler 仅绑定与调用。完整所选叶的源贡献/背景协方差与参考模板截断分别处理。
- 修复 F04：校准/平场/修复/alpha 的源 Poisson 与 background/RN 分量在正确条件下传播；非1标量和空间 alpha 均有生产调用证据，不重复加源噪声。
- F06 控制点的真实中心叶、合法 leaf0、leaf/tile 阶、非重叠 patch 支持及孔洞正确。R 的 CSR 不允许重复贡献被漏算或归约多算。

结果：直接调用算法的解析/独立数值反例正确，且真实 signal 的线性响应与捕获 R 一致。保留修复前反例输出和旧实验，不修改 oracle 去适应错误实现。

### B. 完成 Runtime 帧对象和八节点迁移

- 在既有 RunContext/BlockFrame 实现 typed object publish/acquire/finish consumer，按合同 §7.1 拓展 host 内部承载，不另建全局对象仓。
- ingest 形成冻结 FrameIdentity，input occurrence 是身份的一部分；同 basename、重复文件、净化名称冲突均不串帧。母版/response/PSF/noise/geometry 使用真实内容身份。
- 实现全部合同对象：CalibratedPlane、CleanedPlane、AstrometrySolution、ScienceSourceCatalogue、EffectivePsfModel、PhotometricSolution、PhotoAppliedPlane、ConditionalNoiseModel、DrizzleObservation、SparseReferencePrecision、FrameScientificProduct。
- 逐边同时替换 producer、consumer、descriptor、IR、registry、artifact bridge 与生命周期。不能仅新增 schema_id 或 f_prec_* 向量就宣称 typed port 已消费。
- WCS 消费 calibrated；科学源/PSF及 photometry 消费 cleaned；noise/Drizzle 消费真实 photoapplied 及其 scale/noise models。上游 revision 变化让下游失效重算。
- 普通运行的元数据不靠 p1_*.json 写后读；必要科学 spill、resume cache 与最终 HiPS staging 使用既有 AIO，身份、预算和取消释放完整。legacy 文件仅显式兼容/诊断，不作静默回退。
- p1_session 保留兼容 facade 身份，本轮不以删除它代替上述实际迁移。module_adapters/normalize_precision 不保留自行复制的 PSF/GLS 公式。

结果：真实 normalize 命令走同一 typed DAG；删除/扰动诊断文件不能改变正常科学输入，异步视图不悬空，多帧/取消/异常不串帧或泄漏。

### C. 校准、坏点、天测与空间 PSF

- 校准暗场含/不含 bias 两分支、曝光、平场/单位解释与 floor 状态对齐科学正本；提供已知母版不确定度的 Jacobian/共享身份路径，未知项不设0。
- cosmetic 传递 validity、修复类别和可用算子/未建模声明；饱和/坏点/修复样本实际被下游模型消费。
- 保留唯一 IPV 求解器，逐帧记录 hint/attempt/solved/failed，完整 TAN+SIP 与0/1基适配；不以原始头 census 代替解算，不另建外部 solver。
- 科学源星表保持 Gaia/source identity，IPV 私有候选不冒充科学测光源；科学与验证样本分开。
- PSF 库提供位置求值和像元积分响应，按正式三角重心凸组合基础模型落实空间支持，包含椭率、旋转、子像元相位、总/窗内通量和截断。不能无证据使用圆对称帧中位 PSF 填全帧；无支持点 unavailable。

结果：真实无预置 WCS 的帧由 IPV 处理，独立留出残差可报告；空间 PSF 对注入与留出星有定量误差，模型近似和条件精度分名。

### D. XP、逐帧响应和两种参考定义

- 完成逐 occurrence 的 frame_overrides 绑定，逐帧 masters/gain/response/QE 实际进入计算；相同曲线身份可共享 cache，不同响应独立求值。不根据滤镜字符串拒绝合法数据，也不强套 block 第一帧曲线。
- XP 拟合系数、颜色/支持域、scalar/spatial alpha 与 actually_applied/application_id 一致；确定性尺度应用一次，BUNIT 和 sidecar 来自真实单位而非是否拟合成功。
- 实现 explicit_linear_flux 单位/归一配对和 sed_response 公开积分链。用已归一谱+真实响应+拟合 a*m 得到逐点 F_native，再转换到 applied 模板配对单位；星等输入无零点则明确 unavailable，有完整谱归一输入不得强制要求另一个零点。
- unknown response 诚实 native/relative/unknown，不伪造 absolute；legacy F0/source median 留作诊断，不接 canonical。
- 空间 alpha 下对模板、F_ref、noise 全窗变换；拟合参数共同误差与参考谱系统限制可被表达和求值，有信息才建模，缺信息显式记录。

结果：两种参考在完整输入下真实生产控制点，SED 响应积分用独立已知谱/响应验证，非1测光标度与混合设备/滤镜生产验证存在。

### E. 噪声完成、模型保存和唯一发布

- ConditionalNoiseModel 可求值已知 background/RN/dark/source/calibration/cosmetic 项以及可得共同误差；不把实际天区源亮度方差当作参考源条件，不用一个方差面暗中混合两种条件。
- 修复 F03/F05：按正式局部显式 representation 保存 P/h/C/Z/R 与真实 shape、单位、秩、背景列和模型身份，所有控制点值能从持久化模型独立重算。
- 模型不可辨识、数据缺失、数据损坏分别处理。全局状态/每点状态/摘要继承 PSF/截断/几何/母版/测光等真实能力，不能只列 master 就称其余完整；upper_bound 有严格适用前提。
- frame_science.json reader 无损恢复 reference/response、所有 status/accuracy、SIP 系数、leaf/order、模型引用/单位/hash；完成 schema 与物理一致性校验。发现损坏不仍返回可消费成功；合法 unavailable 仍可保留 signal。
- 新产品在 run-private staging 内整体核验，writer 等待 AIO 后发布并最后写完成 manifest；失败/取消不留下伪完整 sidecar。旧 HiPS/未知 schema 按兼容表读取，不能自动从旧 Catalogue 补 canonical。
- 局部模型不依赖服务器 /testdata 路径，母版/响应/参考身份与公开产品内必要求值内容完整，跨机器可复算。

结果：生产 product→独立 reader→重算 GLS/transfer/summary 完整闭合，离开原始输入目录也可验证必要局部模型。

### F. 球面几何、HiPS 层级与诊断一致性

- 复用共享 HEALPix/NESTED 与 HiPS 排列接口，独立验证 handness、leaf/tile、面边界/极区/RA wrap、SIP、源/叶尺度比、pixfrac。
- 按 DRIZZLE 正本明确 chord 路线的曲边误差，使用真实曲边参数化/细分或等面积积分 oracle 独立量化。既有生产近似只有在其声明域与误差有独立支持时可作为具名近似保留；超出可支持域的几何须改进曲边采样/求积或给出准确能力失败，不强加 epsilon 抹掉失败。生产和 capture 共用实际生效几何。
- 分别验证常值面亮度、分配通量、真实孔径通量、输出协方差及层级 downsample 的物理目标。不要把分割残差称作全部曲边精度，也不把父层当独立观测。
- ACSD_DEV_MODE OFF/ON+none/ON+perf/ON+selected snapshots 实际同源；probe 只读且元数据对应实际内容，快照失败不影响正式结果。低内存 spill、多 worker 不改变数学来源。
- 排查本轮涉及的残留故障环境变量和诊断 I/O 分支，测试专用注入不进入普通发布/开发科学选择。

结果：所支持几何域与近似误差清楚，科学链在不同构建/资源条件下等价；性能/写盘/RSS实测，无新增科学中间文件依赖。

### G. 统一实现说明、回归与完整交付

- source-set/CMake/安装头/配置 schema/示例/port registry/IR 与实际生产行为一致；清理本轮相关虚构输入边、死参数和重复算法，不隐藏既有失败。
- 更新对应 detail/API 实现说明，区分目标、已经实现、真实观测和独立验证。向前台提交科学/工程正本需更新的实际差异，不自行修改正本以追认近似。
- 复核 quantization.s12 和 registry parity 失败的来源与影响。本轮改动涉及的归属/schema/registry 错误应实际修复；与 Normalize 无关的历史失败可保留精确证据，不调整真值/容差凑全绿。
- Linux 生产构建与 Normalize 回归完整执行；Windows 构建/ABI与关键回归在可用节点执行。若当前无 Windows 条件，明确环境阻塞与可复现说明，不冒称双平台完成，也不停止可完成的 Linux/科学工作。

## 4. 独立证据矩阵

以下是每种科学行为的测量目标，不是审批门或固定服务器批次数。执行者选择资源合适的实际命令与帧集，保存输入、版本、随机种子、量纲、样本量、原始输出和偏差。

| 问题 | 独立证据与必要反例 |
|---|---|
| GLS | 现有 F01 异方差反例；满行/满列/一般秩亏；背景 known/constant/plane；模板/背景可辨识与失败；Monte Carlo 通量散度对预测 V_F |
| 信号 R | 在固定几何/有效集合下扰动单个源像元，生产 signal 变化等于 R 列；比较持久化 CSR，不调用同一构造函数作为 oracle |
| 标度/校准 | alpha≠1 与空间场；F/V/SNR缩放；两种暗场约定；flat 非常量；已知 master/共享项与缺项输入 |
| PSF/参考 | 椭圆/旋转/欠采样/子像元相位/空间变化；注入与空间留出；总/窗内通量；SED 与响应有可解析积分的构型 |
| 坐标/面积 | 独立 HEALPix/WCS 读者、原点/SIP/极区/面缝；真实曲边 oracle；非1 pixfrac/不同角尺度/覆盖边缘 |
| 持久化 | 生产模型重算 h/C/Z/V_F/V_B/T/summary；模型 dimensions/hash/unit/leaf/引用/summary 损坏应被识别；不支持 schema 与合法 unavailable |
| Runtime | 同 basename/重复 occurrence、多帧、revision失效、取消/异常/消费者释放；诊断文件扰动与有效 cache恢复；低预算 spill与普通内存路径 |
| 真实输入 | 从冻结运行清单枚举；无预置WCS的31帧逐帧记录实际尝试和原因；XP标度生效帧；相同设备重复帧与不同设备/通带，未知响应对照 |
| 构建/诊断 | Release同优化的OFF/ON四模式、串行独立目录、多worker及低内存；科学差异与run/time元数据分开计数 |
| 图像保真 | 单帧全景、星点/边角/低表面亮度结构、层级/面缝及覆盖边缘；若用HST则作为已知信号模板，不作原生噪声真值 |

原始真实帧有失败可以报告；不能只挑一帧成功后称31/31或完整设备域已支持。资源较大的实验可用有身份的切片/ROI，说明被覆盖的域；不得用ROI结果冒称全帧/全量。

## 5. 完成状态与报告

本轮目标是 A–G 持续完成。报告逐项给出：实现位置、实际 typed producer/consumer、支持域/近似、独立证据路径、运行/未运行、缺真实输入/环境/剩余代码。不要把阶段一仍有 not_wired 的核心能力合并成“少量限制”后宣布完成。

最终提交一个完整事实报告和证据清单，包含：

- 全部输入 occurrence 的索引与 source/master/config/build 哈希，Gaia/response/SED 身份；
- 每个八节点的正式调用、对象类型/port、实际消费证据；
- 所有独立 oracle 与反例脚本、失败记录及修复后的原始读数；
- 真实产品目录、可独立重算的 frame_science/models、全景与局部预览；
- 构建/运行复现命令、资源/诊断对照、软件 SHA；
- 旧产物兼容、无参考/无gain/无PSF/未知响应和损坏输入的真实行为；
- 精确列出设计已实现但真实输入受限、环境未验证和代码仍未完成，三者不能混称；
- 本轮未修改 Mosaic 默认权重/背景/Export 的事实与接口影响。

原始 FITS、母版、HST 大文件不提交 Git；提交脚本、输入索引、小型必要图表、误差读数与事实报告。实验图表/预览实际保存，不提交不存在的链接。过程中不清除历史失败证据。

基础参考精度修复前的 SNR=19.9989 和模型文件保留历史身份；修复后重算并记录差异，不能沿用旧数值宣称新算法被验证。最终状态由前台独立审查，不由测试数量、exit=0 或 Agent 自称完成替代。
