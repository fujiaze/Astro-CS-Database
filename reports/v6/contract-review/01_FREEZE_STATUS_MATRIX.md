> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 1. 三类状态统计（逐条见机器表 `clauses`）

| 状态 | 条数 |
|---|---|
| FROZEN（本任务冻结，可实施） | 39 |
| PENDING_OWNER_SIGNOFF（值唯一但待负责人签字） | 49 |
| OPEN（未落定，登记 owner） | 8 |
| 合计条款 | 96 |

## 2. 待签字条款（PENDING_OWNER_SIGNOFF）

| 条款 id | 主题 | 值 | 签字 | 原因 |
|---|---|---|---|---|
| FZ-UNIT-VAR-IN | pixel_variance_in v_j | ADU^2 | SO-01 | F-OBS-01：DRIZZLE §3 把输入 v_j(ADU^2) 与输出 variance_p(ADU^2/px^4) 同名写作 variance；术语/单位修正须负责人签字（宪章 §1.2） |
| FZ-UNIT-VAR-SB | sb_variance_out variance_p | ADU^2/px^4 | SO-01 | 同上（输出面亮度方差命名/单位修正） |
| FZ-UNIT-IVAR-SB | sb_ivar_out | px^4/ADU^2 | SO-01 | 同上（输出 ivar 单位 px^4/ADU^2 修正） |
| FZ-BUNIT-SEMANTICS | BUNIT/pixel_area_power | BUNIT 必须量纲可判：显式 px 幂次 或 provenance pixel_semantics=surface_brightness+pixel_area_power=-2 | SO-01 | F-OBS-01：BUNIT 量纲可判 + provenance pixel_semantics/pixel_area_power 属 FROZEN SCI 术语/单位修正 |
| FZ-FORMULA-DRIZZLE-SB | 目标态面亮度归一 | S_p = Sum_j B_j a_jp / Sum_j a_jp, B_j=x_j/A_pixel,j (等价 c_jp=a_jp/Sum a_jp) | SO-02 | F-OBS-02/S2：面亮度保持归一 S_p=ΣB_j a_jp/Σa_jp 是 FROZEN DRIZZLE §5/§7 公式变更，须签字并重跑全部 Drizzle 门 |
| FZ-FORMULA-DRIZZLE-VAR | Drizzle 方差传播 | variance_p = Sum_j v_j w_jp^2 / D_p^2; ivar_p=1/variance_p | SO-02 | 权重定义 w_jp:=w_SB_jp 后方差传播随 (B) 归一变化，属同一 FROZEN 公式变更 |
| FZ-COND-FLUX-CONSERV | 通量守恒条件不变量 | pixfrac=1: Sum_p F_p=Sum_j x_j 严格; pixfrac<1: 总输出通量=pixfrac^2*Sum_j x_j, provenance flux_conservation_factor | SO-02 | 通量守恒由严格不变量改为条件不变量 + flux_conservation_factor，属 FROZEN 公式变更 |
| FZ-GATE-CONST-SB | 常量场 Oracle | 按 B0 构造 x_j=B0*A_pixel_j; S_p=B0 对全部 pixfrac in (0,1]; \|S_p/B0-1\|<1e-3(沿用) | SO-03 | S3：常量场 Oracle 判据取代 FROZEN DRIZZLE §11（S_p=C 表述）须签字（容差 1e-3 不变） |
| FZ-GATE-PARENT-VAR | HiPS 父级方差对角近似 | 声明下界 + 另存相关核/算子摘要 + 误差门 deficit=(exact-diag)/exact<=阈值(ALG 冻结) | SO-04 | AR-030/AR-031：DRIZZLE §1/§9a『协方差产品为非目标』被取代为强制相关核输出面，须签字 |
| PSFSW-T-DEPTH | 共同星集深度稳定性最大相对偏差 | 0.05 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-DEPTH-K | 深度扫描点数下限 | 5 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-DEPTH-SPAN | 深度跨度下限 | 1 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-DEPTH-RHO | 单调漂移上限 \|Spearman rho\| | 0.8 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-NMIN | n_common 硬下限 | 3 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-NROBUST | n_common 稳健下限 | 10 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-NPREF | n_common 优选值 | 30 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-NU | 分量空间非均匀上限 (p95-p05)/p50 | 0.3 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-TREND | 分量系统趋势上限 max\|线性拟合值\|/p50 | 0.1 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-POWERLOSS | 标量功率损失上限（detection power 相对损失） | 0.05 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-FLUXBIAS | 标量通量偏差上限（注入源相对偏差） | 0.01 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-WRANGE | 帧权重动态范围 guard max/min | 100 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-COVMC | covariance MC 一致性 \|analysis-MC\|/MC | 0.03 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-BOOT | 基线声明 bootstrap 重采样下限 | 200 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-CI | 基线声明置信水平 | 0.95 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-EPSFTOL | effective PSF 定义复算容差 max\|P_eff_op-P_eff_analytic\| | 1e-9 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-T-NU-LOW | LOW 档分量空间非均匀收紧上限 | 0.2 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-COMPOSITE-ALPHA | PSFSW 复合指数 alpha | 2 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-COMPOSITE-BETA | PSFSW 复合指数 beta | 1 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-COMPOSITE-GAMMA | PSFSW 复合指数 gamma | 2 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-COMPOSITE-DELTA | PSFSW 复合指数 delta | 1 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-COMPOSITE-CNORM | PSFSW 归一常数 C_norm（尺度简并，不改 W_psfsw） | 1 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-COMPOSITE-FLOOR | 分量下限（S/Conc/N/B 防下溢） | 1e-12 | SO-07 | SO-07 数值/数据面/标定确认 |
| PSFSW-BASELINE-NONINFERIOR | 基线 non_inferior_to 不劣界 | 0.02 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-EPS-PIXIVAR | pixel-ivar 近似误差门 epsilon（面积加权 p95 rho） | 0.05 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-EPS-PIXIVAR-SUP | pixel-ivar 逐元素硬上限（=4ε） | 0.2 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-RANK-RTOL | 秩判定相对容差 sigma_i/sigma_max | 1e-10 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-KAPPA-MAX | 条件数上限 cond_2(D^-1 A^T W A D^-1) | 1000000 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-UPM-MINFRAMES | UPM 乘法/加性可辨识最少帧数 | 2 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-IDENT-RTOL | GLS 恒等式容差 R C_in R^T == (A^T C^-1 A)^-1 | 1e-9 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-MC-RELTOL | GLS covariance vs Monte Carlo 相对容差 | 0.03 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-EPSF-RTOL | 注入单位点源 vs 解析 P_eff 容差 | 1e-12 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-REJ-CALIB-BINMIN | rejection 校准每箱最小样本数 | 50 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-REJ-CALIB-ABS | rejection 可靠性绝对偏差上限 \|obs-mean(p)\| | 0.1 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2S-REJ-BSS-MIN | rejection Brier skill score 下限 | 0.1 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2PT-SNR-IDENT-RTOL | 独立帧 SNR_combined^2=ΣSNR_k^2 相对容差 | 1e-9 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-AP2PT-CORR-RATIO-MIN | 相关帧朴素求和过度乐观检出阈值（joint/naive 方差比） | 1.05 | SO-07 | SO-07 数值/数据面/标定确认 |
| CF-T-CONST-SB-TOL | Drizzle 常量面亮度门容差（沿用，不改） | 0.001 | SO-03 | SO-07 数值/数据面/标定确认 |
| FZ-AP1-DEFICIT-THRESH | HiPS 父级方差 deficit=(exact-diag)/exact 阈值（提案，待签字） | 0.2 | SO-07 | SO-07 数值/数据面/标定确认 |
| FZ-PROV-KCORR-VALUE | k_corr 已声明适用域内冻结值（未复跑标定前） | 1.4 | SO-07 | SO-07 数值/数据面/标定确认 |

