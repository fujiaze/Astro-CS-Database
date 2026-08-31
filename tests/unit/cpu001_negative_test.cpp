// tests/unit/cpu001_negative_test.cpp — CPU-001 (G3) provider 加载负例
// 只含 preflight 负例逻辑(不调用 provider self_test): feature bits 不满足的
// provider 必须 FALLBACK_BASELINE(拒绝加载), 永不 REJECT_SECURITY 误伤 baseline。
// 与 cpu001_provider_selftest.cpp 分离, 避免 avx2/avx512 变体 target 重复链接。
#include "backend_loader.h"
#include "cpu_features.h"
#include "astrocs/common_abi_v1.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using astrocs::backend_host::ManifestEntry;
using astrocs::backend_host::preflight_entry;
using astrocs::backend_host::LoadResult;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  const uint64_t detected = astrocs_cpu_detect_features_v1();
  CHECK((detected & ACS_FEAT_SSE2) != 0);   // amd64 基线恒置位

  const char* d = std::getenv("TMPDIR");
  const std::string dir = d ? d : "/tmp";
  const std::string fpath = dir + "/cpu001_negative.so";
  {   // 写占位文件(仅 preflight, 不 dlopen)
    std::FILE* f = std::fopen(fpath.c_str(), "wb");
    CHECK(f != nullptr);
    if (f) {
      const char junk[] = "cpu001 negative fixture: not a real backend binary";
      std::fwrite(junk, 1, sizeof(junk) - 1, f);
      std::fclose(f);
    }
  }
  const std::string real_sha = astrocs::backend_host::file_sha256_hex(fpath);
  CHECK(real_sha.size() == 64);

  // ① required 含检测不到的子集位(如 AVX512BW bit6) → 恒不满足 → 拒绝
  {
    ManifestEntry e;
    e.file = "cpu001_negative.so";
    e.backend_id = "fake-avx512";
    e.sha256 = real_sha;
    e.abi_version = ACS_ABI_VERSION_V1;
    e.required_features = ACS_FEAT_SSE2 | ACS_FEAT_AVX512F | (1ull << 6);
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::FALLBACK_BASELINE);
    CHECK(reason.find("unsupported ISA") != std::string::npos);
    std::printf("cpu001: negative (required bits ⊄ detected) → FALLBACK: %s\n",
                reason.c_str());
  }
  // ② required 含不存在的伪造位(1<<41) → 永不满足 → 拒绝
  {
    ManifestEntry e;
    e.file = "cpu001_negative.so";
    e.backend_id = "fake";
    e.sha256 = real_sha;
    e.abi_version = ACS_ABI_VERSION_V1;
    e.required_features = 1ull << 41;
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision == LoadResult::FALLBACK_BASELINE);
    CHECK(reason.find("unsupported ISA") != std::string::npos);
  }
  // ③ baseline(required=0)在 preflight 层面恒满足(文件存在性除外) — 不 REJECT
  {
    ManifestEntry e;
    e.file = "baseline.so";
    e.backend_id = "baseline";
    e.sha256 = std::string(64, '0');
    e.abi_version = ACS_ABI_VERSION_V1;
    e.required_features = 0;
    std::string reason;
    LoadResult r = preflight_entry(dir, e, detected, &reason);
    CHECK(r.decision != LoadResult::REJECT_SECURITY);   // 永不安全拒绝(文件缺失→FALLBACK)
  }
  std::remove(fpath.c_str());

  if (failures == 0) {
    std::printf("CPU-001 NEGATIVE TESTS PASS (unsupported provider 加载被拒)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-001 NEGATIVE TESTS FAIL (%d)\n", failures);
  return 1;
}
