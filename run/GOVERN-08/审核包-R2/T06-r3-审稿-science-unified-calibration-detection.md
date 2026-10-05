# T06 第 3 轮独立审稿 · 车道 `docs/science/unified/** docs/science/calibration/** docs/science/detection/**`

**审稿人**：第三轮独立审稿员。**只审不订正**：全程零文件修改、零 git 写操作（git 仅 `status/log/show/diff/grep`，中文路径一律 `git -c core.quotepath=false`）。
**审稿依据**：`AGENTS.md`、`docs/ACSD_DESIGN.md`、`工作包-RECTIFY-09原件/standards/03_READING_AND_ADVERSARIAL_REVIEW.md`（十轮）、`…/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`（三类证据）。
**车道前三轮材料**：`T06-审稿-SCI-unified-calibration-detection.md`（第 1 轮）、`T06-第1轮订正-SCI-unified-calibration-detection.md`（第 1 轮订正）、`T07-重复断言收敛-science.md`（止血）。

> ⚠️ **登记事实**：`T06-r2-审稿-SCI-unified-calibration-detection.md`（本车道第 2 轮审稿件）**在本仓不存在**。`find run/GOVERN-08 -name "*r2-审稿*"` 只命中 4 份：DOC-DETAIL-GLOSSARY / DOC-ENG / noise_snr / SCI-psf-drizzle-resample。**本车道缺第 2 轮审稿记录**，因此「订正本身引入 32+ 新问题」在本车道无独立审稿件可对账，我只能拿第 1 轮审稿 + 第 1 轮订正 + T07 三份作基线。这本身是一条流程缺口。

---

## 1 读完了吗

| 目录 | 文档 | 行数 | 我逐行读完 |
|---|---|---|---|
| `unified/` | `DATA_SEMANTICS.md` | 300 | ✅ 全文 |
| | `SCIENCE_SCOPE.md` | 134 | ✅ 全文 |
| | `UNIFIED_SCIENCE_MODEL.md` | 224 | ✅ 全文 |
| | `README.md` | 3 | ✅ |
| `calibration/` | `CALIBRATION.md` | 292 | ✅ 全文 |
| | `CCD_DEFECT.md` | 237 | ✅ 全文 |
| | `README.md` | 3 | ✅ |
| `detection/` | `STAR_DETECTION.md` | 210 | ✅ 全文 |
| | `ASTROMETRY.md` | 229 | ✅ 全文 |
| | `README.md` | 3 | ✅ |

**本片 10 份 / 1 635 行，逐行读完 10 份 / 1 635 行，未读完 0 份。**
另逐行读完前车材料 `T07-重复断言收敛-science.md`（413 行）、`T06-第1轮订正-SCI-unified-calibration-detection.md` 相关节。
脚本只用于**定位与复跑核验**（§4 每条都给出可复跑命令/实算值），未替代阅读。

---

## 2 止血效果抽查

### 2.1 决定性事实：止血对本车道**实质改动 = 2 个空行**

```
$ git -c core.quotepath=false show --stat 18dd6c64 -- docs/science/unified docs/science/calibration docs/science/detection
 docs/science/calibration/CALIBRATION.md    | 1 -
 docs/science/unified/SCIENCE_SCOPE.md      | 1 -
```
**两处都是删除「参考文献与参考代码」标题后的一个多余空行**，与任何断言无关。
`T07` §1.1 逐条账 D1–D9 **无一涉及本车道任何一行**；§7.2 车道内待明项 5 条也无一涉及本车道。

⇒ **结论：不是「止血判本车道为对」，是「止血根本没看本车道」。**
本车道所有第 1 轮新写下的断言（σ_bg 两族、1.5012、Young–van Vliet 换算节、标度链、κ 论证、坏列复算表…）**没有一条经过第 2 轮或止血轮的复核**。

### 2.2 「是否只改一处、别处还留旧值」——在本车道**无实例可查**（因为没改），但车道内的重复断言**至今未收敛**

| 重复断言 | 副本位置 | 判定 |
|---|---|---|
| 5-token 标度类别词表 + 同句「由 engineering 正本的数值标准承载」 | `unified/SCIENCE_SCOPE.md:31` ↔ `unified/DATA_SEMANTICS.md:134`（正本在 `docs/engineering/standards/NUMERIC.md:33`） | **逐字副本**，第 1 轮条目 1-03 点名「保留一处引用 + 一处结果引用，删重复全抄」——**从未执行**，T07 也未收。T07 的机械普查（≥40 字逐字相同）因句首差「信号的」三字而漏掉 |
| 标度链 `raw_adu→calibrated_adu→photo_scaled_adu→surface_brightness` 及各档单位 | `SCIENCE_SCOPE.md:33-38`（4 行链）vs `DATA_SEMANTICS.md:99-107`（7 行推导表） | **部分重叠副本**；`SCIENCE_SCOPE.md:40` 只把「标度律」改指 DATA_SEMANTICS，**链本身仍两处各写一遍** |
| `bgnoise` 行差分估计器 | `STAR_DETECTION.md:32` / `:50` / `:158` | 三处复述 |
| YvV 平滑器描述 + `0.133174181553` + `37.5448` | `STAR_DETECTION.md:34`/`:52`/`:140`；`:59`/`:141`；`:65`/`:141` | 各两到三处 |

### 2.3 止血**是否又造了新矛盾**——**有，且我能给出一条机器可判的**

**🔴 S1（本轮最重，止血车道外但根源在本车道）**：`NOISE_SNR.md` 的 `w`/`W_k` 单位与 **canonical schema 的冻结 `const` 冲突**。

| 面 | 对 `point_information` / `W` 的单位声明 |
|---|---|
| `eng/contracts/schemas/unified/point_information.schema.json:81` | **`"bunit": {"const": "ADU^-2"}`**（机器可判的冻结值） |
| `docs/engineering/UNIFIED_OBJECTS.md:42` | `ADU^-2`（=1/Var(F_hat)，点源通量口径） |
| `unified/DATA_SEMANTICS.md:190` | `W_info` = `ADU⁻²` |
| `unified/UNIFIED_SCIENCE_MODEL.md:143`/`:150` | `point_information` = `ADU⁻²` |
| **`noise_snr/NOISE_SNR.md:325`/`:347`/`:460`** | **`F_syn` 通量的逆二次方 = `F_syn⁻²`** |

两族差 `k_photo²`（`NOISE_SNR.md:351` 自己写「二者只在 `k_photo,k ≡ 1` 时同值」）。实测 `photscal = 2.3846837130250378e-17`（`NUMERIC.md:115`），`k_photo² ≈ 5.7e-34`。
**成因**：T07 的 D3/D4 改了 `NOISE_SNR` 的 `W_k` 与 `w` 单位标签，但**只对照了 `ARTIFACTS.md:31`，从未对照 canonical schema 与 `UNIFIED_OBJECTS.md`**。
**后果**：`W_info` 是落盘对象、canonical schema 用 `const` 冻结，两侧不能同时为真。

**🔴 S2（跨切面，规模远大于 S1）**：指向本车道正本 `DATA_SEMANTICS.md` 的**死节锚 336 处**（`docs/`+`eng/`+`lib/` 全域，66 种死节号）：

