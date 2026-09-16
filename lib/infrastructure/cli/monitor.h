// astrocs 进程/系统资源监控模块 (MON-001) — Linux /proc 读取 + Windows API 桩
// 07 §2 必采指标: 进程 user/sys CPU、等效核数(平均/峰值)、thread/runnable/ctxsw、
// RSS/PSS、系统可用内存/swap/page-faults、进程 read/write bytes/ops、wall/吞吐。
// 单调时间(steady_clock), 采样开销可测量(见 overhead())。原始 timeseries 留节点;
// 本模块产出"摘要 + 降采样样本"结构, 供 CLI 以 resource 事件分层输出。
// 禁止硬编码线程数/频率; ABI 冻结(v1), 不改公共 ABI——仅 CLI 侧内部工具。
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
// WIN-001: windows.h 噪音宏 (ERROR/OPTIONAL/REQUIRED/interface/NEAR/FAR/small/DELETE/YIELD/
// TRUE/FALSE 等) 会污染其后 include 的 AstroCS 头 (enum 值/标识符, C2143/C2065 级联)。
// windows.h 内宏用途已展开完毕; undef 恢复干净命名空间 (monitor.h 是主 CLI 链唯一
// windows.h 引入点, 此处清理一次保护全部下游头; 与下游公共头的 include 自清理配合)。
#ifdef ERROR
#undef ERROR
#endif
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#ifdef REQUIRED
#undef REQUIRED
#endif
#ifdef interface
#undef interface
#endif
#ifdef NEAR
#undef NEAR
#endif
#ifdef FAR
#undef FAR
#endif
#ifdef small
#undef small
#endif
#ifdef DELETE
#undef DELETE
#endif
#ifdef YIELD
#undef YIELD
#endif
#else
#include <dirent.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace astrocs {

// 单调时钟(steady), 与墙钟(UTC)双源: 采集耗时/开销用 steady, 事件时间戳用 UTC。
using SteadyClock = std::chrono::steady_clock;
using SteadyNs = std::chrono::nanoseconds;

struct ProcSample {
    double cpu_seconds = 0.0;      // 进程 user+sys CPU 时间(秒)
    double user_seconds = 0.0;
    double sys_seconds = 0.0;
    uint64_t rss_bytes = 0;        // /proc/self/status VmRSS
    uint64_t pss_bytes = 0;        // /proc/self/smaps_rollup Pss(可得时)
    uint64_t vms_bytes = 0;        // VmSize
    uint32_t threads = 0;          // /proc/self/status Threads
    uint64_t ctx_switches = 0;     // ctxt_switches(voluntary+nonvoluntary)
    uint64_t read_bytes = 0;       // /proc/self/io read_bytes
    uint64_t write_bytes = 0;      // /proc/self/io write_bytes
    uint64_t read_ops = 0;         // rchar
    uint64_t write_ops = 0;        // wchar
    uint64_t sys_mem_avail = 0;    // MemAvailable(kB)
    uint64_t sys_swap_free = 0;    // SwapFree(kB)
    uint64_t page_faults = 0;      // minor+major(page faults)
    // RUNTIME-CI-001 (§10.5 I/O wait): /proc/stat 首行 iowait(系统级, 秒)。
    double sys_io_wait_seconds = 0.0;

    // RUNTIME-CI-001 (§10.5 每线程 CPU): 逐线程累计 CPU(utime+stime, 秒)快照。
    // thread_cpu 为 (tid, 累计秒) 列表; max/sum 由快照直接得出。
    // Windows 无线程级廉价等价 → 保持空/0(未观测哨兵), 不臆造。
    std::vector<std::pair<uint32_t, double>> thread_cpu;
    double per_thread_cpu_max_seconds = 0.0;   // 单线程累计 CPU 峰值
    double per_thread_cpu_sum_seconds = 0.0;   // 全部线程累计 CPU 之和 = 等效核秒
    uint32_t threads_seen = 0;             // 快照线程数

    // 与上一采样差值的微指标
    double d_cpu_seconds = 0.0;
    uint64_t d_rss_bytes = 0;
    uint64_t d_read_bytes = 0;
    uint64_t d_write_bytes = 0;
    uint64_t d_ctx_switches = 0;
    // §10.5 每线程 CPU 区间增量
    double d_per_thread_cpu_max_seconds = 0.0;
    double d_per_thread_cpu_sum_seconds = 0.0;
    uint32_t d_active_compute_threads = 0;  // 区间内有正向 CPU 增量的线程数
    // §10.5 I/O wait 区间增量(系统级 iowait 秒)
    double d_sys_io_wait_seconds = 0.0;
};

