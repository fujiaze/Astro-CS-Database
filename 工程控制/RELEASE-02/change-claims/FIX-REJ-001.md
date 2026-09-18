# 变更 claim：FIX-REJ-001 — Phase2 逐像素排异按输入集合大小 n 自适应选择（对标 WBPP/Siril）+ 卫星线残留根因订正

- 控制包：RELEASE-02 / 任务 FIX-REJ（排异专项：根因诊断 + 自动选择逻辑 + 映射表）
- 变更对象：
  - `docs/plugins/algorithms_phase2/12_rejection.md`（须冻结内置映射表、不适用判定表、用户表达式语法）
  - `ASTROCS_DESIGN.md` §4.5 第 5 条（CLI 合同；**本 shard 未改该文档**，只按订正后口径备 claim）
  - `lib/algorithms/coverage/include/astro/phase2/rejection.h` / `src/rejection.cpp`（新增 n=2 先验 σ 极值方法 + 方法感知 gate）
  - `lib/infrastructure/scheduler/src/module_adapters.cpp` `p2_op_reject`（group-level AUTO → 逐输出像素按 n 解析；消费 `reject` 配置块）
- 关联条目：`docs/science/REJECTION.md:20-21,47-53`（method/profile 词表与默认路由）；`contracts/schemas/phase_config_mosaic.schema.json:130`（`algorithm_rejection_method` enum）；`config/templates/mosaic.phase_config.json:7`
- 日期：2026-09-18
- 依据条款：`ASTROCS_DESIGN.md` §4.5（负责人补充要求，commit b96b5ff5）、§3.5（预检三级）；ENGINEERING_SPEC §3（科学正确性优先 + 变更 claim + 一致性回归）；AGENTS §8
- 状态：**草案，待前台/负责人裁决落地**（本 shard 不改代码、不构建）

---

## 1 问题描述（根因结论，一句话）

排异子系统**已完整接线**（`p2_reject_plan_resolve` → `p2_collect_candidate_stack` → `p2_reject_stack_ex` → 逐样本 mask → `p2_op_integrate` 逐样本剔除，全链 fail-closed），
但 **AUTO 只在「整组总帧数」上解析一次**（L4 m42 = 12 帧 ⇒ winsorized），而每个输出像素的**实际候选数 n 只有 2/3/4**：
`n=2` 被 `underdetermined_n=2` 白名单全接受、`n=3/4` 被 Siril 语义的 `N-r<=4` 最小保留闸判为"无可拒绝"，
⇒ **L4 m42 只有 2.29% 的像素具备任何排异能力**，卫星线所在的高权重/低 n 像素原样进入叠加。

根因链（四层，全部有 file:line + 数据）：

| # | 层 | 事实 | 证据 |
|---|---|---|---|
| R1 | 解析粒度 | AUTO 用 `nominal_contributors = 全组帧数(12)` **一次解析**为 winsorized；tile/像素不重选 | `module_adapters.cpp:4414-4418`（`req.request = P2_REJECT_AUTO`；`req.nominal_contributors = frames.size()`；`profile` 默认 `wbpp_current`）；`rejection.h:183-189` 明示 `wbpp_current` = group-level 一次解析 |
| R2 | 门槛 | 生产 gate 要求 `eligible_count > 2 ∧ eligible_count >= minimum_n`；n=2 一律 UNDERDETERMINED 全接受 | `module_adapters.cpp:4522-4524`；`rejection.cpp:1759-1765`；`rejection.h:155` 默认 `underdetermined_n=2` |
| R3 | kernel 能力 | winsorized / linear_fit / median_sigma 在 `N-r<=4` 时**立即停止拒绝**；n≤4 恒零拒绝（忠实 Siril 1.4.3） | `rejection.cpp:1346`（winsorized）、`:1457`（linear_fit）、`:1647`（median_sigma）；Siril 1.4.3 `src/stacking/rejection_float.c:188,239,273`、`median_and_mean.c:791,845,880` |
| R4 | 容错 | percentile 在 n≤4 全拒时**回退为全接受**（SCIENCE_FREEZE 容错），把 n=2 的 percentile 直接抹掉 | `rejection.cpp:1860-1867` |

**量化（RELEASE-01 L4 m42 实测 `p2_rejection.json`，证据目录 `run/RELEASE-01/e2e/evidence/l4__p2_m42/`）**：

