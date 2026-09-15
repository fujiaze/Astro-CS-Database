# ALG-P2-SURF-COVARIANCE-EPSF — covariance 传播与 effective PSF 规格

- 文档 ID：`ALG-P2-SURF-COVARIANCE-EPSF`（`ALG-P2-SURF-001` 子文档）
- 上位：`SCI-P2-001-COVARIANCE-EPSF §0/§2/§3/§4`；`UNIFIED §6/§7`；`DESIGN-P2 §6.1/§9`；`DESIGN-P3 §4`；`SCI-PSFW-001 §5`；`RULINGS #5`
- 裁决锚：`FZ-FORMULA-COV-PROP`；`FZ-GATE-PSFSW-COV`；`FZ-GATE-PSFSW-EPSF`；`ADJ-P2-03`；`ADJ-AR-02`；`ADJ-OBS-01`
- 本任务条款：`ALG-P2S-COV.1..6`、`ALG-P2S-EPSF.1..5`
- 文献锚：Fruchter & Hook 2002 PASP 114,144；Zackay & Ofek 2017 II arXiv:1512.06879；Horne 1986 DOI:10.1086/131801

## 1 covariance 传播（`ALG-P2S-COV.1`）

三模式共用一个不变量（`SCI-P2-001-COVARIANCE-EPSF §0`）：

```text
C_out = R C_in R^T                                   （矩阵形式）
Var(out) = c^T C_in c = sum_{i,j} c_i c_j [C_in]_ij  （单输出标量形式）
```

- `R`（或向量 `c`）= **实际组合算子**：含 UPM 光度尺度 `g_k`、重采样/插值核 `K_k`、rejection/validity 掩码、归一化与逐帧权重；
- `C_in` = 随机项 + 可表示共享项（低秩 `L L^T`/相关核/master ID+强度参数）；
- **禁止**用理想 GLS 公式反推，**禁止**由权重标量反推 variance（`FZ-FORMULA-COV-PROP`；`RULINGS #5`；`SCI-PSFW-001 §5`）。

### 1.1 surface_gls 的实际系数（`ALG-P2S-COV.2`）

```text
R = (A^T C_in^-1 A)^-1 A^T C_in^-1               （GLS 精确）
R~ = 实现的近似组合算子（如像素 ivar）→ 必须报告 R~ C_in R~^T 与比值 rho（见 pixel-ivar 门）
```

- 恒等式 `R C_in R^T == (A^T C_in^-1 A)^-1`（容差 `FZ-AP2S-IDENT-RTOL=1e-9`）既是解析结果也是独立 Oracle 不变量；
- 只落盘对角 variance 时，必须另存相关核 `rho_ij = [C_out]_ij / sqrt([C_out]_ii [C_out]_jj)` 或可重建算子摘要（`ADJ-AR-02`，`SC-ADJ-AR02.1..2`）。

### 1.2 UPM 与共享系统项（`ALG-P2S-COV.3`）

```text
C_out = C_stat + J C_theta J^T + C_shared
```

- `J C_theta J^T`：UPM 参数不确定度（`ALG-P2-SURF-UPM.md §5`）；
- `C_shared`：低秩/相关核/共同 master 表示（`ADJ-OBS-01`；`ADJ-F-OBS-04`），无法表示 → `unavailable` 或系统误差预算；
- 含/不含 UPM 项的方差比 > 1 必须可检出（Oracle C9）。

### 1.3 psfsw_robust 的严格边界（`ALG-P2S-COV.4`）

```text
alpha_k(p) = W_psfsw,k * v_k(p) / sum_j W_psfsw,j * v_j(p)
Var(I_out(p)) = sum_{k,l} alpha_k(p) alpha_l(p) [C_in]_kl(p)
```

- `alpha_k` 必须是**实际**系数（含组内归一 `W_psfsw`、validity 门 `v_k`、UPM 尺度）；
- `FZ-GATE-PSFSW-COV`：`covariance.method=propagated_from_composite_coefficients`、`variance_from_weight=false`、`uses_relative_weight_as_ivar=false`；
- **禁止** `Var = 1/W_psfsw`、`Var = sum_k W_psfsw,k` 或把任何诊断量当方差面 → REJECT（`RULINGS #5`；`ADJ-P2-03`）。

### 1.4 可复算的方差来源声明（`ALG-P2S-COV.5`）

`covariance.method`、非空 `combination_coefficients`、`variance_from ∈ {combination_coefficients, linear_combination_coefficients, actual_combination_coefficients}`。把 `variance_from` 写成 `weight/psfsw_robust_weight/median_source_snr/support/coverage/fwhm/psf_residual/effective_psf/source_snr/rejection_probability` 一律 REJECT（`WEIGHT_PROVENANCE_GATE R6`）。