```
指向 DATA_SEMANTICS 的节锚总数 = 417
  其中 §号在 DATA_SEMANTICS 中不存在 = 336 次（66 种节号）
  其中 §号存在（仍需逐条核内容）   =  81 次
DATA_SEMANTICS 现存小节: 1 2 3 3.1…3.9 4 4.1…4.5 5 5.1…5.3 6 7
死节号集合: 8 8.1 8.2 9 9.1 9.2 10 … 25.1 … 28.6 … 31.1a 31.1 31.2 31.3 31.5 31.5a 31.6 31.7 31.8 31.10
```
`docs/` 下 57 处死锚，`docs/engineering/UNIFIED_OBJECTS.md` 独占 **13 处**，其中 `:42` 正是钉 `point_information = ADU^-2` 的那一行，而它引作权威的「数据语义卷 §31.1a / §31.5 / §28.6」**三节全不存在**。
**这解释了 S1 为什么能存活**：唯一能裁决 `F_syn⁻²` vs `ADU⁻²` 的那几行，引的是死节。
**止血只清了 `GLOSSARY.md` 一份的 § 锚**，`UNIFIED_OBJECTS.md`、`INTEGRATION.md`、`PHASE2_SAMPLER.md` 与 11 份 `algorithms/*.md` 未清。
（`T07` 本身把「§ 锚」当成要收敛的对象，却在 `GLOSSARY.md` 之外的任何地方都没执行。）

### 2.4 「删副本处的信息落点」——**逐条给落点，本车道零丢失**

**派单点名的 `bkg0` 参数来源那条，答案：信息没有真空，已改对。**

| 落点 | 原文 |
|---|---|
| 唯一正本 | `algorithms/STAR_PSF_ALGORITHMS.md:114`：「init B=bkg0（拟合窗内**下半区**截尾 MAD clip 后中位，单星/批量两路同源）」 |
| 唯一正本（实现锚表） | `algorithms/STAR_PSF_ALGORITHMS.md:189`：「初值链 bkg0=下半区截尾 MAD clip 后中位 / A0=max_val−bkg0 / …」 |
| 副本① | `psf/PSF.md:62`：「`bkg0` 由拟合窗自身像元算出（像元取值分布下半幅经截尾后的中位数），**不是上游输入**，其定义与算法见 [3]」 |
| 副本② | `psf/PSF.md:252`：「本模块**不接收外部背景读数**：`B` 的初值与背景自洽基准 `bkg0` 都由拟合窗自身像元导出，见 [3]」 |
| 副本③ | `psf/PSF.md:267`：「严格低于窗内中位数的那一半构成下半幅，按 `2·kappa·MAD` 截尾后取中位数」 |

⇒ **原话「窗口外的背景估计由上游提供」确已删除，且正确信息在正本与三个副本里都在。「信息真空」不成立。**
（唯一残留：`algorithms/STAR_DETECTION_ALGORITHMS.md:122` 的检测侧 `bkg0` 措辞「**下半像素**截尾 MAD clip 后中位」是病句、且不给截尾系数；正本在 algorithms 车道，跨车道只报不改。）

**D7（29 处死指针样板行）**：`test -e docs/engineering/SCIENTIFIC_REFERENCES.md` → 不存在；`git log --diff-filter=D` → `0b278ba4`（T07 记载正确）。
**本车道从未持有该行**：`git show 0b278ba4^:…` 三份均无。两份参考小节完整：
- `CALIBRATION.md:292` 参考代码段完整保留 PixInsight PCL（许可 + 仓库 + commit `5a39021…` + `XISFReader.cpp::NormalizeSamples` + `PixelTraits.h::UInt16PixelTraits::MaxSampleValue()`）、astropy ccdproc（BSD-3-Clause）、LSST `ip_isr`（GPL-3.0）、及「传染性许可只读不复制入仓」。
- `CCD_DEFECT.md:237` 参考代码段完整保留 IRAF `ccdred`（NOAO/IRAF，非 OSI 开源）、Siril（GPL-3.0）、LSST `afw`（BSD-3-Clause）、astropy ccdproc（BSD-3-Clause）。
- 引用编号连续无缺口：`CALIBRATION` [1]–[8]、`CCD_DEFECT` [1]–[19]。

---

## 3 前两轮订正的正确性回查

