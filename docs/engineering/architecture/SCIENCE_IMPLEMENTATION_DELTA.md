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
| 逐帧响应 | `p1_op_photometry::p1_resolve_frame_binding` 已读取 `photometry.frame_overrides`，为逐帧拟合选择 filter/QE 并保留继承来源；Issue #106 起支持 `response` one-of（`parametric_narrowband` / `measured_curve`），同层并存判 `RESPONSE_SOURCE_CONFLICT`，跨层整体替换并登记来源 | 匹配依输入路径、basename 或净化 key，第一条命中即停止；重复路径的不同 occurrence、同名冲突和冻结输入序号仍未满足严格身份合同，不能将其概括为完整 occurrence 绑定 |
| 参考来源 | `normalize_precision.cpp::parse_reference_config` 支持 explicit_linear_flux；缺参考明确 unavailable | sed_response 明确记录积分 not_wired；不能宣称已由实际参考 SED/响应生产 F_ref,k，也不以检出星中位数补 canonical |
| GLS 矩阵与秩 | `complete_spherical_reference` 的常规路径已对 Qᵀ 作列主元 Householder QR，投影白化模板并独立判背景秩/通量可辨识；Jacobi 对照保留平方逆公式 | m×nl 超过工作集阈值时仍回到 Jacobi oracle；两条路径的截断与边界需分别验证。不能继续写“生产只用 Jacobi”或“背景冗余必然拒绝通量” |
| 数值子空间诊断 | `PivotedQr` 显式重算残余列范数并以相对主元门限截断；有 solver/rank/tolerance 诊断 | I_F 已改为显式残差两轮投影后取平方范数，保存响应/残差范数和 condition_estimate_method；旧范数平方相减问题不再描述该分支。QR 主元门限不等于奇异值门限，主元比平方仍不能仅凭方法名成为谱条件数或保守上界；通量残差平方与阈值的尺度、Jacobi 分支分别核对 |
| 最终信号算子 | `build_patch_windows` 用完整捕获贡献汇总 N，设置 R=(a/A_drop)/N；Drizzle 的捕获筛选已按目标叶保存贡献 | 捕获完整性、候选遗漏和同次正式叶归一仍需独立核对；不能将局部算子自洽等同全域几何正确 |
| 原生 gain 与 alpha | completion已纳入α_j²F_native P_j/g的源泊松项，合成非平凡常量标度有证据[5] | 生产请求已有 pixel_alpha_at，经同一 photo_spatial_m 求值，再填每窗 pixel_alpha；优先于旧 cfg.pixel_scale 和常量。已有真实applied空间场运行记录，不能再统称只有alpha=1证据；本轮只查阅日志/改动，未重跑。当前实现以位置alpha_star定义模板(α/α_star)P与源泊松项，窗口sum仅诊断；正式正本已按Issue6统一为位置参考幅度，另保留实际applied总通量；源码注释仍称“总通量”的位置需实现方同步，F_ref与物理全域积分不可混称 |
| PSF 与模板 | `pixel_integrated_psf.cpp::pixel_integrated_moffat4_profile` 已实现归一椭圆 Moffat4 的像元求积；`normalize_precision.cpp::pixel_integrated_profile` 已实际调用 | 已消费逐帧 psf_params，PSF 库构建三角剖分并混合顶点积分响应；逐点包络/求积误差与帧摘要已经分离。当前模型无孔洞/跨无效区约束输入，凸包外/剖分失败仍回退帧常 sigma，不能由拟合成功行推定全部质量与空间支持合格；相邻阶差只作自检 |
| 随机与共同误差 | 背景方差、参考源分量及 omitted_terms 已进入 completion | 生产请求固定 master_uncertainty_modelled=false；母版 Jacobian、修复相关、光度参数共同项与交叉项仍未完整求值；upper_bound 的使用须满足真实 PSD 缺项与固定模板前提 |
| 支持与摘要 | 目标叶由中心 sky 的共享 ang2pix 取得，叶号0具显式有效性；摘要使用面积加权 log SNR 中位数 | 当前支持来自捕获邻域 Σmin(D_l,Ω_leaf)，不是一般覆盖并集，也不自动等于整个非重叠 patch 支持。当前查询半径和首次叶归属的采样选择仍影响摘要 |
| 科学模型载荷 | h、Z 和预算内 C 已产生；预算外 writer 使用 local-factor/v2，分别声明源 v、派生 C 与 R 的单位/shape，以及 solver 和假设字段 | PSF载荷已新增planar_template、source_pixel_alpha与reference_alpha_star，读写函数均有对应处理；模板实际为位置标度约定。covariance note为v2、逐点诊断分开。完整顶点参数/支持、模型通用reader接线和外部可求值仍须核对，不能由新增数组声明独立读取全部完成 |
| 叶阶与单位 | controls 使用实际 leaf order；signal 保存 scale_state；侧车有单位字段 | `p1_op_drizzle` 已在修复提交 `e8ff4bc90` 将 signal.leaf_order 赋值为 k；但 reader/schema 仍为 frame-science/v1，且缺跨文件产品几何验收。sidecar 默认 ADU/sr/adu 及 writer 固定 BUNIT 仍未随 applied 数值单位完整更新 |
| 应用身份 | reader 会拒绝 scale_applied=true 却缺 application_id | 生产fdoc已在frame_applied分支生成application_id/response_id，并有真实applied记录。ID目前将标量k用std::to_string、响应用库键/路径组装；极小标度的字符串精度与完整曲线内容绑定仍有静态风险，不能把ID非空当无损身份 |
| reader | 已恢复 SIP、leaf/order、kind/domain/representation 与状态；另有 `load_local_model_payload`、`reconstruct_factor_covariance` 和 `verify_frame_science_factor_models` 实现 | 通用 `read_frame_science_document` 尚未调用模型 verifier；独立读取仍缺真实模型 hash/重建及 h-C-Z-R、V_F/T/summary 全面核对。重建函数已加入 expected_linear_unit，核对基础线性单位和平方指数；该函数修复不等于通用 reader 已调用它。发布器的 hash 校验不能替代读侧完整验证 |
| 发布 | 发布器逐文件验证 hash、原子写模型及 sidecar、回读并返回结果 | 整帧 signal 与所有模型/sidecar 尚非同一发布事务；回读失败直接返回且未撤回已写 sidecar；writer 将该失败记为 precision 不可用，不能等同合法 unavailable 产品已完整发布 |
| Drizzle frame 生命周期 | `p1_op_drizzle` 在 `hp_drizzle_run_phase1_hips` 返回后调用 `aio_pipeline_frame_destroy(frame)`；`aio_pipeline.cpp` 的 destroy 释放块并 delete frame | 后续 precision staging 的 exposure fallback 在原始 EXPTIME 无效时仍调用 `aio_frame_kv_get(frame,"header","EXPTIME")`，形成释放后访问路径。需在销毁前保存所需元数据或延长明确所有权；本轮仅静态定位，没有执行缺 EXPTIME 构型或修复代码 |

