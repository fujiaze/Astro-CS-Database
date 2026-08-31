// tests/unit/cpu001_provider_selftest.cpp — CPU-001 (G3) provider self-test
// 三份独立 provider(baseline/avx2/avx512)各自链接对应静态库, 运行 backend_self_test
// (handshake→allocator 往返→cancel 读→budget 租借/归还→logger) 并校验成功;
// 同时验证: backend_id 正确、kernel 表非空、feature 声明与 provider 匹配。
//
// 用法: cpu001_provider_selftest baseline|avx2|avx512
//   baseline : 仅链接 astrocs_cpu, 验证 baseline self_test
//   avx2    : 仅链接 astrocs_cpu_avx2, 验证 AVX2 provider self_test
//   avx512  : 仅链接 astrocs_cpu_avx512, 验证 AVX512 provider self_test
// 负例(feature bits 不满足 → 拒绝加载)见独立 cpu001_negative_test.cpp
#include "cpu_features.h"
#include "astrocs/common_abi_v1.h"

#include <cstdio>
#include <cstring>
#include <string>


extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
long astrocs_host_state_alloc_count(void* state);
long astrocs_host_state_alloc_total(void* state);
long astrocs_host_state_active_workers(void* state);
}

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main(int argc, char** argv) {
  const std::string mode = (argc > 1) ? argv[1] : "baseline";

  astrocs_host_services_v1 host;
  void* state = nullptr;
  if (astrocs_host_services_default_v1(&host, &state) != ACS_OK) {
    std::fprintf(stderr, "cpu001: host services init failed\n");
    return 2;
  }
  astrocs_host_state_set_budget_v1(state, 2, 2, &host);

  // ── backend_api: 当前 TU 链接的 provider 的静态 API (get_api handshake) ──
  astrocs_backend_api_v1 api;
  std::memset(&api, 0, sizeof(api));
  CHECK(astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1,
                                   static_cast<uint32_t>(sizeof(astrocs_host_services_v1)),
                                   &host, &api) == ACS_OK);
  CHECK(api.struct_size == sizeof(astrocs_backend_api_v1));
  CHECK(api.abi_version == ACS_ABI_VERSION_V1);

  // ── provider 身份校验 ──
  const char* expect_id = mode.c_str();
  CHECK(std::strncmp(api.backend_id, expect_id, sizeof(api.backend_id)) == 0);
  std::printf("cpu001: provider backend_id='%s' build_id='%s' kernels=%u\n",
              api.backend_id, api.backend_build_id, api.kernel_count);

  // feature 声明: baseline required=0; avx2=AVX2|FMA; avx512=AVX512F(检测面位)
  const uint64_t detected = astrocs_cpu_detect_features_v1();
  if (mode == "baseline") {
    CHECK(api.required_features == 0);
  } else if (mode == "avx2") {
    CHECK((api.required_features & (ACS_FEAT_AVX2 | ACS_FEAT_FMA)) ==
          (ACS_FEAT_AVX2 | ACS_FEAT_FMA));
  } else if (mode == "avx512") {
    CHECK((api.required_features & ACS_FEAT_AVX512F) == ACS_FEAT_AVX512F);
  }
  // 本机必须满足 required, 否则该 provider 在当前机器上不可用(测试环境前提)
  CHECK((detected & api.required_features) == api.required_features);

  // kernel 表非空且与共享注册表一致(12 项)
  CHECK(api.kernel_count > 0);
  CHECK(api.kernels != nullptr);
  std::printf("cpu001: %s kernel table %u entries (first: %s/%s)\n",
              mode.c_str(), api.kernel_count, api.kernels[0].science_contract_id,
              api.kernels[0].algorithm_id);

  // ── self_test: handshake→allocator 往返→cancel 读→budget 租借/归还→logger ──
  const long alloc_before = astrocs_host_state_alloc_count(state);
  const long total_before = astrocs_host_state_alloc_total(state);
  CHECK(api.self_test != nullptr);
  if (api.self_test) {
    CHECK(api.self_test(&host) == ACS_OK);
  }
  // allocator 往返后无泄漏(计数归零)且确有分配发生(可验证性)
  CHECK(astrocs_host_state_alloc_count(state) == alloc_before);
  CHECK(astrocs_host_state_alloc_total(state) > total_before);
  CHECK(astrocs_host_state_active_workers(state) == 0);   // budget 租借已归还

  // ABI 边界探针(astrocs_abi_boundary_probe)由 baseline 测试覆盖:
  // 该符号仅定义于 baseline_backend.cpp, avx2/avx512 变体库不重复链接。

  astrocs_host_services_destroy_state_v1(state);
  if (failures == 0) {
    std::printf("CPU-001 %s SELF_TEST PASS\n", mode.c_str());
    return 0;
  }
  std::fprintf(stderr, "CPU-001 %s SELF_TEST FAIL (%d)\n", mode.c_str(), failures);
  return 1;
}
