# 连续检查点与控制包/审核包工作流

检查点是机器核对，不是让 Agent 等人批准。除 C9 外，完成后立即进入下一项可执行 Task；外部 review capsule 异步返回。

| Checkpoint | 所需 Task | 逐项机器检查 | 失败动作 |
|---|---|---|---|
| C0 起点 | BASE-001 GOV-001 | main 三 SHA、外部变化、V3/V4 六风险、AGENTS 短规则 | 修起点/规则；不得进入定义修改 |
| C1 定义 | VER-001 TRACE-001 DOC-001 SCI-001..007 ALG-001..007 | alpha 单一版本；七组 SCI/ALG 章节完整；引用 locator；单位唯一；trace schema/mutation | 对失败合同开修复 commit；其他独立 ARCH inventory 可继续 |
| C2 架构接口 | ARCH-001..005 API-001..005 CLI-001..008 P3-001..004 | 单 exe；无 shell-out；命令/schema/exit；完整 API 函数；Phase3 真重投影 | 修对应 Task；不得用 stub/no-op 跨关 |
| C3 CPU 自适应 | ABI-001..003 ISA-001..005 BENCH-001..005 | baseline 安全；CPUID/XGETBV；逐 kernel Oracle；无全局 ISA；profile/fallback | 失败 backend 从候选移除并修复；baseline 失败则禁止运行 |
| C4 并行资源 | MON-001..004 ISO-001 PAR-001..007 | 监控 fixture；低 CPU compute 必失败；泄漏注入被抓；ACR dormant；统一预算 | 修锁/队列/竞态/粒度；不得 waiver |
| C5 合成一致 | SYN-001..009 DOCCHK-001..002 | 七算法+接缝+pipeline；全后端/1/N worker；symbol/signature/unit mutation | 科学失败回 SCI/ALG；实现失败开原子修复 Task |
| C6 Linux alpha | LNX-001..005 REV-001 REV-002 | 双配置 clean、quick profile、sanitizer、单入口包、alpha 命名、review capsules | 修完 Linux；Fatduck 离线不影响此前任务 |
| C7 Windows 基础 | WIN-001..005 | 同 SHA；MSVC 零错误；full benchmark；synthetic/resource/memory 全过 | 修复并 push main，再重新拉同 SHA；禁止启动 32R |
| C8 Windows 真实 | WIN-006..009 REV-003 | 小真实先过；32R 当前候选恰好一次；32/32；接缝/资源；单入口 alpha 包 | 记录失败 run，不秘密重跑；开根因 Task，修后新候选需最终报告说明次数 |
| C9 最终审核 | REL-001..004 | P0/P1=0；core 100%+other 20%；双平台 hash；最终包 validator | 输出 `AWAITING_EXTERNAL_RELEASE_REVIEW` 并停止 |

## 控制包与审核包循环

1. **控制包**（本包）冻结范围、Task、检查点、门限、模板和机器检查器；Agent 不得修改它来适配结果。
2. **执行证据**随每个 Task 产生，代码/文档通过 main 原子 commit；大产物只登记。
3. **Review capsule** 是每个 commit 的小型当前版本快照，异步交外部审核；不让 Agent 停工。
4. **打回**产生新的 finding 和修复 Task/commit，不篡改旧证据，不回滚历史版本做对比。
5. **最终审核包**只在 C9 生成一次，白名单打包；审核者抽查推导、源码、真实 HiPS 和资源利用后决定 alpha 放行。

## Checkpoint 记录规则

每关向 `CHECKPOINTS.csv` 写一行。`required_tasks` 必须展开为实际 ID；checker 根据 ledger 计算，不能手填 PASS。检查命令、exit code、commit 和证据路径必填。C8 必须另外记录 32R run_id；C9 必须证明审核包 `SHA256SUMS`。

