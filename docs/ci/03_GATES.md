# CI 门禁与状态（Gates & States）

## 1. 门禁原则

- P0 红灯无豁免；P1 红灯须负责人登记豁免（`ci/exemptions.json`，只减不增）；
- 豁免只豁免"检查项"，不豁免科学/工程硬约束；
- 机器门禁通过后自动推进，不设频繁人工 checkpoint。

## 2. 状态语义（唯一口径，与最高设计 §11.3 一致）

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
（语义继任者 `AGENTS-GOV`）。逐条依据与重新注册前置条件见 `01_CHECKS.md §2.3`；机器可读面见
`ci/id_migration_map.json::reserved_targets`。**保留一个不存在的 P0 门 = 把假绿写进本表。**

## 4. 禁止事项

- 用 waiver 掩盖红灯（未被授权豁免的条款不可豁免）；
- 把"能编译"当"验收过"（必须跑断言/测试/机器门）；
- 合成测试标成 VERIFIED；
- 用历史版本全量重算代替科学 Oracle。

## 5. 可豁免门（waivable）的护栏：skip 必须可见，且不得变成永不验证

CI 合同允许「本机环境不满足」的执行单元以 **exit 77** 结束，由运行器记为
`SKIPPED(waivable)` 而不是红灯（`ci/run_checks.py` 的 `SKIP_EXIT_CODE=77` 分支，
`ci/run.py` 同口径）。**只有登记为 `waivable: true` 的执行单元**享有该语义；
非 waivable 的执行单元以 77 结束一律判 **FAIL**（fail-closed，见 W4-A3 / LEDGER-CI
工单 R1：`非 waivable 执行单元以 SKIP 退出码 77 结束：合同化 SKIP 仅适用 waivable 单元`）。

### 5.1 护栏①：skip 计数必须每次显示（不许静默跳过）

两条运行器的汇总行**无条件**打印跳过计数，为零时也打印：

```
ci/run_checks.py → verdict=PASS entries=1 steps=4 pass=3 fail=0 timeout=0 prereq=0 skip_platform=0 skip_waivable=1
ci/run_checks.py → verdict=PASS entries=1 steps=1 pass=1 fail=0 timeout=0 prereq=0 skip_platform=0 skip_waivable=0
ci/run.py        → ... skipped_waivable=N（CI_RESULT.summary.skipped_waivable）
```

任何改动都不得让该计数变成"条件打印"：看不到 `skip_waivable` 行 = 该次运行不可信。

### 5.2 护栏②：发布矩阵里至少一台机器必须**真跑** CPU 特性门

登记为 waivable 的 CPU 特性门当前只有 `UT-CPU-AVX512`（`CHK-ISA-EQ` 的执行单元，
依据 `问题扫描/_verify/scripts_v9_table.py:29`「A2 waivable + 缺 AVX-512F 即 exit 77」；
实现见 `tests/cpu/avx512/run_provider_avx512_checks.py`）。

**发布矩阵硬要求**：`fast` / `linux-main` / `windows-main` 三个 profile 的发布验证中，
**至少一台具备 AVX-512F 的机器必须真跑 `UT-CPU-AVX512` 并得到 PASS**（不是 SKIP）。
只有"本机确实没有 AVX-512F"才可以 SKIP；能在具备该特性的机器上跑却出现 SKIP，视为门禁失败
（采集零用例判红同口径）。

### 5.3 护栏③：无该环境时必须显式登记为待补 + 写明后果

**当前状态（2026-09-17，W4-A3 复核）：待补。** 现有 CI runner 与开发节点均未声明具备
AVX-512F，`UT-CPU-AVX512` 在 CI 上以 `SKIPPED(waivable)` 结束。

后果（必须一起读）：

- **CPU 提供者路径在 CI 上从未被验证** —— AVX-512 分派/提供者实现（`lib/infrastructure/benchmark/**`
  的 ISA 分派面）只经过 AVX2 与 dispatch 两组门，AVX-512 分支的编译期与运行期行为没有机器证据；
- 因此 **AVX-512 相关的绿灯不得作为发布依据**；相关改动必须在具备 AVX-512F 的机器上补跑并留证；
- 补齐方式：把一台具备 AVX-512F 的 runner 加入发布矩阵（或在本机 `lscpu` 确认 `avx512f` 后
  真跑 `python3 tests/cpu/avx512/run_provider_avx512_checks.py` 并留 `run/<task>/logs/` 证据）；
- 补齐后本节改为"已覆盖"，并把 `UT-CPU-AVX512` 的 `waivable` 保留（机器差异仍存在）但要求矩阵覆盖。

## 6. 门禁通过定义

`gates-report` 汇总：全部 P0 绿 + P1 绿（或已豁免）→ 门禁通过 → 打包发布候选；否则失败。
