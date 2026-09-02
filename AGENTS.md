# AstroCS Agent 入口

全程中文执行与汇报。开始任务前先读：

1. **冻结约束**：根 [`AstroCS_ENGINEERING_CONSTRAINTS.md`](AstroCS_ENGINEERING_CONSTRAINTS.md) —— 项目负责人冻结工程约束（来源与控制包 hash 关系见其文件头 YAML）。优先级最高，Agent 不得修改、放宽或重新解释；修改权仅在项目负责人。
2. **记忆**：根 `memory.md` 及本任务相关模块 memory。
3. **模块文档**：相关模块 `README.md`、`module.yaml`、公共头与共址测试；科学/算法权威在 `docs/science/`、`docs/algorithms/`。
4. **控制包**（执行控制包派发任务时）：对应控制包 `00_READ_FIRST.md` 与本任务规格。

具体规则一律以冻结约束文件为准，`AGENTS.md` 不复制长文。

## 节点与角色

- **Linux amd64**：Agent 执行与控制节点 —— 控制、静态分析、轻量编译、小合成实验。默认环境为 bash + git + Linux 工具链；禁止以 Git Bash / 默认 PowerShell 作为开发默认环境。
- **Windows x64**：正式开发/客户端/发布平台 —— 用户只面对 `astrocs.exe`（运行时/I/O/科学模块/CPU provider 为 DLL）；Windows 编译、benchmark、真实数据与重计算经远程节点执行，离线不阻塞 Linux 任务。

## 执行纪律（详见约束文件）

- 仅 `main` 原子提交并立即 push；禁止分支、force push 及破坏性 Git；SubAgent 不直接 commit。
- 一个 task 对应一个可独立验证的 commit；科学、架构、性能、文档清理不混提。
- 科学定义 = 算法 = 接口 = 代码 = 测试；科学公式与默认容差不得改动。
- 重计算禁止单线程并自动资源监控；线程/ISA/block 由逐内核 benchmark 选择，禁止硬编码。
- 所有外部命令带 timeout 并保存日志；修改后必须验证才能报告完成。

## 目录规范

- `run/`：临时操作与临时文件
- `工程控制/`：控制包解压文档
- `reports/`：报告
