# R1–R5 历史科研证据与创新归属审计（任务 A2）

> **本文件是审计报告，不是新的科学正本、不是验收门禁、也不是 PASS 记录。** 它只回答三件事：历史证据文件是否真的存在且能对应到生成它的脚本；旧数字能否在本次审计环境里重新得到；R1–R5 各自有哪些数学内容是先前他人成果、哪些可能是 ACSD 自己的组合。
>
> - 被审档案：`main@adc7f33d58359ac3d65014f239d7f53fe92e4912`（提交标题「收敛 #111 科学误差预算、严格曲边微内核与 WCS 合同」）下的 `实验/science-foundation/`。
> - 该子树的 Git tree SHA 为 **`bf85d25ea8e06f3fba3503dbeacb3c68bdec3283`**，共 **146 个文件、约 3.9 MB**。
> - 审计分支：`rebuild/acsd-v2`；全程**只读** `git show` / `git archive` 到 `/tmp`，未 checkout、未改 `src/`、未改任何既有文档。
> - 本次唯一新建文件：本文件。
> - 审计时间：2026-11-10。审计者未参与 R1–R5 任一算法的原始设计或实现。

---

## 0. 结论摘要（先给裁决，再给依据）

| 方向 | 历史证据存在性 | 数字可否重放 | 创新性裁决 |
|---|---|---|---|
| R1 Gaia XP 响应/标度 | 存在（`statistics_independent.json` + `GAIA_RESPONSE.md`） | **可重放**（`statistics_independent.py` 实跑，主表数字逐位相同） | **仅工程集成 + 不可辨识性的正确表述**；Gaia 合成光度、GLS、潜变量模型均为他人成果 |
| R2 曲边球面 Drizzle / 包含型面积 | 存在且最完整（37 个 oracle；`evidence/` 下 77 个 JSON + 16 个源码快照 + 9 个逐叶细化用例） | **可重放**（`round2_science_budget.py` 3184/3184 精确；`round2_sliver.py` A+B 双求解器 745/745 逐位相同）；C++ 微原型**不可重放**（本机无 MPFR 头文件） | **本审计中唯一够格的“方法候选”**：真叶包含型面积 + 面积→科学容差反推 + 双方向独立 oracle 的闭合方式 |
| R3 统一噪声 / Fisher vs scene 方差 | 存在 | **可重放**（同上） | **成熟基础**（协方差传播、GLS、BLUE、Fisher）+ 与几何/未知项闭合的**工程集成** |
| R4 相对天光差模 / 低频保护 | 存在 | **可重放**（同上） | **成熟基础**（Montage/Madam 式公共天空 + GLS）+ `K H=0` 只是所声明 H 的**线性代数恒等式**，不是新定理 |
| R5 小 N coadd / 选择 / 有效核 | 存在，且**失败证据完整** | **可重放**（失败与修订两版都在同一脚本内） | **成熟基础**（BLUE/PSD/QP/经典选择统计）+ **正确的保守化工程决策**；不构成方法创新 |

**一句话总结**：R1–R5 不是五项原创算法。按证据能支撑的程度排序，只有 R2 有资格作为“方法候选”进入后续原创性论证；R1/R3/R4/R5 的实质贡献是**把成熟统计/几何定理与 ACSD 的具体未知项、单位、支持域和失败状态正确闭合**，属于高质量工程集成与科学纪律，不是新数学。

**最重要的两条审计发现**：

1. **历史数字是真的，而且今天还能重算出来。** 本次审计在完全不同的 OS/Python/库版本上实跑三个历史 oracle，得到的不是“差不多”，而是**逐位相同**（详见 §2）。因此本报告**不**把 R2/R3/R4/R5 的主体证据标为 `HISTORICAL_UNREPRODUCED`——那样写会是错的。
2. **`evidence/manifest.json` 自身有 7 条哈希对不上**（§1.4）。这不是造假，而是清单在五份 foundation 文档最后修订后没有重新生成；但若有人拿这份 manifest 当“交付身份”，那 7 条会直接失败。必须记录，不能悄悄修。

---

## 1. 证据身份核验（文件是否真的存在、是否真的由该脚本生成）

### 1.1 文件级存在性

任务书点名的文件全部存在，路径已按 `git -c core.quotepath=false` 还原（Git 默认把中文路径显示成八进制转义，直接 `grep science-foundation` 会得到转义串，容易误判“文件不存在”）：

| 任务书点名 | 实际路径（`adc7f33d:` 前缀） | 字节 | 存在 |
|---|---|---|---|
| `STATISTICS_RESEARCH.md` | `实验/science-foundation/STATISTICS_RESEARCH.md` | 21873 | ✅ |
| `ROUND2_SCIENCE_BUDGET.md` | `实验/science-foundation/ROUND2_SCIENCE_BUDGET.md` | 19160 | ✅ |
| `ROUND2_SLIVER_RESEARCH.md` | `实验/science-foundation/ROUND2_SLIVER_RESEARCH.md` | 12276 | ✅ |
| `evidence/manifest.json` | `实验/science-foundation/evidence/manifest.json` | 15897 | ✅ |
| `evidence/round2_manifest.json` | `实验/science-foundation/evidence/round2_manifest.json` | 44261 | ✅ |
| `oracles/` | 37 个文件（**35 个 Python + 1 个 C++（`round2_affine.cpp`）+ 1 个 `detector_independent_notes.md`**） | — | ✅ |
| `GAIA_RESPONSE.md` | **不在** `science-foundation/` 下，而在 `docs/science/foundation/GAIA_RESPONSE.md` | 11167 | ✅（位置与任务书描述不同） |
| `oracles/round2_affine.cpp` | `实验/science-foundation/oracles/round2_affine.cpp` | 15817 | ✅ |

另有一个必须澄清的**表面矛盾**：当前分支五份 foundation 文档页首写的是「固定基线档案 `c5fee09fe`」，而任务书与 `docs/workplans/v2-science-engineering/science/03-R1-R5-RESEARCH-EXPERIMENTS.md` 写的是 `adc7f33d`。**两者不是矛盾**：

- `git merge-base --is-ancestor adc7f33d c5fee09fe` → 真；`c5fee09fe` 就是 `adc7f33d` 之后**一个**提交（`docs: 定义科学地基驱动的新核心架构与独立验证合同`）。
- `git rev-parse adc7f33d:实验/science-foundation` 与 `c5fee09fe:实验/science-foundation` 都是 `bf85d25ea8e06f3fba3503dbeacb3c68bdec3283`。
- `git diff adc7f33d c5fee09fe -- 实验/science-foundation/ docs/science/foundation/` **输出为空**。

结论：两个 SHA 对这批档案**完全等价**，可互换引用。本报告以 `adc7f33d` 为主键。

### 1.2 证据 → 脚本的身份链（最关键的一步）

证据文件里必须能查到“是哪个脚本、哪个字节版本生成了我”。逐条核对：

| 证据文件 | 记录的源脚本哈希字段 | 对应脚本 | 该脚本实际 SHA256 | 一致 |
|---|---|---|---|---|
| `statistics_independent.json` | `/identity/script_sha256` | `oracles/statistics_independent.py` | `4f802064…344153b` | ✅ |
| `sparse_variance_reference.json` | `/identity/script_sha256` | `oracles/sparse_variance_reference.py` | `bbbcb2d8…5947d53c` | ✅ |
| `round2_science_budget.json` | `/source_sha256` | `oracles/round2_science_budget.py` | `b0e708a8…f1afddfa30d` | ✅ |
| `round2_sliver.json` | `/source_sha256/round2_sliver.py` | `oracles/round2_sliver.py` | `572c87cc…19d2b00668` | ✅ |
| `round2_performance_final_kernel.json` | `/build/source_sha256` | `oracles/round2_affine.cpp` | `8406ca35…62edad3dcebd` | ✅（且 `round2_manifest.checks.final_cpp_sha256` 也记同一值） |
| `export_independent.json` | `/identity/script_sha256` | `oracles/export_independent.py` | `5958250f…1ce88a86389c1` | ✅ |
| `detector_independent.json` | `/source_sha256` | **`evidence/source_snapshots/detector_interval_pre_spill_guard.py`** | `93c34692…84a942c20b` | ⚠️ 不是 `oracles/detector_independent.py`（后者 `fef07f3f…`） |

