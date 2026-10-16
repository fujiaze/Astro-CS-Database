# 科学设计与生产源码差异地图

本页连接最高设计[1]、科学模型[2]与真实生产源码。目标合同、静态实现和已有实验分别列出；本页不包含新的编译、数值实验或真实帧运行。源码路径和函数名是可追踪定位，数学定义仍由对应 science 正本拥有。Normalize 是当前实现收敛范围，Mosaic/Export 在此只说明相邻消费和产品边界。

## 1. 已有生产路径

| 环节 | 静态实现 | 结论边界 |
|---|---|---|
| Normalize 八节点 | `lib/infrastructure/cli/commands.cpp::run_phase1_block` → IR → `lib/infrastructure/scheduler/src/module_adapters.cpp::P1NodeModule` 与 `p1_op_*` | 不是 calibration-only 的 `lib/phase1_session/p1_session.cpp::run_session` |
| WCS 与科学源 | `p1_op_wcs` 真调用 IPV；`p1_op_star_psf_impl` 消费已解 WCS | 原始头无 WCS 不等于求解失败；solver 候选和科学源不同 |
| 球面参考精度 | `p1_op_drizzle` → `lib/infrastructure/scheduler/src/normalize_precision.cpp::stage_frame_precision` → `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp::complete_spherical_reference` | canonical 计算已接入代码；不是所有响应、PSF、噪声和单位状态都已闭合 |
| precision 持久化 | `lib/algorithms/integration/normalize_science/src/normalize_science.cpp` 的 `write_local_model_file`、`write_frame_science_document`、`read_frame_science_document`、`publish_staged_frame_science`；writer 有实际调用 | serializer、reader 与发布均存在；合同覆盖范围见下表 |
| Mosaic 自由组合 | `lib/algorithms/coverage/src/coverage.cpp` 不因 filter 字符串不同拒绝输入 | 保留可解释坐标、格式与几何的数据检查；不能据此声称未知通带已同一化 |
| Mosaic 最终产品 | `module_adapters.cpp::p2_op_write` 使用 `AIO_HIPS_PRODUCT_SIGNAL` | 内部 support/variance 仍参与计算，不构成最终图像层 |
| Export 最终产品 | `module_adapters.cpp::p3_op_writer` 的 `unc=false`；正式写出走单信号路径 | 不要求阶段二发布 variance/ivar，不凭单信号制造精确误差层 |
| 开发编译边界 | 根 `CMakeLists.txt` 将 `ACSD_PROBES`、开发子项由 `ACSD_DEV_MODE` 派生，并固定关闭 checkpoint 子项 | 兼容 option 名称仍存在不等于可独立启用；运行等价依证据而定 |

## 2. Normalize：合同与实现边界

字段、单位和状态以 NORMALIZE_DATA 为准[3]；接口与实际落点见实现规格[4]。下表是本页唯一的实施差异登记，不由其它导航页另立平行清单。

