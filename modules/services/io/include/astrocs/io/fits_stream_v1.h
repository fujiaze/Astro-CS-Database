/* AstroCS FITS Stream C ABI v1 — modules/services/io/include/astrocs/io/fits_stream_v1.h
 *
 * 角色: astrocs_io.dll 对外的 FITS 流式读/写/校验 C ABI (IO-001 冻结)。
 * 冻结合同见 docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md (DOC-IO-INTERFACE-001)。
 *
 * 关键约束 (v1 不可变; 扩展须升版本):
 *   1) CFITSIO 等第三方 FITS 类型/句柄绝不跨本 DLL 边界: 本头不出现任何 CFITSIO 头/类型,
 *      只使用 POD + 定长字符数组 + opaque handle + 注入回调。
 *   2) 所有跨边界结构前两字段 = struct_size + abi_version (同 acs_head 模式); 失配即拒。
 *   3) 所有权: 只读路径 out 缓冲由调用方分配; opaque handle 仅经 open/close 对管理;
 *      无跨边界托管分配 (骨架无跨边界 free 义务)。
 *   4) 文本公共格式 UTF-8 定长数组; 禁 STL/异常/RTTI。
 *   5) 错误码数值与 include/astrocs/common_abi_v1.h 的 acs_status 一致, 并扩展 IO 专属码。
 *
 * 纯 C11 可编译 (extern "C" 兼容 C++17)。
 */
#ifndef ASTROCS_IO_FITS_STREAM_V1_H
#define ASTROCS_IO_FITS_STREAM_V1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_FIO_ABI_VERSION_V1 1u
#define ACS_FIO_HEADER_MAX_CARDS 1024
#define ACS_FIO_KEYWORD_NAME_MAX 9    /* FITS 卡片关键字 ≤8 字符 + NUL */
#define ACS_FIO_KEYWORD_VALUE_MAX 72
#define ACS_FIO_KEYWORD_COMMENT_MAX 72
#define ACS_FIO_BUNIT_MAX 32
#define ACS_FIO_PATH_MAX 512
#define ACS_FIO_ERR_TEXT_MAX 96
#define ACS_FIO_NAXIS_MAX 3
#define ACS_FIO_DATASUM_LEN 16        /* "4294967295" + NUL 冗余 */

/* BITPIX 值 (FITS 标准) */
enum {
  ACS_FIO_BITPIX_U8 = 8,
  ACS_FIO_BITPIX_I16 = 16,
  ACS_FIO_BITPIX_I32 = 32,
  ACS_FIO_BITPIX_I64 = 64,
  ACS_FIO_BITPIX_F32 = -32,
  ACS_FIO_BITPIX_F64 = -64
};

/* ───────── 错误码 (v1 冻结; 数值与 common_abi_v1 acs_status 对齐) ───────── */
typedef enum acs_fio_status {
  ACS_FIO_OK = 0,
  ACS_FIO_ERR_PARAM = 1,
  ACS_FIO_ERR_ABI_MISMATCH = 2,
  ACS_FIO_ERR_NOMEM = 3,
  ACS_FIO_ERR_IO = 4,
  ACS_FIO_ERR_UNSUPPORTED = 5,
  ACS_FIO_ERR_CANCELLED = 6,
  ACS_FIO_ERR_STATE = 7,
  ACS_FIO_ERR_TRUNCATED = 8,
  ACS_FIO_ERR_BAD_HEADER = 9,
  ACS_FIO_ERR_MISMATCH = 10,
  ACS_FIO_ERR_CHECKSUM = 11,
  ACS_FIO_ERR_NANINF = 12,
  ACS_FIO_ERR_DISKFULL = 13,
  ACS_FIO_STATUS_COUNT = 14 /* 哨兵: 等于该值及以外均非法 */
} acs_fio_status;

/* 读写方向 (trace 用途) */
enum { ACS_FIO_DIR_READ = 0, ACS_FIO_DIR_WRITE = 1 };

/* ───────── 注入 hooks (host → io; IO-001 trace bytes) ───────── */
typedef struct acs_fio_trace_hooks_v1 {
  uint32_t struct_size;   /* sizeof(acs_fio_trace_hooks_v1) */
  uint32_t abi_version;   /* ACS_FIO_ABI_VERSION_V1 */
  /* 每次底层文件 read/write 完成后以实际字节数回调; 可为 NULL */
  void (*on_read_bytes)(void* ud, uint64_t bytes);
  void (*on_write_bytes)(void* ud, uint64_t bytes);
  /* 取消轮询: 返回非 0 = 已取消; 可为 NULL (永不取消) */
  int (*is_cancelled)(void* ud);
  void* user_data;
} acs_fio_trace_hooks_v1;

