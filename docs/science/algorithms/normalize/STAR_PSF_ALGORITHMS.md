# PSF Algorithms (ALG-PSF)

> 上游：ACSD_DESIGN.md [D-1]「节点流程」一节（Phase1 节点流程）

## 1 上游 SCI 与输入输出

- 上游: `SCI-PSF-001` (Moffat4 β=4, FWHM=1.230310·σ, q_psf=A/residual_scale)
- 输入: 校准图像 `float32[H×W]` + 背景噪声 `σ_bg` (NoiseWeightModelV1)
- 输出: 星点表 `(x,y,flux,A,B,σ,q_psf,residual_scale)` + PSF 块 `[N,9]`——
  **存在三种互不兼容的 9 列布局，各自独立、取值互不代用**（按接口/函数名与结构体字段指认）：

### 1.1 布局 A：oracle/表面亮度参数布局（科学参数序，9 列）

`[N,9]` (B, A, x0, y0, sx, sy, θ, residual_scale, q_psf)

- 语义：Moffat4 拟合参数的**科学表面亮度布局**——直接对应 §2 离散公式
  F1 的参数向量序 (B,A,x0,y0,sx,sy,theta)，物理直观：背景 B、振幅 A、
  中心偏移 (x0,y0)、两轴宽度 (sx,sy)、位置角 θ、残差尺度、QA 代理 q_psf。
- 列数/shape：[N,9]；dtype/单位：B/A/flux/ADU 域、x0/y0/sx/sy/fwhm=px、
  θ=rad、residual_scale=ADU、q_psf=无量纲（均为 double/FLOAT64）。
- 使用面：LM 拟合参数向量序（常数与维数 `lib/algorithms/psf/src/dpsf_psf.cpp`）、
  [S-1]「公式与推导」一节 伪代码 init/回填、测试oracle（[S-1]「判据与误差」一节解析Moffat4 合成图回收 B,A,cx,cy,sx,sy,θ）。
- 逐列 dtype/invalid 详见 `docs/detail/registry/acsd.phase1.star-psf.md`（DATA-P1-PSF 端口，
  `DPSFFitResult` 逐列面）的生产表布局 B 对照。

### 1.2 布局 B：生产 PSF 块布局（编排序列化序，9 列，权威现状）

`[N,9]` (status, B, flux, cx, cy, fwhm, A, mad, eccentricity)

- 实测锚：`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp` 的 PSF 块组装段——row[0..8]
  逐一赋值，`fn_add_block` 注记
  "PSF 拟合结果: status,B,flux,cx,cy,fwhm,A,mad,eccentricity"，
  dims=[N,9] FLOAT64（同文件）。
- 语义：DPSFFitResult 的**编排序列化布局**——面向下游消费者：[0]=status
  （整值 0..3，PHOTOMETRIC 仅 status=0 入匹配）、[1]=B、[2]=flux、[3]/[4]=cx/cy
  （全图 0-based px）、[5]=fwhm（(fwhm_x+fwhm_y)/2 平均）、[6]=A、[7]=mad
  （列名 `mad` 的权威语义 = 10–90% 截尾均值 |残差|，即 `residual_scale`）、
  [8]=eccentricity；dtype 全列 double/FLOAT64，单位 B/A/flux/mad=ADU、
  cx/cy/fwhm/eccentricity 如上。
- **θ 列不在此布局中**（布局 B 无 θ 列）；布局 A 的 θ 单位 rad，**规范值域
  `[0, π)`**，消费方必须先归约再作位置角解释（实测真实产物 `|θ|>π` 占 63.1%，
  见 `docs/science/psf/PSF.md` [S-1]「参数与常数」一节）。
- **[7] 列名 `mad` 与语义**：权威语义 = 10–90% 截尾均值 |残差|（`residual_scale`，
  单位 ADU），**不是**中位绝对偏差；`robust_residual_sigma = [7]/0.7316727929211932`
  仅在 Gaussian 残差且 `m ≥ 441` 时具绝对标度意义（`docs/science/psf/PSF.md` [S-1]「拟合残差的截尾标度」一节）。
