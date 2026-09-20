# PMM-STUDY — PhotometricMosaic 方法研究（RELEASE-02 参考研究分片）

> **分片**：PMM-STUDY（只读研究）
> **负责人令**：「能正确拟合就可以。pmm 脚本是如何做的？你可以参考参考」
> **研究对象**：`run/RELEASE-02/pmosaic/PhotometricMosaic/`（PhotometricMosaic v4.0.2，John Murphy）
> **研究基线**：git HEAD `06216ec86a7cdd8c2342a2f76c7dab0d701a8d19`
> **证据**：`run/RELEASE-02/pmm-study/evidence/pmm-evidence.txt`（1793 行，E0–E15，逐条 file:line + 原文摘录 + sha256 指纹）
> **约束遵守**：零生产代码/文档改动；零 git 写；未运行 ninja/cmake/ctest；`TMPDIR=/dev/shm/astrocs_pmm`

---

## 0. 执行摘要

| 问题 | PMM 的做法（一句话） | 对我们的结论 |
|---|---|---|
| **Q-A 收敛/拟合质量** | **PMM 根本没有迭代求解器，因此根本没有收敛判据。** 它用的是闭式最小二乘 + 闭式 SurfaceSpline 一次求解；"拟合好不好"由**结构前置条件（样本数/星数）**、**固定比例的确定性剔除**、**唯一的尺度归一化（线性区 = 0.7×本帧最大值）**和**人工看图**四件事承担。**全文 `converg` 0 命中，`chi2` 0 命中，无 R²、无残差 RMS、无 χ²。** | 我们的 `tol=1e-6` 绝对门是**范式错误**：PMM 的教训是"判据必须自归一化/无量纲"。但 PMM 本身**没有**可抄的停止判据 —— 应改用**无量纲标准化残差**判据（UPM 循环里已有 `z = r/max(\|uncertainty\|, sigma_floor)`），并把「收敛」与「拟合质量」**拆成两个独立状态**。可引用的第一手依据是 **MINPACK `lmdif` 的 ftol/xtol/gtol**（相对判据）。 |
| **Q-B 多帧一致性** | **没有任何"拒绝帧"的跨帧一致性门。** 唯一用到"跨帧比例"的地方是**星点匹配的预筛**（三段式：tol=32 粗估 → tol=4 精估 → tol=用户值终匹配），它**拒绝的是匹配对，不是帧**，且失败时**降级**（无星 → 用重叠区均值/中值差估计梯度）而非报错。唯一的 fail-closed 前置门是"图像是否被 TrimMosaicTile 修过边"，且**可点 Ignore 覆盖**。PMM 语义是**纯相对**（`absolute` 全文 0 命中）。 | 与负责人裁决**高度一致**。组间 `k` 散度门（`module_adapters.cpp:3514-3524`）应当**删除**，只保留**帧内**判据。PMM 的三段式"粗估→精估→终筛"是**收敛式的匹配策略**，比单一固定阈值稳健得多，值得借鉴到我们的星表引导匹配。 |
| **Q-C 星点通量口径** | **纯孔径测光，没有 PSF 拟合。** 通量 = 星检测结构像素和 − 中值背景 × 像素数；背景 = **矩形环带中值**（不是 annulus 平均、不是全局）；孔径 = **两星包围盒并集 + 按最亮星通量线性膨胀**；参考星选择 = **峰值线性区截断 + 负通量剔除 + 固定百分比离群剔除 + 1px 去重**。空间变化 = **重叠区上的加性差分 SurfaceSpline**（不是乘性增益场）。 | **与我们 A6 定案（PSF 域通量 D1/D2）方向相反** —— PMM 的孔径口径正是 A6 判定要淘汰的东西（`f-instr-canon.md` 实测 5×5 口径 M_seeing = 1.353 mag）。**不要**照搬 PMM 的口径；但它的**背景中值环带**与**膨胀孔径**思路对 A4 的 `m(x,y)` 有间接价值（见 §3.4）。 |

**最该先落地的一条**（详见 §5）：**Q-A —— 把绝对 `1e-6` 换成无量纲的「标准化残差相对变化 + 目标函数相对变化」双判据，并把 `converged` 从"通过/失败"改成"状态"。** 理由：它是三者中唯一**当前正在产生错误生产状态**的问题（永远 `converged=0`），而现行的 `tolerance_relative` 补丁把门放宽了 **18 个数量级**（`CONFORM-SWEEP-3.md:67`），是**反方向的同一个错误**。

---

## 1. 许可证与引用注意（**必须先读**）

### 1.1 PhotometricMosaic 本体：**禁止再分发、禁止修改**

`lib/LeastSquareFit.js:3-11`（**每一个** PhotometricMosaic 自研文件都有同一段头）：

```
 3: // ======== #license ===============================================================
 4: // This program is free for personal use only.
 5: // You may not redistribute or modify it. Download it from the official website:
 6: // https://astroprocessing.com/
 7: //
 8: // This program is provided in the hope that it will be useful, but WITHOUT
 9: // ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
10: // FITNESS FOR A PARTICULAR PURPOSE.
11: // =================================================================================
```

配套事实：
- 每个脚本都有 PixInsight 代码签名（`PhotometricMosaic.xsgn:8-11`，`developerId="John Murphy"`，`CreationTime 2025-01-22`）；
- `lib/HelpDialog.js:26-30` 自述商业模式为"免费 + 请我喝咖啡"，代码量自述 27,000 行；
- 该目录落在 `.gitignore:19`（`run/*`）之下 ⇒ **不在版本库里**，是本地研究副本。

**⇒ 硬性纪律（本分片全程遵守，后续引用者亦须遵守）：**
1. ✅ **可以**读源码做方法研究，并在报告里**引述其做法 + file:line + 原文摘录**（事实性引用 / 合理使用）；
2. ❌ **不得复制其代码**（任何形式：直接粘贴、逐行翻译、改改变量名）；
3. ❌ **不得把它当引用文献**（它不是可引用的学术来源；官方文档站 `astroprocessing.com` 为死链，无稳定 URL）；
4. ❌ **不得再分发**该目录或其任何部分。

### 1.2 目录内混有**第三方**代码，许可证**不同**

| 文件 | 权利人 | 许可证 | 证据 |
|---|---|---|---|
| `lib/StarDetector.jsh` | Pleiades Astrophoto S.L.（PJSR） | 类 BSD **+ 第 4 条强制致谢条款** | `lib/StarDetector.jsh:16-38`：`Copyright (c) 2003-2020 Pleiades Astrophoto S.L.`；第 4 条要求在产品文档中复现 "This product is based on software from the PixInsight project…" |
| `lib/PreviewControl.js` | Andres del Pozo / Juan Conejero | 类 BSD（双条款） | `lib/PreviewControl.js:16-30`：`Copyright (C) 2013-2020, Andres del Pozo` |

⇒ 即使只看 `StarDetector.jsh`，其条款也**不允许**在未加致谢的情况下并入我们的产品。**本分片未复制任何代码。**

### 1.3 可引用的**替代一手来源**（本次已独立核验）

