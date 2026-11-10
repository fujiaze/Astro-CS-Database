# C0｜22 process／57 typed operation 实现审查（独立 C0 审查者）

> 审查者：C0 独立审查（不参与实现、未改 `src/`、未改他人文档、未 commit）。
> 工作目录 `/workspace/Astro CS Database`，分支 `rebuild/acsd-v2`。
> 任务给定 HEAD `6a50e79bf`；**实际 HEAD 为 `c858bb130`**，但 `git diff --stat 6a50e79bf HEAD` 只含 4 个文档/验证文件（`docs/audit/b5-export-review.md`、`docs/audit/d0-error-semantics.md`、`validation/README.md`、`validation/projection/export_constant_probe.cpp`），**`src/` 与 `include/` 与 6a50e79bf 逐字相同**。本报告的 file:line 对两处一致。
> 依据：`AGENTS.md`、`docs/workplans/v2-science-engineering/engineering/10-ARCHITECTURE-PROCESS-MATRIX.md`、`docs/engineering/rebuild/README.md`＋`SYSTEM.md`＋`standards/ALGORITHM_CONTRACTS.md`、`include/acsd/model.hpp`、`include/acsd/modules/generated/inputs.hpp`；并实际读取 registry 装配、各 process adapter 与算法实现，未依赖测试通过计数。

## 0. 本次审查的“证据基线”（先说清楚，避免把旧 PASS 当当前结论）

| 事实 | 证据 |
|---|---|
| registry 实际注册 **22 process / 57 operation** | 自建探针 `/tmp/c0/reg_dump.cpp` 冻结真实装配后打印 `Registry::manifest_json()`：22 process、57 op、输出类型与白名单逐项一致；`build/science/acsd_typed_smoke` 实跑输出 `1255 checks passed; 22 processes, 57 operations, 102 TypeIds, 41 provisional bounds` |
| 57 个 operation_id 与 `processes/README.md` 白名单逐项一致（无漏、无多、无重名） | 同上 manifest；`src/runtime/generated/typed_operations.cpp` 也有 57 个 `operation_id`（但见 P2-4：那是**未被使用的死代码**） |
| **HEAD 全量 science 构建失败** | `ninja -C build/science` → `FAILED: .../validation/process/photometry.fit/m5_process_plan.cpp.o`：`MetaAction` 未实现 `LinearAction` 的纯虚 `dependency_arrays`/`dependency_children`（`include/acsd/model.hpp:205,207`；fixture `validation/process/photometry.fit/m5_process_plan.cpp:79`）。库与 `acsd_typed_smoke` 可以构建，**但 57 op 的 process driver 无法整套重编** |
| `check_design` 目标在 HEAD 失败 | `tools/generate_contract_schema.py:129` `ValueError: Unparsed declarator Unit: sr_power = 0` → 由 `tools/check_design.py` 调用而失败。生成器对 `model.hpp` 已不再可解析，`generated/codecs.cpp`/`inputs.hpp` 因此**不能保证与当前 `model.hpp` 同步**（见 P2-3） |
| 各 process 的 `comparison.json` 全部是 **2026-10-24** 的旧基线 | `validation/process/*/comparison.json` mtime 均为 10月24日；仅 8 个 process 有 pass 记录（catalogue.access、coadd.integrate、coordinates.wcs、observation.regroup、response.integrate、sources.astrometry、sources.measure、sources.psf） |
| 11 个 process **没有 process driver**（0 个 `.cpp`；少数目录只有 README/case 残留） | `coadd.select`、`coadd.weights`、`information.reference`、`information.scene`、`photometry.apply`、`projection.export`、`sky.controls`、`sky.fit`、`uncertainty.covariance`、`operators.action`、`geometry.drizzle` |
| X01 有较新实跑，但结论是“换判据后通过” | `validation/x01/observed/compare.json`（2026-11-11 04:14，`status=measured`）：`blind_solve_rms_pixel=0.2109`、`astrometry_rms_pixel=0.1491`、`signal_relative_error=0.1033`、`source_recovered=true`；同一文件 `historical_source_recovered=false`，`historical_criterion="max relative signal error <= 5% (superseded...)"`。**原 5% 判据仍未过（10.33%）**，现结论依赖 4σ 归一化判据 |

结论口径：**“已注册”只证明 typed 边界存在；本报告只把第 1–4 节里逐行核对的代码路径与可复现反例当作结论。** 只有 X01 覆盖的链（normalize→mosaic→export）有当前源码的全链实跑；其余 process 的“PASS”全部是 2026-10-24 旧证据，不能认证当前源码。

## 1. 22 process／57 operation 逐项清单（以真实 registry 为准）

列说明：**合同** = `docs/engineering/rebuild/processes/*.md`；**实现** = 真实 C++；**入口签名** = `model.hpp` 唯一定义＋registry 实际 type_id；**估计量/单位** = 该 op 真正产出的量与单位处理；**状态/错误** = 关键 Code/Assessment 路径；**资源/计划** = adapter 的 region/plan 形态；**实跑** = 是否有当前可重建的 driver/证据。

### 01 observation.calibrate
- 合同 `observation.calibrate.md`；实现 `src/observation/calibration_process.cpp`＋`calibration_plan.cpp`＋`calibration_kernel.cpp`＋`calibration_signal.cpp`＋`calibrate.cpp`（流式块路径）。
- `calibrate_observation(Context&, Ref<RawObservation>, const Calibration&) → Ref<NativeObservation>`；registry：`acsd.input.Calibrate → acsd.NativeObservation`。
- 估计量：`x=(d-b-r·(dk⊖b))/f`，`f=flat/flat_normalization`，`r=exposure/dark_exposure`；`x` 仍是已平场归一 ADU（`unit_adu()` 与 `unit_signal()` 返回同一 `Unit`，见 P2-11 的单位判据问题）。区间：同一表达式的定向区间 `[x_i.lower,x_i.upper]`；NaN 样本禁止携带有限区间（`calibration_process.cpp:239-252`）。
- 状态：非有限 raw 且 DQ 未标记 → `invalid_model`；flat≤0 或非有限 → value=NaN＋DQ bad（不静默抹零）；无 `raw_noise` 且无 gain/read → `NoiseState::unknown`＋`unknown_terms`（`calibration_kernel.cpp:441-470`），不填零。
- 资源：stream plan，`allow_spill=true`，块 = `min(elements, kDefaultBlockElements)`；3 份 `has_gain/has_read` 条件分别写在 plan/kernel/signal（P2-6）。
- 实跑：`validation/process/calibration-faults`（3 个 driver）＋M4 `cal01 41/41`（旧）。**本次未发现新 P0；B2 修复的可信度高。**

