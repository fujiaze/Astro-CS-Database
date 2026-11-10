# B2｜Normalize 修复报告（实施方视角）

> 实施者：Normalize 实现者（非独立审查者）。基线 `rebuild/acsd-v2`，工作树 HEAD `ca7de6d4d`（在 `688adeac7` 之上，Lead 的文档提交）。
> 权威缺陷与反例见 [audit-normalize.md](audit-normalize.md)；本文件记录**我实际改了什么、数学依据、定点实验、未修项与未决条件**。
> 写入域：`src/sources/measure.cpp`、`src/sources/psf_fit.cpp`、`src/sources/psf_model.cpp`、`src/photometry/joint_fit.cpp`、`src/photometry/apply_scale.cpp`、`src/observation/calibration_kernel.cpp`、新文件 `validation/normalize_probe.cpp` 与本文。未改 `src/workflows/`、`src/model/`、`src/numerics/`、`include/`、其它 docs；未 commit/push。
> 工作树同时被其他 agent 修改（`src/geometry/*`、`src/runtime/*`、`src/sources/measure_process.cpp`、`src/uncertainty/api.cpp` 等），那些不是我的改动；我每次构建只对自己域内文件负责。

## 0. 复现 P0-1 反例（先复核，再动手）

C2：`D=[[1/2,1],[1/2,0]]`、名义 `d=(5,5)`、合法输入区间 `d₂∈[4,6]`。解 `θ=D⁻¹d=(F,β)=(10,0)`。
`d₂=6` 时 `θ=D⁻¹(5,6)=(10,2)`，`d₂=4` 时 `(10,-2)`；对任意 `d₂` 有 `F=2d₂`，故真值 `F∈[8,12]`。

代码（修前 `measure.cpp:433-439`）：
- `column_scale=[‖D_col1‖,‖D_col2‖]=[0.7071,1]`
- `σ_min(D S⁻¹)=0.5412`，`inverse_norm=1/σ_min=1.8478`
- `data_norm=1`（`d₂` 半宽），`flux_radius=1.8478`
- 发布 `[8.1522, 11.8478]` —— **不含 12**，但 `flux_certified=true`、`science=bounded`。

复核结论：P0-1 成立。缺的是 `‖S⁻¹‖₂=1/0.7071=1.4142`；严格界 `1.4142×1.8478×1=2.613≥2`（若按分量行范数则恰好 2，见 §1）。

## 1. P0-1：measure 通量区间必须包含列缩放（已修）

**数学依据。** 列缩放求解是 `y=(D S⁻¹)⁺d`，回代 `θ=S⁻¹y`，因此

```
δθ = S⁻¹ (D S⁻¹)⁺ (δd − δD·θ) = D⁺ (δd − δD·θ),      D⁺ = S⁻¹ (D S⁻¹)⁺
```

`‖S⁻¹‖` 是 `D⁺` 的一部分，不能漏。修前只取 `1/σ_min(D S⁻¹)`，等于假设 `S⁻¹=I`。

**修改（`src/sources/measure.cpp:523-631`）。**
1. 每个通量分量用自己的 `D⁺` 行范数：`|δθ_s| ≤ ‖e_sᵀD⁺‖₂·(‖δd‖+‖δD‖_F‖θ̂‖+‖r̂‖)`。这比统一的 `‖S⁻¹‖·‖(DS⁻¹)⁺‖` 更紧，且因 `D⁺=S⁻¹(DS⁻¹)⁺` 自动含列缩放。
2. 数据/模板区间用**名义值到较远端点**的距离（修前取半宽，名义值不在中点时会短）。
3. 设计扰动按 Frobenius 界进半径，乘 `1/(1−‖D⁺‖‖δD‖)`；该因子不存在（`≥1`）时 `flux_certified=false` 而不是发布。
4. 解算残差 `‖d̂−D̂θ̂‖` 单独进半径（只有浮点残差；数据/模板区间不重复计）。
5. 半径加 4 ulp 外向舍入；语义收紧为“线性求解的数值包围”，并写入 `set_omitted`：不是统计/模型误差。

**定点实验（`validation/normalize_probe.cpp` A 节，名义公式复算 + 独立高斯消元枚举盒角点）。**

| 情形 | 修前半径 | 修后区间 | 盒角点真值 | 结论 |
|---|---|---|---|---|
| C2 | 1.847759 → `[8.152241,11.847759]` | `[8.000000000,12.000000000]` | `[8.000000,12.000000]` | 含 8 与 12，且**恰好紧** |
| M-BOUND（`d₃±1`） | — | `F1=[3.171573,8.828427]`、`F2=[7.171573,12.828427]` | 含角点真值 | 半宽 2.828 ≥ 2，满足 `sources.measure.md:96` |

