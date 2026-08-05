// lib/acr/utilization/system_metrics.cpp — SystemMetrics 实现
//
// Phase G：实际系统指标读取。
//   - CPU: GetSystemTimes（idle/kernel/user 100ns 单位）
//   - RAM: GlobalMemoryStatusEx
//   - GPU/VRAM: NVML 动态加载（nvml.dll），不可用时队列预算估算
//
// NVML 通过 LoadLibrary + GetProcAddress 动态加载，编译期不依赖 nvml.h/nvml.lib。
// 这样 CPU-only 构建（ACR_BUILD_CUDA=OFF）也能在 NVIDIA GPU 机器上读取 GPU 利用率。
#include "system_metrics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <unordered_map>

// Windows API（MinGW 提供）
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace astro::compute::utilization {

namespace {

// ===== NVML 类型与函数指针（手动声明，避免依赖 nvml.h）=====
typedef int nvmlReturn_t;
#define NVML_SUCCESS 0
#define NVML_ERROR_UNINITIALIZED 2
#define NVML_ERROR_NO_PERMISSION 5
#define NVML_ERROR_NOT_FOUND 7
#define NVML_ERROR_NOT_SUPPORTED 8
#define NVML_ERROR_GPU_IS_LOST 13
#define NVML_ERROR_UNKNOWN 999

typedef struct {
    unsigned int gpu;     // 百分比 0-100
    unsigned int memory;  // 百分比 0-100
} nvmlUtilization_t;

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

typedef void* nvmlDevice_t;

typedef nvmlReturn_t (*PFN_nvmlInit_v2)(void);
typedef nvmlReturn_t (*PFN_nvmlShutdown)(void);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetCount_v2)(unsigned int* count);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetHandleByIndex_v2)(unsigned int index, nvmlDevice_t* device);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetUtilizationRates)(nvmlDevice_t device, nvmlUtilization_t* utilization);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMemoryInfo)(nvmlDevice_t device, nvmlMemory_t* memory);
typedef const char* (*PFN_nvmlErrorString)(nvmlReturn_t result);

inline std::uint64_t now_ns() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

// FILETIME → 100ns 单位的 uint64
inline std::uint64_t filetime_to_100ns(const FILETIME& ft) {
    ULARGE_INTEGER ul;
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;
    return ul.QuadPart;
}

} // anonymous namespace

struct SystemMetrics::Impl {
    mutable std::mutex mtx;

    // ---- CPU 基线（GetSystemTimes）----
    bool cpu_has_baseline{false};
    std::uint64_t prev_idle{0};
    std::uint64_t prev_kernel{0};
    std::uint64_t prev_user{0};

    // ---- NVML 动态加载 ----
    HMODULE nvml_dll{nullptr};
    std::atomic<bool> nvml_loaded{false};
    std::atomic<bool> nvml_init_attempted{false};
    PFN_nvmlInit_v2                 fn_init{nullptr};
    PFN_nvmlShutdown                fn_shutdown{nullptr};
    PFN_nvmlDeviceGetCount_v2       fn_get_count{nullptr};
    PFN_nvmlDeviceGetHandleByIndex_v2 fn_get_handle{nullptr};
    PFN_nvmlDeviceGetUtilizationRates fn_get_util{nullptr};
    PFN_nvmlDeviceGetMemoryInfo     fn_get_mem{nullptr};
    PFN_nvmlErrorString             fn_error_string{nullptr};

    // NVML 设备句柄（按 index）
    std::vector<nvmlDevice_t> nvml_devices;

    // ---- 已注册 backends（NVML 不可用时用于队列预算估算）----
    std::vector<std::string> backends;
    std::unordered_map<std::string, std::uint32_t> backend_queue_depth;
    std::uint32_t queue_budget_max_depth{8};  // 达到此队列深度视为 100%

    // ---- 构造/析构 ----
    Impl() {
        // 不在构造函数初始化 NVML，延迟到首次使用（lazy）
    }

