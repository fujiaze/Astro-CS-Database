> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# ALG-P2-POINT-001 — Phase2 点源算法规格

- 文档 ID：`ALG-P2-POINT-001`
- 状态：`TARGET_NORMATIVE`（算法层目标规范；正式冻结由 W4 `CONTRACT-FREEZE-001` 写入 `docs/algorithms/v6/frozen/`）
- 任务：`工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/ALG-P2-POINT-001.md`（wave 3，depends_on SCI-ADJ-001）
- 基线：`HEAD = main = 125bc0999363be1a42a1f2df3254601e0cc7b8fb`（本机 `git rev-parse HEAD` 实测）
- 写域：`docs/algorithms/v6/phase2-point/`（人读规格）；机器规格/Oracle/证据/日志在 `run/v6/alg-p2-point/`（gitignore 工作区）
- 性质：**算法层规格**。不改科学公式/容差/冻结门，不写生产源码，不 commit/push，不派生子任务。
- 机器伴生：`run/v6/alg-p2-point/spec/phase2_point_spec.json`（本条每条 clause 的机器形态，含 units/formulas/gates/mutations）

> 覆盖：Phase2 `point_information` 的 **Q/W 合并**、**proper/effective PSF**、**点源检测/测光**、
> **输出帧重算**、**independent-frame `SNR_combined² = Σ_k SNR_k²` 验证与相关帧拒绝**。
> 不覆盖：`surface_gls`（ALG-P2-SURF-001）、`psfsw_robust` 复合权重细节（ALG-P2-PSFSW-001）、
> Phase1 `W_info` 生产（ALG-P1-001）、Phase3 传播（ALG-P3-001）、schema/字段词表（DATA-DESIGN-001 / SCHEMA-INTEGRATE-001）。

---

## 0. 权威、口径与边界

- 权威分层（宪章 §1.1）：`ASTROCS-CONSTITUTION-001` > `ASTROCS-PROJECT-SPEC-002` >
  `DESIGN-P2-001` > `ASTROCS-SCIENCE-MODEL-001` / `SCI-PSFW-001` > 本 ALG > 实现/测试。
- 科学冻结源：`docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md`、
  `SCI-ADJ-001_CONFLICT_MATRIX.md`、`reports/v6/science-adjudication/adjudications.json`；
  控制器裁决 `CONTROLLER_LOG.md` C-004.1/C-004.2/C-004.3 与 `RULINGS.md` #3/#5。
- 文献锚：Horne 1986 PASP 98,609（DOI:10.1086/131801，已知轮廓/方差下最优提取）；
  Naylor 1998 MNRAS 296,339（成像最优 PSF 光度）；Zackay & Ofek 2017 ApJ 836,187
  （arXiv:1512.06872，逐帧 matched filter 组合）与 ApJ 836,188（arXiv:1512.06879，proper coadd）；
  Fruchter & Hook 2002 PASP 114,144（Drizzle 线性重建与相关噪声）。
- **F1 基线分歧（控制器级，只登记不裁决）**：工作树相对 HEAD 有 tracked 回退/删除；本规格一切生产面判定以
  **HEAD=125bc099** 为准，Oracle 不调用生产实现。
- **AR-033 根构建面 owner（控制器级，只登记不裁决）**：本任务不注册任何 CI/构建面测试。

---

## 1. 符号、单位与约定

| 符号 | 含义 | 单位 | 冻结锚 |
|---|---|---|---|
| `a_k` | 帧 k 光度响应尺度（归一到公共通量尺度） | 1 | ADJ-P2-01 |
| `P_k` | 帧 k 归一 PSF 轮廓，`Σ_p P_k,p = 1` | 1 | ADJ-P2-01 |
| `C_k` | 帧 k 输入 covariance（随机项 + 可表示相关项） | ADU² | UNIFIED §6/§7 |
| `d_k` | 帧 k 数据向量 | ADU | UNIFIED §2 |
| `Q_k` | 帧 k 线性充分统计量 | ADU⁻¹ | FZ-UNIT-Q |
| `W_info,k` | 帧 k 点源信息权重 | ADU⁻² | FZ-UNIT-WINFO |
| `F_hat` | 点源通量估计 | ADU | FZ-UNIT-FLUX |
| `Var(F_hat)` | 通量估计方差 | ADU² | FZ-UNIT-FLUX |
| `S(x)` | 检测统计量（匹配滤波显著性） | 1 | UNIFIED §4/§10 |
| `P_eff` / `P_proper` | effective PSF / proper coadd PSF | 1 | COVARIANCE_AND_EFFECTIVE_PSF §3 |
| `signal_sb` | 上游 Phase1 面亮度（仅登记） | ADU/px² | FZ-UNIT-SIGNAL-SB |

