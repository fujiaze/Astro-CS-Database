# B2｜Normalize 科学链：独立源码—数学审查与反例

> 审核对象：`rebuild/acsd-v2@fa4649d57`（2026-11-10）。审查者：`normalize-auditor`，未参与该链设计与实现。
> 只读审查；本文件是唯一写入。B1 的共享数值修复（`docs/audit/b1-numerics-remediation.md`）不作为上游证书继承——本文对每个用到的原语在其实际调用点重新判断。
> 一句话结论：**Normalize 的坐标/PSF 像元积分/α 应用顺序基本正确，但“已认证通量区间”漏掉列尺度因子（可构造合法输入使真值落在区间外）、`measure` 用 WCS 位置冻结模板、PRF/样本响应根本没有进入积分、photometry 丢弃测量协方差并给未知 XP 协方差填单位阵、apply_scale 不传播 θ 随机影响；X01 的 10.3% 是“按测量峰值归一”的比值统计量，lead 的 σ 用错了对象，但其“无系统偏差”的结论方向成立（正确归一化 z=−1.77，48 项最大 |z|=1.77）。**

## 0. 审查方法与实跑范围

- 先读五份 foundation 中的 `OBSERVATION_COORDINATES.md`、`GAIA_RESPONSE.md` 与 Normalize 链 10 篇 process 合同（calibrate/regroup/wcs/catalogue.access/astrometry/psf/measure/response.integrate/photometry.fit/photometry.apply），自行重推式 (1)(2)(3)(5)(6)、`d=HF+Zβ+ε`、`q=∫fRλ/(hc)dλ`、`α=exp(−hᵀθ)`。
- 再逐文件读 `src/observation`、`src/coordinates`、`src/catalogue/sky_geometry.cpp`、`src/sources`、`src/response`、`src/photometry`、`src/workflows/normalize.cpp` 的实际 C++ 路径（不读 `src/areas` 之外的 geometry/drizzle 细节）。
- **本次实际执行**（只读、不改仓库）：
  1. 复算 X01 七个源的联合通量+背景 OLS 设计（Python/numpy，不调用生产 helper），逐帧复现 `measure` 的输出（差 ≤1.5%），并用 trace 中的实际位置与 PSF 参数做对照。
  2. 对 A/B 组各 4 帧做自由位置+自由 σ 的联合拟合，得到 B σ=1.33948±0.00022、A σ=1.35489±0.00012。
  3. 用 trace 位置、真值 σ 与 Poisson+read 方差重新算 `(DᵀWD)⁻¹`，并算“按测量峰值归一”的比值统计量的协方差（这是 lead 脚本没有算的量）。
  4. 读 `validation/x01/observed/{compare.json,e2e_summary.json,flux_uncertainty.json}` 与 `truth/x01_truth.json`。
- **未实跑**：生产二进制的重建与三命令复跑（依赖 lead）；盲解求解器内部逐行；catalogue 索引/KD 树内部；geometry/drizzle/export。

## 1. 逐 operation 的真实入口、数据边、单位与裁决