### 02 observation.regroup
- 合同 `observation.regroup.md`；实现 `src/observation/regroup_process.cpp`（adapter）＋`regroup.cpp`（唯一实现）。
- `regroup_observation(Context&, Ref<NativeObservation>, const RegroupRequest&) → Ref<NativeObservation>`；registry：`RegroupObservationInput → NativeObservation`。
- 估计量：`y=G·x`（物理 `physical_sum` 行系数 1、`physical_mean` 行系数 1/k 且要求每行成员数==`mean_member_count`、`linear_view` 允许任意符号）；噪声 `C_y=G C Gᵀ`（把每个 root influence 与 `effective_g` 复合）；区间按 `c·[lo,hi]` 求和（`regroup.cpp:196-206`）。
- 状态/错误：物理模式禁止负系数、禁止复用原生光电子、要求输出索引尺寸一致；`linear_view` 显式 `Code::unsupported` issue＋`no_physical_collection_domain`（`regroup.cpp:432-440`）。
- 资源：barrier region，`cohorts=1`，`allow_spill=false`，`max_window=1`；work=`n+1+n·m` 但被 `max(...,1<<20)` 抬到 1M（P2-7）。
- 实跑：`validation/process/observation.regroup`（2026-10-24 pass）。
- **缺陷：P1-5（`input_is_mean` 的 compose 维度/顺序）、P1-8（无父区间时把名义值当精确区间）、P2-2（`old_from_new/offset` 只查可逆、从未与 G/输出足迹比对）。**

### 03 coordinates.wcs（7 op）
- 合同 `coordinates.wcs.md`；实现 `src/coordinates/wcs_process.cpp`＋`wcs.cpp`＋`tan_sip.cpp`＋`footprint_provider.cpp`。
- 入口：`make_wcs(HeaderFields,Box,AstrometricContext,Id,NumericBudget) → WcsModel`；`forward.center0`/`forward.edge0`（PixelCenter0/Edge0→SkyICRS）、`inverse`（SkyICRS→InverseResult）、`derivative`（→DerivativeResult）、`solid_angle_jacobian`（Box→Interval）、`bind_wcs`（NativeObservation×WcsModel×AstrometricSolution→NativeObservation）。
- 单位/index：`PixelCenter0` 与 `PixelEdge0` 是两个不同强类型，`to_fits/from_fits_centre|edge` 是唯二换算入口（`model.hpp:461-462`）；`cd_rad_per_pixel` 显式写单位；`solid_angle_jacobian` 返回球面面积区间。
- 状态：`make_wcs` 必须给 `domain` 覆盖与单射证书；`bind_wcs` 要求观测足迹的 `detector_frame` 与 WCS 一致，并要求 WCS 域覆盖“整个探测器域”。
- 资源：`make_wcs` barrier，标量 6 op 走 `run_scalar`（barrier），`bind_wcs` barrier（`wcs_process.cpp:600-608`）。
- 实跑：`validation/process/coordinates.wcs`（旧 pass）＋X01 的 0.1491 px。
- **缺陷：P1-2（bind_wcs 内容哈希不再覆盖父观测内容）、P1-3（覆盖率检查用的是 sample 网格 box 而不是 detector 域）、P1-4（`complete=true` 与 “astrometric error unknown” 并存）。**

### 04 catalogue.access（5 op）
- 合同 `catalogue.access.md`；实现 `src/catalogue/catalogue_access_process.cpp`＋`index.cpp`＋`provider.cpp`＋`columns.cpp`＋`sky_geometry.cpp`。
- 入口：`build_catalogue_index(CatalogueBuildRequest) → CatalogueIndex`；`make_catalogue_provider(CatalogueIndex) → CatalogueProvider`；`CatalogueProvider::query(CatalogueQuery) → CataloguePage`；`query_quads(QuadQuery) → QuadPage`；`lookup(span<U64>) → vector<CatalogueStar>`。
- 估计量/单位：星位是 ICRS 单位向量；`query` 把每颗星用 `propagate_star(row, request.epoch_julian_year)` 传播到请求历元（`provider.cpp:384-401,601-604`），`pmra_cosdec` 已按 `μ_α*` 处理（`sky_geometry.cpp:91-94`），视差/径向速度用匀速空间运动（`sky_geometry.cpp:101-124`）；`epoch_year` 随星返回。
- 状态：`lookup` 对缺失 id 追加 `no_support` 但**仍 `complete=true`**（P1-7）；顶心改正作为 omitted 记录，且 `omitted_position_bound_rad` 在无界时置 null（不写 0，符合 unknown 合同）。
- 资源：barrier region；index/provider 构建读全表。
- 实跑：`validation/process/catalogue.access`（旧 pass）＋`validation/cat/*.json`。

### 05 sources.astrometry（1 op）
- 合同 `sources.astrometry.md`；实现 `src/sources/astrometry_process.cpp`＋`astrometry.cpp`＋`astrometry_detection.cpp`＋`astrometry_fit.cpp`＋`astrometry_match.cpp`＋`astrometry_wcs.cpp`。
- `solve_astrometry(Context&, Ref<NativeObservation>, const CatalogueProvider&, const SolverRequest&) → Ref<AstrometricSolution>`。
- 估计量：检测（tile median/MAD 背景、SNR、5×5 极大、NMS）→ 四星组描述符→ 目录 quad 查询→ affine 假设→ 互近邻匹配→ `fit_wcs`（plane+SIP）→ `make_wcs` 准入。输出 `AstrometricSolution{wcs, parameters, parameter_noise, influence, support, model_bias_rad}`。
- 状态：检测数不足→`unsolved`；假设预算耗尽→`unsolved` 且 assessment 明确“搜索耗尽不等于无解”；成功时 validity=`certified`、noise=`conditional`（`astrometry.cpp:864-880`）。
- 资源：barrier，`persistent≈64·candidates+512KiB`，`progress=26·native+128·quads+512·candidates`（`astrometry_process.cpp:196-206`）。**与 solver 内部真实分配无绑定**（P2）。
- 实跑：`validation/process/sources.astrometry`（旧 pass）＋X01（blind RMS 0.2109 px）。
- **缺陷：P1-1（未检查 `query_quads` 结果即 `val()` 解引用）、P1-6/P1-9（参数数组与参数协方差的单位）。**

