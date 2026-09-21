# 帧级 SNR 定案（FRAME-SNR-CANON）

> 工作项：**FRAME-SNR-CANON**（RELEASE-02）。调研记录：`实验/SCI-B/docs/surveys/frame-snr-survey.md`。
> 实验代码：`实验/SCI-B/code/reverse_verify/frame_snr/`（**纯 Python，独立构建，不并入主线**）。
> 中间产物：`run/reverse_verify/frame_snr/`（gitignore）。
> 硬约束遵守情况：**未改** `lib/` `docs/` `eng/tests/` `ci/`；**零 git 写**；**未跑** `ninja`/`cmake`/`ctest`。
> 唯一的外部编译是 `g++` 直编生产 TU（只读）用于对拍，见 §3 P12。

---

## 0 一句话定案

> **AstroCS 的帧级 SNR 是「点源（PSF）信号 SNR」：**
> `SNR_frame = F_signal / sigma_F`，
> **`F_signal` 已扣局部背景（天光均值绝不进分子）**，
> **`sigma_F` 由 PSF 加权最优提取的方差给出、且天光散粒噪声必须计入**。
> **固定源通量、天光变亮 ⇒ SNR 严格单调下降；天光→∞ ⇒ SNR→0。**
> 它是**无量纲**量，**不需要** gain / 口径 / 曝光时间等 FITS 头拿不到的物理量；
> **不得**与面亮度 SNR 混用，**不得**被天光抬高。

---

## 1 调研结论：各类科学文献与主流实现到底用什么（分类归纳）

> 逐条记录见 `实验/SCI-B/docs/surveys/frame-snr-survey.md`；此处只给归纳与判定。

### 1.1 分类归纳

| 类别 | 对象 | 定义「帧级 SNR 标量」？ | SNR/误差定义 | 天光均值进分子？ | 天光散粒进分母？ |
|---|---|---|---|---|---|
| 巡天管线 | **LSST/Rubin SMTN-002** | **是**（唯一；仅用于深度 m5，非产品字段） | `SNR = C/sqrt(C/g + (B/g+σ_instr²)·n_eff)` | **否** | **是** |
| 巡天管线 | LSST `meas_base` | 否（`grep -i snr` = 0 命中） | 逐源 `instFlux/instFluxErr` | 否 | 是（方差图 `var=image/gain+(RN/gain)²`；扣背景不改方差） |
| 巡天管线 | SDSS photoop/frames | 否（只有帧级噪声 `sigPix`） | Eq(25/26) PSF MLE + `Var=4πα²n²` | 否 | 是（`n²` 含 "photon noise from the sky"）；源泊松**否** |
| 巡天管线 | DES DM | 否（只有 `FLUXERR`/`MAGERR`） | SExtractor 式 | 否 | 是 |
| 巡天管线 | Pan-STARRS IPP/psphot | 否（`SN` 有三条不同口径） | Kron/矩/PSF 三套 | 否（上游已扣，内部 `sky≡0`） | 部分核对 |
| 巡天管线 | HSC | 否 | Eq(28) matched filter + Eq(30) `σ_MF²` | 否 | 是（Eq(32) 用**减除前**的背景水平 `b`） |
| 巡天管线 | JWST `source_catalog` | 否（`snr` 0 命中） | 来自 ERR 扩展，`sqrt(Σw²σ²)` | 否 | 是 |
| 巡天管线 | HST/DrizzlePac | 否（只有 weight `W=1/(Var·scale⁴)`） | IVM `= flat²/(dark+sky·flat+RN²)` | 否 | 是 |
| 测光工具 | SExtractor | 否（逐源） | `FLUXERR=sqrt(Σ(σ_i²+p_i/g_i))`；`SNR_WIN=FLUX_WIN/FLUXERR_WIN` | 否（`p_i` 已扣背景） | 是 |
| 测光工具 | DAOPHOT/IRAF `phot` | 否（逐源 `merr`） | `err=sqrt(flux/epadu + area·stdev² + area²·stdev²/nsky)` | 否 | 是（且**含 nsky 项**） |
| 测光工具 | DAOPHOT/IRAF `allstar` | 否（协方差对角元） | 四项逐像素方差 | 是（作 `I` 的一部分） | 是 |
| 测光工具 | PSFEx | **不定义、不输出** | 只有拟合权重 `σ_i²=σ_b²+p_i/g+(αp_i)²` | 否 | 是 |
| 测光工具 | photutils 3.0.0 | 否 | `aperture_sum_err=sqrt(Σw²σ_tot²)` | 否（要求 data 已扣背景） | 是（**须调用方写进 error 数组**） |
| 测光工具 | `sep` 1.4.1 | 否 | `sumerr=sqrt(Σvar + sum/gain)` | 否 | 是（**但重复计入两次**） |
| 理论 | Horne 1986 / Naylor 1998 | —（最优提取） | `σ_F^-2 = Σ_i P_i²/σ_i²`，`SNR_F=F/σ_F` | 否 | 是 |
| 反例 | PixInsight "standard SNR" 式[20] | —（**反面教材**） | `σ²/σ_n²`，**分子是图像方差** | **是 ⇒ 被抬高** | — |

### 1.2 五条可引用的共识（本定案的文献基础）

1. **没有任何一家把天光均值放进 SNR 分子。** 七家巡天 + 六款测光工具，全部把天光从信号里扣掉
   （SExtractor `p_i` "subtracted from the background"；DAOPHOT `flux = sum − area·msky`；
   SDSS `O_i` "with the sky background subtracted"；HSC/psphot/JWST/HST 同理）。
2. **天光散粒噪声必须进分母。** 各家的承载方式不同（SExtractor 的 `σ_i²`、DAOPHOT 的 `stdev²`、
   LSST 的方差图 `image/gain`、HST 的 IVM、JWST 的 ERR、HSC 的 `b+α(φ+ε)`），但**都进**。
3. **扣背景 ≠ 扣噪声。** LSST 有一手实现级证据：`MaskedImage::operator-=` 只改 image 面
   （`afw/include/lsst/afw/image/MaskedImage.h:823-826`），方差面原样保留。
4. **源自身泊松是否进分母，各家取舍不同**（SDSS 不进、SExtractor/HSC/JWST/LSST 进、HST-IVM 不进）
   ⇒ **AstroCS 必须显式声明自己的取舍**，不能默认"大家都这么做"。
5. **帧级 SNR 是点源（PSF）SNR，不是面亮度 SNR。** LSST 的 `n_eff = 2.266(FWHM/pixelScale)²`
   与 HSC 的 matched filter 都是**点源最优**口径；Zackay & Ofek 2017 明确"面亮度最优组合是逆方差加权、
   点源最优是 PSF 匹配滤波，两者不可混用"。

