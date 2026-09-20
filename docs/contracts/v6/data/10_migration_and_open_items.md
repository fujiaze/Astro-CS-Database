> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 10 — 迁移建议、开放项与需裁决/签字项（只登记不擅改）

上位锚：`ASTROCS_DESIGN.md` §0 权威链；PROJECT_SPEC §11（迁移原则）；CONTROLLER_LOG C-004.1..6；SCI-ADJ-001 §7/§8。
（**2026-09-20 订正**：原文「宪章 §1.2/§14.5/§16.2」已作废——宪章 `ASTROCS-CONSTITUTION-001` 由 ROOT-007 删除、**不作权威**，依据 `ASTROCS_DESIGN.md:31` + `docs/owner/PROJECT_SPEC.md:9`；该引用仅存历史溯源，见本文件 `:1` 的 DOC-001 注记。）
机器：`contracts/proposals/v6/data/astrocs.v6.data-design-catalog.v1.json` 的 `migrations` / `open_items` / `signoff_items` / `controller_only_items`。

## 1. 迁移建议（逐条，含 reader 规则）

| ID | From | To | Reader 规则 | Owner | 锚 |
|---|---|---|---|---|---|
| MIG-WEIGHTMODE-LEGACY | ASTROCS_WEIGHT_MODE 整数 {0,1,2}（**已删键**，§9.73 A44）；DATA_SEMANTICS §30.3；PUBLIC_API:1165；ACR {auto,ivar,equal,support_x_snr2} | 显式字符串枚举 {point_information,surface_gls,psfsw_robust} + 基线 {equal,pixel_ivar} | 整数 0 一律 REJECT；1→equal、2→pixel_ivar 仅作基线对照；ACR 旧枚举标 ARCHIVED | W6 | ADJ-S1；ADJ-AR-01；`FZ-FIELD-WEIGHTMODE` |（**已按 §9.73 A44 作废**：ASTROCS_WEIGHT_MODE 键已从库头摘除、「权重模式」概念不存在；本行只作 v6 设计档案留痕，**不得**据此定义生产键名/枚举）
| MIG-UNITS-PXVARIANCE | 同名 `variance`（输入 ADU² / 输出 ADU²/px⁴） | `pixel_variance_in` / `sb_variance_out` 分离 | BUNIT=ADU 且无 pixel 语义 → 单位不可判 REJECT | W4 + 负责人 | `FZ-UNIT-VAR-IN`/`-SB`；`FZ-BUNIT-SEMANTICS`；SO-01 |
| MIG-NORM-DRIZZLE | `w_jp=a_jp/A_drop`（pf<1 时 `S_p=B0/pixfrac²`） | 面亮度保持 `S_p=Sum_j B_j a_jp/Sum_j a_jp` + `flux_conservation_factor` | legacy 归一须带版本；缺 factor 禁用于绝对通量 | W4 + 负责人 | `FZ-FORMULA-DRIZZLE-SB`；`FZ-COND-FLUX-CONSERV`；SO-02 |
| MIG-PSFSW-VOCAB | SCI-PSFW 词表 `weight_kind/weight_units/normalization.scope` 与 SCI-P2 词表 `weight.kind/weight.units/group_normalized` | W6 归一为单一 schema（**已归一**：canonical = `weight.kind` / `weight.units` / `weight.group_normalized`。旧写法「本提案两词表并存互映，不发明第三套」**已作废**——2026-09-20，依据 `W6_SCHEMA_INTEGRATION.md:29,38,104` + `contracts/data/v6_weight_vocabulary_v1.json`（`status=PRODUCTION_INTEGRATED`）） | 自创第三套词表 → REJECT（**已归一之后**；旧写法「**归一前**自创第三套词表 → REJECT」中的「归一前」已作废——2026-09-20，同上） | W6 | ADJ-C004-03；ADJ-S1 |
| MIG-COVARIANCE-PRODUCT | DRIZZLE §1/§9a「协方差产品为非目标」；只存对角 variance | 协方差/相关核为强制输出面（对角+rho/算子摘要） | 只对角无核/近似误差 → REJECT | W4 + 负责人 | ADJ-AR-02；`FZ-GATE-PARENT-VAR`；SO-04 |
| MIG-DIAGNOSTIC-NOT-WEIGHT | median(SNR_F)/support/coverage/FWHM/residual 作权重（SCI-CW weight_mode=2 等） | 仅诊断/深度/门 | 诊断别名出现在 weight.sources/weight_value/variance_from → REJECT | W4 | `FZ-GATE-MEDIAN-SNR`/`-SUPPORT-COVERAGE`；ADJ-C004-02 |（已按 §9.73 A44 作废：该概念不存在）
| MIG-SCHEMA-OWNER | 生产 schema 仅 `phase_product_exchange.schema.json` plane_id={signal,support,variance,ivar,mask} | W6 并入本提案对象（W_info/psfsw/effective PSF/covariance 描述层） | science plane 枚举扩展须 schema 与 runtime validator 同一提交 | W6 | F-UNC-003；DATA_SEMANTICS §30.2 |

