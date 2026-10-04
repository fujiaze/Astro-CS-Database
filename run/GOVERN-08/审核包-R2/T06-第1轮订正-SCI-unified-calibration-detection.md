# T06 第 1 轮订正 · SCI-unified-calibration-detection

> **车道**：`docs/science/unified/**`、`docs/science/calibration/**`、`docs/science/detection/**`；`docs/DOCUMENT_INDEX.yaml` 中属本批的 7 条。
> **审稿来源**：`T06-审稿-SCI-unified-calibration-detection.md`，第 1–5 轮共 **80 条实质问题** + §4.1 噪声常数裁决 + §5 十二条推翻记录。
> **基线**：HEAD `128a1b00610367d6492c14e1aaa018a724fdef5e`；本车道 7 份文档 + 索引条目已改写，零 git 写操作、零编译、零 ctest/pytest。
> **写入文件**：`docs/science/unified/{DATA_SEMANTICS,SCIENCE_SCOPE,UNIFIED_SCIENCE_MODEL}.md`、`docs/science/calibration/{CALIBRATION,CCD_DEFECT}.md`、`docs/science/detection/{ASTROMETRY,STAR_DETECTION}.md`、`docs/DOCUMENT_INDEX.yaml`。

---

## 1 处置总表

| 处置 | 条数 | 编号 |
|---|---|---|
| **已改**（本车道文档内落地） | **66** | 1-01,1-02,1-03,1-04,1-07,1-08,1-09,1-10,1-11,1-12,1-13,1-14；2-01,2-02,2-03,2-04,2-05,2-06,2-07,2-09,2-10,2-12,2-13,2-16；3-01,3-02,3-03,3-04,3-05,3-06,3-07,3-08,3-10,3-11,3-12,3-14；4-01,4-02,4-04,4-05,4-06,4-07,4-08,4-10,4-11,4-12,4-13,4-14,4-15,4-16,4-17,4-18,4-19,4-20,4-21；5-01,5-02,5-03,5-04,5-05,5-06,5-07,5-08,5-09,5-10,5-11,5-12,5-13 |
| **已改但降级**（审稿定性过强，按实际缺陷改） | **4** | 2-08、2-15、4-03、3-09 |
| **推翻／撤回审稿判定**（保留原判不抹除） | **4** | 3-13(=5-G)、4-13、4-16(=5-13)、2-17 的前提 |
| **登记代码／合同侧**（本单不改代码） | **7** | 1-05、1-06、2-11、2-14 侧、3-11 侧、2-17 侧、5-07 侧 |
| **登记跨车道／前台** | **8** | 1-18(=2-18)、§4.1 噪声常数、2-05 术语表、4-08 ESA 对照、5-04 PLATESOLVE 同步、5-12 跨卷、A 附带的 13 处旧路径、4-19 第二个生成器 |
| **保留原判的正面登记** | **1** | 4-22 |
| **合计** | **80+** | |

---

## 2 四条最重项：逐字改前改后

### 2.1 平场 floor 相对误差式（3-01）—— 自推，审稿正确

**改前**（`CALIBRATION.md`）：
> 由 `cal = num / max(flat_norm, 0.1)`，真实响应 `f < 0.1` 的像元被按 `0.1` 相除，该像元的校准值相对误差为 `(0.1/f − 1)`，**单边偏低**（欠校正）

**改后**：
> 由 `cal = num / max(flat_norm, 0.1)`，真实响应 `f < 0.1` 的像元被按 `0.1` 相除，即 `cal = num/0.1` 而真值为 `cal_true = num/f`，故该像元的校准值相对误差为 `(f / 0.1) − 1`；它是 `f` 的增函数、在 `f = 0.1` 处为零、随 `f → 0` 单边趋于 `−1`，即**单边偏低**（欠校正）……倒过来写 `0.1/f − 1` 得到的是**响应倒数**的相对误差，随 `f → 0` 发散，量纲上描述的不是校准值。

**我的推导**（`python3`，不采信审稿结论）：`cal/cal_true = (num/0.1)/(num/f) = f/0.1`，故相对误差 `= f/0.1 − 1`。

| `f` | 我的式 `f/0.1−1` | 审稿指出的旧式 `0.1/f−1` | 文档表格原值 |
|---|---|---|---|
| 0.05 | −50.0% | +100% | −50.0% |
| 0.02 | −80.0% | +400% | −80.0% |
| 0.01 | −90.0% | +900% | −90.0% |

旧式与**同节表格**直接矛盾，且符号与同句「单边偏低」的定性相反。表格数值本身正确，只改正文式。同步把表头补成「相对误差 `(f/0.1 − 1)`」，使表头自带口径。

### 2.2 面亮度星等的 `Ω_ref` 项符号（3-02）—— 自推，审稿正确

**改前**（`DATA_SEMANTICS.md`）：
> `SB_mag = ZP_k − 2.5·log10(signal) + 2.5·log10(Ω_ref)`

**改后**：
> `SB_mag = ZP_k − 2.5·log10(signal) − 2.5·log10(Ω_ref)`，其中 `Ω_ref` 为选定参考立体角，`SB_mag` 读作「面积 `Ω_ref` 内该面亮度的总星等」。推导：`F = signal·Ω_ref`，代入 `m = ZP_k − 2.5·log10 F` 得 `m(Ω_ref) = ZP_k − 2.5·log10(signal·Ω_ref)`，`Ω_ref` 项因此是减号……

**我的推导**：唯一使 `Ω_ref` 出现的读法是「`Ω_ref` 内该面亮度的总星等」，而 `ZP − 2.5log10(SB·Ω)` 的展开对 `log10 Ω` 必为负号。反向核对用标准式 `μ = m + 2.5·log10(A)`（`A` 以 `arcsec²` 计），换到立体角口径后同一项落在减号。