| depth n | 像素数 | 占比 | 现行路径排异能力 |
|---|---|---|---|
| 2 | 98,304,000 | 71.7% | **零**（R2 白名单） |
| 3 | 2,359,296 | 1.7% | **零**（R3 闸） |
| 4 | 33,292,288 | 24.3% | **零**（R3 闸） |
| 5 | 524,288 | 0.4% | ≤1 样本 |
| 8 | 2,621,440 | 1.9% | ≤3 样本 |
| **合计** | 137,101,312 | 100% | **有排异能力仅 3,145,728 = 2.29%** |

实测 `stats`：`rejected_samples=21,976`（占全部样本 1.6e-4）、`underdetermined_pixels=101,649,311`（74.1%）、`plan.method=2`（`astrocs.winsorized_sigma_siril_1_4_3.v1`）、`profile=wbpp_current`。

**排除项（逐条给证据）**：

- ✗「AUTO 在 n 小时本就不可判」——部分成立但不是全部：`n=3/4` 若按 n 解析会走 **percentile**（WBPP 对 n<6 的选择），而 percentile **没有** `N-r<=4` 闸，实测 n=3/4 剔除率 1.000（见 §2.4）。所以问题不是"小 n 不可判"，而是**小 n 被错配了 winsorized**。
- ✗「阈值过宽」——继承阈值 4.0/3.0/8 与 WBPP 默认完全一致（`rejection.cpp:1053-1061` vs WBPP `BPP-parameters.js:740-743`：`percentileLow=0.2, percentileHigh=0.1, sigmaLow=4.0, sigmaHigh=3.0`），**不是**阈值问题。
- ✗「L4 走了 legacy 等权/未调 kernel」——**不成立**：L4 m42 config `weight_mode=1`（等权，`run/RELEASE-01/e2e/l4/configs/p2_m42.json`），但排异 mask 确实产生并被消费（`module_adapters.cpp:4899-4914` 逐样本 `sm==0 → continue`，`:4988` 尺寸/空洞校验）；`p2_integrated.json` 记 `sample_mask_consumed=true`。排异"没生效"是**能力为零**，不是"没接线"。
- ✗「只在帧级排异」——**不成立**：kernel 输入是逐像素的 `P2CandidateStack`（`module_adapters.cpp:4496-4534` 每像素一次 gather+kernel），mask 索引 `[slot*tile_span+p]` 逐样本。
- ✓ **配置面缺口（独立缺陷，非直接根因）**：`mosaic.config.algorithm_rejection_method`（schema `:130`）**没有任何 C++ 消费者**（全仓 grep 仅命中 schema/config_registry/docs）；session 配置的 `reject` 对象**只被校验类型、从不被读取**（`module_adapters.cpp:5556` 仅 `is_object` 校验），`p2_op_reject` 里方法是**硬编码 AUTO**（`:4414`）。⇒ 用户即便显式写 `"reject":{"method":"minmax"}` 也会被静默忽略。这正是 §4.5 第 5 条要堵的"静默改算法"。
- ✓ **large-scale 无法自造种子**：`large_scale.enabled` 默认 0（`rejection.cpp:1067`），且语义是"对**已有** pixel-level mask 做连通分量 grow"（`rejection.h:132-149`），不能替代像素 kernel 的种子检测。

---

## 2 证据

### 2.1 WBPP 一手源码（PixInsight 官方脚本，本机解出）

位置：`run/RELEASE-02/FIX-REJ/wbpp/BatchPreprocessing/`（由 `run/RELEASE-02/inbox/scripts.zip` 解出）。

- **自动选择** `BPP-FrameGroup.js:1304-1312` `bestRejectionMethod()`：
```js
  let n = this.activeFrames().length;
  if ( n < 6 )  return ImageIntegration.prototype.PercentileClip;
  if ( n <= 15 || this.imageType == ImageType.BIAS || this.imageType == ImageType.DARK )
     return ImageIntegration.prototype.WinsorizedSigmaClip;
  return ImageIntegration.prototype.LinearFit;
```
  ⇒ `n<6 → PercentileClip`；`6≤n≤15（或 BIAS/DARK）→ WinsorizedSigmaClip`；`n>15 → LinearFit`。
