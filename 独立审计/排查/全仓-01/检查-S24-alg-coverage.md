# 检查-S24-alg-coverage：lib/algorithms/coverage（Phase2 核心）对抗性静态复查

> 责任域：`lib/algorithms/coverage/` 全部 .cpp/.h/.hpp（hips_properties.cpp/h、
> include/astro/phase2/ 12 头、src/ 13 实现、tests/ 11 测试、tools/、configs/、
> README.md/module.yaml/memory.md），交叉核对 docs/science/algorithms/PHASE2_{COVERAGE,INTEGRATION,
> REJECTION,SAMPLER,SESSION,UPM_IMPL}.md 与 docs/science/{PHASE2_UPM,REJECTION,INTEGRATION}.md。
> 方法：逐文件读 + 机械提取 9 份交叉文档中 305 条 `file:line` 锚逐条开文件比对源码行
> （0 条越界）+ D 系终裁对数（D-05/D-08/D-10、§9.73 A44）+ 负例门非退化审读。
> 纪律：零 git 写、零构建、零编译、零测试（纯静态）；只写本报告一个文件。
> 已查-修复验证.md PASS 表通过项与同批已立案项（见文末「已登记并案对照」）一律不重复计数；
> 科学公式/默认容差/冻结口径问题只登记红/黄＋证据，不代改。

---

## 一、问题清单

### 红（1 条）

---

**S24-R01 / 红 / docs/plugins/algorithms_phase1/07_noise_snr.md:195 ↔ lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:593,:730,:833、lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:199,:255、lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp:1894 / D-05 已批裁决「idw_power 默认 1.0＋配置化＋日志 p*」完全未落码，文档以现在时主张已接线且默认值与代码相反 / 面④（域外专项点名）**

- 描述：07_noise_snr.md:195 带 `<!-- 订正: D-05；负责人已批「配置化＋运行日志输出、不随产物落盘」 -->` 注记，主张「其 `idw_power` 默认 **1.0**……均**按配置解析**；每次重建把实际生效的插值设置写入**运行日志**（IDW 口径含实测最优 `p*`（argmin）、`K` 与噪声档），**不随产物落盘**」。代码四点全部相反：①默认值：三处生产者硬编码 `out_model->idw_power = 2.0`（snr_estimator.cpp:593/:730/:833），两处消费端回退 `2.0`（snr_evaluator.cpp:199/:255），头注两处写「默认 2.0」（snr_estimator.h:588、snr_evaluator.h:46/:109）；②配置解析：全仓无 `idw_power` 配置键；③运行日志 p*（argmin）：noise_snr/drizzle 两域 grep `argmin`/`p*` 零命中；④不随产物落盘：aio_healpix_io.cpp:1894 把 `idw_power` 作为 snr_model 块第 3 个 f64 标量**写入产品**（:1804/:1893/:2132/:2146 读回同位）。
- 证据（检索式）：
  - `grep -rn 'idw' eng/packaging/config/*.json eng/contracts -r` → 0（defaults.json/filters.json/合同 schema 均无此键）；
  - `grep -rn 'value("idw' lib/ eng/ --include=*.cpp` → 0（无任何 parser 读取）；
  - `grep -rn 'argmin' lib/algorithms/noise_snr lib/algorithms/drizzle` → 0；
  - 分歧台账.md:62-63（D-05 终裁）：「默认 idw_power = 1.0……idw_power/K 配置化，每次重建在日志输出实测 p*（argmin）……**影响文档: defaults/配置（idw_power 2.0→1.0，配置化+日志，负责人已批）**」——影响项未执行。
