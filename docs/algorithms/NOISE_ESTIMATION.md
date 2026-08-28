# Noise Estimation Algorithms (ALG-NOISE)

> ID: ALG-NOISE-001  范围: ALG-NOISE-001..003  上游 SCI: SCI-NOISE-001  状态: DERIVED (T204 冻结; V5 ALG-003 重验 2026-08-28)  模块: snr_estimator/noise_model

## 1 上游 SCI 与输入输出

- 上游: `SCI-NOISE-001..015` (8×8 patch, MAD 1.4826, 平面场 a+b·x+c·y, floor 1e-12)
- 输入: 校准图像 `float32[H×W]` + `star_x/y` (掩膜) + `SnrNoiseModelConfig`
- 输出: `NoiseWeightModelV1` (patch variance空间场 + 全局兜底 + ivar + degenerate标志)

## 2 离散公式

```text
F1: σ_bg = 1.482602218505602 · median(|x−median(x)|)  (MAD→σ Gaussian)
F2: 5σ裁剪 ≤2轮: 剔除 |x−median|>5σ, 剩余求 σ_bg
F3: var(x,y)=a+b·x+c·y (LS平面, enable_spatial_field==1 && n_ctrl≥4 else 全局中位数)
F4: variance = max(var, 1e-12), ivar=1/variance
F5: g_model_floor[model*]=floor 指针隔离
F6: 诊断: var_ADU=max(signal,0)/gain + (rn/gain)² (仅 SNR-005, 不入生产)
```

来源: `noise_model.cpp:32-464` `default_config variance_floor=1e-12`

## 3 伪代码

```text
function snr_noise_model_v1(image, star_x/y, cfg):
  grid 8×8 patches
  for each patch:
    mask rmax=max(1,r0)·max(1,scale) 固定不按振幅
    vals = unmasked pixels; med=median(vals); mad=median(|vals−med|)
    sigma=1.4826·mad; 2轮内剔除 |v−med|>5σ → patch_variance
  if n_valid <4 or !spatial_field → global_median fallback
  else LS平面 a,b,c 最小二乘 patch_centers→variance
  g_model_floor[model*]=1e-12; for fill: var=max(a+b·x+c·y, floor)

function snr_noise_model_v1_free(model): g_model_floor.erase(model*)
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 全星场无空 patch | NO_DATA fallback global |
| `MAD==0` | sigma==0 degenerate |
| `variance≤0` | max→1e-12 |
| 输入 NaN | skip |

## 5 确定性与归约

- patch独立 OpenMP 并行, median局部无跨patch归约；平面LS按patch索引固定顺序。

## 6 复杂度

- O(pixels) mask+median; LS O(8×8)

## 7 CPU-only 后端策略（V5）

- 仅 CPU：patch 网格(8×8)间独立，worker pool（按 affinity）patch 级并行，**禁止硬编码线程数**；平面最小二乘(3 参数)为全控制点固定序归约(FP64, 禁重结合)。

## 5c SIMD 安全与取消点

- patch 内 MAD/median 为排序选择(固定输入序)；`fill` 阶段 `ivar=1/max(var,floor)` 逐像素独立(SIMD 安全: 数组连续无别名)；`g_model_floor` 指针 key 注册表为单线程资源(锁自由设计, 跨线程不共享写)。
- 取消点: patch 网格按行带粒度检查; 取消时模型对象半成品作废(免费 `_free` 语义), 已写 fill 输出行带不回滚(调用方按返回码整帧重做)。

## 8 参考实现/Oracle

- MC Gaussian/Poisson/场恢复 SNR-004..006; 1/unc²对比

## 9 容差来源

- sigma 5% (MAD鲁棒性), floor 1e-12 预冻结。

## 10 关联 ARC/API/TST

- API: `snr_estimator.h: snr_noise_model_v1/_f64/_fill/_free`
- TST: `TEST-SCI-NOISE-*` MC矩阵

## 11 数据布局

- 输入校准图 `float32[H×W]` 行主序连续（就地, 不复制整图）；`star_x/y`：`double[n_star]`。
- patch 划分为 `8×8` 网格：逐 patch 就地 mask + median/MAD（不物化整图副本）；记录
  `(cx, cy, patch_variance)` 控制点数组（length ≤ n_ctrl ∈ {1..64}）。
- 平面场 `var(x,y)=a+b·x+c·y`：3 个 double 系数；`g_model_floor` = `std::map<void*,double>`
  （以模型指针为 key 的 floor 注册表, 无全局共享）。
- 输出 `NoiseWeightModelV1`：`variance_bg_global`(ADU²)、`has_spatial_field`、
  `degenerate` 标量 + 平面系数/全局兜底。
- 内存：主图 O(H·W)·4B 就地；patch/平面 O(64)/O(1)。无额外整图副本。

## 12 误差预算

- FP64 全链路；MAD 常数 `1.482602218505602`（15 位截断, 与 `1.4826022185` 差 <1e-12）。
- `σ_bg`：Gaussian 假设 + 污染/非高斯空背景 → 科学容差 **5%**（`SNR-004` oracle）。
- `5σ` 裁剪 ≤2 轮：抑制 cosmic/hot, 引入偏差 <~2%。
- 平面场 LS：已知梯度场恢复 `a,b,c` → **10%**（`SNR-006`）；负预测 clamp 到 `variance_floor`。
- `variance_floor=1e-12`：保证 `ivar` 有限; FP64 下数值相对误差 ~1e-15。
- 预算排序：**数值精度(FP64, ≪1e-12) ≪ 科学鲁棒性容差(5%/10%) ≪ 门禁阈值**。误差预算用于
  确定 oracle 容差与门禁, 不宣称覆盖科学不精确性（非 Gaussian/污染天光）。
- 各 F 步骤映射：`F1`→`noise_model.cpp:robust_sigma`（`SNR-004`）；`F3`→`snr_noise_model_v1`
  LS 平面（`SNR-006`）；`F4`→`_fill`（`SNR-002`/`TST-NOISE-INV-*`）。
