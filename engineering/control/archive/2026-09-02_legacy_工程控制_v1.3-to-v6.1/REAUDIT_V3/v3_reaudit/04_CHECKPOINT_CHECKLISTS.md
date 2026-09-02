# Checkpoint 清单

每个 `[ ]` 必须由 Agent 改为 `[x]` 并在 `templates/CHECKPOINT_RESULTS.csv` 给出证据相对路径和 SHA-256。没有证据的勾选等于 FAIL。

## CP0 身份与数据

- [ ] 当前分支 main。
- [ ] HEAD=main=origin/main。
- [ ] tracked worktree 干净或外部变化逐项登记。
- [ ] 记录起点 commit/tree。
- [ ] 记录 Linux 2C2G 实际资源。
- [ ] 32 Red 为 11+11+10，hash 唯一。
- [ ] 3 masters 完整。
- [ ] 未提交、未 push、未创建分支。

## CP1 并行设计

- [ ] 所有生产计算 stage 有入口和调用链。
- [ ] 所有 >1s stage 有并行策略。
- [ ] 全局 worker 预算只有一个所有者。
- [ ] 嵌套并行与 oversubscription 被禁止并测试。
- [ ] 异步 I/O 有界、可取消、可传播错误。
- [ ] ACR CPU/GPU/Mixed 路由定义清楚。
- [ ] 设计修改已按 Task 原子 commit/push。

## CP2 Linux 并行

- [ ] Sampler、UPM、Integration、ACR CPU 生产路径均 max_threads>=2。
- [ ] 2C CPU 密集窗口平均 CPU>=150%。
- [ ] 1T/2T speedup>=1.50。
- [ ] 串行段均<1s且累计<1%。
- [ ] 1T/2T 科学结果过容差。
- [ ] 重复 2T 结构/hash 确定。
- [ ] 无 race/deadlock/oversubscription。
- [ ] CP2 前未运行 32R。

## CP3 科学与算法

- [ ] 每个底层定义有唯一 SCI authority。
- [ ] Noise/SNR/variance/ivar 单位明确。
- [ ] Drizzle 输入/输出物理量与面积因子一致。
- [ ] UPM gauge、Huber、weight 与代码一致。
- [ ] Rejection/Integration 规则和图层语义一致。
- [ ] ALG 从 SCI 推导并含并行/复杂度/误差。
- [ ] 独立 oracle 不调用被测实现生成期望值。
- [ ] 容差先于运行冻结。

## CP4 文档、API、检查器

- [ ] 实际 AST API 数量与合同完全一致。
- [ ] 所有 public 和科学核心函数语义字段完整。
- [ ] 架构图与 build/call graph 一致。
- [ ] 串并行、异步、GPU 架构与生产路径一致。
- [ ] SCI->ALG->API->SRC->TEST 无断链。
- [ ] V2 已知 checker 假阴性全部有 mutation。
- [ ] 正例全 PASS、负例/mutation 全 FAIL。
- [ ] 报告统计由机器单源生成且互相一致。

## CP5 Linux 预发布

- [ ] 根级正式构建入口成功。
- [ ] Release/Debug clean build 成功。
- [ ] 所有正式 CLI/库已构建。
- [ ] FAIL=0，非外部依赖 SKIP=0。
- [ ] ASan/UBSan/竞态检查无错误。
- [ ] 合成全链路所有正式图层存在。
- [ ] 三板块各 2 帧真实 mini pipeline PASS。
- [ ] Linux 运行保持并行门禁。

## CP6 Windows/ACR

- [ ] Fatduck 只拉取 CP5 accepted main SHA。
- [ ] Windows clean build PASS。
- [ ] Windows tests FAIL=0。
- [ ] CPU/GPU/Mixed 三路均真实激活。
- [ ] 三路科学结果过冻结容差。
- [ ] Mixed 同时有 CPU/GPU 活动证据。
- [ ] 所有修复均回到 main 原子 commit/push。

## CP7 32R/接缝

- [ ] A/B/C/D 小矩阵先 PASS。
- [ ] A/B/C/D 各一次成功 32R。
- [ ] 32 帧输入和配置完全冻结。
- [ ] 运行均非单线程。
- [ ] 所有 seam 指标完整且相对 B 判定。
- [ ] D 无接缝回归。
- [ ] HiPS Browser 固定视场/固定 STF 对照齐全。
- [ ] 大产物仅 manifest，不入审核包。

## CP8 最终包

- [ ] `origin/main` 与候选 SHA 相同。
- [ ] P0=0，P1=0。
- [ ] findings、summary、ledger 计数一致。
- [ ] commits 每 Task 一项且均已 push。
- [ ] 审核包 <=25 MiB；单文件 <=5 MiB。
- [ ] 禁入文件为 0。
- [ ] SHA256SUMS 与 MANIFEST 全通过。
- [ ] Agent 只声明 AWAITING_EXTERNAL_REVIEW。
