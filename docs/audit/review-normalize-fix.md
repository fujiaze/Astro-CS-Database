# B2｜Normalize 修复的独立复核（第二意见）

> 复核对象：`rebuild/acsd-v2` 工作树 HEAD `48eaf0612`；Normalize 修复内容位于 `77a13c89c`（2026-11-10），报告 `docs/audit/normalize-fix-report.md`。原审查 `docs/audit/audit-normalize.md`。
> 复核者：`normalize-auditor`（未参与该修复）。**未改代码**；本文件是唯一写入。
> 方法：逐文件读 diff 与修后源码，独立手算/独立 Python 复算（不使用 `validation/normalize_probe.cpp` 的断言），并核对该修复在 X01 实际 trace 上的结果。
> **一句话：P0-1 的列缩放在公式上确已修好（我独立复算 C2 得 `[8,12]`），但修复把“数据残差”当成“浮点求解残差”加进半径，使 X01 的“数值包围”变成 50%–190% 的统计区间；P1-1/3/4/6 的机制已接线但各有残留；P1-2/P1-5 仍是“文本降级”而非修复；X01 的 dark 夹具（4.8 vs 480）与 XP 系数（全 `[1,0,0]`）仍未修，且修复后的 X01 e2e 没有跑完。**

## 0. 逐项裁决

| 项 | 裁决 | 依据 |
|---|---|---|
| P0-1 列缩放 | **C2 已关闭；新开 R1（P1）** | `measure.cpp:579-631`；本文 §1 |
| P1-1 暗母版 Jacobian | **主路径已关闭；R2（P1）残留猜测** | `calibration_kernel.cpp:73-87,420-540`；§2 |
| P1-3 counts 协方差 + 背景边缘化 | **主路径已关闭；R3/R4（P1）残留** | `joint_fit.cpp:751-825`；§3 |
| P1-4 PRF/样本增益 | **积分已关闭；尾界 R5（P2）与“无生产者”残留** | `psf_model.cpp:561-780`；§4 |
| P1-2 未知 XP 协方差 | **仅文本降级，对象语义仍冒充 known** | `joint_fit.cpp:847-890`；`api.cpp:663`；§5 |
| P1-5 θ 随机影响 | **诚实降级；但“不可实现”的理由已随同一提交失效** | `apply_scale.cpp:492-551`；§6 |
| P1-6 单位 | **可区分；但泄漏进 BUNIT，且无强制校验** | `apply_scale.cpp:393-405`；`fits_writer.cpp:393-395`；§7 |
| P1-7 位置身份 | 显式记录（未统一） | `measure.cpp:815-818`；§3.4 |
| P1-8 背景 Z | 已关闭（X01 仍 Z=1，数值不变） | `measure.cpp:350-463`；§3.5 |
| P1-10 psf_fit | 加权仅在“可证对角”时启用；盒仍非证书 | `psf_fit.cpp:317-395,923`；§8 |
| P1-11 published 清空 | **已关闭** | `normalize.cpp:1201-1204` |
| api.cpp 1/√λ→√λ | **修正正确，无点估计变化** | `api.cpp:626-640`；§9 |
| linear_action compose 转置 | **修正正确，影响非交换链，无点估计变化** | `linear_action.cpp:325-338`；§9 |
| X01 dark 夹具 4.8 vs 480 | **仍需修（案缺陷）** | `calibration.json`；§10 |
| X01 XP 系数全 `[1,0,0]` | **仍需修（案缺陷，阻断 gauge 验证）** | `cli_inputs.py:91`；§10 |

## 1. P0-1：列缩放（C2）与新的实质缺陷 R1

### 1.1 C2 已真正修复（我的独立复算）

`measure.cpp:523-631` 的修后半径是

`radius_s = [1/(1−‖D⁺‖·‖δD‖)] · ‖e_sᵀD⁺‖₂ · (‖δd‖ + ‖δD‖_F‖θ̂‖ + ‖r̂‖)`，

其中 `D⁺ = S⁻¹(DS⁻¹)⁺`，`inverse[k][c] = pseudo(c,k)/column_scale[c]`（原 `:398`，未变）正是 `D⁺` 的转置。对审计 C2（`D=[[1/2,1],[1/2,0]]`、`d=(5,5)`、`d₂∈[4,6]`）：

