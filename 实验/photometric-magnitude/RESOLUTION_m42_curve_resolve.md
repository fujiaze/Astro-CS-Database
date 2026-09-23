# 订正：M42 真实数据测光残差散度 7.8 倍超标的根因（曲线解析缺陷）与判据带口径澄清

> **对象**：创新点一（测光星等坐标系）在 M42 真实数据上的判红（第五实验单元
> `实验/m42-realdata`，49 帧，逐帧 `2.5·sigma_residual_dex` 中位 **0.44746 mag**）。
> **本文件性质**：根因订正记录。**不改** `REPORT_paper.md`（本实验的判据带与结论正文）、
> **不改** `实验/m42-realdata/`（只读）、**不改**任何科学公式/容差/阈值。
> **一手证据目录**：`run/M42-SCIA-ROOTCAUSE-01/`（code/ 脚本、evidence/ 结果 JSON、
> build/ 探针二进制与抽取的被测函数）。**根因结论全部可复跑**。

---

## 1 结论（先给判定）

**根因是生产实现的曲线解析缺陷 + 配置缺 QE，不是判据带口径问题，也不是选星/拟合口径
需要向本实验对齐。**

生产配置声明 `filter="Baader R"`（`run/RELEASE-05/vis/configs/p1_m42.json` 的
`photometry.fit` 段），但 `load_curve` 的**文本搜索**在**转录版** `filters.json` 上
命中了错误曲线：实际用于合成 `F_syn` 的是 **`Antlia V Pro Series B`（53 点，
420–524 nm 蓝端）**，且 `qe_json`/`qe_name` 未配置 ⇒ `Q(λ)≡1`。

后果：逐星残差 `r = log10(F_instr/F_syn)` 的散度里混入了**通带失配的色项**，
49 帧中位从应有的 **0.04505 mag** 膨胀到 **0.42720 mag（9.5 倍）**。
修好通带后 M42 真实数据的散度**落回 EXP-04 三帧物理前向仿真的量级**
（0.045344 / 0.057457 / 0.051718 mag），说明**判据带作为量级参照是可搬运的**，
前提是生产口径正确。

---

## 2 根因的一手证据链（三条互相独立）

### 2.1 配置层：意图是 Baader R，且没有 QE

`run/RELEASE-05/vis/configs/p1_m42.json`（T2/T3 两个 block 同）：

```json
"photometry": { "fit": {
  "enabled": true,
  "gaia_data_dir": ".../gaia/GaiaDR3SP",
  "filter": "Baader R",
  "filters_json": ".../eng/packaging/config/filters.json"
} }
```

⇒ 声明通带 **Baader R**；`filters_json` 指向**转录版**；**无 `qe_json`/`qe_name`**。

### 2.2 代码层：`load_curve` 的文本搜索在转录版上失效

`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:62-91`（修复前）：
`content.find("\"Baader R\"")` 取**第一次文本出现**，再从该偏移往后找第一个
`"wavelength_nm"`。转录版 `eng/packaging/config/filters.json` 的顶层键序是
`[filters_schema, library_id, task, authority, transcription, provenance, lookup, filters]`
—— `provenance.per_filter` 段在 `filters` 段**之前**，`"Baader R"` 先在那里命中
⇒ 偏移落在 `filters` 段之前 ⇒ 取到 `filters` 段的**第一个**滤镜
`Antlia V Pro Series B`（53 点 / 420–524 nm）。

`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1368-1413` 的
`load_filter_curve` 是**同一缺陷的第二份独立实现**（本文件域不含该文件 ⇒ 未改，见 §6）。

### 2.3 数值层：用生产代码本体复算，与落盘值逐位一致

探针 `code/probe_zp.cpp` 直接链接生产的 `spectrum_integrator.cpp` +
`gaia_client.c`（`prepare_filter_cache` + `compute_f_syn_cached_xpsd` +
`gaia_client_cone_search_with_spectrum`），复算 T2/M1 帧的 `ZP_syn`：

| 曲线来源 | 滤镜名 | QE | n_zp | zero_point_mag | scatter(1.4826MAD) |
|---|---|---|---|---|---|
| `eng/packaging/config/filters.json` | "Baader R" | 无 | 2338 | −15.892792 | 0.404377 |
| **`eng/packaging/config/filters.json`** | **"Baader R"** | **无（QE 分支未进）** | **2338** | **−15.126346727** | **0.411522171** |
| `lib/.../response_curves/filters.json` | "Baader R" | KAF-16803 | 2339 | −15.071573 | 0.039422 |
| `lib/.../response_curves/filters.json` | "Baader R" | 无 | 2339 | −14.265714 | 0.041566 |