| # | 条目 | 对/错/不完整 | **我自己重算的推导或一手来源** |
|---|---|---|---|
| R1 | **`bkg0` 参数来源（第 2 轮最重一条）** | ✅ **对** | 见 §2.4 落点表。`psf/PSF.md:62/:252` 与 `STAR_PSF_ALGORITHMS.md:114/:189` 四处口径一致，均写「窗内导出、不是上游输入」 |
| R2 | **`k_photo` 方向（D3）** | ✅ **对**（noise_snr 车道） | `F_sys ≡ k·F_adu` ⇒ 前向亮度因子必为 `1/k`。`NOISE_SNR:343` 的 `d_k=(1/k)·F_sys,k·P_k+n_k` 与 `:346/:347` 的 `Q_k=(1/k)PᵀC⁻¹d`、`W_k=PᵀC⁻¹P/k²` 自洽；`W_k` 单位 `F_syn⁻²`，与 `1/σ_F^{sys,2}` 匹配 |
| R3 | **`F0` 单位（D5）** `[F_syn]` | ✅ **对** | `F0 = 10^(−0.4(m_ref−ZP_syn))`，配 `ZP_syn`（Gaia XP 绝对谱合成、与帧无关）⇒ 必在合成系通量标度 |
| R4 | **`c_jp` 单位（D2）改 `sr⁻¹`** | ✅ **对** | `w_jp` 无量纲、`N_p` 单位 sr ⇒ `c_jp = w_jp/N_p` 是 `sr⁻¹`。我独立复核：`DRIZZLE.md:88` `w_jp = a_jp/A_drop,j`、`:104` `N_p = D_p/pixfrac²`、`:110` `c_jp = a_jp/(A_pixel,j·D_p)`（sr⁻¹）✓ |
| R5 | **`M_eff` 改尺度不变定义** | ✅ **对** | `M_eff = (Σc)²/Σc²` ⇒ `1+ρ(M_eff−1)` 与一般式 `1−ρ+ρ(Σc)²/Σc²` 恒等；旧 `1/Σc²` 对守恒 `c_jp` 带面积量纲（`sr²`），量纲不成立 |
| R6 | **`sparse_snr_layer` 默认产出**（T07 报「车道内 0 副本」） | ✅ **属实** | `grep -rn "默认产出\|默认稀疏\|现行默认\|默认开启" docs/science/` → 0 命中。我复核同 |
| R7 | **平场方向（本车道第 1 轮）** | ✅ **对，文档正确** | `calibrator.cpp:129/:139/:173/:183` 四处 `if (flat) v /= std::max(flat[i], 0.1f);` ⇒ **除法**。物理：`num = I_true·f ⇒ cal = num/f`。数值反例 `g=0.05, S=2000, bias=100, I_d t=0.1` ⇒ `raw=200.1`；除法得 2000.0（真值），乘法得 5.0（差 400×） |
| R8 | **偏置归属（第 1 轮）** | ✅ **对，文档正确** | `calibrator.cpp:128/:137/:172/:180` 四处 `v -= bias[i]` ⇒ **逐像元加性**。`master_dark = bias + I_d t` ⇒ 标准式只减一次；兼容式 `raw − bias − K(dark − bias)` 不二次减。`K=1` 恒等 ✓ |
| R9 | **`f/0.1 − 1` floor 相对误差式** | ⚠️ **对但不完整** | 七行表数值逐位正确（`f=0.05/0.02/0.01 ⇒ −50/−80/−90%`）。**但 `:91` 表头把只在 `f<0.1` 成立的式子用在全部七行**——`f≥0.1` 的四行真实误差是 0，字面套用得 900%/400%/100%/0%。一般式应为 `f/max(f,0.1) − 1`。见 §4-N8 |
| R10 | **MAD 常数 `1.482602218505602`** | ✅ **对** | python：`NormalDist().inv_cdf(0.75) = 0.6744897501960817`，倒数 `1.482602218505602`，**16 位逐位相同**。`CALIBRATION.md:166/:178`、`CCD_DEFECT.md:137` 三处一致 |
| R11 | **`κ ≥ 1/Σw² = 2`（3-11「不是信息论下界」）** | ✅ **对** | `var_repaired = (Σw²)var·κ ≥ var_j ⇔ κ ≥ 1/Σw²`。单列两邻 `Σw²=1/2 ⇒ κ≥2`；贴边单侧 `Σw²=1 ⇒ κ≥1`。代数无误 |
| R12 | **五点插值权重 `{−0.274, 0.774, 0.000, 0.774, −0.274}`** | ✅ **逐字对** | 我 `curl` 取 arXiv:1705.06766 全文 `pdftotext`，§4.5 逐字：「the resulting weights for a one-dimensional interpolation with α = 1 are {−0.274, 0.774, 0.000, 0.774, −0.274}; the bad pixel has of course a weight of 0.000」。权重和 = 1.000 ✓ |
| R13 | **`CALIBRATION.md:223` / `CCD_DEFECT.md:61`「无随仓复跑脚本 ⇒ 历史读数」** | ⚠️ **自标过强** | 我用**仓内现成** `run/NOISE-TAXONOMY-01/code/xisf.py` + 文档自己写的公式，从原始 XISF 复算：T2/T3/T4 九个中位、T4 斜率 `+0.420480`、截距 `1021.855`、max\|res\| `0.0342`、Δb `105.70`、T2 斜率 `−0.038236`/截距 `1042.111`/res `21.072`、CCD §3.1 的 `σ=1.7791`、`|J|=97`、连通域 `260755/76757/8` —— **11/13 个读数逐位命中**。「脚本不在仓内」为真，「因此不是可复跑判据」不成立 |
| R14 | **`UNRESOLVED-1`「须负责人裁」** | ❌ **推翻** | 见 §4-N1。`DATA_SEMANTICS.md:141` 自称「四者是同一个量的四个面名」——**同量 ⇒ 量纲唯一**，而 `engineering/standards/NUMERIC.md:35` 就在 DATA_SEMANTICS 自己指名的承载面里写着 `photo_scaled_adu` 量纲 = **`α × ADU`**、`:36` `surface_brightness` 量纲 = **`<标度量纲>/sr`**。这是**本车道内一句话定胜负**，不是跨层互斥，不需要负责人裁 |
| R15 | **T07 §2 N5「`a_k` 同名不同义，保留并区分」** | ❌ **推翻** | `UNIFIED_SCIENCE_MODEL.md:54` **从未写「无量纲」**（原文只说「相对亮度因子（可并入 `P_k`）」）。T07 把**代码实例**（`information_weight.h` 的 `ADU⁻²`）记成了**文档陈述**。且两处不是两个无关的量，而是同一因子在两张承载面上的取值 |
| R16 | **T07 §7.2「MAD 常数 1 ULP 两值并存」** | ✅ **维持不改** | 我 python 复核 `1/Φ⁻¹(0.75) = 1.482602218505602`（15 位有效 + 尾数 602）。`PSF.md:296` 的 `…023` 是其自身 bisection 脚本的输出。改任一侧都会造新矛盾，与本轮要止的病相同 |

**统计**：本车道第 1 轮订正条目中我判定 **12 条对 / 2 条不完整（§3-R9、§3-R13）**；止血轮判定 **13 条对（含 3 条推翻）**。**未发现「第 1 轮把方向改反」的情况**——两条方向性错误（平场、偏置）在本车道第 1 轮**改对了**，方向性错误在 `noise_snr` 车道（`k_photo`）。

---

## 4 本轮新发现清单