### 1.3 「会被天光抬高」的定义（必须否决）

| 编号 | 定义 | 为什么被抬高 | 一手依据 |
|---|---|---|---|
| **RED-A** | 未扣背景通量型 `(F + n_pix·B)/σ` | 分子含天光基座，`B→∞` 时 `SNR→√(n_pix·B)→∞` | SExtractor/DAOPHOT **显式扣背景**（反证） |
| **RED-B** | 功率比型 `(Σ I)²/Σσ²` / PixInsight 式[20] `σ²/σ_n²` | **分子是图像方差/和的平方**，任何加性图像内容抬高它 | PixInsight 官方逐字：*"a big airplane trail that introduces a **strong bias in the variance used as the numerator of the SNR equation** (see Equation [20])"* |
| **RED-C** | 未扣局部背景的窗口口径 `Σ_i w_i I_i / sqrt(Σ w_i²σ_i²)` | 窗口和含天光基座 | SExtractor `SNR_WIN` 的分子取自**已减背景**图像（反证） |

**判定一个口径是否安全的唯一问句：分子里有没有天光均值？** 有 ⇒ 必被抬高 ⇒ 否决。

> **注意区分**：PixInsight 的 **PSFSNR（式[18]）** 分子是"FWTM 孔径内像素**减局部背景**求和"的平方，
> 因此**它本身不会被天光均值抬高**（天光只进 `σ_n`）。AstroCS 不采用它的**功率比形式**，
> 只借鉴方法学（信号取数、稳健噪声、独立背景）—— 与本仓 `docs/plugins/algorithms_phase1/07_noise_snr.md` 一致。

---

## 2 定案定义式

### 2.1 逐源科学 SNR（基础）

对已扣局部背景的源通量 `F`，令 `P_i` 为**离散归一化 PSF**（`P_i >= 0`，`sum_i P_i = 1`）：

```
sigma_i^2  = sigma_sky^2 + (sigma_R/g)^2 + max(F,0)*P_i/g          [ADU^2]        (2.1)
sigma_F^-2 = sum_i P_i^2 / sigma_i^2                                              (2.2)
SNR_F      = F / sigma_F                                                          (2.3)
```

- **(2.1)** 逐像素总方差：天光散粒 + 读出 + 源散粒。`sigma_sky` 是**逐像素空背景 rms**（含天光散粒；
  若它已含读出噪声，则 `(sigma_R/g)^2` 项**必须置零**以免重复计入 —— 见 §4 差距 G4）。
- **(2.2)** Horne 1986 最优提取（对角/白噪声近似）。
- **(2.3)** 通量型（一次方比），**不是**功率比。

### 2.2 帧级 SNR（定案）

帧级量是 (2.3) 在**组内公共参考通量** `F_ref` 上的取值：

```
SNR_frame(k) = F_ref / sigma_F,k        (F_ref 组内公共; sigma_F,k 逐帧)          (2.4)
```

其中 `sigma_F,k` 用**该帧**的 `sigma_sky,k` 与 PSF 轮廓代入 (2.1)(2.2)。

**天空受限简化式（生产实际使用的分支，gain 未知时）**：当 `g <= 0`（FITS 头拿不到增益）时
源泊松项不可评估，(2.2) 退化为

```
sigma_F   = sigma_sky / sqrt(sum_i P_i^2) = sigma_sky * sqrt(A_NEA)               (2.5)
A_NEA    ≡ 1 / sum_i P_i^2                                        [pixel]         (2.6)
SNR_frame = F_ref * sqrt(sum_i P_i^2) / sigma_sky                                 (2.7)
```

**(2.7) 是本项目唯一可无条件执行的帧级 SNR 式**：它只用到 `F_ref`（ADU）与 `sigma_sky`（ADU），
**不需要 gain / 口径 / 曝光时间**（见 §2.4）。

### 2.3 符号与单位

| 符号 | 含义 | 单位 | 备注 |
|---|---|---|---|
| `F` / `F_ref` | 源总通量 / 组内公共参考通量，**已扣局部背景** | 与图像同标度的线性信号单位（ADU） | **绝不含天光均值** |
| `sigma_F` | `F` 的通量不确定度 | 与 `F` 同单位（ADU） | 含天光散粒、读出、源散粒 |
| `sigma_sky` | 逐像素空背景 rms | ADU | 由**帧本身**稳健估计（MAD×1.4826） |
| `P_i` | 离散归一化 PSF | 1/pixel | `sum P_i = 1` |
| `A_NEA` | 等效噪声面积 `1/ΣP_i²` | pixel | 由 Horne 1986 在 `σ_i=σ` 常数下直接推出（**未定位独立一手出处**，见调研 C5） |
| `g` | 转换增益 | e-/ADU | **可缺省**；缺省时用 (2.5) |
| `sigma_R` | 读出噪声 | e- | 可缺省 |
| `SNR_frame` | 帧级信噪比 | **无量纲 [1]** | 对信号单位线性缩放**严格不变**（P14 实测 1.5e-15） |
| `m_5` | 5σ 点源深度 `ZP − 2.5 log10(5 sigma_F(ref))` | mag | 需要 ZP；**当前生产从不产出**（§4 G2） |
| `sigma_m` | 星等误差 | mag | `sigma_m = 1.0857 / SNR`（DAOPHOT `merr = 1.0857·err/flux`，1.0857 = 2.5/ln10） |

**`F_ref` 的角色**：它是**跨帧可比性的锚**。若不固定 `F_ref`，各帧的 `SNR_frame` 会随各自星等分布漂移，
无法比较。定案要求 `F_ref` **组内公共**（同一 output_dir 的所有帧同一个值），
且在产物里**显式落盘**（生产已做到：`p1_snr.json` 的 `reference_flux_scope="group"`、
`reference_flux_source="group_median"`、`reference_flux_adu`）。

### 2.4 无物理单位闭合（**负责人 2026-09-19 纠正的强制要求**）

**禁止**用物理闭合式（如 `k = g·h·c·1e9/(A·t)`）反推增益/口径/曝光来"补"出物理单位。
设计前提就是 **FITS 头拿不到这些量**。因此定案式必须满足：

| 要求 | 定案式的满足情况 |
|---|---|
| 不依赖 gain | (2.7) 只用 `F_ref` 与 `sigma_sky`；`g` 仅在可选精化项 (2.1) 出现 |
| 不依赖口径/焦距/像素尺度 | 完全不出现 |
| 不依赖曝光时间 | 完全不出现 |
| 信号单位可任意线性缩放 | `SNR` 无量纲，`F` 与 `sigma_sky` 同标度 ⇒ **严格不变**（P14） |
| 与测光坐标系（星等）兼容 | `sigma_m = 1.0857/SNR`；星等零点在 `SNR` 中约掉 |
| `k_photo` 的绝对值 | **无物理意义**（吸收增益/口径/曝光等未知量）；有意义的只有**测光一致性**与**帧间一致性**两个尺度无关判据 |

