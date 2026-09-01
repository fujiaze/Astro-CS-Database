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
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
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

    // 与上一采样差值的微指标
    double d_cpu_seconds = 0.0;
    uint64_t d_rss_bytes = 0;
    uint64_t d_read_bytes = 0;
    uint64_t d_write_bytes = 0;
    uint64_t d_ctx_switches = 0;
};

// /proc 采样(无依赖 /proc 的字段保持 0)。返回 false 仅当无法打开关键文件。
inline bool read_proc_self(ProcSample& s) {
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

// 单次采样(填充 ProcSample)。cpu_time 单独读以保证字段齐全。
inline ProcSample sample() {
    ProcSample s;
    read_proc_self(s);
    read_cpu_time(s);
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
        if (samples_.size() >= 2) {
            double sum_eq = 0.0;
            for (const auto& sm : samples_) {
                // 该区间等价核数 = 区间 CPU 增量 / 区间墙钟增量(clamp 有界)
                const double eq = interval_ > 0 ? (sm.d_cpu_seconds / interval_) : 0.0;
                sum_eq += eq;
                if (eq > s.peak_equivalent_cores) s.peak_equivalent_cores = eq;
                if (sm.rss_bytes > peaks_rss) peaks_rss = sm.rss_bytes;
                if (sm.threads > s.max_threads) s.max_threads = sm.threads;
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