- 反方核验：（a）IDW 属**备选口径**，生产默认算子 = natural_bicubic_spline_clip_v1（D-10，integration/v6 词表零 IDW 档）⇒ 主链行为不被本项影响，严重级不因行为面扩大而上调以外的减责不适用——文档现在时主张与已批裁决落空本身即「文档说有、代码没接」红级；（b）PASS 表只覆盖 **docs 侧**订正（检查-行文逻辑.md:91 已扫「idw_power=2 默认」在 docs+五论文无残留），落码项从未进 PASS；（c）`healpix_db/archive/legacy/**` 内同名 2.0 属归档遗留，不计入（已从证据面剔除）。
- 建议改法（不代改）：二选一走变更流程——①按 D-05 落码：defaults/配置加 `idw_power`（默认 1.0）+ 两处消费回退同步 + 每次重建日志输出实测 p*/K/噪声档 + snr_model 产品块对 idw_power 的落盘与「不随产物落盘」裁决对齐（去块或改登记）；②若负责人改判，收回 07:195 现在时主张。禁止只改文档掩盖未执行的已批裁决。

---

### 黄（4 条）

---

**S24-Y01 / 黄 / lib/algorithms/coverage/src/sampler.cpp:79-83,:91-92,:580（旁证 tests/kcorr_lookup_test.cpp:63）/ kcorr 注释与日志仍称 1.4 为「冻结保守值/保守侧」，与 D-08 终裁「代码默认/域外回退（实现记录），声明域内不保守」相反 / 面①②④（交接 §10 点名登记项）**

- 描述：:79-83 注释「冻结：…k_corr_empirical = 1.3883，N_eff ≈ 181 < N_retained=251。**冻结保守值 1.4**。」后接 `constexpr double kControlCorrDefault = 1.4;`；:91-92「域外一律返回冻结默认 kControlCorrDefault（**1.4 ≥ 实证 1.3883，保守侧**）」；:580 stderr 文案「冻结默认」。D-08 终裁口径：1.4 = **代码默认/域外回退（implementation record）**，且声明域内**不保守**（N_retained=5 端低估 32%、源尺度端约 2×——分歧台账 D-08/摘要 :245）；docs 侧已按此改口径（PASS 红-8：QA_MATRIX/DATA_ARTIFACTS/PHASE2_UPM_IMPL 标题订正通过），代码注释/日志是残留的旧口径。
- 证据（行为面反方核验，故仅黄不红）：`kControlCorrDefault=1.4`（:83）与 upm.cpp:2233 `kMaKCcorrFrozenInDomain = 1.4 // FZ-PROV-KCORR-VALUE` 一致；kcorr_lookup 域/表/回退（:93-117）与 PHASE2_SAMPLER.md:364-372 一致；消费式 sampler.cpp:875 `cvar = kcorr_f * kPiHalf * sigma*sigma / n_ret` 与 upm.h:438 定义一致；kcorr_lookup_test F2 四组断言锁齐（域外→1.4）。即**行为合规、文案反裁决**。
- 建议改法：:79-83/:91-92 注释改「代码默认/域外回退（实现记录，D-08）；声明域内非保守，域内以 lookup 为准」，:580 日志措辞同步；按交接 §10 登记为**代码修复单候选**，本单不代改。

---

**S24-Y02 / 黄 / lib/algorithms/coverage/tests/ivar_wiring_test.cpp:437-440 / 头注仍写「仅显式 legacy_allow_weight_fallback=true 才降级并逐像素计数」，与同文件 :475-479 正文及现行 A44 硬错误行为直接矛盾 / 面②**

- 描述：头注（M4-C-03 注释）:440 收尾「仅显式 legacy_allow_weight_fallback=true 才降级并逐像素计数」；而本 TEST 正文 :475-476 写「§9.73 裁决 A44：ivar 缺失**恒** fail-closed（rc=7）——原 legacy 降级开关已删除」，断言体 :477-492 按 rc=7 恒等断言。代码侧佐证：stage2_common.cpp:459-467 对该键出现即 `return false`（硬错误），stage2.cpp:793-800 同形态拒绝。同文件两说，读者按头注理解会得出「开关仍可设」的错误结论。
- 反方核验：断言体与现行代码一致、负例非退化（ivar tile 被破坏 → 恒 rc=7，非恒真）；缺陷仅在头注旧文案，测试可信度不受损。
- 建议改法：头注改为「ivar 缺失恒 fail-closed；legacy_allow_weight_fallback 已按 §9.73 A44 删除，出现即硬错误（见 stage2_common.cpp:459-467）」。