> **本文的实验代码不反推任何物理量。** 仿真里的 `gain`/`read_noise` 是**合成旋钮**（设定值），
> 从不从真实帧反推；真实数据实验（P13）只用真实帧的**结构**，天光水平由外部设定。

### 2.5 点源 vs 面源：适用边界与禁止宣称

- **本帧级量是点源（PSF）SNR**：分母是 `1/ΣP_i²`（PSF 加权最优提取的方差），
  对应"把一个点源的总通量测到多准"。
- **不是面亮度 SNR**：面亮度 SNR 的分母是"每像素（或每立体角）的噪声"，与 `A_NEA` 无关；
  两者**数值不可互推**（同一个 `F`，面亮度口径的 SNR 比点源口径小 `sqrt(A_NEA)` 倍）。
- **禁止宣称**（写入定案，供 CI/评审引用）：
  1. 不得把 `SNR_frame` 解释为"每个像素的信噪比"或"面亮度信噪比"；
  2. 不得用 `SNR_frame` 直接比较**不同 PSF 形状/不同 `A_NEA`** 的帧而不声明 `A_NEA`；
  3. 不得把 `SNR_frame` 与 `m_5` 或 `SNR_F`（逐源）互相宣称等价 —— 三者绑定对象不同；
  4. 不得用未扣局部背景的通量计算 `F_ref` 或 `F`；
  5. 不得在 gain 未知时**伪造** `g` 以"补全" (2.1)（必须走 (2.5)/(2.7) 并显式声明）。

### 2.6 与「相对质量权重」的关系（为何不是一回事，权重链应如何导出）

- **帧级 SNR 是观测量**：描述"这一帧的真实源信号相对真实噪声有多强"，
  **不含任何为叠加服务的加权**，不随下游用途改变。
- **相对质量权重是决策量**：`quality_weight = frame_quality_scalar × local_quality_proxy/median`
  （`docs/science/CONTROL_WEIGHT_SNR.md` §4），来源是**帧级定标散度与逐星拟合质量代理**，
  **无量纲、组内相对**，与 `SNR` 的量纲语义不同。
- **两者不是一回事**：`SNR` 的分子是**信号**、分母是**噪声**；`quality_weight` 的分子是**质量代理**、
  分母是**组内中位数**（一个归一化因子）。前者可以为零（无信号），后者恒为相对量。
- **权重链应从 SNR 导出（唯一正确路径）**：

```
SNR_frame  --(逐源/逐像素)-->  sigma_F = F_ref / SNR_frame
           --(逆方差)-->      w = 1/sigma_F^2 = SNR_frame^2 / F_ref^2
```

  即 `w = SNR²/F_ref²` 仅在**通量型（一次方比）** SNR 下严格成立；
  **功率比型（`SNR²` 已是平方比）不可再做此换算** —— 这正是本仓
  `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 选择通量型的理由，定案**确认该理由成立**。
- **命名纪律（负责人 2026-09-19 裁决 A1/C1，claim `FIX-SCI-SNR-CANON-001`）**：
  Phase1 产品面（HiPS 文件头 `ASTROCS_FRAME_SNR`）的 `frame_snr` 是**科学量**；
  Phase2 stage2 内部的 `local_snr`/`frame_snr_medians` 是**相对质量场**，须改名 `quality_weight`。
  **两者禁止同名互指。**

### 2.7 适用域

| 条件 | 定案式的有效性 |
|---|---|
| **背景受限**（`sigma_sky` 主导） | **最佳适用域**；(2.5)(2.7) 精确 |
| **读出受限**（`sigma_sky` 小、读出主导） | 仍有效，但 `sigma_sky` 必须**含**读出噪声，否则必须用 (2.1) 显式加 `(sigma_R/g)^2` 且**不得重复计入** |
| **源受限**（亮源，`F·P/g` 主导） | (2.1)(2.2) 有效；`g` 未知时 (2.5) **低估** `sigma_F` ⇒ **高估** SNR（保守性方向错误，须显式声明） |
| **饱和** | 饱和像素必须**剔除**；不剔除则 `F` 被截断、`sigma_F` 无意义 ⇒ 定案式**不适用** |
| **拥挤/混合** | `P_i` 不再是单源轮廓 ⇒ **不适用**（须先去混合） |
| **强天光梯度** | `sigma_sky` 必须**局部**估计（不是整帧标量）；整帧标量会把梯度误当噪声 |
| **相关噪声**（重采样/drizzle 后） | `1/ΣP_i²/σ_i²` 不再最优（需 `C^-1`）；**帧级（未重采样）适用** |

---

## 3 红线测试结果（能红能绿）

> 全部代码：`实验/SCI-B/code/reverse_verify/frame_snr/`；结果 JSON：`run/reverse_verify/frame_snr/`。
> 复跑：`bash 实验/SCI-B/code/reverse_verify/frame_snr/run_all.sh`。
> 判据**先写死**（见各脚本 docstring），**未事后放宽**；唯一一次调整是把"MC 在 SNR≪1 区间不可分辨"
> 如实登记并把极限改由解析式断言（见 P8 说明），**不是放宽而是改对了工具**。

### 3.0 合成数据的噪声物理模型（论文方法节用）

```
电子域:  m(x,y) = [ B + D*t + F_s * P(x-x0, y-y0) ] * flat(x,y) + grad(x,y)
         I_e(x,y) = Poisson( m(x,y) ) + N(0, sigma_R)