最后一行不是缺陷，但**必须写清楚**：`detector_independent.json` 的 20 个构型结果由“修 tiny-spill 显示条件之前”的加载快照生成，而该快照被完整保存在 `evidence/source_snapshots/` 且哈希相符。`docs/science/foundation/INDEPENDENT_ORACLES.md:109` 对此有明确声明，属于**诚实的身份分离**，不是掩盖。

### 1.3 无法映射到本仓库任何 blob 的 40 个 64-hex 值

把全部 evidence JSON 里的 64 位十六进制串抽出来，对 `adc7f33d` 全树 4198 个 blob 做匹配，得到 **40 个不匹配**。逐类归因（**全部有合理解释，无一是伪造**）：

| 类别 | 数量 | 说明 |
|---|---:|---|
| `/build/executable_sha256` | 27 | 编译产物哈希。二进制本来就不入库，**无法核验**，只能核验它由哪个 source hash 编出（已核，见 §1.2） |
| `manifest.json` 的 7 条 | 7 | 见 §1.4，真实缺陷 |
| 被后续修正取代的脚本版本 | 3 | `cross_geometry.py` 的 `9a24b65a…`（`cross_geometry_cone_failure.json` / `cross_geometry_interval_pilot.json` 用）、`hips_orientation.py` 的 `5022f747…`（初始接缝失败用）、`statistics_independent.py` 的 `31ade30f…`（并行 LOO 失败用）——这三个旧字节版本**未入库存档** |
| `chart_overlap_independent_review.json` 的 `/historical_findings/reviewed_sha256 = ce2a27d1…` | 1 | 该文件自己写明「retained for provenance, not recomputed against corrected code」 |
| `round2_wcs_failed_*` 的 `/source_sha256` | 2 | 两个 WCS 合同失败运行的旧 `round2_wcs_contract.py` 版本 |

### 1.4 【发现 F-1】`evidence/manifest.json` 自相矛盾（中等严重，必须保留）

`manifest.json` 记录了 69 个文件的 sha256 + 字节数。逐条对 `adc7f33d` 的 blob 重算：**62 条完全一致，7 条不一致**。

| 路径 | manifest 记录 | `adc7f33d` 实际 | 字节记录 / 实际 |
|---|---|---|---|
| `docs/DOCUMENT_INDEX.yaml` | `9579813931e9…` | `1713835c32c1…` | 44248 / 44268 |
| `docs/science/foundation/GAIA_RESPONSE.md` | `74b6dddd3149…` | `4acfd8d99865…` | 10541 / **11167** |
| `docs/science/foundation/INDEPENDENT_ORACLES.md` | `cba2fcc78f1d…` | `1285ca452fbf…` | 21235 / **32734** |
| `docs/science/foundation/OBSERVATION_COORDINATES.md` | `9ec15992e88a…` | `ab4564568866…` | 13991 / **34221** |
| `docs/science/foundation/SPHERICAL_DRIZZLE.md` | `918810004356…` | `d197fd200f0a…` | 17470 / **32106** |
| `docs/science/foundation/STATISTICS_SKY_COADD.md` | `f62caa7548cd…` | `724faa978cc6…` | 17530 / **23347** |
| `实验/science-foundation/.gitattributes` | `a58579141a9b…` | `d8d67d904dd0…` | 120 / 292 |

诊断：manifest 的 `research_date_local` 是 `2026-10-20`，而五份 foundation 文档在 10-20 之后被大幅扩写（例如 `OBSERVATION_COORDINATES.md` 从 14 KB 涨到 34 KB），**清单没有随之重新生成**。这不是证据造假（62/69 逐字节相符已证明清单机制本身是认真的），但意味着：

- **不得**把 `manifest.json` 的 `files` 块当作“交付身份”引用；
- 与它对照，`evidence/round2_manifest.json` 的 **152 条全部逐字节相符（152/152）**，是可靠的。

---

## 2. 实跑复现：本次审计真实执行了什么（不是引用旧 PASS）

这是本报告与“只读旧报告”最关键的区别。审计者在 `/tmp` 下用 `git archive` 取出历史档案（**未污染工作树**），在**不同的操作系统、Python 与库版本**上重跑了三个 oracle。

复现环境（与档案声明 `Python 3.12.2 / NumPy 1.26.4 / SciPy 1.15.3 / Astropy 7.1.0 / mpmath 1.3.0 / Windows-11` **全部不同**）：
`Python 3.13.5`、`numpy 2.2.4`、`scipy 1.15.3`、`astropy 7.0.1`、`astropy_healpix 2.0.1`、`mpmath 1.4.1`、`Linux-6.12.107+deb13-amd64`。

| 脚本 | 命令 | 耗时 | 对比结果 | 判定 |
|---|---|---|---:|---|
| `oracles/statistics_independent.py` | 直接运行 | 9.5 s | 1134/1134 键齐全；**104 个值有差异，但全部是 1e-16…1e-8 相对量级的末位**，集中在非线性 Gaia `least_squares` 与 `~1e-17` 的零残差诊断量；**所有 Monte-Carlo 分数、覆盖率、阈值逐位相同** | **REPRODUCED** |
| `oracles/round2_science_budget.py` | 直接运行（纯标准库） | 0.37 s | 3184/3184 键齐全；**仅 14 项不同，全部是 `elapsed_seconds` 与 `environment` 元数据**；所有精确分数端点、误差界、细化历史逐位相同 | **REPRODUCED（完全确定性）** |
| `oracles/round2_sliver.py --max-nodes 12001` | **完整重跑 A（chart 包含积分）与 B（detector 域积分）两套求解器**，4 个解析构型 × 12001 节点 | 2 m 36 s | 745/745 数值键齐全；**仅 13 项不同，全部是 `seconds` / `wall_seconds` / `elapsed_seconds` / `command`**；所有解析真值区间、mpmath.iv 面积界、逐叶区间、`budget_exhausted` 状态**逐位相同** | **REPRODUCED（含几何求解器本体）** |

由此可以明确推翻一个常见误判：**R2 的几何区间证据与 R3/R4/R5 的统计证据不是“只能引用、无法重算”的历史遗物**。任务书要求的“不能重放的标 HISTORICAL_UNREPRODUCED”在本例中只适用于下列对象（§7）。

**未执行**（因此不得声称已复现）：`round2_affine.cpp`（本机缺 `/usr/include/mpfr.h`，无法重建 C++ 微原型）、`detector_independent.py` 的 20 构型全量、`round2_original_sliver_refine.py`、`round2_mp_affine_fallback.py`、`export_independent.py` 的四百万点 detector 积分、`sparse_variance_reference.py`。

---

## 3. R1–R5 事实卡

### 3.1 R1｜Gaia XP 跨设备经验响应 / 标度

**先前一手成果（可核）**
1. Gaia Collaboration 2023（A&A 674 A33，DOI 10.1051/0004-6361/202243709）§2、式 8–12、Appendix B：XP 合成光度、线性谱基积分及其**完整系数协方差**、`R_f = FWHM_passband / FWHM_LSF ≳ 1.4` 的经验适用域。**`q = v^T c` 与 `Cov(q_k,q_l)=v_k^T Σ_c v_l` 是原文的表示，不是 ACSD 的发明。**
2. Montegriffo et al. 2023（A&A 674 A3）外定标；GaiaXPy 官方实现。
3. Padmanabhan et al. 2008（ubercal，DOI 10.1086/524677）：全局相对标度 + 共享星 + gauge 的成熟方法。
4. Bessell & Murphy 2012（DOI 10.1086/664083）：photon/energy passband 身份与零点。

