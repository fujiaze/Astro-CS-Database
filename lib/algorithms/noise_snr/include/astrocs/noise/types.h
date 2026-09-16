/* types.h - astrocs.p1.noise 模块常量与词表 (module ABI v1 迁移面)
 *
 * 对齐先例: lib/algorithms/drizzle/hips/include/astrocs/hips/types.h (P1-HIPS-IMPL, 1959dc89)、
 *           lib/algorithms/cosmetic / lib/algorithms/calibration 同构 (P1-COS-IMPL 948dfcba /
 *           P1-CAL-IMPL adf820ac)。本头只承载 C ABI adapter 层的常量/词表/
 *           诊断码, 不含任何科学常量 (1.482602218505602·MAD robust sigma、
 *           5σ cosmic 裁剪、variance_floor 默认 1e-12、kTrimMeanToSigma 等公式
 *           全部留在生产源 lib/algorithms/noise_snr/cpp/src/noise_model.cpp 与
 *           ALG-NOISE-001..003, 本迁移 scientific_change=false 生产源零改动)。
 *
 * 合同链: SCI-NOISE-001..015 (NOISE_MODEL.md FROZEN T104 2026-08-23)
 *         -> ALG-NOISE-001..003 (NOISE_ESTIMATION.md §13)
 *         -> API-NOISE-001 (docs/contracts/PUBLIC_API.md)
 *         -> lib/algorithms/noise_snr/module.yaml
 *            (module_id=astrocs.p1.noise, dll_name=astrocs_p1_noise.dll,
 *             threading_model=host_executor_lease,
 *             determinism=fixed_reduction_order)
 */
#ifndef ASTROCS_NOISE_TYPES_H
#define ASTROCS_NOISE_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───────── 模块标识 ───────── */

#define ASTROCS_NOISE_MODULE_ID       "astrocs.p1.noise"
#define ASTROCS_NOISE_MODULE_VERSION  1u
#define ASTROCS_NOISE_ABI_VERSION     1u
/* 构建标识: 迁移任务 P1-NOISE-IMPL (独立 DLL 化, 不改科学域) */
#define ASTROCS_NOISE_BUILD_ID        "p1-noise-impl-2026-09-09"

/* module.yaml module_id=astrocs.p1.noise (矩阵行 P1-NOISE); SCI 面
 * SCI-NOISE-001..015 为 module.yaml science_contracts[0] 段, describe 的
 * sci_id 槽承载主公式面 SCI-NOISE-001 (blank-sky 稳健方差, 与 HIPS 单槽
 * 模式一致); ALG 面 001..003 中 alg_id 槽承载 ALG-NOISE-001 (产品语义),
 * 002 (fill/free/scale_law) / 003 (gain 诊断) 经 op 词表同链。 */
#define ASTROCS_NOISE_SCI_ID  "SCI-NOISE-001"
#define ASTROCS_NOISE_ALG_ID  "ALG-NOISE-001"
#define ASTROCS_NOISE_API_ID  "API-NOISE-001"

/* config schema 版本 (词表见 NOISE_CFG_KEY_*; v1 冻结) */
#define ASTROCS_NOISE_CONFIG_SCHEMA_VER 1u

/* plan 输出词表版本 (词表演进必须升版本号) */
#define ASTROCS_NOISE_PLAN_VERSION 1u