| 主题 | 可引用来源 | 核验方式 |
|---|---|---|
| 拼接/重采样的事实标准实现 | **Bertin et al. 2002, "The TERAPIX Pipeline", ASP Conf. Ser. 281, 228** | `https://www.astromatic.net/software/swarp/` → "Acknowledging SWarp" 段落原文 |
| 拼接保测光（保位置+保强度） | **Jacob et al. 2010, "Montage: a grid portal and software toolkit for science-grade astronomical image mosaicking"**, arXiv:1005.4454（IJCSE 6(2):65–77） | `https://arxiv.org/abs/1005.4454` 标题/作者/摘要原文："The mosaics constructed by Montage preserve the astrometry (position) and photometry (intensity) of the sources in the input images." |
| 星检测/测光（PMM 用的 StarDetector 的学术对应物） | **Bertin & Arnouts 1996, A&AS 117, 393**（ADS bibcode `1996A&AS..117..393B`） | `https://www.astromatic.net/software/sextractor/` → "Acknowledging SExtractor"。⚠️ 该页把卷号误写为 "Supplement 317"，**正确卷号是 117**，引用时以 ADS bibcode 为准 |
| 非线性最小二乘的**相对**停止判据（Q-A 的核心依据） | **More, Garbow & Hillstrom, MINPACK-1, Argonne National Laboratory ANL-80-74 (1980)**；`lmdif` 源码 | `https://netlib.org/minpack/lmdif.f` 逐字：`ftol` = "the actual and predicted **relative** reductions in the sum of squares"；`xtol` = "the **relative** error between two consecutive iterates"；`gtol` = "the cosine of the angle between fvec and any column of the jacobian" |
| 前向全局测光标定（帧间独立标定的范例） | **Burke et al. 2018, AJ 155, 41, DOI 10.3847/1538-3881/aa9f22**（arXiv:1706.01542） | `https://arxiv.org/abs/1706.01542` → `Related DOI : https://doi.org/10.3847/1538-3881/aa9f22` |

**未能在本次核验、因此本报告不引用**（诚实登记）：
- **Stetson 1987, PASP 99, 191（DAOPHOT）** —— ADS 出口被 CAPTCHA 拦截（`ui.adsabs.harvard.edu` HTTP 405 "Human Verification"），本次**未独立核验**；
- **SWarp 的 `FSCALE` / `FSCALASTRO_TYPE` 关键字语义** —— 官方手册 PDF 路径 404、readthedocs 404、GitHub 仓库仅 `README.md` 无源码树，本次**未核验**。**不得**在未独立核验前把它当作"SWarp 支持相对/绝对光度匹配"的出处。

---

## 2. Q-A：收敛 / 拟合质量判据

### 2.1 PMM 的**具体做法**

#### (a) 没有迭代求解器 ⇒ 没有收敛判据

```
$ grep -rn -i "converg\|iterat" run/RELEASE-02/pmosaic/PhotometricMosaic
run/.../lib/StarLib.js:1334:    P.layers = [// enabled, biasEnabled, bias, noiseReductionEnabled, ..., noiseReductionIterations
```
**`converg` 0 命中**；`iterat` 仅 1 命中且是 `MultiscaleLinearTransform` 的参数名，与求解无关。
⇒ **PMM 主路径不存在"迭代—判收敛—再迭代"的循环，因此不存在收敛容差这个量。**

#### (b) 最小二乘是**闭式解**，一次算完

`lib/LeastSquareFit.js:25-31`（注释即公式）：
```
25: /**
26:  * This object calculates Least Square Fit
27:  * y = mx + b
28:  * m = (N * Sum(xy) - Sum(x) * Sum(y)) /
29:  *     (N * Sum(x^2) - (Sum(x))^2)
30:  * b = (Sum(y) - m * Sum(x)) / N
31:  */
```
`lib/LeastSquareFit.js:56-71`：`n_>1` 直接代数求解；`n_==1` / `n_==0` **只打 warning 并返回退化值**（`sumY_/sumX_` 或 `m=1,b=0`），**不失败、不报错、不设阈值**。

#### (c) "迭代"只出现在**离群剔除**，且次数由**用户百分比**预先决定

`lib/StarLib.js:627-645`：
```
630:        let removeN = Math.round(starPairs.length * data.outlierRemovalPercent / 100);
631:        // Remove outliers
632:        for (let i=0; i<removeN; i++){
633:            if (starPairs.length < 4){
634:                    console.warningln("Channel[" + channel + "]: Only " + starPairs.length +
635:                    " photometry stars. Keeping outlier.");
636:                break;
637:            }
638:            let linearFitData = calculateScale(starPairs);
639:            starPairs = removeStarPairOutlier(starPairs, linearFitData);
640:        }
```
⇒ 循环次数 = `round(n × outlierRemovalPercent/100)`（默认 2%，`PhotometricMosaic.js:28`），**不是"直到收敛"**。唯一的提前退出条件是 `starPairs.length < 4`，且**只是 warning + break**。

#### (d) 剔除判据是**垂距**，不做 σ 归一、不算 χ²

`lib/StarLib.js:914-933`：
```
922:        let perpDist = Math.abs(
923:                (linearFit.m * x - y + linearFit.b) / Math.sqrt(linearFit.m * linearFit.m + 1));
924:        if (perpDist > maxErr){
925:            maxErr = perpDist;
926:            removeStarPairIdx = i;
```
⇒ **每轮剔除"到拟合线垂距最大"的 1 个点**。注意 `x`、`y` 是**原始通量**，垂距因此**带通量量纲**；它只用于**排序**（取最大），所以量纲不影响结果 —— 这**恰好回避了**我们踩的绝对阈值坑。

#### (e) "拟合成功"的判据：**结构前置条件 + 计数告警 + 人工看图**

| 判据 | 位置 | 行为 |
|---|---|---|
| 对星数 `>3` 才安静；否则 warning | `PhotometricMosaic.js:336-351` | `console.warningln(text)`，**继续执行** |
| 无星 → 从图像估计梯度 | `PhotometricMosaic.js:340-341` + `lib/LeastSquareFit.js:101-139` | `estimateGradient`：用重叠区 `(mean−median)` 之比，**降级不报错** |
| 对星数 `<6` → **强制过原点单参数拟合** | `lib/StarLib.js:888-906` | 减少自由度，**提高稳健性**（不是提高精度） |
| 样条样本 `<3` → **硬错误** | `PhotometricMosaic.js:352-362` | `"Error: Too few samples to create a Surface Spline."` + `return` |
| 样条对象 `isValid==false` → 抛异常 → MessageBox | `lib/Gradient.js:206-235` + `PhotometricMosaic.js:436-446` | **唯一的"拟合失败"路径，且与残差大小无关** |
| **拟合质量** | —— | **不存在**。全文 `chi2`/`chiSq`/`reduced` **0 命中**；`rms` 命中全是 `Redistribution`/`errMsg` 假阳性；`stddev` 仅 1 命中且在第三方 `StarDetector.jsh:366`（质心矩阵截断） |

#### (f) 尺度归一化：**有，但只有一处**

`lib/Cache.js:22-25, 54-85`：
```
23:    /** Linear range: Auto default assumes 70% of image's maximum value */
24:    let linearRangeRef = LINEAR_RANGE;
...
65:                linearRangeRef = Math.round(1000 * refView.image.maximum() * LINEAR_RANGE) / 1000;
...
73:                linearRangeTgt = Math.round(1000 * tgtView.image.maximum() * LINEAR_RANGE) / 1000;
```
（`LINEAR_RANGE = 0.7`，`PhotometricMosaic.js:41`）

