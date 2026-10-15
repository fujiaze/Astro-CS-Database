# Normalize 完整收敛 — 第一轮（A/D 批）事实报告

分支 `audit/acsd-science-docs-p5`；基线 `12204600edbad8e23ba9e8fe4cbc8427efee0bf9`。
本轮提交：`9a2529ea9`（数学/算子/持久化）、`efe280b45`（证据）+ 本轮 F07 提交。
本报告只列**实际实现位置**、**真实生产消费链**、**实测读数**与**未完成/受限项**；
不以测试数量、exit=0 或 Agent 自称完成代替科学正确性。

## 1. 读取的正本（按 AGENTS §3 顺序）

| 层 | 文件 |
|---|---|
| 最高设计 | `docs/ACSD_DESIGN.md` |
| 科学 | `docs/science/normalize/NORMALIZE_SCIENCE.md`、`docs/science/noise_snr/NOISE_SNR.md`（§3/§11.1/§11.2/§11.3）、`docs/science/photometry/PHOTOMETRY.md`、`docs/science/psf/PSF.md`、`docs/science/drizzle/DRIZZLE.md`、`docs/science/calibration/CALIBRATION.md`+`CCD_DEFECT.md`、`docs/science/detection/ASTROMETRY.md`+`STAR_DETECTION.md` |
| 工程 | `docs/engineering/contracts/NORMALIZE_DATA.md`、`docs/detail/normalize/contracts.md`、`docs/engineering/architecture/NORMALIZE_PIPELINE.md`、`docs/engineering/governance/NORMALIZE_CONVERGENCE.md`、`docs/engineering/architecture/DEV_MODE.md`、`docs/engineering/standards/DIAGNOSTIC_PROBES.md` |
| 任务包 | `实验/normalize-precision-p5/NORMALIZE_COMPLETE_TASK_PACKAGE.md`、`INDEPENDENT_REVIEW_950C261.md` |

从正本取得的关键口径（判据/常数/合同）：
- 球面 GLS 真值 `V_F=[(A^T C^{-1}A)^{-1}]_{FF}`，`A=[h,Z]`；R 满列秩时
  `M = A_src^T V^{-1} A_src`（NOISE_SNR §11.1 条件信息恒等式），且
  `B = V^{1/2} G A_src`、`M = B^T H^{+2} B`（§11.1 明示 `G 与 V^{1/2} 一般不交换`）。
- 最终信号算子 `R_pj = w_jp/N_p`（§11.2：写盘前 flux 载荷 `w_jp D_p/N_p` 仍缺一次
  除以 `D_p`）；`N_p` 必须来自同次生产叶的**完整**有效样本，不在小窗口重新归一。
- 源泊松 §11.3：α 标度后 `α^2 F_native P/g`，以 `F_applied` 表示即 `α F_applied P/g`；
  确定性统一标度下 F 与 √V_F 同比 ⇒ SNR 不变。
- 控制点状态：`available|unavailable`，`accuracy=conditional_complete|approximate|
  upper_bound`，`omitted_terms` 具名；缺 source Poisson 且模板/域固定才可称 `upper_bound`。
- 帧摘要 = 支持面积加权 log SNR 中位数取 exp。
- detail §7.4：`h` 长度 nl、`C` 为 nl×nl row-major、`Z` 为 nl×n_bg、`covariance_rank/
  condition_number` 指 C 的保留子空间；目标 leaf 由统一 NESTED ang2pix 求得，编号 0 合法。
- detail §7.2：`frame_overrides` 按 input occurrence 一次解析，优先级 显式逐帧 > block
  继承 > 允许的真实元数据推导，来源逐项保留；滤镜头名称只是观测记录。

## 2. F01–F06 / F07 修复位置与实测

