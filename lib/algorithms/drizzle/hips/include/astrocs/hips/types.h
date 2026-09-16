/* types.h - astrocs.p1.hips_writer 模块常量与词表 (module ABI v1 迁移面)
 *
 * 对齐先例: lib/gaia_xpsd_client/include/astrocs/gaia/types.h (CAT-GAIA-IMPL,
 * babe752d) 与 lib/drizzle/include/astrocs/drizzle/types.h (P1-DRZ-IMPL,
 * 2c065ace)。本头只承载 C ABI adapter 层的常量/词表/诊断码, 不含任何科学
 * 常量 (surface brightness=flux_sum/covered_area、support=covered_area/A_cell、
 * variance=var_num_sum/covered_area²、MOC UNIQ=4·4^m+(c>>2(K−m)) 等公式全部
 * 留在 aio_hips_writer.cpp / ALG-HIPS-001..005, 本迁移 scientific_change=false
 * 生产源零改动)。
 *
 * 合同链: SCI-DRZ-001 -> ALG-HIPS-001..005 -> API-HIPS-001
 *         (docs/contracts/PUBLIC_API.md) -> lib/hips/module.yaml
 *         (module_id=astrocs.p1.hips_writer, dll_name=astrocs_p1_hips_writer.dll,
 *          threading_model=host_executor_lease, determinism=fixed_reduction_order)
 */
#ifndef ASTROCS_HIPS_TYPES_H
#define ASTROCS_HIPS_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───────── 模块标识 ───────── */

#define ASTROCS_HIPS_MODULE_ID       "astrocs.p1.hips_writer"
#define ASTROCS_HIPS_MODULE_VERSION  1u
#define ASTROCS_HIPS_ABI_VERSION     1u
/* 构建标识: 迁移任务 P1-HIPS-IMPL (独立 DLL 化, 不改科学域) */
#define ASTROCS_HIPS_BUILD_ID        "p1-hips-impl-2026-09-07"

/* SCI-DRZ-001 为共享引用 (module.yaml science_contracts[0]; 本模块不持
 * 独立 SCI); ALG 面 001..005 中 describe 的 alg_id 槽承载主公式面
 * ALG-HIPS-001 (产品语义), 002..005 同链 (ALG-HIPS-002 variance/ivar,
 * 003 hierarchy 聚合, 004 MOC/UNIQ, 005 SNR Catalogue)。 */
#define ASTROCS_HIPS_SCI_ID  "SCI-DRZ-001"
#define ASTROCS_HIPS_ALG_ID  "ALG-HIPS-001"
#define ASTROCS_HIPS_API_ID  "API-HIPS-001"

/* config schema 版本 (词表见 HIPS_CFG_KEY_*; v1 冻结) */
#define ASTROCS_HIPS_CONFIG_SCHEMA_VER 1u

/* plan 输出词表版本 (词表演进必须升版本号) */
#define ASTROCS_HIPS_PLAN_VERSION 1u

/* ───────── op 词表 (config "op"; v1) ─────────
 * execute 单事务通道: manifest 携带叶级 tile 行数据 + SNR 点集 + provenance,
 * 一次 execute 完成 product_begin -> 逐 tile write_signal_support_tile /
 * write_variance_tile -> write_snr_points -> set_drizzle_provenance ->
 * aio_hips_finalize 的完整 HiPS 产品集 (IO-003 发布层对齐属 P1-HIPS-INT)。
 * legacy aio_hips_write (aio_hips.h:163, HISS 中转验证用批量兼容通道, 旧
 * support uint8 0..255 语义映射) 不进模块 v1 op 面: 九导出仍全量编译进
 * DLL 且 legacy 导出面经 version-script/DEF 降 local (ABI-006), ABI 合同
 * 不裁剪; 模块事务面只承载生产链 (astro_sphere_sink.cpp:52-167 同构序列)。 */
#define ASTROCS_HIPS_OP_WRITE_PRODUCT "write_product"

/* ───────── config 键词表 (HIPS_CFG_KEY; v1) ─────────
 * product_begin 参数在 create 期固化 (config), tile 行数据在 execute 期
 * 经 manifest 承载 (typed artifact 输入, inline base64 平面)。 */

