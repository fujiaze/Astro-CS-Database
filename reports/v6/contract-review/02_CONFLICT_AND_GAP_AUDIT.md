# CONTRACT-FREEZE-001 冲突/缺口审计（已冻结 / 待负责人签字 / 仍开放）

> 纪律：本审计**不把任何未签字项写成已冻结**。状态字面量严格取 `FROZEN`（W4 冻结的 V6 合同，可实施）/ `PENDING_OWNER_SIGNOFF`（值唯一但须负责人按 SO-xx 签字）/ `OPEN`（未落定）。逐条台账见 `01_FREEZE_STATUS_MATRIX.md` 与机器表 `clauses`。

## 0. 统计

| 桶 | 条数 | 说明 |
|---|---|---|
| 已冻结 `FROZEN` | 39 | 33 语义 + 6 数值/常数（继承项） |
| 仍需负责人签字 `PENDING_OWNER_SIGNOFF` | 49 | 9 语义（SO-01/02/03/04）+ 40 数值阈值（SO-03/07） |
| 仍开放 `OPEN` | 8 | 7 条 pending_freeze（QA-MATRIX-001）+ Phase3 epsilon_corr |
| SO-01..07 签字项 | 7 | 全部保持待签 |
| 登记开放项（DI/OI/PF/AR） | 27 | owner 均已登记 |
| 被取代非 v6 SCI 段 | 10 | 只登记清单与建议措辞 |

## 1. 已冻结（FROZEN，39 条）

### 1.1 语义条款（33 条，SCI-ADJ owner_signoff_required=false）

`FZ-UNIT-SIGNAL-SB`、`FZ-UNIT-Q`、`FZ-UNIT-FLUX`、`FZ-UNIT-WINFO`、`FZ-UNIT-PSFSW`、`FZ-FORMULA-WINFO`、`FZ-FORMULA-Q`、`FZ-FORMULA-FHAT`、`FZ-COND-WHITENOISE`、`FZ-FORMULA-GLS`、`FZ-GATE-PIXIVAR-APPROX`、`FZ-FORMULA-COV-PROP`、`FZ-FORMULA-PSFSW-COMPOSITE`、`FZ-FIELD-PSFSW-4COMP`、`FZ-FIELD-PSFSW-UNIT`、`FZ-GATE-PSFSW-FAILCLOSED`、`FZ-GATE-PSFSW-COV`、`FZ-GATE-PSFSW-EPSF`、`FZ-MODE-PRODUCTION`、`FZ-MODE-BASELINE`、`FZ-MODE-DEFERRED`、`FZ-FIELD-WEIGHTMODE`、`FZ-GATE-MEDIAN-SNR`、`FZ-GATE-SUPPORT-COVERAGE`、`FZ-P3-MODES`、`FZ-P3-FAILCLOSED`、`FZ-P3-QW-RECOMPUTE`、`FZ-P3-KERNEL-REGISTRY`、`FZ-P3-BUNIT-QUADRATIC`、`FZ-DEGRADE-SCALAR`、`FZ-PROV-SHARED-SYSTEMATIC`、`FZ-PROV-KCORR`、`FZ-PROV-MINIMAL-SET`

### 1.2 数值/常数（6 条，继承既有合同）

`FZ-REJ-INHERITED-THRESH`（ALG-REJ-001 排异阈值原样继承）、`FZ-UPM-CONVERGENCE`（UPM_SOLVER 收敛参数原样继承）、`FZ-CAL-FLOOR`（flat 归一 floor 0.1）、`FZ-CAL-QUANTUM-DEFAULT`（q_adu 缺省 1 ADU）、`FZ-AP1-GLS-QW-RTOL`（Q/W==GLS 容差 1e-9 沿用）、`FZ-P3-OMEGA-NONCONST`（逐像素 Omega 非常数；CAR max/min=2.0000 证据）

> 说明：上述 39 条是 W4 对 **V6 目标态（`docs/**/v6/**`）** 的冻结；它与 FROZEN 非 v6 SCI 的正式取代关系另按 SO-01..06 走负责人签字（见 §2 与 §4）。

## 2. 仍需负责人签字（SO-01..07，保持 PENDING_OWNER_SIGNOFF）

