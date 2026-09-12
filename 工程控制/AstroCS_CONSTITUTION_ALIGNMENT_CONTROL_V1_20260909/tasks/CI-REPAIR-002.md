# CI-REPAIR-002｜CI 修复常驻线 round2（复跑回拉 + 归因维护 + 基线退出）

## 目标
承接 `CI-REPAIR-001` 建立的常驻线机制（`ci/ci_repair_round.py`），执行 round2 及之后的轮次维护：

1. **复跑回拉**：对本轮各原子任务落地后的 SHA 逐一执行
   `python3 ci/ci_repair_round.py --repo fujiaze/Astro-CS-Database --sha <SHA> --round-dir run/ci_repair/round<N> --monitor-dir run/ci/monitor`，
   产出 `FINDINGS.json` / `SUMMARY.md` / `PULL.json` / `raw/`（fail-closed 语义不得绕过）；
2. **归因映射维护**：`ci/ci_repair_round.py` 的归因域字典必须随本轮新增检查项（`CTEST-*`、`KNOWN-FAILURES-*`、`STANDARDS-*` 等）同步，未登记归因域一律 `domain=UNKNOWN + needs_mapping=true`，不得静默丢弃；
3. **known-failures 基线只减不增**（裁决 R-05）：逐条核对 `ci/known_failures.json` 现有条目——`UT-CLI`、`p1_noise_adapter`——在对应根因任务（`UT-CLI-MAINT` / `MOD-001A`）闭环后**执行移除**，并验证 `KNOWN-FAILURES-BASELINE-CHECK` 在失败集收敛后转绿；**禁止**为了转绿而新增基线条目；
4. **注册挂账清理**：`CI-REPAIR-ROUND` 检查项注册（`CI-REPAIR-001` 在制期间被前台裁定豁免同提交）须在 `ci/checks.json` 单写者静默后补提交落地；
5. 轮报增量写入 `05_FINDINGS_REGISTER_20260911.md`（前台负责并入，本任务只产出 `run/ci_repair/FINDINGS_REGISTER_INCREMENT.md`）。

## 依赖
`CI-REPAIR-001`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `run/ci_repair/`
- `ci/`
- `tools/quality/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-01**：本任务改归 `repo-write` lane（`ci-repair` lane 停止派发新任务，维护宪章 §14.5「同一工作区 tracked 文件写入必须串行」）。
- 裁决 **R-05**：known-failures 基线**只减不增**；P0/P1 一律修复，禁止扩基线掩盖。
- 宪章 §17.7 同 SHA 双虚拟机 CI 通过；§14.5「不设置频繁人工 checkpoint，机器门禁通过后自动推进」；§12.3 机器一致性检查。

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- **严禁以"放宽检查规则 / 加入 known-failures 基线 / 改 waivable / 跳过测试"的方式让红灯消失**（裁决 R-05、R-13）。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、`docs/standards/STANDARDS_REGISTRY.md`、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→`ci/checks.json` 显式检查项；漂移锚→复测工具。
5. 修复后必须**本地复现 CI 同构条件**验证（不得只在本机默认环境跑绿即报 PASS）。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 本任务对应的 CI 检查项在本地同构条件下 **exit 0**，并给出命令与日志路径。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
