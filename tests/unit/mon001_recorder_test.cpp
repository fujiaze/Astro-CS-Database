// tests/unit/mon001_recorder_test.cpp — MON-001 (G3) 资源记录器单元测试
// 覆盖: 采样记录/阶段分段(init/active/flush)/percentile 统计(p50/p95)/三产物写入
//       (resource_samples.csv / resource_summary.json / worker_balance.csv)/开销字段。
#include "resource_recorder.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
    astrocs::ResourceRecorder rec(0.25);
    // 构造 10 个样本: init 2 / active 6 / flush 2; active 高 CPU 多 worker
    for (int i = 0; i < 2; ++i) {
        astrocs::ProcSample s;
        s.d_cpu_seconds = 0.05; s.rss_bytes = 100; s.vms_bytes = 1000;
        s.d_read_bytes = 10; s.d_write_bytes = 5;
        rec.set_stage(astrocs::ResStage::Init);
        rec.set_workers(0, 0);
        rec.record(s);
    }
    rec.set_stage(astrocs::ResStage::Active);
    rec.set_workers(2, 2);
    for (int i = 0; i < 6; ++i) {
        astrocs::ProcSample s;
        s.d_cpu_seconds = 0.22;  // ~88% 单核(0.22/0.25)
        s.rss_bytes = static_cast<uint64_t>(200 + i * 10); s.vms_bytes = 2000;
        rec.record(s);
    }
    rec.set_stage(astrocs::ResStage::Flush);
    rec.set_workers(1, 1);
    for (int i = 0; i < 2; ++i) {
        astrocs::ProcSample s;
        s.d_cpu_seconds = 0.01; s.rss_bytes = 300; s.vms_bytes = 1500;
        rec.record(s);
    }
    CHECK(rec.n_samples() == 10);

    // 阶段统计: active 6 样本, workers_mean≈2, cpu p50≈88
    const auto stats = rec.stage_stats();
    CHECK(stats.size() == 3);
    const astrocs::ResStageStats* act = nullptr;
    for (const auto& s : stats) if (std::string(s.stage) == "active") act = &s;
    CHECK(act != nullptr);
    CHECK(act->n_samples == 6);
    CHECK(act->workers_mean > 1.9 && act->workers_mean <= 2.0);
    CHECK(act->cpu_pct_p50 > 80.0 && act->cpu_pct_p50 < 95.0);
    CHECK(act->rss_peak_bytes >= 250);   // 200+5*10

    // percentile 辅助(最近秩: 偶数集合中位数取上中位)
    {
        std::vector<double> v = {1, 2, 3, 4};
        CHECK(astrocs::percentile_sorted(v, 0.5) == 3.0);
        CHECK(astrocs::percentile_sorted(v, 0.95) == 4.0);
    }

    // 三产物写入
    const std::string dir = "/tmp/mon001_test_out";
    std::system(("rm -rf " + dir).c_str());
    std::system(("mkdir -p " + dir).c_str());
    CHECK(rec.write_all(dir, 2.5, 0.1));
    std::ifstream csv(dir + "/resource_samples.csv");
    std::string line;
    std::getline(csv, line);
    CHECK(line.rfind("elapsed_seconds", 0) == 0);   // header
    std::getline(csv, line);
    CHECK(line.find("init") != std::string::npos);  // 首样本 init
    std::ifstream sum(dir + "/resource_summary.json");
    std::string js((std::istreambuf_iterator<char>(sum)), {});
    CHECK(js.find("\"n_samples\":10") != std::string::npos);
    CHECK(js.find("\"normalized_cpu_100pct_all_allocated_cores\":true") != std::string::npos);
    CHECK(js.find("\"stage\":\"active\"") != std::string::npos);
    CHECK(js.find("sample_overhead_ms") != std::string::npos);
    std::ifstream bal(dir + "/worker_balance.csv");
    std::getline(bal, line);
    CHECK(line.rfind("elapsed_seconds", 0) == 0);
    std::getline(bal, line);
    CHECK(line.find(",") != std::string::npos);

    if (failures == 0) {
        std::printf("MON-001 TESTS PASS (采样/阶段分段/p50-p95/三产物/开销字段)\n");
        return 0;
    }
    std::fprintf(stderr, "MON-001 TESTS FAIL (%d)\n", failures);
    return 1;
}
