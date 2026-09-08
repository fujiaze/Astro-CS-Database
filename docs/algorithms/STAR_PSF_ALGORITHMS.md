# PSF Algorithms (ALG-PSF)

> ID: ALG-STARPSF-001  上游 SCI: SCI-PSF-001  状态: DERIVED (T201 冻结; V5 ALG-002 重验 2026-08-28)  模块: star_detector + dynamic_psf

## 1 上游 SCI 与输入输出

- 上游: `SCI-PSF-001` (Moffat4 β=4, FWHM=1.230310·σ, q_psf=A/residual_scale)
- 输入: 校准图像 `float32[H×W]` + 背景噪声 `σ_bg` (NoiseWeightModelV1)
- 输出: 星点表 `(x,y,flux,A,B,σ,q_psf,residual_scale)` + PSF 块 `[N,9]`——
  **存在两种互不兼容的 9 列布局，严禁混用**（实测锚 @f7fa3160）：

### 1.1 布局 A：oracle/表面亮度参数布局（科学参数序，9 列）

`[N,9]` (B, A, x0, y0, sx, sy, θ, residual_scale, q_psf)

- 语义：Moffat4 拟合参数的**科学表面亮度布局**——直接对应 §2 离散公式
  F1 的参数向量序 (B,A,x0,y0,sx,sy,theta)，物理直观：背景 B、振幅 A、
  中心偏移 (x0,y0)、两轴宽度 (sx,sy)、位置角 θ、残差尺度、QA 代理 q_psf。
- 列数/shape：[N,9]；dtype/单位：B/A/flux/ADU 域、x0/y0/sx/sy/fwhm=px、
  θ=rad、residual_scale=ADU、q_psf=无量纲（均为 double/FLOAT64）。
- 使用面：LM 拟合参数向量序（`dpsf_psf.cpp:24-25,191-192`）、
  §3 伪代码 init/回填、测试 oracle（§10 解析 Moffat4 合成图回收 B,A,cx,cy,sx,sy,θ）。
- 逐列 dtype/invalid 详见 DATA_SEMANTICS §15 生产表布局 B 对照。

### 1.2 布局 B：生产 PSF 块布局（编排序列化序，9 列，权威现状）

`[N,9]` (status, B, flux, cx, cy, fwhm, A, mad, eccentricity)

- 实测锚：`lib/orchestrator/cpp/src/orchestrator.cpp:2428-2464`——row[0..8]
  逐一赋值（:2443-2451），`fn_add_block` 注记
  "PSF 拟合结果: status,B,flux,cx,cy,fwhm,A,mad,eccentricity"（:2456-2458），
  dims=[N,9] FLOAT64（:2453-2455）。
- 语义：DPSFFitResult 的**编排序列化布局**——面向下游消费者：[0]=status
  （整值 0..3，PHOTOMETRIC 仅 status=0 入匹配）、[1]=B、[2]=flux、[3]/[4]=cx/cy
  （全图 0-based px）、[5]=fwhm（(fwhm_x+fwhm_y)/2 平均）、[6]=A、[7]=mad
  （历史列名，权威语义=10–90% 截尾均值 |残差|，即 residual_scale）、
  [8]=eccentricity；dtype 全列 double/FLOAT64，单位 B/A/flux/mad=ADU、
  cx/cy/fwhm/eccentricity 如上。
- 消费者：PHOTOMETRIC（必需块，缺失退出码 3，orchestrator.cpp:2563-2570）、
  snr_psf_fit_quality（`snr_estimator.h:57-75` PsfFitQualityRow 同序映射）；
  逐列 dtype/invalid 权威表见 **DATA_SEMANTICS §15.2 生产表布局 A**
  （该文件在飞，本文件只作消歧引用，不复制其表）。

> **消歧声明**：本节 1.1（科学参数序）与 1.2（生产序列化序）列名零重合顺序、
> 列位置完全不同；按 1.1 解析 1.2 的块会全列错位（如把 status 当 B、把 fwhm 当
> θ），属科学结果错误。任何新消费者/文档引用 PSF 块列序时必须先声明采用哪张
> 布局表。

## 2 离散公式

