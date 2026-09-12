// ============================================================================
// aio_hips.h - IVOA HiPS 生产链 C API (Phase1 Final Closure , HiPS 直写)
//
// 唯一 AIO: HiPS 全部由 astro_image_io.dll 写入/读取。
// 标准: IVOA HiPS 1.4 (https:// www.ivoa.net/documents/HiPS/), NESTED,
// tile_width=512, NorderK/DirD/NpixN.fits, properties, MOC, Catalogue HiPS。
// FITS 一律由 vendored CFITSIO 4.6.4 写入 (不再手写 header/checksum)。
//
// 生产数据流 (无 HISS 中转):
// Drizzle TileAccumulator -> AstroSphereTileView -> aio_hips_* (本文件)
//
// 三个独立标准 HiPS 数据集 (子产品):
// <out_dir>/signal/ Image HiPS, signal = flux_sum / covered_area
// <out_dir>/support/ Image HiPS, support = covered_area / A_cell
// <out_dir>/snr/ Catalogue HiPS (TSV tiles)
// ============================================================================

#ifndef AIO_HIPS_H
#define AIO_HIPS_H

#include <stdint.h>

#ifdef _WIN32
#define AIO_HIPS_EXPORT __declspec(dllexport)
#else
#define AIO_HIPS_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// HiPS 子产品标志
enum AioHipsProductFlag {
    AIO_HIPS_PRODUCT_SIGNAL  = 1,
    AIO_HIPS_PRODUCT_SUPPORT = 2,
    AIO_HIPS_PRODUCT_SNR     = 4,
    // variance/ivar 产品 (Drizzle 方差传播)
    AIO_HIPS_PRODUCT_VARIANCE = 8,
    AIO_HIPS_PRODUCT_IVAR     = 16,
    // nused/nrej 诊断统计平面 (int32 子产品)
    // 位值由 DATA-UNC-001 §30.2 (DATA-P2-REJ-001) 冻结: NREJ=32 / NUSED=64;
    // 1/2/4/8/16 已被上述产品占用, 32/64 此前为空闲位。
    // AIO_ALL 掩码扩展由本域实现任务登记 (DATA_SEMANTICS §30.2 原文:
    // "AIO_ALL 掩码扩展由实现任务在 AIO 域合同登记, 本节只冻结位值不冻结掩码")。
    AIO_HIPS_PRODUCT_NREJ    = 32,
    AIO_HIPS_PRODUCT_NUSED   = 64,
    AIO_HIPS_PRODUCT_ALL     = 7,     // 向后兼容 (signal+support+snr)
    AIO_HIPS_PRODUCT_ALL_V19 = 31,    // 全产品 (含 variance/ivar)
    // 全产品 (含 variance/ivar/nrej/nused)。合法位域上界 —— 高于本值或为负
    // 的 flags 一律 begin 拒绝 (未定义位不得静默忽略)。
    AIO_HIPS_PRODUCT_ALL_V20 = 127
};

// 数据类型
enum AioHipsDataType {
    AIO_HIPS_FLOAT32 = 0,
    AIO_HIPS_FLOAT64 = 1
};

// Drizzle Tile 直写视图 ( 05_HIPS_DIRECT_PRODUCTION §2)
// parent_ipix: NESTED, 叶级 tile 父单元 (Norder K = log2(nside)-9)
// leaf_order: 叶级 Norder L (= log2(nside))
// width: 512 (HiPS 标准 tile)
// data_type: AIO_HIPS_FLOAT32 / AIO_HIPS_FLOAT64
// flux_sum: [width*width] 累计通量 (LeafAccumulator.sumFlux)
// covered_area: [width*width] 球面覆盖面积 sr (LeafAccumulator.sumArea)
// valid_mask: [width*width] 1=有效 (covered_area>0), 可 NULL (全部有效)
typedef struct {
    uint64_t parent_ipix;
    uint32_t leaf_order;
    uint32_t width;
    int32_t  data_type;
    const void* flux_sum;
    const void* covered_area;
    const uint8_t* valid_mask;
    // 方差传播分子 Σ v_j × w_jp² (w_jp = a_jp/A_j_drop)
    // variance = var_num_sum / covered_area² ; ivar = 1/variance
    // 可 NULL (不写 variance/ivar 产品)
    const void* var_num_sum;
} AstroSphereTileView;