- **合法性约束** `BPP-FrameGroup.js:1229-1293` `rejectionIsGood()`（`n = this.fileItems.length`）：

  | 算法 | 约束 | 原文/行号 |
  |---|---|---|
  | auto | 恒允许 | `:1231-1232` |
  | NoRejection | **拒绝** | `:1237` "No pixel rejection algorithm has been selected" |
  | MinMax | **拒绝** | `:1239` "Min/Max rejection should not be used for production work" |
  | CCDClip | **拒绝**（已废弃） | `:1241` |
  | PercentileClip | 仅 n ≤ 8 | `:1252-1254` |
  | SigmaClip | 8 ≤ n ≤ 15 | `:1256-1260` |
  | WinsorizedSigmaClip | n ≥ 8 | `:1262-1264` |
  | AveragedSigmaClip | 8 ≤ n ≤ 10 | `:1266-1270` |
  | LinearFit | n ≥ 8；n<20 提示更差 | `:1272-1276` |
  | Rejection_ESD | n ≥ 8；n<20、n<25 提示更差 | `:1278-1283` |
  | Rejection_RCR | n ≥ 15 | `:1285-1287` |
- `ImageIntegration.prototype.auto = 999`：`BPP-global.js:180`。
- 默认参数：`BPP-parameters.js:740-743`（`percentileLow=0.2, percentileHigh=0.1, sigmaLow=4.0, sigmaHigh=3.0`）。
- 消费点：`BPP-processing.js:247,338`。

**WBPP 自身文案/判据不一致（如实登记，不照抄文案）**：
1. `LinearFit` 文案写 "requires at least 15 images"，代码判 `n < 8`（`:1273-1274`）；
2. `Rejection_ESD` 文案写 "requires at least 15 images"，代码判 `n < 8`（`:1279-1280`）；
3. `case Rejection_ESD` **缺 `break`**，n≥25 时贯穿落入 `case Rejection_RCR`（`:1278-1285`），故 ESD 实际仅在 n≥25 返回"可用"；
4. `bestRejectionMethod` 的 `n<6 → Percentile` 与 `rejectionIsGood` 的 "WinsorizedSigmaClip n≥8" 相互矛盾：**6≤n≤7 时 auto 选出的 winsorized 若被显式选择会被自家校验拒绝**。

### 2.2 Siril 一手源码（GPL-3.0，只读对照，未复制入仓）

版本：**1.4.3**（与 AstroCS 语义 ID `astrocs.winsorized_sigma_siril_1_4_3.v1` 对齐）。
取证：`https://gitlab.com/free-astro/siril/-/archive/1.4.3/siril-1.4.3.tar.gz`（17,274,718 B），解到 `/dev/shm/astrocs_rej/siril-1.4.3/`（仓外）。

- **无按 n 自动选择**：`src/core/settings.h:37-46` 仅定义 `rejection` 枚举 `{NO_REJEC, PERCENTILE, SIGMA, MAD, SIGMEDIAN, WINSORIZED, LINEARFIT, GESDT}`；默认 `src/stacking/stacking.c:641` `args->type_of_rejection = NO_REJEC;`；GUI 直接 `gtk_combo_box_get_active()`（`src/gui/stacking.c:132,248,271`）。⇒ Siril 把选择权完全交给用户，**不以 n 路由**。
- **拒绝只作用于 mean 叠加**：`src/stacking/stacking.c:131`（median/min/max 无排异）。
- **`N-r<=4` 最小保留闸（n≤4 零拒绝的直接来源）**：`src/stacking/rejection_float.c:188`（SIGMA/MAD）、`:239`（WINSORIZED）、`:273`（LINEARFIT）；ushort 路径 `src/stacking/median_and_mean.c:791,845,880`。
- **winsorized 语义**：`rejection_float.c:223-259`，`σ ← 1.134 × sd(winsorized@median±1.5σ)`，收敛判据 `|σ-σ0| ≤ 5e-4·σ0`。
- **percentile 语义（乘性、无 `N-r<=4` 闸）**：`rejection_float.c:31-44`，`median-pixel > median*plow → 低拒`，`pixel-median > median*phigh → 高拒`。
- **GESDT（generalized ESD）**：`rejection_float.c:301-348`，`max_outliers = (int)nb_frames*sig[0]`，`critical_value` 表按 `iter+removed` 索引。

AstroCS 复刻保真度：`rejection.cpp:1299-1358` 与 Siril `rejection_float.c:223-259` 逐式对应（1.134、±1.5σ、5e-4 收敛、`N-r<=4`）。

### 2.3 算法—权威出处表（DOI/NIST 章节，已核验）