    ~Impl() {
        if (nvml_loaded.load(std::memory_order_acquire) && fn_shutdown) {
            fn_shutdown();
        }
        if (nvml_dll) {
            FreeLibrary(nvml_dll);
            nvml_dll = nullptr;
        }
    }

    // 尝试加载 NVML（lazy，首次 read_gpu_* 或 reload_nvml 时调用）
    bool try_load_nvml_locked() {
        if (nvml_init_attempted.load(std::memory_order_acquire)) {
            return nvml_loaded.load(std::memory_order_acquire);
        }
        nvml_init_attempted.store(true, std::memory_order_release);

        // 常见路径：System32（NVIDIA 驱动安装时放置）
        const char* candidates[] = {
            "nvml.dll",
            "C:\\Windows\\System32\\nvml.dll",
            nullptr
        };
        for (int i = 0; candidates[i] != nullptr; ++i) {
            nvml_dll = LoadLibraryA(candidates[i]);
            if (nvml_dll) break;
        }
        if (!nvml_dll) {
            std::fprintf(stderr,
                "[acr.utilization] NVML not available (nvml.dll not found). "
                "GPU utilization will be estimated from queue budget.\n");
            return false;
        }

        fn_init          = reinterpret_cast<PFN_nvmlInit_v2>(GetProcAddress(nvml_dll, "nvmlInit_v2"));
        fn_shutdown      = reinterpret_cast<PFN_nvmlShutdown>(GetProcAddress(nvml_dll, "nvmlShutdown"));
        fn_get_count     = reinterpret_cast<PFN_nvmlDeviceGetCount_v2>(GetProcAddress(nvml_dll, "nvmlDeviceGetCount_v2"));
        fn_get_handle    = reinterpret_cast<PFN_nvmlDeviceGetHandleByIndex_v2>(GetProcAddress(nvml_dll, "nvmlDeviceGetHandleByIndex_v2"));
        fn_get_util      = reinterpret_cast<PFN_nvmlDeviceGetUtilizationRates>(GetProcAddress(nvml_dll, "nvmlDeviceGetUtilizationRates"));
        fn_get_mem       = reinterpret_cast<PFN_nvmlDeviceGetMemoryInfo>(GetProcAddress(nvml_dll, "nvmlDeviceGetMemoryInfo"));
        fn_error_string  = reinterpret_cast<PFN_nvmlErrorString>(GetProcAddress(nvml_dll, "nvmlErrorString"));

        if (!fn_init || !fn_get_count || !fn_get_handle || !fn_get_util || !fn_get_mem) {
            std::fprintf(stderr,
                "[acr.utilization] NVML dll loaded but required symbols missing. "
                "Falling back to queue budget estimation.\n");
            FreeLibrary(nvml_dll);
            nvml_dll = nullptr;
            return false;
        }

        nvmlReturn_t r = fn_init();
        if (r != NVML_SUCCESS) {
            const char* err = fn_error_string ? fn_error_string(r) : "nvmlInit_v2 failed";
            std::fprintf(stderr,
                "[acr.utilization] nvmlInit_v2 failed: %s. "
                "Falling back to queue budget estimation.\n", err ? err : "unknown");
            FreeLibrary(nvml_dll);
            nvml_dll = nullptr;
            return false;
        }

        unsigned int count = 0;
        r = fn_get_count(&count);
        if (r != NVML_SUCCESS) {
            const char* err = fn_error_string ? fn_error_string(r) : "nvmlDeviceGetCount_v2 failed";
            std::fprintf(stderr,
                "[acr.utilization] nvmlDeviceGetCount_v2 failed: %s.\n", err ? err : "unknown");
            // 已 init，保留 dll 以便后续重试
            nvml_devices.clear();
            nvml_loaded.store(true, std::memory_order_release);
            return true;  // NVML 已加载，但无设备
        }

        nvml_devices.clear();
        nvml_devices.resize(count);
        bool any_ok = false;
        for (unsigned int i = 0; i < count; ++i) {
            nvmlReturn_t ri = fn_get_handle(i, &nvml_devices[i]);
            if (ri != NVML_SUCCESS) {
                nvml_devices[i] = nullptr;
            } else {
                any_ok = true;
            }
        }
        if (!any_ok) {
            std::fprintf(stderr,
                "[acr.utilization] NVML loaded but no device handles obtained.\n");
        }
        nvml_loaded.store(true, std::memory_order_release);
        std::printf("[acr.utilization] NVML loaded: %u GPU device(s).\n", count);
        return true;
    }