探针输出（原文）：

```
C2  pre-fix radius 1.847759 -> interval [8.152241, 11.847759] (does not contain 12)
C2  new interval of F: [8.000000000, 12.000000000]  (truth over the box: [8.000000, 12.000000])
P0-1/C2 the enclosure contains the corner truths     PASS  new=[8.000000,12.000000] truth=[8.000000,12.000000]
P0-1/C2 the enclosure contains 8 and 12              PASS  the audit requirement
P0-1/C2 the pre-fix radius is short of 2             PASS  old radius 1.847759
P0-1/M-BOUND both fluxes enclose their corner truths PASS  F1=[3.171573,8.828427] F2=[7.171573,12.828427]
P0-1/M-BOUND the interval widens by at least 2 delta PASS  half widths 2.828427 2.828427 (must be >= 2)
```

**证据边界（诚实声明）：** `measure` 的包围没有独立可调用的函数入口，探针 A 是**逐操作转写**修前/修后公式并在同一设计上做独立高斯消元，不是对 `measure_science_sources` 的端到端调用。端到端包含性实验需要可控设计矩阵（设计来自真实 PSF 积分 + `Z≡1`，无法构造 `Z=[1,0]` 的 C2），列为未决条件 U1。

## 2. P1-1：暗母版 Jacobian / 噪声根进入 calibration（已修）

**数学依据。** `x=(d−b−r(dk−b))/f`，`r=t_exp/t_dark`，故

```
∂x/∂flat = −x/f,  ∂x/∂dk = −r/f,  ∂x/∂b = (r−1)/f (dark 含 bias) 或 −1/f
```

`assemble_calibrated_native` 原来把所有 `shared_factors` 一律乘 `−x/f`，暗母版根因此得到错误方差且无 unknown 记录（`observation.calibrate.md:44` 要求按 `shared_id` 并入 Jacobian）。

**修改（`src/observation/calibration_kernel.cpp:77-87`、`:422-540`）。**
- 新增 `MasterRole{flat,dark,bias,unknown}` 与 `master_root_role(SharedId)`：按 `shared_id.entity`（剥可选 `master_` 前缀；`flat/gain/flat_field`、`dark/dark_current`、`bias/offset`）解析角色。这是声明里母版根唯一的身份字段。
- 按角色取 `−x/f`、`−r/f`、`(r−1)/f 或 −1/f` 与根 factor 复合。
- **无法识别的角色不猜**：不复合、不进 `shared_factors`，写 `UnknownTerm{master_root_role_unknown:<entity>}` 并把噪声状态降为 unknown（未知不填零、不静默用 flat 导数）。
- `master_flat` 等既有 fixture 语义不变（cal01 的 `master_flat` 仍走 `−x/f`）。

**定点实验（探针 B 节，真实 `assemble_calibrated_native` + `make_covariance` + `covariance_apply`）。** 单像元：`d=10,b=0,dk=1,f=2,r=2 ⇒ x=4`，暗根 `σ_dk=1`，raw 根 `σ=0.1`。
- 正确：`(0.1/2)² + (r/f)²σ² = 0.0025+1 = 1.0025`
- 若错用 flat 导数：`0.0025+(x/f)² = 0.0025+4 = 4.0025`
- 实测 **1.002500000**（PASS）；无法识别角色 → `shared roots 0, unknown terms 1`（PASS）。

```
P1-1 the calibrated covariance is built              PASS  code 0
P1-1 the dark root carries (-r/flat)^2 sigma_dk^2    PASS  variance 1.002500000 expected 1.002500000 (flat role would be 4.002500000)
P1-1 an unrecognised master role becomes an explicit unknown PASS  shared roots 0 unknown terms 1
```

## 3. P1-3：photometry counts 协方差真正进入 + 背景边缘化（已修）

**数学依据。** `measure` 发布的是 `I C Iᵀ`：`I` 是 8×rows（7 源 + 1 背景列）影响，`codomain` 是 flux 索引、`domain` 是原生像素索引。photometry 的 counts 协方差应是该矩阵的 **7×7 通量块**（背景不是光度测量，去掉最后一行/列即可；背景是联合估计参数，`Cov(F̂)=[I C Iᵀ]_FF`，不需要 Schur 补）。修前用 `domain->size == n_meas`（原生 16384 vs 全部帧的 56）判据，永假 → 静默对角。