### 06 sources.psf（3 op）
- 合同 `sources.psf.md`；实现 `src/sources/psf_process.cpp`＋`psf.cpp`＋`psf_fit.cpp`＋`psf_model.cpp`；`bind_observation_model` 在 `src/observation/bind.cpp`。
- `fit_psf(observation,wcs,span<CatalogueStar>,PsfRequest) → EffectivePSF`；`evaluate_psf(psf,PsfEvaluationRequest) → PsfEvaluation`；`bind_observation_model(observation,kernel,response) → NativeObservation`。
- 估计量：像素积分 PSF（`psf_model.cpp:326` 高斯核含 `1/(2π√det)` 归一，光电子积分而非中心采样），参数/区间/尾界；`PsfEvaluation` 同时给 `values/numerical_lower/numerical_upper/native_tail_upper`。
- 状态：`fit` 收敛失败/尾界不达标必须降状态；`bind` 要求 kernel 与观测同 frame、同 detector，且 response 支撑是合法区间、有波长与 throughput 样本。
- 资源：barrier（psf_process 全部 op）＋`EffectivePSF` 快照计入 persistent。
- 实跑：`validation/process/sources.psf`（旧 pass）。
- **缺陷：P1-4 同类（bind 后 `complete=true` 与 unknown 并存），P1-10（`bind_observation_model` 对 `observation->footprints/native_index` 空指针直接解引用）。**

### 07 sources.measure（1 op）
- 合同 `sources.measure.md`；实现 `src/sources/measure_process.cpp`＋`measure.cpp`。
- `measure_science_sources(observation,wcs,span<CatalogueStar>,Ref<EffectivePSF>,PsfRequest) → SourceMeasurements`。
- 估计量：`d=H F+Z β+ε` 的联合 GLS 通量（含背景列）、共享噪声；每星 `StarFlux{flux, variance, window, influence}`；`flux_numerical_lower/upper` 给严格界。
- 资源：barrier；每星窗口矩阵与 joint noise 在 persistent 上界中体现（本次未逐项复算上界）。
- 实跑：`validation/process/sources.measure`（旧 pass）＋X01 的 48/48 残差。
- 本次未发现新的 P0/P1（B2 已覆盖该链）；保留 P2 项：`H` 的 PSF 像素积分与 `PsfEvaluation` 的复用关系未在 adapter 层校验（由 measure.cpp 内部承担）。

### 08 response.integrate（1 op）
- 合同 `response.integrate.md`；实现 `src/response/integrate_process.cpp`＋`api.cpp`＋`spectral.cpp`。
- `integrate_response(Context&, Ref<Response>, Ref<SpectralBasis>, const NumericBudget&) → Ref<SpectralPrediction>`。
- 估计量/单位：photon 约定 `v_kl=∫ b_l R_k λ_nm·1e-9/(hc) dλ_nm`；energy 约定 `∫ b_l R_k dλ`（`api.cpp:6-10`，`spectral.hpp:23-27`）；输出单位 `photon_rate_m2:...`（nm_power=0）或 `watt_m2:...`（nm_power=-1），`linear_power=1`；再叠加浮点求和界 `γ_n·Σ|x_i|`。
- 状态：积分区间被裁到 response 已知范围且尾未知 → `complete=false`＋`unavailable` issue（`api.cpp:568-582`）；面板未收敛→`nonconverged`；两者都不伪装完整。
- 资源：barrier；`max_panels` 由 `max_iterations` 限制；work 记账只 `consume_work(1)`/段（远低于真实面板数，P2）。
- 实跑：`validation/process/response.integrate`（旧 pass）。
- **缺陷：P1-11（`shape_noise` 的 innovations/unknown_terms 被丢弃）、P2-8（top-hat response 的 `support_nm` 被忽略）。**

### 09 photometry.fit（1 op）
- 合同 `photometry.fit.md`；实现 `src/photometry/photometry_fit_process.cpp`＋`joint_fit.cpp`＋`api.cpp`＋`spatial_basis.cpp`。
- `fit_photometry(span<Ref<SourceMeasurements>>, span<CatalogueStar>, span<Ref<SpectralPrediction>>, PhotometryRequest) → Ref<PhotoSolution>`。
- 估计量：共同 Gaia XP 潜变量＋每帧每基函数系数 `theta`（shape[K,P]），按 source 消元后全局 Schur/QR；空间项以训练加权均值为零；参数协方差 `L Lᵀ=C`（`api.cpp:623-641` 用 `sqrt(λ)`，与 `apply_scale.cpp:520-527` 的注释相反，见 P2-1）；`RankReport`。
- 状态：`likelihood.kind` 非 `joint_flux_gaussian` → `unsupported`；无 XP 协方差 → 记录 unknown_terms 且 `covariance_state_known=false`（不当已知协方差发布）。
- 资源：barrier；dense 上限 `dense_max_bytes`。
- 实跑：`validation/process/photometry.fit` 的 2 个 driver 中 **`m5_process_plan.cpp` 在 HEAD 不能编译（P0-3/P1-13）**，`x01_minimal_repro` 可编。

### 10 photometry.apply（1 op）
- 合同 `photometry.apply.md`；实现 `src/photometry/photometry_apply_process.cpp`＋`apply_scale.cpp`。
- `apply_scale(solution, observation, frame) → NativeObservation`。
- 估计量：α(u)=exp(−h(u)·θ) 在**最原始物理父样本**上逐样本相乘，再沿 parent 链用各自 `regrouping` 把值/噪声/transfer 复合到当前层；`source_scale`（源端 a）留在 `EffectivePSF`。
- 状态：frame 不在 solution、已应用同 `application_id`（identity_conflict）、无足迹 → 具名拒绝；噪声缺 θ 随机影响 → `NoiseState::unknown`＋omitted。
- 资源：stream；parent 行块。
- 实跑：**无 process driver**；只在 X01 的 normalize 中被调用。
- **缺陷：P1-0（中位数 gauge 未按 `GAIA_RESPONSE.md:123` 记录）、P1-12（未校验 observation.response 与 solution.responses 一致）、P2-1（注释里的 P1-5 理由已过期）。**

### 11 geometry.overlap（11 op）
- 合同 `geometry.overlap.md`；实现 `src/geometry/overlap_process.cpp`＋`overlap.cpp`＋`containment.cpp`＋`refine.cpp`＋`exact_*`＋`coordinates/footprint_provider.cpp`。
- op：`make_footprint_provider`、`detector_footprint`、`footprint`、`measure`、`compute_overlap`、`refine_geometry`、`footprint.forward_interval`、`footprint.boundary_interval`、`footprint.derivative_interval`、`refinable.open`、`refinable.refine`。
- 估计量：曲边足迹 → 每叶 `SphericalAreaInterval{native_area,drop_area,overlap_area}`＋`GeometryStatus{termination,bound_validity,support,unresolved}`；`measure` 给面积区间；`compute_overlap` 给贡献表；`refine` 产出**新 revision**（不原地改）。
- 状态：预算/精度未达 → `Termination::work/precision` 且 `unresolved`，不返回伪 0 面积（B3 已修）；`SupportState::bounded_undecided` 保留。
- 资源：stream（除 `refine` 的 snapshot 打开）；work/key 由 budget 控制。
- 实跑：`validation/process/geometry.overlap`（旧，**无 comparison.json**）。
- 本次未发现新的 P0/P1；`refine_geometry` 的 `GeometryRevision.consumed` 真实记账未复核（P2）。

