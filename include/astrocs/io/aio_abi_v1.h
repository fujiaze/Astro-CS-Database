/* AstroCS 唯一 AIO C ABI v1 — include/astrocs/io/aio_abi_v1.h (AIO-001)
 *
 * 角色: AIO (Astro Image IO) 域唯一版本化 C ABI 合同头。AIO-001 冻结。
 * 任务: AIO-001 (ASTROCS-CONSTITUTION-ALIGNMENT-V1) "建立唯一AIO C ABI与内容哈希复核"。
 * 合同登记: contracts/data/aio_abi_contract_v1.json (唯一事实源)。
 *
 * 宪章锚 (ASTROCS-CONSTITUTION-001):
 *   - §8.3  lib/infrastructure/aio 是唯一 FITS/HiPS/manifest I/O 边界 (目标态);
 *     本头是该唯一边界的版本化 C ABI v1 合同面, 现有实现 (astro_image_io /
 *     runtime/io) 的 ABI 数值域向本头收敛。
 *   - §8.6  跨 DLL 边界使用版本化 C ABI: 不传 STL、C++ exception、RTTI 对象或
 *     编译器私有类型; 结构体带 struct_size/abi_version; buffer 所有权和释放方
 *     明确; 失败返回稳定状态码。
 *
 * 对齐矩阵 (数值冻结; v1 不可变, 扩展须升版本):
 *   - 0..7  与 include/astrocs/common_abi_v1.h acs_status 共同子域数值一致。
 *   - 0..13 与 modules/services/io/.../fits_stream_v1.h acs_fio_status 全域一致
 *     (IO 家族同域; _Static_assert 编译期对齐证明)。
 *   - 14/15 为 AIO 内容哈希复核专属扩展码。
 *
 * 纯 C11 可编译 (extern "C" 兼容 C++17); 禁 STL/异常/RTTI; 无第三方类型。
 * 并发合同逐函数标注; 文本公共格式 UTF-8。
 */
#ifndef ASTROCS_IO_AIO_ABI_V1_H
#define ASTROCS_IO_AIO_ABI_V1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIO_ABI_VERSION_V1 1u

/* 内容哈希复核算法与摘要形态 (v1 冻结; DATA-001 manifest content_digest
 * algorithm=sha256/64hex 同源; 未来换算法必须升 ABI 版本) */
#define AIO_HASH_ALGORITHM_ID "sha256"
#define AIO_HASH_ALGORITHM_SHA256 1u
#define AIO_DIGEST_HEX_LEN 64u /* 不含 NUL; 调用方缓冲须 >= 65 字节 */

/* 文件复核读块大小 (栈上; reentrant 每调用独立) */
#define AIO_HASH_FILE_CHUNK 32768u

/* ───────── 状态码 (v1 数值冻结) ───────── */
typedef enum aio_status {
    AIO_OK = 0,
    AIO_ERR_PARAM = 1,          /* 参数非法 (NULL/越界) */
    AIO_ERR_ABI_MISMATCH = 2,   /* struct_size/abi_version 失配 */
    AIO_ERR_NOMEM = 3,
    AIO_ERR_IO = 4,             /* 文件打开/读失败 */
    AIO_ERR_UNSUPPORTED = 5,
    AIO_ERR_CANCELLED = 6,
    AIO_ERR_STATE = 7,
    AIO_ERR_TRUNCATED = 8,      /* 文件字节数与声明不符 (IO 家族同值) */
    AIO_ERR_BAD_HEADER = 9,
    AIO_ERR_MISMATCH = 10,
    AIO_ERR_CHECKSUM = 11,
    AIO_ERR_NANINF = 12,
    AIO_ERR_DISKFULL = 13,
    /* AIO 专属扩展 (14+) */
    AIO_ERR_DECL_INVALID = 14,  /* 声明 digest 非 sha256/64hex 或 size 非法 */
    AIO_ERR_HASH_MISMATCH = 15, /* 重算 sha256 与声明不一致 (篡改/损坏) */
    AIO_ERR_INTERNAL = 70,      /* 未分类内部错误 (同 acs_status INTERNAL=70 语义) */
    AIO_STATUS_COUNT = 71       /* 哨兵: 等于该值及以外均非法 (0..15 与 70 有效) */
} aio_status;

