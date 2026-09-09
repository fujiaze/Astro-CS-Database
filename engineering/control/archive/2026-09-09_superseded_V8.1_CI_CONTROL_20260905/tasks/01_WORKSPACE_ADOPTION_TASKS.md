# ADOPT 原地接管任务逐项清单

## V81-ADOPT-001 控制包与根目录

- 在控制包目录运行 validator 和负向 selftest，保存命令、exit code、包 SHA256。
- 从 Agent 当前工作目录运行 `git rev-parse --show-toplevel`；不得扫描服务器寻找其他副本。
- 确认当前分支为 main；记录现有 worktree，但不新增、不删除。
- 输出 `WORKSPACE_IDENTITY.json`；不修改项目源码。

## V81-ADOPT-002 预存状态与远端关系

- 保存 `git status --porcelain=v2`、diff stat、HEAD/main/origin/main、submodule 和脱敏 remote。
- 对每个预存修改记录来源/归属；绝不自动 reset、stash、clean 或覆盖。
- fetch 后计算 HEAD 与 origin/main 的 ahead/behind：
  - 同步：继续；
  - 工作树干净且只 behind：`pull --ff-only`；
  - 只 ahead：继续验证，之后正常 push；
  - diverged 或 dirty 冲突：写精确 blocker，但继续所有不相交只读任务。
- 输出 `PREEXISTING_CHANGES.md` 和 `REMOTE_RELATION.json`。

## V81-ADOPT-003 任务与证据对账

- 当前仓库已有 TASK_STATE/提交/证据优先，再外连接 V7.1 台账和审核快照。
- claimed CLOSED 只做当前 SHA 快检，不跑历史全量数据。
- 消解缺失 `CLI-002`；旧 DISPATCHED 只有存在当前可验证修改时才继承。
- 输出 `TASK_STATE.json`、`COMMIT_LEDGER.jsonl`、`STATE_RECONCILIATION.csv`。

## V81-ADOPT-004 工具链盘点

- 读取现有 CMake presets、AGENTS 和环境脚本，不重新配置服务器。
- 记录现有 CMake、编译器、Ninja/Make、Python、ccache 和分析工具实际版本。
- 缺少非关键工具时继续不依赖任务；需要管理员权限时只记录一条准确命令，不自行创建用户或新主机布局。
- 输出 `ci/toolchain.lock.json`。

## V81-ADOPT-005 治理目录与活动索引

- 读取根 `AGENTS.md`、`memory.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md` 和文档索引。
- 沿用当前控制、证据、日志和归档目录规范。
- 旧控制/审核材料仅按既有规范归档，不移动源码，不删除用户资料。
- 若缺索引，只补最小活动索引；不重写科学内容。

## V81-ADOPT-006 Alpha版本统一

- 将 `VERSION`、CMake `project()`、CLI、product manifest 和活动顶层文档统一为 `0.11.0-alpha.2`。
- 活动文档不得把旧 SHA/版本写成当前值；历史归档保持原样。
- 增加机器测试并用负向样例确认版本漂移会非零退出。