## 2. 开放项（本任务只登记，不定值/不擅改）

| ID | 事项 | Owner | 为何不在本任务 |
|---|---|---|---|
| ~~DI-01~~ | ~~schema 词表单一化（两既有词表 → 单一）~~ **已闭合**（2026-09-20：W6 `SCHEMA-INTEGRATE-001` 以 `contracts/data/v6_weight_vocabulary_v1.json`（`status=PRODUCTION_INTEGRATED`）机器固化，依据 `W6_SCHEMA_INTEGRATION.md:104`；原文「C-004.3 明确归 W6；本任务只给映射建议」为闭合前状态，保留留痕） | — | 已不在开放项 |
| DI-02 | 数值阈值：surface_gls 的 epsilon、HiPS 父级 deficit 阈值、PSFSW 指数/归一常数、共同星集深度稳定性阈值 | ALG-P2-SURF-001 / ALG-P1-001 / ALG-P2-PSFSW-001（W3）+ 负责人 | 数值阈值不属 DATA schema 设计；不得由实现自行发明 |
| DI-03 | shared systematic 的低秩/相关核数据面实例化 | ALG-P1-001 / IMPL-P1-CAL-001 / W6 | 本提案只冻结三种允许表达与 fail-closed |
| DI-04 | k_corr 标定脚本 + 固定种子 MC 复跑 | ALG-P2-UPM-001 / IMPL-P2-UPM-001 + 负责人 | 属 SO-07 数据面/标定 |
| DI-05 | AR-048 参数生效证明（参数被记录 ≠ 生效） | 本提案给门 `G-PARAMETER-EFFECTIVENESS`；运行时验证归 RUNTIME-CI-001 | 属运行时/CI 域 |
| DI-06 | 生产 plane 枚举扩展与 runtime validator 同一提交（F-UNC-003） | W6 / IMPL-AIO-001 | 属生产 schema/validator 域 |
| ~~DI-07~~ | ~~`weight_units` 字面量 `"1"`(冻结) vs `dimensionless_relative`(SCI-P2 R5) 的 W6 双射落定~~ **已闭合**（2026-09-20：canonical = 冻结字面量 `"1"`，`dimensionless_relative` 仅 reader 别名，依据 `W6_SCHEMA_INTEGRATION.md:34,104`；原文「C-004.3 归 W6；本设计只提供别名可表示与结构门」为闭合前状态，保留留痕） | — | 已不在开放项 |

## 2b. 与相邻 W3 算法规格的词表一致性（HEAD 集成后复核）

HEAD=`28e0ac6c` 已集成 `ALG-P2-PSFSW-001` 与 `ALG-P2-POINT-001`（`git log 125bc099..HEAD`）。两者的 schema 归属均显式写 `DATA-DESIGN-001`(W3) / `SCHEMA-INTEGRATE-001`(W6)，与本设计不冲突：

