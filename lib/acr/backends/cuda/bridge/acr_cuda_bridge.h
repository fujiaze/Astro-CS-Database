// lib/acr/backends/cuda/bridge/acr_cuda_bridge.h — CUDA 桥接 C ABI
//
// 背景（23 号计划 §3）：ACR 主构建使用 MSYS2 MinGW 工具链，nvcc 11.8 不支持
// MinGW host；本桥接 DLL 用 MSVC + nvcc 构建，通过纯 C ABI 暴露真实 GPU kernel，
// 由 MinGW 侧加载器（cuda_bridge_loader.cpp）LoadLibrary 动态调用——
// 与项目现有 DLL 模块架构一致，无 ABI 冲突。
//
// 构建（证据命令记录于 docs/cuda_bridge_build.md）：
//   vcvars64 → nvcc -arch=sm_86 --allow-unsupported-compiler \
//     -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -shared \
//     acr_cuda_bridge.cu -o acr_cuda_bridge.dll
//
// 错误约定：所有函数返回 0 表示成功；非 0 时 *last_error 指向静态错误串。
#ifndef ACR_CUDA_BRIDGE_H
#define ACR_CUDA_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(ACR_CUDA_BRIDGE_BUILD)
#  define ACR_CUDA_BRIDGE_API __declspec(dllexport)
#elif defined(_WIN32)
#  define ACR_CUDA_BRIDGE_API __declspec(dllimport)
#else
#  define ACR_CUDA_BRIDGE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ===== 设备探测 =====
// 初始化 CUDA 并枚举设备；返回设备数（无设备/驱动错误返回 0）。
ACR_CUDA_BRIDGE_API int acr_cuda_bridge_init(const char** last_error);
ACR_CUDA_BRIDGE_API int acr_cuda_bridge_device_count(void);
ACR_CUDA_BRIDGE_API const char* acr_cuda_bridge_device_name(int device);

// 25 号计划 §3.4：真实设备元数据（显存、SM/CU、计算能力）
ACR_CUDA_BRIDGE_API int acr_cuda_device_memory(
    int device, uint64_t* total_bytes, uint64_t* free_bytes,
    const char** last_error);
ACR_CUDA_BRIDGE_API int acr_cuda_device_compute(
    int device, int* sm_count, int* cc_major, int* cc_minor,
    const char** last_error);

// ===== Executor 句柄 =====
// recommended_chunk/min_chunk 用于预分配设备缓冲（AXPY/COPY/CONV 按块工作）。
ACR_CUDA_BRIDGE_API void* acr_cuda_executor_create(
    int device, size_t recommended_chunk,
    size_t min_chunk, const char** last_error);
ACR_CUDA_BRIDGE_API void acr_cuda_executor_destroy(void* handle);
ACR_CUDA_BRIDGE_API int acr_cuda_executor_available(void* handle);
ACR_CUDA_BRIDGE_API int acr_cuda_executor_sync(void* handle, const char** last_error);

// ===== 真实 GPU kernel 提交 =====
// 全部为同步语义：H2D → launch → D2H（y 在 host 上），elapsed_ns 为真实耗时。

// y[i] = alpha * x[i] + y[i]，i in [begin, end)
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_axpy(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x, float alpha,
    uint64_t* elapsed_ns, const char** last_error);

// y[i] = x[i]，i in [begin, end)
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_copy(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x,
    uint64_t* elapsed_ns, const char** last_error);

// 归约（FP64 累加）：partials[chunk_index * blocks + blockIdx] = sum(x[begin..end) 的 block 和)
// blocks_per_chunk 固定（如 256），merge 阶段累加全部 partials。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_reduce(
    void* handle,
    size_t begin, size_t end,
    const float* x,
    double* partials,
    size_t blocks_per_chunk,
    uint64_t chunk_index,
    uint64_t* elapsed_ns, const char** last_error);

// 3x3 直接卷积（y 为 w*h 连续行主序，kernel9 为 3x3 权重）
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_conv3x3(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x,
    size_t width, size_t height,
    const float* kernel9,
    uint64_t* elapsed_ns, const char** last_error);

// ===== 聚焦版（08 号计划 §3）：目标合成 Operation 内核 =====
// 积分/Drizzle 类逐像素算法的 GPU 实现；全部同步语义，elapsed_ns 为真实耗时。

// Dense pixel accumulate（FP32 输入 + FP64 累加器）：
//   y[i] = (double)y[i] + x[i]（累加在 double 中进行，结果写回 float y）
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_dense_accumulate_fp64acc(
    void* handle,
    size_t begin, size_t end,
    float* y, const float* x,
    uint64_t* elapsed_ns, const char** last_error);

// Drizzle-like scatter/accumulate（FP64 累加）：
//   partials[bin(x[i])] += x[i]（bin 由确定性 hash 计算，原子加）
//   bins 为输出桶数（如 256/1024），host 侧提供 partials（double*）
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_drizzle_scatter(
    void* handle,
    size_t begin, size_t end,
    const float* x, double* partials, size_t bins,
    uint64_t* elapsed_ns, const char** last_error);

// Resident chain：连续两个 GPU 算子，只上传一次、最后下载一次。
//   y[i] = x[i] + 1（显存写）→ z[i] = y[i] * 2（显存读）→ z 返回 host
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_chain(
    void* handle,
    size_t begin, size_t end,
    float* z, const float* x,
    uint64_t* elapsed_ns, const char** last_error);

