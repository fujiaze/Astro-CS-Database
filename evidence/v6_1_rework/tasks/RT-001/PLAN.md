# RT-001 PLAN — 冻结 Runtime 与模块公共合同

## 需求 (04_TASK_SPECIFICATIONS.md RT-001)
在 public headers 冻结：Runtime create/run/cancel/inspect；ModuleDescriptor/ModulePlan/IModule；
typed Port/DataArtifact；ThreadBudget/ThreadLease；logger/metrics/checkpoint。每个参数写
ownership/nullable/lifetime/thread-safety/blocking/unit/schema。C ABI 不抛异常，C++ 返回 Result/Error。
不得把算法实现塞进 ABI；模块不得拿全局 scheduler。ABI layout test 在 GCC/Clang/MSVC 验证 size/align/offset/version。

## 现状证据
- 已有 include/astrocs/core/{contracts,artifact,module,context,scheduler,pipeline,logging,checkpoint}.h。
- 缺：Runtime 接口、ModulePlan/IModule、ThreadBudget、RAII ThreadLease、ABI layout 测试。
- F-007（lease 无原子预留）、F-008（RunContext 无同步）将在 RT-002/003 修复；RT-001 冻结合同。

## 修改
1. include/astrocs/core/runtime.h（新）：Runtime/ModulePlan/IModule/工厂，逐参数合同注释。
2. include/astrocs/core/context.h：ThreadLease 改 RAII 可移动；新增 ThreadBudget 原子租约合同 + create_thread_budget。
3. lib/core/src/context.cpp：ThreadBudget::acquire 原子 CAS 实现 + create_thread_budget。
4. lib/core/src/runtime.cpp（新）：RuntimeImpl 最小实现 + create_runtime。
5. CMakeLists.txt：astrocs_core 增加 runtime.cpp；tests/unit 增加 rt001_abi_test。
6. tests/unit/rt001_abi_test.cpp（新）：ABI layout（enum 值/offset/size/factory 签名）。
7. docs/contracts/RT-001.md（新）：冻结合同文档。

## 科学影响
无（架构接口冻结，不改科学公式）。

## 风险
- RunContext 的 acquire_lease 暂用 make()（无原子预留），RT-003 接入 ThreadBudget 后替换；
  本任务确保合同编译通过。

## 验收命令
1. `cmake --build run/temp/build_v61 --target rt001_abi_test` → build=0
2. `./run/temp/build_v61/tests/unit/rt001_abi_test` → RT-001_ABI_PASS
3. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt001"` → 9/9 PASS