### 12 operators.action（7 op）
- 合同 `operators.action.md`；实现 `src/operators/action_process.cpp`＋`api.cpp`＋`linear_action.cpp`。
- op：`make_action`、`apply`、`transpose_apply`、`compose`、`action.dependency_blocks`、`action.serialize_recipe`、`action.row_enclosure`。
- 估计量：线性算符（identity/diagonal/csr/compose/sum/transpose/factor_covariance/solve/sky/selection/leaf_merge）；`row_enclosure` 给逐行 `ExactInterval`；`compose(outer,inner)=outer∘inner`，中间维必须相等（`api.cpp:249-258`）。
- 状态：形状不匹配 → `invalid_model`；无 action → `invalid_request`。
- 资源：stream；CSR 表与对角值计入 payload。
- 实跑：**无 case**（`validation/process/operators.action` 无 cpp）；仅 `acsd_operators_selftest`（lead）。

### 13 uncertainty.covariance（5 op）
- 合同 `uncertainty.covariance.md`；实现 `src/uncertainty/covariance_process.cpp`＋`api.cpp`＋`covariance.cpp`。
- op：`make_covariance`、`covariance_apply`、`quadratic_form`、`geometry_covariance_bound`、`control_covariance`。
- 估计量：`C=F Fᵀ`（共享 root 保留同一 factor）、`vᵀCv`、PSD 约束、几何绝对界；`BoundOrUnavailable` 区分“有界”与“不可得”。
- 状态：`unknown_terms` 携带 influence → `unavailable`；非 PSD → `invalid_covariance`。
- 资源：stream。
- 实跑：**无 process case**；有 `acsd_covariance_selftest`。

### 14 geometry.drizzle（1 op）
- 合同 `geometry.drizzle.md`；实现 `src/geometry/drizzle_process.cpp`＋`drizzle.cpp`＋`kernel.cpp`。
- `build_drizzle(overlaps, observation, DrizzleRequest) → DrizzleResult{op, estimate}`。
- 估计量：`R` 与支持归一、signed 误差、同一 R 作用于 signal/PSF/noise/背景、外存 CSR；`SignalEstimate` 带 `numerical_lower/upper`。
- 状态：几何未决 → 传播 `GeometryStatus`，不把 unresolved 当 0。
- 资源：stream。
- 实跑：**无 process case**；由 X01 normalize 实际调用。

### 15 information.reference（1 op）
- 合同 `information.reference.md`；实现 `src/information/information_reference_process.cpp`＋`information.cpp`＋`internal.cpp`。
- `estimate_flux_information(InformationRequest) → Information{flux_variance, rank}`。
- 估计量：`V_F=cᵀ(RᵀR)⁺c`（白化后自由设计的最小二乘），协方差零空间当精确约束先解；不可估 → `not_identifiable`＋nullopt（`information.cpp:323-341,375-390`）。
- 状态：**`flux_variance.state` 无条件写 `known`（`information.cpp:184,409,428`）**，即使 `covariance->model->state` 是 conditional/approximate、或 unknown_terms 无 influence（P1-14）；`flux_variance.unit` 只抄协方差的 `variance_unit`，模板 `h` 的单位从不参与（P1-15）。
- 资源：barrier；dense 协方差分解。
- 实跑：**无 case**。

### 16 information.scene（3 op）
- 合同 `information.scene.md`；实现 `src/information/information_scene_process.cpp`＋`triangulation.cpp`＋`information.cpp`。
- op：`evaluate_scene_variance(frames,frame,leaves)`、`build_supported_triangulation(controls,chart,support)`、`reconstruct_scene_variance(controls,triangulation,queries)`。
- 估计量：逐叶 `V_B`（面亮度方差，sr^-2）；三角化必须完全落在观测支持内；holes 保留；`VariancePrediction{values,lower,upper,support_states,conditional}`。
- 状态：controls 非 actual-scene → `invalid_request`；chart 半径超首期上限 → `unsupported`（`information_scene_process.cpp:167`）；<3 控制点 → `unavailable`。
- 资源：barrier（evaluate/reconstruct）与 stream（build 的候选过滤）。
- 实跑：**无 case**。

### 17 sky.controls（1 op）
- 合同 `sky.controls.md`；实现 `src/sky/sky_controls_process.cpp`＋`sky.cpp`。
- `build_sky_controls(const FramesProvider&, const SkyRequest&) → SkyControls`。
- 估计量：`c=T y=H s+A β+e`，`C_c=T C_y Tᵀ`；默认 patch 构造器给每个 patch 一列共同天空（`H(row,patch)=1`，`sky.cpp:1250-1258`）；只有 `response_proof` 非空的组才可比较（`sky.cpp:1094-1098`），异谱不造差模边。
- 状态：无 comparable group → `invalid_request`；`K H=0` 的 gauge 约束必须满足。
- 资源：barrier（连通性/基冻结）＋stream（patch 装配）。
- 实跑：**无 case**（目录空）；只在 X01 mosaic 中被调用。

### 18 sky.fit（2 op）
- 合同 `sky.fit.md`；实现 `src/sky/sky_fit_process.cpp`＋`sky.cpp`。
- `fit_relative_sky(controls,request) → SkyFit`；`apply_sky(signal,fit[,frame]) → SignalEstimate`。
- 估计量：`[H,A G]` 联合 GLS＋gauge/零空间；`P=I−E K T`；`apply_sky` 按**叶身份**定位 E 的行块，绝不按长度猜（`sky.cpp:2595-2610`）；两重载在 `sky.cpp:3098-3106`，registry 只登记带 frame 的 `ApplySkyInput`。
- 状态：断连/gauge 未定 → `geometry_solve_unresolved`/未决；`apply_sky` 失败具名。
- 资源：barrier（fit）＋stream（apply）。
- 实跑：**无 case**；只在 X01 mosaic 中被调用。

### 19 coadd.select（1 op）
- 合同 `coadd.select.md`；实现 `src/coadd/coadd_select_process.cpp`＋`coadd.cpp`。
- `select_samples(frames,request) → Selection`。
- 估计量：DQ/支持/可比较性硬筛选＋`single_max_loo`；LOO 方差 `Γ_kk+tᵀΓ_rest t−2Γ_{k,rest}t`（`coadd.cpp:838-860`），Bonferroni 阈值 `normal_quantile(1−α/(2n))`；最多拒一个、只拒一次；数值区间跨阈值 → `selection_unresolved` 而非硬判（`coadd.cpp:868-880`）。
- 状态：mixed observation 或无外部固定 Γ → `rejection_note`，`single_max_loo` 不执行（不伪造）。
- 资源：stream。
- 实跑：**无 case**；mosaic 通过 registry 真实调用（`src/workflows/mosaic.cpp:349`）。