/* ───────── op 词表 (config "op"; v1) ─────────
 * 与 API-NOISE-001 七导出映射 (noise_model.cpp 实现面):
 *   estimate_noise_model = snr_noise_model_v1 / snr_noise_model_v1_f64
 *     (+ 可选内联 snr_noise_model_v1_fill; 调用时序 build → fill → free
 *      为 README §5 冻结单事务, g_model_floor 注册表在事务内全程有效,
 *      fill 使用真实 build floor —— 与 direct 通道 bitwise 一致) +
 *     事务收尾 snr_noise_model_v1_free (DISP-NOISE-009 注册表泄漏防线)。
 *   fill_noise_field     = snr_noise_model_v1_fill 独立通道 (模型自
 *     estimate_noise_model 输出 manifest round-trip 重建; 影子实例不在
 *     g_model_floor 注册表 → floor 回退 1e-12, DISP-NOISE-002 build/fill
 *     floor 语义不一致的跨 ABI 忠实现状, 如实登记不消化)。
 *   noise_diagnostic     = snr_noise_scale_law + snr_noise_gain_variance
 *     标量诊断 (ALG-NOISE-002/003; SNR-002 传播法则 / SNR-005 Poisson+read
 *     诊断; tiny 串行, plan 标 serial, 不入 heavy 并行轴)。
 * legacy 乘法 SNR 通道 (snr_estimate 星号族 / snr_extract_model 星号族,
 * snr_estimator.h :19-20 已降级 legacy diagnostic) 与测光/PSF 质量符号
 * (snr_phot_cal_quality / snr_psf_fit_quality, P1-PHOT/P1-PSF 合同视角)
 * 不进本模块 op 面; 七个 noise 导出符号仍全量编译进 DLL 且 legacy 导出面
 * 经 version-script/DEF 降 local (ABI-006), ABI 合同不裁剪。 */
#define ASTROCS_NOISE_OP_ESTIMATE  "estimate_noise_model"
#define ASTROCS_NOISE_OP_FILL      "fill_noise_field"
#define ASTROCS_NOISE_OP_DIAG      "noise_diagnostic"

/* ───────── config 键词表 (NOISE_CFG_KEY; v1; 顶层平铺) ─────────
 * 形状/参数在 config (plan 期可推导 work_units/memory/io, 无魔数),
 * 数据平面在 execute manifest (typed artifact inline base64 平面,
 * native 字节序, 与 hips/drizzle 约定一致)。 */

#define NOISE_CFG_KEY_OP            "op"
#define NOISE_CFG_KEY_DTYPE         "dtype"               /* 0=f32 1=f64 (build) */
#define NOISE_CFG_KEY_H             "h"                   /* u64 >=1 (build/fill) */
#define NOISE_CFG_KEY_W             "w"                   /* u64 >=1 (build/fill) */
#define NOISE_CFG_KEY_FILL_VARIANCE "fill_variance"       /* 0/1 (build 内联 fill 输出) */
#define NOISE_CFG_KEY_FILL_IVAR     "fill_ivar"           /* 0/1 (build 内联 fill 输出) */
/* SnrNoiseModelConfig 字段键 (可选; 缺省=生产 snr_noise_model_v1_default_config
 * 逐字段默认, 与生产 cfg=NULL 路径 bitwise 等价; 提供“部分键”时其余字段取
 * 生产默认值, 静默钳位语义归生产实现 —— DISP-NOISE-007 现状不新增校验) */
#define NOISE_CFG_KEY_PATCH_GX      "patch_grid_x"
#define NOISE_CFG_KEY_PATCH_GY      "patch_grid_y"
#define NOISE_CFG_KEY_MASK_R0       "source_mask_radius_px"
#define NOISE_CFG_KEY_MASK_SCALE    "mask_radius_scale"
#define NOISE_CFG_KEY_GAIN          "gain_e_per_adu"
#define NOISE_CFG_KEY_READNOISE     "read_noise_e"
#define NOISE_CFG_KEY_SATURATION    "saturation_level"
#define NOISE_CFG_KEY_CLIP_SIGMA    "cosmic_clip_sigma"
#define NOISE_CFG_KEY_MIN_SAMPLES   "min_patch_samples"
#define NOISE_CFG_KEY_CLIP_ROUNDS   "max_clip_rounds"
#define NOISE_CFG_KEY_USE_GAIN_MODEL "use_gain_model"
#define NOISE_CFG_KEY_SPATIAL_FIELD "enable_spatial_field"
#define NOISE_CFG_KEY_VARIANCE_FLOOR "variance_floor"
#define NOISE_CFG_KEY_MAX_WORKERS   "max_workers"         /* u32 租约上限 (0=不设限; 回显) */
/* diagnostic op 专属 */
#define NOISE_CFG_KEY_DIAG_SUBOP    "subop"               /* "scale_law"|"gain_variance" */
#define NOISE_CFG_KEY_ALPHA         "alpha"               /* scale_law */
#define NOISE_CFG_KEY_VARIANCE      "variance"            /* scale_law 输入 (可省略=NULL 语义) */
#define NOISE_CFG_KEY_IVAR          "ivar"                /* scale_law 输入 (可省略=NULL 语义) */
#define NOISE_CFG_KEY_SIGNAL        "signal"              /* gain_variance */
/* read_noise_e 键复用 NOISE_CFG_KEY_READNOISE (cfg 与 gain_variance 诊断同名同义) */