| 项 | 实现位置 | 修复内容 | 实测读数 |
|---|---|---|---|
| F01 | `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp` `retained_subspace_quadratic` + GLS 块 | 矩阵次序 `B=V^{1/2}GA_src`（旧 `GV^{1/2}A_src`）**且**幂次 `H^{+2}`（旧只用一次 `H^{-1}`） | 独立审查反例：修复前 `V_F=0.89692823838151814`（rel err −0.66116）；修复后 `V_F=2.6470588235294117`，与解析真值 `1/(0.36/1+0.16/9)` **逐位一致**（rel 0.0） |
| F02 | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 捕获判据；`lib/infrastructure/scheduler/src/normalize_precision.cpp` `build_patch_windows` | 捕获判据 = 「该叶是控制点目标叶」（去掉按控制点半径过滤源像元）；`c_lj=(a_lj/A_drop,j)/N_l`，`N_l` 由同次生产该叶**全部**贡献像元汇总 | 真实生产：控制点 `leaf_order=18`（=log2(262144)，即 tile 阶+9，正确）、`operator coverage of N_p=1.000000`、`enclosed profile sum=1.000000`；量纲反例（Ω=a=2 sr，N=D=2 sr）现在 `R=1/(2 sr)` 而非 1 |
| F03 | `sparse_reference.cpp` LocalWindow 段 + `normalize_science.cpp` serializer | `h` 长度 nl、`C` nl×nl row-major（`R diag(v) Rᵀ`）、`Z` nl×n_bg、`covariance_rank/condition_number` 指 C 的保留子空间 | 反例 m=2,nl=3：修复前 `h` 长 2、`covariance_rank=1`；修复后 `h` 长 3、`covariance_rank=2`（真值）。真实产物 `p0_r0.psf.json`：`values=798`、`leaf_ipix=798`、`unit=sr^-1`；`p0_r0.covariance.json`：`unit=adu^2/sr^2`、`factor_length=633` |
| F04 | `sparse_reference.cpp` 源泊松项 + `SphericalCompletionConfig`（`plane_scale`/`pixel_scale`/`gain_provenance`）+ `normalize_precision.cpp` cfg 绑定 | applied 面 `(α_j²/a_ref)·F_applied·P_j/g`；原生 gain provenance 单独保留 | α=0.5、F_applied=500、g=2：`V_F=136.67918376202016`，独立源空间真值 `136.6791837620199`（rel 1.9e-15）；SNR(native)=SNR(α=0.5)=`42.76798765528369`；构造"重复缩放 α²"方差给不同答案（判别力存在） |
| F05 | `lib/algorithms/integration/normalize_science/src/normalize_science.cpp` reader | 恢复 SIP 系数（a/b/ap/bp → i*6+j）、attempt 记录、photometry/reference/calibration/drizzle 身份、models 的 kind/domain/representation、control 的 leaf/leaf_order/refs/accuracy/omitted_terms/frame_summary；`findings` 非空 ⇒ `ok=false`（不再"识别出损坏仍返回可消费成功"） | `frame_science_sidecar_test` 24 项全 PASS（含 corrupt/unsupported/legacy 负例） |
| F06 | `sparse_reference.h`（`has_target_leaf`/`leaf_n_full`/`leaf_n_covered`）、`normalize_precision.cpp` | 目标叶由统一 `acsd::healpix::ang2pix_nest` 求得；leaf 0 合法；算子截断（窗口对 N_p 覆盖不足）记 `operator_window_truncation` 并把 accuracy 降为 approximate | 反例：leaf 0 作目标叶 ⇒ available 且 `leaf_ipix` 保持 0；`has_target_leaf=false` ⇒ unavailable（不静默当 0） |
| F07 | `lib/infrastructure/scheduler/src/module_adapters.cpp` `p1_resolve_frame_binding` + 拟合请求 + `p1_phot.json` | 按 input occurrence 解析 `photometry.frame_overrides[]`（input 路径 / basename / frame_key），逐帧替换 `freq.filter_name`/`freq.qe_name`/`declared_filter_passband`；来源 `explicit_frame_override|block_inherit` 逐项落盘；缺曲线 ⇒ 具名全局配置失败（不套 block 第一帧） | 代码路径已接通（见 §4 证据）；真实混合滤镜运行**未执行**（见 §6 数据受限） |

### 2.1 顺带修复的 schema/契约问题（任务包 G 项）

- `eng/contracts/schemas/normalize_frame_science.schema.json`：control 增加 `leaf_order`；
  `support_area_sr`/`covariance_rank`/`leaf_order` 在 unavailable 时可空
  （`NORMALIZE_DATA §7「未知数用 null」`）；`precision.status=unavailable ⇒ controls maxItems 0`
  （此前实现会把逐点 unavailable 的控制写进 controls，违反 schema）。
- `eng/contracts/data/examples/normalize_frame_science.available.example.json`：同步 `leaf_order`。
- `eng/contracts/schemas/phase_config_normalize.schema.json`：`photometry.frame_overrides[]`
  正式声明（含 input/input_light/frame_key、filter、qe_name、master_*、gain_e_per_adu）。
- 单元检验 `eng/tests/unit/sparse_reference_gls_test.cpp` 按新 API（显式
  `has_target_leaf`、`leaf_n_full/covered`）更新，**判据与 oracle 数值未动**。

## 3. 独立证据矩阵（本轮实际执行）

全部命令与原始输出：`实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log`。

