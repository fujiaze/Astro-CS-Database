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