**ACSD 声称的数学新增（`docs/science/foundation/GAIA_RESPONSE.md` §1–§4）**
- 式(1) `q_{ks}=∫ f_{s,λ} R_k λ·10⁻⁹/(hc) dλ` 的**单位与量纲**必须显式：旧 `∫f T QE λ dλ` 数字不能直接称为 photon flux（这一条在 §8 有可核的源码反证，见下）。
- 式(2) `C_{ks}=a_k exp(b(u_s)^T θ_k) q_{ks}+ε` 中灰系数 `a` 含 `A_eff·g` 的**不可分乘积** ⇒ 对任意 c，`A_eff→cA_eff, g→c g` 观测完全相同 ⇒ **Gaia 标度不能分离口径与电子增益**。
- §4 的有限恒星集反例：若存在 h 使所有训练星 `∫f_s h=0`，则 `R` 与 `R+h` 给全部星相同测量、对新线谱不同测量 ⇒ **有限恒星无法恢复无限维响应**。
- 空间标度 `α(u)` 必须逐像元应用，不能把 `α_*` 与 `Σα_j P_j` 混同。

**历史证据与我的核对**

| 声称（`STATISTICS_RESEARCH.md` 表 / `INDEPENDENT_ORACLES.md:115-136`） | 证据位置 | 我的核对 |
|---|---|---|
| 3 nm / 7 nm photon q = `693.97311255` / `1621.10833603` s⁻¹m⁻² | `statistics_independent.json` → `Gaia_integral/3,7` | ✅ 逐位相同（`693.9731125517898` / `1621.1083360332875`），且重跑后仍相同 |
| 解析多项式对 quad 相对误差 `1.11e-16` / `2.22e-16` | 同上 `quadrature_error_estimate` 族 | ✅ 重跑后 `1.1102230246251565e-16` |
| 2 nm 粗格矩形通带偏差 `−33.6463%` / `−14.7072%` | 同上 | ✅ 存在且重跑一致 |
| σ=0.05 nm 真线、σ=4 nm 低分辨 → 带内线通量偏差 `−71.2144%` / `−38.9240%` | 同上 | ✅ 一致；文件明确注明「4 nm 是合成 LSF，不是 XP 实测」 |
| 连续谱 3→7 标量 `2.33598148`、中央线偏差 `+133.598%` | `Gaia_integral/continuum_scalar_3_to_7` = `2.335981476389415` | ✅ 一致 |
| 共享谱联合拟合：无噪声参数 max 误差 `6.25e-17`；两帧 log 零点 cov `3.33377e-5` | `Gaia_joint_calibration` | ✅ `6.245004513516506e-17`、`3.333769304939344e-05`（重跑差 6e-21，末位） |
| Gaia 4 参数 1000 次 68% coverage `.651/.686/.669/.683`、95% `.954/.954/.948/.951` | `Gaia_joint_calibration/parameter_coverage` | ✅ 完全一致 |
| 颜色=位置的 rank 负例：15 维联合 Jacobian 秩 14 → `GAIA_JOINT_FIT_NOT_IDENTIFIABLE` | 同上 `rank_negative_colour_equals_position` | ✅ 存在；最小奇异值重跑为 `4.32e-14`（档案 `9.28e-14`），仍是秩亏判定 |

**失败 / 负例证据（必须保留）**
- `statistics_independent.json` 的 `Gaia_integral/unknown_covariance_output = null`：缺谱协方差时 **variance 为 null 而不是 0**——这是“未知不填零”的可执行范例，必须保留。
- 3 nm / 7 nm 的 −71% / −39% 线通量偏差：这是**物理分辨率反例**，与求积误差无关；任何“求积到 1e-16 所以窄带正确”的说法都被它证伪。
- `STATISTICS_RESEARCH.md:176` 引用的旧生产源码缺陷（`spectrum_integrator.cpp:733-736,791-802`）**我已逐行核对，属实**：

```cpp
// git show adc7f33d:lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp  (sed -n 791,796p)
total += cache.node_weight[k] * fv * cache.node_qe[k] * lam;   // 未除 hc，未把 nm 换成 m
```

  该文件在 `rebuild/acsd-v2` 上**已不存在**（`lib/` 整个被移除），所以这条是纯 `HISTORICAL` 静态审计，不能当当前缺陷。

**当前 rebuild 分支差异**
- `src/response/api.cpp:11` 已按 `v_kl = ∫ b_l R_k λ_nm·1e-9/(hc) dλ` 实现，并有显式 `Response::Convention::photon_throughput` 与 `response/spectral.cpp:67 photon_weight()`；`src/response/api.cpp:584-586` 在谱支持不完整时返回具名失败而不是把未知当零。
- `src/photometry/joint_fit.cpp:272-412` 已实现**逐星 Gaia XP 系数潜变量 + 每星白化子块**的联合拟合，`src/photometry/api.cpp:441-480` 有 `not_identifiable` 路径。
- 差异：历史脚本是**小稠密 numpy** 基线；当前实现走 typed process + 存储层。历史 `fit_shared_gaia` 的**rank 负例、coverage 数值、3/7 nm 反例在当前分支没有任何对应测试或证据文件**——即“数学迁移了，证据没有迁移”。

**裁决**：**不是方法创新。** Gaia 合成光度、协方差传播、潜变量联合拟合、`R_f` 适用域全部是他人成果。ACSD 真正做对的是：把 photon/energy 单位、gain–口径不可分、未知谱协方差不得填零、以及“恒星标度正确 ⇏ 未知星云 SED 可变换”写成了**可证伪的表述**。这属于科学纪律与工程集成。**创新性：仅工程集成。**

> 未能取回：任务书要求的「#98 100 星合成评论中的模型和输入」。Issue 内容不在 Git 仓库内，本次无法核对，只能记为 **Issue-only 记录**，不得凭它支撑任何数字。

---

### 3.2 R2｜探测器原生像元 → 真 HEALPix 叶的包含型球面 Drizzle

**先前一手成果（可核）**
1. Fruchter & Hook 2002（PASP 114, 144；DOI 10.1086/338393）§2、§7：pixfrac 收缩、线性分配、相关噪声。**Drizzle 本身不是 ACSD 的。**
2. Calabretta & Roukema 2007（DOI 10.1111/j.1365-2966.2007.12297.x）§2.1–2.4、式(3)–(10)：HPX 等面积 chart。**12-face 等面积表是原文的，不是 ACSD 的。**
3. Górski et al. 2005 + Astropy-HEALPix：NESTED 与真实叶定义。
4. Farin（CAGD Ch.4.1–4.6）Bernstein 凸包/de Casteljau；Moore–Kearfott–Cloud 2009（SIAM，DOI 10.1137/1.9780898717716）包含型运算；MPFR/GMP 手册。**区间算术与精确有理裁剪都是成熟工具。**

**ACSD 声称的数学新增（`docs/science/foundation/SPHERICAL_DRIZZLE.md` §3–§6 + 三份 round2 报告）**
1. **算法 A**：在等面积目标域上做**包含型积分**——对整目标块给内/外判定，只在不能判定时细分；候选由区间下界保证不漏叶。
2. **算法 B**：在**探测器域**做独立积分（不同解析路径），与 A 双向对拍。**“两个数学结构不同的求解器互证”是这套证据的骨架。**
3. **面积区间 → 最终科学容差的反推**（`ROUND2_SCIENCE_BUDGET.md` 式 S1–S5）：由 `ε_B` 反推每个 primitive 允许的面积半径，含 signed numerator 的端点积、`N` 的分母损失、`RCR^T` 与孔径方差的**确定数值扰动界**（S4/S5），以及线性求解证书 `ρ=|| |I−GM₀|+|G|E_M ||_∞<1`。
4. **C++ 主路线**：affine-strip cut cell——区间一阶导包住光滑 HPX chart，GMP 有理半平面裁剪给精确面积/一阶矩，外层以**精确整数 floor/ceil 外包到 `2^-256 sr` 格**，避免全局有理分母爆炸。
5. **零/切触/极窄支持的三态分离**：`no_support` / `support_unresolved` / `weak_support` / 正支持，且**不把 `B=0` 当无支持**。

**历史证据与我的核对**