- 消费者：PHOTOMETRIC（必需块，缺失退出码 3，`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp`）、
  snr_psf_fit_quality（`lib/algorithms/noise_snr/cpp/include/snr_estimator.h` PsfFitQualityRow 同序映射）；
  逐列 dtype/invalid 权威表见 **`docs/detail/registry/acsd.phase1.star-psf.md` 的
  DATA-P1-PSF 端口输出面**（`DPSFFitResult` 逐列序）
  （该文件在飞，本文件只作消歧引用，不复制其表）。

### 1.3 布局 C：批量 ABI 参数布局（`dpsf_fit_batch_f32/f64` 的消费/产出序，9 列）

`[N,9]` (B, A, cx, cy, sx, sy, θ, fwhm_x, fwhm_y)

- 实测锚（权威 = 头文件逐列注释）：`lib/algorithms/psf/include/dynamic_psf.h`
  逐列注明 `[0]=B [1]=A [2]=cx [3]=cy [4]=sx [5]=sy [6]=theta [7]=fwhm_x [8]=fwhm_y`；
  行写出 = `lib/algorithms/psf/src/dpsf_psf.cpp`（f32 / f64 两处），
  成功行 compact 左移（同文件）；schema 宏 `lib/algorithms/psf/include/dynamic_psf.h`。
- 生产调用方：`lib/infrastructure/scheduler/src/module_adapters.cpp`。
- **与 A/B 都不同序**：C 无 status/flux/mad/eccentricity 列；`[2]/[3]` 是**全图** cx/cy
  （而 A 的 `[2]/[3]` 是**相对偏移** x0/y0、B 的 `[5]` 是**平均** fwhm）；
  C 保留 `fwhm_x`/`fwhm_y` 两列而 B 只给平均。
- **布局 A 无生产写出面**：`residual_scale`/`q_psf` 在 `lib/algorithms/psf/src/dpsf_psf.cpp`
  全文件 **0 命中** ⇒ A 是 oracle/测试与 SCI 参数序面，**不是**任何接口产出的生产块。
- **DATA 侧待订正（登记移交，不代改）**：`docs/detail/registry/acsd.phase1.star-detection.md`（DATA-P1-STAR 端口输出面）把布局 C
  与布局 B 当「同名异物」处理；实测二者**既不同名也不同物**（B 首列 status、C 首列 B）
  ⇒ 该处待订正；该文件当前在飞，本节只作消歧引用。

> **消歧声明**：本节 1.1（科学参数序）、1.2（生产序列化序）、1.3（批量 ABI 参数序）
> 三张表的列名与列位置互不相同；按错表解析会全列错位（如把 status 当 B、把 fwhm 当
> θ、把全图 cx 当相对偏移），属科学结果错误。任何新消费者/文档引用 PSF 块列序时必须先
> 声明采用哪张布局表。

## 2 离散公式

```text
F1: I(r)=B+A/(1+Q)^4, Q=p1dx²+2p2dxdy+p3dy², dx=x−(cx+x0), dy=y−(cy+y0)
    p1=cos²θ/(2sx²)+sin²θ/(2sy²), p2=sin2θ/(4sx²)−sin2θ/(4sy²), p3=sin²θ/(2sx²)+cos²θ/(2sy²)
F2: 各向同性 sx=sy=σ→ Q=0.5·r²/σ², α=√2σ, FWHM=2√2σ·√(2^{1/4}−1)=1.230307652590102σ
    # 实现常量 MOFFAT4_FWHM_FACTOR=1.230310（dpsf_psf.cpp），与精确值相对差 +1.91e-6
    # 适用域：仅 β=4；各向异性按轴 FWHM_x=1.230310·sx、FWHM_y=1.230310·sy（同文件）
F3: flux=2πA·sxsy/3 (β=4 整平面；对任意 sx,sy,θ 成立：det M=1/(4sx²sy²) ⇒ ∫∫A/(1+Q)^4=πA/(3√det M))
    # 截断适用域：窗口半径 r_win 的窗外通量占比 f_out=(1+r_win²/α²)^{−3}；
    # r_win=3.7172σ（sdet s_factor）⇒ f_out=2.02e-3（发布值偏高 0.20%）；
    # r_win<1.41σ ⇒ f_out>3.1%，该域下 flux 不得当全通量用
F4: residual_scale=10–90% trimmed mean |residual|, robust_residual_sigma=residual_scale/0.7316727929211932
    (kTrimMeanToSigma=0.7316727929211932, E[trimmed mean |r|]=0.731673σ, Gaussian)
    # 闭式：2(φ(Φ⁻¹(0.55))−φ(Φ⁻¹(0.95)))/0.8 = 0.7316730952806134（与常量相对差 4.13e-7）
    # 适用域：Gaussian 专属；Uniform/Laplace/t5 残差下 σ̂/σ = 1.184/0.802/0.865（偏差 ≤±18%）
    # 实现取 lo=int(0.1m)、hi=int(0.9m)（dpsf_psf.cpp），两端裁剪不对称 ⇒ 有限 m 偏低：
    # m=121 −0.98%、m=169 −0.47%、m=441 −0.28%、m=1024 −0.11%；适用域 m≥441（|偏差|<0.3%）
    # 证据 实验/engineering-evidence/
F5: q_psf=A/residual_scale, q_psf为QA代理不进science weight
```