```text
F1: I(r)=B+A/(1+Q)^4, Q=p1dx²+2p2dxdy+p3dy², dx=x−(cx+x0), dy=y−(cy+y0)
    p1=cos²θ/(2sx²)+sin²θ/(2sy²), p2=sin2θ/(4sx²)−sin2θ/(4sy²), p3=sin²θ/(2sx²)+cos²θ/(2sy²)
F2: 各向同性 sx=sy=σ→ Q=0.5·r²/σ², α=√2σ, FWHM=2√2σ·√(2^{1/4}−1)=1.230310σ
F3: flux=2πA·sxsy/3 (β=4 整平面)
F4: residual_scale=10–90% trimmed mean |residual|, robust_residual_sigma=residual_scale/0.7316728
    (kTrimMeanToSigma=0.7316727929211932, E[trimmed mean |r|]=0.731673σ, Gaussian)
F5: q_psf=A/residual_scale, q_psf为QA代理不进science weight
```

来源: `dpsf_psf.cpp:13-18,66-95,351-368` `noise_model.cpp:35-37`

## 3 伪代码

```text
function detect_centroid(image, sigma_bg):
  for each pixel: if image > bkg+1.5·σbg → candidate (sdet_detector.cpp:215 THRESH_FACTOR=1.5)
  centroid weighted mean of 3×3 neighborhood
  if saturated median+0.7·dynrange reject

function fit_psf_moffat4(image, cx,cy, fitRadius):
  init B=median(patch), A=max−B, x0=y0=0, sx=sy=1.2, θ=0
  LM 7参 Levenberg-Marquardt (dpsf_psf.cpp:lm_solve) iter≤50 tol=1e-6
    J via finite diff, Δ=(JᵀJ+λI)⁻¹ Jᵀr, λ adaptive
  post: FWHM=1.230310·sx/sy, flux=2πA·sxsy/3
  θ消歧 4候选 {θ,π/2−θ,π/2+θ,π−θ} 取 trimmed-mad 最小 (dpsf_psf.cpp:351-363)
  residual_scale = trimmed mean |image−model| (10–90%)
  q_psf = A / residual_scale
  guards: sx/sy>0 else reject (333-344), MAD==0 skip scale, 平坦星 reject

function psf_block_batch(image, n_stars):
  parallel for each star: fit_psf_moffat4 independent

Batch deterministic: input order fixed, per-star independent, reduction none cross-star.
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 图像含 NaN/Inf | 该 patch 跳过拟合，status=BAD |
| `max−B ≤0` | reject `A≤0` (dpsf_psf.cpp:45) |
| `sx/sy ≤0` | reject invalid params (333-344) |
| `MAD==0` | `robust_residual_sigma` 不换算，q_psf仍计算 |
| 平坦星 `‖∇‖→0` | LM 奇异 → iter limit, cost 阈校验 fail (169-170) |
| 饱和 `median+0.7·dynrange` | 掩膜 reject (star_detector.h) |

## 5 确定性与归约

- 每星独立 LM，无跨星归约；OpenMP tile/块并行按输入索引固定顺序；浮点归约仅 per-star `JᵀJ` 7×7 矩阵，顺序固定。

## 6 时间/空间复杂度

- O(pixels) 检测 + O(n_stars × iter × patch) 拟合；空间 O(patch) per thread

## 7 CPU-only 后端策略（V5）

- 仅 CPU：逐星独立 LM 拟合，worker pool（按 affinity）按星批并行，**禁止硬编码线程数**；per-star 结果与线程调度无关（无跨星归约）。

## 5c SIMD 安全与取消点

- 同星窗口内逐像素 residual 为逐元素算术(SIMD 安全: 无别名/行连续)；LM 内的 normal-equation 累加为**该星内固定顺序归约**(窗口行序)，禁重结合；FP32/FP64 IEEE-754, 禁 fast-math。
- 取消点: 按星批粒度检查; 取消时未完成星不写结果(调用方以 status 判别)。

## 8 参考实现/Oracle

- 合成 Moffat 图像 (已知 B,A,σ,θ) 恢复测试 PSF-001..008 位置 ≤0.05px FWHM ≤1%；残差 Gaussian 假设校验 trimmed-mean 0.73167 复算。

## 9 容差来源

- 位置: FP64 LM 0.05px (sub-pixel 插值误差)；FWHM: 1% (离散化 + α² 缩放)；预冻结。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` OpenMP per-tile
- API: `dynamic_psf.h: dpsf_fit, dpsf_fit_batch`, `star_detector.h: sdet_detect`
- TST: `TST-PSF-001` 合成恢复, `TST-PSF-INV` q_psf解耦, `TST-PSF-FAIL` 饱和/平坦拒

