// CPU-002 单元测试: baseline AMD64 + Runtime lease 多线程
#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"

#include <cstdio>
#include <cstring>
#include <vector>

extern "C" int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
extern "C" void astrocs_host_services_destroy_state_v1(void* state);
extern "C" void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                                 astrocs_host_services_v1* out);
extern "C" uint32_t astrocs_baseline_last_workers_used(void);

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static acs_baseline_params_v1 make_cal_params(uint32_t w, uint32_t h,
                                              std::vector<float>* in0, std::vector<float>* in1,
                                              std::vector<float>* in2, std::vector<float>* in3,
                                              std::vector<float>* out) {
  acs_baseline_params_v1 p{};
  p.head.struct_size = sizeof(acs_baseline_params_v1);
  p.head.abi_version = ACS_ABI_VERSION_V1;
  p.op = ACS_KOP_CALIBRATION;
  p.w = w; p.h = h; p.k = 2.0f;
  in0->resize(static_cast<size_t>(w) * h);
  in1->resize(static_cast<size_t>(w) * h);
  in2->resize(static_cast<size_t>(w) * h);
  in3->resize(static_cast<size_t>(w) * h);
  out->resize(static_cast<size_t>(w) * h);
  for (size_t i = 0; i < in0->size(); ++i) {
    (*in0)[i] = static_cast<float>(i % 100);
    (*in1)[i] = static_cast<float>(i % 50);
    (*in2)[i] = 1.0f;
    (*in3)[i] = 2.0f;
  }
  p.in0 = ACS_SPAN_F32(in0->data(), in0->size());
  p.in1 = ACS_SPAN_F32(in1->data(), in1->size());
  p.in2 = ACS_SPAN_F32(in2->data(), in2->size());
  p.in3 = ACS_SPAN_F32(in3->data(), in3->size());
  p.out0 = ACS_SPAN_F32(out->data(), out->size());
  return p;
}

int main() {
  astrocs_host_services_v1 hs;
  void* state = nullptr;
  CHECK(astrocs_host_services_default_v1(&hs, &state) == 0);
  // budget: 2 核, max_workers=2
  astrocs_host_state_set_budget_v1(state, 2, 2, &hs);

  // 1) baseline backend 加载 (AMD64 基本能力; handshake 带 struct_size)
  astrocs_backend_api_v1 api;
  std::memset(&api, 0, sizeof(api));
  int rc = astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1,
                                      static_cast<uint32_t>(sizeof(astrocs_host_services_v1)),
                                      &hs, &api);
  CHECK(rc == 0);
  CHECK(api.struct_size == sizeof(astrocs_backend_api_v1));
  CHECK(api.abi_version == ACS_ABI_VERSION_V1);
  CHECK(std::strcmp(api.backend_id, "baseline") == 0);

  // 2) self_test 通过
  CHECK(api.self_test(&hs) == ACS_OK);

  // 3) heavy kernel (calibration) 经 Runtime lease 多线程执行
  const uint32_t W = 512, H = 512;
  std::vector<float> in0, in1, in2, in3, out;
  auto p = make_cal_params(W, H, &in0, &in1, &in2, &in3, &out);
  const astrocs_kernel_entry_v1* cal = nullptr;
  for (uint32_t i = 0; i < api.kernel_count; ++i) {
    if (std::strcmp(api.kernels[i].science_contract_id, "ALG-001") == 0) {
      cal = &api.kernels[i]; break;
    }
  }
  CHECK(cal != nullptr);
  CHECK(cal->struct_size == sizeof(astrocs_kernel_entry_v1));
  acs_status krc = cal->fn(&hs, &p, sizeof(p), nullptr, nullptr);
  CHECK(krc == ACS_OK);
  uint32_t workers = astrocs_baseline_last_workers_used();
  CHECK(workers >= 1);
  CHECK(workers <= 2);  // 不超过 budget

  // 4) 确定性: 结果与串行参考一致
  for (size_t i = 0; i < out.size(); ++i) {
    float expect = (in0[i] - in1[i] - 2.0f * in2[i]) * in3[i];
    if (out[i] != expect) { ++failures; std::fprintf(stderr, "mismatch at %zu\n", i); break; }
  }

  // 5) 预算不足: 1 核时 workers 降级为 1 但结果仍正确
  astrocs_host_state_set_budget_v1(state, 1, 1, &hs);
  std::vector<float> out2;
  auto p2 = make_cal_params(W, H, &in0, &in1, &in2, &in3, &out2);
  krc = cal->fn(&hs, &p2, sizeof(p2), nullptr, nullptr);
  CHECK(krc == ACS_OK);
  CHECK(astrocs_baseline_last_workers_used() == 1);
  for (size_t i = 0; i < out2.size(); ++i) {
    if (out2[i] != (in0[i] - in1[i] - 2.0f * in2[i]) * in3[i]) {
      ++failures; std::fprintf(stderr, "budget-1 mismatch at %zu\n", i); break;
    }
  }

  // 6) ABI 边界: 异常不外泄
  CHECK(astrocs_abi_boundary_probe(1) == ACS_ERR_INTERNAL);
  CHECK(astrocs_abi_boundary_probe(0) == ACS_OK);

  astrocs_host_services_destroy_state_v1(state);
  if (failures == 0) {
    std::printf("CPU-002 TESTS PASS (baseline AMD64, lease workers=%u, 确定性, budget 降级)\n",
                workers);
    return 0;
  }
  std::fprintf(stderr, "CPU-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