来源: `lib/algorithms/psf/src/dpsf_psf.cpp`（F1 残差 / F2 FWHM 系数 / F3 解析积分 / F4 截尾均值 / 常量）。
`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`

## 3 伪代码

> **阈值口径**：`5.0·σbg` 中的 `σbg` 是 `sdet_compute_bgnoise()` 的输出（行差分 FnNoise1 族），
> 即**未平滑原图**噪声；候选阈值 `thr = bg + 5.0·bgnoise`（`sdet_api.cpp` 的 `O3` 冻结值）。
> **引用「5σ」时必须声明 σ 属哪幅图**（同口径见 `STAR_DETECTION_ALGORITHMS.md` 与
> `docs/science/algorithms/common/GATES_AND_TOLERANCES.md` 门表两处限定）。

```text
function detect_centroid(image, sigma_bg):
  for each pixel: if image > bkg+5.0·σbg → candidate
  centroid weighted mean of 3×3 neighborhood
  if saturated median+0.7·dynrange reject

function fit_psf_moffat4(image, cx,cy, fitRadius):
  init B=bkg0（拟合窗内**下半区**截尾 MAD clip 后中位，单星/批量两路同源）、
       A=max_val−bkg0（A0≤0 ⇒ INVALID_PARAMS(2)）、x0=y0=0、
       sx=sy=sx0=0.15·rw（rw=rect 宽；**全文件无 1.2**）、θ=0
  LM 7参 Levenberg-Marquardt (dpsf_psf.cpp 的 lm_solve) max_iter=200 tol=1e-8（实参硬编码）
    J via finite diff, Δ=(JᵀJ+λI)⁻¹ Jᵀr, λ adaptive
  post: FWHM=1.230310·sx/sy, flux=2πA·sxsy/3
  θ消歧 4候选 {θ,π/2−θ,π/2+θ,π−θ} 取 trimmed-mad 最小 (dpsf_psf.cpp)
  residual_scale = trimmed mean |image−model| (10–90%)
  q_psf = A / residual_scale
  guards: sx/sy ≤ 0.3 ⇒ **NO_CONVERGENCE(1)**（dpsf_psf.cpp；不是 INVALID_PARAMS(2)），
          LM 内步后钳位 x[4]/x[5] < 0.3 → 0.3（同文件）；MAD==0 不换算 scale；平坦星 reject

function psf_block_batch(image, n_stars):
  parallel for each star: fit_psf_moffat4 independent

Batch deterministic: input order fixed, per-star independent, reduction none cross-star.
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 图像含 NaN/Inf | **逐像素**跳过（采样阶段 `isfinite` 过滤，`dpsf_psf.cpp`），其余像素正常拟合；**整窗**全部非有限 ⇒ `status=DPSF_FIT_INVALID_PARAMS`(2)（同一文件，`DPSF_STAGE_ALL_NONFINITE`）。状态码域 = {0,1,2,3}，无 `BAD` 码 |
| `max−B ≤0` | reject `A0≤0` ⇒ `INVALID_PARAMS`(2)（`dpsf_psf.cpp`，`DPSF_STAGE_AMPLITUDE_LE0`） |
| `sx/sy ≤ 0.3` | 验证链一 ⇒ `DPSF_FIT_NO_CONVERGENCE`(1)（`dpsf_psf.cpp`）；**不是** INVALID_PARAMS(2) |
| `MAD==0` | `robust_residual_sigma` 不换算，q_psf仍计算 |
| 平坦星 `‖∇‖→0` | LM 奇异 → λ×10 重试 → 无进展中止（`kDpsfStallRel=1e-6`，判据均在 `dpsf_psf.cpp`）⇒ `DPSF_FIT_ITERATION_LIMIT`(3)（同文件） |
| 饱和 `median+0.7·dynrange` | 掩膜 reject (star_detector.h) |

## 5 确定性与归约

- 每星独立 LM，无跨星归约；OpenMP tile/块并行按输入索引固定顺序；浮点归约仅 per-star `JᵀJ` 7×7 矩阵，顺序固定。

## 6 时间/空间复杂度

- O(pixels) 检测 + O(n_stars × iter × patch) 拟合；空间 O(patch) per thread

## 7 CPU-only 后端策略（V5）

- 仅 CPU：逐星独立 LM 拟合，worker pool（按 affinity）按星批并行，**线程数取自 benchmark profile**；per-star 结果与线程调度无关（无跨星归约）。

## 5c SIMD 安全与取消点

- 同星窗口内逐像素 residual 为逐元素算术(SIMD 安全: 无别名/行连续)；LM 内的 normal-equation 累加为**该星内固定顺序归约**(窗口行序)，禁重结合；FP32/FP64 IEEE-754, 禁 fast-math。
- 取消点: 按星批粒度检查; 取消时未完成星不写结果(调用方以 status 判别)。

## 8 参考实现/Oracle

- 合成 Moffat 图像 (已知 B,A,σ,θ) 恢复测试 PSF-001..008 位置 ≤0.05px FWHM ≤1%；残差 Gaussian 假设校验 trimmed-mean 0.7316727929211932 复算。

## 9 容差来源

- 位置: FP64 LM 0.05px (sub-pixel 插值误差)；FWHM: 1% (离散化 + α² 缩放)；预冻结。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` OpenMP per-tile
- API: `dynamic_psf.h: dpsf_fit, dpsf_fit_batch`, `star_detector.h: sdet_detect`
- TST: `TST-PSF-001` 合成恢复, `TST-PSF-INV` q_psf解耦, `TST-PSF-FAIL` 饱和/平坦拒