| SO | 事项 | 被 gate 的条款（状态 = PENDING_OWNER_SIGNOFF） | owner |
|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | `FZ-UNIT-VAR-IN`、`FZ-UNIT-VAR-SB`、`FZ-UNIT-IVAR-SB`、`FZ-BUNIT-SEMANTICS` | 项目负责人 + CONTRACT-FREEZE-001 |
| SO-02 | F-OBS-02/S2 面亮度保持归一 (B) + 重跑全部 Drizzle 门 | `FZ-FORMULA-DRIZZLE-SB`、`FZ-FORMULA-DRIZZLE-VAR`、`FZ-COND-FLUX-CONSERV` | 项目负责人 + CONTRACT-FREEZE-001 |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | `FZ-GATE-CONST-SB`、`CF-T-CONST-SB-TOL` | 项目负责人 + CONTRACT-FREEZE-001 |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | `FZ-GATE-PARENT-VAR` | 项目负责人 + CONTRACT-FREEZE-001 |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | （治理面，无条款级 gate；宪章修订须负责人签字） | 项目负责人 |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | （取代清单，见 `03_SUPERSEDED_SCI_SECTIONS.md`；不改写非 v6 文件） | 项目负责人 + CONTRACT-FREEZE-001 + DOC-CONVERGE-001 |
| SO-07 | F-OBS-03/04/05 与 surface_gls/pixel-ivar 等的数值阈值/数据面/标定确认 | 40 条数值阈值（PSFSW-T-*、FZ-AP2S-*、FZ-AP2PT-*、FZ-AP1-DEFICIT-THRESH、FZ-PROV-KCORR-VALUE 等；见 `01_FREEZE_STATUS_MATRIX.md` §2） | ALG-* / CONTRACT-FREEZE-001 + 负责人 |

签字前约束（写入每条款 fail-closed）：条款值与文本已唯一确定，实现**不得放宽**；涉及 FROZEN 非 v6 SCI 的声明在签字前不得作为已生效修订引用，相关面按 fail-closed 处理（例如 HiPS 父级对角 alpha 面不得声明精确）。

## 3. 仍开放（OPEN，8 条 + 登记项）

### 3.1 条款级 OPEN（8）

| 条款 id | 事项 | owner | 处置 |
|---|---|---|---|
| `QF-G-INJ-01` | 注入源 flux bias 门（设计默认 0.02） | ALG-P2-POINT-001 | pending_freeze；未冻结前不得冒充已冻结容差 |
| `QF-G-INJ-02` | 扩展源三量一致性门（设计默认 0.03） | ALG-P2-SURF-001 | 同上 |
| `QF-G-INJ-03` | W_info 注入恢复门（建议 0.05） | ALG-P2-PSFSW-001 | 同上 |
| `QF-G-INJ-07` | Phase3 flux 恢复门（设计默认 0.02） | ALG-P3-001 | 同上 |
| `QF-G-RD-01` | M42 接缝/噪声清单失败数（0） | REAL-SCIENCE-001 | W10 预注册后冻结 |
| `QF-G-RD-02` | 银心清单失败数（0） | REAL-SCIENCE-001 | W10 预注册后冻结 |
| `QF-G-BASE-03` | psfsw 基线效应量与 CI | ALG-P2-PSFSW-001/预注册 | 预注册后冻结 |
| `CF-T-P3-CORR-EPSILON` | Phase3 相关核近似误差阈值 epsilon_corr | CONTRACT-FREEZE-001 + 负责人(SO-07) | 数值未提出；该面 fail-closed |

### 3.2 设计/工程开放项（DI-01..07，owner 已登记）

`DI-01` schema 词表单一化（W6）；`DI-02` 数值阈值（W3+负责人）；`DI-03` shared systematic 数据面（W5/W6）；`DI-04` k_corr 标定脚本+固定种子 MC（W5/负责人）；`DI-05` AR-048 参数生效证明（RUNTIME-CI-001）；`DI-06` 生产 plane 枚举与 runtime validator 同一提交（W6/IMPL-AIO-001）；`DI-07` `weight_units` 字面量 `"1"` vs `dimensionless_relative` 的 W6 双射落定。

### 3.3 Wave 3 登记的开放项（OI-01..05）

`OI-01` Phase1→Phase2 PSFSW 归一契约/接口拆分（本 W4 登记，仍需批准生效）；`OI-02` DESIGN-P1 §4.2 `V_b+α²V_b` 与同 master_id `(1−α)²` 合并关系；`OI-03` SCI-CAL-001 §9a/§1「不传播 variance/不建模 gain-readnoise」取代登记（SO-06）；`OI-04` `docs/**/v6/**` 未登记进 `docs/DOCUMENT_INDEX.yaml`（DOC-CONVERGE-001）；`OI-05` common star set 阈值（本 W4 已给唯一值 + SO-07 待签）。

### 3.4 覆盖缺口（只登记，处置权在控制器/负责人）

`AR-032` 非 v6 SCI 无 W1–W8 owner（取代清单已由本任务产出，见 `03_SUPERSEDED_SCI_SECTIONS.md`）；`AR-034` 7 项历史 CI 红无逐名验收锚（建议 RUNTIME-CI-001）；`AR-035` 785 合并层缺陷账本无销账任务（建议 FINAL-AUDIT-001）；`AR-051` 账本数字必须带层号；`AR-036/AR-019/AR-026` 宪章 §10.5/§17.6 修订签字（SO-05）。

## 4. SCI-ADJ 冲突项收口结论（逐项）