**生产落盘（`p1_phot.json → photscale_fit[T2/M1]`）**：
`zero_point_mag = −15.126346726632235`、`zero_point_scatter_mag = 0.4115221714279918`、
`zero_point_n_stars = 2338`。

⇒ **第二行与落盘值逐位一致**。探针日志同时给出曲线身份：
`滤光片 53 点 ... 范围 F[420.0,524.0]` = `Antlia V Pro Series B`。

### 2.4 匹配层：`sigma_residual` 与 `k_photo` 逐位复现

在 Python 里逐字复现生产口径（`module_adapters.cpp:4518-4565` 的 PSF 星装配、
`star_matcher.cpp:185-368` 的双向最近邻半径 2.0 px、`:378-627` 的
`mag_tolerance=3.0` 预过滤 + IRLS/Tukey c=4.685 + `MAD(r_inliers)/0.6744897501960817`、
`wcs_transform.cpp` 的 TAN+SIP 投影），用**生产实际命中的曲线**（Antlia B，无 QE）：

| 帧 | 落盘 `n_matched` | 复现 | 落盘 `sigma_residual_dex` | 复现 | 落盘 `k_photo` | 复现 |
|---|---|---|---|---|---|---|
| T2/M1 `...20251212_012404` | 511 | **511** | 0.2039505600336004 | **0.2039505597904689** | 2.3846837130250378e−17 | **2.384683712967724e−17** |
| T3/M1 `...20251212_021119` | 550 | **550** | 0.21548911328879902 | **0.21548911317764927** | 2.5578689941696292e−17 | **2.557868994211206e−17** |

⇒ 口径复现忠实；通带是唯一被替换的因子。

---

## 3 受控对照实验（49 帧全跑，每项「改前 → 改后」）

`code/step6_controls.py`，`results = run/M42-SCIA-ROOTCAUSE-01/evidence/step6_controls.json`。
除被改因子外，其余全部保持生产口径（半径 2.0 px、tol 3.0、Tukey c=4.685、
星等窗 [6.0, 16.0]（`frame_photometry_fit.h:63-64` 默认值 + `pc_api.cpp` 自适应
`mag_max` 上限 16.0）、饱和位 `quality&1` 剔除）。

| 变体 | 改动 | 49 帧中位 (mag) | min | max | 相对生产 |
|---|---|---|---|---|---|
| STORED | 生产落盘 | **0.44746** | 0.33280 | 0.56886 | — |
| V0_prod | 复现生产（Antlia B，无 QE） | 0.42720 | 0.28972 | 0.56983 | 1.00× |
| **V1_band_fix** | **只把通带改成 Baader R（仍无 QE）** | **0.04505** | 0.02913 | 0.09348 | **0.105×** |
| V2_band_qe_fix | 再计入 KAF-16803 QE | 0.05104 | 0.03217 | 0.09528 | 0.119× |
| V3_band_qe_m2 | 再加 `m_degree=2` 空间增益场 | 0.03245 | 0.02149 | 0.06489 | 0.076× |
| V4_band_qe_mag15 | 星等截断收紧到 magG<15 | 0.04705 | 0.03277 | 0.12457 | 0.110× |
| V5_band_qe_lowbg | 剔除背景最亮的 40%（`p1_flux.background`） | 0.04322 | 0.02441 | 0.08913 | 0.101× |
| V6_prod_m2 | **错误通带 + 空间增益场** | 0.33658 | 0.27414 | 0.41702 | 0.788× |
| V7_band_qe_chi2proxy | 剔除相对通量误差最大 20%（chi2 代理） | 0.04670 | 0.03056 | 0.10196 | 0.109× |
| V8_band_qe_nosat | 不剔除饱和位（负向对照） | 0.05193 | 0.03323 | 0.09554 | 0.122× |

**逐项结论**：

1. **通带曲线（色项）是唯一能解释 7.8 倍超标的因子**：单改这一项，散度降 9.5 倍，
   从 0.42720 落到 0.04505 mag。
2. **空间增益场 `m_degree=2` 在错误通带下无效**（V6 = 0.33658 mag，仍是生产口径的 6.6 倍）；
   在正确通带下有小幅改善（0.05104 → 0.03245，−36%）。⇒ **不是"生产该拟合空间场"能解决的问题**。
3. **星等截断 / chi2 代理截断 / 背景污染剔除 / 饱和剔除都是次级项**（各 −0 ~ −15%），
   加在一起也解释不了 9.5 倍。
