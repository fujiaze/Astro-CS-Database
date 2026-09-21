# ACR Work-Domain Equivalence Science (SCI-ACR-EQUIV)

> 上游：ASTROCS_DESIGN.md §1.3（非目标）、§4（normalize）

> ID: SCI-ACR-EQUIV-001  状态: FROZEN  上游: SCI-SCOPE-001 + SCI-INT/REJ/UPM/DRIZZLE  下游 ALG: ALG-ACR-EQUIV-001..  模块: acr × phase2 (acr_kernels)

## 1 目的与非目标

- **目的**：定义 CPU / GPU / 混合分块的工作域 `equiv`、数值等价边界与失败回退语义，使 `phase2` 加权叠加在任意 `ACR` 调度下科学结果一致，允许的数值差异仅来自编译器/FMA/归约阶且预冻结冻结。
- **生产地位**：`ACR` 是隔离实验，生产不可达（`ASTROCS_DESIGN.md` §1.3/§8.1）；本文件定义其工作域等价边界，供隔离实验与回归使用。
- **非目标**：不决定 `ACR` 资源调度/性能优化（见 `PERFORMANCE_MODEL.md`）；不定义 `ACR` 通用硬件画像（聚焦 `phase2` 热点）；不改变 `phase2` 权重/排异科学（`acr_kernels.cpp: CPU reference 是权威 science semantics`）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `kOpMosaicReject` | 合成 Operation `synthetic.mosaic_reject.fp64acc` | `acr_kernels.h:phase2` |
| `inv` | `KernelInvocation`（buffers+scalars） | `acr_kernels.cpp:mosaic_reject_legacy` |
| `px` | `pixel_count`（该子域像素数） | `scalars[0]` |
| `depth` | `stack_depth`（候选深度） | `scalars[sizeof_t]` |
| `p0` | tile 内偏移（`inv.scalars`） | `scalars+p0` |
| `wmode` | 实现标量槽（分支号 0/1/2 为实现事实；权重是阶段二按该天球像素对应帧集合现场算出的派生量） | `scalars+wmode` |
| `mosaic_reject_legacy` | CPU reference launcher（逐像素 `rejection+integrate`） | `acr_kernels.cpp` |
| 逐像素 ivar 权重 | 阶段二按该天球像素对应帧集合现场算出的派生量 | `stage2_common.cpp:378` |
| `ACR-IVAR-001` | ivar 时 ACR 块禁用 → CPU canonical | `stage2_common.cpp:391` |

## 3 物理量和单位

- `values, signal`: ADU；`support`: 无量纲 [0,1]；`weights, ivar`: 信号⁻²；`pixel_count, depth`: 无量纲；`frame_id`: uint64；误差：ADU（信号域）。

## 4 输入有效域

- `inv.buffers` 含 `buffer0=out signal`, `buffer1=values` 必备，否则 `throw runtime_error`；`buffer2/3/4/5/6` 可空（`support/snr/out_sup/out_rej/out_valid`）。
- `scalars` 须 `px>0 && depth>0`，否则 `throw`；`method/underdetermined_n/sigma_lower+upper/max_iterations/p0/wmode` 均有缺省（`1u/2u/-4.0/3.0/8/0/0`）。
- 逐像素 ivar 权重时 `ACR` 块禁用，走 CPU `canonical p2_integrate_pixel` 路径（`ACR-IVAR-001`），不进入 `GPU/Mixed` 分块。
- `acr_route∈{auto,cpu}`（`auto==cpu` 语义，见 `stage2_common.cpp:391-392`）。

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
  逐像素 ivar 权重路径 → 强制 CPU canonical (ACR-IVAR-001)
  无画像/无画像信任 (model_available≠model_trusted) → OpenMP fallback (acr memory.md: BDR Reviewed)
  候选栈 non-finite / UNDERDETERMINED / ALL_REJECTED 等冻结语义在任意设备上一致