| AstroCS 语义 ID | 算法 | 权威出处（已核验） | 核验级别 |
|---|---|---|---|
| `astrocs.generalized_esd_nist.v1` | generalized ESD | Rosner, B. 1983, *Technometrics* **25**(2), 165–172, **DOI 10.1080/00401706.1983.10487848**；NIST/SEMATECH e-Handbook **§1.3.5.17.3**（`https://www.itl.nist.gov/div898/handbook/eda/section3/eda35h3.htm`）：λ_i 公式、**"very accurate for n ≥ 25, reasonably accurate for n ≥ 15"** | 一手已取（DOI 解析、NIST 页全文） |
| （同上，单离群） | Grubbs / 已知 σ 极值 | NIST/SEMATECH e-Handbook **§1.3.5.17.1**（Grubbs 1969；Stefansky 1972）：`G = max|Y_i−Ȳ|/s` | 一手已取 |
| `astrocs.robust_mad_clip.v1` | sigma clip（median+MAD） | Beers, Flynn & Gebhardt 1990, *AJ* **100**, 32, **DOI 10.1086/115487**（`mad_std` 1.4826 一致性因子）；Astropy `astropy.stats.sigma_clip`/`mad_std` 文档 | DOI 解析确认；Astropy 文档已取 |
| `astrocs.winsorized_sigma_siril_1_4_3.v1` | winsorized sigma | Siril 1.4.3 `src/stacking/rejection_float.c:223-259`（冻结语义源）；winsorized 尺度理论：Wilcox, R. R. 2012, *Introduction to Robust Estimation and Hypothesis Testing*, 3rd ed., Academic Press（ISBN 978-0-12-386983-8） | 源码一手已取；教科书条目**登记**（未取全文） |
| `astrocs.averaged_sigma.v1` | averaged sigma | IRAF `imcombine` `avsigclip`（Tody, D. 1986, *Proc. SPIE* **627**, 733, **DOI 10.1117/12.968154**）：`σ = mean|x−x̄|·√(π/2)` | **登记**：IRAF 帮助页 403/404，未取得一手；公式与 `rejection.cpp:1361-1396` 一致 |
| `astrocs.percentile_siril.v1` | percentile clipping | Siril 1.4.3 `rejection_float.c:31-44`；IRAF `imcombine` `pclip` 同源 | 源码一手已取 |
| `astrocs.median_std_clip.v1` | median/MAD clip | Siril 1.4.3 `MAD`/`SIGMEDIAN` 分支（`rejection_float.c:174-222`）；Beers+1990（尺度） | 源码一手已取 |
| `astrocs.linear_fit_siril_1_4_3.v1` | linear fit clipping | Siril 1.4.3 `rejection_float.c:260-300`（排序后对 index 做最小二乘，`σ = mean|x−(a·i+b)|`） | 源码一手已取 |
| `astrocs.minmax.v1` | min/max 极值剔除 | WBPP `BPP-FrameGroup.js:1239`（明确不建议生产使用）；IRAF `imcombine` `minmax`；极值统计理论见 NIST §1.3.5.17.1 | WBPP 源码一手已取；IRAF 登记 |
| `astrocs.large_scale_rejection.v1` | 大尺度结构 grow | WBPP Large-Scale Pixel Rejection（`BPP-parameters.js` `largeScaleClipLow/High`，默认关闭） | 源码一手已取 |

### 2.4 合成卫星线注入实验（能红能绿）

脚本：`run/RELEASE-02/FIX-REJ/rejection_n_experiment.py`（Python 逐行语义复刻 + 自证锚点；**非生产**，不执行 C++）。
输出：`run/RELEASE-02/FIX-REJ/out/rejection_n_experiment.json`。

**保真自证**：25/25 PASS —— 复刻对 `lib/algorithms/coverage/tests/synthetic_gate.cpp:425-486`（M4A01）与 `tests/unit/p2_rejection_test.cpp:40` 的**已冻结 C++ 断言**逐条一致（含 n=3/4/5 percentile 的 accepted/low/high、n=4 全拒→UNDERDETERMINED 容错、n=6 显式 percentile ALL_REJECTED、auto n=6→winsorized）。

**A) 卫星线（+50σ）剔除率：现行 vs 按 n 自适应**

