// lib/acr/backends/cuda/cuda_bridge_loader.cpp — CUDA 桥接 DLL 加载器
//
// 23 §3：GPU 不可用时不创建 executor（运行时探测，不得仅凭编译宏）。
// 流程：
// 1. LoadLibrary acr_cuda_bridge.dll（MSVC+nvcc 构建，C ABI）；
// 2. 填充 bridge::api() 函数指针；
// 3. 探测真实设备；有设备时注册 CudaBridgeExecutor（每个设备一个）；
// 4. 无 DLL / 无设备 → 不注册，CPU executor 继续使用。
#include "scheduler/device_executor.hpp"

#include "astro/compute/kernel_registry.hpp"
#include "bridge/cuda_bridge_api.hpp"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace astro::compute::cuda::bridge {

namespace {

BridgeApi g_api;
thread_local void* tls_handle = nullptr;
thread_local std::uint64_t tls_elapsed_ns = 0;
std::once_flag g_load_flag;

// 候选 DLL 路径（按顺序尝试）
std::vector<std::wstring> candidate_paths() {
    std::vector<std::wstring> paths;
    // 1. 环境变量 ACR_CUDA_BRIDGE_DLL（UTF-8）
    const char* env = std::getenv("ACR_CUDA_BRIDGE_DLL");
    if (env && *env) {
        std::string s(env);
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len > 0) {
            std::wstring w(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
            paths.push_back(w);
        }
    }
    // 2. 可执行文件同目录
    {
        wchar_t buf[1024];
        DWORD n = GetModuleFileNameW(nullptr, buf, 1024);
        if (n > 0) {
            std::wstring exe(buf, n);
            std::size_t pos = exe.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                paths.push_back(exe.substr(0, pos + 1) + L"acr_cuda_bridge.dll");
            }
        }
    }
    // 3. 常用工作目录
    paths.push_back(L"run\\temp\\cuda_bridge\\acr_cuda_bridge.dll");
    paths.push_back(L"lib\\acr\\build\\bridge\\acr_cuda_bridge.dll");
    return paths;
}

HMODULE try_load() {
    for (const auto& p : candidate_paths()) {
        HMODULE m = LoadLibraryW(p.c_str());
        if (m) return m;
    }
    // 最后依赖 PATH/系统搜索
    return LoadLibraryA("acr_cuda_bridge.dll");
}

template <class F>
bool load_symbol(HMODULE mod, const char* name, F& out) {
    FARPROC p = GetProcAddress(mod, name);
    if (!p) return false;
    out = reinterpret_cast<F>(p);
    return true;
}

} // anonymous namespace

BridgeApi& api() noexcept { return g_api; }
void set_tls_handle(void* handle) noexcept { tls_handle = handle; }
void* get_tls_handle() noexcept { return tls_handle; }
void set_tls_elapsed(std::uint64_t ns) noexcept { tls_elapsed_ns = ns; }
std::uint64_t get_tls_elapsed() noexcept { return tls_elapsed_ns; }