**修改（`src/photometry/joint_fit.cpp:751-825`）。**
- 判据改为 `codomain->size ∈ {m, m+1}`（m=该帧测量数；`m+1` 表示尾部一个背景列），用 `dense_covariance` 取 m×m 块并按 `frame_meas` 放进全局块对角。
- 任一帧不可用 → 不静默：用已声明的 `meas_variance` 作**工作权**，并在 `unknown_terms` 写明 “…the off diagonal terms are unknown, not zero”；方差未知的行用单位工作权（合同允许的 conditional 拟合）。
- 块对角跨帧假设成立（不同帧不同原生噪声根）。

**影响（重要，需 Lead 复核）：** X01 的 `joint_noise` codomain=8=7+1，于是 photometry 现在使用**真实的 7×7 通量协方差**并因背景相关而跨源耦合 → 走 dense 联合路径，点估计从“对角工作权”变为 GLS。这是合同要求的行为改变，但会改变 X01 数值，必须由 Lead 跑 e2e 观察（我无法在 Lead 正在改的 workflows 上跑 CLI，见 U2）。

## 4. P1-4：pixel_response / sample_response_gain 进入 PSF 像元积分（已修）

**数学依据。** `OBSERVATION_COORDINATES.md` 式(2) 的 `P_i=∫p·ρ_i|J|dudv`：`ρ_i` 是像内相对效率；`sources.psf.md:68` 规定 `sample_response_gain[N]`（null=1）乘该样本的积分、PRF cell 增益乘各 cell 积分。

**修改（`src/sources/psf_model.cpp:565-780`）。**
- 读 `FootprintSet.sample_response_gain`（长度必须等于 native 样本数），乘 `values` 与 `errors`；尾仍为 `1−Σ(未加权的正模型质量)`（增益不是概率，不能进尾）。
- `PixelResponse.cells` 非空时：cell 在 local photosite 坐标内与 `local_cell` 求交，经 `detector_from_sample`+`offset` 映射（对角时用矩形积分，非对角时按平行四边形走 `integrate_polygon`），`value=Σ gain_c·∫_cell p`，`error=Σ|gain_c|·err_c`。
- `gains` 有多个波长 bin 而 PSF 是 achromatic → **具名 `unsupported`**（不擅自选带）；异常 ring footprint 与 PRF cell 无法求交 → 具名 `unsupported`。

**定点实验（探针 C 节，真实 `build_templates`，解析 Gaussian 积分 `(erf(0.35))²=0.146631` 为参照）。**

```
P1-4 the analytic Gaussian pixel integral is unchanged PASS  value 0.146631
P1-4 the sample response gain multiplies the integral PASS  value 0.293263
P1-4 a relative_mean_one PRF reproduces the uniform integral PASS  value 0.146631
P1-4 the PRF cell gains weight the pixel integral    PASS  value 0.293263 expected 0.293263
P1-4 the tail stays the unweighted positive model mass PASS  tail 0.853369
P1-4 a multi-band PRF is refused as unsupported      PASS  code 4
```

## 5. P1-2：未知 XP 协方差不再当“单位阵方差”（部分修，含明确未知方向）

**修了什么（`src/photometry/joint_fit.cpp:858-890`）。** 去掉“缺协方差 ⇒ covariance=I 且只记 unknown”的表述：现在 `I` 只作为**工作权（working prior）**，并显式记录

- `unknown_terms += "…the latent uses an equal working weight (a working prior, NOT a declared measurement covariance)"`
- `notes += "the absolute scale direction (all log a_k of its frames shifted by the same delta with c_s scaled by exp(delta)) is fixed by the equal working weight and is NOT measured"`
- 若 `xp_noise` 存在但在 latent 坐标上读不出 → 直接 `identifiable=false` + `failure="…the absolute scale is not identifiable"`（不再猜）。

