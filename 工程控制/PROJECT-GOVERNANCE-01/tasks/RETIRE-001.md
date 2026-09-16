# 任务：RETIRE-001 旧世代打包/审计工具退役与追溯门退役登记

状态：NOT_STARTED
层：L1　依赖：TEST-GREEN-001（ci/checks.json 唯一写者交接）
文件域互斥组：S2-R（tools/ 的旧世代工具 + ci/checks.json 中相关注册项）

## 背景（前台实测 2026-09-16）

`artifacts/` 按负责人裁决整体删除（`b1290525`）后，一批**旧世代打包/审计工具**的输入路径消失：

| 工具 | 受影响形态 |
|---|---|
| `tools/pack_audit_package.py` | 输出目录 `artifacts/prerelease_v5/` 已不存在（ROOT-006 已改其白名单，但输出路径仍需处置） |
| `tools/assemble_audit.py` | 引用 `artifacts/prerelease_v5/` 树 |
| `tools/make_capsule.py` / `tools/make_rev2_capsule.py` | 生成 `artifacts/prerelease_v5/capsules/*.zip`（已删） |
| 其它 | 用 `git grep -l 'artifacts/prerelease_v5' -- tools ci tests` 实测补齐清单 |

## 目标

把「已无输入的旧世代打包/审计工具」**显式退役**（不是静默坏掉），并在 CI 注册表与 CI 规范中留下可追溯的退役记录。

## 权威依据

- ASTROCS_DESIGN.md §0（权威链：旧世代控制包产物不构成判据）、§12（版本信息下线）；
- ENGINEERING_SPEC.md §8（每项检查必须能红能绿——**坏掉即红的门要退役或修好，不允许静默**）；
- docs/ci/01_CHECKS.md §1-§5（注册项的语义与变更流程）；
- 负责人裁决：历史版本控制包全部作废；`artifacts/` 不归档不保留。

## 改动范围（文件域）

### 允许改
- `tools/**` 中的旧世代打包/审计工具（加退役抬头，或改写为「输入缺失即 FAIL 并说明已退役」）；
- `ci/checks.json`：相关注册项的退役（**必须等 TEST-GREEN-001 交出该文件写权**；逐条记录，禁止整文件覆盖）；
- `docs/ci/01_CHECKS.md`：退役登记（依据 + 日期 + 退役后的替代路径，若有）；
- 新建 `reports/PROJECT-GOVERNANCE-01/retire/RETIREMENT_LEDGER.md`：逐条「工具/注册项 → 退役理由 → 复原方法（git 历史坐标）」；

### 禁止改
- 产品代码（`lib/**`、`cli/**`）与算法/科学文档；
- **不得删除文件本体**（一律加退役抬头保留可复跑性）；
- 不得退役**仍在活动使用**的工具（例如 `tools/quality/**` 下的检查器）——先实证其输入/消费者是否仍存在。

## 步骤

1. 用 `git grep -l` 实测补齐受影响工具清单（含 CI 注册项里指向它们的条目）；
2. 逐个判定：**输入是否已不存在**、**是否仍被 CI/脚本消费**、**是否有活动替代**；
3. 对确认退役项：加退役抬头（写明依据、日期、复原命令 `git show <commit>:<path>`）；
4. 更新 `ci/checks.json`（退役或改为「输入缺失即 FAIL」的显式语义）；
5. 更新 `docs/ci/01_CHECKS.md` 与新建 RETIREMENT_LEDGER；
6. 复跑：`python3 ci/validate_registry.py --registry ci/checks.json --strict` → error_count=0；对每个退役项给「现在跑会怎样」的实测输出（应为明确 FAIL/退役文案，不是 traceback 崩）。

## 验收门（前台独立复跑）

- [ ] 受影响工具清单完整（`git grep -l 'artifacts/prerelease_v5'` 前后对照）
- [ ] 每个退役项：退役抬头 + 依据 + 复原命令，且**文件仍在**
- [ ] `ci/validate_registry.py --strict` error_count=0
- [ ] 退役项实跑输出为**明确失败/退役文案**（无未捕获 traceback）
- [ ] RETIREMENT_LEDGER 覆盖全部退役项（计数一致）
- [ ] 未越界改动产品代码（`git status` 自查）

## 交付物

1. 工具退役改动；2. 注册表与规范更新；3. RETIREMENT_LEDGER；4. 自证摘要。