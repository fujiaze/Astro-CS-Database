// cli/resource_recorder.h — MON-001 (G3) 资源记录器
// 每个 heavy node 自动生成 resource_samples.csv / resource_summary.json / worker_balance.csv
// (无需操作者额外脚本)。分 init/active/flush 阶段; 样本含 elapsed/进程与系统 CPU/
// active+runnable workers/RSS/PSS/commit/fault/read/write/queue depth/lock wait/progress。
// Windows/Linux 统一 cpu_pct 单位 = "100% = 1 核满载"(percent_of_one_core,
// 即等价核数×100; 4 核满载=400)。**不是** "100%=全部分配核用满"——已分配容量
// 归一只在门禁侧完成(见 resource_gate.h::cpu_percent_of_allocated_capacity 与
// commands.cpp run_with_resource_gate); M5a-G-002 修复前该自证字段与事实相反。
// 采样开销用真实 wall 总开销计算(ProcessMonitor 已自测每样本开销)。
// ABI 冻结(v1)不改公共 ABI; 本模块纯 CLI 侧内部工具。
//
// P36 D2 (分帧/分段可读):
//   §10.5 指标原为进程级单曲线, 节点重叠后分不清"本分段/本帧"与邻段/邻帧的 RSS
//   爬坡 (P24 §2.8 实测门禁误报)。本记录器新增**分段设域**:
//     - resource_samples.csv 追加 frame_id / segments 两列 (哪些节点执行窗口覆盖
//       该样本), 供外挂监控按帧/按段过滤;
//     - resource_segments.json = 每帧每分段的边界 + 段内 RSS 峰值 + CPU 均值等;
//     - resource_summary.json 追加 frame_id / n_segments / 可选 admission (D5)。
//   分段边界来自 Runtime 的真实节点观测 (NodeTrace: UTC 起点 + duration_ms), 不
//   由配置值冒充; 判定仍在外挂曲线 + 前台 (P26 记录/裁决分离), 本文件不新增任何
//   程序内失败语义。
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
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
    double cpu_pct = 0.0;           // 进程 ΔCPU秒 / 采样间隔 × 100 = 等效核×100
                                    // (100=1 核满载; 非已分配容量百分比)
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

// ── P36 D2: 分段设域 ──
// 输入: Runtime 真实节点执行窗口 (NodeTrace: started_utc + duration_ms)。
struct ResNodeSegmentInput {
    std::string segment_id;        // 节点 ID = 计算分段标识
    int64_t start_epoch_ms = 0;    // 段起点 UTC epoch ms (Runtime 观测)
    int64_t end_epoch_ms = 0;      // 段终点 UTC epoch ms
    uint32_t granted_workers = 0;  // Runtime 记录的授予宽度 (run 峰值口径)
};

// 输出: 每帧每分段的边界与段内峰值 (供外挂监控/前台判定)。
struct ResSegmentStats {
    std::string frame_id;
    std::string segment_id;
    double start_seconds = 0.0;    // 相对资源记录起点的单调秒
    double end_seconds = 0.0;
    uint64_t rss_peak_bytes = 0;   // 段窗口内进程 RSS 峰值
    uint64_t rss_start_bytes = 0;
    uint64_t rss_end_bytes = 0;
    double cpu_pct_mean = 0.0;     // 段窗口内样本均值 (percent_of_one_core)
    uint32_t granted_workers = 0;
    uint64_t read_bytes = 0;
    uint64_t write_bytes = 0;
    uint64_t n_samples = 0;
};

// "YYYY-MM-DDTHH:MM:SS[.mmm]Z" -> UTC epoch ms; 解析失败返回 -1 (不伪造时间)。
inline int64_t res_parse_utc_ms(const std::string& s) {
    int Y = 0, Mo = 0, D = 0, h = 0, mi = 0, se = 0, ms = 0;
    const int n = std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d.%d",
                              &Y, &Mo, &D, &h, &mi, &se, &ms);
    if (n < 6) return -1;
    std::tm tm{};
    tm.tm_year = Y - 1900;
    tm.tm_mon = Mo - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
    const std::time_t t = timegm(&tm);
    if (t == static_cast<std::time_t>(-1)) return -1;
    return static_cast<int64_t>(t) * 1000 + (n >= 7 ? ms : 0);
}