/* ───────── manifest 输入键 (execute 输入 JSON; 顶层平铺 v1) ─────────
 * 平面按 native 字节序 base64 (与 hips leaf_*_base64 / drizzle 约定一致)。 */

#define NOISE_M_KEY_DATA_B64        "data_base64"         /* f32/f64 × h·w (build 必填) */
#define NOISE_M_KEY_MASK_B64        "source_mask_base64"  /* f32 × h·w (可选; ≠0=源;
                                                           DISP-NOISE-006: 掩膜非 NULL
                                                           忽略 star 通道, 现状直传) */
#define NOISE_M_KEY_STAR_X_B64      "star_x_base64"       /* f64 × N (可选) */
#define NOISE_M_KEY_STAR_Y_B64      "star_y_base64"       /* f64 × N (可选; 与 X 等长) */
#define NOISE_M_KEY_N_STARS         "n_stars"             /* u64 (star 平面提供时必填;
                                                           缺省=平面长度) */
/* fill_noise_field op: 模型 round-trip 字段 (estimate_noise_model 输出) */
#define NOISE_M_KEY_N_CTRL          "n_control_points"    /* u64 */
#define NOISE_M_KEY_CTRL_X_B64      "ctrl_x_base64"       /* f64 × n */
#define NOISE_M_KEY_CTRL_Y_B64      "ctrl_y_base64"       /* f64 × n */
#define NOISE_M_KEY_CTRL_SIG_B64    "ctrl_sigma_base64"   /* f64 × n */
#define NOISE_M_KEY_CTRL_VAR_B64    "ctrl_variance_base64"/* f64 × n */
#define NOISE_M_KEY_CTRL_IVAR_B64   "ctrl_ivar_base64"    /* f64 × n */
#define NOISE_M_KEY_SIGMA_BG        "sigma_bg_global"     /* f64 */
#define NOISE_M_KEY_VARIANCE_BG     "variance_bg_global"  /* f64 */
#define NOISE_M_KEY_IVAR_BG         "ivar_bg_global"      /* f64 */
#define NOISE_M_KEY_SOURCE          "source"              /* u8 (round-trip) */
#define NOISE_M_KEY_HAS_SPATIAL     "has_spatial_field"   /* u8 (round-trip) */
#define NOISE_M_KEY_DEGENERATE      "degenerate"          /* u8 (round-trip) */

/* ───────── 输出 manifest 键 (execute 输出 JSON; v1) ───────── */

#define NOISE_O_KEY_OP              "op"
#define NOISE_O_KEY_RC              "rc"                  /* 生产返回码 0=成功(含退化兜底)
                                                           1=完全退化 (DATA_SEMANTICS §4a:
                                                           ivar_bg_global==0 显式不可用;
                                                           ACS_OK 语义, 科学结果非故障) */
#define NOISE_O_KEY_DTYPE           "dtype"
#define NOISE_O_KEY_H               "h"
#define NOISE_O_KEY_W               "w"
#define NOISE_O_KEY_N_CTRL          "n_control_points"
#define NOISE_O_KEY_N_QUALIFIED     "n_qualified_patches"
#define NOISE_O_KEY_N_REJECTED      "n_rejected_patches"  /* 空 patch 与质量拒绝混计
                                                           (DISP-NOISE-008 现状直传) */
#define NOISE_O_KEY_SIGMA_BG        "sigma_bg_global"
#define NOISE_O_KEY_VARIANCE_BG     "variance_bg_global"
#define NOISE_O_KEY_IVAR_BG         "ivar_bg_global"
#define NOISE_O_KEY_SOURCE          "source"
#define NOISE_O_KEY_HAS_SPATIAL     "has_spatial_field"
#define NOISE_O_KEY_DEGENERATE      "degenerate"
#define NOISE_O_KEY_VARIANCE_FLOOR  "variance_floor"      /* 回显 (config); DISP-NOISE-002
                                                           登记: 独立 fill 通道 DLL 内
                                                           注册表不可达 → 回退 1e-12 */
