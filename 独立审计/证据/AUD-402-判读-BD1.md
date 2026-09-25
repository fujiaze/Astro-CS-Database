# AUD-402 · 常数判读 `<Q>` = **BD1**（具名科学常数试点）成稿

- 被审基线 HEAD=`c8f64e9a`；独立只读审查节点。
- 开工 `git status --porcelain`：`?? ACSD整治工作包_AUDIT-06.zip` / `?? site/` 两行；收工复跑见本文末「基线自证」。**两次完全一致，无差异可报。**
- 本会话零 git 写、零 `git config` 写、未执行任何构建/cmake/ctest/`run_checks.py`/`eng/**` 脚本/端到端；判"生产实际取值"一律沿代码路径推导。
- 队列输入：`独立审计/批次清单/P402-BD-01.csv`（25 符号）＋ `P402-BD-02.csv`（25 符号）＝ **50 个不同符号**（派单写"25 个"，按两个批次文件的实际行数计为 50；本判读把 50 个全判完，未缩范围）。
- 输出：`独立审计/证据/AUD-402-判读-BD1.csv`（50 判读行，12 列，UTF-8-BOM）。

## 0 判读粒度与口径（与派单一致）

逐**符号**判读：每符号给实际取值、全仓出现处数（`git grep` 确定命令见 §5）、五件事缺哪几件、四选一出处与处置；同一符号在科学文档／配置／代码三侧取值不同则三侧并列并立案。

处置分布（机器统计，命令见 §5）：

| 处置 | 行数 | 占比 |
|---|---|---|
| 不适用（结构性常数／非数值：累加器初值、哨兵兜底、成员默认、测试激励、格式串假阳性） | 33 | 66% |
| ④待确认 | 9 | 18% |
| ②可由关系导出 | 7 | 14% |
| ③需实验标定（已标定但结果不入库） | 1 | 2% |

来源现状分布：`无` 38、`A（一手标准/解析关系）` 4、`C（仅仓内文档陈述）` 4、`B（本仓可复现）` 3、定义式 1。

**试点要回答的问题的答案在这张表里**：具名常数队列 2/3 是"不进科学语义的结构性值"，1/3 才是需要取证的科学量——机械层不做这一步区分，而它决定了每符号的取证预算（§4）。

---

## 1 立案清单

### 1.1 多侧默认不一致（4 条）