/* ───────── ABI 信息 (唯一握手) ───────── */
typedef struct aio_abi_info_v1 {
    uint32_t struct_size; /* sizeof(aio_abi_info_v1) */
    uint32_t abi_version; /* AIO_ABI_VERSION_V1 */
    uint32_t status_count;        /* AIO_STATUS_COUNT */
    uint32_t hash_algorithm_id;   /* AIO_HASH_ALGORITHM_SHA256 */
    uint32_t digest_hex_len;      /* AIO_DIGEST_HEX_LEN = 64 */
    uint32_t reserved0;           /* 恒 0 (对齐填充, v1) */
    uint64_t capability_bits;     /* AIO_CAP_* 位或 */
    char hash_algorithm_name[16]; /* "sha256" + NUL */
} aio_abi_info_v1;

/* capability bits (v1) */
#define AIO_CAP_BASIC 0x00000001u          /* ABI 握手与状态码域 */
#define AIO_CAP_HASH_REVIEW 0x00000002u    /* 内容哈希复核 (buffer+file) */

#define AIO_ABI_INFO_V1_INIT                                                \
    {                                                                       \
        sizeof(aio_abi_info_v1), AIO_ABI_VERSION_V1, AIO_STATUS_COUNT,      \
        AIO_HASH_ALGORITHM_SHA256, AIO_DIGEST_HEX_LEN, 0u,                  \
        (AIO_CAP_BASIC | AIO_CAP_HASH_REVIEW), AIO_HASH_ALGORITHM_ID        \
    }

/* ───────── 唯一握手入口 ─────────
 * out 由调用方分配 (栈/静态均可); 成功时整体填充。
 * out == NULL                    -> AIO_ERR_PARAM (不写)
 * out->struct_size/abi_version   -> 失配即 AIO_ERR_ABI_MISMATCH (不猜布局)
 * reentrant; threadsafe; internal_parallel=none (只填静态常量)。 */
int aio_abi_query_v1(aio_abi_info_v1* out);

/* ───────── 内容哈希复核 v1 (sha256 重算 = 复核原语) ─────────
 *
 * 背景 (AIO-001): DATA-003 合同的 read_verified/hash 复核在消费端落地前,
 * 内容 digest 声明曾只验格式 (64hex) 不重算内容。本 ABI 把"重算复核"收口
 * 为 AIO 唯一 C ABI 服务: 任何消费方对已声明 sha256 的内容/文件, 一律经
 * 本入口重算并比对, 拒绝静默信任声明。 */

/* 对内存内容重算 sha256 并输出 64 字符小写 hex + NUL。
 * data 可 NULL 当且仅当 len == 0 (空内容合法, = e3b0c442... 摘要)。
 * out_hex 调用方分配, 容量 >= AIO_DIGEST_HEX_LEN+1。
 * data NULL 且 len != 0 -> AIO_ERR_PARAM; out_hex NULL -> AIO_ERR_PARAM。
 * reentrant (无全局状态); internal_parallel=none (确定性单流)。 */
int aio_content_hash_buffer_v1(const void* data, uint64_t len, char* out_hex);

/* 复核内存内容: 重算 sha256 并与声明 declared_hex64 比对。
 * 一致 -> AIO_OK; 重算不一致 -> AIO_ERR_HASH_MISMATCH;
 * 声明非法 (非 64 位 [0-9a-f] 小写 hex) -> AIO_ERR_DECL_INVALID (先验声明,
 * 不给篡改内容以"格式错"混淆篡改的机会)。
 * declared_hex64 NULL -> AIO_ERR_DECL_INVALID。
 * reentrant; internal_parallel=none。 */
