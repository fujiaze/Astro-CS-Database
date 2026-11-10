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

---

## 12. 第二轮（R1/R1b/R2/R3/R4/P1-2/P1-6）：实施记录

> 基线：`rebuild/acsd-v2` 工作树 HEAD `1f48c58ad`。权威缺陷：[review-normalize-fix.md](review-normalize-fix.md)（R1/R1b/R2/R3/R4、§5/§6/§7）。
> 本轮写入：`src/sources/measure.cpp`、`src/observation/calibration_kernel.cpp`、`src/photometry/joint_fit.{cpp,hpp}`、`src/photometry/api.cpp`、`src/workflows/fits_writer.cpp`、新探针 `validation/normalize_probe2.cpp`、本文件。未改 `src/geometry`、`src/sky`、`src/coadd`、`src/runtime`、`src/operators`、`src/model`、其它 docs；未 commit/push；未改任何容差或真值。
> `joint_fit.hpp` 只加了一个语义字段 `FitResult::covariance_state_known`（P1-2 的 API 无法用既有字段表达，属最小补丁，见 §17）。
> 工作树同时被其他 agent 修改（`src/workflows/fits_writer.cpp` 在我动手前已被另一 agent 改过 BUNIT；我在其基础上继续）。未改的数据夹具缺陷见 §19。

### 12.1 R1：数据残差不是数值半径（`measure.cpp`）

**数学。** 对最小二乘，精确解满足 `Dᵀr = 0`，于是 `D⁺r = (DᵀD)⁻¹Dᵀr ≡ 0`；`δθ = D⁺(δd − δDθ)` 与数据残差 `r` 无关。`‖r‖` 是统计量（噪声 + 模型失配，量级 `√m·σ`），把它加进确定性半径会把“数值包围”变成统计区间（这正是 R1）。
浮点解算误差另有一个**可证**来源：算出的 `θ̂` 是某个扰动问题 `(D+δD)θ = d+δd` 的精确最小二乘解，且 `‖δd‖₂ ≤ γ‖d‖₂`、`‖δD‖_F ≤ γ‖D‖_F`，`γ = mε/(1−mε)`（Higham 最小二乘后向误差）。它与“声明区间”同型，可直接并入同一公式，不需要 `r`。

**修改（`src/sources/measure.cpp`）。**
- 新函数 `measure_flux_numerical_radius`（定义 `:135`，`measure_science_sources` 调用 `:740`）是半径的唯一实现：`radius_s = 1/(1−‖D⁺‖₂·‖δD‖_F) · ‖e_sᵀD⁺‖₂ · (‖δd‖ + ‖δD‖_F‖θ̂‖)`，其中 `‖δd‖ = data_norm + γ‖d‖₂`、`‖δD‖_F = design_norm + γ‖D‖_F`（Frobenius）。删除 `residual_norm`（旧 `:592-599`）。
- `flux_certified` 只在每个半径有限且 `amplification/spectral/data_norm/design_norm` 全有限时置真（`:756`），否则清空端点并写 `numerical_flux_uncertified`（原有）。
- 正规方程残差 `‖Dᵀr‖` 仍计算但只作诊断写入 assessment（`normal_equation_residual_D^T_r=`），不进半径；理由：它的围包本身需要同阶舍入界，`γ` 界才是可证形式。
- 文件头注释（`:8-15`）与半径注释同步改写，明确“数据残差是统计量，`D⁺r=0`”。

**修复前后数值（探针 A，`validation/normalize_probe2.cpp`；设计矩阵、解、行范数、旧半径由探针独立高斯消元复算，新半径调用生产函数链接）。**

| 情形 | ‖r‖₂ | ‖Dᵀr‖ | 旧半径（含 ‖r‖） | 新半径 | 相对通量 |
|---|---|---|---|---|---|
| 合成 1 源+背景，160 行，噪声 σ=30 | 213.095 | 4.61e-11 | **1055.418 ADU** | **2.212e-08 ADU** | 2.35e-11（F=942.85） |

