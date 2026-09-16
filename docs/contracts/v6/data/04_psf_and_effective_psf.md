> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# 04 — PSF 与 effective PSF 对象 schema

上位锚：`FZ-GATE-PSFSW-EPSF`、`FZ-GATE-MEDIAN-SNR`、`FZ-GATE-SUPPORT-COVERAGE`、`FZ-COND-WHITENOISE`、`FZ-P3-FAILCLOSED`；
DESIGN-P1 §6/§8.3；SCI-P2-001 `COVARIANCE_AND_EFFECTIVE_PSF.md` §3；PHASE3_REVIEW C-P3-PROP-11/12/13；Horne 1986；Naylor 1998；Zackay & Ofek 2017。
机器：`astrocs.v6.psf.v1.schema.json`（`psf.v1`）、`astrocs.v6.effective-psf.v1.schema.json`（`effective-psf.v1`）。

## 1. 输入 PSF 模型（`psf.v1`）

| 字段 | 类型 | 单位 | 适用域 | fail-closed | 条款锚 |
|---|---|---|---|---|---|
| `psf_id` / `psf_family` / `model_id` | string | — | 全部 | 缺失 → 不可引用 | DESIGN-P1 §6 |
| `parameters[]` | {name,value,unit} | 各自 | 参数化族 | 无参数且无 empirical_grid → REJECT | DESIGN-P1 §6 |
| `normalization_sum_P` | const 1.0 | — | 全部 | ΣP≠1（容差由 ALG 冻结）→ point_source_flux REJECT | `FZ-COND-WHITENOISE`；`FZ-P3-FAILCLOSED` |
| `normalization_convention` | enum integral/peak | — | 全部 | 未声明 → FWHM/EE 歧义 REJECT | SCI-P2-001 §3 |
| `spatial_representation` | enum scalar/model/map/control_points | — | 空间变化 | scalar 须过均匀性门+分位数 | `FZ-DEGRADE-SCALAR` |
| `effective_domain` | string | — | 全部 | 超域求值 → REJECT | DESIGN-P1 §6 |
| `fwhm` | {value,unit=px,role=diagnostic} | px | **仅诊断** | 单独作权重/epsf → REJECT | `FZ-GATE-MEDIAN-SNR` |
| `fit_residual` | {value,role=diagnostic} | — | **仅诊断** | 未过概率模型直接乘入权重 → REJECT | DESIGN-P1 §6；PSF_SIGNAL_WEIGHT §8 |
| `a_nea` | {value,unit=px^2,context=white_noise_only} | px² | 白噪声近似 | C 非对角/σ_pix 未声明时不得用 | `FZ-COND-WHITENOISE` |
| `information_kernel` | {kernel_id,representation} | — | 一般信息核 PᵀC⁻¹P | 声明却无表示 → REJECT | DESIGN-P1 §6 |
| `parameter_covariance` | {representation,ref} | — | 系统预算 | PSF 模型误差伪装随机 ivar → REJECT | ADJ-OBS-01 |

**禁止**：`FWHM`、`fit_residual` 单独或组合冒充 PSF signal weight / ivar / 科学权重（`FZ-GATE-MEDIAN-SNR`；PSF_SIGNAL_WEIGHT §8）。
PSF 拟合质量只作 validity/诊断，不能未经概率模型直接乘入权重（DESIGN-P1 §6 末段）。

## 2. effective PSF（`effective-psf.v1`）——三生产模式**必输**

操作式定义（SCI-P2-001 §3）：

```text
P_eff(x;x_o) = [ Sum_k alpha_k(x_o) * a_k * (P_k (x) K_k)(x - x_o) ]
             / [ Sum_k alpha_k(x_o) * a_k * (P_k (x) K_k)(0) ]        (peak 归一)
conventional coadd : P_eff = Sum_k alpha_k a_k (P_k (x) K_k) / Sum_k alpha_k a_k
matched-filter     : q_k = C_k^-1 P_k;  P_eff ∝ Sum_k (q_k (x) P_k)
```

| 字段 | 类型 | 适用域 | fail-closed | 条款锚 |
|---|---|---|---|---|
| `effective_psf_id` | string 非空 | 全部 | 空 → REJECT | `FZ-GATE-PSFSW-EPSF` |
| `definition` | const `impulse_response_of_combination` | 全部 | 缺失/非操作式定义 → REJECT | `FZ-GATE-PSFSW-EPSF` |
| `product_family` | enum | 全部 | 未声明 → 归一约定歧义 REJECT | SCI-P2-001 §3 |
| `formula_family` | enum conventional / matched_filter_proper | 全部 | — | Zackay & Ofek 2017 |
| `normalization` | enum peak/integral | 全部 | 未显式声明 → REJECT | `FZ-GATE-PSFSW-EPSF` |
| `values_or_model` | {kind,values|model} | 全部 | 两者皆空或只给 fwhm 标量 → REJECT | 门 R7 |
| `kernel_transfer[]` | {frame_id,kernel_id,kernel_version} | 全部 | 缺 K_k/transfer → 不可复现 REJECT | DESIGN-P3 §3 |
| `combination_coefficients_ref` | string | 全部 | 缺实际组合系数 → REJECT | SCI-P2-001 §3 |
| `fwhm_from_effective` | {value,role=derived_diagnostic} | 诊断 | 用逐帧 FWHM 摘要代替 P_eff → REJECT | SCI-P2-001 §3 |
| `reproducibility` | {p_eff_hash,kernel_set_hash} | 全部 | 缺失 → REJECT | 宪章 §4.3 |

**FWHM/encircled energy 必须从 `P_eff` 测量**，不得由逐帧 FWHM 摘要（median/平均）代替：
同帧组在 equal 与 pixel-ivar 权重下 `FWHM(P_eff)` 不同（Oracle C6：equal 3.805 / pixel_ivar 5.460 / W_info 5.248 / MF 7.694 px）。

## 3. 适用域与失效域

- 归一约定**必须按产品族显式声明**（面亮度产品要求 `Sum P_eff=1`；peak 归一用于点源/detection statistic），否则 FWHM/EE 有歧义。
- Phase3：`P_eff` 由同一采样算子传播 `pi_i = (S p)_i`；缺 PSF 或 `ΣP≠1` 时 `point_source_flux` 必须拒绝（C-P3-PROP-13）。
- 只给 FWHM 标量不构成 effective PSF（`FZ-GATE-PSFSW-EPSF`；门 R7）。

## 4. fail-closed 与负向门

- 任一生产模式缺 effective PSF → REJECT（`G-EPSF-PRESENT`）；
- effective PSF 无归一约定/无 kernel transfer/无组合系数引用 → REJECT（`G-EPSF-NORMALIZATION`/`G-EPSF-REPRODUCIBLE`/`G-EPSF-COEFFS`）；
- 用 `median(FWHM_k)` 代替 `FWHM(P_eff)` → REJECT（`G-EPSF-FWHM-DERIVED`）；
- 把 FWHM 或拟合残差当权重来源 → REJECT（`G-DIAGNOSTIC-NOT-WEIGHT`）。
