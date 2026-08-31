# CHK-003 PLAN — 建立 serial-heavy 与资源接线检查器

## 需求 (04_TASK_SPECIFICATIONS.md CHK-003)
从生产调用图定位每个 cpu_heavy 节点实现与编译 target，核对并行轴、lease 调用、compile definition、
OpenMP/std::thread 实现、MSVC/Linux 实际路径、resource gate caller。扫描固定 workers=1、
hardware_concurrency 模块自取、裸 thread pool、serial pixel/sample loop、全局锁。
allowlist 仅允许 <5s I/O/metadata（含 owner/reason/expiry/test）。
负面 fixture 必须抓：Phase2 宏关闭、P3 双循环、无 gate caller。

## 现状证据（真实扫描基线，F-003/004/005/006 复现）
- 根 CMake 无 P2_ENABLE_OPENMP/P2_PARALLEL；sampler.cpp 注释"默认构建 P2_ENABLE_OPENMP=OFF => 实际串行"。
- sampler/upm 用 hardware_concurrency 自取 worker；`!defined(_MSC_VER)` 排除 MSVC。
- p3_session.cpp 二维 y/x 串行主循环；p3_resample.cpp 无并行/lease。
- evaluate_gate/fast_fail_first10s 只在 resource_gate.h，无生产 caller。

## 影响文件
- tools/quality/check_serial_heavy.py（新）
- evidence/v6_1_rework/tasks/CHK-003/{PLAN.md,TASK_RESULT.json,logs/*}

## 科学影响
无（检查器）。

## 风险
- 当前真实扫描 FAIL 是预期基线，修复由 P2-001/P2-002/P3-003/MON-002 完成；届时本检查器转 PASS。

## 验收命令
1. `python3 tools/quality/check_serial_heavy.py --selftest` → SELFTEST_PASS（P2 宏关/P3 双循环/无 gate caller 全抓）
2. `python3 tools/quality/check_serial_heavy.py --repo .` → SERIAL_HEAVY_FAIL 逐项列出基线缺陷
