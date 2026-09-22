# CI 门禁与状态（Gates & States）

> 上游：ASTROCS_DESIGN.md §12.4（验证层级与四层验收）、§12.5（状态阶梯）

## 1. 门禁原则

- P0 红灯无豁免；P1 红灯须负责人登记豁免（`eng/ci/exemptions.json`，只减不增）；
- 豁免只豁免"检查项"，不豁免科学/工程硬约束；
- 机器门禁通过后自动推进，不设频繁人工 checkpoint。

## 2. 状态语义（唯一口径，与最高设计 §12.5 一致）

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
| L2-FROZEN-GATE-SELFTEST / L2-FROZEN-GATE-REPLAY / WORKER-BALANCE-METRIC-SELFTEST / WORKER-BALANCE-METRIC-REPLAY（L2 冻结判据 fail-closed + 指标判别力） | P0 | 是 | 否 |
| CHK-REGISTRY-VALIDATE / CHK-GATE-FAILCLOSED-SELFTEST / CHK-FAILCLOSED-SURVEY（注册表门 + fail-closed 契约自测 + 全门禁普查） | P0 | 是 | 否 |
| CHK-SECRET-HYGIENE / CHK-ROOT-CLEAN | P0 | 是 | 否 |
| CHK-ISA-EQ / SANITIZER / VERSION-CONSISTENCY / VERSION-NAMESPACES / DOC-L0 / CHK-ENV-ADOPTION / STD-REG | P1 | 是 | 负责人登记 |
| CHK-DANGLING / STALE-DOC | P1 | 是 | 负责人登记 |
| CHK-COVERAGE / GLOSSARY-DOCS | P2 | 否 | —— |

**RESERVED（文档曾承诺但无实现，不进本表）**：`CHK-FMT`、`CHK-DUAL-TOL`、`CHK-AGENT-HARD-RULES`
（语义继任者 `AGENTS-GOV`）。逐条依据与重新注册前置条件见 `01_CHECKS.md §2.3`；机器可读面见
`eng/ci/id_migration_map.json::reserved_targets`。**保留一个不存在的 P0 门 = 把假绿写进本表。**

## 4. 禁止事项

- 用 waiver 掩盖红灯（未被授权豁免的条款不可豁免）；
- 把"能编译"当"验收过"（必须跑断言/测试/机器门）；
- 合成测试标成 VERIFIED；
- 用历史版本全量重算代替科学 Oracle。

## 5. 门禁通过定义

`gates-report` 汇总：全部 P0 绿 + P1 绿（或已豁免）→ 门禁通过 → 打包发布候选；否则失败。

---

## 6. L2 性能门与监控字段语义（GATE-501 落地）

> 正本：docs/ci/CI_SPEC.md §9；判据数值源：eng/contracts/resource_gate_v1.json。
> 本节只登记**门禁事实**（哪条门、什么判据、怎么红），不重复规范正文。

### 6.1 L2 冻结判据（违规必红）

四条判据（平均利用率 ≥0.85 / p50 ≥0.90 / 达标样本占比 ≥0.70 / 无连续 ≥10s
低利用窗）在 CI 裁决面由 `eng/ci/l2_frozen_gate.py` fail-closed 判定，任一
违规即红。历史证据回放：`artifacts/acceptance/l2_performance/gates/*_gate.json`
9 份 `verdict=pass` 的违规证据现在全部判红（`L2-FROZEN-GATE-REPLAY`）。
生产侧 `run_monitored.py` 的 `record_and_justify` 字段是记录语义，
**不得**再被当作"门禁通过"的依据。

### 6.2 requires_monitor 声明对照表（8 个检查 / 11 个执行单元）

`requires_monitor: true` = 真强制（声明即必须监控，证据缺失判红），
不改名 `monitor_capable`。逐个核对结果（生成自 `eng/ci/checks.json`）：