`lib/DialogControls.js:208-214` 原文：
```
208:        toolTip: "<p>Restricts the stars used for photometry to those " +
209:            "that have a peak pixel value less than the specified value.</p>" +
210:            "<p>Use this to reject stars that are outside the " +
211:            "camera's linear response range.</p>" +
212:            "<p>The default value is set to 0.7 x the highest value in the image. " +
213:            "If the image does not contain any saturated stars, this may be an " +
214:            "underestimate.</p>"
```
⇒ **唯一被显式归一化的量是"线性区"**，而且归一化基准是**本帧图像自身的最大值**（逐帧自适应），**不是固定绝对数**。这是我们最该学的一条。

#### (g) 收敛 vs 拟合质量：**没有分成两个判据，因为一个都不存在**

PMM 用**另一条完全不同的路径**替代了两者：**把所有中间量画成图，交给人看**。
- 光度图：`lib/StarLib.js:967-1162` `displayStarGraph`（ref flux vs tgt flux + 拟合线）；
- 梯度图：`lib/GradientGraph.js:690-847`（沿接缝坐标的差值曲线 + 样本点）；
- 检测星图 / 测光星图 / 掩膜星图：`DetectedStarsDialog.js` / `PhotometryStarsDialog.js` / `MaskStarsDialog.js`。

梯度图曲线的构造（`lib/GradientGraph.js:745-771`）是 **17 点滑动平均**（`plusMinusRange=4`，`useMedian=false`），再 Akima 插值 —— **纯可视化，不参与任何判决**。

### 2.2 对 UPM 的**具体建议**

#### 现状（已核验）

| 位置 | 内容 |
|---|---|
| `lib/algorithms/coverage/src/upm.cpp:249` | `cfg.tolerance = 1e-6;`（默认） |
| `upm.cpp:991-1006` | `tol_M = cfg.tolerance; tol_C = cfg.tolerance;` … `if (max_dM < tol_M && max_dC < tol_C) { m->converged = 1; break; }` |
| `upm.cpp:993-1001` | `tolerance_relative=1` 时 `tol_M = cfg.tolerance * max(scale_M, 1.0)`，`scale_M = max\|M\|` |
| `upm.cpp:982-983` | `m->objective += raw_w[i] * huber_rho(r / sigma_eff, cfg.huber_delta);`（**χ² 型，无量纲**） |
| `upm.cpp:2161-2173` | `ma_objective`：同型（GN-IRLS 路径） |
| `upm.cpp:2385-2410` | GN-IRLS：`if (maxstep < cfg.tolerance) break;`（**同一个绝对 1e-6**） |
| `reports/RELEASE-02/c-delta-ruling.md:235-236` | `ULP(max\|M\|=3.09e15) = 0.5`；`ULP(max\|C\|=4.96e14) = 0.0625` |
| `reports/RELEASE-02/conformance/CONFORM-SWEEP-3.md:67` | 生产 `tol=1e-3, tolerance_relative=1` ⇒ 实际门 `≈3e12 ADU`，比冻结门宽 **18 个数量级**；且 `grep -rn "tolerance_relative" docs/ contracts/` = **0 命中** |

#### 诊断：**两个方向上的同一个错误**

- **原判据（绝对 1e-6）**：要求 `max|ΔM| < 1e-6`，而 `M` 的 ULP 是 `0.5` ⇒ 需要 **21 位有效数字**。**原理不可达**，`converged` 恒 0。
- **现行补丁（相对 1e-3 × max\|M\|）**：门变成 `3.09e12`。相邻两次迭代的 `ΔM` 只要小于 `3e12`（即相对变化 < 0.1%）就"收敛" —— 这**不是收敛判据，是"随便停"**。

**根因**：拿**有量纲的参数**（`M`、`C`，单位是 ADU/F_syn）去比一个**纯数**。PMM 从不这么做：它要么比无量纲量（线性区比例、通量比），要么只做排序（垂距）。**这是 PMM 给我们的真正教训，不是它的具体阈值。**

#### 建议（**需走 SCI/合同变更 claim**，因 `FZ-UPM-CONVERGENCE` 冻结）

**建议 1（核心）：改用**无量纲**判据 —— UPM 循环里已经现成有 `z`。**

`upm.cpp:735-739` 已经算了：
```cpp
const double r = obs[i].value - M[ck] - m->C[m->frame_index[obs[i].frame_id]][ck];
const double sigma_eff = std::max(std::fabs(obs[i].uncertainty), cfg.sigma_floor);
w[i] = raw_w[i] * huber_w(r / sigma_eff, cfg.huber_delta);
```
⇒ `z_i = r_i / sigma_eff,i` **天然无量纲、天然跨帧可比、天然与 `M` 的量级无关**。

建议停止判据：
```
(1) 步长：  max_dM / max(scale_obs, eps) < tol_step      且  max_dC / max(scale_obs, eps) < tol_step
(2) 目标：  |obj_new − obj_old| / max(|obj_old|, eps) < tol_obj
(3) 质量：  rms_z = sqrt( Σ w_i z_i² / Σ w_i )  ≤ q_max   且  收敛后不再显著下降
```
其中 **`scale_obs` 应取"观测量的尺度"（如 `max|obs.value|` 或 `median sigma_eff`），而不是"参数的尺度" `max|M|`** —— 这是现行补丁最本质的毛病：`M` 是**解**，用解的尺度去归一化步长，等价于"解越大越容易收敛"。

**建议 2：把 `converged` 从"通过/失败"改成"状态"。**
现在 `upm.cpp:98-99` 是 `int converged{0};`，语义 `0 = 迭代耗尽`。建议改为枚举：
```
0 = max_iter（未达步长门，但目标函数已平稳）
1 = converged（步长门 + 目标门均达）
2 = stalled（目标函数连续 K 轮不再下降 —— 数值上已到机器精度）
3 = invalid（出现非有限值 / Cholesky 持续失败）
```
**理由**：`2 = stalled` 恰恰是 `M~3e15` 场景下的**正常且正确**的终态。把它和"未收敛"混为一谈，是当前"永远报未收敛"的直接原因。**"没到 1e-6"不等于"拟合错了"** —— 这正是 PMM 的立场（`LeastSquareFit.js:63-69`：只有 1 个点也照样出结果，只 warning）。

**建议 3：拟合质量判据必须独立于收敛判据（这是 PMM 明确缺失、我们必须补的）。**
PMM 用"人眼看图"代替质量判据；我们没有 GUI，所以**必须把它机器化**。可用的、**尺度无关**的候选（按推荐序）：

| 候选 | 定义 | 尺度无关？ | 已有实现？ |
|---|---|---|---|
| **a. 标准化残差 RMS** | `rms_z = sqrt(Σ w z² / Σ w)`，`z=r/max(\|σ\|,σ_floor)` | ✅ 无量纲 | ✅ 循环内已有 `z` |
| **b. 有效样本保留率** | `Σw / Σraw_w`（Huber 降权比例） | ✅ 比值 | ✅ `raw_w`/`w` 都在 |
| **c. 帧内 `k` 的 MAD** | 逐帧标定散度 | ✅ dex | ✅ `sigma_residual_dex` 已存在 |
| **d. 帧间残差 mean\|Δ\|** | `fix-regress.md:39` 实测 448.018 | ❌ **有量纲** | ✅ 但**不能**做门 |

⇒ **建议把 (a)(b)(c) 作为"拟合质量"三元组写进 `p2_upm_model.json`**（现在只写了 `iterations/converged/objective`，`upm.cpp:1190-1202`），让 `converged` 与 `quality` 在产物里**分开可见**。

**建议 4：数值论证（给变更 claim 用）**