---

**S24-Y03 / 黄 / lib/algorithms/coverage/README.md:14-17,:27-31,:38-54 与 lib/algorithms/coverage/module.yaml:1-15 / 模块合同三件套的行数与行锚全量过期，却仍自述「实测 / 行号 grep 实测」/ 面④**

- 描述与实测对照：
  | 文档主张（自述实测） | 实测（wc/grep，本片） |
  |---|---|
  | coverage.cpp **239 行**（README:14-15、module.yaml:1-2） | **455 行** |
  | coverage.h **59 行**（README:16、module.yaml:2-3） | **171 行** |
  | p2_coverage_build **:144**（README:28、module.yaml:4） | **:173** |
  | p2_coverage_free **:233**（README:30、module.yaml:4） | **:274** |
  | parse_props **:20** / inspect_frame **:59**（module.yaml:5「行号 grep 实测」） | **:21 / :60** |
  | inspect_frame :59-140 / filter :181-193 / target_order :194-196 / union :204-214 / 容量 :218-228 / free :233-237（README:40-54） | :60-… / filter 判定 :210-216 / target_order **:235-237** / unique **:254** / free **:274**（整体 +29~+41 漂移） |
- 反方核验：（a）同批 S6B 反方核验（检查-S6-algorithmsB.md:57）确认**算法文档侧** PHASE2_COVERAGE.md 的 coverage.cpp 455/coverage.h 行数实测相符——错的只是模块三件套（README/module.yaml，2026-09-07 冻结后实现增长未回写）；（b）README 的语义性主张（职责边界、obs_filter fail-closed、union 只并不交、单线程无 OpenMP）逐条抽查仍成立；（c）module.yaml「coverage.cpp 全文无 OpenMP pragma」复核成立。故定黄不红。
- 建议改法：行数改口「以源文件为准，不复述绝对行数」或更新为 455/171；行锚改**符号锚**（p2_coverage_build/inspect_frame 等函数名，与 D-67 处置一致）；凡带「实测/grep 实测」字样处必须与复测一致。

---

**S24-Y04 / 黄 / docs/science/algorithms/PHASE2_SAMPLER.md:321,:360,:511；PHASE2_SESSION.md:194-199；PHASE2_UPM_IMPL.md:10,:50,:52,:378-402,:407-410 / 五处算法合同文档的 file:line 锚逐条实开核验为错（同文新旧两说、声明锚指空行、整表 −13 系统漂移）/ 面④（与 S6B-R01/D-67 同族，本片给出逐条实测样本）**