    // backend name → NVML device index（约定 "cuda:N" → N）
    int backend_to_index(const std::string& backend) const {
        // "cuda:0" → 0, "cuda:1" → 1
        auto pos = backend.find(':');
        if (pos == std::string::npos) return 0;
        try {
            return std::stoi(backend.substr(pos + 1));
        } catch (...) {
            return 0;
        }
    }

    // 队列预算估算：queue_depth / max_depth，clamp [0,1]
    // 调用方必须持有 mtx
    double estimate_from_queue_locked(const std::string& backend) const {
        auto it = backend_queue_depth.find(backend);
        if (it == backend_queue_depth.end()) return 0.0;
        if (queue_budget_max_depth == 0) return 0.0;
        double r = static_cast<double>(it->second) / static_cast<double>(queue_budget_max_depth);
        if (r < 0.0) r = 0.0;
        if (r > 1.0) r = 1.0;
        return r;
    }
};

// ===== 构造/析构/移动 =====
SystemMetrics::SystemMetrics() : impl_(std::make_unique<Impl>()) {}
SystemMetrics::~SystemMetrics() = default;
SystemMetrics::SystemMetrics(SystemMetrics&&) noexcept = default;
SystemMetrics& SystemMetrics::operator=(SystemMetrics&&) noexcept = default;

// ===== CPU 利用率 =====
CpuUtilizationSample SystemMetrics::read_cpu_utilization() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    CpuUtilizationSample out;
    out.timestamp_ns = now_ns();

    FILETIME idle_ft{}, kernel_ft{}, user_ft{};
    if (!GetSystemTimes(&idle_ft, &kernel_ft, &user_ft)) {
        // 失败，返回 valid=false
        std::fprintf(stderr, "[acr.utilization] GetSystemTimes failed: %lu\n",
                     static_cast<unsigned long>(GetLastError()));
        return out;
    }

    std::uint64_t cur_idle = filetime_to_100ns(idle_ft);
    std::uint64_t cur_kernel = filetime_to_100ns(kernel_ft);
    std::uint64_t cur_user = filetime_to_100ns(user_ft);

    if (!impl_->cpu_has_baseline) {
        // 首次调用，建立基线，返回 valid=false
        impl_->prev_idle = cur_idle;
        impl_->prev_kernel = cur_kernel;
        impl_->prev_user = cur_user;
        impl_->cpu_has_baseline = true;
        return out;
    }

    std::uint64_t delta_idle = cur_idle - impl_->prev_idle;
    std::uint64_t delta_kernel = cur_kernel - impl_->prev_kernel;
    std::uint64_t delta_user = cur_user - impl_->prev_user;
    impl_->prev_idle = cur_idle;
    impl_->prev_kernel = cur_kernel;
    impl_->prev_user = cur_user;

    // total = idle + kernel + user（kernel 含 idle，需修正）
    // Windows GetSystemTimes: kernel 时间含 idle 时间。
    // 标准公式: busy = (delta_kernel - delta_idle) + delta_user
    //           total = delta_kernel + delta_user  (因为 kernel 含 idle)
    //           utilization = busy / total
    std::uint64_t delta_busy = 0;
    if (delta_kernel >= delta_idle) {
        delta_busy = (delta_kernel - delta_idle) + delta_user;
    } else {
        delta_busy = delta_user;  // 异常情况，仅 user
    }
    std::uint64_t delta_total = delta_kernel + delta_user;
    if (delta_total == 0) {
        // 极短窗口内系统计数器未变化（tick 粒度）：不是合法 0% 样本，
        // 标记无效，调用方不得记录为实际利用率。
        out.valid = false;
        out.ratio = 0.0;
        return out;
    }
    double ratio = static_cast<double>(delta_busy) / static_cast<double>(delta_total);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    out.ratio = ratio;
    out.valid = true;
    return out;
}