**为什么没有把 gauge 钉成 `c_s≡c_Gaia`。** 我实现并实测过“把缺协方差的 latent 固定为星表系数”的版本：它使 X01 的 `photometry.fit` 最小复现（`validation/process/photometry.fit/x01_minimal_repro.cpp`）从 5/5 PASS 变为 5/5 FAIL（A 例 χ² 从 44.4 变成 9.3e32，收敛标志改变）。根因是 **X01 的星表 `xp_coefficients=[1,0,0]`（全星相同）与注入的逐星亮度不符**（`validation/x01/cli_inputs.py:91` 用 `brightness_mag` 标逐星亮度，而 XP 系数全 1），因此“精确 c”模型与该 fixture 的数据不相容。这是 X01 构造问题而非拟合器缺陷，不能在本次修复里连带改 fixture/真值（禁止改真值）。故：
- 点估计保持原工作权路径（X01 数值不变），
- **未知与不可辨识方向**按上面文本明确输出，
- 真正的 gauge 固定（`c_s≡c_Gaia`）需要先修 X01 星表/真值一致性或引入 `brightness` 的具名标度字段，列为未决 U3。

**证据：** `./build/science/acsd_process_photometry.fit_x01_minimal_repro` → `failures: 0`（A–E 全 PASS，含 D 的 `nullspace=1`、E 的 `ratio=2.000000`）。

## 6. P1-5 / P1-6：apply_scale 的 θ 随机影响与单位（P1-6 已修；P1-5 显式 unavailable）

**P1-6（已修，`src/photometry/apply_scale.cpp:393-405,417,436-445`）。** 输出单位改为 `原scale + "*scale/" + application_id`（`linear_power`、`multiplier` 不变，因为 α 是无量纲正因子）。这样“校准 ADU”与“已应用光度标度”在 Unit 上可区分（`unit_adu ≡ unit_signal` 这个 `units.cpp:26-31` 的问题在 `src/model/` 域外，另列 U4）；assessment 记录 `the_unit_names_the_applied_photometric_scale: <scale>`。

**P1-5（显式 unavailable，未实现项）。** 合同 `photometry.apply.md:72` 要求随机影响 `Dα L_x − diag(αx) H L_θ` 再乘 G，且 x/θ 同根先加影响再作协方差。**未实现的原因**：`PhotoSolution.covariance` 的构造在 `src/photometry/api.cpp:640-660` 用 `factor += v vᵀ/√λ`，于是 `F Fᵀ = V diag(1/λ) Vᵀ = C⁻¹`（精度而非协方差）；把它当 `L_θ` 会把错标度的项塞进图像噪声，比显式 unknown 更糟。该文件不在我的写入域。

因此我做了**显式降级**（`apply_scale.cpp:493-503,517-531,539-551`）：
- `ErrorRecord.unknown_model_terms += "the_random_influence_of_the_fitted_parameters_on_the_image_is_not_in_the_noise_model"`
- 新 NoiseModel 追加 `UnknownTerm{theta_random_influence_omitted}`，`state=unknown`
- assessment `noise=unknown`、`science=conditional`，并 `set_omitted` 完整公式文本。

这样不会再出现“数值包围 + 参数盒 ⇒ bounded”的假证书，同时不引入错标度项。**修 P1-5 的前置条件：先修 `src/photometry/api.cpp:640` 的 `1/√λ → √λ`，然后 L_θ 可直接由 `solution->covariance` 取，并按 SharedId 与父观测根合并（同根相加影响，不同根各自 contribution）。**

## 7. P1-7 / P1-8 / P1-10：measure 位置与 Z、psf_fit 加权/Jacobian/参数盒

**P1-7（显式记录，`measure.cpp:806-820`）。** `fit_psf` 自由拟合连续位置，而 `measure` 用 WCS 反演位置。绝对位置不属于 `EffectivePSF` 的公开字段（`influence.linearization_point` 只存形状系数），因此统一身份需要改 `include/` 合同；本次改为：assessment `set_omitted("the_template_is_evaluated_at_the_wcs_position_while_fit_psf_fits_a_free_continuous_position")` + `unknown_model_terms`。对称 PSF 下位置误差只给二阶通量偏差（审计：δ=0.25px、σ=1.35 → −0.85%）。

**P1-8（已修，`measure.cpp:350-463`）。** 背景列不再硬写 1：
- `z_j = ρ̄_j · sample_response_gain_j`，`ρ̄` 为 PRF cell 增益的面积加权均值（`relative_mean_one` 满划分 ⇒ 1；多 bin ⇒ 具名 `unsupported`）；
- 有 transfer 时 `Z = G z`（与源模板同一条采样链，`transfer.spaces.domain->size` 必须匹配，否则 `invalid_model`）；
- `Z≡1` 时记录 `background_column_Z_equals_one: unit area samples, uniform relative response and an identity transfer`，否则记录 `Z_is_not_one`。