### 🔴 高（5 条）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| **N1** | `unified/DATA_SEMANTICS.md:102`（连带 `:63/:105/:107/:184/:186`） | 「`α_k · ADU`（**`α_k` 无量纲**）」错。`α_k ≡ k_photo ≡ photscal ≡ scale` **带量纲 `[F_syn 单位]/ADU`** | ① `NUMERIC.md:35` `photo_scaled_adu` 量纲写「**α × ADU**」、`:36` `surface_brightness` 写「**`<标度量纲>/sr`**」——而 `DATA_SEMANTICS.md:134` 自己声明该词表「由 engineering 正本的数值标准承载」，**本文件指向的承载面否定了本文件**。② `NUMERIC.md:115` 实测 `photscal = 2.3846837130250378e-17`，`×194.056 ADU ≈ 4.6e-15`，落在 Gaia XP 绝对通量（`W·m⁻²·nm⁻¹`）可信区间；无量纲则「194」不是通量。③ `:102` 自己的单位列写「`α_k · ADU`」——写这一列就预设了 `α_k` 带量纲。④ `ARTIFACTS.md:31`/`NOISE_SNR.md:270` 同向 | 删「（`α_k` 无量纲）」；`:141` 四名同量后补「四者同为带量纲的 `[F_syn]/ADU`」。⚠️ **`:63/:105/:184` 的 `ADU/sr` 是连带面，必须与 engineering 同一次改**——仓内唯一自洽写法是 `NUMERIC.md:36` 的 `<标度量纲>/sr`。单点改 `:102` 正是前两轮「修一处、坏三处」的形状 |
| **N2** | `NOISE_SNR.md:325/:347/:460` ↔ `point_information.schema.json:81` | `W`/`w` 单位 `F_syn⁻²` 与 canonical schema 的 **`const: "ADU^-2"`** 互斥，差 `k_photo² ≈ 5.7e-34`。本车道两处（`DATA_SEMANTICS.md:190`、`UNIFIED_SCIENCE_MODEL.md:143/:150`）与 schema 同侧，**NOISE_SNR 单方面相反** | 见 §2.3 S1 表格。`NOISE_SNR.md:351` 自己承认「二者只在 `k_photo ≡ 1` 时同值」 | 两册统一到一族；或三处互点「合成通量点源族」并声明**哪一个是产品落盘族**（生产 `information_weight.h:52` 与 schema 同侧 ⇒ 倾向 `ADU⁻²`）。跨车道，只报不改 |
| **N3** | `unified/UNIFIED_SCIENCE_MODEL.md:54`/`:57`/`:58` ↔ `NOISE_SNR.md:267/:346/:347` | `a_k` 未声明承载面 ⇒ 同符号跨文档差 `1/k_photo ≈ 4.19e16`。帧面点源面 `a_k ≡ 1` ⇒ `W ∈ ADU⁻²`；合成面 `a_k = 1/k_photo` ⇒ `W ∈ F_syn⁻²`。生产 `phase2_integrate.cpp:330` `f.a = ext["point_source"].value("a", 1.0)` **无量纲护栏**，照 `NOISE_SNR` 填 `1/k_photo` 会让 `W_info` 差 `1.76e33` 倍且无处拦截 | `detail/UNIFIED_MODEL.md:92` 已写 `a_k := F_ref,k/F0 = 1/k_photo,k` 并在 `:95/:99` 给两档权重式 —— **detail 早已统一，science 正本没跟上** | 在 `:54` 补一句「本式默认 `d_k` 与 `F` 同在帧面（`a_k ≡ 1`）；若 `F` 取合成系通量则 `a_k = 1/k_photo,k`，`W` 变 `F_syn⁻²`，**两档必须分名**」。与 N1 合并为一条待裁项 |
| **N4** | `unified/UNIFIED_SCIENCE_MODEL.md:98`/`:101`、`unified/DATA_SEMANTICS.md:171`/`:210` ↔ `NOISE_SNR.md:333`–`:339` | **叠加权重的归一化基准正面互斥**。顶点 `ACSD_DESIGN.md:111`/`:282` 与本车道两处给 `w = SNR²/F_ref²`；`NOISE_SNR.md:333` **明文**：「归一化因子**必须取公共锚 `F0`，不能取该帧的 `F_ref,k`**」，并给出理由「逐帧被该帧的测光标度二次缩放，`sum_k` 不再等于 `1/Var(F_hat)`」。同一符号 `w`，两套分母，差 `k_photo²` | 我逐行核 `NOISE_SNR.md:334-341` 原文与 `ACSD_DESIGN.md:111/:282`。`F_ref,k = F0/k_photo`（`NOISE_SNR.md:267`）⇒ 两式不等 | `DATA_SEMANTICS.md:202-208` 的落盘键表**没有承载 `F0`/`ZP_syn` 的键**（`F0` 在本车道三份中只出现在 `:208` 的推导里）。要么两档并列并写明「跨帧叠加必须走公共锚档」，要么补公共锚落盘键 |
| **N5** | `detection/ASTROMETRY.md:143`/`:144`/`:156` | **桥接门「三处取值互锁」的断言不成立：保守性下界在同一文件里被写成两个差 100 倍的数**。`:143` 与 `:156` 写 `1.79e-3` 角秒/像元，`:144` 写 `0.179″/px` | python：`ε = C_env·u·sec²Δ/s_rad`。`s=1.79e-3″/px, C=78 ⇒ ε=1.028e-6`（**与 `:156` 的「1.03e-6」「与 `1e-6` 恰好同量级」吻合**）；`s=0.179, C=78 ⇒ ε=1.028e-8`（**差 100 倍，与 `:156` 矛盾**）。`:144` 的「余量不低于 `5.0` 倍（`0.9″/px ÷ 0.179″/px`）」在 `:156` 的口径下应为 **503 倍** | `:144` 的 `0.179″/px` 应为 `1.79e-3″/px`（疑与同段「独立区间算术包络」的 `0.18″/px` 串行），随之「余量 5.0 倍」需重算或改述。⚠️ `:156` 自己写「三处取值互锁，必须成组引用」——**现在就不互锁** |