```
R1/A2 the statistical residual case is certified   PASS  F=942.850655 ||r||=213.095 ||D^T r||=4.610e-11
  pre-fix radius=1055.418 new radius=2.212e-08 (new/F=2.35e-11) spectral=4.953
R1/A2 the pre-fix radius was dominated by ||r||      PASS  pre-fix/new = 4.77e10
R1/A2 the normal equation residual is a rounding defect PASS ||D^T r||/(||D||_F ||r||) = 1.710e-14
R1/C2 the enclosure still contains the corner truths [8,12] PASS radius 2.000000000 interval [8.000000000,12.000000000]
```

**X01 量级估计（未跑 e2e，见 §20/U2）。** 按 review 的 X01 帧 00（m=1372、‖r‖≈1045、row_norm≈4.94）：`γ = 1372·2.22e-16 = 3.0e-13`，`‖d‖₂ ≈ 1.5e5`、`‖D‖_F ≈ 37`、`‖θ‖ ≈ 4e3` ⇒ 括号 ≈ `3.0e-13·(1.5e5 + 37·4e3) ≈ 9e-8`，半径 ≈ `4.94·9e-8 ≈ 4.4e-7 ADU`（相对 ≈5e-11）。即从“通量的 52–190%”回到**相对 1e-11 的浮点解算误差量级**；绝对量级随 `m·ε·‖d‖` 增长，不是固定的 1e-12。

### 12.2 R1b：收缩因子用谱范数（`measure.cpp:226` 起）

**数学。** 不动点论证需要 `‖D⁺‖₂`；`√p·max_s‖e_sᵀD⁺‖₂` 是合法上界（`‖Ax‖₂ ≤ √p max_i|row_iᵀx|`），但只用最大行范数不是。实现：`D⁺ = S⁻¹VΣ⁻¹Uᵀ` ⇒ `‖D⁺‖₂ = ‖S⁻¹VΣ⁻¹‖₂`；构造 p×p 的 `M = S⁻¹VΣ⁻¹`（p=sources+1），取其最大奇异值（`jacobi_svd`），再与解析界 `√p·max row norm` 取**较小值**（两者都是上界，取小仍合法；小矩阵 SVD 的误差因此不影响证书）。

**反例（探针 A3，D=[[1,0],[0,1],[0.9,0.9],[0.9,0.900001]]，‖δD‖_F=1.001）：**

```
max row norm=0.786082, pre-fix contraction=0.786868 (certified)
||D^+||_2=1.000000, spectral contraction=1.001000  -> refused (radius count 0)
```

旧规则 `0.786<1` 判可认证（半径有限、自称 enclosed），谱范数规则 `1.001>1` 直接拒绝——与 review 的“证明缺口”一致。另有 A4：`data_norm=NaN` 与 `design_norm=1e300` 都返回“不认证”。

### 12.3 R2：母版角色不猜（`calibration_kernel.cpp`）

**数学。** `x=(d−b−r(dk−b))/f`。`∂x/∂flat=−x/f`、`∂x/∂dk=−r/f`、`∂x/∂b=(r−1)/f` 或 `−1/f`。`gain` 不是 flat：`∂x/∂gain=−x/gain`，只在 `gain≡flat` 时相同；反例 flat=1、gain=0.5、σ=1 时用 flat 导数会把方差算成 `x²`，真实 `(2x)²`，**小 4 倍（非保守）**。

**修改。**
- `parse_master_role`（`:111`）替换原 `master_root_role`：仅接受 entity（可带 `master_` 前缀）为 `flat/flat_field`、`dark/dark_current`、`bias/offset`；`gain` 明确 unknown 并给出理由；角色写在 `component`（如 entity=`master`、component=`flat`）明确 unknown 并说明“entity 是唯一角色声明”。
- 声明角色但对应数组缺失（`flat` 根而 `has_flat=false`、`dark` 根无 dark、`bias` 根无 bias）一律转 unknown（`:553-573`），不再用 `f=1` 的 `−x`、也不再静默零贡献。unknown 分支写 `UnknownTerm`（id 前缀 `master_root_role_unknown:`，reason 逐例不同）并把 noise state 降为 unknown（`:598`）。

**探针 B（真实 `assemble_calibrated_native`；x=4、flat=2、σ_f=0.1 时 `(−x/f)²σ_f²=0.04`，raw 根 0.0025）。**