## 11 冻结附录（SRC-PSF-001 源码实测）

> 本节为冻结附录：只登记现状与测试设计，不改[S-1]科学公式。
> 实测基准 = `lib/algorithms/psf/src/dpsf_psf.cpp`（**1432 行**；原文「934 行」为旧稿）与
> `lib/algorithms/psf/include/dynamic_psf.h`。模块合同入口 =
> `lib/algorithms/psf/README.md`（r1）+ `lib/algorithms/psf/module.yaml`（CONTRACT_READY）。

### 11.1 实现锚（SRC-PSF-001，VERIFIED）

| 符号/语义 | 锚 |
|---|---|
| moffat4 残差（F1 离散式实现，β=4） | `moffat4_residual`（`dpsf_psf.cpp`） |
| `lm_solve`（LM 主循环，λ 初值 1e-3；奇异→λ×10；步后钳位 x[4]/x[5]≥0.3；收敛判据 ‖Δx‖<tol·(‖x‖+1e-30)；无进展中止判据） | `dpsf_psf.cpp` |
| `compute_trimmed_mad`（实为 10–90% 截尾均值 \|残差\|，即 F4 residual_scale；lo=int(0.1m)/hi=int(0.9m)，lo≥hi 回退中位） | `dpsf_psf.cpp` |
| `moffat4_fit_tmpl`（薄包装）/ `moffat4_fit_tmpl_core`（采样→初值→LM→验证链→θ 消歧→派生量） | `dpsf_psf.cpp`（`moffat4_fit_tmpl` / `moffat4_fit_tmpl_core`） |
| 初值链 bkg0=下半区截尾 MAD clip 后中位 / A0=max_val−bkg0 / sx0=0.15·rw / params={bkg0,A0,0,0,sx0,sx0,0} | `dpsf_psf.cpp` |
| LM 调用 tol=1e-8 / max_iter=200（硬编码实参） | `dpsf_psf.cpp` |
| 验证链一：非有限/A≤0/sx≤0.3/sy≤0.3 ⇒ NO_CONVERGENCE(1) | `dpsf_psf.cpp` |
| 验证链二：FWHM>rect 尺寸 ⇒ NO_CONVERGENCE(1) | `dpsf_psf.cpp` |
| 验证链三：背景约束 \|B−bkg0\|/max(bkg0,0.01)>0.5 ⇒ NO_CONVERGENCE(1) | `dpsf_psf.cpp` |
| θ 消歧 4 候选 {θ, π/2−θ, π/2+θ, π−θ} trimmed-MAD 最小（§3 一致） | `dpsf_psf.cpp` |
| F3 flux=2π·A·sx·sy/3（Moffat4 解析积分） | `dpsf_psf.cpp` |
| FWHM=1.230310·sx/sy（F2 系数 MOFFAT4_FWHM_FACTOR） | `dpsf_psf.cpp`（常量同文件） |
| eccentricity=√(1−(smin/smax)²)；img_cx=cx+x0 | `dpsf_psf.cpp` |
| `gauss_solve_buf`（法方程求解） | `dpsf_psf.cpp` |
| `moffat4_fit`/`moffat4_fit_d`/`moffat4_fit_batch_cell_f`/`_d`（薄包装） | `dpsf_psf.cpp`（四组薄包装） |
| C API 导出（7 个）：dpsf_fit / dpsf_fit_batch / dpsf_fit_batch_f / dpsf_free_results / dpsf_fit_batch_d / dpsf_fit_batch_f32 / dpsf_fit_batch_f64 | dpsf_psf.cpp |
| star_det v1 `FLOAT64[N,6]` / `psf_params:FLOAT64[N,9]` schema 宏 | `dynamic_psf.h` |
| 错误码 OK=0 / NO_CONVERGENCE=1 / INVALID_PARAMS=2 / ITERATION_LIMIT=3 | `dynamic_psf.h` |
| 批拟合 OpenMP `schedule(dynamic) reduction(+:success_count)` 4 处 | dpsf_psf.cpp |