### 🟠 中（7 条）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| **N6** | `unified/DATA_SEMANTICS.md:78` ↔ `drizzle/DRIZZLE.md:115`–`:121` | **同符号不同义：`w_jp`**。`DRIZZLE.md` 明确区分两个参数化：`w_jp = a_jp/A_drop,j`（drop 归一，配 `N_p = D_p/pixfrac²`）与 **`w'_jp = a_jp/A_pixel,j`**（像元面积归一，配 `N'_p = D_p`），并写 `w'_jp = pixfrac²·w_jp`。`DATA_SEMANTICS.md:78` 用**符号 `w_jp`** 写 `a_jp / A_pixel,j` | `DRIZZLE.md:182` 明写「**信号与方差必须用同一个 `w_jp`**」。`docs/GLOSSARY.md:14` 的 `variance` 词条**已经把这条区分登记为契约**：「`D_p` 写法须配按像元面积归一的权重 `w'_jp=a_jp/A_pixel,j`…把 drop 面积归一的 `w_jp` 代入 `D_p` 写法会整体偏大 `1/pixfrac⁴` 倍」，且该词条**把 `DATA_SEMANTICS.md:72` 列为共同权威锚点** | `DATA_SEMANTICS.md:78` 的符号改 `w'_jp`（或加一句「本分册的 `w_jp` 即守恒映射正本的 `w'_jp`」）。**T07 的 N3 只收了 `c_jp`，漏了真正驱动 `pixfrac⁴` 误差的 `w_jp`/`w'_jp` 这一对**。风险量级：`pixfrac=0.8` 时 `pixfrac⁴=0.41`，即 2.44 倍 |
| **N7** | `detection/ASTROMETRY.md:49`/`:58` | **CRVAL 被误认成「天球北极的坐标」**。原文：「此时参考天球坐标（`CRVAL`）就是天球北极的坐标」/「落在原生极点（此处即天球北极）上」 | 我 `curl` 取 arXiv:astro-ph/0207413 全文：§2.3 逐字「For zenithal projections, (φ₀, θ₀) = (0, 90°) so the **CRVAL_ia specify the celestial coordinates of the native pole**, i.e. (α₀, δ₀) = (α_p, δ_p)」。我按该文式 (2) 在 (θ₀,φ₀)=(90°,0) 处求值：δ = sin⁻¹(sin δ_p) = δ_p ⇒ **δ_p = δ₀ = CRVAL2**。故原生极点落在天球北极**当且仅当 CRVAL2 = 0** | `:49` 改为「CRVAL 给出**原生极点**的天球坐标（原生极点即投影原点/切点）；`CRVAL2` 与天球北极的关系由标准式 (8) 决定，仅当 `CRVAL2 = 0` 时落在天球北极」；`:58` 删「（此处即天球北极）」 |
| **N8** | `detection/ASTROMETRY.md:54` | **`θ` 被标成「原生余纬」，与同文件 `:58` 自相矛盾**。`:54` 写「`θ` 为原生余纬」，`:58` 写「原生**纬度**取 `90°` 正是为了让参考点落在原生极点」 | 同上论文：§5 逐字「defining the radius as a function of **native latitude**, R_θ」；§5.1 图注「Gnomonic (TAN) projection; **diverges at θ = 0**」；正文「the gnomonic projection **diverges at the equator**」。验算：`R_θ=(180°/π)cot θ` ⇒ `R_θ=0 ⇔ θ=90°`（原生极点/切点）、发散在 `θ=0`（原生赤道） | `:54` 的「原生余纬」改为「原生纬度（`θ = 90°` 为原生极点/切点，`θ = 0` 为原生赤道）」 |
| **N9** | `calibration/CCD_DEFECT.md:91` | **与所引原式的方向相反**。原文：「距离越大，形式噪声越小——按邻元水平给填补像元记账，恰恰落在该式给出的**下限方向**上」。但 `σ_mask = σ_org·(2d+1)^(−0.5)` 对 `d≥0` **单调递减**，`σ_org` 是 `d=0` 端的**最大值**，即**上限** | 我实算：`d=0 → σ_org`（最大）、`d=1 → σ_org/√3`、`d→∞ → 0`。**注**：`:81`「按邻元方差记账会系统性**低估**该处方差」若理解为「低估**真实不确定度**」则是成立的（作者自陈「很难给填补值指定不确定度」），错的只是 `:91` 对**该式**方向的判定 | `:91` 改为「按邻元水平记账落在该式的**上限（`d=0` 贴边处）**」；`:81` 明确「低估」所指为**真实不确定度**而非该式给出的形式噪声 |
| **N10** | `calibration/CALIBRATION.md:131` vs `:134` | **隔三行自相矛盾**：`:131` 说偏置失配「按一条**与 `K` 无关的常量平移**进入两支的分子」，`:134` 却定义「偏置电平失配项 = `(b_bias − b_light)·(K − 1)`」（含 `K`）。**公式正确，散文定性写反** | python：`(b_bias−b_light)=100` 时 `K=1,2,3,4 ⇒ 0,100,200,300`（严格仿射，非常量）。第 1 轮订正记录 `T06-第1轮订正…:275` 自称「B4 给出了 `(b_bias − b_light)(K − 1)` 的**精确形式**」——**形式对了、散文没跟上**，第 2 轮未抓住 | `:131` 改为「进入分子的偏置项是 `K` 无关的常量，而残差中的偏置电平失配项含 `(K−1)`」。⚠️ **结论（须单独实测与登记）保留，只换理由** |
| **N11** | `calibration/CCD_DEFECT.md:156` | **SNR 后果判据的比值方向未写明，按自然读法是错的**。「比较『施加 κ』与『不施加 κ』两种口径下修复区的信噪比，**比值必须等于 `√κ`**（本链取 `√2`）」 | `var_不施加 = (Σw²)var = 0.5var`；`var_施加 = κ·0.5var = 1.0var`。故 `SNR_不施加/SNR_施加 = √2 = √κ`，而 `SNR_施加/SNR_不施加 = 1/√κ ≈ 0.707`。`:123` 自己也写「修复像元的信噪比会被系统性高估约 `√2` 倍」 | 把「比值」写成显式有序式 `SNR(不施加)/SNR(施加) = √κ`。**按自然读法（施加/不施加 = `√κ`）该门恒红**，即一条永不可能变绿的判据 |
| **N12** | `calibration/CALIBRATION.md:91` | **表头公式只在 `f<0.1` 成立，却用于全部七行**。`:86` 给 `(cal−cal_true)/cal_true = (f/0.1) − 1`，`:91` 表头列直接写「相对误差 `(f/0.1 − 1)`」，而 `f = 1.0/0.5/0.2/0.1` 四行的真实误差是 **0**，字面套用会得 900%/400%/100%/0% | 七行数值全部正确（我逐行实算）；**问题只在列名复用的公式不是分段式**。一般式应为 `f/max(f,0.1) − 1` | 表头改为 `f/max(f,0.1) − 1`，或注明「本列仅对 `f<0.1` 的行成立，其余行的 floor 未生效故误差为 0」 |
| **N13** | `unified/DATA_SEMANTICS.md:117` | **内部不自洽**：`Ω_ref = 1 arcsec²` 项文档写「`−26.5717` mag」，但同句的自检值 `36.5721` / `−16.5721` 只有取 **`+26.5721`** 才闭合 | python：`2.5·log10(1 arcsec²) = 26.572125665882297`（`1 arcsec² = 2.3504430539097885e-11 sr`）。`10 − (−26.5717) = 36.5717 ≠ 36.5721`；`10 − 26.5721 = −16.5721` ✓ | `−26.5717` 改为 **`+26.5721`**。⚠️ **这是第 1 轮 3-02 订正时新引入的**（订正把方向从 `0.1/f−1` 改对时，同句的位数字残留） |

### 🟡 低（8 条）

| # | 位置 | 问题 | 依据 |
|---|---|---|---|
| **N14** | `calibration/CCD_DEFECT.md:52`–`:57` vs `:66`–`:71` | 仓内复算表的「97 处跳变 → 反号就近配对成 53 段 → 保留 111 列」**在正本给出的公式下不可复算**：`J` 只定义跳变**集合**，「反号就近配对成段」**无公式**，且 `111 > 97` 的映射规则未写 | 两个子代理独立复跑：`σ=1.4826·MAD` 与 `\|J\|=97` **逐位复现**（数据链可信）；但「53 段 / 111 列 / p90=927」在任何合理重建下都得不到。子代理 B 指出 `\|J\|=97` 的符号分布是 **正 49 / 负 48**，纯反号配对上限 48 段，要得 53 段必须额外允许「未被配对的单跳变自成一段」——**这条规则文档没写**。定性：**判据欠规格，不是数值错** |
| **N15** | `detection/STAR_DETECTION.md:161` | 「**2 轮** 3σ 门剔掉约 `0.27%`」——`0.27%` 是**一轮**的值（`2Φ(−3)=0.26998%`）。两轮累计：独立相乘给 `0.539%`；迭代裁剪（第二轮作用于已截尾样本）MC 给 **≈0.28%** | 我 python + 子代理 MC。**注**：两个口径差别来自「轮」的实现方式，文档没写是哪种。无论取哪个，`0.27%` 都是一轮值。定性结论（3σ 会真实重塑分布 vs 5σ 的 `1e-6` 量级）**不受影响** |
| **N16** | `detection/STAR_DETECTION.md:70` | 「比生产核**低** `5.91%`」方向与所指量都对不上 | python：连续高斯核 `‖K‖₂=0.1410474` vs 生产 `0.1331742` ⇒ **高 5.912%**；而句中另两数 `7.0898`、`35.4491` vs 生产 `7.5090`、`37.5448` ⇒ **低 5.582%**。三个数没有一个是「低 5.91%」 |
| **N17** | `detection/ASTROMETRY.md:45` | 等式链首项标签错 0.5 像元：「偏移量取 `像元中心 − w/2 = (k+1) − (w/2+0.5) = k+0.5 − w/2`」——上一句刚定义「像元中心」为 `k+1`，故首项应是 `(k+1) − w/2`；中间实际减去的是 `CRPIX` 不是 `w/2` | python：`((k+1)−w/2) − ((k+1)−(w/2+0.5)) = 0.5`。**值的正确性我核过**（`ipv_select.cpp:1118-1124` `U.x = det_x − img_w/2.0`），**只有首项标签挂错**。讽刺的是这正是该段要防的 0.5 像元误判 |
| **N18** | `detection/STAR_DETECTION.md:78` | 违反本文件自立的强制条款：`:72`「σ 由哪一个背景噪声估计器给出…这条声明是**强制项**」、`:163`「凡写『背景噪声』的判据**必须点名用哪一套**」；而 `:78` 只写「拟合振幅除以**背景噪声标准差**」 | 实质不冲突（门表 `GATES_AND_TOLERANCES.md:37` 已钉在行差分 `bgnoise`），但本册强制条款未在自身正文执行 |
| **N19** | `detection/STAR_DETECTION.md:158`/`:159`/`:163` + `:192` | §5.1「各管一摊」的映射**对产出检出目录的节点不成立** | 我亲手读源码：`star_detector/wrapper_phase1/star_detector.cpp:153` `thr = cat.background + detection_sigma_ * cat.noise_sigma`，其 `noise_sigma` 来自 `estimate_background()`（`:93-121`，**2 轮 median±3σ 裁剪后 RMS** = **裁剪族**），作用在**未平滑原图**上、无 YvV；`module_adapters.cpp:3954` 构造 `StarDetector(5.0)`、`:4131` `det.detect(...)` 接线。⇒ **star-psf 盲检路径的阈值与信噪比分母同族**；`star_detector.cpp:110` 明写「`// sigma-clip 2 轮: median ± 3σ`」 |
| **N20** | `calibration/CCD_DEFECT.md:121`；`detection/STAR_DETECTION.md:161` | 判据依据的实测数**无复跑路径、无来源文件**。`:121` 的 `Var(pred − truth) = 0.5502·σ²`；`:161` 的两族比值 `1.5012` | `:161` 的绝对值落点在 `algorithms/STAR_DETECTION_ALGORITHMS.md:443`（`20.7384 vs 13.8148 ADU` ⇒ `1.50116`），但本册只给裸比值、无帧号无绝对值。同文件 `:80` 对自己那三个读数**明确**写了「复现脚本与结果文件不在仓内…不作判据依据」——**同一文档两套证据标准** |
| **N21** | `calibration/CCD_DEFECT.md:196`–`:203` | 参考文献列表 `[2]` 与 `[3]` 之间有**两个连续空行** —— 与止血在 `CALIBRATION.md:274`、`SCIENCE_SCOPE.md:126` 修掉的是**同一类缺陷，本车道漏改** | `awk` 连续空行扫描：`CCD_DEFECT.md` 唯一一处 |

