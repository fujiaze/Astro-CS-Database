// ============================================================================
// aio_hips_reader.h - IVOA HiPS 读取器 C API (Phase1 Final Closure)
//
// Browser / HIPS_VERIFY 唯一后端: 不允许直接解析 properties/CFITSIO。
// ============================================================================

#ifndef AIO_HIPS_READER_H
#define AIO_HIPS_READER_H

#include <stdint.h>

#ifdef _WIN32
#define AIO_HIPS_RD_EXPORT __declspec(dllexport)
#else
#define AIO_HIPS_RD_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum AioHipsProduct {
    AIO_HIPS_RD_SIGNAL  = 0,
    AIO_HIPS_RD_SUPPORT = 1,
    AIO_HIPS_RD_SNR     = 2,
    // variance/ivar 产品读取
    AIO_HIPS_RD_VARIANCE = 3,
    AIO_HIPS_RD_IVAR     = 4,
    // nused/nrej 诊断统计平面读取 (int32, DATA-UNC-001 §30.2)
    AIO_HIPS_RD_NREJ     = 5,
    AIO_HIPS_RD_NUSED    = 6
};

typedef struct AioHipsDataset AioHipsDataset;

// 打开一个 HiPS 子产品 (signal/support/snr), 读取 properties。
// 返回句柄或 NULL (aio_hips_reader_last_error)。
AIO_HIPS_RD_EXPORT AioHipsDataset* aio_hips_open(
    const char* out_dir,      // 产品集根目录
    int product);             // AIO_HIPS_RD_*

// 读取 properties 关键字段 (hips_version/hips_order/...), 写入 buf。
AIO_HIPS_RD_EXPORT int aio_hips_get_properties(
    AioHipsDataset* d, char* buf, int buf_size);

// 获取叶级 tile 数
AIO_HIPS_RD_EXPORT int aio_hips_tile_count(AioHipsDataset* d);

// 获取第 i 个叶级 tile 的 NESTED ipix
AIO_HIPS_RD_EXPORT int aio_hips_tile_ipix(AioHipsDataset* d, int i, uint64_t* out_ipix);

// ── Issue #87: tile 读取完整性分类 ──────────────────────────────────────────
// 旧 *_f32/_f64/_i32 返回 0=成功、负=失败，保留原有负码语义（见下）；新增的
// *_ex 入口把"合法无覆盖"、"声明内缺失"、"I/O 故障"、"FITS 损坏"、"校验失败"、
// "足迹不可判"分开，并携带 CFITSIO 原始状态/路径/声明身份。
//
// 覆盖身份（footprint identity）来源与强弱（数据合同见 docs/engineering/contracts/
// HIPS_IO.md「状态与缺覆盖」）:
//   MOC_LEAF        : signal/Moc.fits 完整枚举 order==hips_order 的叶级单元
//                     ⇒ 集合内缺失 = DECLARED_MISSING；集合外缺失 = OUTSIDE。
//   MOC_UNDETERMINED: MOC 存在但含低阶(父级)或非法阶条目，无法归约到叶级；
//                     或 manifest.n_leaf_tiles 大于 MOC 叶级条目数（覆盖声明不完整）
//                     ⇒ 集合外缺失 = FOOTPRINT_UNDETERMINED（不冒充合法空洞；
//                     父级覆盖归约是独立任务 #95，本 reader 不做静默归约）。
//   MANIFEST_ONLY   : 无 MOC，但 manifest.n_leaf_tiles>0 声称有覆盖却无 tile 身份
//                     ⇒ 任意缺失 = FOOTPRINT_UNDETERMINED。
//   UNVERIFIED_LEGACY（数值=旧 NONE，0）: 无 MOC 且 manifest 无覆盖计数。
//                     没有任何**可靠覆盖来源**可判定"某单元是否本应存在"⇒ 缺失
//                     是"未证实的缺失"，不是"已证实的无覆盖"；本 reader 返回
//                     UNKNOWN_LEGACY_COVERAGE（旧 NONE 把它记成 OUTSIDE 是把
//                     "证据缺席"当成"证据为空"的缺陷）。兼容政策：调用方可继续
//                     产出 best-effort 可视图（NaN/coverage=0），但**不得**把该
//                     结果声明为已证输入完整性的科学完整产品；身份必须随成功产物
//                     显式落盘（aio_hips_coverage_verified()==0）。
//
// 失败分类（缺省不改变旧调用方 "0=成功 / 非0=失败" 的判法）:
//   OUTSIDE_DECLARED_FOOTPRINT  已证实无覆盖（旧码 -2，负缓存可缓存）
//   DECLARED_MISSING            声明覆盖内 tile 缺失（旧码 -7，输入完整性错误）
//   IO_ERROR                    真实打开/读取/权限/IO 故障（旧码 -8）
//   INVALID_TILE                FITS 截断/维度/BITPIX/头部损坏（旧码 -3..-6）
//   CHECKSUM_FAILED             存在 DATASUM/CHECKSUM 且验证失败（旧码 -9）
//   FOOTPRINT_UNDETERMINED      覆盖声明存在但身份不足（旧码 -10）
//   UNKNOWN_LEGACY_COVERAGE     无可靠覆盖来源，缺失无法证实也无从否证（旧码 -11，
//                               非 fail-closed：best-effort 可视图允许，但身份降级）
typedef enum AioHipsTileCode {
    AIO_HIPS_TILE_OK = 0,
    AIO_HIPS_TILE_OUTSIDE_DECLARED_FOOTPRINT = 1,
    AIO_HIPS_TILE_DECLARED_MISSING = 2,
    AIO_HIPS_TILE_IO_ERROR = 3,
    AIO_HIPS_TILE_INVALID = 4,
    AIO_HIPS_TILE_CHECKSUM_FAILED = 5,
    AIO_HIPS_TILE_FOOTPRINT_UNDETERMINED = 6,
    AIO_HIPS_TILE_UNKNOWN_LEGACY_COVERAGE = 7
} AioHipsTileCode;

