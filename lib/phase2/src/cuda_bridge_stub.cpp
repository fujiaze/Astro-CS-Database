// lib/phase2/src/cuda_bridge_stub.cpp — Linux stub: CUDA bridge always unavailable
#include "cuda_bridge_api.hpp"
namespace astro::compute::cuda::bridge {
BridgeApi& api() noexcept { static BridgeApi inst{}; return inst; }
void set_tls_handle(void*) noexcept {}
void* get_tls_handle() noexcept { return nullptr; }
void set_tls_elapsed(uint64_t) noexcept {}
uint64_t get_tls_elapsed() noexcept { return 0; }
void ensure_bridge_loaded() {}
}
namespace astro::compute::scheduler {
struct ExecutorRegistry;
void try_append_cuda_bridge_executors(ExecutorRegistry&) {}
}