### 20 coadd.weights（1 op）
- 合同 `coadd.weights.md`；实现 `src/coadd/coadd_weights_process.cpp`＋`weights.cpp`＋`coadd.cpp`。
- `solve_weights(frames,selection,request) → WeightSolution`。
- 估计量：BLUE `t=Γ⁻¹1/(1ᵀΓ⁻¹1)`；PSD 零空间当精确约束（最小范数单位和解，`weights.cpp:96-122`，方差精确 0）；非负 QP 用 active-set/Bland；`WeightPolicy::automatic` 按可比较性降级并具名。
- 状态：`RankReport`/`condition`；不可解 → `rank_unresolved` 传播，不返回伪权重。
- 资源：stream；Γ 稠密块按叶。
- 实跑：**无 case**；mosaic 经 registry 调用（`mosaic.cpp:363`）。

### 21 coadd.integrate（1 op）
- 合同 `coadd.integrate.md`；实现 `src/coadd/coadd_integrate_process.cpp`＋`coadd.cpp`（`coadd_with_inputs`）。
- 类型入口 `CoaddInput → CoaddResult`；自由函数 `coadd(frames,sky,request)` 只是“先 select/weights 再进同一 core”（`coadd.cpp:1761-1780`）。
- 估计量：只消费已封存 `Selection`/`WeightSolution`，不内部二次求权；输出 `CoaddResult{estimate,selection,weights,sky}`。
- 状态：`validate_coadd_inputs` 只读元数据；`block_elements` 必须覆盖全部叶（whole-frame core）。
- 资源：stream；`kFrameViewBytesPerPair=11`、`kCoefficientBytesPerPair=8`。
- 实跑：`validation/process/coadd.integrate`（旧 pass）＋X01 mosaic 1/1。

### 22 projection.export（1 op）
- 合同 `projection.export.md`；实现 `src/projection/export_process.cpp`＋`export_projection.cpp`；工作流 `src/workflows/export_workflow.cpp`。
- `build_export(product,ExportRequest) → ExportResult{Q, estimate, target}`。
- 估计量：Q 行为目标像素对叶的**面积权重**：`supported_mean` 用 `overlap/total`（单位 sr^-1）、`supported_flux` 用 `overlap`（单位不变）、`extrapolated_full_flux` 用 `native_area·overlap/total`；不支持像素写 **NaN**（不是 0）；负值/有限零保留。`pixfrac=1` 不二次收缩。
- 状态：几何 `Termination != tolerance_met` → `resource_exhausted`/`precision_exhausted`/`unsupported`，不发布与基线不同的 payload；`extrapolated_full_flux` 需显式 `assume_uniform_uncovered`。
- 资源：stream；目标像素数 checked。
- 实跑：**无 process case**；X01 export 1/1＋`validation/projection/export_constant_probe.cpp`（新加，非 process driver）。
- **缺陷：P1-16（数值界漏掉 overlap 面积区间）、P1-17（`effective_area_approx` 与 quantity 脱钩）。**

## 2. P0 清单（按“是否阻塞 B0–B5 已完成链／X01·full 产品”排序）

> 说明：本轮**没有**发现 X01 链上能给出“正确实现给出错误数值”的 P0 算法错误；P0 集中在**身份**与**可复现证据**两处。原本列为候选 P0 的 `photometry.apply` 中位数 gauge，经核对 `GAIA_RESPONSE.md:123` 已被科学正本**明确规定**，故降为 P1-0（真正的缺陷是“未按要求记录”而非公式）。

### P0-1 bind_wcs / bind_observation_model 的内容哈希不覆盖父内容 → 身份冲突
- 位置：`src/observation/bind.cpp:28-33`（`revise()`）＋`:95-97`（bind_wcs）＋`:190-192`（bind_observation_model）。payload 分别是 `"native:"+frame+":"+native_index->size+":"+wcs_hash` 与 `"native:"+frame+":"+native_index->size+":"+kernel_hash+":"+response_hash`。
- 反例：同一 `frame`、同 `native_index->size`，两次标定（不同 master flat）得到像素值完全不同的 `NativeObservation`；各自 `bind_wcs(...,同一 WcsModel,...)` ⇒ **`content_hash` 与 `object_id` 完全相同**。产品闭包、按 hash 去重、重开校验都把两份不同数据当同一对象；反之更改 DQ/noise/像素值都不会改变绑定后的身份。
- 这正是 `SYSTEM.md:43`（跨进程身份/产品重开）和 OBJECTS 对 `content_hash` 的要求；当前实现只保证“同一 WCS 的绑定结果稳定”，不保证“不同内容不同身份”。

### P0-2 当前 HEAD 的全量 science 构建不可复现（57 op driver 不能整套重编）
- 现象：`ninja -C build/science` 在 `validation/process/photometry.fit/m5_process_plan.cpp:79` 失败；`MetaAction` 缺少 `LinearAction` 新增的两个纯虚（`include/acsd/model.hpp:205,207`）。
- 影响：可以构建库、`acsd`、`acsd_typed_smoke`，但**无法重建全部 process driver**，因此 57 op 的“当前源码 PASS”不能重新生成；现有 `validation/process/*/comparison.json` 全是 2026-10-24 旧基线。
- 另一条同源事实：`check_design` 的 `tools/generate_contract_schema.py:129` 解析 `Unit` 失败，生成器与 `model.hpp` 已脱节（见 P2-3）。
- 分类说明：它不阻塞 `acsd normalize/mosaic/export` 运行（X01 仍可跑），但阻塞“B0–B5 完成后逐 op 复核”的证据链，故列 P0（证据/可复现），不列“产品错误”。

## 3. P1 清单

**P1-0 `photometry.apply` 的帧中位数 gauge 未按科学正本记录（相对标度被当成已应用的光度定标）**
- 科学正本：`docs/science/foundation/GAIA_RESPONSE.md:123` 明确“`apply_scale` 取帧内因子中位数为 gauge（α←α/median α），只应用相对 α；**该选择必须记录在 assessment 中，不能把相对标度写成绝对物理标定**”。因此中位数本身不是缺陷，**未记录**才是。
- 代码：`src/photometry/apply_scale.cpp:298-308` 执行 gauge；`:549-555` 的 `included` 只有 `per_native_sample_spatial_factor`/`physical_regrouping_after_the_factor`/`noise_chain_scaled_with_the_same_factor`/`the_unit_names_the_applied_photometric_scale`，`:566` 的 `omitted` 只有 θ 随机影响——**没有任何条目说明“已按帧中位数归一、只保留相对 α”**。同时 `:416-418` 把单位 `scale` 改名为 `...*scale/<application_id>`，读起来像已完成绝对定标。
- 最小反例：某帧 `θ₀=ln 2`、空间项全 0（该帧计数系统性是 Gaia 预测的 2 倍）⇒ α≡0.5，median(α)=0.5，除后 α≡1：整帧的全局定标误差被完整保留，而产物 assessment 的记录方式与“已应用光度标度”不可区分。
- 跨链风险（不是本条独有的错，但需具名）：`source_scale`（源端 a）只进 `EffectivePSF`，`workflows/mosaic.cpp` 与 `src/coadd/coadd.cpp` 不消费它；两帧 θ₀ 不同时，mosaic/coadd 直接混合各自原始水平的图像。基础文档已承认绝对标度不可辨识，因此这必须作为**产品 profile/omitted 的显式限制**出现（例如 `allow_relative_uncalibrated` 或 `relative_scale_gauge=frame_median`），不能只在 `condition="photometric_scale"` 里含混过去。