int aio_content_hash_verify_buffer_v1(const void* data, uint64_t len,
                                      const char* declared_hex64);

/* 复核磁盘文件: 分块流式重算 (不整体载入) sha256, 与声明比对;
 * expected_size 非 0 时同时核对文件字节数 (DATA-001 manifest size 语义)。
 * path_utf8 为 UTF-8 路径 (与 IO-001 acs_fio_* 同语义; Windows 宽字符适配
 * 属上层)。declared_hex64 非法 -> AIO_ERR_DECL_INVALID;
 * 打开/读失败 -> AIO_ERR_IO; 字节数不符 -> AIO_ERR_TRUNCATED;
 * 摘要不符 -> AIO_ERR_HASH_MISMATCH。
 * actual_size_out 可 NULL; 非 NULL 时成功/截断路径写入实际字节数。
 * reentrant (无全局状态; 调用期间文件只读); internal_parallel=none。 */
int aio_content_hash_verify_file_v1(const char* path_utf8,
                                    const char* declared_hex64,
                                    uint64_t expected_size,
                                    uint64_t* actual_size_out);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ───────── 编译期对齐证明 (IO 家族数值域一致; AIO-001) ─────────
 * C11 _Static_assert / C++11 static_assert 双侧成立。
 * 0..13 必须与 acs_fio_status 全域逐值一致; 0..7 同时与 acs_status 一致。 */
#if defined(__cplusplus)
static_assert(AIO_OK == 0 && AIO_ERR_PARAM == 1 && AIO_ERR_ABI_MISMATCH == 2 &&
                  AIO_ERR_NOMEM == 3 && AIO_ERR_IO == 4 &&
                  AIO_ERR_UNSUPPORTED == 5 && AIO_ERR_CANCELLED == 6 &&
                  AIO_ERR_STATE == 7 && AIO_ERR_TRUNCATED == 8,
              "AIO-001: aio_status 0..8 必须与 acs_status/acs_fio_status 数值一致");
static_assert(AIO_ERR_BAD_HEADER == 9 && AIO_ERR_MISMATCH == 10 &&
                  AIO_ERR_CHECKSUM == 11 && AIO_ERR_NANINF == 12 &&
                  AIO_ERR_DISKFULL == 13 && AIO_ERR_DECL_INVALID == 14 &&
                  AIO_ERR_HASH_MISMATCH == 15 && AIO_ERR_INTERNAL == 70 &&
                  AIO_STATUS_COUNT == 71,
              "AIO-001: aio_status 9..15/70/71 数值冻结");
#else
_Static_assert(AIO_OK == 0 && AIO_ERR_PARAM == 1 && AIO_ERR_ABI_MISMATCH == 2 &&
                   AIO_ERR_NOMEM == 3 && AIO_ERR_IO == 4 &&
                   AIO_ERR_UNSUPPORTED == 5 && AIO_ERR_CANCELLED == 6 &&
                   AIO_ERR_STATE == 7 && AIO_ERR_TRUNCATED == 8,
               "AIO-001: aio_status 0..8 必须与 acs_status/acs_fio_status 数值一致");
_Static_assert(AIO_ERR_BAD_HEADER == 9 && AIO_ERR_MISMATCH == 10 &&
                   AIO_ERR_CHECKSUM == 11 && AIO_ERR_NANINF == 12 &&
                   AIO_ERR_DISKFULL == 13 && AIO_ERR_DECL_INVALID == 14 &&
                   AIO_ERR_HASH_MISMATCH == 15 && AIO_ERR_INTERNAL == 70 &&
                   AIO_STATUS_COUNT == 71,
               "AIO-001: aio_status 9..15/70/71 数值冻结");
#endif

#endif /* ASTROCS_IO_AIO_ABI_V1_H */