**数值自检**（`python3`）：`ZP=0`、`SB=1e-4/sr`、`Ω=10 sr` ⇒ 总星等 `7.5`；`Ω_ref = 1 arcsec² = 2.350443e-11 sr` ⇒ 正确式 `36.5721`，加号式 `−16.5721`，**差 53.1443 mag**。`Ω_ref = 1 sr` 时两式都等于 `10.0000` ⇒ 符号错误只在 `Ω_ref ≠ 1 sr` 显形。审稿给出的 53.14 mag 与我的复算逐位一致。

### 2.3 HiPS tile 行列方向（3-13 = 审稿 5-G）—— **推翻审稿判定**

**审稿原判（保留）**：
> `FITS index = (511 − x)·512 + y` 的行项**我判定为行方向反了**……文档写的 `511 − x` 是北向减的量，把 `x = 0`（最南）放到行 511（最北）⇒ **南北镜像**；「`FITS index` 的行项应为 `x·512`」；并列为「本车道最重的一条结构/公式缺陷」。审稿自注本单曾被一个相反子代理结论误导、但仍按原方向推翻。

**结论：审稿判错，文档正确。行项 `511 − x` 保留不动。**

我回到一手来源重走三条独立链：

1. **Górski 2005 §5.2 散文与式 (13)(14)(15)(17)**（`curl -sSL https://arxiv.org/pdf/astro-ph/0409513 | pdftotext` 逐字取）：
   > "They both have their origin in the **southernmost** corner of each base-resolution pixel, with the x index running along the **North-East** direction, while the y index runs along the **North-West** direction."
   > `x = [… b2 b0]₂`　`y = [… b3 b1]₂`　`v = x + y`　`i = F1(f)·Nside − v − 1`　`z = 1 − i²/(3Nside²)`

   `z = cos θ`、θ 自北极量起 ⇒ `i` 增大更南；`i = … − v − 1` ⇒ `v` 增大更北；`v = x+y` ⇒ **x、y 都向北**。散文与式 (17) 一致，审稿依赖的「式 (11) 前 F1 索引 southernmost corner」这条读法也同向。
2. **healpy 1.20.1 实测**（我自己跑，非采信）：nside=64 逐面，`(0,0)→(63,0)` 与 `(0,0)→(0,63)` 的 Dec 分别 `+0.597°→+41.810°`（face 0）、`−41.014°→0.000°`（face 5）、`−89.269°→−41.810°`（face 8）⇒ x、y 在**全部 12 个面**都向北增。`pix2xyf` 的 `x` 取偶数位。
3. **真实 tile 的 WCS 求值**（`astropy` 7.0.1 + wcslib，读 `https://alasky.u-strasbg.fr/2MASS/H/Norder5/...` 的 FITS tile）：`CTYPE1=RA---HPX`、`CTYPE2=DEC--HPX`、`PV2_1=4`、`PV2_2=3`；**行 0→511 一律向南，列 0→511 一律向西**。四种假设穷举比对（healpy 天球位置为真值、wcslib 给数组索引的天球位置）：`row=511−x, col=y` 最大偏差 **0.004″**（CD 矩阵舍入量级），`row=x, col=y` 平均 3522″，其余两种 3566″ / 5407″。另用**不依赖 WCS** 的父子 tile 规则做纯图像检验：按 `row=511−x, col=y` 把 4 个子 tile 拼回父 tile，与真实父 tile 的秩相关 `+1.0000`，其余假设落在平移对照的噪声基。

**审稿推理链的断点**：它把「x 向北增」与「FITS 行号北向增」直接相消，得出行号随 x 递减 —— 这一步本身对，但它把公式里的 `x` 当成了**第一轴**。FITS 的第一轴是 `NAXIS1`（列）、行是 `NAXIS2`；在 `index = (511 − x)·512 + y` 里步长为 1 的项是 `y` ⇒ **y 才是列、`511 − x` 才是行**。G&C Paper I §5.1 的「second axis increasing upwards」说的正是**行轴**，而面内 `x` 对应的也是行轴 ⇒ 行号随 `x` 递减，`row = 511 − x`。两者同轴，不是镜像。

**我据此做的改动**（不是「不改」，是「不改公式、改证据」）：

- 公式一字不动，补上 `FITS 第一轴（列）= y , FITS 第二轴（行）= 511 − x` 一行，把轴对应写明。
- 采纳对抗复核发现的**真实表述缺陷**：§3.1 原先把平面像素坐标也记作 `x, y`（`x` = 第一轴 = 列），与面内 `x, y`（`x` = 第二轴 = 行）**同名反义** —— 这几乎肯定是审稿误判的诱因。已把平面符号改为 `(i, j)` 并加一句「两套符号不混用」。
- 把「该映射由外部 HiPS 生成工具族冻结」换成可核对的一手链：位序（Górski 式 13/14 + HEALPix 位压缩掩码 `0x5555…`）、面内方向（Górski §5.2，明确声明**该文对文件布局完全沉默、不锚定 FITS 行方向**）、FITS 行方向（G&C Paper I §5.1 + REC-HIPS-1.0 §4.2.1.3 注）、列方向（标准只写「第一轴向右」，本链按天文显示约定取右为西）、实测对拍（上表）。
- 写明**事实标准而非规范强制**：REC-HIPS-1.0 不定义面内 `(x,y)→(row,col)`，HEALPix 库与 astropy-healpix 都不提供该函数，故由 HiPS 生产层冻结。

### 2.4 Zackay & Ofek 转述漏归一化分母（3-03）—— 一手核对，审稿正确

**改前**（`UNIFIED_SCIENCE_MODEL.md`）：
> 在独立测量 `θ_j = μ_j T + ε_j`、`Var(ε_j) = σ_j²` 下，似然比关于 `T` 线性，其充分统计量权重恰为 `1/σ_j²`，最大信噪比估计量为 `Σ_j μ_j θ_j / σ_j²`。