**P1-1 `solve_astrometry` 未检查 `query_quads` 结果就解引用 `Result` 的 optional（UB／崩溃）**
`src/sources/astrometry.cpp:615-617`：`Result<QuadPage> candidates = catalogue.query_quads(...); for (const QuadMatch& match : val(candidates).matches)`。`val()` 是 `*result.value`（`src/model/result.hpp:17-20`），失败时是空的 `std::optional` ⇒ 未定义行为。合法触发路径：低 work/H 预算下 `query_quads` 返回 `resource_exhausted`、请求非法时返回 `invalid_request`；正确行为应是具名 Issue，而不是崩溃或读脏内存。

**P1-2 `bind_wcs` 的“覆盖整个探测器域”检查比较的是 sample 网格而不是 detector 域**
`src/observation/bind.cpp:77-87`：用 `width/height = footprints->width/height` 当 detector 域，忽略 `FootprintSet.detector_from_sample/offset/local_cell`。反例：2×2 bin 后 `width=height=50`、`detector_from_sample=[[2,0],[0,2]]`、`offset=(0.5,0.5)`，真实 detector 域是 `[0,100]²`；WCS 域 `[-0.5,49.5]²`（只盖住左上 1/4）通过检查并被 `bind_wcs` 接受，产物却声称 WCS 覆盖全探测器。downstream 的 `overlap/drizzle` 用真实足迹，会在未覆盖区得到 `unresolved` 或缺叶，而 `bind_wcs` 已把它标成 `wcs_bound_to_the_observation`。

**P1-3 bind 的 `complete=true` 与“unknown”并存**
`src/observation/bind.cpp:99`（bind_wcs）与 `:195`（bind_observation_model）无条件 `assessment.complete = true`；紧接着 bind_wcs 在同一次调用里可能写 `omitted=astrometric_parameter_error_is_unknown` 并 `add_issue(Code::unavailable,...)`（`:100-118`）。按 `ALGORITHM_CONTRACTS.md:24`，条件性结论不能变成“完整”；`SYSTEM.md:120` 也要求 unknown/partial 反映到 Assessment。反例：`solution->parameter_noise=null`、`influence.from_measurements=null` 的解，绑定结果 `complete=true`，消费者据此认为误差项齐全。

**P1-4 `bind_observation_model` 对 `observation->footprints`/`native_index` 空指针直接解引用**
`src/observation/bind.cpp:162` 读 `observation->footprints->detector_frame`，`:191` 读 `observation->native_index->size`，两处都没有判空；而 `calibrate_observation` 会把 `raw->footprints`（可为 null）原样搬进 native。合法输入（无足迹的校准观测）即可触发 UB。

**P1-5 regroup 的 `input_is_mean` 物理和分支维度/顺序不对，n≠m 时必失败**
`src/observation/regroup.cpp:314-325`：`compose(context, scale, request.G)`，而 `compose(outer,inner)=outer∘inner` 且要求 `outer.domain == inner.codomain`（`src/operators/api.cpp:249-258`）。`scale` 是 n×n、`G` 是 n→m。典型 2×2 bin（n=4m）⇒ 中间维不匹配 ⇒ 返回 `invalid_model`，`input_is_mean && physical_sum` 这一被声明支持的组合永远不可用。若 n==m，则实际构造的是 `scale∘G`（先 G 后乘常数），虽然标量可交换、数值碰巧相同，但语义与注释“先用 member count 还原积分再做 G”相反。正确写法应是 `compose(G, scale)`（G∘S）。

**P1-6 参数/派生数组的“单位”被声明成 dimensionless，混合单位的 double 被同一 Unit 覆盖**
`solve_astrometry` 把 `crpix_x/y`（px）、`ra0/dec0`（rad）、`cd[4]`（rad/px）、`sip_a/b`（px^-(i+j-1)）拼成一个数组并以 `model::unit_dimensionless()` 存储（`src/sources/astrometry.cpp:802-819`）；同一参数块的 `NoiseModel.variance_unit = unit_variance("astrometric_parameter")`（`astrometry.cpp:329`）。后果：`covariance_apply`/`quadratic_form`/`geometry_covariance_bound` 无法区分“px²”“rad²”“rad²/px²”，任何跨参数 functional 的单位推导都是错的；这与矩阵文档“不允许将相同单位编码成无语义 double”和 `ALGORITHM_CONTRACTS.md:7`（输入单位/index）直接冲突。最小反例：`c=(1,0)` 作用在 CRPIX 上应有 px²，作用在 RA 上应有 rad²，当前都报同一个 `astrometric_parameter` 单位、且数组本身 dimensionless。

**P1-7 `catalogue.lookup` 缺 id 仍 `complete=true`**
`src/catalogue/provider.cpp:574`（`assessment.complete = true`）与 `:583-587`（缺 id 时 `no_support`＋continue）。反例：请求 `[id1,id2]`、`id2` 不在索引 → 返回 1 颗星、`Code::ok`、`complete=true`。调用方（例如按 id 取 XP 系数/位置的下游）无法从 `complete` 区分“完整解析”与“部分缺失”，属于 unknown 被当完整。

**P1-8 regroup 在父观测无区间时把名义值当精确区间**
`src/observation/regroup.cpp:293-297`：`if (!have_bounds)` 时 `row_lower[r]=row_upper[r]=regrouped[r]`，即 `[v,v]`；`ErrorRecord.complete=true`（`regroup.cpp:406`），只在 Assessment 里留一句 `the_parent_carries_no_numerical_bounds`。下游（drizzle/information/export）若以 `numerical_lower/upper` 作严格界，会把“无数值误差信息”当成“数值误差恰为 0”。这是 `unknown`→`0` 的典型形态，应改为 null 或使 `ErrorRecord.complete=false`。

