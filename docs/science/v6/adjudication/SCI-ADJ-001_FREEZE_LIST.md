> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# SCI-ADJ-001 冻结清单（字段 / 公式 / mode / 适用域 / 降级 / 验证门）

> 上游：ASTROCS_DESIGN.md §2（核心科学方法）、§3（数据对象与配置）

- 文档 ID：`SCI-ADJ-001-ADJUDICATION` freeze-list 渲染产物
- 基线：`HEAD = main = bc166e9d4828b45ef32b679156e12d954640534a`
- 语义源：`reports/v6/science-adjudication/adjudications.json`（本文件由 `run/v6/adjudication/tools/render_freeze_list.py` 机械渲染，保证人读正文与机器表一致）
- 条款锚见 `docs/science/v6/adjudication/SCI-ADJ-001_CONFLICT_MATRIX.md`
- 说明：本清单是**目标态语义冻结**。正式冻结由 `CONTRACT-FREEZE-001`(W4) 写入 `docs/science/v6/frozen/`；数值阈值（标注由 ALG 冻结者）不在本任务范围。

## 1. 冻结单位表

| 符号 | 单位 | 含义 | 方差单位 | ivar 单位 |
|---|---|---|---|---|
| `signal_sb` | ADU/px^2 | Phase1 Drizzle/HiPS 面亮度 signal | ADU^2/px^4 | px^4/ADU^2 |
| `pixel_variance_in` | ADU^2 | 输入源像素逐像素方差 v_j | — | — |
| `W_info` | ADU^-2 | 点源信息权重 = 1/Var(F_hat) | — | — |
| `Q` | ADU^-1 | 点源线性充分统计量 | — | — |
| `flux` | ADU | F_hat 点源通量估计 | ADU^2 | ADU^-2 |
| `psfsw_robust_weight` | 1 | 无量纲组内相对复合权重 | — | — |
| `phase2_mosaic_signal` | BUNIT(声明) | Phase2 马赛克 signal; 面亮度产品则 ADU/px^2 | BUNIT^2 | 1/BUNIT^2 |
| `phase3_var_out` | BUNIT^2 | Phase3 输出方差 = 主 HDU BUNIT 平方 | — | 1/BUNIT^2 |

二次律冻结：variance = signal^2，ivar = 1/variance；Phase3 输出 variance BUNIT = (主 HDU signal BUNIT)^2（`FZ-P3-BUNIT-QUADRATIC`）。W_info 严格为 signal^-2（`FZ-UNIT-WINFO`）；psfsw_robust_weight 严格无量纲 = 1（`FZ-UNIT-PSFSW`）。

## 2. 冻结 mode

| mode | 状态 | 权重对象 | 单位 | 权威式 | covariance 来源 | effective PSF | 组内归一 | 禁止声明 |
|---|---|---|---|---|---|---|---|---|
| `point_information` | PRODUCTION_FROZEN | W_info | ADU^-2 | Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; F_hat=Q/W; Var=1/W | combination_coefficients | 必输 | 否 | support/coverage/median_source_snr/fwhm/residual as weight；pixel ivar equivalence for arbitrary PSF |
| `surface_gls` | PRODUCTION_FROZEN | A^T C^-1 A | 1/(surface_brightness^2) | x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1 | combination_coefficients | 必输 | 否 | pixel ivar unconditional optimality |
| `psfsw_robust` | PRODUCTION_FROZEN | psfsw_robust_weight | 1 | Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j) | combination_coefficients | 必输 | 是 | ivar；fisher_information；variance_from_weight；1/W_psfsw |
| `equal` | DOCUMENTED_BASELINE | unit_weight | 1 | I_out=mean_k d_k | combination_coefficients | 必输 | 否 | scientific optimality |
| `pixel_ivar` | DOCUMENTED_BASELINE | pixel_ivar | 1/BUNIT^2 | I_out=Sum_k w_k d_k/Sum_k w_k, w_k=1/v_k | combination_coefficients | 必输 | 否 | point-source optimality for arbitrary PSF |
| `psf_snr_power` | DEFERRED_NOT_PRODUCTION | ratio_of_powers | 1 | DEFERRED (未冻结) | combination_coefficients | 必输 | 是 | production；fisher_optimality |

