# CI 门禁与状态（Gates & States）

## 1. 门禁原则

- P0 红灯无豁免；P1 红灯须负责人登记豁免（`ci/exemptions.json`，只减不增）；
- 豁免只豁免"检查项"，不豁免科学/工程硬约束；
- 机器门禁通过后自动推进，不设频繁人工 checkpoint。

## 2. 状态语义（唯一口径，与最高设计 §11.4 一致）

| 状态 | 语义 | CI 中 |
|---|---|---|
| CONTRACT_READY | 权威文档/合同/schema 冻结在位 | 文档一致性检查绿 |
| IMPLEMENTED | 生产源码在位且当前提交实际执行通过 | 构建+单测绿 |
| INSTALLED | 进入安装树+产品清单，CLI/loader 可发现 | 安装树检查绿 |
| VERIFIED | 正式平台（Windows x64）+ 真实数据验收通过 | 负责人触发复验，非自动 |
| NOT_IMPLEMENTED / NOT_VERIFIED / DEFERRED / DORMANT / FAIL | 负向状态 | 如实报告，不冒充 |

## 3. 检查项门禁表（简表，完整见 01_CHECKS.md）

| ID | 级 | 阻塞合并 | 豁免 |
|---|---|---|---|
| CHK-BUILD-LINUX / WIN | P0 | 是 | 否 |
| CHK-WARN / STATIC | P0 | 是 | 否 |
| CHK-MODULE-MANIFEST / CONTRACT-REF / SCI-REF / CONTRACT-TEST / AGENTS-GOV / ENG-CONSTRAINTS / CHK-REGISTRY-DOC-SYNC | P0 | 是 | 否 |
| CHK-UNIT / ORACLE / INVARIANT / ABI / SCHEMA | P0 | 是 | 否 |
| CHK-SYNTH-P1/P2/P3 / NWORKER / RESOURCE | P0 | 是 | 否 |
| RESOURCE-GATE-REAL / RESOURCE-GATE-REAL-NEG（真实重计算面利用率 + 负例面） | P0 | 是 | 否 |
| CHK-SECRET-HYGIENE / CHK-ROOT-CLEAN | P0 | 是 | 否 |
| CHK-ISA-EQ / SANITIZER / VERSION-CONSISTENCY / VERSION-NAMESPACES / DOC-L0 / CHK-ENV-ADOPTION / STD-REG | P1 | 是 | 负责人登记 |
| CHK-DANGLING / STALE-DOC | P1 | 是 | 负责人登记 |
| CHK-COVERAGE / GLOSSARY-DOCS | P2 | 否 | —— |

**RESERVED（文档曾承诺但无实现，不进本表）**：`CHK-FMT`、`CHK-DUAL-TOL`、`CHK-AGENT-HARD-RULES`
（语义继任者 `AGENTS-GOV`）。逐条依据与重新注册前置条件见 `01_CHECKS.md §2.1`；机器可读面见
`ci/id_migration_map.json::reserved_targets`。**保留一个不存在的 P0 门 = 把假绿写进本表。**

## 4. 禁止事项

- 用 waiver 掩盖红灯（未被授权豁免的条款不可豁免）；
- 把"能编译"当"验收过"（必须跑断言/测试/机器门）；
- 合成测试标成 VERIFIED；
- 用历史版本全量重算代替科学 Oracle。

## 5. 门禁通过定义

`gates-report` 汇总：全部 P0 绿 + P1 绿（或已豁免）→ 门禁通过 → 打包发布候选；否则失败。
