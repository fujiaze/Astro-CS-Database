/* AstroCS P1 calibration 模块静态标识与词表（P1-CAL-IMPL 迁移面）
 *
 * 文件: lib/calibration/include/astrocs/calibration/types.h
 * 对齐先例: lib/gaia_xpsd_client/include/astrocs/gaia/types.h (CAT-GAIA-IMPL)
 *           lib/drizzle/include/astrocs/drizzle/types.h (P1-DRZ-IMPL)
 *
 * 纯宏头: 无任何实现/分配/函数; C11 与 C++17 双可编译。
 * 字符串值与 module.yaml / DLL descriptor 三方一致 (12 §5):
 *   module_id   = astrocs.p1.calibration      (module.yaml module_id 字段)
 *   version     = 0.11.0-alpha.2              (module.yaml module_version, 避免双口径)
 *   abi_version = 1                           (module.yaml abi_version)
 * 合同 ID 三组 (docs/algorithms/CALIBRATION_ALGORITHMS.md §8 冻结):
 *   sci_id = SCI-CAL-001 / alg_id = ALG-CAL-001 / api_id = API-P1-001
 */
#ifndef ASTROCS_CAL_TYPES_H
#define ASTROCS_CAL_TYPES_H

/* ── 模块静态标识 ── */
#define ASTROCS_CAL_MODULE_ID    "astrocs.p1.calibration"
#define ASTROCS_CAL_VERSION      "0.11.0-alpha.2"
#define ASTROCS_CAL_ABI_VERSION  1u
#define ASTROCS_CAL_BUILD_ID     "P1-CAL-IMPL"
#define ASTROCS_CAL_SCI_ID       "SCI-CAL-001"
#define ASTROCS_CAL_ALG_ID       "ALG-CAL-001"
#define ASTROCS_CAL_API_ID       "API-P1-001"

/* plan / config schema 版本 (与 ABI version 分离, 12 §4) */
#define ASTROCS_CAL_PLAN_VERSION       1
#define ASTROCS_CAL_CONFIG_SCHEMA_VER  1

/* ── config 键词表 (validate/plan/create/execute 四操作共享; API-P1-001 §8) ── */
#define ASTROCS_CAL_CFG_KEY_OP                "op"
#define ASTROCS_CAL_CFG_KEY_WIDTH             "width"
#define ASTROCS_CAL_CFG_KEY_HEIGHT            "height"
#define ASTROCS_CAL_CFG_KEY_FRAMES            "frames"
#define ASTROCS_CAL_CFG_KEY_SIGMA_LOW         "sigma_low"
#define ASTROCS_CAL_CFG_KEY_SIGMA_HIGH        "sigma_high"
#define ASTROCS_CAL_CFG_KEY_MAX_ITERATIONS    "max_iterations"
#define ASTROCS_CAL_CFG_KEY_COMBINE           "combine"
#define ASTROCS_CAL_CFG_KEY_DARK_OPTIMIZATION "dark_optimization"
#define ASTROCS_CAL_CFG_KEY_DARK_SCALE_FACTOR "dark_scale_factor"
#define ASTROCS_CAL_CFG_KEY_HOT_SIGMA         "hot_sigma"
#define ASTROCS_CAL_CFG_KEY_COLD_SIGMA        "cold_sigma"
#define ASTROCS_CAL_CFG_KEY_METHOD            "method"
#define ASTROCS_CAL_CFG_KEY_MAX_STRUCTURE     "max_structure_size"
#define ASTROCS_CAL_CFG_KEY_MAX_WORKERS       "max_workers"

/* op 词表: 与 legacy AC_API 科学导出 1:1 (source_symbols 的 10 个科学符号;
 * ac_set_num_threads / ac_version 为工具通道不进 op 面 —— 线程改经 host
 * executor 租约注入, threading_model=host_executor_lease 整改点)。
 * 词表外 op → ACS_ERR_PARAM + ECODE 100。 */
#define ASTROCS_CAL_OP_CALIBRATE_F32   "calibrate_frame"
#define ASTROCS_CAL_OP_CALIBRATE_F64   "calibrate_frame_f64"
#define ASTROCS_CAL_OP_CORRECT_F32     "correct_frame"
#define ASTROCS_CAL_OP_CORRECT_F64     "correct_frame_f64"
#define ASTROCS_CAL_OP_MASTER_BIAS_F32 "generate_master_bias"
#define ASTROCS_CAL_OP_MASTER_DARK_F32 "generate_master_dark"
#define ASTROCS_CAL_OP_MASTER_FLAT_F32 "generate_master_flat"
#define ASTROCS_CAL_OP_MASTER_BIAS_F64 "generate_master_bias_f64"
#define ASTROCS_CAL_OP_MASTER_DARK_F64 "generate_master_dark_f64"
#define ASTROCS_CAL_OP_MASTER_FLAT_F64 "generate_master_flat_f64"