- 生产科学模式（`FZ-MODE-PRODUCTION`）：`point_information` / `surface_gls` / `psfsw_robust`
- 文档基线模式（`FZ-MODE-BASELINE`）：`equal` / `pixel_ivar`（仅基线比较，非科学最优声明）
- 延迟模式（`FZ-MODE-DEFERRED`）：`psf_snr_power`（DEFERRED/NOT_IMPLEMENTED，不进 V6 生产路由，C-004.1）

## 3.1 冻结公式（9 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-FORMULA-DRIZZLE-SB` | 目标态面亮度归一 | S_p = Sum_j B_j a_jp / Sum_j a_jp, B_j=x_j/A_pixel,j (等价 c_jp=a_jp/Sum a_jp) | Phase1 Drizzle signal | DESIGN-P1 §9:120-126; UNIFIED §7; ADJ-F-OBS-02/S2 | 常量面亮度 S_p=B0 全 pixfrac |
| `FZ-FORMULA-DRIZZLE-VAR` | Drizzle 方差传播 | variance_p = Sum_j v_j w_jp^2 / D_p^2; ivar_p=1/variance_p | Phase1 variance | DRIZZLE.md §5:53-57; ADJ-F-OBS-01 | 缩放律 x->a*x ⇒ var->a^2 var |
| `FZ-FORMULA-WINFO` | 点源信息权重 | W_info,k = a_k^2 P_k^T C_k^-1 P_k = 1/Var(F_hat_k) | Phase1/Phase2 点源 | UNIFIED §4; PSF_SIGNAL_WEIGHT §2; ADJ-P2-01 | W=1/Var 与 CRLB |
| `FZ-FORMULA-Q` | 点源线性充分统计量 | Q_k = a_k P_k^T C_k^-1 d_k | Phase2 point_information | UNIFIED §4:45; ADJ-P2-01 | Q/W==GLS |
| `FZ-FORMULA-FHAT` | 通量估计与方差 | F_hat=Sum Q_k/Sum W_info,k; Var(F_hat)=1/Sum W_info,k | Phase2 point_source | UNIFIED §4:47-51; ADJ-P2-01 | 注入源 sigma_F=1/sqrt(W) |
| `FZ-FORMULA-GLS` | 扩展源 GLS | x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1 | Phase2 surface_gls | UNIFIED §5:60-61; ADJ-P2-02 | G C G^T == (A^T C^-1 A)^-1 |
| `FZ-FORMULA-COV-PROP` | covariance 传播 | C_out = R C_in R^T; 标量 c^T C_in c | Phase1/2/3 | UNIFIED §7; DESIGN-P3 §4; RULINGS.md #5; ADJ-P2-03/AR-02 | 禁止从权重标量反推 variance |
| `FZ-FORMULA-PSFSW-COMPOSITE` | PSFSW 复合与组内归一 | Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j); 指数/常数版本化 | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §3; PSFW_FREEZE §4.3; ADJ-P2-03 | 组内 median=1 且全正 |
| `FZ-P3-QW-RECOMPUTE` | Phase3 Q/W 输出帧重算 | Q=a*pi^T C_y^-1 f; W=a^2*pi^T C_y^-1 pi; pi=S p; 禁止重采样输入 Q/W; 消费上游 W_info 不重算不替换 | Phase3 point_source_flux | PHASE3_PROPAGATION_REVIEW C-P3-PROP-14/15:175-194; ADJ-P3-01 | q^T C_y^-1 q 与 MC 一致; 对角化过度乐观须检出 |