| 检查项 / 执行单元 | requires_monitor | 监控包装 | 监控证据面 | 判定旗标 | 执行语义 |
|---|---|---|---|---|---|
| CHK-BUILD-LINUX / BUILD-GCC-RELEASE | 是 | 是 | run/ci/monitor/BUILD-GCC-RELEASE.json | 否 | 真强制（证据缺失判红） |
| CHK-BUILD-LINUX / DEEP-CLANG-BUILD | 是 | 是 | run/ci/monitor/DEEP-CLANG-BUILD.json | 否 | 真强制（证据缺失判红） |
| CHK-CONTRACT-TEST / V6-CTEST-INTEGRATION | 是 | 是 | run/ci/monitor/V6-CTEST-INTEGRATION.json | 否 | 真强制（证据缺失判红） |
| CHK-UNIT / V6-CTEST-UNIT | 是 | 是 | run/ci/monitor/V6-CTEST-UNIT.json | 否 | 真强制（证据缺失判红） |
| CHK-NWORKER / V6-DETERMINISM | 是 | 是 | run/ci/monitor/V6-DETERMINISM.json | 否 | 真强制（证据缺失判红） |
| CHK-SANITIZER / DEEP-SAN-ASAN | 是 | 是 | run/ci/monitor/DEEP-SAN-ASAN.json | 否 | 真强制（证据缺失判红） |
| CHK-SANITIZER / DEEP-SAN-TSAN | 是 | 是 | run/ci/monitor/DEEP-SAN-TSAN.json | 否 | 真强制（证据缺失判红） |
| CHK-COVERAGE / DEEP-COV-CPP | 是 | 是 | run/ci/monitor/DEEP-COV-CPP.json | 否 | 真强制（证据缺失判红） |
| CHK-COVERAGE / DEEP-COV-PY | 是 | 是 | run/ci/monitor/DEEP-COV-PY.json | 否 | 真强制（证据缺失判红） |
| RESOURCE-GATE-REAL / RESOURCE-GATE-REAL | 是 | 是 | run/ci/resource-gate-real/resource_gate_real.json | **是** | 真强制 + 判定证据必须兑现 |
| CHK-REALDATA-E2E / CHK-REALDATA-E2E | 是 | 是 | run/ci/monitor/CHK-REALDATA-E2E.json | 否 | 真强制（证据缺失判红） |

- "监控包装"= 命令以 `eng/ci/resource_monitor.py` 前缀执行；
- "监控证据面"= 登记 `outputs` 中由该包装产出的证据 JSON（含 `cpu_samples`）；
- 只有 `RESOURCE-GATE-REAL` 请求资源门判定（重计算面）；
  其余为非重计算面（构建/单测/sanitizer/coverage/E2E），仅采样留证，
  不强制 `frozen_gate`（owner 裁决 F-CI-002-04/06 维持）。

### 6.3 mutates_workspace（真强制）

`mutates_workspace: true` 不再跳过工作区前后对比：可写面 = 登记 `outputs`
∪ `dirty_ignore_*`，写出该面之外 ⇒ `FAIL(dirty)`。7 个顶层声明
（CHK-SCI-REF / CHK-CONTRACT-TEST / CHK-UNIT / CHK-SYNTH-P2 / CHK-NWORKER /
LINUX-MAIN-FIXTURES / LINUX-MAIN-BUILD-TREE）与 6 个 step 声明逐条适用同一判据。

### 6.4 fail-closed 普查

`CHK-FAILCLOSED-SURVEY` 对 230 个执行单元（78 个注册项）注入「缺失证据 / 坏证据 /
无输出」三面，226 个适用面全部判红、4 个显式无适用面（`SILENT_OK_UNITS` 静默豁免 2 个 +
`TESTKIT-LIST`/`UT-CPU-AVX512` 无证据面）；普查表见
`artifacts/evidence/release-05/FAILCLOSED_SURVEY.md`。

