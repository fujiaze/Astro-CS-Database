# 全仓对抗性静态排查 · S16 · 实验单元 P4 dense-snr-reconstruct

**检查员**：S16 只读检查员（只查不改；本文件为唯一产出）
**责任域**：`实验/dense-snr-reconstruct/` 全部文件（REPORT_paper.md / REPORT_experiment.md / README.md / refs.md / docs/derivations.md；code/ 与 results/ 静态核对，不运行）
**方法**：① 先读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表与 `分歧台账.md` D-01…D-11/A-P4-xx 终裁（不重报已裁决与已 PASS 项）；② 逐文档全文通读 + 逐条 file:line 锚实开核对；③ 报告读数对 `results/` 存档 JSON 逐值比对、对 `code/` 源码逐行比对；④ 文献 DOI/arXiv 用 Crossref / arXiv API 抽验；⑤ 与 docs/science/algorithms/PHASE2_SAMPLER.md、docs/science/UNCERTAINTY_AND_COVARIANCE.md、修复包②/④、docs/ASTROCS_DESIGN.md 互查（③面）；⑥ "文档说有、代码没接"专项：defaults.json / schema enum / lib 源码逐键核对。
**纪律**：零运行、零写盘（除本报告）、零 git 写；涉及科学公式/默认容差/冻结定义的问题只登记上呈（红级＋证据），不给越权改法。
**日期口径**：当前工作树静态读数。

---

## 一、问题记录

### 红（3 项）

---

**红-1 | 红 | ④（兼②）**
**文件:行**：`实验/dense-snr-reconstruct/REPORT_experiment.md:6, :74-82`；`README.md:5`；`REPORT_paper.md:5`；`code/run_all.sh:6, :17`；`code/route1/exp_p4_01_weight_optimality.py:23`（exp_p4_02:34、exp_p4_03:34、exp_p4_04:45 同式）；`code/route2/exp_P4R2_0x_*.py:25-26`（各文件）；`code/route3/exp0x_*.py:17/:20/:27/:18/:21/:24`
**问题描述**：三路脚本输出路径解析规则互不一致，且**没有一条**把产物写进存档所在的 `results/route{1,2,3}/`；REPORT_experiment §7 声称"脚本输出路径指向各 route 的 results/（本单元内同名目录）"与代码事实不符；§7 的单项示例在文档给定的工作目录下，route1 一条**必失败**。
**证据**：
- `run_all.sh:6` 为 `cd "$(dirname "$0")"` ⇒ 运行时 CWD = `code/`。route1 各脚本 `OUT = "../results/xxx.json"`（**CWD 相对**，且 route1 全目录 `grep mkdir` 零命中）⇒ 落 `实验/dense-snr-reconstruct/results/` **顶层**，非 `results/route1/`；route2 `RESULTS = Path(__file__).resolve().parent.parent / "results"` ⇒ `code/results`，且 `exp_P4R2_01:26` `RESULTS.mkdir(parents=True, exist_ok=True)` **新建**该目录（单元 `code/` 下原本只有 route1/route2/route3 + run_all.sh + SEEDS.md，find 已列）；route3 `OUT = "results/xxx.json"`（CWD 相对）⇒ `code/results`。
- §7 三条命令的 CWD = 单元根（`cd "实验/dense-snr-reconstruct"` 后直接执行）：route1 ⇒ `../results` = `实验/results`——实测 `ls -d "实验/results"` ⇒ *没有那个文件或目录* ⇒ `open(OUT,"w")` 必 `FileNotFoundError`；route3 ⇒ 落单元 `results/` 顶层（非 `route3/`）；route2 落 `code/results/`。
- `run_all.sh:17` 提示 "compare outputs with results/route{1,2,3}/*.json"，但按上述三条路径无一产物落该处 ⇒ "逐字段同构"无法按提示核对。
- **反方核验**：原审计目录布局为 `独立审计/实验重做/P4重建稠密SNR/路线N/{code,results,report.md,refs.md}`（find 全列）；在原布局下 route1（CWD=路线N/code）与 route2（__file__ 相关）可正确，route3 仍依赖 CWD=路线N ⇒ REPORT_experiment:82 "若在审计原目录重跑则输出到原 results/，两者逐字段同构"对 route3 同样不成立。本条为**静态推断**（依 CWD 解析规则 + 目录存在性 + 源码逐行），未运行任何脚本（纪律）。
**建议改法**：三路输出统一改为按 `__file__` 解析到 `results/route{1,2,3}/`，并同步 REPORT_experiment §7 的三条命令、:82 的落位说明、`run_all.sh:17` 的比对提示与 README:5 / REPORT_paper:5 的复现指针。（只读纪律：只登记，改动由前台执行。）

---

