# EXP-02 天光噪声估计的「结构污染」缺陷：独立复核、修法与判据

> 实验单元：`实验/absolute-snr/`（SCI-B 绝对 SNR 传递链）｜任务单元：**SCI-B-EXP-02**
> 上游缺陷来源：`实验/absolute-snr/docs/EXP-01-DELTA-AND-ESTIMATOR.md` §2.2② 与 D8
> 权威依据：`AGENTS.md` §5（科学工作纪律）／`ASTROCS_DESIGN.md` §2.2、§5.3、§12.1、§12.2、§12.3
> 固定 seed：**20260925**｜本单元**只读** `lib/**`、`eng/**`、`docs/**`、`testdata/**`
> **不运行任何 ACSD 可执行文件**；**不使用 `ulimit -v`**；无 git 写操作。

---

## 0. 结论页（先给判定）

| # | 问题 | 判定 | 证据锚 |
|---|---|---|---|
| 1 | EXP-01 的「结构污染」缺陷是否成立？ | **成立，独立复现** | §3、§5.1–5.3 |
| 2 | 与 EXP-01 的数字是否一致？ | **一致**（真实数据逐帧逐位一致；HST 比值列 1.5% 内、污染列 0.35pp 内） | §3.2 |
| 3 | 缺陷的定量关系是什么？ | **σ̂/σ_true − 1 ≈ sqrt(1+r²) − 1 只对"尺度 ≳ 帧"的结构成立**；紧凑结构几乎免疫；真正的驱动量是**结构在裁剪窗口内的存活份额**，由**空间尺度 + 幅度分布**共同决定 | §2、§5.1、§5.2 |
| 4 | 什么时候开始失去语义？ | 大尺度结构在 **r = σ_s/σ_n ≳ 0.3** 时超出 δ=1.4% 预算；**r ≳ 3** 时 σ̂ 已无 σ_sky 语义（+216%）；真实 M42 帧上 σ̂ 是结构感知估计的 **1.16~3.66 倍** | §5.1、§5.3 |
| 5 | 修法 | **F1 结构感知估计**（mesh 局部背景 + 残差稳健尺度）+ **F2 provenance 三档门**（5 个无需真值的代理量）；两者正交、应同时落地 | §7 |
| 6 | 判据是否非退化？ | **是**：解析臂 240 条记录中 115 条被认证，**假绿 0 条**；无结构负例修法效应 Δ = +2.2e-5 ± 2.0e-5（3σ 内归零）；三类故障注入全部判红 | §8 |
| 7 | 对生产链的含义 | **现行 `noise_sigma` 不能作为跨帧绝对 SNR 的 σ_sky 输入**；M42 全部 16 帧在本判据下 fail-closed。这不是 δ 能预算的项 | §10 |

---

## 1. 任务、权威链与只读边界

### 1.1 这件事的规范依据（沿索引逐层下钻）

| 层 | 文档 | 本条要求 |
|---|---|---|
| 最高设计 | `ASTROCS_DESIGN.md` §2.2 | 帧级 SNR 只计真实源信号能量与噪声；**信号经独立局部背景估计与扣除，天光只通过其散粒噪声进入噪声项** |
| 最高设计 | `ASTROCS_DESIGN.md` §5.3 | 实际 SNR = 帧级 × 帧内相对因子；权重由 SNR 现场换算为逆方差 `w = 1/σ²` |
| 最高设计 | `ASTROCS_DESIGN.md` §12.1 | 每个科学结论需**三类独立一手证据**：论文/标准 + 开源库代码（项目+版本+文件:行）+ 仓内实测 |
| 最高设计 | `ASTROCS_DESIGN.md` §12.2 | 三类实验数据：HST 真实模板+物理前向仿真／纯解析代数合成（含"真值无效应⇒归零"负例）／testdata 真实数据 |
| 最高设计 | `ASTROCS_DESIGN.md` §12.3 | 实验报告八要素；**每个度量具备非退化判据**，恒真门没有证据资格 |
| 手册 | `AGENTS.md` §5 | 发现科学文档或自己的前提可能有错时，先质疑前提、做实验/查一手证据；开源实现同样可能有错 |
| 手册 | `AGENTS.md` §6 | 不动科学公式与冻结定义；不宣布发布；不用 waiver 盖红灯 |

### 1.2 生产代码事实（只读核实，**一律按符号名定位**——行号已多次漂移）

| 事实 | 代码锚（符号名） | 核实方式 |
|---|---|---|
| `noise_sigma` 的**唯一生产者** = 全帧 2 轮 `median ± 3·1.4826·MAD` 裁剪后的 **RMS**（中心取中位数） | `StarDetector::estimate_background`（`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp`） | 读源码 |
| 裁剪阈值常数 `1.482602218505602` 内联在函数体内；轮数硬编码 2；`k=3.0` | 同上 | 读源码 |
| 中位数取 `nth_value(scratch, kn, kn/2)` = **上中位数**（偶数 n 时非两中值平均） | `nth_value`（同文件匿名命名空间） | 读源码 |
| 最终 RMS 用**串行顺序求和** `for i: sum += (keep[i]-bg)^2`（非成对求和） | `StarDetector::estimate_background` | 读源码 |
| 非有限输入 ⇒ 整帧 `return false`（fail-closed） | 同函数入口 `reduction(|:nonfinite)` | 读源码 |
| `noise_sigma` 被检测阈值消费（`thr = bg + detection_sigma_·noise_sigma`）与逐源 SNR（`s.snr = (peak-bg)/noise_sigma`） | `StarDetector::detect` | 读源码 |
| `noise_sigma` 写入 `p1_sources.json` 的 `frames[].noise_sigma` | `module_adapters.cpp` 中 `{"noise_sigma", cat.noise_sigma}` | 读源码 |
| 下游 `p1_op_noise` 读作 `cfg.sigma_sky_adu` 并声明 `SNR_SIGMA_SKY_EMPIRICAL_TOTAL_RMS` | `module_adapters.cpp` 中 `cfg.sigma_sky_adu = src_frame->value("noise_sigma", 0.0)` | 读源码 |
| 该值进入 `sigma_i² = σ_sky² + F·P_i/g`（`σ_sky` 已含读噪，禁止再加 `(RN/g)²`） | `snr_science.cpp`（`snr_source_snr_f64`） | 读源码 |
| `σ_sky ≤ 0` 或非有限 ⇒ 帧级 SNR fail-closed | `snr_frame_science.cpp` | 读源码 |

**关键结构事实**：`estimate_background` 的输入是**整帧全部像素**，作用域是**帧全局**，且它以**全局中位数**为裁剪中心。
这意味着"平滑的大尺度天区结构"根本不是离群点——它是分布的**主体**。

> 注：`star_detector.cpp` 函数上方保留块与 `module_adapters.cpp` 的注释把 `noise_sigma` 描述为
> "noise_model 的空天稳健尺度 (1.4826*MAD)"。**代码事实是裁剪后 RMS**，不是 MAD 尺度。
> 该描述与实现的差异已由 EXP-01 D2 登记；本单元独立复核确认（§3.4）。

### 1.3 本单元的只读边界

- **不改**生产代码（`lib/**`、`eng/**`、`docs/**`、`CMakeLists.txt` 全程只读）；
- **不运行**任何 ACSD 可执行文件（全部结论来自 Python/NumPy/SciPy/astropy 独立重写与只读共享仿真器）；
- **不使用 `ulimit -v`**；每次运行用 `timeout` + `/usr/bin/time -v` 记峰值 RSS（实测峰值 ≤ 0.92 GB）；
- `testdata/**` 只读；不写 `run/` 之外的位置；不执行任何 git 写操作；
- 新增产物全部落 `实验/absolute-snr/{code/exp02,results,docs}/`，日志落 `run/SCI-B-EXP-02/logs/`。

---

## 2. 缺陷机理（解析可证部分）

### 2.1 为什么"加轮数 / 收紧 k"救不了

设帧 = 结构 `S` + 噪声 `n`，`σ_s = rms(S)`，`σ_n = rms(n)`。生产第一轮的裁剪阈值是

```
thr = 3 · 1.4826 · MAD_total ≈ 3 · σ_total        (近似高斯)
σ_total = sqrt(σ_n² + σ_s²)
```

**裁剪窗口被结构自身抬高**：`thr ∝ σ_total`。当结构是"成片存在"的平滑场时，结构像素的偏差
在 3σ_total 以内 ⇒ 保留比 `keep_frac → 1`（实测：r=1000 的斜坡 keep_frac = 1.0000）。
于是最终 RMS 测的是 **结构 + 噪声**，而不是噪声。

解析预测（大尺度/斜坡结构，裁剪近乎无效时）：

```
σ̂/σ_n ≈ sqrt(1 + r²)     ⇒   σ̂/σ_true − 1 ≈ sqrt(1+r²) − 1
```

解析臂实测（`ramp`，r=1001.96）：**+100095.5%**，与解析式 `sqrt(1+1001.96²)−1 = 1001.96` 差 0.1%（残差来自裁剪仍剔掉了少量尾部）✔

### 2.2 但"结构 rms / 噪声 rms"**不是**充分统计量

同一个 `r`，污染量级可以差 4 个数量级。解析臂（`results/exp02_e1_analytic.json`，box=32）：