§3 伪代码与实测一致：LM 实参 tol=1e-8、max_iter=200（dpsf_psf.cpp 调用点，硬编码）；
`DPSFFitParams.maxIter/tolerance` 字段不被消费（DISP-PSF-003）。[S-1]「参数与常数」一节 NaN/Inf 行为 =
**逐像素**采样过滤 + 整窗全非有限时 `INVALID_PARAMS`(2)（均在 dpsf_psf.cpp）；
状态码域 = {0,1,2,3}（dynamic_psf.h）。[S-1]「参数与常数」一节 饱和掩膜 reject 属 star_detector 侧，
dynamic_psf 不消费饱和列 [4]/[5]（列注释见 dpsf_psf.cpp；两处批循环均不索引 detections 第 4/5 列）。

### 11.2 拟合失败语义（冻结，P1-PSF-TEST 逐码负例）

| 码 | 宏 | 触发（实测锚） | 单星接口（dpsf_fit/moffat4_fit） | 批接口（f32/f64 [N,9]） |
|---|---|---|---|---|
| 0 | DPSF_FIT_OK | 收敛判据且过验证链一~三（均在 `dpsf_psf.cpp`） | 全参数回填（12 字段，`dpsf_psf.cpp`） | 计入 out_n_valid；写 9 字段（f32 / f64 两处），compact 左移（同一文件） |
| 1 | DPSF_FIT_NO_CONVERGENCE | 验证链一 / 二 / 三（`dpsf_psf.cpp`） | result 已 memset 0 +status（同一文件） | 不计入 compact 行；逐星 out_status=1（失败星一律不占参数行） |
| 2 | DPSF_FIT_INVALID_PARAMS | 空指针/w≤0/h≤0 与 `dpsf_batch_dims_check`；rect 面积<9；rect 越界；空 rect（均在 `dpsf_psf.cpp`） | 同上 | 批整体 -1（等 4 个批入口），不触碰输出 |
| 3 | DPSF_FIT_ITERATION_LIMIT | max_iter=200 耗尽或无进展中止（`dpsf_psf.cpp`） | 仍回填当前最优参数（同一文件） | 非 OK→NaN，不计 valid |

`gauss_solve_buf` 奇异（λ×10 重试）不单独出码，最终由收敛判据归类（均在 `dpsf_psf.cpp`）。
简并兜底：验证链一 sx/sy>0.3 判定 + 步后钳位；θ 对称简并由
4 候选消歧确定性回选（同文件），不产生不确定状态。

### 11.3 DISP-PSF-001..006（登记不改码，整改归 P1-PSF-IMPL/INT）