**红-2 | 红 | ②（兼①）**
**文件:行**：`REPORT_paper.md:11, :38, :88, :148`；`REPORT_experiment.md:16, :48, :57`；`README.md:9, :25`；`refs.md:28`
**问题描述**："节点复现残差 ≤3.6e-15"作为**跨路/全部算子**的最大值在 7 处重复（含摘要、结论、README 一句话结论），被本单元自己的存档 JSON 否证；且同一份 REPORT_paper 内 `§2.3 (:38)` 写"1.8e-15 / 3.55e-15 / **≤9.3e-15**"，与摘要/§4.2/结论的"**≤3.6e-15**"两说。
**证据**：
- `results/route3/exp05_operator_axioms.json:22`：`A3_node_reproduction.max_abs_deviation["natural_bicubic_spline_clip_v1"] = 9.325873406851315e-15`——同一**生产默认算子**，9.3e-15 > 3.6e-15；`summary.json` 自记 `node_reproduction_max_abs.route3_all_ops = "<=9.3e-15"`。
- `REPORT_paper.md:88` 声称"**全部算子 × 全部 Δ** 最大绝对残差 3.55e-15（路线1）/ 3.6e-15（路线2）/ 1.8e-15 样条实现（路线3）"——三处引锚 `exp_p4_02, exp_P4R2_02, exp03` 均未含 exp05 的 9.3e-15；同段 :38 又写"三路实测残差 … ≤9.3e-15"并在同括注内写"实测 ≤3.6e-15"。
- 同 `exp05:23`：`..._mesh_median_v1` 节点偏差 **38.33**（附 "mesh filter is unconditional by spec" 说明）⇒ "全部算子节点复现"若含 mesh 档更不成立，须限定口径。
- **反方核验（不改判）**：对 1e-9 门仍余六个数量级，全部 gates `pass=true`（`exp05.all_pass=true`、`exp_p4_02.all_gates_pass=true`），科学结论方向不变；route1=3.55e-15、route2=3.55e-15（`exp02 node_reproduction_max_abs=3.552713678800501e-15`）、route3 exp03 样条=1.776e-15 单独看均 ≤3.6e-15 成立 ⇒ 缺陷是"跨路最大值/全部算子"的表述与同文两说，不是门或结论。
**建议改法**：把摘要/结论/README/实验报告的 ≤3.6e-15 限定为"分路读数（route1 3.55e-15、route2 3.6e-15、route3 exp03 样条 1.8e-15）"，另给"跨全部测次最大 9.3e-15（exp05 A3），mesh 档按 pre-filter 口径另计"；`:38` 与 `:88`、`:11`、`:148` 全部同步，避免同文两说。

---

**红-3 | 红 | ①**
**文件:行**：`实验/dense-snr-reconstruct/docs/derivations.md:35`（§6 Moffat4 FWHM/σ 闭式）
**问题描述**：理论腿推导链 **"2D rms 半径口径 σ² = ⟨r²⟩/2 = α²/2"** 两个等号至少一个不成立：链内自相矛盾，且文中数值 1.2303077 并不由该式给出。
**证据**：
- 独立复算：β=4 的 Moffat 归一矩 ⟨r²⟩ = α²·[∫₀∞ t(1+t)⁻⁴dt] / [∫₀∞ (1+t)⁻⁴dt] = α²·(1/6)/(1/3) = **α²/2** ⇒ ⟨r²⟩/2 = **α²/4 ≠ α²/2**（等式链断裂）。
- 数值一致性反证：FWHM = 2α√(2^{1/4}−1) = 0.86994α；文中登记 1.2303077 = 0.86994α / (α/√2)，即只在 **σ² = ⟨r²⟩ = α²/2（σ = √⟨r²⟩，2D rms 半径）** 时成立；若按字面 "σ² = ⟨r²⟩/2 = α²/4" ⇒ σ = α/2 ⇒ FWHM/σ = **1.7399 ≠ 1.2303077**。
- 同文高斯对照 1.6651 = 2.3548/√2（`results/route2/exp09:7-8` `gaussian_control_fwhm_over_sigma=1.6651092207543554`、`gaussian_theory_2sqrt_2ln2=2.3548200450309493`）同样要求 σ = √⟨r²⟩，与 "/2" 矛盾。
- **数值本身无误（反方核验）**：`exp09` `analytic_value=1.2303076525901024`、`numeric_value=1.2303077930596018`、`registered=1.23031`、`rel_err=1.793808388299165e-06`，与 REPORT_paper/:111、REPORT_experiment:23、README 引用一致；错的是推导等式，不是登记常数。
**建议改法**：属科学公式面 ⇒ **只登记上呈（红级＋证据），不给越权改法**：请科学文档负责人按变更流程裁定 σ 口径（"σ² := ⟨r²⟩ = α²/2" 或补出 ⟨r²⟩ 中间量并同步 1.6651 高斯对照的口径说明），定稿后同步 derivations §6 与引用该式的两份报告。

---

### 黄（12 项）

---

**黄-1 | 黄 | ②**
**文件:行**：`REPORT_paper.md:61`（对照同文 `:59`）
**问题描述**：结论段写 "**γ=1 = 以约 26 倍组合方差损失**换保守"，与同节 :59 "γ=1 相对最优效率 **3.73**、等权 104.58；最差 γ=0 比 γ=2 差 26.3 倍"自相矛盾——26.3× 属 γ=0。
**证据**：`results/route2/exp01_weight_identity_gamma.json`：γ=0 → 1.6997640123594642e-04、γ=1 → 1.2460562970307707e-05、γ=2 → 6.457851872185678e-06、`worst_over_gamma2_gain = 26.32088883426455`（= γ=0/γ=2）；γ=1/γ=2 = **1.93**。`results/route3/exp01:28` γ=1 efficiency = **3.730388848795341**。REPORT_experiment.md:15 的写法（"γ=0 差 26.3×、γ=1 效率 3.73"）与存档一致。
**反方核验**：分歧台账 A-P4-02 证据栏原句即"改 γ=1 = 以 26.3× 方差损失换保守的明确代价"，报告系照抄台账；本条**只修报告内部两读**，不动 A-P4-02 主结论（γ=2 非自由参数、审查"改 γ=1"不成立），不构成翻案。
**建议改法**：改为"γ=0 = 26.3×（route2），γ=1 = 3.73×（route3）"，或在引台账原句时标注其 26.3× 对应 γ=0；两报告与台账的口径差异一并登记给总编。

---

**黄-2 | 黄 | ①**
**文件:行**：`REPORT_paper.md:131`；`REPORT_experiment.md:20`（H6 行）
**问题描述**："实测 1.007/2.025/3.956/7.980 vs 理论 1/2/4/8（**0.5% 内**）"——实际相对偏差 0.726% / **1.247%** / 1.094% / 0.254%，最大 1.25%，无一档（除末档）在 0.5% 内。
**证据**：`results/route2/exp04_plane_conditioning.json` `measured_inflation_* = 1.0072591712986452 / 2.0249335974669953 / 3.956234266717374 / 7.979643761136983`，`theory_inflation = 1.0 / 2.0 / 4.0 / 7.999999999999993`；同 JSON `conclusions` 自记"实测 SE 各向异性放大与理论 … **在 10% 内一致**"。反方核验：我独立复算四个百分比同上；结论词"保守守卫"不受影响。
**建议改法**：改"（相对偏差 ≤1.3%，最大档 axis=2 为 1.25%）"，或直接引存档口径"10% 内一致"。