```
R2 the flat role still carries (-x/flat)^2 sigma_f^2   PASS  variance 0.042500000 expected 0.042500000
R2 entity gain is unknown, not the flat derivative     PASS  shared roots 0 unknown terms 1
R2 a flat root without a flat map is unknown, not -x   PASS  shared roots 0
R2 a dark root without a dark map is unknown, not a silent zero PASS shared roots 0
R2 a bias root without a bias map is unknown, not a silent zero PASS shared roots 0
R2 a role in the component field is unknown and says so PASS unknown terms 1
```

对照修前：`gain` 走 flat 导数（σ=1 时方差 0.0025+4=4.0025 而非 unknown）、flat 根无 flat 数组时用 `−x`（σ=1 时 16，真实导数 0）、dark/bias 根无数组时权重全 0 且**无 unknown**。

### 12.4 R3：fallback 必须重置为纯对角（`joint_fit.cpp:853`）

**缺陷。** 前面帧写入的**非对角块**在后续帧缺 `joint_noise` 时不被清掉（旧代码只覆盖对角线），结果“部分帧相关、部分帧对角”，却记录“off diagonal terms are unknown, not zero”。

**修改。** `counts_covariance = numerics::Matrix(problem.n_meas, problem.n_meas);` 再填对角；unknown 文案补“the blocks of every frame… were discarded, not partially kept”。

**探针 C（2 帧、1 星；帧 0 两测 [100,110]、声明方差 4；帧 1 一测 [105]、方差 4）。** 手算对角 `χ²=(5²+5²)/4=12.5`；旧残差矩阵（对角被覆盖为 4、非对角 3 保留 ⇒ `[[4,3],[3,4]]`）沿 `(1,−1)` 方差 `4−3−3+4=2`、沿 `(1,1)` 为 14，`χ²=350/7=50`。

```
none    chi2=12.500000000 theta[0]=4.653960350
partial chi2=12.500000000 theta[0]=4.653960350     （修复后 == 全对角）
R3 the record says the earlier blocks were discarded PASS
old residual matrix: chi2=50.002523000 (diagonal 12.500000000)   （差 4 倍；多出的 0.0025 是 working prior 的 Gaia 行）
```

### 12.5 R4：跨帧共享母版根 → 显式 unknown 交叉项（`joint_fit.cpp:287,300,900-940`）

**数学。** 装配是逐帧块对角，等于假设不同帧的原生噪声根独立。共享母版根 `r` 破坏该假设：`Cov(F_k,F_l) = (∂F_k/∂r)(∂F_l/∂r)σ_r²`（flat 角色 `|∂F/∂r| = F/flat`），量级 `F_kF_lσ_r²`，**不是 0**。交叉项需要每帧根对通量的影响，块对角装配不携带它，故按合同（`photometry.fit.md`“共享校准根须作为全局 latent/稀疏耦合保留，不能删去交叉块”）记录为 unknown，而不是补一个猜的矩阵。

**修改。** `shared_root_key`（`:287`）用 `external_namespace|entity|model_revision|component` 标识共享根；`frame_shared_roots` 从 `joint_noise->model` 与 `observation->noise` 收集；检测到同一 key 出现在 ≥2 帧即写 `unknown_terms`（“shared by the frames …; the cross frame block Cov(F_k,F_l) = (dF_k/dr)(dF_l/dr) sigma_r^2 is unknown, not zero”）与 `notes`（含精确小例与该组帧最大通量）。单帧持有同一根不记录。

**探针 D。** 两帧各 1 测、共享 `master_flat` 根：

```
R4 the shared root of two frames is recorded as an unknown PASS  unknown terms 2
R4 the record names the omitted cross covariance            PASS  Cov(F_k,F_l)
R4 a single frame with the same root records nothing        PASS
R4 the worked example: current 0 vs correct 100 ADU^2       PASS  Cov = F^2 sigma_f^2 = 100.000 ADU^2; block diagonal = 0.0
```

**当前值/应得值（σ_f=0.01、F=1000、两帧）**：块对角装配给 `Cov(F₁,F₂)=0`；应得 `=F²σ_f²=100 ADU²`（同根对每帧方差也是 100，故两帧经此根完全相关，其余噪声另加）。**未修**：交叉块本身仍未进入协方差（见 §21）。

### 12.6 P1-2：working prior 的协方差状态（`joint_fit.cpp:992`、`api.cpp:670`）