- 逐条实开证据：
  1. **PHASE2_SAMPLER.md:321** 引 `p2_upm_control_variance` 输入门在 `upm.cpp:2766-2790` → 实 `p2_upm_control_variance` 在 **upm.cpp:2954**；:2766-2790 实为 ma 模型 hash/info 段。同文 :329 的订正注已把**同一函数**旧锚 2772-2783 改到 2954-2975——:321 漏改，**同文新旧两说**（行文逻辑面加重项）。
  2. **PHASE2_SAMPLER.md:360** 引 FZ-PROV-KCORR 写盘门在 `upm.cpp:2265-2275` → 实门在 **upm.cpp:2480-2490**（k_corr_applicability_domain 空 → rc=7；k_corr≠1.4 缺 k_corr_calibration_run_id → rc=7）；:2265-2275 实为 `ma_eig` Jacobi 扫描循环。反方核验：**行为主张为真**（门存在、rc=7、upm.h:275/:309-314 口径一致），仅锚错。
  3. **PHASE2_SAMPLER.md:511** 引「control_reliability 归一化消费 upm.cpp:565」→ :565 实为 `lambda_s` 行；归一化在 **upm.cpp:342/:655**。PASS 表仅记录 UPM_SOLVER.md:26 同锚残留（FAIL 未修），本行属**新位点**。
  4. **PHASE2_SESSION.md §7 表（:194-199）6 处声明锚错**：p2_upm_build 引 upm.h:95-97（实 **:121**）、p2_upm_info 引 :110（实 **:136**）、p2_upm_save 引 :108（实 **:134**）、p2_upm_close 引 :192（实 **:219**，:192 是**空行**）、p2_sampler_default_config 引 sampler.h:60（实 **:66**，:60 是 CON-004 注释）、p2_sample_controls 引 sampler.h:103-114（实 **:109-117**，:103-105 是 p2_stats_*）。同表 coverage.h 两锚（:57-59/:62）正确 ⇒ 错锚集中于 upm.h/sampler.h（+26/+27/+6 漂移）。
  5. **PHASE2_UPM_IMPL.md**：头部 :10 称「upm.cpp（2793 行）…upm.h（384 行，实测）」→ 实测 **2981/450**；§13 默认值表（:378-397）14 行 × 2 个 upm.cpp 锚**全部系统性 −13**（如 huber_delta :254/:273 → 实 :267/:286；final_gauge :270/:291 → 实 :283/:304；pre文本 :378 称缺省面 `upm.cpp:254-292` → 实 **:261-305**）；:402 kStallPatience 引 :719/kObjImproveFloor 引 :720 → 实 **:732/:733**；:407-410 收敛判据 :707-718/:1021-1040 全部 +13；:50 引 upm.h:278、:52 引 upm.cpp:56 均落在**空行**；:61/:87/:182-183 仍把已删除的 `p2_upm_normalized_weights` 列为有效 API 面（upm.cpp:1999 `RETIRED`、全仓零消费者）——此项已登记 D-67/ACSD-T30（未修），本片复核仍存。
- 反方核验：机械提取 9 份交叉文档 305 条锚、0 条越界，多数锚相符（PHASE2_COVERAGE/REJECTION/INTEGRATION/SESSION 头部与结构锚抽验全对）——即「混代不更新」而非全错；本条只登记实开为错的位点。
- 建议改法：按 D-67 处置改**符号锚**；:321 随 :329 同批改 2954-2975（消除同文两说）；:360 改 2480-2490；:511 改 342/655；SESSION §7 表 6 锚按上表更新；UPM_IMPL §13 整表 +13 回写并更新头部行数。与 S6B-R01 并案时以本片样本补齐逐条位点。

---

### 绿（6 条）

---

**S24-G01 / 绿 / lib/algorithms/coverage/tests/kcorr_lookup_test.cpp:19 / 锚注「sampler.cpp:91-94」与查表常量实际行位（:96-99）微漂 / 面④**
- 证据：表矩阵与域常量在 sampler.cpp:93-99（kDomainLo/kDomainHi 与 2×3 矩阵），:91-92 是域外回退注释。断言体本身正确。
- 建议：注改 :93-99。

**S24-G02 / 绿 / lib/algorithms/coverage/hips_properties.cpp:1 / 头注自述路径仍是迁移前的 lib/phase3_session/hips_properties.cpp / 面②**
- 证据：文件实际落位 lib/algorithms/coverage/（CR-21 已载迁移「批次 4 → lib/algorithms/coverage/」）；头注未随迁。
- 建议：头注路径随文件改；namespace astrocs::phase3 属 ABI 面不动。

**S24-G03 / 绿 / lib/algorithms/coverage/tests/retired_object_reject_negative_test.cpp:16 / mutation 证据指 `run/PSFSW-RETIRE-01/REPORT.md`，该 run/ 已按轮次回收，当前跟踪面不可达 / 面④**
- 证据：`ls run/PSFSW-RETIRE-01/` → 空（run/ gitignore、round_start 回收为既定纪律）；与 08_修复包 D-68 同型（冻结判定出处落 run/ 即字面断链）。
- 反方核验：测试本体可执行、断言非退化，证据缺失不影响门的能红能绿。
- 建议：证据改指入库面（如 08_修复包报告/检查报告行），或注明「run/ 回收件，可重跑再生」。

