/* types.h - astrocs.p1.cosmetic 模块常量与词表 (module ABI v1 迁移面)
 *
 * 对齐先例: lib/calibration/include/astrocs/calibration/types.h (P1-CAL-IMPL,
 * commit adf820ac) / lib/drizzle/include/astrocs/drizzle/types.h
 * (P1-DRZ-IMPL, commit 2c065ace) / lib/gaia_xpsd_client (CAT-GAIA-IMPL,
 * commit babe752d)。
 *
 * 纯宏头: 无任何实现/分配/函数; C11 与 C++17 双可编译。
 * 字符串值与 module.yaml / DLL descriptor 三方一致 (12 §5):
 *   module_id   = astrocs.p1.cosmetic         (module.yaml module_id 字段)
 *   version     = 0.11.0-alpha.2              (module.yaml module_version, 避免双口径)
 *   abi_version = 1                           (module.yaml abi_version)
 * 合同 ID 三组 (docs/algorithms/COSMETIC_ALGORITHMS.md §8 冻结;
 * docs/contracts/PUBLIC_API.md API-COS-001):
 *   sci_id = SCI-CAL-001 / alg_id = ALG-COS-001 / api_id = API-COS-001
 */
#ifndef ASTROCS_COS_TYPES_H
#define ASTROCS_COS_TYPES_H

/* ── 模块静态标识 ── */
#define ASTROCS_COS_MODULE_ID    "astrocs.p1.cosmetic"
#define ASTROCS_COS_VERSION      "0.11.0-alpha.2"
#define ASTROCS_COS_ABI_VERSION  1u
#define ASTROCS_COS_BUILD_ID     "P1-COS-IMPL"
#define ASTROCS_COS_SCI_ID       "SCI-CAL-001"
#define ASTROCS_COS_ALG_ID       "ALG-COS-001"
#define ASTROCS_COS_API_ID       "API-COS-001"

/* plan / config schema 版本 (与 ABI version 分离, 12 §4) */
#define ASTROCS_COS_PLAN_VERSION       1
#define ASTROCS_COS_CONFIG_SCHEMA_VER  1

/* ── config 键词表 (validate/plan/create/execute 四操作共享; API-COS-001) ── */
#define ASTROCS_COS_CFG_KEY_OP                "op"
#define ASTROCS_COS_CFG_KEY_WIDTH             "width"
#define ASTROCS_COS_CFG_KEY_HEIGHT            "height"
#define ASTROCS_COS_CFG_KEY_HOT_SIGMA         "hot_sigma"
#define ASTROCS_COS_CFG_KEY_COLD_SIGMA        "cold_sigma"
#define ASTROCS_COS_CFG_KEY_METHOD            "method"
#define ASTROCS_COS_CFG_KEY_MAX_STRUCTURE     "max_structure_size"
#define ASTROCS_COS_CFG_KEY_MAX_WORKERS       "max_workers"

/* op 词表: 与 cosmetic 域 legacy AC_API 科学导出 1:1 (source_symbols 中
 * ac_correct_frame / ac_correct_frame_f64 两个科学符号; ac_set_num_threads
 * 为工具通道不进 op 面 —— 线程改经 host executor 租约注入,
 * threading_model=host_executor_lease 整改点 DISP-COS-008)。
 * 词表外 op → ACS_ERR_PARAM + ECODE 100。 */
#define ASTROCS_COS_OP_CORRECT_F32     "correct_frame"
#define ASTROCS_COS_OP_CORRECT_F64     "correct_frame_f64"

/* method 词表 (config "method"; 精确字符串匹配, 不截断):
 *   "median"   → AC_METHOD_MEDIAN   (0)
 *   "bilinear" → AC_METHOD_BILINEAR (1; 名义 bilinear 实为 4 方向
 *                 1/dist IDW, DISP-COS-003 冻结现状行为保持)
 * 其余字符串 → PARAM + 103 (adapter 词表层拒绝; legacy 对任意非 0 整数
 * 走 IDW 无校验 —— DISP-COS-003 的 adapter 面收敛, 科学面零改动)。 */
#define ASTROCS_COS_METHOD_MEDIAN    "median"
#define ASTROCS_COS_METHOD_BILINEAR  "bilinear"

/* ── manifest 键词表 (execute 输入; DATA-P1-COS inline base64 平面口径,
 * CAL-IMPL 同款; 全部键扁平标量, 禁嵌套/数组) ── */
#define ASTROCS_COS_MAN_KEY_SCHEMA_VER  "schema_version"
#define ASTROCS_COS_MAN_KEY_WIDTH       "width"
#define ASTROCS_COS_MAN_KEY_HEIGHT      "height"
#define ASTROCS_COS_MAN_KEY_DTYPE       "dtype"
#define ASTROCS_COS_MAN_KEY_DATA        "data_base64"
#define ASTROCS_COS_MAN_KEY_MASTER_DARK "master_dark"
#define ASTROCS_COS_MAN_KEY_MASTER_BIAS "master_bias"
/* 输出 manifest 键: schema_version/width/height/dtype/out_base64/op/hot/cold */

/* ── plan 词表 ── */
#define ASTROCS_COS_PLAN_AXIS_PIXEL     "pixel"

#endif /* ASTROCS_COS_TYPES_H */