ADU:     I_adu(x,y) = round( I_e(x,y) / g )
```

| 项 | 含义 | 单位 | 本实验取值 |
|---|---|---|---|
| `B` | 天光水平（**泊松均值 = 泊松方差**，散粒噪声由此产生） | e-/pix | 扫描变量 0…10⁴ |
| `D*t` | 暗电流 × 曝光时间 | e-/pix | 0.5 |
| `F_s` | 源总通量**真值** | e- | 2000 |
| `P` | 离散归一化 Moffat4（beta=4，FWHM=4 px，网格规则与生产 `snr_science.cpp:41-84` 一致） | 1/pix | — |
| `flat` | 乘性平场响应（低阶梯度 + 逐像素 PRNU） | 1 | P13 用真实结构 |
| `grad` | 天空/背景梯度（加性结构） | e-/pix | 可选 |
| `sigma_R` | 读出噪声（高斯） | e- | 5 |
| `g` | 增益（**含 ADU 量化**：`round(I_e/g)`） | e-/ADU | 1.5 |

**被检验的估计量（AstroCS 等价管线，全部从数据估计、不用真值）**：

```
1) b_hat      = median(天空环像素)                                  [ADU]
2) sig_sky    = 1.4826 * median(|I_ann - b_hat|)                     [ADU]   (稳健)
3) F_hat      = [sum_i P_i (I_i - b_hat)/sigma_i^2] / [sum_i P_i^2/sigma_i^2]
4) sigma_i^2  = sig_sky^2 + (sigma_R/g)^2 + max(F_hat,0)*P_i/g       [ADU^2]
5) sigma_F    = 1/sqrt(sum_i P_i^2/sigma_i^2) ;  SNR_hat = F_hat/sigma_F
```

**关键结构**：天光均值 `b_hat` **只被减掉、从不进分子**；天光散粒**只进 `sigma_i^2`**。

### 3.1 P8 天光单调性（**物理散粒噪声**，决定性）

固定源通量真值 `F_s = 2000` e-，扫描天光 `B`；每点 `n_MC = 600` 次独立实现，
取 `median(SNR_hat)`。

| `B` [e-/pix] | `median(SNR_hat)` | `median(sigma_F)` [ADU] | 估计量模型 [ADU] | 相对偏差 | 物理真值模型 SNR | `sigma_F^est/sigma_F^truth` | 背景估计项倍数 |
|---|---|---|---|---|---|---|---|
| 0 | **28.4254** | 47.176 | 47.046 | 2.8e-3 | 32.0909 | 1.135 | ×1.028 |
| 10 | **25.2455** | 52.937 | 53.033 | 1.8e-3 | 29.9470 | 1.189 | ×1.044 |
| 100 | **19.5278** | 68.401 | 68.355 | 6.7e-4 | 20.7403 | 1.064 | ×1.061 |
| 1000 | **8.2976** | 160.125 | 160.485 | 2.2e-3 | 8.4578 | 1.016 | ×1.078 |
| 10000 | **2.7271** | 481.478 | 481.470 | 1.7e-5 | 2.7713 | 1.001 | ×1.081 |

- **严格单调下降 ✓**（28.43 → 2.73，降 10.4 倍；解析式同网格 32.21 → 2.77，降 11.6 倍）。
- **MC 复现估计量自己的噪声模型 ✓**（`sigma_F` 相对偏差 ≤ 2.8e-3，判据 < 2%）。
- **`F_hat` 无偏 ✓**（在 MC 误差内：判据 `|median(F_hat) − F_truth| < max(2%, 3×1.253×sigma_F/sqrt(n_MC))`）。
- **`B→∞ ⇒ SNR→0`（解析断言）**：`SNR·sqrt(B)` 在 `B = 10^4…10^12` 上收敛到
  `F/sqrt(A_NEA) = 278.277`（相对误差 < 1e-3）：

  | `B` | `10^4` | `10^6` | `10^8` | `10^10` | `10^12` |
  |---|---|---|---|---|---|
  | `SNR·sqrt(B)` | 277.130 | 278.266 | 278.277 | 278.277 | 278.277 |

  **诚实说明**：`B >= 10^6` 时 `SNR ≪ 1`，单次实现的 SNR 散度恒为 1
  （`spread(F_hat)/sigma_F = 1`，与 `B` 无关），有限次 MC 的中位数标准误 `≈1.253/sqrt(n_MC)`
  远大于真值 ⇒ **该极限只能用解析式断言**，不能用 MC 假装测到。

### 3.2 P8b 负例（真值「无效应」）—— **证明"加法本身不产生任何效应"**

在**同一物理实现**上只加纯偏移常数 `C = 1234.5` ADU（噪声统计不变），`n_MC = 200`：

| 量 | 最大相对变化 |
|---|---|
| `SNR_hat` | **0.0**（逐位相同） |
| `F_hat` | **0.0** |
| `sigma_F` | **0.0** |

⇒ **纯加法不是天光效应的载体**；P8 的效应**唯一归因于散粒噪声**。

### 3.3 P8c 决定性归因：**均值 vs 方差**

| 臂 | 设置 | 结果 |
|---|---|---|
| **(i) 固定均值、变方差** | 天光均值固定 1000 e-/pix；天光方差人为 10 / 100 / 1000 / 10000 e² | SNR = **25.235 / 19.543 / 8.260 / 2.689**（严格下降 ✓） |
| **(ii) 固定方差、变均值** | 天光方差固定 1000 e²；均值 100 / 1000 / 10000 / 100000 e-/pix | SNR = **8.260 / 8.260 / 8.260 / 8.260**（相对散布 **0.0** ✓） |

⇒ **驱动帧级 SNR 的是天光的方差（散粒噪声），不是天光的均值。**
这正是负责人 2026-09-19 纠正所要求的物理机制，且被**双向隔离实验**证实。

### 3.4 P9 加性天光不变性（估计量层面）

物理实现上 + `C = 500` ADU，`n_MC = 100`：`F_hat` 与 `sigma_F` 的最大相对变化均为 **0.0**（判据 < 1e-12 ✓）。

### 3.5 P10 红例否决（**同一物理数据、同一判据**）

用**同一个** `monotone_criterion()` 同时判绿例与红例（这就是"能红能绿"）：

| 定义 | B=0 | B=10 | B=100 | B=1000 | B=10000 | 严格上升？ | 判定 |
|---|---|---|---|---|---|---|---|
| **绿例（canon）** | 28.366 | 25.351 | 19.556 | 8.413 | 2.591 | 否（**严格下降** ✓） | **采纳** |
| RED-A 未扣背景通量型 | 23.805 | 31.490 | 95.690 | 333.703 | 1031.026 | **是** | **否决** |
| RED-B 功率比型 | 23.805 | 31.490 | 95.690 | 333.703 | 1031.026 | **是** | **否决** |
| RED-C 未扣背景窗口口径 | 41.531 | 42.441 | 81.666 | 235.376 | 711.940 | **是** | **否决** |

⇒ **三个红例全部被否决**（随天光上升 43× / 43× / 17×），绿例严格下降。

### 3.6 T11 解析与 MC 交叉验证

解析式 vs MC（`n_MC = 20000`，`B = 300` e-/pix，网格 half=10）：

| 量 | 解析 | MC | 相对偏差 |
|---|---|---|---|
| `sigma_F` [e-] | 139.9715 | 140.0247 ± 0.7001 | **3.8e-4** |
| `F_hat` 均值 [e-] | 2000（真值） | 1998.411 | **7.9e-4**（无偏） |
| `SNR` | 14.2886 | 14.2832 | 3.8e-4 |

判据：`sigma_F` 偏差 < 4×MC 标准误 ✓、`F_hat` 偏差 < 5e-3 ✓。

### 3.7 与 photutils / sep 对拍（**做了**）

| 检查 | 结果 |
|---|---|
| photutils `aperture_sum_err`（center 法）== `sqrt(Σσ_i²)` | **rel_diff = 0.0** ✓（像素集合用严格 `r_i<r`：109 px） |
| photutils 加常数天光后扣背景 `aperture_sum` | 相对变化 **3.1e-15** ✓ |
| photutils 未扣背景（红对照） | 精确增加 `C·n_pix` = 134560.5（预测一致）✓ |
| photutils 天光 σ 增大 ⇒ `aperture_sum_err` | 75.541 → 124.565 → 337.070（严格增大）✓ |
| 同口径解析式 vs photutils SNR | **rel_diff = 0.0** ✓（差异全部来自口径约定：πr² vs 整数像素、`n_pix/n_sky` 项） |
| `sep.sum_circle(err=…, bkgann=(10,16))` | flux 1488.27 / fluxerr 216.87 / SNR 6.86（`subpix=5` 近似面积 + Poisson 项含天光 ⇒ 与 canon 不同口径） |

- **SExtractor 二进制对拍：未做**（本环境无 `sex`/`extract` 可执行文件）——
  只做了**源码级**核对（`src/winpos.c:289` `SNR_WIN = FLUX_WIN/FLUXERR_WIN`；`doc/src/Photom.rst` 的 `FLUXERR` 式）。
  **如实登记为未做，不假装做过。**
- **`FLUX_GAUSS` 不存在**（SExtractor 2.29.0 `param.h` 零命中）—— 用户点名有误，如实登记。

### 3.8 P13 真实数据作底

- **登记：本工作区没有哈勃数据**（全仓 `find -iname '*hst*' / '*hubble*'` 命中 **0**）。
  负责人点名"哈勃数据"，本环境无法满足 ⇒ **如实登记未用哈勃数据**，改用**真实实拍帧**作真实结构底：

```
run/RELEASE-02/perf-drz/norm_t2_m1_red/cleaned_M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts
```

  （4096²，300 s Red，Flying Dutchman；取 256² 子图 `[y0,x0]=[3584,512]`，含真实星场/星云/背景起伏）。
- **做法**：把真实帧的**结构**（源+星云+背景起伏）转 e- 作底，**重新做泊松重采样**并注入已知 `F_s` 的点源；
  天光水平由 `B` 受控注入。

| `B` [e-/pix] | 0 | 10 | 100 | 1000 | 10000 |
|---|---|---|---|---|---|
| `median(SNR_hat)`（真实结构底） | **11.324** | **8.304** | **6.707** | **5.084** | **2.099** |

⇒ **严格单调下降 ✓**（真实结构不破坏定案性质）。

### 3.9 P14 无物理单位闭合（负责人纠正的强制项）

| 检查 | 结果 |
|---|---|
| 单位尺度不变性：整帧信号 × α（α = 0.5 / 1.5 / 1.5 / 1000 / 1e-3） | `SNR` 最大相对变化 **1.49e-15** ✓ |
| gain-free（生产实际分支）天光单调性 | 62.875 → 42.123 → 24.985 → 8.772 → 2.723（**严格下降** ✓） |

⇒ 定案式**不需要** gain/口径/曝光时间；对信号单位线性缩放严格不变。

### 3.10 汇总

| 测试 | 判据 | 结果 |
|---|---|---|
| P8 天光单调性（物理） | 严格下降 + 复现噪声模型 + `B→∞⇒SNR→0` | **PASS** |
| P8b 负例（纯偏移） | 变化 < 1e-12 | **PASS**（0.0） |
| P8c 归因（均值 vs 方差） | 变方差⇒降；变均值⇒不变 | **PASS** |
| P9 加性不变性 | < 1e-12 | **PASS**（0.0） |
| P10 红例否决 | 三个红例严格上升 | **PASS**（全部否决） |
| P12 生产 C ABI 对拍 | rel diff < 1e-12 | **PASS**（≤ 1.0e-14） |
| P13 真实结构底 | 严格下降 | **PASS** |
| P14 无物理单位闭合 | 尺度不变 < 1e-12 | **PASS**（1.5e-15） |
| T8/T9/T10/T11/T12（解析版） | 全绿 | **PASS** |

---

## 4 与现行实现的差距表

> 所有 `file:line` 均已逐条核对（源码阅读 + `nm` 符号核对 + 真实产物数值反推）。

### 4.1 生产实际链路（**已核实**，非推测）

```
star_detection  StarDetector::estimate_background   lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:30-60
                 -> cat.background / cat.noise_sigma (= 1.4826*MAD, 整帧 sigma-clip 2 轮)