| 量 | 值 | 含义 |
|---|---|---|
| `max\|M\|` | `3.09e15` | 生产实测（`c-delta-ruling.md:235`） |
| `ULP(max\|M\|)` | `0.5` | `M` 可表示的最小间隔 |
| 绝对门 `1e-6` | `1e-6` | 比 ULP 小 **5.7 个数量级** ⇒ **不可达** |
| 现行门 `1e-3 × 3.09e15` | `3.09e12` | 相对变化 0.1% 即停 ⇒ **过松 18 个数量级** |
| `rms_z` 的合理量级 | `O(1)` | Huber `delta=1.345` 下，健康拟合 `rms_z ≲ 1.5` |
| `rms_z` 的相对变化门 `1e-3` | `~1.5e-3` | **物理可达**（因为 `rms_z` 是 O(1)，1e-3 相对变化 ≫ ULP(1.5)=2.2e-16） |

⇒ **无量纲判据把"21 位有效数字"变成"13 位有效数字"，从不可达变成轻松可达**，且**判据本身有物理意义**（"标准化残差不再改善"）。

**建议 5：与 MINPACK 对齐（可引用的第一手依据）。**
MINPACK `lmdif`（`https://netlib.org/minpack/lmdif.f`，本次已逐字核验）用的正是这个范式：
```
ftol: "termination occurs when both the actual and predicted RELATIVE
       reductions in the sum of squares are at most ftol"
xtol: "termination occurs when the RELATIVE error between two consecutive
       iterates is at most xtol"
gtol: "termination occurs when the cosine of the angle between fvec and
       any column of the jacobian is at most gtol in absolute value"
```
⇒ **两个相对判据 + 一个无量纲正交性判据，且三个判据各自独立报告**（`info` 返回 1/2/3/4 区分是哪一个触发的）。**这正是我建议 UPM 采用的形态**，而且它有 40 年的第一手文献支撑，**不需要引用 PhotometricMosaic**。

---

## 3. Q-B：多帧 / 多板块一致性

### 3.1 PMM 的**具体做法**

#### (a) 两幅图的重叠区：非零像素交集

`lib/Geometry.js:676-723`（节选）：
```
705:        for (let i=0; i<bufLen; i++){
706:            let isOverlap = true;
707:            for (let c = nChannels - 1; c > -1; c--) {
708:                if (tgtBuffer[c][i] === 0 || refBuffer[c][i] === 0) {
709:                    isOverlap = false;
710:                    break;
711:                }
712:            }
713:            if (isOverlap) {
714:                maskBuffer[i] = 1;
```
⇒ **重叠 = ref 与 tgt 在任一通道都非零的像素**（黑边即无效区）。

`PhotometricMosaic.js:133-137`：无重叠 ⇒ **硬错误 + return**：
```
133:        if (!overlap.hasOverlap()){
134:            let errorMsg = "Error: <b>" + referenceView.fullId + "</b> and <b>" + targetView.fullId + "</b> do not overlap.";
135:            new MessageBox(errorMsg, TITLE(), StdIcon_Error, StdButton_Ok).execute();
136:            return;
```

#### (b) 跨帧一致性：**只有"匹配对预筛"，没有"帧拒绝门"**

`lib/StarLib.js:409-448` —— **三段式**（这是 PMM 最精妙的一处）：
```
432:    // Get a very rough estimate of gradient from the 50 brightest photometry stars
433:    // Large tolerance allows matching images from 16 bit and 12 bit sensors, with up to 2x scale dif
434:    let estimateArray = matchStars(quadTree, rStars, searchRadius, 1, 32, 50);
...
439:    // Refine the estimate by reducing tolerance
440:    estimateArray = matchStars(quadTree, rStars, searchRadius, estimatedGradient, 4, 50);
...
445:    // Create the StarMatch array with the refined gradient estimate
446:    let starMatchArray = matchStars(quadTree, rStars, searchRadius, estimatedGradient, fluxTolerance, 2000);
```
⇒ **`tol=32` 粗估 → `tol=4` 精估 → `tol=用户值(默认 1.5)` 终匹配**。注释明确说明"大 tolerance 是为了允许 16bit 与 12bit 传感器、以及最多 2× 的尺度差"。

`lib/StarLib.js:503-527`：
```
503:    let minGradient = gradient / tolerance;
504:    let maxGradient = gradient * tolerance;
...
524:            if (!isFluxTooHigh(rStar, tStar, maxGradient) &&
525:                    !isFluxTooLow(rStar, tStar, minGradient)){
526:                starMatchArray.push(new StarMatch(rStar, tStar));
```

**关键点：超限的后果是"这一对星不匹配"，不是"这一帧被拒绝"。** 每个被拒的星仍然留在池子里（只是这次没配上），下一轮用新的 `gradient` 重新评估。

#### (c) 门是**拒绝还是警告**？阈值是什么？

| 情形 | 位置 | 行为 | 阈值 |
|---|---|---|---|
| 匹配对亮度比超窗 | `StarLib.js:503-527` | **拒绝该匹配对** | `ratio ∈ [g/tol, g·tol]`，`tol` 默认 1.5，范围 1.01–2 |
| 拟合后按本次斜率再筛一次 | `StarLib.js:613-623` | **拒绝该匹配对** | 同上，`g` 换成本次拟合 `m` |
| 负通量星 | `StarLib.js:717-761` | **剔除该星**；若 `>100 颗` 且 `>5%` ⇒ **warning** | `flux ≤ 0` |
| 检测星不可靠 | `StarLib.js:852-882` | **仅 warning**（"Warning: Invalid stars might have been detected."） | `hasUnreliableStars_` |
| 对星数 `≤3` | `PhotometricMosaic.js:344-348` | **仅 warning** | `nStarPairs > 3` |
| 对星数 `=0` | `PhotometricMosaic.js:340-341` | **仅 warning + 降级** | —— |
| 样条样本 `<3` | `PhotometricMosaic.js:356-359` | **硬错误**（结构性欠定） | `<3` |
| 未 Trim 修边 | `PhotometricMosaicDialog.js:1772-1805` | **Warning 对话框（Ignore/Abort）** | FITS HISTORY 里有无 `TrimMosaicTile` |

**⇒ 唯一的 fail-closed 前置门是"未 Trim"，而且用户可以点 Ignore 直接越过。除此之外，PMM 从不因为"质量差/不一致"而拒绝处理。**

#### (d) 不同仪器 / 不同透明度怎么处理？

- **不特殊处理，因为模型本身就是相对的。** `PhotometricMosaic.js:20-21` 的 feature-info 原文：
  ```
  20: #feature-info Creates mosaics from previously registered images, using photometry \
  21: to determine the brightness scale factor and a surface spline to model the relative gradient.
  ```
- 尺度因子 `scaleFactors[c].m` 是 **target/ref 的比值**，本身**允许任意量级**（PMM 从不对它做"合理范围"检查）；
- 12bit vs 16bit 传感器、2× 尺度差被**显式**在注释里点名支持（`StarLib.js:433`）；
- 唯一的兜底：若结果越界则截断并 warning（`PhotometricMosaic.js:575-601`），且**写进 FITS HISTORY**。

#### (e) `relative` vs `absolute` 的语义

**全文 `absolute` 0 命中。** PMM **没有**"absolute 光度匹配模式"。