// SNR catalogue 控制点 (: 携带 stable star_id 与真实状态字段)
typedef struct {
    double ra_deg;
    double dec_deg;
    double snr;
    int64_t star_id;            // PSF 阶段 stable star_id (禁止重新编号)
    uint32_t quality_flags;     // 位标志: 1=PSF_OK 2=saturated 4=has_saturated
                                // 8=photo_matched 16=photo_rejected
    uint32_t photometric_status; // 0=unmatched 1=used 2=rejected
} AioHipsSnrPoint;

typedef struct AioHipsProductSet AioHipsProductSet;

// ============================================================================
// aio_hips_product_begin - 开始一个 HiPS 产品集 (流式, 不要求 tiles 全在 RAM)
//
// 参数:
// out_dir - 输出根目录 (内含 signal/ support/ snr/ 三个子产品)
// nside - 叶级 NSIDE (2 的幂, >= 512)
// tile_width - 标准 512
// data_type - AIO_HIPS_FLOAT32 / AIO_HIPS_FLOAT64 (signal/support 存储精度)
// flags - 子产品位或 (AIO_HIPS_PRODUCT_*)
// creator_did - 数据集标识 (properties creator_did)
// obs_title - 数据集标题
// obs_filter - 滤光片 (可 NULL)
// exposure_s - 曝光时间秒
// obs_date - 观测日期 ISO (可 NULL)
// moc_order - MOC 阶 (<= tile order K), 0=auto(=K)
//
// 返回: 句柄 (失败返回 NULL, 用 aio_hips_last_error 获取原因)
// ============================================================================
AIO_HIPS_EXPORT AioHipsProductSet* aio_hips_product_begin(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    int32_t data_type,
    int flags,
    const char* creator_did,
    const char* obs_title,
    const char* obs_filter,
    double exposure_s,
    const char* obs_date,
    uint32_t moc_order);

// 写一个 signal/support 叶级 Tile (写 signal 与 support 两个 Image HiPS 的
// NorderK/DirD/NpixN.fits, 由 CFITSIO 写入含 checksum 的标准 FITS)
// 语义: signal = flux_sum/covered_area, support = covered_area/A_cell
// covered_area<=0 -> signal=NaN, support=0
AIO_HIPS_EXPORT int aio_hips_write_signal_support_tile(
    AioHipsProductSet* ps,
    const AstroSphereTileView* view);

// 写 variance/ivar 叶级 Tile (与 signal/support 同 view)。
// variance = var_num_sum / covered_area² ; ivar = 1/variance
// covered_area<=0 -> variance=NaN, ivar=NaN (与 signal NaN 语义一致)
// var_num_sum 为 NULL 或全部 <=0 -> 返回 -2 (无有效方差数据)。
// 产品目录: <out_dir>/variance/ 与 <out_dir>/ivar/。
AIO_HIPS_EXPORT int aio_hips_write_variance_tile(
    AioHipsProductSet* ps,
    const AstroSphereTileView* view);

