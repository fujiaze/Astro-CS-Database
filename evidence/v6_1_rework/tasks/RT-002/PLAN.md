# RT-002 PLAN — 实现全局原子 ThreadBudget 租约

## 需求 (04_TASK_SPECIFICATIONS.md RT-002)
实现共享原子 token/semaphore：`acquire(min,max,policy)`、RAII release、取消/异常自动归还。
Scheduler 自身 worker 与节点内部 work 必须使用同一总预算，避免 N 个节点各获 N。
测试同时启动两个 heavy node 各请求全部核，断言全程 sum(active)<=budget；注入异常/取消后 available 恢复。
1 worker 只允许 available=1 或测试 reference。

## 现状证据
- RT-001 已冻结 ThreadBudget 接口（无 policy、无 CV、无测试）。
- F-007（lease 无原子预留、每节点可得全部 budget）需本任务闭环。

## 修改
1. include/astrocs/core/context.h：AcquirePolicy 枚举（BLOCK/NONBLOCK/BEST_EFFORT）；acquire 带 policy；CV+mutex。
2. lib/core/src/context.cpp：acquire 实现三策略（CAS 原子预留 + BEST_EFFORT 降级 + BLOCK 等待）；_make_lease 归还并 notify_all。
3. tests/unit/rt002_budget_test.cpp（新）：6 组测试。
4. tests/unit/CMakeLists.txt：注册 rt002_budget。

## 科学影响
无（调度资源语义）。

## 风险
- BLOCK 策略依赖调用方保证归还；测试用确定性时序避免死锁。

## 验收命令
1. `cmake --build run/temp/build_v61 --target rt002_budget_test` → build=0
2. `./run/temp/build_v61/tests/unit/rt002_budget_test` → RT-002_PASS（8 次重复全 PASS）
3. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt001|rt002"` → 10/10 PASS
