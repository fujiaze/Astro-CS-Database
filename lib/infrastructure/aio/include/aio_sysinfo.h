// ============================================================================
// aio_sysinfo.h - AIO 系统信息探测 C API（可用内存）
//
// 依据
//   - ASTROCS_DESIGN.md §10 逐字：「aio 是文件级唯一 I/O 边界：任何文件读写经
//     aio，不得有第二处 I/O 实现」；机器判据「全仓文件打开 / 流式读写 / 文件
//     系统写操作，除 aio 内部外应为 0」。
//   - 工程控制/RELEASE-02/GAP_AUDIT.md §9.73【裁决 U5】负责人逐字：
//     「全部走 aio……没有其他需要读写的地方了」。
//   - 任务 AIO-SYSINFO-01：把 scheduler 的「可用内存探测」收进 aio 边界。原实现
//     lib/infrastructure/scheduler/src/module_adapters.cpp 的
//     p1_available_memory_bytes() 直接 std::fopen("/proc/meminfo") 读
//     MemAvailable ⇒ CHK-AIO-IO-BOUNDARY 判 UNREGISTERED（新增越界 I/O）+
//     PRODUCTION-RESIDUAL（生产路径直连 I/O）。
//
// 语义（冻结）
//   - 返回**当前可用内存字节数**（口径见下「平台」）；调用方按字节使用。
//   - 返回 0 = 不可判定（探测失败 / 未知平台）⇒ 调用方必须 fail-closed。
//     本项目的帧级并行内存上限按 0 处理为「退回串行 cap=1」，**不得**把 0 当作
//     「内存充足」（见 module_adapters.cpp p1_memory_cap）。
//   - 每次调用重新探测，不缓存：可用内存是随时间变化的物理量，缓存会让
//     「同一 run 内不同节点的并发上限」依赖调用顺序。无跨调用共享可变状态 ⇒
//     reentrant（多线程并发调用安全）。
//
// 平台（口径按平台补全；依据 ASTROCS_DESIGN.md §9「可用 CPU = 亲和性 ∩ cgroup
//         ∩ Job Object」的同款交叠语义）
//   - Linux  : ① /proc/meminfo 的 MemAvailable（内核估算的「不触发 swap 即可满足
//     新分配」的内存，**含可回收 page cache**）。这是本项目既有口径：
//     sysconf(_SC_AVPHYS_PAGES) 在 Linux 只反映 MemFree（实测本机 MemFree
//     约 1 GB vs MemAvailable 约 22 GB），换成它会把帧并发度压到 1 而使并行
//     失去意义。读不到 MemAvailable ⇒ 回退 sysconf(_SC_AVPHYS_PAGES) ×
//     sysconf(_SC_PAGESIZE)。
//     ② **∩ cgroup 内存余量**（v2: memory.max − memory.current；
//     v1: memory.limit_in_bytes − memory.usage_in_bytes）：MemAvailable 是
//     **主机全局**量，容器里会远大于本 cgroup 实际可用 ⇒ 必须取交。
//     memory.max="max"/不可读/无该层级 ⇒ 该项不参与（fail-open 到主机口径，
//     不返回 0）。
//   - Windows: GlobalMemoryStatusEx 的 ullAvailPhys（含待命/可回收列表）。
//     Job Object 内存上限当前不参与 —— 登记为未核实项。
//   - 其他   : 同 Linux 的 sysconf 回退；失败 ⇒ 0。
// ============================================================================

#ifndef AIO_SYSINFO_H
#define AIO_SYSINFO_H

#include <stdint.h>

#ifdef _WIN32
#define AIO_SYSINFO_EXPORT __declspec(dllexport)
#else
#define AIO_SYSINFO_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 当前可用内存字节数；0 = 不可判定（fail-closed，调用方不得据此认为内存充足）。
// 并发合同: reentrant（无跨调用共享可变状态）。
AIO_SYSINFO_EXPORT uint64_t aio_system_available_memory_bytes(void);

// 当前**进程树**（本进程 + 其全部后代进程）的常驻集大小 RSS 之和（字节）。
// 用途: 内存压力的实测分子（压力 = 进程树 RSS / 内存预算）。口径与外部看门狗
//   eng/tools/monitoring/mem_guard.py 的 --max-rss-gb 一致（两者都按**进程树**求和），
//   使「程序自身预算」与「外部上限」可在同一张曲线上比较，阈值关系可自洽核对。
// 语义（冻结）
//   - 返回 0 = 不可判定（探测失败 / 未知平台）⇒ 调用方必须 fail-closed。
//   - 每次调用重新探测，不缓存：RSS 是随时间变化的物理量，缓存会让压力判定依赖
//     调用顺序。无跨调用共享可变状态 ⇒ reentrant（多线程并发调用安全）。
//   - 只读不写，不产生任何文件系统副作用。
// 平台
//   - Linux: 本进程 = /proc/self/status 的 VmRSS（回退 /proc/self/statm 的
//     resident 页 × 页大小）；后代 = 枚举 /proc/<pid>/status 的 PPid 关系图，
//     自本进程起 BFS 求和 VmRSS。枚举失败 ⇒ **回退为仅本进程**（仍是有效下界，
//     fail-open 到本进程口径，不返回 0）——与可用内存探测的 cgroup 项「不可判定
//     即不参与」同款处置。
//   - Windows: GetProcessMemoryInfo 的 WorkingSetSize（**仅本进程**；后代枚举
//     未接入，登记为未核实项）。取不到 ⇒ 0。
AIO_SYSINFO_EXPORT uint64_t aio_process_tree_rss_bytes(void);

#ifdef __cplusplus
}
#endif

#endif  // AIO_SYSINFO_H