#define HIPS_CFG_KEY_OP          "op"
#define HIPS_CFG_KEY_OUT_DIR     "out_dir"         /* 事务 sink 根目录 (必填) */
#define HIPS_CFG_KEY_NSIDE       "nside"           /* u32, 2 的幂, >=512 */
#define HIPS_CFG_KEY_TILE_WIDTH  "tile_width"      /* u32, 恒 512 (HiPS 标准) */
#define HIPS_CFG_KEY_DATA_TYPE   "data_type"       /* 0=AIO_HIPS_FLOAT32 1=FLOAT64 */
#define HIPS_CFG_KEY_FLAGS       "flags"           /* AioHipsProductFlag 位或 */
#define HIPS_CFG_KEY_CREATOR_DID "creator_did"     /* 缺省 ivo://astrocs/phase1 */
#define HIPS_CFG_KEY_OBS_TITLE   "obs_title"       /* 缺省 "AstroCS Phase1" */
#define HIPS_CFG_KEY_OBS_FILTER  "obs_filter"      /* 可空 */
#define HIPS_CFG_KEY_EXPOSURE_S  "exposure_s"      /* f64 秒, >=0 */
#define HIPS_CFG_KEY_OBS_DATE    "obs_date"        /* ISO 字符串, 可空 */
#define HIPS_CFG_KEY_MOC_ORDER   "moc_order"       /* u32, 0=auto(=tile order) */
#define HIPS_CFG_KEY_MAX_WORKERS "max_workers"     /* u32 租约上限 (0=不设限) */

/* ───────── manifest 输入键 (execute 输入 JSON; 顶层平铺 v1) ─────────
 * 多 tile 平面按 tile 序拼接 (每 tile width² 元素, native 字节序 base64,
 * 与 drizzle reverse 通道 leaf_*_base64 约定一致); per-tile 可选性经
 * u8 位图承载 (1=该 tile 提供该平面)。 */

#define HIPS_M_KEY_N_TILES       "n_tiles"              /* u32, >=0 */
#define HIPS_M_KEY_LEAF_ORDER    "leaf_order"           /* u32, ==log2(nside) */
#define HIPS_M_KEY_WIDTH         "width"                /* u32, ==tile_width */
#define HIPS_M_KEY_PARENT        "parent_ipix"          /* u64×N JSON 数组 */
#define HIPS_M_KEY_FLUX_B64      "flux_base64"          /* dtype×N×W² */
#define HIPS_M_KEY_COVERAGE_B64  "coverage_base64"      /* dtype×N×W² */
#define HIPS_M_KEY_HAS_MASK_B64  "has_mask_base64"      /* u8×N 位图 (可缺省=无) */
#define HIPS_M_KEY_MASK_B64      "valid_mask_base64"    /* u8×N×W² (位图非全 0 时必填) */
#define HIPS_M_KEY_HAS_VAR_B64   "has_variance_base64"  /* u8×N 位图 (可缺省=无) */
#define HIPS_M_KEY_VARNUM_B64    "var_num_base64"       /* dtype×N×W² (位图非全 0 时必填) */
#define HIPS_M_KEY_SNR_COUNT     "snr_count"            /* u32, >=0 */
#define HIPS_M_KEY_SNR_RA_B64    "snr_ra_base64"        /* f64×M */
#define HIPS_M_KEY_SNR_DEC_B64   "snr_dec_base64"       /* f64×M */
#define HIPS_M_KEY_SNR_VAL_B64   "snr_val_base64"       /* f64×M */
#define HIPS_M_KEY_SNR_ID_B64    "snr_star_id_base64"   /* i64×M */
#define HIPS_M_KEY_SNR_QF_B64    "snr_qflags_base64"    /* u32×M */
#define HIPS_M_KEY_SNR_ST_B64    "snr_status_base64"    /* u32×M */
#define HIPS_M_KEY_PROV_PIXFRAC  "prov_pixfrac"         /* f64 (0,1] 可选 */
#define HIPS_M_KEY_PROV_SCALE    "prov_scale_arcsec"    /* f64 >=0 可选 */