typedef enum AioHipsFootprintIdentity {
    /* 数值 0 保持 ABI/产物兼容；语义 = legacy unverified coverage（旧名 NONE）。
     * 覆盖身份不可证 ⇒ 缺失记 UNKNOWN_LEGACY_COVERAGE，coverage_verified=0。 */
    AIO_HIPS_FOOTPRINT_NONE = 0,
    AIO_HIPS_FOOTPRINT_MOC_LEAF = 1,
    AIO_HIPS_FOOTPRINT_MOC_UNDETERMINED = 2,
    AIO_HIPS_FOOTPRINT_MANIFEST_ONLY = 3
} AioHipsFootprintIdentity;

/* 语义别名：FP_NONE 的正式名字（legacy unverified coverage）。 */
#define AIO_HIPS_FOOTPRINT_UNVERIFIED_LEGACY AIO_HIPS_FOOTPRINT_NONE

#define AIO_HIPS_TILE_PATH_MAX 1024
#define AIO_HIPS_TILE_DETAIL_MAX 512

typedef struct AioHipsTileStatus {
    int code;                 // AioHipsTileCode
    int legacy_code;          // 旧 *_f32 返回码（0 或 -2..-10），供旧调用方兼容
    int cfitsio_status;       // 底层 CFITSIO status（0 = 无）
    int footprint_identity;   // AioHipsFootprintIdentity
    int declared;             // 1=在覆盖声明集合内, 0=不在, -1=不可判
    int checksum_present;     // 1=存在可验证校验字段, 0=缺省（缺省不算失败）
    uint64_t tile_ipix;
    char tile_path[AIO_HIPS_TILE_PATH_MAX];
    char detail[AIO_HIPS_TILE_DETAIL_MAX];
} AioHipsTileStatus;

// 读取一个叶级 tile 为 float32 (signal/support)。out 至少 512*512*sizeof(float)。
// 返回 0=成功。
// out 为 standard HiPS row-major —— out[y*512+x] 中
// (x,y) 是 tile 二维图像坐标 (FITS NAXIS1=x, NAXIS2=y 原样), 不是 NESTED local。
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_f32(AioHipsDataset* d, uint64_t ipix, float* out);

// 读取一个叶级 tile 为 float64 (standard HiPS row-major, 同上)
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_f64(AioHipsDataset* d, uint64_t ipix, double* out);

// 读取一个叶级 tile 为 int32 (nused/nrej 诊断平面; standard HiPS row-major)。
// out 至少 512*512*sizeof(int32_t)。tile BITPIX 必须 = 32, 否则返回 -6
// (诊断平面 dtype 由 §30.2 固定 int32, 不接受 float 冒充)。
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_i32(AioHipsDataset* d, uint64_t ipix, int32_t* out);