| operation | 真实入口（file:line） | 数据边 / 单位 | 裁决 |
|---|---|---|---|
| `observation.calibrate` | `calibration_kernel.cpp:115 calibrate_sample`、`:184 assemble_calibrated_native`；signal 编排 `calibration_signal.cpp:198 calibrate_block` | Raw ADU + Calibration → x=(d−b−r(dk−b))/f，r=t_exp/t_dark；x 单位 = `unit_signal(detector_identity)`（linear_power 1）；噪声根 shot/read/母版 | **需局部修复**：暗母版 Jacobian 缺失；Poisson 顶点用实现值（approximate）；x 的区间只含表达式舍入，不含外部已知误差 |
| `observation.regroup` | `regroup.cpp`（`regroup_observation`）；`regroup_process.cpp` typed adapter | G、M/b，Gx、GCGᵀ；physical_sum/mean 与 linear_view 分叉 | **有条件可保留**：结构正确，但洞/缺孔 footprint 只部分落实；`linear_view` 未做 signed 区间反例 |
| `coordinates.wcs` | `wcs.cpp:482 make_wcs`、`:810 forward`、`:832 inverse`、`:982 derivative` | PixelCenter0/Edge0 共享连续轴，u=p+1−CRPIX；rad；g=abs(det C)abs(det J_F)/(1+qᵀq)^{3/2} | **可保留（局部修复）**：变换顺序/逆/面积正确；但 `inverse` 的 root_box 证书用点残差+硬编码 1e-15；容差 1e-11 未走 NumericBudget |
| `coordinates.tan_sip` | `tan_sip.cpp:138 tangent_offset`、`:147 direction_from_tangent`、`:158 solid_angle_density`、`:281 jacobian` | 与式(3)(6)逐项一致 | **可保留** |
| `coordinates.footprint_provider` | `footprint_provider.cpp:19 TanSipFootprintProvider::forward_interval` | 只复用 tan_sip，无第二套 TAN | **可保留** |
| `catalogue.access` | `sky_geometry.cpp:82 propagate_star`；`provider.cpp`；`catalogue_access_process.cpp` | 2016 历元 → 观测历元；pmra_cosdec 已含 cosδ；RV/视差空间运动 | **有条件可保留**：传播公式正确；`lookup` 历元语义须消费方遵守；XP 协方差缺失时下游填单位阵（见 P1-2） |
| `sources.astrometry` | `astrometry.cpp`/`astrometry_fit.cpp`/`astrometry_process.cpp` | 检测→quad→匹配→TAN/SIP；输出 WcsModel | **本次未逐行复核**：仅审查数据边、注册与 X01 实测残差（B 组 0.162–0.257 px）。列为未证明条件 U3 |
| `sources.psf` | `psf_fit.cpp:71 fit_psf`、`psf.cpp:125 evaluate_psf`、`psf_model.cpp:519 build_templates` | 模板=像素积分；整平面归一；尾=1−Σ；参数盒 | **需局部修复**：像元积分正确；PRF/`pixel_response`/`sample_response_gain` 未进入积分；拟合目标无权重；Jacobian 用差分；参数盒=最后一步步长 |
| `sources.measure` | `measure.cpp:97 measure_science_sources` | d=HF+Zβ+ε，Z=1；position 来自 WCS；单位 ADU；joint_noise=I C Iᵀ | **需局部修复（含 P0）**：OLS 而非 GLS（仅效率损失）；通量区间漏列尺度；位置冻结 WCS；Z=1 只在均匀 PRF/恒等 transfer 下成立 |
| `response.integrate` | `api.cpp:372 integrate_response` | v_kl=∫b_l R λ/(hc)dλ；分段 GK；support/truncated | **规格缺口**：合同要求的“多项式→八阶 Taylor→纯包含”决策程序未实现，只有名义 GK，且诚实地标 approximate |
| `photometry.fit` | `api.cpp:433 run_joint_fit` 调用；`joint_fit.cpp` 求解 | 联合 counts+Gaia latent；θ=[log a, 空间项] | **需局部修复**：counts 协方差恒为对角（联合路径结构性死代码）；Gaia 协方差未知→单位阵；参数盒只证数值不含统计 |
| `photometry.apply` | `apply_scale.cpp:164 apply_scale` | α=exp(−h(u)ᵀθ) 在 root 原生样本一次；G Dα 链；source 端 a 只进模板 | **需局部修复**：α 一次正确；单位不更新；θ 随机影响未进图像噪声 |
| `workflows/normalize.cpp` | `normalize.cpp:324 normalize` | calibrate→regroup?→solve/supplied→bind_wcs→fit_psf→measure→bind_model→integrate_response→fit_photometry→apply_scale→geometry→publish | **需局部修复**：`result.published.clear()`；photometry 帧序被工作流覆盖；catalogue provider 调用在 Host/plan 之外（设计允许，但无资源账本） |

## 2. 数学与实现发现（逐条 file:line 与证据）

### 2.1 calibrate

- 公式 `x=(d−b−r(dk−b))/f`（`calibration_kernel.cpp:142-162`）与合同一致；区间 `sample.lower/upper` 是同一表达式的向外包络（`:176-180`）。共享母版按 shared_id 合并（`:405-449`）。
- **P1-1 暗母版 Jacobian 缺失**：`assemble_calibrated_native` 只构造 `flat_jacobian = −x/f`（`:409-415`）并与 `master_noise` 的共享因子复合；`∂x/∂dk = −r/f` 的根从未并入。若用户提供带共享读噪的 dark 母版，标定后方差少一项 `(r/f)²σ_dk²`，且没有 unknown term。合同要求“母版的噪声根按 shared_id 并入 Jacobian”（`observation.calibrate.md:44`），是复数母版。
- **P1-2 Poisson plug-in 是“实现值”**：`shot = sqrt(max(d−b,0)/gain)`（`:311-324`），方差随数据实现抖动；合同允许但必须标 approximate（已标，`:370-377`）。对 X01 的影响：`measure` 的权重（若将来启用）会随噪声实现偏移，但当前 `measure` 根本不用权重（见 2.4）。
- **X01 案本身的物理不一致（不是实现缺陷，但影响“已验证”声明）**：`validation/x01/input/cli/calibration.json` 的 dark 主版是 `12/2.5=4.8 ADU`；而 `truth_x01.py:102-107` 把 `dark_level=12 e⁻/s` 乘 100 s 再除 gain，注入 480 ADU。x 中被多减/少减的 475.2 ADU 被 β 完全吸收，所以通量不受影响；但该案**没有真正检验 dark 单位/曝光比路径**，不能据此宣称 N0 的 dark 转换已认证。

### 2.2 regroup

- G 作用于同一原生随机量：值 `Gx`（`regroup.cpp:276`）、噪声按同一 G/影响复合（`:301-360`）、mean 要求成员数一致（`:241-245`）。这条与合同一致。
- **未证明**：有洞/缺孔 footprint 与 `linear_view` signed 行的实际构造未逐行核对；`mean→sum` 的恢复乘成员数是常数缩放而非逐成员（`:261-274`），对“每个 bin 成员数不同”的输入只能靠 `mean_member_count` 拒绝，不能表达混合成员。

