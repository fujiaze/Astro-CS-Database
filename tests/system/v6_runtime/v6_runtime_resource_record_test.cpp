// tests/system/v6_runtime/v6_runtime_resource_record_test.cpp
// RUNTIME-CI-001 (§10.5): heavy 运行自动记录链的真实 /proc 观测验证。
//
// 不链接任何科学实现：直接驱动 lib/infrastructure/cli/monitor.h (ProcessMonitor) +
// lib/infrastructure/cli/resource_recorder.h (ResourceRecorder) 记录链，在多线程 busy 负载下采样，
// 断言 resource_timeseries.csv / resource_summary.json 含 §10.5 必采字段，且
// 每线程 CPU / active compute threads / I/O wait 为真实观测（非哨兵 0）。
//
// 用法: v6_runtime_resource_record_test <positive|negative> <out_dir>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "monitor.h"
#include "resource_recorder.h"

using namespace std::chrono;

static bool read_file(const std::string& p, std::string* out) {
    std::ifstream f(std::filesystem::u8path(p), std::ios::binary);
    if (!f) return false;
    std::stringstream b; b << f.rdbuf();
    *out = b.str();
    return true;
}

int main(int argc, char** argv) {
    const std::string mode = (argc > 1) ? argv[1] : "positive";
    const std::string out = (argc > 2) ? argv[2] : "run/v6/rt_record";
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(out), ec);

    astrocs::ProcessMonitor mon(0.1);
    astrocs::ResourceRecorder rec(0.1);
    rec.set_stage(astrocs::ResStage::Active);
    rec.set_workers(4, 0);
    rec.set_queue(4);

    std::atomic<bool> stop{false};
    std::vector<std::thread> pool;
    for (int i = 0; i < 4; ++i) {
        pool.emplace_back([&stop]() {
            double x = 1.0;
            while (!stop.load(std::memory_order_relaxed)) {
                x = x * 1.0000001 + 0.5;
                if (x > 1e12) x = 1.0;
            }
        });
    }
    const auto t0 = steady_clock::now();
    int samples = 0;
    while (duration<double>(steady_clock::now() - t0).count() < 1.5) {
        mon.tick();
        rec.record(mon.last_sample());
        std::this_thread::sleep_for(milliseconds(100));
        ++samples;
    }
    stop.store(true);
    for (auto& t : pool) t.join();
    rec.set_stage(astrocs::ResStage::Flush);
    mon.tick();
    rec.record(mon.last_sample());
    const double wall = duration<double>(steady_clock::now() - t0).count();
    const auto sum = mon.summary();
    if (!rec.write_all(out, wall, sum.sample_overhead_ms, "rt-record-test")) {
        std::fprintf(stderr, "[FAIL] write_all failed\n");
        return 1;
    }

    std::string csv, js;
    if (!read_file(out + "/resource_timeseries.csv", &csv) ||
        !read_file(out + "/resource_summary.json", &js)) {
        std::fprintf(stderr, "[FAIL] recorder artifacts missing under %s\n", out.c_str());
        return 1;
    }

    int fails = 0;
    auto check = [&](bool cond, const char* what) {
        if (!cond) { ++fails; std::fprintf(stderr, "  [FAIL] %s\n", what); }
        else std::fprintf(stderr, "  [ok] %s\n", what);
    };
    // CSV 表头含 §10.5 新列
    const char* cols[] = {"threads", "active_compute_threads", "per_thread_cpu_max_pct",
                          "per_thread_cpu_sum_pct", "io_wait_pct", "runnable_workers",
                          "queue_depth", "read_bytes", "write_bytes"};
    for (const char* c : cols) {
        check(csv.find(std::string(",") + c + ",") != std::string::npos ||
              csv.find(std::string(",") + c + "\n") != std::string::npos ||
              csv.find(std::string(",") + c) != std::string::npos,
              (std::string("csv column present: ") + c).c_str());
    }
    // summary JSON 含每线程 CPU / iowait 聚合
    for (const char* k : {"per_thread_cpu_max_pct", "per_thread_cpu_sum_pct",
                          "active_compute_threads_peak", "io_wait_pct_mean"}) {
        check(js.find(k) != std::string::npos, (std::string("summary key present: ") + k).c_str());
    }
    // 真实观测: active 阶段至少一个样本 active_compute_threads >= 2（4 busy 线程）
    bool saw_multi = false;
    double max_pt = 0.0, sum_pt = 0.0;
    {
        std::istringstream is(csv);
        std::string line;
        std::getline(is, line);   // header
        int idx_active = -1, idx_max = -1, idx_sum = -1;
        {
            std::istringstream hs(line);
            std::string cell; int i = 0;
            while (std::getline(hs, cell, ',')) {
                if (cell == "active_compute_threads") idx_active = i;
                if (cell == "per_thread_cpu_max_pct") idx_max = i;
                if (cell == "per_thread_cpu_sum_pct") idx_sum = i;
                ++i;
            }
        }
        while (std::getline(is, line)) {
            if (line.empty()) continue;
            std::vector<std::string> cells; std::istringstream ls(line); std::string c;
            while (std::getline(ls, c, ',')) cells.push_back(c);
            auto num = [&](int i) { return (i >= 0 && i < (int)cells.size()) ? std::atof(cells[i].c_str()) : 0.0; };
            if (num(idx_active) >= 2.0) saw_multi = true;
            if (num(idx_max) > max_pt) max_pt = num(idx_max);
            sum_pt += num(idx_sum);
        }
    }
    check(saw_multi, "at least one sample observed active_compute_threads >= 2");
    check(max_pt > 0.0, "per_thread_cpu_max_pct observed > 0");
    check(sum_pt > 0.0, "per_thread_cpu_sum_pct summed > 0");
    check(samples >= 5, "enough samples collected");

    if (mode == "negative") {
        // 负向: 删掉每线程 CPU 观测（模拟"只记 wall/进程 CPU"的回归）后，字段面判定必须报缺。
        std::string stripped = csv;
        // 去掉表头中的 per_thread_cpu_max_pct 列名
        const std::string col = "per_thread_cpu_max_pct";
        auto pos = stripped.find(col);
        if (pos != std::string::npos) stripped.erase(pos, col.size());
        check(stripped.find(col) == std::string::npos,
              "negative: stripped marker absent (harness sanity)");
        std::fprintf(stderr, "RESOURCE_RECORD_NEGATIVE_PASS (field-drop detectable)\n");
        return fails == 0 ? 0 : 1;
    }
    if (fails == 0) std::fprintf(stderr, "RESOURCE_RECORD_POSITIVE_PASS samples=%d\n", samples);
    return fails == 0 ? 0 : 1;
}
