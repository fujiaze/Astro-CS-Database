// CPU-001 单元测试: C ABI 边界 (struct_size/version + allocator 合同 + span head)
#include "astrocs/common_abi_v1.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 引用 host_services 默认构造 (链接 astrocs_cpu)
extern "C" int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);

static void test_host_services_handshake() {
  astrocs_host_services_v1 hs;
  void* state = nullptr;
  int rc = astrocs_host_services_default_v1(&hs, &state);
  CHECK(rc == 0);
  CHECK(hs.struct_size == sizeof(astrocs_host_services_v1));
  CHECK(hs.abi_version == ACS_ABI_VERSION_V1);
  CHECK(hs.allocator.struct_size == sizeof(acs_allocator));
  CHECK(hs.allocator.abi_version == ACS_ABI_VERSION_V1);
  CHECK(hs.logger.struct_size == sizeof(acs_logger));
  CHECK(hs.cancel.struct_size == sizeof(acs_cancel));
  CHECK(hs.budget.struct_size == sizeof(acs_thread_budget));
}

static void test_allocator_contract() {
  astrocs_host_services_v1 hs;
  void* state = nullptr;
  CHECK(astrocs_host_services_default_v1(&hs, &state) == 0);
  // 对齐合同: align 2 幂; 16 对齐分配
  void* p = hs.allocator.alloc(hs.allocator.user_data, 100, 16);
  CHECK(p != nullptr);
  CHECK((reinterpret_cast<uintptr_t>(p) & 15u) == 0);
  hs.allocator.free(hs.allocator.user_data, p);
  // 非法 align (非 2 幂) 拒绝
  void* bad = hs.allocator.alloc(hs.allocator.user_data, 100, 3);
  CHECK(bad == nullptr);
  // free(nullptr) 安全
  hs.allocator.free(hs.allocator.user_data, nullptr);
}

static void test_span_head() {
  float f[4] = {1, 2, 3, 4};
  acs_span_f32 s = ACS_SPAN_F32(f, 4);
  CHECK(s.head.struct_size == sizeof(acs_span_f32));
  CHECK(s.head.abi_version == ACS_ABI_VERSION_V1);
  CHECK(s.count == 4);
  CHECK(s.data == f);
  acs_span_u8 u = ACS_SPAN_U8(nullptr, 0);
  CHECK(u.head.struct_size == sizeof(acs_span_u8));
  CHECK(u.count == 0);
}

static void test_kernel_entry_head() {
  // kernel_entry 带 struct_size/abi_version (CPU-001)
  CHECK(offsetof(astrocs_kernel_entry_v1, struct_size) == 0);
  CHECK(offsetof(astrocs_kernel_entry_v1, abi_version) == 4);
}

static void test_enum_stability() {
  // 退出码/状态数值稳定 (v1 冻结)
  CHECK(ACS_OK == 0);
  CHECK(ACS_ERR_PARAM == 1);
  CHECK(ACS_ERR_ABI_MISMATCH == 2);
  CHECK(ACS_ERR_CANCELLED == 6);
  CHECK(ACS_ERR_INTERNAL == 70);
}

int main() {
  test_host_services_handshake();
  test_allocator_contract();
  test_span_head();
  test_kernel_entry_head();
  test_enum_stability();
  if (failures == 0) {
    std::printf("CPU-001 TESTS PASS (ABI handshake/allocator/span head/kernel head)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-001 TESTS FAIL (%d)\n", failures);
  return 1;
}