| # | 符号 | 各侧取值（路径:行） | 判定 |
|---|---|---|---|
| L1 | **iterativeClipSigma** | 9.0f：`lib/algorithms/star_detection/src/sdet_api.cpp:996`（默认）、`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1569`（生产）、`sdet_fp64_test.cpp:54`、`sdet_saturation_cursor_test.cpp:114`、`p1star_tests_core.cpp:552`、`实验/absolute-snr/code/b7_recon_driver.cpp:191`；文档镜像 `star_detection/README.md:119`、`docs/contracts/PUBLIC_API.md:833`<br>**5.0f：`lib/infrastructure/scheduler/src/module_adapters.cpp:3959` 与 `:4846`（同为生产入口）**、`p1star_guided_test.cpp:72`、本队列 `ipv_platform_binding_test.cpp:65` | **立案（P1 面）**：两个生产入口对同一参数取不同值，合同/PUBLIC_API 只登记 orchestrator 一侧。现行无行为差异（该字段在 `lib/algorithms/star_detection/src/` 内**只赋值不读取**），故不是科学缺陷而是登记面缺陷；已被仓内台账记为 OPEN：`artifacts/evidence/audit-2026-01/FIX_LEDGER.csv:209`（M3b-C-05，"五字段四字段零读取而文档称有消费面；检测阈三套并存"） |
| L2 | **sigma_floor** | 1e-3：`docs/contracts/DATA_SEMANTICS.md:2086`（冻结）、`lib/algorithms/coverage/include/astro/phase2/upm.h:83`、`src/upm.cpp:273/:292`、`stage2_common.h:60`、`stage2_common.cpp:169` 兜底、5 个 `lib/algorithms/coverage/configs/stage2_*.json`<br>0.02：`eng/tests/backend/syn008_seam_main.cpp:105`（本队列）<br>1e8：`eng/tests/validation/release02/fix_p2a_seam_oracle/p2a_oracle.cpp:134` | **三侧并列成立但分级低**：后两个是测试侧自取，非合同默认。要点是 **0.02 不是无害占位**：同夹具 `kNoiseRms=0.05`（:25），地板与噪声同阶 ⇒ 该用例实测的是"抬高地板之后的降权"，不等于冻结口径行为，且行内无任何说明。另**同符号单位两说**：`DATA_SEMANTICS:2086` 作"σ（ADU）"、`docs/algorithms/PHASE2_SESSION.md:142` 作"面亮度 ADU·sr⁻¹" |
| L3 | **io_wait_high_percent = 50.0（承载 iowait_percent 判据）** | 唯一源＝代码字面量 `lib/infrastructure/cli/resource_gate.h:207`；同族残留 `:363`（wall<5.0）、`:368`（cpu<20.0 && iowait<5.0）、`:369`（bandwidth<15.0）<br>契约侧：**`eng/contracts/resource_gate_v1.json` 内 iowait/io_wait/bandwidth 键零命中**，生成头 `resource_gate_thresholds_generated.h.in` 亦无对应占位 | **立案（唯一数值源被旁路）**：该 JSON `:7` 自述"本文件是该权威下的**唯一数值源**，实现侧不得再出现字面量阈值"，而四条阈值仍以字面量活在实现侧。生成头注释说明 GATE-FIX-RES 已把 compute/memory/applicability 三面收敛，**io 与"三低组合"面漏收**。注：`:363` 现返回显式 NotApplicable（非旧式静默 Ok），故不是豁免破口，只是数值出源 |
| L4 | **测试侧兜底哨兵与判据方向不匹配（形态级）** | `r_scaled`/`frac_scaled` 取 `-1.0` 兜底（`p1001_real_nodes_test.cpp:3749/:3750`）后断言 `r_scaled < 60.0`（:3752）；对照 `zpk`/`snrf` 取 `0.0` 兜底后配等式/下界判据（`p1snr_fref_baseline_test.cpp:270`、`p1001:3261`） | **立案（空门）**：负哨兵＋只判上界 ⇒ 键缺失时 -1 < 60 恒真，该条 red-side 判据在"字段根本不存在"时不红。同用例第二条 `r_scaled > 0.0 && rel <= 1e-5` 能红，故非全盲。可推广为机械判据：**读键兜底值为负、判据为纯上界 ⇒ 空门候选** |

### 1.2 该导出却硬编码（5 条）