| 概念 | PMM 的语义 | 位置 |
|---|---|---|
| 亮度尺度 | **纯相对**：`m = ref_flux / tgt_flux`（或过原点 `m = Σxy/Σx²`） | `LeastSquareFit.js:20-23, 79-88` |
| 梯度 | **纯相对**：差分曲面 `z = tgtMedian − refMedian` | `SampleGrid.js:31-33`；`Gradient.js:199-201` |
| 唯一的"绝对量" | **线性区**（`linearRangeRef/Tgt`），单位是图像自身的强度，且**自动取本帧最大值的 70%** | `Cache.js:65, 73` |
| 无星时的回退 | 用重叠区 `(mean − median)` 之比 | `LeastSquareFit.js:130-138` |
| provenance | 每帧的 `scale[c]` 与全部参数写进 FITS HISTORY | `FitsHeader.js:302-309` |

`lib/LeastSquareFit.js:130-138`：
```
130:    let refMedian = Math.median(refArray);
131:    let refMean = Math.mean(refArray);
132:    let tgtMedian = Math.median(tgtArray);
133:    let tgtMean = Math.mean(tgtArray);
134:    let refDif = refMean - refMedian;
135:    let tgtDif = tgtMean - tgtMedian;
136:    let m = (refDif > 0) && (tgtDif > 0) ? refDif / tgtDif : 1;
```

### 3.2 对 Phase2 的**具体建议**

**负责人裁决（`工程控制/RELEASE-02/GAP_AUDIT.md:1487-1495`）原文：**
```
1487: #### ✅ 定案 2：**帧间独立**，**删除组间 `k` 散度门**
1488: **负责人逻辑**：既然每帧独立用 Gaia 标定到**同一测光坐标系**，标定后所有帧已在同一体系；
1489: **跨帧 `k` 不同是正常的、正确的**（不同夜/望远镜透明度本就不同）⇒ **要求跨帧 `k` 一致 = 逻辑错误**。
1490: - **删除组间 `k` 散度门**；
1491: - **只做帧内极度异常值拒绝 + 抛错**；
1492: - **其他合理范围一律接受**；
1493: - **不同光学系统的帧混装不得报错**。
1495: 门只有一个：**单帧标定是否可信**（匹配星数、拟合残差、极度离群），**与其它帧无关**。
```

**PMM 的做法与这条裁决完全同构**：PMM 也**没有**任何跨帧一致性门；它的一致性保证来自"每幅图都被拉到同一个参考系（ref image）"，而不是"检查各幅图的因子是否接近"。**PMM 提供了独立佐证。**

#### 建议 1：**删除 `module_adapters.cpp:3514-3524` 的组间 `k` 散度门**

现状（已核验）：
```
3200:  constexpr double P1_PHOT_MAX_SPREAD_DEX = 0.02;  // ≈0.05 mag 峰峰（负责人判据）
...
3514:    if (scales_complete && kmin > 0.0) {
3515:      const double spread_dex = std::log10(kmax / kmin);
3516:      if (!std::isfinite(spread_dex) || spread_dex > P1_PHOT_MAX_SPREAD_DEX) {
3517:        photscale_error = "photscale inconsistent across frames (max/min=" + ...
3521:                          " dex); refusing to apply a mixed photometric system";
3522:        scales_complete = false;
```
⇒ 这条正是负责人所说的"**把不同光学系统的帧塞进来全报错**"。**按裁决删除。**

#### 建议 2：把"组间一致性"从**门**降级为**报告字段**（PMM 的 warning 范式）

PMM 对可疑但可解释的现象一律 **warning + 继续**（`StarLib.js:749, 878`；`PhotometricMosaic.js:341, 347, 600`）。建议：
- 保留 `spread_dex` 的**计算与落盘**（manifest / `photscale_detail`），供人工审阅与事后诊断；
- **删除它的 fail-closed 分支**；
- 若确实想保留提示，用 `CHK-WARN`（负责人已把预算上调至 180 s，`GAP_AUDIT.md:1497`），**不阻断**。

#### 建议 3：**帧内**判据保留并加强（对应裁决的"门只有一个"）

裁决要求"匹配星数、拟合残差、极度离群"。现状已具备：
```
3198:  constexpr int P1_PHOT_MIN_FIT_STARS = 3;
3199:  constexpr double P1_PHOT_MAX_SIGMA_DEX = 1.0;
```
⇒ 这两条是**帧内**的，**与裁决一致，保留**。可考虑按 `f-instr-canon.md:185` 的建议收紧 `MAX_SIGMA_DEX`（该文件建议 `MAD×1.4826 ≤ 0.03 mag`，对应 ≈0.013 dex；现 1.0 dex = 2.5 mag 确实过松）。**但这是 A6 的后续，不在本分片定案范围。**

#### 建议 4：借鉴 PMM 的**三段式匹配策略**（`StarLib.js:434-446`）

我们定案 1 要改成"星表引导拟合"（`GAP_AUDIT.md:1480-1485`）。PMM 的三段式正好是一个可直接借鉴的**稳健初值策略**：
1. 用**最亮的 N 颗**星、**极宽松**的窗口做粗匹配 → 得到粗略尺度；
2. 用粗尺度把窗口**收紧一个量级** → 重估尺度；
3. 用精尺度 + 用户窗口做**终匹配**。

⇒ 好处：**不需要预先知道两帧的透明度/口径差**，粗匹配自己会把尺度估出来。这直接服务于"不同光学系统混装"的场景。

#### 建议 5：**不要**照搬 PMM 的"无绝对检查"

PMM 是**交互式 GUI**，用户能立刻看到坏结果并撤销；我们是**批处理**，没有这个回路。所以：
- **PMM 不设绝对门**（因为它有人兜底）⇒ 我们**不能**因此也不设；
- 但**门的种类**应当学 PMM：**帧内、无量纲、可解释**。**帧间**的门（`k` 散度）应删除。

---

## 4. Q-C：星点通量口径

### 4.1 PMM 的**具体做法**

#### (a) **纯孔径测光，没有 PSF 拟合**

**全文无任何 PSF 模型拟合**（无 Gaussian/Moffat 拟合、无 PSF 星表）。通量定义在 `lib/StarDetector.jsh:383-395`：
```
383:      // Total flux, peak value and structure size
384:      for ( let i = 0; i < starPoints.length; ++i )
385:      {
386:         let p = starPoints[i];
387:         let f = image.sample( p.x, p.y );
388:         params.flux += f;
389:         if ( f > params.peak )
390:            params.peak = f;
391:      }
392:      params.size = starPoints.length;
```
⇒ **`flux` = 星检测结构内全部像素的直和**（**未扣背景**），`size` = 像素个数。

背景在 `StarDetector.jsh:352-362`：
```
352:      // Calculate the mean local background as the median of background pixels
353:      let r = rect.inflatedBy( this.bkgDelta );
354:      let b = [[],[],[],[]];
355:      image.getSamples( b[0], new Rect(    r.x0,    r.y0,    r.x1, rect.y0 ) );
356:      image.getSamples( b[1], new Rect(    r.x0, rect.y0, rect.x0, rect.y1 ) );
357:      image.getSamples( b[2], new Rect(    r.x0, rect.y1,    r.x1,    r.y1 ) );
358:      image.getSamples( b[3], new Rect( rect.x1, rect.y0,    r.x1, rect.y1 ) );
...
362:      params.bkg = Math.median( b[0] );
```
⇒ **背景 = 包围盒外扩 `bkgDelta`（默认 3）像素后的"回字形"区域的中值**（矩形环带，**不是圆环 annulus**，**不是全局**）。