### 2.3 coordinates.wcs / tan_sip

- 顺序正确：SIP 先、CD 后（`tan_sip.cpp:138-145`）；方向 `n=(c+qx e+qy north)/sqrt(1+qᵀq)`（`:147-156`）；雅可比 `g=|detC||detJ_F|/(1+qᵀq)^{3/2}`（`:158-174`）与式(6)一致；`J_W=(I−nnᵀ)ECJ_F/s`（`:281-314`）。
- LONPOLE 规范化 `C_canonical=R(ρ)C`，`R=[[cosρ,sinρ],[−sinρ,cosρ]]`（`wcs.cpp:672-680`）与式(5)一致；极点缺省规则 `wcs.cpp:670`。
- `make_wcs` 的准入：60° 域用区间 qq（`:709-715`）、单射证书 `H=J_F(u_c)⁻¹, η=sup‖I−HJ_F‖∞<1`，失败回退 `L=sup‖∇D‖∞<1`，都走区间导数（`:417-446`）。这与 foundation §3.1 的充分判据一致；未认证则 `injectivity_unresolved`。
- **P1-3 逆 root_box 不是区间包围**：`wcs.cpp:939` `eps = residual*(1+1e-12)+1e-15` 是点残差加硬编码绝对量；合同 §3.3 要求 `ε=‖F(û)−z‖∞` 的**包围**。SIP 次数到 9、|u|~60 时，多项式中间项可远大于 1，点求值的舍入误差可超过 1e-15，此时 `radius` 偏小。X01 的 B 组 SIP 次数 3、系数小，实际安全；但“certified”在一般准入类上不是证明。另：Newton 容差硬编码 `1e-11`（`:874`），合同要求来自 NumericBudget（`coordinates.wcs.md:102`）。
- `assessment.validity=certified` 依赖 `interval_primitive_status()==directed_rounding`（`wcs.cpp:757`）——B1 修复后该条件在 x86-64 成立，但上述 root_box 的 ε 不来自区间运算，所以 validity 的“certified”仍应视为条件于 `eps` 的经验界。

### 2.4 sources.psf / sources.measure

像元积分本身正确：
- 可分离 Gaussian 的 `0.25(bx−ax)(by−ay)`（`psf_model.cpp:359-376`）等于两个一维 erf 积分之积；整平面归一（`density` 是归一密度，`psf_model.cpp:300-303`）；有限窗**不**重新归一，尾=`1−Σ`（`:576-579`）。
- `evaluate_psf` 是唯一积分实现，fit/measure 都调用它（`psf.cpp:281-282`；`measure.cpp:326`）。

但有四个实质问题：

1. **P0-1 通量区间漏掉列尺度（可构造反例 C2）**：`measure.cpp:436` 取 `inverse_norm=1/σ_min(D)`，`:439` 用 `flux_radius=inverse_norm*(data_norm+design_norm*solution_norm)`，再对每个分量给同一半径。但解是 `θ=S⁻¹D⁻¹d`（`solve_scaled` 返回 `solution[c]/column_scale[c]`，`psf_model.cpp:616-618`），扰动界应是 `‖S⁻¹‖₂·‖D⁻¹‖₂(‖δd‖+‖δA‖‖θ‖)`。`column_scale[c]=‖A_c‖₂`：σ=1.35 的高斯列约 0.295，背景列 `sqrt(m)`≈34，因此 `‖S⁻¹‖≈3.4`。代码的“certified”区间比严格界窄约 3.4×，**不是包含区间**。
2. **P1-4 PRF/样本响应未接线**：`FootprintSet.pixel_response` 与 `sample_response_gain` 被 CLI ingest（`normalize_runtime.hpp:121-138`）与 footprint 复制（`normalize.cpp:137,174`），但 `psf_model.cpp:540-575` 的积分只用解析密度×矩形/环，**从未乘 PRF 或样本增益**。合同 `sources.psf.md:68` 要求“PRF 积分乘此系数”，`OBSERVATION_COORDINATES` 式(2) 的 ρ_i 缺失。X01 为 uniform（null），所以 10% 与此无关；但这是真实仪器（flat 残差、Bayer/PRF）上的通量系统偏差来源，且没有任何 unknown/omitted 标注。
3. **P1-5 拟合目标无权重**：`psf_fit.cpp:357-358` 目标 `Σ0.5r²`，而噪声随像素从源心到窗口边缘变化数十倍。点估计在模型正确时无偏，但协方差/参数盒不是有效方差的；合同要求固定已声明 C。位置/形状 Jacobian 用中心差分（`psf_fit.cpp:384, 422`），不是解析/区间 AD。
4. **P1-6 参数盒=最后一步步长**：`psf_fit.cpp:794-800` `parameter_lower/upper = θ ∓ (|step|+1e-12 max(1,|θ|))`，并自标 `omitted: the_parameter_box_is_conditional_on_the_gauss_newton_hessian`（`:858`）。X01 stderr 的最后步 ~1e-9，于是“PSF 形状区间”是 ±1e-9，而真实形状统计误差由 7 星/4 帧决定（本文自由拟合给出 σ 的 1σ 级约 2e-4，系统层面 1e-2）。这不是 score 根盒，合同 `sources.psf.md:112` 要求的“真实 score/Hessian 与区间 AD”未实现。
5. **P1-7 `measure` 位置冻结在 WCS**：`measure.cpp:162-163` 用 `detector_position(wcs, star)`，而 `fit_psf` 是**自由拟合位置**的（`psf_fit.cpp:217-236, 326-330`）。两者不一致：PSF 形状在“自由位置”下拟合，通量在“WCS 位置”下测量。对称 PSF 的位置误差只给二阶通量偏差 `exp(−δ²/(4σ²))`（δ=0.25 px、σ=1.35 → −0.85%），X01 不足以解释 10%，但这是无标注的模型不一致，且在盲解误差大时不可忽略。
6. **P1-8 背景列 Z=1 无适用条件检查**：`measure.cpp:363` 硬写 1.0。合同允许 Z0=∫P_j r_j=1 只在“单位面积、均匀响应、未变换样本”（`sources.measure.md:66`）；一旦 `sample_response_gain` 非平凡或有 regroup/transfer，Z 必须由同一 PRF 链积分。代码既不检查也不报告条件违约。

