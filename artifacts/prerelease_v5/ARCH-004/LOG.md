# ARCH-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ARCH-004 行(全局 thread budget;每阶段串行 IO/CPU task/async pipeline/backpressure/nested parallelism;每 symbol 线程模型;验收=静态 checker 能找未登记线程创建+重 kernel 预算来源); 旧 THREADING_MODEL.md(确定性锚点); ARCH-001 清单(62 线程创建/46 锁)。

## 动作
1. 新建 docs/architecture/THREAD_BUDGET_ARCH.md: §1 单一全局预算(available_cpus=affinity∩cgroup∩Job Object+层级分配+禁私有线程池+禁硬编码); §2 五阶段执行画像表(串行 IO/CPU 粒度/async 深度/backpressure); §3 异步仅两类(IO pipeline+后台服务), 科学计算无 async; 取消=全局原子标志+逐内核取消点; 禁嵌套并行(watchdog 唯一豁免); §4 并发正确性合同(旧锚点 upm:495/sampler 串行/drizzle:1662,1751,1834,1843 全保留+V5 修正 ACR/browser dormant); §5 静态 checker 合同; §6 关联。
2. 实现 tools/arch/check_thread_budget.py(§5 合同): 扫 lib/ 生产源(排除 archive/third_party/tests)五 pattern; 字面量 num_threads(N) 绝对 FAIL; omp_set_num_threads 须在 REGISTERED(带注记)否则 FAIL; 未登记线程创建 FAIL; watchdog 行级/文件级豁免。
3. 首跑真发现: 修正 REPO 路径 bug(两层→三层, 与 ARCH-001 同款)后 7 处命中逐一定性登记——orchestrator.h:336(watchdog 成员声明)/nanoflann.hpp 2 处(vendored 第三方 std::async)/omp_set_num_threads 3 文件 4 处(aio_pipeline_engine=迁移整改点 ABI-001 收编, ac_api.cpp=ac_set_num_threads 预算注入旧形态由 ARCH-003 host callback 取代, acr/examples 2 处=dormant)——全部显式登记带注记, 无一掩盖。
4. 测试 tests/arch/test_thread_budget.py 5 用例: 真实仓 PASS/mutation 未登记 thread FAIL/mutation 未登记 omp_set FAIL/登记必带注记/字面量恒 FAIL。

## 验证
- THREAD_BUDGET_CHECK_PASS 未登记=0 硬编码=0 豁免登记=10。
- 全量回归 unittest **41/41 OK**(新增 5)。

## 产物
docs/architecture/THREAD_BUDGET_ARCH.md; tools/arch/check_thread_budget.py; tests/arch/test_thread_budget.py; 本日志。

## PASS 判定
全局预算单一来源+每阶段画像+异步两类+取消架构齐; 静态 checker 实测能抓未登记线程创建(mutation 证实); 重 kernel 预算来源=§1 层级分配+BENCH-003 候选(无硬编码 core count)。ARCH-004 = PASS。
