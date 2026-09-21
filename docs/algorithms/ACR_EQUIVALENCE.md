# ACR Equivalence Algorithms (ALG-ACR-EQUIV)

> 上游 SCI: SCI-ACR-EQUIV-001  状态: DERIVED  模块: acr×phase2

## 1 上游 SCI 与输入输出

- 上游: `SCI-ACR-EQUIV-001` (工作域等价, 归约误差, 失败回退不变科学语义)
- 输入: `KernelInvocation` (buffers+scalars, kOpMosaicReject) + split {px_i,p0_i}
- 输出: {signal, support, rejection_count, status} per-pixel, 与CPU reference等价

## 2 离散公式

```text
F1: split不变: Σ px_i = total_pixels, p0_i连续不重叠 → 任意分块等价 (SCI-ACR)
F2: mosaic_reject_legacy 路径: per-pixel rejection+integrate逐位等价, signal=Σw·x/Σw, support=max
F3: 逐样本 ivar 权重（Phase2 按该天球像素对应帧集合现场算出的派生量） → CPU canonical禁用ACR (ACR-IVAR-001)
F4: 容差: float32 max_abs ≤1e-6, float64 ≤1e-12; support/rejection exact
```

来源: `acr_kernels.cpp:1-100` `stage2_common.cpp:378-391`

## 3 伪代码

```text
function mosaic_reject_legacy(inv):
  if missing buffers throw; if px==0||depth==0 throw
  for each p in 0..px-1: stack=values[p*depth..], wmode→weights, rejection 7方法 → integrate Σw·x/Σw
  # per-pixel独立,无跨pixel归约

function cpu_gpu_equiv(inv, split):
  for each split {px_i,p0_i}: legacy(inv subset) → concat signal/support
  assert split科学输出 ≡ cpu_only(inv) within tolerance

Fallback: if 逐样本 ivar 权重 → cpu_only; if !model_trusted → OpenMP fallback
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| px==0/depth==0 | throw |
| 缺buffer0/1 | throw |
| ivar mode | CPU禁用 |
| 无画像信任 | OpenMP fallback |
| non-finite candidate | INVALID_INPUT一致 |

## 5 确定性与归约

- per-pixel独立无跨pixel归约；split按p0 固定顺序concat；归约仅 per-pixel vs/wsum FP64固定顺序。

## 6 复杂度

- O(total_pixels·depth) 每tile

## 7 CPU/GPU 划分可交换

- CPU 为 reference; Mixed分块沿px切分, kernel逐pixel栈语义不变; GPU kernel FMA差异在容差内.

## 8 参考实现/Oracle

- CPU/GPU/Mixed等价门 float32 1e-6; 分块1×N vs N×1; 回退门 ivar.

## 9 容差来源

- 1e-6 float32 (depth≤32累积), 1e-12 float64, support exact, 预冻结.

## 10 关联 ARC/API/TST

- API: acr_kernels.h: kOpMosaicReject, stage2_common.h: 逐样本 ivar 权重分支
- TST: TST-ACR-* 等价/分块/回退

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- 浮点语义/归约非结合：IEEE 754-2019；Goldberg 1991, ACM Comput. Surv. 23, 5（DOI 10.1145/103162.103163）。
- 归约误差界与确定性：Higham 2002, Accuracy and Stability of Numerical Algorithms, 2nd ed., SIAM（ISBN 0-89871-521-0）。
- 可复现求和：Demmel & Nguyen 2013, Proc. 21st IEEE Symp. Computer Arithmetic (ARITH)。
- 并行执行语义：OpenMP Application Programming Interface（OpenMP ARB）；ACR 现处 dormant（AGENTS §2）。
- CPU reference = 权威 science semantics：Project-defined（本文件 §5）。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