| n | 现行 method | 现行剔除率 | 现行残留率 | 按 n method | 按 n 剔除率 | 按 n 残留率 |
|---|---|---|---|---|---|---|
| 2 | winsorized | **0.000** | **1.000** | percentile | 0.502 | 0.498 |
| 3 | winsorized | **0.000** | **1.000** | percentile | **1.000** | 0.000 |
| 4 | winsorized | **0.000** | **1.000** | percentile | **1.000** | 0.000 |
| 5 | winsorized | 1.000 | 0.000 | percentile | 1.000 | 0.000 |
| 6/7/8/12 | winsorized | 1.000 | 0.000 | winsorized | 1.000 | 0.000 |
| 16/20/30 | winsorized | 1.000 | 0.000 | linear_fit | 1.000 | 0.000 |

⇒ **红**：现行路径在 n=2/3/4 完全不剔除（与 L4 m42 实测吻合）；**绿**：按 n 路由在 n=3/4 恢复剔除。

**B) n=2 percentile 的窗口（sky=100, σ=1）—— 关键反例**

| Δ/S | percentile 剔除污染帧 | 全拒回退→全接受 | 先验 σ 极值 |
|---|---|---|---|
| 0.05 | 0.000 | 0.000 | 0.503 |
| 0.10 | 0.000 | 0.000 | **1.000** |
| 0.30 | 1.000 | 0.000 | 1.000 |
| 0.50 | 0.493 | 0.507 | 1.000 |
| **0.80** | **0.000** | **1.000** | 1.000 |
| **1.00 / 2.00 / 5.00** | **0.000** | **1.000** | 1.000 |

⇒ **n=2 用 percentile 不可靠**：只在一个窄窗（约 `0.25S < Δ < 0.6S`）有效；**亮卫星线（Δ>S）必然全拒 → 触发 n≤4 容错 → 全接受**（R4）。这是"卫星线没被去除"的最直接机制。

**C) 各方法单离群能力矩阵（Δ=50σ）**

| method | n=2 | n=3 | n=4 | n=5 | n=6 | n=8 | n=12 | n=20 |
|---|---|---|---|---|---|---|---|---|
| percentile | 0.495 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| winsorized | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| median_sigma | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 |
| linear_fit | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 |
| generalized_esd | 0.000 | 0.721 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| minmax (min_kept=4) | 0.000 | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 | 1.000 | 1.000 |

**D) 拟议 n=2 先验 σ 极值剔除（`astrocs.extreme_value_clip_prior_sigma.v1`，新）**

`z_i=(v_i−prior_sky)/prior_sigma`，`z>k_hi`→高拒、`z<−k_lo`→低拒；`k=5` 时：

| n | percentile 剔除率 | percentile 全拒回退 | 先验 σ 剔除率 | 先验 σ 假阳率 |
|---|---|---|---|---|
| 2 | 0.511 | 0.488 | **1.000** | 0.0000 |
| 3 | 1.000 | 0.000 | 1.000 | 0.0000 |
| 4 | 1.000 | 0.000 | 1.000 | 0.0000 |

⇒ n=2 唯一可用的确定性方法是**已知先验天光/噪声的单离群检验**（Grubbs 统计量的已知方差变体，NIST §1.3.5.17.1；多重比较用 Bonferroni `k=Φ⁻¹(1−α/(2N))`，N=帧数，α=0.05）。

**E) 订正前后在 L4 m42 实测 depth 分布上的排异有效像素占比**

| 路径 | 有排异能力的像素 | 占比 |
|---|---|---|
| 现行（group AUTO + `underdetermined_n=2`） | 3,145,728 | **2.29%** |
| 订正（按 n + n=2 先验 σ） | 137,101,312 | **100%** |

---

## 3 按 n 的内置映射表（与 WBPP 对照 + 取舍理由）

**n 的定义（冻结）**：`n` = **该输出像素的输入集合大小** = 资格层（finite ∧ support>0 ∧ 未被 mask 剔除）后的候选样本数，即 `module_adapters.cpp:4521` 的 `cand`。**不是** tile depth，**不是**全组帧数。