净通量：`lib/StarLib.js:100-102`
```
100:    let _starRadius = Math.max(rect.width, rect.height) / 2;
101:    // Calculated star only flux (total flux - background flux)
102:    let _starFlux = flux - bkg * size;
```

#### (b) **统一孔径**：两星包围盒并集 + 按最亮星通量**线性膨胀**

`lib/StarLib.js:284-348`（`StarPair`）：
```
288:  * The star aperture is calculated by:
289:  * (1) The union of refStar's and tgtStar's original boundingBox.
...
292:  * (2) An aperture inflation is calculated, using the max flux from refStar and tgtStar.
...
317:        let maxFlux = Math.max(refStar.getStarFlux(), tgtStar.getStarFlux());
...
330:        this.getInflate = function (apertureAdd, apertureGrowthRate){
331:            return Math.round(calcApertureCorrection(apertureAdd, apertureGrowthRate, maxFlux));
332:        };
```
`lib/StarLib.js:280-282`：
```
280: function calcApertureCorrection(apertureAdd, growthRate, starFlux){
281:    return apertureAdd + growthRate * starFlux;
282: }
```
⇒ `inflate = round(apertureAdd + growthRate × maxFlux)`。默认 `APERTURE_ADD=1`、`APERTURE_GROWTH=0.25`（`PhotometricMosaic.js:35-36`）。
⇒ **关键设计**：**同一颗星在两帧里用同一个孔径**（由两帧中较亮者决定）。这**消除了孔径不一致**带来的系统差 —— 但**没有消除 seeing 依赖**（因为孔径本身随通量变）。

**注意（对 A6 至关重要）**：`lib/StarLib.js:25-40` 有一个**显式的生长上限**：
```
25: function calcDefaultGrowthLimit(data){
26:    let pixelAngle = calcDegreesPerPixel(data.pixelSize, data.focalLength);
27:    // 0.005 deg = 18 arcsec
28:    return 0.007 / pixelAngle;      // 25 arcsec
29: }
...
36: function calcDefaultTargetGrowthLimit(data){
...
39:    return 0.0375 / pixelAngle;     // 135 arcsec
40: }
```
⇒ PMM **已经意识到"孔径随通量膨胀"会失控**，所以用**角尺度上限**（25″ / 135″）把亮星的孔径封顶。这是对 A6 所批评的"孔径口径"的一个**补丁**，但**不是根治**。

#### (c) 背景环带的几何（`PmStar`，`lib/StarLib.js:156-271`）

```
161:    let _bgInnerRect = _starAperture.inflatedBy(gap);
162:    let _bgOuterRect = _bgInnerRect.inflatedBy(bgDelta);
...
185:    function calcBackgroundMedian(image, channel){
186:        let rects = [];
187:        rects.push(new Rect(_bgOuterRect.x0, _bgOuterRect.y0, _bgOuterRect.x1, _bgInnerRect.y0));  //top
188:        rects.push(new Rect(_bgOuterRect.x0, _bgInnerRect.y1, _bgOuterRect.x1, _bgOuterRect.y1));  // bottom
189:        rects.push(new Rect(_bgOuterRect.x0, _bgInnerRect.y0, _bgInnerRect.x0, _bgInnerRect.y1));  // left
190:        rects.push(new Rect(_bgOuterRect.x1, _bgInnerRect.y0, _bgOuterRect.x1, _bgInnerRect.y1));  // right
...
200:                    if (samples[i] > 0){ // don't include black pixels
201:                        allSamples.push(samples[i]);
...
206:        return Math.median(allSamples);
```
默认几何（`lib/StarLib.js:57-76`）：
```
57:  * Set the outer photometry aperture thickness to 70 microns on the detector
62: function calcDefaultApertureBgDelta(data){
63:    return Math.round(70 / data.pixelSize);
...
67:  * Set the gap between photometry aperture rings to 1.8 arcsec (0.0005 degrees)
71: function calcDefaultApertureGap(data){
74:    let gap = Math.round(0.0005 / pixelAngle);
75:    return Math.max(1, gap);
```
⇒ **环带厚度 = 70 µm（物理单位，按像素尺寸换算）**；**间隙 = 1.8″**。

`lib/StarLib.js:214-232` 的净通量与有效性：
```
222:        for (let i = 0; i < length; i++) {
223:            if (samples[i] > 0) {
224:                flux += samples[i];
225:                nSamples++;
226:            }
227:        }
228:        let bg = calcBackgroundMedian(image, channel);
229:        let starFlux = flux - bg * nSamples;
230:        _fluxOk = (nSamples === length && starFlux > 0);  // false if star rect contained black samples
```
⇒ **孔径内有黑像素 ⇒ 整颗星作废**（`isFluxOk()==false`，`StarLib.js:609` 处被过滤）。

#### (d) 参考星选择（**五道筛选**）

| # | 判据 | 位置 | 阈值 |
|---|---|---|---|
| 1 | **峰值线性区截断**（饱和剔除） | `StarLib.js:370-379` `filterStars` | `peak < peakUpperLimit`，`peakUpperLimit = linearRange = 0.7 × 本帧最大值` |
| 2 | **在重叠区内** | `StarLib.js:729-735` | 掩膜采样 `> 0` |
| 3 | **净通量 > 0** | `StarLib.js:740-761` | `flux ≤ 0` 剔除；`>100 颗且 >5%` 时 warning |
| 4 | **星等上限（最暗的按百分比剔除）** | `DialogControls.js:168-174` | `limitPhotoStarsPercent` 默认 100%，**上限 2000 颗** |
| 5 | **固定百分比离群剔除** | `StarLib.js:627-645` | `outlierRemovalPercent` 默认 2% |
| 6 | **去重（1 px 半径，保留最亮）** | `StarLib.js:817-850` | `radius = 1.0` |

`lib/StarLib.js:370-379`：
```
370: function filterStars(stars, peakUpperLimit){
371:    let filteredStars = [];
372:    for (let star of stars) {
373:        if ((star.getPeakValue() < peakUpperLimit) &&
374:                star.insideOverlap && star.getStarFlux() > 0) {
375:            filteredStars.push(star);
376:        }
377:    }
378:    return filteredStars;
379: };
```
`lib/StarLib.js:823-838`：
```
823: function combienStarArrays(detectedRefStars, detectedTgtStars){
824:    let radius = 1.0;
825:    let otherStars = detectedRefStars.getStars().concat(detectedTgtStars.getStars());
...
829:    otherStars.sort((a, b) => b.getStarFlux() - a.getStarFlux());
...
834:        let index = quadTree.search(qtStar.rect);
835:        if (!index.length){
836:            quadTree.insert(qtStar);
```
⇒ 去重是**跨 ref/tgt 两幅图一起做的**（防止同一颗星在两幅图里各算一次），先按亮度降序 ⇒ **保留最亮的那个**。

**注意：PMM 的"离群剔除"不是 σ-clip，是"固定剔除最坏的 N 颗"**（`N = round(n × 2%)`）。它**不做** `median ± k·MAD` 这类统计裁剪。`DialogControls.js:274-276` 原文只说"Outliers can be due to variable stars, or measurement errors."，**没有给任何统计定义**。

#### (e) 空间变化改正：**加性差分 SurfaceSpline**，不是乘性增益场

