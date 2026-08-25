# Calibration Algorithms (ALG-CAL)

> 上游 SCI: SCI-CAL-001  状态: DERIVED (T200 冻结, 2026-08-23)  模块: calibration

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

来源: `calibrator.cpp:78-93,104-136,147-179` `master_generator.cpp:222-234`

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

- 逐像素独立，`parallel for schedule(static) num_threads(16)` 按行固定顺序，`nth_element` 局部排序不跨像素归约，确定性。
- `generate_master` 每像素独立 sigma-clip，无跨像素归约。

## 6 时间/空间复杂度

- `normalize_flat/calibrate`: O(n) 时间, O(n) 空间 (flat 原地)，单 pass
- `generate_master`: O(n_frames·npix) 时间, O(n_frames) per-pixel 缓冲，空间 O(npix) 输出

## 7 CPU/GPU 等价策略

- 当前仅 CPU OpenMP；GPU 扩展时 `calibrate` 逐像素算术 `1:1` 等价，`generate_master` 的 median 需确定性并行 top-k 等价门，容差 `float32 1e-6`。

## 8 参考实现/Oracle

- 主实现即 reference；`master` 对比 Siril 语义 (04 spec)；NumPy 对 `F2/F3` 双分支复算 `max_abs≤1e-6` (float32)。

## 9 容差来源

- `float32` 相对精度 `~1e-7` × `flat floor 0.1`放大 ≤10× → `rtol 1e-6 atol 1e-7` 预冻结 (同 SCI-CAL)。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` OpenMP 16 线程
- API: `astro_calibration.h: ac_generate_master_bias/dark/flat, ac_calibrate_frame, ac_correct_frame`
- TST: `TST-CAL-001` 常量场、`TST-CAL-INV-001` 幂等、`TST-CAL-FAIL-001` 参数校验
