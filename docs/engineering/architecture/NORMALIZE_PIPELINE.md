# Normalize 生产链工程正本：真实接线、Typed 数据合同与改码责任

## 1. 只有一个完整的 Normalize DAG，另一个 Phase1 session 不是全链

项目实际用户入口：lib/infrastructure/cli/commands.cpp 的 run_phase1_block → build_pipeline_ir → scheduler P1NodeModule。对应 lib/infrastructure/scheduler/src/module_adapters.cpp 按 P1NodeOp 调度：

| 真实节点 | 生产 C++ 实现 | 当前实物输入/输出 |
|---|---|---|
| calibration | p1_op_calibrate、calibration 库 | lights + masters → calibrated |
| cosmetic | p1_op_cosmetic、cosmetic 库 | calibrated/masters → cleaned/validity |
| WCS/IPV | p1_op_wcs → ipv_solve_from_memory_with_callback_d → lib/algorithms/platesolve/cpp/ipv | calibrated + Gaia + hint → 每帧 p1_wcs |
| guided detection + PSF | p1_op_star_psf_impl、star detector/PSF | cleaned + **已解 WCS** → p1_sources / p1_psf |
| Gaia XP photometry | p1_op_photometry、photometry | calibrated + sources + WCS + 每帧响应 R_k → photoapplied + p1_phot |
| noise/reference SNR | p1_op_noise、noise_snr | cleaned + sources + photometric provenance → p1_snr / noise |
| sphere Drizzle | p1_op_drizzle → hp_drizzle_run_phase1_hips | 已缩放面 + WCS/SIP + PSF/噪声 → frame HiPS |
| writer/product manifest | p1_op_writer | frame HiPS/状态/模型 → p1_final、p1_products |

这是生产**已存在的节点/入口**，不是证明每条数学模型已完整实现。lib/phase1_session/p1_session.cpp 只实现 io_read、calibrate、cosmetic、io_write，其源码明确承认 WCS/photometry/noise/drizzle 等 unavailable。应把它归为校准子会话或兼容组件；不能把它与上表并列称“另一套完整 Normalize”。

## 2. 数据依赖边是契约，不是临时文件出现的巧合

最小依赖图：

1. calibrated → IPV WCS；calibrated → cosmetic；
2. solved WCS + cleaned → catalog-guided science_sources / spatial PSF；
3. science_sources + solved WCS + calibrated + per-frame R_k → photometric solution 和 applied pixels；
4. valid pixels + PSF + photometric solution + covariance inputs → conditional noise + canonical sparse reference SNR；
5. WCS/SIP + applied pixels + noise/PSF/support → Drizzle；
6. per-frame outputs → 单一科学 writer/manifest。

现有 module_adapters.cpp 里的 p1_*_descriptor 已加入 WCS→sources/photometry/drizzle、photprov→noise/drizzle、snr→drizzle 等 typed 依赖，这是**正确的已有修复，应保留**。但其真实数据仍经 output_dir 下的 p1_wcs.json、p1_phot.json、p1_snr.json、p1_sources.json、p1_stack.json 反复落盘/读取。调度排序正确≠没有旧内容/错误路径/跨构建差异。

**目标**：DAG typed ports + frame_id + version + unit + coordinate + status + provenance 管理同阶段数据对象，优先复用已有 P1Image、PipelineFrame、ModuleDescriptor、AIO 和内存工作单元。数据真正需要科学 spill 或恢复缓存时，给它独立、原子、带版本的正式缓存合同，OFF/ON 一致；纯 SnapshotProbe 仅旁路保存，不能让关闭开发编译开关改变科学消费者的来源。完整阶段产品由一处 writer 发布，不能把工作区 JSON 当跨阶段稳定公开 API。

## 3. 必须存在的 Typed 对象（逻辑接口，名称不代表已有 C++ ABI）