### 2.5 response.integrate

- 分段点取 basis knots + response 波长 + 额外断点（`api.cpp:340-357`），单位 `photon_rate_m2:response/<hash>`、`linear_power=1`（`:512-524`），光子权重 `photon_weight(λ)=λ·1e-9/(hc)`（`:462`）。这些与 `GAIA_RESPONSE` 式(1)(3)一致。
- **P1-9 认证路径未实现（规格缺口）**：合同 `response.integrate.md:72-87` 规定“有限次多项式→八向卷积解析 / 八阶 Taylor 余项 / 纯包含”的能力顺序，并明确“不得用 GK 差值冒充证书”。实现只调用 `adaptive_integrate_gauss`（`:466-470`），误差 = 面板差 + 浮点求和界（`:485-495`），并诚实标 `Validity::approximate`（`:565-567`）。因此 `response.integrate` 在当前实现下**永远不能**产出 certified 的 v；`photometry.fit` 的“bounded”只可能来自参数盒，不来自 v。

### 2.6 photometry.fit / apply

- **P1-10 counts 协方差恒为对角（联合路径是死代码）**：`joint_fit.cpp:758-763` 只有当 `measurements->joint_noise->input_output.domain->size == n_meas` 时才用联合协方差；`measure` 的 `joint_noise` 的 domain 是**原生像素索引**（X01 为 16384），codomain 是 flux 索引（每帧 8 = 7 源 + 背景），而 `n_meas` 是**全部帧的测量总数**（X01 为 7×8=56），条件永不成立，于是 `:778` 用 `meas_variance` 填对角。（即使把字段换成 codomain，8 也≠56，且还需把 8×8 中的背景分量边缘化掉。）即便把检查改成 codomain，也需要从 8×8（含背景）边缘化出 7×7 通量块——代码没有这一步。后果：`M-MEASURE` 的 `Cov(F1,F2)=4` 被当作 0，比值方差被高估 1.79×（反例 C4）。
- **P1-11 未知 XP 协方差填单位阵（任意先验）**：`joint_fit.cpp:806-819`，`covariance=I` 并记 unknown term。X01 的 catalogue 给出**精确** `xp_coefficients=[1,0,0]`，正确模型是零方差；实现给出方差 1。由于模型 `C_ks=a_k exp(bᵀθ)vᵀc_s` 的 gauge 为 `c_s→c_s/λ, a_k→a_kλ`，counts 行对 λ 完全无信息，唯一信息来自 Gaia 行：在 `Φ=½rᵀWr` 约定下 `Var(log λ)=1/Σ_s|c_s|²=1/7`，即 **7 颗星时标度相对误差约 38%**，而真实误差应≈0（反例 C3）。点估计仍正确（gauge 被钉在 c=1），所以只查数值的测试会通过。结果 `assessment.science` 因 unknown_terms 降为 conditional（`api.cpp:717-721`），这点是诚实的；但把“未知”具体化为单位阵不是合同里的模型，且给出了一个可被误读的 38% 数值。
- **P1-12 参数盒只证数值不含统计**：`joint_fit.cpp:1290-1319` 的 `parameter_lower/upper` 由通量数值区间 + 秩证书搜索得到；统计协方差另存（`api.cpp:612-665`）。`apply_scale` 只用前者构造 α 区间（`apply_scale.cpp:239-259`），后者没有进入图像。
- **P1-13 apply_scale 不传播 θ 随机影响**：`apply_scale.cpp:279-290` 只做 `exp(−θ·basis)` 对角因子，`scale_noise`（`:138-160`）只把该因子乘进既有噪声。合同 `photometry.apply.md:72` 要求随机影响 `Dα L_x − diag(αx)H L_θ` 再乘 G，且 x/θ 同根先加影响再作协方差。实现缺 `diag(αx)H L_θ` 项 → 每帧噪声漏掉标度不确定度（含 P1-11 的 38% gauge 与空间倾斜）。
- **P1-14 单位不更新**：`apply_scale.cpp:407` 保留 `observation->unit`。而 `unit_adu` 与 `unit_signal` 完全相同（`units.cpp:26-31`，都是 `make_unit(identity,0,0,0,1,1)`），所以“校准 ADU”与“已除 a 的模型标度”在 Unit 上不可区分；只有 `scale_applications`/`response` 元数据暗示差异。下游任何仅凭 Unit 判断尺度/做单位换算的消费者都会错（反例 C6）。
- **P1-15 工作流覆盖用户帧序**：`normalize.cpp:802-804` 把 `fit_request.frames` 改成“产生了测量的帧序”，覆盖 `PhotometryRequest.frames` 的显式唯一顺序（`photometry.fit.md:53`）。X01 用户给空帧序，所以能跑，但合同要求的“显式顺序 K”被工作流自创。

