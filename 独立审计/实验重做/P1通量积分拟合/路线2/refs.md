# P1 通量积分拟合 · 路线 2 · 文献核验记录（refs.md）

**身份**：三路独立研究路线之第 2 路，独立取证、不与他路通信。
**日期**：2026-09-26。**核验工具**：Crossref REST API（api.crossref.org/works）、arXiv API、一手网页抓取（Gaia DR3 官方文档、statsmodels/MASS 源码 raw 文件、PMC）。
**状态字段**：RESOLVED（DOI/原文/源码逐字核对）／UNRESOLVED（查不到，不编造）／PARTIAL（存在性核实、关键数值无法在线核对）。

---

## A. 一手文献核验（RESOLVED）

### A1. Beaton & Tukey 1974 — Tukey biweight 出处（S1 / 01-C11 书目缺）

| 字段 | 值 |
|---|---|
| DOI | 10.1080/00401706.1974.10489171 |
| 题名 | The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data |
| 作者 | G. E. P. Box 席作者席：A. E. Beaton, J. W. Tukey |
| 年份/刊 | 1974, Technometrics 16(2), 147–185 |
| 核验方式 | Crossref works API 回包逐字段比对 |
| 用途 | c=4.685 的 biweight 权函数历史出处；PHOTOMETRIC_FIT.md §参考文献行 1 已登记，本路独立复核 DOI 真实指向 |

### A2. Holland & Welsch 1977 — IRLS 权函数表中 bisquare c=4.685 的表格出处（S1）

| 字段 | 值 |
|---|---|
| DOI | 10.1080/03610927708827533 |
| 题名 | Robust regression using iteratively reweighted least-squares |
| 作者 | P. W. Holland, R. E. Welsch |
| 年份/刊 | 1977, Communications in Statistics – Theory and Methods 6(9), 813–827 |
| 核验方式 | Crossref works API 题名检索回包第一命中，作者/年份/刊名一致 |
| 适用边界 | 该文为 IRLS 权函数表出处；**不得**给它加挂 MAD→σ 系数 1.345（01/C11 的告诫保留） |

### A3. c=4.685 ⇔ 95% 渐近效率 — 可核验载体（S1）

- statsmodels 源码（raw GitHub 主干）：
  - `statsmodels/robust/norms.py`：`class TukeyBiweight(RobustNorm): def __init__(self, c=4.685)`，docstring 记默认值 4.685。
  - `statsmodels/robust/_tables.py`：`tukeybiweight_eff[0.95] = (4.685065, 0.119414)`（效率→(调参, 崩溃点)）。
- 本路独立复算（exp1）：`ARE(4.685) = 0.9499…`；解 `eff=0.95` 得 `c = 4.685065`（与 statsmodels 表逐位一致）；`ARE(3.882662)=0.9000`、`ARE(5.182361)=0.9662`（对应表中 0.90/0.10 行）。
- **对照发现（与审查员相左的锚修正）**：PHOTOMETRY.md §14 第 1 条以 **PMC6768164** 作为该值"实证核对"载体。本路抓取该 PMC 全文：其为 biweight **位置估计方差 MC 研究**，调参主角是 **c=6**（"Using a biweight scale and a tuning constant of c = 6, the biweight attains an efficiency…"），全文**不含 "4.685"**。⇒ 该实证锚**张冠李戴**，应改引 statsmodels `_tables.py` + Holland & Welsch 1977 表 + 本报告 exp1 闭式复算。

### A4. Rousseeuw & Croux 1993 — MAD 作为稳健尺度量的定位文献（S2）

| 字段 | 值 |
|---|---|
| DOI | 10.1080/01621459.1993.10476408 |
| 题名 | Alternatives to the Median Absolute Deviation |
| 作者 | P. J. Rousseeuw, C. Croux |
| 年份/刊 | 1993, Journal of the American Statistical Association 88(423), 1273–1289 |
| 核验方式 | Crossref works API |
| 用途 | MAD/0.6745 的统计学正典出处；0.6744897501960817 = Φ⁻¹(3/4) 是解析恒等式（exp1 二分法复算，相对差 <1e-16） |