### 🟢 仅登记（3 条，不建议本轮动）

- `unified/SCIENCE_SCOPE.md:36` 与 `DATA_SEMANTICS.md:36`：`photo_scaled_adu` 写「`α_k·ADU`」、`surface_brightness` 写「`ADU/sr`」——N1 的连带面，须与 engineering 同步，不宜单点改。
- `detection/STAR_DETECTION.md:59` 把二维核的 L2 范数记成 `‖k‖₂`（真一维 `‖k‖₂ = 0.364930`；该值是**平方范数**即平滑增益因子）。算术全对，**记号**应改 `‖K‖₂`。
- `calibration/CALIBRATION.md:36` 的「**待裁决的 1 LSB 口径差**」把工作日志叙事（「仓内一份真实母版的回归反解」「取值冻结前须由负责人裁决」）写进科学正本，违反 AGENTS §5「正文无历史叙事」。数值本身（`1/65535` vs `1/65536`，相对差 `1.5259e-5`）我实算正确。

---

## 5 本轮是否零发现

**明确：不是零发现。共 21 条（🔴 高 5 / 🟠 中 7 / 🟡 低 8 / 🟢 登记 3），全部落在本车道十份文件内。**

按十轮归类：结构 2（§2.2 的四处车道内重复）、口径 5（N1/N2/N3/N4/N19）、公式 7（N6/N9/N10/N12/N13/N17）、证据 2（N15/N20）、引用 1（N21）、推理 1（N5/N7/N8）、负向 2（N11 的恒红门、N18 的自设强制条款未执行）、一致性 3（N2/N4/N6）、语言 1（`CALIBRATION.md:36` 的历史叙事）、可复现 3（N15/N20 及 §3-R13 的自标过强）。

**我亲自实算/取原文核对并通过的（防止下轮重复劳动）**：
- `1/Φ⁻¹(0.75) = 1.482602218505602`（16 位逐位）；`Φ⁻¹(0.75) = 0.6744897501960817` ✓
- `T2` 最小二乘斜率 `−0.038236`、截距 `1042.111`、max\|res\| `21.072`、Δb `+40.24` ✓；`T4` 斜率 `+0.420480`、截距 `1021.855`、max\|res\| `0.0342`、Δb `+105.70` ✓；`105.70/57.82 = 1.828` ✓
- `flat` floor 七行表 `(f/0.1−1)` 全对（`−50/−80/−90%`）；`65536/65535−1 = 1.5259e-5` ✓
- `CALIBRATION.md:125-128` 三步展开代数逐位闭合；`Δt_max = t_d(ε σ_frame/|Δb| − 1)` 推导 ✓
- Young–van Vliet：`q = 3.97156 − 4.14554√(1−0.26891σ_s) = 1.1532634818423229`（逐位）、`B+b1+b2+b3 = 1`、`5/0.133174181553 = 37.5448`、`1/0.133174181553 = 7.5090`、连续高斯 `0.1410474`/`7.0898`/`35.4491` 全对 ✓
- Moffat4 / 高斯宽度：`2√(2ln2)=2.354820045`、`2√2·√(2^{1/4}−1)=1.230307652590102`、`2√(2^{1/4}−1)=0.869958884`、`比值 1.9140` 全对 ✓（我初判时手算有误，python 复核后确认文档全对，**不构成发现**）
- Clopper–Pearson：`0.05^(1/64)=0.95427`、`^(1/100)=0.97049`、`^(1/298)=0.99000`、`^(1/1000)=0.99701`、`ln.05/ln.99=298.0729` 全对 ✓
- FP32：`1e-12·α² ∈ [1.2966e-46, 3.610e-45]`、最小次正规数 `1.4013e-45`、`4.204/1.4013 = 3.000` ulp、`1/floor = 7.14e44 > fp32max` 全对 ✓
- 桥接门：`sec²Δ = 1+(20°/2 rad)² = 1.0304617`、`γ_max ≈ 9.90°`、`s_rad(0.9″/px) = 4.3633e-6`、`C=128 ⇒ ε=3.356e-9`（文档 3.36e-9）、`1e-8/3.356e-9 = 2.980` 全对 ✓
- **ASTROMETRY 的 LONPOLE 缺省规则**：我取 arXiv:astro-ph/0207413 全文，§2.2 逐字「The default value of LONPOLE will be 0 for δ₀ ≥ θ₀ or 180° for δ₀ < θ₀. Thus, for example, in zenithal projections the default is always 180° (unless δ₀ = 90°) since θ₀ = 90°」—— **`:49` 表述完全正确**（我曾按记忆怀疑它漏了 θ₀=90/0 两个分支，**取原文后撤回**）
- **TAN 式 (54)(55)** 与「diverges at θ = 0」逐字对上；`s0 = 3600·√|det(CD)|` 量纲闭合 ✓
- **FITS 4.0 逐条**：Table 3 含 `steradian` ✓、Table 6 幂次只允许 `**`/`ˆ`/并置（ASCII `^` 确不允许）✓、`case is significant throughout` 在 **§4.3 正文内**（不在 §4.3.1）✓
- **五点插值权重** `{−0.274, 0.774, 0.000, 0.774, −0.274}` 与 HSC 原文逐字一致，权重和 = 1.000 ✓
- **引用完整性**：本车道 10 份的方括号编号连续、无孤儿、无悬空角标 ✓