// loader 内部使用：加载一次并填充 api()
void ensure_bridge_loaded() {
    std::call_once(g_load_flag, [] {
        HMODULE mod = try_load();
        if (!mod) return;
        bool ok = true;
        ok &= load_symbol(mod, "acr_cuda_bridge_init", g_api.init);
        ok &= load_symbol(mod, "acr_cuda_bridge_device_count", g_api.device_count);
        ok &= load_symbol(mod, "acr_cuda_bridge_device_name", g_api.device_name);
        ok &= load_symbol(mod, "acr_cuda_device_memory", g_api.device_memory);
        ok &= load_symbol(mod, "acr_cuda_device_compute", g_api.device_compute);
        ok &= load_symbol(mod, "acr_cuda_executor_create", g_api.executor_create);
        ok &= load_symbol(mod, "acr_cuda_executor_destroy", g_api.executor_destroy);
        ok &= load_symbol(mod, "acr_cuda_executor_available", g_api.executor_available);
        ok &= load_symbol(mod, "acr_cuda_executor_sync", g_api.executor_sync);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_axpy", g_api.submit_axpy);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_copy", g_api.submit_copy);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_reduce", g_api.submit_reduce);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_conv3x3", g_api.submit_conv3x3);
        // 聚焦版（08 §3）：目标合成 Operation（旧 DLL 缺失时整体视为不可用）
        ok &= load_symbol(mod, "acr_cuda_executor_submit_dense_accumulate_fp64acc",
                          g_api.submit_dense_accumulate_fp64acc);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_drizzle_scatter",
                          g_api.submit_drizzle_scatter);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_chain", g_api.submit_chain);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_launch_event",
                          g_api.submit_launch_event);
        ok &= load_symbol(mod, "acr_cuda_executor_transfer_h2d", g_api.transfer_h2d);
        ok &= load_symbol(mod, "acr_cuda_executor_transfer_d2h", g_api.transfer_d2h);
        ok &= load_symbol(mod, "acr_cuda_executor_upload_persistent",
                          g_api.upload_persistent);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_dense_accumulate_resident",
                          g_api.submit_dense_accumulate_resident);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_reduce_resident",
                          g_api.submit_reduce_resident);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_drizzle_scatter_resident",
                          g_api.submit_drizzle_scatter_resident);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_chain_resident",
                          g_api.submit_chain_resident);
        ok &= load_symbol(mod, "acr_cuda_executor_upload_persistent_slot",
                          g_api.upload_persistent_slot);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_weighted_integration",
                          g_api.submit_weighted_integration);
        ok &= load_symbol(mod, "acr_cuda_executor_submit_mosaic_reject",
                          g_api.submit_mosaic_reject);
        ok &= load_symbol(
            mod, "acr_cuda_executor_submit_weighted_integration_resident",
            g_api.submit_weighted_integration_resident);
        ok &= load_symbol(mod, "acr_cuda_executor_configure_streams",
                          g_api.configure_streams);
        ok &= load_symbol(mod, "acr_cuda_executor_stream_count",
                          g_api.stream_count);
        ok &= load_symbol(mod, "acr_cuda_executor_upload_count",
                          g_api.upload_count);
        if (!ok) {
            g_api = BridgeApi{};  // 符号缺失：视为不可用
        }
    });
}

} // namespace astro::compute::cuda::bridge

namespace astro::compute::scheduler {

namespace {

// ===== CudaBridgeExecutor：真实 GPU 执行器（通过桥接 DLL）=====
class CudaBridgeExecutor : public DeviceExecutor {
public:
    CudaBridgeExecutor(void* handle, int device, std::string name)
        : handle_(handle), device_(device), name_(std::move(name)) {}

    ~CudaBridgeExecutor() override {
        if (handle_) cuda::bridge::api().executor_destroy(handle_);
    }