---

**黄-3 | 黄 | ①**
**文件:行**：`REPORT_paper.md:92`
**问题描述**："高对比域 Δ≥32 时全部五个算子 E>0.5、**算子间极差 <2.5 倍**"——按**倍数**口径在 Δ=32、Δ=64 不成立；只有按**绝对差**（max−min，E 无量纲）才 <2.5，"倍"字与该读法不相容。
**证据**：`results/route3/exp03_sparse_dense_reconstruction.json` domains.highcontrast：Δ=32 五算子 E = [0.5122, 0.6562, **1.3895**, 0.6848, 0.8896] ⇒ max/min = **2.71**；Δ=64 = [1.7197, **3.6419**, 1.4719, 1.2964, 2.3141] ⇒ **2.81**；Δ=128 = 2.27、Δ=256 = 1.25。绝对差 max−min 分别 0.877 / 2.346 / 1.894 / 0.379，均 <2.5。"全部五个算子 E>0.5"本身成立（Δ=32 最小 0.5122 > 0.5）。
**建议改法**：改为"算子间极差（max−min）< 2.5（E 无量纲）"或改按倍数给出真实值（Δ=32/64 为 2.7/2.8）。

---

**黄-4 | 黄 | ③**
**文件:行**：`REPORT_paper.md:92, :140`（对照 `docs/ASTROCS_DESIGN.md:429`、`docs/contracts/UNIFIED_OBJECTS.md:117`、`docs/plugins/algorithms_phase1/07_noise_snr.md:102`、`docs/science/CONTROL_WEIGHT_SNR.md:217`）
**问题描述**：P4 论文在"未分辨结构域"给出 "**算子选型不是主要矛盾，控制点估计量才是**"（:92）并把"mesh 档高对比域胜出"改写为"依赖控制点被未分辨结构抬偏的估计器偏差前提、单独算子对比无结论力"（:140）；而 L1 与正本是**正向强制约束**：HST 类高对比域默认档 Δ*=32、生产 Δ=64 落失效区（劣 2.6 倍）⇒ **必须**显式改 `..._mesh_median_v1`，design:429 明写"该结论与重建算子绑定（mesh 档下稀疏反而更优）"。同名域、方向相反的选型结论。
**证据**：四方原文已列（UNIFIED_OBJECTS:117 "该域**必须**显式改用 `..._mesh_median_v1`（Δ*=128…）；默认档是**回退**"；07:102 "只有 mesh 档（0.0490）胜出"；CONTROL_WEIGHT_SNR:217 同）。
**反方核验**：`grep -n "mesh" 分歧台账.md 与 五单元成稿简报.md` 均**零命中** ⇒ P4 的"估计器偏差前提"改写**没有 D/A 条目背书**（唯一事实源未裁过）；且两侧数据不同源——P4 引的是 route3/exp03 合成尖峰 fixture（其中 mesh 在 Δ=32/64 反而最差 1.3895/3.6419），正本数字来自 实验/absolute-snr EXP-04 的 HST 模板 fixture ⇒ 两说**可并存但必须限定 fixture/域**，否则读者读成"高对比域算子选型不重要"，与 L1 正向约束相抵。
**建议改法**：在 :92/:140 明确"本条只对 route3/exp03 合成尖峰 fixture 成立；与 docs/ASTROCS_DESIGN:429 / 07 §4.5 的 HST 模板域 mesh 强制档并存，选型规则以正本为准"，并把该口径分歧登记总编（是否回填台账）。

---

**黄-5 | 黄 | ④**
**文件:行**：`REPORT_paper.md:41, :148`；`REPORT_experiment.md:49, :63`；`README.md:25`
**问题描述**："IDW 插值设置（idw_power、K）**配置化，每次重建在运行日志输出实测最优 p*（argmin）与 K**"以现在时陈述，但源码里**既无配置键、也无日志、也无可达 IDW 重建路径**——典型"文档说有、代码没接"。
**证据**：
- `grep -i "idw" eng/packaging/config/defaults.json` ⇒ **零命中**（无 idw_power 键，K/γ 亦无）；
- `eng/contracts/schemas/unified/sparse_snr_layer.schema.json:307` enum 仅四值（natural_bicubic_spline_clip_v1 / …_mesh_median_v1 / bilinear_regular_grid_v1 / nearest_control_point_v1）——**词表本就无 IDW 档**（与 D-05"生产词表无 IDW 档"一致，此点 P4 写法正确）；
- `grep -ri "idw" lib` ⇒ 仅 `lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp`（`snr_model` 块序列化字段）与 `healpix_db/archive/legacy/…/snr_evaluator.cpp`（默认 `idw_power_=2.0`），**全仓无任何输出 p*/argmin 的日志代码**；
- `独立审计/08_修复包/②跨帧绝对信噪比/01_缺陷清单.md:62`：`hp_drizzle_run_phase1_hips` 的 **IDW 稠密重建依赖从未挂上的 snr_model 块**（生产路径不可达）；
- `分歧台账.md` D-05 影响栏把"defaults/配置（idw_power 2.0→1.0，**配置化+日志，负责人已批**）"列为**待办**（未落地）。
**反方核验**：D-05 主结论（默认 idw_power=1.0、备选口径、配置级）本单元照写无误（README:9 "IDW 为备选口径…默认 idw_power=1.0（台账 D-05 终裁）"、paper:95-97 "备选口径（生产算子词表无 IDW 档）"、experiment:57）——**定位未被写成生产默认**，符合本片专项要求；问题仅在"已配置化/已日志输出"的时态。
**建议改法**：改为"**已批待接线**（D-05 影响文档在办：defaults.json 现无 idw_power 键、生产词表无 IDW 档、日志无 p* 输出）"，四处同步。

