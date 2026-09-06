# AstroCS Agent 入口

全程中文执行与汇报。开始任务前先读：

1. **冻结约束**：根 [`AstroCS_ENGINEERING_CONSTRAINTS.md`](AstroCS_ENGINEERING_CONSTRAINTS.md) —— 项目负责人冻结工程约束（来源与控制包 hash 关系见其文件头 YAML）。优先级最高，Agent 不得修改、放宽或重新解释；修改权仅在项目负责人。
2. **记忆**：根 `memory.md` 及本任务相关模块 memory。
3. **模块文档**：相关模块 `README.md`、`module.yaml`、公共头与共址测试；科学/算法权威在 `docs/science/`、`docs/algorithms/`。
4. **控制包**（执行控制包派发任务时）：对应控制包 `00_READ_FIRST.md` 与本任务规格。

具体规则一律以冻结约束文件为准，`AGENTS.md` 不复制长文。
## 执行纪律（详见约束文件）

- 仅 `main` 原子提交并立即 push；禁止分支、force push 及破坏性 Git；SubAgent 不直接 commit。
- 一个 task 对应一个可独立验证的 commit；科学、架构、性能、文档清理不混提。
- 科学定义 = 算法 = 接口 = 代码 = 测试；科学公式与默认容差不得改动。
- 重计算禁止单线程并自动资源监控；线程/ISA/block 由逐内核 benchmark 选择，禁止硬编码。
- 所有外部命令带 timeout 并保存日志；修改后必须验证才能报告完成。

## 治理要点（GOV-001 恢复块；完整规则以冻结约束文件为准）

只在 main 原子提交并立即 push，禁止分支及破坏性 Git。仅支持 amd64。
节点分工：vm-bj Linux 负责静态、文档、合成小测和调度；Fatduck 在线时负责 Windows 编译、
benchmark、真实数据和重计算，Fatduck 离线不中止 Linux 可执行任务。
发布每个平台仅一个 astrocs CLI，Phase1/2/3 由 CLI 调用；未来 Windows GUI 只控制 CLI。
ACR 暂不接入，生产仅纯 CPU 自适应 backend。
重计算自动监控；低利用率或异常内存增长为失败。
ISA、workers、block 由逐内核 benchmark 选择，禁止硬编码。
未经最终外部审核不得宣称发布；全部通过后只能输出 `AWAITING_EXTERNAL_RELEASE_REVIEW`。
Task 状态流转：NOT_STARTED -> IN_PROGRESS -> PASS | FAIL | BLOCKED | REVIEW_PENDING；
waiver 与历史 PASS 不算 PASS；REVIEW_PENDING 表示审阅胶囊已异步提交，Agent 继续其他
无依赖 Task，不设等待外部批准的停止点。

## 目录规范

- `run/`：临时操作与临时文件
- `工程控制/`：控制包解压文档
- `reports/`：报告
