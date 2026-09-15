// tests/unit/p36_resource_segments_test.cpp — P36 D2 资源记录分帧/分段设域单元测试
// 覆盖: UTC 解析 / 分段窗口聚合 (段内 RSS 峰值) / 重叠段同时归属一个样本 /
//       write_all 产出 resource_segments.json 且 resource_summary.json 带 frame_id。
#include "resource_recorder.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>
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
  // 1) UTC 解析 (确定值; 失败返回 -1 不伪造)
  CHECK(astrocs::res_parse_utc_ms("1970-01-01T00:00:01.500Z") == 1500);
  CHECK(astrocs::res_parse_utc_ms("1970-01-01T00:00:02Z") == 2000);
  CHECK(astrocs::res_parse_utc_ms("garbage") == -1);

  // 2) 分段窗口聚合: 两个重叠段 (A[1,5], B[4,8]) + 样本 RSS 阶梯
  std::vector<astrocs::ResRecord> samples;
  for (int i = 0; i <= 10; ++i) {
    astrocs::ResRecord r;
    r.elapsed_seconds = static_cast<double>(i);
    r.cpu_pct = 100.0 + static_cast<double>(i);
    r.rss_bytes = static_cast<uint64_t>(100 + i * 10);
    r.read_bytes = 1;
    r.write_bytes = 2;
    samples.push_back(r);
  }
  std::vector<astrocs::ResNodeSegmentInput> segs(2);
  segs[0].segment_id = "A";
  segs[0].start_epoch_ms = 2000;  // t=1s (record start = 1000ms)
  segs[0].end_epoch_ms = 6000;    // t=5s
  segs[0].granted_workers = 2;
  segs[1].segment_id = "B";
  segs[1].start_epoch_ms = 5000;  // t=4s
  segs[1].end_epoch_ms = 9000;    // t=8s
  segs[1].granted_workers = 1;
  const auto stats = astrocs::res_segment_stats(samples, /*record_start_epoch_ms=*/1000,
                                                "frameX", segs);
  CHECK(stats.size() == 2);
  CHECK(stats[0].segment_id == "A");
  CHECK(stats[0].frame_id == "frameX");
  CHECK(stats[0].start_seconds > 0.999 && stats[0].start_seconds < 1.001);
  CHECK(stats[0].end_seconds > 4.999 && stats[0].end_seconds < 5.001);
  CHECK(stats[0].rss_peak_bytes == 150);  // 样本 t=5 -> 100+50
  CHECK(stats[0].n_samples == 5);         // t=1..5
  CHECK(stats[1].rss_peak_bytes == 180);  // 样本 t=8 -> 100+80
  CHECK(stats[1].granted_workers == 1);

  // 3) 重叠归属: t=4.5 同时落在 A 与 B
  std::string joined;
  CHECK(astrocs::res_sample_in_segments(4.5, stats, &joined));
  CHECK(joined == "A;B");
  joined.clear();
  CHECK(!astrocs::res_sample_in_segments(9.5, stats, &joined));
  CHECK(joined.empty());

  // 4) 产物落盘 (tmp 目录, 不入仓库)
  std::error_code ec;
  const auto dir = std::filesystem::temp_directory_path(ec) /
                   ("p36_res_seg_" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir, ec);
  astrocs::ResourceRecorder rec(0.25);
  rec.set_stage(astrocs::ResStage::Active);
  rec.set_workers(2, 2);
  for (int i = 0; i < 4; ++i) {
    astrocs::ProcSample s;
    s.d_cpu_seconds = 0.1;
    s.rss_bytes = static_cast<uint64_t>(1000 + i * 100);
    rec.record(s);
  }
  const std::string adm = "{\"k_mem\":1,\"reason\":\"memory_insufficient\"}";
  bool wrote = rec.write_all(dir.string(), 1.0, 0.01, "runX", "frameX", segs, adm);
  CHECK(wrote);
  {
    std::ifstream f((dir / "resource_segments.json").string());
    std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(body.find("astrocs.resource_segments") != std::string::npos);
    CHECK(body.find("\"segment_id\":\"A\"") != std::string::npos);
    CHECK(body.find("\"frame_id\":\"frameX\"") != std::string::npos);
  }
  {
    std::ifstream f((dir / "resource_summary.json").string());
    std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(body.find("\"frame_id\":\"frameX\"") != std::string::npos);
    CHECK(body.find("\"n_segments\":2") != std::string::npos);
    CHECK(body.find("memory_insufficient") != std::string::npos);
  }
  {
    std::ifstream f((dir / "resource_samples.csv").string());
    std::string line;
    std::getline(f, line);  // header
    CHECK(line.find("frame_id,segments") != std::string::npos);
  }
  std::filesystem::remove_all(dir, ec);

  if (failures == 0) {
    std::printf("P36_RESOURCE_SEGMENTS_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "P36_RESOURCE_SEGMENTS_FAIL failures=%d\n", failures);
  return 1;
}