## 3.2 冻结单位（9 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-UNIT-SIGNAL-SB` | S_p (Phase1 drizzle/HiPS 面亮度 signal) | ADU/px^2 | Phase1 signal | DRIZZLE.md §5:48; ADJ-F-OBS-01 | BUNIT 量纲可判门 |
| `FZ-UNIT-VAR-IN` | pixel_variance_in v_j | ADU^2 | Drizzle 输入逐像素方差 | DRIZZLE.md §3:27; ADJ-F-OBS-01 | 量纲代数门 |
| `FZ-UNIT-VAR-SB` | sb_variance_out variance_p | ADU^2/px^4 | Phase1 variance | DRIZZLE.md §5:55; ADJ-F-OBS-01 | variance=signal^2 |
| `FZ-UNIT-IVAR-SB` | sb_ivar_out | px^4/ADU^2 | Phase1 ivar | DRIZZLE.md §5:56; ADJ-F-OBS-01 | ivar=1/variance |
| `FZ-UNIT-Q` | Q_k | ADU^-1 | Phase2 point_information | UNIFIED §4; ADJ-P2-01 | Q/W 量纲自洽 |
| `FZ-UNIT-FLUX` | F_hat | ADU | Phase2 point_source | UNIFIED §4; ADJ-P2-01 | Var(F)=1/W |
| `FZ-UNIT-WINFO` | W_info | ADU^-2 | Phase1/Phase2 点源信息权重 | PSF_SIGNAL_WEIGHT §2:35; ADJ-GEN-01 | W_info=signal^-2 |
| `FZ-UNIT-PSFSW` | psfsw_robust_weight | 1 | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §3/§5; ADJ-P2-03 | 无量纲门(禁 flux^-2/ivar) |
| `FZ-P3-BUNIT-QUADRATIC` | Phase3 variance BUNIT | variance BUNIT = (signal BUNIT)^2; ivar = 1/(signal BUNIT)^2 | Phase3 产品 | DATA_SEMANTICS §30.4:2475; ADJ-P3-01/GEN-01 | 非二次律即拒绝 |

## 3.3 冻结字段（4 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-BUNIT-SEMANTICS` | BUNIT/pixel_area_power | BUNIT 必须量纲可判：显式 px 幂次 或 provenance pixel_semantics=surface_brightness+pixel_area_power=-2 | 全部产品写盘 | ADJ-F-OBS-01; DATA_SEMANTICS §30.4:2475 | 单位不可判 -> unavailable/REJECT |
| `FZ-FIELD-PSFSW-4COMP` | PSFSW 四分量 | psfsw.signal / psfsw.concentration / psfsw.noise / psfsw.background (measurement_id 互异; p05/p50/p95; 有效覆盖) | Phase1/2 PSFSW | PSF_SIGNAL_WEIGHT §3; PSFW_FREEZE §4.2; ADJ-P2-03 | 四分量塌陷/缺分量即 REJECT |
| `FZ-FIELD-PSFSW-UNIT` | PSFSW 单位/归一语义 | weight_kind=relative_dimensionless; weight_units=1; group_normalized=true; normalization.scope=group | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §3; ADJ-P2-03 | units 含 flux^-2/ivar 即 REJECT |
| `FZ-FIELD-WEIGHTMODE` | weight_mode 语义 | 显式三模式; legacy 整数 {0=support x snr^2,1=equal,2=ivar} 被取代; 0 不得进科学权重面; schema 词表归 W6 | Phase2 | ADJ-S1; C-004.3; UNIFIED §11 | schema 词表未归一前不得发明第三套 |（已按 §9.73 A44 作废：该概念不存在）

## 3.4 冻结模式（4 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-MODE-PRODUCTION` | 生产科学权重口径 （已按 §9.73 A44 作废：该概念不存在；权重是阶段二按该天球像素对应帧集合现场算出的派生量） | point_information | surface_gls | psfsw_robust | Phase2 配置/路由 | PROJECT_SPEC §5; PSF_SIGNAL_WEIGHT §4; C-004.1; ADJ-S1 | 未知模式/legacy 0 进生产即 REJECT |
| `FZ-MODE-BASELINE` | 文档基线模式 | equal | pixel_ivar (仅基线比较, 非科学最优声明) | Phase2 基线对比 | DESIGN-P2 §6.3; ADJ-S1 | 基线模式冒充最优即 REJECT |
| `FZ-MODE-DEFERRED` | 延迟模式 | psf_snr_power (DEFERRED/NOT_IMPLEMENTED, 不进 V6 生产路由) | Phase2 | CONTROLLER_LOG C-004.1; 00_READ_FIRST; ADJ-C004-01 | 进生产模式列表即 REJECT |
| `FZ-P3-MODES` | Phase3 输出模式 | surface_brightness | point_source_flux | visualization | Phase3 配置 | DESIGN-P3 §1:11-13; ADJ-P3-01 | 模式未声明即拒绝 |

