# Calibration Algorithms (ALG-CAL)

> ID: ALG-CAL-001  范围: ALG-CAL-001..004  上游 SCI: SCI-CAL-001  状态: DERIVED (T200 冻结; V5 ALG-001 重验 2026-08-28)  模块: calibration

## 1 上游 SCI 与输入输出

- 上游: `SCI-CAL-001` (CALIBRATION.md: bias/dark/flat/cosmetic 双分支公式 + flat_norm median1.0 floor0.1)
- 输入: `raw` 亮场 `float32[H×W]` + `masterBias/masterDark/masterFlat` + `cosmetic bad_mask` + `K=t_light/t_dark` + `dark_opt∈{0,1}`
- 输出: `cal` 亮场 `float32[H×W]` 落盘 `run/calibrated/<dataset>/<filter>/`，`actual_k` 回传

## 2 离散公式

```text
F1: flat_norm = max(flat / median(flat), 0.1)  if median>0 else flat (不归一)
F2: dark_opt=0: cal = (raw − dark) / max(flat_norm,0.1)
F3: dark_opt=1 && bias&&dark: cal = (raw − bias − K·(dark−bias)) / max(flat_norm,0.1), K=k_init
    else: fallback F2 且 K→1.0
F4: master: generate_master sigma-clip 迭代: median/MAD→sigma=1.4826·MAD, 剔除 [median−sigma_low·σ, median+sigma_high·σ] 外，迭代 max_iter，合并 combine=mean/median
```

推导来源: **SCI-CAL-001 §5 连续定义的离散化**(F1–F4 与之一一对应);实现一致性锚(非推导依据): `calibrator.cpp:78-93,104-136,147-179` `master_generator.cpp:222-234`

## 3 伪代码

```text
function normalize_flat(flat, w,h):
  med = median(flat)
  if !(med>0) return
  inv=1/med
  parallel for i in 0..n-1: flat[i]=max(flat[i]*inv, 0.1)

function calibrate(light,dark,flat,bias,out,dark_opt,k_init):
  n=w*h; k=k_init
  if dark_opt==1 && bias&&dark:
    parallel for i: out[i]=(light[i]-bias[i]-k*(dark[i]-bias[i]))/max(flat[i],0.1)
  else:
    k=1.0
    parallel for i: out[i]=(light[i]-dark[i] if dark else light[i])/max(flat[i] if flat else 1,0.1)

function calibrate_d(double 版): 同上 double 算术，不降级 float

function generate_master(stack,n_frames,w,h,sigma_low/high,max_iter,combine):
  if n_frames==1: copy→out
  parallel for each pixel:
    vals = stack[:,idx]
    for iter in 0..max_iter-1:
      med=median(finité vals); mad=median(|vals−med|); sigma=1.4826·mad; if sigma==0 break
      mark NaN those outside [med−sigma_low·σ, med+sigma_high·σ]
    out[idx]=mean或median(剩余 finite vals)
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `median(flat)<=0` | 不归一，保持原样 |
| `flat==NULL` | 跳过除法，退化减法 |
| `dark==NULL` | `v=raw` |
| `bias/dark 缺一且 dark_opt==1` | 回退 dark_opt=0 语义 |
| 输入含 NaN | 仅 finite 参与 median/MAD，其余跳过 |
| `w<=0/h<=0/空指针` | `AC_ERR_PARAM` |

## 5 确定性与归约

- 逐像素独立：线程数由运行时调度按**有效 CPU affinity 的 worker pool** 决定，**禁止硬编码线程数**（V5 硬约束）；静态按行分块，输出 bitwise 与线程数/分块无关。
- `nth_element` 中位数为选择而非跨像素归约；`combine=mean` 的归约顺序**冻结为帧序升序串行累加**（FP32），与线程数无关；`generate_master` 每像素独立 sigma-clip，无跨像素归约。

## 5a variance/mask 传播（ALG-001 检查点）

- **variance 不在本层传播**（SCI-CAL §9 边界）：ivar 由 SCI-NOISE 独立估计；master 的样本方差仅用于 sigma-clip 判决，不写入输出。
- **mask 传播**：输入 NaN → 输出 NaN（不产出伪有效值）；`bad_mask=1` 像素仅被 cosmetic 修复（ALG-CAL-004），不改变校准算术；`flat=NULL/dark=NULL` 的退化分支即 mask-free 路径。

## 5b SIMD 安全条件与取消点

- SIMD 安全：`in/out` 无别名（checked）、行连续、逐像素独立无跨像素归约；FP32/FP64 遵循 IEEE-754，**禁全局 fast-math/重结合**（确定性前提）；SIMD 变体仅经逐内核 benchmark 注册（V5 ABI，标量参考路径恒存在）。
- 取消点：`generate_master` 按帧循环间检查 cancel（帧粒度）；`calibrate` 按行块检查（row-block 粒度）；取消时已写输出作废并返回 `AC_ERR_CANCEL`（语义随 API-003 冻结）。

## 6 时间/空间复杂度

- `normalize_flat/calibrate`: O(n) 时间, O(n) 空间 (flat 原地)，单 pass
- `generate_master`: O(n_frames·npix) 时间, O(n_frames) per-pixel 缓冲，空间 O(npix) 输出

## 7 CPU-only 后端策略（V5：GPU/ACR 不在本轮，dormant）

- 仅 CPU：标量参考路径恒存在且为正确性基准；ISA 变体（SSE4/AVX/AVX2/AVX512）由逐内核 benchmark 自适应选择并经 ABI 注册，**无全局 `-march` 编译参数**；profile 缺失时 baseline 后端+动态 worker（保守不等于单线程）。

## 8 参考实现/Oracle

- 主实现即 reference；`master` 对比 Siril 语义 (04 spec)；NumPy 对 `F2/F3` 双分支复算 `max_abs≤1e-6` (float32)。

## 9 容差来源

- `float32` 相对精度 `~1e-7` × `flat floor 0.1`放大 ≤10× → `rtol 1e-6 atol 1e-7` 预冻结 (同 SCI-CAL)。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` OpenMP 16 线程
- API: `astro_calibration.h: ac_generate_master_bias/dark/flat, ac_calibrate_frame, ac_correct_frame`
- TST: `TST-CAL-001` 常量场、`TST-CAL-INV-001` 幂等、`TST-CAL-FAIL-001` 参数校验
