/* AstroCS catalog gaia service module — 公共类型 (CAT-GAIA-IMPL)
 *
 * 角色: 模块对外只经 module C ABI v1 (include/astrocs/abi/module_api_v1.h)
 * 暴露; 本头仅保留跨边界可复用常量/键名, 不引入任何实现。
 * 冻结规则 (status_codes.h): 纯 POD/宏; 无 STL/异常/RTTI; C11/C++17 双可编译。
 *
 * config JSON 合同 (GAIA_QUERY.md §3.1 迁移合同; module.yaml API-GAIA-001):
 *   {"op":"<ASTROCS_GAIA_OP_*>", "catalog_dir":"<catalog.xpsd_dir port>",
 *    "db_type":0|1|2(可选,默认0=AUTO), "max_workers":N(可选,≤file_count),
 *    ...op 参数(全部有限值; NaN/Inf → ACS_ERR_PARAM, README 已知限制7强化)}
 * op 参数 (GAIA_QUERY.md §1/§2 单位: deg/mag/arcsec, J2000/ICRS):
 *   cone_search:                 ra,dec,radius_deg,mag_low,mag_high
 *   cone_search_for_solver:      ra,dec,radius_deg,mag_high
 *   cone_search_with_spectrum:   ra,dec,radius_deg,mag_low,mag_high
 *   cone_search_with_photometry: ra,dec,radius_deg,mag_low,mag_high
 *   query_spectrum_by_coords:    ra_list[],dec_list[],match_radius_arcsec,
 *                                mag_low,mag_high
 */
#ifndef ASTROCS_GAIA_TYPES_H
#define ASTROCS_GAIA_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 模块静态标识 (module.yaml / DLL descriptor / product manifest 三方一致, 12 §5) */
#define ASTROCS_GAIA_MODULE_ID   "astrocs.catalog.gaia"
#define ASTROCS_GAIA_VERSION     "0.11.0-alpha.2"
#define ASTROCS_GAIA_ABI_VERSION 1u
#define ASTROCS_GAIA_BUILD_ID    "CAT-GAIA-IMPL"

/* 合同 ID (CAT-GAIA-DOC 冻结) */
#define ASTROCS_GAIA_SCI_ID "SCI-AST-001"
#define ASTROCS_GAIA_ALG_ID "ALG-GAIA-001"
#define ASTROCS_GAIA_API_ID "API-GAIA-001"

/* config 键 (validate_config/plan/execute 共享; 测试 host 侧解析) */
#define ASTROCS_GAIA_CFG_KEY_OP           "op"
#define ASTROCS_GAIA_CFG_KEY_CATALOG_DIR  "catalog_dir"
#define ASTROCS_GAIA_CFG_KEY_DB_TYPE      "db_type"
#define ASTROCS_GAIA_CFG_KEY_MAX_WORKERS  "max_workers"
#define ASTROCS_GAIA_CFG_KEY_RA           "ra"
#define ASTROCS_GAIA_CFG_KEY_DEC          "dec"
#define ASTROCS_GAIA_CFG_KEY_RADIUS       "radius_deg"
#define ASTROCS_GAIA_CFG_KEY_MAG_LOW      "mag_low"
#define ASTROCS_GAIA_CFG_KEY_MAG_HIGH     "mag_high"
#define ASTROCS_GAIA_CFG_KEY_RA_LIST      "ra_list"
#define ASTROCS_GAIA_CFG_KEY_DEC_LIST     "dec_list"
#define ASTROCS_GAIA_CFG_KEY_MATCH_RADIUS "match_radius_arcsec"

/* op 词表 (未知 op → PARAM + detail 100) */
#define ASTROCS_GAIA_OP_CONE            "cone_search"
#define ASTROCS_GAIA_OP_CONE_SOLVER     "cone_search_for_solver"
#define ASTROCS_GAIA_OP_CONE_SPEC       "cone_search_with_spectrum"
#define ASTROCS_GAIA_OP_CONE_PHOT       "cone_search_with_photometry"
#define ASTROCS_GAIA_OP_SPEC_BY_COORDS  "query_spectrum_by_coords"

/* plan JSON 版本与键 (work_units 真实推导自叶块统计, 禁空转假 plan) */
#define ASTROCS_GAIA_PLAN_VERSION 1

/* 模块自定义 err.detail_code (lifecycle_v1.h §4: 自定义从 100 起) */
enum {
    ACS_GAIA_ECODE_NONE = 0,
    ACS_GAIA_ECODE_OP_UNKNOWN = 100,        /* op 不在词表 */
    ACS_GAIA_ECODE_CATALOG_DIR_MISSING = 101, /* catalog_dir 缺失/空 */
    ACS_GAIA_ECODE_PARAM_MISSING = 102,     /* op 必需数值参数缺失 */
    ACS_GAIA_ECODE_PARAM_NOT_FINITE = 103,  /* NaN/Inf (README 限制7 强化) */
    ACS_GAIA_ECODE_CATALOG_OPEN_FAIL = 104, /* XPSD 目录打不开/坏数据集 */
    ACS_GAIA_ECODE_EXECUTOR_MISSING = 105   /* 需要 lease 而 host.executor==NULL */
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ASTROCS_GAIA_TYPES_H */