// ===== RAM =====
MemorySample SystemMetrics::read_ram() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    MemorySample out;
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) {
        std::fprintf(stderr, "[acr.utilization] GlobalMemoryStatusEx failed: %lu\n",
                     static_cast<unsigned long>(GetLastError()));
        return out;
    }
    out.total_bytes = ms.ullTotalPhys;
    out.avail_bytes = ms.ullAvailPhys;
    out.valid = true;
    return out;
}

// ===== GPU 利用率 =====
std::vector<GpuUtilizationSample> SystemMetrics::read_gpu_utilizations() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::vector<GpuUtilizationSample> out;
    const std::uint64_t ts = now_ns();

    // 确保 NVML 已尝试加载
    if (!impl_->nvml_init_attempted.load(std::memory_order_acquire)) {
        impl_->try_load_nvml_locked();
    }

    const bool nvml_ok = impl_->nvml_loaded.load(std::memory_order_acquire) && !impl_->nvml_devices.empty();

    if (nvml_ok) {
        // NVML 实读
        for (std::size_t i = 0; i < impl_->nvml_devices.size(); ++i) {
            GpuUtilizationSample s;
            s.backend = "cuda:" + std::to_string(i);
            s.timestamp_ns = ts;
            if (impl_->nvml_devices[i] == nullptr) {
                s.valid = false;
                s.estimated = true;
                out.push_back(s);
                continue;
            }
            nvmlUtilization_t util{};
            nvmlReturn_t r = impl_->fn_get_util(impl_->nvml_devices[i], &util);
            if (r == NVML_SUCCESS) {
                s.ratio = static_cast<double>(util.gpu) / 100.0;
                if (s.ratio < 0.0) s.ratio = 0.0;
                if (s.ratio > 1.0) s.ratio = 1.0;
                s.estimated = false;
                s.valid = true;
            } else {
                // NOT_SUPPORTED on some devices（如无 GPU 进程时）—— 退化为估算
                s.ratio = impl_->estimate_from_queue_locked(s.backend);
                s.estimated = true;
                s.valid = true;
            }
            out.push_back(s);
        }
        // 若有已注册 backend 但 NVML 设备数 < backend 数，补齐估算
        for (const auto& b : impl_->backends) {
            bool found = false;
            for (const auto& s : out) {
                if (s.backend == b) { found = true; break; }
            }
            if (!found) {
                GpuUtilizationSample s;
                s.backend = b;
                s.timestamp_ns = ts;
                s.ratio = impl_->estimate_from_queue_locked(b);
                s.estimated = true;
                s.valid = true;
                out.push_back(s);
            }
        }
        return out;
    }

    // NVML 不可用：队列预算估算
    for (const auto& b : impl_->backends) {
        GpuUtilizationSample s;
        s.backend = b;
        s.timestamp_ns = ts;
        s.ratio = impl_->estimate_from_queue_locked(b);
        s.estimated = true;
        s.valid = true;
        out.push_back(s);
    }
    return out;
}