## 3. 开放项（OPEN）

| ID | 主题 | 类别 | owner | 锚 |
|---|---|---|---|---|
| DI-01 | schema 词表单一化（SCI-PSFW 词表 vs SCI-P2 词表 → 单一 schema） | open_design | SCHEMA-INTEGRATE-001(W6) | C-004.3; ADJ-C004-03; DATA-DESIGN-001 10_migration §2 |
| DI-02 | 数值阈值：surface_gls epsilon / HiPS deficit / PSFSW 指数与归一常数 / 共同星集深度稳定性阈值 | threshold_pending | ALG-P2-SURF-001 / ALG-P1-001 / ALG-P2-PSFSW-001 (W3) + 负责人 | FZ-GATE-PIXIVAR-APPROX; FZ-GATE-PARENT-VAR; FZ-FORMULA-PSFSW-COMPOSITE; SO-07 |
| DI-03 | shared systematic 的低秩/相关核数据面实例化 | data_face | ALG-P1-001 / IMPL-P1-CAL-001 / SCHEMA-INTEGRATE-001 | FZ-PROV-SHARED-SYSTEMATIC; ADJ-F-OBS-04 |
| DI-04 | k_corr 标定脚本 + 固定种子 MC 复跑 | calibration | ALG-P2-UPM-001 / IMPL-P2-UPM-001 + 负责人 | FZ-PROV-KCORR; ADJ-F-OBS-05; SO-07 |
| DI-05 | AR-048 参数生效证明（参数被记录 ≠ 生效） | runtime_gate | DATA-DESIGN-001(G-PARAMETER-EFFECTIVENESS) / RUNTIME-CI-001 | reports/v6/review-audit/03 AR-048 |
| DI-06 | 生产 schema plane 枚举扩展与 runtime validator 同一提交（F-UNC-003） | schema_runtime | SCHEMA-INTEGRATE-001(W6) / IMPL-AIO-001 | DATA_SEMANTICS §30.2 F-UNC-003 |
| DI-07 | weight_units 字面量 "1"(冻结) vs dimensionless_relative(SCI-P2 R5) 的 W6 双射落定 | vocabulary | SCHEMA-INTEGRATE-001(W6) | C-004.3; DATA-DESIGN-001 10_migration §2b |
| OI-01 | Phase1 单帧无法形成帧组：四分量+未归一 Wt+归一契约归 Phase1，组内 median=1 归 Phase2；接口拆分须 W4 批准 | interface_ratification | CONTRACT-FREEZE-001(W4) | ALG-P1-001 §4.5/§8.3 |
| OI-02 | DESIGN-P1 §4.2 的 V_b + α²V_b 与同 master_id 的 (1−α)² 合并关系 | finding | CONTRACT-FREEZE-001(W4)/负责人 | ADJ-OBS-01; ALG-P1-001 §2.2 |
| OI-03 | SCI-CAL-001 §9a/§1『不传播 variance / 不建模 gain-readnoise』与 V6 目标冲突（取代登记） | supersede_registration | CONTRACT-FREEZE-001(W4) SO-06 | ALG-P1-001 §2.4; SO-06 |
| OI-04 | docs/**/v6/** 未登记进 docs/DOCUMENT_INDEX.yaml；check_doc_index.py docs_fully_covered 现为红 | coverage_gap | DOC-CONVERGE-001(W12)/控制器 | OI-04; ALG-P1-001 §8.3 |
| OI-05 | common star set n_common 下限、selection bias 深度稳定性阈值、四分量非均匀拆 tile 阈值未冻结 | threshold_pending | ALG-P2-PSFSW-001(W3)/CONTRACT-FREEZE-001(W4) SO-07 | ALG-P1-001 §8.3; PSFSW-G11/G18 |
| OPEN-P2S-01 | point_source 产品规范输出形态（map vs statistic） | domain_open | ALG-P2-POINT-001/控制器 | ALG-P2-SURF-VERIFICATION §6.3 |
| OPEN-P2S-02 | UPM 乘法尺度 g_k 的生产数据面/schema 与空间模型表示 | data_face | DATA-DESIGN-001/SCHEMA-INTEGRATE-001 | ALG-P2-SURF-VERIFICATION §6.3 |
| OPEN-P2S-03 | 跨帧完整联合 C 的低秩/相关核表示 | data_face | DATA-DESIGN-001/CONTRACT-FREEZE-001 | ALG-P2-SURF-VERIFICATION §6.3 |
| P3-OPEN-EPSILON-CORR | Phase3 相关核近似误差阈值 epsilon_corr 具体数值 | threshold_pending | CONTRACT-FREEZE-001(W4)+负责人(SO-07) | ALG-P3-001 §11 / ALG-P3-001-VERIFICATION §8.3 |
| PF-01 | G-INJ-01 注入源 flux bias 容差（设计默认 2% 未冻结） | threshold_pending | ALG-P2-POINT-001 | QA_MATRIX G-INJ-01 threshold_status=pending_freeze |
| PF-02 | G-INJ-02 扩展源三量一致性容差（设计默认 3% 未冻结） | threshold_pending | ALG-P2-SURF-001 | QA_MATRIX G-INJ-02 threshold_status=pending_freeze |
| PF-03 | G-INJ-03 W_info 注入恢复容差（建议 5% 未冻结） | threshold_pending | ALG-P2-PSFSW-001 | QA_MATRIX G-INJ-03 threshold_status=pending_freeze |
| PF-04 | G-INJ-07 Phase3 flux 恢复容差（设计默认 2% 未冻结） | threshold_pending | ALG-P3-001 | QA_MATRIX G-INJ-07 threshold_status=pending_freeze |
| PF-05 | G-RD-01 真实数据 M42 接缝/噪声清单门（W10 预注册冻结） | threshold_pending | REAL-SCIENCE-001 | QA_MATRIX G-RD-01 threshold_status=pending_freeze |
| PF-06 | G-RD-02 真实数据银心清单门（W10 预注册冻结） | threshold_pending | REAL-SCIENCE-001 | QA_MATRIX G-RD-02 threshold_status=pending_freeze |
| PF-07 | G-BASE-03 psfsw 基线比较效应量与 CI（预注册冻结） | threshold_pending | ALG-P2-PSFSW-001/预注册 | QA_MATRIX G-BASE-03 threshold_status=pending_freeze |
| AR-032-GAP | 非 v6 docs/science/*.md 在 W1–W8 无 owner；正式取代归 DOC-CONVERGE-001 | coverage_gap | DOC-CONVERGE-001(W12)/SO-06 | SCI-ADJ-001_CONFLICT_MATRIX §5 |
| AR-034-GAP | 7 项历史 CI 红无 V6 逐名验收锚（建议 RUNTIME-CI-001 逐名登记终态） | ci_gap | 控制器/RUNTIME-CI-001 | ADJ-AR-03 |
| AR-035-GAP | 785 合并层缺陷账本无销账任务（只登记，处置权在控制器/负责人） | ledger_gap | 控制器/FINAL-AUDIT-001 | ADJ-AR-03; P0_GATE_FAMILY |
| AR-036-SIGNOFF | 宪章 §10.5/§17.6『记录 vs 自动判决』需负责人签字（V6 无承载） | governance_signoff | 项目负责人 | ADJ-CTRL-01; SO-05 |

## 4. SO-01..07（保持待签）

| ID | 事项 | owner | 状态 |
|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-02 | F-OBS-02 / S2 面亮度归一改为 (B) 面亮度保持 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | 项目负责人 | PENDING_OWNER_SIGNOFF |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | 项目负责人 + CONTRACT-FREEZE-001 + DOC-CONVERGE-001 | PENDING_OWNER_SIGNOFF |
| SO-07 | F-OBS-03/F-OBS-04/F-OBS-05 的数值阈值/数据面/标定 | ALG-P1-001/ALG-P2-UPM-001/CONTRACT-FREEZE-001 + 负责人 | PENDING_OWNER_SIGNOFF |