**P1-9 `information.reference` 把条件/近似协方差发布成 `known`，且方差单位不随模板 `h` 传播**
`src/information/information.cpp:184`、`:409`、`:428` 无条件 `state=known`；`covariance_has_unknown_term()`（`src/information/internal.cpp:151-157`）只看带 influence 的 `unknown_terms`，**不看 `NoiseModel.state`**。反例：`estimate_flux_information` 用 `AstrometricSolution` 的 `parameter_noise`（其 state 明确为 conditional）作为 covariance → 返回 `V_F` 且 `state=known`。单位：`flux_variance.unit = covariance->model->variance_unit`（`:181-183`），完全没有 `h` 的量纲；`V_F=cᵀ(RᵀR)⁺c` 的量纲是 `[cov]·[h]⁻²`，当 `h` 不是无量纲时当前单位必错。

**P1-10 `projection.export` 的数值界不含 overlap 面积区间**
`src/projection/export_projection.cpp:272-275`（面积取区间中点）＋`:431-437`（把 Q 作用到叶值下/上界得数值界）＋`:452-453`。Q 的系数本身就是有区间的几何量（`SphericalAreaInterval`），但数值界只传播了叶值误差、把面积当中点常数。反例：单叶，叶值 `[1,1]`，该叶与目标像素的 overlap 面积区间 `[0.5,1.5]`，`quantity=supported_flux` ⇒ 真值落在 `[0.5,1.5]`，而发布 `numerical_lower=upper=1.0`。产物声称“数值包含”，实际不包含；export 的 `supported_flux/native_area·…` 分支都受影响。

**P1-11 `integrate_response` 丢弃 `Response::shape_noise` 的 innovations 与 unknown_terms**
`src/response/api.cpp:554-560` 只把 `shape_noise->shared_factors` 的 `shared_id` 抄进 `SpectralPrediction.shared_spectral_error`；`innovations`、`unknown_terms`（含带 influence 的未知项）全部丢失，而 `assessment.complete` 在非截断时为 true。反例：某 response 的 throughput 形状误差建模为 innovation root（仪器 QE 逐次标定误差），`integrate_response` 发布的 `basis_integrals` 误差只有求积误差，photometry 联合拟合会把该形状误差当 0 → 通量误差偏小。

**P1-12 `photometry.apply` 不校验观测的 response 与解一致**
`src/photometry/photometry_apply_process.cpp:106-171` 与 `src/photometry/apply_scale.cpp:172-200` 检查了 frame 成员、重复 application_id、足迹、数组形状，**没有**检查 `observation.response` 是否等于 `solution.responses[frame_index]`（`photometry.apply.md` 的“公开操作语义”明确要求“原单位与模型匹配”）。反例：用滤光片 A 的响应拟合的解，应用到同一 frame id、但用响应 B 标定的观测：α 由 B 的光谱形状算出，`unit` 被改名为 `…*scale/<application_id>`，产物看起来“已定标”，实际光谱基准错误。

**P1-13 patches/证据面：11 个 process 没有 process driver，8 个 comparison.json 是旧基线**
（工程证据，非算法错误）`coadd.select/weights`、`information.reference/scene`、`photometry.apply`、`projection.export`、`sky.controls/fit`、`uncertainty.covariance`、`operators.action`、`geometry.drizzle` 无 driver；其余 8 份 pass 记录 mtime=2026-10-24。当前 X01 只覆盖 `calibrate→regroup→astrometry→bind_wcs→psf→measure→response→photometry→apply_scale→geometry/drizzle→sky/coadd→export` 的一条链，其余 21 个 process 仍是 `IMPLEMENTED_NOT_VERIFIED`。

## 4. P2 清单（不阻塞链，但会造成误判或返工）

1. **`apply_scale.cpp:512-541` 的 P1-5 理由已过期**：它说 `photometry/api.cpp` 用 `1/sqrt(λ)` 构造因子，实际 `api.cpp:623-641` 已用 `sqrt(λ)` 并注释说明。于是“θ 随机影响不能进图像噪声”的**技术理由不成立**（省略本身仍是 P1-12 之外的已知缺口）。错误注释会误导下一轮修复。
2. **regroup 的 `old_from_new/offset` 只用来查行列式**（`regroup.cpp:93-97`）：既不与 `G` 的行结构比对，也不与 `output_footprints->detector_from_sample/offset` 比对（`FootprintSet` 里就存着同一变换）。物理 `physical_collection=true` 的“采集域/面积/chirality”因此只由调用者自证；Bayer/半像元/镜像错误不会被发现。矩阵文档第 02 行要求的“手性/物理面积”没有被验证。
3. **生成头与生成器脱节**：`include/acsd/modules/generated/inputs.hpp:385-391` 的 `CoaddInput` 有 `selection/weights`，而 `model.hpp:511` 的 `coadd()` 只有 `(frames,sky,request)`；`:363-369` 的 `ApplySkyInput.frame` 与 `model.hpp:507-508` 的两个重载也不一致。头注释声称“从 model.hpp 机械生成”，实际是旧声明；`check_design` 已不能解析 `model.hpp`（P0-3 同源）。这份公开头是 codec/schema 的真实输入，必须先恢复生成器再改声明。
4. **`src/runtime/generated/typed_operations.cpp` 是 57 个 op 的死代码**：`integrated_processes()`（`builtin_processes.cpp:188-215`）使 22 个 process 全部走模块注册，生成的 shell（含 `"not integrated before M5"` 等旧字符串，例如 `:4368,4554-4556`）只参与编译不参与运行。它制造了“同 op 第二套 plan/run 语义”的错觉，也让 grep 审查误判。
5. **`solve_astrometry` 的 `best_residual` 混用像素与弧度**：`astrometry.cpp:654` 存入 px，`:778` 把 `fit.rms_rad`（rad）也塞进同一变量再比较。该变量目前只做诊断、不进最终 reason，故为 P2；但这是“同一 double 两种单位”的实例。
6. **标定分支条件三处重复**：`calibration_plan.cpp:174-181`、`calibration_kernel.cpp:307-311`、`calibration_signal.cpp:692-697` 各自重写 `has_gain/has_read/poisson_plugin`。现在三处一致，但任何一处改动都会让 plan 的资源上界与实际噪声分支不符（`SYSTEM.md:82` 要求一份实现）。
7. **`regroup_work` 的 1M work 地板**（`regroup_process.cpp:190-197`）：小图（如 32×32）也会申报 `≥1<<20` work，低预算下会先 `resource_exhausted`，属于“预算改变可执行性”的隐患。
8. **top-hat response 忽略 `support_nm`**：`load_response` 里 `support_nm` 只收缩 `known_*`（`api.cpp:294-297`），而 `plan_integration` 对 top-hat 用 `band_*`（`:329-336`）⇒ 调用者声明的更窄 support 不生效；sampled response 则生效。同一字段两种语义。
9. **`coadd()` 与 typed `coadd` 两套入口语义**：`coadd.cpp:1761-1780` 先 `select_samples`/`solve_weights` 再进核心，typed op 消费封存对象。同一 core 无分叉，但同一 process 存在“会重算 Selection”的自由函数入口；若工作流某处混用，`Selection` 与 `WeightSolution` 的版本一致性只能靠调用者自觉（`coadd.integrate` 的 `validate_coadd_inputs` 只查元数据）。
10. **`make_assessment` 默认 `validity=certified`**（`src/model/units.cpp:269`）。独立探针 `/tmp/c0/assess_probe.cpp`（只 include 公开 `acsd/model.hpp`）实测：`Assessment{}` → `validity=0 (certified)`、`noise=0 (known)`、`complete=0`。任何忘记覆写的模块都自动发布“certified/known”。B1 已把超越函数定位为条件性界，这与 `ALGORITHM_CONTRACTS.md:24` 相反，应改成最保守值（`invalid`/`unknown`）。
11. **单位判据两套语义**：`unit_is_dimensionless()` 接受空 `scale`（`src/model/units.cpp:46-52`），`units_equal()` 按字符串比较 `scale`（`:37-43`），而 `Unit{}` 的 `scale=""`（探针实测）。同一“默认/未设置 Unit”在一处算无量纲、另一处不等于 `unit_dimensionless()`（`scale="1"`）；`calibration_process.cpp` 的 `same_layout` 又硬编码 `scale=="1"`。默认 `Unit` 因此是“既无量纲又不相等”的第三态，容易把缺单位当合法单位。