| # | 常数 | 现状 | 应写法（导出式） | 影响 |
|---|---|---|---|---|
| D1 | **1.253（√(π/2)，被 `sigma_logflux_dex` 行核对）** | `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:279` `return 1.253 * sigma_logflux_dex / sqrt(n)`，而**同文件 :272 注释自己写** "1.253 = sqrt(pi/2)"；测试 `p1snr_linux_test.cpp:244` 以 rtol=1e-15 断言该截断值 | `std::sqrt(M_PI / 2.0)`（精确值 1.2533141373155001，本机复算） | 系统性偏差 **−2.51e-4**（相对）；量级可忽略，但被 1e-15 容差**冻结成合同**，日后订正必红 |
| D2 | **kMoffat4FwhmFactor 1.230310** | 生产 `dpsf_psf.cpp:25` 与本队列测试均写 6 位截断值；`docs/algorithms/STAR_PSF_ALGORITHMS.md:62-63` 已给精确值 1.230307652590102 并登记偏差 +1.91e-6（本机复算 −1.908e-6 同） | 按式 `2√2·√(2^{1/4}−1)` 算 | 科学可忽略，属标准 02 §4.2"代码按公式计算、不硬编码"违背；已记为耦合缺陷 DISP-PSF-001 |
| D3 | **kPxScaleDeg 0.0002777777777777778** | `p1001_real_nodes_test.cpp:1810` 写 19 位字面量，注释只说 "sqrt|det CD| deg/px" | `1.0 / 3600.0`（本机核验二进制精确相等）⇒ 语义即"1″/px" | 单位意图被小数吞掉；无精度问题 |
| D4 | **kLimit 2500** | `mem_wire_test.cpp:223`；设计约束写在注释里（`2×1000 ≤ 2500 < 3×1000`） | 由 `kPerNode` 表达成不等式并断言之 | 改 `kPerNode` 会静默失效，无守护断言 |
| D5 | **hips_pixel_scale（浏览器真值件，顺带扫出）** | `lib/infrastructure/hips_browser/healpix_browser_qt/tools/gen_geometry_truth.py:177` 硬写 `hips_pixel_scale=58.6`，而同文件 `:168` 的 `hips_order=leaf_order`（SHIFT=9 ⇒ nside≥512） | 按 `180/π·√(π/3)/nside(leaf_order)` 算（生产写出路径 `aio/src/hips/aio_hips_writer.cpp:1890` 已是按式算） | 58.6323° 正是 **nside=1 基面**像元尺度（本机复算）⇒ 与声明阶数差 ≥512×，且无单位注释。范围：该工具的几何真值文件与浏览器目视核对，**不影响交付产品链**（生产侧单位已由 M2b-B-03 修正并有正/负两面锁） |

### 1.3 抄观测值（查后判定：本批无高危，2 条登记可复核性缺口）

派单点名的形态（"合同把门阈值写成一次观测到的 142 s"）在本批 50 符号里**没有再现**——具名阈值侧（资源门 10 核·秒、p50 90%、mean 85%、per-sample 85%/0.7、队列 60%、半径硬上界 60 px）经追源全部为**整数或已由公式/裁决导出**，且 `eng/contracts/resource_gate_v1.json` 与 `docs/science/NOISE_MODEL.md:368` 各自给出了唯一数值源与导出链（正例，值得当模板）。

被观测值污染的是**锚值类**，其性质是"输入"而非"阈值"，故不致错判，但都缺可复核锚：

- `kRealSky = 260.8590110604006`：注释称由独立 NumPy oracle 复算（`run/RELEASE-02/conform-fix-a/harness/conf1_oracle.py`），来源件不入库；**且逐字重复于两处**（本队列 `p1snr_linux_test.cpp:144` 与 `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp:165`，后者另注"取自 P2 t4_a p1_sources.json"）⇒ 同一实测锚**无唯一数值源、双份复制**，任一侧更新即静默分叉。同表 `kReal[5]` 的 20 个浮点钉值同状态。
- `kZPsyn = -14.269`（同族 `kK1=6.272203e-17`、`kK2=5.685037e-17`）：`git grep -F 14.269` 全仓仅本行一条真命中，注释只说"取自 L4 t2_m1_red 的 Gaia XPSD 拟合**量级**"，该拟合值不在跟踪集内。缓解：本用例是自证恒等式（:171 把同一个 kZPsyn 写进合成输入，:269-270 再断言乘积等于它，文件头 :23 自称"恒等式"），绝对值取错既不致漏判也不构成证真 ⇒ 记为登记缺口，不记为科学缺陷。
- `sigma_true` 用例（`p1_stars_test.cpp:204-212`）：2%/6% 判据的依据指向 `run/CLEAN-401/third_sigma/`（不入库），按标准 02 §1"B 级须结果落档"**不达标**。此处 6% 上界确实由实测 4.76% 上取，但同用例负例仍锁 2%（:229-231 自证"非放宽"）⇒ 与 142 s 案的差别在于**负对照守住了判别力**，可接受但须落档。