// Launch/event/sync 固定开销：空 kernel 启动 + event record + sync
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_launch_event(
    void* handle,
    size_t begin, size_t end,
    uint64_t* elapsed_ns, const char** last_error);

// 纯传输：H2D 上传 host_bytes 字节到设备暂存（不计算）
ACR_CUDA_BRIDGE_API int acr_cuda_executor_transfer_h2d(
    void* handle,
    size_t host_bytes, const void* host,
    uint64_t* elapsed_ns, const char** last_error);

// 纯传输：D2H 下载 device_bytes 字节到 host
ACR_CUDA_BRIDGE_API int acr_cuda_executor_transfer_d2h(
    void* handle,
    size_t device_bytes, void* host,
    uint64_t* elapsed_ns, const char** last_error);

// ===== 聚焦版 v2（08 号计划 §2/§4）：resident 持久上传与提交 =====
// 数据先上传到设备并保留（persistent d_x），后续 resident 提交跳过 H2D，
// 只 launch（必要时 D2H 输出）——用于真实 resident 曲线测量与驻留复用。

// 上传并保留到 d_x（persistent buffer）；再次调用同一范围视为已驻留复用
ACR_CUDA_BRIDGE_API int acr_cuda_executor_upload_persistent(
    void* handle,
    size_t begin, size_t end,
    const float* x,
    uint64_t* elapsed_ns, const char** last_error);

// 上传并保留到指定 persistent 槽位（slot 0 = d_x；slot 1 = d_w）。
// 加权积分需要 frames（d_x）与 weights（d_w）两个输入分别驻留；
// 其他 Operation 使用 slot 0 保持向后兼容。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_upload_persistent_slot(
    void* handle,
    int slot,
    size_t begin, size_t end,
    const float* x,
    uint64_t* elapsed_ns, const char** last_error);

// resident dense accumulate：d_x 已驻留，y 输出 D2H
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_dense_accumulate_resident(
    void* handle,
    size_t begin, size_t end,
    float* y,
    uint64_t* elapsed_ns, const char** last_error);

// resident pixel reduce：d_x 已驻留，partials D2H
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_reduce_resident(
    void* handle,
    size_t begin, size_t end,
    double* partials, size_t blocks_per_chunk, uint64_t chunk_index,
    uint64_t* elapsed_ns, const char** last_error);

// resident drizzle scatter：d_x 已驻留，partials D2H
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_drizzle_scatter_resident(
    void* handle,
    size_t begin, size_t end,
    double* partials, size_t bins,
    uint64_t* elapsed_ns, const char** last_error);

// resident chain：d_x 已驻留，两个 kernel 全程显存，只下载最终 z
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_chain_resident(
    void* handle,
    size_t begin, size_t end,
    float* z,
    uint64_t* elapsed_ns, const char** last_error);

// ===== ACR 架构冻结（07 号计划 C）：加权积分 =====
// synthetic.weighted_integration.fp64acc：FP32 输入/权重、FP64 累加、FP32 输出。
// frame-major 连续输入：frames[f * pixel_count + p]，权重 weights[f]。
// 每个输出像素 p：output[p] = Σ_f weight[f]*frame[f,p] / Σ_f weight[f]。
// 同步语义：H2D(整帧) → launch → D2H(输出范围)；elapsed_ns 为真实耗时。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_weighted_integration(
    void* handle,
    size_t begin, size_t end,
    float* output,
    const float* frames, const float* weights,
    size_t frame_count, size_t pixel_count,
    uint64_t* elapsed_ns, const char** last_error);

// 加权积分 resident：frames/weights 已驻留（upload_persistent_slot 0/1），
// 只 launch（输出范围 D2H 物化）。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_weighted_integration_resident(
    void* handle,
    size_t begin, size_t end,
    float* output,
    size_t frame_count, size_t pixel_count,
    uint64_t* elapsed_ns, const char** last_error);

// ===== Phase2 mosaic_reject（synthetic.mosaic_reject.fp64acc）=====
// 逐像素栈：有效样本（finite && support>0）→ 迭代 median/MAD sigma-clip
// （low/high sigma，max_iterations）→ 接受样本 SNR²×support 加权均值。
// 样本不足（< min_samples）fallback=全接受。同步语义 H2D → launch → D2H。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_submit_mosaic_reject(
    void* handle,
    size_t begin, size_t end,
    float* output,
    const float* frames, const float* support, const float* frame_snr,
    float* out_support,
    float* out_reject_count, float* out_valid_count,
    size_t frame_count, size_t pixel_count,
    size_t begin_offset,
    float sigma_low, float sigma_high,
    int max_iterations, int min_samples,
    uint64_t* elapsed_ns, const char** last_error);

// ===== ACR 架构冻结（01_ARCHITECTURE_FREEZE.md §5）：GPU 内部通道 =====
// 每 GPU 只有一个 executor；stream 只是 executor 内部通道（1..3），
// 共享同一 GPU 队列、显存预算与成本模型。禁止把多个 stream 报告为多张 GPU。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_configure_streams(
    void* handle, int stream_count, const char** last_error);
ACR_CUDA_BRIDGE_API int acr_cuda_executor_stream_count(void* handle);

// persistent 槽位（0 = frames/d_x，1 = weights/d_w）的真实上传次数
// （resident-reuse 验收：同一帧栈多次调用时 frames 上传必须保持 1）。
ACR_CUDA_BRIDGE_API int acr_cuda_executor_upload_count(
    void* handle, int slot);

#ifdef __cplusplus
}
#endif

#endif // ACR_CUDA_BRIDGE_H