| n 档 | 方法（语义 ID） | 参数 | 依据 / 与 WBPP 对照 |
|---|---|---|---|
| 0 | —（无样本，不输出像素） | — | — |
| 1 | `none`（UNDERDETERMINED，provenance 显式登记） | — | 单样本无对照量；WBPP 对 n=1 无定义 |
| **2** | **`astrocs.extreme_value_clip_prior_sigma.v1`（新增）** | `k_hi=k_lo=Φ⁻¹(1−α/(2N))`，α=0.05 | WBPP auto 在此档选 percentile；**实测 percentile 在 Δ>S 时全拒→容错全接受（§2.4 B）**，故必须换成先验 σ 极值。WBPP 拒 MinMax 是"有更好方法时"的工程判断，n=2 无替代；NIST §1.3.5.17.1 提供统计依据。**代价**：需 prior_sky/prior_sigma 输入（见 §9） |
| **3–7** | `astrocs.percentile_siril.v1` | low 0.2 / high 0.1（继承） | WBPP auto：n<6 → Percentile；**WBPP 自家 validator 要求 winsorized n≥8**，故把 percentile 延到 7，消解 WBPP 6≤n≤7 的自相矛盾（§2.1 不一致 4） |
| **8–15** | `astrocs.winsorized_sigma_siril_1_4_3.v1` | 4.0/3.0, 8 iter（继承） | WBPP auto：6≤n≤15 → Winsorized；validator n≥8 → 取交集 8..15 |
| **≥16** | `astrocs.linear_fit_siril_1_4_3.v1` | 5.0/3.5, 8 iter（继承） | WBPP auto：n>15 → LinearFit；16≤n<20 按 validator 发 WARN（§4） |

**仅显式可选（不参与 auto 路由）**：`robust_mad_clip`（sigma）、`averaged_sigma`（8–10）、`generalized_esd`（≥25）、`median_sigma`、`rcr`（≥15）、`minmax`、`none`。

**与 WBPP 的差异登记（3 处，均须负责人知悉）**：
1. n=2 用先验 σ 极值而非 percentile（WBPP 无此方法）——理由是实测 percentile 在亮线上失效；
2. 6≤n≤7 用 percentile 而非 winsorized（WBPP auto 选 winsorized，但 validator 判其不可用）——取 validator；
3. 16≤n<20 用 linear_fit 但发 WARN（WBPP auto 选 linear_fit 且 validator 也发提示）——与 WBPP 一致。

---

## 4 不适用判定表（WARN 级；对齐 §3.5 预检分级，**不新增阻断级别**）

`WARN+确认` = 显示检查页面 → 交互 `yes` 继续；`-y` 跳过确认；`-force` 跳过确认并强制执行（设计 §3.5:193）。
`WARN` = 只提示，不请求确认。**error 级仅用于方法名不存在 / 表达式语法错误 / 缺终止分段**（强制阻断，`-y` 也不行）。

| 方法 | WARN+确认 区间 | 依据 |
|---|---|---|
| `none` | 恒（排异是必需步骤） | WBPP `:1237`；DESIGN §4.5.1 |
| `minmax` | 恒（"should not be used for production work"） | WBPP `:1239` |
| `percentile` | n > 8 | WBPP `:1252-1254` |
| `percentile` | n ≤ 4 且发生全拒回退（provenance 标红） | `rejection.cpp:1860-1867`；实测 §2.4 B |
| `sigma`(robust_mad) | n < 8 或 n > 15 | WBPP `:1256-1260` |
| `winsorized_sigma` | n < 8 | WBPP `:1262-1264` |
| `winsorized_sigma`/`median_sigma`/`linear_fit` | n ≤ 4（kernel 恒零拒绝） | `rejection.cpp:1346/1457/1647`；Siril `N-r<=4` |
| `averaged_sigma` | n < 8 或 n > 10 | WBPP `:1266-1270` |
| `linear_fit` | 8 ≤ n < 20（WARN，不请求确认） | WBPP `:1272-1276` |
| `generalized_esd` | n < 25（n<15 更强）；NIST 仅在 n≥15 起"reasonably accurate" | WBPP `:1278-1283`；NIST §1.3.5.17.3 |
| `rcr` | n < 15 | WBPP `:1285-1287` |
| 自定义分段中的任一档 | 命中上表即 WARN（**不阻塞**，按用户定义执行） | DESIGN §4.5.5 |

---

## 5 用户自定义表达式 / 分段映射（语法冻结提案）

**形式 A（推荐，JSON 原生，schema 可锁）**：
```json
"reject": {
  "method": "auto",
  "method_map": [
    {"n_max": 2,  "method": "extreme_value_clip_prior_sigma"},
    {"n_max": 7,  "method": "percentile"},
    {"n_max": 15, "method": "winsorized_sigma"},
    {"method": "linear_fit"}
  ],
  "params": { "percentile": {"low_fraction": 0.2, "high_fraction": 0.1} }
}
```
**形式 B（字符串糖，等价）**：
```json
"method_expr": "n<=2->extreme_value_clip_prior_sigma; n<=7->percentile; n<=15->winsorized_sigma; else->linear_fit"
```

