# RT-006 PLAN — 唯一 Scheduler + Runtime DAG 调度

## 需求 (04_TASK_SPECIFICATIONS.md RT-006)
IR 构图后按依赖就绪调度；CPU pool、有限 I/O executor、backpressure 和 memory reservation 统一。
NodePlan 先估计 work/memory，再取 lease。失败只标失败节点，其依赖 SKIPPED；独立节点策略明确。
状态输出必须带 node ID，不能只给无名 vector。测试 DAG 并发、diamond、失败、取消、恢复、预算、
内存回压、确定性。

## 现状证据
- 旧 Scheduler::run 输出无名 vector<NodeStatus>；全局失败即停（独立节点被 CANCELLED，语义错误）。
- 旧 add_node 有 move 后使用空 id 的 bug（status_ 空 key）。
- RuntimeImpl 只登记 IR，不构图、不调度、不执行模块。

## 修改
1. include/astrocs/core/scheduler.h：NodeSpec 加 estimated_memory_bytes/min_workers/max_workers；
   run 状态输出改 vector<pair<node_id,status>>；构造加 memory_limit_bytes。
2. lib/core/src/scheduler.cpp：修复 add_node move bug；重写 run：
   - 失败节点 FAILED，传递依赖 SKIPPED，独立节点继续 COMPLETED；
   - 取消 → CANCELLED；
   - 内存回压（memory_limit 内串行化超限节点）；
   - 状态输出带 node ID。
3. lib/core/src/runtime.cpp：load_pipeline 用 PipelineIRParser 解析+ModuleRegistry 校验+构图，
   每个节点绑定 IModule 工厂执行；run 经 Scheduler；inspect/node_statuses 带 ID。
4. tests/unit/rt006_scheduler_test.cpp（新）：7 组测试。
5. tests/unit/core_scheduler_test.cpp：适配新签名 + 断言 node ID 语义。

## 科学影响
无（调度语义；不动科学公式）。

## 风险
- 失败传播语义改动（全局停→按依赖 SKIPPED）会改变既有测试期望 → 已更新断言。
- 并发调度需 TSan 验证。

## 验收命令
1. `cmake --build run/temp/build_v61 --target rt006_scheduler_test` → build=0
2. `./run/temp/build_v61/tests/unit/rt006_scheduler_test` → RT-006_PASS
3. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt0"` → 13/13 PASS
4. TSan 下 rt006/core_scheduler 无 data race
