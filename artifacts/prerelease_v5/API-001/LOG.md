# API-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS API-001 行(公共 POD/opaque handle/allocator/span/errors/cancel/logger/thread budget;逐字段单位/所有权;验收=headers 可被 C/C++ 独立编译+ABI layout tests+无 STL/exception 越界); ARCH-003 §4(host services 同构); ARCH-004 §1(budget)。

## 动作
1. 新建 docs/api/COMMON_ABI_V1.md(API-COMMON-001): acs_ 前缀+五结构 struct_size/abi_version handshake; 逐字段单位/所有权注释作为合同(acp_span count=元素数+外部分配方/allocator 分配方释放/handle 生命周期 create-destroy 对); acs_status 九错误码(禁异常); acs_thread_budget(available_cpus=affinity∩cgroup∩Job+acquire/release 原子租借, 禁自建线程池); acs_cancel 单向置位; §3 并发合同模板(reentrant/threadsafe/internal_parallel/aliasing/取消点粒度五字段必填, 与 ALG 5c 对齐); §4 头独立性(C11/C++17 双编译+layout 断言+amd64 LP64/LLP64 规避 long); §5 任务映射。
2. 实现试金石 tests/api/test_common_abi.py 6 用例: 七类 host services 定义/handshake 计数/单位所有权注记/**C 与 C++ 双独立编译真实 gcc/g++ 落锤**/合同模板字段。期间修复 3 处测试自身 cls/self 误用。

## 验证
- gcc -x c11 与 g++ -x c++17 对合同头片段独立编译通过(测试内真实编译)。
- 全量回归 unittest **52/52 OK**(新增 6, 净 52)。

## 产物
docs/api/COMMON_ABI_V1.md; tests/api/; 本日志。

## PASS 判定
公共 POD/handle/allocator/span/errors/cancel/logger/budget 全定义且逐字段单位/所有权标注; C/C++ 独立编译实证; layout tests 合同+禁 STL/exception 越界(backend TU 可 -fno-exceptions)。API-001 = PASS。