**S24-G04 / 绿 / lib/algorithms/coverage/tests/synthetic_gate.cpp:3326 / 注释用 A44 已废概念「默认 weight_mode=ivar」/ 面②**
- 证据：A44 后无「权重模式」概念（ASTROCS §3.1），实际语义 = `use_ivar_weight=1`（upm.h:89）；:4119 测试配置仍写 `snr_weight_mode`（与 CR-47-24 同族）。反方核验：G5WeightTruthGate 断言体非退化（ivar 最优 1/27.778=0.036 vs 等权 0.1，阈值带区分度），仅注释措辞。
- 建议：注释改「默认 use_ivar_weight=1（ivar 权重）」。

**S24-G05 / 绿 / lib/algorithms/coverage/README.md:52、memory.md:196 / 「唯一冻结式」公式用旧名 geometric_reliability 未注现名 / 面③**
- 证据：正本 PHASE2_UPM.md:23 已定名 `control_reliability`（旧名 geometric_reliability）；README:52 写 w_UPM=quality_factor×**geometric_reliability**×control_ivar，memory.md:196 同旧名。AUD-101-DA02 已登记 docs/algorithms 侧残留，**模块 README/memory 侧为本片新位点**。
- 建议：改现名并（或）注「旧名」，与正本同款。

**S24-G06 / 绿 / lib/algorithms/coverage/src/stage2_common.cpp:437-438、tests/weight_mode_retire_negative_test.cpp:123 / 引 docs/ASTROCS_DESIGN.md「§3.1:175」的行号已漂（实 :256）/ 面④**
- 证据：被引句「权重的产生链固定为两步、没有可选择项」现位于 docs/ASTROCS_DESIGN.md:**256**；:170-180 为 PSF 信号 SNR 命名段。反方核验：**条文内容为真**且被引语义不变；同款引用遍布审计件与修复包（S6B/08_修复包 02 §92 同用）⇒ 改锚需全局同批，单点改会制造新不一致。
- 建议：随任何一次 docs/ASTROCS_DESIGN 变更窗口全局同步为符号锚（§3.1 + 引文），或在引用处只写 §3.1 不带行号。

---

## 二、已登记并案对照（本片复核确认仍在，不重复计数）