---

**黄-6 | 黄 | ③**
**文件:行**：`refs.md:26`；`REPORT_paper.md:159, :172`
**问题描述**：Aitken 引用仍标"**DOI 未解析**（锚形式 UNRESOLVED）"，与权威链已核状态冲突。
**证据**：`检查-修复验证.md:38` 黄-10（**PASS**）：absolute-snr 论文:106/:116 与 `docs/ASTROCS_DESIGN.md:426` 已更新为**已核 DOI 10.1017/S0370164600014346**（并注 INSPIRE 出版年 1936，卷 55 跨 1935–36），与 G-3 / `docs/science/PHOTOMETRY.md:305` / `CONTROL_WEIGHT_SNR.md:243` / `实验/photometric-magnitude/refs.md` V11 对齐；`独立审计/证据/核验-CIT-04.md:130` 记 "核验态：已核"。
**本次独立抽验**：`api.crossref.org/works/10.1017/S0370164600014346` ⇒ status 200，title "IV.—On Least Squares and Linear Combination of Observations"，firstAuthor Aitken，year **1936**，journal *Proceedings of the Royal Society of Edinburgh* ✓；refs.md 所记 `10.1017/S0080456800012684` 实测 **404** ✓（该号确错，但"因此 DOI 未解析"的结论已过时）。
**反方核验**：`分歧台账.md §4.2` 把 P4 的 Aitken DOI 列为开放项——本条**不翻案黄-10**（黄-10 已 PASS），只登记"P4 单元未跟上已 PASS 的全仓订正"。
**建议改法**：refs.md:26 与 REPORT_paper:159/:172 补 "DOI 10.1017/S0370164600014346（Crossref 已核，出版年 1936 / 卷 55 跨 1935–36）"，保留"旧试 10.1017/S0080456800012684 为 404"的对照；标注级可保留但缺口描述须改。

---

**黄-7 | 黄 | ④**
**文件:行**：`REPORT_paper.md:176`（+ `refs.md:17`）、`REPORT_paper.md:177`（+ `refs.md:18`）
**问题描述**：两条参考文献**题名与一手来源不符**（作者/编号/年份均对），而 refs.md:33-34 声称的核验方式恰是"题名/作者/年份逐字段比对"。
**证据**：
- arXiv:2207.12005（arXiv API，status 200）实际题名 = **"Finite-sample bias-correction factors for the median absolute deviation based on the Harrell-Davis quantile estimator and its trimmed modification"**，作者 Andrey Akinshin ✓、2022-07-25 ✓；文中写 "Finite-sample bias correction for the Mean Absolute Deviation"。
- arXiv:1905.08677 **v1 与 v2** 题名均为 **"The shape of the Photon Transfer Curve of CCD sensors"**；期刊版 `api.crossref.org/works/10.1051/0004-6361/201935508` ⇒ "The shape of the photon transfer curve of CCD sensors"（同一题名）；文中写 "…of **area array CCDs**"（crossref 题名检索无此题名条目）。
**建议改法**：按一手题名订正两条；或注明引的是另一版本/另一文献并补出处。

---

**黄-8 | 黄 | ④**
**文件:行**：`REPORT_paper.md:51`（对照 `code/route1/exp_p4_01_weight_optimality.py:42-43, :57`）
**问题描述**："路线1 …定权最优性 MC（**n=2000 随机 σ**、4 万次试验）"与本单元收录的代码不符：代码抽 **200000** 个 σ_F/F_ref 样本。
**证据**：脚本 `:42` `sig = 10.0 ** rng.uniform(-0.5, 2.5, size=200000)`、`:43` `Fref = … size=200000`、`:57` `n_trials = 40000`、`n_obs = 8`；`results/route1/exp_p4_01_weight_optimality.json` 只记 `n_trials=40000`、`n_obs=8`，**无 n=2000 字段**。反方核验：同句"4 万次试验"与存档一致；"n=2000"可追溯到原审计件 `独立审计/实验重做/P4重建稠密SNR/路线1/report.md:30`（原报告同写 n=2000），但**单元自证代码为 200000** ⇒ 属单元内"文档 vs 代码"漂移（原报告的 2000 是否指另一变量无法在本单元内仲裁）。
**建议改法**：改为 size=200000（或注明所指变量），并与原报告口径差异一并登记。

---

**黄-9 | 黄 | ②**
**文件:行**：`REPORT_paper.md:11`（摘要）
**问题描述**："四路独立验证最大相对偏差 **4.44e-16–5.55e-16**"区间与四路实际读数不符（漏掉 3.7e-16 与 4.4e-16）。
**证据**：四路 max = route1 **5.551115123125783e-16**、route2 **4.440892098500626e-16**（逐帧 F_ref 变体 5.55e-16）、route3 **3.7220768166518007e-16**、P2 路线1 **4.4e-16 = 2 ulp**（summary.json `identity_max_rel_dev` 五字段、REPORT_paper:57 逐路正文）⇒ 区间应为 **3.7e-16–5.55e-16**。
**反方核验**：`docs/derivations.md:9` "四路实测 ≤5.6e-16"（上界写法）无误；结论"机器精度成立"不受影响。
**建议改法**：摘要改区间或改写"四路最大偏差均 ≤5.6e-16（最小 3.7e-16）"。

---

