// CPU-004 单元测试: AVX2/AVX-512 provider 同 kernel ID/semantics + manifest 匹配
#include "cpu_features.h"
#include "astrocs/common_abi_v1.h"

#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

extern "C" uint64_t astrocs_cpu_detect_features_v1(void);

// 复刻 backend_loader 的 feature 匹配 (05 §3): required ⊆ detected
static bool features_satisfy(uint64_t detected, uint64_t required) {
  return (detected & required) == required;
}

int main() {
  // 1) AVX2 provider 注册基线: baseline required=0 (最低 amd64)
  //    模拟 manifest 条目匹配链 (与 backend_loader preflight_entry 同语义)
  struct Entry { std::string id; uint64_t required; };
  const Entry entries[] = {
    {"baseline", 0},                 // 恒可加载
    {"avx2", ACS_FEAT_AVX2 | ACS_FEAT_FMA},
    {"avx512", ACS_FEAT_AVX512F | ACS_FEAT_AVX2},
  };
  uint64_t detected = astrocs_cpu_detect_features_v1();
  // baseline 必须恒满足
  CHECK(features_satisfy(detected, entries[0].required));
  // 本机 AVX-512 全置位 → avx2 与 avx512 条目均满足 (降级链验证)
  if (detected & ACS_FEAT_AVX512F) {
    CHECK(features_satisfy(detected, entries[1].required));
    CHECK(features_satisfy(detected, entries[2].required));
    printf("CPU-004: avx512 provider 满足 (AVX-512 主机)\n");
  } else if (detected & ACS_FEAT_AVX2) {
    CHECK(features_satisfy(detected, entries[1].required));
    CHECK(!features_satisfy(detected, entries[2].required));  // 无 AVX-512 → 回退 avx2
    printf("CPU-004: avx2 provider 满足, avx512 不满足 → 降级\n");
  } else {
    // 纯 SSE: avx2/avx512 都不满足 → 仅 baseline
    CHECK(!features_satisfy(detected, entries[1].required));
    CHECK(!features_satisfy(detected, entries[2].required));
    printf("CPU-004: 仅 baseline 可用 (SSE2)\n");
  }

  // 2) 伪造 feature: 不存在的位必须拒绝 (损坏 provider/ABI mismatch 降级)
  const uint64_t FAKE = 1ull << 41;
  CHECK(!features_satisfy(detected, FAKE));
  CHECK(!features_satisfy(detected, detected | FAKE));

  // 3) 静态校验: avx2/avx512 backend 与 baseline 共享 kernel 表源
  //    (同 kernel ID/semantics 零复制漂移 — 三 TU include 同一 impl/table)
  {
    std::string base = std::string(std::getenv("ASTROCS_REPO") ? std::getenv("ASTROCS_REPO") : "..");
    auto check_shared = [&](const std::string& f) {
      std::string p = base + "/lib/backend_host/" + f;
      std::FILE* fp = std::fopen(p.c_str(), "r");
      CHECK(fp != nullptr);
      if (fp) {
        char buf[4096];
        size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
        buf[n] = 0;
        std::fclose(fp);
        std::string s(buf);
        CHECK(s.find("baseline_kernels_impl.inc") != std::string::npos);
        CHECK(s.find("backend_table.inc") != std::string::npos);
      }
    };
    check_shared("avx2_backend.cpp");
    check_shared("avx512_backend.cpp");
    check_shared("avx_backend.cpp");
  }

  if (failures == 0) {
    std::printf("CPU-004 TESTS PASS (provider 同 kernel ID/semantics, 降级链, manifest 匹配)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