- `D⁺ = (DᵀD)⁻¹Dᵀ = [[0,2],[1,−1]]`，F 行范数 = 2（我手算）；
- `‖δd‖ = max(|6−5|,|5−4|) = 1`，`δD=0`，`‖r̂‖=0`（精确解）；
- `radius_F = 2·1 = 2` → **`[8,12]`，与盒角点真值完全一致**。

即：修前的 1.8478 短少因子 `‖S⁻¹‖`（1.414），修后用逐分量行范数而不是各向同性半径，**这一点正确且比统一界更紧**。`data_error` 改为“名义值到较远端点的距离”（`measure.cpp:552-560`）也正确：名义不在中点时不再短少。

### 1.2 R1（P1，新发现）：`residual_norm` 是数据残差，不是浮点求解残差

`measure.cpp:592-599`：

```cpp
double model = Σ_c design(k,c)*solution[c];
const double residual = measurement[k] - model;   // 这是最小二乘的数据残差
...
residual_norm = sqrt(Σ residual²);
```

然后 `:618` 把它加进半径：`common = data_norm + design_norm*solution_norm + residual_norm`。

**数学事实**：对最小二乘，精确解满足 `Dᵀr = 0`，且 `r` 与 `D` 的列空间正交，因此 `D⁺r ≡ 0`；`r` 对解的一阶扰动**没有任何贡献**（`δθ = D⁺δd` 与 `r` 无关）。浮点求解残差应是 `‖Dᵀ(d−Dθ)‖/‖D‖` 或后向误差，量级 `ε·‖d‖ ~ 1e-12 ADU`；而 `‖r‖` 是**统计残差**（噪声 + 模型失配），量级 `sqrt(m)·σ_pix`。

**独立数值（X01，trace 实测位置 + 真值 σ，逐帧 OLS）**：

| frame | rows | ‖r‖₂ (ADU) | max row norm | 修后半径 (ADU) | 半径/F |
|---|---|---|---|---|---|
| 00 A | 1372 | 1045.6 | 4.943 | 5168 | 52%–73% |
| 04 B | 1183 | 1133.8 | 4.952 | 5614 | 92%–166% |
| 06 B | 1183 | 1279.2 | 4.952 | 6335 | 102%–190% |

于是 `measure` 仍置 `flux_certified=true`、`assessment.science=bounded`（`:631,795`），但发布的“数值包围”已是通量的 0.5–1.9 倍，并且 `photometry.fit` 的参数盒半径直接取 `0.5(high−low)`（`joint_fit.cpp:1261`）——**参数盒由此变成统计盒**，而它自称“numerical parameter box”。这不是包含性失败（区间更大、仍含真值），而是**种类错误**：把统计残差写进确定数值半径，恰好违反原审计要求的“数值/统计分账”。
建议：删除 `residual_norm`（或改为正规方程残差 `‖Dᵀr‖` 的包围）；`flux_certified` 还应要求所有半径有限。

### 1.3 R1b（P2，证明缺口）：收缩因子用了最大行范数而非谱范数

`:615-618` 用 `contraction = largest_row_norm · design_norm` 判 `1/(1−contraction)`。固定点论证需要 `‖D⁺‖₂`（最大行范数 ≤ ‖D⁺‖₂ ≤ √p·最大行范数）。反例（我构造）：`D=[[1,0],[0,1],[0.9,0.9],[0.9,0.900001]]`，`‖D⁺‖₂=1.000`、最大行范数=0.786；取 `‖δD‖_F=1.0` 时代码 `contraction=0.786<1` 判可认证，而真实 `‖D⁺‖₂‖δD‖=1.000` 已到非收缩边界。实际 PSF 模板区间 ~1e-15，X01 无影响；但对“任意声明区间”不是证明。

## 2. P1-1：暗母版 Jacobian 与角色解析

