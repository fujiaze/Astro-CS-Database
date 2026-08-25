# ACR Equivalence Algorithms (ALG-ACR-EQUIV)

> 上游 SCI: SCI-ACR-EQUIV-001  状态: DERIVED (T209 冻结, 2026-08-23)  模块: acr×phase2

## 1 上游 SCI 与输入输出

- 上游: `SCI-ACR-EQUIV-001` (工作域等价, 归约误差, 失败回退不变科学语义)
- 输入: `KernelInvocation` (buffers+scalars, kOpMosaicReject) + split {px_i,p0_i}
- 输出: {signal, support, rejection_count, status} per-pixel, 与CPU reference等价

## 2 离散公式

```text
F1: split不变: Σ px_i = total_pixels, p0_i连续不重叠 → 任意分块等价 (SCI-ACR)
F2: legacy launcher: per-pixel rejection+integrate逐位等价, signal=Σw·x/Σw, support=max
F3: weight_mode=2 ivar → CPU canonical禁用ACR (ACR-IVAR-001)
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

Fallback: if weight_mode==2 → cpu_only; if !model_trusted → OpenMP fallback
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

- Legacy CPU为reference; Mixed分块沿px切分, kernel逐pixel栈语义不变; GPU kernel FMA差异在容差内.

## 8 参考实现/Oracle

- CPU/GPU/Mixed等价门 float32 1e-6; 分块1×N vs N×1; 回退门 ivar.

## 9 容差来源

- 1e-6 float32 (depth≤32累积), 1e-12 float64, support exact, 预冻结.

## 10 关联 ARC/API/TST

- API: acr_kernels.h: kOpMosaicReject, stage2_common.h: weight_mode
- TST: TST-ACR-* 等价/分块/回退
