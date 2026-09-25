// ============================================================================
// aio_sysinfo.cpp - AIO 系统信息探测实现（可用内存）
//
// 合同头: lib/infrastructure/aio/include/aio_sysinfo.h（唯一权威签名源）。
// 依据: ASTROCS_DESIGN.md §10「aio 是文件级唯一 I/O 边界」+ §9.73 裁决 U5。
// 任务: AIO-SYSINFO-01（scheduler 的 /proc/meminfo 直读收进本边界）。
//       可用内存口径按平台补全：Linux 再 ∩ cgroup 内存余量。
//
// 机制说明
//   - 本 TU 位于 aio 边界**内部**，文件读取仍走 aio 唯一机制原语
//     aio_file::read_all（aio_file_io.h；fopen/fread/fclose 唯一实现在 aio 内），
//     不在本文件复制第二份打开/读取通道。
//   - Linux 口径（依据 ASTROCS_DESIGN.md §9「可用 CPU = 亲和性 ∩ cgroup ∩ Job Object」
//     的同款交叠语义：进程可用资源 = 主机侧估计 ∩ 容器/Job Object 上限）:
//       ① 主机侧 = /proc/meminfo 的 MemAvailable（内核给出的「不触发 swap 即可满足新分配」
//          的估计，**含可回收 page cache**；不是 MemFree）。解析口径与 AIO-SYSINFO-01
//          原实现逐字一致（首个 MemAvailable: <N> kB 行；N > 0 ⇒ N × 1024 字节）。
//       ② cgroup 侧 = 内存控制器余量（v2: memory.max - memory.current；
//          v1: memory.limit_in_bytes - memory.usage_in_bytes）；memory.max 为 "max"
//          （无限制）或文件不可读 ⇒ 该项不参与。
//       ③ 返回 min(①, ②)。容器/Job Object 场景下 MemAvailable 是**主机全局**量，
//          只用它会把预算算到远超本 cgroup 实际可用的程度 ⇒ 必须取交。
//       ④ cgroup 文件读取失败/不存在（cgroup v1 无 v2 层级、非容器、权限不足）一律
//          **不阻断**：该项退化为不参与，口径回落到 ①（fail-open 到主机口径，不返回 0）。
//   - Windows 口径: GlobalMemoryStatusEx 的 ullAvailPhys（系统给出的「当前物理可用」，
//     含待命/可回收列表；Job Object 内存上限当前不参与，登记为未核实项）。
// ============================================================================

#include "aio_sysinfo.h"

#include "aio_file_io.h"  // aio 唯一整文件读取机制（aio_file::read_all）

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "aio_atomic_file.h"  // aio 唯一目录遍历原语 for_each_child

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>   // GetProcessMemoryInfo（进程 RSS 口径，与 monitor.h 同源）
#else
#include <unistd.h>
#endif

#ifndef _WIN32
namespace {

// 解析一个 cgroup 内存计数字面量（字节）；"max"/非数字/0 ⇒ 返回 0 = 不可判定。
uint64_t parse_cgroup_bytes(const std::string& text) {
  std::istringstream in(text);
  std::string tok;
  if (!(in >> tok)) return 0;
  if (tok == "max") return 0;  // v2 无限制
  unsigned long long v = 0;
  if (std::sscanf(tok.c_str(), "%llu", &v) != 1) return 0;
  // v1 用 9223372036854771712 (PAGE_COUNTER_MAX) 表示「无限制」⇒ 视作不可判定。
  if (v == 0 || v >= (1ull << 62)) return 0;
  return static_cast<uint64_t>(v);
}

// cgroup 内存余量 = limit - usage（>0 才有意义）；不可判定 ⇒ 0（不参与 min）。
uint64_t cgroup_available_bytes() {
  static const char* kLimitV2 = "/sys/fs/cgroup/memory.max";
  static const char* kUsageV2 = "/sys/fs/cgroup/memory.current";
  static const char* kLimitV1 = "/sys/fs/cgroup/memory/memory.limit_in_bytes";
  static const char* kUsageV1 = "/sys/fs/cgroup/memory/memory.usage_in_bytes";
  const char* limit_path = kLimitV2;
  const char* usage_path = kUsageV2;
  std::string limit_text;
  if (!aio_file::read_all(limit_path, &limit_text)) {
    limit_path = kLimitV1;
    usage_path = kUsageV1;
    if (!aio_file::read_all(limit_path, &limit_text)) return 0;
  }
  const uint64_t limit = parse_cgroup_bytes(limit_text);
  if (limit == 0) return 0;  // "max" / 无限制 ⇒ 该项不参与
  std::string usage_text;
  if (!aio_file::read_all(usage_path, &usage_text)) return 0;
  const uint64_t usage = parse_cgroup_bytes(usage_text);
  if (usage >= limit) return 1;  // 已到/超上限 ⇒ 余量取最小正数（不返回 0=不可判定）
  return limit - usage;
}

}  // namespace
#endif  // !_WIN32

