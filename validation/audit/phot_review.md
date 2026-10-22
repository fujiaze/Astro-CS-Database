# phot 独立审核：catalogue / sources / response / photometry 及其消费路径

- 审核人：`phot`（Agent）
- 审核对象修订：`e171d8995704f6427d7017931e809d8229798d09`（`rebuild/acsd-v2`）
- 纪律：**只读审核**。未修改任何实现文件、未做任何 git 写操作；只新建本文件与
  `validation/audit/phot_review.json`。未阅读旧系统实现（`实验/ eng/ lib/ out/ run/ gaia/ testdata/`）。
- 独立性声明：**`src/response` 与 `src/photometry` 由本审核人本人实现**（提交 `6d6d6a348`），
  对这两个模块的结论是**自审**，不构成独立复核。`src/catalogue`、`src/sources` 与消费路径
  （`apps/cli`、`apps/contract`、`src/workflows`）非本审核人所写，结论为独立审核。
- 运行验证树会重写自身的 observed/comparison 证据文件（`validation/*/observed/*.json`、
  `validation/run_summary.json`）；这是验证框架的既有行为，本审核未新增或改写这些文件的内容。

## 1. 阅读范围

`/workspace/acsd-work/brief.md`；`docs/engineering/rebuild/{README,SYSTEM,OBJECTS,API,EXECUTION,VERIFICATION}.md`；
`docs/science/foundation/{OBSERVATION_COORDINATES,GAIA_RESPONSE,SPHERICAL_DRIZZLE,STATISTICS_SKY_COADD,INDEPENDENT_ORACLES}.md`；
`include/acsd/contracts.hpp`（全文）；实现：`src/catalogue/{columns,index,provider,sky_geometry}.cpp`、
`src/sources/{astrometry*,psf*,measure}.cpp`、`src/response/*`、`src/photometry/*`、
`apps/cli/commands.cpp`、`apps/contract/driver.cpp`、`src/workflows/normalize.cpp`、
`schemas/{contract,science-input,config}.schema.json`；底座 `src/numerics/linalg.*`、`src/model/*`、
`src/operators/*`、`src/uncertainty/*`。

## 2. 实际运行的命令

| 命令 | 结果 |
|---|---|
| `python3 validation/run_all.py` | C01 pass、G01 pass、U01 pass、**F01 fail**、S01/P01/R01/E01/V01/W01/X01 pending_implementation |
| `cmake -S validation/cat -B /tmp/audit/catbuild -G Ninja && cmake --build …` | 构建通过 |
| `/tmp/audit/catbuild/catalogue_check` | ALL CHECKS PASSED（描述符盒、行的排序、游标、区域查询、lookup 顺序与缺失标识） |
| `/tmp/audit/catbuild/astrometry_check` | ALL CHECKS PASSED（188 个匹配源，最差角误差 1.00787″；空图与无解返回 `unsolved`） |
| `python3 validation/cat/astrometry_check.py`（astropy 7.0.1） | pass，最大差 0.2931617464924404″（容差 2″） |
| `python3 validation/cat/epoch_check.py`（astropy 7.0.1 / ERFA） | 角度最大差 1.92e-5″；**距离最大差 0.0514 au**（见 F09） |
| `/tmp/audit/epoch_case.cpp`（自建，仅用公开 API） | 见 F03 |
| `/tmp/audit/psf_probe.cpp`（自建诊断） | 像素积分本身正确（1×1 像元、σ=1.2 → 0.417517；远源 → 0） |
| `/tmp/audit/f01_probe.cpp`（`validation/native/f01_driver.cpp` 的 /tmp 副本 + 诊断打印） | 见 F01 |
| schema 与合同驱动的操作名比对（python3） | 见 F05 |

## 3. 逐条追踪（科学量 → producer → API → consumer → 持久化 → 独立真值）