| 证据 | 程序/命令 | 结果 |
|---|---|---|
| GLS 解析真值（F01 逐字反例） | `code/verify_round1.cpp` | `V_F` 与 `1/(PᵀV⁻¹P)` rel 0.0 |
| 独立真值 A：源空间闭式 | 同上（与生产不同代码路径） | 4 组输入全部 rel ≤ 2e-15 |
| 独立真值 B：显式 nl 空间 `C=R diag(v) Rᵀ` 重算 h/C | 同上 | h、C 逐项 rel ≤ 1e-12；C 对称、对角 > 0 |
| 背景 known/constant_fit/plane_fit | 同上 | 可辨识口径全部匹配；m<ncol ⇒ 具名 unavailable 且不回填 V_F |
| 秩亏 | 同上 | `covariance_rank=1`（不虚报）、通量在保留子空间仍可估计、零模板 ⇒ unavailable |
| Monte Carlo 通量散度 | 同上，n=20000 | `sd=20.053691` vs `sqrt(V_F)=20.081935`，ratio 0.9986（<2%） |
| 标度不变性 | 同上 | SNR 不变；重复缩放会被判出 |
| 既有单元检验 | `eng/tests/unit/sparse_reference_gls_test.cpp` | 24 项 ALL PASS |
| sidecar 单元检验 | `eng/tests/unit/frame_science_sidecar_test.cpp` | 24 项 ALL PASS |
| schema 符合性 | `eng/tests/conformance/normalize_frame_science_schema_test.py` | PASS 18 / FAIL 0 |
| Linux Release 构建 | `cmake --build build/linux-sci-p5 -j 8` | exit 0 |

## 4. 真实生产链与实测产物

真实运行（M42 T2/M4 单帧，4096×4096，原始 FITS 头**无** WCS）：

```bash
./build/linux-sci-p5/acsd normalize -y --json run/norm-round1-r1.json   # exit=0
```

- 产物根：`run/norm-round1-r1/out/M42_M4_T2_flying_dutchman-20251212_033644-600S-H-alpha/`
- `frame_science.json`（155 KB，`acsd.normalize.frame-science/v1`）：
  - 64/64 控制点 `availability=available`；`precision.status.accuracy=approximate`，
    `omitted_terms=[master_uncertainty]`（母版方差未传播 ⇒ 如实缺项，不伪造完整）。
  - 控制点 `p0_r0`：`leaf_ipix=359682634784`、`leaf_order=18`、
    `reference_flux=1000`（`explicit_linear_flux`）、`flux_variance=49.96236257513848`、
    `reference_snr=141.4746136629736`、`support_area_sr=1.216050653379998e-08`、
    `covariance_rank=626`、`condition_number=2.299438223991435e9`、
    `background_nuisance=constant_fit`。
  - `surface_transfer`：`surface_variance=2.499597910833713e+23` `adu^2/sr^2`、
    `transfer_factor=5.00296179363929e21` `sr^-2`、`covariance_condition=reference_source`。
  - 帧摘要 `area_weighted_median_log_snr = 153.29579909162405`，`n_valid=64`，
    `supported_area_sr=7.772057070639066e-07`。
  - **与修复前 `SNR=19.9989` 不同 ⇒ 新数值只与本次 SHA 绑定，不沿用旧值。**
- 256 个局部模型文件（64 控制点 × psf/covariance/drizzle_operator/support），
  `representation`：192 个 `acsd.normalize.local-model/v1`、64 个
  `acsd.normalize.local-factor/v1`（该 64 个控制点 nl=798 > 落盘预算 256，
  显式 nl×nl 矩阵改为**版本化精确因子** `v_j` + 同控制点 drizzle_operator CSR，
  `C=R diag(v) Rᵀ` 可精确重算；见 §5 UNRESOLVED-1）。
- `drizzle_operator` 模型含 `source_n_leaf_sr`（每叶完整 N_l）与 `normalization=drop_area`，
  使 R 的归一根可被独立复核。
- 无 reference 配置的默认路径（`snr.reference` 缺席）保持 `unavailable` + 具名 reason，
  signal 产品不受影响（沿既有 `p1_final.json` 证据）。

### 4.1 八节点的 typed 端口现状（不含糊）

