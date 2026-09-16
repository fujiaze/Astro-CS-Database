# CI 检查项目录（CI Checks Registry）

## 1. 注册表原则

- `ci/checks.json` 是唯一检查注册表；`ci/` 提供确定性执行器；
- 每项检查必须可"能绿能红"（有正例与负例）；
- 豁免显式登记 `ci/exemptions.json`，只减不增，需负责人批准；
- **可执行负例面**（`ENGINEERING_SPEC.md §8`）：每项检查必须提供**机器可执行**的负例入口
  （`--self-test` 或 `--fault-inject`）；仅有人工说明不算。检查器的注册步骤中必须能看到该入口；
- **fail-closed**：输入缺失 / 路径不存在 / 依赖不可用时必须**判红**，不得崩溃后静默通过，
  也不得把「文件不存在」当「无违规」（`scanned == 0 ⇒ rc != 0`）；
- **锚存活**：判据里硬编码引用的仓库路径必须存在；失效时以 `ANCHOR_STALE: <常量名> <路径>`
  **显式失败并点名**，不得 traceback、不得静默降级；
- **注册表双向一致**：`ci/checks.json` 与本文件 §2 必须双向对齐（既不得「注册未登记」，
  也不得「文档承诺 P0 但无实现」），由 `CHK-REGISTRY-DOC-SYNC` 机器保证；

## 2. 检查项清单

