# ACR Work-Domain Equivalence Science (SCI-ACR-EQUIV)

> ID: SCI-ACR-EQUIV-001  状态: FROZEN (T109 冻结, 2026-08-23)  上游: SCI-SCOPE-001 + SCI-INT/REJ/UPM/DRIZZLE  下游 ALG: ALG-ACR-EQUIV-001..  模块: acr × phase2 (acr_kernels)

## 1 目的与非目标

- **目的**：定义 CPU / GPU / 混合分块的工作域 `equiv`、数值等价边界与失败回退语义，使 `phase2` 加权叠加在任意 `ACR` 调度下科学结果一致，允许的数值差异仅来自编译器/FMA/归约阶且预冻结冻结。
- **非目标**：不决定 `ACR` 资源调度/性能优化（见 `PERFORMANCE_MODEL.md`）；不定义 `ACR` 通用硬件画像（聚焦 `phase2` 热点）；不改变 `phase2` 权重/排异科学（`acr_kernels.cpp: CPU reference 是权威 science semantics`）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `kOpMosaicReject` | 合成 Operation `synthetic.mosaic_reject.fp64acc` | `acr_kernels.h:phase2` |
| `inv` | `KernelInvocation`（buffers+scalars） | `acr_kernels.cpp:mosaic_reject_legacy` |
| `px` | `pixel_count`（该子域像素数） | `scalars[0]` |
| `depth` | `stack_depth`（候选深度） | `scalars[sizeof_t]` |
| `p0` | tile 内偏移（`inv.scalars`） | `scalars+p0` |
| `wmode` | `weight_mode`（0 legacy / 1 equal / 2 ivar） | `scalars+wmode` |
| `mosaic_reject_legacy` | CPU reference launcher（逐像素 `rejection+integrate`） | `acr_kernels.cpp` |
| `weight_mode=2 (ivar)` | 逐像素 ivar 产品权重 | `stage2_common.cpp:378` |
| `ACR-IVAR-001` | ivar 时 ACR 块禁用 → CPU canonical | `stage2_common.cpp:391` |

## 3 物理量和单位

- `values, signal`: ADU；`support`: 无量纲 [0,1]；`weights, ivar`: 信号⁻²；`pixel_count, depth`: 无量纲；`frame_id`: uint64；误差：ADU（信号域）。

## 4 输入有效域

- `inv.buffers` 含 `buffer0=out signal`, `buffer1=values` 必备，否则 `throw runtime_error`；`buffer2/3/4/5/6` 可空（`support/snr/out_sup/out_rej/out_valid`）。
- `scalars` 须 `px>0 && depth>0`，否则 `throw`；`method/underdetermined_n/sigma_lower+upper/max_iterations/p0/wmode` 均有缺省（`1u/2u/-4.0/3.0/8/0/0`）。
- `weight_mode=2 (ivar)` 时 `ACR` 块禁用，走 CPU `canonical p2_integrate_pixel` 路径（`ACR-IVAR-001`），不进入 `GPU/Mixed` 分块。
- `weight_mode∈{auto,ivar,equal,support_x_snr2}`，`acr_route∈{auto,cpu}`（`auto==cpu` 语义，见 `stage2_common.cpp:391-392`）。

## 5 连续定义

```text
工作域等价 (SCI-ACR-EQUIV-001):
  ∀ tile 子域分块 {px_i, p0_i} 满足 Σ px_i = total_pixels 且 p0_i 连续不重叠
     ACR 调度前  cpu_only(inv)  ≡ gpu_only(inv)  ≡ mixed(inv, split={px_i})
  的科学输出 {signal, support, rejection_count, status} 在容差内等价；
  分块不改变 per-pixel 候选栈语义（frame_id 绑定不变，rejection/integrate 逐像素独立）。

两阶段不变:
  CPU reference 为权威 science semantics；ACR 只加速热点
    (block calibration / rejection / weighted reduction) (acr_kernels.cpp:W9)
  首版合成 Operation synthetic.mosaic_reject.fp64acc:
    legacy_parallel launcher 直接执行 phase2 CPU 语义 (rejection+integrate) 保证等价；
    GPU kernel 后续在 profile 后添加但不改变语义。

失败回退 (不得改变科学语义):
  ivar 生产路径 (weight_mode==2) → 强制 CPU canonical (ACR-IVAR-001)
  无画像/无画像信任 (model_available≠model_trusted) → OpenMP fallback (acr memory.md: BDR Reviewed)
  候选栈 non-finite / UNDERDETERMINED / ALL_REJECTED 等冻结语义在任意设备上一致
```