#define ACS_FIO_TRACE_HOOKS(ud, onr, onw, canc) \
  { sizeof(acs_fio_trace_hooks_v1), ACS_FIO_ABI_VERSION_V1, (onr), (onw), (canc), (ud) }

/* ───────── 关键字卡片 ───────── */
typedef struct acs_fio_keyword_v1 {
  char name[ACS_FIO_KEYWORD_NAME_MAX];
  char value[ACS_FIO_KEYWORD_VALUE_MAX];
  char comment[ACS_FIO_KEYWORD_COMMENT_MAX];
} acs_fio_keyword_v1;

/* ───────── header 结构 (读出) / 写入声明 ───────── */
typedef struct acs_fio_header_v1 {
  uint32_t struct_size;      /* sizeof(acs_fio_header_v1) */
  uint32_t abi_version;      /* ACS_FIO_ABI_VERSION_V1 */
  int32_t  bitpix;           /* 8/16/32/64/-32/-64 */
  int32_t  naxis;            /* 0..3 (v1 支持域) */
  int64_t  naxis_n[ACS_FIO_NAXIS_MAX]; /* NAXIS1..NAXISn; 主顺序 C 视图: naxis_n[0]=最快轴 */
  int32_t  keyword_count;
  acs_fio_keyword_v1 keywords[ACS_FIO_HEADER_MAX_CARDS];
} acs_fio_header_v1;

/* ───────── opaque handles ───────── */
typedef struct acs_fio_reader_v1_s acs_fio_reader_v1;
typedef struct acs_fio_writer_v1_s acs_fio_writer_v1;

/* ───────── 生命周期 / 结构查询 ───────── */

/* 打开 FITS 文件准备读。不加载数据区; 立即解析主 header (结构合法性校验)。
 * 失败返回非 0 且 *out=NULL; err/err_cap 可 NULL。所有权: 句柄转移给调用方, 必须 close。
 * 并发: reentrant; 句柄非共享 (同一句柄不同时调用)。 */
int acs_fio_reader_open_v1(const char* path_utf8,
                           const acs_fio_trace_hooks_v1* hooks,
                           acs_fio_reader_v1** out,
                           char* err, size_t err_cap);

/* 读取已解析的 header (结构字段 + 关键字表)。hdr 由调用方分配, 本函数填充;
 * 关键字表为 END 前卡片顺序视图。 */
int acs_fio_get_header_v1(acs_fio_reader_v1* rd,
                          acs_fio_header_v1* hdr,
                          char* err, size_t err_cap);

/* 读取第 plane_index 个平面 (0-based, NAXIS3 方向) 到 buf。
 * 语义: 平面 = NAXIS2(行) x NAXIS1(列) 的连续 C 视图 (行主序)。
 * nx/ny/bpix: 调用方期望的平面宽/高/dtype; 与 header 不符 → ACS_FIO_ERR_MISMATCH。
 * 期望为 0/0 表示按 header 实际值; bpix=0 表示按 header BITPIX。
 * bunit 期望 (可为 NULL/空 = 不校验); 不符 → ACS_FIO_ERR_MISMATCH。
 * buf: 调用方分配; buf_elem_capacity 元素容量 (非字节); *out_got 返回实际元素数。
 * strict_nan: 1=平面含 NaN/Inf → ACS_FIO_ERR_NANINF; 0=放行 (NaN/Inf 合法 FITS 值)。
 * 失败: 数据已部分写入 buf; *out_got 为该部分元素数。 */
int acs_fio_read_plane_v1(acs_fio_reader_v1* rd,
                          int plane_index,
                          int64_t nx, int64_t ny, int32_t bpix,
                          const char* bunit,
                          void* buf, int64_t buf_elem_capacity,
                          int strict_nan,
                          int64_t* out_got,
                          acs_fio_trace_hooks_v1* trace,   /* 读取累计计数 (可 NULL=丢弃) */
                          char* err, size_t err_cap);

/* 读取数据区连续 chunk: 从平面 plane_index 的第 first_elem 个元素起读 count 个元素。
 * 元素序 = 该平面 C 主序 (最快轴 NAXIS1)。跨平面边界的行为: 只读本平面, 越界取实际可得。
 * dtype 转换: 期望 dtype 与文件不同 → ACS_FIO_ERR_MISMATCH (v1 不隐式转换)。
 * 返回值: 实际读得元素数; buf 容量由调用方保证 (buf_elem_capacity 仍须 ≥ count, 否则 PARAM)。 */
int acs_fio_read_chunk_v1(acs_fio_reader_v1* rd,
                          int plane_index,
                          int64_t first_elem, int64_t count,
                          void* buf, int64_t buf_elem_capacity,
                          int strict_nan,
                          int64_t* out_got,
                          acs_fio_trace_hooks_v1* trace,
                          char* err, size_t err_cap);