// ===== VRAM =====
std::vector<GpuMemorySample> SystemMetrics::read_gpu_memories() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::vector<GpuMemorySample> out;

    if (!impl_->nvml_init_attempted.load(std::memory_order_acquire)) {
        impl_->try_load_nvml_locked();
    }

    const bool nvml_ok = impl_->nvml_loaded.load(std::memory_order_acquire) && !impl_->nvml_devices.empty();

    if (nvml_ok) {
        for (std::size_t i = 0; i < impl_->nvml_devices.size(); ++i) {
            GpuMemorySample m;
            m.backend = "cuda:" + std::to_string(i);
            if (impl_->nvml_devices[i] == nullptr) {
                m.valid = false;
                m.estimated = true;
                out.push_back(m);
                continue;
            }
            nvmlMemory_t mem{};
            nvmlReturn_t r = impl_->fn_get_mem(impl_->nvml_devices[i], &mem);
            if (r == NVML_SUCCESS) {
                m.total_bytes = mem.total;
                m.used_bytes = mem.used;
                m.free_bytes = mem.free;
                m.estimated = false;
                m.valid = true;
            } else {
                m.estimated = true;
                m.valid = false;
            }
            out.push_back(m);
        }
        return out;
    }

    // NVML 不可用：标记 estimated
    for (const auto& b : impl_->backends) {
        GpuMemorySample m;
        m.backend = b;
        m.estimated = true;
        m.valid = false;
        out.push_back(m);
    }
    return out;
}

// ===== NVML 状态 =====
bool SystemMetrics::nvml_available() const noexcept {
    return impl_->nvml_loaded.load(std::memory_order_acquire);
}

std::size_t SystemMetrics::gpu_count() const noexcept {
    if (impl_->nvml_loaded.load(std::memory_order_acquire)) {
        return impl_->nvml_devices.size();
    }
    // 无 NVML 时返回已注册 backend 数
    return impl_->backends.size();
}

// ===== Backend 注册与队列预算 =====
void SystemMetrics::register_backend(const std::string& backend) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (std::find(impl_->backends.begin(), impl_->backends.end(), backend)
        == impl_->backends.end()) {
        impl_->backends.push_back(backend);
        impl_->backend_queue_depth[backend] = 0;
    }
}

void SystemMetrics::report_queue_depth(const std::string& backend, std::uint32_t depth) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->backend_queue_depth[backend] = depth;
    // 同时确保 backend 已注册
    if (std::find(impl_->backends.begin(), impl_->backends.end(), backend)
        == impl_->backends.end()) {
        impl_->backends.push_back(backend);
    }
}

void SystemMetrics::set_queue_budget_max_depth(std::uint32_t max_depth) noexcept {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (max_depth > 0) {
        impl_->queue_budget_max_depth = max_depth;
    }
}

std::uint32_t SystemMetrics::queue_budget_max_depth() const noexcept {
    return impl_->queue_budget_max_depth;
}

bool SystemMetrics::reload_nvml() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    // 先卸载
    if (impl_->nvml_loaded.load(std::memory_order_acquire) && impl_->fn_shutdown) {
        impl_->fn_shutdown();
    }
    if (impl_->nvml_dll) {
        FreeLibrary(impl_->nvml_dll);
        impl_->nvml_dll = nullptr;
    }
    impl_->fn_init = nullptr;
    impl_->fn_shutdown = nullptr;
    impl_->fn_get_count = nullptr;
    impl_->fn_get_handle = nullptr;
    impl_->fn_get_util = nullptr;
    impl_->fn_get_mem = nullptr;
    impl_->fn_error_string = nullptr;
    impl_->nvml_devices.clear();
    impl_->nvml_loaded.store(false, std::memory_order_release);
    impl_->nvml_init_attempted.store(false, std::memory_order_release);
    return impl_->try_load_nvml_locked();
}

// ===== 状态 JSON =====
std::string SystemMetrics::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"nvml_available\":" << (impl_->nvml_loaded.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"nvml_init_attempted\":" << (impl_->nvml_init_attempted.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"gpu_count\":" << impl_->nvml_devices.size();
    os << ",\"registered_backends\":" << impl_->backends.size();
    os << ",\"queue_budget_max_depth\":" << impl_->queue_budget_max_depth;
    os << ",\"backends\":[";
    for (std::size_t i = 0; i < impl_->backends.size(); ++i) {
        if (i > 0) os << ",";
        const auto& b = impl_->backends[i];
        os << "{\"backend\":\"" << b << "\"";
        auto it = impl_->backend_queue_depth.find(b);
        if (it != impl_->backend_queue_depth.end()) {
            os << ",\"queue_depth\":" << it->second;
        }
        os << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace astro::compute::utilization