与 `lib/phase2/src/acr_kernels.cpp:1-100`、`lib/phase2/src/stage2_common.cpp:378-391`、`lib/acr/memory.md: BDR Reviewed` 一致。

## 6 假设

- `phase2` 热点为逐像素独立栈（`rejection+integrate`），分块仅沿像素维切分，不跨像素依赖；
- `frame_id` 稳定绑定不受分块影响；
- `GPU` 浮点阶差异仅来自 FMA/并行归约阶，不含算法分支差异。

## 7 独立不变量

- **分块不变量**：任意 `split`（含 `1×total` 与 `N×1` 极端）`signal/support` 等价。
- **设备不变量**：`cpu_only` vs 单设备 `gpu_only` vs `mixed`（同 `total_pixels`）结果等价。
- **回退不变量**：回退到 CPU 的结果与直接 CPU 一致，无相位内 `weight_mode` 偷换。
- **常量场不变量**：常数 `values=C` 时 `signal=C` 与设备/分块无关。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `px==0` / `depth==0` | `throw runtime_error missing scalars` | `acr_kernels.cpp` |
| 缺 `buffer0/1` | `throw missing buffers` | 同上 |
| `weight_mode=2` (ivar) | 禁 ACR，CPU canonical | `ACR-IVAR-001` |
| 无画像信任 | OpenMP fallback | `acr memory.md BDR` |
| 非有限 candidate | `INVALID_INPUT` 一致 | `integrate.cpp` |

## 9 精度策略

- FP32 信号域 `signal` 与 FP64 累积 `vs/wsum` 的 `dtype-specific` 容差：
  - `float32 mosaic_reject`：`CPU/GPU` 差 `max_abs ≤ 1e-6 ADU`（FMA 阶差异，预冻结）；
  - `float64` 内部累积：`max_abs ≤ 1e-12 ADU`（归约阶差异）；
  - 推导：`float32` 机器精度 `~1e-7` × `depth≤32` 累积 × `tiles 714` 独立像素，实测 `≤1e-6` 包络；禁止失败后增大。
- `support/max` 与 `rejection_count` 为 `exact` 等价（离散计数，不可放宽）。

## 10 不可接受变化

- 在 `GPU/Mixed` 分块中改变 `rejection` 阈值/归一化/large_scale 语义；
- 在 `weight_mode=2` 时仍走 `GPU` 分块（违反 `ACR-IVAR-001`）；
- 将 `status` 的 `UNDERDETERMINED/INVALID` 语义改写为设备相关；
- 以放宽 `1e-6/1e-12` 容差掩盖分块越界或 `p0` 错位。

## 11 验证 Oracle

- **CPU/GPU/Mixed 等价门**：同 `total_pixels` 的 `cpu_only vs gpu_only vs mixed(2/4/8 splits)` 的 `signal max_abs ≤ 1e-6`、`support exact`（`synthetic_gate` 变种）。
- **分块不变量门**：`1×N` vs `N×1` 切分结果等价。
- **回退门**：`weight_mode=ivar` 的 `ACR` 强制 CPU 与纯 CPU `canonical` 等价（`ivar_wiring_test`）。
- **流量守恒门**：常数场 `C` 的 `signal==C` 与分块无关。
- **Python 参考**：NumPy 对同分块的 `rejection+integrate` 复算 `signal/support`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-ACR-EQUIV-001` 合成 Operation 分块等价（`mosaic_reject_legacy` → ACR 调度）
- `ALG-ACR-EQUIV-002` ivar 生产禁用与 OpenMP fallback 路由

## 13 追溯与测试

- 权威文件: `docs/science/ACR_EQUIVALENCE.md` (SCI-ACR-EQUIV-001)
- 实现: `lib/phase2/src/acr_kernels.cpp` (`kOpMosaicReject, mosaic_reject_legacy`), `lib/phase2/src/stage2_common.cpp` (`weight_mode/ACR-IVAR-001`), `lib/acr/scheduler/*` (Dispatcher/Profile)
- 公开 API: `register_phase2_acr_kernels, kOpMosaicReject`
- 测试: `TST-ACR-001` CPU/GPU等价、`TST-ACR-INV-001` 分块不变量、`TST-ACR-FAIL-001` 极端回退（新增/映射见 `docs/TRACEABILITY.csv`）