### 2.7 工作流接线与输出

- **P1-16 已发布位置被清空**：`normalize.cpp:1122` 置 `outcome.published=true`、`:1125` push `result.published`，但 `:1129 result.published.clear()`。CLI 在 `apps/cli/commands.cpp:1206-1208` 用该列表发 `product_published` 事件，因此**产品确实写盘、帧状态显示 published，但命令输出永远没有已发布目录**。e2e 的 `normalize` 阶段只显示 `published: 8`（来自 frames 计数），与此一致。
- `normalize.cpp:431-433` 是空块（注释 + 空 if），属占位残渣。
- 资源：`pool.cpu_threads=2`、`host_bytes=1GiB`、`work=4e9` 全部硬编码（`:348-354`），不读配置；X01 的 CLI resources（threads=1, work=1e7）没有传到工作流池。`catalogue.query/lookup` 直接在工作流 `Context` 上调用（`:593, 724`），不进 Host/plan，设计文档明确允许 provider 方法（`WORKFLOWS`），但意味着 catalogue 分页/XP 读取的耗时与内存不计入 `plan` 的资源账本。
- 无重复 TAN/PSF 实现；但 `healpix.cpp:26 face_point` 与 `chart.cpp:59 chart_from_face` 都是 face→sky，疑似重复实现（P2，本文未做差分验证）。

## 3. X01 B 组 ~10% 相对误差：独立判断

### 3.1 先确认生产估计量就是被复现的那个

用 trace 中 `measured_stars` 的实际 WCS 位置、真值 σ=1.35、窗口 ±6 与单背景列，独立 OLS 复现 `measure` 的通量：

| frame 06 / src | 生产 | 本文独立 OLS | 真值 |
|---|---|---|---|
| 1000 | 6282.81 | 6218.81 | 6119.34 |
| 1003 | 3374.88 | 3341.53 | 3665.87 |

逐源差 ≤1.5%，来自 PSF 参数/窗口细节；估计量与数据都被复现。**因此 10.3% 不是读错 trace 或单位错。**

### 3.2 逐项排查系统偏差（候选全部否定）

1. **冻结 WCS 位置**：B 组实测残差 0.162–0.257 px；x01-06 的 1003 偏移仅 dx=−0.018、dy=−0.012 px。对称 PSF 的位置误差只给 `exp(−δ²/(4σ²))`；δ=0.25 px、σ=1.35 → −0.85%。**不是 10% 的来源。**
2. **PSF 宽度**：A/B 各 4 帧自由位置+自由 σ 联合拟合：B=1.33948±0.00022、A=1.35489±0.00012。用拟合 σ 代替真值造成的统一通量偏差为 `2/(1+(σ_t/σ_m)²)`：B −0.78%、A +0.36%，且对**所有源同号**；观测到的是逐源正负混布。**不是。**
3. **背景列**：拟合 β 与 `(sky+dark)·t/gain` 在 0.05% 内（frame 06：3279.6 vs 3280；frame 04：2881.4 vs 2880）。**不是。**
4. **响应身份/单位**：亮源 B/A 通量比 0.596 vs 真值 0.600（−0.7%）；通量以 ADU 计与真值 ADU 一致（无 2.5× gain 错）。**不是。**
5. **残差结构**：四帧联合（B）逐源相对偏差 −0.5/+2.0/+2.0/−4.4/−2.4/+2.5/+1.0%，与每源 σ≈2.5% 的 χ² 一致（7 dof，χ²≈6.4）。**无系统性。**

### 3.3 lead 统计结论的漏洞（必须写清）

lead 的 `uncertainty_x01.py` 算的是**绝对通量**的 GLS 协方差，并与 10.33% 直接比。但 X01 的验收量是 `compare_x01.py:84-88` 的