| 声称 | 证据 | 我的核对 |
|---|---|---|
| 四角/测地近似在极点附近错 ≈10% | `geometry_experiments.json → polar_counterexample`：N=4 `−8.9336%`，N=8192 `−9.9684%` | ✅ 存在；这是**对“四角近似”的定量证伪**，不是新理论（球面多边形面积是经典结果），但反例本身有价值 |
| SIP 四角 vs 真平面：`.47` vs `.49`（κ=0.12） | `geometry_experiments.json → sip_halfplane_counterexample` | ✅ 完全一致 |
| 18 构型全部 `bounded`，MC 无“已命中叶被候选漏掉” | `geometry_experiments.json → cases`（18 个，`status` 全 `bounded`） | ✅ 计数与状态一致 |
| N=1 解析族：strict nonoverlap / exact touch / d≈2e-4 / d≈1e-7 | `round2_sliver.json`：a = `0` / `0` / `4.7123887046840114e-08` / `1.1780972450503195e-14`；A 叶0 `[0,3.9011148780457595e-09]`（touch）；B `[0,5.810116191610502e-10]` | ✅ **逐位相同，且完整重跑 A/B 后仍逐位相同** |
| 原 `tiny_boundary_sliver` 282/293 真值 `[1.7943443981693295e-14, 1.7943443981693300e-14]` sr | `round2_original_sliver_refined.json → analytic_truth` | ✅ 一致 |
| A 请求 1e-11 sr / 15001 节点 → `budget_exhausted`，282/293 各 `[0, 4.762103122614453e-13]` | 同上 `runs[0]`，8.04 s | ✅ 一致 |
| B 请求 1e-16 sr → 31941 节点收敛，各 `[1.7920708298562244e-14, 1.7966119907953488e-14]`，中点误差 ≤ `2.987844e-20` sr，39.48 s | 同上 `runs[1]` | ✅ 一致 |
| 8 个有理科学场景（常场/signed/亮星 1e9/弱扩展/强梯度/partial/极低正支持） | `round2_science_budget.json → scenarios` | ✅ **8/8 数字逐位相同**：`.00046992/.00926527/16 次`、`8.4132e-7/3.3653e-6/7`、`7.1863e-8/3.9346e-6/8`、`1.2231e-12/3.3832e-11/6`、`8.1577e-6/3.4818e-4/11`、`3.4550e-6/8.1401e-5/10`、`2.7627e-6/6.5084e-5/8` |
| 58 个等面积 chart 真值叶（face4 φ=0 接缝 24、北极 apex 14、face0/1 公共边 20） | `round2_science_budget.json → equal_area_sky_regions` | ✅ 三组存在 |
| C++ 接线 6 行「大多未收敛」 | `round2_cpp_science_bridge_final.json` | ✅ 全部核对：`114010`、`836454`、`1.6549e-5`、`125.913/5137.23`、`2407.65/16877.17`、`930.29/6725.45`，与 `ROUND2_SCIENCE_BUDGET.md` §7 表**逐项一致** |
| 高精度 fallback：TAN 33 节点、SIP 14089 节点，`.01 L` 目标达标 | `round2_mp_affine_fallback.json` | ✅ 数值一致（`pred .0022216194` / `2.5752e-3`，`actual .00035581` / `7.7135e-5`），**但 TAN 两行计时文档写 `.015s/.058s`，JSON 实为 `0.0175s/0.0598s`** → 见 F-3 |

**失败 / 负例证据（必须保留，且**不得**删除或回写为通过）**

| 文件 | 内容 | 我的核对 |
|---|---|---|
| `detector_cone_completeness_counterexample.json` | 18 个 bad 子矩形。`rect=[0.175,0.35,-0.175,0.0]`、`unique=274877380095`，但 `sample_members` 含未返回的 `274877380437`；`total_nodes=1577` | ✅ 与 `INDEPENDENT_ORACLES.md:103` 描述**逐字一致**。这是对**旧 cone-only 面积包围**的证伪；`manifest.failure_identity_notes.external_cone` 明确写「retain as failure evidence only」 |
| `cross_geometry_cone_failure.json`、`cross_geometry_interval_pilot.json` | 同一次 cone 失败的更早记录 | ✅ 存在；其记录的 `cross_geometry.py` 哈希 `9a24b65a…` **未入库** → 这两个文件的历史字节不可重放 |
| `round2_cd_domain_underbound.json` | 近奇异 CD（精确 binary64 行列式 `+4.635139105904548e-16 deg²`，但 binary64 区间行列式跨零 `[-5e-324, 8.88e-16]`）；`worst_width_sr=1.3043214480009042e-21` < 真值 `1.4115048601862064e-19`；`contains_true_positive_total_area=false`；`judgment=UNSAFE_ABSOLUTE_DETERMINANT_UNDERBOUND` | ✅ 完全一致。**这是“连资源耗尽后的界也不包含真值”的真实缺陷**，必须保留 |
| `round2_cd_domain_review.json` | 最终版对同一输入返回 `returncode=2`、`stderr=unsupported_cd_determinant_sign` | ✅ 一致。拒绝含义是“该精度下无法证明非奇异”，不是“矩阵必奇异” |
| `round2_high_order_focus_header_serialization_failure.json` | FITS 卡把 `2.777777777777778e-9` 写成 `2.77777777777777E-9`，WCSLIB 解析后改变 CD 原 binary64 值约 `3e-15` 相对 → 两个**不同输入模型**的区间不交 | ✅ 存在；这是**输入身份反例**，不是几何错误 |
| `round2_mp_affine_rational_ledger_partial.json` + `source_snapshots/round2_mp_affine_rational_ledger_partial.py` | 全局有理分母爆炸导致 SIP 精化中断；TAN 与 SIP 有限预算记录保留 | ✅ 两者都在，快照哈希 `eae31eac…` |
| `round2_performance_initial.json`、`round2_affine_pre_radius_fix.cpp` 等 5 个 `*_pre_*` 快照 | 半径用普通 `(x1−x0)/2`、spill 只扩一次 ulp → **撤销其包围证据资格** | ✅ 快照齐全，`round2_manifest.historical_qualifications` 逐条声明 |
| `round2_sliver_budget_pilot.json` | 4 构型全源 `budget_exhausted`（A/B 各 12001 节点仍未全局收敛） | ✅ 我的完整重跑**同样得到 `budget_exhausted`**——失败状态可复现 |
| `round2_wcs_failed_1792556236789758400.json`、`…750475921000.json` | WCS 合同两次失败运行 | ✅ 存在，源脚本旧版本未入库 |

**当前 rebuild 分支差异**
- `src/geometry/containment.cpp`（641 行）实现包含型目标域积分，用 dyadic `BigInt` 的 `atan` 交替级数做**向外**包，`kSegmentsPerEdge=8` 分段边界；`src/geometry/exact_polygon.cpp` 用 `ExactFraction` 做精确半平面裁剪与交点。
- `src/geometry/kernel.cpp`（756 行）有 `work_budget_exhausted` / `shared_work_budget_exhausted`；`src/geometry/drizzle.cpp:409-415` 区分 `no_support` / `support_unresolved`。
- **实质差异**：历史证据的“双方向独立 oracle + 面积→科学容差反推 + `2^-256` dyadic ledger”在当前分支**没有对应的证据文件**；当前分支只有实现。`docs/audit/IMPLEMENTATION-STATUS.md` 把共享严格数值列为 `BLOCKED_CERTIFICATION`（N-01…N-06）；截至本审计时该页记载 N-01/N-05 的**源码已修但上游复验仍 `PENDING`**。因此**不能**把历史 R2 的收敛结论当作当前 C++ 的认证。
- 另注：`src/geometry/README.md` 仍写「仅职责目录，尚未实现」，而同目录已有 3000+ 行实现——**文档陈旧**（见 F-5）。

**裁决**：**R2 是本审计中唯一够格的“方法候选”。** 分解：
- **成熟基础**：Drizzle 权重、HPX 等面积 chart、球面多边形面积、区间算术、MPFR/GMP、Bernstein 凸包。**全部不可声称原创。**
- **可能是 ACSD 的**：① 在**真叶**（而非四角/测地近似）上把“连续 TAN/SIP 曲边 + 12-face”做成**双向独立包含型算法**并用解析真值族定标；② **由最终科学容差反推面积预算**（S1–S3）并证明 signed numerator 的正确端点处理；③ 面积区间到 `RCR^T`/孔径方差的**确定扰动界**（S4/S5）与求解证书 `ρ<1` 的串联；④ affine-strip + 精确 dyadic ledger 的**可实施高精度 fallback**（33 / 14089 节点实测）。
- **但**：这些是**组合与工程化**，不是新定理。S4/S5 是一阶展开 + 绝对值三角不等式；`ρ<1` 是标准扰动引理。**没有任何一条被投过外部同行评审。**
- **创新性：方法创新（候选，未验证）** —— 需要独立审稿人给出反例或找到先前文献才能升级；在此之前只能写「组合层面的方法候选」。

