# Rejection Algorithms (ALG-REJ)

> 上游：ASTROCS_DESIGN.md §5.5（逐像素排异）

> ID: ALG-REJ-001  范围: ALG-REJ-001..008  上游 SCI: SCI-REJ-001  状态: DERIVED  模块: phase2/rejection

## 1 上游 SCI 与输入输出

- 上游: `SCI-REJ-001..008` (7方法+wbpp路由+INVALID/UNDERDETERMINED分层)
- 输入: candidate stack (values/support/weights/frame_ids) + P2RejectionPlan (method/profile/thresholds)
- 输出: per-sample reason (P2_REASON_*) + stack status (P2_STATUS_*) + low/high计数

### 1a 状态/原因枚举取值（唯一事实源）

| 名称 | 值 |
|---|---|
| P2_REASON_ACCEPTED | 0 |
| P2_REASON_REJECTED_LOW | 1 |
| P2_REASON_REJECTED_HIGH | 2 |
| P2_REASON_UNDERDETERMINED | 3 |
| P2_STATUS_OK | 0 |
| P2_STATUS_MIN_SAMPLES | 1 |
| P2_STATUS_ALL_REJECTED | 2 |
| P2_STATUS_INVALID_INPUT | 3 |
| P2_STATUS_UNDERDETERMINED | 4 |
| P2_STATUS_INVALID_CONFIGURATION | 5 |
| P2_STATUS_INVALID_METHOD | 6 |
| P2_STATUS_INTERNAL_ERROR | 7 |

- 唯一事实源: `lib/algorithms/coverage/include/astro/phase2/rejection.h`
  (`enum P2RejectReason` = `:94-99` / `enum P2RejectStatus` = `:102-111`)；
- `P2_STATUS_MIN_SAMPLES=1` 的语义 = **候选数 < 调用方显式 `min_samples`，或 `count==0`**
  （`rejection.h:104`、`rejection.cpp:2018/:2250-2259`）；样本不足的判定态用
  `P2_STATUS_UNDERDETERMINED=4`（`n <= underdetermined_n` ∨ `n < minimum_n`，
  `rejection.cpp:2064-2073`）。**`underdetermined_n` 的默认值随 profile 而定**（`rejection.cpp:1218-1225`）：
  生产档 `astrocs_adaptive_pixel` ∧ AUTO ⇒ 3；该档 ∧ `extreme_value_clip_prior_sigma` ⇒ 1；
  `wbpp_2_9_1`/`wbpp_current`/`astrocs_adaptive` ⇒ 2。**本层无 `NO_CANDIDATES` 状态**
  （该名属积分域 `P2IntegrateStatus`，`integrate.h`）。
- 机器门: `eng/tools/docs_machine_consistency.py :: rejection_status_full_set`（全集合比对，禁 subset）。


## 2 离散公式

```text
方法集（11 个显式方法 + AUTO，`rejection.h:45-62`）：
  NONE=0 / SIGMA=1 / WINSORIZED_SIGMA=2 / AVERAGED_SIGMA=3 / LINEAR_FIT=4 /
  GENERALIZED_ESD=5 / RCR=6 / PERCENTILE=7 / MEDIAN_SIGMA=8 / MINMAX=9 /
  AUTO=10（只在规划层解析，永不进 kernel）/ EXTREME_VALUE_PRIOR_SIGMA=11
  （显式 opt-in，永不参与任何 AUTO 路由）。

F1: plan resolve（N = 该输出像素的几何覆盖帧数，一次解析；`rejection.cpp:1182-1288`）:
      生产档 astrocs_adaptive_pixel（`rejection.cpp:1148-1158`）:
        1≤N≤3 → none（不排异，直接逆方差加权积分）; 4≤N≤5 → percentile 0.2/0.1;
        6≤N≤15 → winsorized 4/3/8; N≥16 → linear_fit 5/3.5/8
      对照档 wbpp_2_9_1 / wbpp_current / astrocs_adaptive（`rejection.cpp:1262-1268`）:
        N<6 → percentile; 6≤N≤15 → winsorized; N>15 → linear_fit
      min/max 不用于生产（AUTO 路由禁止产出 min/max 与 NoRejection，fail-closed；
      `rejection.cpp:1167-1179/:1272-1278`）
      与 WBPP 档界的差异及其依据见 docs/science/REJECTION.md
F2: sigma: median ws, MAD→σ=1.482602218505602·MAD, thresholds 4.0 low /3.0 high 8iter
      （低侧阈 4.0 > 高侧阈 3.0 ⇒ **高侧更敏感**，正离群优先被剔；`rejection.cpp:1229`）
F3: winsorized: winsor at σ阈, 再sigma
F4: linear_fit: 残差尺度 = **平均绝对残差** mean|stack[i]−fit(i)|（`rejection.cpp:1665-1668`），
      阈值 5.0 low /3.5 high，8 iter；**不是** MAD 尺度
F5: ESD: Rosner α=0.05 max10；样本标准差用**单次** sqrt（`rejection.cpp:1730-1733`）
F6: RCR: Maples Chauvenet，3-pass 链（Median+DoubleLine → Median+68th → Mean+StdDev，
      `rejection.cpp:1785-1806`）；large_scale trail 仅扩展结构、compact 不生长
F7: 状态机: n <= underdetermined_n → UNDERDETERMINED（生产档默认 3、对照档 2）；
      non-finite values/weights → INVALID_INPUT hard fail（`rejection.cpp:2025-2034`）
```

