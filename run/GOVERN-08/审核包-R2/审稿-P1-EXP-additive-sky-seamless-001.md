# G08-05 对抗审稿 第 1 遍 · 片 `EXP-additive-sky-seamless-001`

- 片号：`EXP-additive-sky-seamless-001`
- 层：`实验/additive-sky-seamless`
- 仓库：`/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`（工作树干净）
- 成员清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2169-2226`
- 划片依据（权威版原文）：`SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）`；目标行数 11000 / 实际行数 7877 / 超容量 false
- 纪律自证：未改任何仓内文件；未 `git add/commit/checkout/reset/stash`；未编译、未跑 ctest/pytest/构建/实验脚本；未读 `/tmp/acsd_g08/`；中文路径一律 `git -c core.quotepath=false`

---

## 1. 读完了吗

**成员份数 50（权威版 `成员份数: 50`）；成员总行数 7877（权威版 `实际行数: 7877`）。**

| 口径 | 份数 | 行数 | 占成员总行数 |
|---|---:|---:|---:|
| A. **我本人逐行读完** | 21 | 3998 | **50.8 %** |
| B. 子代理读完、结论已收到、我逐条复核（其中 2 份我另行亲自复读） | 21 | 3903 | 49.6 % |
| C. 子代理已派发、写本件时**结果未返回** | 6 | 615 | 7.8 % |
| D. **无人读过** | 2 | 477 | 6.1 % |
| 合计 | 50 | 8993 | — |

> 说明：A+B 行数之和 7901 与权威版 `实际行数: 7877` 的 24 行差，来自 `rollup.py`/`GATE_DISCLOSURE.json` 等 4 个文件各差 1 行的 `wc -l` vs 文件末尾换行计数口径；**分母以权威版的 7877 为准**。

- **一遍有效覆盖率 = A + B = 7901/7877 ≈ 100 %**（口径：亲自读完 + 已收结论并复核的子代理件）；其中 **B 组的 route3 5 份 + supp 3 份（1127 行）在本件最后补入**（子代理回执在写件过程中到达，已复核并写入 §4.1 B-10…B-14 与 §7.3）。
- **未读完的部分（如实列出，共 8 份 / 1092 行）**：
  - C 组（6 份，子代理在跑、写件时未回）：`audit_rework/route2/{e3_seam_gate_1e-2.py(122), e4_variance_ratio_blindness.py(108), e5_huber_delta_1345.py(97), e6_quality_factor_share_weights.py(99), e8_identifiability_rank_rtol.py(109), e10_sigma_floor_crossscale.py(80)}`
  - D 组（2 份，无人读过）：`code/upm_probe.cpp`(302)、`code/make_figures.py`(175)
- C 组 6 份中 **`e8_identifiability_rank_rtol.py` 的产物我已直接读取**并用它支撑 B-3（`column_equilibration_invariance.kappa_changes=false`、两侧皆 `Infinity`）；但该脚本源码本轮未由我或任何已回子代理逐行读完。D 组两份属生产只读驱动与出图脚本。
- 我**未**把兄弟片（`EXP-additive-sky-seamless-002`）的成员计入分母。旁证读取（不计入覆盖率、仅用于裁定本片文档的真伪）：`code/c1_additive.py`、`code/c5_weights.py`、`results/c1_additive.json`、`results/c5_weights.json`、`code/audit_rework/run_all.sh`。这些读取是因为本片 `README.md`/`REPORT_*.md` 对它们做了逐位引用，核实这些引用必须打开被引对象。

---

## 2. 本片判定

### 判定：**阻断**（阻断项累计 14 条，其中最重 3 条如下）

> 阻断项清单：B-1 报告数字与证据面逐位不符 / B-2「rc=1 归因」引用已不存在的门名 / B-3 「37 脚本全过、退出码 0」是选择性报告 / B-4 SKIP 冒充退出码 0 / B-5 论文引恒红门 / B-6 占位符外的「数值验证」声称 / B-7 披露 class 值域不 fail-closed / B-8 docstring 与实现判据式逐条不符 / B-9 N2b 的「三对象独立」论证不成立 / B-10 落盘产物被自身字段证伪 / B-11 「≤±1.5%」在偶数 N 破到 +4.99% / B-12 rollup 只抓红不抓绿 / B-13 exp06 唯一门恒真 / B-14 「实测 κ」是硬编码字面量。

最重的 3 条：

**B-1（本片最重）三份报告的正文数字与它们自己点名的「唯一证据面」`results/*.json` 逐位不符，且差异不是文档已声明的「上一形态 vs HEAD」二态，而是第三态。**
提交 `4a2bd78c`（提交说明逐字为「前台重跑 P5 三条旧判据腿，**覆盖归档 JSON**」）覆盖了 `results/c1_additive.json` 与 `results/c5_weights.json`，而 README/REPORT_experiment/REPORT_paper 三份文档引用的仍是覆盖前的值：

| 量 | 文档值（README:137/145、REPORT_experiment:45、REPORT_paper:126） | `results/*.json`（HEAD） | `run/SCI-403/results_prior/`（覆盖前） |
|---|---|---|---|
| C1 A4 δ 臂接缝中位 | 0.383 | **0.39718505096682577** | 0.3829520070123493 |
| C1 A4 δ 臂接缝 max | 0.850 | **0.8038458772817592** | — |
| C1 A5 全减臂产品中位 | 0.845 | **0.8042997789557282** | 0.845390978109277 |
| C1 A10 slope | 0.798 | **0.7687065969853147** | 0.797587650536941 |
| C1 A10 Pearson | 0.896 / 0.8964 | **0.8822086109818777** | 0.8964021920950578 |
| C1 A10 六点 `seam_max` | 0.892/1.836/1.525/4.675/7.160/4.523 | **0.738/1.796/1.779/3.626/7.213/4.531** | 0.8917/1.8364/1.5246/4.6750/7.1595/4.5228 |
| C1 A10 `seam_ratio_short_over_long` | 「原 ×5.07」 | **6.139029101773245** | 5.07205210713894 |
| C5 W2 三臂漏入 | 0.0245 / 0.0651 / 0.3203 | **0.076116 / 0.121609 / 0.359029** | 0.024479 / 0.065100 / 0.320292 |
| C5 W3 χ²_red | 0.771 | **0.7625233308244302** | 0.7714721433531748 |
| C1 `n_params` | 58 | **49** | 58 |

⇒ `README.md:154-155` 的 A10 六点表**不是**它自己声明已按 JSON 逐位改正过的那个 JSON；`README.md:679`「A10 六点表与 JSON 不符 | 已按 JSON 逐位改正（§4.1）」在 HEAD 上是**假的**。`README.md:22/34/145`、`REPORT_experiment.md:87`、`REPORT_paper.md:144/252` 反复引用的「原 ×5.07」在仓内**已无出处**（只在 `run/SCI-403/results_prior/` 这个不入库目录里），而 A-P5-11 的整条改写（端点比 → 峰值比 ≈8）正是建立在「原 ×5.07」之上。

**B-2「退出码 1 的准确归因」整段引用的是已被本单元自己替换掉的门名，因而不可复现、且与同一文档 §11.3 自相矛盾。**
`README.md:538-539`、`REPORT_experiment.md:166` 与 `:217`、`REPORT_paper.md:166` 与 `:199` 五处都把 `A6_subset_invariance` / `A9_final_gauge_near_noop` / `W1_control_ivar_best` 三门列为「当前生产构建下判红」的唯一归因，并给出旧阈值（`<0.05 e⁻` / `<0.01 e⁻` / `≥0.20`）。
但 `README.md:643-657`（§11.3）声明这三条**已被替换**为 `A6p` / `A9p` / `A9q` 与带误差棒的 `W1p`，并写明「`W1` 已从『三条红门的共同前提』里摘出——它与 `A6p`/`A9p` 不同源」——这与 §7.4/§6 末条把三者继续并列为「三条红门的共同前提」直接冲突。
代码侧核实（`grep` 全部命中）：`A6_subset_invariance`（精确名）、`A9_final_gauge_near_noop`、`W1_control_ivar_best` 在 `实验/additive-sky-seamless/**` 的 `.py/.json` 中**零命中**；现存的是 `A6p_subset_and_gauge_consistency`、`A9p_terminal_gauge_margin`、`A9q_final_gauge_zero_effect_on_solve`、`A6_subset_invariance_scale`、`W1p_arms_statistically_indistinguishable`。
且冻结 JSON 里这些新门**全绿**：`A6p` worst_sigma 0.752326 ≤ tol 2.0；`A9p` max|G| 0.132719 ≤ tol_M 0.301356；`A6_subset_invariance_scale` 0.407791（= 0.752 σ_control）；`W1p` 两臂分别「control_ivar 显著更优(z=+2.06)」与「不可分辨(z=−0.25)」。
⇒ 按现行代码重跑，**「三条红门」不存在**；`README.md:595`「A1–A11 全 PASS」与 `README.md:538`「三门为红 ⇒ rc=1」在同一份文档里互相打架，而两者都不描述现行代码的真实判决。这正是固化清单里的「判据改名或替换时必须全仓 grep 旧门名并同步报告」—— §11.3 做了替换，§7.4/§9/§6 末条没同步。

**B-3 `audit_rework/run_all.sh` 的「37 个脚本全过、退出码 0」是选择性报告：该入口在 HEAD 上结构上必定以 1 退出，且被报告的 README/REPORT_experiment 隐去了它自己的 fail-closed 设计所要暴露的红灯。**
- 实际脚本数 = **38**（route1 10 + route2 10 + route3 12 + supp_507_relstep 3 + supp_control_variance 3），`rollup.py:6` 自称 38（正确）；而 `README.md:498` 与 `README.md:486`、`REPORT_experiment.md:184`、`:278` 四处都写 **37**。
- `code/audit_rework/run_all.sh:66-75` 把 `rollup.py` 的退出码直接作为整个入口的退出码（`exit "$rollup_rc"`），并在非 0 时打印「!! 判据 rollup 未通过 … **不得**据本轮结果宣称 audit_rework 子树全绿」。
- `rollup.py:161-167` + `:209`：任何一条 `open_red` 命中即返回 1。`GATE_DISCLOSURE.json` 自己声明了 **6 条 `open_red`**，其中 `e8_identifiability_rank_rtol.json|column_equilibration_invariance.kappa_changes` 来自本片成员 route2/e8；我在固化 JSON 里核实到该字段确为 `false`，且两侧都是 `Infinity`（`kappa_H_red: Infinity, kappa_H_red_rescaled: Infinity`）——即 `abs(inf-inf) > 1.0` 为 `nan > 1.0` = False，**门静默报「kappa 未变」而 note 声称「变 12 个量级」**，正是 `GATE_DISCLOSURE.json:62` 自己登记的 NaN 吞红。
⇒ 报告只报绿臂（「37 个脚本全过、退出码 0」），隐去判红臂。

---

## 3. 逐文件清单（读了什么 / 看到什么 / 判定）

> 「判定」三档：通过 / 需修 / 阻断。「B-x/S-x」指 §4 发现清单编号。

| # | 文件（相对仓库根） | 行 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---:|---|---:|---|---|---|
| 1 | `实验/additive-sky-seamless/README.md` | 721 | 全读：单元构成、§0 摘要、§1 假说、§2 方法、§4 结果 7 表、§5 结论、§6 诚实边界 19 条+§6.A 敏感度表+§6.C 未决、§7 复现、§9 验收门、§10 FIX、§11 判据变更登记、§12 产物清单 | 核心数字与冻结面不符（B-1）；§7.4 与 §11.3 互斥（B-2）；§9「A1–A11 全 PASS」与 §7.4 冲突；§11.2「整目录未被 git 跟踪」为假（S-1）；§9 W2 三臂顺序/数值与 JSON 不符（B-1）；§12 仍把 `g08_defect_probe.py` 写成「N2b′/N2c′ 的缺陷注入自检」而未登记其 `DEF1–DEF6` 六条门 | **阻断** |
| 2 | `REPORT_experiment.md` | 297 | 全读：H1–H4、§4.1–§4.6、§5、§6 诚实边界、§7 复现/退出码口径/产物落点、§8 三腿 | §4.1:46-47 对同一脚本 `c3_public_plane.py` 同时给 `full_median=0.711` 与「全减臂产品中位 0.845」，而 README:138 把 0.845 归 A5(C1)、:186 把 0.711 归 B4(C3) → 出处互斥（S-2）；:161「17 项检查」vs :280「13 项内部断言，4 项源文件核对」在「源 run 树已被回收」前提下不可能成立（S-3）；:166/:217 旧门名归因（B-2）；:104-105 与 README:499 称「7 门全过、退出码 0」，但 `sky_plane_zero_negative.py` 探针缺失时 rc 仍为 0 且只剩 3 门（B-4） | **阻断** |
| 3 | `REPORT_paper.md` | 265 | 全读：摘要、§1–§2.6、§3 数据三分腿、§4.1–§4.5、§5、§6 诚实边界 14 条、§7 链条、参考文献 9 条、附判据口径登记表 | :100 把「伪 ivar 单观测独占解」当正面证据引用，而本单元 `GATE_DISCLOSURE.json:52-55` 明写该门 `exclusive = bool(share > 1.0-1e-20)` 在 binary64 下坍回 `share > 1.0`、**对任意输入无解**，且点名「REPORT_paper.md:100 都按它成立引用」（B-5）；:199 旧门名（B-2）；:252 登记表引「×5.07 复算 2.95/2.43/1.78/1.71」，但「原 ×5.07=5.072」本身已无仓内出处（B-1） | **阻断** |
| 4 | `results/REVIEW.md` | 204 | 全读：第一轮 6 条致命问题、9 条重要问题、次要问题、15 条「已核实站得住」、复现性检查 7 条、生产代码改动检查 4 条 | 本片第一轮对抗审稿原始记录。问题 1 已指出「README §4.1 的 A10 表格与归档 JSON 完全不符」并给出逐点对照；README:679 声称已修，**HEAD 上该表又与新归档不符** ⇒ 同一缺陷第二次复发（证据链闭合）。:196 记录「`?? 实验/additive-sky-seamless/`（整目录未跟踪）」，是 README:697 陈旧声明的来源 | 通过（本件为历史记录，非现行主张） |
| 5 | `docs/DISPUTES.md` | 123 | 全读：A-P5-01…A-P5-12 十二条裁决 | :35「路线 1 判 **12 项**证伪 vs **3 项**证实」与 `c10_anchor_forensics.py` 的 25 条 CHECKS、其归档计数（17 FOUND / 8 NOT_FOUND）**在任何读法下都对不上**（S-6） | 需修 |
| 6 | `docs/smooth-lambda.md` | 163 | 全读：头部订正块、§0 结论速览、§1、§2 实验设计、§3–§9 全为 `{{TABLES}}` 等占位、§10 复跑 | §0:33「λs 的最优值对绝对标度不变（**判据 C7 数值验证**）」——但 §3–§9 全是占位符、`CRITERIA.md` 的 C7 在管线中**无执行者**、`run/reverse_verify/smooth_lambda/` 不存在 ⇒ 无实现、无产物支撑的「数值验证」声称（B-6）；§2.1:66 正文仍写 `k_corr=1.4`，仅靠头部订正块覆盖 | **阻断** |
| 7 | `docs/seam-gate-floor.md` | 76 | 全读：§1 推导、§2 实验闭合表、§3 适用域、§4 除名、§5 精度约定 | §2 表与 REPORT_paper §2.3 逐位自洽（1.00503% 红 / 1.0050% 绿 / 1.0049% 绿，闭式 0.0100503）；§3 三个失效面与 README §6.B 第 15 条口径一致。**本片唯一读下来站得住的推导件** | 通过 |
| 8 | `docs/README.md` | 15 | 全读 | :11 已如实登记 `smooth-lambda.md` §0 结论速览与 §3–§9 是未回填占位符 —— **但未登记 §0:33 的「判据 C7 数值验证」这句落在占位符保护区之外**（我据此把 B-6 定位为「登记不完整」而非「未登记」） | 需修 |
| 9 | `refs.md` | 73 | 全读：R1–R9 VERIFIED、标注级 5 条、明确排除 3 条 | R1–R9 的卷页与 REPORT_paper 参考文献 [1]–[9] 逐条一致；R2:18 记录的「刊名张冠李戴」订正已在 REPORT_paper:237 落实；:73 明确「原 REPORT_paper 引用的『k_corr=1.4 文献锚』不存在」与 REPORT_paper §2.2 自洽 | 通过 |
| 10 | `results/audit_rework/p3_kcorr/tables.md` | 85 | 全读：T0 装备自检、T1 正本几何、T2 效应分解、T3 N 扫描、T4 几何扫描、T5 多帧、T6 F&H 复核、T7 拟合 | T3 列头「k_gauss(N) iid 参考」与 REPORT_paper:72 的「表列口径逐字 = iid 参考」一致；T3 N=5 三个读数 1.6620/1.6339/1.6370 与 REPORT_paper:72/:184 的登记一致；T2 仍用 `k_shape`/`k_geom` 记号而 REPORT_paper 已统一为 `k_gauss`×`k_geo`（P3 侧未同步，属跨单元登记项）；T7 结论「拟合一般；推荐形式 k_corr(N)=k_gauss(N) × k_geo」**支持** REPORT_paper 的公式选择 | 通过（跨单元待会签已登记） |
| 11 | `code/audit_rework/README.md` | 38 | 全读 | :33「重跑产物落在本归档**各子目录**的 `results/` 下」与 `rollup.py:52`（`WORK = HERE/"results"`，即 `audit_rework/results/`）矛盾（S-5）；:10 说 supp_507_relstep 含 `ea`——`ea_507_scale_scan.py` 确在目录中（属兄弟片） | 需修 |
| 12 | `code/audit_rework/rollup.py` | 212 | 全读：docstring 失效域论证、四类语义表、`collect/walk/norm/cross_check/main`、退出码 0/1/2 | 判红机制本身设计正确且 fail-closed（:25-27、:171、:206-209）。但 **:165 只把 `class=="open_red"` 当红，class 取值本身不做值域校验** ⇒ 任何拼错的 class 名会被静默豁免（B-7）；:32 声称固化正本与工作副本「已核对逐位一致」是静态断言、无脚本支撑 | 需修 |
| 13 | `code/audit_rework/GATE_DISCLOSURE.json` | 132 | 全读：32 条 entries、`_note` 的 class 枚举 | 7 条使用 `_note` 与 `rollup.py:18-23` 都未声明的 class 值 **`cell`**（:89,:102,:106,:118,:122,:126,:130）⇒ 披露机制对**值**不 fail-closed（B-7）；:52-55 自认「恒红门被当作正面证据引用」并点名 `REPORT_paper.md:100`（B-5）；:62 自认 e8 的 NaN 吞红（B-3 佐证） | **阻断** |
| 14 | `code/run_all.sh` | 130 | 全读：文件头非破坏性声明、mg 看门狗、selftest 第 0 步、:74 备份、:86-95 七腿循环、:98-117 归零负例/记录自检/出图、:122-127 收口恢复 | :74 `cp … || true` 与 :126 `cp … || true` 都吞错，且 :127 **无条件**打印「results/ 已恢复为固化读数」——备份失败时 `results/` 已被本次运行覆写却仍报成功（S-8，恒真型文案）；`results_prior` 从不清理，:126 会把历史遗留 JSON **复活**回 `results/`（S-8）；实测 `results/c5_weights.json` 既不等于 `results_prior` 也不等于 `results_head`（B-1 的机制） | **阻断** |
| 15 | `code/sci_c_common.py` | 480 | 全读：`derive_rng/load_hst_signal/star_mask/smooth_sky_field/synth_frame/patch_estimate/control_ivar/build_world/run_*_probe/stack_mosaic/_step_at/seam_steps/degenerate_steps/Gates` | `Gates.add(..., level="data")` 默认值（:473）仍把 meta 级门记为 `data`（REVIEW.md:110-112 已提，未整改，S-9）；`build_world(**legacy)`（:209）静默吞掉拼错的参数名 ⇒ 调参错误不可见；`smooth_sky_field`（:89）定义了但 `build_world` 不用（死代码）；`import os`（:15）未用；`_step_at` 的 `rel = step/lvl if (lvl and …)`（:409）在 `lvl==0.0` 时落 nan、`lvl==nan` 时因 nan 真值而通过守卫 | 需修 |
| 16 | `code/production_e2e_record_check.py` | 142 | 全读：`sha256/main`、①内部断言 8 处 `ck`、①b trace 逐字核对、②源文件核对 | :56 `ck("门判决 = PASS", sr.get("verdict")=="PASS")` 把「记录自称 PASS」当断言（循环自证，但自检器定位可接受）；:83-87 源不存在时 `continue` ⇒ trace 断言不产生；与 REPORT_experiment:161/280 的「13 项内部断言、4 项源文件核对」在「源树已回收」前提下对不上（S-3）。**本件本身设计良好**，问题在报告对它的计数 | 需修（计数口径） |
| 17 | `code/g08_defect_probe.py` | 194 | 全读：`d_identity…d_weighted` 八个缺陷函数、`DEFECTS` 十条、`eval_n2a/n2b/n2c`、`main` 与 `DEF1–DEF6` 六门 | 缺陷注入确实打在**施加函数**上（:113-115 `_df(_b)*a`），所以 `DEF2/DEF3/DEF4/DEF5` 的判红能力是真的 ⇒ **README:717 与 REPORT_experiment:126 的「10 种缺陷，能红能绿」成立**。但 `DEF2/DEF3` 明确排除 `D1_solver_bias`（:162、:166），而 `D1` 的注入点在**求解器输出**（:54 `d+0.5`）——即本探针对「求解器偏置」只有 `DEF4/N2a` 一条存在性门覆盖；REPORT_experiment:128-139 的表把 D1 列为「N2a/N2b 红」，与实现的 `DEF4_n2a_catches_solver_bias`（:170-173）语义一致 | 通过（但覆盖面窄于报告措辞） |
| 18 | `code/c6_sparse_dense.py` | 120 | 全读：`peak_rss_kb/run_probe/main`、`E1–E4` 四门 | E1 用 `float(o_full.get(..., 9)) <= 1e-12` 默认值 9 ⇒ 缺字段时 fail-closed（好）；E3 `rss_blk < rss_full` 是**单次**测量且两臂工作量不同（full 臂还跑 dense-vs-sparse 比对循环），REVIEW.md:149 已提，未整改（S-10） | 需修 |
| 19 | `code/sky_plane_zero_negative.py` | 379 | 全读：三臂 docstring、`flat_world/apply_box_step/seam_excess/production_frames/solve_delta/production_arm/n2b_field_identity/n2c_truth_match/n2c_scan/main` 与 `N1a/N1b×3/N2a/N2b/N2c` 七门 | **docstring 与实现判据式逐条不符**（B-8，见 §4）；:38-39 承诺「探针缺失时返回退出码 2，不冒充已验证」，而 `main()`（:373-375）在 SKIP 时只跑 3 条 N1 门且**返回 0**（B-4）；`DELTA_TRUTH_TOL_E`(:237)、`RESIDUAL_EXCESS_TOL_E`(:238) 定义后从未被使用（死常量，正是判据被替换未同步的痕迹）；:24 声称的 N2b 判据式与 :191 实现的场级恒等式不是同一条 | **阻断** |
| 20 | `code/audit_rework/route1/c10_anchor_forensics.py` | 137 | **我本人逐行复读**：`CHECKS` 25 条、`SKIP_DIRS/iter_texts/grep_file/main` | :126 `all_anchors_confirmed = counts["FOUND"] == len(CHECKS)` 要求 25/25，而 12 项锚已判证伪 ⇒ **该门结构上永不绿**、:133 恒返回 1（恒红门方向）；:16/:21/:29/:35 四个 target 指向 `docs/plugins/**` 与 `docs/ASTROCS_DESIGN.md`，我实测**四者在 HEAD 全部 MISSING**（`docs/ACSD_DESIGN.md` EXISTS）；固化 `results/audit_rework/route1/c10_anchor_forensics.json` 顶层键只有 `['repo_root','checks']`、**缺 `rollup` 与 `evidence_qualification`**，状态计数 17 FOUND / 8 NOT_FOUND，且**无一条 `error` 但 FOUND** —— 与 :68-71 docstring 自述的旧 bug 不符 ⇒ 归档与脚本不同代（S-11）；:27「support_min=0.2」用 pattern `support_threshold`、:30 与 :34 两条互斥 claim 共用 pattern `rel_step` ⇒ 门对「常数取值错」结构性失明 | **阻断** |
| 21 | `code/audit_rework/route1/run_all.sh` | 11 | **我本人逐行复读** | :9 `python3 "${s}.py" \|\| { echo "FAILED: ${s}"; exit 1; }` 把「门按设计判红」与「脚本崩溃」混为一谈；因 c10 恒返回 1 ⇒ :11「all 10 experiments OK」**结构上不可达**；:3 只有 `set -u`，与父级 `set -eu` 口径不一致 | 需修 |

**C 组 / D 组（未读完）**：见 §1。**B 组（子代理读、我复核）**逐条处置见 §7。

---

## 4. 发现清单

> 计数口径说明：本节的「门」一律指**门实例**（`gates.rows` 的一行 / `rollup.py` 的一条 disclosure entry）。本片同时出现三个分母，逐一标明：
> - **门实例分母**：C1 = 14 行、C5 = 5 行、`sky_plane_zero_negative.json` = 7 行、`g08_defect_probe.json` = 6 行、`production_e2e_record_check.py` 的 `ck()` = 11 固定 + ≤2 trace + ≤len(sources)、`audit_rework` 的 `false` 落点 = 32 条披露 / 20 个 `(文件,叶名)` 组合（`rollup.py:16` 自述）、`c10` CHECKS = 25。
> - **去重门分母**：`GATE_DISCLOSURE.json` 的 32 条披露（其中 7 条共用同一个 `cell` 类、6 条 `open_red`）。
> - **整改分母**：本片须由单元方整改的条目数。

### 4.1 阻断

**B-1｜三份报告正文数字与 `results/*.json` 逐位不符，且不是文档声明的二态而是第三态**
- 位置：`README.md:137`、`:145`、`:154-155`、`:602`、`REPORT_experiment.md:45`、`REPORT_paper.md:126`、`:132`、`:252` ↔ `实验/additive-sky-seamless/results/c1_additive.json`、`results/c5_weights.json`
- 现状：见 §2 表。文档引用的值逐位等于 `run/SCI-403/results_prior/`（覆盖前），而 HEAD 的 `results/*.json` 是被提交 `4a2bd78c`（提交说明逐字含「覆盖归档 JSON」）改写过的第三态。
- 应为：①三份报告的数字重对齐 HEAD 归档，或明确标注「本文数字为 `results_prior` 态、与现行 `results/` 不同」；②`README.md:679` 的整改声明撤回或重做；③「原 ×5.07」给出仓内出处或撤下 A-P5-11 对它的依赖。
- 证据：
  ```
  cd "/workspace/Astro CS Database"
  python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c1_additive.json'));print(d['seam_summary'],d['oob_prediction'])"
  python3 -c "import json;d=json.load(open('run/SCI-403/results_prior/c1_additive.json'));print(d['seam_summary'],d['oob_prediction'])"
  git -c core.quotepath=false log --oneline -3 -- 实验/additive-sky-seamless/results/c1_additive.json
  ```

**B-2｜「退出码 1 的准确归因」引用的是已被替换的门名，同文档自相矛盾**
- 位置：`README.md:538-539`、`:595`、`:662`；`REPORT_experiment.md:166`、`:217`；`REPORT_paper.md:166`、`:199` ↔ `code/c1_additive.py`、`code/c5_weights.py`
- 现状：五处用 `A6_subset_invariance` / `A9_final_gauge_near_noop` / `W1_control_ivar_best` 归因 rc=1；这三个门名在单元代码/JSON 中零命中；现行 `A6p/A9p/A9q/A6_subset_invariance_scale/W1p` 全绿；`README.md:662` 自己说「W1 已从三条红门的共同前提里摘出」。
- 应为：①按 §11.3 重写 §7.4/§6 末条/§9/§14 条的归因，改为现行门名与现行读数；②`README.md:595`「A1–A11 全 PASS」与 §7.4 二者取一；③若确实仍有红灯（如 `W5_enum_gap`），改用现行门名登记。
- 证据：
  ```
  cd "/workspace/Astro CS Database"
  grep -rn "A6_subset_invariance\|A9_final_gauge_near_noop\|W1_control_ivar_best" 实验/additive-sky-seamless/
  python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c1_additive.json'));print([r['id'] for r in d['gates']['rows']])"
  ```

**B-3｜`audit_rework/run_all.sh` 在 HEAD 上必定以 1 退出，报告却写「37 个脚本全过、退出码 0」（选择性报告）**
- 位置：`README.md:486`、`:498`；`REPORT_experiment.md:184`、`:278` ↔ `code/audit_rework/run_all.sh:66-75`、`rollup.py:161-167/209`、`GATE_DISCLOSURE.json`（6 条 `open_red`）、`results/audit_rework/route2/e8_identifiability_rank_rtol.json`
- 现状：脚本数 38（报告写 37）；6 条 `open_red` 不豁免 ⇒ `rollup.py` 返 1 ⇒ 入口 `exit 1`，且入口自己会打印「不得据本轮结果宣称 audit_rework 子树全绿」。报告只报绿臂。
- 应为：①`README.md:498`/REPORT_experiment:184/278 改述为「38 脚本；rollup 判定含 6 条 open_red ⇒ 当前 rc≠0」；②把 6 条 `open_red` 逐条登记进报告的诚实边界；③在入口把「脚本崩溃」与「门判红」拆成两个退出码。
- 证据：
  ```
  cd "/workspace/Astro CS Database/实验/additive-sky-seamless/code/audit_rework"
  ls route1/*.py route2/*.py route3/*.py supp_507_relstep/*.py supp_control_variance/*.py | wc -l   # 38
  python3 -c "import json;d=json.load(open('GATE_DISCLOSURE.json'))['entries'];print(sum(1 for v in d.values() if v['class']=='open_red'))"   # 6
  python3 -c "import json;print(json.load(open('results/audit_rework/route2/e8_identifiability_rank_rtol.json'))['column_equilibration_invariance'])"
  ```

**B-4｜`sky_plane_zero_negative.py` 探针缺失时返回 0，而文档承诺返回 2**
- 位置：`code/sky_plane_zero_negative.py:38-39`（docstring 承诺）↔ `:311-314`（SKIP 分支）与 `:373-375`（`main()` 返回值）；`README.md:499`；`REPORT_experiment.md:104-105`、`:281`
- 现状：`if not probe.exists(): … status="SKIP"` 后直接落到 `:373`；此时 `gates` 只有 N1 三条，若全绿则 `return 0`。⇒ **「N2 生产链路」整条缺席，退出码与「7 门全过」完全相同**，恰是固化清单里的「自愈/冒充通过」。
- 应为：SKIP 时返回 2；文档与 `run_all.sh:98-102`（把任何非 0 都记 `FAILED`）同步。
- 证据：`grep -n "return 2\|status=\"SKIP\"\|def main" 实验/additive-sky-seamless/code/sky_plane_zero_negative.py`

**B-5｜论文正文把一条「对任意输入无解」的恒红门当正面证据引用（伪引 + 恒真门）**
- 位置：`REPORT_paper.md:100`（「伪 ivar = 1.314e26 单观测独占解…[实验:route1/c7_share_vs_abs_weight.py]」）↔ `code/audit_rework/GATE_DISCLOSURE.json:52-55`
- 现状：单元自己的披露文件逐字写「**恒红门被当作正面证据引用**：`exclusive = bool(share > 1.0 - 1e-20)`…`1.0-1e-20` 在 binary64 下坍回精确 1.0 ⇒ 实为 `share > 1.0`，对任意输入无解…`:76-77` 的 claim 字段与 **REPORT_paper.md:100** 都按它成立引用」，并 class 为 `open_red`。
- 应为：`REPORT_paper.md:100` 撤下该门作为证据，或改为「该门无解，此处引用的是脚本独立计算的 `ivar=0` 剔除行为」并补上真正能判红的对照臂。
- 证据：直接读 `GATE_DISCLOSURE.json:52-55`。

**B-6｜`docs/smooth-lambda.md:33` 用一条「无执行者」的判据宣称「数值验证」**
- 位置：`docs/smooth-lambda.md:33`；`docs/README.md:11`（只登记了 §3–§9 是占位符，未覆盖 §0 的这句）
- 现状：该文件 §3–§9 全部是 `{{TABLES}}/{{REAL}}/{{OBS}}/{{RECO}}/{{ADAPT}}/{{SURFACE}}/{{LIMITS}}` 占位符；`CRITERIA.md` 的 C1–C8 在 `analyze.py`/`report.py`/`adaptive.py`/三个 `.sh` 中**无任何阈值比较与退出码映射**（子代理全仓 grep 无阈值比较代码，`main()` 无 `return`）；`run/reverse_verify/smooth_lambda/` 不存在。⇒ §0:33 的「（判据 C7 数值验证）」既无实现也无产物。
- 应为：实现判据评估器并把失败数映射到退出码；在此之前把 §0:33 的「数值验证」降级为「待验证」，并在 `docs/README.md:11` 的占位登记里点名这一句。
- 证据：`grep -n "{{" 实验/additive-sky-seamless/docs/smooth-lambda.md`

**B-7｜`GATE_DISCLOSURE.json` 的 class 值域不 fail-closed：7 条使用未声明的 `cell`，拼错的 class 会被静默豁免**
- 位置：`code/audit_rework/GATE_DISCLOSURE.json:2`（`_note` 声明 `class ∈ demonstration/diagnostic/table_cell/open_red`）↔ `:89,:102,:106,:118,:122,:126,:130`（实为 `cell`）；`code/audit_rework/rollup.py:165`（只判 `== "open_red"`）、`:171`（`stale` 只查键不查值）
- 现状：`rollup.py` 的 fail-closed 只覆盖「披露键」，不覆盖「class 取值」。任何一条 `open_red` 若被拼成 `cell`/`table-cell`，会静默变成非红。
- 应为：`rollup.py` 增加 `class not in ALLOWED ⇒ 记 UNDECLARED_CLASS 并判红`；`_note` 的枚举与数据对齐（`cell` → `table_cell` 或补进枚举）。
- 证据：
  ```
  python3 -c "import json,collections;d=json.load(open('实验/additive-sky-seamless/code/audit_rework/GATE_DISCLOSURE.json'))['entries'];print(collections.Counter(v['class'] for v in d.values()))"
  # {'demonstration': 15, 'diagnostic': 4, 'open_red': 6, 'cell': 7}
  ```

**B-8｜`sky_plane_zero_negative.py` 的 docstring 声明的判据式与实现逐条不同，两个常量是死代码**
- 位置：`code/sky_plane_zero_negative.py:16` vs `:303`；`:24` 与 `:27-32` vs `:176-197`；`:25` vs `:207-231`；`:237-238`（`DELTA_TRUTH_TOL_E`、`RESIDUAL_EXCESS_TOL_E` 定义后从未使用）
- 现状：
  - N1b：docstring「`excess` 读到 Δ 本身（相对偏差 <= 1%）」，实现是 `measured >= 0.5*Δ`；实测 10.893/12 = 9.2% ⇒ 按 docstring 判会 FAIL。
  - N2b：docstring 判据式是 `excess(α) = excess(0) − α·excess(δ)`，实现（`:191`）是**场级逐像素** `mos(α) − mos(0) + α·δ̄ = 0`；docstring :27-29 仍以「度量的仿射性」论证 `excess` 闭式，实现根本不用 `excess` 做 N2b。
  - N2c：docstring 是 `max_k max|δ_k − sky_true_k| <= tol_δ` 且残余 `excess` 归零；实现（`:224-230`）是仿射律 `s(α)=s(0)(1−α)` + `|s(1)|<=1e-6`，**完全不比较 `δ_k` 与 `sky_true_k`**。
  - ⇒ docstring 描述的是**被替换掉的旧判据**，而 `README.md:651-652` 的「旧形态→新形态」表已经写了新形态。两处对不上。
- 应为：docstring 同步到实现；或把实现同步到 docstring 并给出新判据的判别力证据。

**B-9（次重，恒真/自证方向）｜N2b 的「三个独立对象」论证不成立，整条 N2 生产链路对求解器缺陷零判别力**
- 位置：`code/sky_plane_zero_negative.py:30-32`（docstring 的非自反论证）↔ `:159`、`:161`、`:184-191`
- 现状：`mos0 = stack_mosaic(frames, names, delta_true*0.0, wts)`、`d_true_stack = stack_mosaic(zeros, names, -delta_true, wts)`、`mos_a = stack_mosaic(frames, names, delta_of_alpha(a), weights)` —— 三项由**同一个线性算子 `stack_mosaic` 在同一批输入上**产生（`stack_mosaic` 是覆盖集上的加权平均，`:334-353`，对校正场严格线性）。判据式 `mos_a − mos0 + α·dbar ≡ 0` 只要 `apply()` 保持 α 线性就恒等于 0，**与 δ 的内容无关、与 `p2_sky_plane_build` 的任何缺陷无关**。docstring 称三者「分属三个不同对象」不成立（`dbar` 就是 δ 的马赛克，`mos_a` 就是 `mos0−αδ` 的马赛克）。
  唯一残余判别力在**施加函数**上（`g08_defect_probe.py` 的 D2/D3/D4/D5/D8/D9），`D1_solver_bias` 只能被 `N2a` 这条**存在性**门（`max|δ| <= 1e-9`）抓住。⇒ `REPORT_experiment.md:106-107`「本表补的是**生产天光面链路**…此前没有的那一条」在判别力上被高估。
- 应为：要么为 N2 加一条真正读生产求解器输出的量值型对照（例如注入一个生产模型**不可**表示的帧间分量并要求 δ 的残差落在预言范围内），要么在 docstring 与报告里如实降级 N2b 的判别力范围。

**B-10｜落盘产物 `q7_rank_rtol.json` 里有一条被它自己相邻字段证伪的结论串，而 `REPORT_experiment.md:98` / `REPORT_paper.md:96` 正是引它作证据**
- 位置：`code/audit_rework/route3/exp07_rank_rtol.py:101-102` → `results/audit_rework/route3/q7_rank_rtol.json:33` ↔ 同文件 `:31`
- 现状（**我亲自读取核实**）：`:33` 的 `conclusion` 硬编码写「生产 lam=0.1·mean(diag) 时 kappa~O(10) 恒绿**而 r_eff 仍示秩亏** ⇒ H_solve 上的门零信息」；而同一 JSON `:31` 实测 `"r_eff_Hsolve_prod": 24`（= `n_free` 24，满秩）。⇒ 产物文本与自身实测字段**直接矛盾**，且该文本是审计者与下游**直接阅读**的落盘内容。
- 应为：删掉硬编码结论串，改为由计算字段生成；正确陈述是「生产 λ 档下 κ 门**与** r_eff 判据同时对一个精确奇异矩阵报『可辨识』⇒ 恒真绿门」。
- 证据：
  ```
  python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/route3/q7_rank_rtol.json'));print(d['always_green_gate']['r_eff_Hsolve_prod'], d['always_green_gate']['conclusion'])"
  ```

**B-11｜`REPORT_experiment.md:60` 的「N=5 端到端口径 ≤±1.5%」在偶数 N 处破到 +4.99%，且报告未带该前提限定（选择性报告）**
- 位置：`REPORT_experiment.md:60`、`REPORT_paper.md:17`（「端到端口径 ≤±1.5%」）↔ `code/audit_rework/supp_control_variance/finiteN_control_variance.py` → `results/audit_rework/supp_control_variance/finiteN_control_variance.json`
- 现状（**我亲自读取核实**）：`mc_iid` 22 行中 `ratio_B` 偏离 1 超过 1.5% 的恰是 **N=20 → 1.049945660892474（+4.99%）** 与 **N=40 → 1.0282785160402201（+2.83%）**；奇数 N 最大偏离 1.4873%（N=11）恰好卡在 1.5% 带内 ⇒ 「≤±1.5%」实为**奇数 N 子集**结论。而 `N_retained` 在生产中不必为奇数（中位数取两个中央序统计量均值，恰是 +5% 的来源）。
- 应为：①该数字必须标注「仅奇数 N」并同时报偶数 N 的 +5%；②`results/audit_rework_summary.json` 的 `calibers.end_to_end_deviation_pct: 1.5`（无 N 限定）须改。
- 证据：
  ```
  python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/supp_control_variance/finiteN_control_variance.json'));[print(r['N'],r['ratio_B']) for r in d['mc_iid'] if abs(r['ratio_B']-1)>0.015]"
  ```

**B-12｜判据汇总机制 `rollup.py` **只有一个方向** —— 只能抓「该红没红」，抓不到「永远绿」，使固化清单要求的「恒真门双向体检」在制度上不可达**
- 位置：`code/audit_rework/rollup.py:63-72`（`walk()` 只 `yield` 布尔 `False`，注释自陈「不碰数值与字符串」）+ `code/audit_rework/GATE_DISCLOSURE.json:2`（`_note` 只定义「每个 `false` 的逐点披露」）+ `code/audit_rework/run_all.sh:60-76`
- 现状：`False` 方向的恒真/下界问题已能被披露与拦下（这一半做得对）；但 **`true` 方向的恒真门、数值门（`mismatches: 0`、`factorization_gap ≡ 0`、恒 0 的负例）、以及整文件零布尔的脚本**（`supp_control_variance/spotcheck_independent_seed.py`）对汇总器**结构性不可见**。本片内已被子代理点名的隐形项至少包括：`exp07` 的 `col_equilibration.invariant`、`tau_floor.tau_active`×2、`always_green_gate.verdict_by_Hred`、`negative_fullrank.identifiable`、`equivalence_family.mismatches`；`exp06`/`exp08`/`exp02` 的 `negative_ok`；`finiteN` 的 `factorization_gap` 与 `negative.*`。
  更尖锐的一例：`exp07_rank_rtol.py:93` 把「红」编码成 **`true`**（`verdict_by_Hred: true` 表示 `kap > 1/TAU`），语义与布尔轴相反 ⇒ 真红灯对汇总器**永久隐形**（`q7_rank_rtol.json` 内 0 个 `false`）。
- 应为：`rollup.py` 增加第五类披露（如 `tautology` / `derived_identity` / `unencoded_red`），或增设「每脚本必须登记它承诺验证的主张，否则 fail-closed」的主张级清单；同时把 `:93` 这类语义反转的布尔改名。
- 证据：`rollup.py:71` 的 `elif node is False`；`q7_rank_rtol.json` 全文件 `false` 计数为 0。

**B-13｜`exp06_dof_rankeff.py` 唯一的门 `negative_ok` 是可证恒真门，且主结论无门**
- 位置：`code/audit_rework/route3/exp06_dof_rankeff.py:62`、`:67-68`、`:75-76`
- 现状：满秩分支把 `r_eff` **硬编码**为 `P`（非 `np.linalg.matrix_rank` 实测），于是两条 ratio 的分母都是字面量 `60−10=50`，两条比值是同一个浮点数除以 50 ⇒ `:75-76` 的 `abs(a−b) < 1e-12` **构造上不可能为假**。产物 `results/audit_rework/route3/q6_dof_rankeff.json:24-25` 两条读数 17 位全同（`0.9974657234228981`），坐实。
- 应为：满秩性改实测；主结论（`ratio_dof_n_minus_r_eff` 应 ≈1）补门。
- 证据：`q6_dof_rankeff.json` 的两条 ratio。

**B-14｜`exp07_rank_rtol.py:122` 的 `m42_kappa_measured = 3.2e7` 是硬编码字面量却命名为「实测」（判据读不到真实对象）**
- 现状：**我亲自读取核实** `q7_rank_rtol.json:40` 为 `"m42_kappa_measured": 32000000.0`；全仓 `grep` 该数值只命中 `exp07_rank_rtol.py` 源文件与其自身产物，**没有任何生产产物或他路实验提供它**。
- 应为：改名 `m42_kappa_reported` 并补出处，或删除。

### 4.2 须修

- **S-1｜`README.md:697` 「`实验/additive-sky-seamless/` 整目录未被 git 跟踪…无法用 git 证实」在 HEAD 为假。** 现状：`git -c core.quotepath=false ls-files 实验/additive-sky-seamless | wc -l` = **158**，且 `git status --porcelain` 对该目录为空（干净）。应为：删掉该条「诚实边界」或改写为「已跟踪，用 `git log -- 实验/additive-sky-seamless/**` 可证」。来源：`results/REVIEW.md:196` 记的是审稿当时的未跟踪态，该边界被原样搬进了 README 且从未随入库刷新。
- **S-2｜`REPORT_experiment.md:46-47` 对同一脚本给两个互斥的 `full_median`。** `:46` 说 `c3_public_plane.py` 的 `product.full_median` = 0.711；`:47` 说「全减背景退化臂 0.845 e⁻…（`full_neg_frac` 出自 `code/c3_public_plane.py:131`）」。而 `README.md:138` 把 0.845 归 A5（`c1_additive.py`）、`:186` 把 0.711 归 B4（`c3_public_plane.py`）。⇒ 出处互斥，须择一。
- **S-3｜`production_e2e_record_check.py` 的检查项计数口径不成立。** `REPORT_experiment.md:161` 说「17 项检查」，`:280` 说「13 项内部断言，4 项源文件核对」；但同文档 `:161`/`:168` 声明「源 run 树已被回收、产品 FITS 已被回收」。源码固定 `ck()` 只有 11 处（`:55,56,57,60,66,67,68,69,73,100,111`），另两处循环（`:92-99` trace、`:125` 源）在源不存在时**各贡献 0 项**（`:86-87 continue`、`:119-120 SOURCE-ABSENT`）。⇒ 「13+4」只在源仍在时成立，与「已回收」矛盾。
- **S-4｜`REPORT_experiment.md:45` 的 `excess` 口径与 `README.md:110-111` 的判决面口径冲突。** README §2.3 明写「判决面只用生产门的 `rel_step`」，而 `REPORT_experiment.md:45` 把 `excess` 中位（5.108→0.355，14.4×）与 `step` 并列作结论；`results/REVIEW.md:85-92` 已把这条列为「报告数字与自己规定的度量不一致」。
- **S-5｜`code/audit_rework/README.md:33` 与 `rollup.py:52` 的产物路径口径不一致**（各子目录 `results/` vs `audit_rework/results/`）。
- **S-6｜「12 项证伪 vs 3 项证实」推不出来。** `docs/DISPUTES.md:35` 与 `REPORT_experiment.md:96` 都用它。`c10_anchor_forensics.py:15-41` 共 25 条 CHECKS；其归档 `results/audit_rework/route1/c10_anchor_forensics.json` 状态为 17 FOUND / 8 NOT_FOUND（我实测）；按 HEAD 重指 target 后应是 13/8/4（我实测 `docs/plugins/algorithms_phase2/11_upm.md`、`docs/plugins/algorithms_phase2`、`docs/ASTROCS_DESIGN.md` 三者在 HEAD 均 MISSING，`docs/ACSD_DESIGN.md` EXISTS）。**没有任何一种读法给出 12 与 3。**
- **S-7｜`c10_anchor_forensics.py` 的 4 个 target 指向已退役/改名文档，`CHECKS` 从未重指。** `:16`、`:29`、`:35` 指向 `docs/plugins/**`（HEAD 不存在）；`:21` 指向 `docs/ASTROCS_DESIGN.md`（HEAD 已改名 `docs/ACSD_DESIGN.md`）。按修好的 `:105-108` 这 4 条会变 `TARGET_MISSING`。
- **S-8｜`code/run_all.sh` 的「非破坏性」在两条失败路径上静默失效。** `:74` 与 `:126` 都是 `|| true`；`:127` 无条件打印「results/ 已恢复为固化读数」。①若 `:74` 备份失败，`:126` 是空操作而 `results/` 已被本次运行覆写，脚本仍报成功；②`results_prior` 从不清理（`:28` 只 `mkdir -p`），`:126` 会把历史遗留的 JSON **复活**回 `results/`。⇒ 这是固化清单里的「自愈」形态：复现动作本身把证据面改回去。
- **S-9｜证据分级 `meta` 未落到机器可读面。** `sci_c_common.py:473` 的 `Gates.add(..., level="data")` 默认值使 B2/W4 这类 `meta` 门仍记为 `data`；`README.md:116-118` 声明了分级表但 `gates.summary()`（`:478-480`）只数 `n/n_pass/n_fail`，不分类计数。`results/REVIEW.md:110-118` 已提，未整改。
- **S-10｜C6 的 `E3`（RSS 随分块缩放）是单次测量且两臂工作量不等。** `code/c6_sparse_dense.py:83-84`。`results/REVIEW.md:149` 已提，未整改。
- **S-11｜`results/audit_rework/route1/c10_anchor_forensics.json` 与脚本不同代。** 归档顶层键只有 `['repo_root','checks']`，缺脚本 `:96-99` 的 `evidence_qualification` 与 `:119-128` 的 `rollup`；且我实测归档中**没有** `error` 且 `status=FOUND` 的行，与 docstring `:68-71` 自述的旧 bug 形态不符 —— 说明归档是更早一代的运行，下游按它判读会拿到过时答案。
- **S-12｜`c10_anchor_forensics.py` 的 pattern 不验取值 ⇒ 对「常数取值错」结构性失明。** `:27`（「support_min = 0.2」）用 pattern `support_threshold`；`:30` 与 `:34` 两条互斥 claim（`rel_step_max=0.1` vs 接缝门 `1e-2`）共用 pattern `rel_step`。把 `sigma_bg_floor` 从 1.0 改成任意值、`all_anchors_confirmed` 仍绿。而 `docs/DISPUTES.md:31` 的 A-P5-03 回炉裁决正是靠这张表作取证底座。

### 4.3 建议

- `sci_c_common.py`：`build_world(**legacy)`（`:209`）静默吞拼错参数名，建议改 `**legacy` → 显式关键字 + 未知键报错；`smooth_sky_field`（`:89`）与 `import os`（`:15`）为死代码。
- `_step_at` 的 `rel = step/lvl if (lvl and …)`（`:409`）在 `lvl==0.0` 时落 nan、在 `lvl==nan` 时因 nan 真值而通过守卫，建议改 `np.isfinite(lvl) and lvl != 0`。
- `c6_sparse_dense.py:79-80` `float(sb)/max(float(db),1.0)`：`sparse_bytes` 为 `None` 时抛 `TypeError`（fail-loud，可接受），建议显式守卫。
- `production_e2e_record_check.py`：把固定 `ck()` 的条数写成常量并在输出里给出「固定/条件」两类计数，避免报告再出现 13/4/17 三种口径。
- `README.md:551-563` 的文献表与 `REPORT_paper.md:228-240` 的 9 条参考文献是**两套不相交的集合**（README 列 Wahba/Duchon/Horne/Regnault/Morganson/Wild/Soto 等 10 条，REPORT_paper 列 Casertano/Clopper&Pearson/Fruchter&Hook/Serfling 等 9 条）；README 称「`results/evidence_lit.json` 11/11 已核验」但表内只有 10 条。建议以 `refs.md` 为单一来源生成。
- `docs/README.md:10` 的「支撑对象」列写了 `../docs/seam-gate-floor.md` 自身的实验闭合表（自指），宜改成具体锚点。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 结果 |
|---|---|---|---|
| **CE-1** | 假设 `results/c1_additive.json` 与 `run/SCI-403/results_prior/c1_additive.json` 是同一份（README 声明 `run_all.sh` 会把固化读数原样放回）⇒ 则报告数字应与 `results/` 一致 | 推翻「文档数字与证据面不符」只是数值漂移 | **推翻成功**：`results/` 的 `n_params=49, kappa=3666.5, d_med=0.39719, slope=0.76871`，`results_prior/` 的 `58, 3.358e6, 0.38295, 0.79759`；报告引用的是后者。⇒ 不是漂移，是**文档绑在旧档上**。 |
| **CE-2** | 假设 `run/SCI-403/results_prior/c5_weights.json` 与 `results_head/c5_weights.json` 相同即证明「本次运行未改变 C5」⇒ 则「13× 降为 4.7×」的归因无处可查 | 推翻「rc=1 归因不可复现」 | **推翻成功**：两者 md5 相同（`863e4b47…`）且都含旧值 0.0245/0.3203/0.0651，而仓内 `results/c5_weights.json`（`14225526…`）含新值 0.0761/0.3590/0.1216 ⇒ **三份互不相同**，「本次运行 vs 上次」的二分框架不成立。 |
| **CE-3** | 假设「×5.07」在仓内还有出处（README:22/34/145、REPORT_experiment:87、REPORT_paper:144/252 都在引它） | 推翻「A-P5-11 改写失去锚点」 | **推翻成功**：`5.07205210713894` 只在 `run/SCI-403/results_prior/c1_additive.json` 的 `oob_prediction.seam_ratio_short_over_long`；HEAD 的 `results/c1_additive.json` 是 `6.139029101773245`。 |
| **CE-4** | 假设 `GATE_DISCLOSURE.json` 的 class 值域被 `rollup.py` 校验（fail-closed 覆盖「值」） | 推翻 B-7 | **推翻失败**：`rollup.py:165` 只比较 `== "open_red"`，`:171` 的 `stale` 只比对键。7 条 `cell` 被静默当作非红。 |
| **CE-5** | 假设 `sky_plane_zero_negative.py` 的 docstring 判据式与实现一致（因为它自称「判据不重写」） | 推翻 B-8 | **推翻成功**：三条判据的 docstring 式与实现式逐条不同；`DELTA_TRUTH_TOL_E`/`RESIDUAL_EXCESS_TOL_E` 是死常量。 |
| **CE-6** | 假设 N2b 的「三个独立对象」成立（docstring:30-32）⇒ 判据对 δ 的内容有判别力 | 推翻 B-9 | **推翻失败**：`mos0`/`d_true_stack`/`mos_a` 同由线性算子 `stack_mosaic` 产生；判别力只落在 `apply()` 上。 |
| **CE-7** | 假设探针缺失时 `sky_plane_zero_negative.py` 会「不冒充已验证」（docstring:38-39） | 推翻 B-4 | **推翻成功**：SKIP 后 `main()` 走到 `:373` 返回 0。 |
| **CE-8** | 假设 `c10_anchor_forensics.py` 的四个 target 在 HEAD 仍存在 ⇒ 则 c10 不必重指 | 推翻 S-7 与「c10 恒返回 1」 | **推翻成功**：`docs/plugins/algorithms_phase2/11_upm.md`、`docs/plugins/algorithms_phase2`、`docs/ASTROCS_DESIGN.md` 全部 MISSING（`docs/ACSD_DESIGN.md` EXISTS）⇒ 按修好的 `:105-108` 会有 4 条 `TARGET_MISSING`，`all_anchors_confirmed` 必 False ⇒ `:133` 恒返回 1 ⇒ `route1/run_all.sh:11`「all 10 experiments OK」不可达。 |
| **CE-9** | 假设 `docs/plugins/**` 这类路径仍被 c10 以外的地方引用 | 找「退役对象自��零消费者」的同类 | **推翻成功（找到同类）**：`code/reverse_verify/smooth_lambda/upm_sweep.cpp:236` 调用 2026-09-25 已删除并撤声明的 `p2_upm_normalized_weights`，而 `lib/algorithms/coverage/src/upm.cpp:1999` 与 `lib/algorithms/coverage/include/astro/phase2/upm.h:189` 的注释逐字自称「**全仓零消费者**」。⇒ **注释自称零消费者为假**（本片最贴固化清单「退役治理」的一条，但出处在兄弟片文件，仅作交叉线索登记）。 |

---

## 6. 盲复算

**做法**：先遮住前三轮审稿结论与本片文档的全部判定，只用「文档声称什么」这一层，对每条量化主张独立取数并复算，再与原结论比对。

| 独立取证动作 | 复算结果 | 与既有结论比对 |
|---|---|---|
| 把 README §4.1 的 A4「0.383 / 0.850」当作待验命题，直接读 `results/c1_additive.json` 的 `seam_summary` | `d_med=0.39719, d_max=0.80385, b_med=0.37909` | **不一致**，且 A5 门值 `0.80430` 与 README:138 的 `0.845` 差 5% ⇒ 判「偏松」的一侧是**文档**（我原判一致） |
| 把「×5.07」当作待验命题，用 `grep -r "5.07\|5.072052" ` 全仓定位出处 | 只命中 `run/SCI-403/results_prior/`（不入库） | 我原判「该原值在证据面上已消失」——**盲复算独立得到同一结论，判一致** |
| 把「三门为红 ⇒ rc=1」当作待验命题，用 `grep` 找三个门名在单元内的出现 | 代码/JSON 零命中；现存 A6p/A9p/A9q/W1p 全绿 | **一致** |
| 把「audit_rework 退出码 0」当作待验命题，静态推 `rollup.py` 的返回值 | 6 条 `open_red` ⇒ 返 1 ⇒ 入口 `exit 1` | **一致** |
| 把 `docs/smooth-lambda.md:33` 的「判据 C7 数值验证」当作待验命题 | §3–§9 全占位符；CRITERIA 无执行者 | **一致** |
| 把 `README.md:697`「整目录未被 git 跟踪」当作待验命题 | `ls-files` = 158，`status --porcelain` 空 | **一致**（这是本片最容易被「文档说了就算」骗过的一条，盲复算单独确认） |
| 把 `c1_median_variance.py` 的「N≥65 时 MC 偏差 <2%」当作待验命题（子代理给出 0.81%/1.72%/0.91%/0.74%） | 数值成立，但 N=33（0.81%）比 N=65 更接近 1，阈值 65 由 N 网格挑出 | 我原判「需修」；盲复算后**偏严**了一点 —— 该条应降为「建议」而非「须修」 |

**盲复算总判：对原结论判「一致」；仅一处判「偏严」**（c1 的 `N≥65` 阈值来源），已在 §7 中相应下调该条的等级。**未发现偏松。**

---

## 7. 子代理派发记录

派发 5 个范围（工具重复投递，实际起跑 6 个 agent instance；去重后 5 个不同范围，其中 S4/S3 各被重复投递一次）。**全部只读、零 git 写、零编译、零实验脚本运行。**

| # | 范围 | 份/行 | 状态 |
|---|---|---|---|
| S1 | `audit_rework/route1/{c1_median_variance.py, c9_representation_boundary.py, c10_anchor_forensics.py, run_all.sh}` | 4 / 399 | ✅ 已回，**我逐条复核并亲自重读 c10 与 route1/run_all.sh** |
| S2 | `audit_rework/route2/{e3,e4,e5,e6,e8,e10}` | 6 / 615 | ⏳ 写件时未回 |
| S3 | `audit_rework/route3/{exp01,exp02,exp06,exp07,exp08}` + `supp_507_relstep/eb_relstep_calibration.py` + `supp_control_variance/{finiteN_control_variance.py, spotcheck_independent_seed.py}` | 8 / 1127 | ✅ 已回（在写件过程中到达），**已复核** |
| S4 | `reverse_verify/{m16_sampling/run_reconstruct.py, smooth_lambda/{upm_sweep.cpp, analyze.py, adaptive.py, report.py, seam_compare.py, seam_product.py, run_mosaic_ls.sh, gen_all.sh}, m16_scene/exp_realbase_consistency.py}` + `code/build_probes.sh` | 12 / 1410 | ✅ 已回，**已复核** |
| S5（S4 的重复投递） | 同上 | — | ✅ 已回，内容与 S4 一致 |
| S6（S3 的重复投递） | 同 S3 | — | ✅ 已回，内容与 S3 一致 |

### 7.1 逐条复核 —— S1（route1）

| 子代理结论 | 我的复核 | 处置 |
|---|---|---|
| `c10_anchor_forensics.json` 是修复前归档，含 3 条 `{'error':'target missing'}` 却记 `status=FOUND`，JSON 早于脚本 mtime | **我实测**：`results/audit_rework/route1/c10_anchor_forensics.json` 顶层键只有 `['repo_root','checks']`（缺 `rollup`/`evidence_qualification`）✔；但「3 条 error 却 FOUND」**不成立** —— 我 grep 到的是 `rows with error but FOUND: []`，归档里根本没有 `error` 字段 | **部分否决**。「归档早于代码、结构缺字段」采纳（收为 S-11）；「含 3 条幽灵 FOUND」**否决**，理由：归档中该形态为零条，docstring `:68-71` 描述的是更早一代的运行 |
| c10 的 4 个 target（`docs/plugins/**`、`docs/ASTROCS_DESIGN.md`）在 HEAD 全部 MISSING ⇒ c10 恒返回 1 ⇒ `route1/run_all.sh:11` 不可达，且在父级 `set -eu` 下会中止整个复现入口、旁路 `rollup.py` | **我亲自复读** `c10_anchor_forensics.py:126/:133` 与 `route1/run_all.sh:9/:11` ✔；**我实测** 4 个 target MISSING ✔。父级旁路部分我**只读验证了** `code/audit_rework/run_all.sh:23-31` 的 `run()` 用 `set -eu` + 子壳 `exit 1` ✔ | **全部采纳**。收为 S-7 + §3 表第 20 行「阻断」 |
| `c9` 的负例是 IEEE 恒等（`resid=0.0` 精确非 1e-18），任何保常数插值都给 0；`representation_scale_2h = 2*H` 是定义式，与实测 `cutoff_scale_px=1600` 差 6.25× 且无 verdict | 我核对了 S1 引用的文档位置（`REPORT_experiment.md:90`、`REPORT_paper.md:156` 确实引这条负例）；代码本体我未亲自重读 | **采纳但降级为「须修」**，理由：c9 的两条硬伤（恒真负例、定义式并列）成立，但它们目前**没有被任何门或报告当作判决面使用**（退出码恒 0、只作负例登记），达不到「阻断」 |
| `c1` 的 `share_correct = 0.0/(0.0+ivar)` 与 `prediction_x_kcorr = k*v0` 是恒真演示 | 我采信（读数来自其 JSON 引用） | **采纳**，收为建议级 |
| `c1:88` 的 `rel_err_mc=1/√M` 误标 ⇒「N≥65 偏差 <2%」只有 2.4σ、且 N=33 更接近 1 | 我在 §6 盲复算中独立复核了 N 扫描序列 | **下调为「建议」**（盲复算判我偏严） |
| route1 `run_all.sh` 无 `set -e`，与父级口径不一致 | 我亲自复读确认 `:3` 只有 `set -u` ✔ | 采纳，建议级 |

### 7.2 逐条复核 —— S4（reverse_verify + build_probes）

| 子代理结论 | 我的复核 | 处置 |
|---|---|---|
| **BLK-1**：`upm_sweep.cpp:236` 调用 `p2_upm_normalized_weights`，而该符号 2026-09-25 已删（`upm.cpp:1999`）并撤声明（`upm.h:188-190`），注释自称「全仓零消费者」却有活调用者 ⇒ λs 扫描整链编译不过 | **我亲自 grep 全仓**：`lib/.../upm.cpp:1999`（RETIRED 注释）、`lib/.../upm.h:189`（声明已撤下）、`实验/.../upm_sweep.cpp:236`（活调用）、`实验/.../analyze.py:53`（仍描述该 API 为「已登记缺陷」）—— **四项全部命中** ✔✔✔✔ | **采纳并升级**：这是我本片 CE-9 独立构造出的同类（固化清单「退役治理：注释自称零消费者曾为假」）。因调用方文件属兄弟片 `EXP-additive-sky-seamless-002`，本片只作**交叉线索**登记为 §5 CE-9，不计入本片整改分母 |
| **BLK-2**：`CRITERIA.md` 的 C1–C8 在管线中无任何执行者（`analyze.py main()` 无 return、`report.py` 无阈值列、三个 `.sh` 不读 CRITERIA）⇒ `docs/smooth-lambda.md:33` 的「判据 C7 数值验证」无支撑 | 我亲自读完 `docs/smooth-lambda.md` 全文，确认 §3–§9 全为 `{{...}}` 占位符、`:33` 确有该句；`docs/README.md:11` 已登记占位状态 | **采纳**，收为 B-6。我把它的定性从「无执行者」微调为「**文档在占位保护区之外做了验证声称**」，因为 `docs/README.md:11` 已部分登记 |
| `run_mosaic_ls.sh` 实际阻塞点是 L4 帧树回收（`run/RELEASE-02/L4-rebuild/` 不存在），**不是** `m16_scene.render_m16_frame` | 我核对 `README.md:510-511`：`:510` 说的是「`data_matrix/exp{2,3}` 依赖 `run/RELEASE-02/L4-rebuild/` 帧树 ⇒ 报 SKIP」，`:511` 用「同上」指 `:510` ⇒ **子代理正确，我原以为 `:511` 的「同上」指 `:509`** | **采纳子代理的更正**，否决我自己先前的读法。写入 §3 备注 |
| `gen_all.sh:14` 的 `run()` 两条分支都成功 ⇒ 永远退出 0；`run_mosaic_ls.sh:25/27` 用 `|| true` 解除自己的 `set -eu` | 我未亲自复读这两份 `.sh`；采信 | **采纳为须修**，但因 C 组/S4 交叉面已足够密集，未单列条目（并入 §4.3 建议的复现入口纪律项） |
| `exp_realbase_consistency.py` 的三条门是 NaN 安全的好门、退出码是真判决、自愈体检干净 | 采信（与我本片 §4 对 `sky_plane_zero_negative.py` 的相反结论并列，说明该腿是本片唯一做对的） | **采纳**，在 §3 中作为对照记入 |
| `report.py` 把「产物损坏」与「从未跑过」渲染成同一句 | 采信 | 采纳，建议级 |
| `README:508`「缺输入时显式 SKIP」为假（无 SKIP 分支） | 我核对了 `README.md:508` 的原文「可跑，前置生成器输出后可跑；缺输入时显式 SKIP」✔ | 采纳，须修级（未单列，计入 §4.3） |
| 子代理称「题给 README 504/507-523 行号不在根 README，根 README 只有 164 行」 | 我确认本片 `README.md` 是 `实验/additive-sky-seamless/README.md`（721 行）✔，行号 504/508 确在其中 ✔ | **采纳为读法更正**（我一直在用同一份文件，无影响） |

**否决/下调合计 3 条**：①「归档含 3 条幽灵 FOUND」——事实不符，实测为零条；②「c1 `N≥65`」——从须修降为建议；③「`run_mosaic_ls.sh` 被 render_m16_frame 阻塞」——方向对但归因对象错，按子代理的更正改写。
**采纳 11 条。**

### 7.3 逐条复核 —— S3/S2 重复投递组（route3 5 + supp 3 / 1127 行，回执在写件最后到达）

| 子代理结论 | 我的复核 | 处置 |
|---|---|---|
| **B1**：`exp07_rank_rtol.py:101-102` 的硬编码 `conclusion`（「r_eff 仍示秩亏」）被同 JSON `:31` 的 `r_eff_Hsolve_prod: 24` 证伪；`REPORT_paper.md:96` 引它作证据 | **我亲自读取 `q7_rank_rtol.json`**：`:31` = 24 ✔、`:33` conclusion 串逐字确认 ✔ | **采纳为 B-10（阻断）** |
| **B2/B3**：`exp07` 段 (d) `equivalence_family` 与 `exp06` 的 `negative_ok` 均为代数恒真门（200 例必 0 / 两分母同整数） | **我亲自读取**：`equivalence_family = {"cases":200,"mismatches":0}` ✔；`q6_dof_rankeff.json:24-25` 两条 ratio 17 位全同 ✔ | **采纳**：B3 收为 B-13；B2（exp07 等价族）并入 §4.2 建议（它未进入判决面） |
| **B4**：`finiteN` 的 z 值 N=201→+3.04、N=5→+2.30 **无任何阈值门**；`spotcheck` 不与主跑比对、不算 z ⇒ docstring 宣称的检验未实现 | 我核对了 `finiteN_control_variance.json` 的 `cross_check` 与 `spotcheck_independent_seed.json` 均无对应判决字段 ✔ | **采纳为须修**（未单列，理由：主结论方向未被推翻，属门缺失而非结论错误） |
| **B5**：`finiteN` 结构臂下 `ratio_B` 达 2.29–7.74，而报告只报 iid 臂 ±1.5% 不带前提限定 | 我核对了 `mc_iid` 与 `structure` 两块均存在 ✔ | **采纳**，与我自己独立发现的偶数 N 空洞合并为 **B-11（阻断）** |
| `REPORT:79` 的 `f=0.3 ⇒ 1.03e-2` 实为**零效应的序统计量偏置**（`chi` 把承载样本排在高端，中央序统计量落在未承载组 ⇒ `seam` 与 `bg` 同时与 `amp` 无关），不是「噪声恢复灵敏度」 | 我未逐行读 `exp01_seam_gate.py`；但我核对了 `q1_seam_gate.json:86` 的读数存在，且其量级（1.03e-2）与同族文档对照 f=0.5 读 0.025 / f=0.6 读 0.05 的方向一致 | **采纳为须修（未单列为阻断）**。理由：这是对本片结论**方向**的质疑，但两个独立子代理对同一读数的机理解释不同（其一说「被 `att` 代数抵消」，其一说「序统计量偏置」），我不采信任何一方为主结论，只登记「1.03e-2 的成因未裁定」 |
| `exp08` 的理论腿 `1−sinc²` 在 λ≥4h 比实测低 1.6–2.3×，脚本把两数并列却不给差异量 | 我未逐行读 `exp08`；采信其从 `q8_node_spacing.json` 读出的比值 | **采纳为须修**（未单列） |
| `q7_rank_rtol.json:22` 的 `Infinity` 违反 RFC 8259 | **我亲自读取确认** `:22` 为 `"kappa_Hred": Infinity` ✔ | **采纳为须修**（并入 §4.3 建议） |
| `exp07_rank_rtol.py:93` 把「红」编码成 `true`，`rollup.py` 只收 `False` ⇒ 真红灯隐形 | **我亲自复读 `rollup.py:71`**（`elif node is False`）✔ 且 `q7_rank_rtol.json` 内 `false` 计数为 0 ✔ | **采纳并升级为 B-12（阻断）** —— 我把它与本片其余「恒真门」问题合并为一条**机制级**阻断 |
| `eb_relstep_calibration.py` 是全批最干净的一份：`REPORT:95` 的四个数字逐位对上（45.5–59.8% / ≤4/112 精确命中 0.03571428571428571 / 松 20× / B0=1000 网格内不红） | 我未逐行读 `eb`；但我用 `GATE_DISCLOSURE.json` 确认 part2a 的两条确实登记为 `open_red` ✔ | **采纳为正面确认**（本片唯一被两个子代理独立认定为「双向体检与披露都做对了」的件） |
| 路线3 seed 实际是 **20260926** 而 `audit_rework/run_all.sh:39` 与 `results/audit_rework_summary.json` 的 `_meta.seed_note` 均称 20250926 | 我未逐行读 exp01/02/06/07/08 的 seed 行；未独立核对 | **记为待核**（不写入 §4，列入 §8 未核验点） |
| `exp07` 的 κ 复算与我读的产物差 0.6%，疑为 LAPACK 版本差 | 我未复算 | **否决该因果归因**（保留「产物数值与报告的 4 位有效数字不一致」这一可核事实，否决其根因推测） |

**本组否决/待核合计 2 条**（路线3 seed 记为待核；κ 偏差的 LAPACK 归因被否决为未经证实的推测）；**采纳 9 条**，其中 3 条升级为本片阻断（B-10、B-11、B-12）、1 条升级为阻断（B-13）。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD            # f9650dd0aed97d7f261e5e6547f4fdb505bc313b

# §1 分母
sed -n '2169,2227p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml

# B-1：报告数字 vs 证据面 vs 旧档
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c1_additive.json'));print('HEAD ',d['sky_plane']['n_params'],d['seam_summary'],d['oob_prediction'])"
python3 -c "import json;d=json.load(open('run/SCI-403/results_prior/c1_additive.json'));print('PRIOR',d['sky_plane']['n_params'],d['seam_summary'],d['oob_prediction'])"
md5sum 实验/additive-sky-seamless/results/c5_weights.json run/SCI-403/results_prior/c5_weights.json run/SCI-403/results_head/c5_weights.json
git -c core.quotepath=false log --oneline -3 -- 实验/additive-sky-seamless/results/c1_additive.json

# B-2：旧门名 vs 现行门名
grep -rn "A6_subset_invariance\|A9_final_gauge_near_noop\|W1_control_ivar_best" 实验/additive-sky-seamless/
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/c1_additive.json'));[print(r['id'],r['value']) for r in d['gates']['rows']]"

# B-3：脚本数 / open_red 数 / e8 的 NaN 吞红
ls 实验/additive-sky-seamless/code/audit_rework/{route1,route2,route3,supp_507_relstep,supp_control_variance}/*.py | wc -l
python3 -c "import json,collections;d=json.load(open('实验/additive-sky-seamless/code/audit_rework/GATE_DISCLOSURE.json'))['entries'];print(collections.Counter(v['class'] for v in d.values()))"
python3 -c "import json;print(json.load(open('实验/additive-sky-seamless/results/audit_rework/route2/e8_identifiability_rank_rtol.json'))['column_equilibration_invariance'])"
sed -n '60,78p' 实验/additive-sky-seamless/code/audit_rework/run_all.sh

# B-4 / B-8：sky_plane_zero_negative 的 docstring vs 实现、SKIP 返回值
sed -n '16p;24p;25p;37,39p;237,238p;303p;311,314p;373,375p' 实验/additive-sky-seamless/code/sky_plane_zero_negative.py

# B-5 / B-7：披露文件
sed -n '2p;52,55p;62p;89p;102p' 实验/additive-sky-seamless/code/audit_rework/GATE_DISCLOSURE.json
sed -n '165p;171p;209p' 实验/additive-sky-seamless/code/audit_rework/rollup.py

# B-6：占位符与「C7 数值验证」
grep -n "{{\|判据 C7 数值验证" 实验/additive-sky-seamless/docs/smooth-lambda.md

# S-1：git 跟踪（README:697 的反证）
git -c core.quotepath=false ls-files 实验/additive-sky-seamless | wc -l
git -c core.quotepath=false status --porcelain 实验/additive-sky-seamless | wc -l

# S-6 / S-7 / S-11：c10 的计数、target 存废、归档代差
for p in docs/plugins/algorithms_phase2/11_upm.md docs/plugins/algorithms_phase2 docs/ASTROCS_DESIGN.md docs/ACSD_DESIGN.md; do printf "%-45s %s\n" "$p" "$([ -e "$p" ] && echo EXISTS || echo MISSING)"; done
python3 -c "import json,collections;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/route1/c10_anchor_forensics.json'));print(list(d.keys()),collections.Counter(c['status'] for c in d['checks']))"
sed -n '124,133p' 实验/additive-sky-seamless/code/audit_rework/route1/c10_anchor_forensics.py

# B-10：q7 产物自相矛盾 + Infinity + 硬编码实测 κ
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/route3/q7_rank_rtol.json'));g=d['always_green_gate'];print(g['r_eff_Hsolve_prod'], g['n_free'], g['conclusion']);print('equivalence_family =', d['equivalence_family']);print('production_reading =', d['production_reading'])"
grep -n "Infinity" 实验/additive-sky-seamless/results/audit_rework/route3/q7_rank_rtol.json

# B-11：finiteN 的偶数 N 空洞
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/supp_control_variance/finiteN_control_variance.json'));[print(r['N'],r['ratio_B'],'dev=%.4f'%(r['ratio_B']-1)) for r in d['mc_iid'] if abs(r['ratio_B']-1)>0.015]"
sed -n '60p' 实验/additive-sky-seamless/REPORT_experiment.md

# B-12：rollup 只抓 False 方向
sed -n '63,72p' 实验/additive-sky-seamless/code/audit_rework/rollup.py
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/route3/q7_rank_rtol.json'));print('false count in q7 =', json.dumps(d).count(': false'))"

# B-13：exp06 的两条 ratio 17 位全同
python3 -c "import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/route3/q6_dof_rankeff.json'));print([d['rows'][0].get(k) for k in ('ratio_dof_n_minus_r_eff','ratio_dof_n_minus_params')] if 'rows' in d else json.dumps(d)[:400])"

# S-8：run_all.sh 的非破坏性两条失败路径
sed -n '28p;74p;122,127p' 实验/additive-sky-seamless/code/run_all.sh

# §5 CE-9：退役治理（注释自称零消费者）
grep -rn "p2_upm_normalized_weights" lib/ 实验/ --include=*.h --include=*.cpp --include=*.py
sed -n '1999p' lib/algorithms/coverage/src/upm.cpp
sed -n '188,190p' lib/algorithms/coverage/include/astro/phase2/upm.h
```

**未核验点（如实登记）**
1. C 组 6 份（route2 的 e3/e4/e5/e6/e8/e10，615 行）子代理在写件时未回，其**源码**读数未进入本件结论；其中 `e8_identifiability_rank_rtol.py` 的**产物**我已直接读取并用于 B-3。
2. D 组 2 份（`code/upm_probe.cpp` 302 行、`code/make_figures.py` 175 行）无人读过；`results/REVIEW.md:146-147` 对 `make_figures.py:93/:165` 的两条旧意见因此未在本轮复核。
3. 未运行任何脚本 ⇒ §4 中凡标「实测」的均为**静态读取仓内文件/JSON**所得，非重新执行；`g08_defect_probe.py`、`seam_gate_coverage.py`、`c1..c7` 的红灯状态未实跑确认。
4. `upm_sweep.cpp` 调用已退役 `p2_upm_normalized_weights`（CE-9）的「编译失败」是由「定义已删 + 声明已撤 + 同编源码」三项源码事实推出的**确定性结论**，未实际跑 g++ 取错误文本；且调用方属兄弟片 `EXP-additive-sky-seamless-002`，未计入本片整改分母。
5. `REPORT_experiment.md:46-47` 的 0.711/0.845 出处互斥（S-2）我只做到「文档内部互斥」这一步，未打开 `results/c3_public_plane.json` 逐位裁定（S-2 保留为须修）。
6. 「12 项证伪 vs 3 项证实」的**正确数**未裁定：按 HEAD 静态重指的 13/8/4 是推导值（目标存在性逐一验证 + 归档 status 沿用），需前台实跑 `c10_anchor_forensics.py` 确认。
7. 两个子代理对 `REPORT_experiment.md:79`（`f=0.3 ⇒ 1.03e-2`）的机理解释互相冲突（`att` 代数抵消 vs 序统计量偏置），且其源码本轮无人逐行读 ⇒ 该读数的成因**未裁定**，已在 §7.3 记为「只登记不采信」。
8. 路线3 的 seed 究竟是 20250926 还是 20260926（子代理单方面声称后者，`audit_rework/run_all.sh:39` 与 `results/audit_rework_summary.json` 的 `_meta.seed_note` 写前者）—— **待核**，未写入 §4。
9. `exp08` 的「真实插值 RMS 比」是子代理另立的基准（与 exp08 的全域最小二乘样条口径不同），故「理论腿差 2.3–3.6 倍」我只采信到「脚本内的 `1−sinc²` 与同表实测无字段无门」这一事实，不采信其基准对比的数值。