**改后**：
> ```text
> S = Σ_j (μ_j / σ_j²) · θ_j            似然比线性于 T，其权重是 μ_j/σ_j²，不是 1/σ_j²
> I = Σ_j μ_j² / σ_j²                   S 在 H0 下的方差
> T̃  = ( Σ_j μ_j θ_j / σ_j² ) / ( Σ_j μ_j² / σ_j² ) = S / I        最大信噪比估计量
> ```
> 分母 `I` 不可省：它把 `S` 归一成对 `T` 无偏的估计量……这与上式在「权重 = 逆方差乘以响应」上一致——`μ_j` 正是一个待估源在第 `j` 帧的相对响应。

**一手证据**（我自己 `curl -sSL https://arxiv.org/pdf/1512.06872 | pdftotext -layout` 取全文逐字）：

| 位置 | 原文 |
|---|---|
| 式 (17) | `θ_j = μ_j T + ε_j` |
| 式 (18) | `T̃ = (Σ_j μ_j θ_j/σ_j²) / (Σ_j μ_j²/σ_j²) = S / I` |
| 式 (9) | `S = Σ_j μ_j/σ_j² · X_j` |
| 式 (11) | `V[S\|H0] = Σ_j μ_j²/σ_j² ≡ I` |

审稿的两点（权重是 `μ_j/σ_j²`、估计量是 `S/I`）**逐字成立**。我补的第三点是把 `μ_j` 与本分册的 `a_k` 对上——`μ_j` 就是待估源在第 `j` 帧的相对响应，正是 `a_k P_kᵀC_k⁻¹P_k` 的形状。

---

## 3 我推翻的审稿判定（保留原判，不抹除）

| # | 审稿原判 | 我的裁定 | 依据 |
|---|---|---|---|
| **3-13 / 5-G** | HiPS tile `FITS index` 行项 `(511 − x)` 是南北镜像，应改 `x`，「本车道最重的一条结构/公式缺陷」 | **审稿判错，公式正确** | 三源闭合 + 真实 tile 逐像元对拍（最大 0.004″ vs 三种镜像假设 3522–5407″）+ 不依赖 WCS 的父子 tile 秩相关 `+1.0000`。审稿把 FITS 第一轴误当作 `x`；实际第一轴是 `y`。见 §2.3 |
| **4-13** | 65535 的取证来源 `github.com/PixInsight/PCL` 返回 404、「两个独立子代理各自核对到同一结论」、文档所钉 commit 与 `NormalizeSamples` 无法核实，应换成可解析来源或登记 UNRESOLVED | **审稿判错；文档三处小错仍需改** | PCL 不在 GitHub 而在 **GitLab**：`git ls-remote https://gitlab.com/pixinsight/PCL` → exit 0，`5a3902196a7d7a701385a7113cbdce2976ae1a85 HEAD`；`…/-/raw/<sha>/src/pcl/XISFReader.cpp` → HTTP 200。文档钉的 SHA、路径、函数名**全部正确**。真错三处：host、许可名（Version 2.0，非 2.0.1）、**「映射除数」应为「乘数」**（`scale = MaxSampleValue()/range`）。三处已改 |
| **4-16 / 5-13** | `CCD_DEFECT.md:186` 自标「全文未取」已过时；该文是开放获取，`:37` 的「个位数百分比」无定量支撑 | **审稿判错，文档结论正确** | 全文取到（CC-BY，HTTP 200）：摘要逐字 "a fraction of **2.7%** of the individual exposures with a typical exposure time of **11 minutes** are crossed by satellites"；正文 "an average fraction of **2.7 ± 0.2%**"、"satellite trails **traverse the entire field of view (FoV)** … in most cases, appear as straight lines"。文档「个位数百分比量级」「多呈贯穿整个视场的直线」**逐字忠实**。真缺陷只是自标核验状态过时——已把 `2.7 ± 0.2% @ 11 min` 写进正文并更新核验状态 |
| **2-17** | 代码注释把 SIP 量纲矛盾指向 `docs/science/ASTROMETRY.md §5`，而该节无对应内容；ASTROMETRY 未声明本链 A/B 是像素域约定 | **前提被推翻；代码注释本身才是错的** | Shupe et al. 2005 p.492 式 (1)–(3)：`(x,y)ᵀ = CD·(u+f, v+g)ᵀ`，`f` 先与像元 `u` **相加**⇒ `[A_pq] = pixel^(1−p−q)`，**与本链约定相同，不差一个 CD 量纲**。ASTROMETRY 原 §2.2/§3.1 已写了该量纲，只是不精确（说成「相乘给出度」，应为「先相加再乘 CD」）。已改文档口径并把它写成显式声明；**代码注释的错误与 13 处旧路径登记给后续任务** |
| **2-08** | `point_information` 的 `ADU⁻²` 与同表 `ivar` 的 `sr²/ADU²` 构成冲突 | **降级：无冲突，缺族限定语** | `W = a² Pᵀ C⁻¹ P` 的量纲完全由 `d_k` 所在承载面决定：面亮度面 ⇒ `sr²/ADU²`，帧面点源面 ⇒ `ADU⁻²`。`DATA_SEMANTICS.md:175` 已把点源族冻结，两者本就一致。已按降级补族限定语，**未改单位** |
| **2-15** | `1.230310` 未声明参数化口径，读者按标准 Moffat 公式会算出 0.869959 | **降级：闭式已在仓内，只缺本册来源列** | `PSF.md` 全宽半高节与 `dpsf_psf.h` 都已写明闭式 `FWHM = 2√2·σ·√(2^{1/4}−1) = 1.230307652590102·σ`（`α = √2σ`）。已把闭式与口径写进本册正文与常数表来源列 |
| **3-09** | 文档「约 `1e-46`」把 `α²` 量级误当换算后地板值，差约 12 个量级；应改 `1.297e-58 … 3.610e-57` | **部分推翻：审稿的替代数字是错的** | `NUMERIC.md:50` 给 `α ∈ [1.1387e-17, 6.0083e-17]` ⇒ `α² ∈ [1.2966e-34, 3.6100e-33]`，`1e-12·α² ∈ [1.2966e-46, 3.6100e-45]` —— **文档的「约 1e-46」本来就是区间下端，是对的**；审稿把 `1e-12` 减了两次。真正的缺陷只有「**精确**下溢为 0」的无条件表述（33 例中 1 例是非零次正规数 `4.204e-45`）。已按此改；`NUMERIC.md:72` 的符号误标登记给 engineering 车道 |