---

## 6 我推翻的既有结论

| # | 被推翻的说法 | 原出处 | 推翻依据 |
|---|---|---|---|
| 1 | 「UNRESOLVED-1 是 science 与 engineering **同层正本互斥**、AGENTS §4 无更高层可裁 ⇒ **须负责人裁**」 | `T07` §7.1 UNRESOLVED-1 | `DATA_SEMANTICS.md:141` 自称「四者是同一个量的四个面名」⇒ **同量即量纲唯一**；而 `NUMERIC.md:35/:36`（`DATA_SEMANTICS.md:134` 自己指名的承载面）写「`α × ADU`」「`<标度量纲>/sr`」。**本车道内一句话定胜负，不是跨层互斥** |
| 2 | 「`UNIFIED_SCIENCE_MODEL.md:54` 的 `a_k`……**无量纲**」（作为**文档陈述**） | `T07` §2 N5 | 该行原文只写「相对亮度因子（可并入 `P_k`）」，**从未出现「无量纲」三字**。T07 把代码实例（`information_weight.h` 的 `ADU⁻²`）记成了文档陈述。⇒ 同名不同义的判定建立在一条不存在的引文上 |
| 3 | 「`a_k` 判为**同名不同义，保留并明确区分**即可」 | `T07` §2 N5 | 不是两个无关的量，是**同一因子在两张承载面上的取值**（帧面 ≡1 / 合成面 `=1/k_photo ≈ 4.19e16`）。`detail/UNIFIED_MODEL.md:92/:95/:99` 早已统一，science 正本没跟上；数值差 `a_k² = 1.76e33`，且 `phase2_integrate.cpp:330` 无量纲护栏。**「保留并区分」掩盖了真后果**（见 N3） |
| 4 | 派单与前轮暗示的「`0.7316727929211932 = 1/√(2−√2) = 1/1.36602540378`」 | 派单 / 前轮 | python：`1/√(2−√2) = 1.3065629648763766`、`1/1.36602540378 = 0.7320508075688772`，**三者皆非**。两个 detection 子代理独立重推出真身：它是高斯噪声 **10–90% 截尾均值 \|r\|** 的 σ 因子，闭式 `2(φ(Φ⁻¹(0.55)) − φ(Φ⁻¹(0.95)))/0.8 = 0.731673095280613`，与代码常数相对差 `4.13e-7` |
| 5 | 我自己初判的「ASTROMETRY 的 LONPOLE 缺省规则漏了 θ₀=90/0 两个分支、推导无效」 | 我本轮中途 | 取 arXiv:astro-ph/0207413 §2.2 原文后**撤回**——论文给的就是两分支式，并紧接着给出天顶族的结论。**`:49` 正确** |
| 6 | 我自己初判的「Moffat4 `FWHM/α = 0.869958884` 与 `1.230307652590102` 两位数有误」 | 我本轮中途 | python 逐位复核**两数全对**，是我手算精度不足。**不构成发现** |

**我否决的子代理结论（5 条）**：
- 子代理（detection）给出的 `δ_p = arctan(1/sin δ₀)` —— **错**。同篇论文原文即 `(α₀,δ₀) = (α_p,δ_p)`，我按式 (2) 在 (θ₀,φ₀)=(90°,0) 处求值也得 `δ_p = δ₀`。**N7 的结论我采纳，推导换成论文原文。**
- 子代理（detection）的 F6「`ASTROMETRY.md:101` 的 `s0²` 量纲不成立」—— **不采纳**。`rms_arcsec/s0`（除）与 `rms_arcsec·s0`（乘）的比恰为 `s0²`，原句可读通。
- 子代理（detection）的 F3 归纳「`53 > 48` 故算术不可能」—— **不采纳**（同一车道另一子代理已给出可行重建：44 对 + 9 单跳变段 = 97）。**真实缺陷是「单跳变成段」的规则没写**，不是数值错。N14 已按后者表述。
- 子代理（unified）关于「『case is significant throughout』在 §4.3.1 不在 §4.3，第 1 轮订正记录写错了」—— **不采纳**。我 `pdftotext` 定位：该句在 `fits40.txt:1796`，`4.3.1. Construction of units strings` 标题在 `:1814`，**该句落在 §4.3 正文内**。第 1 轮订正记录的说法**是对的**。
- 子代理（unified）的「FITS 4.0 Table 4 未列 `px`，故 `:119` 成立」—— **不采纳**。我取 FITS 4.0 全文 `fits40.txt:1581-1585`，Table 4 的 Additional allowed units **同时列有 `pixel` 与 `pix`**（类型 `(image/detector) pixel`）。⇒ `DATA_SEMANTICS.md:119`「`px` 不是标准列出的单位符号」**不成立**，且与同文件 `:196`「读侧兼容同幂次的 `px`、`pixel` 写法」自相矛盾。**该条我并入 N6 一并报告**（`:119` 的第二条理由——ASCII `^` 非允许幂号——仍成立）。

---

## 7 自证段

### 7.1 我做了什么

1. **逐行读完本车道 10 份 / 1 635 行**（read 工具逐篇，非脚本扫描）；逐行读完 `T07`（413 行）与第 1 轮订正记录相关节。
2. **亲手重推并用 python3 实算 20+ 组**：MAD 常数（16 位逐位）、T2/T4 最小二乘（斜率/截距/残差/Δb）、floor 七行表、`Δt_max` 推导、三步展开闭合式、Young–van Vliet 全部系数与 `‖K‖₂`/等效倍数、Moffat4 与高斯宽度系数、Clopper–Pearson 全表、FP32 次正规数/上溢、`SB_mag` 的 `Ω_ref` 项（发现 N13）、桥接门 `C_env` 全套（发现 N5）、σ-clip 剔除率、指向 DATA_SEMANTICS 的死节锚全量普查（§2.3 S2）、`80789.7/65535`、`(b_bias−b_light)(K−1)` 的仿射性、ASTROMETRY 等式链的 0.5 像元差。
3. **取一手文献全文并逐字核对**（`curl` + `pdftotext`）：FITS Standard 4.0、IVOA HiPS REC-HIPS-1.0、Calabretta & Greisen 2002（Paper II）、HSC pipeline（arXiv:1705.06766 §4.5）。
4. **读一手源码判定方向**：`lib/algorithms/calibration/src/calibrator.cpp`（平场除法、偏置逐像元加性）、`star_detection/wrapper_phase1/star_detector.cpp:93-153`（σ_bg 族）、`module_adapters.cpp:3954/:4131`（接线）、`lib/algorithms/calibration/src/master_generator.cpp`、`hiss_format.h:300`（`photscal = 1.0` 「实际应用比例」）、`NUMERIC.md:115`（`photscal = 2.3846837130250378e-17`）。
5. **核 canonical schema 与 engineering 正本**：`point_information.schema.json:81`（`const: "ADU^-2"`）、`UNIFIED_OBJECTS.md:36/:41/:42/:89/:115/:124-130`（13 处死节锚）。
6. **派发 6 个子代理**（unified×2、calibration×2、detection×2，**每车道对抗双跑**），逐条复核并记录采纳/否决（§6）。

