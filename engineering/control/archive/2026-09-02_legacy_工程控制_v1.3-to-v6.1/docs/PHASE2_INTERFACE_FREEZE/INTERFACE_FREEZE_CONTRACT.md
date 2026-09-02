# Phase2 接口冻结契约

> 本文件为 W2 冻结快照（2026-08-10）；生产权威以 `lib/phase2/include/astro/phase2/*.h`（p2_upm_build / sampler / rejection / integrate）为准，旧 HICS 时代结构仅作历史快照。
（2026-08-10）

## 状态

```text
PHASE1_CODE_FREEZE = PASS
PHASE1_DATASET_DOMAIN = WAITING_T1
PHASE2_INTERFACE_FREEZE = IN_PROGRESS
PHASE2_SYNTHETIC_GATE = NOT_RUN
```

目标：在 main 实施 Phase2——统一光度模型、动态分块、排异框架、HiPS 输出；
合成 Gate 通过后接 ACR 与真实马赛克。**禁止修改 Phase1 科学语义**
（Drizzle/SNR/Photometric 既有产出语义不变）。

## 1. 范围与冻结原则

- 新增 Phase2 接口/实现，不改 Phase1 既有接口语义（`stage1.schema.json` v1.1、
  `PhotometricDiag`、`PcMatchRecord`、HiPS 生产链 `aio_hips_*`）。
- 并行四子模块：统一光度模型（Unified Photometry）、动态分块（Dynamic
  Tiling）、排异框架（Rejection Framework）、HiPS 输出（HiPS Output / HICS）。
- 合成 Gate：全部子模块先用合成数据验证（确定性、数值一致性），再接真实
  数据与 ACR。
- 禁止：改 Phase1 科学语义；新增业务 Adapter 改真实积分/Drizzle；多 stream。

## 2. 接口契约（冻结）

### 2.1 统一光度模型（lib/photometric_calib）

保持既有 C API 兼容，新增 Phase2 结构：

```c
// UnifiedPhotometryConfig：统一光度模型配置（合成 Gate 可确定性注入）
typedef struct {
    int    model_id;            // 0=scale-only(Phase1 兼容), 1=scale+gradient, 2=scale+gradient+rejection
    double initial_scale;       // 初始 scale（默认 1.0）
    int    max_irls_iter;       // IRLS 最大迭代（默认 50）
    double tukey_c;             // Tukey 常量（默认 4.685）
    double sigma_low;           // 排异下限（默认 -3.0）
    double sigma_high;          // 排异上限（默认 +3.0）
    int    enable_rejection;    // 0/1：启用排异框架
} UnifiedPhotometryConfig;

// UnifiedPhotometryResult：统一模型输出（在 PhotometricDiag 基础上新增）
typedef struct {
    double scale_factor;        // 与 Phase1 一致
    double sigma_residual;      // 与 Phase1 一致（MAD/0.6745）
    double gradient_a, gradient_b; // model>=1 时平面梯度（arcsec^-1 或像素级由配置决定）
    int    n_used;              // 排异后参与拟合星数
    int    n_rejected;          // 排异星数
    double rejection_sigma;     // 实际排异 sigma（合成 Gate 校验）
} UnifiedPhotometryResult;
```

### 2.2 动态分块（ACR Dispatcher）

在 ACR 既有 `RouteProfileV2`/`BenchmarkRouteEstimator` 之上冻结：

```text
OperationId:
  synthetic.weighted_integration.fp64acc  (已有)
  synthetic.mosaic_reject.fp64acc          (Phase2 合成马赛克+排异)

RouteHints 增加:
  frame_count       (已有)
  reuse_count_hint  (已有)
  rejection_rounds  (新增, 默认 1: 每轮 sigma-clip)

Dispatcher 顶层：
  RouteProfileV2 + BenchmarkRouteEstimator 唯一权威（不变）
  GPU Direct / Legacy OpenMP / Mixed（不变）
```

### 2.3 排异框架（Rejection Framework）

独立纯函数接口（合成 Gate 直接调用）：

```c
// RejectionMode: 0=sigma-clip, 1=median-abs-dev, 2=Tukey-irls
typedef enum { REJECT_SIGMA=0, REJECT_MAD=1, REJECT_TUKEY=2 } RejectionMode;

// RejectResult
typedef struct {
    int    n_total;
    int    n_kept;
    int    n_rejected;
    double keep_ratio;         // n_kept/n_total
    double mean_kept;          // 保留值均值
    double sigma_kept;         // 保留值标准差
} RejectResult;

// 对输入值数组做排异（值=每帧该像素叠加的流量/权重等）
int acr_reject_values(const double* values, int n,
                      RejectionMode mode,
                      double sigma_low, double sigma_high,
                      int max_iter, RejectResult* out);
```

### 2.4 HiPS 输出（HICS / 球面马赛克）

复用既有 `aio_hips_*` 生产链，新增：

```text
AioHipsMosaicInput:
  tiles[]: { parent_ipix, leaf_order, width, flux_sum, covered_area, valid_mask }
  bands:   ["L","R","G","B","HA","OIII"] 或单通道
  combine: "sum" | "mean" | "weighted"     (马赛克叠加)
  rejection: RejectConfig（sigma/mad/tukey）
```

输出：`<out>/mosaic/signal/ + support/ + snr/`（IVOA HiPS 1.4 标准，与 Phase1
同 writer）。

## 3. 合成 Gate 验收

合成数据（确定性 seed）：
1. 多帧球面合成帧（已知叠加真值）；
2. 已知离群值（注入 outlier）验证排异；
3. 已知梯度注入验证统一光度模型 gradient 解；
4. 多帧马赛克 → HiPS 输出与逐像素真值比较（mismatch=0 或 <= 容差）。

Gate：
```text
SYNTHETIC_UNIFIED_PHOTOMETRY = PASS  (scale/gradient/rejection 恢复真值)
SYNTHETIC_REJECTION          = PASS  (outlier 100% 检出、keep_ratio 正确)
SYNTHETIC_DYNAMIC_TILING     = PASS  (分块数/驻留/路由与 oracle 一致)
SYNTHETIC_HIPS_MOSAIC        = PASS  (HiPS tiles 与逐像素真值 mismatch=0)
```

全部 PASS 后才进入：ACR 真实马赛克合成路径接入（不接真实业务）与真实数据验证。

## 4. 目录

- `工程控制/docs/PHASE2_INTERFACE_FREEZE/` 本契约
- `lib/photometric_calib/cpp/` 统一光度模型（新增 `unified_photometry.h/.cpp`）
- `lib/acr/` 动态分块（新增合成 mosaic_reject Operation + rejection 纯函数）
- `lib/astro_image_io/` HiPS 输出（复用 `aio_hips_*`，新增 mosaic 组合接口）
- `run/evidence/phase2_*/` 证据