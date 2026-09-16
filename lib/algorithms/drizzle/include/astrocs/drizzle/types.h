/* types.h - astrocs.p1.drizzle 模块常量与词表 (module ABI v1 迁移面)
 *
 * 对齐先例: lib/gaia_xpsd_client/include/astrocs/gaia/types.h (CAT-GAIA-IMPL,
 * commit babe752d)。本头只承载 C ABI adapter 层的常量/词表/诊断码,
 * 不含任何科学常量 (Eriksson 剖分、面积权重、NaN-Inf 传播语义全部留在
 * drizzle_engine / SCI-DRZ-001, 本迁移 scientific_change=false 零改动)。
 *
 * 合同链: SCI-DRZ-001 -> ALG-DRZ-001 -> API-DRZ-001 (docs/contracts/PUBLIC_API.md)
 *         -> lib/drizzle/module.yaml (module_id=astrocs.p1.drizzle,
 *            dll_name=astrocs_p1_drizzle.dll, threading_model=host_executor_lease)
 */
#ifndef ASTROCS_DRIZZLE_TYPES_H
#define ASTROCS_DRIZZLE_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───────── 模块标识 ───────── */

#define ASTROCS_DRIZZLE_MODULE_ID       "astrocs.p1.drizzle"
#define ASTROCS_DRIZZLE_MODULE_VERSION  1u
#define ASTROCS_DRIZZLE_ABI_VERSION     1u
/* 构建标识: 迁移任务 P1-DRZ-IMPL (独立 DLL 化, 不改科学域) */
#define ASTROCS_DRIZZLE_BUILD_ID        "p1-drz-impl-2026-09-02"

#define ASTROCS_DRIZZLE_SCI_ID  "SCI-DRZ-001"
#define ASTROCS_DRIZZLE_ALG_ID  "ALG-DRZ-001"
#define ASTROCS_DRIZZLE_API_ID  "API-DRZ-001"

/* config schema 版本 (词表见 CFG_KEY_*; v1 冻结) */
#define ASTROCS_DRIZZLE_CONFIG_SCHEMA_VER 1u

/* plan 输出词表版本 (词表演进必须升版本号) */
#define ASTROCS_DRIZZLE_PLAN_VERSION    1u

/* ───────── op 词表 (config "op"; v1) ─────────
 * execute 线程内单帧通道 (PipelineFrame "data" 块 -> HiPS/legacy .hiss),
 * 输入经 manifest JSON 承载 (typed artifact 输入, base64 行数据):
 *   op=drizzle        帧通道: 单帧 [H,W] f32/f64 -> hp_drizzle_run_hips
 *                     (事务 sink 输出: HiPS 产品集 + signal_plane_base64 诊断回传)
 *   op=reverse        球面 -> 平面反向投递: hp_drizzle_reverse_run
 *                     (输出行数据 base64; 六导出中仅这两个在 v1 模块面)
 * 其余四导出 (fits_to_ahpx / reverse_capability / reverse_version) 为
 * 工具通道, 不在模块 v1 op 面 (API-DRZ-001 已冻结; 迁移不裁剪 legacy 导出)。 */
#define ASTROCS_DRIZZLE_OP_DRIZZLE   "drizzle"
#define ASTROCS_DRIZZLE_OP_REVERSE   "reverse"

/* ───────── config 键词表 (CFG_KEY; v1) ───────── */

#define DRZ_CFG_KEY_OP             "op"
#define DRZ_CFG_KEY_NSIDE          "nside"              /* u32, 2 的幂, >=1 */
#define DRZ_CFG_KEY_NESTED         "nested"             /* 0=RING 1=NESTED */
#define DRZ_CFG_KEY_PIXFRAC        "pixfrac"            /* f64 [0,1] (DISP-DRZ-003 口径) */
#define DRZ_CFG_KEY_PRECISION      "precision_mode"     /* 0=FP32 1=FP64 -1=auto(header) */
#define DRZ_CFG_KEY_OUTPUT_PATH    "output_path"        /* legacy .hiss 路径 (可空) */
#define DRZ_CFG_KEY_HIPS_DIR       "hips_dir"           /* HiPS 产品集根目录 */
#define DRZ_CFG_KEY_MAX_WORKERS    "max_workers"        /* u32, 租借上限 (0=不设限) */