```
M_i = | (m_i/m_peak) / (t_i/t_peak) − 1 |
```

即**按测量峰值归一**的比值。一阶展开 `m_i=F_i(1+ε_i)`、`m_0=F_0(1+ε_0)` 得 `M_i≈|ε_i−ε_0|`，其方差是

`Var(ε_i−ε_0)=Var(F_i)/F_i²+Var(F_0)/F_0²−2Cov(F_i,F_0)/(F_iF_0)`，

不是 lead 用的 `Var(F_i)/F_i²`。用 `(DᵀWD)⁻¹` 独立算（trace 位置、Poisson+read 期望方差）：

| x01-06 / 1003 | σ_rel(绝对) | σ(M_i) | 观测 M_i | z |
|---|---|---|---|---|
| lead 的口径 | 5.03% | — | −10.333% | −1.58（lead 的算法） |
| 正确口径 | 5.03% | **5.849%** | −10.333% | **−1.766** |

全部 48 个（7 源 × 8 帧，除峰值源）M_i 的 max|z|=1.77，rms≈0.8，与 N(0,1) 完全相容。

**所以 lead 的结论（无系统偏差、5% 阈值不可达）成立，但他的量错了三处**：
(a) 用绝对通量 σ 代替比值统计量 σ，把“不可达”说轻了（实际 5% 对 B 组淡源只有 0.85–1.07σ，而非 1.0–1.3σ）；
(b) 用 GLS/BLUE 协方差，而 `measure` 是 OLS（`measure.cpp:383-401` 无权重）。GLS 是任何线性无偏估计的方差下界，所以他的 z 在这个方向上**偏保守**；但“估计量自身 σ≈5%”不是生产估计量的方差（X01 天空主导、方差近均匀，二者数值差 <0.3%，所以不改变结论）；
(c) 用真值 σ=1.35，而生产 `measure` 用**拟合 σ**（B 组 1.3395），漏掉 −0.78% 的统一偏置（把所有 B 组 z 整体 +0.15，不改变结论）。
**结论：lead 不需要撤回结论，但 X01 的 5% 命题必须改写成“M_i 的归一化残差在声明 σ 内”，并保留这次 10.3% 的失败记录。**

## 4. 手算最小反例（7 个）

### C1 比值统计量把峰值源的噪声注入淡源（无需任何实现错误）

设一帧峰值源真值 P=1000、淡源真值 F=100，实现完全正确但噪声使峰值测量为 P′=1027（+2.7%）、淡源恰好精确。则

`M = |100/1027 − 100/1000| / (100/1000) = |0.09737−0.1|/0.1 = 2.63%`。

淡源自身误差为 0，指标却已 2.63%。取 X01 实测：1003 的 `m/t=0.9206`、峰值 `m/t=1.0267`，`1−0.9206/1.0267=10.33%` 就是 −7.94% 与 +2.67% 的比值放大。**普通测试只查逐源通量会全过；这个反例说明 5% 阈值在 B 组不可达，且 lead 的 σ 口径与指标不匹配。**

### C2 `measure` 的“certified”通量区间漏列尺度（可让真值落在区间外）

2 像元、1 源、1 背景：`D=[[1/2,1],[1/2,0]]`，名义 `d=(5,5)`。解 `F=10, β=0`。
声明输入区间 `d₂∈[4,6]`（合法数值区间），则真实解 `F∈[8,12]`（半宽 2）。
代码：`s_F=‖H‖₂=0.7071`、`s_β=1`；scaled `D_s=[[0.7071,1],[0.7071,0]]`，`σ_min=0.5412`，`inverse_norm=1.8478`，`data_error=0.5·2=1`，故 `flux_radius=1.8478 < 2`。发布区间 `[8.152,11.848]` **不含** `d₂=6` 时的真值 12。
正确界需乘 `‖S⁻¹‖₂=1/0.7071=1.414` → 2.613 ≥ 2。**普通测试（区间有限、对称、含名义值、秩满）会通过。** 对应 `measure.cpp:416,436,439`。

### C3 未知 XP 协方差→单位阵把“精确星表”变成 38% 绝对标度误差

1 星、1 帧、1 个 latent 系数、`c_Gaia=1`、`v=1`、counts=2（方差 1）。模型 `m=a·c`。目标 `½[(2−ac)²+(1−c)²]`：对任意 c 取 a=2/c 可零化第一项，第二项给 c=1、a=2；点估计正确。gauge 方向 `c→c+δ, a→a(1−δ)` 只被 Gaia 项惩罚，`Var(δ)=1` → 单星 `σ(log a)=100%`；n 星 `Var=1/n`，X01 的 7 星 ≈ 38%。而 catalogue 的系数精确，正确模型给 0。**只查 a=2 的测试通过**。对应 `joint_fit.cpp:806-819`。

### C4 丢弃测量协方差使比值方差高估 1.79×