| 主题 | 已有实现与可追踪符号 | 仍受限制的合同或静态风险 |
|---|---|---|
| Runtime typed 对象 | `lib/include/acsd/normalize_data.h` 已有 ObjectHeader、ReferenceDefinition、ReferenceControl、SparseReferencePrecision；descriptor 已有关键排序边 | `lib/include/acsd/core/context.h::RunContext` 仍以 artifact descriptor 为主；实际跨节点读取 p1_*.json/FITS，缺完整 typed 仓、revision 与消费者 lease；noise 的合成 `fluxes` 边和生产读取不一致 |
| 帧身份 | `p1_frame_key` 及侧车 frame_id 已贯穿若干路径 | 当前键来自净化 basename；生产 fdoc 构造没有绑定 input_occurrence；重复输入、同名文件与对象版本未满足 occurrence 合同 |
| 逐帧响应 | `p1_op_photometry::p1_resolve_frame_binding` 已读取 `photometry.frame_overrides`，为逐帧拟合选择 filter/QE 并保留继承来源 | 匹配依输入路径、basename 或净化 key，第一条命中即停止；重复路径的不同 occurrence、同名冲突和冻结输入序号仍未满足严格身份合同，不能将其概括为完整 occurrence 绑定 |
| 参考来源 | `normalize_precision.cpp::parse_reference_config` 支持 explicit_linear_flux；缺参考明确 unavailable | sed_response 明确记录积分 not_wired；不能宣称已由实际参考 SED/响应生产 F_ref,k，也不以检出星中位数补 canonical |
| GLS 矩阵与秩 | `complete_spherical_reference` 的常规路径已对 Qᵀ 作列主元 Householder QR，投影白化模板并独立判背景秩/通量可辨识；Jacobi 对照保留平方逆公式 | m×nl 超过工作集阈值时仍回到 Jacobi oracle；两条路径的截断与边界需分别验证。不能继续写“生产只用 Jacobi”或“背景冗余必然拒绝通量” |
| 数值子空间诊断 | `PivotedQr` 显式重算残余列范数并以相对主元门限截断；有 solver/rank/tolerance 诊断 | QR 主元门限不等于奇异值门限，背景投影后 I_F 仍以两个范数平方相减，近共线时有消去风险；主元比平方不等于谱条件数或保守上界。Jacobi 分支的负谱处理仍需单独核对 |
| 最终信号算子 | `build_patch_windows` 用完整捕获贡献汇总 N，设置 R=(a/A_drop)/N；Drizzle 的捕获筛选已按目标叶保存贡献 | 捕获完整性、候选遗漏和同次正式叶归一仍需独立核对；不能将局部算子自洽等同全域几何正确 |
| 原生 gain 与 alpha | completion 已纳入 alpha²/a_ref 的源泊松项，合成非平凡常量标度有证据[5] | 生产请求只赋标量 applied_scale；空间 pixel_scale 未贯通。`a_ref=Σalpha P` 使用当前窗口，而信息模板仍取 w.profile；在截断总通量/空间标度下的配对未被现有验证覆盖 |
| PSF 与模板 | `pixel_integrated_psf.cpp::pixel_integrated_moffat4_profile` 已实现归一椭圆 Moffat4 的像元求积；`normalize_precision.cpp::pixel_integrated_profile` 已实际调用 | 当前调用仍传帧级圆化中位 sigma，未贯通逐位置椭圆/旋转 PSF。相邻阶差仅作自检；每点包络和误差目前使用帧级极值，不能冒充该点真实诊断 |
| 随机与共同误差 | 背景方差、参考源分量及 omitted_terms 已进入 completion | 生产请求固定 master_uncertainty_modelled=false；母版 Jacobian、修复相关、光度参数共同项与交叉项仍未完整求值；upper_bound 的使用须满足真实 PSD 缺项与固定模板前提 |
| 支持与摘要 | 目标叶由中心 sky 的共享 ang2pix 取得，叶号0具显式有效性；摘要使用面积加权 log SNR 中位数 | 当前支持来自捕获邻域 Σmin(D_l,Ω_leaf)，不是一般覆盖并集，也不自动等于整个非重叠 patch 支持。当前查询半径和首次叶归属的采样选择仍影响摘要 |
| 科学模型载荷 | h、Z 和预算内 C 已产生；预算外 writer 使用 local-factor/v2，分别声明源 v、派生 C 与 R 的单位/shape，以及 solver 和假设字段 | PSF 文件仍主要写 h，所需平面 P 未完整持久化；部分 support_note/representation_note 残留 v1，逐点诊断引用帧极值。精确因子仅对声明的对角平面模型精确，不补全遗漏相关项 |
| 叶阶与单位 | controls 使用实际 leaf order；signal 保存 scale_state；侧车有单位字段 | `p1_op_drizzle` 仍给 signal.leaf_order 赋 k−9，与 controls 的 k 不一致；sidecar 默认 ADU/sr/adu 及 writer 固定 BUNIT 未随 applied 数值单位完整更新 |
| 应用身份 | reader 会拒绝 scale_applied=true 却缺 application_id | 生产 fdoc 只写 scale_applied/status，没有赋 application_id；因此已标度分支不能由 native 单帧记录推断已成功发布 |
| reader | 已恢复 SIP、leaf/order、kind/domain/representation 与状态；另有 `load_local_model_payload`、`reconstruct_factor_covariance` 和 `verify_frame_science_factor_models` 实现 | 通用 `read_frame_science_document` 尚未调用模型 verifier；独立读取仍缺真实模型 hash/重建及 h-C-Z-R、V_F/T/summary 全面核对。重建函数只检查 sr 幂次，未完整核对基础线性单位。发布器的 hash 校验不能替代读侧调用 |
| 发布 | 发布器逐文件验证 hash、原子写模型及 sidecar、回读并返回结果 | 整帧 signal 与所有模型/sidecar 尚非同一发布事务；回读失败直接返回且未撤回已写 sidecar；writer 将该失败记为 precision 不可用，不能等同合法 unavailable 产品已完整发布 |
| Drizzle frame 生命周期 | `p1_op_drizzle` 在 `hp_drizzle_run_phase1_hips` 返回后调用 `aio_pipeline_frame_destroy(frame)`；`aio_pipeline.cpp` 的 destroy 释放块并 delete frame | 后续 precision staging 的 exposure fallback 在原始 EXPTIME 无效时仍调用 `aio_frame_kv_get(frame,"header","EXPTIME")`，形成释放后访问路径。需在销毁前保存所需元数据或延长明确所有权；本轮仅静态定位，没有执行缺 EXPTIME 构型或修复代码 |

