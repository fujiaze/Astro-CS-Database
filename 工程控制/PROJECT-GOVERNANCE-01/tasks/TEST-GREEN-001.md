# 任务：TEST-GREEN-001 让 tests/quality 全绿（四项红灯根因闭合）

状态：NOT_STARTED
层：L1　依赖：无（但需与 CI-001 / MOD-001 串行，见「串行约束」）
文件域互斥组：S2-T（tests/quality 的失败集与 ci/checks.json 的既有错误）

## 目标

把 `python3 -m unittest discover -s tests/quality -t tests/quality` 从 **rc=1（4 failed）** 修到 **rc=0**，且不是靠跳过/放宽/删除检查项，而是**修掉根因**。

## 基线状态（前台与执行线各自实测，2026-09-16）

命令：`python3 -m unittest discover -s tests/quality -t tests/quality` → `Ran 99 tests … FAILED (failures=4, skipped=6)`；四项失败逐条：

| # | 失败测试 | 根因（已定位） | 归属 |
|---|---|---|---|
| 1 | `test_doc_line_anchors.TestBaseline.test_t01_real_repo_all_anchors_green` | 锚点歧义来自已删除的 `设计大纲/_evidence/packs/history/**`（该目录已于 `4511712b` 按负责人裁决删除）＋ C4 符号漂移 | 本任务 |
| 2 | `test_doc_machine_check.TestDocMachineCheck.test_05_command_tree_mutation_fails` | `tools/check_api_docs.py` 的 `check_command_tree` 仅在 `build/cli/astrocs` 存在时比对，缺失即**静默跳过** ⇒ 门退化为恒绿（物理清运删除 build/ 后暴露，GAP-027） | 本任务（与 CI-001 划界，见下） |
| 3 | `test_docchk002_mutation.TestDocChk002.test_01_real_repo_passes` | `docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md` 的 ADU/e⁻ 单位歧义（科学文档，需按权威口径消歧，不得改科学定义） | 本任务（文档消歧，不改公式） |
| 4 | `test_known_failures_baseline_ci.TestRegistryWiring.test_t13b_check_gate_is_last_linux_main_entry` | `ci/checks.json` 的 linux-main 末位不是 KNOWN-FAILURES-BASELINE-CHECK；且 `ci/validate_registry.py --strict` 基线本身就是 rc=1（1 error） | 本任务（注册表次序）；与 CI-001 的整体 ID 收敛**串行** |

## 权威依据

- ENGINEERING_SPEC.md §5（测试规范）、§8（每项检查必须能红能绿）；
- docs/ci/01_CHECKS.md §1-§5（检查项语义、注册顺序、fail-closed 要求）；
- AGENTS.md §5（不得以放宽检查/注释测试让门变绿）；
- ASTROCS_DESIGN.md §12（不得用声明冒充验证）。

## 改动范围（文件域）

### 允许改
- `tools/check_api_docs.py`（把「缺 build 产物即静默跳过」改为 **fail-closed**：缺产物判 FAIL 并给出明确文案）；
- `tools/doccheck/**` 中与锚点/单位歧义相关的检查器逻辑（只允许修「怎么检查」，**不允许**放宽被检查的文档内容标准）；
- `docs/ci/01_CHECKS.md`：登记 `CHK-ROOT-CLEAN`（ROOT-002 遗留待办：它在 `ci/checks.json` 已注册但文档未登记）+ 本任务新增/改名的检查项；
- `docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md`：**仅**消解 ADU/e⁻ 单位歧义（补单位说明、明确所指量），**不得**改任何公式、数值、结论；
- `tests/quality/**`：修测试自身的缺陷（若测试断言写错），但**不得**删除用例或放宽断言；
- `ci/checks.json`：**仅**修 linux-main 末位次序与既有 1 个 error（**不得**做整体 ID 收敛，那是 CI-001 的域）。

