# QA 轴审计报告（ROOT-004 扩展轴 · 测试矩阵与验证层级）

- **轴名**：QA（测试矩阵与验证层级：ctest/gtest_discover_tests 盲区、skip 面、恒真断言、双平台差、sanitizer/coverage、VERIFIED 铸造面）
- **基线核对（开工第一步，timeout 30 git rev-parse HEAD main origin/main）**：
  - 声明基线：`2c328348304d033aecfa81faf79d1c6cd802b30a`
  - 实测（第 1 次）：HEAD=`4fc3e898…` main=`4fc3e898…` origin/main=`4511712b…` —— **三者互异，且均≠声明基线**
  - 实测（第 3 次，约 1 分钟后）：HEAD=main=origin/main=`900916fb…` —— 审计窗口内 HEAD 两次前移、远端被并行推送
  - `git status --porcelain`：20 个已跟踪文件被修改 + 138 个未跟踪条目；`git rev-list --count 2c32834..HEAD` = 7
  - 结论：**基线不一致 → 按硬禁令立即停止，本轴实质审计未执行。**
- **方法**：仅执行了只读 git 核对命令（见 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/QA.log`）；未读取权威文档全文、未扫描测试树、未跑任何检查器。
- **摘要（≤10 行）**：
  1. 声明基线 2c32834 落后当前 HEAD 至少 7 个提交，HEAD≠origin/main，三 SHA 不一致。
  2. 审计进行中该仓库仍被其它 agent/前台持续向 main 提交并 push（4fc3e898→d2b955cb→900916fb），工作树另有 20 M + 138 ??。
  3. 在此漂移下，任何"path:line 重新定位"证据在写出瞬间即可能失效，QA 轴六项扫描（ctest 盲区/skip/恒真断言/双平台/sanitizer/VERIFIED 面）全部**未启动、未产出、不立条**（禁止编造证据）。
  4. 建议 ROOT 编排方：冻结基线（约定新 SHA 或 `git stash` 级隔离+锁）后重新派发本轴；TEST-GREEN-001（tests/quality 四项红灯）与本轴"能红能绿/禁 skip 掩盖"高度相关，重审时优先对照。
- **发现计数**：P0 = 0，P1 = 0，P2 = 0（本轴零发现，原因=基线核对失败触发停止，非"干净"结论）

| ID | 定位 | 违反条款 | 当前证据 | 严重度 | 影响 | 整改建议 | 文件域 | 验收门 | GAP/任务关系 | 旧清单同源 |
|----|------|----------|----------|--------|------|----------|--------|--------|--------------|------------|
| —  | —    | —        | 本轮按停止条件终止，未进入取证阶段 | — | — | — | — | — | — | — |

> 备注（非发现条目）：GAP_AUDIT.md 本身在脏工作区中被并行修改（git status 显示 `M 工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md`），对照面内容不可信快照；且 HEAD 之上有新提交 d2b955cb 新开 TEST-GREEN-001 共 33 任务、并登记 GAP-026/027/028——均晚于本包 TASK_LIST 的 31 任务口径。重派时以冻结后的树为准。

*生成时间：2026-09-16T07:45:47.482Z；产物日志：run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/QA.log*
