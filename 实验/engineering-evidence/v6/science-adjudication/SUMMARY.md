# SCI-ADJ-001 科学裁决整合 — 机器可读裁决摘要（人读索引）

- 任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/SCI-ADJ-001.md`
- 基线：`HEAD = main = bc166e9d4828b45ef32b679156e12d954640534a`
- 性质：integration_adjudication_only；本文件给出语义冻结；schema 词表归一归 SCHEMA-INTEGRATE-001(W6)，正式冻结归 CONTRACT-FREEZE-001(W4)；本任务不写 schema/代码/生产实现。
- 机器可读源：`reports/v6/science-adjudication/adjudications.json`（schema `astrocs.v6.sci-adjudication/v1`）
- 本轮实测：`all_ok=True`，建议状态 `PASS`

## 1. 裁决清单（24 条）

| id | 主题 | 裁定（摘要） | 生效范围 | 实施 owner | 需签字 |
|---|---|---|---|---|---|
| `ADJ-OBS-01` | 统一观测模型 d=A x+n 的随机/共享/系统三项划分 | 冻结三类划分：①独立随机项进对角 variance/ivar；②共享系统项(共同 master、共同天空/背景、重采样相关)必须进入 covariance 面，表示为低秩因子/相关核/共同 master ID+强度参数，不得继续留空也不得存完整巨矩阵；③模型偏差(光度零点、PSF 模型误差、WCS、UPM 参数)进 validity/quality+系统误差… | Phase1/Phase2/Phase3 全部 covariance 产品与 manifest；数据面表示由 DATA-DESIGN-001(W3) 设计 | DATA-DESIGN-001、ALG-P1-001、ALG-P2-SURF-001 | 否 |
| `ADJ-F-OBS-01` | DRIZZLE 单位冲突：§3 符号表 ADU/ADU^2 vs §5 推导 ADU/px^2、ADU^2/px^4，以及写盘 BUNIT 语义 | 冻结术语与单位：输入逐像素方差改名 pixel_variance_in (符号 v_j, 单位 ADU^2)，输出面亮度方差改名 sb_variance_out (符号 variance_p, 单位 ADU^2/px^4)，输出 ivar 为 px^4/ADU^2；DRIZZLE §3 符号表中把二者同名写作 variance 是历史错误，V6 目标态以 §… | Phase1 Drizzle 输出 signal/variance/ivar + HiPS BUNIT + Phase2/Phase3 消费侧单位声明 | ALG-P1-001、DATA-DESIGN-001、IMPL-P1-DRZ-001、IMPL-AIO-001 | 是 |
| `ADJ-F-OBS-02` | pixfrac<1 时常量面亮度不变量失效：冻结归一选择与失效域 (同时是 S2 / AR-013 面亮度归一因子 A_drop vs A_pixel) | 冻结选择 (B) 面亮度保持归一作为 V6 目标态：(1) 目标信号层采用 S_p=sum_j B_j a_jp/sum_j a_jp (B_j=x_j/A_pixel,j)，等价的组合系数 c_jp=a_jp/sum_j a_jp；常量面亮度不变量 S_p=B0 对全部 pixfrac in (0,1] 成立。 | Phase1 Drizzle 前向/反向算子、HiPS 面亮度产品、所有面亮度/通量守恒 Oracle | ALG-P1-001、IMPL-P1-DRZ-001、DATA-DESIGN-001 | 是 |
| `ADJ-F-OBS-03` | HiPS 父级 tile 方差对角近似：误差门与相关核要求 | 冻结：HiPS 父级 tile 方差若用仅对角归约，必须同时满足三项——①产品/manifest 显式声明该值是下界(lower bound)且不得用于 aperture/总量误差；②随产品另存相关核 rho_ij 或可重建算子摘要；③给出误差门度量与门：以父级精确二次型 Var(S_parent)=sum_{p,q}(D_p D_q/(sum D)^2)C… | Phase1 HiPS 父级 variance 产品与任何下游 aperture/总量误差使用 | ALG-P1-001、IMPL-P1-DRZ-001、SCHEMA-INTEGRATE-001 | 否 |
| `ADJ-F-OBS-04` | 共享系统项必须进入 covariance 面的表达(低秩/共同 master ID/强度参数) | 冻结共享项的合同表达为三种允许形式之一：(a) 低秩因子 L: C_shared = L L^T；(b) 相关核 sigma/scale + kernel；(c) 共同 master ID + 强度参数 alpha_m。 | Phase1 校准 covariance、Phase2 联合 C、Phase3 输入 C_in | DATA-DESIGN-001、ALG-P1-001、IMPL-P1-CAL-001、ALG-P2-SURF-001 | 否 |
| `ADJ-F-OBS-05` | k_corr 冻结值的可复现口径与文档注记 | 冻结 k_corr 的可复现口径与注记要求：①定义冻结 k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]；②适用域必须显式(几何/pixfrac/control patch 尺寸/稳健估计器版本/是否球面 Drizzle 输出)；③标定脚本与固定种子 MC 必须纳入可复跑证据(替换仅留档的 UPMW-00… | Phase2 UPM control_variance/control_ivar (ALG-UPM-CONTROL-IVAR-001) 与其 provenance | ALG-P2-UPM-001、IMPL-P2-UPM-001、DATA-DESIGN-001 | 否 |
| `ADJ-P2-01` | Phase2 point_information：Q/W 权威式与最优性前提 | 冻结 Q=a P^T C^-1 d、W=a^2 P^T C^-1 P、F_hat=Q/W、Var=1/W 为 point_information 的唯一权威式；白噪声近似 W=a^2/(sigma_pix^2 A_NEA) 为条件式，仅在 C 对角且 sigma_pix 声明时可用。 | Phase2 point_source 产品族与 Phase1 W_psf 输出 | ALG-P2-POINT-001、IMPL-P1-PSFW-001、P2-INTEGRATE-001 | 否 |
| `ADJ-P2-02` | Phase2 surface_gls：A^T C^-1 A 权威式，像素 ivar 仅条件近似及其误差门 | 冻结 A^T C^-1 A 正规方程与 Cov=(A^T C^-1 A)^-1 为 surface_gls 唯一权威式。 | Phase2 surface_brightness 产品族 | ALG-P2-SURF-001、P2-INTEGRATE-001 | 否 |
| `ADJ-P2-03` | Phase2 psfsw_robust：四分量+组内归一+无量纲；covariance 只能 C_out=R C_in R^T；effective PSF 必输 | 冻结：①四分量 signal/concentration/noise/background 必须分别落产品，各带 measurement_id、p05/p50/p95 与有效覆盖；②复合 Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta)，W_psfsw,k=Wt_k/median_j(Wt_j)，组内 media… | Phase2 psfsw_integration 产品族与 Phase1 psfsw 分量输出 | ALG-P2-PSFSW-001、IMPL-P1-PSFW-001、P2-INTEGRATE-001 | 否 |
| `ADJ-S1` | weight_mode 三面互斥：字段/枚举语义冻结 (SCI-CW 2=support x snr^2 vs DATA_SEMANTICS 2=ivar vs V6 三模式) | 冻结 weight_mode 语义面为三面互斥的显式枚举：生产科学模式 = {point_information, surface_gls, psfsw_robust}；文档基线模式 = {equal, pixel_ivar}(仅基线比较，非科学最优)；legacy 整数 {0=support x snr^2, 1=equal, 2=ivar} 一律被取代且… | Phase2 配置/合同/schema 的 weight_mode 语义；schema 实现归 W6 | SCHEMA-INTEGRATE-001、DATA-DESIGN-001、CONTRACT-FREEZE-001 | 否 |
| `ADJ-S3` | 常量场 Oracle 判据：构造与负向门 (DRIZZLE §11 S_p=C vs §5/§7 S_p=B0) | 冻结常量场 Oracle 判据：①真值构造必须按面亮度 B0(源像素通量 x_j=B0*A_pixel_j)；②在冻结的 (B) 面亮度保持归一(SC-ADJ-F02.1)下门为 S_p=B0 对全部 pixfrac in (0,1] 成立，容差沿用现行常量场门 |S_p/B0-1|<1e-3(p1drz_oracle.hpp:124-127)，本任务不改容… | Drizzle 常量场/积分通量/variance/correlation 验收门真值定义 | ALG-P1-001、IMPL-P1-DRZ-001、QA-MATRIX-001 | 是 |
| `ADJ-S4` | Phase3 采样核归属与高阶核注册 (V4 R09 保持四象限双线性 vs DESIGN-P3 §3 须先有独立 Oracle 的注册核) | 冻结采样核归属与注册规则：①采样核是产品语义，由版本化 kernel registry 注册，不由现有代码倒推冻结；②现行四象限最近中心双线性只能作为注册核 bilinear_4quad，且注册前必须有独立 Oracle + 通量/面亮度语义声明 + 误差/边界定义；③nearest 仅用于离散 mask/诊断或显式用户选择；④高阶核(如三次/lanczos… | Phase3 重采样/采样核与 kernel registry | ALG-P3-001、IMPL-P3-RSMP-001、P3-INTEGRATE-001 | 否 |
| `ADJ-P3-01` | Phase3 三模式适用域、fail-closed 条件与 Q/W 输出帧重算 (F3-01..04) | 冻结 Phase3 三模式：①surface_brightness(行归一 R，Sum_j R_ij=1，BUNIT 面亮度)；②point_source_flux(Q/W/flux/detection，必须逐像素立体角 Omega_i、光度尺度 a、PSF/effective PSF，缺则 fail-closed，禁止静默退化)；③visualizatio… | Phase3 投影/采样/不确定度/PSF/Q-W 产品与 manifest | ALG-P3-001、IMPL-P3-RSMP-001、P3-INTEGRATE-001、IMPL-P3-PROJ-001 | 否 |
| `ADJ-GEN-01` | 通用单位表冻结 (signal/variance/ivar/W_info[ADU^-2]/psfsw[1]) | 冻结单位表(见 freeze_table FZ-UNIT-*):signal(SB 输出)=ADU/px^2；pixel_variance_in v_j=ADU^2；sb_variance_out variance_p=ADU^2/px^4；sb_ivar_out=px^4/ADU^2；Q=ADU^-1；flux/F_hat=ADU；W_info=ADU^-… | 全部 Phase 产品 BUNIT/单位声明与合同 | DATA-DESIGN-001、SCHEMA-INTEGRATE-001、CONTRACT-FREEZE-001 | 否 |
| `ADJ-GEN-02` | 字段命名冻结 (量不混名；无前缀 weight/snr 禁用) | 冻结规范字段名：point_source_information(或 W_info)、psfsw_robust_weight、psfsw.signal、psfsw.concentration、psfsw.noise、psfsw.background、depth_m5、source_snr、support、coverage、validity、rejection… | 全部产品/合同/schema 字段命名 | SCHEMA-INTEGRATE-001、DATA-DESIGN-001、CONTRACT-FREEZE-001 | 否 |
| `ADJ-GEN-03` | provenance 要求冻结 | 冻结 provenance 最小集：产品类型/schema 版本、软件完整 SHA、run ID、输入产品哈希、科学配置哈希、单位(BUNIT 语义与 pixel_area_power)、坐标 frame、像素/采样语义、算法 ID、module/provider、近似与降级(含 unavailable 原因)、归一/权重版本(psfsw 归一常数版本、we… | 全部 Phase 产品 manifest/properties | DATA-DESIGN-001、IMPL-AIO-001、SCHEMA-INTEGRATE-001 | 否 |
| `ADJ-GEN-04` | 降级路径冻结：帧级标量必须过功率损失门并带 p05/p50/p95 | 冻结降级路径：①空间量(W_info/背景/variance/photometric response/PSF)默认空间模型；②压成帧级标量必须同时过(a)空间残差/趋势门与(b)功率损失门(对最终 flux bias/variance/detection power 的损失低于阈值)，阈值由 ALG 冻结；③标量摘要必须带 p05/p50/p95、最大系统… | Phase1/Phase2 空间量标量降级与 manifest | ALG-P1-001、ALG-P2-PSFSW-001、IMPL-P1-PSFW-001、CONTRACT-FREEZE-001 | 否 |
| `ADJ-C004-01` | psf_snr_power 本包不解冻 (控制器 C-004.1) | 遵守 C-004.1(不得推翻)：psf_snr_power 在本包(=V6)保持 DEFERRED/NOT_IMPLEMENTED，不得进入生产 weight_mode 枚举与路由；其模式登记、归一常数版本化与只声明优于指定基线、不得等同 Fisher 最优的条件由 W3/W4 写清但不放行。 | Phase2 生产模式面/schema/配置 | ALG-P2-PSFSW-001、CONTRACT-FREEZE-001、SCHEMA-INTEGRATE-001 | 否 |
| `ADJ-C004-02` | 帧级 median(SNR_F) 仅诊断 (控制器 C-004.2) | 遵守 C-004.2(不得推翻)：median(SNR_F)/median_source_snr 冻结为诊断/深度表达，禁止作为 weight_mode 权重来源、禁止冒充 PSFSW/W_info/ivar；权重来源面的诊断别名集 {median_source_snr, median_snr, source_snr_median, support, cov… | Phase1/Phase2 权重来源面与诊断面分离 | CONTRACT-FREEZE-001、P1-INTEGRATE-001、RUNTIME-CI-001 | 否 |
| `ADJ-C004-03` | schema 词表由 W6 归一；本任务只给语义冻结 (控制器 C-004.3) | 遵守 C-004.3：两套词表由 SCHEMA-INTEGRATE-001(W6) 归一为单一 schema；W3 算法规格必须引用 W6 词表，不得各自发明。 | Phase2 权重 schema 词表 | SCHEMA-INTEGRATE-001、ALG-P2-PSFSW-001、ALG-P2-POINT-001 | 否 |
| `ADJ-CTRL-01` | 控制器级事项只登记不裁决：F1 基线分歧 + AR-033 根构建面 owner | 本任务只登记、不裁决以下控制器级事项：F1(工作树不等于 HEAD 的 16 回退+10 删除)、AR-033(根 CMakeLists.txt/tests/unit/CMakeLists.txt/p1snr CMakeLists 无 V6 owner)、AR-034(7 项历史 CI 红)、AR-035(785 账本销账)、AR-036(宪章 §10.5/… | 控制包治理面；不影响本任务科学冻结的生效 | 控制器 | 是 |
| `ADJ-AR-01` | AR-029 ACR_EQUIVALENCE 权重枚举陈旧 — 语义面裁定 (与 S1 同源) | 冻结 ACR_EQUIVALENCE 的 weight_mode 枚举不得作为生产权威：生产权重枚举以 ADJ-S1 为唯一来源；ACR 等价/对比门若保留其旧枚举，必须显式标注为 ARCHIVED/HISTORICAL 且不得出现在产品配置 schema 的合法取值域。 | ACR 等价文档与 Phase2 权重枚举权威 | CONTRACT-FREEZE-001、DOC-CONVERGE-001 | 否 |
| `ADJ-AR-02` | AR-030/AR-031 covariance 产品：DRIZZLE 非目标声明与相关核入产品 | 冻结：协方差/相关核是强制输出面(非目标声明被取代)。 | Phase1/Phase2/Phase3 covariance 产品与相关核表达 | ALG-P1-001、IMPL-P1-DRZ-001、SCHEMA-INTEGRATE-001、ALG-P3-001 | 是 |
| `ADJ-AR-03` | AR-032/AR-034/AR-035 覆盖缺口登记：非 v6 SCI 迁移 / 7 项 CI 红 / 785 账本销账 | 登记(不裁决处置)：①非 v6 SCI(DRIZZLE/CONTROL_WEIGHT_SNR/ACR_EQUIVALENCE/INTEGRATION)与目标冲突段由本任务的 conflict matrix 逐条给出取代关系，正式取代清单由 CONTRACT-FREEZE-001 产出，文档收口归 DOC-CONVERGE-001；②7 项历史 CI 红须在 … | 控制包覆盖缺口登记；不影响本任务科学冻结 | CONTRACT-FREEZE-001、RUNTIME-CI-001、DOC-CONVERGE-001 | 否 |

## 2. 冻结表（42 条，按类别）

| kind | 条数 |
|---|---|
| degradation | 1 |
| domain | 2 |
| field | 4 |
| formula | 9 |
| gate | 10 |
| mode | 4 |
| provenance | 3 |
| unit | 9 |

完整逐条冻结清单见 `docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md`；结构完整性强制项 required_freeze_ids 共 19 条，缺失即红。

## 3. mode 冻结

| mode | 状态 | 单位 | covariance 来源 | effective PSF | 组内归一 |
|---|---|---|---|---|---|
| `point_information` | PRODUCTION_FROZEN | ADU^-2 | combination_coefficients | 必输 | 否 |
| `surface_gls` | PRODUCTION_FROZEN | 1/(surface_brightness^2) | combination_coefficients | 必输 | 否 |
| `psfsw_robust` | PRODUCTION_FROZEN | 1 | combination_coefficients | 必输 | 是 |
| `equal` | DOCUMENTED_BASELINE | 1 | combination_coefficients | 必输 | 否 |
| `pixel_ivar` | DOCUMENTED_BASELINE | 1/BUNIT^2 | combination_coefficients | 必输 | 否 |
| `psf_snr_power` | DEFERRED_NOT_PRODUCTION | 1 | combination_coefficients | 必输 | 是 |

## 4. 验证与负向门（实测）

| 步骤 | 期望 | 实测 rc | 结论 |
|---|---|---|---|
| render-freeze-list | zero | 0 | OK |
| render-summary | zero | 0 | OK |
| check-adjudication | zero | 0 | OK |
| doc-consistency | zero | 0 | OK |
| doc-consistency-selftest | zero | 0 | OK |
| scope-check | zero | 0 | OK |
| mutate-and-check | zero | 0 | OK |
| oracle-baseline | zero | 0 | OK |
| oracle-mutate-legacy_is_sb_preserving | nonzero | 1 | OK |
| oracle-mutate-flux_unconditional | nonzero | 1 | OK |
| oracle-mutate-pixel_ivar_is_gls | nonzero | 1 | OK |
| oracle-mutate-psfsw_variance_from_weight | nonzero | 1 | OK |

- 结构一致性检查：`PASS`，21/21（枚举合法 / 单位表一致 / 禁止项缺省即红 / required freeze 缺失即红 / psfsw 边界 / C-004 消费）
- 冻结表负向 mutation：18/18 CAUGHT（删除冻结、改单位、psfsw 写 ivar/权重来源、median SNR 入权重、psf_snr_power 进生产等）
- 独立 Oracle（numpy，不调用生产实现）：`PASS`，5/5；4 项 Oracle 自我 mutation 全部 rc!=0
- 人读正文与机器表交叉引用：doc-consistency rc=0；注入 ADJ-FAKE-999 selftest CAUGHT

复现（单一 rc）：`python3 run/v6/adjudication/tools/run_all.py`

## 5. 需负责人签字项（本任务无权签署，7 项）

| ID | 事项 | 理由 | authority |
|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | FROZEN SCI 术语修正 | 宪章 §1.2 |
| SO-02 | F-OBS-02 / S2 面亮度归一改为 (B) 面亮度保持 | FROZEN DRIZZLE.md §5/§7/§11 公式变更 + 重跑全部 Drizzle 不变量/variance 门 | 宪章 §1.2 |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | FROZEN 判据取代登记 | 宪章 §1.2 |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | FROZEN DRIZZLE §1/§9a 取代登记 | 宪章 §1.2 |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | 宪章修订须负责人签字；不得由 Agent 放宽 | 宪章 §1.2/§18 |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | 跨文档权威裁定与 DOC-CONVERGE 收口 | PROJECT_SPEC §11 |
| SO-07 | F-OBS-03/F-OBS-04/F-OBS-05 的数值阈值/数据面/标定 | 误差门数值、低秩数据面、k_corr 标定脚本须 W3/W4 冻结并由负责人确认 | CSI 冻结流程 |

## 6. 控制器级事项（只登记不裁决）

| ID | 事项 | 裁决 owner | 引用 |
|---|---|---|---|
| CTRL-F1 | F1 工作树不等于 HEAD (16 回退 + 10 删除) | 控制器 | C-004.6 |
| CTRL-AR033 | 根构建面 (CMakeLists) 无 V6 owner | 控制器 | C-004.4 |
| CTRL-AR034 | 7 项历史 CI 红 | 控制器/DOC-CONVERGE-001/RUNTIME-CI-001 | AR-034 |
| CTRL-AR035 | 785 缺陷账本无销账任务 | 控制器 | AR-035 |
| CTRL-AR036 | 宪章修订签字项 | 项目负责人 | AR-036 |

## 7. 与控制器 C-004 的衔接

- C-004 全部 6 条裁决被遵守，本任务未推翻任何控制器裁决
- psf_snr_power 保持 DEFERRED (C-004.1)
- median(SNR_F)/median_source_snr 保持诊断 (C-004.2)
- schema 词表归 W6，本任务只给语义 (C-004.3)

## 8. 声明

- 本任务只写 `docs/science/v6/adjudication/`、`reports/v6/science-adjudication/`、`run/v6/adjudication/`（run/* gitignore）。
- 未 commit/push/git add；未建分支/worktree；未 stash/reset/clean/rebase；未派生子代理；未宣布发布。
- 未改动任何 SCI 公式、容差或冻结门；F1 与 AR-033 只登记不裁决。