// nused/nrej 诊断统计平面叶级 Tile 视图 (int32, DATA-UNC-001 §30.2)。
// parent_ipix / leaf_order / width 语义与 AstroSphereTileView 逐字一致
// (NESTED 叶级父单元 / 叶级 Norder / 512); data_type 不适用 —— 诊断平面
// dtype 固定 int32 (BITPIX=32), 无 precision 开关 (§30.2 冻结)。
// nused / nrej: [512*512] NESTED local 序 int32 计数平面, 可 NULL (该产品不写)。
//   语义 (SCI-INT §5 / SCI-REJ §5 冻结量的逐像素投影, 本通道不做任何新科学定义):
//     nused(p) = P2PixelResult.n_used
//     nrej(p)  = |{ s | reason_s ∉ {ACCEPTED, UNDERDETERMINED} }|
//   无覆盖像素 = 0/0 (int 无 NaN, 0 即"无"; 禁 −1 哨兵 —— 负值一律拒绝)。
typedef struct {
    uint64_t parent_ipix;
    uint32_t leaf_order;
    uint32_t width;
    const int32_t* nused;
    const int32_t* nrej;
} AioHipsDiagTileView;

// 写 nused/nrej 诊断统计平面叶级 Tile (BITPIX=32 int32 FITS + DATASUM/CHECKSUM)。
// 产品目录: <out_dir>/nrej/ 与 <out_dir>/nused/ (由 flags 位 AIO_HIPS_PRODUCT_NREJ /
// AIO_HIPS_PRODUCT_NUSED 选择; 未被 flags 选中的通道不落盘)。
// view->nused / view->nrej 为 NULL 且对应位已置 → 返回 -2 (无数据, 不写空占位)。
// 任一平面出现负值 → 返回 -5 (禁 −1 哨兵, §30.2 invalid 冻结)。
// 诊断平面不参与 hierarchy 低阶聚合 (§30.2 未冻结聚合语义, 不得臆造)。
AIO_HIPS_EXPORT int aio_hips_write_diag_tile(
    AioHipsProductSet* ps,
    const AioHipsDiagTileView* view);

// ═══════════════════════════════════════════════════════════════════════════
// DATA-UNC-001 §30.3 (DATA-P2-PROV-001) provenance 五键通道
//
// 五键 (键名由 DATA_SEMANTICS §30.3 冻结, 值语义同表):
//   ASTROCS_INPUT_MANIFEST_HASH  64hex sha256 (§20.3 公式)
//   ASTROCS_MODEL_HASH           UPM model_hash
//   ASTROCS_UNCERTAINTY_AVAILABLE true/false (§30.1 unavailable 规则判定结果)
//   ASTROCS_WEIGHT_MODE          0/1/2 (cfg.weight_mode)
//   ASTROCS_REJECT_PROFILE       版本化 profile 串 (如 wbpp_2_9_1)
//
// 双写面 (§30.3 冻结): 每个 image 子产品 properties (文本键, ASTROCS_ 前缀)
// + finalize 写出的 manifest.json (JSON, 键同名小写)。
//
// 通道策略 = 全或无: 未调用本 setter → 五键整体不写 (legacy 产品面, 如 P1);
// 调用后五键必须齐备落盘 —— 参数不合法立即返回非 0 (禁静默缺键/占位值)。
// ═══════════════════════════════════════════════════════════════════════════
AIO_HIPS_EXPORT int aio_hips_set_provenance(
    AioHipsProductSet* ps,
    const char* input_manifest_hash,   // 64 hex; 否则返回 2
    const char* model_hash,            // 64 hex; 否则返回 2
    int uncertainty_available,         // 0/1; §30.1 判定结果
    int weight_mode,                   // 0/1/2
    const char* reject_profile);       // 非空版本化 profile 串

