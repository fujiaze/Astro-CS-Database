# 调度员手册（OPERATOR）

> 面向**你的调度员 Agent**（前台）。执行 Agent 只干活、不提交；调度员负责派发、独立验收、原子提交、维护台账。

## 1. 角色与权限

| 角色 | 能做 | 不能做 |
|---|---|---|
| 负责人（你） | 批准控制包、裁决冲突、最终发布决定 | —— |
| 调度员（前台 Agent） | 派发任务、独立复跑验收门、`git add`（路径限定）+`commit`+`push`、维护 `ACCEPTANCE.md` | 代替负责人裁决、伪造 PASS、用 waiver 掩盖红灯 |
| 执行 Agent（SubAgent） | 按任务文件改限定文件域、跑本任务门、交自证材料 | commit/push/建分支/stash/reset/clean；改任务外文件 |

## 2. 每个任务的标准动作（顺序不可乱）

1. **派发**：用 `DISPATCH.md §1` 的通用提示词，替换 `<TASK-ID>`。
2. **收料**：检查自证摘要是否包含：改动文件清单、逐条门的命令+退出码+输出片段、未决项。缺项打回。
3. **独立复跑**：按任务文件「验收门」逐条自己跑一遍（`DISPATCH.md §3.2` 的复核提示词）。**不得复用执行 Agent 的输出。**
4. **验边界**：`git status --porcelain=v1` + `git diff --stat`，核对是否越出「改动范围」；与 BASE-001 的预存清单比对，确认没有把预存改动混进来。
5. **提交**（仅当全部 PASS）：
   ```bash
   cd "/workspace/Astro CS Database"
   git add <仅本任务文件域内的路径>          # 禁止 git add -A / git add .
   git commit -m "<类型>(<任务ID>): <做了什么>；依据 <权威条款>"
   git push
   git fetch && git rev-parse HEAD main origin/main   # 三 SHA 必须一致
   ```
6. **登记**：在 `ACCEPTANCE.md` 该任务行写入状态、机器门结果、证据路径、你的结论；在 `TASK_LIST.md` 更新状态标记。
7. **解锁下游**：只有依赖任务 PASS 后，才派发下游任务。

## 3. 状态口径（唯一）

- `NOT_STARTED` → `IN_PROGRESS` → `PASS` / `FAIL` / `BLOCKED`；
- `PASS` **只能**由调度员独立复跑后写入；执行 Agent 自述一律记 `IN_PROGRESS`；
- `FAIL` 必须写清哪条门红、实际输出、回退或补修方案；
- `BLOCKED` 必须写清卡点类别（文档冲突 / 科学歧义 / 缺失 / 拆分不当）与所需裁决。

## 4. 什么时候停下来找负责人

只有这五类（其余自动推进，不设频繁 checkpoint）：

1. 最高设计/文档集与现状冲突，且需要改文档而不是改代码；
2. 科学定义有歧义或两篇权威文档打架；
3. 权限/数据/环境缺失导致无法推进；
4. 不可恢复失败（例如必须重写历史才能修）；
5. 任何涉及「发布」或「放宽门禁」的决定。

## 5. 提交纪律（ENGINEERING_SPEC §6）

- 一个 commit = 一个任务 = 一个可独立验证的目的；
- 科学 / 架构 / 性能 / 文档**不混提**；
- 只 `main`；禁止分支、worktree、额外 clone、force push、amend、历史重写；
- 提交消息写「做了什么 + 依据哪条权威条款」，不写流水账、不复制文档长文；
- 每次 push 后核对 `HEAD = main = origin/main`。

## 6. 反作弊清单（每次验收必查）

- [ ] 有没有用 facade / 空实现 / no-op 冒充完成？
- [ ] 有没有把测试注释掉、跳过、删掉负例？
- [ ] 有没有改检查器让它恒绿，或放宽阈值？
- [ ] 有没有改 `docs/science/**`、`docs/algorithms/**` 的公式/容差？
- [ ] 有没有把「本任务外」的文件顺手改了（尤其根 `CMakeLists.txt`、中央 registry、`ci/checks.json`）？
- [ ] 有没有把运行产物丢在仓库根目录？
- [ ] 有没有在 Alpha 前写入版本号？