/* 关闭句柄并释放内部资源。rd=NULL 为空操作。 */
void acs_fio_reader_close_v1(acs_fio_reader_v1* rd);

/* ───────── 原子写 ───────── */

/* 开始原子写: 同目录临时文件 → 全部写完后 rename。
 * 声明 header 结构 (bitpix/naxis/shape)。bunit 写入 BUNIT 卡 (可为 NULL/空)。
 * overwrite: 1=允许覆盖已存在目标 (原子替换); 0=目标已存在则失败 (ACS_FIO_ERR_IO)。
 * 成功: 句柄转移; 必须 end (提交) 或 abort (放弃)。临时文件默认同名目录。 */
int acs_fio_writer_begin_v1(const char* path_utf8,
                            const acs_fio_header_v1* decl,
                            const char* bunit,
                            int overwrite,
                            const acs_fio_trace_hooks_v1* hooks,
                            acs_fio_writer_v1** out,
                            char* err, size_t err_cap);

/* 顺序写一个完整平面 (行主序 C 视图, 元素序与 read_plane 相同)。
 * 必须在声明 shape 内: plane_index < NAXIS3; 元素数 = NAXIS2*NAXIS1。
 * data_bytes = 元素数 * dtype 字节宽。 bpix 与声明一致, 否则 PARAM。 */
int acs_fio_write_plane_v1(acs_fio_writer_v1* wr,
                           int plane_index,
                           const void* data, size_t data_bytes,
                           acs_fio_trace_hooks_v1* trace,
                           char* err, size_t err_cap);

/* 顺序写 chunk (同一平面内连续元素, 或跨平面按声明 shape 的全局线性序)。
 * 语义: 把 writer 内部视为 (NAXIS3*NAXIS2*NAXIS1) 的线性 C 数组, 每元素 dtype 宽;
 * 调用按序推进; 超出总量 → ACS_FIO_ERR_STATE。 */
int acs_fio_write_chunk_v1(acs_fio_writer_v1* wr,
                           const void* data, size_t data_bytes,
                           acs_fio_trace_hooks_v1* trace,
                           char* err, size_t err_cap);

/* 提交: 填充尾部 2880 块、写 DATASUM (write_datasum=1) / CHECKSUM (write_checksum=1) 卡
 * (需要重写 header 块), 关闭临时文件, 可选重读校验 (verify_before_rename=1: 结构+长度),
 * 原子 rename。成功后 wr 失效。 */
int acs_fio_writer_end_v1(acs_fio_writer_v1* wr,
                          int write_datasum, int write_checksum,
                          int verify_before_rename,
                          acs_fio_trace_hooks_v1* trace,
                          char* err, size_t err_cap);

/* 放弃: 删除临时文件。wr 失效。 */
void acs_fio_writer_abort_v1(acs_fio_writer_v1* wr);

/* ───────── 校验 / checksum ───────── */

/* 独立 DATASUM 算法 (无 CFITSIO): 对文件数据区 2880 字节块做 32 位 1 补码校验,
 * 返回 10 位十进制 ASCII (同 FITS DATASUM 语义)。datasum 至少 ACS_FIO_DATASUM_LEN。
 * 只校验数据区; 调用方应已确认文件结构合法 (或由本函数内部先做结构校验: strict=1)。
 * verify_checksum=1: 若 header 含 CHECKSUM 卡也校验整个 HDU。
 * 返回 ACS_FIO_ERR_BAD_HEADER/TRUNCATED/CHECKSUM 等; ok 时 *out_len = 字符数 (不含 NUL)。 */
int acs_fio_verify_file_v1(const char* path_utf8,
                           int verify_checksum,
                           char* err, size_t err_cap);

/* 计算文件数据区 DATASUM 十进制串 (同 verify 内部算法; 供调用方比对/记录)。
 * datasum 缓冲容量 ≥ ACS_FIO_DATASUM_LEN; 成功时 *out_len=10, 缓冲 NUL 结尾。 */
int acs_fio_compute_file_datadigest_v1(const char* path_utf8,
                                       char* datasum, size_t datasum_cap,
                                       size_t* out_len,
                                       char* err, size_t err_cap);

/* trace 辅助: 返回该句柄自创建以来的累计 read/write 字节 (经 hook 注入累计; 无 hooks 时
 * 内部仍计数, 便于测试断言)。rd/wr 可为 NULL → 返回 0。 */
uint64_t acs_fio_reader_bytes_read_v1(acs_fio_reader_v1* rd);
uint64_t acs_fio_writer_bytes_written_v1(const acs_fio_writer_v1* wr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_IO_FITS_STREAM_V1_H */
