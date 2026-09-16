#ifndef AIO_PIPELINE_H
#define AIO_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AIO_EXPORT __declspec(dllexport)
#else
#define AIO_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * 命名块管线模型
 * ---------------------------------------------------------------------------
 * PipelineFrame 是一个纯命名块容器，所有数据（header/data/psf/snr/...）
 * 都是按块名索引的块。模块按块名读取数据，计算结果写为新块或注入已有块。
 * 编排器按阶段丢弃不需要的块释放内存。任意阶段可 save_cache 保存所有块
 * 到 .aio 缓存文件，或 export_block_* 导出单个块用于调试。
 * =========================================================================== */

typedef enum {
    STAGE_CALIBRATE    = 0,
    STAGE_PLATESOLVE   = 1,
    STAGE_PHOTOMETRIC  = 2,
    STAGE_DRIZZLE      = 3,
    STAGE_STACK        = 4,
} PipelineStage;

/* 块数据类型 */
typedef enum {
    AIO_BLOCK_FLOAT32 = 0,   /* float  数组 */
    AIO_BLOCK_FLOAT64 = 1,   /* double 数组 */
    AIO_BLOCK_INT32   = 2,   /* int32  数组 */
    AIO_BLOCK_INT64   = 3,   /* int64  数组 */
    AIO_BLOCK_STRING  = 4,   /* UTF-8 字符串 (data 指向 char 数组, count=字节数, n_dims=1, dims=[count]) */
    AIO_BLOCK_KV      = 5,   /* key-value 对 (data 指向 AioKVEntry 数组, count=条目数, n_dims=1, dims=[count]) */
    AIO_BLOCK_RAW     = 6,   /* 原始字节 (data 指向 uint8_t 数组, count=字节数) */
} AioBlockType;

/* KV 条目 (用于 header / cal_stats / photo_stats 等块) */
typedef struct {
    char key[64];
    char value[256];
} AioKVEntry;

/* ===========================================================================
 * 冻结资源上限 (BLOCKER-DF-001: 防止损坏输入触发异常分配)
 * =========================================================================== */
#define AIO_CACHE_MAX_BLOCKS      65536
#define AIO_CACHE_MAX_DIMS        4
#define AIO_CACHE_MAX_BLOCK_BYTES (1LL << 32)   /* 4 GiB / 单块 */
#define AIO_CACHE_MAX_FRAME_BYTES (1LL << 34)   /* 16 GiB / 整帧 */
#define AIO_CACHE_MAX_STR_LEN     65535         /* name/description/KV 上限 */
#define AIO_BLOCK_NAME_MAX        63            /* AioBlock.name[64] 实际上限 */

/* 命名块 */
typedef struct {
    char          name[64];        /* 块名 (如 "header", "data", "psf") */
    AioBlockType  type;            /* 数据类型 */
    void*         data;            /* 数据指针 (frame 拥有, destroy 时释放) */
    int64_t       count;           /* 元素个数 (KV=条目数, STRING=字节数, 数值=元素数) */
    int           dims[4];         /* 维度 (如 [H,W] 或 [N,4]) */
    int           n_dims;          /* 维度数 (0 表示标量/未指定) */
    char          description[128];/* 人类可读描述 */
} AioBlock;

/* PipelineFrame = 纯命名块容器 */
typedef struct {
    AioBlock* blocks;           /* 块数组 (动态扩容) */
    int       n_blocks;         /* 当前块数量 */
    int       blocks_capacity;  /* 块数组容量 */
    int       stages_completed; /* 阶段完成位掩码 */
} PipelineFrame;

/* ===========================================================================
 * ABI 握手信息 (Dataflow_ABI_Contract)
 * =========================================================================== */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t pointer_size;
    uint32_t enum_fingerprint;
    uint32_t pipeline_frame_size;
    uint32_t aio_block_size;
    uint32_t aio_kv_entry_size;
    uint64_t capability_bits;
    char     build_id[64];
} AstroAbiInfo;

/* capability bits */
#define AIO_CAP_BASIC        0x00000001u
#define AIO_CAP_CACHE_V1     0x00000002u
#define AIO_CAP_ALLOCATOR    0x00000004u
#define AIO_CAP_FP64         0x00000008u

/* 旧版 PipelineStageFn 已废弃，保留别名以兼容；error_msg/error_capacity 契约：error_msg 可为 NULL，
 * error_capacity>0 时保证 NUL 终止、超长截断；input/output/params 均为 const 语义（output 仅通过块 API 写入） */
typedef int (*PipelineStageFn)(const PipelineFrame* input, PipelineFrame* output, const void* params, char* error_msg, int error_capacity);

/* ===========================================================================
 * 帧生命周期
 * =========================================================================== */

AIO_EXPORT PipelineFrame* aio_pipeline_frame_create(void);
AIO_EXPORT void aio_pipeline_frame_destroy(PipelineFrame* frame);
AIO_EXPORT size_t aio_pipeline_frame_memory_usage(const PipelineFrame* frame);