> **量纲推导注**：`d_k` 与 `C_k` 的量纲由冻结式 `Q_k = a_k P_kᵀC_k⁻¹d_k`（FZ-FORMULA-Q）与 `Q=ADU⁻¹`（FZ-UNIT-Q）、`W_info=ADU⁻²`（FZ-UNIT-WINFO）、`flux=ADU`（FZ-UNIT-FLUX）唯一确定，即 `d_k=ADU`、`C_k=ADU²`（输入逐像素方差，FZ-UNIT-VAR-IN）；`a_k` 吸收像素面积/光度尺度换算，故无量纲。机器门 `GATE-UNIT-01` 按此推导做代数校验，不由实现自行发明单位。

约定：

- **ALG-P2PT-SYM-01**（锚：PROJECT_SPEC §3、DESIGN-P2-001 §6.2、UNIFIED §2/§4、ADJ-P2-01）：
  点源观测模型 `d_k = a_k F P_k + n_k, Cov(n_k)=C_k`；`P_k` 为归一 PSF 轮廓 `Σ_p P_k,p = 1`；目标为单点源固定位置与形状。
- **ALG-P2PT-SYM-02**（锚：FZ-UNIT-Q/WINFO/FLUX、ADJ-GEN-01）：单位冻结
  `Q=ADU⁻¹`、`W_info=ADU⁻²`、`flux=ADU`、`Var(F)=ADU²`、`SNR=1`；
  上游 `signal_sb=ADU/px²`、`pixel_variance_in=ADU²`；二次律 `variance=signal²`、`ivar=1/variance`。
- **ALG-P2PT-SYM-03**（锚：DESIGN-P2-001 §9、COVARIANCE_AND_EFFECTIVE_PSF §3、FZ-BUNIT-SEMANTICS）：
  PSF 归一约定与产品语义显式声明；点源/检测用 peak 归一，面亮度用积分归一 `ΣP=1`；两约定不得互换而不声明。

### 1.1 输入与输出

- **ALG-P2PT-IO-01**（锚：PROJECT_SPEC §5、DESIGN-P2-001 §1/§4/§6.2、UNIFIED §9）：
  **输入**：逐帧原始量 `a_k`（光度响应尺度）、`P_k`（归一 PSF 空间模型）、`C_k`（随机+可表示相关项的 covariance）、
  `d_k`（帧数据）、validity/coverage 门、UPM 参数与其 covariance、rejection 状态；输入必须合同兼容且单位可判，不可判即拒绝。
- **ALG-P2PT-IO-02**（锚：DESIGN-P2-001 §9、PROJECT_SPEC §5、UNIFIED §9）：
  **输出**（point_source 产品族）：`Q`、`W`、`F_hat=Q/W`、detection statistic `S`、逐帧 `Q_k/W_info,k/SNR_k`、
  proper/effective PSF、variance（`1/W`）、相关核/算子摘要、support/coverage/validity/rejection、UPM、provenance 与 manifest；
  不得只输出一张 signal 与语义不明 weight。

> 单位自洽由机器门 `GATE-UNIT-01` 按声明量纲指数做代数验证（非子串断言），见 §11。

---

## 2. 逐帧充分统计量 Q/W

~~~text
Q_k      = a_k * P_k^T C_k^-1 d_k                      # ADU^-1
W_info,k = a_k^2 * P_k^T C_k^-1 P_k = 1/Var(F_hat_k)   # ADU^-2
~~~

- **ALG-P2PT-QW-01**（锚：FZ-FORMULA-Q、UNIFIED §4、DESIGN-P2-001 §6.2、ADJ-P2-01）：
  逐帧线性充分统计量 `Q_k = a_k P_k^T C_k^-1 d_k`，单位 ADU⁻¹。