## 11 P1-PSF-DOC 冻结附录（2026-09-07，SRC-PSF-001 源码实测）

> 本节为 P1-PSF-DOC 冻结附录：只登记现状与测试设计，不改 §1–§10 科学公式。
> 实测基准 = `lib/dynamic_psf/src/dpsf_psf.cpp`（934 行，2026-09-07 工作区）与
> `lib/dynamic_psf/include/dynamic_psf.h`。模块合同入口 =
> `lib/dynamic_psf/README.md`（r1）+ `lib/dynamic_psf/module.yaml`（CONTRACT_READY）。

### 11.1 实现锚（SRC-PSF-001，VERIFIED）

| 符号/语义 | 锚 |
|---|---|
| moffat4 残差（F1 离散式实现，β=4） | dpsf_psf.cpp:72-101 |
| `lm_solve`（LM 主循环，λ 初值 1e-3 :112；奇异→λ×10 :150-153；收敛判据 ‖Δx‖<tol·(‖x‖+1e-30) :163） | :104-188 |
| `compute_trimmed_mad`（实为 10–90% 截尾均值 \|残差\|，即 F4 residual_scale；lo≥hi 回退中位） | :190-220 |
| `moffat4_fit_tmpl`（采样→初值→LM→验证链→θ 消歧→派生量） | :225-407 |
| 初值链 bkg0 中位/A0=max−B/params={bkg0,A0,0,0,sx0,sx0,0} | :300,308,315 |
| LM 调用 tol=1e-8 / max_iter=200（硬编码） | :320-321 |
| 验证链一：非有限/A≤0/sx≤0.3/sy≤0.3 | :336-338 |
| 验证链二：FWHM>rect 尺寸 | :341-346 |
| 验证链三：背景约束 \|B−bkg0\|/max(bkg0,0.01)>0.5 | :349-354 |
| θ 消歧 4 候选 {θ, π/2−θ, π/2+θ, π−θ} trimmed-MAD 最小（§3 一致，实测 :358-368） | :358-373 |
| F3 flux=2π·A·sx·sy/3（Moffat4 解析积分） | :374-375 |
| FWHM=1.230310·sx/sy（F2 系数 MOFFAT4_FWHM_FACTOR :24） | :339-340 |
| eccentricity=√(1−(smin/smax)²)；img_cx=cx+x0 | :378,383 |
| C API 导出（7 个）：dpsf_fit :427 / dpsf_fit_batch :482 / dpsf_fit_batch_f :580 / dpsf_free_results :599 / dpsf_fit_batch_d :612 / dpsf_fit_batch_f32 :694 / dpsf_fit_batch_f64 :822 | dpsf_psf.cpp |
| star_det v1 `FLOAT64[N,6]` / `psf_params:FLOAT64[N,9]` schema 宏 | dynamic_psf.h:104-105 |
| 错误码 OK=0 / NO_CONVERGENCE=1 / INVALID_PARAMS=2 / ITERATION_LIMIT=3 | dynamic_psf.h:33-36 |
| 批拟合 OpenMP `schedule(dynamic) reduction(+:success_count)` 4 处 | dpsf_psf.cpp:528,635,738,866 |

与 §3 伪代码的出入（如实登记，不改 §3）：实测 LM 参数为 tol=1e-8、max_iter=200
（:320-321），§3 "iter≤50 tol=1e-6" 为旧稿；`DPSFFitParams.maxIter/tolerance`
字段不被消费（DISP-PSF-003）。§4 "NaN/Inf patch 跳过 status=BAD" 无对应实现：
BAD 码不存在（dynamic_psf.h:33-36 仅 0–3），NaN/Inf 经 LM 传导至参数非有限由
验证链一 (:336-338) 判 NO_CONVERGENCE。§4 饱和掩膜 reject 属 star_detector 侧，
dynamic_psf 不消费饱和列 [4]/[5]（:741）。

### 11.2 拟合失败语义（冻结，P1-PSF-TEST 逐码负例）

