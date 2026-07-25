# AstroCS 工程控制包：自治执行入口

本目录是 AstroCS 后续长期开发的工程控制中枢。它管理项目状态、任务依赖、数据与接口契约、测试证据、独立复核、阶段 Gate 和发布流程。

## 用户侧使用方式

用户只需要完成两件事：

1. 把 `AstroCS_Autonomous_Agent_Pack_*.zip` 放到 AstroCS 项目目录；
2. 把压缩包根目录 `AUTONOMOUS_START_PROMPT.txt` 中的提示词发送给具备终端和文件操作能力的 Agent。

用户不需要手工解压、把目录改名为 engineering、运行 PowerShell、选择下一任务或逐项回复“继续”。

## Agent 自动完成的事项

Agent 必须自动：

- 查找并校验压缩包；
- 解压到临时目录；
- 安装或恢复 `engineering/`；
- 执行控制包自检；
- 读取当前状态和任务；
- 完成实现、测试、证据归档与独立复核；
- 自动推进依赖已满足的下一任务；
- 在运行环境结束前保存检查点；
- 只在真实硬阻塞或全部任务完成时停止。

## 当前阶段

初始状态从 `P00-001` 开始。它只做仓库基线预检，不修改业务算法。后续顺序由 `control/MASTER_TASK_REGISTER.csv` 的依赖自动决定：

`P00 基线` → `P01 可复现构建` → `P02 数据契约` → `P03 接口契约` → `P04 测试基础设施` → `P05 Stage 1 验证` → `P06 Stage 2 系统调试` → `P07 性能与稳定性` → `P08 发布与演进`

## 每次恢复

重复发送同一条自治启动提示词即可。Agent 必须保留已有 `engineering/` 状态，不得用压缩包初始状态覆盖进度，并按 `agent/SESSION_RESUME_PROMPT.md` 恢复。

## 用户只需查看的三个文件

- `control/CURRENT_WORK.md`：当前任务或阻塞；
- `control/PROJECT_STATE.yaml`：项目阶段和状态；
- `control/MASTER_TASK_REGISTER.csv`：全部任务、依赖和证据状态。

## 状态转换工具

Agent 应优先使用：

```text
python engineering/tools/task_controller.py status
python engineering/tools/task_controller.py submit --task <TASK_ID> --evidence-dir <EVIDENCE_DIR>
python engineering/tools/task_controller.py approve --task <TASK_ID> --review-report <REVIEW_REPORT>
```

这可避免任务注册表、PROJECT_STATE 和 CURRENT_WORK 彼此失配。