- `FitResult` 新增 `covariance_state_known`（`joint_fit.hpp:95`）：任何源缺 XP 协方差、latent 由等权 working prior 支撑时置 false（`joint_fit.cpp:992`）。
- `api.cpp:670`：`noise_model->state = covariance_state_known ? known : unknown`；并追加 `UnknownTerm{parameter_covariance_uses_a_working_prior}`，**不带 influence**——已知方向仍可用（`covariance_apply` 不被整体拒绝），但 gauge 方向明确不是测量。assessment 因 `unknown_terms` 非空为 conditional。

**探针 E：**

```
P1-2 the working prior marks the covariance conditional   PASS
P1-2 a measured XP covariance keeps the covariance known  PASS
P1-2 the published covariance model state is unknown      PASS  state 3 (unknown)
P1-2 the working prior is an explicit unknown term        PASS
P1-2 the covariance is still applicable on the measured directions PASS
P1-2 the published science state is conditional           PASS
```

（`fit_photometry` 端到端：1 帧 1 星、缺 XP 协方差 ⇒ `PhotoSolution.covariance->model->state=unknown` 且保留可应用性。）

### 12.7 P1-6：BUNIT 不再承载内部恒等（`fits_writer.cpp`）

**修改。** 新 `FitsUnitText fits_unit_text(Unit)`（`:268`）与 `fits_unit_text_from_scale(text)`（`:298`），三个 writer 共用：导出图（`:530`）、HiPS tile 头（`:397,441`）、`properties`（`:552-558`）。
- 若声明 scale **本身就是单位**（显式白名单：`L/sr`、`ADU`、`Jy`、`K`、`sr`…，或带 provider/fixture 前缀如 `fixture:L/sr` 时取冒号后的单位部分）→ `BUNIT` 写该单位。
- 否则 `BUNIT` 只写由 Unit 合约可证的量纲（`linear_power` + sr/pixel/nm 逆幂），内部恒等写入 `ACSUNIT`，并写 `ACSSTD='nonstandard_model_unit'` 作为**显式非标准标记**（不伪装成标准单位）；HiPS `properties` 用 `hips_unit=`（同一标准量纲）+ `acsunit=` + `acsunit_standard=false`。

**实测（`/tmp/unit_probe.cpp` 调用真实 `write_export_image` 后读 header）：**

| 单元 scale | 修前 BUNIT | 修后 BUNIT | 附加卡 |
|---|---|---|---|
| `acsd.native:x01-00*scale/x01-scale*1/sr` | `acsd.native:x01-00*scale/x01-scale*1/sr`（非标准长串） | `ADU/SR` | `ACSUNIT=…^sr-1^L1`、`ACSSTD='nonstandard_model_unit'` |
| `fixture:L/sr` | `fixture:L/sr` | `L/sr` | `ACSUNIT=fixture:L/sr^sr-1^L1` |
| `1` | `1` | `1` | — |

**副作用（需 Lead 处理）**：#112 的冻结证据 `validation/apps/evidence/export/export_evidence.json` 记录 `BUNIT: "fixture:L/sr"`；重跑该证据会得到 `L/sr`（同一物理单位、去掉夹具前缀）。这是对外部语义字段的**期望变化**，不是回归；该证据属于 #112 agent 的域，我未改。

### 12.8 构建与实跑

```bash
cmake --build build/science -j16
#   [73/74] Linking CXX executable acsd   （exit 0；期间一次 check_design 因其他 agent
#   正在写 generated schema 而瞬时失败，随后同一命令通过）

g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math -mfpmath=sse \
    -I src -I include validation/normalize_probe2.cpp \
    -Wl,--start-group build/science/libsources.a build/science/libobservation.a \
    build/science/libphotometry.a build/science/libresponse.a build/science/libcoordinates.a \
    build/science/libcatalogue.a build/science/liboperators.a build/science/libuncertainty.a \
    build/science/libnumerics.a build/science/libmodel.a build/science/libstorage.a \
    build/science/libresources.a build/science/libruntime.a -Wl,--end-group -lpthread \
    -o /tmp/normalize_probe2 && /tmp/normalize_probe2
#   ALL PROBES PASS (0 failure(s))  —— 37 项（A 11 / B 6 / C 7 / D 6 / E 7）

# 回归（每项 "pass": false 计数为 0）
./build/science/acsd_process_sources.measure_measure                # 14 checks, 0 false
./build/science/acsd_process_sources.psf_psf                        # 0 false
./build/science/acsd_process_observation.regroup_regroup            # 0 false
./build/science/acsd_process_photometry.fit_m5_process_plan         # 38 checks, 0 false
./build/science/acsd_process_photometry.fit_x01_minimal_repro        # failures 0
./build/science/acsd_process_coordinates.wcs_wcs                     # 0 false
/tmp/normalize_probe1（第一轮探针，未改）                              # ALL PROBES PASS
```