| 结构模型 | r = σ_s/σ_n | 生产 σ̂/σ_true − 1 | 说明 |
|---|---|---|---|
| `ramp`（线性斜坡，尺度 = 帧） | 1001.96 | **+100095.5%** | 裁剪完全失效 |
| `smooth_corr128`（高斯相关场，相关长度 128 px） | 1004.27 | **+100327.7%** | 同上 |
| `smooth_corr32`（相关长度 32 px） | 1001.74 | **+100073.7%** | 同上 |
| `blob_sigma8`（单个高斯团块，σ=8 px） | 1001.54 | **−1.20%** | 几乎免疫 |
| `blob_sigma32`（σ=32 px） | 1025.49 | **+4.18%** | 仅轻微污染 |

**机理**：裁剪是"尾部剔除器"。**紧凑结构是尾部**（少数像素），被剪掉；
**大尺度结构是主体**（多数像素），剪不掉。
⇒ 任何"用结构 rms 与噪声 rms 的比值做安全门"的设计都**不充分**（这正是本单元不采用该方案的原因，见 §7.3）。

HST 臂进一步给出反直觉证据（表 B1）：
- `p99.9/σ = 3.42`（结构 rms/σ = 3.34）⇒ 生产 σ̂ 偏差 **−0.40%**（比无结构基线还小）；
- `p99.9/σ = 8550.93`（结构 rms/σ = 8352.08）⇒ **+18240%**。
同一"rms 比值"口径下，M16 核心区的高动态范围使 rms 由最亮 0.01% 像素主导，而**驱动裁剪的是 p99.9 幅度**。

### 2.3 独立重写时踩到的两个实现陷阱（对落地很重要）

为了让修法可信，本单元**独立重写**了生产 recipe（`exp02_common.production_clip_sigma`），
过程中在 mesh 背景侧踩到两个会**伪造出结构污染**的实现陷阱，如实登记：

| 陷阱 | 现象 | 正确做法 |
|---|---|---|
| 背景图边界被"钉住" | mesh 坐标 `u` 与 `y0/y1` 一起 clip ⇒ 帧边缘半个 mesh 的背景被固定为首个 mesh 值；线性结构上注入约 `amp/(2M)` 的假残差，实测把残差 RMS 从 **0.02% 抬到 +2.1%** | 背景图**线性外推 1 格**后再双线性展开 |
| 偶数窗口 median filter | 边界窗口被裁成 2×k（偶数）时 `np.median` 取两中值**平均**，造出数据里不存在的值；单调结构上把残差 RMS 抬到 **+13000%** | 用奇数窗口 + `mode="nearest"`（等价 SExtractor `BACK_FILTERSIZE` 的奇数核语义） |

**教训**：结构污染的量级判据必须配"结构无效应 ⇒ 度量归零"的负例，否则实现 bug 会被误读成物理效应。
本单元的所有负例都放在 §8。

---

## 3. ① 独立复核结论：与 EXP-01 一致 / 不一致

### 3.1 复核方法（不复用 EXP-01 的结论代码）

- 生产 recipe：**独立重写**（不 import `exp01_common`），并给出 C++ 代码锚；
- HST 臂：直接调用仓内**只读**共享仿真器 `实验/shared/synthetic/{render.py,noise_model.py}`，自建场景（不 import `exp01/hst_sim.py`）；
- 真实数据臂：自建逐帧统计（含块间离散、棋盘分半），文件选择与裁剪口径与 EXP-01 §2.2③ 对齐以便逐帧对照；
- 与 EXP-01 镜像做**双向交叉核对**（`crosscheck_mirror`）。

### 3.2 一致的部分

**(a) 真实数据臂：逐帧逐位一致**（`testdata/M42_T2T3_mosaic_Flying_dutchman/T2/**/*-300S-Red.fts`，2048² 中心裁剪，16 帧）

| 帧 | 本单元 σ̂_prod [ADU] | 保留比 kf | EXP-01 σ̂_prod | EXP-01 kf | 判定 |
|---|---|---|---|---|---|
| M1-20251212 | 16.1581 | 0.9794 | 16.158 | 0.979 | ✔ 一致 |
| M1-20251224 | 15.6483 | 0.9796 | 15.648 | 0.980 | ✔ |
| M2-20251212 | 59.4080 | 0.8319 | 59.408 | 0.832 | ✔ |
| M2-20251216 | 64.0428 | 0.8314 | 64.043 | 0.831 | ✔ |
| M2-20251224a | 62.9687 | 0.8292 | 62.969 | 0.829 | ✔ |
| M2-20251224b | 63.3635 | 0.8290 | 63.364 | 0.829 | ✔ |
| M3-20251216 | 18.9893 | 0.9827 | 18.989 | 0.983 | ✔ |
| 块间 σ p95/p05（M2） | **9.08** | — | **9.08** | — | ✔ |

**(b) HST 臂：在 EXP-01 §2.2② 的原始 p999 网格上逐点一致**

| target p99.9 [e⁻] | EXP-01 结构/噪声 | 本单元 结构/噪声 | EXP-01 nostar_rel | 本单元 prod_rel |
|---|---|---|---|---|
| 0 | 0.00 | 0.00 | −1.430% | −1.64% |
| 60 | 6.19 | **6.26** | +2.97% | **+2.62%** |
| 200 | 20.6 | **20.87** | +16.1% | **+15.60%** |
| 1000 | 103 | **104.37** | +165% | **+165.03%** |
| 50000 | 5162 | **5218.56** | +11371% | **+11365%** |

比值列一致到 **1.5% 以内**，污染列一致到 **0.35pp 以内**。
`p999=0` 行的差（−1.64% vs −1.430%）来自**真值口径**：本单元用背景口径 `σ_sky`
（天光+暗流+读噪+量化，**不含**底图星云自身的泊松），EXP-01 的 `σ` 含底图泊松项。

> **过程留痕（诚实）**：本单元在中期曾一度判定 EXP-01 的"结构/噪声"列少乘曝光时间 300×。
> 该判断**错误并已撤回**。根因：`render.py::render_frame` 中 `rate = src_canvas + rb_rate`、
> `src_rate = rate/t`、`expose(src_e_per_s=src_rate, …)` 内部再乘 `t`，故
> **`frame.src_e ≡ rb_rate` 数值恒等**（实测 `std(frame.src_e)/std(rb_rate) = 1.000000`），
> `std(rb_rate)/gain` 本来就是 ADU 口径。EXP-01 该列正确。

### 3.3 与 EXP-01 不一致的部分

**无实质不一致。** 只有三处口径差异，均已定量对齐：

| 项 | EXP-01 | 本单元 | 影响 |
|---|---|---|---|
| 真值 σ 口径 | `sqrt(mean(V_i))`，`V_i` 含 `src_e`（底图泊松） | 背景口径 `σ_sky`（不含底图泊松），另单列 `sigma_pix_true_adu` / `neb_poisson_adu` | 只在有底图行产生 ≤0.2pp 差 |
| 结构幅度参数化 | 按 `target_p999_e` 电子数 | 同时给 `struct_over_noise`（rms 比）与 `struct_p999_over_noise`（p99.9 比） | **后者才是驱动裁剪的量**（§2.2） |
| 生产 recipe 镜像细节 | `np.median` + 成对求和 | **上中位数** + **串行顺序求和**（逐字对 C++） | 见 §3.4，差异 ≪ 1e-9 |

### 3.4 交叉核对与新增证据

**(a) 生产镜像的独立交叉核对**（`n = 2^20` 高斯样本，seed=20260925）：

| 量 | 值 |
|---|---|
| 本单元 σ̂ vs EXP-01 镜像 σ̂ 的相对差 | **−2.2e-10** |
| 保留集合大小差 | 0 |
| 串行求和 vs 成对求和（同一集合） | 8.2e-15 |

⇒ 两条镜像同源、数值等价；本单元额外覆盖了 EXP-01 未覆盖的"上中位数 / 串行求和"逐字细节。

**(b) 本单元新增的、EXP-01 未给的证据**：

1. **结构形态决定污染量级**（§2.2）—— 团块几乎免疫、大尺度结构全额污染；
2. **"何时失去语义"的判据**（§5.1）：r ≳ 0.3 超出 δ 预算、r ≳ 3 无 σ_sky 语义；
3. **真实帧上的定量污染倍数**：M42 T2 帧的 σ̂_prod/σ̂_mesh ∈ [1.16, 3.66]（中位 1.38）；
4. **一套无需真值的结构污染代理量与三档门**（§7.3、§8），含 0 假绿的标定审计；
5. **两处 mesh 实现陷阱的定量登记**（§2.3）。

---

## 4. 实验设置（三类数据齐备，固定 seed）

| 臂 | 数据类（§12.2） | 数据来源 | 真值 | seed |
|---|---|---|---|---|
| A `e1_analytic_scan.py` | 第 2 类：纯解析代数合成 | 解析构造：斜坡 / 高斯相关场（相关长度 8/32/128 px）/ 高斯团块（σ=8/32 px）+ 高斯白噪声（ADU 域） | **完全已知** σ_n = 20 ADU | 20260925 |
| B `e2_hst_scan.py` | 第 1 类：HST 真实模板 + 物理前向仿真 | `testdata/HST_M16/*.fits`（只读）作纯信号底图 + `实验/shared/synthetic/render.py` 完整物理前向 | 仿真器真值（背景口径 σ_sky 与逐像素 V_i） | 20260925 |
| C `e3_real_data.py` | 第 3 类：testdata 真实数据 | `testdata/M42_T2T3_mosaic_Flying_dutchman/T2/**/*-300S-Red.fts`（16 帧，只读） | **无真值**（只做可证的自洽量） | 20260925 |