| 节点 | 本轮的 typed 端口真实性 |
|---|---|
| calibration / cosmetic | 由既有 `p1_op_*` 描述符经 output_dir 文件传递；**未**迁到 Runtime typed payload（见 §5） |
| WCS/IPV | 同上 |
| guided source/PSF | 同上；`frame_psf_sigma_px` 从 `p1_sources.json` 取 Moffat4 σ（不把检测 Gaussian FWHM 当 Moffat 参数） |
| photometry | **本轮新增逐 occurrence 响应绑定**（`p1_resolve_frame_binding` → `freq.filter_name/qe_name` → `p1_phot.json#response_*`），仍是文件端口 |
| noise | `ConditionalNoiseModel` 平面模型经 `p1_snr.json` + variance 面；本轮的 GLS/秩判定在 noise 库 |
| Drizzle | **本轮改为从生产几何路径取完整叶 N**；`sparse_reference`/`frame_reference` 由 Drizzle 调 noise 库完成（无循环依赖） |
| writer | 唯一最终发布者：staging → AIO → 原子发布 + hash 复核（既有实现，本轮 reader 补齐后闭环） |

## 5. UNRESOLVED（需前台裁决，附条款与源码）

**UNRESOLVED-1（§7.1 显式矩阵 vs 落盘预算）**
- 条款：`NORMALIZE_DATA §7.1` covariance 行要求「`C_ref` 矩阵」；detail §7.4 要求
  「C 为 nl×nl row-major」。
- 事实：生产窗 `nl≈800`（真实读数 `leaf_ipix` 长度 798）、`m≈645`；若按条款写显式
  nl×nl，64 控制点 × 798² × 8 B ≈ 326 MB（JSON 约 1.5 GB）。
- 代码：`sparse_reference.cpp` `lw.covariance_explicit = nl <= cfg.covariance_matrix_budget`
  （默认 256）；超出时写 `acsd.normalize.local-factor/v1`（`v_j` + CSR）并在
  `representation_note` 写明理由。这是**精确**表示（可重算 C），但不是"显式矩阵"。
- 影响：reader 侧 `representation` 已如实登记，不冒充；Phase2 需按两种 representation 取值。
- 需要前台：要么接受版本化因子表示作为 §7.1 的合法 carrier，要么给定 nl 上限/分块策略。

**UNRESOLVED-2（观测域规模 vs O(m³) 开销）**
- 条款：NOISE_SNR §11.2「局部模型要包含所选观测叶的全部随机贡献」。
- 事实：为满足该条，观测域取"归属叶的全部贡献像元" ⇒ 生产实测
  `overlaps=207957`、`targets=51002`、`m` max 645（sum 40110）、`nl` max 802、
  `nnz` max 3292、64 控制点。生产 R **秩亏**（`covariance_rank=626 < m=633/645`），
  因此 Cholesky 快路径不成立（H 半正定），必须走保留子空间特征分解。
- 实测运行时（Release, `build/linux-sci-p5`，单帧 4096×4096, nside=262144）：
  - `run/norm-round1-r1`（已完整结束, exit=0）：`drizzle_run=34.7s hips_write=8.9s
    total=43.6s`，精度段约 20 min。
  - `run/norm-round1-r4`（Cholesky 快路径构建后的确认重跑；报告撰写时仍在精度段，
    已 27 min）：`drizzle_run=48.2s hips_write=12.9s total=61.2s`。
    快路径因 R 秩亏未被采用，与诊断一致；该运行是"确认"而非新证据。
  归因：循环 Jacobi 在 m≈645 上单控制点约 19 s（O(sweeps·m³)）。
- 代码：`sparse_reference.cpp` 已同时实现 Cholesky 快路径（H ≻0 时 O(m³/3)）与
  保留子空间特征路径（秩亏时唯一正确路径）。本轮尝试以 Householder+QL 替换 Jacobi
  以求 O(m³) 总量，独立验证未通过即撤回，**未**以未验证数值例程进入生产。
- 需要前台：是否接受「显式观测域上限 + 记录 truncation」作为默认（会把
  `accuracy` 从 `conditional_complete` 降为 `approximate`），或授权实现并验收
  生产级对称特征求解器。

**UNRESOLVED-3（PSF 的空间积分响应）**
- 条款：PSF.md §4「在每个探测器像元面积内求积……不以像元中心函数值代替欠采样像元积分；
  参考位置的子像元相位必须进入求积」；detail §3 `flux_estimator=analytic_total|
  aperture_with_correction|diagnostic`。
- 事实：`normalize_precision.cpp::moffat4_profile` 仍是**像元中心函数值**归一化；
  `LocalModelPayload`/sidecar 无 `flux_estimator` 字段，`dynamic_psf.h` 的
  `DPSFFitResult` 也无截断/降级标记（PSF.md §7.4 已指出这两处）。
- 本轮处理：P **不再随窗口重归一**（ΣP = 包络份额，`enclosed profile sum` 已落盘），
  f_out 闭式份额记入 support note；但像元积分与子像元相位**未实现**。