    DeviceId id() const override { return static_cast<DeviceId>(device_ + 1); }
    std::string device_id() const override { return "cuda:" + std::to_string(device_); }
    std::string backend_type() const override { return "cuda"; }
    bool available() const override {
        return handle_ != nullptr &&
               cuda::bridge::api().executor_available(handle_) == 1;
    }
    bool supports(OperationId op) const override {
        return global_kernel_registry().supports(op, "cuda");
    }
    QueueState queue_state() const override {
        // 24 §4.3：真实队列状态（提交中/排队）
        QueueState qs;
        qs.depth = pending_count_.load(std::memory_order_relaxed);
        qs.load = (qs.depth > 0) ? 1.0 : 0.0;
        qs.busy = (qs.depth > 0);
        return qs;
    }
    std::size_t recommended_chunk() const override { return 65536; }
    std::size_t min_effective_chunk() const override { return 256; }
    std::string name() const override { return name_; }
    // 聚焦版 v3（08 §3）：真实驻留执行。
    // prefetch_input 经桥接上传整帧到本 executor 的 device buffer 并记录
    // device view；input_resident 供 Dispatcher 判断是否已驻留。
    bool prefetch_input(const void* host, std::size_t bytes) override {
        if (host == nullptr || bytes == 0) return false;
        auto& api = cuda::bridge::api();
        if (!api.upload_persistent) return false;
        std::uint64_t el = 0;
        const char* err = nullptr;
        if (api.upload_persistent(handle_, 0, bytes / sizeof(float),
                                  static_cast<const float*>(host),
                                  &el, &err) != 0) {
            return false;
        }
        views_[host] = bytes;
        slot_host_[0] = host;
        return true;
    }
    // ACR 架构冻结（07 C）：hosts 是本次执行需要的完整输入集合
    // （加权积分 = {frames, weights}）。已驻留（同 host 指针）复用不重传；
    // 新输入分配槽位：优先空槽，否则覆盖"不属于本次集合"的旧槽位
    // （resident-reuse 场景：frames 保持 slot0，新 weights 覆盖 slot1）。
    bool prefetch_inputs(const std::vector<const void*>& hosts,
                         const std::vector<std::size_t>& bytes) override {
        auto& api = cuda::bridge::api();
        if (!api.upload_persistent_slot) {
            return DeviceExecutor::prefetch_inputs(hosts, bytes);
        }
        // 本次集合中仍被占用的槽位（不能被覆盖）
        bool in_set[2] = {false, false};
        for (const void* h : hosts) {
            for (int s = 0; s < 2; ++s) {
                if (slot_host_[s] != nullptr && slot_host_[s] == h) {
                    in_set[s] = true;
                }
            }
        }
        bool all_ok = true;
        for (std::size_t i = 0; i < hosts.size(); ++i) {
            if (hosts[i] == nullptr || bytes[i] == 0) { all_ok = false; break; }
            if (views_.find(hosts[i]) != views_.end()) continue;  // 已驻留复用
            int slot = -1;
            for (int s = 0; s < 2; ++s) {       // 优先空槽
                if (slot_host_[s] == nullptr) { slot = s; break; }
            }
            if (slot < 0) {                     // 覆盖本次集合外的旧槽位
                for (int s = 0; s < 2; ++s) {
                    if (!in_set[s]) { slot = s; break; }
                }
            }
            if (slot < 0) { all_ok = false; break; }
            std::uint64_t el = 0;
            const char* err = nullptr;
            if (api.upload_persistent_slot(
                    handle_, slot, 0, bytes[i] / sizeof(float),
                    static_cast<const float*>(hosts[i]), &el, &err) != 0) {
                all_ok = false;
                break;
            }
            // 槽位被覆盖时，清除旧 host 的驻留记录（避免 reuse 循环回旧输入
            // 时误判"已驻留"而使用被覆盖的 device buffer）
            if (slot_host_[slot] != nullptr &&
                slot_host_[slot] != hosts[i]) {
                views_.erase(slot_host_[slot]);
            }
            views_[hosts[i]] = bytes[i];
            slot_host_[slot] = hosts[i];
        }
        return all_ok;
    }
    // ACR 基座收尾（02_GENERATION_COHERENCE.md）：同 host 指针原地修改 +
    // generation++ 时，从驻留映射删除 host；若 persistent slot 正绑定该
    // host 则清除槽位绑定。下一次 prefetch 强制真实上传（不再按指针复用）。
    void invalidate_input(const void* host) override {
        views_.erase(host);
        for (int s = 0; s < 2; ++s) {
            if (slot_host_[s] == host) {
                slot_host_[s] = nullptr;
            }
        }
    }

