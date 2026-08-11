// ============================================================================
// aio_upm.h - Unified Photometric Model (UPM) 模型文件容器 C API
//
// Phase2 控制包 AstroCS_Phase2_Implementation_Control_Package_V1
// （05_UPM_IMPLEMENTATION §5）唯一 AIO 新增：
//   aio_upm_write_sparse / aio_upm_open / aio_upm_read_info /
//   aio_upm_dense_begin / aio_upm_dense_write_controls /
//   aio_upm_dense_write_frames / aio_upm_dense_end /
//   aio_upm_dense_info / aio_upm_read_dense_block / aio_upm_close
//
// 语义（冻结）：
//   - 稀疏模型为权威形态：JSON 文本（format=astrocs-upm-v1），
//     model_hash 由 phase2 计算（内容哈希）并随 JSON 保存；
//   - dense cache 是同一模型的求值缓存：固定 512B 头部 JSON 行
//     （source_hash/target_order/precision/control_count/frame_count/
//     checksum）+ 二进制 controls(double) 块 + frames(u64+double) 块；
//   - 任何读取入口先校验 source_hash 与调用方模型 hash 一致，
//     不一致返回 2（stale cache）——"stale hash 必须拒绝加载"；
//   - checksum = SHA-256（文件除头部 checksum 槽置 '0' 外的字节），
//     读取时校验完整性；
//   - 本模块只做容器/文件层，不解释模型科学语义。
// ============================================================================

#ifndef AIO_UPM_H
#define AIO_UPM_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AIO_UPM_EXPORT __declspec(dllexport)
#else
#define AIO_UPM_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AioUpmSparse AioUpmSparse;
typedef struct AioUpmDense AioUpmDense;

// ===== 稀疏模型容器 =====
// 原子写稀疏 JSON（UTF-8）。返回 0=成功。
AIO_UPM_EXPORT int aio_upm_write_sparse(const char* path, const char* model_json);

// 打开稀疏模型（读入并校验 format）。失败返回 NULL（aio_upm_last_error）。
AIO_UPM_EXPORT AioUpmSparse* aio_upm_open(const char* path);

// 读取模型信息。任何 out_* 可空。
AIO_UPM_EXPORT int aio_upm_read_info(
    AioUpmSparse* f, uint32_t* version, uint32_t* precision,
    uint32_t* target_order, uint64_t* control_count,
    uint64_t* observation_count, char* model_hash, int hash_buf_size);

// 读取完整 JSON 内容到调用方缓冲（含 '\0'）；buf_size 不足返回 -1。
AIO_UPM_EXPORT int aio_upm_read_all(AioUpmSparse* f, char* buf, int buf_size);

AIO_UPM_EXPORT void aio_upm_close(AioUpmSparse* f);

// ===== 稠密缓存容器 =====
// 开始写稠密缓存：写固定 512B 头部 JSON 行（checksum 槽占位）。
// 失败返回 NULL。
AIO_UPM_EXPORT AioUpmDense* aio_upm_dense_begin(
    const char* path, const char* source_hash, int target_order,
    uint32_t precision, uint64_t control_count, uint64_t frame_count);

// 写 controls 块（count 个 double z 值）。
AIO_UPM_EXPORT int aio_upm_dense_write_controls(
    AioUpmDense* d, const double* z, uint64_t count);

// 写 frames 块（count 个 (frame_id u64, offset double) 对）。
AIO_UPM_EXPORT int aio_upm_dense_write_frames(
    AioUpmDense* d, const uint64_t* frame_ids, const double* offsets,
    uint64_t count);

// 结束：回填 checksum、释放句柄。返回 0=成功。
AIO_UPM_EXPORT int aio_upm_dense_end(AioUpmDense* d);

// 中止：关闭并删除部分文件、释放句柄。
AIO_UPM_EXPORT void aio_upm_dense_abort(AioUpmDense* d);

// 稠密缓存信息：target_order/pixels/checksum 校验（source_hash 必须匹配）。
// 返回 0=ok, 1=io/parse, 2=stale。
AIO_UPM_EXPORT int aio_upm_dense_info(
    const char* path, const char* source_hash,
    int* out_target_order, uint64_t* out_pixels, char* out_checksum,
    int checksum_buf_size);

// 读取稠密缓存一块：output[i] = input[i] - frame_offset(frame_id)。
// 返回 0=ok, 1=io/parse, 2=stale/checksum-mismatch。
AIO_UPM_EXPORT int aio_upm_read_dense_block(
    const char* path, const char* source_hash, uint64_t frame_id,
    const double* input_signal, double* output_signal, uint64_t count);

AIO_UPM_EXPORT const char* aio_upm_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // AIO_UPM_H