- 需要前台：`EffectivePsfModel` 的 `flux_estimator` 取值与 sidecar 字段增补。

## 6. 三类事项（严格分开，不混称）

### 6.1 真实输入受限（设计已实现，但缺真实输入）

1. **混合设备/滤镜的真实 normalize 运行未执行**：F07 的 `frame_overrides` 解析与逐帧
   `freq.filter_name/qe_name` 替换已接通并有配置 schema，但 testdata 中没有
   "同块多滤镜 + 多设备 + 各自响应曲线" 的输入集合；需前台提供或指定可运行配方。
   （`testdata/M42_T2T3_mosaic_Flying_dutchman` 是同一 T2 设备。）
2. **α≠1 的真实测光标度帧未运行**：本轮 α≠1 的正确性由解析真值 + 独立数值真值 +
   判别性反例证明；真实 `k_photo` 施加帧（`frame_applied=1`）未见于此轮输入
   （RUN.md 记录的该帧 `photometry` 通道缺席 ⇒ `native_calibrated`）。
3. **母版不确定度传播未闭合**（`CALIBRATION.md §5.7` / `NORMALIZE_PIPELINE §4.6`）：
   真实产物 `omitted_terms=[master_uncertainty]`、`accuracy=approximate`，
   与正本口径一致；未伪造"完整绝对 σ_F"。

### 6.2 环境未验证

1. **Windows 构建/ABI/关键回归未执行**（本机无 Windows/MSVC）。
2. **`ACSD_DEV_MODE` OFF/ON 四模式对照未在本轮执行**（此前 `RUN.md` 有 1496 tiles
   ON/OFF 记录，但那不是本轮新数学的对照）。
3. **HST 结构前向合成未执行**（大文件不上传 Git，本轮未排入）。

### 6.3 代码未完成（不冒充已完成）

1. **F08 Runtime typed 迁移未做**：`RunContext`/`BlockFrame` 仍只存数值字节/artifact
   descriptor；八节点的跨节点取数仍经 `output_dir` 下的 `p1_*.json`；`p1_session.cpp`
   仍是兼容 facade。
2. **`sed_response` 参考定义未接线**：`complete_spherical_reference` 对
   `kSedResponse` 显式返回 unavailable + 具名 reason（不伪造绝对零点）；photometry 的
   响应积分公开 API 未被 consumed。
3. **`explicit_linear_flux` 与 `sed_response` 的 F_ref 生产链**：本轮只走
   `explicit_linear_flux`（配置 flux），`sed_response` 的 `F_native(u)=a_k m_k(u) Q_k,ref`
   未实现。
4. **空间 α（`pixel_scale` 向量）无生产填充方**：结构与数学已支持（含 a_ref 配对），
   `PrecisionFrameRequest.pixel_scale` 目前由调用方留空 ⇒ 走常量 `plane_scale` 分支。
5. **PSF 空间模型（三角重心凸组合 + 空间留出量化）未实现**（见 UNRESOLVED-3）。
6. **F02 的"生产 signal 逐项等于 R 列"端到端对拍未做**：本轮用同一几何路径导出的
   CSR + 独立解析/数值真值验证 V_F/h/C；"扰动单个源像元 ⇒ 生产 signal 变化 = R 列"
   的实测扰动实验未执行。
7. **帧摘要的跨控制点 leaf 去重未实现**：patch 之间可能共享叶 ⇒ `supported_area_sr`
   是各控制点 `Σ min(D_l,Ω_leaf)` 之和，不保证全局不重叠（detail §7.4 要求按 leaf 归属
   唯一 patch 去重）。当前真实读数 `supported_area_sr=7.77e-07` 未做该校正。
8. **`quantization.s12` 与 registry parity 失败未排查**（任务包 G 项点名的两项）。

## 7. 本轮未触碰的范围（事实）

- Mosaic 默认权重、背景拟合、Export 算法：**未修改**（`lib/algorithms/coverage/`、
  `lib/algorithms/integration/phase2_integrate/`、`lib/algorithms/fits_output/`、
  `lib/phase3_session/` 无改动）。
- 未上传原始 FITS、母版、HST 大文件；只提交脚本、输入索引、读数与事实报告。
- 未创建 PR、未改 main、未 force-push。

## 8. 输入与构建哈希

见 `实验/normalize-precision-p5/evidence.json`（既有，脚本从磁盘实算）与本报告 §4
的运行路径。本轮新增运行：`run/norm-round1-r1.json` / `run/norm-round1-r4.json`。
