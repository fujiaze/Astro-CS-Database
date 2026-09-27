# 检查报告：S17 —— 实验单元 P5 additive-sky-seamless（全域静态对抗核查）

- **检查对象**：`实验/additive-sky-seamless/` 全域——`REPORT_paper.md`（223 行全文）、`REPORT_experiment.md`（139 行全文）、`README.md`（472 行，重点节全文＋其余抽查）、`refs.md`、`docs/`（smooth-lambda / seam-gate-floor / control-variance-adjudication / data-README）；`code/` 与 `results/` 静态交叉核对（不执行）。
- **权威链**：L1 `docs/ASTROCS_DESIGN.md` ↔ `docs/science/PHASE2_UPM.md` ↔ 实验交付 ↔ `独立审计/08_修复包` ↔ 源码 lib|eng；科学争议以 `独立审计/实验重做/总编对账/分歧台账.md` D-07/D-08/A-P5-* 终裁为准，不重复翻案。
- **预读排除**：`独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（红 7 PASS＋红-4 摘要 3 处后续闭合、黄 15 PASS＋黄-14 后续闭合、行漂移 Y-3 a–f 6/6、遗留清单 4 条闭合、观察项 4 条）已预读并逐条比对，**均不重复上报**；其「合法命中」清单（订正注/旧对照、healpix tables.md T2、KEY_RESULTS、REPORT_experiment g2 表、08_修复包 k_shape）已作为反向排除清单使用。
- **方法**：纯静态核查（无构建/无测试/git 仅只读 log）；数值反证为只读解析 JSON＋独立算术复算。
- **纪律**：科学公式/默认容差/冻结定义问题只登记上呈（红级＋证据），不越权给改法；`nanoflann.hpp`/`cfitsio`/`nlohmann/json.hpp` 等第三方仅接口面观察，无发现。

---

## 一、红级（4 项，必须改）

### 红-1 ①②｜REPORT_experiment.md:66 —— 「确定性下限」仍以对数口径闭式 e^{1e-2}−1 表述，属红-5/行文R2 已裁决错误的第三处未修残留

- **文件:行**：`实验/additive-sky-seamless/REPORT_experiment.md:66`（§4.3 结果表首行）
- **问题描述**：该行原文 `| 确定性下限 | 闭式 e^{1e-2}−1 = 0.0100503；1.0050% 判红、1.0049% 判绿 | [推导]＋[实验:route1/c3_seam_gate.py] |`。接缝门 rel_step 是正本 `PHASE2_UPM.md` §9a 的**线性差分比**，确定性下限按 §17.3 线性硬界 `gate/(1−gate/2)` 给出；把 `e^{1e-2}−1`（对数口径的跨 patch 电平比映射）冠以「闭式」作为下限定义，正是台账裁决与红-5/行文R2 判定必须更正的口径归属错误。
- **证据（含反证核验）**：
  1. 同单元 `REPORT_paper.md:74` 已按 R2 更正为「线性口径 …|Δ|/L ≤ gate/(1−gate/2) = **1.00503%**」并带完整订正注，订正注明写原句即「|Δ|/L ≤ e^{1e-2}−1 = 0.0100503」为错误口径；`docs/seam-gate-floor.md:26-37` 同款订正注。**红-5/PASS 的修复范围为 REPORT_paper:74 与 seam-gate-floor.md（:9/:18/:21-23/:29-30），未覆盖 REPORT_experiment:66**（PASS 表红-5 证据行逐字核对），残留 grep 关键词「对数口径（接缝门）」也扫不到本行（本行不含该词）——故为新发现，非重复上报。
  2. 算术反证：e^{0.01}−1 = 0.0100501671 → 7 位小数为 **0.0100502**，不等于行文所写 0.0100503；0.0100503 实为线性式 0.01/(1−0.005) = 0.010050251… 的舍入值。即该行**公式标签与数值本身互相矛盾**（标签对数、数值线性）。
  3. 同表「1.0050% 判红、1.0049% 判绿」与 `results/audit_rework/route1/c3_seam_gate.json`（A7=1.005e-2 红 / A8=1.004e-2 绿）相符——数值结论无误，错的仅是闭式归属。
- **建议改法**：按红-5/R2 已裁决口径对齐（与 REPORT_paper:74 同款线性闭式 `gate/(1−gate/2)`＋订正注，注明数值巧合），**具体文本上呈总编执行**；本条为科学口径问题，按纪律只登记不自改。

### 红-2 ③④｜固定 seed 声明与脚本头/固化 JSON 全面不符（6 处）：路线3 实为 20260926、路线1 其余实为 20260317–24 系、P2 补实验实为 20260601/05

- **文件:行**（同一根因，一并登记）：
  - `REPORT_paper.md:107`：「路线1 …接缝门 20260319、方差比 20260320、表示边界 20260325、**其余 20260926 系**）、路线2 e1–e10 与**路线3 exp01–exp12（seed=20250926）**」
  - `REPORT_experiment.md:26`：「**路线2/路线3/补实验 = 20250926**（写死于脚本头/`p5c_common.SEED_BASE`）；路线1 …**其余 20260926 系**」
  - `code/audit_rework/README.md:28-31`：「**路线2 / 路线3 / 补实验（含 P2 control_variance 补实验）：seed = 20250926**」「其余 **20260926 系**（以各脚本源码头部标注为准）」
  - `code/audit_rework/run_all.sh:26`「路线1（seed 20260319/20260320/20260325/**20260926 系**…）」、`:37`「**路线3（seed 20250926 系）**」
  - `results/audit_rework_summary.json:8`（`_meta.seed_note`）：「路线2/**路线3**/补实验 seed=20250926；路线1 按主题分段写死于脚本头。」
- **问题描述**：两份定稿报告的「数据/复现」声明与实际固定 seed 系统性不符；按其声明重跑将无法复现固化 JSON。
- **证据（含反证核验）**：
  1. **路线3**：`code/audit_rework/route3/exp01–exp12.py` 全部 `SEED = 20260926`；`results/audit_rework/route3/q1–q12.json` 12 件 `"seed": 20260926`（本会话逐件读取）。文档声称 20250926。
  2. **路线1 其余**：脚本头 `SEED = ` 逐件 grep——c1=20260317、c2=20260318、c3=20260319、c4=20260320、c5=20260321、c6=20260322、c7=20260323、c8=20260324、c9=20260325、c10 无 seed 字段。除文档点名的三件外，其余实为 **20260317/18/21/22/23/24**（202603 系），与「20260926 系」不符；20260926 是路线3 的 seed，出现在路线1 行属错位。
  3. **补实验**：`supp_507_relstep` 经 `p5c_common.SEED_BASE = 20250926` ✓（文档此部分正确）；但 `supp_control_variance` 实为 `finiteN: SEED = 20260601`、`production_chain: SEED = 20260605`、`spotcheck: SEEDS = [20260602, 20260603, 20260604]`（脚本头），固化 JSON `finiteN_control_variance.json "seed": 20260601`、`production_chain_control_variance.json "seed": 20260605`（读取核验）。REPORT_experiment:26 与 audit_rework README:28 把它并入「= 20250926」——**该补实验正是 D-07 定稿口径三口径的来源（REPORT_experiment:54-55 引用）**。
  4. 反向核验：路线2 e1/e10 JSON `"seed": 20250926` ✓，说明并非全部错——错位是逐路线/逐补实验的，三处错、两处对。
- **建议改法**：按脚本头与固化 JSON 的实测值改正 6 处声明（路线3→20260926；路线1 其余→20260317–20260324 系且 c10 无 seed；supp_control_variance→20260601/20260605 及 spotcheck 20260602–04），或统一改为「以各脚本头标注为准」的指针式表述。

### 红-3 ①④｜C7 真实数据样本量虚报 12 倍：「M42 场 49 帧（C7）」实为 4 帧 6 对

- **文件:行**：`REPORT_paper.md:108`（§3 数据第 3 条）、`REPORT_experiment.md:30`（「testdata 真实数据 M42（C7，49 帧）」）
- **问题描述**：两份定稿报告把 C7 真实数据腿的样本量写成 49 帧；C7 实际只取 4 帧。引句中的统计量本身就是 4 帧 6 对的统计——句内自相矛盾。
- **证据（含反证核验）**：
  1. `code/c7_realdata.py:54` `def load_real_frames(n=4)`、`:120` 调用 `load_real_frames(4)`；glob 目录为 `testdata/M42_T2T3_mosaic_Flying_dutchman/T3/M1/*Red.fts`（该目录 `find | wc -l` = **6**）。
  2. `results/c7_realdata.json`：`data` 数组长度 **4**；`pair_mismatch` 长度 **6** = C(4,2)——若 49 帧应对 1176 对。
  3. **引数自证**：REPORT 两处所引「斜率中位 0.995（0.806–1.323）」即 `mismatch_summary.slope` {n: **6**, median: 0.99464…, min: 0.80637, max: 1.32310}——同一句里的数字证明样本是 4 帧。
  4. 单元其余文档一致写 4 帧：`README.md:46`「真实数据（M42 M1 T3 Red，**4 帧**）」、`README.md:120/:230`、`data/README.md:37`「C7 取 4 帧」。
  5. 「49」的出处反证：`find testdata/M42_T2T3_mosaic_Flying_dutchman -name '*Red.fts' | wc -l` = **49**——即整个 M42 testdata 树的文件计数（与 C7 读取子集无关），被误挂到 C7 名下；另 `docs/smooth-lambda.md:74` 的「49 帧」是另一数据集（L4 p2_samples）的帧数，易混淆但非本句出处。
- **建议改法**：两处改为「M42 M1 T3 Red 4 帧（testdata M42 树共 49 个 *Red.fts，C7 取 4）」或等价的准确表述；真实数据腿结论（截距不可辨识）不受影响，但样本量必须如实。

### 红-4 ①③｜results/audit_rework_summary.json 的 D-08 陈述仍用 k_shape×k_geo 记号且公式面缺 k_gauss，与同块数值自相矛盾；无生成器、被实验报告引为汇总权威

- **文件:行**：`results/audit_rework_summary.json:32`（`kcorr_adjudication_D08.statement`）、`:33`（`two_factor`）；引用链 `REPORT_experiment.md:34`（「完整数字与来源路线见 …」）、`:134`（「汇总：…每个数字注明来源路线与固化文件」）。
- **问题描述**：statement 写「k_corr 改为 **k_shape x k_geo** 两因子公式 + …几何查表」，two_factor 把 `k_shape` 定义为「非高斯边际形状贡献，N>=9 时 |k_shape-1| <= 5%，**可忽略**」——公式面第一因子被声明可忽略且**不含 k_gauss**，按字面即 k_corr ≈ k_geo：N=5 紧凑端得 1.27，而同一对象 `frozen_1p4_consequences` 给 `kcorr_at_N5_compact = 2.05`——**同一 JSON 内两数互斥**（1.27 vs 2.05，唯一桥梁是被漏掉的 k_gauss(5)=1.63），恰是红1（行文R1/跨文档红1）裁决要消灭的「两读」，且会复现 D-08 判死的 32% 低估。该块亦与同文件 `declarative_obligations`「N<=25 时 k_gauss(N) > 1.08 不可忽略」自相矛盾。
- **证据（含反证核验）**：
  1. 记号冲突面：`REPORT_paper.md:67/:70`、`docs/control-variance-adjudication.md:68`、`PHASE2_UPM.md` §5 均已统一为 `k_corr = k_gauss(N_retained) × k_geo` 并带红1 订正注；本 JSON 无任何订正注（同文件 line 16 的 D-07 块反而带「正本原句…按 D-07 订正」——说明该 JSON 的惯例是随裁决带订正语，唯 D-08 记号块没有）。
  2. **非机器生成反证**：全仓 `grep -rln 'audit_rework_summary|kcorr_adjudication_D08' --include='*.py' --include='*.sh'` **为空**——无生成器，`statement`/`two_factor` 是手写散文，不属于 PASS 表「机器生成结果 JSON 原始键名」合法类（该合法清单逐字比对：healpix tables.md T2 / KEY_RESULTS / REPORT_experiment g2 表 / 08_修复包，不含本文件）。
  3. `_meta.authoritative_sources` 声明事实源为分歧台账+五单元简报——台账 D-08 line 91/:93/:245 确实保留旧记号（历史终裁文本、域外，不重复上报），但本 JSON 位于本单元 results/ 且被两份报告引为「每个数字注明来源」的汇总面，属本单元交付物自留的红1 残留。
  4. 数值链反证：按 statement+two_factor 计算 N=5 紧凑 k_corr = 1.0×1.27 = 1.27，与同块 2.05±0.09 差 38%，与台账「低估 32%」直接冲突。
- **建议改法**：按红1 已裁决的统一记号改写 statement/two_factor（补 k_gauss(N_retained) 因子、k_shape 降为附带说明），或至少加与 REPORT_paper:70 同款的记号订正注指针；**公式面改动上呈总编执行**。

---

## 二、黄级（4 项，建议改）

### 黄-1 ④｜docs/smooth-lambda.md 三处源码行锚全部失效（内容主张为真，行号不符）

- **文件:行**：`docs/smooth-lambda.md:40`（目标函数 `upm.cpp:543,669`）、`:43`（跨帧归一 `upm.cpp:645-649`）、`:103`（生产标志 `module_adapters.cpp:4991-5040`）
- **证据（实开逐行核验）**：
  1. `upm.cpp:540-546` 现为图遍历 DFS 栈（`:543` = `if (!gseen[v]) {`），不是目标函数；`:669` 区域为权重归一（`raw_w[i] = raw_w[i]/s × reliability`）。**实际目标/法方程装配在 `upm.cpp:685-692`**（注释 `// Ap = W p + λs L p + λ0 p`，`Ap[k] = obs_w*p + lambda_s*lp + anchor*p`），`lambda_s` 定义于 `:565`。
  2. `upm.cpp:645-649` 现为 raw_weight rc 检查＋sums 累加（`:649` = `sums[...] += raw_w[i]`）——归一恒等式「= control_reliability」的**除法×reliability 在 `:669`**，引用范围只覆盖运算前半程，应为 :643-670 或直接指 :669。
  3. `module_adapters.cpp:4991-5040`（`lib/infrastructure/scheduler/src/module_adapters.cpp`，注意非 pipeline/ 路径）现为 WCS roundtrip 门代码；**生产标志块实际在 :9570-9585**：`:9573` tolerance_relative=1、`:9577` gs_damping=0.5、`:9578` m_full_frame=1、`:9579` final_gauge=1、`:9582` target_order、`:9583` sigma_floor=1e-3/zero_anchor=1e-3/grid=8、`:9585` use_ivar_weight=1。
  4. **反证（内容主张为真）**：上述标志值本身与文档所列逐一相符（顺序、取值全对）——错的仅是行锚；且仓内旧快照 `run/.../modreadme_before/lib/core/src/module_adapters.cpp` 中 gs_damping 也位于 :9286，4991-5040 从来不是该块——属写入时即错/大幅漂移。
- **建议改法**：按 `UPM_SOLVER.md:26` 先例，以订正注补实锚（不改写历史正本正文）：目标 `upm.cpp:685-692（λs :565）`、归一 `upm.cpp:649/:669`、标志 `module_adapters.cpp:9570-9585`。

### 黄-2 ②④｜docs/smooth-lambda.md §3–§9 为未填充模板占位符，§0 结论速览自指占位——「定案」给不出推荐值

- **文件:行**：`:112` `{{TABLES}}`、`:118` `{{REAL}}`、`:124` `{{OBS}}`、`:130` `{{RECO}}`（§6「定案：推荐生产默认值」整节内容）、`:136` `{{ADAPT}}`、`:142` `{{SURFACE}}`、`:148` `{{LIMITS}}`；另 §0 结论速览 `:23-27` 含未填字面量「N×」「X 掉到 Y」「峰区压暗 = X%」「X% vs Y%」「台阶比 X→Y」「`upm.smoothing_lambda` = **X**（区间 [a, b]）」。
- **问题描述**：该文档是 REPORT_paper:223 列举的历史正本之一（文首已带红1/D-07/D-08 订正注 ✓），但其七个结果节全是模板 token；§0 各行「见 §3.2 / 见 §6」指向的正是占位符——结论速览的证据链与「定案」推荐值均未交付。
- **证据（含反证核验）**：`grep -rn '{{TABLES}}|{{RECO}}' code/` **为空**——无填充脚本；`code/reverse_verify/smooth_lambda/report.py:82` 只把表写到运行目录 `report_tables.md`（docstring 自述「供 docs/smooth-lambda.md 引用」），从不回写本文档。即仓内不存在任何把占位符替换为数值的机制。
- **建议改法**：二选一——(a) 由 report.py 产出后人工回填 §3–§9 与 §0 表；(b) 在文首订正注中明示「§3–§9 为模板、数值以 run/report_tables.md 为准」，避免读者把 `{{RECO}}` 当定案。涉及正本文本，改法上呈总编执行。

### 黄-3 ③④｜README E3 峰值 RSS 读数与所标注来源 results/c6_sparse_dense.json 不符（11,688/14,568 vs 11,744/14,520）

- **文件:行**：`README.md:45`（§0 结论 6）、`README.md:225`（§4.6 E3 行）
- **证据（含反证核验）**：
  1. README:219 小节标题即写「`results/c6_sparse_dense.json`，4/4 PASS」，同表 E1（3.11e-15）、E2（14,001 vs 8,389,129，0.167%）、E4（49/262,144）与 JSON **逐位相符**——唯 E3 行两数不符。
  2. `results/c6_sparse_dense.json` 实读：`upm_memory.rss_64block_kb = 11744`、`rss_full_grid_kb = 14520`；`gates.rows[E3_rss_scales_with_block].value = {blk: 11744, full: 14520}`。README 写 11,688 / 14,568（两向各差 −56/+48 kB）——为更早一次运行的读数残留。
  3. README:439 审稿整改表自述「已用最终代码重跑 c1、c2、c4、c6、c7（本轮）」「全部数字见 results/*.json」——与其 E3 行自述矛盾。
  4. 门方向结论不变（11744 < 14520 仍 PASS），故非红级；`results/REVIEW.md:149` 同引 11,688/14,568 为历史正本、不改写。
- **建议改法**：README:45/:225 同步为 11,744 / 14,520（或标注「REVIEW 期读数」）。

### 黄-4 ③④｜README §6/§10 与 results/REVIEW.md 的「3 项生产缺陷（FIX，不改代码）」登记及配套行锚已被 SCI-502 修复 commit 作废

- **文件:行**：`README.md:50-52`（「登记 3 项生产缺陷（FIX，**不改代码**）：绝对容差…／converged 只有 0/1…／kappa_max 3.16e7…」）、`README.md:308`（「代码事实（`upm.cpp:1527-1536` 只有 0/1）」）、`README.md:402`（FIX-1 锚「`module_adapters.cpp:5948-5949`（tolerance=1e-6, tolerance_relative=0…）」）、`README.md:441`（「FIX-1 行号漂移 | 已核对为 :5948-5949」）；同源陈述 `results/REVIEW.md:101/:143/:171`。
- **证据（含反证核验）**：
  1. `git log`（只读）：commit **d77fd11f「SCI-502：UPM 三个生产缺陷修复（相对容差 / 四态收敛 / 近奇异自适应）」** 同时是 `converged = 2` 与「生产默认走相对判据」注释块的引入点——三项登记缺陷**已全部修复入产**，「不改代码」的登记状态过期。
  2. 现行源码实开：`module_adapters.cpp:9568` `uc.tolerance = 1e-6`、`:9573` `uc.tolerance_relative = 1`（注释 :9570-9572 明写「生产默认走相对判据；…SCI-C C1 A7b」）——README:402 所述 `tolerance_relative=0` 的 :5948-5949 位置现为无关代码（do_apply 帧循环）；`upm.cpp` 现有 `m->converged = 3`（:1047）/`= 2`（:1061）分支，`:1901` 注释明写「0 = max_iter / 1 = converged / 2 = stalled / 3 = invalid」——README:308/REVIEW:171「只有 0/1、全仓无 2/3 分支」与行锚 `upm.cpp:1527-1536`（现为 JSON 模型装载、0/1 注释实际在 :106-113）双双失效。
  3. 交叉核对：现行 `REPORT_paper.md:164` 已把 FIX-3 当**在产机制**讨论（「FIX-3 自适应重试增大 roughness_penalty…」）——与 d77fd11f 后的源码一致，说明**当前定稿报告未继承该过期说法，冲突仅存在于冻结的 README §4–§12 与 REVIEW.md**（REPORT_paper:223 声明二者为历史正本不改写）。
  4. PASS 排除比对：`检查-修复验证.md` 行漂移 6 处（Y-3 a–f）逐条为 docs/ 侧锚（PHASE2_UPM/UPM_SOLVER/UNCERTAINTY/NOISE_MODEL/GATES），**不含本条任何锚**；黄 16 项亦无 FIX 登记相关项——非重复上报。
- **建议改法**：不改写历史正本正文；按仓内先例加一条状态订正注/指针（FIX-1/FIX-2/FIX-3 已由 d77fd11f（SCI-502）修复，实锚 :9568/:9573、upm.cpp:1047/:1061/:1901），或将登记状态移入 §11.2「仍存边界」之外的已闭合清单；具体处置上呈总编（涉及冻结定义与变更台账边界）。

---

## 三、绿级（4 项，可不改）

### 绿-1 ④｜「48.4% 负值像素」标注 [实验:c1_additive.py]，该量实际由 c3_public_plane 产出

- **文件:行**：`REPORT_paper.md:17`（摘要）、`REPORT_paper.md:50`（§2.1）、`REPORT_experiment.md:42`（§4.1 表）
- **证据**：`grep -n 'neg' code/c1_additive.py` 为空、`results/c1_additive.json` 无任何 neg 键且全文不含 0.48/48.4；`code/c3_public_plane.py:131` `full_neg_frac` → `results/c3_public_plane.json` 实测 **0.4839820861**。同句其余三数（0.845、0.275、0.383）确在 c1 ✓，299.17/297.33 在 c3（REPORT_paper:46 的双文件标注即正确）。数值本身无误、单元内有源。
- **建议改法**：引用补上 `c3_public_plane.py`（或改标 c3）。价值仅在追溯精度，不改结论。

### 绿-2 ①｜k_gauss「N=5→1.63」与正本表 1.637 不一致（旧值/截断值）

- **文件:行**：`REPORT_paper.md:70`、`docs/control-variance-adjudication.md:68`
- **证据**：正本 `PHASE2_UPM.md` §5 canonical 表 **N=5→1.637**，且其「旧对照」行明列「N=5→**1.63**」为**订正前旧值**（同轮红-7 把四处承载文档全表统一为 1.637/1.316/1.144/1.083/1.046/≈1.00）；`results/audit_rework/p3_kcorr/tables.md:45` 直接定征 1.6370、3×40000 高精度 1.6339（1.63 与后者截断相符）。差 0.4%，不影响任何门槛（N=5 ≥1.6、N≤25 >1.08 等判定两值同侧）。
- **建议改法**：顺手改为 1.637 与正本全表一致；不改亦可。

### 绿-3 ④｜多重性检出标「解析/MC 双口径」但只列解析列三元组

- **文件:行**：`REPORT_paper.md:74`、`REPORT_experiment.md:68`
- **证据**：`results/audit_rework/route1/c3_seam_gate.json` 解析列 0.64619/0.87482/0.98433 ✓（所列 0.646/0.875/0.984 即此列）；MC 列为 0.65175/0.870/0.98275，n_s=1 处差 0.006。`docs/seam-gate-floor.md` §2 表与 `audit_rework_summary.json`（`n1: [0.646, 0.652]`）均并列双值——两报告只给一组，读者按「双口径」回查 MC 会差 0.006，无结论影响。
- **建议改法**：改为「解析 0.646/0.875/0.984（MC 0.652/0.870/0.983）」或注明所列为解析值。

### 绿-4 ①｜p3_kcorr/tables.md T3 参考列 N=5=1.6620 与正本/同文件直接定征 1.6370 不同源

- **文件:行**：`results/audit_rework/p3_kcorr/tables.md:30/:39/:40/:41`（k_corr 附列）与 `:43`（参考行脚注）vs 同文件 `:45`
- **证据**：T3 脚注「k_gauss(N) iid 高斯参考：N=5:**1.662**, N=9:1.316, N=17:1.144, N=25:1.083, N=49:1.046…」——**N≥9 各档与正本表逐值一致，唯 N=5 与 `:45`「直接定征(400k) k=1.6370」差 1.5%**（大 N 档 0.977/1.040 的散布表明该列带 MC 噪声）；`k_excess` 列（紧凑 1.2333 等）由 1.6620 除得，若用 1.6370 则为 1.2521，两者均落台账「紧凑 ≈1.27±0.03」带内。正文无任何处引用 1.662。
- **建议改法**：T3 参考列 N=5 档注记口径/NMC 或改用直接定征 1.6370；不改不影响交付（正文与台账均用 1.637 系）。

---

## 四、已查无问题面

### ① 科学性（常数/公式/单位/适用域 vs D-07/D-08/A-P5 终裁与非退化判据）

**查了什么、结果如何（全部无问题）**：
- **公式面**：`Var(control)=k_corr·(π/2)·σ_bg²/N_retained`（REPORT_paper:57）与正本 §5 合同式逐字一致；两因子式 `k_corr = k_gauss(N_retained) × k_geo` 在 REPORT_paper:67/:70/:17（订正注）、control-variance-adjudication.md:68（订正注）、PHASE2_UPM §5 全域一致（红1 修复面复核，PASS 不重复计）。
- **D-07 三口径**：REPORT_paper:17/:62、REPORT_experiment:53-55、summary JSON:16-24 与台账 D-07 line 80 逐词一致（纯公式高估≈9.5% / 端到端 ≤±1.5% / 生产链亮端低估 1.3–3.2% / κ 四点 rel −0.86%~+0.19%——四点实为 `finiteN_control_variance.json` 的 rel_diff = −0.860%、−0.541%、−0.378%、+0.192%，范围精确相符）；「原句 N=5 低估 8.5% 方向词错误」订正注与台账一致。
- **D-08 面**：实测带 1.27–1.43、1.3445±0.0416（16 相位×8 seed）与 tables.md T1 一致；1.3883「带内一次实现」与台账一致；冻结 1.4 → 32%（N=5 紧凑 k_corr≈2.05±0.09 = T3 实测 2.0497±0.0864）/约 2 倍（583–600″/px）与 g6_fit、台账一致；k_geo 各档与台账 D-08 证据行逐词一致（含「几何扫描全域 1.00–5.0」与多帧 1.424/1.338/1.328——台账措辞为权威，未翻案）；引用义务三条（标定元组/N_retained 档位/几何不匹配 fail-closed）与台账一致。
- **接缝门口径**：REPORT_paper:74 线性硬界、`seam-gate-floor.md` 全文（§1 订正注、地板恒等式 1.0050%→0.01000005、§3 适用域）、PHASE2_UPM §9a/§17.3 三者一致；0.01005025 与 0.01005017 的巧合说明只出现在订正注/旧对照（合法）；漏检面（2d/w=×0.50/0.25/0.125 与 c3 JSON probe 逐位一致、反号相消 0.06% vs 1.09% 与 JSON 一致、01 D-57 1.73% 引注属实）。**唯一口径残留已入红-1**。
- **阶统计/常数独立复算**：0.286834 vs 0.314159 → 比值 1.0953、高估 9.53% ✓；中位数密度 5!/(2!2!)·Φ²(1−Φ)²·φ（N=5）正确；δ(0.95)=1.3449975 与 e5 analytic 1.3449975085 ✓、MC 0.94804(c5)/0.95069(e5)→「0.948/0.9507」✓；κ≈23.138/2.2138e10→「23 / 2.2e10 贴门」与台账 A-P5-08 ✓；5.26e13 的 ULP=2^-7=0.0078125 独立复算 ✓；h=0.0355°=127.8″÷0.989″/px≈129≈「128 px」与 c1_additive.py:43 SKY_CFG、c3_public_plane.py:28、c9 注释四处同源一致，2h≈256 px 与 A-P5-11 一致。
- **非退化判据纪律**：REPORT_paper:80（退化/非退化判据分离 0.09σ vs 36.4σ、假阳性 0/60）、:163（λs=0 恒真 PASS 不得充证据）、各负例清单（真值无效应⇒归零、VR→1、ivar=0 剔除、恒等 k≈1…）与 AGENTS §5「恒真门无证据资格」一致；报告未用恒真门充当证据。
- **D-07/D-08 以外的科学读数**：0.14830（e6 varying_ivar_max_rel_dtheta=0.1483029）、0.05931（1/Σw）、0.845/0.275/0.383/4.80→12.5×（c1）、299.17≈297.33 与 0.483982（c3）、27.37→6.31=4.33× 与 0.560→5.892e-4（c2）、6.98/0.975/36.4/0.09/0-60（c4）、0.0245/0.0651/0.3203/1.532/1.674/2.069/8.5%（c5）、3.1086e-15/14,001/8,389,129/0.167%/49=节点（c6）、1.5e-16/1.314e26/×2500（c7）——全部容差匹配反查固化 JSON 相符（49 帧误挂已入红-3；RSS 一处已入黄-3）。
- **N 域与档位义务**：N≥65 <2%、N≤25 k_gauss>1.08、N=5 紧凑 ≥1.6/分散 1.54、N_retained∈[5,289] 与 PHASE2_UPM §5、台账一致。
- **豁免清单**：quality_factor_initial=0.5 归一化规范（max|Δθ|=0.0 与 e6/exp12 一致）、min_samples=5、IRLS tolerance、σ_floor=1e-3、max_nodes 与 REPORT_paper:98 ↔ PHASE2_UPM §4/§5 两级权重表一致（A-P5-12 豁免口径）。

**抽查方法**：台账 D-07/D-08/A-P5 原文与 PHASE2_UPM §5/§9a/§17.3 原文逐句对照报告引文；所有关键读数以数值容差匹配反查 `results/*.json`（audit_rework 42 件＝route1 10＋route2 10＋route3 12＋p3_kcorr 5＋supp_507 2＋supp_control_variance 3，另历史正本 c1–c7 共 7 件＋audit_rework_summary）；比值/百分比/ULP/角度-像素换算独立复算（本报告内注明处）。

### ② 行文逻辑（断链/自相矛盾/订正注双说/UNRESOLVED 冒充结论）

**查了什么、结果如何**：
- **全文通读**：REPORT_paper 1-223、REPORT_experiment 1-139、README 摘要/§0/§4/§6/§9/§11、docs/ 四件套、refs.md——除入册项外无断链。
- **订正注双说结构**：11 处订正注（REPORT_paper:17/:70/:74、control-variance-adjudication:68、seam-gate-floor:26-37、README:151 Y4 行、refs.md 标注等）全部为「原句→更正原因→数值结论是否受影响」三段结构 ✓，无只说新不说旧、也无旧冒充新。
- **交叉引用落地**：「见 §x.y」「详见 docs/…」「[实验:file]」抽查全部可定位；唯一自指断链为 smooth-lambda §0→§3.2/§6 占位符（入黄-2）。
- **UNRESOLVED 表述**：REPORT_experiment:108「生产链被估量 y 的合同语义…待负责人裁决（P2 链，不属本单元）」、:109「support_min=0.2 出处未定」「Tikhonov 1963 题录未核不作依据」、README §11.2「不整改，如实登记」——均登记为待决/边界，未包装成结论 ✓。
- **05_正向规格 回炉纪律**：REPORT_paper:80/:142/:152、seam-gate-floor §4 均以 A-P5-03/A-P5-10 重新立基（「整段回炉，不得作修复依据」「按其真实身份…不可标定」），无处把回炉的判据01/05 当有效依据引用 ✓。
- **摘要-正文对应**：摘要四结论（:17）与 §4 各表逐条对应（除红-2/红-3 所涉数据行）。
- **入册项**：红-1（REPORT_experiment:66 与 REPORT_paper:74 同页矛盾）、黄-2（占位断链）。

**抽查方法**：顺序全文读＋交叉引用跳读＋「结论-证据-引用」三点抽查（每条主结论回溯到 JSON 行）。

### ③ 跨文档冲突（五方权威链）

**查了什么、结果如何（除入册项外一致）**：
- **记号与口径五方一致**：k_corr/k_gauss 统一记号、D-07 三口径、rel_step_max=0.1 除名（A-P5-10）、质量因子两级权重+豁免、12 项幻觉锚回炉（A-P5-03）在 L1↔docs↔实验↔08_修复包↔源码间一致；08_修复包 k_shape 残留与台账 D-08 自身措辞属已登记观察/域外，**未重复上报**。
- **数值链逐行抽查**：REPORT_experiment §4 三张表 vs 固化 JSON（上①所述全数通过）；README §4.6 四门 vs c6 JSON（3/4 逐位相符，E3 入黄-3）；REPORT_paper §4 vs README §4 同源读数一致。
- **复现入口**：REPORT_paper:11 / REPORT_experiment:117-124 所指 `code/audit_rework/run_all.sh`、`code/run_all.sh`、单项脚本全部存在；产物路径 `results/audit_rework/{route1(10件), route2(10件), route3(12件), p3_kcorr(5件), supp_507(2件), supp_control_variance(3件)}` 共 42 件 JSON 全部可定位 ✓。
- **源码接线**：文档声明的配置键 `gs_damping / zero_anchor_weight / tolerance_relative / smoothing_lambda / target_order / use_ivar_weight` 在 `module_adapters.cpp:9570-9660` 有解析与默认值、`stage2_common.cpp:152/:538` 有透传 ✓（FIX 登记状态入黄-4）。
- **入册项**：红-2（seed 六处）、红-3（49 帧两处）、红-4（summary JSON vs 报告/正本）、黄-3（README vs c6 JSON）、黄-4（README/REVIEW vs 源码 SCI-502）。

**抽查方法**：公式串/口径串/常数串/seed 串在「文档↔文档」「文档↔JSON」「文档↔源码」三向 grep 对照；与 PASS 排除清单逐条比对防重复上报。

### ④ 幻觉与锚（书目核验、file:line 实开、「文档说有代码没接」）

**查了什么、结果如何**：
- **书目抽查**：refs.md 9 条 VERIFIED 中抽 4 条经 arXiv 核验全中——Padmanabhan *An Improved Photometric Calibration of the SDSS*（astro-ph/0703454，ApJ 674,1217）✓；Gruen/Seitz/Bernstein *Implementation of robust image artifact removal in SWarp…*（arXiv:1401.4169，PASP 126,158，DOI 10.1086/675080）✓；Andrae/Schulze-Hartung/Melchior *Dos and don'ts of reduced chi-squared*（arXiv:1012.3754，式(9) 引用面属实）✓；Casertano et al. *WFPC2 Observations of the Hubble Deep Field-South*（astro-ph/0010245，AJ 120,2747）✓。其余 5 条（Huber 10.1214/aoms/1177703732、Holland&Welsch 10.1080/03610927708827533、Clopper&Pearson 10.1093/biomet/26.4.404、Fruchter&Hook 10.1086/338393、Serfling 10.1002/9780470316481）为 refs.md 已登记三重核验条目，本轮未重复联网；Serfling/Kendall 在 REPORT_paper:60 明标「书目级/二手标注」不作数值依据 ✓。
- **file:line 锚实开**：REPORT 各 [实验:file] 全部存在并含所引数（上①逐值反查）；`PHASE2_UPM §9a/§17.3`、`§4/§5` 引文相符 ✓；README:402/:441/:308 与 smooth-lambda 三锚**实开不符→入黄-1/黄-4**；REVIEW.md 锚同源入黄-4。
- **「文档说有、代码没接」反证**：(a) 无生成器的手写结果 JSON——`grep -rln 'audit_rework_summary|kcorr_adjudication_D08' --include='*.py' --include='*.sh'` 全仓为空 → 入红-4；(b) 无填充机制的模板——`grep '{{' code/` 为空、report.py 只写运行目录 → 入黄-2；(c) 其余文档声明（负例清单、门定义、seed 不可覆盖、零 git 写）逐条对脚本核实为真 ✓。
- **反向排查（PASS 合法清单比对）**：k_shape/1.26/旧口径关键词在本单元的全部命中要么是订正注/旧对照（合法），要么即本报告红-4 所指 JSON 块（不在合法清单）——无重复上报。

**抽查方法**：arxiv_search 4 次核题名/作者/卷页；每个行锚用 sed 实开原文比对；生成器/接线用双向 grep 反证。

---

## 五、计数

- **红：4**（红-1 接缝下限口径残留；红-2 seed 声明不实×6 处；红-3 C7 样本量 49→4；红-4 summary JSON 记号两读）
- **黄：4**（黄-1 smooth-lambda 三行锚失效；黄-2 smooth-lambda 模板占位未填；黄-3 README E3 RSS 与 JSON 不符；黄-4 README/REVIEW FIX 登记与行锚被 SCI-502 作废）
- **绿：4**（绿-1 48.4% 引 c1 实为 c3；绿-2 k_gauss N=5→1.63 vs 正本 1.637；绿-3 双口径只列解析三元组；绿-4 T3 参考列 N=5=1.6620 vs 直接定征 1.6370）
- **合计 12 条**；域外登记 0 条（台账 D-08 自身措辞、08_修复包 k_shape、REVIEW 历史读数均属已登记观察/历史正本，按纪律不再上报）。
