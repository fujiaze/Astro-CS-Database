> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# SCI-ADJ-001 冲突矩阵（冲突项 → 各方立场 → 控制器/本任务裁定 → 生效条款）

- 文档 ID：SCI-ADJ-001-CONFLICT-MATRIX
- 基线：HEAD = main = bc166e9d4828b45ef32b679156e12d954640534a
- 机器可读伴生：reports/v6/science-adjudication/adjudications.json（每条裁决的 id/topic/input/positions/ruling/clauses/effective_scope/implementation_owner/verification_gate/owner_signoff_required）
- 裁决编号：本表用 ADJ-*（与机器 JSON 一致）；生效条款用 SC-ADJ-*。

## 0. 口径与权威分层

1. 权威分层（宪章 §1.1）：FROZEN 宪章 > docs/owner/PROJECT_SPEC.md > 三份 docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md > docs/science/UNIFIED_SCIENCE_MODEL.md / PSF_SIGNAL_WEIGHT.md > 专项 SCI/ALG/DATA/API > 代码/测试 > 历史文档。
2. PROJECT_SPEC.md §11：现有 FROZEN SCI 与目标规范冲突时作为待迁移基线而非反证；本任务据此把与目标冲突的 FROZEN SCI 段登记为 superseded，正式取代由 CONTRACT-FREEZE-001(W4) 写入 docs/science/v6/frozen/，公式级变更须负责人签字（宪章 §1.2）。
3. 本任务只读输入：不改 docs/science/*.md、docs/owner/**、docs/design/**、docs/references/** 与生产代码；不 commit/push；不派生子代理。
4. 控制器裁决 C-004 全 6 条被遵守，本任务未推翻任何控制器裁决；F1 与 AR-033 只登记不裁决。

---

## 1. 观测模型与 Drizzle 冲突（F-OBS-01..05）

### 1.1 F-OBS-01 — DRIZZLE §3 符号表单位 vs §5/§7 推导

| 项 | 内容 |
|---|---|
| 冲突项 | docs/science/DRIZZLE.md §3（S,F,x: ADU；v,variance: ADU^2；ivar: ADU^-2）与 §5/§7（S_p=F_p/D_p=ADU/px^2；variance_p=Σv_jw²/D_p²=ADU^2/px^4；ivar=px^4/ADU^2）矛盾；同名 variance 指两个不同量纲对象 |
| 各方立场 | SCI-OBS-001：§3 与 §5 冲突（门 U2）；建议裁定术语并声明 §30.4 的 BUNIT² 幂次。AUDIT-REVIEW-001：AR-023 登记 DRIZZLE.md §5/§7/§11 自相矛盾。DATA_SEMANTICS §30.4：BUNIT=<signal BUNIT>^2，缺省 ADU，未显式承载 px 幂次 |
| 控制器/本任务裁定 | ADJ-F-OBS-01：冻结 pixel_variance_in(输入 v_j, ADU²) 与 sb_variance_out(输出 variance_p, ADU²/px⁴) 分离命名；V6 目标态以 §5 推导单位为准。写盘 BUNIT 必须量纲可判：(a) 显式含 px 幂次（canonical ADU/px^2 与 ADU^2/px^4）；或 (b) BUNIT=ADU 时 provenance 必须声明 pixel_semantics=surface_brightness+pixel_area_power=-2+目标像素面积。缺 (b) 的裸 ADU = 单位不可判 → unavailable/REJECT |
| 生效条款 | SC-ADJ-F01.1..4；冻结条目 FZ-UNIT-VAR-IN / FZ-UNIT-VAR-SB / FZ-UNIT-IVAR-SB / FZ-BUNIT-SEMANTICS |
| 实施/签字 | ALG-P1-001 / DATA-DESIGN-001 / IMPL-P1-DRZ-001 / IMPL-AIO-001；需负责人签字（FROZEN SCI 术语修正，SO-01） |

### 1.2 F-OBS-02 / S2 / AR-013 — pixfrac<1 常量面亮度不变量与面亮度归一因子

| 项 | 内容 |
|---|---|
| 冲突项 | 前向 Drizzle w_jp=a_jp/A_drop(A_drop=pixfrac²·A_pixel) 下常数面亮度场输出 S_p=B0/pixfrac²（Oracle D4：pf=0.8→1.5625、pf=0.5→4.0），故 DRIZZLE.md §7 的 S_p=B0 只在 pf=1 成立；DESIGN-P1 §9(S_p=Σ B_j a_jp/Σ a_jp, B_j=x_j/A_pixel,j) 则面亮度保持；UNIFIED §7 要求常量面亮度与总积分通量 Oracle 同时成立 |
| 各方立场 | 现行 DRIZZLE §5/§7 + 生产门：通量守恒 ΣF=Σx 成立，常量场门固定 pf=1（p1drz_tests_core.cpp:71）。DESIGN-P1 §9 / UNIFIED §7（TARGET_NORMATIVE）：面亮度保持归一是目标态。反向算子(reverse_drizzle.cpp:213-214)：pf<1 时保持面亮度，正向不保持（不对称）。AUDIT-REVIEW-001：AR-013 A_drop vs A_pixel 分叉必须重开；AR-023/AR-030 同源 |
| 控制器/本任务裁定 | ADJ-F-OBS-02：冻结选择 (B) 面亮度保持归一为目标态：①S_p=Σ_j B_j a_jp/Σ_j a_jp（B_j=x_j/A_pixel,j，等价 c_jp=a_jp/Σ a_jp），常量面亮度 S_p=B0 对全部 pixfrac∈(0,1] 成立；②通量守恒冻结为条件不变量：pf=1 严格 Σ_p F_p=Σ_j x_j，pf<1 时总输出通量 =pixfrac²·Σ_j x_j，该因子必须写入 provenance(flux_conservation_factor)并由 aperture/总通量换算显式使用；③失效域：pf<1 且按 w_jp=a_jp/A_drop 直接把 S_p 当绝对面亮度（缺 1/pixfrac² 补偿）→ 光度零点偏差 1/pixfrac²；④实施须重跑全部 Drizzle 不变量/variance 门并同步反向算子 |
| 生效条款 | SC-ADJ-F02.1..4、SC-ADJ-S2.1；冻结条目 FZ-FORMULA-DRIZZLE-SB / FZ-COND-FLUX-CONSERV / FZ-FORMULA-DRIZZLE-VAR |
| 实施/签字 | ALG-P1-001 / IMPL-P1-DRZ-001 / DATA-DESIGN-001；需负责人签字（FROZEN 公式变更，SO-02） |

### 1.3 F-OBS-03 — HiPS 父级方差只保留对角项，无误差门

| 项 | 内容 |
|---|---|
| 冲突项 | aio_hips_writer.cpp:668-671,1110-1126 用 variance_parent=Σ_p var_num_p/(Σ_p D_p)² 只留对角；精确 Var(S_parent)=Σ_{p,q}(D_pD_q/(ΣD)²)Cov(S_p,S_q)；门 D3 合成实测相对缺口 0.8225 且必为下界 |
| 各方立场 | PROJECT_SPEC §3：任何对角 covariance 假设必须有适用域与误差门。UNIFIED §7：只存对角 variance 必须另存 correlation kernel/scale。DRIZZLE §9a：只说不存完整矩阵，未声明父级低估也无门 |
| 控制器/本任务裁定 | ADJ-F-OBS-03：对角归约必须同时满足 ①声明为下界且不得用于 aperture/总量误差；②另存相关核 rho_ij/可重建算子摘要；③给出误差门度量 deficit=(exact-diag)/exact 与门（阈值由 ALG-P1-001 冻结）。不满足任一 → variance 面标 unavailable |
| 生效条款 | SC-ADJ-F03.1..3；FZ-GATE-PARENT-VAR |
| 实施/签字 | ALG-P1-001 / IMPL-P1-DRZ-001 / SCHEMA-INTEGRATE-001；数值阈值属 W3/W4（SO-07） |

### 1.4 F-OBS-04 — 共享系统项未进入 covariance 面

| 项 | 内容 |
|---|---|
| 冲突项 | 现状只有逐像素对角随机方差（module_adapters.cpp 的 ivar 求和），无低秩/共同 master 项；门 C4 合成 4 帧共享共同模式（幅度 0.6σ）实测方差低估 3.48× |
| 各方立场 | UNIFIED §6 / PHASE1 §4.2：共享项须低秩 covariance/相关核/共同 master ID+强度参数，不存完整巨矩阵。现状：留空 |
| 控制器/本任务裁定 | ADJ-F-OBS-04：冻结共享项三种允许表达 ①低秩因子 C_shared=L Lᵀ；②相关核 sigma/scale+kernel；③共同 master ID+强度参数 alpha_m；必须进 covariance 传播链；无法表示 → unavailable 或系统误差预算（进 validity/quality），禁止按独立随机项处理 |
| 生效条款 | SC-ADJ-F04.1..3；FZ-PROV-SHARED-SYSTEMATIC；与 ADJ-OBS-01 三项划分联动 |
| 实施/签字 | DATA-DESIGN-001 / ALG-P1-001 / IMPL-P1-CAL-001 / ALG-P2-SURF-001（数据面 SO-07） |

### 1.5 F-OBS-05 — k_corr 冻结值不可独立复现

| 项 | 内容 |
|---|---|
| 冲突项 | 冻结 k_corr=1.3883（pixfrac=0.8, N_retained≈251, N_eff≈181, UPMW-005 MC），保守取 1.4；独立平面 MC（门 D5）只复现方向（k_corr≈2.19），数值依赖生产球面几何/patch 口径 |
| 各方立场 | UNCERTAINTY §V19R3 / sampler.cpp:81-92：冻结值+按尺度查找表。SCI-OBS-001：机制方向被复现，数值不可独立复现，建议显式化口径与 provenance |
| 控制器/本任务裁定 | ADJ-F-OBS-05：冻结 ①定义 k_corr=Var(median)/[πσ_bg²/(2N_retained)]；②适用域（几何/pixfrac/patch/估计器/球面）显式；③标定脚本+固定种子 MC 纳入可复跑证据；④未复跑前只允许已声明域内取 1.4，禁止外推；⑤按尺度查找表保留并记 provenance |
| 生效条款 | SC-ADJ-F05.1..4；FZ-PROV-KCORR |
| 实施/签字 | ALG-P2-UPM-001 / IMPL-P2-UPM-001 / DATA-DESIGN-001（标定脚本 SO-07） |

### 1.6 ADJ-OBS-01 — d=A x+n 的随机/共享/系统三项划分（F-OBS-04 上游口径）

冻结三类量：独立随机项进对角 variance/ivar；共享系统项（共同 master、共同天空/背景、重采样相关）进 covariance 面（低秩/相关核/master ID+强度参数）；模型偏差（光度零点、PSF 模型误差、WCS、UPM 参数）进 validity/quality+系统误差预算+参数 covariance，禁止伪装随机 ivar。条款 SC-ADJ-OBS-01.1..3。

---

## 2. Phase2 三模式冲突（S1 / AR-024 / AR-028 / AR-029 / AR-027 / AR-003 / AR-037）

### 2.1 S1 / AR-028 / AR-024 / AR-029 — weight_mode 三面互斥（已按 §9.73 A44 作废：该概念不存在）

| 项 | 内容 |
|---|---|
| 冲突项 | CONTROL_WEIGHT_SNR.md:66,71：weight_mode=2 且 weights[s]=support[s]×snr_v²；DATA_SEMANTICS.md:987 / PUBLIC_API.md:1165：2=ivar；1=等权；0=support×snr²；ACR_EQUIVALENCE.md:33：weight_mode∈{auto,ivar,equal,support_x_snr2}；V6 目标：显式三模式 |（已按 §9.73 A44 作废：该概念不存在）
| 各方立场 | 三套互斥字面量（SCI-CW / DATA_SEMANTICS / V6）。UNIFIED §11 撤销 support×snr²；宪章 §6.3 禁 support/coverage 作权重 |
| 控制器/本任务裁定 | ADJ-S1 + ADJ-AR-01：冻结 生产科学模式 {point_information, surface_gls, psfsw_robust}；文档基线模式 {equal, pixel_ivar}（仅基线比较，非最优）；legacy 整数 {0=support×snr²,1=equal,2=ivar} 被取代且 0 不得进科学权重面。字段的 schema 词表归一由 W6（C-004.3），本任务只冻结语义与取值域；ACR 旧枚举标 ARCHIVED，不得进产品 schema 合法域 |
| 生效条款 | SC-ADJ-S1.1..4、SC-ADJ-AR01.1；FZ-MODE-PRODUCTION / FZ-MODE-BASELINE / FZ-FIELD-WEIGHTMODE |
| 实施/签字 | SCHEMA-INTEGRATE-001(W6) / DATA-DESIGN-001 / CONTRACT-FREEZE-001 |

### 2.2 point_information / surface_gls / psfsw_robust 裁定（AR-027 / AR-003 / AR-037）

| mode | 权威式（冻结） | 适用域与最优性 | 失效域/门 |
|---|---|---|---|
| point_information | Q_k=a_kP_kᵀC_k⁻¹d_k；W_info,k=a_k²P_kᵀC_k⁻¹P_k；F̂=Q/W；Var=1/W（独立帧 Q=ΣQ_k,W=ΣW_k） | 模型/C 门通过时 BLUE、最大点源 SNR、最小通量方差 | 白噪近似为条件式；跨帧相关须联合 C；仅 Drizzle 后逐像素 ivar 无法重建 W_info；相关帧简单求和必须被拒 |
| surface_gls | x̂=(AᵀC⁻¹A)⁻¹AᵀC⁻¹d；Cov=(AᵀC⁻¹A)⁻¹ | GLS 假设成立时 Gauss–Markov BLUE | 像素 ivar 仅条件近似（同点采样+噪声独立+a_k 一致），误差门度量 Var_approx/Var_GLS ≤ 1+ε（ε 由 ALG-P2-SURF-001 冻结）；必须报告 R̃C_inR̃ᵀ |
| psfsw_robust | Wt_k=C_norm·S^α·Conc^β/(N^γ·B^δ)；W_psfsw,k=Wt_k/median_j(Wt_j)（组内 median=1，无量纲） | 只能声明"在指定验收数据上优于指定基线"；不得声明 Fisher 最优 | 四分量分别落产品；最终 covariance 只能 C_out=R C_in Rᵀ，禁止 1/W_psfsw；effective PSF 必输；无共同星集/背景非正/星不足/选择偏差门失败 → unavailable，禁止回退 median source SNR |

条款：SC-ADJ-P201.1..3 / SC-ADJ-P202.1..3 / SC-ADJ-P203.1..6；冻结条目 FZ-FORMULA-WINFO / FZ-FORMULA-Q / FZ-FORMULA-FHAT / FZ-COND-WHITENOISE / FZ-FORMULA-GLS / FZ-GATE-PIXIVAR-APPROX / FZ-FORMULA-PSFSW-COMPOSITE / FZ-FIELD-PSFSW-4COMP / FZ-FIELD-PSFSW-UNIT / FZ-GATE-PSFSW-FAILCLOSED / FZ-GATE-PSFSW-COV / FZ-GATE-PSFSW-EPSF。

### 2.3 AR-003 / AR-037 — PSFSW 定位

| 项 | 内容 |
|---|---|
| 冲突项 | V5 曾把 PSFSW 定位为 QA-only；V4 R03 曾定 Phase2 默认 ivar |
| 各方立场 | UNIFIED §4.1 / PSF_SIGNAL_WEIGHT §1：PSFSW 双轨，psfsw_robust 是正式可选 conventional integration（非 QA-only）；V6 RULINGS #3 同向 |
| 控制器/本任务裁定 | ADJ-P2-03：psfsw_robust 是正式生产模式，与 Q/W、surface GLS 并列；但无量纲复合权重不得冒充 ivar/Fisher information；W_info 与 psfsw_robust_weight 分别落产品，唯一合法耦合是 covariance 传播与基线比较 |
| 生效条款 | SC-ADJ-P203.1..6；AR-003 superseded，V4 R03 默认 ivar superseded |

---

## 3. Phase3 与采样核冲突（S4 / AR-017 / F3-01..06）

### 3.1 S4 / AR-017 — Phase3 采样核归属

| 项 | 内容 |
|---|---|
| 冲突项 | V4 R09「保持四象限最近中心双线性核」 vs DESIGN-P3 §3「当前四象限双线性只能在独立 Oracle 和误差/边界定义后作为一个注册核；采样核是产品语义，不能由现码倒推为永恒目标」 |
| 各方立场 | 历史裁决（保持冻结核） vs TARGET_NORMATIVE（注册核+Oracle） |
| 控制器/本任务裁定 | ADJ-S4：采样核由版本化 kernel registry 注册；现行核只能作为 bilinear_4quad 且须先过独立 Oracle+通量/面亮度语义+误差/边界定义；nearest 仅 mask/诊断/显式选择；高阶核各自注册带 Oracle。AR-017 superseded |
| 生效条款 | SC-ADJ-S4.1..4；FZ-P3-KERNEL-REGISTRY |

### 3.2 F3-01..F3-06 裁决

| finding | 内容 | 裁决 |
|---|---|---|
| F3-01 | 只输出对角 variance 时须给相关核/近似误差；Phase3 与代码只出对角 | ADJ-P3-01/ADJ-AR-02：相关核/算子摘要为强制输出；只出对角无相关核 → 拒绝 |
| F3-02 | Σc²u 适用域"输入 covariance 对角"未声明；Drizzle 输入 mean|ρ|≈0.19 下低估 23.3% | ADJ-P3-01：适用域必须显式声明；非对角输入须用完整/带核 C_y，对角化过度乐观须被检出 |
| F3-03 | Phase3 无逐像素立体角/面积元，每像素通量无合同对象 | ADJ-P3-01：point_source_flux 必须逐像素 Ω_i 与光度尺度 a；surface_brightness 做 flux 换算须显式 Ω（归 DATA-DESIGN-001 设计面） |
| F3-04 | point_source_flux/visualization/effective PSF/Q-W 传播 NOT_IMPLEMENTED | ADJ-P3-01：三模式+Q/W 输出帧重算冻结；实现归 IMPL-P3-RSMP-001/P3-INTEGRATE-001（W5/W7） |
| F3-05 | FROZEN SCI-P3-001 §1/§9a-10 仍含"variance 输入拒绝 / SIN·CAR 拒绝"旧字样；投影未同步宪章 §18.1 | 本任务登记：不得改冻结正文，须走正式 amendment（归 CONTRACT-FREEZE-001/DOC-CONVERGE-001，SO-06） |
| F3-06 | p3_resample.{cpp,h} 命中 F1 工作树回退（科学行与 HEAD 一致） | 只登记（控制器级 F1，见 §6） |

---

## 4. 通用冻结与跨阶段冲突（AR-023 / AR-030 / AR-031 / AR-012）

| 冲突项 | 各方立场 | 裁定 | 生效条款 |
|---|---|---|---|
| S3 / AR-023 常量场 Oracle：DRIZZLE §11 S_p=C vs §5/§7 S_p=B0，且 §5 明禁把每像素常量 ADU 与常量面亮度混同 | §11 自相矛盾于 §5/§7 | ADJ-S3：Oracle 真值必须按面亮度 B0 构造（x_j=B0·A_pixel_j）；在 (B) 归一下门 S_p=B0 对全 pixfrac，容差沿用 |S_p/B0-1|<1e-3（不改数值）；负向门：常量 ADU 构造、无条件 pixfrac、S_p=F_p 三错法必红 | SC-ADJ-S3.1..3；FZ-GATE-CONST-SB（需签字 SO-03） |
| AR-030/AR-031 协方差产品：DRIZZLE §1/§9a"协方差产品为非目标" vs UNIFIED §7 强制相关核 | 非目标 vs 强制输出 | ADJ-AR-02：协方差/相关核是强制输出面；只出对角须给核/算子摘要+近似误差；coverage 不得代替 variance。UNCERTAINTY 的"不保存完整矩阵"本身不违规（差距是 AR-031 而非文档错误） | SC-ADJ-AR02.1..2（需签字 SO-04） |
| AR-012 support/coverage/quality 与 ivar 分离 | 宪章 §6.3 / UNIFIED §3 逐字一致 | 保留 valid：support/coverage 只作门，不得作 inverse-variance/SNR/科学权重 | FZ-GATE-SUPPORT-COVERAGE；SC-ADJ-GEN02.2 |
| AR-010 / C-004.2 帧级 median(SNR_F) | HEAD 有 snr_frame_coefficient.*；PROJECT_SPEC §4 只许诊断 | ADJ-C004-02：只作诊断/深度表达，禁入权重面；诊断别名集命中即 REJECT | SC-ADJ-C00402.1..2；FZ-GATE-MEDIAN-SNR |
| 通用单位表 | DRIZZLE §3/§5 冲突 + PSF_SIGNAL_WEIGHT §2 + DATA_SEMANTICS §30.4 | ADJ-GEN-01：冻结 signal=ADU/px²、sb variance=ADU²/px⁴、sb ivar=px⁴/ADU²、W_info=ADU⁻²、psfsw=1、Q=ADU⁻¹、flux=ADU；Phase3 variance BUNIT=(signal BUNIT)² | SC-ADJ-GEN01.1..3；units_table + FZ-UNIT-*/FZ-P3-BUNIT-QUADRATIC |
| 通用字段命名 | 宪章 §4.1 / UNIFIED §3 / WEIGHT_PROVENANCE_GATE R3/R5 | ADJ-GEN-02：冻结规范字段名与禁止无前缀 weight/snr；psfsw 产物任何层禁 ivar/variance/fisher/w_info 键；schema 键名归 W6 | SC-ADJ-GEN02.1..3；FZ-FIELD-* |
| provenance | 宪章 §4.3 / UNIFIED §9 / DATA_SEMANTICS §30.3 | ADJ-GEN-03：冻结 provenance 最小集（含单位+pixel_area_power、归一版本、相关核摘要、flux_conservation_factor、k_corr）；unavailable 必须显式登记 | SC-ADJ-GEN03.1..3；FZ-PROV-MINIMAL-SET |
| 降级路径 | UNIFIED §8 / PHASE1 §8.3 / PROJECT_SPEC §4 / PSF_SIGNAL_WEIGHT §6 | ADJ-GEN-04：空间量默认空间模型；帧级标量须双过空间残差/趋势门与功率损失门，带 p05/p50/p95+最大系统偏差+采样覆盖+模型误差+适用域；否则 map/model/control points；IPV 配置面维持冻结 | SC-ADJ-GEN04.1..4；FZ-DEGRADE-SCALAR |