### 禁止改
- `docs/science/**` 中除上述单文件单位说明以外的任何内容；
- `ci/checks.json` 的整体 ID 重命名（CI-001 域，串行）；
- 不得通过 `skip`/`expectedFailure`/`xfail` 让失败消失；
- 不得删除 `tests/quality` 下任何现有测试文件或用例；
- 不得改 `工程控制/**`。

## 串行约束（重要）

- `ci/checks.json` 目前**冻结给 CI-001**（整体 ID 收敛）：本任务若必须先改该文件，**先报告并等前台放行**；可先做不依赖它的部分（1、2、3 三项）；
- 第 4 项与 CI-001 的收敛会互相影响：收敛后 linux-main 末位的判据可能变化，届时按**收敛后的注册表**重跑本项。

## 步骤

1. 逐项复现四个失败（保存完整输出到日志），确认根因与上表一致；不一致就报告；
2. 修 1：定位锚点歧义来源——若来源是已删除的 `设计大纲/`，把检查器的锚点范围改为**只扫现存路径**（并说明为何这不降低标准）；若还有 C4 符号漂移，按当前权威文档修正检查器的符号表映射；
3. 修 2：`check_command_tree` 改 fail-closed，并新增**负例自测**（缺产物时必须 FAIL）；
4. 修 3：在 `OBSERVATION_MODEL_REVIEW.md` 里消解单位歧义（不改数值/公式）；给出改前/改后对照；
5. 修 4：修 linux-main 末位次序，使 `ci/validate_registry.py --strict` 的 error_count 归零（若该 error 属 CI-001 域则报告）；
6. 登记 `CHK-ROOT-CLEAN` 到 `docs/ci/01_CHECKS.md`；
7. 全量复跑 `tests/quality` → 必须 `rc=0`（skipped 数量不得增加）。

## 验收门（前台独立复跑）

- [ ] `python3 -m unittest discover -s tests/quality -t tests/quality` → **rc=0**，skipped 不增加（基线 6）
- [ ] `python3 ci/validate_registry.py --registry ci/checks.json --strict` → error_count = 0 或明确说明剩余 error 归属 CI-001
- [ ] `tools/check_api_docs.py` 的负例自测：人为移走/隐藏 build 产物 → **rc≠0**（证明能红）
- [ ] 单位歧义修正仅涉及说明文字：`git diff docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md` 中无公式/数值改动（逐行核对）
- [ ] 零删除：`git diff --stat tests/quality` 无删除行（只允许修改与新增）
- [ ] `CHK-ROOT-CLEAN` 已在 `docs/ci/01_CHECKS.md` 登记

## 禁止

- 放宽/跳过/删除任何检查或用例；
- 用「环境问题」解释失败；
- 改科学定义或数值来迁就检查器。

## 交付物

1. 四处根因修复 + 负例自测；2. `docs/ci/01_CHECKS.md` 登记；3. 全量复跑 rc=0 的证据；4. 自证摘要。

## 通用纪律

- 开工前按 AGENTS.md §1 读完「权威依据」全部条款；未读不开工；
- 只改本任务声明的文件域；不顺手改无关文件；
- 不改 `docs/science/**` 与 `docs/algorithms/**` 的公式、阈值、推导；
- 不用 facade / 空实现 / 注释掉测试 / 放宽检查 / 跳过 来让门变绿；
- 不 commit / 不 push / 不建分支 / 不 stash / 不 reset / 不 clean / 不 checkout；
- 外部命令带 timeout，日志落 `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/`；
- 报告必须附命令 + 退出码 + 关键输出片段 + 产物路径；禁止用「环境问题」掩盖失败；
- **不要改** `工程控制/**`（含 GAP_AUDIT/ACCEPTANCE）——那是前台的域，你只交报告与证据；
- **不要读** `FATDUCK_ACCESS.md` 的内容（AGENTS.md §5；另一条线在处理）。

## 交付物

1. 限文件域内的改动；2. 日志与证据；3. 自证摘要（改动清单 / 逐门命令与退出码 / 未决项）。