---

## 4 需代码／合同侧订正的问题（本单不改代码，逐条登记）

| # | 位置 | 问题 | 建议改法 |
|---|---|---|---|
| 1-05 | `docs/science/algorithms/CALIBRATION_ALGORITHMS.md:386,465,470,489,513`、`lib/algorithms/calibration/README.md:53,58,135,142`、`lib/algorithms/calibration/src/master_generator.cpp:212` | 8 处 `SCI §3a/§4/§7/§8/§9a/§11/§15` 悬空节锚指向本车道正本；现 `CALIBRATION.md` 只有 7 章，无这些小节 | 按 migration 映射改指新正本的自然语言小节名，去掉 § 锚 |
| 1-06 | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1107,1368,1522,1592`、`aio_pipeline.h:328-330`、`aio_hips.h:44,218`、`module_adapters.cpp:2543,4575,8668,8882,8946,9369,12238,13498`、`gaia_xpsd_client/*`、`eng/packaging/config/defaults.json:966`、`eng/contracts/data/unified_object_registry.json:206,244`、`eng/contracts/schemas/phase_config_normalize.schema.json:225`、`frame_snr.schema.json` 的 `propertyNames.not.description` | 一整族 `DATA_SEMANTICS §4a/§8/§8.2/§10.2/§10.3/§10.5/§11.2/§12.4/§13.4/§16.1/§30.4/§31.7/§31.8` 悬空节锚（现文档 7 章、`κ` 在 `CALIBRATION.md` 不在 `DATA_SEMANTICS.md`），另有 `κ` 处方应改指 `CCD_DEFECT.md` | 同上；建议一次性脚本化重写并加机器门 |
| 2-11 | `eng/contracts/schemas/unified/frame_snr.schema.json:5,301` | 合同字面写「真实信号/噪声比，**不受天光影响**」，与最高设计的单调性论断方向相反（`ACSD_DESIGN.md:106,110`：`N_sky/g` 在 `N_e` 内 ⇒ 固定源通量下天光越亮 SNR 越低） | 改为「天光只经散粒噪声进入分母；对参考电平不敏感、对天光电平敏感，在散粒主导且未饱和的成立域内随天光单调下降」 |
| 2-14 侧 | `docs/science/algorithms/CALIBRATION_ALGORITHMS.md:465-471` | 算法分册的不变量编号表仍是 I1–I6，缺「偏置参与」「缩放因子参与」「约定等价」三条；本车道侧已补到九条 | 把算法侧同步到与本册同名的九条 |
| 3-11 侧 | `eng/packaging/config/defaults.json:961,966,968` | `source` 字段复述了同一条已被推翻的信息论断言（「不得声称比一次独立测量携带更多信息 ⇒ var ≥ σ² ⇒ κ ≥ 1/Σw² = 2」），与本车道新写的非重复计数记账约定打架 | 与 `lib/algorithms/calibration/include/astro_calibration.h:288-292` 已有的正确表述对齐 |
| 2-17 侧 | `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:364-365`（及 `ipv_wcs.h:63`、`p3_wcs.cpp:37`、`gaia_client.h:15`、`platesolve/README.md:7,71`、`platesolve/module.yaml:74`、`platesolve/memory.md:69`、`eng/tools/astrometry/closure_metric.py:6,47`、`eng/tools/astrometry/README.md:8,17`、`eng/tools/quality/v19r3_traceability.py:230`） | 注释声称「SIP 标准的 A_ij 定义在中间世界坐标（度）上、量纲 deg/像素^(i+j)，与本数组相差一个 CD 量纲」——**该断言本身是错的**（Shupe 2005 式 1–3 给出 `[A_pq] = pixel^(1−p−q)`）；另全仓 13 处 `docs/science/ASTROMETRY.md` 是失效旧路径 | 删除 `:364-365` 两行；13 处旧路径改 `docs/science/detection/ASTROMETRY.md`；注意 `v19r3_traceability.py:230` 改路径会影响该检查器结果，须一并跑 |
| 5-07 侧 | 已在本车道侧完成合并与重编号；算法卷若也引 ACS 手册条目需同步 | — | 无 |

---

## 5 需权威补充才能定的问题

| # | 问题 | 需要什么 | 现登记状态 |
|---|---|---|---|
| 4-08 | 星表 `magG/magBP/magRP` 的 `×0.001 − 1.5` 量化出处 | PixInsight XPSD 客户端规范的一手说明（`pixinsight.com/distribution/xpsd/` 返回 HTTP 406，取不到）；**或** DPAC/IDT 压缩星等的官方说明 | 已改归 XPSD 存储约定并给出仓内 `文件:行`；ESA Gaia DR3 侧已**反证**（`gaiadr3.gaia_source.phot_*_mean_mag` 为 `float`，官方定义为「由该波段平均通量加 Vega 零点换算」）。按代码级证据登记，**不向外交叉换算** |
| 4-13（新） | XISF→ADU 的换算因子 65535 还是 65536 | 负责人裁决。仓内一份真实母版的回归反解给 `ADU/65536`，相对差 `1.5e-5`（1 LSB）；`65535` 是 UInt16 可表示域满量程，`65536` 是 `BZERO=32768` 有符号存储下的自然满量程，而本仓亮场正是后者 | 已在 `CALIBRATION.md` 写成待裁决项；冻结前按声明制消费、逐帧登记声明值 |
| 3-09 侧 | `NUMERIC.md:72` 把「换算后地板」标成 `α²`；`:49-50` 引的 `run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json` **不存在** | engineering 车道订正符号；补上 33 帧 `α` 实测证据文件 | 已在本车道侧按正确读法改写并给出实测区间 |
| 4-19 侧 | HiPS tile 行列映射的**跨生成器一致性** | 第二个独立生产者的 FITS tile（本次搜索引擎不可用，只验证了 alasky 上唯一的 2MASS FITS HiPS）；另需确认是否存在故意写北向上 FITS tile 的生产者 | 已在正文写明「事实标准而非规范强制」「在取得第二个生成器之前只在已验证的那一个上被证实」 |
| 1-09 侧 | `masonry` 是否真是第三个注入面 | 该词只出现在 unified 三份与目录 README，**在 `ACSD_DESIGN.md`、`engineering/`、`detail/`、`lib/`、`eng/` 全无**；而权威链只有 normalize / mosaic / export 三个命令 | 已按笔误改为 `mosaic`；架构命名需集成/导出侧确认 |
| 4-22 侧 | 仓内存在相近但不合召回协议的测量（`run/SCI-FIX-STARPSF-01/code/e2_detect_recall.cpp`：固定 seed、但对星做了随机亚像元抖动，与结果头自述的「峰值对齐像元中心」矛盾；样本量 3×25=75/档 < 协议的 1000/档） | 召回阈表的复跑入口仍不在仓内 | 保留审稿的正面登记，本车道未改 |
| §4.1 | 噪声常数争议（`DISPUTE_RESOLUTION.md` 属 noise_snr/顶层，**不在本车道**） | 负责人定 `target`（1.2% vs 1.5%）；另需 `run/GATE-DERIVE-01/REPORT.md` 之外的一手来源钉 `1.36046` vs `1.361` | 整条登记给前台/对应车道，本单不越权 |
| 1-18 = 2-18 | `docs/ACSD_DESIGN.md:562` 附录 A 术语表漏 `depth_m5` | 顶层文档改动须负责人批准 | 登记给前台 |

---

## 6 文献核对

### 6.1 核到原文（逐条落进文档）

| 文献 | 核对方式 | 核到的内容 |
|---|---|---|
| Greisen & Calabretta 2002, Paper I（arXiv:astro-ph/0207407） | 全文 `pdftotext` | §5.1 图像显示约定原文；页码经 Crossref + OpenAlex 双源核为 **1061–1075**（三处已改）。⚠️ **arXiv 预印本无期刊页码**（页眉 `A&A manuscript no. wcs`，内部分页 1–16），不能用它核页码 |
| Greisen & Calabretta 2002, Paper II（arXiv:astro-ph/0207413） | 全文 `pdftotext` | §2.2「the gnomonic projection **diverges at the equator**」；Fig. 8「Gnomonic (TAN) projection; diverges at θ = 0」；§2.3 天顶族 `(φ₀,θ₀)=(0°,90°)` ⇒ CRVAL 是原生极点；式 (54)(55) |
| Górski et al. 2005（arXiv:astro-ph/0409513） | 全文 `pdftotext` + healpy 1.20.1 实测 | §5.2 散文（southernmost / North-East / North-West）、式 (13)(14)(15)(17)；该文对 FITS 布局**零陈述**（已在文档中明确它不锚定行方向） |
| IVOA HiPS REC-HIPS-1.0-20170519 | 全文 `pdftotext`（1326 行） | §4.1 目录布局（**must**）、§4.2.1 `512×512` 是 "a good compromise"、§4.2.1.3 注「JPEG/PNG top->down」、§4.4.1 `hips_pixel_scale` 单位为度；**全文不定义面内 (x,y)→(row,col)** |
| Zackay & Ofek 2017（arXiv:1512.06872） | 全文 `pdftotext -layout` | 式 (9)(11)(17)(18) 逐字；刊名经 Crossref 核为「How to COAAD Images. I. … of Point Sources Using …」 |
| Shupe et al. 2005 | CaltechAUTHORS 存档全文（逐页页脚 491–495） | 6 作者；题名「The SIP Convention for Representing Distortion in FITS Image Headers」；p.492 式 (1)–(3) ⇒ `[A_pq] = pixel^(1−p−q)` |
| Naylor 1998 / Zackay 2017 / van Dokkum & Pasha / Desai / Massey / Erben / Górski / Greisen | Crossref API | Naylor `296(2)`；Pasha, **Imad** ⇒ `Pasha I.`；Desai, **S.** ⇒ `Desai S.`；Massey 题名补 `Advanced Camera for Surveys`；Erben 页码 `432–464` 且题名补 `of multi-chip cameras`；Górski 刊名 `…distributed on the sphere` |
| Rousseeuw & Croux 1993（JASA） | OpenAlex 摘要 | 摘要只印 4 位 `1.4826` ⇒ 「其权威全精度值」不成立（已删） |
| Croux & Rousseeuw 1992（*Computational Statistics*） | Crossref + 免费 PDF（HTTP 200，18 页） | 卷 **7**（仓内原写「1」）、页 411–428、DOI `10.1007/978-3-662-26811-7_58`（**新条目，仓内原引的 `10.1016/0167-9473(92)90079-9` 返回 404**） |
| Kruk et al. 2023（Nature Astronomy） | 开放获取全文（HTTP 200） | `2.7 ± 0.2% @ 11 min`、`traverse the entire field of view`、多呈直线（见 §3 推翻条） |
| Desai et al. 2016（arXiv:1601.07182） | 全文 | §4.2 逆方差权重 + 高斯抽样；§4.1 插值可行性；§3.2 坏列由圆顶平场与偏置离群像元标定。全文仅 1 处 `diffraction`，且是把 diffraction spikes 列为 **persistent** 类、从未作「成像伪影」例证 ⇒ 删 |
| PixInsight Class Library | GitLab 活仓逐字 | `git ls-remote https://gitlab.com/pixinsight/PCL` → exit 0；`XISFReader.cpp` 的 `NormalizeSamples` / `scale = MaxSampleValue()/range`；`PixelTraits.h` 的 `UInt16PixelTraits::MaxSampleValue()`；许可 `Version 2.0` |
| XISF 1.0 规范正文 | `pixinsight.com/doc/docs/XISF-1.0-spec/XISF-1.0-spec.html`（HTTP 200） | §8.5.5「representable range is … the range of pixel sample values that can be represented on display devices」，lower/upper 即黑点/白点。⚠️ **XISF 1.0 没有 PDF**（`dist/xisf/xisf-1.0.pdf` → 404） |
| JCGM 100:2008（GUM） | BIPM 发布版 `pdftotext` | §4.2.3 NOTE 2 关于方差/标准差量纲**逐字命中**；线性传播律在 **§5.1.2 式 (10)** 与 §5.2.2 式 (13)，文档两条标度律是其 `N = 1` 特例，**不是逐字引文** |
| FITS Standard 4.0 | `pdftotext` | §4.3「Note that, per IAU convention, **case is significant throughout**」（在 §4.3，不在 §4.3.1）；Table 3 `sr`、Table 4 `adu`、§4.4.2.5 `BZERO = 32768 and BSCALE = 1` 是 "a specific, common example" |
| EMVA 1288 Release 4.0 **Linear** | 官方 PDF（HTTP 200） | §2.4 Noise Model 式 (14)(15) 含 `signal independent` 的 `σ_d²`；**General 模块无噪声模型节**；旧 URL `emva-1288/`（少一 v）404 |
| Gaia DR3 | ESA Gaia Archive TAP 元数据（HTTP 200） | `gaiadr3.gaia_source.phot_*_mean_mag` 为 `float`，官方定义为「由该波段平均通量加 Vega 制星等零点换算」⇒ **`×0.001 − 1.5` 不是 Gaia DR3 口径** |

### 6.2 核对不到 / 明确登记（不凭印象填）

| 项 | 状态 |
|---|---|
| `×0.001 − 1.5` 量化本身的一手出处 | PixInsight 返回 406，Web 搜索无可用结果 ⇒ 仅代码级证据（`gaia_client.c:1827/1962/2106` 的 `uint16` 解码 + 声明的上游 MIT 客户端名）。按代码级证据登记 |
| HiPS tile 行列映射的规范条文 | REC 不定义 ⇒ 事实标准，非规范强制 |
| 第二个 HiPS FITS 生产者的 tile | 搜索引擎不可用，未取得 ⇒ 跨生成器一致性未验证 |
| `masonry` 架构命名 | 上层全无此词 ⇒ 需集成/导出侧确认 |
| `run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json`（`NUMERIC.md:49-50` 引用的 33 帧 α 实测） | **文件不存在** ⇒ 端点算术可复现，33 个逐值无法审计 |
| 33 帧 `α²` 逐值 | 无可定位证据文件；只核到端点算术自洽 |
| CDS Hipsgen 的 MAPTILES 逐像素冻结证据文件 | `lib/algorithms/shared/healpix/` 无 oracle 数据文件；只有代码注释与第三方声明（第三方声明未提 Hipsgen） |
| CR92 免费 PDF 的式 (4) 与 `b_n` 表逐字 | PDF 下载成功但本机无文本层，未逐字复核（书目层已核） |
| `ADU/65535` vs `ADU/65536` | 已如实写成待裁决项，未自行定值 |
| RC93 出版方页面 | tandfonline 403，改走 OpenAlex 摘要；RC93 与 CR92 全文均未逐页读 |

---

## 7 索引变更与真解析器验证

**变更**：`docs/DOCUMENT_INDEX.yaml` 中属本批的 7 条，只改 `duty`（1 条）与 `notes`（6 条），补上本轮落定的口径要点：`κ=2` 的依据不是信息论下界、坏列七步判据无复跑脚本、HiPS tile 行列方向已对拍、单位串大写 `ADU` 是显式偏离、发散区在原生赤道、桥接门三组常数互锁、`flat`/`flat_norm` 符号约定、暗流截距失配的前提、`s_log` 符号与冻结面归属、阈值等效倍数按生产核冻结、权威路径为星表引导检测、帧间相关用一般分块协方差、点源族与面亮度族由承载面决定。未新增/删除条目，未动 `path`/`status`/`upstream`（避免与并发的 detail/engineering 车道撞车）。

**真解析器验证输出**（`python3` + `yaml.safe_load`，非文本 grep）：

```text
yaml.safe_load OK; top keys: ['doc_index']
schema_rev: 3
active entries: 148  archived entries: 0
duplicate paths: []
active paths missing on disk: []
docs human-readable files not registered: []
  AGENTS.md: registered
  README.md: registered
  VERSION: registered
  docs/ACSD_DESIGN.md: registered
  docs/DOCUMENT_INDEX.yaml: registered
archived with wrong status: []
  docs/science/calibration/CCD_DEFECT.md: fields OK; notes=yes
  docs/science/unified/DATA_SEMANTICS.md: fields OK; notes=yes
  docs/science/detection/ASTROMETRY.md: fields OK; notes=yes
  docs/science/calibration/CALIBRATION.md: fields OK; notes=yes
  docs/science/unified/SCIENCE_SCOPE.md: fields OK; notes=yes
  docs/science/detection/STAR_DETECTION.md: fields OK; notes=yes
  docs/science/unified/UNIFIED_SCIENCE_MODEL.md: fields OK; notes=yes
```

**本车道引用完整性复验**（真解析：正文方括号编号 vs 文末条目，脚本比对）：

```text
docs/science/calibration/CALIBRATION.md: PASS
docs/science/calibration/CCD_DEFECT.md: PASS
docs/science/detection/ASTROMETRY.md: PASS
docs/science/detection/STAR_DETECTION.md: PASS
docs/science/unified/DATA_SEMANTICS.md: PASS
docs/science/unified/SCIENCE_SCOPE.md: PASS
docs/science/unified/UNIFIED_SCIENCE_MODEL.md: PASS
--- 7/7 通过（无悬空引用、无孤儿条目、编号连续）
```

**章结构复验**（标准 02 §2.1 七段式）：

```text
CALIBRATION.md / CCD_DEFECT.md / ASTROMETRY.md / STAR_DETECTION.md /
DATA_SEMANTICS.md / UNIFIED_SCIENCE_MODEL.md: 7 章（主题与目标 · 物理模型 · 公式与推导 ·
  参数与常数 · 判据与误差 · 与上下游的关系 · 参考文献与参考代码）
SCIENCE_SCOPE.md: 8 章（多出的第 5 章是本册专有的「假设与适用域」，原第 7 章的四节
  「有效域 / 失效条件 / 不产出 / 参考文献」已拆入第 6 章与第 8 章）
```

**编码与文风复验**：本车道 10 份 md 中 U+FFFD 计数 0；`旧版/作废/曾是/原来` 0 命中；`见 §` / `SCI §` / `DATA_SEMANTICS §` 0 命中；`20\d\d-\d\d-\d\d` 0 命中。

---

## 8 我自己动手重推的推导（不采信审稿结论的部分）

| 量 | 我的推导 | 结果 |
|---|---|---|
| 平场 floor 相对误差 | `cal/cal_true = (num/0.1)/(num/f) = f/0.1` | `f/0.1 − 1`；f=0.05/0.02/0.01 → −50/−80/−90%，与文档表一致 |
| 面亮度星等符号 | `F = signal·Ω_ref` 代入 `m = ZP − 2.5log10 F` | `Ω_ref` 项为**减号**；1 arcsec² 处差 **53.1443 mag**；`Ω_ref = 1 sr` 时两式退化相同 |
| `blockdiag` 自洽性 | `C = blockdiag(C_1..C_K)` ⇒ `C⁻¹` 块对角 ⇒ `PᵀC⁻¹P = Σ_k P_kᵀC_k⁻¹P_k` | 恒等成立，文档同句的「不等于 `Σ_k W_k`」不成立 |
| `cd_inv` 维度 | `inv(L/3600) = 3600·inv(L)`（逐元素）；`det` 口径才是 `3600²` | 审稿正确（放大不是缩小）；生产注释 `ipv_wcs.cpp:340` 写的也是 3600 |
| `sec²Δ` 与视场 | `1/cos²(15°) = 1.0718`；`sec² = 1.03046` ⇒ 半角 `9.9000°`、全视场 `19.80°` | 括号里的「半角 15 度」无来源；仓内来源是 `FOV ≤ 20°` |
| 桥接门三组常数互锁 | `ε = C_env·u·sec²Δ/s_rad`，`s_rad = s″/206264.806` | `C=78, s=1.79e-3″/px` ⇒ `1.028e-6 px`（≈ 全域门 1e-6）；`C=128, s=2.93e-3″/px` ⇒ `1.031e-6 px`；`C=128, s=0.9″/px` ⇒ `3.356e-9 px`，对紧门 1e-8 余量 **2.98 倍**（与仓内注释的 2.98× 吻合）。5 倍 = `0.9/0.179`；2.8 倍 = 独立区间包络 `1.78e-8 px @0.18″/px` 按 `1/s` 外推到 0.9″/px 得 **2.809** |
| `κ` 的方差 | `Var(ŷ) = Σ_j w_j²·var`（独立 ⇒ 交叉项 0），MC `Var(mean of 2 iid) = 0.500247` vs 理论 `0.5` | `κ = 1` 才是插值量本身的正确方差 ⇒「不得声称比一次独立测量携带更多信息」在数学上为假；`κ = 2` 是非重复计数的记账约定 |
| YvV 生产核范数 | 按 `sdet_image.cpp:106-123` 独立复现递推（B=0.227285251090804、b1=1.2327675485381264、b2=−0.5533139546361017、b3=0.09326115500717135，DC 增益 1），单位冲激响应 `Σk² = 0.133174181553`（N=41/81/161/321/641 收敛到同值），可分离二维 `ΣK² = 0.017735362632` ⇒ `‖k‖₂ = 0.133174181553` | `1/‖k‖₂ = 7.5089630`、`5/‖k‖₂ = 37.5448149`，比连续高斯值高 **+5.9120%** |
| 差分噪声 | 行内相邻列差分 RMS = `1.413231`（σ=1 理论 √2），跨相邻行差分 RMS = `1.413753` | 差分 RMS = `√2·σ`，代码的 `×0.70710678` 把 `bgnoise` 还原成单像元 `σ_n` |
| `1/Φ⁻¹(3/4)` | `scipy.stats.norm.ppf(0.75) = 0.6744897501960817` ⇒ 倒数 `1.482602218505602` | 16 位逐位相同 |
| `CD` 行列假设穷举 | HiPS tile 的 `Npix → face/order-K cell → 原生 nest (x,y)` 复原后，四种行列假设与 healpy 天球位置逐点比对 | 文档式最大 0.004″；三种镜像 3522–5407″ |

---

## 9 自证段

### 9.1 派发了哪些子代理、否决了哪些

派发 **5 个**子代理，全部只读取证、零 `/workspace` 写入、零 git 写：

| # | 范围 | 回报 | 我的处置 |
|---|---|---|---|
| 1 | **HiPS 像素序对抗复核**（专攻推翻我的裁决） | 三种独立方法全部失败，裁决成立；并给出比我的更强的证据（14 个 tile、全部 12 面、逐像元约 367 万点，文档式最大 **0.004″**；另有**不依赖 WCS** 的父子 tile 秩相关 `+1.0000`）。另报出**我链条里三处需收紧**：Górski 对 FITS 布局零陈述不可作行方向权威；我报的 ~7.9″ 是探针取整、穷举实测 0.004″；并发现**真实表述缺陷**——§3.1 把平面像素坐标也记作 `x,y`（x = 列）与面内 `x,y`（x = 行）同名反义 | **全部采纳**：符号改为 `(i,j)`；明确写出 Górski 只锚定面内方向；实测数字换成 0.004″。**保留审稿的原始判定不抹除**，同时把它登记为本车道最重的一条「已推翻」 |
| 2 | `ASTROMETRY.md` 全项 + 生产代码取证 | A1 页码、A2 Shupe、A3 `3600²`、A5 发散区、A6 注释指向 §5、A8 `C_env` 全部 CONFIRMED；A4 PARTLY；A7 给出 5×/2.8× 的真实推导；A10 指出桥接链条的措辞隐患 | **全部采纳**，并据此把 `C_env`、FOV、两组余量的来源全部写进正文。**它还推翻了审稿 2-17 的前提**（SIP 标准的 A/B 就是像元域量纲，与本仓一致），我据此把代码注释的错误登记给后续任务 |
| 3 | `CALIBRATION.md` + `CCD_DEFECT.md` 全项 | **推翻审稿 4-13**（PCL 在 GitLab，SHA/路径/函数名全对）；B4 给出 `(b_bias − b_light)(K − 1)` 的精确形式并**当场查出我第一版展开式的代数错误**；B7 指出我写的「逐条对齐」是假声明；B12 指出 4-16 审稿判错 | **全部采纳**。它当场抓到我落的 `:123` 展开式多算了一次 `b1·(K−1)` 且多一个 `+K·I_d·t_d`（数值验算差 1800）——**我立即按它给的正确展开重写**，并在自证里保留这次自查。已撤回「逐条对齐」假声明、改正「七条→九条」 |
| 4 | `unified` 三份全项 | C1 `blockdiag` WRONG、C10 两处书目错、C13(c) 三态表 bug、C14 mojibake 全部 CONFIRMED；**并纠正了审稿 3-09 的替代数字**（把 `1e-12` 减了两次）；C11 指出 5σ 深度正本在 `NOISE_SNR.md` §3.3 而非 `:237` | **全部采纳**，并按它对 3-09 的纠正把该条**部分推翻**。C14 第 1 项它「反驳」审稿的措辞有误（审稿说的是「不在参考文献表里」，子代理复述成了别的意思），我按原审稿判读处理 |
| 5 | `STAR_DETECTION.md` 全项 | D1/D2/D3/D5 CONFIRMED（**并独立复算出与我相同的 `7.5089630 / 37.5448149`**）；D7 **判审稿 4-03 错**（数据在仓内，缺的只是指向）；D8 指出审稿 4-04 的前半「零数字」不成立、后半「尖峰形比例」确实无任何数字；D4 把 2-15 降级 | **全部采纳**。它额外报出 D2 的量化后果（按文档字面实现阈值高 41.4%），我据此把 §3.1 的强制声明扩成「σ 属于哪幅图 **+** σ 由哪个估计器给出」两条 |

**否决/改写掉的子代理结论**（保留其正确部分）：#2 提出的「删掉 `ipv_wcs.cpp:364-365` 两行」属代码写入，本单不越界，改为登记；#3 提出的「同步改 `CALIBRATION_ALGORITHMS.md` 到九条」属他车道文件，改为登记；#3 报告末尾自行承认它拿不到 CR92 PDF 的文本层，我把该条列入「核对不到」，没有采信它的逐字断言；#4 自我「反驳」的 C14 第 1 项我判定为它误读审稿原句，按审稿原判处理。

### 9.2 并发与协作

- 一个取证子代理在我编辑期间检测到 `CALIBRATION.md` 正在被改写并发出协调告警——那是我本人。它正确地未写任何文件，全程以内容而非行号取证；我按它的指正修正了自己的代数错误与两处不一致表述。
- `docs/detail/`、`docs/engineering/`、`docs/science/{noise_snr,psf,resample,drizzle}/` 与索引的其余条目在本单期间由其他车道改动。本单只写 7 份本车道文档 + 索引的 7 条 `notes`/`duty`，索引编辑前后各跑一次 `yaml.safe_load` 与「path 唯一 / 磁盘存在 / `docs/**` 全覆盖」检查，全程通过。
- 全程**零 git 写操作**（无 add/commit/checkout/reset/stash/tag），中文路径一律 `git -c core.quotepath=false`。工作树改动留给前台统一提交。

### 9.3 没做的事

- 零编译、零 ctest/pytest、零仓内脚本执行（YvV 复现、HiPS 对拍等复算件全部写在 `/tmp`：`fix_*.py`、`acs_derive` 系列、`yvv` 复现、HiPS tile 下载与穷举比对）。
- 没有为「让它看起来能复跑」而虚构任何复现命令：`CALIBRATION.md` §5.3 的最小二乘表与 `CCD_DEFECT.md` §3.1 的七条读数，仓内确实没有对应的复跑脚本，我写的是**如实登记**（数据文件齐备、解码器现成、七步判据缺席），不是编造命令。
- 没有为 `×0.001 − 1.5`、65535/65536、HiPS 跨生成器一致性、`masonry` 架构命名填任何我核不到的答案。