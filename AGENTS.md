# AstroCS AGENTS.md
## 基本规则
使用中文进行分析、开发注释、汇报。
保持：
科学定义 = 算法 = 接口 = 代码 = 测试。
## AstroCS 开发与节点
中文分析、开发、提交和汇报。只在 main 原子提交并立即 push，禁止分支及破坏性 Git。仅支持 amd64。vm-bj Linux 负责静态、文档、合成小测和调度；Fatduck 在线时负责 Windows 编译、benchmark、真实数据和重计算，离线不阻塞 Linux 任务。发布每个平台仅一个 astrocs CLI，Phase1/2/3 由 CLI 调用；未来 Windows GUI 只控制 CLI。ACR 暂不接入，生产仅纯 CPU 自适应 backend。重计算自动监控；低利用率或异常内存增长为失败。ISA、workers、block 由逐内核 benchmark 选择，禁止硬编码。未经最终外部审核不得宣称发布。
## 连续执行状态机
Task 状态流转：NOT_STARTED -> IN_PROGRESS -> PASS | FAIL | BLOCKED | REVIEW_PENDING。
- 状态唯一来源是控制包 `02_TASK_LEDGER.csv`；同一时间只允许一个 Task 为 IN_PROGRESS。
- PASS 必须有当前 SHA 的测试、证据、commit 与 push；PARTIAL、WARN、未运行、waiver、历史 PASS 一律不算 PASS。
- BLOCKED 仅限真实外部依赖，必须记录阻塞对象、实测命令与时间，并继续其他不受阻的 Task。
- REVIEW_PENDING 仅表示审阅胶囊已异步提交，Agent 继续其他无依赖 Task，不得停工等待。
- 除最终发布审核外，不设等待外部批准的停止点；Fatduck 离线不中止 Linux 可执行任务。
- Agent 无权宣布发布；全部通过后只能输出 `AWAITING_EXTERNAL_RELEASE_REVIEW`。
## 验证
修改后必须测试。
报告：
修改内容、测试结果、限制、遗留问题。
未经验证不得声明完成。
## 工程管理
memory.md记录稳定结论。
logs记录过程。
精细化commit。
禁止破坏性 Git 操作（不 force push、不改写历史、不 reset --hard）。
执行命令必须设置 timeout，并保存日志。
## Linux环境
开发环境：
vm-bj Linux。
默认：
bash + git + Linux工具链（禁止以 Git Bash/pwsh 作为默认环境）。
所有长时间任务必须：
- 有超时；
- 有日志；
- 可恢复。
---
## Windows验证
Windows仅作为远程验证节点。
需要时通过 SSH ：fujia@fatduck 调用 Windows PowerShell 进行：
- Windows编译；
- MSVC测试；
- 经 astrocs CLI/JSONL/取消协议驱动的验证（本轮无独立 GUI）。
Windows不可用时记录等待，不阻塞Linux开发。
Fatduck 接入细节（免密 SSH 命令、时间窗、离线处理策略）见根目录 FATDUCK_ACCESS.md：
- 每日北京时间 07:00（最早 06:30）～23:30 在线确定性高，Fatduck 任务安排在该窗口内。
- Fatduck 离线不中止目标：计时等待恢复后重试，不放弃、不降级、不伪造。
---
## 目录规范
在run目录执行临时操作，存储临时文件
在工程控制目录存放控制包解压文档
在reports目录撰写报告
## 工程包执行
收到工程包：
把工程包在工程控制文件夹展开；
读取规范；
分析依赖；
执行任务；
完成测试；
提交报告。
不要自行扩大任务范围。
复杂且困难，无法解决的问题可以记录为阻断项，在审核包中汇报。