## 3.5 冻结适用域（2 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-COND-FLUX-CONSERV` | 通量守恒条件不变量 | pixfrac=1: Sum_p F_p=Sum_j x_j 严格; pixfrac<1: 总输出通量=pixfrac^2*Sum_j x_j, provenance flux_conservation_factor | Phase1 flux 换算/aperture | UNIFIED §7; ADJ-F-OBS-02 | 缺 flux_conservation_factor 即不可用于绝对通量 |
| `FZ-COND-WHITENOISE` | 白噪声近似(条件式) | W_info,k = a_k^2/(sigma_pix,k^2*A_NEA,k), A_NEA=1/Sum P_p^2 | Phase1/2 点源 | PSF_SIGNAL_WEIGHT §2:30-33; ADJ-P2-01 | 仅 C 对角且 sigma_pix 声明时可用 |

## 3.6 冻结降级（1 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-DEGRADE-SCALAR` | 帧级标量降级门 | 空间残差/趋势门 + 功率损失门 双过; 标量带 p05/p50/p95+最大系统偏差+采样覆盖+模型误差+适用域; 否则 map/model/control points | Phase1/Phase2 | UNIFIED §8:78-86; DESIGN-P1 §8.3; PROJECT_SPEC §4; ADJ-GEN-04 | 缺分位数/未过门即 REJECT |

## 3.7 冻结provenance（3 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-PROV-SHARED-SYSTEMATIC` | 共享系统项表示 | 低秩因子 L/C_shared=L L^T 或 相关核 sigma+kernel 或 共同 master ID+强度参数; 进 covariance 传播链; 无法表示 -> unavailable 或系统误差预算 | Phase1/2/3 covariance | UNIFIED §6:68-72; PHASE1 §4.2:58; ADJ-OBS-01/F-OBS-04 | 当独立项处理即 REJECT(ratio>1 须检出) |
| `FZ-PROV-KCORR` | k_corr 可复现口径 | 定义 k_corr=Var(median)/[pi sigma_bg^2/(2 N_retained)]; 适用域(几何/pixfrac/patch/估计器/球面)显式; 标定脚本+固定种子 MC 可复跑; 未复跑前仅域内用 1.4; 按尺度查找表保留 | Phase2 UPM | UNCERTAINTY §V19R3; sampler.cpp:81-92; ADJ-F-OBS-05 | 跨域外推/忽略相关(k_corr=1)即 REJECT |
| `FZ-PROV-MINIMAL-SET` | provenance 最小集 | schema/软件 SHA/run ID/输入+配置哈希/单位+pixel_area_power/frame/像素语义/算法 ID/provider/近似+降级原因/归一版本/相关核摘要/flux_conservation_factor/k_corr/时间/输出哈希 | 全部产品 | 宪章 §4.3; UNIFIED §9; DATA_SEMANTICS §30.3; ADJ-GEN-03 | 缺键/单位不可判/unavailable 无原因即 REJECT |

## 3.8 冻结验证门（10 条）