    bool input_resident(const void* host) const override {
        return views_.find(host) != views_.end();
    }
    // ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §5）：内部 stream 槽位数
    std::size_t max_in_flight() const override {
        auto& api = cuda::bridge::api();
        return (handle_ && api.stream_count)
            ? static_cast<std::size_t>(api.stream_count(handle_)) : 1;
    }
    bool set_streams(std::size_t count) override {
        auto& api = cuda::bridge::api();
        if (!handle_ || !api.configure_streams) return false;
        const char* err = nullptr;
        return api.configure_streams(handle_, static_cast<int>(count),
                                     &err) == 0;
    }
    // persistent 槽位真实上传次数（来自桥接 handle，与 Dispatcher 复用路径一致）
    std::uint64_t slot_upload_count(int slot) const override {
        auto& api = cuda::bridge::api();
        if (!handle_ || !api.upload_count || (slot != 0 && slot != 1)) {
            return 0;
        }
        return static_cast<std::uint64_t>(api.upload_count(handle_, slot));
    }

    SubmitHandle submit(const WorkToken& token,
                        const KernelInvocation& invocation) override {
        SubmitHandle h;
        h.device = id();
        h.op_id = std::string(invocation.id);
        h.attempt = token.attempt;
        const KernelRegistration* reg = global_kernel_registry().find(invocation.id);
        if (reg == nullptr || !reg->cuda.has_value()) {
            h.status = SubmitStatus::Rejected;
            h.error = "no cuda launcher registered: " + std::string(invocation.id);
            return h;
        }
        // 24 §5.2：提交前统一契约校验
        const std::string contract_err =
            validate_invocation(*reg, invocation, "cuda");
        if (!contract_err.empty()) {
            h.status = SubmitStatus::Rejected;
            h.error = "invocation contract violation: " + contract_err;
            return h;
        }
        if (!available()) {
            h.status = SubmitStatus::Rejected;
            h.error = "cuda executor not available";
            return h;
        }
        pending_count_.fetch_add(1, std::memory_order_relaxed);
        cuda::bridge::set_tls_handle(handle_);
        try {
            (*reg->cuda)(invocation, nullptr);
            h.status = SubmitStatus::Ok;
            h.items_done = token.size();
            h.bytes_done =
                token.size() * (invocation.traits.bytes_read_per_item +
                                invocation.traits.bytes_written_per_item);
            h.elapsed_ns = cuda::bridge::get_tls_elapsed();
        } catch (const std::exception& e) {
            h.status = SubmitStatus::Failed;
            h.error = std::string("cuda kernel failed: ") + e.what();
        } catch (...) {
            h.status = SubmitStatus::Failed;
            h.error = "cuda kernel unknown failure";
        }
        pending_count_.fetch_sub(1, std::memory_order_relaxed);
        return h;
    }

    void sync() override {
        if (handle_) cuda::bridge::api().executor_sync(handle_, nullptr);
    }

private:
    void* handle_{nullptr};
    int device_{0};
    std::string name_;
    // host 输入指针 → 已驻留字节（真实 device view 缓存）
    std::unordered_map<const void*, std::size_t> views_;
    // persistent 槽位 → 已驻留 host 指针（slot 0 = d_x，slot 1 = d_w）
    std::array<const void*, 2> slot_host_{nullptr, nullptr};
    std::atomic<std::size_t> pending_count_{0};
};

} // anonymous namespace

// ===== 强定义：追加 CUDA 桥接执行器（覆盖 device_executor.cpp 的 weak no-op）=====
void try_append_cuda_bridge_executors(ExecutorRegistry& registry) {
    cuda::bridge::ensure_bridge_loaded();
    auto& api = cuda::bridge::api();
    if (!api.loaded()) return;

    const char* err = nullptr;
    const int count = api.init(&err);
    if (count <= 0) return;

    for (int dev = 0; dev < count; ++dev) {
        void* h = api.executor_create(dev, 65536, 256, &err);
        if (!h) continue;
        const char* nm = api.device_name(dev);
        registry.register_executor(std::make_unique<CudaBridgeExecutor>(
            h, dev, nm ? std::string(nm) : std::string("cuda:" + std::to_string(dev))));
    }
}

} // namespace astro::compute::scheduler