| 来源 | 词表 | 本设计的可表示性 |
|---|---|---|
| `docs/algorithms/v6/phase2-psfsw/PSFSW_ALGORITHM_SPEC.md` §7.4 | `weight_kind="relative_dimensionless"`、`weight_units="1"`、`normalization.scope="group"`+`median_target=1.0` | **已作废（2026-09-20）**：原文「`psfsw.v1.weight.kind_alias_sci_psfw` / `units_alias_sci_psfw` / `normalization.scope`+`median_target`」——生产 schema 已**删除**这些桥接字段，别名只存在于迁移层 reader 规则（依据 `W6_SCHEMA_INTEGRATION.md:38`；`contracts/schemas/v6/` 无该键）。现行 canonical 落点见下一行 |
| 同上，SCI-P2 侧 | `weight.kind="psfsw_robust_weight"`、`weight.units="dimensionless_relative"`、`group_normalized=true` | `psfsw.v1.weight.kind`(const) / `weight.units`(canonical "1") / `group_normalized`(const true) |
| `docs/algorithms/v6/phase2-point/ALG-P2-POINT-001_SPEC.md` | `weight.kind="W_info"`、`units="ADU^-2"`、`group_normalized=false` | `point-information.v1` + `weight-mode.v1` |

**字面量差已由 W6 落定（2026-09-20 订正）**：canonical = 冻结字面量 `"1"`（`FZ-UNIT-PSFSW`/`FZ-FIELD-PSFSW-UNIT`），`SCI-P2-001` 权重来源门 R5 的 `dimensionless_relative` 仅作 **reader 别名**（旧写法「**唯一需 W6 落定的字面量差**」已作废，依据 `W6_SCHEMA_INTEGRATION.md:34`）。
本设计取 canonical `"1"`；**归一已完成**，`dimensionless_relative` 只存在于迁移层 reader 规则（不再有 `units_alias_sci_psfw` 字段），第三套词表仍禁止（旧写法「使两套既有词表在 **W6 归一前**互相可认（`C25-dual-vocabulary` 结构门；不发明第三套）」已作废，依据 `W6_SCHEMA_INTEGRATION.md:34,38,39`）。
`exposure` 只作 `baseline_id` 比较协议字段、不是 `weight_mode` 合法值，与 PSFSW 规格 §3 一致。（已按 §9.73 A44 作废：该概念不存在）

## 3. 需负责人签字项（只登记，不擅改；来自 SCI-ADJ-001 §7）

SO-01（DRIZZLE §3 术语/单位修正）、SO-02（面亮度归一改 (B) + 重跑全部 Drizzle 门）、SO-03（常量场 Oracle 判据取代 §11）、
SO-04（协方差产品非目标声明取代）、SO-05（宪章 §10.5/§17.6 记录/裁决分离）、SO-06（AR-032 非 v6 SCI 迁移/取代清单）、
SO-07（F-OBS-03/04/05 数值阈值/数据面/标定）。本设计与之衔接但不代签、不改冻结正文/容差/门。

## 4. 控制器级事项（只登记不裁决）

CTRL-F1（工作树 ≠ HEAD 的 16 回退 + 10 删除）、CTRL-AR033（根 CMakeLists/tests CMakeLists 无 V6 owner）、
CTRL-AR034（7 项历史 CI 红）、CTRL-AR035（785 缺陷账本销账）、CTRL-AR036（宪章修订签字）。
本设计一切生产面描述以任务基线 HEAD=`125bc099` 为准；交付时 HEAD=main=`28e0ac6c`（控制器集成并行 W3 兄弟任务 `ALG-P2-PSFSW-001`/`ALG-P2-POINT-001`，未触及本写域）。
科学冻结输入在两态一致（W1 双态核验），结论不依赖 F1。

## 5. 本任务「不做什么」清单

- 不写生产 schema（`contracts/schemas/`、`contracts/data/`）、不实现校验器、不改 `runtime/`；
- 不改 `docs/science/*.md`、`docs/owner/**`、`docs/design/**`、`docs/references/**`、生产源码、测试、CI；
- 不定数值阈值、不改冻结门/容差、不宣布发布；
- 不 commit / push / git add / 建分支 / worktree / stash / reset / clean / rebase；
- 不派生子代理。