| 冲突项 | 结论 | 落点 |
|---|---|---|
| F-OBS-01 DRIZZLE §3 单位同名 | 已冻结 V6 命名/单位（`FZ-UNIT-VAR-IN/-SB`、`FZ-UNIT-IVAR-SB`、`FZ-BUNIT-SEMANTICS`）；**非 v6 SCI 术语修正待 SO-01 签字** | §2 / `03_SUPERSEDED` SUP-01 |
| F-OBS-02/S2 面亮度归一 | 已冻结 (B) 面亮度保持 + 条件通量守恒；**FROZEN 公式变更待 SO-02** | §2 / SUP-02 |
| F-OBS-03 HiPS 父级对角方差 | 已冻结三项要求（下界声明 + 相关核 + deficit 门）；deficit 数值 0.20 **待 SO-07**；非目标声明取代**待 SO-04** | §2 / SUP-04 |
| F-OBS-04 共享系统项 | 已冻结三种允许表达 + 进 covariance 传播链（`FZ-PROV-SHARED-SYSTEMATIC`）；数据面实例化 `OPEN(DI-03)` | §3.2 |
| F-OBS-05 k_corr 可复现 | 已冻结定义/适用域/查找表语义（`FZ-PROV-KCORR`）；域内值 1.4 与标定脚本 **待 SO-07 / OPEN(DI-04)** | §2/§3.2 |
| S1/AR-028/AR-024/AR-029 weight_mode 三面互斥 | 已冻结显式三模式 + legacy 退役 + 禁止值（`FZ-MODE-*`、`FZ-FIELD-WEIGHTMODE`） | §1.1 |
| AR-027/AR-003/AR-037 三模式定位 | 已冻结 point_information/surface_gls/psfsw_robust 权威式与边界；psfsw 非 QA-only 但不得冒充 ivar/Fisher | §1.1 |
| AR-012 support/coverage 与 ivar 分离 | 已冻结（`FZ-GATE-SUPPORT-COVERAGE`） | §1.1 |
| AR-010/C-004.2 median(SNR_F) | 已冻结仅诊断（`FZ-GATE-MEDIAN-SNR`）；诊断别名集命中即 REJECT | §1.1 |
| C-004.1 psf_snr_power | 已冻结 DEFERRED（`FZ-MODE-DEFERRED`）；生产枚举出现即 REJECT | §1.1 |
| S3/AR-023 常量场 Oracle | 已冻结按 B0 构造 + 全 pixfrac + 容差 1e-3 不变；**判据取代待 SO-03** | §2 / SUP-03 |
| S4/AR-017 Phase3 采样核 | 已冻结 kernel registry 规则（`FZ-P3-KERNEL-REGISTRY`）；nearest 受限、bilinear_4quad 须 Oracle | §1.1 |
| AR-030/AR-031 协方差产品 | 已冻结协方差/相关核为强制输出面；**非目标声明取代待 SO-04** | §2 / SUP-04/SUP-07 |
| F3-01..04 Phase3 | 已冻结三模式 fail-closed + Q/W 输出帧重算 + 逐像素 Omega（`FZ-P3-*`） | §1.1 |
| F3-05 SCI-P3-001 陈旧文字 | **只登记**（SUP-09）；不得改冻结正文，正式 amendment 待签字 | §3.4 / SUP-09 |
| AR-002..? 通用单位/字段/provenance/降级 | 已冻结（`FZ-UNIT-*`、`FZ-PROV-*`、`FZ-DEGRADE-SCALAR`） | §1.1 |

## 5. 控制器级事项（只登记不裁决）

| ID | 事实 | 处置 |
|---|---|---|
| CTRL-F1 | 工作树 ≠ HEAD（预存回退/删除），未裁决 | registered_only；本任务一切生产面描述以 HEAD 为准，未改 tracked 文件 |
| CTRL-AR033 | 根/公共 CMakeLists 无 V6 owner | registered_only；本任务不写构建面 |
| CTRL-AR034 | 7 项历史 CI 红 | registered_only（建议 RUNTIME-CI-001 逐名登记） |
| CTRL-AR035 | 785 合并层账本无销账任务 | registered_only（建议 FINAL-AUDIT-001） |
| CTRL-AR036 | 宪章修订签字项 | registered_only（SO-05） |

## 6. 未决风险（如实登记）

1. **签字依赖**：SO-01..07 未签前，涉及非 v6 FROZEN SCI 的条款（9 语义 + 40 数值）只能作为 V6 目标态合同实施，不得对外声称已完成对 FROZEN SCI 的正式修订；相关面 fail-closed。
2. **数值阈值**：7 条 pending_freeze 与 Phase3 epsilon_corr 数值未定，实现不得以设计默认值冒充已冻结容差（P0-06 规则）。
3. **schema 词表**：W6 归一前两套词表并存；本任务只给语义与双向映射建议，不发明第三套。
4. **F1 基线**：生产面描述以 HEAD=`ebefe00d` 为准；若最终基线取工作树，改变的是「现状」陈述而非靶向冻结。
5. **真实数据/运行时**：Wave 10/W11/W9 执行，不在本任务范围。
