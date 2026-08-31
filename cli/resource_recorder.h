// cli/resource_recorder.h — MON-001 (G3) 资源记录器
// 每个 heavy node 自动生成 resource_samples.csv / resource_summary.json / worker_balance.csv
// (无需操作者额外脚本)。分 init/active/flush 阶段; 样本含 elapsed/进程与系统 CPU/
// active+runnable workers/RSS/PSS/commit/fault/read/write/queue depth/lock wait/progress。
// Windows/Linux 统一 "100%=全部分配核用满" normalized 口径。
// 采样开销用真实 wall 总开销计算(ProcessMonitor 已自测每样本开销)。
// ABI 冻结(v1)不改公共 ABI; 本模块纯 CLI 侧内部工具。
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "monitor.h"

namespace astrocs {

// 阶段枚举: init(启动/加载) / active(节点计算) / flush(落盘/收尾)
enum class ResStage { Init, Active, Flush };

inline const char* res_stage_name(ResStage s) {
    switch (s) {
    case ResStage::Init:   return "init";
    case ResStage::Active: return "active";
    case ResStage::Flush:  return "flush";
    }
    return "init";
}

// 单行资源样本(对齐规格字段; 单位已规范化)
struct ResRecord {
    double elapsed_seconds = 0.0;   // 距起始单调秒
    const char* stage = "init";     // init|active|flush
    double cpu_pct = 0.0;           // 进程 CPU / 墙钟 × 100(normalized)
    double system_cpu_pct = 0.0;    // 系统级 CPU 占用(可得时)
    uint32_t active_workers = 0;    // active workers(外部注入)
    uint32_t runnable_workers = 0;  // runnable workers(外部注入)
    uint64_t rss_bytes = 0;
    uint64_t pss_bytes = 0;
    uint64_t commit_bytes = 0;      // VmSize(近似 commit)
    uint64_t page_faults = 0;
    uint64_t read_bytes = 0;
    uint64_t write_bytes = 0;
    uint64_t queue_depth = 0;       // 队列深度(外部注入)
    uint64_t lock_wait_ns = 0;      // 锁等待(外部注入)
    double progress = 0.0;          // 0..1(外部注入)
};

// 每阶段统计: mean/p50/p95/peak/slope(规格: 统计 mean/p50/p95/peak/slope)
struct ResStageStats {
    const char* stage = "init";
    uint64_t n_samples = 0;
    double wall_seconds = 0.0;
    double cpu_pct_mean = 0.0;
    double cpu_pct_p50 = 0.0;
    double cpu_pct_p95 = 0.0;
    double cpu_pct_peak = 0.0;
    double workers_mean = 0.0;
    double workers_p50 = 0.0;
    double workers_peak = 0.0;
    uint64_t rss_peak_bytes = 0;
    int64_t rss_slope_bytes_per_s = 0;
};

// 记录器: 线程安全; 采样由外部(monitor 线程)驱动; 阶段/注入由执行线程设置。
class ResourceRecorder {
public:
    explicit ResourceRecorder(double interval_seconds = 0.25) : interval_(interval_seconds) {}

    // 阶段切换(执行线程调用)
    void set_stage(ResStage s) {
        std::lock_guard<std::mutex> lk(mu_);
        cur_stage_ = s;
    }
    // 外部注入: workers/队列/锁/进度(执行线程在节点边界调用)
    void set_workers(uint32_t active, uint32_t runnable) {
        std::lock_guard<std::mutex> lk(mu_);
        active_workers_ = active; runnable_workers_ = runnable;
    }
    void set_queue(uint64_t depth) {
        std::lock_guard<std::mutex> lk(mu_);
        queue_depth_ = depth;
    }
    void set_progress(double p) {
        std::lock_guard<std::mutex> lk(mu_);
        progress_ = p;
    }

    // 采样(monitor 线程调用; 每次 tick 后调用)
    void record(const ProcSample& s) {
        std::lock_guard<std::mutex> lk(mu_);
        const double now = std::chrono::duration<double>(
            SteadyClock::now() - t0_).count();
        ResRecord r;
        r.elapsed_seconds = now;
        r.stage = res_stage_name(cur_stage_);
        r.cpu_pct = interval_ > 0 ? (s.d_cpu_seconds / interval_) * 100.0 : 0.0;
        r.rss_bytes = s.rss_bytes;
        r.pss_bytes = s.pss_bytes;
        r.commit_bytes = s.vms_bytes;
        r.page_faults = s.page_faults;
        r.read_bytes = s.d_read_bytes;
        r.write_bytes = s.d_write_bytes;
        r.active_workers = active_workers_;
        r.runnable_workers = runnable_workers_;
        r.queue_depth = queue_depth_;
        r.progress = progress_;
        records_.push_back(r);
        ++n_;
    }

    // 生成三个产物(由 run 命令在收尾调用):
    //  out_dir/resource_samples.csv, out_dir/resource_summary.json, out_dir/worker_balance.csv
    bool write_all(const std::string& out_dir, double wall_total, double sample_overhead_ms);

    // 摘要(供测试/事件): 按阶段统计
    std::vector<ResStageStats> stage_stats() const;