### 1.4 无量纲依据（阈值/容差缺导出或标定，5 条）

| # | 常数 | 现状 | 缺口 |
|---|---|---|---|
| N1 | **closure_rel_tol 生产默认 1e-6** | 唯一源 `phase1_product.h:131`（代码结构体默认），`docs/**` 内该键**零命中** | 科学文档（`docs/science/DRIZZLE.md:97`）把通量守恒称**严格不变量**，实现却用 1e-6 相对容差放行 ⇒ 缺一条 FP64 累加误差包络推导（对照 G-P1-WCS-BRIDGE 的 1e-8 px 有逐项推导＋适用域下限，本键两头都没有） |
| N2 | **min_sep_px ≥0.9（W4-A1 判别力门）** | 名义值应为 1 px（原点错置一位），行内 :1835-1836 给"防恒真"理由；文档条款写 **≥1 px**（`GATES_AND_TOLERANCES.md:67`） | 10% 松弛未登记；且实测分离度经 SIP 项（`A=8.0e-5·dx²`、`B=−8.0e-5·dy²`，:1824-1825）扰动，非严格 1 px ⇒ 属③需标定而未标 |
| N3 | **max_recip < 5e-3（IVAR-002(a)）** | `p1001_real_nodes_test.cpp:3261` | float32 互反偏差量级约 1e-7 ⇒ 5e-3 高约 4 个数量级，判据判别力弱；同文件对 1e-5 有逐项推导（:3756-3759），此处一字未给 |
| N4 | **tol = 1e-12（check_close 绝对地板）** | `p1snr_linux_test.cpp:52`；同值散落全仓 7 处（lib 4/eng 1/实验 2），**无一处给依据** | 标准 02 §3 明列 epsilon 须给物理或数值来源；`NUMERIC_STANDARD.md:68-71` 只对 variance_floor 的 1e-12 做过量纲论证，不可移用 |
| N5 | **裁剪 σ 倍数族无对比登记** | `cosmic_clip_sigma 5.0`（NOISE §5 冻结）、`background_clip_sigma 3.0`、`iterativeClipSigma 9.0`、`fwhmClipSigma 3.0` | 四套倍数各有条款、**无一处文档横向说明"为何不同、何时用哪个"**；标准 02 §3 要求有效有限域登记，而各 σ 倍数未标其统计假设（MAD vs RMS、单侧 vs 双侧） |

---

## 2 待确认与影响范围（UNRESOLVED 候选，9 条）

按"影响生产科学结论"排序；每条给保守取值方向。