/* reverse op 专属 (球面 -> 平面) */
#define DRZ_CFG_KEY_REV_WIDTH      "width"              /* u32 输出平面宽 */
#define DRZ_CFG_KEY_REV_HEIGHT     "height"             /* u32 输出平面高 */
#define DRZ_CFG_KEY_REV_SCALE      "pixel_scale_arcsec" /* f64 像元尺度 */
#define DRZ_CFG_KEY_REV_RA0        "ra_center_deg"      /* f64 中心 RA */
#define DRZ_CFG_KEY_REV_DEC0       "dec_center_deg"     /* f64 中心 Dec */
#define DRZ_CFG_KEY_REV_PROJ       "projection"         /* 字符串: TAN/SIN/ARC */

/* ───────── manifest 输出键 (execute 输出 JSON; v1) ───────── */

#define DRZ_OUT_KEY_OP             "op"
#define DRZ_OUT_KEY_NSIDE          "nside"
#define DRZ_OUT_KEY_NESTED         "nested"
#define DRZ_OUT_KEY_PIXFRAC        "pixfrac"
#define DRZ_OUT_KEY_PRECISION      "precision_mode"
#define DRZ_OUT_KEY_N_HEALPIX      "n_healpix_pixels"
#define DRZ_OUT_KEY_N_SOURCE       "n_source_pixels"
#define DRZ_OUT_KEY_N_TILES        "n_tiles"
#define DRZ_OUT_KEY_HIPS_DIR       "hips_dir"
#define DRZ_OUT_KEY_HISS_PATH      "legacy_hiss_path"
#define DRZ_OUT_KEY_ARTIFACTS      "artifacts"           /* 事务 sink 产物 URI 数组 */
#define DRZ_OUT_KEY_WORKERS        "workers"             /* 实际租借 worker 数 */

/* ───────── 诊断 detail_code (自定义从 100 起; ABI-002) ─────────
 * 通用域用 ACS_DIAG_ECODE_* (1..7); 模块专属从 100 起。 */

typedef enum {
    /* config (domain=CONFIG) */
    DRZ_ECODE_MISSING_FIELD    = 100, /* 必填 config 键缺失 */
    DRZ_ECODE_BAD_VALUE        = 101, /* 值非法 (非有限/越界/非 2 幂) */
    DRZ_ECODE_UNKNOWN_OP       = 102, /* op 不在词表 */
    /* input manifest (domain=DATA) */
    DRZ_ECODE_MANIFEST_FIELD   = 110, /* manifest 必填字段缺失/非法 */
    DRZ_ECODE_DATA_DECODE      = 111, /* base64 解码失败 / 长度失配 */
    DRZ_ECODE_FRAME_BUILD      = 112, /* PipelineFrame 构造失败 (host io 不可用) */
    /* execute (domain=SCIENCE_PRECONDITION) */
    DRZ_ECODE_LEGACY_REJECT    = 120, /* legacy 错误码转发 (负值域映射, 见 message) */
    /* reverse (domain=DATA) */
    DRZ_ECODE_REV_SHAPE        = 130  /* 输入像素数 != width*height */
} drz_ecode;

/* ───────── legacy 错误码 (API-DRZ-001 冻结, 只转发不改) ─────────
 * hp_drizzle_run 帧通道: 负值 -1..-13 (0=成功);
 * hp_drizzle_reverse_run: 正值 1..7 (0=成功)。
 * adapter 在 ACS_ERR_DOMAIN_SCIENCE_PRECONDITION 下原样记录进
 * err->message ("legacy_code=%d"), 不重解释 (DISP-DRZ 缺陷登记在案,
 * 本迁移不消化: scientific_change=false)。 */

#ifdef __cplusplus
}
#endif

#endif /* ASTROCS_DRIZZLE_TYPES_H */