// ═══════════════════════════════════════════════════════════════════════════
// aio_hips_verify_product_set - 产品集双向一致性核验 (HIPS_VERIFY 扩展面)
//
// 断言规则 (全部机器可判; 违反即返回非 0, 报告结构始终填充):
//   V1 signal 子产品存在且 properties 可解析            → 违反 rc=-1
//   V2 provenance 全或无: 五键计数 ∈ {0,5}             → 部分缺键 rc=4
//   V3 uncertainty_available=true  ⇒ variance/ivar 子产品
//      存在且 tile 数 == signal tile 数                → 违反 rc=2
//      uncertainty_available=false ⇒ variance/ivar 子产品不存在 (禁占位) → rc=3
//   V4 manifest.json products 声明与目录事实一致:
//      声明 nrej/nused ⇒ 目录存在且 tile 数 == signal tile 数 → rc=5
//      未声明 ⇒ 目录不存在 (禁占位)                          → rc=6
//   V5 诊断平面值域: 逐 tile 回读 nused/nrej, 负值 = 契约违反 → rc=7
//   V6 properties 与 manifest.json 五键值逐键一致 (双写面禁止分叉) → rc=8
//
// 返回 0 = 全部规则满足 (产品集自洽); 非 0 见上表; 报告结构在返回前填满,
// 供调用方登记而不必重解析。
// ═══════════════════════════════════════════════════════════════════════════
typedef struct {
    int signal_present;             // 1/0
    int n_signal_tiles;             // -1 = 不可得
    int variance_present, ivar_present;
    int n_variance_tiles, n_ivar_tiles;
    int nrej_present, nused_present;
    int nrej_declared, nused_declared;   // manifest.json products 声明
    int n_nrej_tiles, n_nused_tiles;
    int prov_keys_present;          // 0..5 (signal/properties)
    int uncertainty_available;      // -1 未登记 / 0 false / 1 true
    int manifest_keys_present;      // 0..5 (manifest.json provenance 块)
    int diag_negative_pixels;       // 诊断平面负值像素计数 (V5)
    int unreadable_tiles;           // 声明/存在的子产品中不可读的 tile 数 (V4)
    int value_mismatch;             // properties↔manifest 值分叉计数 (V6)
} AioHipsVerifyReport;

AIO_HIPS_EXPORT int aio_hips_verify_product_set(
    const char* out_dir, AioHipsVerifyReport* out);

// 写 SNR Catalogue HiPS 控制点 (finalize 时按 tile 分组写出 NorderK/.../NpixN.tsv)
AIO_HIPS_EXPORT int aio_hips_write_snr_points(
    AioHipsProductSet* ps,
    const AioHipsSnrPoint* pts,
    int n);

// （K_CORR_DOMAIN 选项 B）：设置 Drizzle provenance（pixfrac /
// 像素角尺度），finalize 时写入 properties（ASTROCS_DRIZZLE_PIXFRAC /
// ASTROCS_DRIZZLE_SCALE_ARCSEC）。Phase2 sampler 按帧读取以选择
// control-ivar 的 k_corr 标定值。默认未设置时 properties 不写这两键。
AIO_HIPS_EXPORT int aio_hips_set_drizzle_provenance(
    AioHipsProductSet* ps, double pixfrac, double scale_arcsec);

// 结束产品集: 写 properties/MOC/低阶 hierarchy (从磁盘 leaf tiles 聚合),
// 释放句柄。返回 0=成功。
AIO_HIPS_EXPORT int aio_hips_finalize(AioHipsProductSet* ps);

// 中止: 清理已写部分 (尽力) 并释放句柄
AIO_HIPS_EXPORT int aio_hips_abort(AioHipsProductSet* ps);

// 旧 Tile 结构 (兼容声明, 仅 aio_hips_write 使用)
typedef struct {
    uint64_t parent_ipix;
    uint32_t depth;
    const void* signal;
    const uint8_t* support;
} AioHipsTile;

// 兼容旧接口 (HISS 中转验证用): 全量写信号+support+SNR
AIO_HIPS_EXPORT int aio_hips_write(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    const AioHipsTile* tiles,
    int n_tiles,
    int signal_dtype,
    const AioHipsSnrPoint* snr_points,
    int n_snr,
    const char* creator_did,
    const char* obs_title,
    int moc_order);

// 获取最后错误信息 (线程局部)
AIO_HIPS_EXPORT const char* aio_hips_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // AIO_HIPS_H