主路径正确：`x=(d−b−r(dk−b))/f` 的三个导数 `−x/f, −r/f, (r−1)/f 或 −1/f`（`calibration_kernel.cpp:424-473`）我逐项复算无误；`dark_weights = −ratio/f`，`ratio=t_exp/t_dark`。手算（与实现者同输入但独立）：`d=10,b=0,dk=1,f=2,r=2 ⇒ x=4`，`Var = (0.1/2)² + (2/2)²·1² = 1.0025`；错用 flat 导数为 4.0025。修复后的方差公式正确。

**R2（P1，残留猜错风险）**：角色来自 `SharedId.entity` 的自由字符串（`SharedId` 还有未用的 `component/model_revision`）。未知角色走 unknown（安全），但别名是语义猜测：

1. `entity=="gain"` 被当作 flat（`:82`）。数学上 `∂x/∂flat=−x/f` 与 `∂x/∂gain` 只在 gain 数值恰好等于 flat 时相同；真实 `x∝1/g` 给 `∂x/∂g=−x/g`。反例：`flat=1`、真实 `gain=0.5`、gain 根 σ=1 → 代码用 `−x/1`，真实 `−x/0.5=−2x`，**方差小 4 倍（非保守）**。gain 不是 calibrate 公式里的母版，应归 unknown。
2. 母版声明了根但对应数组缺失：`flat` 根而 `has_flat=false` 时代码用 `f=1` 给 `−x`（真实导数 0，因为公式没有除 flat）；`bias` 根而 `has_bias=false` 时权重全 0（`:471`），根仍被 push 且**方差贡献静默为零、无 unknown**。二者都应 unknown/拒绝，而不是零或猜测导数。
3. 角色若写在 `component`（例如 entity 为通用 "master"），会被判 unknown（安全但不可用）；这不是错误，是声明字段与解析字段不一致的规格缺口。

## 3. P1-3：m×m 通量块与背景边缘化

**数学正确**。`measure` 的 `joint_noise` 满足 `covariance_apply(v) = I C Iᵀ v`（`uncertainty/covariance.cpp:89-131`：先 `Iᵀ`、再 `L Lᵀ`、再 `I`），其 codomain 是 8 维 flux 索引（7 源 + 背景末位）。`dense_covariance(context, noise, codomain)` 得到 `I C Iᵀ`，取前 m×m 即 `Cov(F̂)`；`F̂ = I_F d` 的协方差就是该块，**不需要 Schur 补**（背景已在 measure 内被 profile 掉，块内已含其相关）。修前 `domain(16384) != n_meas(56)` 永假、静默对角，确属真缺陷；修后判据 `codomain ∈ {m, m+1}` 正确。

**R3（P1，残留 bug）**：fallback 只重写对角线。若前面的帧成功写入块、后面的帧缺 `joint_noise` 而把 `joint_counts_covariance=false`，`counts_covariance` 里已写入的前几帧**非对角块不会清零**（`joint_fit.cpp:808-820` 只做 `counts_covariance(i,i)=...`；`Matrix` 构造为零初值，故缺失块为 0、已写入块保留）。结果：部分帧用真实相关、部分帧用对角，而 `unknown_terms` 声明“off diagonal terms are unknown, not zero”。修法：fallback 前把矩阵重置为对角（`counts_covariance = Matrix(n,n);` 再填对角）。

**R4（P1，合同违反）**：跨帧块被假定为零，但共享母版根使不同帧的通量相关。合同 `photometry.fit.md:81` 明确“跨 source 共享校准根须作为全局 latent/稀疏耦合保留，不能删去交叉块”。手算：共享 flat 根 σ_f=0.01、F=1000 ADU 两帧，`Cov(F₁,F₂) ≈ F²σ_f² = 100`（相关系数 10%）；当前块对角装配把它当 0，且**没有任何 unknown 记录**。代码注释“块对角跨帧假设成立（不同帧不同原生噪声根）”与 P1-1 刚接线的共享母版直接矛盾。至少应在检测到共享根时记 unknown。

**X01 影响**：修后 fit 走 dense 联合路径（trace 中 `fit_photometry` code=ok，`:1` 次）。但由于 §10 的 XP 系数缺陷，X01 的该路径本身是错配模型，点估计无科学意义；`measure` 的 trace 通量与修前逐位相同（我对比了 8 帧 56 个通量），说明修复未扰动测量点估计。