4. **QE 未配置使散度略升**（0.04505 → 0.05104，+13%）：`Q(λ)` 缺失让有效通带与真实
   仪器响应不符，属"显式未建模项"。物理上 QE 必须计入；计入后仍远低于生产口径。

**注**：EXP-04 的 `m_degree=2` 在星等残差域拟合，其 `sigma_obs_after_m_mag` **不再乘 2.5**
（`实验/photometric-magnitude/code/scia_calib.py:153-155` 明确订正过"此前多乘了 2.5"）。
本文件的 V3/V6 已按该口径（mag 域 MAD）报出。

---

## 4 判据带口径的澄清（本实验结论的订正说明，不改判据带本体）

1. **`REPORT_paper.md:71-80` 的双边界判据是"帧自算"的**：
   `σ_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 本帧预算项²)`，预算逐帧由本帧误差项推出
   ⇒ **不存在一个可搬运的常数上界**。
2. **`0.057457 mag` 是 EXP-04 帧 B 的 `σ_obs`（观测值），不是带上界**
   （`REPORT_paper.md:147-149` 列的是三帧 `σ_obs`；三帧的 `σ_ceiling` 为
   0.056830 / 0.103015 / 0.159899）。`实验/m42-realdata/code/c1_photometry.py:21-23`
   把它写作"EXP-04 band 上界"是**不精确的表述**。
3. **但这不改变判红结论的正确性**：`0.44746 mag` 远超任何合理预算；且**修好通带后
   实测 0.04505 mag**，与 EXP-04 帧 A 的 0.045344 mag 几乎相同
   ⇒ **判据带（作为量级参照）对生产口径是可搬运的**，前提是生产曲线口径正确。
4. **独立于 EXP-04 的依据**（`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md`）：
   - §1.1「Gaia DR3 XP 提供 330–1050 nm 通量定标低分辨率分光光度，**完全落在该波长
     范围内的任何通带**都可由此得到直接锚在物理单位通量上的合成测光」[B5]；
   - §1.3 `F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`（W·m⁻²·nm）——**不含 `×10^(−0.4·G)` 因子**；
   - **§1.4「绝对精度上限 ≈ 1%，现实精度由通带失配支配」** ⇒ 通带错配是合成测光定标的
     主误差项，量级可达亚星等 —— 与本轮实测的 0.41 mag 一致；
   - §1.8「若 XP 谱未绝对定标或**通带不覆盖**，则退化为『不能』」。
5. **文档与实现的口径分歧（发现但未改，见 §6）**：
   `docs/science/PHOTOMETRY.md` 与 `spectrum_integrator.h:16` 写的
   `F_syn = ∫…× 10^(−0.4·G_Gaia)` 是**旧 uint8 猜测公式**的口径；
   生产实际用 XPSD 官方解码（`compute_f_syn_cached_xpsd`，**不乘 magG**，
   `pc_api.cpp:378-390` 注释"不再使用 uint8*10^(-0.4G) 猜测公式"）。
   实测：本文件所有复现按**不乘 magG** 才与落盘逐位一致（乘了会差 6.4 dex）。
   ⇒ 科学文档的公式行需要按变更流程订正（不在本任务文件域内做）。

---

## 5 `k_photo` 帧间散布 0.37/0.72 dex 的性质（回答"能否跨帧相加"）

`code/step7_common_stars.py`：按 pointing（M1..M6）分组，取**同组全部帧共同匹配到的
Gaia 星**（109–459 颗），用同一批星重算逐帧 `location`，与"各帧各自星集"对照。

| 组 | 帧数 | 共同星 | 生产口径散布 | 修通带后 | 用共同星（修通带后） |
|---|---|---|---|---|---|
| T2 M1 | 2 | 459 | 0.1130 mag | 0.0903 | 0.0907 |
| T2 M2 | 4 | 185 | 0.0257 | 0.0246 | 0.0211 |
| T2 M3 | 2 | 357 | 0.0594 | 0.0559 | 0.0547 |
| **T2 M4** | 3 | 149 | **0.4532** | 0.4813 | 0.4797 |
| T2 M5 | 3 | 221 | 0.1098 | 0.1237 | 0.1232 |
| T2 M6 | 2 | 370 | 0.1883 | 0.1933 | 0.1912 |
| T3 M1 | 6 | 373 | 0.1169 | 0.1032 | 0.1014 |
| T3 M2 | 4 | 185 | 0.0913 | 0.1194 | 0.1177 |
| T3 M3 | 6 | 273 | 0.1007 | 0.1079 | 0.1093 |
| **T3 M4** | 6 | 109 | **1.7481** | 1.7684 | 1.7673 |
| T3 M5 | 5 | 189 | 0.2354 | 0.2487 | 0.2508 |
| T3 M6 | 6 | 307 | 0.4172 | 0.4296 | 0.4272 |