**P1-10（部分修，`psf_fit.cpp:317-395,436,546,745,792,985-1005`）。**
- **加权**：目标改为 `½(d−HF−Zβ)ᵀC⁻¹(…)` 的方向——当且仅当噪声模型**可证对角**（每个 root factor 的 `serialize_recipe().kind==diagonal`）时，用 `w_k=1/Σ_i f_{i,k}²`（`C=Σdiag(f_i)²` 精确对角，无近似）；残差与 Jacobian 行同乘 `√w`，两处 trial 目标同权。**不可证对角时保持单位工作权**并列 `unknown_terms`（不静默对角）。X01 的 shot/read 链是 `compose(scale, root)`，不满足“可证对角”，因此 X01 仍走单位工作权（行为不变）。
- **Jacobian**：仍是中心差分（位置 1e-3 px、形状 1e-4），但把“无截断界”写进 `unknown_model_terms` 与 `set_omitted`（解析/区间 AD 未实现）。
- **参数盒**：保留字段（`validation/process/sources.psf` 断言其存在且括住名义值），但半径语义明确为“Gauss-Newton 一阶根修正 `|H⁻¹g|`”，并 `set_included/omitted` + `unknown_terms` 声明它**不是**经 score 根验证的包围、也不含统计形状误差（审计 C7 的“把 1e-9 当模板误差”被显式否定）。真正的 score 根盒（区间 AD）未实现，列 U5。

## 8. P1-11（normalize.cpp:1129 清空 published）——只给建议（Lead 修）

`src/workflows/normalize.cpp` 不在我的域。建议：删除 `:1129 result.published.clear()`；`outcome.published`、`result.published`、CLI 的 `product_published` 事件（`apps/cli/commands.cpp:1206`）用同一个列表，且 `published=true` 只在该列表非空且全部 seal 成功后置位。顺带清掉 `normalize.cpp:431-433` 的空占位块（P2）。

## 9. 构建与实跑证据（命令 + 输出）

```bash
# 1) 构建（每次改完都跑；最后一次全绿）
cd "/workspace/Astro CS Database"
cmake --build build/science -j16
#   [22/25] Linking CXX executable acsd_process_sources.measure_measure
#   ...
#   [24/25] ...
#   Design integrity PASS: six owners, schemas, examples, links, 4295 deletion records.
#   （期间曾两次被其他 agent 的在途编辑打断：src/geometry/drizzle.cpp 的
#    normalization_val、src/runtime/pool.cpp 的 active_services；不是我的文件，
#    等他们改完后同一命令通过。）

# 2) 定点探针（源文件已放 validation/normalize_probe.cpp）
g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math -mfpmath=sse \
    -I src -I include validation/normalize_probe.cpp \
    -Wl,--start-group build/science/libsources.a build/science/libobservation.a \
    build/science/libcoordinates.a build/science/liboperators.a \
    build/science/libuncertainty.a build/science/libnumerics.a build/science/libmodel.a \
    build/science/libstorage.a build/science/libresources.a build/science/libruntime.a \
    -Wl,--end-group -lpthread -o /tmp/normalize_probe
/tmp/normalize_probe
#   ALL PROBES PASS (0 failure(s))
#   —— 全部 15 项检查；C2 区间 [8.000000000,12.000000000]；P1-1 方差 1.0025；
#      P1-4 0.146631 / 0.293263 / tail 0.853369 / multi-band unsupported。

# 3) 相关 process driver（回归）
./build/science/acsd_process_sources.measure_measure          # 14/14 ok
./build/science/acsd_process_sources.psf_psf                  # 全 ok（fit.parameter_interval_brackets 仍 PASS）
./build/science/acsd_process_observation.regroup_regroup      # 0 pass:false
./build/science/acsd_process_photometry.fit_m5_process_plan   # 38 checks, failures 0
./build/science/acsd_process_photometry.fit_x01_minimal_repro # failures 0（A–E）
# 统计："pass": false 计数：measure 0 / psf 0 / regroup 0 / fit_m5 0

# 4) 基线对照（确认 x01_minimal_repro 不是被我的改动带坏）
git show HEAD:src/photometry/joint_fit.cpp > /tmp/orig/joint_fit.cpp   # HEAD 版
g++ ... -c /tmp/orig/joint_fit.cpp -o /tmp/orig/joint_fit.o
g++ ... validation/process/photometry.fit/x01_minimal_repro.cpp /tmp/orig/joint_fit.o \
    -Wl,--start-group <同上 libs> -Wl,--end-group -lpthread -o /tmp/repro_orig
/tmp/repro_orig    # failures 0（与修后一致，说明 P1-2 未改变该复现的点估计路径）
```