**黄-10 | 黄 | ④**
**文件:行**：`code/route1/exp_p4_04_brightness_forward.py:49`、`code/route3/exp06_white_noise_and_purity.py:64, :78`（连带 `REPORT_paper.md:32`、`docs/derivations.md:3` 的括注）
**问题描述**：代码注释称 `g = 1.3 e⁻/ADU` 是 "**NOISE_MODEL.md S5c frozen convention** / frozen convention"，但正本并未冻结数值 1.3——假锚。
**证据**：`docs/science/NOISE_MODEL.md` 全文 `grep "1.3"` **零命中**；sed 184-215 实读 §5c，只冻结**单位与公式**（"g 是本仓冻结的增益，单位 e⁻/ADU"、`sigma_w² = sigma_bg² + S_src/g`），无数值。数值 1.3 的可查出处是 **`五单元成稿简报.md:167`**（"SNR_c = F_ref/√(σ_slow²+S_src/g)（g=1.3 e⁻/ADU）"，P4 精度约定）。derivations.md:3 写"符号与 NOISE_MODEL/CONTROL_WEIGHT_SNR 口径一致（…g = 1.3 e⁻/ADU）"把数值并入正本口径。
**反方核验**：`REPORT_paper.md:32` 的公式本身与 CONTROL_WEIGHT_SNR:92-93 / NOISE_MODEL §5c 完全一致，只有"数值来自哪"的归属错；单位 e⁻/ADU 正确。
**建议改法**：注释与括注改引 `五单元成稿简报.md:167`（项目约定），或注明"1.3 为本单元取值"，删去"NOISE_MODEL S5c frozen（数值）"的说法。

---

**黄-11 | 黄 | ④**
**文件:行**：`REPORT_paper.md:93`（对照 `code/route1/exp_p4_02_interpolators.py:221, :259, :336, :340`、`eng/contracts/schemas/unified/sparse_snr_layer.schema.json:331`）
**问题描述**：论文与存档记"节点位于 i·Δ+(Δ−1)/2（Δ=32 即 **15.5 px**）"，代码实取整数 `i·Δ+(Δ−1)//2`（Δ=32 ⇒ **15**）；`node_placement.misregistration_px` 以浮点 `(32-1)/2 = 15.5` 记账，而实际 center-vs-corner 两臂的偏移是 15 px。"冻结生效方式"的量化论证自身带 0.5 px 口径差。
**证据**：`:221` `return (np.arange(n) * delta + (delta - 1) // 2)`；`:259` `origin = float((delta - 1) // 2)   # cell_center_v1: node i at i*Delta+(Delta-1)/2`（注释写 /2、代码写 //2）；`:336` `Spline2D(g, 32, origin=(32 - 1) // 2)`；`:340` `"misregistration_px": (32 - 1) / 2`；JSON `misregistration_px: 15.5`。
**反方核验**：schema:331 逐字为 "其中心 = origin_x + i*Δ + (Δ−1)/2"，且坐标语义 "像素序号 p 对应坐标 p"（整数）⇒ 对偶 Δ 冻结公式给半整数，离散实现**必然**要取整；route3/route2 的 cell 中心实现亦为整数栅格 ⇒ 不是"实现错了"，而是**取整规则未被声明**，且 15.5 的记账值并非实算偏移。实测劣化本身可信（rmse 0.0135→0.0397、E 0.0030→0.0348 与 JSON `node_placement` 一致）。
**建议改法**：在 schema/论文侧明确"偶 Δ 取整规则（floor/ceil/最近）"，或把 `misregistration_px` 改记实算偏移 15 并注明冻结公式值 15.5；两报告同步。

---

**黄-12 | 黄 | ③**
**文件:行**：`REPORT_paper.md:57, :59` 与 `REPORT_experiment.md:59-69`（§6 诚实边界）——对照 `独立审计/08_修复包/②跨帧绝对信噪比/02_已确立的算法与验证程序.md:62, :161`
**问题描述**：本单元把恒等式 `w = SNR²/F_ref² ≡ 1/σ_F²` 作为机器精度验证的核心证据链，但**未继承修复包②对该恒等式明确写下的适用域限定**："只保证换算代码不出错，**不保证生产写入的分子真的是 F_ref**"，并要求另立"分子同源"判据（V-4）。
**证据**：修复包② 02:62 逐字："该恒等式的前提是分子取 F_ref … 不保证生产写入的分子真的是 F_ref"；02:161 V-4："只保证代数；分子取 F_i 的口径漏洞恰从这条门下通过 ⇒ 必须另立'分子同源'判据"。P4 §6 诚实边界 9 条中最近的一条是 ":65 γ=2 最优性以 SNR 估计无偏为前提；恒等式检验未测相关噪声"——**没有**分子同源条款。
**反方核验**：P4 的实验确实在自洽合成口径下（自己造 F_ref）检验恒等式，其结论本身不假；上游 `sparse_snr_layer` 合同（schema:375、修复包② 02:83）冻结"绝对 SNR、同一 F_ref"，故这是**适用域继承缺口**而非错误结论。
**建议改法**：在 REPORT_experiment §6（及论文 §7）补一条"恒等式只在分子与 F_ref 同源时成立；生产写入口径由修复包② V-4/分子同源判据把关（P2 面）"，与修复包②对齐。

---

### 绿（4 项）

---

**绿-1 | 绿 | ①**
**文件:行**：`REPORT_paper.md:131`；`docs/derivations.md:17`
**问题描述**："κ = 点云长短轴比（实测偏差 **≤0.03%**）"——axis_ratio=1 档实测 1.0003967 ⇒ **0.0397%**，超 0.03%。
**证据**：`results/route1/exp_p4_03_plane_geometry.json` theory_check：1.0→1.0003967（0.0397%），2→1.9994032、4→3.9988060、8→7.9976098、16→15.995213（均 0.0298–0.0299%）。判据结论（轴比恒等式、阈值 1/16）不受影响。
**建议改法**：改"≤0.04%"；可不改。

---