---

### 3.3 R3｜统一物理噪声 / 相关性、Fisher 与实际场景方差

**先前一手成果（可核）**
Poisson + read 噪声、GLS、Fisher 信息、线性协方差传播、Woodbury 恒等式、BLUE——全部是标准线性统计/估计理论。`STATISTICS_RESEARCH.md:28` 自己也写明「不可称原创：协方差传播/BLUE 等已知线性统计定理」。

**ACSD 声称的组合**
- `V_F ≠ V_B`：**相同 Fisher 信息不等于相同面亮度方差**。
- `C_y = L D L^T + U Λ U^T`，`P = I − E K T`，`C_z = P C_y P^T`（含**负交叉项**）。
- 共享 latent 必须按 source identity 共用列，不能在帧间复制成独立列。
- 几何**确定数值界**不是另一个随机 variance，不得加进 IVAR 分母。
- 稀疏 actual-scene 方差只能插值**同一 estimator、同一条件**的量，`C∞` bump 反证“光滑 + 稀疏控点 ⇒ 有界”。

**历史证据与我的核对**

| 声称 | 证据 | 我的核对 |
|---|---|---|
| 4×3 R + 共享 latent：经验协方差相对 Frobenius 误差 `.0089913` | `covariance/relative_covariance_frobenius_error` = `0.008991315145638867` | ✅ 重跑 `…639013`，相对差 `1.6e-14`；文件明确「不是普适 0.9% 误差界」 |
| 100 帧共享 floor：真方差 `.05`，误当独立 `.0104` | `covariance/shared_floor/100` | ✅ 完全一致 |
| Fisher 相同而 scene 不同：`V_F` 均 `1`；面亮度权 `2` vs `1.2195121951` | `Fisher_vs_surface` | ✅ 逐位一致（重跑仍同） |
| 半正定精确约束 `C=diag(1,0)`：代码给 `V_F=0`，而 `h^T C^+ h=0` 会误判无信息 | `PSD_exact_constraint`（`K=[[0,1]]`, `noise_eigenvalues=[0,1]`） | ✅ 一致；`truncation_relative_tolerance=1e-12` 被显式记为**数值模型**而非科学证书 |
| 信息量不变性 `A^T C^-1 A = (RA)^T (RCR^T)^+ (RA)` | `information_invariance`：native `0.09409214252014025` | ✅ 一致 |
| 未知方差 → `full_variance=null`，理由「nonzero source coefficient has unknown noise」 | `covariance/unknown_variance` | ✅ 存在 |
| 稀疏 actual-scene：25 控点、5000 查询、无噪声再现 `1.6098e-15`、误差/界比 `.9486221`/`.9596158` | `sparse_variance_reference.json`（seed `11120261021`） | ✅ `.9486221437864184`、`.9596157627602997`、Hessian `[[240,70],[70,-140]]` 谱界 `252.4846` |
| `C∞` bump 反例：全部控点为 0，查询点 `V(0)=V₀exp(A)`，任意 A | `nonidentifiable_without_curvature` | ✅ 存在，且明确区分「零的是 log 值，不是方差」 |

**失败 / 负例证据（必须保留）**
- `covariance/unknown_variance` 的 `null`：**未知不填零**的可执行范例。
- `PSD_exact_constraint` 的 `1e-12 λ_max` 数值秩阈值：被显式标注为「定义舍弃的数值模型，不能充当科学误差证书」。
- 稀疏插值的 `C∞` bump：证伪「光滑即可认证稀疏场」。
- 旧生产 `sky_plane.cpp:1000-1018,3915-3930` 的「残差更新只 floor-bin 一个格点、与 bilinear 求值不共享 basis」——我未逐行复核这一段（超时预算），记为**未独立核实**；该文件在当前分支已不存在。

**当前 rebuild 分支差异**
- `src/uncertainty/covariance.cpp`（175 行）+ `covariance_process.cpp`（720 行）+ `operators/linear_action.hpp` 实现 factor action（`apply`/`apply_transpose`），未形成 `Npix²` 稠密矩阵。
- `src/information/`（`information.cpp` 922、`triangulation.cpp` 986、`information_scene_process.cpp` 721）承载 Fisher 与 actual-scene 两条路径。
- 差异：历史脚本是 numpy 小矩阵 + 100000 次 MC；当前实现无对应的 MC 覆盖率证据。`IMPLEMENTATION-STATUS.md` 的 **N-05 P1**（`linalg` 的重建残差/正交缺陷/范数须向外）截至本审计时记为「源码已修、上游复验 `PENDING`」，因此当前分支**尚不能**声称已达到历史证据的严格性。

**裁决**：**成熟基础 + 工程集成。** `V_F≠V_B`、`P C P^T` 含负交叉项、Woodbury 都是教科书内容。ACSD 真正有价值的是**把“几何确定界 ≠ 随机方差”“同一 latent 不得复制成独立列”“稀疏只插值同估计量”写成拒绝伪装的规则**，以及 `C∞` bump 这个正确的反例。**创新性：仅工程集成。**

---

### 3.4 R4｜只估计相对天光差模、保护共同低频天体结构

**先前一手成果（可核）**
- Montage algorithms（IRSA）：overlap 差分 + 全局一致相对背景。**“只估相对差模”是 Montage 的既有思路。**
- Keihänen et al. 2010（Madam，arXiv:0907.0367）：公共天空与加性基线联合求解、公共模消元。
- GLS / profile likelihood / 图的连通分量与规范自由度：标准内容。

**ACSD 声称的数学新增**
- 控制模型 `c = T y = H s + A β + e`，`C_c = T C_y T^T`（用背景控制量自己的协方差，**不得**用点源 Fisher 替代）。
- `K = N D^+ Q_w W`，由 `Q_w W H = 0` **严格推出 `K H = 0`**。
- **每个实际有效 overlap 连通分量分别固定规范**；无绝对锚时公共背景本来不可观测。
- `P = I − E K T`，**同数据**天光扣除必须含交叉项。

**历史证据与我的核对**

| 声称 | 证据 | 我的核对 |
|---|---|---|
| 已知相对系数恢复：max 误差 `9.99e-16` | `sky/coefficient_max_abs_error` = `9.992007221626409e-16` | ✅ 逐位一致 |
| 公共真实梯度：max 改变 `3.55e-15` | `sky/common_gradient_max_error` = `3.552713678800501e-15` | ✅ 逐位一致 |
| 同数据 sky 单坐标：正确方差 `.9869281`、仅加独立 sky `1.0653595`、MC `.9880928`、交叉项 max `1.1503` | `sky/same_data_covariance` | ✅ 全部逐位一致 |
| 同数据 sky 68/95% coverage `.67980/.94980` vs 仅加 sky `.69810/.95791` | 同上 | ✅ 逐位一致；**错误误差条过宽**这一结论成立 |
| 10% 响应差伪装成 sky：假系数 `[-.6,-.15,.6,.15]` | `sky/response_mismatch_absorbed_as_fake_sky` | ✅ 一致（`−0.5999999999999981, −0.15000000000000036, +0.6, +0.15`） |
| PSF 不匹配、真 sky=0：假斜率 `∓0.0189296` | `sky/PSF_mismatch_absorbed_as_fake_sky` = `[1.6e-15, −0.018929606269660653, −1.6e-15, +0.018929606269660653]` | ⚠️ 文档把它写成 **2 元** `[-.0189296,+.0189296]`，证据是 **4 元** basis。属**表述简化**，数值本身正确，但读者会误以为 basis 是 2 维 → F-4 |
| 断连算例：全局规范不足 → `RELATIVE_SKY_NOT_IDENTIFIABLE` | `sky/disconnected_without_component_gauges` | ✅ 一致 |
| 三个平面 offset/slope 规范「各帧系数和为零」，真实系数 `[2,.6,-1,-.2,-1,-.4]` | `sky/true_coefficients` | ✅ 长度 6 一致 |