**求值规则（冻结）**：
1. 语法：`expr := clause (";" clause)* [";" "else->" method]`；`clause := "n" op INT "->" method`；`op ∈ {"<","<=",">",">="}`；无算术、无嵌套、无其他变量；空白不敏感；方法名大小写敏感。
2. `method_map` 键只允许 `n_min`/`n_max`/`n_lt`/`n_le`/`n_gt`/`n_ge`/`method`；同一元素不得同时出现两个 n 条件键。
3. **first-match-wins**，按数组/子句顺序求值。
4. 必须存在**无条件终止项**（形式 A 的无 n 键元素 / 形式 B 的 `else`），否则 `EXPR_NO_TERMINAL` → **error → 强制阻断**。
5. `n_min > n_max` → `EXPR_INVALID_RANGE`；方法名不在冻结词表 → `EXPR_UNKNOWN_METHOD`；均为 **error → 强制阻断**。
6. 存在 `method_map`/`method_expr` ⇒ **覆盖内置映射表**；命中 §4 WARN 表 → WARN（不阻塞）。
7. 形式 A 与 B 解析为**同一有序规则列表**；provenance 记录 `rule_index`。

**优先级**：`method_expr`/`method_map` > 显式单一 `method` > `auto`（留空/`0`/`"auto"`）。三者互斥，同时出现 → error。

---

## 6 CLI 合同与 provenance

- **字段**：phase_config `mosaic.config.algorithm_rejection_method`；session `reject.method` / `reject.method_map` / `reject.method_expr`。
- **auto**（留空 / `0` / `"auto"`）⇒ 生成马赛克 HiPS 时**按该输出像素的 n** 查内置映射表（§3）。
- **显式单一方法** ⇒ 按用户指定执行，不被 auto 覆盖；命中 §4 ⇒ `WARN+确认`（`-y` 跳过 / `-force` 强制执行）。
- **表达式/分段** ⇒ 覆盖内置表；命中 §4 ⇒ `WARN` 不阻塞。
- **不得静默改算法、不得静默降级**；error 级强制阻断（`-y` 无效）。
- **provenance**（写入 `rejection` 面，独立于权重面）：
  `{ "n": <int>, "requested": "<auto|method|map|expr>", "resolved_method": "<semantic_id>", "params": {...}, "rule_index": <int|null>, "profile": "<...>", "warn_codes": ["W_N_LT_8", ...], "force_used": <bool>, "fallback": "<null|all_rejected_underdetermined|prior_sigma_unavailable>" }`
- 每像素逐样本 reason 落盘沿用现有 `p2_rejection_sample_mask.bin`（不变）。

---

## 7 订正前 / 订正后

| 位置 | before | after |
|---|---|---|
| AUTO 解析粒度 | group-level，`nominal_contributors = 全组帧数`（`module_adapters.cpp:4415`） | **逐输出像素**，`nominal_contributors = eligible_count`（每像素 n），按 n 缓存 plan |
| n=2 | `underdetermined_n=2` 白名单 → 全接受（`rejection.cpp:1759`） | 走先验 σ 极值方法；无 prior σ 时 UNDERDETERMINED + provenance `fallback` |
| n=3/4 | winsorized（`N-r<=4` 零拒绝） | percentile（n=3/4 实测 100% 剔除污染帧） |
| n=6/7 | winsorized（WBPP validator 判不可用） | percentile |
| n≤4 全拒 | 一律回退全接受（`rejection.cpp:1860-1867`） | 保持回退，但 provenance 标 `all_rejected_underdetermined` + WARN（**阈值/容错数值不变**） |
| 配置消费 | `reject` 块只校验不读；`algorithm_rejection_method` 无消费者；方法硬编码 AUTO | `reject.method/method_map/method_expr` 生效；auto 走 §3 映射 |
| 不适用组合 | 无提示（静默） | §4 WARN 表 + `-force` 语义 |
| provenance | 仅 `plan.method/minimum_n/underdetermined_n/normalization`（`module_adapters.cpp:4602-4606`） | 增 `n / requested / rule_index / warn_codes / force_used / fallback` |

---

## 8 影响面