| 科学对象 | 至少包含 | 生产 → 消费 |
|---|---|---|
| FrameIdentity | frame_id、真实源路径/哈希、设备/滤镜、时间/曝光、母版适用组、观测响应 | ingest → 全链 |
| CalibratedPlane | corrected pixels、线性单位、dtype、有效性/修复掩膜、母版 IDs 与噪声建模状态 | calibration/cosmetic → IPV/photometry/noise/Drizzle |
| AstrometrySolution | solved/unsolved、ipv/explicit_config 来源、初始 hint 与 Gaia ID、CRPIX/CD/CRVAL/TAN+SIP、真实配对/质量/像素原点 | 唯一 IPV/WCS adapter → guided detection、photometry、Drizzle |
| SolverDetections | IPV 自用盲检候选及数目/质量，绝非 scientific source catalogue | IPV 内部 |
| ScienceSourceCatalogue | Gaia ID、frame/source ID、full WCS 投影和质心、测量 flux/误差、饱和/拥挤/边缘选择状态 | guided detector → PSF、Gaia XP、noise |
| EffectivePsfModel | u→normalized footprint/kernel、有效空间/通量采样积分、适用域/质量 | PSF → Gaia XP、reference SNR、Drizzle |
| PhotometricSolution | 逐帧 R_k/QE、reference SED/参考通带约定、k_photo、spatial model、actually_applied、线性单位、颜色/误差状态 | Gaia XP → noise、Drizzle、writer |
| ConditionalNoiseModel | background、read/shot/dark、calibration masters、correlation model、gain 实际来源、近似与缺项 | calibration/noise → reference SNR、Drizzle |
| SparseReferencePrecision | frame_id、ICRS u、F_ref,k+unit、V_F、SNR_ref、PSF/noise/nuisance/validity/support/caliber | 唯一 P1 canonical producer → Catalogue/sidecar writer → P2 |
| DrizzleModel | actual WCS+SIP、nside/pixfrac、overlap/area conventions、effective PSF、noise correlation 状态 | sphere drizzle → writer |
| Phase1ScientificProduct | signal HiPS、逐帧模型/provenance、canonical controls 或有原因的 unavailable、可用辅助数据、manifest | writer → P2 |

上表是**一个类型系统与单份源数据**，不是要再额外造十套 JSON 文档。优先复用项目既有对象/头文件；科学量不同则新增有名字段，不让同名 SNR/weight 表示不同统计对象。所有对象有 frame_id、版本、单位、sky/pixel coordinate、validity 和来源身份。

## 4. 不许再重复的六项错位

### 4.1 原始 FITS 没有 WCS ≠ Normalize 缺解算能力

实验/science-v2-impl/code/wcs_census.py 只检查原始文件 CTYPE/CRVAL/CRPIX/CD 是否齐备，应改名为 preexisting_header_wcs_present；它**没有**执行 IPV。被列出的 M42 30 帧和 GalaxyCenter 1 帧不能据此判失败、也不能先行安排外部 solver 取代项目自带求解。真实 P1 的 p1_op_wcs 支持用 OBJCTRA/OBJCTDEC、FOCALLEN/XPIXSZ、config 或已解邻帧初始化，求解时从像元自行选星并使用 Gaia。产物有 wcs_source=ipv 或 explicit_config，两者不可冒充。未解时必须记录自身失败原因，不能凭文件头猜是否可解。

### 4.2 WCS 解算用的内部候选与 Gaia 引导科学星表不相同

IPV 的 solver_detections 是内部求解候选，不能提前依赖已经 solved 的 WCS；星表引导的 ScienceSourceCatalogue 必须在 WCS 成功之后，使用完整 SIP。某些旧文档写“天测在星点检测后”是指 IPV 内部候选，不是另设一个与 Gaia 引导 science_sources 循环依赖的调度节点。

### 4.3 block 是校准/光度复用边界，不是用户组合限制

ACSD_DESIGN 的输入示例为多个 block，每块可以共享一套母版与响应条件。代码 p1_op_photometry 的 PASSBAND-IDENTITY-GATE-01 现在对同 block 不同 filter_key 直接标失败；但如果不加区分移除检查，随意用第一帧 R_k 拟合其它帧同样错误。

**目标**：逐帧绑定响应 R_k/QE_k、母版和 Gaia XP fit；内部相同响应可以共享模型/缓存，响应不同必须独立计算或自动内部子组。一个命令与未来 Mosaic 均允许不同设备/滤镜；不能用型号字符串做跨阶段输入过滤。未知 response 明确 relative_only/unknown 而不是默认赋予一个虚构绝对通带。