```

与 `lib/algorithms/coverage/src/acr_kernels.cpp:1-100`、`lib/algorithms/coverage/src/stage2_common.cpp:378-391`、`lib/infrastructure/acr/memory.md: BDR Reviewed` 一致。

## 6 假设

- `phase2` 热点为逐像素独立栈（`rejection+integrate`），分块仅沿像素维切分，不跨像素依赖；
- `frame_id` 稳定绑定不受分块影响；
- `GPU` 浮点阶差异仅来自 FMA/并行归约阶，不含算法分支差异。

## 7 独立不变量

- **分块不变量**：任意 `split`（含 `1×total` 与 `N×1` 极端）`signal/support` 等价。
- **设备不变量**：`cpu_only` vs 单设备 `gpu_only` vs `mixed`（同 `total_pixels`）结果等价。
- **回退不变量**：回退到 CPU 的结果与直接 CPU 一致，无相位内分支偷换。
- **常量场不变量**：常数 `values=C` 时 `signal=C` 与设备/分块无关。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `px==0` / `depth==0` | `throw runtime_error missing scalars` | `acr_kernels.cpp` |
| 缺 `buffer0/1` | `throw missing buffers` | 同上 |
| 逐像素 ivar 权重 | 禁 ACR，CPU canonical | `ACR-IVAR-001` |
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
- 在逐像素 ivar 权重时仍走 `GPU` 分块（违反 `ACR-IVAR-001`）；
- 将 `status` 的 `UNDERDETERMINED/INVALID` 语义改写为设备相关；
- 以放宽 `1e-6/1e-12` 容差掩盖分块越界或 `p0` 错位。

## 11 验证 Oracle

- **CPU/GPU/Mixed 等价门**：同 `total_pixels` 的 `cpu_only vs gpu_only vs mixed(2/4/8 splits)` 的 `signal max_abs ≤ 1e-6`、`support exact`（`synthetic_gate` 变种）。
- **分块不变量门**：`1×N` vs `N×1` 切分结果等价。
- **回退门**：逐像素 ivar 权重的 `ACR` 强制 CPU 与纯 CPU `canonical` 等价（`ivar_wiring_test`）。
- **流量守恒门**：常数场 `C` 的 `signal==C` 与分块无关。
- **Python 参考**：NumPy 对同分块的 `rejection+integrate` 复算 `signal/support`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-ACR-EQUIV-001` 合成 Operation 分块等价（`mosaic_reject_legacy` → ACR 调度）
- `ALG-ACR-EQUIV-002` ivar 生产禁用与 OpenMP fallback 路由

## 13 追溯与测试

- 权威文件: `docs/science/ACR_EQUIVALENCE.md` (SCI-ACR-EQUIV-001)
- 实现: `lib/algorithms/coverage/src/acr_kernels.cpp` (`kOpMosaicReject, mosaic_reject_legacy`), `lib/algorithms/coverage/src/stage2_common.cpp` (`ACR-IVAR-001`), `lib/infrastructure/acr/scheduler/*` (Dispatcher/Profile)
- 公开 API: `register_phase2_acr_kernels, kOpMosaicReject`
- 测试: `TST-ACR-001` CPU/GPU等价、`TST-ACR-INV-001` 分块不变量、`TST-ACR-FAIL-001` 极端回退（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现，不改动 §5 等价定义与 §9 容差。

- **浮点语义与归约非结合**：IEEE 754-2019, IEEE Standard for Floating-Point Arithmetic；Goldberg, D. 1991, ACM Computing Surveys 23, 5（DOI 10.1145/103162.103163）。
- **归约误差界/确定性求和**：Higham, N. J. 2002, Accuracy and Stability of Numerical Algorithms, 2nd ed., SIAM（ISBN 0-89871-521-0）；可复现求和技术见 Demmel, J. & Nguyen, H. D. 2013, “Fast Reproducible Floating-Point Summation”, Proc. 21st IEEE Symp. Computer Arithmetic (ARITH)。
- **并行执行语义**：OpenMP Application Programming Interface（OpenMP ARB）——本模块 fallback 路径（§5）的语义基础；ACR 为隔离实验，生产不可达（`ASTROCS_DESIGN.md` §1.3/§8.1）。
- **CPU reference 为权威 science semantics**：Project-defined（§5）；GPU/Mixed 仅加速热点，不改变 rejection/integrate 语义。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