来源: `rejection.cpp:1-12`（冻结头）/ `:1182-1288`（规划）/ `:1465-1910`（方法核）/
`:1996-2201`（生产 kernel）；`rejection.h:45-62,94-118,203-239`

## 3 伪代码

```text
function p2_reject(stack, plan):
  if plan.method==AUTO → INVALID_METHOD
  n_nominal = plan nominal contributors
  method = resolve_profile(n) # 生产默认 astrocs_adaptive_pixel（自研）；对照档 wbpp_2_9_1
  if n <= underdetermined_n or n<minimum_n → status=UNDERDETERMINED, reason=UNDERDETERMINED
     # underdetermined_n 默认：生产档 3 / 对照档 2（rejection.cpp:1218-1225）
  switch method:
    None: all ACCEPTED
    Sigma/Winsorized/Averaged: iterative σ clipping low/high 8iter
    LinearFit: robust linear fit残差
    ESD: generalized ESD α0.05
    RCR: robust Chauvenet
    Percentile: low 0.2/high0.1
    Minmax: 1/1/4
  large_scale: trail生长 if enabled && extended structure
  return reasons + status OK/ALL_REJECTED/INVALID
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| n <= underdetermined_n（生产档 3 / 对照档 2） | UNDERDETERMINED 全接受（recall=0 显式） |
| non-finite values/weights | INVALID_INPUT hard fail |
| method AUTO | INVALID_METHOD |
| n<minimum_n | UNDERDETERMINED |
| 空栈（count==0） | MIN_SAMPLES（值 1；本层无 NO_CANDIDATES） |
| 全拒且 n≤4 | UNDERDETERMINED 全接受（容错域写死 n≤4；rejection.cpp:2185-2195） |
| 全拒且 n>4 | ALL_REJECTED |

## 5 确定性与归约

- 排序确定性：ESD 等值 tie-break = **较小 frame_id**（`rejection.cpp:1747-1752`）；
  linear_fit 排序键 = **(value, 原始索引)** 字典序（`rejection.cpp:1647-1653`）；
  minmax 比较器**仅按 value**（`std::sort` 非稳定 ⇒ 等值样本的置换不变性未承诺，
  `rejection.cpp:1878-1910`）。ESD/RCR 迭代为固定顺序；无跨像素归约。

## 6 复杂度

- O(n log n) 排序/ESD/RCR

## 7 CPU-only 后端策略（V5）

- 仅 CPU: 逐像素独立, worker pool（按 affinity）按像素行带并行, **线程数取自 benchmark profile**；7 方法逻辑与线程划分无关（每像素独立决策树）；large_scale 结构半径仅邻域读, 无跨像素写。

## 5c SIMD 安全与取消点

- 排序/median/MAD 为固定输入序选择（tie-break 口径见 §5）；ESD/RCR 迭代为固定序统计（逐像素局部数组）；各方法阈值比较逐样本独立（SIMD 安全：栈数组连续无别名）。
- 取消点: 像素行带粒度检查; 取消时该行带 accepted 掩膜不写(整帧重做, 掩膜以帧为原子单元)。

## 8 参考实现/Oracle

- NIST ESD 120/120; 卫星线注入 recall=1.0

## 9 容差来源

- σ阈 4.0/3.0 预冻结, 归约确定性.

## 10 关联 ARC/API/TST

- API: rejection.h: p2_reject, p2_reject_plan_resolve
- TST: synthetic_gate 74/74, NIST ESD

## 11 数据布局

- 输入：每像素候选栈 `values[], weights[], support[], accepted[], frame_id[]`
  （`P2EligibilityGatherInput`，`rejection.h`）；**帧主序** `value_stride=support_stride=chunk_pixels`
  （stage2 `process_cpu_pixel_parallel`：`gidx=s·stride+pixel`），ACR/kernels 亦共享该布局。
- 规划层：`p2_reject_plan_resolve` 以 `n`（nominal contributors，一次解析）路由到 method
  （`rejection.h:227-239` request / `:203-224` plan），路由 = 一次解析的 n；同 n 方法确定性一致。
- **布局单位**：`values` = 面亮度 ADU·sr⁻¹、`weights` = (ADU·sr⁻¹)⁻²、`support` 无量纲 [0,1]
  （单位权威 = `docs/science/REJECTION.md` §3 + DATA_SEMANTICS §22）。
- 输出：reject plan（method+阈值）、per-sample reason（`P2_REASON_*`）、`P2_STATUS_*`；
  结构生长（trail）/紧凑（cosmic 不生长，`rejection.cpp:2342-2433`）。
- 内存：候选栈 O(n)；逐像素拒绝就地；无整帧副本。

## 12 误差预算

- 阈值冻结：`sigma/winsorized/averaged/median_sigma 4.0/3.0/8`，`linear_fit 5.0/3.5/8`，
  `percentile 0.2/0.1`，ESD alpha 0.05/max 10，minmax 1/1/4
  （冻结头注释 `rejection.cpp:1-12`；规划层默认值 `rejection.cpp:1226-1251`）。
- 数值：FP64 全链路；ESD/RCR 参照 NIST 独立实现验证；归一化 `astrocs_median_center_v1` 默认
  （`rejection.cpp:1226-1228`）。**正向约束**：ESD 标准差单次 `sqrt`；非有限 values/weights
  一律 `INVALID_INPUT`，该码即最终读法（`rejection.cpp:2025-2034`）。
- 归约确定性：按固定顺序归约；`n <= underdetermined_n` 全接受、`recall=0` 显式（不做伪剔除）。
- 阈值不变量：同 `n` 的 `plan.resolve` 输出 method 唯一（`synthetic_gate`）；非有限
  weights/support → `INVALID_INPUT` hard fail。
- 误差排序：**数值 FP64 ≪ 统计阈值(冻结) ≪ 门禁容差**；卫星线受控注入 recall=1.0。
- 各 F 映射：`p2_collect_candidate_stack`→`rejection.cpp:1372-1455`（gather，共享）；
  `reject_linear_fit_impl`→`rejection.cpp:1615-1709`；`reject_esd_impl`→`rejection.cpp:1712-1782`
  （`rejection_oracle_compare` NIST 对照）；计划路由→`p2_reject_plan_resolve`（`synthetic_gate`）。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- Generalized ESD：Rosner 1983, Technometrics 25, 165；NIST/SEMATECH e-Handbook §1.3.5.17/§7.1.6。
- RCR：Maples et al. 2018, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d；arXiv:1807.05276）；Konz & Reichart 2023, arXiv:2301.07838。
- winsorization/稳健尺度：Hoaglin et al. 1983；Tukey biweight Beaton & Tukey 1974。
- 次生参考实现（只用于掩码逐元素对拍，**不是**核语义来源）：Siril（GPL-3.0）stacking/rejection；
  档界来源：PixInsight WBPP **2.5.9** `bestRejectionMethod`（官方更新包 sha1 `712cc7c3fdb523643ad0e685104592d511996f82`，
  `WeightedBatchPreprocessing-engine.js:1421-1429`；非学术软件来源）。
- 阈值表（4.0/3.0/8、5.0/3.5/8、0.2/0.1、α=0.05/max10、1/1/4）为 Project-defined 冻结值（本文件 §12），文献不提供门值。
- **方法核来源归属（订正后）**：`linear_fit` 的语义来源 = PixInsight ImageIntegration 官方**式[21]/式[22]**
  （横轴 = 排序秩 `0…N−1`、带升序约束）+ *Numerical Recipes* 3rd ed. **§15.7.3**（`Fitmed`，L1 稳健拟合）；
  `winsorized_sigma` = 官方式[18]/[19]（Huber 体系）；`percentile` = `docs/science/REJECTION.md` §5/§8a 的本层定义
  （IRAF `pclip` 与本层 `percentile` 同名不同义，并列说明见该文件 §8a）；`sigma` = Astropy `sigma_clip(median+mad_std)` 语义；
  ESD = Rosner 1983 + NIST 实现；RCR = Maples et al. 2018 + 官方 RCR 2.4.7 行为对照。
  **Siril 1.4.3 是次生参考实现**（`percentile` 与 `src/stacking/rejection_float.c:31-44` 逐式等价，见该文件 §8a；
  `rejection.cpp:1082-1098` 的 `*_SIRIL` id 与核注释 `:1514/1614` 是**冻结的对拍标识**），**不作为核语义归属**
  （oracle 保留：用未修改的 Siril 1.4.3 官方源码做掩码逐元素对拍）。
  IRAF `combine`/`imcombine` 共用引擎的参数文件 `noao/imred/ccdred/combine.par`（iraf-community/iraf@main）
  的 `reject` 值域为 `none|minmax|ccdclip|crreject|sigclip|avsigclip|pclip`，另有 `nlow=1`/`nhigh=1`/`nkeep=1`/
  `mclip=yes`/`lsigma=3.`/`hsigma=3.`/`pclip=-0.5`/`sigscale=0.1`/`grow=0`；
  **该引擎没有 `lfitclip` 也没有 `winsorize` 参数**（旧引文中这两个名字在 IRAF 中不存在，已删去），
  只作方法族命名来源，不作核语义依据。
- **Tukey biweight（Beaton & Tukey 1974）在本层无对应实现**：本层 11 个方法均不使用 biweight 核，
  该引用只作稳健估计背景。
- **预测残差方差阈值/最优检验**：Zackay et al. 2016（DOI 10.3847/0004-637X/830/1/27）为方法学候选，
  本层当前不采用。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