- **ALG-P2PT-QW-02**（锚：FZ-FORMULA-WINFO、SCI-PSFW-001 §2、ADJ-P2-01）：
  逐帧点源信息权重 `W_info,k = a_k^2 P_k^T C_k^-1 P_k = 1/Var(F_hat_k)`，单位 ADU⁻²。
- **ALG-P2PT-QW-06**（锚：ADJ-P2-01、COVARIANCE_AND_EFFECTIVE_PSF §1、Horne 1986 DOI:10.1086/131801）：
  `Q/W` 必须数值恒等于单参数 GLS 解 `(A^T C^-1 A)^-1 A^T C^-1 d`，`Var=1/W` 等于 `(A^T C^-1 A)^-1`（C 正确时）；偏差超容差即失败。
- **ALG-P2PT-QW-07**（锚：ADJ-P2-01、Naylor 1998 MNRAS 296,339、Zackay & Ofek 2017 ApJ 836,187）：
  最优性声明前提全部满足才可声称 BLUE / 最大点源 SNR / 最小通量方差：模型正确、`P` 归一、`C` 正确且可表示、高斯或 CRLB 意义、目标为点源。

---

## 3. 合并与相关帧

~~~text
独立帧：  Q = Σ_k Q_k ;  W = Σ_k W_info,k
相关帧：  Q = A^T C_in^-1 d ;  W = A^T C_in^-1 A ;  A = stack_k(a_k P_k)
~~~

- **ALG-P2PT-QW-03**（锚：FZ-FORMULA-FHAT、UNIFIED §4、DESIGN-P2-001 §6.2）：
  独立帧合并 `Q = Σ_k Q_k`、`W = Σ_k W_info,k`、`F_hat = Q/W`、`Var(F_hat)=1/W`。
  合并只允许 `Q` 与 `W` 两个科学量；任何帧级额外标量（support/coverage/snr/median SNR 等）若无科学来源即 REJECT。
- **ALG-P2PT-QW-04**（锚：UNIFIED §4/§6/§10、ADJ-P2-01、COVARIANCE_AND_EFFECTIVE_PSF §1）：
  跨帧相关（共享 master、共同天光、UPM 参数、重叠重采样）必须用联合 `C_in` 的 GLS `W = A^T C_in^-1 A`；简单 `Σ_k` 求和必须被拒。
- **ALG-P2PT-QW-05**（锚：FZ-COND-WHITENOISE、SCI-PSFW-001 §2:30-33、ADJ-P2-01）：
  白噪声近似 `W_info,k = a_k^2/(sigma_pix,k^2 A_NEA,k)`、`A_NEA,k = 1/Σ_p P_k,p^2` 为**条件式**，仅在 `C_k` 对角且 `sigma_pix` 显式声明时可用。

---

## 4. 通量、方差、SNR 与点源检测/测光

~~~text
F_hat      = Q / W
Var(F_hat) = 1 / W
sigma_F    = 1 / sqrt(W)
S(x)       = Q(x) / sqrt(W(x)) = F_hat(x) * sqrt(W(x))
SNR_k      = F_ref * sqrt(W_info,k)          # 固定参考通量 F_ref
depth:  F_5sigma = 5 * sigma_F = 5 / sqrt(W)
~~~

- **ALG-P2PT-DET-01**（锚：FZ-FORMULA-FHAT、UNIFIED §4、DESIGN-P2-001 §6.2）：
  测光估计 `F_hat = Q/W`（ADU）；`Var(F_hat)=1/W`（ADU²）；`sigma_F=1/sqrt(W)`。
- **ALG-P2PT-DET-02**（锚：UNIFIED §4/§10、Zackay & Ofek 2017 II、DESIGN-P2-001 §6.2）：
  检测统计量 `S(x) = Q(x)/sqrt(W(x)) = F_hat(x) sqrt(W(x))`，为匹配滤波显著性；阈值 z（如 5σ）判据 `S≥z`。
- **ALG-P2PT-DET-03**（锚：UNIFIED §4/§10、PROJECT_SPEC §8、DESIGN-P2-001 §10）：
  固定参考通量 `F_ref` 时 `SNR_k = F_ref sqrt(W_info,k)`；`SNR_combined² = Σ_k SNR_k²` 仅在帧独立且模型正确时成立。
- **ALG-P2PT-DET-04**（锚：PROJECT_SPEC §4、UNIFIED §3）：
  5σ depth：`F_5sigma = 5 sigma_F = 5/sqrt(W)`，在固定参考 PSF/孔径下报告 depth m5；depth 为摘要，不得作权重。
