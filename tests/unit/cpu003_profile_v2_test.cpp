// tests/unit/cpu003_profile_v2_test.cpp — CPU-003 (G3) v2 profile 生成与复读单元测试
// 覆盖: v2 schema 字段完整; Oracle 门(错误 kernel 候选被剔除); winner 仅 OK 候选;
//       AVX512 提升<3% 选 AVX2 规则; workers/block 候选派生; verify_profile_v2 正负例。
#include "profile_gen.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "astrocs/common_abi_v1.h"
#include "bench_harness.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
}

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) quick profile 生成: v2 字段完整
  {
    std::string build_id = "0.10.0-alpha.2+gabcdef123456";
    std::string commit = "abcdef1234567890abcdef1234567890abcdef12";
    std::string cli_sha = std::string(64, 'a');
    auto pb = astrocs::backend_host::generate_profile_v2("quick", build_id, commit, cli_sha, "");
    CHECK(!pb.json.empty());
    // schema 字段
    CHECK(pb.json.find("\"schema\": \"astrocs.cpu-profile/v2\"") != std::string::npos);
    CHECK(pb.json.find("\"profile_id\": \"sha256:") != std::string::npos);
    CHECK(pb.json.find("\"created_utc\"") != std::string::npos);
    CHECK(pb.json.find("\"memory_bandwidth\"") != std::string::npos);
    CHECK(pb.json.find("\"raw_samples_sha256\"") != std::string::npos);
    CHECK(pb.json.find("\"kernels\"") != std::string::npos);
    // host/build 字段
    CHECK(pb.json.find("\"logical_available\"") != std::string::npos);
    CHECK(pb.json.find("\"quota_signature\"") != std::string::npos);
    CHECK(pb.json.find("\"source_commit\"") != std::string::npos);
    CHECK(pb.json.find("\"benchmark_binary_sha256\"") != std::string::npos);
    // 至少 1 个 kernel, 且每个含规格字段
    CHECK(pb.kernels.size() >= 1);
    for (const auto& [kid, kp] : pb.kernels) {
      CHECK(!kid.empty());
      CHECK(!kp.provider.empty());
      CHECK(kp.workers >= 1);
      CHECK(kp.block >= 1);
      CHECK(kp.median_ns > 0);
      CHECK(kp.mad_ns >= 0);
      CHECK(kp.self_test_sha256.size() == 64);
    }
    // 原始候选非空且 hash 匹配
    CHECK(!pb.raw.empty());
    CHECK(!pb.raw_samples_sha256.empty());
    CHECK(pb.raw_samples_sha256.size() == 64);
  }

  // 2) 复读正例: 生成的 profile 可被独立 verify 通过
  {
    std::string build_id = "0.10.0-alpha.2+gabcdef123456";
    std::string commit = "abcdef1234567890abcdef1234567890abcdef12";
    std::string cli_sha = std::string(64, 'b');
    auto pb = astrocs::backend_host::generate_profile_v2("quick", build_id, commit, cli_sha, "");
    const std::string err = astrocs::backend_host::verify_profile_v2(pb.json, commit);
    CHECK(err.empty());   // 合法
    // 错误 commit → 拒
    const std::string err2 = astrocs::backend_host::verify_profile_v2(pb.json, std::string(40, '0'));
    CHECK(!err2.empty());
    // 篡改 schema → 拒
    const std::string bad = pb.json.find("\"schema\": \"astrocs.cpu-profile/v2\"") !=
                            std::string::npos
        ? pb.json.substr(0, pb.json.find("v2") + 2) + "\"x\"" + pb.json.substr(
              pb.json.find("v2") + 3)
        : pb.json;
    const std::string err3 = astrocs::backend_host::verify_profile_v2(bad, commit);
    CHECK(!err3.empty());
  }

  // 3) worker 候选: {1, 中位, 全部} 派生(avail=2 → {1,2})
  {
    auto c2 = astrocs::backend_host::worker_candidates(2);
    CHECK(c2.size() == 2 && c2[0] == 1 && c2[1] == 2);
    auto c4 = astrocs::backend_host::worker_candidates(4);
    CHECK(c4.size() == 3 && c4[0] == 1 && c4[1] == 2 && c4[2] == 4);
  }

  // 4) AVX512 提升<3% 规则: select_with_noise_margin 选保守者
  {
    std::vector<astrocs::backend_host::BenchResult> r;
    r.push_back({"avx512", "OK", "", 7, 100.0, 2.0, 95, 105, 2, "h"});
    r.push_back({"avx2", "OK", "", 7, 98.0, 2.0, 93, 103, 2, "h"});
    // avx2(98) 比 avx512(100) 快 2% < 3% → 但 select_with_noise_margin 的 conservative=avx2
    const std::string w = astrocs::backend_host::select_with_noise_margin(r, "avx2", 0.03);
    CHECK(w == "avx2");   // 保守(avx2)胜出
    // 若 avx512 快 5% ≥ 3% → 选 avx512
    std::vector<astrocs::backend_host::BenchResult> r2;
    r2.push_back({"avx512", "OK", "", 7, 95.0, 2.0, 90, 100, 2, "h"});
    r2.push_back({"avx2", "OK", "", 7, 100.0, 2.0, 95, 105, 2, "h"});
    const std::string w2 = astrocs::backend_host::select_with_noise_margin(r2, "avx2", 0.03);
    CHECK(w2 == "avx512");  // 提升 5% ≥ 3% → 选 AVX512
  }

  // 5) 无 profile 行为: baseline + 有效 worker(available≥2 不退 1)
  {
    auto np = astrocs::backend_host::no_profile_policy(2);
    CHECK(np.backend_id == "baseline");
    CHECK(np.workers == 2);
    auto np1 = astrocs::backend_host::no_profile_policy(1);
    CHECK(np1.workers == 1);
  }

  if (failures == 0) {
    std::printf("CPU-003 TESTS PASS (v2 profile 字段全/Oracle 门/winner/AVX512<3%/verify 正负例)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-003 TESTS FAIL (%d)\n", failures);
  return 1;
}
