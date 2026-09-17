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
| CHK-SCI-REF | 文档一致性 | 算法引用有效 SCI/ALG；含 ACR/编排层退出面（`ACR-DORMANT` = `tools/check_legacy_exit.py`，LEG-002..004）| ci 检查器 | P0 |
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
| CHK-PKG-CONSISTENCY | 打包 | 产品清单/安装树合同/依赖锁/安装规则/许可登记面一致 + SBOM 实树 hash 自证（含 -NEG 负例面） | `python3 ci/run_checks.py --check CHK-PKG-CONSISTENCY --quiet` | P0 |
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
| CHK-IMPACT-MAP | 治理 | `ci/impact_map.json` 判据一致性（id 两层闭包 / fast 候选 / BASE 核心 / 路径域锚存活与覆盖 / 无退役引用 / 结构完整） | `python3 ci/run_checks.py --check CHK-IMPACT-MAP --quiet` | P0 |

### 2.1 检查器退役与预留

- 检查器退役：从 `ci/checks.json` 移除注册项，检查器文件保留可复跑性；退役检查器无参调用时打印退役标识并 exit 2；
- 工具入口退役：退役工具的 `main` 打印退役标识并 exit 2，原实现保留为 `legacy_main()`，仍被其他检查器消费的函数保持活动语义；
- 已退役检查器：`TASK-RESULT-SCHEMA`、`WORKSPACE-ADOPTION`、`RECONCILE-STATE`、`TRACEABILITY-CODE`；退役明细与复原坐标以仓库 git 历史与 `reports/` 台账为准；
- RESERVED（文档登记但无实现，重新注册前须先有实现与可执行负例）：

| RESERVED 项 | 重新注册前置条件 |
|---|---|
| `CHK-FMT`（格式门） | clang-format 纳入 `ci/toolchain.policy.json` + checker 带 `--self-test` |
| `CHK-DUAL-TOL`（双平台误差） | Windows 侧可比候选产物 + checker 带可执行负例 |
| `CHK-AGENT-HARD-RULES` | 语义由 `AGENTS-GOV` 承接；重新注册须有非重复判据 |

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