**绿-2 | 绿 | ②**
**文件:行**：`REPORT_paper.md:121`（同步 `results/summary.json luminance_follow_ratio`）
**问题描述**："重建场比 **3.19698**"与存档 3.1969498996439123（应写 3.19695）第 5 位小数不符；summary.json 汇总同为 3.19698（汇总与存档不一致）。
**证据**：`results/route2/exp07_luminance_chain_negative.json` `median_recon_snr_ratio = 3.1969498996439123`、`median_true_snr_ratio = 3.1968464743483356`；`REPORT_experiment.md:50` 用"3.1970"无误。
**建议改法**：改 3.19695（或统一写 3.1970），summary.json 同步。

---

**绿-3 | 绿 | ②**
**文件:行**：`REPORT_paper.md:5`
**问题描述**："复现：见 **REPORT_experiment.md §6** 与 code/run_all.sh"——§6 是"诚实边界"（:59-69），复现命令在 **§7**（:71-82）。
**证据**：REPORT_experiment 章节实读：`:59 "## 6 诚实边界"`、`:71 "## 7 复现命令"`。
**建议改法**：§6→§7。

---

**绿-4 | 绿 | ②**
**文件:行**：`REPORT_experiment.md:40`
**问题描述**："route2 …seed 20260926–20261003（**每实验一档**）"——exp09 无 seed。
**证据**：`results/route2/exp09_moffat4_factor.json` 无 `seed` 字段；`code/SEEDS.md:8` 已注明"exp09 无随机性（解析+数值积分，未用 seed）"；exp01…exp08 seed 逐文件核为 20260926/27/28/29/30、20261001/02/03 ✓。
**建议改法**：改"每实验一档（exp09 无随机性，无 seed）"；可不改。

---

## 二、已查无问题面（逐面说明查过什么、怎么查）

### ① 科学性（常数/公式/单位/量纲/适用域；对 D 系终裁）

- **D-05（IDW 默认）**：逐处核 `README.md:9/:25`、`REPORT_paper.md:95-105/:148`、`REPORT_experiment.md:49/:57`——**全部写"备选口径 / 非生产默认 / 配置级"，无一处写成生产默认**（本片专项要求满足）。IDW 全扫表 3 行（0/2%/6% ⇒ (p,K)=(2.0,4)/(0.5,16)/(0.5,32)，rmse 1.639e-3 / 6.827e-3 / 1.414e-2）与 `results/route2/exp02_idw_params.json` 全部 90 格扫描的最小值逐值一致（无噪最小 p=2.0,K=4=1.6393724870641428e-03；2% 最小 p=0.5,K=16=6.826568931039014e-03；6% 最小 p=0.5,K=32=1.413506…e-02）。
- **D-10（生产默认算子）**：`REPORT_paper.md:65` 明写"按 D-10 订正：旧稿总览'现行默认 bilinear_regular_grid_v1'及其旧 EXP-04 对比数字弃用，对比表统一采用路线1 EXP-P4-02 同 fixture"。**对比表 16 格逐值核对**（样条 0.0024/0.0135/0.0892/0.1878、双线性 0.0057/0.0215/0.0853/0.1798、IDW p=2 0.0148/0.0430/0.0985/0.1730、IDW p=1 0.0258/0.0687/0.1311/0.1853）＝ `results/route1/exp_p4_02_interpolators.json` 逐值一致；route3 E 表 15 格（含 Δ=256 样条 2.98e-1 反超）＝ `route3/exp03` 逐值一致。
- **A-P4-02（γ=2 非自由参数）**：`REPORT_paper.md:11/:61`、`docs/derivations.md §1`（Cauchy–Schwarz：(Σw_k)² ≤ (Σw_k²v_k)(Σ1/v_k) ⇒ w_k ∝ 1/v_k ⇒ w ∝ SNR²）推导自足、方向正确（黄-1 只涉 γ=1 的数字归属，不动主结论）。
- **A-P4-01（Δ=64 结构派生）**：`REPORT_paper.md:27` 与 `eng/packaging/config/defaults.json:652-657`（hips.tile_width 冻结 512）、`:694`（"默认 64 = hips.tile_width / 8 = 512 / 8"）逐字一致；"4096/8 疑点"处置与台账同向。
- **A-P4-03/04/05/06/07**：Shepard/Keys DOI 订正（`REPORT_paper.md:165/:168`，见文献抽验）、m_ref 降格记录口径（:31）、Δ=256 反超按 A-P4-07 采信（:92）均与台账一致。
- **常数独立复算（全部相符）**：`1/Φ⁻¹(0.75) = 1.4826022185056023`、`9216 = (1.44/0.015)²`、`2√2·√(2^{1/4}−1) = 1.2303077`、`2√(2 ln 2) = 2.3548`、`√10 = 3.1623`、`10^{-0.4} = 0.39810717`、`10^{-0.8} = 0.158489`、`0.4326/0.0367 = 11.8 ≈ 12`、`1.6998e-4/6.4579e-6 = 26.32`、`1.7399`（Moffat 反例）——均与引用处相符（唯一断裂见红-3）。
- **单位/量纲**：SNR 无量纲、F_ref [ADU]、w [ADU²]⁻¹（REPORT_paper:26）＝ 生产实现式 `lib/algorithms/integration/v6/src/weight_chain.cpp:91` `w = (snr/reference_flux)*(snr/reference_flux)`（**file:line 锚逐行实开，:91 逐字命中**）；`σ_slow²/σ_w²` 单位 ADU² 与 `CONTROL_WEIGHT_SNR.md:92-99` 一致；`σ_w² = σ_slow² + S_src/g` 与 :93、NOISE_MODEL §5c 同式（数值锚另见黄-10）。
- **负例与判据非退化（防恒真门）**：19 个脚本负例关键词命中数全部 ≥4（route1 12/28/14/24；route2 6/4/8/6/8/12/16/4/4；route3 5/14/23/10/9/8）；并实开 8 处核内容：route1 exp01 `flat_negative_control`（等 σ ⇒ 各方案 E 同为 0.0049，MC 噪声内）、route2 exp01 同方差负例 `gain=1.0125`（"metric collapses to ~1"）、route2 exp06 无源两臂 `max_rel_diff = 0.0`、route2 exp09 高斯对照 1.6651（≠1.2303 ⇒ 判据能区分）、route3 exp02 `flat wrong arm E=0.3137`、route3 exp03 `negative_control_flat E=0.0`、route3 exp05 收缩反例 0.54、route3 exp06 无源 `full==missing bitwise` ⇒ **无恒真门**。
- **H1–H10 判定表**：10 行关键读数逐行对存档复核，除已登记项外全部一致（含 H6 放大值、H7 白性/relspread、H8 E 性质、H9 三常数、H10 Δ² 律 4.06/279.8）。