/* ABI 握手: 返回 AIO 模块 ABI 信息 (调用方检查 abi_version/struct_size/指针宽度) */
AIO_EXPORT const AstroAbiInfo* aio_abi_info(void);

/* 规范化分配器 (跨 DLL 边界): add_block_move 只接受 aio_alloc 创建的 buffer */
AIO_EXPORT void* aio_alloc(size_t size);
AIO_EXPORT void* aio_realloc(void* ptr, size_t size);
AIO_EXPORT void  aio_free(void* ptr);

/* ===========================================================================
 * 块管理 API
 * =========================================================================== */

/* 添加块（拷贝数据到 frame 内部 buffer）
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_add_block(PipelineFrame* frame,
    const char* name, AioBlockType type,
    const void* data, int64_t count,
    const int* dims, int n_dims,
    const char* description);

/* 添加块（move 语义：转移数据所有权，frame 接管释放；成功后调用方不得再释放/使用 data）
 * data 必须是 aio_alloc() 分配的 (不能用 new[] 或跨 CRT malloc)，
 * frame 用 aio_free() 释放; 未知 type / 负 count / 非法 dims 在接管前拒绝
 * 返回: 0=成功(move 完成), 非0=失败(所有权未转移, 调用方仍需自行释放) */
AIO_EXPORT int aio_frame_add_block_move(PipelineFrame* frame,
    const char* name, AioBlockType type,
    void* data, int64_t count,
    const int* dims, int n_dims,
    const char* description);

/* 获取块（返回只读指针，不存在返回 NULL）*/
AIO_EXPORT const AioBlock* aio_frame_get_block(const PipelineFrame* frame, const char* name);

/* 获取块数据指针（便捷函数，不存在返回 NULL）*/
AIO_EXPORT void* aio_frame_get_block_data(const PipelineFrame* frame, const char* name);

/* 获取块的元素个数（不存在返回 -1）*/
AIO_EXPORT int64_t aio_frame_get_block_count(const PipelineFrame* frame, const char* name);

/* 获取块类型（不存在返回 -1）*/
AIO_EXPORT int aio_frame_get_block_type(const PipelineFrame* frame, const char* name);

/* 移除并释放块（返回 0=成功, 1=不存在）*/
AIO_EXPORT int aio_frame_remove_block(PipelineFrame* frame, const char* name);

/* 检查块是否存在（1=存在, 0=不存在）*/
AIO_EXPORT int aio_frame_has_block(const PipelineFrame* frame, const char* name);

/* 列出所有块名
 * out_names: 输出缓冲区，每个块名占 64 字节
 * capacity: 缓冲区可容纳的块名数 (sizeof(out_names)/64)
 * out_count: 实际块数量 (即使缓冲区不够也会输出实际数量)
 * 返回: 0=成功, 非0=缓冲区不足或参数错误 */
AIO_EXPORT int aio_frame_list_blocks(const PipelineFrame* frame,
    char* out_names, int capacity, int* out_count);

/* ===========================================================================
 * KV 块操作 API (便捷函数，用于 header / cal_stats / photo_stats 等 KV 块)
 * =========================================================================== */

/* 设置 KV 块中某个 key 的 value (字符串形式)
 * 若块不存在则自动创建一个 KV 块
 * 若 key 已存在则覆盖
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_kv_set(PipelineFrame* frame, const char* block_name,
    const char* key, const char* value);

/* 获取 KV 块中某个 key 的 value
 * 不存在返回 NULL */
AIO_EXPORT const char* aio_frame_kv_get(const PipelineFrame* frame, const char* block_name,
    const char* key);

/* 设置 KV 块中某个 key 的 value (double 自动转字符串 "%.17g") */
AIO_EXPORT int aio_frame_kv_set_double(PipelineFrame* frame, const char* block_name,
    const char* key, double value);

/* 获取 KV 块中某个 key 的 value (字符串转 double)
 * 不存在或转换失败返回 default_value */
AIO_EXPORT double aio_frame_kv_get_double(const PipelineFrame* frame, const char* block_name,
    const char* key, double default_value);

/* ===========================================================================
 * 缓存文件 (.aio) - 无损读写所有块
 * =========================================================================== */

/* 保存所有块到缓存文件 (.aio 自定义二进制格式)
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_save_cache(const PipelineFrame* frame, const char* path);

/* 从缓存文件加载所有块 (清除现有块后加载)
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_load_cache(PipelineFrame* frame, const char* path);

/* ===========================================================================
 * 调试导出
 * =========================================================================== */