- **ALG-P2PT-DET-05**（锚：DESIGN-P2-001 §9、PROJECT_SPEC §5、UNIFIED §9）：
  输出族至少含 detection score map `S`、point-source information map `W`、flux map `Q/W`、proper/effective PSF，
  以及逐帧 `Q_k/W_info,k/SNR_k`；不得只写一张 signal 与语义不明 weight。
- **ALG-P2PT-DET-06**（锚：DESIGN-P2-001 §6.2、UNIFIED §11、Zackay & Ofek 2017 I）：
  不得宣称普通像素 ivar coadd 与点源 PSF-aware 最优等价；检测模板必须与实际测光使用的 `P_k` 一致。

> **诊断边界**：`median(SNR_F)` / `median_source_snr` / support / coverage / FWHM / residual 只能是诊断或门，
> 不得进入 `weight.sources` / `weight_value` / `covariance.variance_from`（FZ-GATE-MEDIAN-SNR、FZ-GATE-SUPPORT-COVERAGE、C-004.2）。

机器可读记录示例（与 `run/v6/alg-p2-point/spec/phase2_point_spec.json` 的 `weight_record` 一致；`weight.sources` 只能列科学原始量）：

~~~json
{
  "mode": "point_information",
  "weight": {"kind": "W_info", "sources": ["psf", "photometric_response", "noise_covariance"], "units": "ADU^-2", "group_normalized": false},
  "covariance": {"propagation": "C_out = R C_in R^T", "variance_from": "combination_coefficients", "combination_coefficients": [0.6, 0.4], "input_covariance": "provided"},
  "effective_psf": {"definition": "impulse_response_of_combination", "normalization": "peak", "values": [0.05, 0.2, 0.5, 0.2, 0.05]}
}
~~~

---

## 5. proper / effective PSF

~~~text
proper coadd   :  P_proper ∝ Σ_k (q_k ⊗ P_k) ,  q_k = C_k^-1 P_k
                  白噪声: q_k = P_k/sigma_k^2  ⇒  P_proper ∝ Σ_k (P_k ⊗ P_k)/sigma_k^2
effective PSF  :  P_eff = [ Σ_k alpha_k a_k (P_k ⊗ K_k) ] / [ Σ_k alpha_k a_k ]
~~~

- **ALG-P2PT-EPSF-01**（锚：COVARIANCE_AND_EFFECTIVE_PSF §3、Zackay & Ofek 2017 ApJ 836,188、Horne 1986）：
  proper coadd PSF：`P_proper ∝ Σ_k (q_k ⊗ P_k)` 且 `q_k = C_k^-1 P_k`；白噪声退化为 `Σ_k (P_k ⊗ P_k)/sigma_k^2`（匹配滤波自卷积）。
- **ALG-P2PT-EPSF-02**（锚：COVARIANCE_AND_EFFECTIVE_PSF §3、DESIGN-P2-001 §6.2/§6.3/§9、FZ-GATE-PSFSW-EPSF）：
  effective PSF 是**实际组合算子**的脉冲响应：`P_eff = Σ_k alpha_k a_k (P_k ⊗ K_k) / Σ_k alpha_k a_k`；
  `alpha_k` 为实际组合系数（含 UPM 尺度/validity/归一）。
- **ALG-P2PT-EPSF-03**（锚：COVARIANCE_AND_EFFECTIVE_PSF §3、DESIGN-P1 §6）：
  归一约定按产品族声明：point_source/detection 用 peak 归一；面亮度产品用积分归一 `Σ_x P_eff = 1`；
  `K_k` 与归一约定必须与 `P_eff` 一起保存。
- **ALG-P2PT-EPSF-04**（锚：COVARIANCE_AND_EFFECTIVE_PSF §3 (Oracle C6)、SCI-PSFW-001 §8）：
  FWHM/encircled energy 必须从 `P_eff` 测量；median/平均逐帧 FWHM 不得代替 `FWHM(P_eff)`。
- **ALG-P2PT-EPSF-05**（锚：FZ-GATE-PSFSW-EPSF、COVARIANCE_AND_EFFECTIVE_PSF §3 (R7)）：
  effective PSF 必输且 `effective_psf_id` 非空；只给 FWHM 标量不构成 effective PSF，必须 REJECT。

