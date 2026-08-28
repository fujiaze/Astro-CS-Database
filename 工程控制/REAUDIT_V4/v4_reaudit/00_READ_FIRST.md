# AstroCS MAIN 预发布重审控制 V4

## 唯一目标

把 AstroCS `main` 收敛到可预发布状态，使：
`SCI 科学定义 → ALG 算法推导 → ARCH/API 软件合同 → SRC 实现 → TEST/运行证据`
逐项可追溯且相互一致；并完成 V3 打回所指的结构性改造：

1. **单一 `astrocs` CLI**（Windows/Linux 同源同行为），退役 orchestrator 双入口与 DLL 运行时加载路径；
2. **Phase1/2/3 进程内调用**；Phase3 = HiPS 球面分块到平面 WCS FITS 重投影（依次 SCI→ALG→ARCH/API→CODE→独立合成 Oracle，禁止 stub/no-op/猜测科学语义）；
3. **amd64 私有 CPU backend**：逐内核 benchmark/profile；无有效 profile 时走 baseline backend，但**必须**按有效 CPU affinity 动态多线程；
4. **强制资源门禁**：一切重计算自动监控 CPU/内存/I/O/线程/阶段；低利用率、异常内存增长、竞态、资源报告缺失一律不得 PASS；
5. 禁止硬编码核心数/worker/block/全局 AVX 编译参数。

## 不可变规则

1. 只在 `main` 开发；禁止创建任何分支（含 PR/审计/历史锚分支）。
2. 当前任务开始时执行 `git fetch origin`，以当时 `origin/main` 为唯一候选起点并记录 SHA（C0-002 冻结）。
3. 历史代码只允许 `git archive <sha>` 导出仓库外只读使用；禁止在历史锚提交或建分支。
4. 每个 Task 恰好一个原子 commit；验证通过后**立即** push `main`；push 失败必须停止。
5. 禁止 force-push、reset --hard、改写历史、删除用户改动。
6. Agent 无权改变任务、阈值、状态词、豁免条件与执行顺序。
7. 唯一状态词：`NOT_STARTED / IN_PROGRESS / PASS / FAIL / BLOCKED`；代码缺陷/测试失败/性能不达标只能记 FAIL，BLOCKED 须外部阻塞证据。
8. **规则 9（并行）**：任何生产重计算阶段持续 >1s 不得单线程；累计串行计算时间不得超过总计算时间 1%。
9. **规则 10（门禁前置）**：资源门禁与 C6 测试未通过前，禁止 32R 全量、历史全量或任何 >60s 科学运行。
10. 所有命令有 timeout、有日志；禁止无限轮询、禁止反复重跑完整基准；32R 仅当前候选一次（失败仅允许修复明确原因后单次重跑该项）。
11. 每个 Task PASS 后同时生成 **review capsule**（最新完整文件集，按 07 规范）。
12. Agent 只报告证据；PASS/REJECT 由外部审核人裁决。除最终发布审核外**不得停等外部批准**；历史 PASS/waiver/PARTIAL/忽略错误/视觉判断均不能替代当前证据。

## 执行入口

1. 读取本文件 → `01_WORKFLOW_AND_GATES.md`。
2. `python3 scripts/validate_control.py .` 非 CONTROL_PASS 立即停止。
3. 逐行执行 `02_TASK_LEDGER.csv`（98 行，不得跳号）；规格见 `03_TASK_SPECIFICATIONS.md`。
4. 到达检查点 C0–C9 时按 `04_CHECKPOINT_CHECKLISTS.md` 打包核验。
5. C9-004 终态冻结后，唯一允许输出：**AWAITING_EXTERNAL_RELEASE_REVIEW**；不得自行宣称正式发布。

## 当前证据起点（继承）

V3 审核包 `run/reaudit_v3/AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z.zip`（sha256
76a43bdd…4949089）为唯一继承证据：其中 TSan 5634 条数据竞争（upm.cpp:532/577/625）、
mosaic 层缺口（仅 signal+support）、串行架构（avg_cores<1）、stage1 版本漂移、
BLD-002/003 范围裁定为已登记债务，分别映射到 C1-007/C5-017/C6-013/C8/C9 任务处理。