## 2 effective PSF 定义（`ALG-P2S-EPSF.1`）

**操作式定义**（可注入验证）：effective PSF 是**实际组合算子**对单位通量点源的脉冲响应，在输出位置 `x_o` 按声明约定归一（`SCI-P2-001-COVARIANCE-EPSF §3`）：

```text
P_eff(x ; x_o) = [ sum_k alpha_k(x_o) * a_k * (P_k (x) K_k)(x - x_o) ]
               / [ sum_k alpha_k(x_o) * a_k * (P_k (x) K_k)(0) ]        （peak 归一）
```

- `alpha_k` = 实际组合系数（含 UPM 尺度/validity/归一）；`K_k` = 重采样/插值核；`(x)` = 卷积。

### 2.1 归一约定（`ALG-P2S-EPSF.2`）

- **积分归一**：面亮度产品要求 `sum_x P_eff = 1`（PSF 归一 `sum P = 1`，`DESIGN-P1 §6`）；
- **peak 归一**：点源/detection statistic 用。
- **归一约定必须按产品族显式声明**，否则 FWHM/EE 有歧义（`SCI-P2-001 §6.5`）。

### 2.2 conventional coadd 与 matched-filter（`ALG-P2S-EPSF.3`）

```text
conventional coadd (surface_gls / psfsw_robust):
  P_eff = sum_k alpha_k a_k (P_k (x) K_k) / sum_k alpha_k a_k

matched-filter / proper coadd (point_information 输出图像面):
  q_k = C_k^-1 P_k;  P_eff  ∝  sum_k (q_k (x) P_k)
  白噪声 q_k = P_k/sigma_k^2  =>  P_eff ∝ sum_k (P_k (x) P_k)/sigma_k^2
```

锚：Horne 1986（matched filter）；Zackay & Ofek 2017 I/II（逐帧匹配滤波后组合、proper coadd 的 PSF 即其自卷积）。

### 2.3 测量纪律（`ALG-P2S-EPSF.4`）

- **FWHM/encircled energy 必须从 `P_eff` 测量**，不得由逐帧 FWHM 摘要（median/平均）代替：`median(FWHM_k) != FWHM(P_eff)`（Oracle C6）；同一帧组在 equal 与 pixel-ivar 权重下 `FWHM(P_eff)` 不同；
- **可复现性**：`P_eff` 必须与 `K_k`（或 transfer 描述）和归一约定一起保存；**只给 FWHM 标量不构成 effective PSF**（门规则 R7 直接 REJECT；`FZ-GATE-PSFSW-EPSF`）。

### 2.4 基线比较报告（`ALG-P2S-EPSF.5`）

必须报告相对 `equal`/`pixel_ivar`/`W_info` 基线的 detection power、`FWHM(P_eff)`、通量偏差、面亮度偏差（`SCI-PSFW-001 §5`；`DESIGN-P2 §6.3`）。禁止只以"看起来更好"通过。

## 3 fail-closed（`ALG-P2S-COV.6`）

| 条件 | 行为 |
|---|---|
| `variance_from` 为权重/诊断量 | REJECT |
| 只出对角 variance 无相关核/算子摘要 | REJECT |
| psfsw 产物 `variance=1/W_psfsw` | REJECT |
| effective PSF 缺失或只有 FWHM 标量 | REJECT |
| `C_in` 含未表示共享项 | `unavailable`/系统误差预算 |
| UPM 项未传播 | REJECT |
| `unavailable` 无原因 | REJECT（`FZ-PROV-MINIMAL-SET`） |

## 4 provenance（`ALG-P2S-COV.6` 续）

`covariance.method`、`combination_coefficients` 摘要、`variance_from`、`correlation_kernel`（或算子摘要）、`input_covariance`（含共享项表示）、`effective_psf_id`、`normalization`、`K_k` 版本、`J C_theta J^T` 项存在标志、`flux_conservation_factor`、`unavailable_reason`（`FZ-PROV-MINIMAL-SET`；`ADJ-GEN-03`）。

## 5 验证门（Oracle）

1. `G C G^T == (A^T C^-1 A)^-1`（`<1e-9`）；
2. GLS 协方差 vs 蒙特卡洛 `rel < 3%`（`FZ-AP2S-MC-RELTOL`）；
3. psfsw 实际系数方差 vs 权重代理显著不同且与 MC 一致；
4. 注入单位点源 → 实际系数组合 == 解析 `P_eff`（`<1e-12`，`FZ-AP2S-EPSF-RTOL`）；
5. `median(FWHM_k) != FWHM(P_eff)` 且 `FWHM(P_eff)` 依赖权重；
6. 相关噪声下简单求和过度乐观被检出；
7. 每条 fail-closed 有 mutation 能红。
实测 rc 见 `ALG-P2-SURF-VERIFICATION.md`。