// Issue #87 分类入口: 返回 AioHipsTileCode（OK=0; 其它见 enum）。st 可为 NULL。
// 成功时写满 out；非 OK 时不写 out。底层 CFITSIO 错因保留在 st->cfitsio_status
// 与 st->detail（诊断串，不用于判成败）。
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_f32_ex(
    AioHipsDataset* d, uint64_t ipix, float* out, AioHipsTileStatus* st);
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_f64_ex(
    AioHipsDataset* d, uint64_t ipix, double* out, AioHipsTileStatus* st);
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_i32_ex(
    AioHipsDataset* d, uint64_t ipix, int32_t* out, AioHipsTileStatus* st);

// 覆盖身份/声明查询（诊断与传播用；失败返回 -1）。
AIO_HIPS_RD_EXPORT int aio_hips_footprint_identity(AioHipsDataset* d);
AIO_HIPS_RD_EXPORT int aio_hips_declared_tile_count(AioHipsDataset* d);
// 1=在覆盖声明集合内, 0=不在, -1=不可判/参数错误。
AIO_HIPS_RD_EXPORT int aio_hips_tile_declared(AioHipsDataset* d, uint64_t ipix);

// Issue #87/E2-R: 输入完整性是否可证。
//   1 = 有可靠覆盖来源（MOC_LEAF），缺失可判为声明内/已证实无覆盖 ⇒ 成功产物
//       可声明 scientifically-verified input completeness；
//   0 = legacy unverified coverage（FP_NONE）：缺失无从证实/否证 ⇒ 产物只能作为
//       具名 best-effort，不得声明完整性已验证；
//  -1 = 参数错误。
AIO_HIPS_RD_EXPORT int aio_hips_coverage_verified(AioHipsDataset* d);

// 按 NESTED leaf ipix 查询单像素值。
// 内部执行 leaf_ipix -> local nested -> 标准 tile xy -> FITS row-major。
AIO_HIPS_RD_EXPORT int aio_hips_read_leaf_f32(AioHipsDataset* d, uint64_t leaf_ipix, float* out);
AIO_HIPS_RD_EXPORT int aio_hips_read_leaf_f64(AioHipsDataset* d, uint64_t leaf_ipix, double* out);

// 读取叶级 tile 的 CFITSIO DATASUM（数据校验和，科学 payload
// 指纹）。返回 0=ok；tile 无 DATASUM 时返回 1 并置 err。
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_datasum(
    AioHipsDataset* d, uint64_t tile_ipix, char* out, int out_size);

// SNR catalogue: 读出 SNR 目录 (ra[], dec[], snr[], star_id[],
// quality_flags[], photometric_status[])。调用方分配, max = 各数组容量。
// 返回语义是**点数返回型** (实现 aio_hips_reader.cpp:650-676), 0 与失败必须分开判:
//   >= 0 = 实际写出点数, 取 min(max, 目录条目数); 目录为空时**合法**返回 0;
//   <  0 = 失败 (实现只可能返回 -1)。调用方必须先判 <0 再用点数,
//   写成 `if (got > 0)` 会把「失败」与「目录为空」并成同一条静默跳过。
// -1 的三条来源: 句柄为空 / product 非 SNR (前置守卫)、读出抛 std::exception、
// 抛未知异常。
// 失败原因取 aio_hips_reader_last_error(), 但该串只能当诊断, 不能用来判成败:
//   (a) 前置守卫的 return -1 不置错误串 (实测 cpp:656 无 set_err);
//   (b) 该串是 thread_local 且成功路径不复位 (cpp:27/:29 只在失败时赋值),
//       故一次失败之后它会残留到下一次成功调用。
// 成败一律只看返回值。
// quality_flags/photometric_status 可为 NULL (旧调用方不读取)。
AIO_HIPS_RD_EXPORT int aio_hips_read_snr_catalog(
    AioHipsDataset* d, double* ra, double* dec, double* snr, int64_t* star_id,
    uint32_t* quality_flags, uint32_t* photometric_status, int max);

AIO_HIPS_RD_EXPORT void aio_hips_close(AioHipsDataset* d);

AIO_HIPS_RD_EXPORT const char* aio_hips_reader_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // AIO_HIPS_READER_H