| ID | 类别 | 名称 | 命令/入口 | 门禁 ||---|---|---|---|---|
| CHK-BUILD-LINUX | 构建 | Linux Release 构建 | `cmake -S . -B build && ninja -C build` | P0 |
| CHK-BUILD-WIN | 构建 | Windows Release 构建 | VS 工具链 + CMake preset | P0 |
| CHK-WARN | 静态 | 编译警告 | MSVC /W4 /WX · GCC -Wall -Werror | P0 |
| CHK-STATIC | 静态 | 静态分析 | 选定分析器（clang-tidy 等） | P1 |
| CHK-MODULE-MANIFEST | 文档一致性 | 模块 manifest/注册表/构建 target/产品清单一致 | ci 检查器 | P0 |
| CHK-CONTRACT-REF | 文档一致性 | 端口引用有效 DATA 合同 | ci 检查器 | P0 |
| CHK-SCI-REF | 文档一致性 | 算法引用有效 SCI/ALG | ci 检查器 | P0 |
| CHK-CONTRACT-TEST | 文档一致性 | 核心合同有独立测试 | ci 检查器 | P0 |
| CHK-DANGLING | 文档一致性 | 删除/重命名无悬空引用 | ci 检查器 | P1 |
| CHK-STALE-DOC | 文档一致性 | 活动文档无陈旧版本号/历史状态冒充 | ci 检查器 | P1 |
| API-DOCS | 文档一致性 | doc↔code 命令树/签名/退出码/schema 一致（命令树：CLI 产物候选缺失即 fail-closed） | `tools/check_api_docs.py` | P0 |
| CHK-ROOT-CLEAN | 目录规范 | 仓库根目录整洁（§7 白名单 / 运行产物落根 / 必需条目缺失） | `tools/quality/check_root_cleanliness.py` | P0 |
| CHK-UNIT | 单元 | 每模块单测 | `ctest --test-dir build`（模块级） | P0 |
| CHK-ORACLE | 模块数值 | SCI/ALG Oracle 测试 | 模块测试 target | P0 |
| CHK-INVARIANT | 模块数值 | 科学不变量/性质测试 | 模块测试 target | P0 |
| CHK-ABI | 合同/ABI | C ABI 兼容性 | ABI 检查器 | P0 |
| CHK-SCHEMA | 合同/ABI | schema 校验 | schema validator | P0 |
| CHK-SYNTH-P1 | 科学 | normalize 合成全链 | 合成全链测试 | P0 |
| CHK-SYNTH-P2 | 科学 | mosaic 合成全链 | 合成全链测试 | P0 |
| CHK-SYNTH-P3 | 科学 | export 合成全链 | 合成全链测试 | P0 |
| CHK-ISA-EQ | 科学 | baseline/AVX2/AVX-512 等价 | ISA 等价测试 | P1 |
| CHK-NWORKER | 科学 | 1 vs N worker 数值一致 | 并行一致性测试 | P0 |
| CHK-SANITIZER | 资源 | ASan/UBSan | sanitizer 构建测试 | P1 |
| CHK-COVERAGE | 资源 | 覆盖率报告 | 覆盖率工具 | P2（报告） |
| CHK-RESOURCE | 资源 | 内存/线程/利用率门禁 | 资源监控测试 | P0 |
| CHK-PACKAGE | 打包 | 发布候选打包/白名单/哈希/版本/provenance | 打包脚本 | P0 |
| CHK-SECRET-HYGIENE | 安全 | 凭据/密钥卫生（tracked 全域扫描） | `python3 tools/quality/check_secret_hygiene.py --scope tracked …` | P0 |
| CHK-ENV-ADOPTION | 环境 | CI 环境接管基线（工具链策略/锁一致性） | `python3 ci/run_checks.py --check CHK-ENV-ADOPTION --quiet` | P1 |
| AGENTS-GOV | 治理 | AGENTS.md 硬禁令 / §0 权威链唯一性 / 旧权威回归 | `python3 tools/check_agents_gov.py` | P0 |
| ENG-CONSTRAINTS | 治理 | 工程约束（§7 目录规范 / 根条目白名单 / 旧权威回归） | `python3 tools/doccheck/check_engineering_constraints.py` | P0 |
| VERSION-CONSISTENCY | 文档一致性 | 版本注入链单一真源 + 现行活动文档集完整性 | `python3 ci/check_version.py --expected 0.11.0-alpha.2` | P1 |
| VERSION-NAMESPACES | 文档一致性 | 版本命名空间一致性（陈旧版本号） | `python3 tools/doccheck/check_version_namespaces.py` | P1 |
| DOC-L0 | 文档一致性 | L0 现行文档集（docs/owner/**）与索引 active 登记完整性 | `python3 tools/check_l0_docs.py` | P1 |
| GLOSSARY-DOCS | 文档一致性 | 词典锚点/别名唯一性（报告项） | `python3 tools/check_glossary.py` | P2 |
| LINUX-MAIN-FIXTURES | 构建 | linux-main 夹具准备（wf_step） | `python3 ci/wf_step.py --step LINUX-PREPARE-FIXTURES` | P0 |
| LINUX-MAIN-BUILD-TREE | 构建 | linux-main 根构建图（wf_step） | `python3 ci/wf_step.py --step LINUX-BUILD-ROOT-GRAPH` | P0 |
| WIN-CANDIDATE-VALIDATE | 打包 | Windows 候选校验（wf_step） | `python3 ci/wf_step.py --step WINDOWS-VALIDATE-CANDIDATE` | P0 |
| STD-REG | 标准 | 标准注册表 C1–C8 判据 + 9 场景 fault-inject 负例面 | `python3 ci/run_checks.py --check STD-REG --quiet` | P1 |
| CHK-REGISTRY-DOC-SYNC | 治理 | 注册表 ↔ 本文件 §2 双向一致（§8） | `python3 ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC --quiet` | P0 |
| RESOURCE-GATE-REAL | 资源 | 真实重计算面利用率门（显式 --gate-required + 判定证据） | `python3 ci/resource_monitor.py --timeout 300 …` | P0 |
| RESOURCE-GATE-REAL-NEG | 资源 | 上项的可执行负例面（串行注入 ⇒ 门必须判红） | `python3 tools/quality/check_resource_gate_real.py --fault-inject serial --seconds 20` | P0 |
| CHK-KNOWN-FAILURES-BASELINE | 测试 | 版本化已知失败基线门（聚合型，linux-main 末位） | `python3 ci/run_checks.py --check CHK-KNOWN-FAILURES-BASELINE --quiet` | P1 |

> **§8 判据登记：`AGENTS-GOV` / `ENG-CONSTRAINTS` 的「非绑定标记豁免」粒度 = 行级（2026-09-16，负责人裁决 B）。**
> 两门的「唯一最高权威」判据原用**整文件豁免**（`if "ARCHIVED_NON_NORMATIVE" in text: continue`）：
> 触发条件是文件内**提到**该字样，而非**自称归档** ⇒ `memory.md` 只在第 8/12/73 行说「别的东西已归档」
> 就被整文件跳过，「`memory.md` 自称唯一最高**权威**」（写法说明：本登记文自身也被同一判据扫描，故在「最高」与「权威」之间加 Markdown 强调断开字面匹配，
> 避免登记文自我触发；判据正则仍为「唯一 + 最高 + 权威/约束/规范/文档」连续匹配）**永远判不出来**（与 R-6 §3.5「空触发」同一失效型，属**假绿来源**）。
> 现行判据：只有**该行自身**带非绑定标记（`NON_BINDING_MARKERS`：ARCHIVED / 已删 / 已删除 / 已归档 / 历史参照…）才跳过该行，
> 与两门其余判据（`legacy_hits` / `legacy_object_body_hits`）**本来就用的行级口径**一致。
> 双向证据（本任务 `run/PROJECT-GOVERNANCE-01/CI-003/logs/B_*`）：真仓 rc=0 且 `scanned=291`；
> 注入「`memory.md` 自称唯一最高**权威**」（写法说明：本登记文自身也被同一判据扫描，故在「最高」与「权威」之间加 Markdown 强调断开字面匹配，
> 避免登记文自我触发；判据正则仍为「唯一 + 最高 + 权威/约束/规范/文档」连续匹配）⇒ **rc=1 `single_authority_entry: memory.md:114`**（指名行号）；
> `--self-test` 计数不变（`AGENTS-GOV` 4 正 6 负 / `ENG-CONSTRAINTS` 3 正 8 负）。
> 两门的 `changed_paths` 已把 `memory.md` 登记为**真实触发面**（此前是空触发）。

### 2.1 退役记录（只减不增；每条必须写依据与日期）

| 退役项 | 日期 | 依据 | 处置 | 可复跑性 |
|---|---|---|---|---|
| `TASK-RESULT-SCHEMA`（`CHK-SCHEMA` 的原 step；检查器 `tools/quality/check_task_result_schema.py`） | 2026-09-16 | 该 step 的**唯一默认输入**是 `evidence/v6_1_rework/tasks/*/TASK_RESULT.json`（V6.1 REWORK 世代证据树）：该树已随旧世代删除，且 `ENGINEERING_SPEC.md §7`（2026-09-16）明令根下**不得**新建 `evidence/` ⇒ 在**任何**检出上恒红（实测 `TASK-RESULT-SCHEMA FAIL rc=1`，零输入 fail-closed）；现行世代无 `TASK_RESULT.json` 生产者（`find 工程控制 -name 'TASK_RESULT*.json'` 零命中） | 从 `ci/checks.json` 的 `CHK-SCHEMA.steps` 退役（保留 `CON-CONFIG-CONTRACTS`、`LOG-CONTRACT-SELFCHECK`）；检查器文件**不删** | 有：`python3 tools/quality/check_task_result_schema.py --results-dir <任一含 TASK_RESULT.json 的树>` 仍全量校验；`git show HEAD:tools/quality/check_task_result_schema.py`。**能力去向**：新世代控制包若定义 TASK_RESULT 证据落位（`artifacts/**`），按 §4 流程以显式 `--results-dir` 重新注册 |
| `WORKSPACE-ADOPTION` / `RECONCILE-STATE`（`CHK-ENV-ADOPTION` 的原 2 个 step；检查器 `ci/verify_workspace_adoption.py`、`ci/reconcile_state.py`） | 2026-09-16 | 两 step 的唯一输入是 `evidence/v8_1_ci_control/**`（V8.1 控制包世代的证据树）：该树已随旧世代删除，且 `ENGINEERING_SPEC.md §7`（2026-09-16）明令「证据与产物落 `artifacts/`；根下**不得**新建 `evidence/`」⇒ 两门在**任何**检出上都必然红（实测 `[FAIL] 证据文件缺失或为空: evidence/v8_1_ci_control/adoption/*`、`Errno 2 ... TASK_STATE.json`），属 §2.2 口径的「坏掉即红的门」 | 从 `ci/checks.json` 的 `CHK-ENV-ADOPTION.steps` 退役（保留 `TOOLCHAIN-VERIFY`，实测 rc=0）；两个检查器文件**不删** | 有：`git show HEAD:ci/verify_workspace_adoption.py`、`git show HEAD:ci/reconcile_state.py`。**能力去向**：待新世代控制包定义「工作区接管 / 任务-状态对账」的证据落位（`artifacts/**`）后，按 §4 流程以可执行负例重新注册 |
| `TRACEABILITY-CODE`（`python3 tools/check_traceability.py`，原三 profile / `waivable=false`） | 2026-09-16 | 负责人裁决。① 本规范不含任何追溯要求（TRACEABILITY/追溯 零命中）；② `ASTROCS_DESIGN.md` 与 `ENGINEERING_SPEC.md` 同样零命中「追溯」；③ 该门**唯一默认输入** `artifacts/prerelease_v5/tables/TRACEABILITY.csv` 位于构建产物目录（从来不是权威落位），已随 `artifacts/` 按负责人裁决删除（commit `b1290525`「不归档、不保留」）；④ 表内容锚在 `docs/VERSIONING.md` 的版本串匹配上，而新设计 §12 明令版本信息下线 ⇒ 口径被新世代废止；⑤ 内容未丢：`git show b1290525^:artifacts/prerelease_v5/tables/TRACEABILITY.csv` | `ci/checks.json` 移除该注册项（147→146）；`ci/impact_map.json` 清 26 处悬空引用；`ci/tests/test_impact_map.py` 的「SCI→TEST 追踪」必含类保留 `TRACEABILITY-MATRIX`/`CON-TRACEABILITY`；`tools/check_traceability.py` 加退役抬头，**文件不删** | 有：`python3 tools/check_traceability.py <claims.csv>` 仍按 R1–R7 全量校验（夹具示例 `tests/quality/fixtures/docchk002_claims_fixture.csv`，11 claim PASS）；无参调用打印 `TRACEABILITY_RETIRED` 并 **exit 2**（不再回退被删快照，不伪装绿） |

其余追溯类注册项**未退役**（2026-09-16 实测保留）：`TRACEABILITY` rc=0、`PIPELINE-TRACE` rc=0；
`CON-TRACEABILITY` rc=1（`TRACE-CORE-MISSING`：PSF/REJ 核心 SCI 未入表，P1）；
`TRACEABILITY-MATRIX` rc=1（矩阵 7 条 error：BOM / ID 格式 / 悬空测试路径，P1）；
`UT-TRACEABILITY` rc=1（2 failures：一为已退役门的旧消费者
`tests/traceability/test_traceability.py::test_01`，一为上述矩阵失败）。
后三项红灯属既有技术债，另行处置，不在本次退役范围。

### 2.2 工具层退役记录（非注册项，只减不增；每条必须写依据与日期）

RETIRE-001（2026-09-16）退役旧世代（V5 控制包）打包/审计工具。**全部只加退役抬头，文件本体不删**（保留可复跑性）。
统一判定口径：① 权威链 `ASTROCS_DESIGN.md §0`（旧世代控制包产物不构成判据）+ 负责人裁决「历史版本控制包全部作废；
`artifacts/` 不归档不保留」（commit `b1290525`）与 `ASTROCS_DESIGN.md §12`（版本信息下线）；② `ENGINEERING_SPEC.md §8`
（坏掉即红的门要退役或修好，**不允许静默坏掉**）。

| 退役项 | 日期 | 依据 | 处置 | 可复跑性 |
|---|---|---|---|---|
| `tools/assemble_audit.py` | 2026-09-16 | 唯一输入 `工程控制/旧发布控制包（已删除）/` 与 `artifacts/prerelease_v5/tables/` 均不存在；实测未捕获 `FileNotFoundError` 且残留空目录 | 加退役抬头；入口改「显式失败」：打印 `ASSEMBLE_AUDIT_RETIRED` 并 exit 2；原实现保留为 `legacy_main()` | 有：`git show 01754fab8618:tools/assemble_audit.py` |
| `tools/make_capsule.py` | 2026-09-16 | 输出 `artifacts/prerelease_v5/capsules/` 已随 `artifacts/` 删除；无参 `IndexError`，带参则静默把 zip 写回已退役路径 | 加退役抬头；入口改「显式失败」：`MAKE_CAPSULE_RETIRED` + exit 2；原实现保留为 `legacy_main()` | 有：`git show 01754fab8618:tools/make_capsule.py` |
| `tools/make_rev2_capsule.py` | 2026-09-16 | 输入三表（`TRACEABILITY/COMMITS/REVIEW_CAPSULE_INDEX.csv`）被静默跳过，输出目录已删 → 实测未捕获 `FileNotFoundError` | 加退役抬头；入口改「显式失败」：`MAKE_REV2_CAPSULE_RETIRED` + exit 2；原实现保留为 `legacy_main()` | 有：`git show 01754fab8618:tools/make_rev2_capsule.py` |
| `tools/pack_audit_package.py` | 2026-09-16 | 打包入口产物落已退役 `artifacts/prerelease_v5/`；实测目录被旁路重建时**静默产出 11.2 MB / 2709 条目且超 10 MB 目标仍 exit 0**，目录不存在则 traceback | **仅退役打包入口 `main`**（`PACK_AUDIT_PACKAGE_RETIRED` + exit 2）；`allowed()/denied()/EXCLUDE_EXT` **保留为活动依赖**（`CHK-SECRET-HYGIENE` 的收录白名单与凭据排除真源），import 语义不变 | 有：`git show 01754fab8618:tools/pack_audit_package.py` |

实测复核（2026-09-16，RETIRE-001 自证）：四个退役项调用均为「明确退役文案 + exit 2，无未捕获 traceback」；
`python3 tools/quality/check_secret_hygiene.py --scope pack` rc=0、`tests/quality/test_secret_hygiene.py` 22/22 OK，
证明 `pack_audit_package.py` 保留函数的活动依赖未受影响。

**经实证不退役（保留原样，不计入退役数）**：`tools/quality/known_failures_baseline.py`（被 `KNOWN-FAILURES-BASELINE`、
`-VERIFY`、`-CHECK` 三个注册项消费，仍产出 `artifacts/KNOWN_FAILURES_BASELINE.json`）；`tools/check_traceability.py`
（已按 GAP-032 于 §2.1 登记）。逐条判定、消费者实测与复原坐标见
`reports/PROJECT-GOVERNANCE-01/retire/RETIREMENT_LEDGER.md`。

### 2.3 RESERVED（文档曾承诺但**无实现**；只减不增）

> 依据：`ENGINEERING_SPEC.md §8`「可执行负例面」+ 本文件 §2.2 统一口径「坏掉即红的门要退役或修好，
> **不允许静默坏掉**」。本表项**不得**出现在注册表 `ci/checks.json`（由 `CHK-REGISTRY-DOC-SYNC`
> 的 R4 判据机器保证）；重新注册前必须① 有实现 ② 有可执行负例入口（`--self-test`/`--fault-inject`）。
> 机器可读面见 `ci/id_migration_map.json` 的 `reserved_targets`。

| RESERVED 项 | 日期 | 依据 | 处置 | 重新注册的前置条件 |
|---|---|---|---|---|
| `CHK-FMT` | 2026-09-16 | 原文档（本文件 §2、`02_PIPELINE.md:27`、`03_GATES.md:24`）承诺为 **P0 格式门**，但 `ci/checks.json` **零实现**；宿主实测 `clang-format` 未安装（`clang-format: 未找到命令`）⇒ 注册即恒红或恒 SKIP，属**假绿**（R-6 §4.1） | 从 P0 门禁表退出，登记 RESERVED；`ci/id_migration_map.json::reserved_targets.CHK-FMT` 同步 | `clang-format` 纳入 `ci/toolchain.policy.json` + 新增 checker（须带 `--self-test`：用注入桩验证「能红能绿」），并按 §4 流程注册 |
| `CHK-DUAL-TOL` | 2026-09-16 | 原文档承诺 P1「双平台允许误差」，注册表零实现；需 Windows 侧可比候选产物（`REAL-001` 面） | 同上（RESERVED，不进 P0/P1 门禁表） | Windows 复验面产出可比候选 + checker 带可执行负例 |
| `CHK-AGENT-HARD-RULES` | 2026-09-16 | 原文档承诺 **P0**「AGENTS.md 硬禁令存在」，注册表零实现；**语义继任者 = `AGENTS-GOV`**（已注册 P0，实测承接全部判据） | 从 P0 门禁表退出；文档与门禁表改指 `AGENTS-GOV` | 若重新注册，必须与 `AGENTS-GOV` 有**非重复**判据，否则应维持退役 |

---

## 3. 门禁分级

| 级 | 含义 | 处理 |
|---|---|---|
| P0 | 硬门禁 | 红灯阻塞合并，无 waiver |
| P1 | 硬门禁（可负责人豁免） | 红灯阻塞；豁免须显式登记 |
| P2 | 报告项 | 不阻塞，但需留存结果 |

## 4. 新增检查项流程

1. 在 `ci/checks.json` 登记（ID/命令/门禁级/正例负例）；
2. 提供可"能绿能红"的正例与负例测试；
3. 本地复跑确认；
4. 合入 CI 流水线。

## 5. 运行方式（本地）

```bash
# 全量
python3 ci/run_checks.py --all
# 指定项
python3 ci/run_checks.py --check CHK-ROOT-CLEAN CHK-UNIT
# 输出机器可读 JSON 供 CI 消费
python3 ci/run_checks.py --all --json-out ci_result.json
```