- **文档**：`docs/plugins/algorithms_phase2/12_rejection.md`（冻结映射表/判定表/表达式语法）；`docs/science/REJECTION.md:20-21,47-53`（词表与默认路由）——**本 shard 未改**。
- **合同/schema**：`contracts/schemas/phase_config_mosaic.schema.json:130` `algorithm_rejection_method` 需扩为 `auto | <method> | {method_map|method_expr}`；`reject` 块 schema 化。
- **实现**：
  - `rejection.h/.cpp`：新增 `P2_REJECT_EXTREME_PRIOR`（或复用 `P2_REJECT_MINMAX` 加 `prior_sky/prior_sigma` 字段——**建议新增方法**，避免改动 minmax 冻结语义）；`p2_reject_plan_resolve` 增 `underdetermined_n` 方法感知（或调用方直接用 `minimum_n`）；
  - `module_adapters.cpp` `p2_op_reject`：逐像素解析 + 消费 `reject` 配置块 + provenance 扩展；
  - `stage2.cpp`：同口径（`astrocs_adaptive` 已是 per-tile，改为 per-pixel）。
- **阈值/科学冻结**：**不动** 4.0/3.0/8、0.2/0.1、alpha 0.05/max 10、`underdetermined_n=2`、n≤4 容错数值；只改**选择逻辑**与新增方法。`p2_reject_plan_thresholds_inherited`（`rejection.cpp:2118`）须同步接受新方法。
- **性能**：逐像素 plan 解析需按 n 缓存（最多 `n_max` 个 plan），成本可忽略；n=2 先验 σ 需 prior_sky/prior_sigma（见 §9 代价）。
- **一致性回归**：新增 n=2/3/4 注入正例（卫星线必剔）+ 无污染负例（不得误剔）；1 worker vs N worker 一致；L4 重跑视觉验收（DESIGN §4.5 验收要求）。

---

## 9 实现方案（本轮不改代码）

1. **`P2_REJECT_EXTREME_PRIOR`（新方法）**
   - 语义 ID `astrocs.extreme_value_clip_prior_sigma.v1`；输入 `prior_sky`、`prior_sigma`（逐样本或逐像素标量）；
   - `z_i = (v_i − prior_sky)/prior_sigma`；`z_i > k_hi` → REJECTED_HIGH；`z_i < −k_lo` → REJECTED_LOW；`k = Φ⁻¹(1−α/(2N))`，α=0.05，N=全组帧数（Bonferroni）；
   - `minimum_n = 1`；仅当 `prior_sigma > 0` 且 finite 时可用，否则 status=UNDERDETERMINED + provenance `fallback=prior_sigma_unavailable`。
   - **prior 来源（推荐）**：`prior_sky(x)` = 该帧该 tile 内 31×31 邻域 corrected 值的中位数；`prior_sigma(x)` = 1.4826×同邻域 MAD。二者只用 clean 像素主导的稳健统计，**不需新增产品输入**；代价 = 每帧每 tile 一次邻域中位数/MAD 扫描（可增量/可向量化，需 benchmark 定档）。
   - **备选**：直接用 Phase1 ivar/variance（`σ_i=1/√ivar_i`）——更准，但需把 ivar 面接入 `p2_op_reject`（当前只读 corrected 值）。
2. **逐像素 plan 解析**：在 `p2_op_reject` 的 `for p` 内用 `eligible_count` 解析（按 n 缓存），替换 `group_plan`。
3. **配置消费**：解析 `reject.method/method_map/method_expr` → 规则列表；实现 §5 求值与 §4 WARN 判定。
4. **CLI**：`-y` 跳过确认、`-force` 强制执行（`parser.cpp:36` 已注册 `-force`；若负责人要 `--force`，需加 alias）。
5. **测试**（新增）：
   - 单测：n=2 先验 σ 正例/负例；n=3/4 percentile 正例；n=6/7 路由为 percentile；表达式解析 error 用例（缺终止、未知方法、非法区间）；provenance 字段完整；
   - 集成：`p2_op_reject` 逐像素 plan（1 vs N worker 一致）；
   - 端到端：L4 m42 重跑 → 视觉验收无卫星线残留。

---

## 10 请求裁决

1. 批准 §3 内置映射表（含 **n=2 新增先验 σ 极值方法** 与 **6≤n≤7 取 percentile** 两处对 WBPP 的偏离）；
2. 批准 §4 WARN 分级（对齐 §3.5，不新增阻断级别）与 §5 表达式语法冻结进 `docs/plugins/algorithms_phase2/12_rejection.md`；
3. 批准 §7「不动冻结阈值、只改选择逻辑 + 新增方法」的最小改动面；
4. 指示 n=2 prior 来源取「31×31 稳健先验」（无新输入，需 benchmark）还是「Phase1 ivar 面接入」（更准，需合同扩面）。