### 4.4 组内 F0 ≠ 参考 SED 下的 F_ref,k

module_adapters.cpp 的 p1_op_noise 与 snr_frame_science.cpp 目前要求 block-common F0，缺值可按输入源流量中位数退化；这个 F0 依赖样本选取，不是不同设备/通带真正可比的独立参考恒星物理标准。科学目标是**同一参考 SED/颜色/光度系统**，各帧在 R_k 与应用后的 photo scale 下得到单位自洽的 F_ref,k。只有统一了数值通量单位时可用共同数值 F_ref。当前基于 F0 的 frame SNR 应如实标注其参考来源。

### 4.5 legacy snr_psf ≠ canonical sparse reference SNR

aio_snr_model.h 的 HioSnrControlPoint.snr_psf=(A−B)/mad；hp_drizzle_api.cpp 直接将其写到 AioHipsSnrPoint.snr。这只能证明 Legacy Catalogue 能存点，不能证明已产生固定参考 F_ref/√V_F 的条件信息量。新 canonical producer 要真正计算 PSF/噪声/背景 nuisance 下的 V_F、参考 F_ref,k、位置/support/估计状态，并使唯一落盘载体可供 P2 读取；**不得改字段名代替数学实现**。

### 4.6 校准母版误差不能“同一项目两份相反口径”

calibration 科学分册旧 §5.7 记载当前还未传播 master 方差，旧 detail/normalize/pipeline.md 却把完整共享 master covariance 当作已存在。**设计裁决**：目标随机模型包含校准方差的 Jacobian 与共享母版相关项；当前没计算时记录 unmodelled，不把它设为零也不声称生产绝对 SNR 已经闭合。误差只在 Noise/Calibration 的单一模型里求一次，不由每个子模块各加一次。

## 5. 正确的帧状态与产品完整性

帧状态各自分名：WCS solved/unsolved，响应 known/unknown，photometry applied/relative_only/failed，PSF usable/limited，noise complete/approx/upper_bound_no_gain，sparse controls available/unavailable，Drizzle geometry valid/invalid。它们是**数据可解释性**而不是阻挡研发的审批门禁。

未解 WCS 的帧不能凭空球面化，但其它合法帧应保留自身结果；命令级 manifest 反映 N 个预期与实际有效产品并记录不完整，不准静默说 N/N 成功。相对测光与未知色项需要如实标注产品物理用途；输入正常但设备异同不是自动错误。

Stage1 signal HiPS、canonical controls、WCS/phot/PSF/noise provenance 是跨阶段产品；p1_wcs.json 等当前工作文件今后归 typed producer/consumer 或显式 cache；debug/perf 只在开发构建旁路观测。Stage2 最终只发布单 signal 科学图像的决定不要求 Stage1 丢掉合理必要的辅助物理信息。

## 6. C++ 模块收敛的明确边界

- **不重造算法**：保持 IPV solver、star detection、Gaia XP、noise、sphere Drizzle 和统一 AIO 的成熟实现。调度层不能成为新星点匹配/求解器。
- **统一几何**：只有 IPV 拥有天体测量求解；wcs_tan 和 photometry/wcs_transform 作为投影消费适配器，不能在 CRPIX/SIP 原点上产生不同科学规范。
- **逐步拆开 module_adapters.cpp**：把其中超长的 p1_op_* 物理计算与取数职责迁往已有对应 algorithm/phase1 integration 模块；留下 ModuleDescriptor、typed-port 连接、生命周期/错误传播。不要为了复制源码行号把旧隐式工作区接口当永恒规范。
- **保留源数据血缘**：同一个 FrameContext/模型版本贯穿 WCS、source/PSF、photo、noise、Drizzle、writer。不同 run 的旧文件不能被当作当前上游产物。
- **避免第二套完整 session**：p1_session.cpp 的 calibration-only 事实必须在工程入口和接口声明中明确，或仅提供兼容/子处理，不再给它完整 Normalize 的产品身份。
- **明确现实资源限制**：大图像可有 spill，内存/线程配额由 Runtime 控制；科学 spill 与实验快照分离，均不依赖是否开发模式 ON/OFF。开发快照格式及可选 probe_ID 依 DIAGNOSTIC_PROBES.md。