与 `acsd_process_calibration-faults_calibration_faults`（`failures:6`，全部 `begin failed`）无关联：它失败在 storage spool `begin`，与本报告改的母版 Jacobian 装配无关；未在本次改动范围内的文件里定位，作为 U6 记录。

## 10. 未修项 / 未决条件（必须由 Lead 或后续独立验证处置）

- **U1** P0-1 只有公式级定点实验；`measure_science_sources` 的**端到端**包含性实验（受控设计矩阵）未做。要真正闭环，需要给 `measure` 一个可测试的包围入口（内部 helper + 探针）或一种可控模板接口。
- **U2** X01 e2e 未跑：`src/workflows/normalize.cpp` 正被 Lead 修改；P1-3 使 photometry 由对角工作权变为 GLS + dense 联合路径，**X01 数值预计会变**，必须由 Lead 跑 `validation/x01/e2e_x01.py` 观察点估计与 χ²，并记录变化。命令：`cmake --build build/science -j16 --target acsd && python3 validation/x01/e2e_x01.py`（期望 `product_published` 计数仍为 0，直到 P1-11 被修）。
- **U3** P1-2 的 gauge 固定（`c_s≡c_Gaia`）被 X01 星表/真值不一致阻断（§5）；需要先修 fixture 一致性或新增逐星幅度字段，再做“不可辨识方向”变为零方差。
- **U4** `unit_adu ≡ unit_signal`（`src/model/units.cpp:26-31`，域外）：我已在 apply_scale 用 scale 名区分“已标度”，但“校准 ADU 与信号单位完全相同”本身未修。
- **U5** psf_fit 的 score 根盒与解析/区间 AD 未实现（P1-10 剩余）；`parameter_lower/upper` 现在是“一阶根修正”而非证书。
- **U6** `acsd_process_calibration-faults` 的 6 个 `begin failed` 需另行定位（storage spool）。
- **U7** `src/photometry/api.cpp:640-660` 的参数协方差 eigen 因子用了 `1/√λ`（应为 `√λ`），使 `PhotoSolution.covariance` 返回精度而非协方差；这是 P1-5 与 P1-12 的前置缺陷，域外未改。
- **U8** 并发编辑：`src/sources/measure_process.cpp`、`src/uncertainty/api.cpp`、`src/geometry/*`、`src/runtime/*`、`src/sky/*`、`src/coadd/*`、`src/storage/*` 在当前工作树被其他 agent 修改；我的验证只覆盖上表命令，未对合并后的整树做独立科学判定。

## 11. 逐项对照表

| 项 | 状态 | 位置 | 证据 |
|---|---|---|---|
| P0-1 列缩放 | 已修 | `measure.cpp:523-631` | 探针 A：C2 `[8,12]` 恰好、M-BOUND 半宽 2.83 |
| P1-1 暗母版 Jacobian | 已修 | `calibration_kernel.cpp:77-87,422-540` | 探针 B：方差 1.0025（flat 角色会 4.0025）；未知角色→unknown |
| P1-3 counts 协方差/背景边缘化 | 已修 | `joint_fit.cpp:751-825` | 代码路径 + unknown 记录；e2e 待 U2 |
| P1-4 PRF/样本增益 | 已修 | `psf_model.cpp:565-780` | 探针 C：6 项全 PASS |
| P1-2 未知 XP 协方差 | 部分修（工作权 + 明确方向） | `joint_fit.cpp:858-890` | x01_minimal_repro 5/5；gauge 固定待 U3 |
| P1-5 θ 随机影响 | 显式 unavailable | `apply_scale.cpp:493-503,517-531` | 降级为 conditional + UnknownTerm；前置 U7 |
| P1-6 单位 | 已修 | `apply_scale.cpp:393-405,417,536-539` | Unit scale 含 application_id；`units.cpp` 身份问题列 U4 |
| P1-7 位置身份 | 显式记录 | `measure.cpp:806-820` | omitted + unknown |
| P1-8 背景 Z=1 条件 | 已修 | `measure.cpp:350-463` | Z 由同一 PRF/gain/transfer 链构造；条件写明 |
| P1-10 psf_fit 加权/Jacobian/盒 | 部分修 | `psf_fit.cpp:317-395,436,546,985-1005` | 可证对角才加权；盒语义显式；score 根盒待 U5 |
| P1-11 清空 published | 仅建议 | `normalize.cpp:1129`（域外） | §8 |