### 3.4 P1-7 / 3.5 P1-8
- P1-7 仍是“记录而非统一”：`measure.cpp:806-820` 把 `fit_psf` 自由位置与 WCS 位置的差异写进 omitted/unknown。合同要求消费者用同一位置身份；本次只是诚实标注。**仍开**（低危）。
- P1-8 正确：背景列 `z_j=prf_mean·sample_gain`，有 transfer 时 `Z=Gz`（`measure.cpp:350-463`）。X01 的 `sample_gain=null`、PRF 空、identity transfer ⇒ Z=1，与修前数值一致（trace 通量未变即证据）。

## 4. P1-4：PRF/增益进入积分；尾界

- `build_templates_from_local` 现在读 `sample_response_gain` 并乘 `integral.value`（`psf_model.cpp:679-751`），PRF cell 与 `local_cell` 求交后按 cell 增益加权；多波长 bin 返回 `unsupported`。手算：σ 高斯单像元积分 `(erf(0.35))²=0.146631`，gain=2 → 0.293263；与实现者 probe 一致，我独立按公式复核无误。
- **R5（P2，尾界语义）**：`mass += cell_mass` 用的是**未加权**正模型质量，`tail = 1−mass`（`:751`）。只有当 sample gain 在窗内外相等时它才等于通量分数。反例（手算）：窗内 gain=0.5、窗外 gain=2，未加权内质量 0.9、外 0.1 ⇒ 加权内=0.45、外=0.2，窗外通量占加权总通量的 **30.8%**，而 `tail=0.1` 低估 3 倍。若将来 `measure` 用 `native_tail_upper` 定窗，必须乘 `max_gain` 或改用加权尾。
- **接线缺口**：`sample_response_gain` 在 `apps/` 与 CLI 配置里**没有生产者**（只有 ingest/copy 与 geometry 字节预算），`native_tail_upper` **没有消费者**（`psf.cpp:426` 只写不读）。因此在现行 CLI 链上该修复是 dormant，X01 不可能覆盖它。
- 规格歧义：校准后的 x 已除 flat；`sample_response_gain` 若按合同“raw gain/flat”填入，会与 calibrate 的除法重复计一次。生产路径必须先明确它是“残余相对响应”而非 raw flat。

## 5. P1-2：working prior 仍可能被当成已知协方差

`joint_fit.cpp:847-890` 现在把 `I` 命名为 working prior，并把“绝对标度方向不可测量”写进 `notes/unknown_terms`——**文本诚实**。但：

- `api.cpp:663` 构造 `PhotoSolution.covariance` 时，`noise_model->state = NoiseState::known` **无条件**，且协方差矩阵里保留了 working prior 造成的 gauge 方差（7 星时约 `1/n`，即 38%）。任何检查 `covariance->model->state` 的下游都会认为它是已知协方差；矩阵本身在该方向是伪信息。
- 当前**没有** in-tree 消费者读 `solution->covariance`（`source_scale` 只被赋值、从不读取），所以风险是潜在的；但 P1-5 一旦实现就会直接把它当 `L_θ`，到那时就会把 38% 伪方差写进图像噪声。
- 裁决：**P1-2 未关闭**。最小修法：unknown 时把 `covariance` 的 `NoiseModel.state` 设为 `unknown`，或在 gauge 方向做投影/伪逆并显式给 null direction。

## 6. P1-5：降级诚实，但“不可实现”的前提已被同一提交修掉

`apply_scale.cpp:492-551` 追加 `UnknownTerm{theta_random_influence_omitted}`、`noise=unknown`、`science=conditional`、`set_omitted(完整公式)`。**这是诚实的降级**，不会出假证书。

但代码注释与修复报告 §6/U7 仍写“`api.cpp` 的 eigen 因子是 `1/√λ`，返回精度而非协方差，所以不能用”。**该前提在同一提交 `77a13c89c` 已被修成 `√λ`**（§9），`covariance_apply` 现在返回 `C`。因此：P1-5 现在**可以直接实现**（`L_θ = solution->covariance` 的因子，按 SharedId 与父根合并），不再被 U7 阻塞。裁决：**行为诚实、理由过时、项仍开**；建议实现并按合同 `photometry.apply.md:72` 复核同根相加。