HST 前向仿真的物理过程（全部经 `render.py::render_frame` → `noise_model.expose`）：
源/天光/暗流在电子域做 Poisson；读出噪声在电子域做 Gaussian；电子→ADU 过增益（1.3 e⁻/ADU）、
饱和（120000 e⁻）与量化；含平场 PRNU（0.5%）、低阶空间项（1%）、渐晕（3%）；天光 0.5 e⁻/s × 300 s；
PSF 为 Moffat4（检测块高斯 FWHM = 2.5 px）；`cr_rate = 0`、`hot_fraction = 0`（与 EXP-01 同）。

**共享仿真器只读**：本单元不修改、不复制 `实验/shared/synthetic/**`，仅 import。
---

## 5. 结果（三类数据）

### 5.1 臂 A：纯解析合成（`results/exp02_e1_analytic.json`，240 条记录）

**σ̂/σ_true 随结构 rms 的关系曲线**见 `results/exp02_figs/a_analytic_scan.png`（左：F0 生产；右：F1b 修法）。
节选（box=32；完整 60 行见 `results/EXP02_TABLES.md` 表 A1）：

| 模型 | r | F0 生产 | F1b 修法 | A1 | A2 | 判据 |
|---|---|---|---|---|---|---|
| ramp | 0.00 | −1.3% | −1.33% | 1.000 | 1.0002 | ok |
| ramp | 0.30 | +2.8% | −1.73% | 1.047 | 1.0007 | diagnostic_only |
| ramp | 3.01 | **+216.6%** | −1.44% | 3.213 | 1.0002 | diagnostic_only |
| ramp | 1001.96 | **+100095.5%** | +1.08% | 1015.9 | 1.0248 | fail_closed |
| smooth_corr128 | 1.00 | +40.3% | −1.28% | 1.423 | 1.0011 | diagnostic_only |
| smooth_corr128 | 1004.27 | **+100327.7%** | **+1768.5%** | 158.1 | 2.9422 | fail_closed |
| smooth_corr32 | 1.00 | +40.1% | +3.04% | 1.422 | 1.0459 | fail_closed |
| smooth_corr8 | 1.00 | +39.6% | +35.60% | 1.411 | 1.3710 | fail_closed |
| blob_sigma8 | 1001.54 | **−1.2%** | −1.20% | 0.999 | 0.9989 | ok |
| blob_sigma32 | 10.25 | +3.4% | +3.40% | 1.034 | 1.0346 | fail_closed |

**"何时开始失去语义"的判据（大尺度/平滑结构）**：

| 阈值 | r = σ_s/σ_n | F0 σ̂ 偏差 | 语义 |
|---|---|---|---|
| r ≲ 0.1 | ≤0.1 | ≤1.0% | 在 δ=1.4% 预算内，可忽略 |
| r ≈ 0.3 | 0.3 | +2.8%（斜坡） | **开始超出 δ 预算** |
| r ≈ 1 | 1.0 | +40% | 显著失真 |
| r ≈ 3 | 3.0 | **+217%** | **σ_sky 语义已丧失** |
| r ≳ 100 | 100 | ≥+9900% | σ̂ ≈ 结构 rms 本身 |

**尺度分辨率**（`results/exp02_figs/a_box_dependence.png`）：修法在结构尺度 ≫ mesh 时有效；
结构尺度 ≲ mesh（`smooth_corr8` vs box≥32）时，修法与生产**同样失效**（A1 ≈ R ≈ 1，只有 A2 能检出，见 §7.3）。

### 5.2 臂 B：HST M16 真实模板 + 物理前向仿真（`results/exp02_e2_hst.json`）

完整 22 行见 `results/EXP02_TABLES.md` 表 B1；曲线见 `results/exp02_figs/b_hst_scan.png`。节选（`nostar`，box=32）：

| p99.9 [e⁻] | 结构 rms/σ | p99.9/σ | F0 生产 | F1b 修法 | kf | A1 | A2 | 判据 |
|---|---|---|---|---|---|---|---|---|
| 0 | 0.00 | 0.00 | −1.64% | −1.81% | 0.9961 | 0.996 | 0.9941 | ok |
| 16.00 | 1.67 | 1.71 | −1.17% | −1.36% | 0.9948 | 0.996 | 0.9943 | ok |
| 48.01 | 5.01 | 5.13 | +0.21% | +0.25% | 0.9932 | 1.001 | 1.0019 | ok |
| 80.02 | 8.35 | 8.55 | +3.74% | +1.79% | 0.9956 | 1.028 | 1.0090 | diagnostic_only |
| 160.05 | 16.70 | 17.10 | +10.07% | +5.92% | 0.9933 | 1.070 | 1.0300 | fail_closed |
| 480.14 | 50.11 | 51.31 | +58.23% | +33.05% | 0.9901 | 1.408 | 1.1836 | fail_closed |
| 1600.46 | 167.04 | 171.02 | +296.01% | +176.15% | 0.9879 | 2.591 | 1.8070 | fail_closed |
| 80022.90 | 8352.08 | 8550.93 | **+18240%** | +11889% | 0.9844 | 5.163 | 3.3754 | fail_closed |

- **保留比 kf ≈ 0.98–0.99**：结构几乎没有被剪掉（与 §2.1 机理一致）；
- **F1b 修法只能把偏差压到约 1/1.5**，因为 M16 核心区的结构跨越了从 ≲1 px 到 ≫mesh 的**全部尺度**；
- `raw`（含 80 颗星）与 `nostar` 行的差 ≤0.7pp ⇒ **源污染是次要项**（与 EXP-01 的 −0.23% 一致）。

> **结构幅度参数化的教训**：若只看 `struct_over_noise`（rms/sigma），`p99.9 = 8550.93` 的行给出 8352 倍；
> 但 `p99.9/σ = 3.42` 的行 rms/sigma 已达 3.34 却**几乎无污染**。
> 原因是 M16 核心区的 rms 由最亮 0.01% 像素主导 ⇒ 该场景下 rms 比值**不是**裁剪行为的良好代理。

### 5.3 臂 C：testdata 真实帧（`results/exp02_e3_real.json`，16 帧，2048² 中心裁剪）

| 帧 | σ̂_prod | kf | σ̂_fix(64) | R=prod/fix | A1 | A2 | A2(mesh-med) | A2(MAD) | D | 块间 p95/p05 | 判据 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| M1-20251212 | 16.1581 | 0.9794 | 13.8779 | 1.164 | 1.193 | 1.0243 | 1.0137 | 1.0183 | 1.11 | 1.09 | fail_closed |
| M1-20251224 | 15.6483 | 0.9796 | 13.0338 | 1.201 | 1.234 | 1.0279 | 1.0192 | 0.9938 | 1.14 | 1.12 | fail_closed |
| M2-20251212 | 59.4080 | 0.8319 | 17.4139 | **3.412** | 3.820 | 1.1197 | 0.9565 | 1.1074 | 3.26 | **9.08** | fail_closed |
| M2-20251216 | 64.0428 | 0.8314 | 17.9845 | **3.561** | 4.046 | 1.1363 | 0.9571 | 1.1437 | 3.38 | **9.66** | fail_closed |
| M2-20251224a | 62.9687 | 0.8292 | 17.2706 | **3.646** | 4.185 | 1.1478 | 0.9588 | 1.1355 | 3.66 | 9.61 | fail_closed |
| M2-20251224b | 63.3635 | 0.8290 | 17.3137 | **3.660** | 4.205 | 1.1489 | 0.9651 | 1.1383 | 3.64 | 9.63 | fail_closed |
| M3-20251216 | 18.9893 | 0.9827 | 13.9138 | 1.365 | 1.395 | 1.0224 | 1.0112 | 1.0209 | 1.11 | 1.13 | fail_closed |
| M4-20251216 | 17.4049 | 0.9838 | 14.2570 | 1.221 | 1.257 | 1.0297 | 1.0147 | 1.0067 | 1.13 | 1.17 | fail_closed |
| M5-20251212 | 43.3752 | 0.8106 | 16.4219 | 2.641 | 2.865 | 1.0848 | 0.9607 | 1.0443 | 2.79 | 7.52 | fail_closed |
| M6-20251212 | 17.9513 | 0.9537 | 13.5513 | 1.325 | 1.354 | 1.0219 | 1.0088 | 0.9943 | 1.14 | 1.33 | fail_closed |

（完整 16 行见 `results/EXP02_TABLES.md` 表 C1；散点见 `results/exp02_figs/c_real_frames.png`。）

**真实数据上的可证结论**（无真值，故只报可证项）：

1. **生产 σ̂ 是结构感知估计的 1.16–3.66 倍**（中位 1.38）——M2/M5 板块（猎户座星云主体）达 2.4–3.7 倍；
2. **保留比 kf 低至 0.805**（M2/M5），与 EXP-01 的 0.829 一致；M1/M3/M4/M6 在 0.94–0.98；
3. **块间 σ 离散 p95/p05 最高 9.66**（M2）——帧内 σ̂ 根本不是一个常数，"帧级 σ_sky"在这类帧上本身就不成立；
4. **A1 = 1.19–4.21**：帧内存在明确的空间相关结构；
5. **全部 16 帧在本判据下 fail_closed**：即便用 mesh 修法，残余结构仍达 **2.0%–14.9%**（A2），超出 1.4% 预算。
   ⇒ 这些帧的 σ_sky **不能**被认证为绝对 SNR 链的标定基础（这是结论，不是判据缺陷，见 §10）。