/* manifest 键 (输入/输出; 平面 dtype 冻结 f32|f64 且与 op 精度严格一致,
 * 不做静默转换 —— direct-vs-plugin BITWISE 口径前提) */
#define ASTROCS_CAL_MAN_KEY_SCHEMA      "schema_version"
#define ASTROCS_CAL_MAN_KEY_FRAMES      "frames"
#define ASTROCS_CAL_MAN_KEY_WIDTH       "width"
#define ASTROCS_CAL_MAN_KEY_HEIGHT      "height"
#define ASTROCS_CAL_MAN_KEY_DTYPE       "dtype"
#define ASTROCS_CAL_MAN_KEY_DATA        "data_base64"
#define ASTROCS_CAL_MAN_KEY_MASTER_BIAS "master_bias"
#define ASTROCS_CAL_MAN_KEY_MASTER_DARK "master_dark"
#define ASTROCS_CAL_MAN_KEY_MASTER_FLAT "master_flat"
#define ASTROCS_CAL_MAN_KEY_OUT         "out_base64"
#define ASTROCS_CAL_MAN_KEY_ACTUAL_K    "actual_k"
#define ASTROCS_CAL_MAN_KEY_HOT         "hot"
#define ASTROCS_CAL_MAN_KEY_COLD        "cold"
#define ASTROCS_CAL_MAN_KEY_OP          "op"
#define ASTROCS_CAL_DTYPE_F32           "f32"
#define ASTROCS_CAL_DTYPE_F64           "f64"

/* plan JSON 固定键 */
#define ASTROCS_CAL_PLAN_KEY_VERSION     "plan_version"
#define ASTROCS_CAL_PLAN_KEY_WORK_UNITS  "work_units"
#define ASTROCS_CAL_PLAN_KEY_WORK_UNIT   "work_unit"
#define ASTROCS_CAL_PLAN_KEY_PARALLEL    "parallel_axis"
#define ASTROCS_CAL_PLAN_KEY_MIN_WORKERS "min_workers"
#define ASTROCS_CAL_PLAN_KEY_MAX_WORKERS "max_workers"
#define ASTROCS_CAL_PLAN_KEY_MEMORY      "memory_bytes_estimate"
#define ASTROCS_CAL_PLAN_KEY_IO          "io_bytes_estimate"
#define ASTROCS_CAL_PLAN_KEY_CANCEL      "cancel_support"
#define ASTROCS_CAL_PLAN_KEY_NODE        "node_id"

/* ── ECODE 段 (acs_error_info_v1.detail_code; 通用值 1..7 见 lifecycle_v1.h,
 * 模块自定义从 100 起为冻结规则) ── */
#define ASTROCS_CAL_ECODE_NONE             0u
#define ASTROCS_CAL_ECODE_OP_UNKNOWN       100u  /* config.op 不在词表 */
#define ASTROCS_CAL_ECODE_PARAM_MISSING    101u  /* 必需 config 键缺失 */
#define ASTROCS_CAL_ECODE_PARAM_TYPE       102u  /* 键类型错误 */
#define ASTROCS_CAL_ECODE_PARAM_RANGE      103u  /* 维度<=0 / 非有限数值 */
#define ASTROCS_CAL_ECODE_PLAN_FAIL        104u  /* plan 期元数据不足 */
#define ASTROCS_CAL_ECODE_EXECUTOR_MISSING 105u  /* cpu_heavy 无 executor 注入 */
#define ASTROCS_CAL_ECODE_MANIFEST_MISSING 110u  /* 必需 manifest 字段缺失 */
#define ASTROCS_CAL_ECODE_MANIFEST_DIMS    111u  /* manifest dims/config 不一致 */
#define ASTROCS_CAL_ECODE_B64_INVALID      112u  /* base64 非法 / 解码长度不符 */
#define ASTROCS_CAL_ECODE_LEGACY_REJECT    120u  /* legacy ac_* 返回非 0 */
#define ASTROCS_CAL_ECODE_EXCEPTION        130u  /* DLL 边界捕获 C++ 异常 */

#endif /* ASTROCS_CAL_TYPES_H */
