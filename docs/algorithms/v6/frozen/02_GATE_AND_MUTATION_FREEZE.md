> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 1. fail-closed 与负向 mutation 冻结矩阵（全部条款）

| 条款 id | 状态 | 验证门 | fail-closed | 负向 mutation |
|---|---|---|---|---|
| FZ-UNIT-SIGNAL-SB | FROZEN | BUNIT 量纲可判门 | signal 缺 units，或 SB signal 单位 ≠ ADU/px^2 且 BUNIT 不可判 → unavailable/REJECT | 把 signal_sb 单位改为 ADU 或 ADU^2/px^4，量纲代数门必须 rc!=0 |
| FZ-UNIT-VAR-IN | PENDING_OWNER_SIGNOFF | 量纲代数门 | 输入逐像素方差单位 ≠ ADU^2（与输出面亮度方差混名）→ REJECT | pixel_variance_in 单位写成 ADU^2/px^4 → rc!=0 |
| FZ-UNIT-VAR-SB | PENDING_OWNER_SIGNOFF | variance=signal^2 | sb_variance_out ≠ ADU^2/px^4 或 variance ≠ signal^2 → REJECT | sb_variance_out 单位改 ADU^2 → rc!=0 |
| FZ-UNIT-IVAR-SB | PENDING_OWNER_SIGNOFF | ivar=1/variance | sb_ivar_out ≠ px^4/ADU^2 或 ivar ≠ 1/variance → REJECT | sb_ivar_out 单位改 ADU^-2 → rc!=0 |
| FZ-UNIT-Q | FROZEN | Q/W 量纲自洽 | Q ≠ ADU^-1 或 Q/W 量纲不自洽 → REJECT（GATE-UNIT-01） | Q 单位改 ADU^-2 或 ADU → rc!=0 |
| FZ-UNIT-FLUX | FROZEN | Var(F)=1/W | F_hat ≠ ADU 或 Var(F_hat) ≠ ADU^2 或 Var(F)≠1/W → REJECT | F_hat 单位改 ADU/px^2 → rc!=0 |
| FZ-UNIT-WINFO | FROZEN | W_info=signal^-2 | W_info ≠ ADU^-2，或与 psfsw/ivar/signal 混名 → REJECT | W_info 单位改 ADU^-1 → rc!=0 |
| FZ-UNIT-PSFSW | FROZEN | 无量纲门(禁 flux^-2/ivar) | psfsw_robust_weight ≠ 1 或出现 flux^-2/ivar 词 → REJECT | weight.units="flux^-2" → rc!=0 |
| FZ-BUNIT-SEMANTICS | PENDING_OWNER_SIGNOFF | 单位不可判 -> unavailable/REJECT | BUNIT=ADU 且无 pixel_semantics=surface_brightness + pixel_area_power=-2 → 单位不可判 → unavailable/REJECT | 删除 provenance 的 pixel_area_power 声明 → rc!=0 |
| FZ-FORMULA-DRIZZLE-SB | PENDING_OWNER_SIGNOFF | 常量面亮度 S_p=B0 全 pixfrac | 归一版本缺失，或 legacy 归一（w=a/A_drop）用于 pixfrac<1 绝对面亮度 → REJECT/不可跨 pixfrac 合成 | 常量 ADU 构造 / 无条件 pixfrac / S_p=F_p 三错法任一 → 常量场门 rc!=0 |
| FZ-FORMULA-DRIZZLE-VAR | PENDING_OWNER_SIGNOFF | 缩放律 x->a*x ⇒ var->a^2 var | variance_p ≠ Σ_j v_j w_jp^2/D_p^2 或缩放律破坏 → REJECT | 方差传播漏 D_p^2（或漏平方）→ rc!=0 |
| FZ-COND-FLUX-CONSERV | PENDING_OWNER_SIGNOFF | 缺 flux_conservation_factor 即不可用于绝对通量 | pixfrac<1 缺 flux_conservation_factor 却用于绝对通量/孔径 → REJECT | 删除 flux_conservation_factor 仍声明绝对通量 → rc!=0 |
| FZ-GATE-CONST-SB | PENDING_OWNER_SIGNOFF | 常量 ADU 构造/无条件 pixfrac/S_p=F_p 三错法必红 | 常量场门未按 B0 构造、未覆盖全部 pixfrac(0,1]、或改动容差 1e-3 → REJECT | 常量 ADU 构造 / 无条件 pixfrac / S_p=F_p 三错法 → rc!=0 |
| FZ-FORMULA-WINFO | FROZEN | W=1/Var 与 CRLB | W_info 公式被改写，或 Q/W 不数值等于 GLS，或 Var≠1/W → REJECT | W 去掉 C^-1，或 Var(F_hat)=W → rc!=0 |
| FZ-FORMULA-Q | FROZEN | Q/W==GLS | Q 公式被替换或量纲不自洽（Q≠ADU^-1）→ REJECT | Q 去掉 C^-1（aP^T d）→ rc!=0 |
| FZ-FORMULA-FHAT | FROZEN | 注入源 sigma_F=1/sqrt(W) | F_hat≠Q/W 或 Var(F_hat)≠1/W → REJECT | Var(F_hat)=W（去掉倒数）→ rc!=0 |
| FZ-COND-WHITENOISE | FROZEN | 仅 C 对角且 sigma_pix 声明时可用 | C 含相关项仍无条件用白噪声近似，或未声明 sigma_pix → REJECT | 非对角 C 用白噪式且未声明 sigma_pix → rc!=0 |
| FZ-FORMULA-GLS | FROZEN | G C G^T == (A^T C^-1 A)^-1 | surface_gls 不用 A^T C^-1 A，或用『先 coadd 后除权重』替代 → REJECT | GLS 退化为 OLS → rc!=0 |
| FZ-GATE-PIXIVAR-APPROX | FROZEN | 无误差门声明即 REJECT; 报告 R~ C_in R~^T | 像素 ivar 近似无误差门声明，或未报告 R~ C_in R~^T 与 Var_approx/Var_GLS → REJECT | 忽略 a_k 的 ivar 近似且宣称最优（rho>1+eps）→ rc!=0 |
| FZ-FORMULA-COV-PROP | FROZEN | 禁止从权重标量反推 variance | 由权重标量/诊断量反推 variance，或 C_out≠R C_in R^T → REJECT | variance=1/W_psfsw 或 variance_from=psfsw_robust_weight → rc!=0 |
| FZ-FORMULA-PSFSW-COMPOSITE | FROZEN | 组内 median=1 且全正 | 组内 median≠1、出现非正 W_psfsw、或指数/常数未版本化 → REJECT | 去掉组内 median 归一（W_psfsw=Wt）→ rc!=0 |
| FZ-FIELD-PSFSW-4COMP | FROZEN | 四分量塌陷/缺分量即 REJECT | 四分量塌陷为三、缺分量、measurement_id 重复、p05>p50>p95 或 valid_area_fraction∉[0,1] → REJECT | concentration 直接复制 signal（measurement_id 重复）→ rc!=0 |
| FZ-FIELD-PSFSW-UNIT | FROZEN | units 含 flux^-2/ivar 即 REJECT | weight.units 含 flux^-2/ivar、group_normalized=false、scope=global、median_target≠1 → REJECT | group_normalized=false → rc!=0 |
| FZ-GATE-PSFSW-FAILCLOSED | FROZEN | 回退 median SNR 即 REJECT | valid=false 时 weight_value 非 null，或 reason 不在 5 项白名单，或回退 median source SNR → REJECT | valid=false 仍写回 median source SNR 值 → rc!=0 |
| FZ-GATE-PSFSW-COV | FROZEN | variance=1/W_psfsw 即 REJECT | covariance.method≠propagated_from_composite_coefficients、variance_from_weight=true、uses_relative_weight_as_ivar=true，或命中 psfsw 禁止键 → REJECT | covariance.variance_from="psfsw_robust_weight"，或产物注入 ivar 键 → rc!=0 |
| FZ-GATE-PSFSW-EPSF | FROZEN | 缺 effective PSF 即 REJECT | effective_psf_id 空，或只给 FWHM 标量，或未声明归一约定 → REJECT | effective_psf={"fwhm":4.71} → rc!=0 |
| FZ-MODE-PRODUCTION | FROZEN | 未知模式/legacy 0 进生产即 REJECT | 生产模式枚举出现未知值/legacy 0/auto/support_x_snr2/psf_snr_power → REJECT | 生产枚举加入 psf_snr_power → rc!=0 |
| FZ-MODE-BASELINE | FROZEN | 基线模式冒充最优即 REJECT | equal/pixel_ivar 声明科学最优或冒充生产模式 → REJECT | pixel_ivar 宣称对任意 PSF 点源最优 → rc!=0 |
| FZ-MODE-DEFERRED | FROZEN | 进生产模式列表即 REJECT | psf_snr_power 进生产路由或进入 schema 合法域 → REJECT（C-004.1 不得解冻） | 把 psf_snr_power 写入生产模式列表 → rc!=0 |
| FZ-FIELD-WEIGHTMODE | FROZEN | schema 词表未归一前不得发明第三套 | legacy 整数 0 进科学权重面、未知模式、或发明第三套词表 → REJECT | weight_mode=0（support×snr²）→ rc!=0 |（已按 §9.73 A44 作废：该概念不存在）
| FZ-GATE-MEDIAN-SNR | FROZEN | 诊断别名进 weight.sources 即 REJECT | median(SNR_F)/median_source_snr 等诊断别名进 weight.sources/weight_value → REJECT（C-004.2 不得接入权重面） | weight.sources=[median_source_snr] → rc!=0 |
| FZ-GATE-SUPPORT-COVERAGE | FROZEN | support/coverage 进 weight.sources 即 REJECT | support/coverage 作 inverse-variance/SNR/科学权重或代替 variance → REJECT | coverage 当 variance → rc!=0 |
| FZ-P3-MODES | FROZEN | 模式未声明即拒绝 | Phase3 模式未声明或不属 {surface_brightness, point_source_flux, visualization} → REJECT | 加入 legacy auto 模式 → rc!=0 |
| FZ-P3-FAILCLOSED | FROZEN | 12 门 mutation 全红 | 12 门任一命中：SB 无 Omega 做 flux 换算/测量却 uncertainty unavailable/BUNIT 非二次律/对角无相关核；PSF 缺/未归一/缺 W 且不可重建/缺 a/未出 effective PSF；visualization measurement_capable=true 或写测量层 → REJECT | 12 条 M-P3-* mutation 逐条 rc!=0 |
| FZ-P3-QW-RECOMPUTE | FROZEN | q^T C_y^-1 q 与 MC 一致; 对角化过度乐观须检出 | 重采样输入 Q/W、W=ΣW_in、或重算/替换上游 W_info → REJECT | 用重采样输入 W 代替输出帧重算 Q/W → rc!=0 |
| FZ-P3-KERNEL-REGISTRY | FROZEN | 未注册/未验证核进生产即 REJECT | 未注册核进生产、nearest 作连续场科学默认、注册缺独立 Oracle/误差界/边界定义 → REJECT | bilinear_4quad 注册缺 Oracle，或零填边界 → rc!=0 |
| FZ-P3-BUNIT-QUADRATIC | FROZEN | 非二次律即拒绝 | var BUNIT ≠ (signal BUNIT)^2 或 ivar ≠ 1/variance → REJECT | Phase3 variance BUNIT 改为 signal BUNIT 的一次幂 → rc!=0 |
| FZ-DEGRADE-SCALAR | FROZEN | 缺分位数/未过门即 REJECT | 帧级标量未同时过空间残差/趋势门与功率损失门，或缺 p05/p50/p95/最大系统偏差/采样覆盖/模型误差/适用域 → REJECT（须存 map/model/control points） | 标量摘要删除 p05/p50/p95 或功率损失门 → rc!=0 |
| FZ-PROV-SHARED-SYSTEMATIC | FROZEN | 当独立项处理即 REJECT(ratio>1 须检出) | 共享系统项按独立随机项处理且无 unavailable/系统误差预算 → REJECT | 共享系统项当独立（联合/朴素方差比>1 未检出）→ rc!=0 |
| FZ-GATE-PARENT-VAR | PENDING_OWNER_SIGNOFF | 对角当精确即 REJECT | HiPS 父级对角归约未声明下界、未另存相关核/算子摘要、或缺 deficit 误差门 → 该 variance 面 unavailable | 把父级对角归约声明为精确（is_lower_bound=false）→ rc!=0 |
| FZ-PROV-KCORR | FROZEN | 跨域外推/忽略相关(k_corr=1)即 REJECT | k_corr 缺适用域/标定脚本/固定种子，或跨域外推，或令 k_corr=1 忽略相关 → REJECT | k_corr 忽略相关取 1.0 → rc!=0 |
| FZ-PROV-MINIMAL-SET | FROZEN | 缺键/单位不可判/unavailable 无原因即 REJECT | provenance 缺最小集键、单位不可判、或 unavailable 无原因 → REJECT | 删除 flux_conservation_factor 或 k_corr 键 → rc!=0 |
| PSFSW-T-DEPTH | PENDING_OWNER_SIGNOFF | PSFSW-G11 | depth_scan_max_rel_dev>0.05 → unavailable(selection_bias_gate_failed) | 深度扫描 max_rel_dev 取 0.20 → rc!=0 |
| PSFSW-T-DEPTH-K | PENDING_OWNER_SIGNOFF | PSFSW-G11 | K<5 → 不可评估 → insufficient_valid_stars | K=2 → rc!=0 |
| PSFSW-T-DEPTH-SPAN | PENDING_OWNER_SIGNOFF | PSFSW-G11 | 跨度<1.0 mag 且 n_common 变化<2 倍 → insufficient_valid_stars | 跨度=0.2 mag → rc!=0 |
| PSFSW-T-DEPTH-RHO | PENDING_OWNER_SIGNOFF | PSFSW-G11 | \|rho_s\|>0.8 → selection_bias_gate_failed | rho=0.95 → rc!=0 |
| PSFSW-T-NMIN | PENDING_OWNER_SIGNOFF | PSFSW-G10 | n_common<3 → valid=false+insufficient_valid_stars+weight_value=null | n_common=2 → rc!=0 |
| PSFSW-T-NROBUST | PENDING_OWNER_SIGNOFF | PSFSW-G10/G18 | 低于 10 → LOW 档旗标 + 非均匀阈值收紧 0.20 | 删除 LOW 档旗标 → rc!=0 |
| PSFSW-T-NPREF | PENDING_OWNER_SIGNOFF | PSFSW-G10 | 仅旗标，不判失败 | 把 30 当硬门拒绝合法记录 → 正向控制 rc!=0 |
| PSFSW-T-NU | PENDING_OWNER_SIGNOFF | PSFSW-G18 | 任一分量>0.30 → 拆 region/tile 或 spatial_nonuniformity_gate_failed | (p95-p05)/p50=0.45 → rc!=0 |
| PSFSW-T-TREND | PENDING_OWNER_SIGNOFF | PSFSW-G18 | 趋势>0.10 → spatial_nonuniformity_gate_failed | 趋势=0.30 → rc!=0 |
| PSFSW-T-POWERLOSS | PENDING_OWNER_SIGNOFF | PSFSW-G19 | 损失>0.05 → spatial_nonuniformity_gate_failed | 损失=0.20 → rc!=0 |
| PSFSW-T-FLUXBIAS | PENDING_OWNER_SIGNOFF | PSFSW-G19 | 偏差>0.01 → spatial_nonuniformity_gate_failed | 偏差=0.10 → rc!=0 |
| PSFSW-T-WRANGE | PENDING_OWNER_SIGNOFF | PSFSW-G14 | >100 → 强制旗标 weight_dynamic_range_exceeded + 集中度报告（不判 unavailable） | 超限即判 unavailable → 正向控制 rc!=0 |
| PSFSW-T-COVMC | PENDING_OWNER_SIGNOFF | PSFSW-G06/V5 | >0.03 → REJECT | rel=0.10 → rc!=0 |
| PSFSW-T-BOOT | PENDING_OWNER_SIGNOFF | PSFSW-G25 | <200 → 声明无效 | bootstrap=50 → rc!=0 |
| PSFSW-T-CI | PENDING_OWNER_SIGNOFF | PSFSW-G25 | <0.95 → 声明无效 | CI=0.68 → rc!=0 |
| PSFSW-T-EPSFTOL | PENDING_OWNER_SIGNOFF | PSFSW-G07/V7 | 超容差 → REJECT | 容差放宽到 1e-3 → rc!=0 |
| PSFSW-T-NU-LOW | PENDING_OWNER_SIGNOFF | PSFSW-G18 | LOW 档 >0.20 → 拆 tile/unavailable | LOW 档仍用 0.30 → rc!=0 |
| PSFSW-COMPOSITE-ALPHA | PENDING_OWNER_SIGNOFF | PSFSW-G14 | 指数/常数未版本化或非正方向 → REJECT | alpha 改为 -2（方向反转）→ rc!=0 |
| PSFSW-COMPOSITE-BETA | PENDING_OWNER_SIGNOFF | PSFSW-G14 | 同上 | beta=0 使 Conc 无效 → rc!=0 |
| PSFSW-COMPOSITE-GAMMA | PENDING_OWNER_SIGNOFF | PSFSW-G14 | 同上 | gamma=-2 → rc!=0 |
| PSFSW-COMPOSITE-DELTA | PENDING_OWNER_SIGNOFF | PSFSW-G14 | 同上 | delta=0 使 B 无效 → rc!=0 |
| PSFSW-COMPOSITE-CNORM | PENDING_OWNER_SIGNOFF | PSFSW-G16 | C_norm 影响 W_psfsw（违反组内中值归一）→ REJECT | C_norm×1e9 改变 W_psfsw（伪实现）→ rc!=0 |
| PSFSW-COMPOSITE-FLOOR | PENDING_OWNER_SIGNOFF | PSFSW-G14 | 顺序颠倒（先 floor 后 fail-closed）或取消下限 → REJECT | B≤0 先 floor 再造权重 → rc!=0 |
| PSFSW-BASELINE-NONINFERIOR | PENDING_OWNER_SIGNOFF | PSFSW-G25 | CI 下界 ≤ -0.02 却声明 non_inferior → REJECT | 用 0.5 作不劣界 → rc!=0 |
| FZ-AP2S-EPS-PIXIVAR | PENDING_OWNER_SIGNOFF | 门 G-PIXIVAR-APPROX | rho_p95>1.05 → 拒绝/升级完整 GLS；无误差门声明即 REJECT | eps 放宽到 0.30 → rc!=0 |
| FZ-AP2S-EPS-PIXIVAR-SUP | PENDING_OWNER_SIGNOFF | 门 G-PIXIVAR-APPROX | 任一有效元素 rho>1.20 → 该元素 fail-closed（转空间模型/unavailable） | 删除逐元素硬上限 → rc!=0 |
| FZ-AP2S-RANK-RTOL | PENDING_OWNER_SIGNOFF | G-P2S-RANK | 秩亏 → 该元素/块 unavailable，禁止伪逆静默 | rank_rtol 放宽到 1e-4 → rc!=0 |
| FZ-AP2S-KAPPA-MAX | PENDING_OWNER_SIGNOFF | G-P2S-KAPPA | kappa>1e6 → 分组件或 unavailable，禁止静默欠定解 | kappa_max 放宽到 1e9 → rc!=0 |
| FZ-AP2S-UPM-MINFRAMES | PENDING_OWNER_SIGNOFF | G-P2S-UPM-MINF | 单帧区拟合 g → 禁止；须 additive-only 降级声明 | min_frames=1 → rc!=0 |
| FZ-AP2S-IDENT-RTOL | PENDING_OWNER_SIGNOFF | 门 G-P2S-IDENT | 恒等式超容差 → REJECT | 容差放宽到 1e-3 → rc!=0 |
| FZ-AP2S-MC-RELTOL | PENDING_OWNER_SIGNOFF | Oracle G1.2 | rel>3% → REJECT | reltol 放宽到 0.20 → rc!=0 |
| FZ-AP2S-EPSF-RTOL | PENDING_OWNER_SIGNOFF | Oracle E1 | 超容差 → REJECT | 容差放宽到 1e-3 → rc!=0 |
| FZ-AP2S-REJ-CALIB-BINMIN | PENDING_OWNER_SIGNOFF | ALG-P2S-REJ.4 | 箱样本<50 → 该箱不参与判定但覆盖须登记 | binmin=5 → rc!=0 |
| FZ-AP2S-REJ-CALIB-ABS | PENDING_OWNER_SIGNOFF | ALG-P2S-REJ.4 | >0.10 → 概率不得作科学门，须回退显式阈值并登记 | 阈值放宽到 0.50 → rc!=0 |
| FZ-AP2S-REJ-BSS-MIN | PENDING_OWNER_SIGNOFF | ALG-P2S-REJ.4 | BSS≤0.10 → 概率不得作科学门 | bss_min=0 → rc!=0 |
| FZ-REJ-INHERITED-THRESH | FROZEN | ALG-P2S-REJ.5 | 改动任一继承阈值或 method=AUTO → invalid_configuration/invalid_method | 改动继承阈值 → rc!=0 |
| FZ-UPM-CONVERGENCE | FROZEN | ALG-P2S-UPM.4 | 改动收敛参数或 sigma_floor → 与既有合同不一致 | 改动 tol/σ_floor → rc!=0 |
| FZ-AP2PT-SNR-IDENT-RTOL | PENDING_OWNER_SIGNOFF | GATE-SNR-12 | 独立帧恒等式超容差 → REJECT | 忘平方（SNR=ΣSNR_k）→ rc!=0 |
| FZ-AP2PT-CORR-RATIO-MIN | PENDING_OWNER_SIGNOFF | GATE-CORR-04 | 比值≥1.05 → 拒绝朴素 Σ，必须联合 C_in | 阈值放宽到 1.50 → rc!=0 |
| CF-T-CONST-SB-TOL | PENDING_OWNER_SIGNOFF | FZ-GATE-CONST-SB | 改动容差或未覆盖全 pixfrac → REJECT | 容差改为 1e-1 → rc!=0 |
| FZ-AP1-DEFICIT-THRESH | PENDING_OWNER_SIGNOFF | FZ-GATE-PARENT-VAR | 未签字生效前该 variance 面不得声明精确；deficit 超阈 → 标 unavailable/给相关核 | 对角归约声明为精确 → rc!=0 |
| FZ-CAL-FLOOR | FROZEN | CAL-COV-FORMULA | f_p≤0 或非有限仍造值 → REJECT（不得静默 floor 造值） | 删除 floor 并除以 0 → rc!=0 |
| FZ-CAL-QUANTUM-DEFAULT | FROZEN | CAL-COV-FORMULA | 量化项未声明 q_adu → 缺省 1 ADU 但必须声明 | 量化方差用 q=0 → rc!=0 |
| FZ-AP1-GLS-QW-RTOL | FROZEN | FZ-FORMULA-WINFO | 超容差 → REJECT | Q/W 允许 1% 偏差 → rc!=0 |
| FZ-PROV-KCORR-VALUE | PENDING_OWNER_SIGNOFF | FZ-PROV-KCORR | 跨域外推/内插或忽略相关 → REJECT | k_corr 取 1.0 忽略相关 → rc!=0 |
| FZ-P3-OMEGA-NONCONST | FROZEN | G-P3-SB-01 | 常数 Omega 近似冒充逐像素面积元 → REJECT | 用常数 Omega 做 flux 换算 → rc!=0 |
| CF-T-P3-CORR-EPSILON | OPEN | G-P3-COV-01 | 数值未落地前该面 fail-closed：只出对角且无相关核/近似误差 → REJECT | 对角化过度乐观（声明 28.71% 低于真实）未检出 → rc!=0 |
| QF-G-INJ-01 | OPEN | G-INJ-01 | 未冻结前不得以本值冒充已冻结容差；executed=0/skip-only → rc=2 | 删除 owner → P0-06 rc!=0 |
| QF-G-INJ-02 | OPEN | G-INJ-02 | 未冻结前不得冒充已冻结容差 | 删除 owner → P0-06 rc!=0 |
| QF-G-INJ-03 | OPEN | G-INJ-03 | 未冻结前不得冒充已冻结容差 | 删除 owner → P0-06 rc!=0 |
| QF-G-INJ-07 | OPEN | G-INJ-07 | 未冻结前不得冒充已冻结容差 | 删除 owner → P0-06 rc!=0 |
| QF-G-RD-01 | OPEN | G-RD-01 | W10 预注册后冻结；本值不得冒充冻结 | 删除 owner → P0-06 rc!=0 |
| QF-G-RD-02 | OPEN | G-RD-02 | W10 预注册后冻结 | 删除 owner → P0-06 rc!=0 |
| QF-G-BASE-03 | OPEN | G-BASE-03 | 预注册前不得冒充冻结；不得声明 Fisher 最优 | 删除 owner → P0-06 rc!=0 |

## 2. 生产面硬禁止（缺省即红）

- 生产权重枚举出现 `psf_snr_power` / `auto` / `support_x_snr2` / legacy `0`（`FZ-MODE-DEFERRED` / `FZ-FIELD-WEIGHTMODE`）。
- 帧级 `median(SNR_F)` / `median_source_snr` / support / coverage / FWHM / residual 进权重来源或方差来源（`FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE`，C-004.2）。
- psfsw 产物出现 `ivar/variance/fisher/w_info/sigma` 等键（`FZ-GATE-PSFSW-COV`）。
- 由权重标量反推 variance 或 `C_out≠R C_in R^T`（`FZ-FORMULA-COV-PROP`）。
- Phase3 重采样输入 Q/W、W=ΣW_in、替换上游 W_info（`FZ-P3-QW-RECOMPUTE`）。

## 3. 零用例/skip-only 即红

任何门不得以零用例、skip-only 或同实现自证判 PASS（宪章 §14.2/§13.1；QA-MATRIX-001 ORACLE_AND_ZERO_CASE_POLICY）；本冻结合同的 Oracle 含正向控制与逐条负向 mutation。