**结论**：

1. **"用同一批星"与"各帧各自星集"给出的帧间散布几乎相同**（差 < 10%）
   ⇒ 帧间散布**不是星族抽样伪差**。
2. **整块（block 级）散布的主因是 pointing 之间的差，而非同 pointing 内的帧间差**：
   T2 块级 0.4036 dex 由 M4 组（0.48 mag）与其他组（0.02–0.19 mag）拉开；
   T3 块级 0.6992 dex 几乎全部来自 **T3/M4 组的 1.77 mag**（6 帧）。
3. **通带错误对 T2 块级散布贡献约一半**（0.4036 → 0.1925 dex）：错误通带让
   `location` 强烈依赖星族颜色 ⇒ pointing 间出现伪差。**T3 的块级散布与通带无关**
   （0.6992 → 0.7074，几乎不变）。
4. ⇒ **`k_photo` 的帧间散布既有真差也有伪差**：
   - **伪差部分**：错误通带造成的 pointing 间零点伪差（T2 块级约一半）；
   - **真差部分**：同 pointing 内的逐帧差（最高 T3/M4 的 1.77 mag ≈ 5 倍通量），
     与通带无关，量级上只能用**帧间真实系统项**（透明度/天光/背景梯度）解释。
5. **因此：修好通带是跨帧可加的必要条件，但不是充分条件**。
   即便修好，T3/M4 组仍有 1.77 mag 的帧间差，靠 `k_photo` 对齐不了 ——
   跨帧相加必须走 Phase2 的加性天光校正 + 逐帧权重（`c_subtracted=false`、
   `sky_plane_mode="delta_to_B_ref"` 的产品语义也指向这一点）。
   **`photscale_spread_gate = "none (owner ruling 9.49: frame-independent)"` 这条裁决
   在本轮中未被改动，也未被动摇。**

---

## 6 修法与「发现但未改」

### 6.1 已改（`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp`）

1. **`load_curve` 改为结构化对象解析**（:60-121）：只接受同时满足
   (a) 键后紧跟 `:` 与 `{`（是对象键而非任意文本）、(b) 该对象内**含
   `wavelength_nm` 数组** 的候选；`provenance` 段的对象不含 `wavelength_nm`
   ⇒ 被跳过。**不改任何科学公式、常数与容差。**
2. **QE 缺失/解析失败显式告警**（:174-192）：`qe_json`/`qe_name` 未配置或解析失败时
   打 stderr 告警（语义仍是 `Q(λ)≡1`，不阻断），避免"没配 QE"与"QE 已计入"
   在下游不可区分。

**编译校验**：按 `ninja -C build -t commands` 取到的生产编译命令，
`g++ -fsyntax-only … frame_photometry_fit.cpp` **无错误无告警**。

**能红能绿实证**（`code/verify_curve_resolve.cpp`，被测函数按括号配平**从生产文件
逐字抽取**到 `code/extracted_load_curve.inc`）：

| 组合 | 结果 |
|---|---|
| [A] 修复前 + 转录版 `filters.json`（配置实际指向） | n=53，[420.0, 524.0] nm ⇒ **判红（取到 Antlia B）** |
| [B] **修复后** + 转录版 | n=73，[572.0, 716.0] nm ⇒ **判绿（取到 Baader R）** |
| [C] 修复前 + 原始 `filters.json` | n=73，Baader R（原本就正确，非回归面） |
| [D] 修复后 + 原始 `filters.json` | n=73，Baader R ⇒ **无回归** |
| [E] 负例：不存在的曲线名 | **正确拒绝**（不静默取到别的曲线） |

### 6.2 发现但未改（超出文件域或需变更流程）

1. **`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1368-1413`**
   的 `load_filter_curve` 是**同一缺陷的第二份实现**（不在本任务文件域
   `lib/algorithms/photometry/**`）⇒ 只登记，未改。**这是同一 bug 的另一入口，
   必须一并修，否则 orchestrator 路径仍会取错曲线。**
2. **`docs/science/PHOTOMETRY.md` 与 `spectrum_integrator.h:16` 的
   `F_syn = ∫…× 10^(−0.4·G_Gaia)`** 与生产实现（XPSD 官方解码，不乘 magG）不一致
   ⇒ 科学文档订正走变更流程，本任务未改。