---

## 6. ② 文献与开源实现对照（逐条可核验来源）

### 6.1 SExtractor（Bertin）：网格化背景 + **σ 图跨 mesh 中值**

| 项 | 内容 | 来源（可核验） |
|---|---|---|
| `BACK_SIZE` | mesh 边长（px），默认 **64**，范围 1..2e9 | SExtractor **2.28.2**（commit `5e82a8d17e19a0ffc981b59d6776630c885f388f`）`src/preflist.h:67-68`；`src/field.c:175-186`；`config/default.sex:68` |
| `BACK_FILTERSIZE` | 对背景图**与 σ 图**同步做中值滤波的窗口，单位是 **mesh**，默认 3，范围 1..11；**`1` 才表示不滤波，`0` 在 2.28.2 是配置错误（exit=1）** | 同上 `src/preflist.h:60-61`；`src/back.c:745-867`（`filterback`）；`src/prefs.c:261-276`；`doc/src/Background.rst:79-82` |
| `BACKPHOTO_TYPE` | `GLOBAL`（默认，用背景图插值 + 全局 `backsig`）/ `LOCAL`（天体周围矩形环内**重做一遍** mesh 统计） | `src/analyse.c:72-78`；`src/back.c:874-982`（`localback`）；`src/preflist.h:65-66`；`doc/src/Photom.rst:195-199` |
| `BACKPHOTO_THICK` | LOCAL 环厚（px），默认 24，范围 1..256 | `src/preflist.h:64`；`src/preflist.h:271`；`src/back.c:889` |
| 背景估计器 | κσ 迭代裁剪 + 众数；`mode = 2.5·median − 1.5·mean`，带 `|mean−median|/σ < 0.3` 守卫退回 median；σ = 该 mesh 裁剪直方图的 rms | `src/back.c:443-587`（`backstat`）、`:669-737`（`backguess`，关键行 `:692-693, :723-731, :734`）；`doc/src/Background.rst:21-43` |
| **噪声/阈值用哪个量** | **逐 mesh 裁剪 σ 图 → 跨 mesh 取中值**：`field->backsig = fqmedian(sigma2, np)`；阈值 `= DETECT_THRESH × backsig`；逐像素噪声图 = σ 图的双三次样条插值 | `src/back.c:297-304`、**`:846`**、`:391-419`（`:405`）、`:1229-1354`（`backrmsline`）；`src/readimage.c:90-92`；`src/weight.c:110-117`；`doc/src/Background.rst:62-63` |
| 一手文献 | Bertin & Arnouts 1996, A&AS **117**, 393–404；**§2 "Background estimation"** 给 Eq.(1) `mode = 2.5 × median − 1.5 × mean`；**§3 "Detection" 未定义噪声 σ 的公式** ⇒ 只能读源码 | DOI [`10.1051/aas:1996164`](https://doi.org/10.1051/aas:1996164)；bibcode `1996A&AS..117..393B`；arXiv 无此文 |

**对 ACSD 的直接对照（子代理本地编译 2.28.2 实测，512²、噪声 σ=10 ADU、椭圆高斯"星云"峰值 60、σx=150/σy=110 px）**：

| 配置 | stdout RMS | 说明 |
|---|---|---|
| `BACK_SIZE=64`（默认） | **10.4673** | 真值 10.0，干净 |
| `BACK_SIZE=16` | 9.9034 | 干净 |
| **`BACK_SIZE=256`** | **18.5386** | mesh ≈ 结构尺度 ⇒ **被污染** |
| 纯噪声图（默认） | 9.8811 | — |
| **ACSD 现行做法（numpy 复算）** | **18.72** | 与 `BACK_SIZE=256` 同病 |

⇒ **保护来自"mesh 空间分解 + 跨 mesh 中值聚合"，不是来自估计器公式。**

### 6.2 photutils 2.2.0（Astropy 生态）

| 项 | 内容 | 来源（可核验） |
|---|---|---|
| `Background2D` 签名 | `data, box_size, *, mask, coverage_mask, fill_value, exclude_percentile=10, filter_size=(3,3), filter_threshold, edge_method, sigma_clip=SigmaClip(sigma=3.0, maxiters=10), bkg_estimator=SExtractorBackground(), bkgrms_estimator=StdBackgroundRMS(), interpolator=BkgZoomInterpolator()` | photutils **2.2.0**，`photutils/background/background_2d.py:215-221` |
| `box_size` / `filter_size` | 等价 `BACK_SIZE` / `BACK_FILTERSIZE`（单位 mesh，两轴须奇数，`(1,1)` = 不滤波） | `background_2d.py:239-240, 247-248, 708-709` |
| `sigma_clip` | 在 `Background2D` 层做**一次**（`_sigmaclip_boxes`），并强制把估计器自带 `sigma_clip` 置 `None` 以免重复裁剪 | `background_2d.py:391-424, 256-261` |
| `bkg_estimator` 候选 | `MeanBackground`(`:189`)、`MedianBackground`(`:243`)、`ModeEstimatorBackground`=3·med−2·mean(`:314`)、`MMMBackground`（同前者，DAOPHOT MMM）、`SExtractorBackground`=2.5·med−1.5·mean(`:424`)、`BiweightLocationBackground`(`:511`) | `photutils/background/core.py` |
| `bkgrms_estimator` 候选 | `StdBackgroundRMS`=`nanstd`(ddof=0，**关于 box 均值**)(`:566`)、`MADStdBackgroundRMS`=1.4826·MAD(`:631`)、`BiweightScaleBackgroundRMS`(`:702`) | `core.py` |
| **`background_rms` 口径** | **全尺寸二维 rms 图**（形状 = 输入 data），**不是全局标量**；全局标量是 `background_rms_median`（低分辨率 rms 图的中值，官方 docstring 明说是 SExtractor stdout 打印的那个 RMS） | `background_2d.py:750-760, 824-842（:842）, 867-876` |
| 官方文档 | 用户指南与 API（版本固定 2.2.0） | [`/en/2.2.0/user_guide/background.html`](https://photutils.readthedocs.io/en/2.2.0/user_guide/background.html)、[`Background2D`](https://photutils.readthedocs.io/en/2.2.0/api/photutils.background.Background2D.html) |

**实测（同一张结构图，真噪声 σ=10）**：`box_size=64` ⇒ `background_rms_median = 10.523`（真值 10.0）；
**`box_size=512`（整帧一个 box）⇒ 18.500** —— **精确复现 ACSD 的失效模式**。

> 官方文档对"全局稳健统计"的判词（[用户指南](https://photutils.readthedocs.io/en/stable/user_guide/background.html)）：
> "…the resulting values are biased by the presence of real sources. A slightly better method involves using statistics that are robust against the presence of outliers, such as the biweight location… **However, for most astronomical scenes these methods will also be biased by the presence of astronomical sources in the image.**"

### 6.3 DAOPHOT / IRAF：逐源局部天光环

| 项 | 内容 | 来源（可核验） |
|---|---|---|
| 天光环的定义与理由 | "靠近目标星、对称包围目标星、像素数远多于孔径的圆环"；用 **mode** 的理由不是"稳健"，而是它是"该处任一随机像素亮度的**最大似然**估计" | Stetson 1987, PASP **99**, 191–222，§III.B（p.199–200）；DOI [`10.1086/131977`](https://doi.org/10.1086/131977)；ADS [`1987PASP...99..191S`](https://articles.adsabs.harvard.edu/pdf/1987PASP...99..191S) |
| **作者本人划清界限** | "把污染裁掉后测弥漫天光"的算法**回答的是另一个问题**，不适合孔径测光 | Stetson 1987 §V.A（p.217–218） |
| 天光参数（真实名） | `salgorithm`（估计器：`mode`/`median`/`centroid`/`gauss`/`ofilter`/`constant`）、`annulus`（**内**半径）、`dannulus`（**宽度**，非外半径）、`sloreject`/`shireject`（迭代 kσ 拒绝因子）、`snreject`（最大拒绝迭代数）、`sloclip`/`shiclip`（百分比预裁剪）；半径以 `datapars.scale` 为单位 | IRAF 官方源码 [`apphot/doc/fitskypars.hlp`](https://github.com/iraf-community/iraf/blob/main/noao/digiphot/apphot/doc/fitskypars.hlp)、[`daophot/doc/fitskypars.hlp`](https://github.com/iraf-community/iraf/blob/main/noao/digiphot/daophot/doc/fitskypars.hlp)、[`daopars.par`](https://github.com/iraf-community/iraf/blob/main/noao/digiphot/daophot/daopars.par)、[`apphot/doc/datapars.hlp`](https://github.com/iraf-community/iraf/blob/main/noao/digiphot/apphot/doc/datapars.hlp) |
| **DAOPHOT 的"全局天光"用途** | DAOPHOT II 的 `SKY` 命令（约 10000 个散布像素 + 裁剪 + `mode = 3·median − 2·mean`）**只喂给 FIND 的检测阈值**；测光减天光永远走逐源局部环 | User Manual for DAOPHOT II，[ucolick 镜像 PDF](https://www.ucolick.org/~bolte/AY257/HMWK3_2015/daophotii.pdf) |
| 天光环的"三尺度取舍" | 内半径须大到恒星贡献可忽略；外半径须小到背景可视为常数或被平面拟合；环内像素数须远多于测光区 | Dolphin 2000, PASP **112**, 1383–1396；DOI [`10.1086/316630`](https://doi.org/10.1086/316630)；[arXiv:astro-ph/0006217](https://arxiv.org/abs/astro-ph/0006217) |

> **订正（本单元查证发现任务书的两处预设与一手来源不符）**：
> ① 参数名 `skyestimator` / `sigmareject` / `clipiters` **在 IRAF/DAOPHOT 官方 `.par`/`.hlp` 中不存在**，
> 对应功能名是 `salgorithm` / `sloreject`+`shireject` / `snreject`；
> ② IRAF `daopars.fitsky` 是**布尔**（是否在 PSF 拟合中重算组天光），**不是**"常数/线性/二次"选择；
> "常数/线性/二次天光面"在 Stetson 1987 中是被**否决**的路线（§III.D.2.a），DAOPHOT II 手册里的
> "Polynomial order" 属于 FUDGE 补图工具，与天光无关。落地文档若引用这两点需按订正后的写法。

### 6.4 局部 vs 全局天光的取舍文献

| 结论 | 来源 |
|---|---|
| mesh 尺度**两侧受约束**：大于源的典型尺寸，小于背景变化尺度；过小会吃掉展源翼部流量 | Bertin & Arnouts 1996 §2（DOI [`10.1051/aas:1996164`](https://doi.org/10.1051/aas:1996164)）；[SExtractor Background 文档](https://sextractor.readthedocs.io/en/latest/Background.html)；[photutils 用户指南](https://photutils.readthedocs.io/en/stable/user_guide/background.html) |
| 背景估计的**空间尺度是显式可调参数**，由多项式阶数/网格尺寸设定 | Bosch et al. 2018, PASJ **70**, S5（HSC pipeline）；DOI [`10.1093/pasj/psx080`](https://doi.org/10.1093/pasj/psx080)；[arXiv:1705.06766](https://arxiv.org/abs/1705.06766) |
| mesh 尺度直接决定背景图的高频成分与噪声；`BACK_SIZE 64→128` 可降低单个 mesh 被亮源污染的风险 | Kelvin, Hasan & Tyson 2023, MNRAS **520**, 2484–2516；DOI [`10.1093/mnras/stad180`](https://doi.org/10.1093/mnras/stad180)；[arXiv:2301.05793](https://arxiv.org/abs/2301.05793) |
| **反向条款**：结构本身就是科学信号时，复杂局部天光模型会导致**局部过减**；宁可让偏差"全局均匀（可标定）"也不要"局部（不可标定）" | Watkins et al. 2024, MNRAS **528**, 4289–4306；DOI [`10.1093/mnras/stae236`](https://doi.org/10.1093/mnras/stae236)；[arXiv:2401.12297](https://arxiv.org/abs/2401.12297) |
| σ 必须从**天光已扣、源已剔除**的像素上测；mesh 内天光用未检测像素均值，且要求未检测像素占比足够 | Akhlaghi & Ichikawa 2015, ApJS **220**, 1（NoiseChisel）；DOI [`10.1088/0067-0049/220/1/1`](https://doi.org/10.1088/0067-0049/220/1/1)；[arXiv:1505.01664](https://arxiv.org/abs/1505.01664) |
| 有真实展源时天光模型取**低空间自由度**（二阶多项式），以免减掉天体本征暗弱特征；1σ 由掩源后的残差标准差得到 | Byun et al. 2018, AJ **156**, 249；DOI [`10.3847/1538-3881/aae647`](https://doi.org/10.3847/1538-3881/aae647)；[arXiv:1810.02075](https://arxiv.org/abs/1810.02075) |

### 6.5 文献对 ACSD 现行做法的判定

**文献与开源实现一致判定：现行"全局 2 轮 median ± 3·1.4826·MAD 裁剪 RMS"不能给出有结构场中的 σ_sky 语义，属已知失效模式。**

四点合一：

1. **"全局"这一层就错了**——DAOPHOT/IRAF/SExtractor/photutils 的共识是天光水平必须局部估计；
2. **裁剪不能救**——结构像素成片存在时，裁剪窗口被结构自身抬高，剩下的"残差 RMS"测的是结构 + 噪声（Stetson 1987 §V.A 明确了这是"另一个问题"）；
3. **DAOPHOT 的全局天光确有用途，但不是这一个**——它只喂检测阈值；测光减天光永远走逐源局部环；
4. **σ 的来源必须是"扣天光 + 掩源后的残差"**——**没有任何一手来源支持"在未扣天光、未掩源的全帧像素上直接取裁剪 RMS 作为 σ_sky"**。

**未找到一手来源的项（负面清单，不引用）**：Da Costa 1992（ASP Conf. Ser. 23, 90，ADS 需 token，未打开原文）；
Trujillo & Fliri 2016 与 Howell 1989（仅在二手引用中出现，未打开原文）；
iraf.net / stsdas.stsci.edu 的 IRAF 在线帮助在本环境 403/404/502，故 IRAF 一律引 GitHub 官方源码镜像（同一份官方文本）。
---

## 7. ③ 候选修法与比较

### 7.1 候选清单

| 代号 | 内容 | 类别 |
|---|---|---|
| **F0** | 现行：全帧 2 轮 `median ± 3·1.4826·MAD` 裁剪 RMS | 基线（被审对象） |
| **F1a** | 结构感知估计：mesh 局部背景（迭代 2 次）+ 逐 mesh 残差 σ → **跨 mesh 取中值**（SExtractor `backsig` 口径，`back.c:846`） | 估计器侧 |
| **F1b** | 结构感知估计：mesh 局部背景（迭代 2 次）+ **全帧残差裁剪 RMS** | 估计器侧（**推荐主口径**） |
| **F1c** | 用 MAD 尺度（1.4826·MAD）替代裁剪 RMS 作为噪声量 | 估计器侧（**已否决**，见 §7.2） |
| **F2** | provenance 判据：5 个**不需要真值**的代理量 + 三档裁决，估计器旁路输出可信域 | 溯源/合同侧（与 F1 正交） |
| **F3** | 逐源天光环局部 σ（DAOPHOT/IRAF 口径）+ 双帧差分让结构对消（`code/b1_sky_scan.py` 已探路） | 架构侧（**本轮未实现**，见 §11） |

### 7.2 F1c 被否决的实测原因（重要，避免后来人重走）

把噪声量换成 `1.4826·MAD` 后，门在解析臂上从 **0 假绿**变成 **5 条假绿**。
根因：纯噪声下 `MAD` 是**无偏低**的，而裁剪 RMS 有 −1.3% 的固有低偏；
于是代理量 `A2_mad = σ̂_fix/(σ_diff_MAD/√2)` 在纯噪声下是 **0.9867 而不是 1.0000**，
与阈值 `1+δ` 的比较失去对称性，门被系统性放松。

**保留的 A2 口径（推荐）**：分子分母用**同一个**裁剪 RMS 估计量 ⇒
固有裁剪低偏在比值中**精确相消**（实测纯噪声 `A2 = 0.9993–1.0019`，40 次重复 p95 = 1.0012）。

> 一般化教训：**判据里的比值必须用同一估计量**，否则估计量自身的偏差会伪装成物理效应或掩盖物理效应。

### 7.3 F2 的 5 个代理量（全部无需真值）

| 量 | 定义 | 检出什么 | 阈值 | 标定证据 |
|---|---|---|---|---|
| `kf` | 生产裁剪的**保留比**（现成已算，未落盘） | 结构被剪掉多少 | ≥ 0.95 | 真实 M2 帧 0.805–0.829；解析臂 r≥1 时 kf→1.00 |
| `A1` | `σ̂_prod/(σ_diff/√2)` | **宽带**结构存在性（生产侧） | ≤ 1+δ | 解析臂 ramp r=1001.96 ⇒ 1015.9；纯噪声 ⇒ 1.0000 |
| `R` | `σ̂_prod/σ̂_fix` | 生产与结构感知估计的**倍数差** | — | 真实 M2 帧 3.41–3.66 |
| `A2` | `σ̂_fix/(σ_diff/√2)` | **认证量**：修法后残余结构 | **≤ 1+δ** | 与真实 excess 单调对应（图 `a_gate_calibration.png`） |
| `C` | `|σ̂(box)/σ̂(2·box) − 1|` | 尺度收敛性（估计器是否被尺度绑架） | ≤ δ | 解析臂 corr32 在 box=16 时 C 显著；真实帧 C p50 = 0.015–0.025 |
| `D` | `p95(σ_mesh)/p05(σ_mesh)` | 帧内**异质性** | ≤ 1.25 | 真实 M2 帧 3.26–3.66；M1/M3/M4/M6 1.10–1.14 |

**三档裁决（与既有词表一致）**：

```
no_structure  := (A1 <= 1+δ) and (R <= 1+δ) and (kf >= 0.95)
fix_certified := (A2 <= 1+δ) and (C <= δ) and (D <= 1.25) and (kf >= 0.80)
verdict = "ok"              if no_structure and fix_certified   # 可证明可忽略
        = "diagnostic_only" if fix_certified                     # 可证明保守，只出诊断
        = "fail_closed"     otherwise                            # 拒发
```

### 7.4 候选修法定量比较（同一批数据）

| 场景 | F0 生产 | F1a mesh-median | F1b 残差裁剪 RMS | 判据 |
|---|---|---|---|---|
| 解析 `ramp` r=1001.96 | **+100095.5%** | −1.01% | +1.08% | fail_closed（门正确地拒发） |
| 解析 `smooth_corr128` r=1004.27 | **+100327.7%** | +988.4% | **+1768.5%** | fail_closed |
| 解析 `smooth_corr8` r=1.00 | +39.6% | +22.18% | +35.60% | fail_closed |
| 解析 `blob_sigma32` r=1025.49 | +4.18% | **−0.47%** | +5.27% | fail_closed |
| 真实 M2-20251212 [ADU] | 59.408 | **14.876** | 17.414 | fail_closed |
| 真实 M1-20251212 [ADU] | 16.158 | 13.735 | 13.878 | fail_closed |

**结论**：F1a 与 F1b 各有胜场——
- F1a（跨 mesh 中值）对**局部化污染**更稳（`blob_sigma32`、真实 M2 帧），代价是对**极陡梯度**的 mesh 中值有已知的极值偏差；
- F1b（全帧残差）对**极展布结构**更稳（`smooth_corr128` r=1004），且与 A2 判据天然同口径；
- **落地建议同时输出两者**，并把 `R_mesh = σ̂_resid/σ̂_meshmedian` 作为第 6 个异质性代理量（真实 M2 帧 R_mesh = 1.17，M1 帧 = 1.01）。

**F2 与 F1 正交**：F1 修"值"，F2 修"可信域"。仅有 F1 会在结构尺度 ≲ mesh 时静默失效
（`smooth_corr8` r=1.00：F1b 仍偏 +35.6%，A1 = 1.411 与 R = 1.411 也检不出，只有 A2 = 1.3710 能检出）；
仅有 F2 则在结构确实可忽略时白拒发。**两者必须同时落地。**

---

## 8. ④ 判据与红/绿自审（`results/exp02_e4_gates.json`）

运行：`python3 code/exp02/e4_gates_selftest.py`（任一门失败则 exit 1）

### 8.1 门清单与结果：`ALL_GATES_PASS = true`

| 门 | 结果 | 含义 |
|---|---|---|
| `analytic.no_false_certification` | **true** | 解析臂 240 条中 115 条被认证，**假绿 0 条** |
| `hst.no_false_certification` | **true** | HST 臂 22 条中 10 条被认证，**假绿 0 条** |
| `analytic.null_zero` | **true** | 无结构负例：修法效应 Δ = +2.2e-5，3σ = 6.0e-5，**归零** |
| `hst.null_zero` | **true** | 同上（HST 无底图场景） |
| `analytic.gate_not_always_red` | **true** | 有 80 条 `ok` ⇒ 门不恒红 |
| `analytic.gate_not_always_green` | **true** | 有 125 条 `fail_closed` ⇒ 门不恒绿 |
| `hst.gate_not_always_red` / `_green` | **true** | 7 `ok` / 12 `fail_closed` |
| `*.identity_fix_red` | **true** | 故障注入①："修法=恒等映射" ⇒ 判红 |
| `*.global_mesh_red` | **true** | 故障注入②："mesh 退化为整帧一个 mesh（＝现行全局做法）" ⇒ 判红 |
| `*.gate_disabled_certifies_bad` | **true** | 故障注入③："门短路恒判 ok" ⇒ 认证了超出预算的帧 |

### 8.2 认证判据的**双重预算**口径（避免把"改善"误判为"违规"）

这是本单元在自审中发现并修正的一处口径错误，如实登记：

- 门内的 δ=1.4% 是**结构项**预算（`A2 ≤ 1+δ`，两侧同估计量 ⇒ 固有裁剪低偏相消）；
- σ̂ 相对**真值**的总误差还含固有裁剪低偏 `S_clip`（本臂实测 1.38%–1.81%），
  它是 EXP-01 已单列的、另预算的系统项 ⇒ 认证的**审计**判据是 `|σ̂_fix/σ_true − 1| ≤ δ + S_clip`。
- 若只用"相对无结构基线的 excess"审计，会把**结构把估计拉回真值**的情形误判为违规
  （HST 臂实测：`A2 = 0.9975` 时 excess = +2.06%，但 `|总误差| = 0.47%` < 无结构基线 1.81%）。

两条口径同时报出（两臂**均为 0 假绿**于总预算口径）：

| 臂 | 认证条数 | 假绿（总预算 δ+S_clip） | 假绿（excess 口径） | 最差总误差（认证集内） |
|---|---|---|---|---|
| 解析 | 115 | **0** | **0** | 1.75% |
| HST | 10 | **0** | 5（口径 artifact，见上） | 2.47% |

### 8.3 负例（非退化性）明细

**解析臂无结构负例**（40 次重复，纯高斯，无结构）：

```
fix_effect_delta  mean = +2.246e-05   sem = 2.011e-05   3σ = 6.033e-05   max|.| = 3.27e-04
prod_rel_err_mean = -1.384%          fix_rel_err_mean = -1.382%   （二者一致）
A1 mean = 1.00003  p95 = 1.00120     A2 mean = 1.00006  p95 = 1.00121
gate_verdict_counts = {ok: 40}       G_null_zero_pass = true
```

**HST 臂无结构负例**（8 次重复）——**必须关掉平场乘性结构**才能成立：

```
flat OFF:  delta_mean = +5.482e-05   3σ = 8.079e-05   max|.| = 1.71e-04   PASS
flat ON :  delta_mean = -1.609e-03   （PRNU/低阶/渐晕是真实乘性结构，mesh 修法理应吸收它）
⇒ 本臂的"结构无效应"负例口径 = flat.prnu_rms = low_order = vignette = 0
```

---

## 9. ⑤ 定量关系表

### 9.1 主关系：σ̂/σ_true − 1 随结构幅度/尺度

| 结构类型 | 结构尺度 vs mesh | 半解析预测 | 实测（解析臂） | 实测（HST 臂） |
|---|---|---|---|---|
| 无结构 | — | 0（只剩 −S_clip） | −1.3% | −1.6% |
| 大尺度平滑（相关长度 ≫ mesh） | ≫ mesh | `sqrt(1+r²) − 1` | r=1004 ⇒ **+100327.7%** | p99.9/σ=8551 ⇒ **+18240%** |
| 线性斜坡（尺度 = 帧） | ≫ mesh | `sqrt(1+r²) − 1` | r=1002 ⇒ **+100095.5%**（解析式 = +100196%） | — |
| 中尺度（相关长度 ≈ mesh） | ≈ mesh | 部分存活，无闭式 | r=100.16 ⇒ +9627.4% | p99.9/σ=171 ⇒ +296% |
| 小尺度（相关长度 ≪ mesh） | ≪ mesh | 大部分被 mesh 吸收 | r=1 时 F0 +39.6% / **F1b 仍 +35.6%** | — |
| 紧凑团块 | ≪ mesh | 被裁剪剔除 | r=1025 ⇒ **+4.18%** | — |

**"何时失去 σ_sky 语义"的判据（本单元给出的操作定义）**：

> 当 `A2 = σ̂_fix/(σ_diff/√2) > 1 + δ`（δ = 该链的容差预算，本实验取 1.4%）时，
> **没有任何基于全帧统计的 σ̂ 能被认证为 σ_sky**；此时应判 `fail_closed`。
> 对现行 F0 估计器，等价的粗判据是大尺度结构 `r = σ_s/σ_n ≳ 0.3`。

### 9.2 代理量 → 真实误差的标定（解析臂 box=32）

| 代理量 | 纯噪声值（应 = 1 或 0） | r=0.3 时 | r=1 时 | r=100 时 | 与真实 excess 的关系 |
|---|---|---|---|---|---|
| `A1` | 1.0000 | 1.045 | 1.41 | 101.6 | 结构幅度 ≫ 噪声时 ≈ `sqrt(1+r²)` |
| `A2` | 1.0000 | 1.0007–1.04 | 1.001–1.37 | 1.02–16.5 | ≈ `1 + 真实 excess`（同估计量口径） |
| `kf` | 0.9972 | 0.997 | 0.997–1.000 | 1.000 | 结构越大 kf 越接近 1 |
| `C` | 0.0 | ≤0.014 | ≤0.04 | ≤0.09 | 检尺度绑架 |
| `D` | 1.08 | 1.08–1.10 | 1.08–1.48 | 3.2–7.7 | 检帧内异质性 |

### 9.3 真实帧上的量级（无真值，只报可证项）

| 量 | M1/M3/M4/M6 板块 | M2/M5 板块（星云主体） |
|---|---|---|
| `σ̂_prod` [ADU] | 15.6–19.0 | 43.4–64.0 |
| `kf` | 0.942–0.984 | **0.805–0.832** |
| `σ̂_fix`(box=64) [ADU] | 13.0–14.3 | 16.4–18.4 |
| `R = σ̂_prod/σ̂_fix` | 1.19–1.40 | **2.44–3.66** |
| `A1` | 1.21–1.44 | **2.64–4.21** |
| `A2` | 1.020–1.030 | **1.081–1.149** |
| `D` | 1.10–1.14 | **2.62–3.66** |
| 块间 σ p95/p05 | 1.09–1.37 | **7.13–9.66** |
| 判据（16/16） | fail_closed | fail_closed |

---

## 10. ⑥ 落地建议（由前台按序执行，本单元不落地）

### 10.1 文档侧

| 动作 | 位置 | 内容 |
|---|---|---|
| 新增小节 | `docs/science/`（沿 `ASTROCS_DESIGN.md` §2.2 索引下钻）——"帧级 σ_sky 的估计口径与可信域" | ① 明确 `σ_sky` 的语义 = **天光扣除后、源剔除后的残差尺度**；② 给出本报告 §9.1 的失效判据；③ 明确"帧级全局 σ"与"逐源局部 σ_sky"是**两个不同语义的量，不可互替**（DAOPHOT `SKY` vs `fitskypars` 的分工为先例） |
| 订正措辞 | `docs/` 中描述 `noise_sigma` 为 "1.4826*MAD 稳健尺度" 之处 | 代码事实是**裁剪 RMS**；订正为准确描述 |
| 登记缺陷 | EXP-01 D8 缺陷登记表 | 升级为"已独立复核 + 已给修法与判据"，链到本报告 |

### 10.2 代码符号侧（**本单元不改，只给锚点**）

| 符号 | 现状 | 建议 |
|---|---|---|
| `StarDetector::estimate_background` | 全帧 2 轮裁剪 RMS | **新增** mesh 局部背景（box 默认 64、`filter_size=3`、迭代 2 次）+ 残差裁剪 RMS；**保留**原函数作为 `global_clip_rms` 口径供检测阈值使用 |
| `StarDetector::detect` | `thr = bg + detection_sigma_·noise_sigma` | **保留全局口径**（检测阈值本来就该用全局量，DAOPHOT `SKY`→`FIND` 为先例）；但需明确注释其语义边界 |
| `module_adapters.cpp` 的 `{"noise_sigma", cat.noise_sigma}` | 唯一写入点 | 改为写 `noise_sigma`（结构感知口径）+ `noise_sigma_provenance` 子对象（§10.3） |
| `module_adapters.cpp` 的 `cfg.sigma_sky_adu = src_frame->value("noise_sigma", 0.0)` | 无条件读取 | 按 `verdict` 分派：`ok`/`diagnostic_only` 照读；`fail_closed` ⇒ 置 0/缺省，让 `snr_frame_science.cpp` 既有的 fail-closed 分支生效 |
| `snr_frame_science.cpp` 的 `σ_sky ≤ 0` fail-closed | 已有 | 扩展为同时检查 provenance 的 `verdict` |

### 10.3 provenance 字段（建议命名空间 `noise_sigma_provenance`）

```jsonc
"noise_sigma": 13.8779,                    // ADU；结构感知口径
"noise_sigma_provenance": {
  "estimator":   "mesh_resid_clip_rms",    // | "global_clip_rms"(旧口径) | "mesh_sigma_median"
  "verdict":     "fail_closed",            // "ok" | "diagnostic_only" | "fail_closed"
  "delta_budget": 0.014,
  "box_px": 64, "filter_size": 3, "n_iter": 2,
  "keep_frac":   0.9794,                   // 生产裁剪保留比（现成已算，零成本落盘）
  "a1": 1.193, "a2": 1.0243, "c_scale": 0.019, "d_mesh": 1.11,
  "sigma_global_clip_rms": 16.1581,        // 旧口径保留，供检测阈值与回归比对
  "sigma_mesh_median":     13.7350,        // SExtractor backsig 口径
  "r_mesh": 1.0104                         // sigma_resid / sigma_mesh_median
}
```

> 全部字段**零额外代价可得**（`keep_frac` 现成已算；其余是同一遍 mesh 的副产物），
> 且全部**不依赖真值** ⇒ 可在生产运行时逐帧算出并落盘。

### 10.4 三档消费规则（与既有词表一致）

| verdict | 语义 | σ_sky 的消费 |
|---|---|---|
| `ok` | 可证明可忽略 | 正常进入 `sigma_i² = σ_sky² + F·P_i/g` |
| `diagnostic_only` | 可证明保守，只出诊断 | 帧级 SNR 降级为诊断输出，**不进跨帧绝对 SNR 标定** |
| `fail_closed` | 拒发 | `sigma_sky_adu = 0` ⇒ `snr_frame_science.cpp` 既有 fail-closed 分支；帧登记为不可标定 |

### 10.5 对 SCI-B 的直接后果（须上呈负责人裁决）

本单元的实测把一条**架构级**问题摆到了台面上：

1. 真实 M42 T2 帧的**块间 σ 离散 p95/p05 高至 9.66** ⇒ 这些帧**根本不存在**一个"帧级 σ_sky"；
2. 即便换成 mesh 结构感知估计，残余结构仍达 2.0%–14.9%（A2）⇒ 这类帧在本判据下**必然 fail_closed**；
3. ⇒ 若绝对 SNR 链坚持"帧级标量 σ_sky"，则**星云场帧将系统性无法参与跨帧标定**。

这不是 δ 能预算的项，而是**量本身的定义域问题**。可选的架构出路（需要独立实验单元验证，本单元不裁决）：

- **(i) 逐源/逐区域 σ_sky**（DAOPHOT/IRAF 口径）：绝对 SNR 链本来就逐源，把 σ_sky 也做成稀疏量，
  与 §5.3 已定义的 `sparse_reconstruct` 模式天然契合；
- **(ii) 双帧差分让结构对消**：`code/b1_sky_scan.py` 已探路，Watkins et al. 2024 的 dither/combine 思路同源；
- **(iii) 结构掩膜 + 低空间自由度模型**（NoiseChisel/Byun 2018 口径）：只在天光确实平坦的像素上测 σ。

按 `AGENTS.md` §8，这类"存在互斥产品方向取舍"的取舍应**带证据上呈负责人**；
本单元的责任是把证据、判据与候选方案交齐（已完成）。
---

## 11. ⑦ 诚实边界与未核实项

### 11.1 解析可证的部分（不依赖任何仿真假设）

1. 现行估计器以**全局中位数**为裁剪中心、作用域是**整帧** ⇒ 成片存在的大尺度结构**不是离群点**；
2. 裁剪阈值 `∝ σ_total` ⇒ 裁剪窗口被结构自身抬高 ⇒ 结构像素的保留比随结构增大趋近 1；
3. 当裁剪近乎无效时，`σ̂/σ_n → sqrt(1+r²)`（解析臂实测偏差 0.1%）；
4. 裁剪是尾部剔除器 ⇒ 紧凑结构被剔除、大尺度结构不被剔除（解析臂两端实测相差 4 个数量级）；
5. `A2` 两侧用同一估计量 ⇒ 固有裁剪低偏精确相消（纯噪声实测 0.9993–1.0019）；
6. 上中位数 vs `np.median` 的差异 **−0.94%**、串行 vs 成对求和 **8.2e-15**、两条生产镜像 σ̂ 相对差 **−2.2e-10**。

### 11.2 依赖仿真假设的部分（结论随假设可变）

| 结论 | 依赖的假设 | 若假设改变的后果 |
|---|---|---|
| HST 臂的具体污染百分比 | 前向仿真的全部物理项（增益/读噪/饱和/量化/平场/PSF/天光率） | **百分比会变**；但"结构未被剪掉 ⇒ 污染"的**方向与机理不变** |
| `δ = 1.4%` 这个预算值 | 取自 EXP-01 的 S_sys 分解 | 改成别的 δ 只会平移 §9.1 的"何时失去语义"阈值，**判据结构不变** |
| mesh 修法的 `box=32/64`、`filter_size=3`、`n_iter=2` | 经验选择（对齐 SExtractor 默认语义） | box 过小会吃掉展源翼部流量（Watkins 2024 的反向条款）；本单元**未**做 box 的完整最优化 |
| 解析臂的"结构 rms"口径 | 解析构造的 rms 定义 | 见 §2.2：rms 比值**本身就不是**充分统计量，故本单元不把它当判据 |

### 11.3 未验证 / 未核实的项（明确列出，不冒充已验证）

1. **本单元未运行任何 ACSD 可执行文件** ⇒ "生产代码实际跑出来的 `noise_sigma` 与本单元镜像一致"
   只有**源码级 + 数值级**证据（§3.4），**没有**端到端运行证据；
2. **真实数据臂没有真值** ⇒ 只能给自洽量（σ̂ 倍数、保留比、块间离散、代理量分布）；
   真实 M42 帧上的"污染 2.4–3.7 倍"是**相对于 mesh 估计**的，不是相对于真值；
3. **F3（逐源天光环 σ / 双帧差分）本轮未实现、未做实验** ⇒ §10.5 的三条出路只是文献与架构上的候选，**没有本单元的实测支持**；
4. **SExtractor 2.28.2 与 photutils 2.2.0 的对照数字由子代理本地编译/安装后实测**，本单元**未独立重跑**；
   但两者的源码行号与公式已逐条读过（§6.1、§6.2）；
5. **DAOPHOT II 手册为非同行评审来源**（Stetson 编写的手册），仅用作"全局 SKY 命令用途"的佐证，
   该论断同时由 Stetson 1987 §V.A 的同行评审文本支持；
6. **未找到**"专门论证天光环内/外半径最优取值"的同行评审论文；
7. **负面清单**（明确声明找不到一手来源、故不引用）：`skyestimator` / `sigmareject` / `clipiters` 三个参数名；
   `daopars.fitsky` 为"常数/线性/二次"选择；"二次天光面在 Stetson 1987 中存在"；
   Da Costa 1992、Trujillo & Fliri 2016、Howell 1989（均未打开原文）；
8. **A1/A2 灵敏但不特异**：真实帧若有**相关噪声**（drizzle/重采样）会天然给出 `A1 > 1`，门会**偏保守**（假红）；
   本单元在真实臂上无法测假红率（无真值）；
9. **MAD 版差分参考在真实帧上偏低**（`A2_mad/A2` 中位 0.981，M2 帧 `A2_meshmedian/A2` 低至 0.957）；
   合理解释是差分场被未掩膜的星点翼/宇宙线抬高，但**本单元未做掩膜实验来证实**；
10. **真实帧 16/16 全部 fail_closed**：本单元能证明"在 1.4% 预算下不可认证"，
    **不能**证明"这些帧的 σ_sky 无法用任何方法认证"；
11. **网格最优化未做**：`box`、`filter_size`、`n_iter`、`k` 只做了有限的扫描（box ∈ {16,32,64,128}），
    未做联合最优化，也未评估 `BACK_SIZE` 过小导致的展源过减风险（Watkins 2024 条款）；
12. **本单元没有独立子代理审稿**：按 `AGENTS.md` §5，结论应经独立子代理多轮审稿；
    本单元的两份文献查证由两个独立子代理完成（SExtractor/photutils 与 DAOPHOT/IRAF），
    **但主结论（缺陷复核 + 修法 + 判据）尚未经第三方子代理复核**，建议前台在提交前补一轮。

### 11.4 过程留痕（自己的错，一并登记）

| # | 我的错误 | 发现方式 | 处置 |
|---|---|---|---|
| 1 | 中期判定"EXP-01 结构/噪声列少乘曝光时间 300×" | 复核 `render.py::render_frame` 的数据流 | **撤回**；根因是把 `rb_rate` 误当"每秒"量（`frame.src_e ≡ rb_rate`，实测比 1.000000）。已向父代理发更正 |
| 2 | mesh 背景图边界被"钉住" | 线性斜坡上残差 RMS 异常（+2.1%） | 改为线性外推 1 格（§2.3） |
| 3 | 偶数窗口 median filter | 线性斜坡上残差 RMS +13000% | 改奇数窗口 + `mode="nearest"`（§2.3） |
| 4 | 门用 `R_struct` 单代理量 ⇒ 29/240 假绿 | 逐条核对认证集 | 增加 `A1`/`A2` 差分代理量 ⇒ **0 假绿**（§7.3） |
| 5 | HST 负例初版失败（Δ = −0.16%） | 检查场景 `flat` 参数 | 负例必须关掉平场乘性结构（§8.3） |
| 6 | HST 臂初版按"结构 rms/噪声"参数化 ⇒ 结果不可解释 | 同一 rms 比对应 4 个数量级差异 | 改为按 **p99.9/σ** 参数化，并同时保留 rms 列（§5.2） |
| 7 | 认证审计误用"相对无结构基线的 excess" | HST 臂出现 5 条"假绿" | 改为 `δ + S_clip` 双重预算口径（§8.2） |
| 8 | `_expand_mesh_map` 在 1×1 mesh（box=帧宽）时索引越界 | 故障注入②崩溃 | 退化网格按常值复制（§8.1 注入②） |

---

## 12. 复现命令

全部命令在 `实验/absolute-snr/` 下执行；外部命令一律带 `timeout`，RSS 用 `/usr/bin/time -v` 记录。
**不使用 `ulimit -v`**；**不运行任何 ACSD 可执行文件**。

```bash
cd "实验/absolute-snr"

# 一键复现（约 8 分钟；峰值 RSS ≤ 0.92 GB）
bash code/exp02/run_all.sh

# 或分臂执行
timeout 3600 python3 code/exp02/e1_analytic_scan.py            # 臂 A 解析      ~104 s
timeout 3600 python3 code/exp02/e2_hst_scan.py                 # 臂 B HST 前向  ~31 s
timeout 3600 python3 code/exp02/e3_real_data.py --crop 2048    # 臂 C 真实数据  ~261 s
timeout  900 python3 code/exp02/e4_gates_selftest.py           # 门自审（失败 exit 1）
timeout  600 python3 code/exp02/make_tables.py                 # 图与表

# 带 RSS 记录（推荐）
/usr/bin/time -v timeout 3600 python3 code/exp02/e1_analytic_scan.py \
  > ../../run/SCI-B-EXP-02/logs/e1_analytic.log 2>&1
```

**产物清单**：

| 路径 | 内容 |
|---|---|
| `code/exp02/exp02_common.py` | 生产 recipe 独立重写、mesh 背景/σ、差分参考、代理量、三档门、结构生成器 |
| `code/exp02/e1_analytic_scan.py` | 臂 A（解析） |
| `code/exp02/e2_hst_scan.py` | 臂 B（HST 真实模板 + 物理前向） |
| `code/exp02/e3_real_data.py` | 臂 C（testdata 真实帧） |
| `code/exp02/e4_gates_selftest.py` | 门自审 + 故障注入 + 候选对比 |
| `code/exp02/make_tables.py` | 图与 Markdown 表生成 |
| `results/exp02_e1_analytic.json` / `e2_hst.json` / `e3_real.json` / `e4_gates.json` | 全部原始结果（含 seed 与元数据） |
| `results/EXP02_TABLES.md` | 完整表格（自动生成） |
| `results/exp02_figs/*.png` | 5 张关系曲线/标定图 |
| `run/SCI-B-EXP-02/logs/*.log` | 运行日志（含 `/usr/bin/time -v` 的峰值 RSS） |

**seed**：全部臂 `20260925`（`e1`/`e2` 显式常量；`e3` 无随机性，确定性读帧）。

---

## 附录 A：本报告引用的外部来源一览

**论文 / 标准**

- Bertin, E. & Arnouts, S. 1996, A&AS **117**, 393–404. DOI [10.1051/aas:1996164](https://doi.org/10.1051/aas:1996164)
- Stetson, P. B. 1987, PASP **99**, 191–222. DOI [10.1086/131977](https://doi.org/10.1086/131977)｜ADS [1987PASP...99..191S](https://articles.adsabs.harvard.edu/pdf/1987PASP...99..191S)
- Dolphin, A. E. 2000, PASP **112**, 1383–1396. DOI [10.1086/316630](https://doi.org/10.1086/316630)｜[arXiv:astro-ph/0006217](https://arxiv.org/abs/astro-ph/0006217)
- Bosch, J. et al. 2018, PASJ **70**, S5. DOI [10.1093/pasj/psx080](https://doi.org/10.1093/pasj/psx080)｜[arXiv:1705.06766](https://arxiv.org/abs/1705.06766)
- Kelvin, L. S., Hasan, I. & Tyson, J. A. 2023, MNRAS **520**, 2484. DOI [10.1093/mnras/stad180](https://doi.org/10.1093/mnras/stad180)｜[arXiv:2301.05793](https://arxiv.org/abs/2301.05793)
- Watkins, A. E. et al. 2024, MNRAS **528**, 4289. DOI [10.1093/mnras/stae236](https://doi.org/10.1093/mnras/stae236)｜[arXiv:2401.12297](https://arxiv.org/abs/2401.12297)
- Akhlaghi, M. & Ichikawa, T. 2015, ApJS **220**, 1. DOI [10.1088/0067-0049/220/1/1](https://doi.org/10.1088/0067-0049/220/1/1)｜[arXiv:1505.01664](https://arxiv.org/abs/1505.01664)
- Byun, W. et al. 2018, AJ **156**, 249. DOI [10.3847/1538-3881/aae647](https://doi.org/10.3847/1538-3881/aae647)｜[arXiv:1810.02075](https://arxiv.org/abs/1810.02075)

**开源实现（项目 + 版本 + 文件:行）**

- SExtractor **2.28.2**，commit `5e82a8d17e19a0ffc981b59d6776630c885f388f`：`src/back.c`、`src/preflist.h`、`src/prefs.c`、`src/weight.c`、`src/readimage.c`、`src/analyse.c`、`config/default.sex`、`doc/src/Background.rst`、`doc/src/Photom.rst`
- photutils **2.2.0**：`photutils/background/background_2d.py`、`photutils/background/core.py`；文档 <https://photutils.readthedocs.io/en/2.2.0/>
- IRAF（iraf-community/iraf，main）：`noao/digiphot/apphot/doc/fitskypars.hlp`、`noao/digiphot/daophot/doc/fitskypars.hlp`、`noao/digiphot/daophot/daopars.par`、`noao/digiphot/apphot/doc/datapars.hlp`
- DAOPHOT II User Manual（Stetson；非同行评审）：<https://www.ucolick.org/~bolte/AY257/HMWK3_2015/daophotii.pdf>

**仓内只读引用**

- `实验/absolute-snr/docs/EXP-01-DELTA-AND-ESTIMATOR.md`（缺陷来源，§2.2②、D8）
- `实验/shared/synthetic/render.py`、`实验/shared/synthetic/noise_model.py`（共享仿真器，只读 import）
- `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp`（`StarDetector::estimate_background`、`StarDetector::detect`）
- `lib/infrastructure/scheduler/src/module_adapters.cpp`（`noise_sigma` 写点与 `sigma_sky_adu` 读点）
- `lib/algorithms/snr/*`（`snr_science.cpp`、`snr_frame_science.cpp`）
- `testdata/HST_M16/*.fits`、`testdata/M42_T2T3_mosaic_Flying_dutchman/T2/**/*-300S-Red.fts`（只读）
