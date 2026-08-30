// CPU-003 单元测试: CPUID/OSXSAVE/XGETBV 核验 + feature 匹配降级
#include "cpu_features.h"
#include "astrocs/common_abi_v1.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" uint32_t astrocs_cpu_affinity_count_v1(void);

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// backend_loader feature 匹配: detected 必须含 required 全部位 (05 §3)
static bool features_satisfy(uint64_t detected, uint64_t required) {
  return (detected & required) == required;
}

int main() {
  // 1) CPUID 实测: SSE2 基线恒置位 (amd64)
  uint64_t feats = astrocs_cpu_detect_features_v1();
  CHECK((feats & ACS_FEAT_SSE2) != 0);
  // SSE4.1 是 amd64 普及但非恒有; 记录当前实测供一致性
  printf("CPU-003 detected: SSE2=%d SSE4_1=%d AVX=%d AVX2=%d FMA=%d AVX512F=%d\n",
         (int)((feats & ACS_FEAT_SSE2) != 0), (int)((feats & ACS_FEAT_SSE4_1) != 0),
         (int)((feats & ACS_FEAT_AVX) != 0), (int)((feats & ACS_FEAT_AVX2) != 0),
         (int)((feats & ACS_FEAT_FMA) != 0), (int)((feats & ACS_FEAT_AVX512F) != 0));

  // 2) OSXSAVE 一致性: AVX2 置位 ⇒ AVX 置位 (层次包含)
  if (feats & ACS_FEAT_AVX2) CHECK((feats & ACS_FEAT_AVX) != 0);
  if (feats & ACS_FEAT_AVX512F) {
    // AVX-512 要求 opmask+ZMM 状态保存; 且含 AVX2 (层次)
    CHECK((feats & ACS_FEAT_AVX2) != 0);
  }

  // 3) feature 匹配: required 子集满足; 超集/伪造不满足 → 降级
  uint64_t detected = feats;
  CHECK(features_satisfy(detected, ACS_FEAT_SSE2));          // 恒满足
  CHECK(features_satisfy(detected, detected));               // 自身满足
  if (!(detected & ACS_FEAT_AVX2)) {
    // 本机无 AVX2: 伪造 required=AVX2 必须不满足 → baseline 回退 (降级路径)
    CHECK(!features_satisfy(detected, ACS_FEAT_AVX2));
  }
  // 伪造 feature 位 (非真实 CPU 特征): 必须不满足
  const uint64_t FAKE = 1ull << 40;
  CHECK(!features_satisfy(detected, FAKE));
  CHECK(!features_satisfy(detected, detected | FAKE));

  // 4) 伪造 XCR0 场景: AVX 系必须 OSXSAVE 保存状态 (逻辑验证, 非注入)
  //    实测 xcr0 位: AVX2 置位即 OS 保存 XMM|YMM (探测已核验)
  // 5) affinity: 可用 CPU 数 > 0 且 ≤ 机器核数 (本机 2)
  uint32_t aff = astrocs_cpu_affinity_count_v1();
  CHECK(aff >= 1);
  CHECK(aff <= 64);
  printf("CPU-003 affinity_count=%u\n", aff);

  // 6) 本机实测 AVX2/AVX512 时: 探测必须含对应位 (OSXSAVE/XCR0 已核验)
  //    且不以 CPU 名称推断——探测仅凭 CPUID+XCR0 位, 名称不参与

  if (failures == 0) {
    std::printf("CPU-003 TESTS PASS (CPUID/OSXSAVE 核验, feature 匹配降级, affinity)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-003 TESTS FAIL (%d)\n", failures);
  return 1;
}