**失败 / 负例证据（必须保留）**
- `response_mismatch_absorbed_as_fake_sky`：**响应失配可被加性平面吸收** → 证明「保护定理的前提（H 正确）是必要的」，不是装饰。
- `PSF_mismatch_absorbed_as_fake_sky`：同一天区不同 PSF **不自动**满足相同 H。
- `disconnected_without_component_gauges = RELATIVE_SKY_NOT_IDENTIFIABLE`：全局单一规范不足，必须逐连通分量。

**当前 rebuild 分支差异**
- `src/sky/sky.cpp`（2017 行）**逐条实现了历史数学**，页首注释即可核对：`c = T y = H s + A β + e`、`C_c = T C_y T^T`、按通带/有效空间响应可比的帧组内建算子、**不同谱响应永不创建差模边**、每连通分量一个规范、`K H = 0`、`P C_y P^T` 含负交叉项、不可估方向「不施加任何改正并报 unavailable」。
- 这是本审计中**历史数学最完整地进入当前生产路径**的一个方向。
- 差异：历史证据是 numpy 小矩阵；当前实现无对应的数值证据文件。历史 R4 的 **`P = I − E K T` 同数据交叉项覆盖率实验在当前分支没有重做**。

**裁决**：**成熟基础 + 工程集成。** `K H = 0` 是 `Q_w W H = 0` 的直接线性代数推论，**不是新定理**；「相对差模」是 Montage/Madam 的既有方案。ACSD 真正的贡献是**把「保护共同低频」精确限定到所声明的 H 子空间**，并用两个反例证明前提不可省。这纠正了一个常见的过度声明，但仍是**条件澄清**，不是新方法。**创新性：仅工程集成。**

> 任务书问「历史『相对背景 A+G』方案是否已被正式放弃」。核对结果：当前 `STATISTICS_SKY_COADD.md` §4 的主线是 `H s + A β` 的 GLS 形式 + 逐分量规范，**未见**“A+G”作为并行方案；历史 `STATISTICS_RESEARCH.md` 也未出现 “A+G” 这一记号。结论：**无法从 Git 档案证实曾存在名为「A+G」的正式方案**，因此不能写“已被放弃”。建议改为「档案中无此命名，需查 Issue #101」。

---

### 3.5 R5｜可靠小 N 传统 Coadd / 选择 / 混响应有效核

**先前一手成果（可核）**
BLUE（`t = Γ⁻¹1 / 1ᵀΓ⁻¹1`）、PSD 约束下的可估子空间、非负约束 QP、经典选择统计、条件平均——全部成熟。`STATISTICS_SKY_COADD.md` §5 式(9)(10) 自己就写成标准 BLUE + LOO 标准化残差。

**ACSD 声称的研究候选**
- N≤7、外部可信 Γ 下的**单步最大 LOO**（不是并行拒所有超阈）。
- Gaussian LOO FWER 的 union bound：`z=Φ⁻¹(1−α/(2N))`，**不要求 `t_k` 独立**。
- 平移等变性：拒绝规则不得因整体加常数而改变。
- 选择之后 coverage 的**条件分界**（`t^TΓt` 只是冻结掩膜公式，不是选择后精确区间）。
- 混响应/PSF 的有效核语义 `K_eff`。

**历史证据与我的核对（这是失败证据最完整的一个方向）**

| 声称 | 证据 | 我的核对 |
|---|---|---|
| **失败**：并行拒所有超阈 → N5 单个 12σ 污染，Gaussian 95% coverage **`.91603`**、Poisson+read **`.91062`** | `statistics_parallel_loo_failed.json`（seed `11120261020`，`script_sha256=31ade30f…`） | ✅ **逐位一致**：`0.91603`（CI `[.9142939,.9177419]`）、`0.91062`；`max_rejections=null` |
| 修订路线：单步最多拒一个最大 `|t|`，α=.001 双侧 Bonferroni | `statistics_independent.py:185-224` 的 `loo_rejection(..., max_rejections=1)` | ✅ 代码中**两版算法并存**（`None` 分支注释明确写「Retained failed comparator」），失败算法未被删除 |
| N5 干净栈误拒 `.00102`，CI `[.00083176,.00123808]` | `rejection/gaussian_N5` | ✅ 逐位一致 |
| 污染后 Gaussian 68/95 `.67990/.95096`（CI `[.9496034,.9522904]`）、Poisson `.67955/.94869` | 同上 / `poisson_read_N5` | ✅ 逐位一致 |
| 相关 Gaussian（共享方差 .7）N5 误拒 `.00103`、68/95 `.67773/.94999` | `correlated_gaussian_N5` | ✅ 一致；文档「此行未测污染」也与 JSON 一致 |
| N1–3 不自动拒绝；注入污染后 coverage=0 是**预期能力边界** | `gaussian_N1.contaminated_coverage.0.95 = 0.0` | ✅ 一致；N7 为 `0.95046`，说明 N4–7 都能识别单个 12σ |
| **旧生产反例**：`median±p|median|` 非平移等变：`[-2,-1,0,1,2]` 只留 `0`；整体 +10 后留 `[8,9,10,11]` 均值 `9.5≠10` | `rejection/percentile_counterexample` | ✅ 完全一致。**且我已核对旧源码**：`git show adc7f33d:lib/algorithms/coverage/src/rejection.cpp`（sed 1140-1160）确认 `n∈[1,3] → P2_REJECT_NONE`、`n<6 → P2_REJECT_PERCENTILE`，与文档描述相符 |
| Export 同叶相关/支持：部分支持面积 `30.08%`、正确均值 `−2`、错用全像元分母 `−.6015` | `export_independent.json → cases/partial_support` | ✅ `0.30075621562656246`、`−2.0`、`−0.6015124312531249` |
| 拆同叶 `.35/.65` 后假独立只报正确白噪声方差的 `.545` | `covariance/duplicate_leaf_counterexample` | ✅ `wrong_to_correct_ratio = [0.545, 0.545, 0.545, 0.545]` |
| 200000 次 Gaussian 协方差模拟最大偏差 `2.36` 个抽样标准误；显式双和差 `5.55e-17` | 同上 | ✅ `2.3606941941678006`、`5.551115123125783e-17` |

**失败 / 负例证据（必须保留）**
1. `statistics_parallel_loo_failed.json` —— **本审计认为这是整个 R1–R5 档案里最重要的一份失败证据**：它证明了一个看起来合理的方案（并行拒所有超阈）会把覆盖率从 `.95` 拉到 `.916`。任何“多点同时拒绝”的重新引入都必须先引用它。
2. `percentile_counterexample` —— 证明 `median±p|median|` 违反平移等变性。
3. N1–3 污染后 `coverage = 0` —— **能力边界**，不是 bug；不得用“通过率”隐藏。
4. `export_initial_precision_failed.json` —— 见下。

**`export_initial_precision_failed.json` 的特别说明（重要）**
该文件只有 756 字节，内容是：

```
absolute_area_tolerance_sr = 2e-05, max_nodes_per_pixel = 60000
outcome: 四个 A overlap 求解至少一个未 bounded；脚本在显式 all-bounded 断言处停止
limitation: 该失败脚本未持久化逐像元结果，exact per-pixel status/count/width 不可得
correction: 下一次改用 3e-4 sr 绝对容差并立即保存每个 overlap
```

即：**初始精度目标（2e-5 sr）失败了，后来成功的 Export 证据用的是粗 15 倍的 3e-4 sr 预算**。文件对此完全诚实，`INDEPENDENT_ORACLES.md` 也照写。但必须点明：**“Export 独立闭环通过”这句话只在 3e-4 sr 的声明精度下成立，不是初始精度。** 这是最容易在后续文档里被悄悄升级的一句话。

**当前 rebuild 分支差异**
- `src/coadd/coadd.cpp`（1355 行）页首注释与历史修订路线**完全一致**：`single_max_loo`、N≥4、外部固定 Γ、最多拒一个、前置不满足则带理由跳过而非猜测、权重 equal/BLUE/非负 QP、混响应只允许 equal 或显式 QP 且「QP 解不叫无约束 BLUE」。
- `src/coadd/coadd_select_process.cpp:90-100` 有 `single_max_loo requires a common-mean proof or an external fixed covariance`、`the LOO alpha must lie strictly between 0 and 1`。
- 差异：历史证据的 100000 次 MC coverage 在当前分支**没有重做**；`IMPLEMENTATION-STATUS.md` 对 M5 只给 `IMPLEMENTED_NOT_VERIFIED` 或 `PASS_SCOPED`。