上表列出的数学/模型风险保持源码证据身份。它们不声明已运行新反例，不修改科学定义，不否定合法 signal 的存在；需要科学裁决的具体反例回到唯一 science 正本处理。

## 3. 相邻阶段的消费与统计含义

`module_adapters.cpp::p2_sparse_layer_probe` 仍读取 `ACSD_SPARSE_SNR_LAYER` 指向的规整像素网格。它不是 frame_science.json reader；新增 canonical sidecar 不表示 Mosaic 已经消费该物理对象。当前积分优先 corrected variance，其次已有 ivar，缺失时才走兼容 SNR 链。阶段二切换不属于 Normalize 文档或接口存在所自动授权的行为。

`lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp` 的 SNR 路径使用 (SNR/F_ref)² 一类公式，得到点源通量精度 I_F；若要成为球面面亮度权重还需同条件 T=V_B/V_F。已有 P1 reference-source transfer 也不能直接作为任意 actual-scene 位置的噪声。精度重建、PSF、相关性与模型误差的边界由噪声科学正本规定[6]。

覆盖模块仍以输入最大叶阶的最小值取 target_order，尚不等于独立配置的科学目标分辨率。Mosaic writer 已记录 response_distribution 并可写混合响应；其 `first.empty()` 判首帧的逻辑使先空通带、后已知通带的序列可能仍被标为 same。分布记录有价值，但 `obs_filter` 不能在此边界被当作精确总体通带。