// /proc 采样(无依赖 /proc 的字段保持 0)。返回 false 仅当无法打开关键文件。
inline bool read_proc_self(ProcSample& s) {
#if defined(_WIN32)
    // MON-004 (WIN hosted): /proc 不存在, 用 Win32 进程/系统采样补齐指标。
    // 语义映射: rss=WorkingSetSize, vms=PagefileUsage, page_faults=PageFaultCount,
    // threads=Thread32 快照按 pid 过滤计数 (PROCESS_MEMORY_COUNTERS_EX 无线程/IO
    // 成员, 不能走 pmc), read/write bytes/ops=GetProcessIoCounters(IO_COUNTERS),
    // sys_mem_avail=GlobalMemoryStatusEx.ullAvailPhys;
    // ctx_switches 无 Win32 廉价等价 → 保持 0 (摘要侧为单调增量, 0 无害)。
    HANDLE proc = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS_EX pmc;
    ZeroMemory(&pmc, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(proc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        s.rss_bytes = static_cast<std::uint64_t>(pmc.WorkingSetSize);
        s.vms_bytes = static_cast<std::uint64_t>(pmc.PagefileUsage);
        s.page_faults = static_cast<std::uint64_t>(pmc.PageFaultCount);
    }
    // threads: 全系统线程快照按 owner pid 过滤计数。快照失败(INVALID_HANDLE_VALUE)
    // 时 threads 保持 0, 不抛错; 采样线程内执行, 无共享状态竞争。
    // 开销: 0.5s 周期下数千线程节点快照毫秒级(<周期 1%), 每样本全量计数保证
    // summary.max_threads 峰值语义不被降频低估。
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        const DWORD pid = GetCurrentProcessId();
        std::uint32_t n_threads = 0;
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) ++n_threads;
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
        s.threads = n_threads;
    }
    // 进程 IO 计数(传输字节 + 操作次数); 失败时各字段保持 0。
    IO_COUNTERS ioc;
    ZeroMemory(&ioc, sizeof(ioc));
    if (GetProcessIoCounters(proc, &ioc)) {
        s.read_bytes = static_cast<std::uint64_t>(ioc.ReadTransferCount);
        s.write_bytes = static_cast<std::uint64_t>(ioc.WriteTransferCount);
        s.read_ops = static_cast<std::uint64_t>(ioc.ReadOperationCount);
        s.write_ops = static_cast<std::uint64_t>(ioc.WriteOperationCount);
    }
    MEMORYSTATUSEX ms;
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        s.sys_mem_avail = static_cast<std::uint64_t>(ms.ullAvailPhys);
    }
    return true;
#else
    bool ok = false;
    // /proc/self/status
    std::FILE* f = std::fopen("/proc/self/status", "r");
    if (f) {
        char line[512];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "VmRSS:", 6) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 6, "%" SCNu64, &v) == 1) s.rss_bytes = v * 1024; }
            else if (std::strncmp(line, "VmSize:", 7) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 7, "%" SCNu64, &v) == 1) s.vms_bytes = v * 1024; }
            else if (std::strncmp(line, "Threads:", 8) == 0) { std::uint32_t v = 0; if (std::sscanf(line + 8, "%u", &v) == 1) s.threads = v; }
            else if (std::strncmp(line, "voluntary_ctxt_switches:", 24) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 24, "%" SCNu64, &v) == 1) s.ctx_switches += v; }
            else if (std::strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 27, "%" SCNu64, &v) == 1) s.ctx_switches += v; }
        }
        std::fclose(f);
        ok = true;
    }
    // /proc/self/io
    f = std::fopen("/proc/self/io", "r");
    if (f) {
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "read_bytes:", 11) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 11, "%" SCNu64, &v) == 1) s.read_bytes = v; }
            else if (std::strncmp(line, "write_bytes:", 12) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 12, "%" SCNu64, &v) == 1) s.write_bytes = v; }
            else if (std::strncmp(line, "rchar:", 6) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 6, "%" SCNu64, &v) == 1) s.read_ops = v; }
            else if (std::strncmp(line, "wchar:", 6) == 0) { std::uint64_t v = 0; if (std::sscanf(line + 6, "%" SCNu64, &v) == 1) s.write_ops = v; }
        }
        std::fclose(f);
    }
    // /proc/stat 首行 "cpu  user nice system idle iowait irq softirq steal ..."
    // iowait 是第 5 个数值(token 6); 单位 = clock ticks → 秒。
    f = std::fopen("/proc/stat", "r");
    if (f) {
        char line[512];
        if (std::fgets(line, sizeof(line), f) && std::strncmp(line, "cpu", 3) == 0) {
            const char* p = line + 3;
            int tok = 0;
            while (*p != '\0' && tok < 5) {
                while (*p == ' ') ++p;
                if (*p == '\0' || *p == '\n') break;
                char* end = nullptr;
                const unsigned long long v = std::strtoull(p, &end, 10);
                ++tok;
                if (tok == 5) {
                    long hz = sysconf(_SC_CLK_TCK);
                    if (hz <= 0) hz = 100;
                    s.sys_io_wait_seconds =
                        static_cast<double>(v) / static_cast<double>(hz);
                }
                p = end;
            }
        }
        std::fclose(f);
    }