用合同自带的 `M-MEASURE`：`H=[[1/2,0],[0,1/2],[1/2,1/2]]`、`Z=1`、`C=I`、`d=[5,7,10]`、`F=[6,10]`、`Cov=[[8,4],[4,8]]`。
比值 `F1/F2` 的一阶方差：`8/36+8/100−2·4/60=0.1689`（σ=0.411）。
若按 `photometry.fit` 的对角近似：`8/36+8/100=0.3022`（σ=0.55），**高估 34%/方差 1.79×**。逐源 `a_k` 仍对，只查点值的测试通过。对应 `joint_fit.cpp:758-778`。

### C5 暗母版噪声根丢失

flat=1、r=1、dark 母版带共享读噪 `σ_dk=1 ADU`、单像元。真实 `Var(x)=Var_shot+Var_read+1`；实现只复合 flat 的 `−x/f` 根（`calibration_kernel.cpp:405-449`），报 `Var_shot+Var_read`。用**无噪** dark 母版做测试会通过；用有噪 dark 会看到 coverage 不足。

### C6 单位不可区分 + α 重复应用的盲区

x=1000 ADU，a=2（ADU/q 单位）→ `x′=500`（q 单位/px），但 `unit` 仍是 ADU（`apply_scale.cpp:407`，`units.cpp:26-31` 二者相等）。若第二次用**另一个** application_id 再除 2（合同未禁止），`x″=250`，单位仍是 ADU；任何只查“Unit 未变”的断言照样通过，只有 application_id 列表能发现重复。X01 的产物 BUNIT/scale 因此不可从 Unit 反查。

### C7 PSF 参数盒=最后一步：σ 的“区间”是 1e-9，而真实形状误差是 1e-2

X01 的 PSF 拟合 stderr 最后步 diff≈1e-9（`observed/e2e_summary.json` 的 normalize stderr）。`psf_fit.cpp:794-800` 给 `σ∈[1.35−1e-9,1.35+1e-9]`；本文 4 帧联合拟合的统计误差 ~2e-4（形式），而 B 组与 A 组的系统差达 1.5e-2。下游 `evaluate_psf` 把这个盒当“模板区间”（`psf.cpp:302-315`）传进 `measure` 的 design_norm，于是“模板数值误差”被低估数个量级。**只查“盒包含名义值”的测试通过。**

## 5. P0 / P1 清单

**P0（科学声明失效）**
- **P0-1** `measure` 的 `flux_numerical_lower/upper` 漏 `‖S⁻¹‖₂`（≈3.4×），却置 `flux_certified=true`、`assessment.science=bounded`（`measure.cpp:436-459,615-622`）。反例 C2：合法输入区间下真值在发布区间外。这是主科学路径上的假证书，必须先修。

**P1（语义/误差/数据边错误，可导致下游不可恢复的错误结论）**
- **P1-1** 暗母版 Jacobian 缺失（`calibration_kernel.cpp:405-449`）。
- **P1-2** 未知 XP 协方差→单位阵，7 星绝对标度 gauge 38% 且来源任意（`joint_fit.cpp:806-819`）；点估计正确故易漏测（C3）。
- **P1-3** `photometry.fit` counts 协方差恒对角；联合路径因 domain/codomain 尺寸检查用错字段而永不触发（`joint_fit.cpp:758-778`）；背景未边缘化（C4）。
- **P1-4** PRF/`pixel_response`/`sample_response_gain` 未进入 PSF 积分（`psf_model.cpp:540-575`）；真实仪器通量系统偏差无 unknown 标注。
- **P1-5** `apply_scale` 不传播 θ 随机影响到图像噪声（`apply_scale.cpp:138-160,279-290`），合同 `photometry.apply.md:72` 的 `diag(αx)H L_θ` 项缺失。
- **P1-6** `apply_scale` 不更新单位，且 `unit_adu≡unit_signal`（`apply_scale.cpp:407`；`units.cpp:26-31`）。
- **P1-7** `measure` 用 WCS 位置而 `fit_psf` 用自由位置（`measure.cpp:162`；`psf_fit.cpp:326-330`），模型不一致且未标注。
- **P1-8** `measure` 背景列硬编码 1（`measure.cpp:363`），无 Z=1 适用条件检查。
- **P1-9** `response.integrate` 无认证积分路径，只有名义 GK（`api.cpp:466-470,565-567`）；合同“禁止 GK 冒充证书”未被违反（已标 approximate），但能力缺口是规格级。
- **P1-10** `psf_fit` 目标无权重、Jacobian 差分、参数盒=步长（`psf_fit.cpp:357,384,422,794-800`）。
- **P1-11** `normalize.cpp:1129` 清空 `result.published`，CLI 永不发 `product_published`（`apps/cli/commands.cpp:1206`）。
- **P1-12** 逆向 root_box 的 ε 非区间包围（`wcs.cpp:939`），容差 1e-11 硬编码（`:874`）。
- **P1-13** 工作流覆盖 `PhotometryRequest.frames`（`normalize.cpp:802-804`）。
- **P1-14** 资源池/上限硬编码（`normalize.cpp:348-354`），CLI 配置不生效；catalogue provider 调用在 Host/plan 外。