背景控制的统计量和方差与参考点源精度分名；连通分量独立规范不提供断连分量的绝对天空锚。乘性残余、颜色差和 PSF 差不能因接缝减小而被统称为加性天光。本文不依据接口存在宣称背景、曲边或 Export 的独立科学验证已完整。


### 3.1 五项研究按最新静态生产实现的裁决（基线 91eb77b；非真实运行）

以下定位来自 #98–#102 的研究报告、与现行 main 的交叉核验；Issue 仍保持其独立缺陷证据，不凭修改文档直接关闭：

| 科学模块 | 确认应复用/已改正 | 仍需执行的最小生产切口 |
|---|---|---|
| R1 Gaia | compute_f_syn_cached_xpsd 已正确处理 XP 数值核且不再次乘 G；star_matcher.cpp 零 MAD 众数簇路径已修早期离群全纳问题；fit_ok 避免 NO_DATA scale=1 伪成功 | fit_frame_photometry 无条件 scan_curve_library 并按 filter_name 取滤镜（measured 路径**逐位不变**）；**参数窄带已直通**（Issue #106：`photometry.fit.response` one-of + `spectrum_integrator` 边界精确分段求积；QE 校验保留；见[测光 §2.1.1](../../science/photometry/PHOTOMETRY.md)）；filter_curve_json.h 有序光谱 response hash (#42，曲线**有序内容** sha256 已入响应身份)、窄带边界分段求积 (#43，**仅参数化模式**；实测曲线的 2nm 粗格求积仍 OPEN)、XP 响应覆盖 (#44，覆盖状态已发布)、spatial_gain.cpp 留出/支持及 Cov_theta (#46)；不将 MAD 当参数 SE |
| R2 Drizzle | drizzle_engine.cpp 按 detector pixfrac 再 WCS，w=a/A 与 F/N/D 数值语义正确；astro_sphere_sink.cpp Writer 已消费连续 area，旧 #66 的 uint8 口径不能再原样批评 | spherical_overlap.cpp 的高阶 boundary4 非真实叶曲边 (#15)，drizzle_engine.cpp 短于60″的 SIP 曲边未严格受控；source candidate bbox 和变形误差需独立证书；辅助 drizzle_science.cpp 的 A=f²Omega 不能充当精确 oracle |
| R3 精度 | normalize_precision.cpp、sparse_reference.cpp 已具备 canonical GLS/因子模型；p3_resample.cpp 同叶系数聚合已修 #88 局部旧状；#69 的 FP 尾差逻辑也有变化 | Mosaic 应优先 actual-scene r*C*r^T，缺项时只用有来源的噪声/PSF场正向预测；现行 weight_chain.cpp 以参考点源 Fisher 当场景像元精度 (#8)；p2_sparse_layer_probe 仍消费旧 ACSD_SPARSE_SNR_LAYER 而非 canonical frame_science (#13)；#64 缺 variance 仍产生部分已知但标 full；#65 重复 COO 覆盖、#67 过宽 white-noise shortcut 与 #70 nearest-fill 需核实际消费者/适用域 |
| R4 Sky | sky_plane.cpp 与 p2_op_upm_fit/apply 当前执行 additive-only，#80 numerical deflation/未识别失败已有实修；旧 upm.cpp 非生产主链 | 当前生产和设计优先 additive-only，相对增益 A+G 仅属外部星点可辨时的扩展；sky_make_residual_fields floor-bin 与 sky_own_eval 四结点双线性 basis 不一致 (#83)；bg controls 的条件 covariance/零 MAD、不同响应共同 S 假设需对拍；p2_op_upm_apply 的 param_cov_included=false 仍是真实声明 (#21/#71/#72) |
| R5 Coadd | p2_integrate_pixel 处理合法负/零信号与数值状态；Mosaic mixed-order p2_hips_order_uniform fail-closed 正确，不开发 remap；原帧 mask 已传 | rejection.cpp N4/5 percentile 不具背景平移等变性 (#20)；source Fisher → scene weight 错位、共享/选择后 Cov 不闭合；Export 通带响应 metadata 丢失 (#79)，边界连通结构 #85 需真实诊断 |

R2 所说“严格相交器”目前是可编码设计，**不是生产已完成的证明**。R3 数字矩阵 oracle 和 R5 百分比平移反例分别验证数学契约和算法反例，不支持“全部真实生产 PASS”。旧 #45/#66/#88 修复状态需按当前实际调用分支判断，而不是机械搬旧问题；#19 的安全拒绝不是必须开发 mixed-order 合成能力。建议代码改造顺序为 response/identity 与 canonical reader → geometry/variance 完整语义 → additive sky 统计与 fit/eval → rejection/weight CoadditionEngine → writer/export；每步用 #103 冻结样本及 same-input 对照复核。


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


## 6. Issue #10/#12 最新基线及 #74/#75 静态证据图（2026-10-17）

基线 `d09371c7f40ed8518fe819fcd311a433e42902cb`；本节全部为**源码静态定位**。Issue #10 已有真实 reader / E2E（`实验/issue10-leaf-order-verify/evidence/REPORT.md`）说明 `leaf_order=k` 修复有效；不能由此推断 v2 schema / reader 交叉验证 / P2/P3 消费已完成。Issue #12 旧 `1e-8*max(1,|h|)` 残留尺度漏洞，唯一新积分域与 oracle 见 `docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md`。

### #74 低 nside 执行异常：定位任务，而非算法结论

实验已观察：相同 M42/M4/T2 + masters/Gaia，auto Nside=2^18 正常约 292.29 秒并生成 sidecar；显式 512 完成 HiPS Norder0 但后约 1204.78 秒终止，无 sidecar；显式 2048 完成 Norder2 后约 236.91 秒终止，无 sidecar。两例观测窗口不同，**ROOT_CAUSE_UNKNOWN / OBSERVED_INCOMPLETE**，不得称“死循环已证明”。复跑入口 `实验/issue10-leaf-order-verify/code/run_e2e_matrix.py` 与 `evidence/{05-e2e-matrix.json,08-k9-anomaly.txt,REPORT.md}`。

生产时序：`module_adapters.cpp::p1_op_drizzle`（8328）→ `normalize_precision.cpp::build_capture_targets`（289；每个控制点 query_disc，可能随较粗球面叶覆盖数量而变）→ `hp_drizzle_run_phase1_hips`（约 9445；完成写叶 tiles 并返回 overlap）→ `aio_pipeline_frame_destroy`（约 9470）→ `build_patch_windows`（325；当前**对每个控制点线性遍历全部 overlaps**，潜在 O(N_control·N_overlap)，但未证明是当次热点）→ `stage_frame_precision`（462；空间 PSF、逐像元 alpha、`complete_spherical_reference` 在 700、局部模型构建/CSR）→ `write_local_model_file` 与 `write_frame_science_document`（约 9689）→ `p1_op_writer`（9883）→ `publish_staged_frame_science`（约 10224，hash/reader）→ final manifest。HiPS 已存在只排除其中部分前段；堆栈可能仍落在该段的线程/异步任务，不可凭日志断言具体调用点。

**唯一取证施工**：保持科学参数、帧输入及真实输出路径完全相同，三个 case 在**同一硬件构型**运行；按 monotonic wall + thread CPU + RSS/峰值 + N_control/N_target_leaves/N_overlap/N_window_pix/N_window_leaves/CSR nnz/solver rank 记录阶段入口/出口、frame/run/nside/thread-id。最少埋点位置为上述八段，单次日志量 O(控制窗口批次) 且须有总字节上限。慢段同时取 `perf record -g`（或等效 Windows ETW）及线程用户态采样堆栈，明确热函数、热点行、系统调用/锁等待/CPU 采样比例；存采样命令、工具版本、退出码、原始 profile、火焰图与耗时分母。超时统一外部监督，发正常取消，采集末栈/指标/未完成产品并按取消处理，不直接 `kill -9` 掩盖现场；异常时候先保留故障证据。以 `N_control×N_overlap` 假说对照实测计数和堆栈，复核 query_disc、GLS/O(nnz)、文件 hash 等备选。先交“热点定位 + 粗复杂度上界”报告，证据支持后再设计等价优化；**不得**擅自减少 precision controls、放弃真实 input、静默改 Nside/阈值或让半产品被发布。源码优化未授权；性能/输出数值 PASS 均 NOT_RUN。

### #75 已有 source stamp 与未闭合产品身份

现有根 `CMakeLists.txt`（约 110–177）在 configure 时从 git HEAD 生成 `version_generated.h` 的 `ACSD_COMMIT_SHA`；已有 **构建期** `eng/tools/gen_build_stamp.py` → `build_stamp_generated.h` → `lib/infrastructure/scheduler/src/build_stamp.cpp` → `acsd::core::build_stamp()`，`commands.cpp::build_run_provenance`（470–493）已把 build_head_sha/dirty/build_source_digest/configure_head_sha 加到 run manifest。旧 `ACSD_COMMIT_SHA` 仍被 CLI 若干路径消费；P1 `normalize_precision.cpp::load_run_identity` 从运行元数据取 `software_sha`。因此**已有部分修补，不能称整个版本机制不存在，也不能将 source_digest 叫 binary hash**。

**唯一施工规格**：先生成依赖闭包报告——确认 custom command 的 `git ls-files` 列表只在 configure 时求值（`CMakeLists` 当前命令依赖 `lib/eng/cmake/CMakeLists/VERSION/CMakePresets`，而 `gen_build_stamp.py::SOURCE_FILES` 额外包含 `eng/contracts/resource_gate_v1.json`、`eng/packaging/config/runtime_resources.json`；当前两份文件是否经其他 CMake 依赖间接触发要逐个验证）。HEAD-only 前进且内容不变会使 build stamp 头在 Ninja 无输入变更时保持不动；必须把现有 `add_custom_command(OUTPUT build_stamp_generated.h)` 重构为有显式有序依赖的 **always-run 轻量 stamp-check target**：每次 Ninja 构建开始均重新收集构建时 HEAD、dirty、受控源集及新增/删除文件；检查 target 在编译 `build_stamp.cpp` 之前完成。只有身份字段 (`head_sha, dirty, source_digest, configure_head_sha`) 变化时才改写 `build_stamp_generated.h` 并重编小 TU；其余不触发重编。**当前 `gen_build_stamp.py::build_stamp` 使用 `datetime.now` 生成 `stamp_utc` 且 `render_header` 将其编进生成头；即使身份字段完全没变，每次执行现有 `write_if_different` 也会因 UTC 改变而重编**。因此唯一改法是：stamp-check 首先比较身份字段，没变化就沿用上次 `stamp_utc` 且不写 header；有变化才取本次 UTC 并落盘，独立诊断可记录每次检查时间但不可进入 TU 身份。新增/删除源集成员纳入生成器 manifest，CMake 的源文件构建图需要按显式列表再配置；不强制重编全仓。对 Git 提交身份的每次变更记录 `build_head_sha`，同时保持同内容 source digest 不变。CMake 输出保持 configure-time `configure_head_sha` 仅供诊断；sidecar 的 `software_sha` 必须来自**运行实际加载**的 build-time 身份而非 configure-time 值。

身份分五层：`repo HEAD`（Git 提交，不保证工作树）、`build_head_sha + build_dirty`（构建时工作树状态）、`build_source_digest`（**声明源集**内容，不含整个 toolchain）、`build_config_digest`（完整编译选项、编译器/链接器/依赖版本/生成文件）、`binary_sha256 + loaded_module_sha256[]`（实际磁盘 exe/DLL 字节）。生产/部署进程需要定位**已加载的** exe 与依赖 DSO 路径，读文件 hash 前后核对装载身份/路径变化；manifest 绑定 input/output 内容与这一整组身份。只有实际产物 SHA 可断言同一文件字节，仍不等于行为或科学正确。安装包文件摘要不能代替运行时被加载 DLL。

确定性负例：保持 CMake build dir，修改并提交 `lib/**.cpp` 后 `ninja -C ...`，新 `build_head_sha/source_digest` 与实编源码一致；仅提交 docs 后 HEAD 更新但 source_digest 保持；dirty 源码与 untracked 头进入显式声明集合；增删源码（更新 Ninja 依赖/重新配置），修改上述 `eng/contracts` 数据生成输入；clean build 同源同 flags；改变 flags/toolchain/source-only 的矩阵；已安装旧 exe + 新 CLI 仓 HEAD 和不同 DLL 字节。逐项比对 **run manifest、frame_science software/build provenance、实际 exe/DLL SHA**。测试失败只能标 UNVERIFIED，旧错误 v1 产品全部拒绝重新由 FITS 计算，不为 #75 增设旧数据补丁。

## 7. 邻接科学实施差异（#8/#9/#13）

#8：`phase2_integrate/src/weight_chain.cpp::weight_from_sparse_layer_pixel / weight_from_snr` 及 `module_adapters.cpp` P2 use_snr_chain 分支仍可能把点源 Fisher `I_F=1/V_F` 用作面亮度 `w_B=1/V_B`。两者必须通过同一被估计量/位置/条件的 `T=V_B/V_F` 转换，详细 oracle 见 DATA_SEMANTICS；静态已定位，真实 E2E NOT_RUN。

#9（P0-B1 生产修复，2026-10-18）：**已实施**。冻结线性基单位词表 {`ADU`,`XPMODEL`} 与 canonical 面亮度链（signal `L/sr`、variance `L^2/sr^2`、ivar `sr^2/L^2`）的唯一实现 = `lib/algorithms/resample/p3_rsmp_units.cpp`（`LinearBase` + `Bunit::base`/`mixed_base`；`p3_input_unit`、`p3_output` 均调用它，`p3_output` 不再自建第二套解析词表）。逐帧单位由**实际施加状态**派生：Normalize 的 `photoapplied` 帧 ⇒ `linear_unit=XPMODEL`（`bunit=XPMODEL/sr`）、`signal/properties`/`p1_stack.json`/`frame_science.signal`/`F_ref`（`reference.flux_unit`）与模型因子（`factor_variance_unit=XPMODEL^2`）全部同步；未施加帧保持 `ADU`。P2 逐帧读取 `signal/properties` 裁决单位并要求线性基一致，混合 native/applied 显式 fail-closed（`response_ids`/`response_mixed` 如实登记，不伪造统一单位）；P3 两个入口以输入裁决的真实 canonical 串写 FITS BUNIT，#91 的缺声明/冲突/表外/裸单位 fail-closed 语义不倒退。真实生产 A/B（native / applied / applied-const，同一 M42 亮帧；另加 2 帧 T3/M1 的完整 mosaic 贯通）证据见 `实验/issue9-p0-b1-production-fix/`：P1 signal 比 = α（相对偏差 5e-12）、variance 比 = α²、ivar 比 = α⁻²（常量 α 下逐像元至 float32 精度）；P2 双帧 mosaic 的 `signal/properties` 与 `p2_final.json`（含 variance/ivar 串、`linear_base`、`response_ids`）分别落 `ADU/sr` 族与 `XPMODEL/sr` 族；P3 FITS BUNIT 经 Astropy 读回同族。

#9 剩余差异（登记，不在生产 `acsd normalize/mosaic/export` 调用面上）：`lib/infrastructure/aio/src/hiss_writer.cpp` 的 BUNIT 白名单与 `aio_healpix_io.cpp` 的硬编码 `ADU/sr`（仅由无调用者的 `aio_hiss_write` C API 到达）、`lib/infrastructure/aio/product_io` 的 `UnitExponents` 单位代数（仅 `phase1_product`/`phase2_integrate` 旧库使用，二者当前无生产调用者）、已退役的 `lib/phase3_session/p3_export.cpp` 字面量。这些入口若重新接线必须先接入冻结单位表，不得各自发明词表。

#13：`module_adapters.cpp::p2_sparse_layer_probe`（约 13038）读取 legacy `ACSD_SPARSE_SNR_LAYER`，与 `normalize_science.cpp::read_frame_science_document` 存在不等于 P2 已调用。P2 v2 球面 canonical 读取和 T 验证均待实施，不能以单独 reader 的存在宣布跨阶段科学闭合。
