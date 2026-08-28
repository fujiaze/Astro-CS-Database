# ABI-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ABI-001 行(实现 05 的 C ABI v1、struct_size/version handshake、host allocator/log/cancel/budget、kernel table/selftest;验收=编译器/Debug/Release ABI tests+异常不跨边界+分配方可验证); 05 §4-5; API-001 合同(COMMON_ABI_V1.md)。

## 动作
1. include/astrocs/common_abi_v1.h(单一头, C11/C++17 双可编译): acs_head/span_f32/f64/u8/handle/status 十一码/allocator/logger/cancel/thread_budget(逐字段单位/所有权注释)+05 §4 扩展: astrocs_host_services_v1 聚合/astrocs_kernel_entry_v1(science_contract_id+algorithm_id+kernel_version+precision+determinism_class+fn)/astrocs_backend_api_v1(backend_id/build_id/sha256+feature bits+对齐/精度/确定性/aliasing/嵌套并行合同+kernel 表+self_test/warmup/shutdown)+astrocs_backend_get_api_v1 唯一入口+astrocs_abi_boundary_probe(边界验证)。线程安全/取消点逐函数注释。
2. lib/backend_host/host_services.cpp: 默认 host services(allocator=对齐 malloc+计数可验证/atomic cancel/budget=CAS 租借 Σ≤max_workers/劣化日志→stderr); extern C 装配+计数查询接口。
3. lib/backend_host/baseline_backend.cpp: get_api_v1(双 handshake 拒绝→静态 12 条目 kernel 表, 锚定 05 §5: 校准/噪声/WCS-PSF/drizzle×3/UPM×3/rejection/integration/HiPS)+self_test(allocator 往返+对齐+cancel+budget+logger)+warmup/shutdown; kernel fn 显式 ACS_ERR_UNSUPPORTED(科学实现属 ABI-003, 不静默); boundary probe 内部 catch→ACS_ERR_INTERNAL(异常不跨边界机制证明); baseline 可 -fno-exceptions 编译(ASTROCS_NO_EXCEPTIONS)。
4. tests/backend/abi_selftest_main.cpp: 运行时落锤(布局行/handshake 拒绝×2/kernel 表 12/UNSUPPORTED 显式/self_test/allocator 计数平衡/预算超租拒绝/边界探针)。
5. tests/backend/test_abi_v1.py 5 测试: C11 -pedantic 编译/MSVC-GCC 可移植面(-fno-exceptions TU)/Debug(-O0)+Release(-O2) 双构建运行全 PASS+**布局行逐字节一致**/kernel 表锚定 05 §5 全 12+无 arch 旗标泄漏/ABI 版本宏单源。

## 验证
- 全量回归 unittest **107/107 OK**(新增 5)。
- g++ 14.2: C11 编译 OK;Debug/Release 双构建 ALL_OK。

## 限制与遗留
- Fatduck MSVC 交叉验证: scp 后编译期间 Fatduck 掉线(在线窗内波动; 实测 ssh 超时)。按 AGENTS 离线不阻塞——Linux 侧闭环先行; MSVC 编译+运行 ABI selftest 列入 WIN/FAT 域与下轮重试(源码为严格 C++17 无扩展, 与 CLI-001..003 已验证的 cl 兼容模式一致)。
- kernel fn 科学实现+CPUID/安全加载(manifest/hash)属 ABI-002/003; 本任务注册结构/合同/selftest 机制齐备。

## 产物
include/astrocs/common_abi_v1.h; lib/backend_host/{host_services,baseline_backend}.cpp; tests/backend/{abi_selftest_main.cpp,test_abi_v1.py}; 本日志。

## PASS 判定
C ABI v1 全要素(handshake/host services/kernel table/selftest)实现; 双编译器面(C11+异常开关)+Debug/Release 布局一致实测; 异常边界机制证明; allocator 计数可验证(selftest 往返+平衡断言)。ABI-001 = PASS。