上表列出的数学/模型风险保持源码证据身份。它们不声明已运行新反例，不修改科学定义，不否定合法 signal 的存在；需要科学裁决的具体反例回到唯一 science 正本处理。

## 3. 相邻阶段的消费与统计含义

`module_adapters.cpp::p2_sparse_layer_probe` 仍读取 `ACSD_SPARSE_SNR_LAYER` 指向的规整像素网格。它不是 frame_science.json reader；新增 canonical sidecar 不表示 Mosaic 已经消费该物理对象。当前积分优先 corrected variance，其次已有 ivar，缺失时才走兼容 SNR 链。阶段二切换不属于 Normalize 文档或接口存在所自动授权的行为。

`lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp` 的 SNR 路径使用 (SNR/F_ref)² 一类公式，得到点源通量精度 I_F；若要成为球面面亮度权重还需同条件 T=V_B/V_F。已有 P1 reference-source transfer 也不能直接作为任意 actual-scene 位置的噪声。精度重建、PSF、相关性与模型误差的边界由噪声科学正本规定[6]。

覆盖模块仍以输入最大叶阶的最小值取 target_order，尚不等于独立配置的科学目标分辨率。Mosaic writer 已记录 response_distribution 并可写混合响应；其 `first.empty()` 判首帧的逻辑使先空通带、后已知通带的序列可能仍被标为 same。分布记录有价值，但 `obs_filter` 不能在此边界被当作精确总体通带。

背景控制的统计量和方差与参考点源精度分名；连通分量独立规范不提供断连分量的绝对天空锚。乘性残余、颜色差和 PSF 差不能因接缝减小而被统称为加性天光。本文不依据接口存在宣称背景、曲边或 Export 的独立科学验证已完整。

## 4. 日志、监控与诊断的接线边界

`runtime_client.cpp::exit_code_for_error_domain` 已实现核心错误域映射，失败节点 manifest 的 `error_kind=input/disk_full` 在最终映射前分类。核心 `Error` 直接成员为 domain/message/cause，不是完整 CLI error_report。当前 CLI 源码未找到 `log_artifacts[]` 写出点；Python 结构化日志生成器与 Core logger 存在，不能据此称正式运行的同源双日志和清单登记已完整接入。

Python 监控的 `TraceSnapshotObserver` 只重放构造时事件快照，未按构造 run_id 过滤；active_workers 当前是活动节点数，granted_workers 取快照内正租约最大值。Linux 默认采样 `/proc/self`，Windows Python 后端为 stub。各字段的实际来源和解释见[资源监控合同](../resources/observability/RESOURCE_MONITORING.md)；代理指标和分母分歧保存于[待核对记录](../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。这些静态事实未重新运行验证。

开发快照入口 `dev_diagnostics.cpp` 仍持全局 mutex 同步写盘，与低干扰目标存在差异；头文件“不阻塞”注释不代表已实现有界异步队列。`probe.h` 的既有性能输出字段与规范中的拟议完整 perf 格式分别解释，不能凭示例 JSON 认为已有相应 schema。正式 cache/spill 和被动快照不能因名字近似而混为同一依赖。

## 5. 已有证据怎样使用

验证记录[5]中异方差反例的生产 V_F 与解析值均为 2.6470588235294117；所列背景、秩亏、leaf 0、确定性标度与 Monte Carlo 构型给出局部支持。QR、像元积分 PSF 和因子 v2 的后续实现与验证由独立复核[8]记录，不用这些实质修复之前的静态结论描述当前代码。日志包含构建记录；本页未重跑，不能据此宣称全 CLI、所有平台、真实响应、空间模板、发布事务或 OFF/ON 科学等价已独立验证。

独立复核[7]保留实际原始头无 WCS 单帧的 IPV 记录，并明确该帧未施加 photometry。它也保存了可判错的数理反例和当时实现事实。查阅此类证据须按对应源码和构型归因，不将已修正的数学问题继续写成现行实现，也不将局部修复升级为完整阶段一验收。历史研究单元与失败脚本继续保留。

## 参考文献

[1] [ACSD 最高设计](../../ACSD_DESIGN.md)。

[2] [统一科学模型](../../science/unified/UNIFIED_SCIENCE_MODEL.md)。

[3] [Normalize 数据合同](../contracts/NORMALIZE_DATA.md)。

[4] [Normalize 接口实现规格](../../detail/normalize/contracts.md)。

[5] [局部 GLS、标度及 Monte Carlo 验证记录](../../../实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log)。

[6] [噪声、参考 SNR 与精度重建](../../science/noise_snr/NOISE_SNR.md)。

[7] [球面参考精度独立复核](../../../实验/normalize-precision-p5/INDEPENDENT_REVIEW_950C261.md)。

[8] [QR、像元积分 PSF 与因子 v2 复核](../../../实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md)。