// 段窗口聚合 (纯函数): 用 samples 的时间轴给每个分段设域。
inline std::vector<ResSegmentStats> res_segment_stats(
    const std::vector<ResRecord>& samples, int64_t record_start_epoch_ms,
    const std::string& frame_id,
    const std::vector<ResNodeSegmentInput>& segs) {
    std::vector<ResSegmentStats> out;
    for (const auto& s : segs) {
        ResSegmentStats st;
        st.frame_id = frame_id;
        st.segment_id = s.segment_id;
        st.granted_workers = s.granted_workers;
        if (record_start_epoch_ms <= 0 || s.start_epoch_ms <= 0) {
            out.push_back(std::move(st));  // 不可定位: 边界留 0 (不冒充)
            continue;
        }
        st.start_seconds =
            static_cast<double>(s.start_epoch_ms - record_start_epoch_ms) / 1000.0;
        st.end_seconds =
            static_cast<double>(s.end_epoch_ms - record_start_epoch_ms) / 1000.0;
        if (st.end_seconds < st.start_seconds) st.end_seconds = st.start_seconds;
        double sum_cpu = 0.0;
        uint64_t rd = 0, wr = 0;
        bool first = true;
        for (const auto& r : samples) {
            if (r.elapsed_seconds + 1e-9 < st.start_seconds) continue;
            if (r.elapsed_seconds - 1e-9 > st.end_seconds) continue;
            if (first) { st.rss_start_bytes = r.rss_bytes; first = false; }
            st.rss_end_bytes = r.rss_bytes;
            if (r.rss_bytes > st.rss_peak_bytes) st.rss_peak_bytes = r.rss_bytes;
            sum_cpu += r.cpu_pct;
            rd += r.read_bytes;
            wr += r.write_bytes;
            ++st.n_samples;
        }
        st.cpu_pct_mean = st.n_samples ? sum_cpu / static_cast<double>(st.n_samples) : 0.0;
        st.read_bytes = rd;
        st.write_bytes = wr;
        out.push_back(std::move(st));
    }
    return out;
}

// 某样本时刻是否落在分段窗口内 (供 CSV segments 列)。
inline bool res_sample_in_segments(double t,
                                   const std::vector<ResSegmentStats>& segs,
                                   std::string* joined) {
    bool any = false;
    for (const auto& s : segs) {
        if (t + 1e-9 < s.start_seconds) continue;
        if (t - 1e-9 > s.end_seconds) continue;
        if (any) joined->push_back(';');
        joined->append(s.segment_id);
        any = true;
    }
    return any;
}

// 记录器: 线程安全; 采样由外部(monitor 线程)驱动; 阶段/注入由执行线程设置。
class ResourceRecorder {
public:
    explicit ResourceRecorder(double interval_seconds = 0.25) : interval_(interval_seconds) {
        start_epoch_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    }

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
        // 单位 = 100×等效核(percent_of_one_core); 下游门禁按 allocated_capacity
        // 归一后才与 85%/90% 阈值可比(M5a-G-002)。
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

    // 生成产物(由 run 命令在收尾调用):
    //  out_dir/resource_samples.csv, out_dir/resource_summary.json,
    //  out_dir/worker_balance.csv, out_dir/resource_segments.json (P36 D2)
    //  frame_id: 帧域标识 (单帧 phase1 run = 输入 light basename / run_id);
    //  segments: Runtime 真实节点执行窗口 (可为空 = 兼容旧调用面);
    //  admission_json: D5 帧级准入判定 JSON 对象文本 (空 = 不写该键)。
    bool write_all(const std::string& out_dir, double wall_total, double sample_overhead_ms,
                        const std::string& run_id = "",
                        const std::string& frame_id = std::string(),
                        const std::vector<ResNodeSegmentInput>& segments =
                            std::vector<ResNodeSegmentInput>(),
                        const std::string& admission_json = std::string());