1. **iterativeClipSigma 5.0 vs 9.0（L1）** — 影响面：P1 星点检测配置面可信度与 `docs/contracts/PUBLIC_API.md:833` 的登记正确性；当前无行为影响（字段零读取）。保守方向：**按 9.0 收敛**（与 README/PUBLIC_API/默认一致），或删字段并同步撤登记；不得两入口并存。
2. **io 面阈值出源（L3）** — 影响面：G-RES-01 契约的自述强度（"实现侧不得再出现字面量阈值"）与后续阈值改动的可追溯性。保守方向：把 `io_wait_high_percent=50.0`、"三低组合"的 20.0/5.0/15.0、`wall<5.0` **迁入 resource_gate_v1.json 并生成**，或在 JSON 内显式声明 io 面不在本契约管辖（择一，不留白）。50% 本身无标定：建议按 iostat 分布标定后再谈硬失败。
3. **closure_rel_tol 1e-6（N1）** — 影响面：drizzle 通量守恒门的松紧（Phase-1/Phase-2 产品验收）。保守方向：在给出误差包络推导前**不放宽**，并把"严格不变量"措辞改为"在 1e-6 相对容差内守恒"，或按 float64 累加项数推出容差下界。
4. **min_sep_px 0.9（N2）** — 影响面：WCS 原点判别的判别力声明与 G-P1-WCS-BRIDGE 条款一致性。保守方向：条款与执行统一（要么文档改 ≥0.9 并给 SIP 扰动推导，要么测试收紧到 ≥1.0−ε 并注明 ε 来源）。
5. **kRealSky 双份复制（1.3）** — 影响面：真实源 SNR 锚（两文件共 11 处引用）。保守方向：指定 `p1snr_linux_test.cpp` 一侧为唯一源、另一侧改引用同一常量文件；把 oracle 结果件（probe_out.json）落进 `实验/absolute-snr/results/` 以满足 B 级"结果落档"。
6. **kZPsyn −14.269（1.3）** — 影响面：仅恒等式测试激励。保守方向：登记拟合出处或在测试头声明"取任意量级值即可，绝对值无口径含义"。
7. **sigma_floor 测试侧 0.02（L2）** — 影响面：SYN-008 无缝结论的可迁移性（该用例不测冻结口径）。保守方向：夹具注释一句"σ_floor=0.02 与 kNoiseRms=0.05 同阶，为降权压力试验"；单位两说须由 `DATA_SEMANTICS` 一侧收敛。
8. **fwhmClipSigma 3.0（死参数）** — 影响面：`ipv_platform_binding_test.cpp:70` 一行的断言意义（README:121-123 称仅 debug 入口消费）。保守方向：不追值，追登记——要么给消费面，要么从 SDetParams 与 README/PUBLIC_API 同删。
9. **tol 1e-12 比较器地板（N4）** — 影响面：本文件全部 SNR 断言的下界。保守方向：登记为 `NUMERIC_STANDARD` 比较器条款的实例（如 K·u64，K 具名），七处共用同一登记。

---

## 3 覆盖率自报

- **判读行：50 / 50**（P402-BD-01 25 ＋ P402-BD-02 25），CSV 51 行含表头，每行 12 列机器校验通过。
- 同簇合并（共用一条出处/取证，**只在语义同源处合并**）：
  - `sigma_floor / snr`（＋同行未登记的 `smoothing_lambda 0.3`、`zero_anchor_weight 1e-3`、`max_iterations 60`、`tolerance 1e-9`）＝ UPM 配置结构体一行 5 常数，共用 UPM 侧出处 → 覆盖 `syn008_seam_main.cpp:103-105` 的 7 个数值位点；
  - `read_noise_e / gain / dark / bias_dark` ＋ 同行 `q_adu 1.0 / flat 1.0 / bias_light 50.0 / r 1000.0 / alpha_m 0.5 / master_variance 1.0` → 共用"Phase-1 校准像素夹具"一条判定，覆盖 `v6_p1_integrate_test.cpp:99-151` 的 12 个位点；
  - `cpu_percent / cpu_p50_percent / cpu_mean_percent / iowait_percent / active_window_seconds / mem_bandwidth_percent` → 共用 G-RES-01 契约一条出处（6 符号，7 位点，含 `mon004:86` 的 0.85·4 推导）；
  - `kGaussFwhmFactor / kMoffat4FwhmFactor / fwhm_px` → 共用"两母函数因子＋跨块混用禁令"一条出处（DISP-STAR-007）；
  - `kZPsyn / zpk / snrf / expect_zp / ZP_k` → 共用 N2 恒等式一条出处（5 符号，含 `kK1/kK2`、`0.4`、`6.0` 三个未入队列的同式常数）；
  - `snr_optimal / sigma_f / kRealSky / sigma_sky_adu / sigma_logflux_dex / legacy_dex / tol` → 共用"独立参考实现与真实源锚表"一条出处（7 符号，另覆盖 `kReal[5]` 表内 20 个钉值与 `kOracleMedian/kOracleFlux5/kGroupRefFlux` 3 个未入队列常数）。
- 合并后**实际覆盖数值位点 ≈ 92 个**（50 个队列符号位点 ＋ 42 个同簇邻居位点）。
- 队列外顺带扫出并已在 §1.1/§1.2 立案的 2 个新事实：`gen_geometry_truth.py:177` 的 58.6、`snr_science.cpp:279` 的 1.253。

