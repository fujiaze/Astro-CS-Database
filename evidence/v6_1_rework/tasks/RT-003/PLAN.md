# RT-003 PLAN — 并发安全 RunContext

## 需求 (04_TASK_SPECIFICATIONS.md RT-003)
日志/metrics 使用线程安全 sink；ArtifactStore 的并发读、唯一 producer 写和 duplicate 冲突确定；
checkpoint 顺序可追溯；取消 atomic。禁止暴露容器裸引用和返回可能因并发插入失效的 map 指针。
TSan 测多个 node 并发 log/store/metrics/cancel。

## 现状证据
- F-008：RunContext 无同步，get_artifact 返回内部指针（并发插入失效风险）。
- TSan 实测发现 Scheduler 多 worker 并发 set_thread_budget 数据竞争（真实缺陷）。

## 修改
1. include/astrocs/core/context.h：RunContext 全成员加 mu_；get_artifact 改 bool+out 快照；
   log_entries/metrics/ticks/checkpoints 返回快照（拷贝）。
2. lib/core/src/context.cpp：全部方法加锁实现。
3. lib/core/src/scheduler.cpp：thread budget 移到 run 入口单线程设置一次（消除并发写）。
4. tests/unit/rt003_context_test.cpp（新）：8 node 并发 log/metrics/store/checkpoint；
   duplicate 双写恰好一次成功；并发 cancel；全部在 TSan 下跑。
5. tests/unit/CMakeLists.txt：注册 rt003_context。

## 科学影响
无（并发安全语义）。

## 风险
- TSan 构建需 -fsanitize=thread；Linux GCC14 可用。

## 验收命令
1. `cmake --build run/temp/build_v61 --target rt003_context_test` → build=0
2. `./run/temp/build_v61/tests/unit/rt003_context_test` → RT-003_PASS
3. TSan 构建下 rt002/rt003/core_context/core_scheduler 4 测试全 PASS（无 data race）
4. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt0"` → 11/11 PASS