---

## 6. 输出帧重算

- **ALG-P2PT-OUT-01**（锚：PHASE3_PROPAGATION_REVIEW C-P3-PROP-14、FZ-P3-QW-RECOMPUTE、ADJ-P2-01）：
  **输出帧重算**：在输出帧位置 `x_o` 用逐帧原始量 `(a_k,P_k,C_k,d_k)` 重算 `Q(x_o)`、`W(x_o)`，
  而不是对输入 `Q_k/W_info,k` map 重采样。`R` 与匹配滤波不可交换，故 `W_out ≠ Σ_i (重采样核) W_in,i`。
- **ALG-P2PT-OUT-02**（锚：FZ-DEGRADE-SCALAR、UNIFIED §8、DESIGN-P1 §8.3、PROJECT_SPEC §4）：
  禁止把帧级/上游标量 `W_info` 直接替换输出帧 `W`；标量降级必须过空间残差门与功率损失门并携带
  `p05/p50/p95` + 最大系统偏差 + 采样覆盖 + 模型误差 + 适用域，否则存 map/model/control points。
- **ALG-P2PT-OUT-03**（锚：FZ-P3-QW-RECOMPUTE、PHASE3_PROPAGATION_REVIEW C-P3-PROP-14/15）：
  Phase3 边界：Phase3 以 `pi = S p` 与 `C_y` 在输出平面重算 `Q/W`（FZ-P3-QW-RECOMPUTE，归 ALG-P3-001），
  消费上游 `W_info` 不重算不替换；Phase2 只导出逐帧原始量与 covariance，不预先把输出 Q/W 作为可重采样输入。

---

## 7. independent-frame `SNR_combined² = Σ_k SNR_k²` 验证

- **ALG-P2PT-SNR-01**（锚：PROJECT_SPEC §8、DESIGN-P2-001 §10、UNIFIED §10）：
  独立帧验证：注入点源满足 `SNR_combined² = Σ_k SNR_k²`（解析恒等 + MC 实测 flux dispersion 与 `1/sqrt(W)` 一致）。
- **ALG-P2PT-SNR-02**（锚：UNIFIED §10、ADJ-P2-01、COVARIANCE_AND_EFFECTIVE_PSF §1 (Oracle C3)）：
  相关帧验证：简单 `Σ_k` 求和必须被检出并拒绝（joint-C 方差大于朴素求和；比值显著 >1 即 red）。
- **ALG-P2PT-SNR-03**（锚：UNIFIED §10、PROJECT_SPEC §4、SCI-P2-001 Oracle C8）：
  改变星表亮度分布只改变 source-SNR 摘要，不改变同一图像的 `W_info`（信息不随样本亮度变化）。

机器判据（`snr_identity`）：独立帧相对容差 `1e-9`；相关帧朴素求和判 `REJECT`，
检出阈值 `correlated_detection_ratio_min = 1.05`（joint/naive 方差比）。

---

## 8. fail-closed 门（负向门清单）

门 ID 与机器规格 `gates` 一致；每条门给出不可满足时的 `unavailable` 原因。

- **ALG-P2PT-GATE-01**（锚：FZ-UNIT-Q/WINFO/FLUX、ADJ-GEN-01）：**单位门** — 由声明量纲指数做代数必须得
  `Q=ADU⁻¹`、`W=ADU⁻²`、`flux=ADU`、`Var=ADU²`；否则 REJECT（`unit_algebra_inconsistent`）。
- **ALG-P2PT-GATE-02**（锚：ADJ-P2-01、FZ-GATE-PSFSW-EPSF、C-P3-PROP-13）：**PSF 归一** — `Σ_p P_k,p = 1`；
  非归一且未声明重标定即 REJECT（`psf_not_normalized`）。
- **ALG-P2PT-GATE-03**（锚：UNIFIED §7、PROJECT_SPEC §3、ADJ-P2-01）：**covariance 门** — `C_k` 必须提供且可表示；
  只给对角 variance 且未给相关核/算子摘要或未声明适用域即 REJECT（`covariance_unrepresentable`）。
- **ALG-P2PT-GATE-04**（锚：UNIFIED §6/§10、ADJ-P2-01）：**独立性门** — 检出跨帧相关时禁止朴素 `Σ`；
  必须用联合 `C_in` 否则 REJECT（`correlated_frames_require_joint_covariance`）。