#if !defined(_WIN32)
    // 非 Linux /proc 单文件字段: 系统内存/swap 用 sysinfo(cgroup 不感知, 记录为系统级)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        s.sys_mem_avail = static_cast<std::uint64_t>(si.freeram + si.bufferram) * si.mem_unit;
        s.sys_swap_free = static_cast<std::uint64_t>(si.freeswap) * si.mem_unit;
    }
#endif
    (void)ok;
    return true;
#endif  /* _WIN32 */
}

inline void read_cpu_time(ProcSample& s) {
#if defined(_WIN32)
    FILETIME ct, et, kt, ut;
    if (GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
        auto ns = [](const FILETIME& ft) {
            return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32 |
                    ft.dwLowDateTime) / 10000000.0;
        };
        s.user_seconds = ns(ut);
        s.sys_seconds = ns(kt);
        s.cpu_seconds = s.user_seconds + s.sys_seconds;
    }
#else
    std::FILE* f = std::fopen("/proc/self/stat", "r");
    if (f) {
        char buf[4096];
        if (std::fgets(buf, sizeof(buf), f)) {
            const char* lparen = std::strchr(buf, '(');
            const char* rparen = std::strrchr(buf, ')');
            if (lparen && rparen) {
                const char* p = rparen + 2;   // field 3 = state(单字母)
                // field 3 是字母(state), 先跳过它; field 4 起才是数字
                while (*p == ' ') ++p;         // 跳过 state 后空格
                if (*p) ++p;                    // 跳过单个 state 字母
                int field = 4;
                char* end = nullptr;
                while (*p && field <= 15) {
                    while (*p == ' ') ++p;
                    if (!*p) break;
                    long val = std::strtol(p, &end, 10);
                    if (field == 14) s.user_seconds = static_cast<double>(val) / 100.0;   // utime(clock ticks)
                    if (field == 15) s.sys_seconds = static_cast<double>(val) / 100.0;    // stime
                    p = end;
                    ++field;
                }
                s.cpu_seconds = s.user_seconds + s.sys_seconds;
            }
        }
        std::fclose(f);
    }
#endif
}