/* 导出单个块为 FITS 文件 (仅 FLOAT32 / FLOAT64 / INT32 / INT16 块)
 * dims 解释: 1D=[N], 2D=[H,W], 3D=[H,W,C]
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_export_block_fits(const PipelineFrame* frame,
    const char* block_name, const char* path);

/* 导出单个块为 XML 文件 (任意类型，含元数据 + base64 数据)
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_export_block_xml(const PipelineFrame* frame,
    const char* block_name, const char* path);

/* 导出所有块为 XML 文件 (含所有块的元数据 + 数据)
 * 返回: 0=成功, 非0=失败 */
AIO_EXPORT int aio_frame_export_all_xml(const PipelineFrame* frame, const char* path);

/* 旧版 export_xml 兼容包装 (导出所有块为 XML)
 * 等价于 aio_frame_export_all_xml，保留是为了不破坏旧调用方 */
AIO_EXPORT int aio_pipeline_export_xml(const PipelineFrame* frame,
    const char* path, const char* comment);

#ifdef __cplusplus
}
#endif

#endif /* AIO_PIPELINE_H */

/* ===========================================================================
 * 命名块容器设计要点
 * ---------------------------------------------------------------------------
 * 1. PipelineFrame 只有 blocks / n_blocks / blocks_capacity / stages_completed
 *    四个字段。所有数据（像素、WCS、SIP、PSF、SNR、HEALPix 等）都作为
 *    命名块存储。
 *
 * 2. 块查找采用名称索引 + 线性扫描。块数量通常 <20，O(n) 查找足够快，
 *    无需缓存指针或哈希表。模块内部可缓存块指针避免重复查找。
 *
 * 3. 内存管理：
 *    - add_block: 拷贝数据到 malloc 分配的内部 buffer
 *    - add_block_move: 转移数据所有权 (data 必须是 malloc 分配)
 *    - remove_block: 释放块数据并从数组移除
 *    - frame_destroy: 遍历释放所有块数据 + free blocks 数组
 *
 * 4. KV 块用于存储 FITS 头字段、WCS、SIP、统计信息等 key-value 数据。
 *    - kv_set/kv_get 支持字符串形式
 *    - kv_set_double/kv_get_double 支持 double 形式 (自动字符串转换)
 *
 * 5. 缓存文件 (.aio) 格式：
 *    [Magic: "AIO1"][Version: int32][N_Blocks: int32][Stages_Completed: int32]
 *    For each block:
 *      [Name_Len: int32][Name: bytes]
 *      [Type: int32][N_Dims: int32][Dims: int32×N_Dims][Count: int64]
 *      [Data_Size: int64][Data: bytes]
 *      [Desc_Len: int32][Desc: bytes]
 *    KV 块的 Data 格式: 连续的 [Key_Len][Key][Val_Len][Val] 条目
 * ===========================================================================
 *
 * ===========================================================================
 * 标准块定义表
 * ---------------------------------------------------------------------------
 * | 块名          | 类型     | dims         | 内容                              |
 * |---------------|----------|--------------|-----------------------------------|
 * | header        | KV       | [N_kv]       | FITS头 + WCS + SIP (key-value)    |
 * | data          | FLOAT32  | [H,W]        | 图像像素数据                       |
 * | weight        | FLOAT32  | [H,W]        | 权重图                            |
 * | snr           | FLOAT32  | [H,W]        | 信噪比图                          |
 * | psf           | FLOAT64  | [6]          | PSF 模型参数                      |
 * | star_det      | FLOAT64  | [N,6]        | 星点检测权威块 (x,y,flux,mag,saturated,has_saturated) |
 * | star_det_psf_compat | FLOAT32 | [N,4] | PSF 兼容视图 (x,y,flux,mag, 显式生成) |
 * | gaia_cat      | FLOAT64  | [N,3]        | Gaia 星表 (ra,dec,mag)           |
 * | grad_map      | FLOAT32  | [H,W]        | 梯度图 M_map                      |
 * | cal_stats     | KV       | [N_kv]       | 校准统计信息                       |
 * | photo_stats   | KV       | [N_kv]       | 光度统计信息                       |
 * | healpix       | RAW      | [N]          | HEALPix 数据包                    |
 * ---------------------------------------------------------------------------
 * 注: 块名大小写敏感。未列出的自定义块名也允许（模块可自由扩展）。
 * ===========================================================================
 *
 * ===========================================================================
 * 块生命周期管理 (编排器按阶段丢弃块)
 * ---------------------------------------------------------------------------
 * | 阶段完成后  | 丢弃的块                                              |
 * |-------------|-------------------------------------------------------|
 * | PLATESOLVE  | weight                                                |
 * | PHOTOMETRIC | star_det, gaia_cat, psf                               |
 * | DRIZZLE     | data, snr, weight, grad_map, cal_stats, photo_stats   |
 * | STACK       | healpix                                               |
 * ---------------------------------------------------------------------------
 * 注: header 块在整个管线生命周期内保留（含元数据+WCS+SIP）。
 *     编排器可通过 aio_frame_remove_block 显式丢弃。
 * ===========================================================================
 */