`lib/Gradient.js:199-205`：
```
199: /**
200:  * Calculates a surface spline representing the difference between reference and target samples.
201:  * Represents the gradient in a single channel. (use 3 instances  for color images.)
202:  * @param {SamplePair[]} samplePairs median values from ref and tgt samples
203:  * @param {Number|undefined} logSmoothing Logarithmic value; larger values smooth more
204:  * @returns {SurfaceSpline}
205:  */
```
`lib/SampleGrid.js:31-33`：
```
31:    this.getDifference = function(){
32:        return this.targetMedian - this.referenceMedian;
33:    };
```
应用方式 `lib/Gradient.js:368-375`：
```
368:            this.apply = function(outSamples){
...
374:                    outSamples[idx] -= self.zVector.at(i);
```
⇒ **`out −= z(x,y)`，纯加性**。

**与乘性的分工**（`PhotometricMosaic.js:562-573`）：
```
565:        let scale = scaleFactors[channel].m * data.adjustScale[channel];
...
571:        tgtCorrector.applyAllCorrections(refView, tgtView, mosaicView, scale,
572:                propagateSurfaceSpline, surfaceSpline, channel);
```
⇒ **全局乘性尺度（1 个数）× 空间变化加性曲面**。**PMM 没有乘性的空间变化场。**

**样本（控制点）的构造**（`lib/SampleGrid.js`）：
- 网格步长 = `sampleSize`；每个 bin 的 `tgtMedian`/`refMedian` = **该 bin 内像素的中值**（`SampleGrid.js:322-323`）；
- **bin 内含任何 0 像素 ⇒ 整块丢弃**（`SampleGrid.js:309-319`）；
- 落在**星点剔除圆**内的 bin 整块丢弃（`SampleGrid.js:212-225, 338-363`），剔除半径 = `星半径 + apertureAdd + growthRate×flux`（`StarLib.js:1274-1277`）；
- 样本数超过上限时**分箱合并**，合并后的 `weight` = 参与合并的原始样本数（`SampleGrid.js:526-574`）—— 即**按面积加权**。

**平滑**：`lib/Gradient.js:220-225`
```
220:    let ss = new SurfaceSpline();
221:    if (logSmoothing !== undefined){
222:        ss.smoothing = Math.pow(10.0, logSmoothing);
223:    } else {
224:        ss.smoothing = 0;
225:    }
```
⇒ `smoothing = 10^logSmoothing`；重叠区默认 `logSmoothing = -1`（`PhotometricMosaic.js:30`）⇒ `smoothing = 0.1`；目标区默认 `2`（`PhotometricMosaic.js:31`）⇒ `smoothing = 100`。`DialogControls.js:900-906` 原文说明理由："Smoothing needs to be applied to this surface spline to ensure it **follows the gradient but not the noise**."

### 4.2 对 A6（PSF 域通量）与 A4（空间增益）的**具体建议**

#### ⚠️ 首要结论：**A6 不要照搬 PMM 的口径**

PMM 的通量口径 = **固定盒和 − 矩形环带中值 × 像素数**，正是 A6 定案要淘汰的"孔径口径"。

`reverse_verify/docs/f-instr-canon.md:143-148`（已定案 A6 的实测）：
```
143: | 5×5 固定盒（现生产） | M_seeing = **1.353 mag**；假增益 2→4 px = **0.516**。把 seeing 读成增益。 |
145: | 固定孔径 r=3/4（`photometer.cpp` 默认 4.0） | M_seeing = **0.751 / 0.443 mag**；且孔径依赖峰峰 0.2–1.3 mag 随 seeing 变（§3.5）。 |
148: | **PSF 总通量 D1/D2** | M_seeing = **0.0037 / 0.0039 mag**；高 S/N 偏差 **≤0.003 mag**；低 S/N 散度**最小**（S/N=10 时 0.076/0.075 mag）。 |
```
⇒ **PMM 的口径属于第 1/2 行，M_seeing 1.35 mag / 0.75 mag 量级。照搬会把 A6 打回原形。**

**但 PMM 有一个值得学的补丁**：`StarLib.js:25-40` 的**角尺度生长上限**（25″/135″）。它把"孔径随通量无限膨胀"封顶，本质上是**限制孔径的 seeing 敏感度**。这是**次优解**，不改变结论。

#### 建议 A6-1：**坚持 D1/D2（PSF 域），不引入 PMM 式孔径**

理由已由 A6 定案给出（`f-instr-canon.md:150-167`）：D1/D2 **没有孔径自由度**，因此 `m(x,y)` 不会被孔径系统差污染。PMM 的做法**没有**解决这个自由度问题。

#### 建议 A6-2：**借鉴 PMM 的"两帧同孔径"思想，迁移为"两帧同 PSF 域"**

PMM 的 `StarPair`（`StarLib.js:284-348`）核心是"**同一颗星在两帧用同一个测量口径**"。PSF 域下的对应物是：
- 用**两帧中较差（较大 FWHM）的那个 PSF** 定义共同积分域；或
- 用**解析积分**（D1 的 `F = (2π/3)·A·s_x·s_y`，`f-instr-canon.md:153`）—— 它**本来就没有域**，天然满足这条。

⇒ **D1 天然满足 PMM 花了大力气才做到的事**，这是选 D1 的又一条独立理由。

#### 建议 A6-3：**把 PMM 的"线性区 = 0.7 × 本帧最大值"直接借鉴为饱和剔除口径**

PMM 的饱和剔除是 `peak < 0.7 × image.maximum()`（`Cache.js:65`），**逐帧自适应、无量纲（比例）**。
⇒ 我们现有的饱和剔除（`quality&1` / `psf_status!=0` / `PC_QF_SATURATED`，见 `f-instr-canon.md:174`）是**位标志**，更严格，**保留**。但如果将来需要"线性区上界"，PMM 的"本帧最大值 × 比例"是一个**尺度无关**的现成范式，优于任何绝对 ADU 阈值。

#### 建议 A4-1：**`m(x,y)` 应当建模为乘性场，但**分两步**：全局乘性 + 空间乘性**

PMM 的结构是：
```
校正后 = ref − [ (tgt × scale) − spline(x,y) ]  ⇒  乘性(1 个数) + 加性(空间)
```
而 A4 要的是 **乘性空间场** `m(x,y)`（`f-instr-canon.md:164`：`I_photo = k_photo · m(x,y) · I_cal`）。
⇒ **不要**把 PMM 的"加性曲面"直接当成 `m(x,y)`。两者**物理上不同**：
- 加性曲面可以吸收"天光/梯度"；
- 乘性场只能由**通量比**估计。

**建议**：`m(x,y)` 的估计量必须是 **"同一颗星在两帧间的通量比"**（`f-instr-canon.md:164`），在**共同 PSF 域**下逐星计算，再**空间平滑**（PMM 的 SurfaceSpline + `smoothing=10^logSmoothing` 是一个可借鉴的**平滑器形态**，但**不是**可复制的代码）。

#### 建议 A4-2：**控制点构造可直接借鉴 PMM 的三条稳健化措施**（这些是**方法**，不是代码）

| PMM 措施 | 位置 | 为什么对 A4 有用 |
|---|---|---|
| 控制点值 = **bin 内像素中值**（不是均值） | `SampleGrid.js:322-323` | 对宇宙线/热点/未剔除的暗星稳健 |
| 含**无效像素（0）的 bin 整块丢弃** | `SampleGrid.js:309-319` | 避免边缘/掩膜区污染；我们有 coverage 掩膜，等价物现成 |
| 落在**亮星剔除圆**内的 bin 整块丢弃，半径随星通量增长 | `SampleGrid.js:212-225` + `StarLib.js:1274-1277` | 我们 A6 已用 `mad` 做离群剔除；PMM 的做法是**几何剔除**，两者互补 |
| 合并样本时按**面积加权**（`weight = 原始样本数`） | `SampleGrid.js:526-574` | 避免"样本多的区域"被过度平滑 |