## 5. 只在注册/声明层 vs 有实跑证据

| 类别 | process |
|---|---|
| **有当前源可重建的 driver 且旧 comparison 为 pass** | coordinates.wcs、catalogue.access、observation.regroup、sources.astrometry、sources.psf、sources.measure、response.integrate、coadd.integrate（8 个；证据 mtime 2026-10-24） |
| **有 driver 源码但无 comparison / 无法编译** | calibration-faults（3 driver，无 comparison）、photometry.fit（`m5_process_plan` 在 HEAD 编译失败；`x01_minimal_repro` 可编）、geometry.overlap（1 driver，无 comparison） |
| **只在注册/声明层（无 process driver，仅可能被 X01 覆盖）** | photometry.apply、projection.export、sky.controls、sky.fit、coadd.select、coadd.weights、geometry.drizzle（被 X01 normalize 调用）、information.reference、information.scene、operators.action、uncertainty.covariance |
| **有全链实跑** | 只有 X01 链（normalize 8 帧→mosaic→export），`validation/x01/observed/*`，2026-11-11 04:14；`signal_relative_error=0.1033`（原 5% 判据不过，现按 4σ 归一化判据 `source_recovered=true`） |

另外：**22 个 process 的 plan 形态**（barrier/stream）已与 adapter 核对；11 个无 driver 的 process 连“plan/run 在真实 invocations 上跑通”的记录也没有，只有 `typed_smoke` 证明其 codec/绑定存在。

## 6. 下一轮修复顺序建议（≤8 项）

1. **修 bind 的 content_hash 载荷（P0-1）**：`revise()` 的 payload 必须包含父 `identity.content_hash`（以及被替换的足迹/模型身份），并补“同 frame 同 size 不同值 ⇒ 不同 hash”的负例；否则产品闭包/重开/去重从一开始就不可信。
2. **恢复 HEAD 可全量构建（P0-2）**：给 `m5_process_plan.cpp` 的 `MetaAction` 补 `dependency_arrays/dependency_children`，并修 `generate_contract_schema.py` 对 `Unit` 的解析；这是重新生成任何逐 op 证据的前提。
3. **按科学正本记录 apply_scale 的 gauge（P1-0）**：把 `relative_scale_gauge=frame_median` 写进 `included/omitted`（以及 full profile 的发布条件），并在文档里明确 mosaic 只能消费相对标度；不要求改公式。
4. **消除 astrometry 的未检查 `val(candidates)`（P1-1）**：改为 `if (!ok(candidates)) {记录 Issue; continue/终止}`，并加一个 `query_quads` 失败注入负例（现路径是 UB）。
5. **补 bind 的域检查与空指针前置条件（P1-2/P1-3/P1-4）**：用 `detector_from_sample/offset/local_cell` 变换后的真实 detector box 做覆盖检查；对 `footprints/native_index` 判空；`complete` 只在真的无 unknown 时置 true。
6. **改 regroup 的 `input_is_mean` 组合与无区间语义（P1-5/P1-8）**：`compose(G, scale)` 并加 n≠m 正例；父无区间时输出 null 界并让 `ErrorRecord.complete=false`。
7. **修 information/export 的 unknown 与单位（P1-9/P1-10/P1-6）**：`covariance_has_unknown_term` 纳入 `NoiseModel.state` 与无 influence 的 `unknown_terms`（`apply_scale` 就产出这种项）；`V_F` 单位带上 `h` 的量纲；export 数值界把 overlap 面积区间一起传播；混合单位参数数组按块拆开或给块级 Unit。
8. **补齐 11 个无 driver 的 process 的独立负例（P1-13）**：优先 `information.*`（conditional→known）、`photometry.apply`（gauge 记录/重复 application/response 不匹配）、`projection.export`（面积区间/NaN）、`sky.controls`（异谱 comparability 与断连）；每项至少一个能反驳错误实现的最小输入＋推导真值。

## 7. 我不能验证的边界

- 未做数值重跑：除 `typed_smoke`（1255 checks）与 registry dump 外，未重跑 8 份 2026-10-24 的 process comparison，也未在 HEAD 重跑 X01（当前 X01 证据为 2026-11-11 04:14、源码与 HEAD 相同，但未由我执行）。因此“旧 pass 是否仍成立”只在代码层判断，未给运行证据。
- 未复核每个 adapter 的资源上界是否真实覆盖 allocator 峰值（`kWorkspaceUpper` 等常数、MPFR/GMP、Eigen 临时、字符串/向量）——“plan 与实测 native 相符”只对 calibration/regroup/astrometry 读过代码，其余只核对了形态。
- 未审 `src/numerics` 原语本身（属 B1/N-01..N-06 范围）；本报告只引用其“条件性包含”现状，没有重新证明。
- 未审 `src/sky/sky.cpp` 的 3100 行全部数学（只读了 controls/H、fit 的 `[H,A G]`、`apply_sky` 的行定位与 gauge 注释）；`P=I−E K T`、gauge 秩证书只在注释层核对。
- 未审 `src/geometry/drizzle.cpp`、`src/geometry/containment.cpp` 的数值核（B3 范围），只核对了 process 层状态传播。
- 未验证 Windows/MSYS2 构建、ASan/UBSan/TSan、低 H/取消/重开在 21 个未覆盖 process 上的行为。
- 未改 `src/`、未改他人 docs、未 commit；探针仅 `/tmp/c0/reg_dump.cpp`。