---

## 5. 覆盖缺口登记（AR-032/AR-034/AR-035/AR-051，只登记与建议）

| 缺口 | 事实 | 本任务处置 |
|---|---|---|
| AR-032 | 非 v6 docs/science/*.md（DRIZZLE/CONTROL_WEIGHT_SNR/ACR_EQUIVALENCE/INTEGRATION）在 W1–W8 无 owner；唯一 docs/ owner 是 DOC-CONVERGE-001(wave12) | 本表逐条给出取代关系；正式取代清单由 CONTRACT-FREEZE-001 产出，文档收口归 DOC-CONVERGE-001（SO-06） |
| AR-034 | 7 项历史 CI 红（含 DOC-INDEX-STRICT 注册零命中）无 V6 逐名验收锚 | 建议 RUNTIME-CI-001 验收逐名登记终态；处置权在控制器 |
| AR-035 | 785 缺陷账本无销账任务 | 建议 QA-MATRIX-001/FINAL-AUDIT-001 增列 P0 门族；处置权在控制器 |
| AR-051 | 账本 785 是合并层口径，叶子 L-id 结构性不可表达 | 所有账本数字强制带"合并层"口径号 |

---

## 6. 控制器级事项：只登记不裁决

| 事项 | 事实 | 本任务处置 |
|---|---|---|
| F1 基线分歧 | 工作树相对 HEAD：16 tracked 回退 + 10 tracked 删除，未裁决（C-004.6：W5 派发前必须裁定） | 只登记（CTRL-F1）。本任务全部输入公式锚点在 W1 双态核验一致，科学结论不依赖 F1；本任务一切生产面描述以 HEAD=bc166e9d 为准 |
| AR-033 根构建面 owner | 根 CMakeLists.txt、tests/unit/CMakeLists.txt、tests/unit/p1snr/CMakeLists.txt 无 V6 owner；W5 测试无法注册 | 只登记（CTRL-AR033）。遵守 C-004.4：W5 各 IMPL 只在自身 write_scope 注册；根构建面由控制器 W5 后独立集成提交（归 RUNTIME-CI-001/W9） |
| AR-036/AR-019/AR-026 | 宪章 §10.5/§17.6「记录 vs 自动判决」需负责人签字；V6 无承载 | 只登记（CTRL-AR036）+ 签字项 SO-05；不代裁 |

---

## 7. 与控制器 C-004 的衔接（本任务不推翻任何一条）

| C-004 条 | 内容 | 本任务落实 |
|---|---|---|
| C-004.1 | psf_snr_power 本包不解冻 | ADJ-C004-01：保持 DEFERRED/NOT_IMPLEMENTED；生产模式列表出现即 REJECT；FZ-MODE-DEFERRED |
| C-004.2 | 帧级 median(SNR_F) 仅诊断 | ADJ-C004-02：FZ-GATE-MEDIAN-SNR；权重来源诊断别名集命中即 REJECT |
| C-004.3 | schema 词表由 W6 归一 | ADJ-C004-03：只给语义冻结与双向 token 映射建议；不写 schema/不改 contracts |
| C-004.4 | 构建面 owner 缺口（AR-033） | 只登记（§6） |
| C-004.5 | 上位规范写保护（Wave1–4 只许写 v6/**） | 本任务写域仅 docs/science/v6/adjudication/+reports/v6/science-adjudication/+run/v6/adjudication/ |
| C-004.6 | F1 基线分歧 | 只登记（§6） |

---

## 8. 需负责人签字项（本任务无权签署）

| ID | 事项 | 理由 | 权威 |
|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | FROZEN SCI 术语修正 | 宪章 §1.2 |
| SO-02 | F-OBS-02/S2 面亮度归一改为 (B) 面亮度保持 | FROZEN 公式变更 + 重跑全部 Drizzle 门 | 宪章 §1.2 |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | FROZEN 判据取代 | 宪章 §1.2 |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | FROZEN DRIZZLE §1/§9a 取代 | 宪章 §1.2 |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | 宪章修订须负责人签字 | 宪章 §1.2/§18 |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | 跨文档权威裁定 | PROJECT_SPEC §11 |
| SO-07 | F-OBS-03/04/05 的数值阈值/数据面/标定脚本 | 误差门数值、低秩数据面、k_corr 标定须 W3/W4 冻结并由负责人确认 | SCI 冻结流程 |

---

## 9. 未决风险（如实登记）

1. F1 未裁决：本任务科学冻结以 HEAD 为准；若最终基线取工作树，p3_resample/module_adapters.cpp 等回退态会改变"现状"陈述，但不改变靶向冻结（W1 双态核验科学行一致）。
2. S2/(B) 归一变更的实施面：须重跑全部 Drizzle 不变量与 variance 门，且与反向算子/Phase2 消费侧同步；实施风险归 ALG-P1-001/IMPL-P1-DRZ-001。
3. 数值阈值缺口：F-OBS-03 的 deficit 阈值、F-OBS-04 的低秩/相关核数据面、F-OBS-05 的标定脚本、surface_gls 的 ε 均为本任务冻结度量与门存在性，数值由 W3/W4 冻结（不得由实现自行发明）。
4. schema 词表未归一前：两套词表并存（W6 归一）；本任务只给语义与映射建议。
5. 真实数据验证不在本任务范围（Wave 10 REAL-SCIENCE-001）。