**P2（受控/可后修）**
- `healpix.cpp:26` 与 `chart.cpp:59` 疑似重复 face→sky（未差分验证）。
- `normalize.cpp:431-433` 空占位块。
- `psf_fit.cpp:384` 位置差分步长 1e-3 无误差界。

## 6. 需要 lead 实跑的命令（按优先级）

```bash
# 1. 复现 X01 现状与 lead 的口径（应仍得 0.1033 / max|z|<1.94）
cd "/workspace/Astro CS Database"
python3 validation/x01/compare_x01.py
python3 validation/x01/uncertainty_x01.py

# 2. 重建并用三命令复跑（验证 P0-1/P1-11 修前状态）
cmake --build build/science -j16 --target acsd
python3 validation/x01/e2e_x01.py        # 产出 validation/x01/observed/*
grep -c product_published validation/x01/observed/e2e_events.jsonl   # 期望 0 —— 证实 P1-11

# 3. P0-1 的实验：在 measure.cpp 临时打印 flux_radius 与 (‖S⁻¹‖₂·flux_radius)，
#    对 B 组四帧比较 |F−truth| 与两种半径（只读实验，不提交）
#    期望：truth 偏差（~8%）远大于两者；但证书口径下界必须用 3.4× 后的值

# 4. C3 的实验：给 X01 catalogue 的每颗星加真实 xp 协方差（如 1e-6 I）与
#    当前“无协方差”两种输入，比较 PhotoSolution 的 parameter_lower/upper 宽度
#    期望：无协方差时出现 ~e^{±0.38} 的标度盒；有协方差时收缩到 e^{±0.01}

# 5. C5 的实验：给 calibration 增加一个带共享噪声根的 dark 母版，
#    比较 measure 发布的 joint_noise 是否包含 (r/f)²σ_dk²
```

## 7. 仍无法证明 / 依赖他人记录

1. **盲解求解器内部**：`astrometry.cpp/astrometry_fit.cpp` 的 quad 枚举、最大匹配、SIP 升阶、留出判据未逐行复核；本文只证明“B 组在 X01 上输出 0.162–0.257 px 且未发布无解帧”，不证明求解器一般正确（U3）。
2. **catalogue index/KD/query 分页**：只核了传播公式与 X01 的 7 星；分片、cursor、quad 索引内部未复核。
3. **regroup 的洞/缺孔 footprint 与 signed linear_view**：未构造反例，不能声称已证。
4. **`evaluate_psf` 的 transfer 分支**（非恒等 G/α/R）**在 X01 中未被执行**；其绝对系数界（`psf.cpp:339-354`）只有代码阅读，未独立验证。
5. **`response.integrate` 的分段积分数值**：X01 的 response 是 1 nm 采样的解析曲线，本文未独立重算 v；10.3% 与 response 无关（比值指标消去公共标度），但 N3 的数值准确度未证。
6. **B1 上游**：`jacobi_svd/classify_rank/pseudo_inverse` 的证书已由 B1 修复，但本文未重跑 interval oracle；`measure`/`photometry` 的 rank 结论依赖它。
7. **真实数据适用性**：全部结论仅限 X01 合成模型；真实 flat/PRF/PSF 尾/大气色项未验证（REQUIRES_REAL_DATA）。

## 8. 建议修复顺序（不改变科学模型的部分优先）

1. **P0-1**：给 `flux_radius` 乘 `‖S⁻¹‖₂`（或改用对 `D` 的区间线性求解直接验证每个分量），并把 `flux_certified` 的语义收紧为“numerical enclosure of the linear solve”，不得与统计/模型误差混称 bounded。
2. **P1-11**（一行，去掉 `clear()`）与 **P1-3/P1-4**（joint_noise 的 flux 块提取 + 背景边缘化；PRF 乘入积分）——这三项直接决定产品误差语义，先于任何性能工作。
3. **P1-2**：未知 XP 协方差不得用单位阵；应返回 `not_identifiable` 或把 gauge 固定为 `c_s≡c_Gaia` 并显式声明“绝对标度未定”（合同 `GAIA_RESPONSE.md:23` 已说不允许分离口径/gain）。
4. **P1-5/P1-6**：apply_scale 增加 θ influence 项、更新 Unit 的 scale 名。
5. **P1-7/P1-8/P1-10**：measure 与 fit_psf 统一位置身份；Z 列按同一 PRF 链构造；psf_fit 用声明噪声权重并把参数盒改为真实 score 根盒（否则明确 `conditional`，不要让下游把 1e-9 当模板误差）。
6. **P1-9/P1-12**：实现 response 的认证积分决策程序；`inverse` 的 ε 改为区间包络。
7. **P1-13/P1-14/P2**：工作流帧序、资源来自配置、重复 face chart 差分验证。
8. **X01 命题**：把 `signal_relative` 从“逐源绝对 5%”改成“按峰值归一的 M_i 的归一化残差”，并保留本次 10.3% 失败记录；同时修正 X01 的 dark 母版（12 e⁻/s × 100 s = 480 ADU）以便真正检验 N0。