    uint64_t n_samples() const { return n_; }

private:
    double interval_;
    SteadyClock::time_point t0_{SteadyClock::now()};
    mutable std::mutex mu_;
    ResStage cur_stage_ = ResStage::Init;
    uint32_t active_workers_ = 0;
    uint32_t runnable_workers_ = 0;
    uint64_t queue_depth_ = 0;
    double progress_ = 0.0;
    std::vector<ResRecord> records_;
    uint64_t n_ = 0;
};

// ---- 统计辅助(内联, 供实现与测试复用) ----
inline double percentile_sorted(std::vector<double>& v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    // 最近秩(round): q=0.5 取中位数, q=0.95 取第 95 分位代表(规格 mean/p50/p95/peak)
    const std::size_t idx = static_cast<std::size_t>(
        q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(idx, v.size() - 1)];
}

inline std::vector<ResStageStats> ResourceRecorder::stage_stats() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<ResStageStats> out;
    for (int si = 0; si < 3; ++si) {
        const ResStage st = static_cast<ResStage>(si);
        ResStageStats st_;
        st_.stage = res_stage_name(st);
        std::vector<double> cpus, workers;
        std::vector<const ResRecord*> recs;
        for (const auto& r : records_) if (std::string(r.stage) == res_stage_name(st)) recs.push_back(&r);
        st_.n_samples = recs.size();
        if (recs.empty()) { out.push_back(st_); continue; }
        double sum_cpu = 0.0, sum_w = 0.0;
        for (const auto* r : recs) {
            cpus.push_back(r->cpu_pct); workers.push_back(r->active_workers);
            sum_cpu += r->cpu_pct; sum_w += r->active_workers;
            if (r->rss_bytes > st_.rss_peak_bytes) st_.rss_peak_bytes = r->rss_bytes;
        }
        st_.wall_seconds = recs.back()->elapsed_seconds - recs.front()->elapsed_seconds;
        st_.cpu_pct_mean = sum_cpu / recs.size();
        st_.cpu_pct_p50 = percentile_sorted(cpus, 0.50);
        st_.cpu_pct_p95 = percentile_sorted(cpus, 0.95);
        st_.cpu_pct_peak = *std::max_element(cpus.begin(), cpus.end());
        st_.workers_mean = sum_w / recs.size();
        st_.workers_p50 = percentile_sorted(workers, 0.50);
        st_.workers_peak = *std::max_element(workers.begin(), workers.end());
        const double dt = std::max(0.001, st_.wall_seconds);
        st_.rss_slope_bytes_per_s = static_cast<int64_t>(
            (static_cast<double>(recs.back()->rss_bytes) -
             static_cast<double>(recs.front()->rss_bytes)) / dt);
        out.push_back(st_);
    }
    return out;
}

inline bool ResourceRecorder::write_all(const std::string& out_dir, double wall_total,
                                        double sample_overhead_ms) {
    std::vector<ResRecord> snap;
    {
        std::lock_guard<std::mutex> lk(mu_);
        snap = records_;
    }
    // resource_samples.csv
    {
        std::FILE* f = std::fopen((out_dir + "/resource_samples.csv").c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "elapsed_seconds,stage,cpu_pct,system_cpu_pct,active_workers,"
                        "runnable_workers,rss_bytes,pss_bytes,commit_bytes,page_faults,"
                        "read_bytes,write_bytes,queue_depth,lock_wait_ns,progress\n");
        for (const auto& r : snap) {
            std::fprintf(f, "%.3f,%s,%.2f,%.2f,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,"
                            "%llu,%llu,%.3f\n",
                         r.elapsed_seconds, r.stage, r.cpu_pct, r.system_cpu_pct,
                         r.active_workers, r.runnable_workers,
                         (unsigned long long)r.rss_bytes, (unsigned long long)r.pss_bytes,
                         (unsigned long long)r.commit_bytes, (unsigned long long)r.page_faults,
                         (unsigned long long)r.read_bytes, (unsigned long long)r.write_bytes,
                         (unsigned long long)r.queue_depth, (unsigned long long)r.lock_wait_ns,
                         r.progress);
        }
        std::fclose(f);
    }
    // resource_summary.json
    {
        std::FILE* f = std::fopen((out_dir + "/resource_summary.json").c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "{\"n_samples\":%zu,\"wall_seconds\":%.3f,\"sample_overhead_ms\":%.3f,"
                        "\"normalized_cpu_100pct_all_allocated_cores\":true,\"stages\":[",
                        snap.size(), wall_total, sample_overhead_ms);
        const auto stats = stage_stats();
        for (std::size_t i = 0; i < stats.size(); ++i) {
            const auto& s = stats[i];
            std::fprintf(f, "%s{\"stage\":\"%s\",\"n_samples\":%llu,\"wall_seconds\":%.3f,"
                            "\"cpu_pct_mean\":%.2f,\"cpu_pct_p50\":%.2f,\"cpu_pct_p95\":%.2f,"
                            "\"cpu_pct_peak\":%.2f,\"workers_mean\":%.2f,\"workers_p50\":%.2f,"
                            "\"workers_peak\":%.2f,\"rss_peak_bytes\":%llu,"
                            "\"rss_slope_bytes_per_s\":%lld}",
                            (i ? "," : ""), s.stage, (unsigned long long)s.n_samples,
                            s.wall_seconds, s.cpu_pct_mean, s.cpu_pct_p50, s.cpu_pct_p95,
                            s.cpu_pct_peak, s.workers_mean, s.workers_p50, s.workers_peak,
                            (unsigned long long)s.rss_peak_bytes,
                            (long long)s.rss_slope_bytes_per_s);
        }
        std::fprintf(f, "]}\n");
        std::fclose(f);
    }
    // worker_balance.csv: 每样本 active vs runnable(供不平衡分类)
    {
        std::FILE* f = std::fopen((out_dir + "/worker_balance.csv").c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "elapsed_seconds,active_workers,runnable_workers,utilization_pct\n");
        for (const auto& r : snap) {
            const double denom = r.active_workers + r.runnable_workers;
            const double util = denom > 0 ? r.active_workers * 100.0 / denom : 0.0;
            std::fprintf(f, "%.3f,%u,%u,%.2f\n", r.elapsed_seconds,
                         r.active_workers, r.runnable_workers, util);
        }
        std::fclose(f);
    }
    return true;
}

}  // namespace astrocs