#### 建议 A4-3：**`m(x,y)` 的空间平滑尺度必须显式声明并做敏感性检验**

PMM 把 `logSmoothing` 做成**用户可调参数**，并在 tooltip 里给出物理判据（`DialogControls.js:942-951`）：
```
943:        "will be applied to the rest of the target image. This correction should " +
944:        "consist of a smooth curve that ignores all local gradients " +
945:        "(diffuse light around bright stars, filter halos, diffraction spikes) " +
946:        "and only follows the gradient trend.</p>" +
947:        "<p>Apply sufficient smoothing to produce a smooth gentle curve. " +
```
⇒ **"跟随趋势，不跟随噪声/星芒/光晕"** —— 这正是 A4 低阶空间增益的验收语义。建议我们的 `m(x,y)` 平滑尺度**写进合同**并给出**同样的可证伪判据**（例如：`m` 的功率谱在某尺度以上才显著；注入已知空间增益场能红能绿）。

---

## 5. 最该先落地的一条

### **Q-A：把 UPM 的绝对 `1e-6` 收敛门换成无量纲判据，并把 `converged` 改成状态**

**为什么是它（而不是 Q-B / Q-C）：**

1. **它是唯一"当前正在产生错误状态"的问题。** Q-B 的组间门虽然与裁决冲突，但它是 fail-closed（拒绝执行），不会产出错误数值；Q-C 的 A6 已有定案、只等实施。而 Q-A 让**每一次生产运行的 `converged` 都是 0**（`fix-p2a-seam.md:129`：`iterations=100, converged=0`），使这个字段**完全丧失信息量**，且掩盖真正的数值问题。
2. **现行补丁是反方向的同一个错误。** `tolerance_relative=1, tol=1e-3` ⇒ 门 = `3.09e12`，比冻结门宽 **18 个数量级**（`CONFORM-SWEEP-3.md:67`）。**`1e-6` 太严到不可达，`3e12` 太松到无意义 —— 两个都不是收敛判据。**
3. **它有现成的、循环内已经算好的无量纲量。** `upm.cpp:735-739` 的 `z = r / max(|σ|, σ_floor)` 就是答案，**不需要新数据、不需要新算法**。
4. **它有第一手文献支撑，不需要引用 PhotometricMosaic。** MINPACK `lmdif` 的 `ftol`/`xtol`/`gtol`（netlib，本次逐字核验）就是"相对 + 无量纲 + 三判据独立报告"的范式。
5. **它是另外两项的前置条件。** A4 的 `m(x,y)` 验收要求"先证明没有问题"（`GAP_AUDIT.md:1073`），而"证明"需要可信的**拟合质量**指标 —— 现在这个指标不存在（`converged` 恒 0，`objective` 无门）。

**落地形态（需走 `FZ-UPM-CONVERGENCE` 变更 claim）：**

| 步骤 | 内容 | 证据锚 |
|---|---|---|
| 1 | 定义 `rms_z = sqrt(Σ w z² / Σ w)` 与 `rel_obj = \|obj_new−obj_old\| / max(\|obj_old\|,eps)` | `upm.cpp:982-983`（`obj` 已是 χ² 型）；`upm.cpp:735-739`（`z` 已算） |
| 2 | 停止判据 = 「步长相对门」+「目标函数相对门」，**分母用观测量尺度，不用 `max\|M\|`** | MINPACK `ftol`/`xtol`（netlib 逐字） |
| 3 | `converged` → 状态枚举 `{converged, max_iter, stalled, invalid}` | `upm.cpp:96-99` |
| 4 | `quality` 三元组 `{rms_z, Σw/Σraw_w, sigma_residual_dex}` 独立落盘 | `upm.cpp:1190-1202`（现有 JSON 字段） |
| 5 | 把 `tolerance_relative` 字段与其生产取值**写进合同**（`PHASE2_UPM_IMPL.md` + `DATA_SEMANTICS` 字段表） | `CONFORM-SWEEP-3.md:71-78`（现为 0 命中） |
| 6 | 负例门：合成数据 + 真实帧，**能红能绿**（注入病态初值必须报 `max_iter`；健康数据必须报 `converged`/`stalled`） | `ENGINEERING_SPEC.md §3`；`ASTROCS_DESIGN.md §11.1` |

**数值论证（一句话）**：绝对 `1e-6` 在 `ULP=0.5` 下要求 21 位有效数字（不可达）；`1e-3·max|M|` 允许 0.1% 相对变化（无意义）；而 `rms_z` 是 O(1) 的无量纲量，其 `1e-3` 相对变化对应 13 位有效数字 —— **可达、可解释、且跨帧跨仪器可比**。

---

## 6. 附：证据索引

| 章节 | 证据文件位置 |
|---|---|
| E0 文件指纹 / 版本 | `evidence/pmm-evidence.txt:5-38` |
| E1 `converg` 0 命中 | `:40-42` |
| E2 `chi2` 0 命中 | `:44-60` |
| E3 全部告警面 | `:62-110` |
| E4 闭式最小二乘 | `:112-161` |
| E5 固定次数离群剔除 | `:163-214` |
| E6 拟合质量判据 | `:216-322` |
| E7 尺度归一化（线性区） | `:324-406` |
| E8 重叠区定义 | `:408-560` |
| E9 三段式匹配 | `:562-744` |
| E10 相对/绝对语义 | `:746-790` |
| E11 唯一 fail-closed 前置门 | `:792-829` |
| E12 通量口径 / 背景环带 / 孔径 | `:831-1288` |
| E13 参考星选择 | `:1290-1463` |
| E14 空间变化改正 | `:1465-1708` |
| E15 许可证 | `:1710-1793` |

**外部一手来源核验记录**（本次会话实际抓取）：

| URL | 状态 | 用途 |
|---|---|---|
| `https://netlib.org/minpack/lmdif.f` | 200，逐字摘录 | Q-A 相对判据依据 |
| `https://www.astromatic.net/software/swarp/` | 200，"Acknowledging SWarp: Bertin et al. 2002, ASP Conf. Ser. 281, 228" | 可引用替代来源 |
| `https://www.astromatic.net/software/sextractor/` | 200，"Acknowledging SExtractor: Bertin, E. Arnouts, S. 1996"（卷号有误，以 ADS bibcode 为准） | 可引用替代来源 |
| `https://arxiv.org/abs/1005.4454` | 200，标题/作者/摘要 | Montage 可引用来源 |
| `https://arxiv.org/abs/1706.01542` | 200，`Related DOI: 10.3847/1538-3881/aa9f22` | Burke et al. 2018 可引用来源 |
| `https://www.astromatic.net/pubsvn/software/swarp/trunk/doc/swarp.pdf` | **404** | SWarp 关键字语义**未核验**，故不引用 |
| `https://swarp.readthedocs.io/en/latest/` | **404** | 同上 |
| `https://ui.adsabs.harvard.edu/abs/1987PASP...99..191S/exportcitation` | **405（CAPTCHA）** | Stetson 1987 **未核验**，故不引用 |

---

*本报告为只读研究产物。未改动任何生产代码或文档；未执行任何 git 写操作；未运行构建/测试。*
