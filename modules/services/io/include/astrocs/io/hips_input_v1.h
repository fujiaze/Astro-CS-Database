/* AstroCS HiPS 输入读取 C ABI v1 — modules/services/io/include/astrocs/io/hips_input_v1.h
 *
 * 角色: astrocs_io.dll 对外的 HiPS 输入读取合同 (IO-002 冻结)。
 * 冻结合同见 docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md (DOC-IO-INTERFACE-002)。
 *
 * 关键约束 (v1 不可变; 扩展须升版本):
 *   1) tile FITS 平面读取全部复用 IO-001 fits_core (fits_stream_v1.h) —— 本头不重复
 *      FITS 解析; 不出现 CFITSIO 等第三方类型。
 *   2) 所有跨边界结构前两字段 = struct_size + abi_version; 失配即拒。
 *   3) 所有权: 只读 out 缓冲由调用方分配; opaque handle 仅经 open/close 对管理。
 *   4) 文本公共格式 UTF-8 定长数组; 禁 STL/异常/RTTI。
 *   5) 错误码 0-7 与 IO-001 acs_fio_status 数值一致; 8-11 为本接口扩展。
 *   6) 缺 tile 绝不父 order 静默回退: 请求 order-K ipix 无文件 → TILE_MISSING;
 *      任何父/子 order 内容都不得冒充该 tile 交付。
 *
 * 纯 C11 可编译 (extern "C" 兼容 C++17)。
 */
#ifndef ASTROCS_IO_HIPS_INPUT_V1_H
#define ASTROCS_IO_HIPS_INPUT_V1_H

#include <stddef.h>
#include <stdint.h>

#include "astrocs/io/fits_stream_v1.h" /* acs_fio_trace_hooks_v1 (trace/cancel 复用) */

