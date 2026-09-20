# 变更 claim：CHG-2026-09-20-PSFSW-RETIRE — 统一数据对象 `psfsw_robust_weight` 退役（14 → 13）

- 控制包：RELEASE-03 / 任务 **FIX-209**（`工程控制/RELEASE-03/tasks/FIX-209.md` 由前台按 GAP_AUDIT §4.5 派发；本任务为**负责人上呈裁决的直接产物**）
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §2.1（**全程只有 SNR，不存在「权重模式」这个概念**：HiPS 只存帧级 SNR 与稀疏相对 SNR 比值；权重是阶段二按天球像素对应帧集合现场算出的**派生量**；阶段一/阶段三不产生也不消费权重）、§2.2（**数据对象唯一性**：每个对象只有一份正本定义，端口只接同一种对象，接错必须直接判红、不允许自动转换）、§2.3（**质量代理不进科学权重**：`q_psf`/残差尺度只作诊断，禁止计入阶段二科学叠加权重）、§12（禁止用版本号作为生效或退役条件）
- 依据（裁决正本）：`工程控制/RELEASE-03/GAP_AUDIT.md` **§4.5 C01**（负责人 2026-09-20 裁决 **B**，逐字见 §1）；`工程控制/RELEASE-02/GAP_AUDIT.md` §9.73【裁决 A44】；`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.1-C01；`工程控制/RELEASE-03/GAP_AUDIT.md` §4.1 Q2（v6 合同层去留）
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（变更 claim + 一致性回归）、§6/§7；`AGENTS.md` §1.1/§4/§5/§8；`CONTROL_PACK_SPEC.md` §5/§6/§7
- 状态：**已落地**（合同层 + 文档层 + 测试层 + 回归同步完成）
- 影响类：**破坏性合同变更**（统一数据对象合同的结构性变更；**已按 `00_README.md` §2.4 上呈负责人并获裁决**）
- 文件域：`contracts/**`、`tests/contracts/**`、`docs/design/UNIFIED_MODEL.md`、`docs/contracts/unified_object_registry.json`（**未触碰** `lib/**`、`config/**`、`ci/checks.json`、`docs/architecture|interfaces|standards|modules|plugins|owner/**`）

## 1 裁决原文（负责人逐字，不得改写）

> **B —— 按 DESIGN-DRAFT §3.1-C01 彻底删除该对象（14 → 13）**

（来源：`工程控制/RELEASE-03/GAP_AUDIT.md` §4.5 C01。上呈依据：`00_README.md` §2.4「顶层三命令划分与 JSON 合同的**结构性**破坏变更、HiPS 产品数据模型的**破坏性**变更（须先上呈）」——本项属「统一数据对象合同的结构性破坏变更」，故上呈。）

同节现状（可复核）：`docs/design/UNIFIED_MODEL.md §2` 的「14 个数据对象」含 `psfsw_robust_weight`；`contracts/schemas/unified/psfsw_robust_weight.schema.json`（+ `examples/`、`negative/`、`port_contract.schema.json`）；`contracts/data/unified_object_compatibility_map_v1.json`、`docs/contracts/unified_object_registry.json`；`tests/contracts/test_unified_object_contract.py` 把「14 对象清单」与「可否作权重」判定**逐字**锁为合同；全仓引用 **124 处**（含 v6 合同层）。

## 2 变更内容（按 GAP_AUDIT §4.5 的 7 条执行要求逐条落地）

| # | 要求（§4.5） | 落地 |
|---|---|---|
| 1 | 删 canonical schema / example / `negative/` 中该对象的反例 | 已删 `contracts/schemas/unified/psfsw_robust_weight.schema.json`、`contracts/schemas/unified/examples/psfsw_robust_weight.example.json`；`negative/` 中**原本不存在**该对象的反例（n1–n4 与本对象无关），改以**新增**反例 `n5_retired_psfsw_robust_weight.schema-violation.json` 锁定「旧声明必败」 |
| 2 | `port_contract.schema.json` / `unified_object_compatibility_map_v1.json` / `unified_object_registry.json` 去引用 | 三处 canonical 引用全部移除；端口合同 4 个枚举与 `allOf` 同对象门由 14 → **13**；兼容映射 `entries` 10 → 9 并把退役条目移入 `retired_entries`；registry `canonical_object_classes` 14 → **13**、`deprecation.legacy_paths` 删除指向已删 canonical 文件的映射（13 → 12） |
| 3 | `docs/design/UNIFIED_MODEL.md §2` 由 14 改 13（写明退役依据） | mermaid 节点与对象表行删除；新增「**canonical 数据对象 = 13 个**」清单与退役依据块（A44 + 本次裁决 B + DESIGN-DRAFT §3.1-C01） |
| 4 | `tests/contracts/test_unified_object_contract.py` 的 `OBJECTS`/`VERDICT` 同步（**收窄断言，不得删测试、不得放宽其它门禁**） | `OBJECTS` 14 → 13、`VERDICT` 删键；硬编码 `14` 改为 `len(OBJECTS)`（**等价收窄**）；负例索引 4 → **5**（新增，不是放宽）；新增 `TestRetiredObjectContract` 4 个测试（无 canonical 正本 / 旧声明显式拒绝 / 退役记录带迁移提示 / 退役名不得回流）；**未删任何测试** |
| 5 | v6 合同层在位保留，去掉「作为 canonical 对象」的引用 | 见 §2.2 |
| 6 | 变更 claim + 影响面 + 兼容策略 | 本文件；影响面逐行清单 `run/RELEASE-03/logs/FIX-209-impact-surface.txt`；兼容策略见 §3 |
| 7 | 回归：`CHK-CONTRACT-TEST`、`tests/contracts` 全绿 | 见 §4 |

### 2.1 变更对象（文件 × 变更性质）

| # | 文件 | 变更性质 | 依据 |
|---|---|---|---|
| 1 | `contracts/schemas/unified/psfsw_robust_weight.schema.json` | **删除**（canonical 正本） | §4.5-1 |
| 2 | `contracts/schemas/unified/examples/psfsw_robust_weight.example.json` | **删除**（正例） | §4.5-1 |
| 3 | `contracts/schemas/unified/port_contract.schema.json` | 去引用：4 个枚举 + 1 个 `allOf` 分支删除；`x-astrocs-object.retired_objects` 落退役记录（拒绝策略 + 迁移提示） | §4.5-2 |
| 4 | `contracts/schemas/unified/negative/EXPECTED.json` | 负例索引 4 → 5（新增 n5） | §4.5-1/4 |
| 5 | `contracts/schemas/unified/negative/n5_retired_psfsw_robust_weight.schema-violation.json` | **新增**负例：旧产品/端口声明退役对象 ⇒ 4 个 enum 门判红 | §4.5-6（负例证据） |
| 6 | `contracts/data/unified_object_compatibility_map_v1.json` | `canonical_object_schema_ids` 去键；`entries` 移除该条目；新增 `retired_entries`（`retired_at_change` + `rejection` + `migration_hint` + `canonical_schema_removed`）；`policy` 增补退役登记口径 | §4.5-2/6 |
| 7 | `docs/contracts/unified_object_registry.json` | 删 `canonical_object_classes` 条目；删 `deprecation.legacy_paths` 中指向已删 canonical 的映射；新增 `deprecation.object_retirements`；v6 psfsw schema 改登记为 `other_schema_files`（`retained_v6_design_archive`，`object_class=null`）；「14 个对象」措辞 → 13 | §4.5-2/5 |
| 8 | `docs/design/UNIFIED_MODEL.md` | §2：删节点/表行，写死 13 个对象与退役依据 | §4.5-3 |
| 9 | `tests/contracts/test_unified_object_contract.py` | `OBJECTS`/`VERDICT` 同步；新增 `TestRetiredObjectContract`；负例计数 5；正例下限 `len(OBJECTS)+1` | §4.5-4 |
| 10 | `contracts/data/v6_weight_vocabulary_v1.json`、`contracts/data/v6_migration_map_v1.json`、`contracts/data/v6_data_dictionary_v1.json`、`contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json`、`contracts/proposals/v6/data/astrocs.v6.psfsw.v1.schema.json`、`contracts/proposals/v6/data/astrocs.v6.data-design-catalog.v1.json` | 加顶层归档标注 `x-astrocs-canonical-object-retirement`（**只加不改**：v6 词表/单位表/禁止项/模式集/冻结条款**逐字未动**，文件不删） | §4.5-5 + §4.1 Q2 |
| 11 | `tests/contracts/v6/tools/gen_data_dictionary.py` | 生成器同步（标注常量 + `doc` 键），保证 v6 字典可重跑再生一致 | §4.5-5 |

**未改**（明确边界）：科学公式 / 默认容差 / SCI/ALG 冻结定义；`lib/**`、`config/**`、`ci/checks.json`；`docs/architecture|interfaces|standards|modules|plugins|owner/**`（DOC-202 在飞）。

### 2.2 v6 合同层（§4.1 Q2 = 在位保留的设计档案）

- **文件不删**：`contracts/schemas/v6/**`（10 件）、`contracts/data/v6_*`、`contracts/proposals/v6/**` 全部在位；
- **去 canonical 对象引用**：`docs/contracts/unified_object_registry.json` 不再把 `contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json` 登记为已退役对象的 `compatibility_projections`，改登记为 `other_schema_files`（`role=retained_v6_design_archive`、`object_class=null`）；`deprecation.legacy_paths` 中「v6 psfsw schema → unified `psfsw_robust_weight`」的映射删除；
- **改述为「v6 提案归档，canonical 已退役」**：6 个 v6 合同层文件加顶层标注 `x-astrocs-canonical-object-retirement`（`v6_layer_status=retained_design_archive`、`retired_at_change=CHG-2026-09-20-PSFSW-RETIRE`），并逐字说明「本文件中的 `psfsw_robust_weight` 是 v6 提案/产品族内部标识（`weight.kind` 值、单位表符号、`variance_from` 禁止项 token），**不是**统一对象 canonical 引用」；
- **`weight_mode` 家族**：v6 层的键面处置（作废留痕）归 DOC-201/FIX-207 面（`docs/contracts/v6/**` 已由 DOC-201 加作废横幅/同行留痕）；本任务**不删任何 v6 文件、不改 v6 冻结语义**；
- **可重跑**：v6 字典与 10 件生产 schema 的生成器在本任务前后均**逐字节可复现**（`tests/contracts/v6/tools/gen_data_dictionary.py`、`gen_production_schemas.py`；实测见 §4）。

## 3 兼容策略（破坏性变更必须写清：**显式拒绝 + 迁移提示**，不得静默接受）

1. **旧产品若声明该对象 ⇒ 显式拒绝（fail-closed）**：
   - `contracts/schemas/unified/psfsw_robust_weight.schema.json` **不存在** ⇒ 任何按 `object_schema_id`/`unified_object` 解析 canonical 正本的校验器**无法解析**，必须判红（不存在可静默接受的路径）；
   - 端口合同 `port_contract.schema.json` 的 `accepts_object` / `accepts_schema_id` / `connected_object_document.unified_object` / `connected_object_document.object_schema_id` 四个枚举**不含**该对象 ⇒ 声明即 **enum 判红**；
   - 负例锁定：`contracts/schemas/unified/negative/n5_retired_psfsw_robust_weight.schema-violation.json`（4 个 enum 门全红）；测试 `TestRetiredObjectContract::test_retired_object_declaration_is_explicitly_rejected` 同时给出**反向控制**（同形端口连接 `point_information` 必须通过 ⇒ 判红来自对象枚举，不是端口形态）。
2. **迁移提示（机器可读，非静默）**：
   - `contracts/data/unified_object_compatibility_map_v1.json#retired_entries[0].migration_hint`：**无等价替代对象**——点源严格权重 → `point_information`（`W_info=1/Var(F_hat)`）；帧级参考 → `frame_snr`（权重由 Phase2 逆方差叠加从 SNR **现场派生**，不落盘）；v6 词表 token 仅存于 v6 设计档案，不构成 canonical 对象；
   - 同文登记于 `docs/contracts/unified_object_registry.json#deprecation.object_retirements` 与 `port_contract.schema.json#x-astrocs-object.retired_objects`（**拒绝点即提示点**）；
   - 机器判据：`TestRetiredObjectContract::test_retirement_record_carries_migration_hint`（变更编号 + 拒绝策略 + 迁移提示 + canonical 文件确实不存在 + 登记表两处逐字一致）。
3. **不自动转换、不静默丢弃**：不提供 `psfsw_robust_weight → point_information` 的自动映射（两者不是同一估计量：前者是质量代理复合权重，后者是点源 Fisher 权重）；旧产品必须**重新生成**或**显式改用**上述对象。
4. **版本号不作为生效/退役条件**（最高设计 §12）：本变更以**变更编号 + 日期** `CHG-2026-09-20-PSFSW-RETIRE` / `2026-09-20` 登记，未引入版本窗口。
5. **零科学影响**：未改任何公式、常数、默认容差、SCI/ALG 冻结定义或数值结论；被删对象本就是「质量代理复合权重」，其科学面已被 §2.1（只有 SNR）与 §2.3（质量代理只作诊断）否定。

## 4 影响面（引用清单）

**口径（可复跑）**：`grep -rn "psfsw_robust_weight" . --exclude-dir=build --exclude-dir=.git --exclude-dir=run --exclude-dir=third_party`。
逐行明细 + 逐文件计数：`run/RELEASE-03/logs/FIX-209-impact-surface.txt`（629 行）。

| 面 | 文件数 | 命中行 | 处置 |
|---|---|---|---|
| **本任务文件域内** | 28 | 92 | 已按 §2 处置（canonical 面清零；v6 层加归档标注；测试/登记表同步） |
| 文件域外 | 129 | 365 | **只登记不处置**（见下） |
| 合计 | 157 | 457 | — |

> 口径差异说明：`GAP_AUDIT §4.5` 记「全仓引用 **124 处**」（该快照的统计口径与本次不同，且本任务新增了退役留痕/负例）。本表以**退役落地后、含新增留痕**的实测为准，命令与逐行明细已落盘，供前台独立复跑。

**文件域外分类登记**（不属 FIX-209，交前台按任务归属处置）：

| 类别 | 代表路径 | 说明 |
|---|---|---|
| 详细层文档（DOC-201/DOC-203 面） | `docs/contracts/{DATA_SEMANTICS,PUBLIC_API,UNIFIED_OBJECTS,DATA_ARTIFACTS}.md`、`docs/contracts/INDEX.yaml`、`docs/contracts/v6/**`、`docs/science/v6/**`、`docs/algorithms/v6/**`、`docs/design/PHASE{1,2}_DETAILED_DESIGN.md`、`docs/validation/v6/**` | 作为 v6 设计档案/历史面**在位保留**；`psfsw_robust_weight` 在其中的出现是 v6 词表 token 与历史记录，**不是**统一对象 canonical 引用；`UNIFIED_OBJECTS.md` 的 14 对象表与旧路径行归 **DOC-203**（本任务文件域不含该文件） |
| v6 运行/消费代码 | `lib/algorithms/**`（`integration/v6*`、`coverage`、`drizzle`、`photometry`、`projection`、`resample`、`calibration`）、`lib/infrastructure/aio/v6/**` | v6 产品族投影实现；**未触碰**（`lib/**` 禁改）；其 `psfsw_robust_weight` 是 v6 `weight.kind`/单位表符号 |
| v6 测试 | `tests/unit/v6_*`、`tests/integration/v6_*`、`tests/contracts/v6/{v6_oracle.py,test_v6_negative_mutations.py,evidence/mutations.json,tools/gen_production_schemas.py}` | v6 独立 Oracle 与负向 mutation；**逐字保留**（v6 层在位保留的设计档案，其内部词表断言不变）；`tests/contracts/v6/tools/gen_data_dictionary.py` 仅加归档标注常量 |
| 历史报告 | `reports/**` | 历史证据，**不得重写**（写入即伪造历史） |
| 控制包 | `工程控制/**` | 裁决正本与任务书；本文件为新增 |

## 5 验证证据（命令 + rc）

| 门 | 命令 | rc | 结果 |
|---|---|---|---|
| 静态清零（验收门 1） | `grep -rn "psfsw_robust_weight" contracts/schemas/unified/ \| grep -v "已退役\|retired"` | 1 | **输出为空**（见 §5.1） |
| OBJECTS 长度（验收门 2） | `python3 -c "from tests.contracts.test_unified_object_contract import OBJECTS; print(len(OBJECTS))"` | 0 | **13** |
| 合同测试（验收门 3） | `flock -w 7200 /var/tmp/astrocs/build.lock timeout 900 python3 -m pytest tests/contracts -q` | 0 | **67 passed**（基线 63 + 新增 4） |
| CI 检查项（验收门 4） | `python3 ci/run_checks.py --check CHK-CONTRACT-TEST` | 见 §5.1 | — |
| 变更 claim 落盘（验收门 5） | 本文件 | — | 在位 |
| 旧声明显式拒绝 + 迁移提示（验收门 6） | 负例 `n5_...schema-violation.json` + `TestRetiredObjectContract` | 0 | 4 个 enum 门全红 + 反向控制通过 + 迁移提示逐字锁定（见 §5.1） |
| 全量 ctest | `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ctest --test-dir build --output-on-failure` | 见 §5.1 | — |

日志目录：`run/RELEASE-03/logs/FIX-209-*.log`。

### 5.1 收口记录（2026-09-20，FIX-209 执行者）

（见本文件末尾补记）

## 6 需前台登记的 CI 检查项（`ci/checks.json`，本任务不写该文件）

| ID | 命令（step） | 期望 rc | 说明 |
|---|---|---|---|
| `CHK-PSFSW-RETIRED-STATIC`（建议） | `bash -lc "! grep -rn 'psfsw_robust_weight' contracts/schemas/unified/ \| grep -v '已退役\|retired'"` | 0 | 静态清零门（canonical 面零残留）；能红证据：对 `git show HEAD:contracts/schemas/unified/port_contract.schema.json` 同一 grep 有命中 |
| `CHK-PSFSW-RETIRED-NEGATIVE`（建议，可并入 UT-CONTRACTS） | `python3 -m pytest tests/contracts/test_unified_object_contract.py -q -k RetiredObjectContract` | 0 | 退役对象行为门：无 canonical 正本 / 旧声明显式拒绝 / 迁移提示 / 不得回流 |

> 说明：`tests/contracts` 已由既有 `CHK-CONTRACT-TEST`（step `UT-CONTRACTS`）覆盖，本任务新增的 4 个测试随之进入该门；上表两项是**可选加固**，由前台在 BLD-201 统一决定是否登记。

## 7 规范依据清单

`ASTROCS_DESIGN.md` §2.1 / §2.2 / §2.3 / §12；`工程控制/RELEASE-03/GAP_AUDIT.md` §4.1 Q2 / **§4.5 C01**；`工程控制/RELEASE-02/GAP_AUDIT.md` §9.73 A44；`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.1-C01；`ENGINEERING_SPEC.md` §3 / §6 / §7；`AGENTS.md` §1.1 / §4 / §5 / §8；`CONTROL_PACK_SPEC.md` §5 / §6 / §7；`00_README.md` §2.4 / §5；`docs/ci/CI_SPEC.md` §3/§7。