### 12.9 未修项 / 风险 / 需 Lead 决定

- **U9（R4 本体未实现）**：交叉块仍未进入 counts 协方差（只记录 unknown）。真正的修法需要把共享根的每帧影响（`I_k F_root`）从 `joint_noise` 取出并装配跨帧块；当前 `CovarianceAction` 只暴露 `I C Iᵀ` 的合成结果，不暴露根的单独影响，属 API 缺口。测量侧若要支持，需要 `SourceMeasurements` 保留每根对通量的影响（或 `CovarianceAction` 暴露按根的分解）。**在此之前 M-MEASURE 一类“共享根”用例的比值方差仍会低估。**
- **U10（X01 e2e 未跑）**：半径从 O(F) 变到 1e-7 量级后，`photometry.fit` 的数值参数盒（`joint_fit.cpp:1372` 由 `0.5(high−low)+model_radius` 驱动）会收缩到 ~1e-11；点估计/χ²/协方差不变（不读该盒），`apply_scale` 的 α 区间随之变成纯数值区间（其 θ 随机影响已是 unknown）。**需要 Lead 跑 `python3 validation/x01/e2e_x01.py` 确认 `fit_photometry` 仍 `code=ok`、参数盒仍 `certified`（search 半径 ~4e-11 时二分仍可表示），并记录 e2e 数值变化。** 我无法在并发修改的工作树上跑 CLI。
- **U11（`measure` 半径语义）**：`flux_certified=true` 现在只意味“线性求解 + 浮点后向误差”的数值包围，不再包含任何统计/模型误差；`assessment.science=bounded` 的下游含义依赖它（`photometry.fit` 的参数盒）。这与原审计的“数值/统计分账”一致，但任何把 bounded 读成“通量不确定度已知”的消费者都应改读 `flux.variance`（其 state 仍是 `observation->noise->state`）。
- **U12（`γ` 界的假设）**：浮点项用 Higham 后向误差 `γ=mε/(1−mε)`，假设标准 IEEE 双精度、无 fast-math（本构建 `-fno-fast-math -ffp-contract=off`）。若将来开启 `-ffast-math` 或 FMA 收缩，该界需重证。
- **U13（P1-2 的 gauge 伪方差仍在矩阵里）**：状态已 unknown，但 `covariance` 的 gauge 方向仍是 working prior 的值（~1/n）。P1-5 实现时必须先投影掉该方向，不能把矩阵直接当 `L_θ`。
- **U14（P1-6 白名单）**：`is_standard_unit_token` 是显式白名单（34 个 token）。新物理单位（如 `W/m2/sr/nm`）会退回 `ADU`+非标准标记而不是伪装；若产品要写新单位，应扩白名单并同步 `OBSERVATION_COORDINATES`。

### 12.10 §19 数据夹具缺陷（不在我的写域，只给修法与影响）

1. **X01 dark 夹具 4.8 vs 480 ADU**：`validation/x01/input/cli/calibration.json` 的 `dark[0]=4.8`，而 `validation/x01/truth_x01.py:102-107` 注入 `12 e⁻/s × 100 s / gain = 480 ADU`。差值 475.2 ADU 被 β 吸收，通量不变，但 **N0 的 dark 单位/曝光比路径从未被检验**。修法：dark 母版改 480 ADU 并同步 `dark_exposure_seconds`（或显式声明该案 dark=0 并另建 N0 专项）。影响：修后 R2 的 dark 角色才有真实数据覆盖；不修则该路径只能算“代码审查通过”。
2. **X01 XP 系数全 `[1,0,0]`**（`validation/x01/cli_inputs.py:91`）：注入星等按 `weight∈[0.7,1.0]` 变化，而模型 `C=a_k exp(bᵀθ)vᵀc_s` 在 constant basis 下对所有星预测同一通量，与数据差 15–30%。因此 **X01 的 `photometry.fit` 是按错配模型拟合**，`code=ok` 不能证明 gauge/绝对标度正确；也阻断 P1-2 的 gauge 验证（精确 gauge 会破坏该夹具）。修法：星表 `c_s ∝ weight`（或引入具名逐星幅度字段），并把真值 `flux` 与该系数一致；禁止用“修实现”迁就夹具。影响：修后 X01 才能验证绝对标度与 P1-2 的零方差方向。