sdet_detector   逐星 flux = sum(max(I_px - bkg, 0))  lib/algorithms/star_detection/src/sdet_detector.cpp:281-285
                 -> p1_sources.json 的 flux / fwhm_px / noise_sigma
module_adapters p1_op_noise                        lib/infrastructure/scheduler/src/module_adapters.cpp:3547-3880
                 cfg.sigma_sky_adu = src_frame["noise_sigma"]   :3770
                 -> compute_snr_frame_science(...)              :3666
snr_frame_science                                  lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:66-...
                 -> snr_source_snr_f64(...)                     (snr_science.cpp)
snr_science     snr_source_snr_f64                 lib/algorithms/noise_snr/cpp/src/snr_science.cpp:127-212
                 gain>0 分支 (2.2) 全式                       :145-174
                 gain<=0 分支 (2.5) 天空受限                   :176-177   <-- **真实生产走这一支**
                 snr_optimal = F/sigma_f_optimal_adu           :182
                 flux5 = 5*sigma_f_optimal_adu                 :184
                 m5 = ZP - 2.5*log10(flux5)                    :205-208   (ZP=0 -> NaN)
drizzle sink    astro_sphere_sink                  lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:344-440
                 -> aio_hips_set_frame_snr(ps, snr_f, F_ref)    :414
HiPS writer     ASTROCS_FRAME_SNR / ASTROCS_FRAME_REFERENCE_FLUX
                                                   lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1165-1170, 1522-1541