### ② 行文逻辑（论证链、前后矛盾、订正注新旧两说、UNRESOLVED 混入）

- **全文通读**：REPORT_paper.md（1–178）、REPORT_experiment.md（1–82）、README.md（1–30）、refs.md（1–36）、docs/derivations.md（1–35）逐行读完；`code/SEEDS.md`、`code/run_all.sh` 全读。
- **订正注（<!-- 订正 -->）新旧两说核对**：`REPORT_paper.md:34`（k_shape×k_geo → k_gauss(N_retained)×k_geo，与 PASS 表红-4 判定一致，现文本已是新说）、`:38`（节点容差语义改挂"P4 豁免清单"、声明与 D-10 无涉，与 PASS 表黄-9 判定一致）——两处均**已修复且自洽**，按纪律**不重报**。
- **UNRESOLVED 未混入正文当结论**：全文 "UNRESOLVED" 仅出现在 `REPORT_paper.md:19`（"先前的 UNRESOLVED 或已裁决、或移入 §7 诚实边界"的陈述）与 `refs.md:22`（标注级小节标题，明写"不承担数值判据"）；未决项（Aitken/Moffat/de Boor、付费墙、版次页）全部落在 refs 标注级表与 §7 诚实边界 :159/:168，**无一条被写成结论**。
- **清单覆盖回溯**：`REPORT_paper.md:19` "路线1 8/8、路线2 9/9、路线3 12/12" ⇄ 原三路 report.md:193 / :41 / :27 逐字一致。
- **已登记的内部两说**：红-2、黄-1、黄-9、绿-2、绿-3、绿-4（均已开原文件给证据）。
- **种子/复现声明**：REPORT_experiment:43 与 SEEDS.md、run_all.sh 三处交叉核对（除绿-4 外一致）；"本单元不重跑实验、读数引存档"与 summary.json `provenance_note` 一致。

### ③ 跨文档冲突（五方权威链：L1 ↔ docs ↔ 实验 ↔ 独立审计 ↔ 源码）

- **默认算子面**：`docs/ASTROCS_DESIGN.md:428`（默认=自然边界双三次样条+值域钳制、词表、cell 中心几何）⇔ `docs/plugins/algorithms_phase1/07_noise_snr.md:184`（§4.5 词表表）⇔ `docs/contracts/UNIFIED_OBJECTS.md:109` ⇔ `eng/contracts/schemas/unified/sparse_snr_layer.schema.json:305-331`（enum 四值 + node_placement const）⇔ `lib/algorithms/integration/v6/src/weight_chain.cpp:142/:147-148/:393-399`（token 解析与散点档约束）⇔ 本单元 `REPORT_paper.md:65` —— **五方一致**（P4 正确写出 D-10 后口径）。
- **控制几何面**：`docs/science/algorithms/PHASE2_SAMPLER.md:46-49/:150-152`（grid=G 默认 8、cell_side = kTileWidth/G = 64、cx = gx*cell_side + cell_side/2、cell 中心 leaf）⇔ 本单元 `REPORT_paper.md:25/:27/:93`（Δ=64、cell_center_v1、i·Δ+(Δ−1)/2）⇔ schema:331 ⇔ defaults.json:694 —— **同源无冲突**（PHASE2_SAMPLER 对 IDW/插值算子**零命中**，故无算子口径交叉；半像素取整细节见黄-11）。
- **权重与 k_corr 面**：`REPORT_paper.md:34`（k_corr = k_gauss(N_retained)×k_geo，P4 只传递 N_retained 与几何元组）⇔ `docs/science/UNCERTAINTY_AND_COVARIANCE.md:51-53` 现行文本（含 PASS 红-4/红-7 修复后的全表 1.637/1.316/1.144/1.083/1.046/≈1.00）⇔ `README.md:26` —— **一致**。
- **IDW 面**：`分歧台账 D-05` ⇔ 本单元（备选、配置级、默认 1.0）⇔ 生产（词表无 IDW、defaults 无键）—— **定位一致**；"配置化+日志"的落地状态见黄-5。
- **修复包互查**：② 的 `02_已确立:57-62/:83/:146`（逆方差适用域、恒等式前提、控制点冻结语义）与本单元对照见黄-12，其余条款（D-01 收窄、`01_缺陷清单:62` IDW 路径不可达、`05_正向规格:584` P-CST-24 idw_power=2.0 保守方向 vs D-05 终裁 1.0）已核——D-05 后出、为本片判读依据，无冲突；**④面积交叠与分配对"重建/dense/IDW"零命中**（grep 全目录）⇒ 与 P4 无交叠条款，无可比项。
- **PASS 表先行规避**：红-4（P4 论文:34 k_shape 连带）、黄-9（P4 论文:38 挂 D-10）两条上轮遗留项已实开确认**当前文本为新说**，不重报。

### ④ 幻觉与锚（文献抽验 / file:line 锚 / 文档说有代码没接）

