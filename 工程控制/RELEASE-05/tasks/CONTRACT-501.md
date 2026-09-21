# 任务：CONTRACT-501 双线合同与冻结点约定

## 目标
在 A/B 双线启动前，把双方依赖的接口、文件域、判据全部写进合同并冻结，使两线互不打扰、争议有据可依。

## 权威依据
最高设计 §8；ENGINEERING_SPEC §4.1、§8；CONTROL_PACK_SPEC 双线纪律（00_README §3）。

## 改动范围
- 允许改/新建：`eng/contracts/**`（schema）、`docs/contracts/**`（合同说明）、`docs/architecture/**`（管线/调度器架构合同）
- 禁止改：lib/ 代码、eng/ci 注册表与 eng/tests（GATE 任务域）

## 步骤
1. **文件域合同**：登记 A 线（lib/、实验 code/results）、B 线（eng/ci、eng/tests、eng/contracts、docs/contracts、docs/ci）、共享面（docs/science、plugins、architecture、owner）的归属与合并点规则；
2. **命名块生命周期合同**：block 元数据 schema（名字/形状/类型/单位/可缺性/生产者/消费者/生命周期阶段）、创建-消费-销毁状态机、缺块降级语义、provenance 流转；
3. **调度器接口合同**：三阶段调度器统一 entrypoint 与 DAG 声明格式；normalize（异步工作流/预取/帧并发）、mosaic（窗口大小参数与确定性归约顺序）、export（子块/有界队列/背压）各自的资源声明与探针事件 schema（节点墙钟、排队、块生灭、RSS、I/O、worker 均衡、缓存命中）；
4. **性能门判据合同**：L2 冻结判据的正确定义（平均利用率、p50、达标占比、低利用窗且无积压）、测量口径、采样窗口、fail-closed 规则；worker_balance 指标正确算法；
5. **监控字段语义**：requires_monitor、mutates_workspace 的执行语义二选一（真强制或改名降级），写成合同供 GATE-501 实现；
6. **合同双向对应**：docs/contracts 每份说明在 eng/contracts 有对应 schema，机器校验双向索引；
7. 冻结评审：前台组织 A/B 两线负责人（子代理）对合同逐条确认后冻结，冻结后变更走差异单。

## 验收门
- [ ] 六类合同成文且 schema/说明双向对应检查通过；
- [ ] A/B 两线按合同能各自列出任务清单而无需询问对方；
- [ ] 探针事件 schema 能被 observability 现有事件流接纳（出映射表）；
- [ ] doc-index/合同门全绿。

## 禁止
- 不在合同里写实现细节代码；不冻结科学公式（科学以 DOC-502 为准）。