Agent 的工作是按这些设计修真实代码、报告与源代码不符之处，**不是**给不同子模块独立重新定义光度系统、WCS 或 SNR。

## 7. 科学证据的准确归属

必须区别 “接口存在”“真实 production 命中”“能写产品”“独立真值成立”。历史 agent 报 1496 tiles ON/OFF 相同只说明那些输入下构建等价；HiPS 头 ACSD_FRAME_SNR 与 p1_snr.json 完全相同只说明字段传递一致；P1 8 帧 HiPS exit=0 不等于全部光度绝对、SNR 稀疏、几何真实性验证完成。旧 header WCS census 既不是 IPV 成功率也不是失败率。未来技术报告需要用这些标签归因，但**不是新增机械任务门禁**。

## 8. 已定的接口与持久化规格

上述逻辑对象现在由 [NORMALIZE_DATA.md](../contracts/NORMALIZE_DATA.md) 定义字段、状态、端口、所有权、八节点完成边界、唯一 frame_science.json 与兼容规则；[contracts.md](../../detail/normalize/contracts.md) 给出待实现 C++ 签名、现有头文件落点、配置和逐边迁移。

目标取数统一为 cleaned→photometry→photoapplied→noise/Drizzle；当前 descriptor 的 calibrated 输入不能继续被当作 photoapplied 的物理单位。WCS 保留 calibrated 输入。noise 先产 planar model，Drizzle 以真实 R 完成 sphere model 后调用 noise 库产 sparse_reference/frame_reference，writer 消费它们，避免循环依赖。PSF 从零消费者工作文件变成显式模型输入。当前代码仍未完成这些接口；机器注册表和 IR 在实施时与真实取数同步改，不把本目标表冒称当前已接线。

## 9. A/D 第一轮独立复核与后续模块责任（2026-10-15）

已读取审核 12204600edbad8e23ba9e8fe4cbc8427efee0bf9 → d0b61a297f140935e63df24caa3a91965ebf44c3 的 5 次提交和 实验/normalize-precision-p5/ROUND1_FACT_REPORT.md。本次**只核对远端源码与已有实验日志，没有运行远端 /testdata 的 4096×4096 实帧**。已实际改写 sparse_reference.cpp、drizzle_engine、normalize_precision.cpp、normalize_science reader、逐 occurrence photometric binding 与 schema；Linux Release/单帧重复的原始数值由 Agent 提供，前台不能冒充独立复跑。

### 三项 UNRESOLVED 已由前台确定实现目标

**A. 模型精确因子是正规载体**：同一科学窗中的 v_j（源空间，单位 linear_unit²）和生产 CSR(R)（球面，单位 sr⁻¹）就是该 diagonal-planar 条件模型 C_sphere=R diag(v)Rᵀ 的**无损可重算表示**。不要求 nl≈800 时每控制点重复写 798² 的 JSON。为消除旧 `local-factor/v1` 将源 v_j 标成面亮度协方差单位的错误，需要新版本 `local-factor/v2` 或等效不能混读的明确 schema：logical C_shape=[nl,nl]、factor_shape=[m]、factor_unit/derived_unit 独立、operator_ref 指向同模型的 CSR、模型维度/背景/随机分量/遗漏项完整。reader 能独立重建 C 及核验 PSD/单位/rank/导出的 SNR 与 T；旧 v1 必须作真实来源的严格适配而非无声改 BUNIT。权威：NORMALIZE_DATA §7.1、detail/normalize/contracts.md §7.4。