- **ALG-P2PT-GATE-05**（锚：FZ-GATE-PSFSW-EPSF、R7）：**effective PSF 门** — 缺 effective PSF 或仅 FWHM 标量即 REJECT（`effective_psf_missing`）。
- **ALG-P2PT-GATE-06**（锚：FZ-FORMULA-COV-PROP、RULINGS.md #5、COVARIANCE_AND_EFFECTIVE_PSF §0/§1）：
  **covariance 来源门** — variance/covariance 必须由实际组合系数传播；用权重标量反推 variance 即 REJECT；
  近似 `C~` 必须报告 `c~^T C c~` 与真实 `C` 的偏差比（`variance_not_from_combination_coefficients`）。
- **ALG-P2PT-GATE-07**（锚：FZ-GATE-MEDIAN-SNR、FZ-GATE-SUPPORT-COVERAGE、C-004.2）：**诊断别名门** —
  `median_source_snr/median_snr/source_snr_median/support/coverage/fwhm/psf_residual` 等不得出现在
  `weight.sources` / `weight_value` / `covariance.variance_from`（`diagnostic_token_as_weight_source`）。
- **ALG-P2PT-GATE-08**（锚：FZ-MODE-PRODUCTION、FZ-MODE-DEFERRED、C-004.1/C-004.3、ADJ-S1）：**模式门** —
  生产科学模式仅 `{point_information,surface_gls,psfsw_robust}`；`psf_snr_power` 保持 DEFERRED；
  legacy `0=support×snr²` 与 `auto/support_x_snr2` 一律 REJECT（`production_mode_not_frozen`）。
- **ALG-P2PT-GATE-09**（锚：FZ-DEGRADE-SCALAR、UNIFIED §8、DESIGN-P1 §8.3）：**标量降级门** —
  `W_info` 默认空间量；压成帧级标量必须同时过空间残差/趋势门与功率损失门，并带分位数/覆盖/模型误差/适用域（`scalar_degradation_gate_failed`）。
- **ALG-P2PT-GATE-10**（锚：FZ-PROV-MINIMAL-SET、宪章 §4.3、DATA_SEMANTICS §30.3）：**provenance 门** —
  缺最小集键、单位不可判、unavailable 无原因即 REJECT（`provenance_incomplete`）。
- **ALG-P2PT-GATE-11**（锚：FZ-P3-QW-RECOMPUTE、C-P3-PROP-14）：**输出帧门** — Q/W 必须在输出帧由原始量重算；
  检出对输入 Q/W map 的重采样/平均求和即 REJECT（`output_frame_recompute_required`）。
- **ALG-P2PT-GATE-12**（锚：PROJECT_SPEC §8、UNIFIED §10）：**SNR 门** — 独立帧必须满足 `SNR_combined²=ΣSNR_k²`（容差内）；
  相关帧朴素求和须被检出为过度乐观并 REJECT（`independent_frame_snr_identity_violated`）。
- **ALG-P2PT-GATE-13**（锚：COVARIANCE_AND_EFFECTIVE_PSF §3、DESIGN-P2-001 §9）：**归一约定门** —
  effective PSF 必须显式声明 peak 或 integral 归一与对应产品族；未声明归一约定即 REJECT（`effective_psf_normalization_undeclared`）。

---

## 9. provenance 与可审计

provenance 最小集沿用 FZ-PROV-MINIMAL-SET（schema/软件 SHA/run ID/输入+配置哈希/单位+`pixel_area_power`/frame/
像素语义/算法 ID/provider/近似+降级原因/归一版本/相关核摘要/`flux_conservation_factor`/`k_corr`/时间/输出哈希）。
点源产品额外登记：`mode`、`science_objective`、权重分量与来源、effective PSF id 与归一约定、
逐帧 `Q_k/W_info,k/SNR_k`、近似 `C~` 与 `c~^T C c~` 偏差比（若用）、输出帧重算标记。
`unavailable` 必须带 §8 的原因码；单位不可判即 REJECT。锚：FZ-PROV-MINIMAL-SET、宪章 §4.3、UNIFIED §9。

---

## 10. 验证矩阵（独立 Oracle + 负向 mutation）