/* ───────── 输出 manifest 键 (execute 输出 JSON; v1) ───────── */

#define HIPS_O_KEY_OP            "op"
#define HIPS_O_KEY_NSIDE         "nside"
#define HIPS_O_KEY_TILE_WIDTH    "tile_width"
#define HIPS_O_KEY_DATA_TYPE     "data_type"
#define HIPS_O_KEY_FLAGS         "flags"
#define HIPS_O_KEY_N_TILES       "n_tiles"              /* 输入 tile 数 */
#define HIPS_O_KEY_N_WRITTEN     "n_tiles_written"      /* signal/support 写入成功数 */
#define HIPS_O_KEY_N_VAR         "n_tiles_variance"     /* variance 写入成功数 */
#define HIPS_O_KEY_N_SNR         "n_snr_points"
#define HIPS_O_KEY_PROV_SET      "drizzle_provenance_set"
#define HIPS_O_KEY_WORKERS       "workers"              /* 实际租借 worker 数 (==1) */
#define HIPS_O_KEY_OUT_DIR       "out_dir"
#define HIPS_O_KEY_ARTIFACTS     "artifacts"            /* 实测存在的事务 sink 产物 */

/* ───────── 诊断 detail_code (自定义从 100 起; ABI-002) ─────────
 * 通用域用 ACS_DIAG_ECODE_* (1..7); 模块专属从 100 起。 */

typedef enum {
    /* config (domain=CONFIG) */
    HIPS_ECODE_MISSING_FIELD = 100,  /* 必填 config 键缺失 */
    HIPS_ECODE_BAD_VALUE     = 101,  /* 值非法 (非有限/越位/非 2 幂/tile_width!=512) */
    HIPS_ECODE_UNKNOWN_OP    = 102,  /* op 不在词表 */
    /* manifest (domain=DATA) */
    HIPS_ECODE_MANIFEST_FIELD = 110, /* manifest 必填字段缺失/非法/长度失配 */
    HIPS_ECODE_DATA_DECODE    = 111, /* base64 解码失败 */
    /* execute: legacy 错误码转发 (逐 call_site 域映射见 module_entry.cpp
     * hips_legacy_status; code 原样进 message "legacy_code=%d", 不重解释
     * 科学语义 —— DISP-HIPS-007 错误码无集中枚举为登记缺陷, 本迁移不消化) */
    HIPS_ECODE_LEGACY_REJECT = 120,
    HIPS_ECODE_BEGIN_REJECT  = 121,  /* aio_hips_product_begin 返回 NULL */
    /* publish (domain=IO; AIO-002 原子发布面) */
    HIPS_ECODE_PUBLISH_STAGE  = 122, /* staging 目录建立失败 */
    HIPS_ECODE_PUBLISH_REJECT = 123  /* fsync/promote 失败 (staging 已丢弃,
                                      * 目标根无 partial) */
} hips_ecode;

/* legacy 错误码语义 (API-HIPS-001 冻结, README §5/§6 核对):
 *   aio_hips_product_begin: NULL + last_error (nside<512/tile_width!=512/
 *     dtype∉{0,1}/flags 越位, writer.cpp:399-404) —— config 层等价校验前置拦截;
 *   aio_hips_write_signal_support_tile: -1 null / -2 视图不匹配 / -3 越界 /
 *     -4 signal FITS / -5 support FITS;
 *   aio_hips_write_variance_tile: -1 null / -2 var_num 缺失 / -3 不匹配 /
 *     -4 越界 / -5 全无效 / -6/-7 FITS;
 *   aio_hips_write_snr_points: -1 null;
 *   aio_hips_set_drizzle_provenance: 1 null / 2 值域 (pixfrac∈(0,1], scale>=0);
 *   aio_hips_finalize: -1 null / -2 重复 / -3..-8 子产品失败;
 *   aio_hips_abort / aio_hips_last_error: 无负值合同。
 * adapter 在对应 ACS_ERR_DOMAIN_* 下原样记录进 err->message, 不重解释。 */

#ifdef __cplusplus
}
#endif

#endif /* ASTROCS_HIPS_TYPES_H */