#ifdef __cplusplus
extern "C" {
#endif

#define ACS_HIPS_ABI_VERSION_V1 1u
#define ACS_HIPS_ORDER_MAX 29       /* order K: 0..29 (NSIDE ≤ 2^38 安全域) */
#define ACS_HIPS_ORDER_MIN 0
#define ACS_HIPS_TILE_WIDTH_MAX 16384
#define ACS_HIPS_PROP_KEY_MAX 40    /* properties 键名 ≤ 39 字符 + NUL */
#define ACS_HIPS_PROP_VALUE_MAX 256 /* properties 值 ≤ 255 字符 + NUL */
#define ACS_HIPS_PROP_MAX 128       /* 单 properties 文件最多键数 */
#define ACS_HIPS_ERR_TEXT_MAX 96    /* 与 IO-001 一致 */
#define ACS_HIPS_PATH_MAX 1024

/* tile 状态 (只读定位; 状态机不承载错误文本) */
enum {
  ACS_HIPS_TILE_PRESENT = 1,   /* 文件存在且布局/头/dtype 校验通过 */
  ACS_HIPS_TILE_MISSING = 2,   /* order-K 域内但文件不存在 (不做父回退) */
  ACS_HIPS_TILE_INVALID = 3    /* 文件存在但布局/头/dtype 非法 */
};

/* ───────── 错误码 (v1 冻结; 0-7 与 acs_fio_status 对齐, 8-11 本接口扩展) ───────── */
typedef enum acs_hips_status {
  ACS_HIPS_OK = 0,
  ACS_HIPS_ERR_PARAM = 1,
  ACS_HIPS_ERR_ABI_MISMATCH = 2,
  ACS_HIPS_ERR_NOMEM = 3,
  ACS_HIPS_ERR_IO = 4,
  ACS_HIPS_ERR_UNSUPPORTED = 5,
  ACS_HIPS_ERR_CANCELLED = 6,
  ACS_HIPS_ERR_STATE = 7,
  ACS_HIPS_ERR_PROPERTIES = 8,   /* properties 缺失/必填键缺/值非法 */
  ACS_HIPS_ERR_ADDRESS = 9,      /* tile 地址/布局不符 (ipix 越界/Norder 不符) */
  ACS_HIPS_ERR_TILE_MISSING = 10,/* tile 不存在 (order-K 域内; 无父回退) */
  ACS_HIPS_ERR_TILE_INVALID = 11,/* tile 存在但布局/头/dtype 非法 */
  ACS_HIPS_STATUS_COUNT = 12     /* 哨兵: 等于该值及以外均非法 */
} acs_hips_status;

/* ───────── opaque handle ───────── */
typedef struct acs_hips_handle_v1_s* acs_hips_handle_v1;

/* ───────── 生命周期 ───────── */

/* 打开 HiPS 子产品目录 (base_dir + '/' + product 或 base_dir 本身当子产品目录)。
 * base_dir: 产品集根目录 (可含尾部 '/'); product: "signal"/"support"/"variance"/
 *   "ivar" 或 NULL/空串 = base_dir 即子产品目录 (含 properties)。
 *   注: "snr" 为 TSV catalogue HiPS (非 FITS 平面) —— 不在本合同科学平面
 *   FITS-only 支持域; 传入 snr/其它未知产品 → ACS_HIPS_ERR_UNSUPPORTED。
 * 立即解析并校验 properties (§3.1 必填键); 失败返回非 0 且 *out=NULL。
 * MOC: optional —— 存在则解析叶级 ipix 列表 (供 *_tile_count/_ipix);
 *   缺失/损坏不失败 (enum 接口返回 0 计数)。
 * 所有权: 句柄转移给调用方, 必须 close。并发: reentrant; 句柄非共享。 */
int acs_hips_open_v1(const char* base_dir_utf8,
                     const char* product,
                     const acs_fio_trace_hooks_v1* hooks,
                     acs_hips_handle_v1* out,
                     char* err, size_t err_cap);

/* 关闭句柄并释放内部资源。h=NULL 为空操作。 */
void acs_hips_close_v1(acs_hips_handle_v1 h);

/* ───────── properties 视图 ───────── */

/* 查询单键值。key 必填; 未找到 → ACS_HIPS_ERR_PARAM (调用方先查存在性/用 serialize)。
 * out 缓冲由调用方分配 (out_cap); 值截断不越界, NUL 结尾。 */
int acs_hips_props_get_v1(acs_hips_handle_v1 h, const char* key,
                          char* out, size_t out_cap,
                          char* err, size_t err_cap);

/* 整表序列化 "key=value\n" 视图 (按读入顺序; 供 manifest/trace 落点)。
 * out 可 NULL (只求长度); *out_len 返回需要字节数 (含 NUL)。容量不足 → PARAM。 */
int acs_hips_props_serialize_v1(acs_hips_handle_v1 h,
                                char* out, size_t out_cap, size_t* out_len,
                                char* err, size_t err_cap);

/* ───────── 布局 / 元数据查询 ───────── */

int acs_hips_get_order_v1(acs_hips_handle_v1 h, int32_t* out_order);
int acs_hips_get_tile_width_v1(acs_hips_handle_v1 h, int32_t* out_width);

/* 叶级 tile 集合大小。来自 MOC optional hint (order==hips_order 的 UNIQ 单元);
 * 无 MOC/无叶单元 → 0 (partial tree 合法)。 */
int acs_hips_tile_count_v1(acs_hips_handle_v1 h, int64_t* out_count);

/* 第 index 个叶级 tile 的 NESTED ipix (0-based; 有 MOC 时有效)。
 * index 越界/无 MOC → ACS_HIPS_ERR_PARAM。 */
int acs_hips_tile_ipix_v1(acs_hips_handle_v1 h, int64_t index, uint64_t* out_ipix);

/* 快速探测: ipix 是否在 order-K 域内且 tile 文件存在。
 * 只做路径存在性; 完整校验见 tile_status/读取接口。 */
int acs_hips_tile_exists_v1(acs_hips_handle_v1 h, uint64_t ipix, int* out_exists);

/* ───────── tile 状态与科学平面读取 ───────── */

/* 单 tile 状态: PRESENT/MISSING/INVALID (见 enum)。ipix 越界 → ADDRESS。
 * INVALID 时 err 附原因文本 (可 NULL)。不做父 order 回退。 */
int acs_hips_tile_status_v1(acs_hips_handle_v1 h, uint64_t ipix,
                            int32_t* out_status,
                            char* err, size_t err_cap);

/* 读 tile 科学平面 (FITS-only) 为 f32/f64 数组 (标准 HiPS 行主序,
 * out[y*TW+x], NAXIS1=x 最快, 同 IO-001 plane C 视图)。
 * out_elem_capacity: 元素容量; 必须 ≥ TW², 否则 PARAM。
 * tile 实际 BITPIX: -32 原样 / -64 原样 / 8 按字节提升; 其它 → INVALID。
 * MISSING → ACS_HIPS_ERR_TILE_MISSING; INVALID → ACS_HIPS_ERR_TILE_INVALID。
 * 成功: *out_got = TW²。err 可 NULL。 */
int acs_hips_read_tile_plane_f32_v1(acs_hips_handle_v1 h, uint64_t ipix,
                                    float* out, int64_t out_elem_capacity,
                                    int64_t* out_got,
                                    char* err, size_t err_cap);
int acs_hips_read_tile_plane_f64_v1(acs_hips_handle_v1 h, uint64_t ipix,
                                    double* out, int64_t out_elem_capacity,
                                    int64_t* out_got,
                                    char* err, size_t err_cap);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_IO_HIPS_INPUT_V1_H */