独立 Oracle：`run/v6/alg-p2-point/oracle/phase2_point_oracle.py`（纯 NumPy 从第一性原理实现，
**不 import/link/exec 任何生产实现**）。结构门：`run/v6/alg-p2-point/oracle/check_alg_p2_point_spec.py`
（读机器规格 + 本人读规格，做量纲代数/门引用/条款-锚一致性检查，**非子串自证**）。

正向 check（O1–O12）：`Q/W==GLS`；`Var==1/W==(AᵀC⁻¹A)⁻¹`；`cᵀCc==1/W`；独立帧 `Q=ΣQ_k,W=ΣW_k`；
相关帧 joint>naive；白噪近似 `W=a²/(σ²A_NEA)`；MC 注入源 `sigma_F==1/sqrt(W)`；
effective PSF 脉冲响应==解析组合；`FWHM(P_eff)≠median` 逐帧 FWHM；`SNR²==ΣSNR_k²`；单位量纲代数；`S=Q/sqrt(W)` 与 5σ depth。

负向门 mutation（M01–M15）与 Oracle 自我 mutation（OM1–OM3）：
每条注入错误后门必须 rc≠0，逐条命令与 rc 见 `run/v6/alg-p2-point/logs/` 与 `summary.json`。

| mutation | 注入 | 命中 |
|---|---|---|
| M01 | Q 去掉 `C⁻¹` | O1/O11 |
| M02 | W 单位改 ADU² | O11/GATE-UNIT-01 |
| M03 | `Var=W` | O2 |
| M04 | `P_k` 不归一 | GATE-PSFPROF-02 |
| M05 | 相关帧允许朴素 Σ | GATE-CORR-04/O5 |
| M06 | `effective_psf={"fwhm":4.71}` | GATE-EPSF-05 |
| M07 | `variance_from=psfsw_robust_weight` | GATE-COVPROP-06 |
| M08 | `weight.sources=[median_source_snr,support,coverage,fwhm,residual]` | GATE-DIAG-07 |
| M09 | 生产模式加入 `psf_snr_power` | GATE-MODE-08 |
| M10 | 移除 provenance 必需键 | GATE-PROV-10 |
| M11 | 帧级标量 W 无门无分位数 | GATE-SCALAR-09 |
| M12 | 允许重采样输入 Q/W | GATE-OUTFRAME-11 |
| M13 | `SNR_combined=ΣSNR_k` | GATE-SNR-12/O10 |
| M14 | 白噪近似用于非对角 C、未声明 σ | GATE-COV-03 |
| M15 | effective PSF 未声明归一 | GATE-EPSF-11 |
| OM1 | 组合系数 c ×1.10 | O3 |
| OM2 | 相关帧 ρ→0 当独立 | O5 |
| OM3 | 单位指数错 | O11 |

---

## 11. 移交、边界与登记

- **下游**：W4 `CONTRACT-FREEZE-001` 正式冻结本规格；W5 `IMPL-P1-PSFW-001`（Phase1 `W_info` 生产）、
  W8 `P2-INTEGRATE-001`（三模式集成）消费；`SCHEMA-INTEGRATE-001`/W6 归一字段词表（本规格只引用，不发明第三套）。
- **同波边界**：`surface_gls` 归 ALG-P2-SURF-001；`psfsw_robust` 复合权重细节归 ALG-P2-PSFSW-001；
  本规格只引用其 covariance `C_out=R C_in Rᵀ` 与 effective PSF 交界，不重定义。
- **需负责人签字项（只登记不擅改）**：SO-01..SO-07（见 FREEZE_LIST §7）。本任务不签署、不放宽容差。
- **控制器级事项（只登记不裁决）**：CTRL-F1（工作树≠HEAD）、CTRL-AR033（根构建面 owner）、
  CTRL-AR034/035/036。本规格以 HEAD=125bc099 为准。
- **未决风险**：
  1. F1 未裁决；本规格生产面判定以 HEAD 为准。
  2. `psf_snr_power` 保持 DEFERRED，本规格不放行。
  3. 相关帧 `C_in` 的低秩/相关核可表示形式与"独立帧求和失效"检测阈值数值由 W4 冻结（本规格只冻结度量与门存在性）。
  4. Phase3 `FZ-P3-QW-RECOMPUTE` 的详细算法归 ALG-P3-001；本规格只冻结 Phase2 侧的输出帧重算不变量与交接边界。