| id | 主题 | 冻结值 | 生效范围 | 条款/证据锚 | 验证门 |
|---|---|---|---|---|---|
| `FZ-GATE-CONST-SB` | 常量场 Oracle | 按 B0 构造 x_j=B0*A_pixel_j; S_p=B0 对全部 pixfrac in (0,1]; |S_p/B0-1|<1e-3(沿用) | Drizzle 验收 | DRIZZLE.md §5/§7; ADJ-S3; p1drz_oracle.hpp:124-127 | 常量 ADU 构造/无条件 pixfrac/S_p=F_p 三错法必红 |
| `FZ-GATE-PIXIVAR-APPROX` | 像素 ivar 近似误差门 | 条件: 同点采样+噪声独立+a_k 一致; 门度量 Var_approx/Var_GLS <= 1+epsilon (epsilon 由 ALG-P2-SURF-001 冻结) | Phase2 surface_gls | UNIFIED §5:64; ADJ-P2-02 | 无误差门声明即 REJECT; 报告 R~ C_in R~^T |
| `FZ-GATE-PSFSW-FAILCLOSED` | PSFSW fail-closed | unavailable 原因属于 {no_common_star_set, background_nonpositive_undefined_transform, insufficient_valid_stars, selection_bias_gate_failed, spatial_nonuniformity_gate_failed}; valid=false 时 weight_value=null | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §3:49; PSFW_FREEZE §5.1; ADJ-P2-03 | 回退 median SNR 即 REJECT |
| `FZ-GATE-PSFSW-COV` | PSFSW covariance 来源 | covariance.method=propagated_from_composite_coefficients; variance_from_weight=false; uses_relative_weight_as_ivar=false | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §5; RULINGS.md #5; ADJ-P2-03 | variance=1/W_psfsw 即 REJECT |
| `FZ-GATE-PSFSW-EPSF` | PSFSW effective PSF | effective_psf_id 非空; 实际组合算子脉冲响应; 归一约定按产品族声明; 只给 FWHM 标量不构成 effective PSF | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §5; COVARIANCE_AND_EFFECTIVE_PSF §3; ADJ-P2-03 | 缺 effective PSF 即 REJECT |
| `FZ-GATE-MEDIAN-SNR` | median(SNR_F) 诊断 | 仅诊断/深度表达; 禁止作权重来源/weight_value/冒充 PSFSW/W_info/ivar | Phase1/Phase2 | PROJECT_SPEC §4; C-004.2; UNIFIED §11; ADJ-C004-02 | 诊断别名进 weight.sources 即 REJECT |
| `FZ-GATE-SUPPORT-COVERAGE` | support/coverage 非权重 | 不得作 inverse-variance/SNR/科学权重; 只作门 | Phase1/2/3 | 宪章 §6.3:191; UNIFIED §3; ADJ-GEN-02 | support/coverage 进 weight.sources 即 REJECT |
| `FZ-P3-FAILCLOSED` | Phase3 fail-closed | surface_brightness: 无 Omega 却 flux 换算/测量却 uncertainty unavailable/BUNIT 非二次律/对角无相关核 -> 拒; point_source_flux: 缺 PSF/PSF 未归一/缺 point_information 且不可重建/对角无相关核/缺 a/未出 effective PSF -> 拒; visualization: measurement_capable=true 或写测量层 -> 拒 | Phase3 | PHASE3_PROPAGATION_REVIEW §6 C-P3-PROP-16; DESIGN-P3 §1/§4; ADJ-P3-01 | 12 门 mutation 全红 |
| `FZ-P3-KERNEL-REGISTRY` | Phase3 采样核注册 | 采样核是产品语义; bilinear_4quad 须先独立 Oracle+误差/边界定义才注册; nearest 仅 mask/诊断/显式选择; 高阶核各自注册带 Oracle | Phase3 重采样 | DESIGN-P3 §3:30-37; ADJ-S4 | 未注册/未验证核进生产即 REJECT |
| `FZ-GATE-PARENT-VAR` | HiPS 父级方差对角近似 | 声明下界 + 另存相关核/算子摘要 + 误差门 deficit=(exact-diag)/exact<=阈值(ALG 冻结) | Phase1 HiPS variance | UNIFIED §7; PROJECT_SPEC §3; ADJ-F-OBS-03 | 对角当精确即 REJECT |

## 4. 结构完整性强制项（required_freeze_ids）

以下 19 条冻结条目**缺失即红**（负向 mutation 逐条删除并以 rc!=0 证明）：

- `FZ-UNIT-WINFO`
- `FZ-UNIT-PSFSW`
- `FZ-BUNIT-SEMANTICS`
- `FZ-FORMULA-DRIZZLE-SB`
- `FZ-GATE-CONST-SB`
- `FZ-FORMULA-WINFO`
- `FZ-FORMULA-GLS`
- `FZ-GATE-PIXIVAR-APPROX`
- `FZ-FORMULA-COV-PROP`
- `FZ-FIELD-PSFSW-4COMP`
- `FZ-GATE-PSFSW-COV`
- `FZ-GATE-PSFSW-EPSF`
- `FZ-MODE-PRODUCTION`
- `FZ-MODE-DEFERRED`
- `FZ-GATE-MEDIAN-SNR`
- `FZ-P3-QW-RECOMPUTE`
- `FZ-P3-FAILCLOSED`
- `FZ-DEGRADE-SCALAR`
- `FZ-PROV-KCORR`