// RUNTIME-CI-001 (宪章 §10.5 每线程 CPU): 逐线程累计 CPU(utime+stime) 快照。
// 读 /proc/self/task/<tid>/stat; 字段 14/15 = utime/stime(clock ticks)。
// 无 /proc(Windows)或读取失败 → 保持空/0(未观测哨兵; 不臆造、不硬编码线程数)。
inline void read_thread_cpu(ProcSample& s) {
#if !defined(_WIN32)
    DIR* d = opendir("/proc/self/task");
    if (d == nullptr) return;
    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) hz = 100;
    struct dirent* e = nullptr;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        const long tid = std::strtol(e->d_name, nullptr, 10);
        if (tid <= 0) continue;
        const std::string path = std::string("/proc/self/task/") + e->d_name + "/stat";
        std::FILE* f = std::fopen(path.c_str(), "r");
        if (f == nullptr) continue;
        char buf[1024];
        if (std::fgets(buf, sizeof(buf), f) != nullptr) {
            // comm 可含空格/括号 → 以最后一个 ')' 为界, 其后 token1 = field 3(state)。
            const char* rp = std::strrchr(buf, ')');
            if (rp != nullptr) {
                const char* p = rp + 1;
                int tok = 0;
                long utime = -1, stime = -1;
                while (*p != '\0' && tok < 13) {
                    while (*p == ' ') ++p;
                    if (*p == '\0') break;
                    char* end = nullptr;
                    const long v = std::strtol(p, &end, 10);
                    ++tok;
                    if (end == p) { ++p; continue; }   // state 等非数字 token
                    if (tok == 12) utime = v;          // field 14 = utime
                    if (tok == 13) stime = v;          // field 15 = stime
                    p = end;
                }
                if (utime >= 0 && stime >= 0) {
                    const double cpu = static_cast<double>(utime + stime) /
                                       static_cast<double>(hz);
                    s.thread_cpu.emplace_back(static_cast<uint32_t>(tid), cpu);
                }
            }
        }
        std::fclose(f);
    }
    closedir(d);
    for (const auto& t : s.thread_cpu) {
        s.per_thread_cpu_sum_seconds += t.second;
        if (t.second > s.per_thread_cpu_max_seconds) s.per_thread_cpu_max_seconds = t.second;
    }
    s.threads_seen = static_cast<uint32_t>(s.thread_cpu.size());
#endif
}

// 单次采样(填充 ProcSample)。cpu_time 单独读以保证字段齐全。
inline ProcSample sample() {
    ProcSample s;
    read_proc_self(s);
    read_cpu_time(s);
    read_thread_cpu(s);
    return s;
}

// 采样器: 记录 baseline, 周期采样并累计摘要; 自测量每样本开销(07 §1 采样开销测量)。
class ProcessMonitor {
public:
    explicit ProcessMonitor(double interval_seconds = 0.5)
        : interval_(interval_seconds), baseline_(sample()), t0_(SteadyClock::now()),
          last_(baseline_), last_t_(t0_), overhead_ns_(0) {}

    void tick() {
        ProcSample cur = sample();
        cur.d_cpu_seconds = cur.cpu_seconds - last_.cpu_seconds;
        cur.d_rss_bytes = cur.rss_bytes > last_.rss_bytes ? cur.rss_bytes - last_.rss_bytes : 0;
        cur.d_read_bytes = cur.read_bytes - last_.read_bytes;
        cur.d_write_bytes = cur.write_bytes - last_.write_bytes;
        cur.d_ctx_switches = cur.ctx_switches - last_.ctx_switches;
        // RUNTIME-CI-001: 每线程 CPU 区间增量 + 区间内有正向 CPU 的线程数。
        // 单调累计量, 线程退出/新建按 tid 对齐; 新线程的累计值计为增量。
        cur.d_per_thread_cpu_sum_seconds =
            cur.per_thread_cpu_sum_seconds > last_.per_thread_cpu_sum_seconds
                ? cur.per_thread_cpu_sum_seconds - last_.per_thread_cpu_sum_seconds : 0.0;
        cur.d_per_thread_cpu_max_seconds =
            cur.per_thread_cpu_max_seconds > last_.per_thread_cpu_max_seconds
                ? cur.per_thread_cpu_max_seconds - last_.per_thread_cpu_max_seconds : 0.0;
        {
            std::uint32_t active = 0;
            for (const auto& t : cur.thread_cpu) {
                double prev = 0.0;
                bool found = false;
                for (const auto& o : last_.thread_cpu) {
                    if (o.first == t.first) { prev = o.second; found = true; break; }
                }
                if (found ? (t.second > prev) : (t.second > 0.0)) ++active;
            }
            cur.d_active_compute_threads = active;
        }
        cur.d_sys_io_wait_seconds =
            cur.sys_io_wait_seconds > last_.sys_io_wait_seconds
                ? cur.sys_io_wait_seconds - last_.sys_io_wait_seconds : 0.0;
        samples_.push_back(cur);
        last_ = cur;
        ++n_;
    }

    void run_for(double seconds) {
        const auto until = SteadyClock::now() + std::chrono::duration<double>(seconds);
        const auto period = std::chrono::duration_cast<SteadyClock::duration>(
            std::chrono::duration<double>(interval_));
        while (SteadyClock::now() < until) {
            const auto a = SteadyClock::now();
            tick();
            const auto b = SteadyClock::now();
            overhead_ns_ += static_cast<uint64_t>(
                std::chrono::duration_cast<SteadyNs>(b - a).count());
            // 睡眠到下一个采样点(扣除采样耗时), 而非固定 interval, 避免漂移
            const auto wake = a + period;
            const auto now = SteadyClock::now();
            if (now < until) {
                if (wake > now) std::this_thread::sleep_for(wake - now);
            }
        }
    }