**B. 不默认截断观测随机贡献求速度**：Drizzle 同次 R 及真实选择叶的完整源贡献是科学估计量定义。当前 m≈645、rank≈626，代码为半正定 Gram 矩阵执行 hand-written Jacobi 约 20–26 分钟，而 Drizzle 约 43–61 秒。数值性能工作归 **lib/algorithms/noise_snr**，不是 scheduler 临时取 m≤256 或另外构造看起来满秩的 R。使用 NOISE_SNR §11.4 的 Q=R√D 行空间投影定理：在经确认的满列秩支路直接计算源空间 GLS；秩亏在数值 rank-revealing QR/SVD 上求正交行/零空间投影，再单独处理 nuisance 背景的通量可辨识性。选择现有工程可验证支持的数值 backend，不能把失败的 Householder+QL 自实现反向塞入生产。输出 solver、r、阈值/条件、真实数值误差和性能；在未独立验证时保留原结果为可重复基线，**不声称当前速率已经解决**。

**C. 先修 PSF 物理模板再解释 SNR**：目前 normalize_precision.cpp::moffat4_profile 是无总积分归一的中心取样，且 profile_enclosed=`min(1,sum(raw))`；这不是 canonical unit-total-flux 的 P。阶段一参考模型要复用 PSF 模块，给出 \(\int_{\mathrm{pixel}}p(x-x_0,y-y_0)\) 和实际子像元中心/椭圆参数、单次通量归一、finite support 的 f_out。`flux_estimator=analytic_total` 可作为 canonical 总通量参考，`aperture_with_correction` 必须显式定义修正模型及误差，`diagnostic` 不可标为已校准 canonical。PSF 有效域和空间模型状态与已拟合参数必须绑定，遗留全帧中位 sigma 仅为 approximate 代理。实现责任在 **lib/algorithms/psf**；scheduler 只能消费 PSF evaluator，不能维护第二套 raw Moffat 模板。修模板后连带重新评估 F_ref、V_F、Poisson 和空间 alpha，不允许拿旧实帧 SNR 数字复用。

### 已发现的实现标签错误与其它未完成项

- 目前 `lib/algorithms/noise_snr/include/acsd/sparse_reference.h` 中注释写的是 `M=Bᵀ H⁺ B`，与数学实际应为 `H^{+2}` 冲突；源文件仍有旧 `B=G V^{1/2}A` 误导性注释。代码实际求解路径按最新实现/独立数值核实，修注释不能当作修算法的替代。
- `normalize_precision.cpp::build_patch_windows` 用 const_cast 修改 const PrecisionFrameRequest 内的 window_radius_px。应去掉这种对输入的隐式写回，窗口半径成为局部计算值/具名输出状态，避免 const 原对象及并发情形的未定义行为。
- 本轮统计 `covariance_rank=626`、`condition_number≈2.3×10^9` 显示真实病态/秩亏；不可单凭 64/64 controls.status=available 推断一般数值可靠性。参考 PSF 模板的物理错误是额外关键缺陷，必须保留记录。
- 帧摘要 `supported_area_sr` 在控制窗重复叶时未去重，当前值只是旧口径累计；科学帧摘要要求按真正唯一的球面支持归属合并，不能把重复叶误认为更多独立信息。
- F08 typed Runtime、sed_response 真正 Gaia XP 参考积分、空间 alpha 模型、母版协方差、实际扰动一列→正式 HiPS signal 的独立对拍、Windows/DEV_MODE 四构建及 mixed instrument 数据仍在后续实现清单；不要把 C++ 类已声明当作这些功能已经完成。
- 单帧 raw FITS 头没有 WCS 但真实 acsd normalize exit=0 的 Agent 结果，是项目 IPV 生产接线的**有意义证据**；不是全部 31 帧或 Gaia 留出星全域精度的保证。
- 本轮未触碰 Mosaic 与 Export，不应让后续 Agent 借“先修科学精度”擅自切换阶段二默认权重。

### 工程分工和顺序

由前台 science/psf 定义像元积分 PSF，science/noise_snr 定义秩亏 GLS 与适用域，NORMALIZE_DATA 定义正式 factor/单位/schema，detail/normalize/contracts 定义 ABI/typed 接线。Agent 先解决与物理正确性直接相关的真实 PSF、精度因子单位及消费者和秩亏求解；**不需要重新发明算法决策**。完成这些后再推进 Runtime typed 迁移和 sed_response/母版错误模型，使八节点的临时 JSON 传递从真实生产依赖迁成统一 typed ports 与显式科学 spill，不能把它们误判为可关闭的开发快照。