| ID | 内容 | 锚 |
|---|---|---|
| DISP-PSF-001 | 参数向量序 B,A,x0,y0,sx,sy,theta 与 MOFFAT4_FWHM_FACTOR=1.230310 常数依赖，重构时序耦合 | `dpsf_psf.cpp`（`NPARAMS=7` 同文件） |
| DISP-PSF-002 | 前向差分雅可比（`h=max(\|x\|·1e-6,1e-8)`）+ 步后硬钳位破坏二阶收敛路径 | `dpsf_psf.cpp` |
| DISP-PSF-003 | `DPSFFitParams.maxIter/tolerance` 死参数（LM 硬编码 1e-8/200；字段仅在 f32/f64 入口赋默认值后无消费者） | `dpsf_psf.cpp` + 头 `dynamic_psf.h` |
| DISP-PSF-004 | 无取消检查点（OpenMP dynamic 4 处批拟合不可中断） | `dpsf_psf.cpp` |
| DISP-PSF-005 | 无参数协方差/不确定性输出（科学专项 covariance 缺口，P1-PSF-IMPL 落地） | DPSFFitResult 12 字段 `dynamic_psf.h` |
| DISP-PSF-006 | 批 f32/f64 路径逐星退败静默（仅 out_n_valid 汇总，per-star 状态不出批）——批 f32/f64 提供可选逐星 `out_status`（`psf_status:INT32[N]`），成功行 compact，星 ID↔行映射由状态真值唯一判定 | `dynamic_psf.h` |

### 11.4 TEST-PSF-DESIGN-001（测试设计，P1-PSF-TEST 执行）

- unit：4 状态码逐码负例（[D-1]冻结表）；rect 面积<9/越界/空 rect；饱和列不消费断言。
- oracle：解析 Moffat4（β=4）合成图回收 B,A,cx,cy,sx,sy,θ；flux=2πAsxsy/3 与
  FWHM=1.230310·σ 恒等复核；独立参考不调用生产 symbol（11 号标准 §5）。
- property：θ 消歧确定性（同输入同 θ 回选）；eccentricity∈[0,1)；批输出成功行
  compact 与 out_status/out_n_valid 一致（3 星中间一颗失败 → row0↔星0、
  row1↔星2，无 NaN 洞）；per-star 独立性（打乱星序不改变逐星结果）。
- boundary：fitRadius 裁边 clamp（`dpsf_psf.cpp`）；FWHM≈rect 边界；背景约束 0.5 阈值
  边界；max_iter 边界（ITERATION_LIMIT 仍回填）。
- performance：1/N worker 缩放、provider=baseline、确定性重跑一致。
- 容差来源：fixtures generator 注记（11 号标准 §5 元数据），不用本文手抄值。
- 状态：VERIFIED（设计冻结，dpsf 套件建立后 TEST-P1-PSF-001 落 EVIDENCE）。

### 11.5 SCI-P1-PSF-001 状态声明

科学专项（matrix P1-PSF 行）映射：known Gaussian/Moffat parameters=[S-1]「物理模型」一节公式 + 「判据与误差」一节
参数序/初值/常量锚；fit failure semantics=[S-1]「判据与误差」一节（四码语义冻结，无含糊）；degenerate/
saturated=§11.2 简并兜底 + §11.1 饱和列不消费登记（P1-PSF-TEST 专项）；covariance=
现状缺失，DISP-PSF-005 显式登记为 P1-PSF-IMPL 整改项，状态词停在登记态。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/psf/PSF.md（PSF 建模科学正本 SCI-PSF-001）。

## 参考文献与参考代码库（含许可证）

- Moffat 轮廓：Moffat 1969, A&A 3, 455（bibcode 1969A&A.....3..455M）。
- β=4 解析通量/FWHM 因子：Project-defined 解析积分（可用 SciPy/sympy 复算）。
- LM：Levenberg 1944；Marquardt 1963；Moré 1978；实现对照 GSL（GPL-3.0）。
- 空间变异 PSF/采样基：Bertin 2011, ASPC 442, 435（PSFEx）；photutils（BSD-3-Clause）MoffatPSF。现状不做空间变异（DISP-PSF-005）。
- 拥挤场 PSF 测光：Stetson 1987, PASP 99, 191。
- q_psf/residual_scale：Project-defined 质量代理，非 SNR/非 Fisher information。