| 既有登记 | 对象 | 本片复核 |
|---|---|---|
| 同批 S6B-R05（红） | PHASE2_INTEGRATION.md:128,:133-136 把 A44 已删的 legacy_allow_weight_fallback 写成现行降级键 | 实开确认：stage2.cpp:781-800 已删该分支并硬拒；同段 :126「无任何 fallback」与 :133 自相矛盾——并入 S6B-R05 |
| 同批 S6B-R01/R14（红）＋08_修复包 D-67 | docs/algorithms 行数/行锚成片漂移 | 本片 Y04 为逐条样本补强 |
| 通读-CR-47-24（CONFIRMED） | `upm_weight_source` 合同键全仓零实现、`snr_weight_mode` 仍被 stage2_common.cpp:163-168 读取 | 复核仍在（8 份 configs/*.json 用 snr_weight_mode；DATA_SEMANTICS:2079/PUBLIC_API:2208/CONFIG_SCHEMA:35 三处合同同写 upm_weight_source 且锚 h:73 反证） |
| 08_修复包 D-06（上呈/修复序） | sky_plane.weight_mode 第二口径枚举（stage2_common.cpp:193 收、sky_plane.cpp:524 静默钳位） | 复核仍在（sky_plane.h:216/默认开） |
| 同批 S11-黄15 | CONFIG_SCHEMA:25 死文档键 patch_radius_leaf（JSON 键 = patch_radius_pixels） | 复核仍在；本片补充：module_adapters.cpp:9269 双名兼容而 stage2 工具直连路径只认 pixels ⇒ 文档名配置在 CLI 路径静默忽略（与 S11 同对象，并案） |
| 08_修复包 D-69（需负责人裁） | upm.cpp:714-715「拟合权重=叠加权重同源」vs PSF_SIGNAL_WEIGHT.md:128「拟合权重≠堆叠权重」 | 复核仍在，已上呈，不翻案 |

---

## 已查无问题面

### ①科学性面（常数/公式/单位/负例非退化）

- **k_corr 几何查表（D-08 专项）**：kcorr_lookup（sampler.cpp:93-117）表矩阵
  {{1.2112,1.3925,1.4980},{2.3958,2.8971,3.2035}}、域 [300,600]″、pixfrac 网格 {0.5,0.8,1.0}
  分段线性、域外回退默认不 clamp——与 PHASE2_SAMPLER.md:364-372、kcorr_lookup_test 四组断言
  （角点精确、pf=0.8 命中、中点 rtol 1e-12、生产两尺度域外→1.4）逐项一致；接线在
  sampler.cpp:567-585（域内 lookup / 域外 stderr 标记），消费 :874-875
  `cvar = kcorr·(π/2)·σ²/n_ret` 与 upm.h:438 定义一致；upm.cpp:2233 与 sampler 默认同值 1.4；
  pf==0 未知 provenance 的静默回退已由 PHASE2_SAMPLER.md:373-391 如实登记为观测性边界（文档不隐瞒）。
- **常数对数**：MAD 1.482602218505602（sampler.cpp:844）、Huber δ=1.345（upm.h:74、upm.cpp:267/:286
  与 PHOTOMETRIC_FIT/D-06 归属一致）、tolerance=1e-6/tolerance_relative=0（upm.h:81/:113）、
  rejection α=0.05（rejection.h:139）、percentile/winsorized/ESD 各档默认与
  REJECTION.md §4/§8 对数一致、background 默认 8/3.0/3/0.35/3.0（sampler.cpp:313-319 与
  PHASE2_SAMPLER §5 同值）、control_grid=8 与 `cfg.grid != 8 → rc=3`（upm.cpp:298）互证。
- **负例非退化**（逐个审读，全部能红能绿）：
  `rejection_nonfinite_weights_test`（NaN/+Inf → INVALID_INPUT 等值断言＋正例保活＋eligibility 有限性）；
  `retired_object_reject_negative_test`（resample/calibration/drizzle 三退役对象拒绝码与文案断言＋正例；
  UNIFIED_MODEL.md:58 权威锚实开相符）；
  `weight_mode_retire_negative_test`（token 门 FZ-MODE-RETIRED/FZ-WEIGHT-SINGLE-PATH rc=2 ＋
  路由硬拒 rc=2＋正例；引文 ASTROCS §3.1/PSF_SIGNAL_WEIGHT §4 内容实开相符——行号漂见证 G06）；
  `synthetic_gate` 抽样 G5WeightTruthGate（ivar 最优方差 1/27.778=0.036 vs 等权 0.1，阈值带
  0.06/0.065 有区分度）、G1ProductionWiringTruth（配置接线/geometry-hash 不变 vs model-hash 变化）、
  M4A01N3/N4/N5/N6 系列（精确计数：中位在带、全拒回退 UNDERDETERMINED/ALL_REJECTED、
  auto 路由 winsorized 分支）——未发现恒真门。
- ivar 接线（ivar_wiring_test WIRE-IVAR-001..005）：ivar 缺失恒 rc=7、等权/降级路径不可达、
  帧置换不变量——与 A44 单一口径一致（唯一缺陷是头注，见 Y02）。

### ②行文逻辑面

- 逐篇通读 PHASE2_{SAMPLER,INTEGRATION,COVERAGE,SESSION,REJECTION,UPM_IMPL} 的关键节
  （k_corr 域、观测性边界、A44 承接、DISP 缺陷登记表、容差冻结清单）与 11 个测试文件的
  头注/断言体；除 Y02（ivar 测试头注自相矛盾）与 Y04 第 1 项（同文新旧两说）外，
  未发现断链论证、UNRESOLVED 当结论、或残留的未清理 `<!-- 订正: -->` 与正文冲突。
- PASS 表 28 项通过内容与三篇总编检查的既有结论未重报；D-05/D-08/D-10/A44 均按终裁口径
  对读，无翻案。S6B-R05 覆盖的矛盾段未重复立案。

### ③跨文档冲突面（五级权威链抽验）

- **A44 单一口径链四侧一致**：ASTROCS §3.1（实 :256 条文真）→ stage2_common.cpp:447-467（两键
  硬拒）→ stage2.cpp:781-800/:1106（分支已删、compact ivar 单路）→ weight_mode_retire_negative_test
  （正负例）→ CONFIG_SCHEMA:58 注记；CHK-NO-WEIGHT-MODE-CODE 门的 C1-C5 判据面与实现互证
  （eng/ci/check_no_weight_mode_code.py 头注权威链与代码锚存活）。
- **D-08 链**：分歧台账 → PHASE2_SAMPLER §5.4（订正注在位）→ 代码默认/lookup/test 一致
  （代码注释旧口径已立案 Y01）；**D-10**：natural_bicubic_spline_clip_v1 默认档在
  UNIFIED_OBJECTS/13_integration/CONTROL_WEIGHT_SNR 与 v6 词表一致（PASS 已验，本片复核无回退）；
  **D-09/D-07** 相关常数在 PHASE2_SAMPLER/PHASE2_UPM 无回退残留。
- 仍冲突但**已登记**的对象（upm_weight_source、sky_plane.weight_mode、PHASE2_INTEGRATION
  降级键、拟合权重同源 D-69）见「已登记并案对照」——本片确认存续、不翻案、不重复立案。

### ④幻觉与锚面（死配置键/静默旁路/硬编码/第三方接口）

- **锚机械核验**：脚本从 9 份交叉文档提取 305 条 `file:line` 锚，逐条开文件比对目标行内容——
  **0 条越界**（无指向超文件长度的锚）；相符占绝大多数（coverage.h/rejection.h/integrate.h/
  sampler.h/PHASE2_SESSION 头部、PHASE2_INTEGRATION 全文 integrate.* 锚抽验全对）；
  实开为错的位点全部登记在 Y04/G01/G06 与并案表，未见漏报的成片失效。
- **死配置键/静默旁路**：stage2_common.cpp 键面（:9-209 解析表、:435-467 A44 拒收、
  :482-503 execution 段）逐键对 CONFIG_SCHEMA 与 8 份 configs/*.json 核过——除已立案的
  patch_radius_leaf（S11-黄15）、upm_weight_source（CR-47-24）、weight_mode/
  legacy_allow_weight_fallback（A44 已正确硬拒，拒绝面存活）外，未发现新的死键或静默旁路；
  A44 负例注入三件套（token/路由/产品身份）拒绝码与文案均在位。
- **无硬编码线程/ISA/block**：execution_options.h:16/:24/:38（0=auto→hardware_concurrency）、
  sampler.h:60（CON-004 lease）、upm.cpp:608 与 sampler.cpp:925「无 hardware_concurrency 模块
  不得自行开线程」、acr_kernels.cpp:218 `num_threads(cpu_workers)`、stage2_common.cpp:484-497
  （cpu/io_workers 0..1024 0=auto、gpu_route 三值），无固定线程数/内联 ISA/写死 block 维度。
- **第三方只做接口面**：nanoflann（sampler/sky_plane KD-tree 查询）、nlohmann::json
  （stage2_common 解析，异常由 :504-507 try/catch 包裹、不穿 ABI）、cfitsio（FITS 经 aio 唯一
  I/O 边界，hips_properties.cpp:7-11 CLEAN-403 自证无自持 I/O）——未见越权直接调用或本片问题。
- **无新文献引用**：本片未引入 DOI/arXiv 新锚（既有 arXiv:0805.2366 等旧锚问题已由 PASS 表
  处理，未复报）。