- **DOI 抽验（Crossref，本次实测）**：Shepard `10.1145/800186.810616` 200 "A two-dimensional interpolation function for irregularly-spaced data" 1968 ACM ✓；Keys `10.1109/TASSP.1981.1163711` 200 "Cubic convolution interpolation for digital image processing" 1981 IEEE TASSP ✓，而 `10.1109/29.90969` **404** ✓（refs.md 的 404 断言成立）；Franke `10.2307/2007474` 200 "Scattered Data Interpolation: Tests of Some Method" 1982 ✓；Lu & Wong `10.1016/j.cageo.2007.07.010` 200 ✓；Horne `10.1086/131801` 200 "An optimal extraction algorithm for CCD spectroscopy" 1986 ✓（refs:14 提示 `10.1086/131901` 为 Vrba 1986 易混号——实测 200，firstAuthor Vrba、1986，非 Horne，**该提示正确**）；Naylor `10.1046/j.1365-8711.1998.01314.x` 200 "An optimal extraction algorithm for imaging photometry" 1998 MNRAS ✓；**`10.1046/j.1365-8711.1998.01333.x` 200 = "Polarimetry of QQ Vul"（firstAuthor Cropper）** ⇒ `REPORT_paper.md:171` 与 `refs.md:15` 的"路线3 所记 01333.x 实为 Cropper 别文、判误订正"**本单元复核裁定成立**；Rousseeuw & Croux `10.1080/01621459.1993.10476408` 200 ✓；Aitken 见黄-6（所记旧号 404 ✓、但仓内已核号存在 ⇒ 黄-6）。
- **arXiv 抽验（arXiv API，本次实测）**：`1512.06872` = "How to coadd images? I. Optimal source detection and photometry…" ✓；`1512.06879` = "…II. A coaddition image that is optimal for any purpose in the background dominated noise limit" ✓（Zackay & Ofek 两条题录与用途限定均准）；`astro-ph/0409513` = "HEALPix — a Framework…" 2004 ✓；`1905.08677`/`2207.12005` 题名不符 ⇒ 黄-7。
- **file:line 锚实开核对（抽查全部命中）**：`weight_chain.cpp:91`（w = (snr/reference_flux)²，**逐字**）、`defaults.json:652/:694`（tile_width=512、Δ=64=512/8）、`sparse_snr_layer.schema.json:305/:307/:329-331`（词表、enum、cell_center_v1+(Δ−1)/2）、`UNIFIED_OBJECTS.md:109/:117`、`07_noise_snr.md:102/:184`、`PHASE2_SAMPLER.md:46-49/:150-152`、`CONTROL_WEIGHT_SNR.md:92-93/:214/:217`、`NOISE_MODEL.md §5b(110)/§5c(184)`、`docs/ASTROCS_DESIGN.md:231/:428/:429`、`检查-修复验证.md:38`、原三路 report.md:30/:41/:193/:27——全部实开命中；`REPORT_paper.md:65` 所引"05 P-ALG-09"经台账 A-P4-04（分歧台账:160）可溯、"P-CST-12"经 分歧台账/原路线报告 可溯 ⇒ 锚有效。
- **"文档说有、代码没接"专项**：`cell_center_v1`/节点几何 ⇒ schema const + route1 实现（黄-11 仅取整细节）；"空控制格按最近控制格值填充（空格≠零）" ⇒ `weight_chain.cpp:284 nearest_valid_fill` 定义、`:489` 调用、`:372-373` 空层 fail **实接线** ✓；"逐帧 F_ref、w 对 m_ref 不变" ⇒ `weight_from_snr(snr, reference_flux, …)` 参数化 ✓；"自然三次样条钳制为默认" ⇒ `weight_chain.cpp:142` 返回该 token ✓；**idw_power 配置键/日志 ⇒ 不存在**（黄-5）；`snr_path` 等死键问题属修复包② 已登记项，本单元未声称其可用（未涉）。
- **第三方代码**：本单元零依赖 `nanoflann/cfitsio/nlohmann`（run_all.sh:4 声明"无仓库内 import、无网络"，脚本 `import` 仅 json/math/pathlib/numpy，逐脚本核）⇒ 无对接面需查。

---

## 三、计数与摘要

| 严重级 | 数量 | 编号 |
|---|---|---|
| 红 | **3** | 红-1 复现命令落位（④②）、红-2 节点残差三值两说（②①）、红-3 Moffat 推导等式链（①） |
| 黄 | **12** | 黄-1 γ=1 归属、黄-2 0.5%内、黄-3 极差2.5倍、黄-4 高对比选型张力、黄-5 IDW 配置化无接线、黄-6 Aitken DOI 滞后、黄-7 两题名不符、黄-8 n=2000、黄-9 摘要区间、黄-10 g=1.3 假锚、黄-11 半像素取整、黄-12 分子同源适用域 |
| 绿 | **4** | 绿-1 κ 0.0397%、绿-2 3.19695、绿-3 §6→§7、绿-4 exp09 无 seed |

**摘要（三行）**：
1. 复现面是最大硬伤：三路脚本输出路径互不一致且均不落 `results/route{1,2,3}/`，REPORT_experiment §7 的单项示例在声明 CWD 下 route1 必 `FileNotFoundError`，§7:82 与 run_all.sh:17 的落位声明均与代码不符（红-1）。
2. 数字口径三处硬矛盾："节点复现 ≤3.6e-15"被本单元 `route3/exp05` 的 9.33e-15 与同文 :38 的"≤9.3e-15"双否（红-2）；derivations §6 "σ²=⟨r²⟩/2=α²/2" 等式链断裂（数值 1.2303077 反而对应 σ²=⟨r²⟩，红-3）。
3. 接线与题录漂移成组：idw_power"配置化+日志 p*"全仓无键无码（黄-5）、Aitken DOI 停在"未解析"而全仓已 PASS（黄-6）、两条 arXiv 题名不符（黄-7）、g=1.3 的"NOISE_MODEL 冻结"假锚（黄-10）。