**裁决**：**成熟基础 + 正确的保守化工程决策。** BLUE/PSD/QP/LOO 都不是新的；union bound 控制 FWER 是标准多重比较。ACSD 真正做对的是：**先做了并行版本、实测它坏了、保留失败、然后退到单步最大拒绝并明确其适用域（N≥4、外部 Γ、单污染）**。这是良好的科学工程习惯，但不是方法创新。**创新性：仅工程集成。**

---

### 3.6 事实卡汇总

| | R1 | R2 | R3 | R4 | R5 |
|---|---|---|---|---|---|
| 最早一手文献 | Gaia DPAC 2023 / Montegriffo 2023 / ubercal 2008 | Fruchter–Hook 2002 / Calabretta–Roukema 2007 / Górski 2005 | 标准估计理论（GLS/Fisher/BLUE/Woodbury） | Montage / Madam 2010 / GLS | BLUE / QP / 经典选择统计 |
| ACSD 严格差异 | photon 单位、gain–口径不可分、有限星集无法定响应 | 真叶包含型双算法 + 面积→科学容差反推 + dyadic ledger fallback | 几何确定界≠随机方差；同估计量稀疏插值；`C∞` 反例 | `K H=0` 限于所声明 H；逐分量规范；同数据交叉项 | 单步最大 LOO + 失败保留 + 选择后 coverage 条件分界 |
| 完整数学命题+条件 | 有（式1–4，条件明确） | 有（S1–S5、ρ<1、单射条件、准入域） | 有 | 有 | 有 |
| 唯一独立复现入口 | `oracles/statistics_independent.py` | `round2_sliver.py` / `round2_science_budget.py` / `round2_affine.cpp` | `statistics_independent.py` / `sparse_variance_reference.py` | `statistics_independent.py` | `statistics_independent.py` / `export_independent.py` |
| 本次是否实跑 | ✅ | ✅（Python 两套；C++ ❌） | ✅ | ✅ | ✅ |
| 真实数据假设 | **未验证** | **未验证** | **未验证** | **未验证** | **未验证** |
| 创新性裁决 | 仅工程集成 | **方法创新（候选，未验证）** | 仅工程集成 | 仅工程集成 | 仅工程集成 |

**不要凑五项原创。** 按本审计的证据，能进入“原创性待论证”清单的只有 R2 的 ①–④；R1/R3/R4/R5 的贡献应表述为「把成熟方法在 ACSD 的具体未知项/单位/支持域上正确闭合」。

---

## 4. 必须保留的失败证据清单（不得删除、不得回写为通过）

| # | 文件（`adc7f33d:实验/science-foundation/evidence/`） | 证明了什么 | 本次可否重放 |
|---|---|---|---|
| 1 | `statistics_parallel_loo_failed.json` | 并行拒所有超阈使 N5 污染后 95% coverage 从 `.95` 掉到 `.91603` / `.91062` | **算法可重放**（脚本保留 `max_rejections=None` 分支）；**该次字节不可**（`31ade30f…` 未入库） |
| 2 | `detector_cone_completeness_counterexample.json` | 旧 cone-only 面积包围**漏叶**：`274877380095` 未返回 `274877380437` | 数据可读；生成脚本可读；未重跑 |
| 3 | `cross_geometry_cone_failure.json` / `cross_geometry_interval_pilot.json` | 同上失败的更早记录 | ❌ 源 `9a24b65a…` 未入库 |
| 4 | `hips_orientation_initial_seam_failure.json` | 单个 HPX 平面 `CRVAL1=0` 切 face 6 的 ±180 中断，100 点中 **46 非有限**；修法是把中心旋转 180°，**不是补索引** | ❌ 源 `5022f747…` 未入库（最终版 `6e3963ff…` 已入库，36 记录 max 角 `9.81e-14`） |
| 5 | `export_initial_precision_failed.json` | 初始 `2e-5 sr` 面积精度**失败**，且未保存逐像元明细；后续改用 `3e-4 sr` | ❌ 数据本身即声明不可得 |
| 6 | `round2_cd_domain_underbound.json` | 近奇异 CD 下「连资源耗尽后的界也不包含真值」`1.30e-21 < 1.41e-19` | 需重编 C++，❌ |
| 7 | `round2_high_order_focus_header_serialization_failure.json` | FITS 卡序列化改变 CD 原 binary64 值 → 两个不同输入模型区间不交 | 需重编 C++，❌ |
| 8 | `round2_mp_affine_rational_ledger_partial.json` | 全局有理分母爆炸导致 SIP 精化中断 | 需 MPFR，❌ |
| 9 | `round2_sliver_budget_pilot.json` | 4 构型全源 `budget_exhausted`（局部正下界 ≠ 全源收敛） | ✅ **本次完整重跑得到同样 `budget_exhausted`** |
| 10 | `round2_performance_initial.json` + `source_snapshots/round2_affine_pre_radius_fix.cpp` | 普通 `(x1−x0)/2` 半径 + 单次 ulp 扩展 → **撤销包围资格** | 需 MPFR，❌ |
| 11 | `round2_wcs_failed_1792556236789758400.json` / `…0475921000.json` | WCS 合同两次失败 | ❌ 源未入库 |
| 12 | `statistics_independent.json` 的 `unknown_variance=null`、`Gaia unknown_covariance_output=null` | 未知量填 `null` 而非 `0` | ✅ 重跑一致 |
| 13 | `rejection/*/contaminated_coverage` N1–3 `=0.0` | 小 N 污染后完全不覆盖是**能力边界** | ✅ 重跑一致 |
| 14 | `geometry_experiments.json → polar_counterexample` / `sip_halfplane_counterexample` | 四角/测地近似在极点错 ≈10%；SIP 四角 `.47` vs 真 `.49` | 数据可读；未重跑 |

---

## 5. 与现有文档的矛盾 / 需修订处

| ID | 严重度 | 内容 | 建议 |
|---|---|---|---|
| **F-1** | 中 | `evidence/manifest.json` 的 69 条中有 **7 条 sha256/字节数**与 `adc7f33d` 实际 blob 不符（五份 foundation 文档 + `DOCUMENT_INDEX.yaml` + `.gitattributes`），因清单未随后续扩写重新生成 | 保留原文件；在 `INDEPENDENT_ORACLES.md` 加一行说明「manifest 的 `files` 块记录的是 2026-10-20 版本，与最终文档不一致；可靠清单是 `round2_manifest.json`（152/152）」。**不要**回写哈希去“修好”它 |
| **F-2** | 低 | 五份 foundation 文档页首引用 `c5fee09fe`，任务书引用 `adc7f33d` | 在 `03-R1-R5-RESEARCH-EXPERIMENTS.md` 注明两者 `实验/science-foundation` 子树 SHA 同为 `bf85d25e…`，避免读者以为存在两份不同档案 |
| **F-3** | 低 | `ROUND2_SCIENCE_BUDGET.md` §8 表 TAN 两行计时写 `.015s` / `.058s`，`round2_mp_affine_fallback.json` 实为 `0.0175s` / `0.0598s`；其余 8 个数字（节点数、区间宽、预测界、真实误差）**全部一致** | 改计时为 JSON 值或加“约”。计时不影响包含性结论 |
| **F-4** | 低 | `STATISTICS_RESEARCH.md` 把 PSF 失配假斜率写成 2 元 `[-.0189296,+.0189296]`，证据是 4 元 basis `[~0, −.0189296, ~0, +.0189296]` | 补全 4 元或注明「仅列非零分量」 |
| **F-5** | 中 | `src/geometry/README.md`、`src/response/README.md` 仍写「仅职责目录，尚未实现」，而同目录已有 641/603/756/869 行实现 | 更新 README 状态；否则会误导“当前无实现”，与 `IMPLEMENTATION-STATUS.md` 的 `IMPLEMENTED_NOT_VERIFIED` 自相矛盾 |
| **F-6** | 中 | 任务书 R1 要求取回「#98 100 星合成评论中的模型和输入」与 R4「相对背景 A+G 方案」 | 两者**都不在 Git 档案内**，本次无法核对。应在文档中明确标为 **Issue-only / 未核实**，不得作为证据引用 |
| **F-7** | 低 | `detector_independent.json` 的 20 构型由 `source_snapshots/detector_interval_pre_spill_guard.py`（`93c34692…`）生成，而非 `oracles/detector_independent.py`（`fef07f3f…`） | 文档已声明（`INDEPENDENT_ORACLES.md:109`），但建议在证据索引表里也加一列“生成脚本路径”，避免按文件名误配 |