---

## 4 判读成本实测（给前台定剩下 600 多个符号的预算）

统计口径：**"取证调用数/符号" = 本会话中该符号的出处判定实际引用了几次独立仓库读取调用**（`git grep`/`git show`/`ctx.py` 上下文提取/python 复算）；输出装配、读派单与标准、写文件等**不计数**，单列在末尾。

| 取证调用数 | 符号数 | 占比 | 典型符号 |
|---|---|---|---|
| ≤1 次 | 10 | 20% | kLimit、max_dev0、frac_scaled、sigma_true、sigma（假阳性）、zpk、snrf、sigma_sky_adu、legacy_dex、limit |
| 2 次 | 22 | 44% | snr、max_rel、read_noise_e、dark、bias_dark、gain、scale、max_pt、fwhmClipSigma、scale_deg_per_px、cpu 三条、kPxScaleDeg、r_scaled、gain_e_per_adu、scale_deg、FX_SIGMA、ZP_k、fwhm_px、tol、sigma_f |
| 3 次 | 16 | 32% | sigma_floor、closure_rel_tol、photometric_scale_a、iterativeClipSigma、active_window_seconds、mem_bandwidth_percent、min_sep_px、max_recip、gain_variance、kZPsyn、expect_zp、kRealSky、sigma_logflux_dex、两个 FWHM 因子、snr_optimal |
| ≥4 次 | 2 | 4% | **iowait_percent（4）**：要追契约 JSON＋生成头＋实现侧字面量才能定"唯一数值源被旁路"；**hips_pixel_scale（5）**：要跨 4 个组件（测试/生产 writer/头文件注释/浏览器工具）比单位口径 |

- **均值 2.22 次取证调用/符号**；**96% 的符号在 3 次以内定出处**（含"出处=无"的判定）；**4% 需要 4–5 次**。
- 必须标待确认（④）的比例：**9/50 = 18%**（清单见 §2）。
- 摊薄效应是主要成本来源：本会话**直接取证 33 次调用服务了 50 个符号**（合计 111 次"符号↔取证调用"引用 ⇒ **3.4 符号/调用**），因为队列按文件聚批——`ctx.py` 一次调用带出 10 个位点的上下文，一次 `git grep` 循环带出 7–8 个符号的三侧分布。**⇒ 派单给队列时"同文件符号排在一起"能显著降本，勿按符号名字典序切批。**
- 真实成本结构（本会话合计 ≈59 次工具调用，按会话台账人工归集）：取证 33（56%）、装配/输出 20（34%）、读派单与标准 6（10%）⇒ **总成本 1.18 次调用/符号、纯取证 0.66 次/符号**。
  - 装配类 20 次里有 **6 次本可避免**：CSV 字段内含半角双引号/反斜杠导致 4 次语法返修、`rg` 本机缺失 1 次、Edit 未命中 1 次。**给后续批的机械建议**：判读行的字段内一律不用半角双引号与 `\n`（改用中文括号或删引号），可省掉约 10% 的总调用。
- **给前台的判断建议**：按本批实测，具名符号队列若 600 余个、且其中 2/3 与本案同为"结构性/激励值"，则按总成本 1.18 次调用/符号计 ≈ 710 次工具调用（其中纯取证 ≈ 400 次）；但**逐符号判读里 66% 的产出不提供科学链路上可行动的结论**（只是把"不适用"写实）。本案 5 条可行动立案（多侧不一致 4 类＋该导出未导出 5 类）全部来自**跨文件追同一名键**这一动作，而该动作在机械层已能预筛：`iterativeClipSigma`、`hips_pixel_scale`、`sigma_floor`、`closure_rel_tol`、`kRealSky` 都是"同符号 ≥2 侧且值/单位不同"才值得取证的对象。**建议下一步不做全量逐符号判读，改做"多侧候选预筛＋只判预筛命中"的路线**（预筛＝机械按符号名聚全部位点并按 lib/eng/docs/config 分侧取值比对，本案已验证该动作可用 `git grep` 一次调用完成 7–8 个符号）。