| 追踪对象 | producer / 类型 | API | consumer | 持久化 | 独立真值 | 结论 |
|---|---|---|---|---|---|---|
| 目录星历元传播 | `catalogue::propagate_star`（`src/catalogue/sky_geometry.cpp:81`） | `CatalogueProvider::query`（带 epoch）/ `lookup`（无 epoch） | `src/sources/astrometry.cpp:641`（验证场，带 epoch ✓）、`:620`（假设，用 lookup ✗） | `CatalogueStar.epoch_year` | astropy ERFA（角度 1.9e-5″ ✓，距离 0.05 au ✗） | **F03 / F09** |
| 盲解四元组（描述符容差/手性/行号） | `describe_quad`（catalogue）+ `observed_descriptor`（sources，两份实现） | `query_quads`（KD 盒 + 基线 + 区域过滤 ✓） | `astrometry.cpp:607-615`（容差盒 ✓） | `quad_descriptors` 两行/四元组（两种手性 ✓） | `catalogue_check`（描述符最大误差 1.7e-16 ✓） | **F07**（两份描述符约定可能分歧） |
| 盲解失败语义 | — | `solve_astrometry` | `normalize.cpp:296-308` | — | `astrometry_check`（空图→`unsolved` ✓，无伪球面解 ✓） | 通过 |
| 参数噪声与同数据影响 | `build_parameter_noise` | `AstrometricSolution::parameter_noise/influence` | 消费者 `bind_wcs` | `Influence` 载荷 | `astrometry_check`（非空 ✓） | 通过 |
| 缺 PSF/大气/观测者信息 | `model::set_omitted` | `Assessment::omitted` | 事件流 | — | `astrometry_check` | 省略项已记 ✓，但**位置误差预算从未给出**（F08） |
| 响应积分（photon/energy） | `src/response/api.cpp` | `integrate_response` | `normalize.cpp:430` | `SpectralPrediction` | validation/phot R1（8.88e-16，两约定 ✓）；缓存键含约定 ✓ | 通过（自审） |
| 联合潜变量光度 | `src/photometry/joint_fit.cpp` | `fit_photometry` | `normalize.cpp:446` | `PhotoSolution` | validation/phot P1（尺度误差 0，跨帧相关 0.4999996 ✓）、P2（`not_identifiable` ✓） | 通过（自审） |
| 数值参数盒 | `joint_fit.cpp` 剖面区间 | `PhotoSolution::parameter_lower/upper` | `apply_scale` | — | validation/phot F01（0.02000067 = log1.01−log0.99 ✓，非对称 ✓） | 简化实现，见 F10（自审） |
| 逐像元标度 | `src/photometry/apply_scale.cpp` | `apply_scale` | 观测链 | 新 `NativeObservation` revision | validation/phot S1（逐像元因子不同 ✓、重复 `identity_conflict` ✓） | 通过（自审） |
| PSF 像元积分 | `src/sources/psf_model.cpp`（唯一实现 ✓） | `evaluate_psf` / `fit_psf` | `measure_science_sources` | `EffectivePSF` | F01 **失败**（F01） | **F01** |
| 有限窗不重归一 / 尾界 | `psf.cpp:402`、`measure.cpp:611` 的省略项；`tail_mass_upper` | `EffectivePSF::tail_mass_upper` | — | — | 代码审查 | 通过 |
| 通量数值区间 | `measure.cpp:602` | `SourceMeasurements::flux_numerical_lower/upper` | `fit_photometry` | — | 代码审查：仅在 `flux_certified` 时构造 ✓，否则为空且记 `numerical_flux_uncertified`、science=conditional ✓ | 通过 |
| CLI → 目录星 → fit_psf/measure | `apps/cli/commands.cpp:721`（目录 JSON）→ `normalize.cpp:352`（`query`，epoch=观测历元 ✓） | `fit_psf`/`measure_science_sources` 同一星集 ✓ | — | — | 代码审查 + F01 链 | JSON 解码有 F06；星集传递 ✓ |
| 合同驱动 wire | `apps/contract/driver.cpp` | — | 机器接口 | — | schema 比对 | **F05** |
| 一个算法一份实现 | 见 F07 | — | — | — | 代码审查 | **F07** |

## 4. 按严重度排序的发现

### P0

**F01 — PSF 拟合走进无效参数区后被误报为“不可辨识”，整条测光链被阻断**
（`src/sources/psf_fit.cpp:451-459`、`:476-501`；`src/sources/psf_model.cpp:177-186`）

- 构型：F01 夹具（Gaussian、空间阶 2、8 颗目录星、16×16 探测器）。`python3 validation/run_all.py f01`
  → `validation/f01/observed/native_f01.json`。
- 期望：拟合收敛；若不收敛应给 `nonconverged` 并说明模型退化。上一迭代的设计矩阵是满秩的
  （/tmp 诊断实测奇异值 2.048, 1.763, …, 0.0898）。
- 实际：返回 `not_identifiable`，理由“the design matrix is rank deficient; the singular values are
  1.000000 0.000000 …”（43 个值，其中 42 个为 0）。/tmp 诊断显示最后一迭代**模板值与全部列范数同时塌成 0**：
  Gauss-Newton 步把高斯相关系数推到 |ρ|=1，`density()` 对无效参数**静默返回 0**，模板整体塌陷，
  秩判定是在一个已退化的模型上做出的。后果：`fit_psf → measure_science_sources → fit_photometry →
  apply_scale` 整条链被阻断（F01 的第二个功能量仍为 pending_implementation）。
- 最小修订：① `density()` 不得对无效参数静默返回 0，模板构造必须检查 σx>0、σy>0、|ρ|<1 并以
  `Code::domain` 失败；② GN 步必须被约束在有效域内（对局域参数加盒，或在 Armijo 循环中拒绝无效试探点）；
  ③ 因模型退化导致的秩塌陷必须报 `nonconverged`/`domain` 而非 `not_identifiable`（设计并不秩亏）。

### P1

**F02 — 交付库里残留 DEBUG `printf`，会污染 CLI 的 stdout 机器接口**
（`src/sources/psf_fit.cpp:196-203, 327-331, 443-449`；另有 `validation/native/f01_driver.cpp`）
CLI 的事件流走 stdout（`acsd normalize --json … > events.jsonl`），库内 8 处 `printf("DEBUG …")`
会把非 JSON 行写进事件流。最小修订：删除这些打印。