### 7.2 我没做什么（诚实边界）

- **未编译、未跑 ctest/pytest、未跑任何仓内脚本、未执行端到端**。所有「代码为准」的判定只到「读 `文件:行` + 配置实测」层；量纲判断是纸面推导 + python 数值代入。
- **零文件修改、零 git 写操作**。交付时实测：`git diff -- docs/science/unified docs/science/calibration docs/science/detection | wc -l` → **0**，我车道 10 份**逐份零修改**。工作树上的其余改动（`docs/engineering/**`、`docs/science/algorithms/**`、`lib/**`、`eng/**`、`README.md`、`docs/DOCUMENT_INDEX.yaml`）**全部来自并发车道**，另有 `run/GOVERN-08/审核包-R2/` 下其它车道并发产出的未跟踪交付件，**均不是我的改动**。
- **未取到全文因而报「核对不到」的**：Tonry 2012 PS1 5σ 深度口径原文、Horne 1986 / Naylor 1998 全文、Janesick 2001 正文、Aitken 1936 卷期页、Croux–Rousseeuw 1992 与 Rousseeuw–Crox 1993（扫描件，我用 `pdftotext` 只出换页符）、LSST `ip_isr` 钉住 commit（Cloudflare 挡）。**不凭印象判定其对错。**
- **未复跑**：`CALIBRATION.md:223`/`CCD_DEFECT.md:61` 所依据的复算（我复现了 11/13 个印刷数字，但用的是我自己写的临时脚本，未入库）；`σ_frame = 57.82`（依赖 T4 组 180 s Lum 亮场，子代理报告实测 57.8215，但我未定位到该文件）；`DATA_SEMANTICS.md:54` 的 HiPS tile 实测对拍（脚本与 tile 数据不在可复跑路径）；`DATA_SEMANTICS.md:167` 所依据的 33 帧 α 数据集（子代理报该数据集在当前树中不存在，我未复核其不存在性）。
- **未追时间线**：未用 `git log -S` / `git blame` 定位「谁改正本而漏改副本」。
- **跨车道只报不改**：`docs/engineering/**`、`docs/detail/**`、`lib/**`、`eng/**`、`noise_snr/**`、`psf/**`、`algorithms/**` 全部只定点对照。
- **本车道第 2 轮审稿件缺失**（见开头登记），因此「第 2 轮引入了哪些问题」我无独立对账依据，只能与第 1 轮审稿/订正和 T07 比对。

### 7.3 跨切面四处数字的复跑真值

被质疑的是 `T06-第1轮订正-子代理E-对抗复核.md:118/:257`（条目 N-12）自证式判据里的四个数。**我用 `git archive 18dd6c64` 抽出 HEAD 树，与工作树分别复跑，并把口径写死**：

| # | 数字 | 子代理 C 声称 | 前车 E 复核称 | **我的复跑真值** |
|---|---|---|---|---|
| 1 | `§` 出现总数 | 255 → 77 | HEAD 709 / 改后 46 | **口径 A**（E 用的命令 `grep -o "§" docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md`）：HEAD **46**、工作树 **46** ⇒ **改前改后相同，E 的「709 → 46」是把两个不同口径的数字相比**。<br>**口径 B**（`docs/detail/` 全递归）：HEAD **432**、工作树 **432** ⇒ 同样无变化。<br>⇒ **「止血把 § 从 709 降到 46」不成立**；止血对 `docs/detail/**` 的 `§` 数量**零改变** |
| 2 | 「X」一节形态 | 114 处（一处未动） | 改前 50 / 改后 127（反而增加） | 精确串 `」一节`：HEAD **131**、工作树 **131**；泛匹配 `」[^ ]{0,12}一节`：**132**。⇒ **C 的 114 与 E 的 50/127 均不可复现**；真实值是 **131（两侧相同）** |
| 3 | 含 `§` 的残留行 / `STAR_DETECTION_IMPL_DESIGN.md` | 残留 57 行，其中该文件 50 行 | 该文件 35 行 | **`STAR_DETECTION_IMPL_DESIGN.md` 含 `§` 行数 = 35** ⇒ **E 的 35 正确、C 的 50 错**。<br>「残留 57 行」不可复现：改后 `docs/detail/**` 含 `§` 的行数实测 **315** |
| 4 | 跨文档 `ALG §` | 16 处 | 0 处 | `grep -rn "ALG §" docs/` → **11**；`docs/detail/` 内 → **8**。⇒ **C 的 16 与 E 的 0 均错**；真实值 **11（全 docs）/ 8（detail）** |

**给前台的元结论**：四处数字中，**只有第 3 处（35 行）被前车复核对了**；第 1 处 E 的复核**方法学有缺陷**——它把「改后」的三 glob 命令结果（46）与「改前」的全树结果（709）相比，两侧口径不一致，结论方向不可靠。第 2、4 处两侧数字均不可复现。
⚠️ `##` 标题里的 `§`（如 `GREP §1`）与跨文档 `ALG §` 两串会随写作用法增加而减少，**我复跑时 HEAD 树与工作树的 `docs/detail/**` 完全一致**（`git status` 该路径无改动），故「改前/改后」两侧相同是事实，不是我的口径误差。

---

## 8 给前台的三个提醒

1. **止血对本车道等于没做（2 个空行），本车道不是「审过了」，是「没被看」。** 下一轮若仍按 `T07 §7.2 待明项清单` 派单，本车道会**第三次**被整体跳过。建议把 `docs/science/{unified,calibration,detection}` **显式列入下一轮派单范围**。
2. **N1/N2/N3/N4 是同一个根**：「帧面 vs 合成面」口径从未声明，导致 `α_k` 量纲、`point_information` 单位、`a_k` 承载面、权重归一分母四条同时悬空。**单点改任何一条都会造新矛盾**——必须一次性成套处置，且 N1/N2 涉及 engineering 与 canonical schema，属跨车道同步。
3. **N6（`w_jp`/`w'_jp`）是本轮唯一一条「止血该收没收、且收错了对象」的实例**：T07 的 N3 收了 `c_jp`（`sr⁻¹` vs 无量纲），却漏了真正驱动 `pixfrac⁴` 误差的 `w_jp`/`w'_jp` 这一对，而 `docs/GLOSSARY.md:14` **早已把这条区分登记为契约**、并把 `DATA_SEMANTICS.md:72` 列为共同权威锚点。建议下一轮把 `GLOSSARY` 的契约面作为重复断言收敛的**校验清单**——凡 GLOSSARY 已登记的区分，止血的 N 清单里必须有对应条目。