    // 摘要: 平均/峰值等价核数、RSS 峰值/斜率、吞吐等(见 07 §2)。
    struct Summary {
        double avg_equivalent_cores = 0.0;
        double peak_equivalent_cores = 0.0;
        uint64_t peak_rss_bytes = 0;
        int64_t rss_slope_bytes_per_s = 0;
        uint64_t total_read_bytes = 0;
        uint64_t total_write_bytes = 0;
        uint64_t total_ctx_switches = 0;
        uint32_t max_threads = 0;
        double wall_seconds = 0.0;
        double avg_cpu_percent = 0.0;
        double sample_overhead_ms = 0.0;
        uint64_t n_samples = 0;
    };

    Summary summary() const {
        Summary s;
        s.n_samples = n_;
        s.wall_seconds =
            std::chrono::duration<double>(SteadyClock::now() - t0_).count();
        uint64_t peaks_rss = baseline_.rss_bytes;
        // RESCUE-FD-08: 峰值线程/RSS 是单点观测量(非统计量), 必须在任何样本数下
        // 计入 —— 采样 1 次也真实观测到 Threads/RSS。此前仅在 samples_.size()>=2
        // 分支内累计, 使短 run(仅 1 个样本)的 max_threads 恒为 0; 而 0 是"未观测"
        // 哨兵, 被 evaluate_gate 的 max_active_threads 回退误用为"只有一个活跃
        // 计算线程"的观测值(误判 single_threaded, exit 10)。判定式与阈值不动。
        s.max_threads = baseline_.threads;
        for (const auto& sm : samples_) {
            if (sm.rss_bytes > peaks_rss) peaks_rss = sm.rss_bytes;
            if (sm.threads > s.max_threads) s.max_threads = sm.threads;
        }
        if (samples_.size() >= 2) {
            double sum_eq = 0.0;
            for (const auto& sm : samples_) {
                // 该区间等价核数 = 区间 CPU 增量 / 区间墙钟增量(clamp 有界)
                const double eq = interval_ > 0 ? (sm.d_cpu_seconds / interval_) : 0.0;
                sum_eq += eq;
                if (eq > s.peak_equivalent_cores) s.peak_equivalent_cores = eq;
                s.total_read_bytes += sm.d_read_bytes;
                s.total_write_bytes += sm.d_write_bytes;
                s.total_ctx_switches += sm.d_ctx_switches;
            }
            s.avg_equivalent_cores = sum_eq / static_cast<double>(samples_.size());
            // RSS 斜率: 末采样 - 首采样 / 墙钟
            const auto& first = samples_.front();
            const auto& last = samples_.back();
            const double dt = std::max(0.001, static_cast<double>(samples_.size()) * interval_);
            s.rss_slope_bytes_per_s =
                static_cast<int64_t>((static_cast<double>(last.rss_bytes) -
                                      static_cast<double>(first.rss_bytes)) / dt);
        }
        s.peak_rss_bytes = peaks_rss;
        s.avg_cpu_percent = s.avg_equivalent_cores * 100.0;  // 等价核数(%) = 等价核数 × 100
        if (n_ > 0) s.sample_overhead_ms = static_cast<double>(overhead_ns_) / static_cast<double>(n_) / 1e6;
        return s;
    }

    uint32_t n_cores_hint() const {
#if defined(_WIN32)
        SYSTEM_INFO inf; GetSystemInfo(&inf);
        return inf.dwNumberOfProcessors;
#else
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        return n > 0 ? static_cast<uint32_t>(n) : 1;
#endif
    }

    // 最近一次样本(供 MON-001 记录器/事件消费; 非线程安全, 由采样线程调用后共享)。
    const ProcSample& last_sample() const { return last_; }

private:
    double interval_;
    ProcSample baseline_;
    SteadyClock::time_point t0_;
    ProcSample last_;
    SteadyClock::time_point last_t_;
    std::vector<ProcSample> samples_;
    uint64_t n_ = 0;
    uint64_t overhead_ns_ = 0;
};

}  // namespace astrocs