### A5. Croux & Rousseeuw 1992 — MAD 有限样本修正因子 b_n 表（S10）

| 字段 | 值 |
|---|---|
| 书目 | C. Croux, P. J. Rousseeuw (1992), "Time-efficient algorithms for two highly robust estimators of scale", *Computational Statistics* **1** (COMPSTAT'92, Physica-Verlag), 411–428 |
| 关键数值 | p.413 表：b_n：n=3→1.495、4→1.363、5→1.206、9→1.107；n>9→n/(n−0.8) |
| 核验状态 | PARTIAL：章节页码书目信息与 PHOTOMETRIC_FIT.md:299 登记一致、存在性无争议；**表值本身**未能在开放网络取得原文逐字核对，改由本路 MC 独立复算（exp1，200 万次重复/每 n，seed 20260926）：n=3 实测 E[S/σ]=0.672（⇒偏差因子 1.488，文献 1.495，相对差 0.5%）；n=9 实测偏差因子 ≈1.11。表值与 MC 相容 ⇒ 数值腿成立 |
| 用途 | 02 式-3 有限样本边界声明（"n=3 偏低约 1.49 倍"）的文献腿 + 实验腿 |

### A6. Huber & Ronchetti 2009 — "先验尺度"路线与 c/尺度成对约束（S1/S3）

| 字段 | 值 |
|---|---|
| 书目 | P. J. Huber, E. M. Ronchetti (2009), *Robust Statistics*, 2nd ed., Wiley, ISBN 978-0-470-12990-6；§6.4 p.133（位置-尺度同时迭代）、§6.5 p.137（先验尺度 M 估计） |
| 核验状态 | PARTIAL（印刷专著，无开放全文；章节页码与 PHOTOMETRIC_FIT.md:301 登记一致） |
| 交叉佐证 | **R MASS 源码逐字核实**（raw GitHub cran/MASS R/rlm.R）：`method="S", k0 = 1.548`、`if (c0 > 1.548)` 才固定尺度——即"固定尺度路线要求 c>1.548"这一主张可在一手开源实现中逐字验证；statsmodels `tukeybiweight_eff[0.95]=(4.685065, **0.119414**)` 的崩溃点 11.9% 同表可查 |

### A7. Gaia DR3 官方文档 §20.12.4 — XP 采样均值谱网格（S12 / 式-1 适用域）

| 字段 | 值 |
|---|---|
| URL | https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html |
| 原文（逐字抓取） | "All mean spectra are sampled to the same set of absolute wavelength positions, viz. **343 values from 336 to 1020 nm with a step of 2 nm**." |
| 字段单位（逐字） | `flux : mean BP + RP combined spectrum flux (float[] array, **Flux[W m - 2 nm - 1 ]**) Externally-calibrated combined BP and RP flux` |
| 核验方式 | web 抓取 HTML 全文，字符串定位 |
| 用途 | PHOTOMETRY.md §14 第 4 条的官方口径独立复核；式-1 "能量谱辐照度"前提的官方侧依据（官方给能量通量，非光子通量） |

### A8. Gaia DR3 官方文档 §5.4.1 式 (5.41) — VEGAMAG 合成通量积分核（S11 / 式-1 上游）

| 字段 | 值 |
|---|---|
| URL | https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html |
| 原文（逐字抓取） | "in VEGAMAG system the mean energy per wavelength units ⟨f_λ⟩ is calculated as: **⟨f_λ⟩ = ∫ f_λ(λ) S(λ) λ dλ / ∫ S(λ) λ dλ** (5.41)" |
| 同页另有 | "current state-of-the art uncertainty on the 'absolute' calibration scales"（绝对刻度 1% 量级声明） |
| 核验方式 | web 抓取 |
| 用途 | 式-1 的 `λ` 幂次（分子含 `λ` 一次）在官方零点管线中的同型出处；也说明本仓去掉分母（逐帧常数）的合法性 |

### A9. Gaia DR3 主目录论文（星数总计，S6/S5/S7 的计数模型锚）

| 字段 | 值 |
|---|---|
| DOI | 10.1051/0004-6361/202243940 |
| 题名/作者 | Gaia Data Release 3（A&A 674, A1）；Gaia Collaboration: A. Vallenari, A. G. A. Brown, T. Prusti 等 |
| 年份/刊 | 2023, A&A 674, A1 |
| 核验方式 | Crossref works API 回包题名+前五位作者（Vallenari/Brown/Prusti/Arenou/Babusiaux）逐字段一致 |
| 用途 | 计数模型锚点：~1.812×10⁹ 源 / 41252.96 deg² ⇒ 天空平均累计密度 4.39×10⁴ deg⁻²（G≲21） |

### A10. Bessell & Murphy 2012 — 光子计数通带与合成测光（S11 理论腿）

| 字段 | 值 |
|---|---|
| DOI | 10.1086/664083 |
| 题名 | Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI, Hipparcos, and Tycho Photometry |
| 作者 | M. S. Bessell, S. J. Murphy |
| 年份/刊 | 2012, PASP 124(913), 140–157 |
| 核验方式 | Crossref works API |
| 用途 | "探测器计量光子数 ⇒ 积分核含 λ" 的经典测光文献载体（photonic passband 约定）；本路 exp8 用纯合成前向独立演示 λ 幂次错配的后果 |

### A11. Montegriffo et al. 2023 — Gaia XP 外定标谱（式-1 数据源）

| 字段 | 值 |
|---|---|
| DOI | 10.1051/0004-6361/202243880 |
| 题名/作者 | Gaia Data Release 3: External calibration of the XP spectra；P. Montegriffo, G. De Angeli, R. Andrae 等 |
| 年份/刊 | 2023, A&A 674, A3 |
| 核验方式 | Crossref works API |
| 用途 | 式-1 的 F_λ 数据源（官方 externally-calibrated combined BP+RP flux） |

---

## B. 结构性/工程值登记（非科学量或项目冻结约定，写明豁免理由）

| 量 | 值 | 豁免/定性理由 |
|---|---|---|
| `irls_tolerance_dex=1e-6`、`irls_max_iterations=50` | 求解器收敛档 | 数值收敛实现选择：其科学内容是"结果对进一步收紧不变"（exp1 证明 1e-6 与 1e-12 结果在干净场一致、被 50 步上限截断的情形干净场为零），不是物理量。02 已登记"条文冻结"，本路补实验腿 |
| FOV 缓冲 1.2 | 几何裕量 | 无文献值（旧路线 2 稿已查、本路结论同：无单一文献），定性为 WCS 误差预算约定；exp4 给出其蕴含的误差预算量化 |
| 锥搜钳位 [1.0, 10.0] deg | 计算成本守卫 | 与星数阈值 2000 联动的成本/覆盖折衷（exp3/exp4 量化），非物理量 |
| `mag_max_arr {12..16}`、早停 2000、循环上限 5 | 查询成本策略 | 锚定 Gaia 计数模型 + 拟合最低星数 3 的工程折衷；exp3 证明阶梯深度与 FOV/密度联动合理 |
| `mag_min=6.0/mag_max=16.0`（ZP_syn 端） | 星等窗约定 | 项目冻结值；两端截断处 XPSD 质量问题（01/C4）超出本路实验能力，登记未决 |
| `max|log10 m|≤1.0 dex` | 防护栏 | 注释自认"约定值"；exp7 证明对已登记的真实失效模式（0.1 dex）恒不 binding |

---

## C. UNRESOLVED 清单

1. **Croux & Rousseeuw 1992 p.413 表值**：原文 PDF 未在开放网络取得，表值改由 MC 复算佐证（A5）。查证途径：COMPSTAT'92 会议录为 Springer/Physica 印刷卷，无合法开放副本。
2. **Huber & Ronchetti 2009 章节/页码**：专著无开放全文，页码沿 PHOTOMETRIC_FIT.md 登记，未独立翻书核对（A6，PARTIAL）。
3. **`mag_min=6.0 亮端 G≈6 的 XPSD 谱质量**（01/C4 遗留）：需官方 XPSD 质量位分布数据，本路无数据源，UNRESOLVED。
4. **"自适应阶梯 {12..16}/2000/5"的设计文献**：不存在单一文献记录该阶梯设计（旧路线 2 已查、本路同判），按 B 表定性为工程折衷，实验腿 exp3 补齐。
5. **c=4.685 的"95% 效率"在 PHOTOMETRY.md §14 的 PMC6768164 锚**：该 PMC 全文无 4.685（c=6 论文）——见 A3 对照发现，属**锚错误**而非 UNRESOLVED。

---

## D. 锚核验记录（05/审查文引用的 文件:行，本路逐行复核）

| 锚 | 本路复核结果 |
|---|---|
| `star_matcher.cpp:21-27` | ✅ _MAD_SCALE=0.6744897501960817 / _TUKEY_C=4.685 / _IRLS_MAX_ITER=50 / _IRLS_CONVERGE=1e-6 逐行在位 |
| `star_matcher.cpp:555` / `:580` | ✅ `:555` = `for (int iter = 0; iter < _IRLS_MAX_ITER; ++iter)`；`:580` = `if (diff < _IRLS_CONVERGE)`（审查-2 的锚行号准确） |
| `star_matcher.cpp:518-536` | ✅ `|r_consistent|<3 ⇒ NO_DATA`（scale=1.0、sigma=0、fit_used=0） |
| `star_matcher.cpp:430-436` | ✅ 饱和星不进匹配定标、记 rejected_quality |
| `frame_photometry_fit.cpp:166-174` | ✅ 现行代码**条件钳位**（`fov<=0 ‖ fov>=30` 才钳 [1,10]）；05"无条件钳位"是规范变更而非现状，05 该段未写明差异 ⇒ 表述缺陷成立 |
| `frame_photometry_fit.cpp:276` / `:292` / `:332` | ✅ `:276` ZP_syn 样本下限复用 kMinFitStars（01/C9 成立）；`:292` **ZP 散度用 4 位截断 1.4826**（本路新发现，见 report §S2）；`:332` n_matched<kMinFitStars ⇒ rc=−6 kFrame（01/B13 成立） |
| `filter_curve_json.h:207 / :356 / :444` | ✅ check_curve_identity / map_filter_name / load_curve 起始行均在位 |
| `spectrum_integrator.cpp:350 / :446-447` | ✅ weighted_wl = wl·trans·q；f = byte·flux_mul+flux_min、integrand = f·weighted_wl |
| `pc_api.cpp:145 / :435` | ✅ 两处字面量 `3.0` 传 mag_tolerance |
| `pc_api.cpp:977-1008` | ✅ ``static const double mag_max_arr[] = {12.0,13.0,14.0,15.0,16.0}``、`for (int i=0; i<5; ++i)`、`if (n_gaia >= 2000 ‖ i == 4)` |
| `module_adapters.cpp:5123` | ✅ `Result<void> p1_op_photometry(...)` 定义行 |
| `photometric_calib.h:126-131` | ✅ F_syn 式注释 + "不含 10^(-0.4·magG)" |
| `defaults.json:540-610` | ✅ 六个 `photometry.*` 键在登记面存在（01/C1 死登记背景） |
| `docs/science/PHOTOMETRY.md:126 / :197 / :200 / :202 / :205-210 / :213-214 / :219-220` | ✅ 逐一比对在位（:126 通带失配表、:197 r_i、:200 delta、:202 预过滤、:205-210 IRLS、:213-214 sigma、:219-220 mag_tolerance 地位声明） |
| `docs/algorithms/NOISE_ESTIMATION.md:213` | ✅ MAD→σ 常数全精度条款在位（`1.482602218505602`、`Φ⁻¹(3/4)=0.6744897501960817`，并明令 4 位简写不可互换） |
| `PHOTOMETRY.md §14 第 1 条 PMC6768164` | ❌ **锚内容不符**（A3）：全文无 4.685，主角 c=6 |