## 7. P1-6：单位可区分，但有两处泄漏

- `scaled_unit.scale = 原scale + "*scale/" + application_id`（`apply_scale.cpp:399-401`），`units_equal` 按 scale 字符串比较 ⇒ **确实可区分**；数值端点也改用 `scaled_unit`（`:439-445`）。
- **泄漏 1**：`fits_writer.cpp:393-395` 直接把 `unit.scale` 写进 `BUNIT`（再拼 "/sr"）。修后 BUNIT 会变成 `"<detector-id>*scale/x01-scale"` 这类非标准长串；BUNIT 是外部 FITS 语义字段，不应承载内部 application_id。应由导出层声明标准 `L/sr` 或具名模型单位，把 scale 身份放到 provenance。
- **泄漏 2**：没有地方用 `units_equal` 校验“送入 measure/psf 的观测必须是校准单位”。对已标度的观测再调 `measure` 仍会跑出“标度后的通量”，只有字符串不同，没有拒绝。`unit_adu ≡ unit_signal`（U4）仍未修。
- 裁决：**P1-6 在“可区分”意义上关闭；在“被强制/不污染 FITS”意义上仍开（P2/P1）**。

## 8. P1-10：加权/Jacobian/参数盒

- 加权只在 `observation->noise->state == known` 且**每个** root factor 的 recipe 为 diagonal 时启用（`psf_fit.cpp:317-395`），用 `w_k = 1/Σf²`（精确对角，无近似）；残差、Jacobian 行、两处 trial 目标同权（`:436,742,792`）。条件不满足时单位工作权并记 unknown。X01 的 Poisson plug-in 状态是 `approximate` ⇒ 仍走单位权，**行为不变**（与 trace 一致）。
- 参数盒仍是 `|step|+1e-12·max(1,|θ|)`（`:886`），现在用 error record/omitted 明确“不是 score 根包围、不含统计形状误差”。**诚实但仍是规格缺口**（合同要求真实 score/Hessian + 区间 AD）。
- Jacobian 仍是中心差分（位置 1e-3、形状 1e-4），已把无截断界写进 omitted。**仍开（P2）**。

## 9. api.cpp 与 linear_action 的两处修正

**9.1 `1/√λ → √λ`（`api.cpp:626-640`）**：`factor = Σ √λ v vᵀ = L`；`CovarianceAction` 的 root factor 就是 `L`，`covariance_apply` 计算 `L Lᵀ v`（`covariance.cpp:110-127`），故现在返回 `C v`。修前返回 `C⁻¹ v`。**正确**。点估计不使用 `PhotoSolution.covariance`（`source_scale` 未被读取），所以**无点估计变化**。

**9.2 compose 转置（`linear_action.cpp:326-338`）**：`apply(x)=children[0](children[1](x))=A B x`（`:134-143`），故转置应为 `Bᵀ Aᵀ`；修后先 `children[0]ᵀ` 再 `children[1]ᵀ`。手算反例：`A=diag(2,1)`、`B=[[1,1],[0,1]]`、`v=(1,1)`：正确 `BᵀAᵀv=(2,3)`，旧代码 `AᵀBᵀv=(2,2)`。**正确**。`transpose_apply` 只被 `covariance_apply` 与 `uncertainty::geometry_covariance_bound`（`uncertainty/api.cpp:574-583`）使用，前向 `apply` 未改 ⇒ **无点估计变化**。
- X01 的噪声链是对角∘对角（可交换），故 X01 的 flux 方差不受该修正影响；该修正对 **regroup/α/R 等非交换链**才产生数值变化（修前那些链的协方差/几何 functional 界是错的）。

## 10. X01 证据状态与仍需修的夹具