## 5. 禁止项（缺省即红）

### 5.1 禁止作为权重来源的诊断别名（forbidden_weight_source_tokens）

`median_source_snr`、`median_snr`、`source_snr_median`、`med_source_snr`、`support`、`support_area`、`coverage`、`coverage_area`、`fwhm`、`psf_fwhm`、`median_fwhm`、`source_fwhm`、`residual`、`psf_residual`、`psf_fit_residual`、`fit_residual`、`psfsw_robust_weight`、`psfsw`

任一 token 出现在 `weight.sources` / `weight_value` / `covariance.variance_from` 即 REJECT（`FZ-GATE-MEDIAN-SNR`、`FZ-GATE-SUPPORT-COVERAGE`；对应 SCI-P2-001 门 R3/R6）。

### 5.2 psfsw 产物禁止键

psfsw_robust 产物任何层不得出现 `ivar`、`variance`、`var`、`sigma`、`sigma2`、`inverse_variance`、`fisher`、`information`、`w_info`、`w_psf`（键集合命中即 REJECT，`FZ-GATE-PSFSW-COV`）。

### 5.3 生产权重枚举禁止值

`psf_snr_power`（C-004.1 延迟）、legacy `0 = support×snr²`、`auto`、`support_x_snr2`（C-004.3/AR-029）；出现即 REJECT。

## 6. 降级路径冻结（`FZ-DEGRADE-SCALAR`）

1. W_info / 背景 / variance / photometric response / PSF 默认空间模型；
2. 压成帧级标量必须**同时**过 (a) 空间残差/趋势门与 (b) 功率损失门（对最终 flux bias / variance / detection power 的损失低于阈值，阈值由 ALG 冻结）；
3. 标量摘要必须带 `p05/p50/p95` + 最大系统偏差 + 采样覆盖 + 模型误差 + 适用域；
4. 任一不满足 → 存 map/model/control points，不得先删科学信息再证明；
5. psfsw 四分量显著空间非均匀时拆 region/tile 或拒绝标量；
6. IPV 配置面维持冻结不可配置（AR-018 / V4 R10）。

## 7. 需负责人签字项（本任务无权签署）

| ID | 事项 | 理由 | owner | 权威 |
|---|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | FROZEN SCI 术语修正 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 |
| SO-02 | F-OBS-02 / S2 面亮度归一改为 (B) 面亮度保持 | FROZEN DRIZZLE.md §5/§7/§11 公式变更 + 重跑全部 Drizzle 不变量/variance 门 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | FROZEN 判据取代登记 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | FROZEN DRIZZLE §1/§9a 取代登记 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | 宪章修订须负责人签字；不得由 Agent 放宽 | 项目负责人 | 宪章 §1.2/§18 |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | 跨文档权威裁定与 DOC-CONVERGE 收口 | 项目负责人 + CONTRACT-FREEZE-001 + DOC-CONVERGE-001 | PROJECT_SPEC §11 |
| SO-07 | F-OBS-03/F-OBS-04/F-OBS-05 的数值阈值/数据面/标定 | 误差门数值、低秩数据面、k_corr 标定脚本须 W3/W4 冻结并由负责人确认 | ALG-P1-001/ALG-P2-UPM-001/CONTRACT-FREEZE-001 + 负责人 | CSI 冻结流程 |

## 8. 控制器级事项（只登记不裁决）

| ID | 事项 | 裁决 owner | 引用 | 状态 |
|---|---|---|---|---|
| CTRL-F1 | F1 工作树不等于 HEAD (16 回退 + 10 删除) | 控制器 | C-004.6 | registered_only |
| CTRL-AR033 | 根构建面 (CMakeLists) 无 V6 owner | 控制器 | C-004.4 | registered_only |
| CTRL-AR034 | 7 项历史 CI 红 | 控制器/DOC-CONVERGE-001/RUNTIME-CI-001 | AR-034 | registered_only |
| CTRL-AR035 | 785 缺陷账本无销账任务 | 控制器 | AR-035 | registered_only |
| CTRL-AR036 | 宪章修订签字项 | 项目负责人 | AR-036 | registered_only |