**F03 — 目录的两个公开入口历元传播不一致；盲解的仿射假设用的是目录历元**
（`src/catalogue/provider.cpp` 的 `lookup` vs `query`；消费者 `src/sources/astrometry.cpp:620`）
自建探针（仅公开 API）：目录历元 2005、观测历元 2025、自行 0.4″/yr 时，同一颗星
`lookup` 与观测历元真值差 **12.93″**（2″/yr → 64.65″；4″/yr → 129.29″），
而 `query` 差 ≤1.1e-5″。文档要求传播到匹配历元（OBSERVATION_COORDINATES §2）。
最小修订：让 `lookup` 接受目标历元（或让求解器统一从 `query` 取位置），不得在同一假设里混用两个历元。

**F04 — sources 模块的自行传播违反 pmRA*cosDec 约定并忽略视差/视向速度**
（`src/sources/psf_model.cpp:477-487`）
`ra += pmra_cosdec*dt` 缺少 1/cos(dec)；在 δ=19° 时 RA 误差 6%，δ=35° 时 16%；
视差与 RV 被忽略。目录模块的实现是正确的（`sky_geometry.cpp:91-108`）——两者不一致（见 F07）。
最小修订：把传播收敛为一份（见 F07）并按 pmRA*cosDec 约定实现。

### P2

**F05** 合同驱动只实现 24 个操作，而 `schemas/science-input.schema.json` 声明了完整公开合同；
本范围的 16 个操作（目录、盲解、PSF、测光、标度、绑定、标定、重组）在驱动里一律 `unsupported`
→ 这四个模块的合同路径**无任何实跑覆盖**。

**F06** `apps/cli/commands.cpp:737-772` 的目录 JSON 解码对未知键静默忽略，XP 系数行可参差
（系数个数取自最后一行）→ 拼错的键会静默产生残缺目录。

**F07** 四类算法各有两到三份实现：历元传播、类星体描述符、SVD 伪逆步（psf_model vs joint_fit）、
一维高斯像元积分（psf_model vs astrometry_detection）。副本之间约定已经不同（F04），
描述符的基线并列时目录与求解器的并列打破规则不同（源标识 vs 点序号），可能导致漏配。

**F08** `AstrometricContext::omitted_position_bound_rad` 全树从未被写入；
`WcsModel::epoch_frame_observer.omitted` 在未做视向差改正时仍为空；
目录里算好的位置误差界 `observer_omission_bound_rad` 是死代码 → 文档要求的“省略项及其位置误差预算”只做了一半。

**F09** 目录传播的**距离**与 ERFA 参考相差最大 0.0514 au（2.4e-6 相对；角距一致 1.9e-5″）：
切平面线性运动漏掉二阶曲率项。对天测影响可忽略，但目录给出的视差不是参考视差，
且视向差改正用同一套距离模型才自洽。

**F10（自审）** 光度模块的数值参数盒是“潜变量固定的剖面区间”，不是 EXECUTION 写的区间-Newton 根证书；
未知测量方差走等权回退 + 记未知项 + 结果 conditional。两者都已在模块自带的
`validation/phot` 中留证，但需要工程正本给出唯一口径。

## 5. VERIFICATION 必需案例的实跑状态

| 案例 | 状态 | 说明 |
|---|---|---|
| F01（PSF → 测光 → 标度 → apply_scale 链） | **fail** | P0：F01 |
| 目录索引/提供者（`validation/cat/catalogue_check`） | pass | 描述符、盒查询、游标、lookup 顺序 |
| 盲解（`validation/cat/astrometry_check` + astropy） | pass | 188 匹配源；astropy 对拍 0.293″（容差 2″） |
| 目录历元传播 vs astropy ERFA | pass（角度）/ 距离有差异 | F09 |
| C01 / G01 / U01 | pass | 全量 `run_all.py` |
| S01 / P01 / R01 / E01 / V01 / W01 / X01 | pending_implementation | 属 sky/产品/资源/CLI 范围，非本审核范围 |
| phot 模块自验证（`validation/phot`） | pass | **自审**，非独立复核 |

## 6. 剩余限制与未做事项

- 本审核没有覆盖真实 Gaia/PSF/aperture 数据（REQUIRES_REAL_DATA）；所有结论基于合成构型。
- `src/response`、`src/photometry` 的结论是自审；它们的独立复核需要另一位审核者。
- 我的自建探针（`/tmp/audit/epoch_case.cpp`）用了一个比目录验证夹具更小的视场，
  其**最终解的绝对精度不可信**（在该夹具上即使 pm=0 也得到约 1000″ 的残差，说明夹具本身与
  `validation/cat` 的夹具不等价）。因此 F03 只主张“API 层历元不一致（实测）+ 求解器用了错误历元
  的位置构造假设”这一可证事实，**不主张**最终精度已因此受损；最终精度影响需用目录夹具重做。
- 合同驱动对本范围操作缺失（F05）意味着这四个模块的机器接口路径**尚无实跑证据**。
- 本次运行重写了验证框架自身的 observed/comparison 证据文件（框架既有行为），未改动任何实现文件。