1. **修复后的 X01 e2e 没有跑完**：`validation/x01/observed/normalize_trace.jsonl` 的最后一次运行在 `x01-02` 的 `compute_overlap` 中途结束（`block_size=1` ⇒ 每帧 16384 个 source row）；`e2e_summary.json` 仍是 `376850340` 的旧运行（normalize exit 6，`TaskSpec.work_upper exceeds ...`，published=1）。`compare.json` 只有 `x01-00`，且 `blind_solve=false`（B 组未测）。**因此目前没有修后 B 组的 e2e 证据**；`48eaf0612` 只是把 work 预算调到实测值。
2. **measure 点估计无回归**：trace 中 8 帧 56 个 `measured_stars` 通量与修前逐位相同（如 x01-06/1003 = 3374.88，x01-04/1003 = 3344.22），说明 P0-1/P1-4/P1-8/P1-10 的改动没有改变 X01 的点估计。B 组归一化残差沿用我上一轮的独立结果（x01-06/1003 z=−1.77，48 项 max|z|=1.77），但因 e2e 未完成，不能据此宣称 `source_recovered_normalized`。
3. **dark 夹具仍需修**：`calibration.json` 的 `dark[0]=4.8 ADU`，而 `truth_x01.py:102-107` 注入 `12 e⁻/s × 100 s = 1200 e⁻ = 480 ADU`。差值 475.2 ADU 被 `measure` 的 β 吸收，所以通量不变，但：该案**从未检验 N0 的 dark 单位/曝光比路径**；若将来把它当“dark 已验证”的证据，就是假证据。修法：dark 母版改为 480 ADU（并同步 `dark_exposure_seconds`），或在案文档里明确该案 dark 为零、另建 N0 专项。
4. **XP 系数夹具仍需修（阻断 P1-2 的 gauge 验证）**：`cli_inputs.py:91` 的 `xp_coefficients` 全是 `[1,0,0]`，而注入的星等按 `weight∈[0.7,1.0]` 与亮源的发射线变化。模型 `C=a_k exp(bᵀθ)vᵀc_s` 在 X01 的 constant basis 下对所有星预测同一通量，与数据差 15%–30%，故 **X01 的 `photometry.fit` 是按错配模型拟合**（trace 里它仍 `code=ok`，这正是“收敛不等于模型正确”）。在修好星表（`c_s ∝ weight` 或引入具名逐星幅度字段）前，X01 不能验证绝对标度/gauge；实现者用它证明“精确 gauge 会破坏 x01_minimal_repro”是把**夹具缺陷**当成了实现约束。

## 11. 需要 Lead 实跑的实验

```bash
cd "/workspace/Astro CS Database"
# 1) 修后完整 X01（当前 trace 停在 geometry）：先确认 block 性能路线，再跑
cmake --build build/science -j16 --target acsd
python3 validation/x01/e2e_x01.py            # 期望退出 0 且三命令 published=8/1/1
grep -c product_published validation/x01/observed/e2e_events.jsonl   # P1-11 修后应 >0

# 2) R1 定点：在 measure.cpp 临时打印 data_norm/design_norm/residual_norm/radius，
#    对 X01 八帧比较 radius 与 F；期望 ratio≈0.5–1.9（证实统计残差主导）
# 3) R3 定点：让第 1 帧缺 joint_noise（rank 未认证），第 0 帧有；
#    检查 counts_covariance 的非对角块是否残留（修前后同一输入对照）
# 4) R4 定点：给 calibration.master_noise 一个 entity="master_flat" 的共享根，
#    两帧同源；检查 photometry 是否记录跨帧相关 unknown（当前不记录）
# 5) P1-2 定点：给 X01 星表真实 XP 协方差 vs 无协方差，读 PhotoSolution.covariance
#    的 NoiseModel.state（当前恒为 known）与 gauge 方差（~1/7）
# 6) P1-5 定点：用修后 √λ 的 covariance 实现 L_θ，比较 apply_scale 的图像方差
```

## 12. 复核边界（未由我重跑）

- 未重跑 `validation/normalize_probe.cpp`（按 Lead 要求不复用实现者断言）；本文 C2/方差/尾界均为独立手算与 Python 复算。
- 未重跑 `acsd_process_*` 系列与 `acsd_interval_oracle`；P0-1 依赖的 `jacobi_svd/pseudo_inverse` 证书仍取 B1 结论。
- X01 的 e2e 未由我跑（工作树有其他 agent 的在途修改；trace 已证明其在 geometry 中途停止）。
- `astrometry`/catalogue 索引内部仍未逐行复核（沿用上一轮 U3）。