#define NOISE_O_KEY_CTRL_X_B64      "ctrl_x_base64"
#define NOISE_O_KEY_CTRL_Y_B64      "ctrl_y_base64"
#define NOISE_O_KEY_CTRL_SIG_B64    "ctrl_sigma_base64"
#define NOISE_O_KEY_CTRL_VAR_B64    "ctrl_variance_base64"
#define NOISE_O_KEY_CTRL_IVAR_B64   "ctrl_ivar_base64"
#define NOISE_O_KEY_OUT_VAR_B64     "out_variance_base64" /* f32 × h·w (fill 输出) */
#define NOISE_O_KEY_OUT_IVAR_B64    "out_ivar_base64"     /* f32 × h·w (fill 输出) */
#define NOISE_O_KEY_FLOOR_FALLBACK  "floor_fallback"      /* fill op: 1=注册表 miss 回退 */
#define NOISE_O_KEY_WORKERS         "workers"             /* 实际租借 worker 数 (==1) */
/* 标量 bitwise 通道: f64 native 数组 base64 (主对拍口径; 文本字段仅人读,
 * %.17g round-trip 但 NaN/-0 文本面不进入 bitwise 判定)。
 *   estimate_noise_model: [sigma_bg_global, variance_bg_global,
 *                          ivar_bg_global, variance_floor 回显]
 *   fill_noise_field:     [sigma_bg_global, variance_bg_global,
 *                          ivar_bg_global, variance_floor 回显 (影子 miss)] */
#define NOISE_O_KEY_SCALARS_B64     "scalars_base64"      /* f64 × 4 */
#define NOISE_O_KEY_DIAG_VARIANCE   "variance"            /* scale_law 输出 (请求时) */
#define NOISE_O_KEY_DIAG_IVAR       "ivar"                /* scale_law 输出 (请求时) */
#define NOISE_O_KEY_DIAG_GAIN_VAR   "gain_variance"       /* gain_variance 输出 */

/* ───────── 诊断 detail_code (自定义从 100 起; ABI-002) ─────────
 * 通用域用 ACS_DIAG_ECODE_* (1..7); 模块专属从 100 起。 */

typedef enum {
    /* config (domain=CONFIG) — ECODE 排布对齐 P1-CAL/COS 词表 (同构审计面) */
    NOISE_ECODE_OP_UNKNOWN      = 100u, /* config.op 不在词表 */
    NOISE_ECODE_PARAM_MISSING   = 101u, /* 必需 config 键缺失 */
    NOISE_ECODE_PARAM_TYPE      = 102u, /* 键类型错误 (dtype 越域等) */
    NOISE_ECODE_PARAM_RANGE     = 103u, /* 维度 <=0 / 越生产 int 域 */
    NOISE_ECODE_PLAN_FAIL       = 104u, /* plan 期元数据不足 (预留) */
    NOISE_ECODE_EXECUTOR_MISSING = 105u, /* cpu_heavy 无 executor 注入 (BUDGET) */
    /* manifest (domain=DATA) */
    NOISE_ECODE_MANIFEST_MISSING = 110u, /* 必需 manifest 字段缺失 */
    NOISE_ECODE_MANIFEST_DIMS    = 111u, /* manifest 平面尺寸/config 不一致 */
    NOISE_ECODE_B64_INVALID      = 112u, /* base64 非法 / 解码失败 */
    /* execute: legacy 生产返回码转发 (rc 原样进 message "legacy_code=%d",
     * 不重解释科学语义; rc=1=完全退化=成功面科学结果不映射错误) */
    NOISE_ECODE_LEGACY_REJECT  = 120u,
    NOISE_ECODE_MODEL_REBUILD  = 121u, /* fill op 影子模型控制点数组分配失败 */
    NOISE_ECODE_EXCEPTION      = 130u  /* DLL 边界捕获 C++ 异常 (vtable 屏障) */
} noise_ecode;

#ifdef __cplusplus
}
#endif

#endif /* ASTROCS_NOISE_TYPES_H */