extern "C" uint64_t aio_system_available_memory_bytes(void) {
#ifdef _WIN32
  // Windows: ullAvailPhys = 当前物理可用内存（含待命/可回收列表的口径由系统给出）。
  MEMORYSTATUSEX st;
  std::memset(&st, 0, sizeof(st));
  st.dwLength = sizeof(st);
  if (GlobalMemoryStatusEx(&st)) return static_cast<uint64_t>(st.ullAvailPhys);
  return 0;
#else
  // Linux ①: /proc/meminfo 的 MemAvailable（含可回收 page cache）。
  // sysconf(_SC_AVPHYS_PAGES) 在 Linux 只反映 MemFree（本机约 1 GB vs
  // MemAvailable 约 22 GB），会把帧并发度压到 1 而使并行失去意义 ⇒ 仅作回退。
  uint64_t host_available = 0;
  std::string text;
  if (aio_file::read_all("/proc/meminfo", &text)) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
      unsigned long long kb = 0;
      if (std::sscanf(line.c_str(), "MemAvailable: %llu kB", &kb) == 1) {
        if (kb > 0) host_available = static_cast<uint64_t>(kb) * 1024ull;
        break;  // 行在但值为 0 ⇒ 视为不可判定, 走回退
      }
    }
  }
  if (host_available == 0) {
    const long pages = sysconf(_SC_AVPHYS_PAGES);
    const long psize = sysconf(_SC_PAGESIZE);
    if (pages > 0 && psize > 0)
      host_available = static_cast<uint64_t>(pages) * static_cast<uint64_t>(psize);
  }
  if (host_available == 0) return 0;
  // Linux ②③④: ∩ cgroup 内存余量（不可判定 ⇒ 不参与，不把结果打成 0）。
  const uint64_t cg = cgroup_available_bytes();
  return (cg > 0 && cg < host_available) ? cg : host_available;
#endif
}


// ── 进程树 RSS（压力分子）────────────────────────────────────────────────────
// 机制说明: 本 TU 在 aio 边界**内部**，文件读取仍走 aio 唯一机制原语
// aio_file::read_all / aio_atomic::for_each_child（不在本文件复制第二份
// 打开/读取/枚举通道）。
// Linux 口径:
//   ① 本进程 = /proc/self/status 的 VmRSS（回退 /proc/self/statm 的 resident 页
//      × sysconf(_SC_PAGESIZE)）。
//   ② 后代 = 单次枚举 /proc，对每个**纯数字名**目录读 /proc/<pid>/status 取
//      PPid 与 VmRSS，建 pid→ppid 关系图；自本进程 pid 起 BFS，后代 RSS 累加。
//      枚举/读取失败的项**跳过**（/proc 是易变目录: 进程在 readdir 与 open 之间
//      消亡属正常，不是错误）⇒ 得到的是**下界**。
//   ③ 枚举器本身返回错误（例如 /proc 不可读）⇒ 本条**不参与**，结果回落到 ①
//      （fail-open 到本进程口径，不返回 0），与 cgroup 项「不可判定即不参与」
//      同款处置。理由: 本进程 RSS 已是压力的有效下界，把它打成 0 会让压力门
//      永久失效（静默失效比保守下界更危险）。
//   ④ 生产计算路径不 fork（唯一子进程创建点 lib/infrastructure/cli/process.cpp
//      用于 run 收尾的图渲染，不在计算期）；因此 ② 在正常生产中恒为空集，
//      进程树 RSS == 本进程 RSS，与外部看门狗 mem_guard.py 的进程树求和一致。
#ifndef _WIN32
namespace {

// /proc/<pid>/status → (VmRSS 字节, PPid)。读失败/字段缺失 ⇒ false 或字段留哨兵。
bool read_proc_status(long pid, uint64_t* rss_out, long* ppid_out) {
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%ld/status", pid);
  std::string text;
  if (!aio_file::read_all(path, &text)) return false;
  uint64_t rss = 0;
  long ppid = -1;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    unsigned long long kb = 0;
    long v = 0;
    if (std::sscanf(line.c_str(), "VmRSS: %llu kB", &kb) == 1) {
      rss = static_cast<uint64_t>(kb) * 1024ull;
    } else if (std::sscanf(line.c_str(), "PPid: %ld", &v) == 1) {
      ppid = v;
    }
  }
  if (ppid < 0) return false;   // 无 PPid ⇒ 不是可用的 status 内容
  *rss_out = rss;
  *ppid_out = ppid;
  return true;
}