### 12.11 逐项对照（第二轮）

| 项 | 状态 | 位置 | 证据 |
|---|---|---|---|
| R1 数据残差进半径 | **已修** | `measure.cpp:135,740,756` | 探针 A2：\|r\|=213 → 旧 1055 / 新 2.2e-8；‖Dᵀr‖/(‖D‖\|r\|)=1.7e-14 |
| R1b 最大行范数当谱范数 | **已修** | `measure.cpp:226` | 探针 A3：谱范数 1.000（旧 0.786）→ 拒绝 |
| R2 gain/缺数组/component 角色 | **已修** | `calibration_kernel.cpp:95,111,543-605` | 探针 B：6 例；有效 flat 方差 0.0425 不变 |
| R3 fallback 残留非对角 | **已修** | `joint_fit.cpp:853` | 探针 C：partial==none（12.5）；旧残余矩阵 50.0025 |
| R4 跨帧共享根 | **记录 unknown（本体未修）** | `joint_fit.cpp:287,300,900-940` | 探针 D：unknown+notes；0 vs 100 ADU² |
| P1-2 working prior 冒充 known | **已修（状态+显式项）** | `joint_fit.cpp:992`、`api.cpp:670` | 探针 E：state=unknown、项存在、仍可 apply |
| P1-6 BUNIT 泄漏 | **已修** | `fits_writer.cpp:229,268,298,397,441,530,552` | 实测三例：`ADU/SR`+ACSUNIT+ACSSTD / `L/sr` / `1` |
| X01 dark 夹具 | 未修（域外） | `validation/x01/...` | §12.10 修法与影响 |
| X01 XP 系数 | 未修（域外） | `cli_inputs.py:91` | §12.10 修法与影响 |

## 13. 复核后补丁（Lead，2026-11-10）

独立复核 §13.5 指出 R4 的跨帧共享根虽然记录了 unknown，但没有把 `covariance_state_known` 置 false，导致 XP 协方差已知时 `PhotoSolution.covariance` 仍标 `state=known`（n 帧共享 flat 时绝对标度方差被低估 n 倍）。已在 `joint_fit.cpp` 的跨帧共享根分支置 `result.covariance_state_known = false`，与 XP working-prior 分支同一规则；跨帧交叉块本体仍需 U9 的按根影响 API，未在本轮实现。R6（帧 ADU/SR → mosaic 1 → export ADU 的单位链）、R7（γ 只用 m）、R8（residual_enclosure 证书串）、R9（offset→bias 别名）作为 P1/P2 留给 B4/B5。

## 14. Lead 修正：X01 产品量级与 fittable gauge（2026-11-11）

审计发现 X01 normalize 产品 signal 达 \(5\times10^{29}\) ADU/sr，根因是 `apply_scale` 直接使用绝对 \(1/a\)：合成谱系数为 O(1) 时 \(q\sim10^{19}\)，fit 的 \(a\) 吸收该量级，\(\alpha\) 变成 \(2.3\times10^{16}\)。绝对标度在无物理响应锚时不可辨识，因此改为帧内中位数 gauge（\(\alpha/\mathrm{median}\,\alpha\)），协方差同因子缩放；同帧 signal 回到 \(8.8\times10^{13}\) ADU/sr（2800 ADU/pix ÷ 2.35e-11 sr），量级正确。同时把 X01 `xp_coefficients` 从全 `[1,0,0]` 改为 `[weight,0,0]`，消除夹具与注入亮度 \(0.7\)–\(1.0\) 的错配。该修正不改变 measured_stars 或旧 5% 失败；Export 的物理量级此前因此错误，B5 必须在新量级上重验。