**未发现**的严重问题（明确记录以免后续重复怀疑）：
- 没有发现伪造的数字。三个可重放脚本在异构环境下给出**逐位相同**的结果，这是“数字确实由这些脚本生成”的最强证据。
- 没有发现历史 PASS 被当前文档伪装成当前 PASS。`INDEPENDENT_ORACLES.md` 与五份 foundation 都有 `HISTORICAL_CHECKOUT_ONLY` / `DERIVED` / `INDEPENDENTLY_TESTED` / `ASSUMPTION` / `REQUIRES_REAL_DATA` 分级，且明确写「修正科学定义不代表本轮改动这些 C++」。

---

## 6. `HISTORICAL_UNREPRODUCED` 清单（严格限定）

只有以下对象应标 `HISTORICAL_UNREPRODUCED`（**不含** R2/R3/R4/R5 的主体数值证据）：

1. **C++ 微原型 `round2_affine.cpp` 的全部运行**——本机无 `/usr/include/mpfr.h`，无法重建；但源码已入库且与证据记录的 `8406ca35…` 相符，所以是“环境不可重放”，不是“来源不明”。
2. **27 个 `/build/executable_sha256`**——编译产物不入库，永远无法逐字节核验。
3. **三个被取代的脚本字节版本**：`cross_geometry.py@9a24b65a…`、`hips_orientation.py@5022f747…`、`statistics_independent.py@31ade30f…`。对应证据（`cross_geometry_cone_failure`、`cross_geometry_interval_pilot`、`hips_orientation_initial_seam_failure`、`statistics_parallel_loo_failed`）的**数字无法从入库字节重算**；其中并行 LOO 的**算法**仍在当前脚本里可执行。
4. **`round2_wcs_failed_*` 两次失败运行**的源脚本版本。
5. **`export_initial_precision_failed.json` 的逐像元明细**——文件本身声明未持久化，永久不可得。
6. **`manifest.json` 中 7 条陈旧哈希所对应的文件版本**——那些字节从未入库。
7. **未执行的脚本**：`detector_independent.py` 20 构型全量、`round2_original_sliver_refine.py`、`round2_mp_affine_fallback.py`、`export_independent.py` 四百万点积分、`sparse_variance_reference.py`。这些是**本次未跑**，不是**不可跑**——不得写成后者。

---

## 7. 仍无法恢复的内容（诚实清单）

- Issue #98/#99/#100/#101/#102/#111 的正文与评论（`#98 100 星合成`、`#101 相对背景研究`）——不在 Git 内。
- 真实 HST / M42 观测数据、真实 QE/滤镜/LSF/星云 SED/大气条件——**从未有过**，档案自己反复声明。
- 原始研究工作目录 `C:\Users\fujia\.codex\…\ACSD-research\实验\science-foundation\`（出现在 `round2_performance_final_kernel.json` 的 build command、`round2_cd_domain_*.json` 的 `--source` 参数里）。本仓库的 `实验/science-foundation/` 是它的**副本**；原目录的其他内容（若有）不可得。
- 27 个可执行文件本体。
- `round2_affine.cpp` 在 Windows/MinGW + MPFR 下的原始构建环境。

---

## 8. 最小纠正建议（按优先级）

1. **（F-1）** 在 `INDEPENDENT_ORACLES.md` 加 3 行，说明 `manifest.json` 的 `files` 块已陈旧、可靠清单是 `round2_manifest.json`。**不改旧文件**。
2. **（F-5）** 更新 `src/geometry/README.md`、`src/response/README.md` 的“尚未实现”，改为指向 `IMPLEMENTATION-STATUS.md` 的 `IMPLEMENTED_NOT_VERIFIED`。
3. **（F-6）** 在 `03-R1-R5-RESEARCH-EXPERIMENTS.md` 的 R1/R4 段把 Issue-only 材料显式标为「未核实，不得引用为证据」。
4. **（F-3/F-4）** 两处小数字对齐 JSON。
5. **创新性表述**：把 R1–R5 的对外表述从「五方向科学研究」改为「1 项方法候选（R2）+ 4 项成熟方法的条件闭合」；R2 的候选资格需外部审稿人给出反例或找到先前文献后才能升级。
6. **证据迁移**：当前分支的 R1–R5 实现（`src/response`、`src/photometry/joint_fit.cpp`、`src/geometry/containment.cpp`、`src/uncertainty`、`src/sky/sky.cpp`、`src/coadd/coadd.cpp`）**没有对应的当前证据文件**。历史证据不能替当前实现认证（`AGENTS.md` 明确禁止）。建议在 `IMPLEMENTATION-STATUS.md` 的 M5 行按方向逐一注明「历史数值证据位于 `adc7f33d`，当前实现未复验」。

---

## 附录 A：本次审计的精确命令与结果

```bash
# 1) 档案清单（必须用 core.quotepath=false，否则中文路径是八进制转义）
git -c core.quotepath=false ls-tree -r --name-only adc7f33d | grep "science-foundation"   # → 146 行
git rev-parse adc7f33d:"实验/science-foundation" c5fee09fe:"实验/science-foundation"
#   → 两者均 bf85d25ea8e06f3fba3503dbeacb3c68bdec3283

# 2) 只读导出到 /tmp（不 checkout、不动工作树）
git -c core.quotepath=false archive adc7f33d "实验/science-foundation" | tar -x -C /tmp/sf

# 3) 哈希核验
#    manifest.json: 69 条 → 62 一致 / 7 不一致（见 F-1）
#    round2_manifest.json: 152 条 → 152 一致

# 4) 实跑复现（环境：Python 3.13.5 / numpy 2.2.4 / scipy 1.15.3 /
#                astropy 7.0.1 / astropy_healpix 2.0.1 / mpmath 1.4.1 / Linux）
python3 oracles/statistics_independent.py            # 9.5 s，104/1134 键末位差异
python3 oracles/round2_science_budget.py             # 0.37 s，仅 14 项元数据/计时差异
python3 oracles/round2_sliver.py --max-nodes 12001   # 2 m 36 s，仅 13 项计时/命令差异

# 5) 旧生产源码反证（当前分支已无 lib/）
git show adc7f33d:lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp | sed -n 791,796p
git show adc7f33d:lib/algorithms/coverage/src/rejection.cpp             | sed -n 1140,1160p
```

## 附录 B：本次审计的自我限制

- 审计者**未**运行 C++ 生产代码（`rebuild/acsd-v2` 的 `src/`），只做了源码阅读与 grep。因此本报告对当前实现的判断是**逻辑审查级**，不是 `IMPLEMENTATION_REVIEWED` 级的逐路径认证。
- 审计者**未**独立推导 R2 的 S1–S5 与 `ρ<1` 引理，只核对了「文档声称的数字 = 证据文件里的数字」与「证据能重算」。**数学正确性本身仍需独立审稿人重推。**
- 审计者**未**取得真实观测数据，所有“真实数据未验证”的结论只是转述档案的自我声明。
- 审计者与 R1–R5 任一算法的原始设计/实现**无关联**，但本次审计本身**不是**外部学界同行评审；原创性裁决只能是“候选”，不能是“已认可”。
- 本报告引用的 `docs/audit/IMPLEMENTATION-STATUS.md` 行文在本次审计期间正被其他任务并行修改（工作树中该文件为已修改状态）。本报告只把它当作**交叉参照**，不把它的任何状态标签当作科学结论；引用处已标注“截至本审计时”。
