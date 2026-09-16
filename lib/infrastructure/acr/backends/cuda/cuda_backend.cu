// lib/acr/backends/cuda/cuda_backend.cu — CUDA 设备管理 + kernel 执行实现
// Phase D：纯 CUDA backend（不依赖 alpaka）。
//
// 实现要点：
// - cudaError → StatusCode 映射（DeviceLost / OutOfMemory / KernelFailed）
// - CudaBackend singleton + std::call_once 幂等初始化
// - 无设备 / 驱动错误时降级：available=false，不抛异常，调用者回退 CPU
// - initialize 注册 GPU 报告回调到 topology（register_gpu_report_callback）
// - axpy 通过 cuda_parallel_for 模板启动（functor 转发 kernel）
// - GPU UUID 按 NVIDIA 工具规范格式化（GPU-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）
#ifdef ACR_BUILD_CUDA

#include "cuda_backend.hpp"

#include <astro/compute/topology.hpp>  // register_gpu_report_callback

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>

namespace astro::compute::cuda {

// ===== cudaError → StatusCode =====
StatusCode cuda_error_to_status(cudaError_t err) noexcept {
    if (err == cudaSuccess) return StatusCode::Ok;
    switch (err) {
        case cudaErrorMemoryAllocation:
        case cudaErrorLaunchOutOfResources:
            return StatusCode::OutOfMemory;
        case cudaErrorDevicesUnavailable:
        case cudaErrorInvalidDevice:
        case cudaErrorDeviceUninitialized:
        case cudaErrorInsufficientDriver:
        case cudaErrorNoDevice:
            return StatusCode::DeviceLost;
        default:
            return StatusCode::KernelFailed;
    }
}

// ===== AXPY functor + 启动器 =====
namespace {

struct AxpyFunctor {
    float* y;
    const float* x;
    float a;
    __device__ void operator()(std::size_t i) const {
        y[i] = a * x[i] + y[i];
    }
};

} // anonymous namespace

StatusCode axpy(float* y, const float* x, float a, std::size_t n,
                cudaStream_t stream) noexcept {
    if (n == 0) return StatusCode::Ok;
    if (y == nullptr || x == nullptr) return StatusCode::InvalidArgument;
    AxpyFunctor f{y, x, a};
    // 复用 cuda_parallel_for（functor 通过值传递到 device kernel）
    StatusCode s = cuda_parallel_for("axpy", n, f, stream);
    return s;
}

// ===== CudaBackend singleton =====
namespace {

std::once_flag g_init_flag;
std::atomic<bool> g_callback_registered{false};

// 格式化 GPU UUID（16 字节 → GPU-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）
std::string format_uuid(const cudaUUID_t& uuid) {
    const unsigned char* b = reinterpret_cast<const unsigned char*>(uuid.bytes);
    char buf[64] = {0};
    std::snprintf(buf, sizeof(buf),
                  "GPU-%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                  "%02x%02x%02x%02x%02x%02x",
                  b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                  b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return std::string(buf);
}

} // anonymous namespace

CudaBackend::CudaBackend() = default;

CudaBackend::~CudaBackend() {
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }
}

CudaBackend& CudaBackend::instance() {
    static CudaBackend inst;
    return inst;
}

StatusCode CudaBackend::initialize() {
    StatusCode result = StatusCode::Ok;
    std::call_once(g_init_flag, [this, &result]() {
        initialized_ = true;

        // 1. 枚举设备
        int dev_count = 0;
        cudaError_t err = cudaGetDeviceCount(&dev_count);
        if (err != cudaSuccess || dev_count <= 0) {
            // 无 CUDA 设备 / 驱动错误：降级，不崩溃
            has_device_ = false;
            device_count_ = 0;
            result = (err == cudaSuccess) ? StatusCode::DeviceLost
                                          : cuda_error_to_status(err);
            return;
        }
        device_count_ = dev_count;

        // 2. 选择 device 0
        err = cudaSetDevice(0);
        if (err != cudaSuccess) {
            has_device_ = false;
            result = cuda_error_to_status(err);
            return;
        }

        // 3. 查询设备属性
        cudaDeviceProp prop{};
        err = cudaGetDeviceProperties(&prop, 0);
        if (err != cudaSuccess) {
            has_device_ = false;
            result = cuda_error_to_status(err);
            return;
        }

        info_.device_id = 0;
        info_.name = prop.name;
        info_.uuid = format_uuid(prop.uuid);
        info_.compute_major = prop.major;
        info_.compute_minor = prop.minor;
        info_.total_memory = static_cast<std::size_t>(prop.totalGlobalMem);
        info_.sm_count = prop.multiProcessorCount;

        // 驱动版本（cudaDriverGetVersion 返回 1000*major + 10*minor）
        int driver_version = 0;
        cudaDriverGetVersion(&driver_version);
        info_.driver_major = driver_version / 1000;
        info_.driver_minor = (driver_version % 1000) / 10;

        // free memory 快照
        std::size_t free_mem = 0, total_mem = 0;
        cudaMemGetInfo(&free_mem, &total_mem);
        info_.free_memory = free_mem;

        // 4. 创建 stream
        err = cudaStreamCreate(&stream_);
        if (err != cudaSuccess) {
            has_device_ = false;
            result = cuda_error_to_status(err);
            return;
        }

        has_device_ = true;

        // 5. 注册 GPU 报告回调（首次生效，CAS 防止覆盖用户已注册的回调）
        bool expected = false;
        if (g_callback_registered.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            register_gpu_report_callback(&CudaBackend::gpu_report_json);
        }
    });
    return result;
}

StatusCode CudaBackend::sync() noexcept {
    if (!has_device_ || stream_ == nullptr) return StatusCode::Ok;
    cudaError_t err = cudaStreamSynchronize(stream_);
    return cuda_error_to_status(err);
}

std::string CudaBackend::gpu_report_json() {
    // 从 singleton 读取已填充的 info_（initialize 内注册后才会被调）
    auto& backend = CudaBackend::instance();
    if (!backend.available()) {
        return "null";
    }
    const CudaDeviceInfo& info = backend.device_info();
    std::ostringstream os;
    os << "{";
    os << "\"status\":\"ok\"";
    os << ",\"device_id\":" << info.device_id;
    os << ",\"name\":\"" << info.name << "\"";
    os << ",\"uuid\":\"" << info.uuid << "\"";
    os << ",\"compute_capability\":\"" << info.compute_major << "."
       << info.compute_minor << "\"";
    os << ",\"sm_count\":" << info.sm_count;
    os << ",\"total_memory\":" << info.total_memory;
    os << ",\"free_memory\":" << info.free_memory;
    os << ",\"driver_version\":\"" << info.driver_major << "."
       << info.driver_minor << "\"";
    os << "}";
    return os.str();
}

} // namespace astro::compute::cuda

#endif // ACR_BUILD_CUDA