3. **`实验/m42-realdata/code/c1_photometry.py:21-23` 把 `0.057457` 写作
   "EXP-04 band 上界"** ⇒ 表述不精确（见 §4.2）。本任务不改该单元（只读），
   订正记录在本文件。
4. **配置侧**：`run/RELEASE-05/vis/configs/p1_m42.json` 未配 `qe_json`/`qe_name`
   ⇒ 属运行配置而非代码，未改（已在 §6.1 加告警使其不再静默）。

---

## 7 诚实边界（未独立验证的结论）

- **未跑生产全量构建与 ctest**：本机正有另一代理执行全档构建/全量 ctest，
  为避让资源与 `build/` 写冲突，只做了 **TU 级 `-fsyntax-only` 编译校验**
  与**独立探针/独立程序的行为验证**。**`ninja -C build` 与相关 ctest 未跑。**
- **未验证修复对 `orchestrator` 路径的效果**（该文件未改）。
- **`chi2` 截断未直接验证**：`p1_flux.json` 无 `chi2` 列，V7 用的是
  `flux_error/flux` 相对误差代理，**不是 EXP-04 的 `chi2<4`**。
- **T2/M2 等帧的逐位复现未达成**：本次只用**单个大锥**（中心 (83.7542, −5.4308)、
  半径 2.05°、mag≤22）覆盖 49 帧；T2/M1 与 T3/M1 逐位一致，其余帧
  `n_matched` 有差异（V0 中位与落盘差 4.5%），归因于锥覆盖/亚像素边界匹配，
  **未逐帧核对**。
- **未独立验证 XP 谱的绝对定标精度**：§4.4 引用的是仓内已核验文献综述的结论，
  本轮未做星表级交叉定标。
- **未验证 `Antlia V Pro Series B` 曲线本身的 provenance**（`filters.json`
  自述 `provenance.status="unverified"`、GAP-025）。
- **T3/M4 组 1.77 mag 帧间差的物理归因未做**（只证明了它与通带无关、与星族抽样无关）。
- 本轮**未做 git 写操作**；**未改** `eng/ci/checks.json`、`eng/ci/id_migration_map.json`；
  **未改**任何判据带、阈值或断言；**未重跑** normalize/mosaic/export 全链。

---

## 8 复现

```bash
cd "<repo root>"
# 1) Gaia 锥（本机 ~1 s / 144 MB RSS）
./run/SCI-401/bin/gaia_xp_dump "gaia/GaiaDR3SP" 83.7542 -5.4308 2.05 22.0 \
    run/M42-SCIA-ROOTCAUSE-01/evidence/gaia_cone_m42_all.csv
# 2) 生产代码本体探针（复算 ZP_syn）
gcc -O2 -fopenmp -I lib/infrastructure/gaia_xpsd_client/src -c \
    lib/infrastructure/gaia_xpsd_client/src/gaia_client.c -o run/M42-SCIA-ROOTCAUSE-01/build/gaia_client.o
g++ -O2 -std=c++17 -fopenmp -I lib/algorithms/photometry/cpp/src -I lib/algorithms/photometry/cpp/include \
    -I lib/infrastructure/gaia_xpsd_client/src run/M42-SCIA-ROOTCAUSE-01/code/probe_zp.cpp \
    lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp run/M42-SCIA-ROOTCAUSE-01/build/gaia_client.o \
    -lz -lpthread -o run/M42-SCIA-ROOTCAUSE-01/build/probe_zp
./run/M42-SCIA-ROOTCAUSE-01/build/probe_zp "/abs/gaia/GaiaDR3SP" 83.28357776392457 \
    -6.374475635068055 0.9335881787256821 6.0 16.0 \
    eng/packaging/config/filters.json "Baader R" \
    lib/algorithms/photometry/data/response_curves/qe_curves.json "KAF-16803"
# 3) 49 帧受控对照 / 共同星实验 / 曲线解析能红能绿
python3 run/M42-SCIA-ROOTCAUSE-01/code/step6_controls.py
python3 run/M42-SCIA-ROOTCAUSE-01/code/step7_common_stars.py
bash    run/M42-SCIA-ROOTCAUSE-01/code/run_verify_curve.sh
```

产物：`run/M42-SCIA-ROOTCAUSE-01/evidence/*.json`、`run/M42-SCIA-ROOTCAUSE-01/logs/`。

---

*本文件为只读复核后的订正记录：未改 `REPORT_paper.md` 与 `实验/m42-realdata/`，
未做 git 写操作；唯一代码改动是 `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp`
的曲线解析修复与 QE 告警。*