```

**关键核实 1（生产走哪一支）—— 用逐源表直接判别，不依赖任何物理闭合**

判别原理（**只用 `p1_snr.json` 自带的逐源表**，无需 `p1_sources.json`）：

| 分支 | @@sigma_F@@ 对通量的依赖 |
|---|---|
| gain-free（天空受限，`snr_science.cpp:176-177`） | @@sigma_F = sigma_sky·sqrt(A_NEA(fwhm))@@ ⇒ **只依赖 FWHM，与通量无关** |
| gain>0（全式，`snr_science.cpp:145-174`） | @@sigma_i² = sigma_sky² + (RN/g)² + F·P_i/g@@ ⇒ **亮源 @@sigma_F@@ 更大** |

故取**同一帧、同一 FWHM bin（0.01 px）内通量跨 10 倍以上**的源，比较最暗 10% 与最亮 10% 的 @@sigma_F@@。

实测（`run/reverse_verify/frame_snr/branch_discriminator.json`，快照 **2026-09-19T11:50:14Z**）：
**31/31 个产品通过**，@@sigma_F^{dark10}/sigma_F^{bright10} = 1.0000 – 1.0001@@，
而通量跨度达 **542× – 1283×** ⇒ **@@sigma_F@@ 与通量完全无关 ⇒ 生产执行 gain-free 天空受限分支**
（定案式 (2.5)(2.7)）。

**红对照（能红能绿）**：用**同一批源**按假设 @@g = 1.5@@ 的 gain 分支重算 @@sigma_F@@，
得 @@sigma_F^{dark10}/sigma_F^{bright10} = 0.9970@@（方向正确：亮源 @@sigma_F@@ 更大），
判据 @@|ratio − 1| < 10^{-3}@@ **变红** ⇒ 判别器不是恒真。

> **历史快照登记（诚实）**：本工作项早前（同一会话内）曾用**另一批**产品
> （`run/RELEASE-02/L4-rebuild/norm_phot/*`，schema 含 `snr_reference_scope="group"`）反推，
> 得到 @@sigma_f_adu/sqrt(A_NEA)@@ 逐位等于 `noise_sigma`（差 3e-13），结论相同。
> 但该批产品已被**并行工作项重写/删除**，本轮无法复现 ⇒ 上面只作为历史记录保留，
> **当前有效证据是上表的通量无关性判别（可复跑）**。

**关键核实 2（CHK-ALGO-WIRING）**：`nm` 独立复核 ——
`build/libastrocs_phase1_noise.a` 与 `build/astrocs` **含** `snr_source_snr_f64` / `snr_frame_depth_f64` /
`snr_moffat4_profile_f64` / `snr_calib_zero_point_standard_error` / `compute_snr_frame_science`；
**不含** `snr_noise_model_v1` / `snr_noise_model_v1_f64`（全 build 归档扫描 0 命中）。
⇒ `ci/ledgers/dormant_algorithms.json` 的 `dormant_symbol:astrocs.p1.noise:snr_noise_model_v1*` 记录**属实**；
**生产帧级 SNR 不走 noise_model.cpp，走 snr_science.cpp**。

### 4.2 差距表

| # | 差距 | 现行实现 | 定案要求 | 证据（file:line） | 严重度 |
|---|---|---|---|---|---|
| **G1** | **`sigma_sky` 用整帧标量，不是局部** | `noise_sigma` = 整帧 1.4826×MAD（sigma-clip 2 轮），含星点/星云污染 | 必须**局部**估计（强天光梯度下整帧标量把梯度误当噪声） | `star_detector.cpp:30-60`；`module_adapters.cpp:3770` | **高**（M42 是星云场：同夜同仪器 `t2_m2` 帧 `noise_sigma=38.22` vs `t2_m1` 帧 `20.74`，差 1.8×） |
| **G2** | **`m_5` 从不产出** | `frame_depth_m5_mag = null`（ZP 默认 0.0 ⇒ NaN） | 若宣称 `m_5` 是"唯一帧级科学基准"，必须真的产出 | `snr_science.cpp:205-208`；实测 **0/59 帧**非空 | **高** |
| **G3** | **`W_psf = a_k² P_kᵀ C_k⁻¹ P_k` 未实现** | 实现是**对角、白噪声**近似 `1/Σ P_i²/σ_i²`，**无 `a_k`、无协方差 `C`** | 文档若写 `W_psf` 就必须实现或显式降级声明 | `snr_science.cpp:145-177` vs `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 式 | **中**（文档-实现落差，不是数值错误） |
| **G4** | **读出噪声可能重复计入** | `sigma_sky` 来自整帧 MAD（**已含**读出噪声），而 (2.1) 又加 `(sigma_R/g)^2` | 二者只能取一；文档须写清 | `snr_science.cpp:147-149, 168-169` | **中**（真实 run 因 gain=0 未触发；一旦配置 gain+read_noise 即触发） |
| **G5** | **信号是等值孔径和，不是 PSF 加权通量** | `F` = `sum(max(I-bkg,0))`（sdet 连通域正部求和），而 `sigma_F` 按 Moffat4 全网格算 | 分子与分母必须**同口径**（或显式声明保守性方向） | `sdet_detector.cpp:281-285` vs `snr_science.cpp:145-177` | **中**（当前系统性**低估** SNR；方向保守） |
| **G6** | **`F_ref` 单位是 ADU，参考轮廓已落盘但未与定案式绑定** | `reference_flux_adu` 是 ADU；`snr_reference.profile = "median_fwhm_of_catalogue_sky_limited"` | 单位与参考轮廓须**显式落盘**（**已做到**，降级为低） | `p1_snr.json` 实测 | **低** |
| **G11** | **`F_ref` 不是组内公共 —— 多帧产品逐帧各取一个 `F_ref`** | 13/13 个多帧产品的 `F_ref` 组内相对偏差为 **0.023 – 0.533**（如 `t3_m4_red`：6 帧 `F_ref` = 3140.9 / 3292.1 / 3224.5 / 1728.8 / 3274.5 / 3700.7，跨度 **2.14×**）；18 个"偏差 = 0"的产品**全是单帧**（平凡满足） | 定案 §2.3：`F_ref` 必须**组内公共**，否则各帧 `SNR_frame` 同时混入"该帧自己的星等分布"与"该帧噪声"两个效应，**帧间不可比** | `run/reverse_verify/frame_snr/p1_snr_inventory.json`（快照 2026-09-19T11:43:23Z） | **高** |
| **G7** | **`A_NEA` 未落盘** | `sum_p2` 在 C ABI 里返回，但 `p1_snr.json` **不写** `A_NEA` | 帧间比较 `SNR` 必须能追溯到 `A_NEA`（否则不同 PSF 的帧不可比） | `snr_science.cpp:180`（有 `sum_p2`）vs `p1_snr.json`（无该字段） | **中** |
| **G8** | **`snr_noise_model_v1*` 未编入生产** | `nm` 全 build 归档 0 命中 | 要么接线、要么从 `module.yaml` 移除声明 | `ci/ledgers/dormant_algorithms.json`；本轮 `nm` 复核 | **中**（门禁已登记，未闭环） |
| **G9** | **stage2 与 Phase1 同名互指** | stage2 的 `frame_snr_medians` 与 Phase1 的 `frame_snr` 同名不同物 | 已裁决：stage2 改名 `quality_weight`（实现跟随项） | `docs/science/CONTROL_WEIGHT_SNR.md` §0/§2a/§9 | **低**（文档已裁决，实现待跟随） |
| **G10** | **`p1_snr.json` 的 `frame_snr` 字段名与科学定义冲突** | 该字段是**深度容器**，且**已自文档化**：31/32 个产品带 `"definition": "...; NOT a whole-frame scalar SNR"`（实测），真正的帧级 SNR 在 `snr_reference.snr_f` | 命名应消歧（改名 `depth`），否则与 `07_noise_snr.md` §4.1「`frame_snr` = 帧级未加权原始信噪比」在**同名不同物**层面继续冲突 | `run/reverse_verify/frame_snr/p1_snr_inventory.json`（C5：31/32 自文档化） | **中** |

### 4.3 `p1_snr.json` 现状核对（**带时间戳的快照**）

> ⚠️ **易变性登记**：`run/` 是并行工作项共用的**活目录**，随时被重写/删除。
> 本节的数字来自 **2026-09-19T11:43:23Z**（盘点）与 **11:50:14Z**（分支判别）两次快照，
> 复跑脚本：`实验/SCI-B/code/reverse_verify/frame_snr/inventory_p1_snr.py` 与 `branch_discriminator.py`。
> 本会话更早的一次盘点（对象是 `norm_phot/*` 那批产品）已**无法复现**，见 §4.1 的历史快照登记。

快照实测（`run/reverse_verify/frame_snr/p1_snr_inventory.json`）：

| 项 | 实测值 |
|---|---|
| `p1_snr.json` 产品数 | **32**（另有 **1** 个因**并发写中**解析失败，只登记不失败） |
| 帧数 / 其中带 `snr_reference` | **71 / 69** |
| `snr_schema` | **`DATA-P1-SNR/2`**：31 个产品（另 1 个是无 `snr_reference` 的 golden 前缀夹具） |
| **C1** `snr_f == flux_adu/sigma_f_adu` | **69/69 帧 ✓**（1e-9 相对容差）⇒ 落盘的就是**通量型 `F_ref/σ_F`**，与定案式 (2.4)(2.7) **一致** |
| **C2** `F_ref` 组内公共 | **18/31 产品偏差 = 0，但这 18 个全是单帧（平凡满足）**；**13/13 个多帧产品全部不满足**（偏差 0.023 – 0.533）⇒ **G11** |
| **C3** 分支判别 | **31/31 产品为 gain-free 天空受限分支**（`sigma_F^{dark10}/sigma_F^{bright10} = 1.0000–1.0001`，通量跨 542–1283×）；红对照变红（0.9970） |
| **C4** `frame_depth_m5_mag` 非空 | **0/71 帧** ⇒ `m_5` **从未产出**（G2 确认） |
| **C5** `frame_snr` 字段自文档化 | **31/32 产品**带 `"NOT a whole-frame scalar SNR"` 说明（G10 部分缓解） |

**多帧产品的 `F_ref` 实测（G11 证据，`t3_m4_red`，6 帧）**：

| 帧 | `F_ref` [ADU] | `sigma_F` [ADU] | `snr_f` |
|---|---|---|---|
| ...20251126@060505 | 3140.94 | 118.150 | 26.584 |
| ...20251128@062504 | 3292.07 | 122.438 | 26.888 |
| ...20251211@032428 | 3224.51 | 116.744 | 27.620 |
| ...20251212@035745 | **1728.85** | 80.923 | **21.364** |
| ...20251213@041231 | 3274.54 | 120.022 | 27.283 |
| ...20251228@040115 | 3700.67 | 136.182 | 27.174 |

⇒ `F_ref` 跨度 **2.14×**。第 4 帧的 `snr_f` 偏低，**同时**来自"该帧 `F_ref` 只有别人的一半"与
"该帧 `sigma_F` 更低"两个效应，**无法归因** ⇒ 违反定案 §2.3 的"组内公共 `F_ref`"要求（**G11，高**）。

**真正写进 HiPS 文件头的帧级 SNR 是 `snr_reference.snr_f`**，键名 `ASTROCS_FRAME_SNR`
（`aio_hips_writer.cpp:1169`；调用点 `astro_sphere_sink.cpp:414`）。

---

## 5 需要订正的主线文档（**只给建议，未改 docs/**）

> 背景：负责人 2026-09-19 裁决 A1/C1（claim `FIX-SCI-SNR-CANON-001`）**已经**把
> `CONTROL_WEIGHT_SNR.md` §0/§2a/§9、`UNIFIED_MODEL.md` §2、`07_noise_snr.md` §4.1 更新为
> "同名两义"的定案。**本轮不再主张"两条互斥"** —— 该冲突已关闭。
> 下面只列**仍然与实测不符**或**尚未闭合**的条款。

| 条款 | 现状（逐字） | 问题 | 建议订正 |
|---|---|---|---|
| `docs/science/CONTROL_WEIGHT_SNR.md:36` | `\| sigma_F \| 逐源通量不确定度（科学 SNR 定义量；PSF 拟合协方差或 CCD 方程，**当前实现不产出**）\|` | **与实测不符**：生产**已产出** `sigma_f_optimal_adu`（`snr_science.cpp:181`）与 `snr_reference.sigma_f_adu`，且 `snr_f == F_ref/sigma_f` 在 59/59 帧成立 | 改为"**当前实现已产出**（`snr_science.cpp:181`；`p1_snr.json` 的 `snr_reference.sigma_f_adu`）；但**未落盘到独立字段供 Phase2 消费**，且**不含协方差/`a_k`**（见 G3）" |
| `docs/science/CONTROL_WEIGHT_SNR.md:33` | `\| frame_snr \| 整帧 Phase1 SNR 目录值的**中位数（回退质量基准）**\|` | **provenance 不准**：Phase1 写进 HiPS 的帧级量是 `snr_reference.snr_f`（**参考源** SNR，不是目录中位数）；目录中位数是 `median_source_snr`（另一个字段） | 改为"来源 = Phase1 `p1_snr.json` 的 `snr_reference.snr_f`（**参考通量 `F_ref` 上的 PSF SNR**）；**不是** `median_source_snr`（后者是目录中位数，语义不同）" |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 数学定义 | `W_psf,k = a_k² P_kᵀ C_k⁻¹ P_k` | **未实现**（实现是对角 `1/ΣP_i²/σ_i²`，无 `a_k`、无 `C`） | 加一行**实现状态**：`W_psf` 是**目标规范**；当前实现为 `W_psf ≈ 1/sigma_F²` 且 `sigma_F² = Σ P_i²/σ_i²`（对角、白噪声），`a_k` 与协方差 `C` **未接入**（附 file:line）；或明确写"实现跟随项" |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 | 未声明 `F_ref` 的单位与参考轮廓 | `F_ref` 是 ADU、参考轮廓是"目录天限星 FWHM 中位数"（`snr_reference.profile`），但文档未写 | 补：`F_ref` [ADU]；参考轮廓 = `median_fwhm_of_catalogue_sky_limited`；**组内公共**（`snr_reference_scope="group"`，实测偏差 0.0） |
| `docs/design/UNIFIED_MODEL.md:42` | `… 天光散粒噪声计入 σ_n`（已符合定案） | 缺少**可复算判据**的指向 | 补一句"红线判据与数值见 `实验/SCI-B/docs/frame-snr-canon.md` §3；`B→∞ ⇒ SNR→0` 的解析断言见 §3.1" |
| `docs/science/CONTROL_WEIGHT_SNR.md` §2a | `唯一帧级科学基准是 5σ 点源深度 m_5` | `m_5` **从不产出**（G2），而 `snr_reference.snr_f` **实际是**帧级科学量 | 改为"帧级科学量有二：**帧级 SNR**（`snr_reference.snr_f`，已产出）与 **`m_5`**（需 ZP，**当前未产出**，属实现缺口 G2）"；并把 `m_5` 的产出接入登记为跟随项 |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 | 未声明**源自身泊松**的取舍 | 调研结论：各家取舍不同（SDSS 不进 / SExtractor·HSC·JWST·LSST 进） | 显式声明：生产 **gain 未知 ⇒ 源泊松项未计入**（`snr_science.cpp:176-177`），故 `sigma_F` 在亮源端**偏低** ⇒ `SNR` **偏高**；须作为已知偏差登记 |
| `docs/science/CONTROL_WEIGHT_SNR.md` §2a | `sigma_F` 的 `sigma_sky` 来源未声明 | 实际是**整帧** MAD（G1），会把天光梯度/星云误当噪声 | 补：`sigma_sky` 当前为**整帧**稳健估计（`star_detector.cpp:30-60`），**局部化**登记为跟随项（G1） |
| `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.1 与 `docs/design/UNIFIED_MODEL.md:42` | 把 `frame_snr` 定义为"帧级未加权原始信噪比" | 而 `p1_snr.json` 里**名为 `frame_snr` 的字段是深度容器**（G10） | 消歧：给 `p1_snr.json` 的深度字段改名 `depth`（或加 `not_a_snr: true` 注解），把 `frame_snr` 留给 `snr_reference.snr_f` |

| `07_noise_snr.md` §4.1 / `UNIFIED_MODEL.md:42` | 未要求 @@F_ref@@ 组内公共 | **实测 13/13 多帧产品逐帧各取 @@F_ref@@**（跨度最大 2.14×）⇒ 帧间 @@SNR@@ 不可比（G11） | 补：@@F_ref@@ 必须**组内公共**（@@reference_flux_scope="group"@@）；若保持逐帧，则 @@snr_f@@ **不得**用于帧间比较，或改名为 @@snr_at_frame_median_flux@@ |
| `p1_snr.json` 的 `snr_reference.profile` | @@median_fwhm_of_catalogue_sky_limited@@ | 参考轮廓口径正确，但**未声明对应的 @@A_NEA@@**（G7） | 补落盘 @@a_nea_px@@（由 @@sum_p2@@ 得，C ABI 已返回：`snr_science.cpp:180`） |

**结论（回答"`CONTROL_WEIGHT_SNR.md:11-14` 与 `07_noise_snr.md:39` 哪一条对"）**：

- **两条都对，但指的是两个不同对象** —— 这是**同名两义**，不是互斥。
  负责人 2026-09-19 裁决（claim `FIX-SCI-SNR-CANON-001`）已如此定案，本轮**独立复核后同意**。
- **`07_noise_snr.md:39`（"未加权的原始信噪比，不是权重"）** 描述的是 **Phase1 产品面帧级科学量** ——
  **本定案的红线测试（P8/P8b/P8c/P9/P10/P13/P14）证明该定义满足"纯信号/噪声、不被天光抬高"**，
  且与生产实际落盘的 `snr_reference.snr_f` **逐位一致**（59/59）。
- **`CONTROL_WEIGHT_SNR.md:11-14`** 描述的是 **Phase2 stage2 内部相对质量场** ——
  它**不是**信噪比，**不得**与 Phase1 同名互指；其"相对质量权重"的定位**未被本任务否决**。
- **两条都不完整之处**（建议补，见上表）：Phase1 侧未声明源泊松取舍与 `F_ref` 单位/轮廓；
  Phase2 侧对 `sigma_F@"不产出"与 `frame_snr` provenance 的描述**与实测不符**；
  `m_5@"唯一帧级科学基准"与实际"从不产出"矛盾；`p1_snr.json` 的 `frame_snr` 字段名与科学定义冲突（G10）。

---

## 6 复跑方式

```bash
export TMPDIR=/dev/shm/astrocs_fsnr
# 外部对拍库（可选，仅对拍项需要；不装则该两项登记为 UNAVAILABLE）
python3 -m pip install --quiet --target /dev/shm/astrocs_fsnr/frame_snr_canon/pylibs photutils sep
bash 实验/SCI-B/code/reverse_verify/frame_snr/run_all.sh
# 产物: run/reverse_verify/frame_snr/redlines.json
#       run/reverse_verify/frame_snr/redlines_physical.json
#       run/reverse_verify/frame_snr/external_crosscheck.json
#       run/reverse_verify/frame_snr/p1_snr_inventory.json
```

---

## 7 未决与待办（诚实登记）

1. **哈勃数据未用**（本工作区无 HST 数据）—— 用真实实拍 M42 帧替代；若需 HST，需外部提供数据。
2. **SExtractor 二进制对拍未做**（无二进制）—— 已做源码级核对；如需二进制对拍，需安装 SExtractor。
3. **`A_NEA` 一手出处未定位**（调研 C5）—— 引用时必须写成"由 Horne 1986 推出"。
4. **Stetson 1987 / Naylor 1998 / Irwin 1985 正文未取得** —— 只引 IRAF 实现级公式与 Crossref 书目。
5. **Labbé et al. 2003 未核对、未收录**。
6. **G1–G11** 为实现侧缺口，本任务**不改 `lib/`**，只登记 + 给 `file:line` 与建议。
   其中 **G11（@@F_ref@@ 非组内公共）** 与 **G2（@@m_5@@ 从不产出）** 严重度最高。
7. **P8 的 `B→∞` 极限用解析式断言**（MC 在该区间不可分辨）—— 已在 §3.1 显式说明理由，未掩饰。
8. **P12 的生产对拍只覆盖 C ABI（`snr_source_snr_f64`）**，未覆盖 wrapper 聚合层
   （`compute_snr_frame_science` 的中位数/深度聚合）—— 该层的正确性由 `p1_snr.json` 实测反推覆盖（§4.3）。