    // 摘要(供测试/事件): 按阶段统计
    std::vector<ResStageStats> stage_stats() const;

    uint64_t n_samples() const { return n_; }

    // MON-002 first-10s gate: 取 elapsed<=max_elapsed 的样本只读快照
    std::vector<ResRecord> records_upto(double max_elapsed) const {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<ResRecord> out;
        for (const auto& r : records_)
            if (r.elapsed_seconds <= max_elapsed) out.push_back(r);
        return out;
    }

    // MON-001: 取指定阶段(init|active|flush)的样本只读快照
    std::vector<ResRecord> records_stage(ResStage stage) const {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<ResRecord> out;
        for (const auto& r : records_)
            if (std::string(r.stage) == res_stage_name(stage)) out.push_back(r);
        return out;
    }

    // 已记录样本总数快照（MON-002 resource summary 事件 raw_n 字段）。
    std::size_t record_count() const {
        std::lock_guard<std::mutex> lk(mu_);
        return records_.size();
    }

    // P36 D2: 资源记录起点 UTC epoch ms (分段边界对齐用)。
    int64_t start_epoch_ms() const { return start_epoch_ms_; }

private:
    double interval_;
    SteadyClock::time_point t0_{SteadyClock::now()};
    int64_t start_epoch_ms_ = 0;
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
        st_.cpu_pct_mean = sum_cpu / static_cast<double>(recs.size());
        st_.cpu_pct_p50 = percentile_sorted(cpus, 0.50);
        st_.cpu_pct_p95 = percentile_sorted(cpus, 0.95);
        st_.cpu_pct_peak = *std::max_element(cpus.begin(), cpus.end());
        st_.workers_mean = sum_w / static_cast<double>(recs.size());
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
                                        double sample_overhead_ms,
                                        const std::string& run_id,
                                        const std::string& frame_id,
                                        const std::vector<ResNodeSegmentInput>& segments,
                                        const std::string& admission_json) {
    std::vector<ResRecord> snap;
    {
        std::lock_guard<std::mutex> lk(mu_);
        snap = records_;
    }
    // P36 D2: 分段设域 (纯函数; 空 segments ⇒ 旧行为完全不变)
    const std::vector<ResSegmentStats> seg_stats =
        res_segment_stats(snap, start_epoch_ms_, frame_id, segments);
    uint64_t frame_rss_peak = 0;
    for (const auto& r : snap) if (r.rss_bytes > frame_rss_peak) frame_rss_peak = r.rss_bytes;
    uint64_t frame_read = 0, frame_write = 0;
    for (const auto& r : snap) { frame_read += r.read_bytes; frame_write += r.write_bytes; }
    // resource_samples.csv (P36: 追加 frame_id/segments 两列, 既有列序不变)
    {
        std::FILE* f = std::fopen((out_dir + "/resource_samples.csv").c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "elapsed_seconds,stage,cpu_pct,system_cpu_pct,active_workers,"
                        "runnable_workers,rss_bytes,pss_bytes,commit_bytes,page_faults,"
                        "read_bytes,write_bytes,queue_depth,lock_wait_ns,progress,"
                        "frame_id,segments\n");
        for (const auto& r : snap) {
            std::string joined;
            res_sample_in_segments(r.elapsed_seconds, seg_stats, &joined);
            std::fprintf(f, "%.3f,%s,%.2f,%.2f,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,"
                            "%llu,%llu,%.3f,%s,%s\n",
                         r.elapsed_seconds, r.stage, r.cpu_pct, r.system_cpu_pct,
                         r.active_workers, r.runnable_workers,
                         (unsigned long long)r.rss_bytes, (unsigned long long)r.pss_bytes,
                         (unsigned long long)r.commit_bytes, (unsigned long long)r.page_faults,
                         (unsigned long long)r.read_bytes, (unsigned long long)r.write_bytes,
                         (unsigned long long)r.queue_depth, (unsigned long long)r.lock_wait_ns,
                         r.progress, frame_id.c_str(), joined.c_str());
        }
        std::fclose(f);
    }
    // resource_summary.json
    {
        std::FILE* f = std::fopen((out_dir + "/resource_summary.json").c_str(), "w");
        if (!f) return false;
        // M5a-G-002: cpu_pct 的单位是 percent_of_one_core(=100×等效核), 不是
        // 已分配容量百分比; 旧字段无条件自证 true 与采集事实相反。保留旧键名但
        // 置 false(下游解析兼容), 并给出真实单位键。
        std::fprintf(f, "{\"run_id\":\"%s\",\"frame_id\":\"%s\",\"n_samples\":%zu,\"wall_seconds\":%.3f,"
                        "\"sample_overhead_ms\":%.3f,"
                        "\"normalized_cpu_100pct_all_allocated_cores\":false,"
                        "\"cpu_pct_units\":\"percent_of_one_core\","
                        "\"rss_peak_bytes\":%llu,\"io_read_bytes\":%llu,\"io_write_bytes\":%llu,"
                        "\"n_segments\":%zu,\"stages\":[",
                        run_id.c_str(), frame_id.c_str(),
                        snap.size(), wall_total, sample_overhead_ms,
                        (unsigned long long)frame_rss_peak,
                        (unsigned long long)frame_read, (unsigned long long)frame_write,
                        seg_stats.size());
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
        std::fprintf(f, "]");
        // P36 D5: 帧级准入判定 (调用方写入; 空则省略键, 不造占位)
        if (!admission_json.empty()) {
            std::fprintf(f, ",\"admission\":%s", admission_json.c_str());
        }
        std::fprintf(f, "}\n");
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
    // P36 D2: resource_segments.json —— 每帧每分段边界 + 段内峰值 (外挂监控/前台判定用)
    {
        std::FILE* f = std::fopen((out_dir + "/resource_segments.json").c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "{\"schema_version\":1,\"kind\":\"astrocs.resource_segments\","
                        "\"run_id\":\"%s\",\"frame_id\":\"%s\",\"wall_seconds\":%.3f,"
                        "\"rss_peak_bytes\":%llu,\"io_read_bytes\":%llu,"
                        "\"io_write_bytes\":%llu,\"frames\":[{\"frame_id\":\"%s\","
                        "\"n_segments\":%zu,\"rss_peak_bytes\":%llu,\"segments\":[",
                        run_id.c_str(), frame_id.c_str(), wall_total,
                        (unsigned long long)frame_rss_peak,
                        (unsigned long long)frame_read, (unsigned long long)frame_write,
                        frame_id.c_str(), seg_stats.size(),
                        (unsigned long long)frame_rss_peak);
        for (std::size_t i = 0; i < seg_stats.size(); ++i) {
            const auto& s = seg_stats[i];
            std::fprintf(f, "%s{\"segment_id\":\"%s\",\"start_seconds\":%.3f,"
                            "\"end_seconds\":%.3f,\"duration_seconds\":%.3f,"
                            "\"rss_peak_bytes\":%llu,\"rss_start_bytes\":%llu,"
                            "\"rss_end_bytes\":%llu,\"cpu_pct_mean\":%.2f,"
                            "\"granted_workers\":%u,\"read_bytes\":%llu,"
                            "\"write_bytes\":%llu,\"n_samples\":%llu}",
                            (i ? "," : ""), s.segment_id.c_str(), s.start_seconds,
                            s.end_seconds, s.end_seconds - s.start_seconds,
                            (unsigned long long)s.rss_peak_bytes,
                            (unsigned long long)s.rss_start_bytes,
                            (unsigned long long)s.rss_end_bytes, s.cpu_pct_mean,
                            s.granted_workers, (unsigned long long)s.read_bytes,
                            (unsigned long long)s.write_bytes,
                            (unsigned long long)s.n_samples);
        }
        std::fprintf(f, "]}]}\n");
        std::fclose(f);
    }
    return true;
}

}  // namespace astrocs