---

## 5 确定命令（计数与统计可复算）

**落档件（本判读所有 `tot=` 数的唯一出处）**：
`独立审计/复算件/q402-BD1/patterns.txt`（50 行"符号|检索式"）→ `独立审计/复算件/q402-BD1/counts.sh` → `独立审计/复算件/q402-BD1/counts.txt`（分侧计数表）；数值复算件 `独立审计/复算件/q402-BD1/numerics.txt`。
CSV 内 8 处计数已按 counts.txt 逐行校正（首过扫描的 artifacts 排除式对带引号路径失效，read_noise_e 353→348、cpu_p50 25→22、cpu_mean 30→26、active_window 30→29、gain_e_per_adu 261→258、gain_variance 85→84，并补 hips_pixel_scale 26 与 sigma 2）。

```bash
REPO="F:/Astro dev/Astro CS Normalization Database"

# (1) 每符号出现处数与分侧分布（即 counts.sh，逐符号读 patterns.txt 的窄检索式）
cd "$REPO" && sh "独立审计/复算件/q402-BD1/counts.sh" > counts.txt
#   内部式：tot=$(git grep -nE -- "$pat" | grep -vc '^artifacts/')
#           lib/eng/docs/实验 四侧各一次 git grep -nE -- "$pat" -- '<侧>/**'

# (2) 泛名必须带词边界，否则计数虚高（本案实测）
git grep -nE -- '\bsigma_f\b' | wc -l     # 55
git grep -nE -- 'sigma_f'    | wc -l      # 1755（前缀吞掉 sigma_floor 等）→ 32 倍失真
git grep -nF -- '14.269'                  # -F 定值检索：全仓仅 1 条真命中

# (3) 数值复算（本文所有"本机复算"出此：脚本 numerics.py，输出 numerics.txt）
python 独立审计/复算件/q402-BD1/numerics.py
#   逐项：
  2*sqrt(2*ln2)=2.3548200450309493（与字面量二进制相等）
  2*sqrt(2)*sqrt(2**0.25-1)=1.2303076525901024 vs 1.230310 → +1.908e-6
  180/pi*sqrt(pi/3)/512=0.11451621372724687 deg → 412.2583694180887 arcsec
  180/pi*sqrt(pi/3)/1  =58.632301428350395 deg（= gen_geometry_truth.py 的 58.6）
  1/3600 = 0.0002777777777777778（== kPxScaleDeg，二进制精确）
  sqrt(pi/2)=1.2533141373155001 vs 1.253 → −2.5065e-4
  1000/2+25/4 = 506.25（= gain_variance 期望值）
  median(kReal[].snr_optimal)=16.04022427426992==kOracleMedian；median(flux)=11581.213134765625==kGroupRefFlux

# (4) 处置/来源统计（§0 两表出此）：读 raw/AUD-402-判读-BD1.csv，按 处置 前缀 Counter 计数

# (5) 0 命中自证（判"零命中"前，同一命令先在一个应命中对象上跑通）
git grep -c "iowait\|io_wait" -- eng/contracts/resource_gate_v1.json   # 无输出、rc=1 ⇒ 真零命中
git grep -c "compute"          -- eng/contracts/resource_gate_v1.json   # 命中 6 行 ⇒ 命令与检索式可用
git grep -n "closure_rel_tol" -- 'docs/**' | wc -l                     # 0；同式对 sigma_floor 跑 → 25（见 counts.txt）
```

## 6 我证伪了队列输入里的哪个机械结论