// 纯数字目录名 ⇒ PID；否则返回 -1（/proc 下非进程项: sys/ net/ self/ 等）。
long proc_pid_from_name(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  const std::string name =
      (slash == std::string::npos) ? path : path.substr(slash + 1);
  if (name.empty() || name.size() > 10) return -1;
  for (char c : name)
    if (c < '0' || c > '9') return -1;
  return std::strtol(name.c_str(), nullptr, 10);
}

}  // namespace
#endif  // !_WIN32

extern "C" uint64_t aio_process_tree_rss_bytes(void) {
#ifdef _WIN32
  // Windows: 仅本进程（后代枚举未接入，登记为未核实项；见头文件平台口径）。
  HANDLE proc = GetCurrentProcess();
  PROCESS_MEMORY_COUNTERS_EX pmc;
  std::memset(&pmc, 0, sizeof(pmc));
  pmc.cb = sizeof(pmc);
  if (GetProcessMemoryInfo(proc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                           sizeof(pmc)))
    return static_cast<uint64_t>(pmc.WorkingSetSize);
  return 0;
#else
  const long self_pid = static_cast<long>(::getpid());
  uint64_t self_rss = 0;
  long self_ppid = -1;
  if (read_proc_status(self_pid, &self_rss, &self_ppid)) {
    // ① 命中
  } else {
    // ① 回退: /proc/self/statm 第二字段 = resident 页数
    std::string statm;
    if (aio_file::read_all("/proc/self/statm", &statm)) {
      unsigned long long size_pages = 0, resident_pages = 0;
      if (std::sscanf(statm.c_str(), "%llu %llu", &size_pages, &resident_pages) == 2) {
        const long psize = sysconf(_SC_PAGESIZE);
        if (psize > 0) self_rss = static_cast<uint64_t>(resident_pages) *
                                  static_cast<uint64_t>(psize);
      }
    }
    if (self_rss == 0) return 0;   // 本进程都不可判定 ⇒ fail-closed
  }
  // ② 后代: 单次枚举 /proc 建 pid→(ppid, rss) 图
  std::map<long, std::pair<long, uint64_t>> procs;
  (void)aio_atomic::for_each_child(
      "/proc",
      [&](const std::string& child, int kind) -> int {
        if (kind != 1) return 0;                 // 只看目录
        const long pid = proc_pid_from_name(child);
        if (pid <= 0 || pid == self_pid) return 0;
        uint64_t rss = 0;
        long ppid = -1;
        if (!read_proc_status(pid, &rss, &ppid)) return 0;  // 已消亡/权限 ⇒ 跳过
        procs[pid] = {ppid, rss};
        return 0;
      },
      nullptr);
  // ③ BFS: 自 self_pid 起收集全部后代
  uint64_t total = self_rss;
  if (!procs.empty()) {
    std::vector<long> frontier{self_pid};
    std::set<long> seen{self_pid};
    while (!frontier.empty()) {
      const long cur = frontier.back();
      frontier.pop_back();
      for (const auto& kv : procs) {
        if (kv.second.first == cur && seen.insert(kv.first).second) {
          total += kv.second.second;
          frontier.push_back(kv.first);
        }
      }
    }
  }
  return total;
#endif
}