| 码 | 宏 | 触发（实测锚） | 单星接口（dpsf_fit/moffat4_fit） | 批接口（f32/f64 [N,9]） |
|---|---|---|---|---|
| 0 | DPSF_FIT_OK | 收敛 :163 且过验证链一~三 | 全参数回填 :391-403 | 计入 out_n_valid；写 9 字段 :784-794/907-916 |
| 1 | DPSF_FIT_NO_CONVERGENCE | 验证链一 :336-338 / 二 :341-346 / 三 :349-354 | result 已 memset 0（:229）+status | 9 字段全 NaN（:729-732 初始化，失败不覆盖） |
| 2 | DPSF_FIT_INVALID_PARAMS | 空指针/w≤0/h≤0 :431-434；rect 面积<9 :236-239；rect 越界 :240-245；空 rect :445-450 | 同上 | 批整体 -1（:700-707），不触碰输出 |
| 3 | DPSF_FIT_ITERATION_LIMIT | max_iter=200 耗尽 :187 | 仍回填当前最优参数 :391-403 | 非 OK→NaN，不计 valid |

`gauss_solve` 奇异（λ×10 重试 :150-153）不单独出码，最终由收敛判据归类。
简并兜底：验证链一 sx/sy>0.3 判定 + 步后钳位（:175-177）；θ 对称简并由
4 候选消歧（:358-368）确定性回选，不产生不确定状态。

### 11.3 DISP-PSF-001..006（登记不改码，整改归 P1-PSF-IMPL/INT）

| ID | 内容 | 锚 |
|---|---|---|
| DISP-PSF-001 | 参数向量序 B,A,x0,y0,sx,sy,theta 与 MOFFAT4_FWHM_FACTOR=1.230310 常数依赖，重构时序耦合 | :24-25,191-192 |
| DISP-PSF-002 | 前向差分雅可比（:120 相对 1e-6/绝对 1e-8）+ 步后硬钳位（:175-177）破坏二阶收敛路径 | :120,175-177 |
| DISP-PSF-003 | `DPSFFitParams.maxIter/tolerance` 死参数（LM 硬编码 1e-8/200）；§3 伪代码参数为旧稿 | :320-321,716-719 |
| DISP-PSF-004 | 无取消检查点（OpenMP dynamic 4 处批拟合不可中断） | :528,635,738,866 |
| DISP-PSF-005 | 无参数协方差/不确定性输出（科学专项 covariance 缺口，P1-PSF-IMPL 落地） | DPSFFitResult 12 字段 dynamic_psf.h:16-31 |
| DISP-PSF-006 | 批 f32/f64 路径逐星退败静默（仅 out_n_valid 汇总，per-star 状态不出批） | :784-804,907-925 |

### 11.4 TEST-PSF-DESIGN-001（测试设计，P1-PSF-TEST 执行）

- unit：4 状态码逐码负例（§11.2 表）；rect 面积<9/越界/空 rect；饱和列不消费断言。
- oracle：解析 Moffat4（β=4）合成图回收 B,A,cx,cy,sx,sy,θ；flux=2πAsxsy/3 与
  FWHM=1.230310·σ 恒等复核；独立参考不调用生产 symbol（11 号标准 §5）。
- property：θ 消歧确定性（同输入同 θ 回选）；eccentricity∈[0,1)；批输出 NaN 占位
  与 out_n_valid 一致；per-star 独立性（打乱星序不改变逐星结果）。
- boundary：fitRadius 裁边 clamp（:438-441）；FWHM≈rect 边界；背景约束 0.5 阈值
  边界；max_iter 边界（ITERATION_LIMIT 仍回填）。
- performance：1/N worker 缩放、provider=baseline、确定性重跑一致。
- 容差来源：fixtures generator 注记（11 号标准 §5 元数据），不用本文手抄值。
- 状态：VERIFIED（设计冻结，dpsf 套件建立后 TEST-P1-PSF-001 落 EVIDENCE）。

### 11.5 SCI-P1-PSF-001 状态声明

科学专项（matrix P1-PSF 行）映射：known Gaussian/Moffat parameters=§2 公式 + §11.1
参数序/初值/常量锚；fit failure semantics=§11.2（四码语义冻结，无含糊）；degenerate/
saturated=§11.2 简并兜底 + §11.1 饱和列不消费登记（P1-PSF-TEST 专项）；covariance=
现状缺失，DISP-PSF-005 显式登记为 P1-PSF-IMPL 整改项，禁止宣称已实现。