1. **`hips_pixel_scale = 412.258369` 判读行不成立**：该行是 `CHECK(props.find("hips_pixel_scale=412.258369") == npos)`，即"旧角秒口径缺陷指纹必须不存在"的**负例断言**；现行产品值 0.114516（度）。机械层把反例指纹记成现行值，会把一条**已修且已双向锁死**的口径写成缺陷候选。
2. **`sigma = .6f`（p1_stars_test.cpp:227）与 `ZP_k = 2.5`（p1snr_fref_baseline_test.cpp:271）是假阳性**：前者是 `fprintf` 格式串 `sigma=%.6f`，后者是失败消息文本 `"N2: ZP_k = ZP_syn - 2.5log10(k_photo)..."` 里的公式文字。二者都不是数值常数 ⇒ 队列里 **2/50 = 4% 的具名符号是字符串内文字**。
3. **多条"现行值"丢了表达式结构，制造了不存在的常数**：`scale = 1.0`（实为 `(i % 17) + 1.0`，17 档扫参）、`limit = 1.5`（实为 `per_node + per_node/2`，且注释 :306 明写"判据的独立输入，非硬编码"，是**正例**）、`fwhm_px = 0.25;1.5`（实为 `1.5 + 0.25*i` 九档扫参）、`legacy_dex = 1.0`（实为 `1/(ln10·0.05)` 负例构造）、`scale_deg = 180.0;3.0;512.0`（是 IVOA 导出式的成分）、`expect_zp = 2.5`（同上，是式系数）。⇒ 共 **6/50 = 12% 的行**若按登记值判读会得出错误结论（尤其"1.5 的 1.5× 余量无依据"这类判法，实际代码已经按公式写）。
4. **`snr = 1.0;100.0` 的 1.0 不是该符号的值**：:96 一行三赋值（`uncertainty=kNoiseRms; snr=100.0; support=1.0`），机械层把 `support=1.0` 并给了 `snr`。同类"同行串值"另见 `dark/gain/read_noise_e`（来自 :99-151 的相邻行合并抽样）。
5. **`来源现状=无来源（行内字面量）` 对 33/50 行不适用而非缺失**：这些是累加器初值、JSON 读取兜底、结构体成员默认与测试激励，**标准 02 §4 的四选一不适用于它们**（机械层把它们一律推给"待确认"会虚增 UNRESOLVED 面）；本案 9 条真待确认已在 §2 具名。
6. **计数口径本身要订正**：`sigma_floor` tot=165、`read_noise_e` tot=353、`fwhm_px` tot=437、`gain_e_per_adu` tot=261 等"出现处数"**含 `实验/**` 与同名异址**（如 `sigma_true` 421 命中里 388 在实验目录），不是同一常数的副本数；机械层若要给 D4 台账"副本数"，必须按"同一声明形式"重算。本案按词边界/定值检索重算的差异见 §5(2)（`sigma_f` 55 vs 1755）。
7. **一处机械漏判**：`max_recip`（本队列判为结构性累加器）所在用例的**真判据 5e-3**（:3261）未被机械层登记成任何一行 ⇒ 无名容差旁表按"行内字面量"抽，但抽了累加器、漏了判据，说明预筛的语义粒度需要补一条"CHECK/断言实参中的字面量优先入册"的规则。

## 基线自证

```bash
git -C "F:/Astro dev/Astro CS Normalization Database" status --porcelain
# 开工：?? "ACSD\346\225\264\346\262\273\345\267\245\344\275\234\345\214\206_AUDIT-06.zip" / ?? site/
# 收工：（见下，原文照录）
git -C "F:/Astro dev/Astro CS Normalization Database" rev-parse --short HEAD   # c8f64e9a（开工/收工同）
```

差异：**无**（两行未跟踪项与开工一致，工作树未被本会话改动；本会话全部写入仅落在 `独立审计/证据/` 与 `独立审计/复算件/q402-BD1/`）。

<!-- PROGRESS: 已判行 50/50（P402-BD-01 25 + P402-BD-02 25；同簇合并实际覆盖 ≈92 个数值位点） -->
