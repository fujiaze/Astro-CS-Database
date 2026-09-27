# 检查-S13 · 实验单元 P1 photometric-magnitude（只读静态排查）

- 责任域：`实验/photometric-magnitude/` 全部文件（REPORT_paper.md / REPORT_experiment.md / README.md / refs.md / docs/ / code/ / results/，排除 `__pycache__`、`.pyc`），含与 `docs/science|plugins`、`独立审计/08_修复包/①测光星等坐标系/`、`独立审计/实验重做/总编对账/` 的互查。
- 方法：纯静态（不执行任何实验代码）；对每条数字声称，打开对应 `results/*.json` 逐值比对；对每个 `文件:行` 锚实开文件核对；文献抽验经 arXiv / Crossref / DOI 直达页；预读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表与 `分歧台账.md` D-01…D-11、§2 A-P1-* 终裁，已 PASS 项与已终裁项不重复上报、不重开。
- 计数：**红 3 · 黄 12 · 绿 1**（另有 1 条与 S7 插件切片重叠，见文末附注，不重复计数）。

---

## 一、红级（必须改）

### 红-1 「通过率 0.055/1.110」数量标签与证据锚双错（论文正文结果）
- **文件:行**：`实验/photometric-magnitude/REPORT_paper.md:146`；`实验/photometric-magnitude/REPORT_experiment.md:71`；`实验/photometric-magnitude/results/redo_summary.json:18`
- **问题描述**：三处均写「干净/污染场**通过率** 0.055/1.110 对照，能红」，证据记为 `route3/exp_S12_dex_bound.json`。其一，「通过率 1.110」大于 1，作为比率不可能；其二，所指 JSON 内根本没有这组数；其三，这组数实际来自另一路线、另一物理量。
- **证据（含反方核验）**：
  - `results/redo/route3/exp_S12_dex_bound.json` 实开：`H12a.table` 只有 amplitude_dex 0.05/0.1/0.15/0.3 → `max_abs_log10m` 0.13/0.26/0.39/0.78，`H12b` 为 0.95/5.9499…，全文 grep `0.055`、`1.110` 均无命中。
  - `results/redo/route1/exp4_spatial_gain.json → S9_amplitude_bound` 实开：`field_amplitude_dex=0.1 → max_abs_log10m_dex=0.05541889097448501`（`bound_1.0dex_binds=false`）、`field_amplitude_dex=2 → 1.1098845277165572`（`bound_1.0dex_binds=true`）——0.055/1.110 即由此来，量纲是**场峰 |log10 m|（dex）**，不是通过率。
  - 反方核验：结论方向本身仍成立——route1 exp4 S9 显示 2.0 dex 场下界能变红（`bound_1.0dex_binds=true`），route3 S12 H12a/b 也从 binding 口径支持「1.0 dex 界非恒真」；错的是数量名称与引证对象，不是结论。
- **建议改法**：把「通过率」改为「场峰 `max_abs_log10m`（dex）」，证据改指 `route1/exp4_spatial_gain.json → S9_amplitude_bound`（或改引 `route3/exp_S12` 实际的 binding 表读数）；`redo_summary.json:18` 同步订正 note。
- **所属面**：③跨文档冲突（报告 vs results）、④幻觉与锚

### 红-2 README 的 F673N WCS 偏移行与所引 JSON 及本单元订正记录冲突
- **文件:行**：`实验/photometric-magnitude/README.md:226`（连带 `README.md:227` 的范围句）
- **问题描述**：README §4.5 表（标题即声明数据源 `results/step3_forward_vs_photflam.json`）写「F673N (26.78, 28.77) px = **1.751″**」，并推出「三滤镜头部与 Gaia 的系统偏移 **1.622–1.751″**」。
- **证据（含反方核验）**：
  - `results/step3_forward_vs_photflam.json → wcs_refinement.F673N` 实开：`shift_px = [23.122746508379805, 33.524868370683976]`、`shift_arcsec = 1.629025821950028`；JSON 全文 grep `26.78`、`28.77`、`1.751` 均无命中。
  - 本单元 `results/DOC_CORRECTIONS.md:64-65`（C4）自己写的就是「F673N：shift = (23.12, 33.52) px = 1.629″」——README 与同单元订正记录互相矛盾。
  - README 该行内部不自洽：√(26.78²+28.77²)=39.30 px，按同表 0.04″/px 口径换算 = 1.57″，并非 1.751″。
  - 反方核验：「~1.6″ 系统偏移」这一结论在 `REPORT_paper.md:142`、`REPORT_paper.md:155`、`README.md:198` 均用 ~1.6″ 表述，不受此行影响；受累的是该行两组 px、1.751″ 与由此得到的上限（实际 1.622–1.629″）。
- **建议改法**：F673N 行按 step3 JSON 改为 (23.12, 33.52) px = 1.629″，`README.md:227` 范围改为 1.622–1.629″。
- **所属面**：③跨文档冲突（README vs results/DOC_CORRECTIONS）

### 红-3 DOC_CORRECTIONS C3 的「始终 PASS」与 step7 归档判定相反
- **文件:行**：`实验/photometric-magnitude/results/DOC_CORRECTIONS.md:52`（连带 `README.md:259-261`、`REPORT_paper.md:138` 同段叙述）
- **问题描述**：C3 写「gx = 0 → 0.4 ADU/px 时 σ_obs 0.04534 → 0.04897（1.08×），**始终 PASS**」；README/论文由此推出「判据对确定性加性图样不敏感 ⇒ 不能认证天光扣除质量」，但同一条负例的归档 JSON 在 gx=0.4 档判定是 **ABOVE_CEILING（判红）**。叙述层只报度量 1.08×、不提该档判红，读者会以为该形态永不判红。
- **证据（含反方核验）**：
  - `results/step7_negatives.json → negatives[N2].arithmetic_gradient_control` 实开：`{gx:0, σ=0.045344256936544786, PASS}`、`{gx:0.1, σ=0.048152664632654836, PASS}`、`{gx:0.4, σ=0.048967603629865054, verdict=ABOVE_CEILING}`。
  - 脚本 `code/step7_negatives.py:151-158` 实开：每档都调 `gate(...)` 并落 `verdict=v`，该判红不是我方复算，而是归档产物本身。
  - 反方核验：σ 数值（0.04534→0.04897、1.08×）与 JSON 完全一致，「度量仅 1.08×（对比 Poisson 2.91×）」的相对不敏感叙述有据；冲突点专在「始终 PASS」与隐含的「不判红」。
- **建议改法**：C3 改为如实表述（如「度量 1.08×；归档 JSON 中 gx=0.4 档 verdict=ABOVE_CEILING」），`README:259-261` 与 `REPORT_paper:138` 同步注明该档判定，能力边界结论按二者并存重述。
- **所属面**：③跨文档冲突（叙述 vs results verdict）、②行文逻辑

---

## 二、黄级（建议改）

### 黄-1 max_stars=5000 ⇒ 6.36e-4 的证据锚指错文件
- **文件:行**：`REPORT_experiment.md:92`；`results/redo_summary.json:20`
- **问题**：豁免清单写「σ_ZP(N) 曲线支撑（5000 ⇒ 6.36e-4 mag）[实验:route3/exp_S08]」，evidence 也记该文件。
- **证据**：`route3/exp_S08_ladder.json → H8b.zero_point_se_by_N` 实开，N 档只有 200/500/2000/10000（se_mag 4.431e-3/2.802e-3/1.401e-3/**6.2665e-4**），无 5000 档、无 6.36e-4；全 `results/` grep `0.000636` 仅命中 `results/redo/route1/exp3_fov_ladder.json → S11_cap_curve`（`n_stars=5000 → sigma_zp_stat_mag=0.0006363961030678928`），且 `REPORT_route1.md:235` 归属正确。反方核验：数值本身真实，仅锚错。
- **建议改法**：evidence 改指 `route1/exp3_fov_ladder.json (S11_cap_curve)`。
- **所属面**：④

### 黄-2 「全档敏感性表（0.5–4.0 px）」与实际档位不符
- **文件:行**：`REPORT_paper.md:147`、`REPORT_experiment.md:91`、`results/redo_summary.json:25`
- **证据**：`route1/exp2_mag_window_match.json → S8_match_radius` 档位 = {0.5, 1, 2, 3, 5}（×σ_jitter 阶梯）；`route3/exp_S10_match_radius.json` 档位 = {1, 2, 3, 4}（grep `r_px` 逐行）。两者都不构成「0.5–4.0」全档表。反方核验：路由级 REPORT_route1/REPORT_route3 未出现该字样，仅汇总三处有。
- **建议改法**：改写为实际档位（route1 {0.5,1,2,3,5} × σ_jit 阶梯；route3 {1,2,3,4}）。
- **所属面**：④

### 黄-3 「2.0 px 正确率 ≥0.996」过宽（仅零抖动档成立）
- **文件:行**：`REPORT_paper.md:147`（`redo_summary.json:25` 的 0.996 同源未注档位）
- **证据**：`route1/exp2 → S8_match_radius` 的 r=2.0 行：σ_jit=0 → 0.9963；σ_jit=0.2 → **0.99591**；σ_jit=0.5 → **0.9957**（均 <0.996），更高抖动档更低。反方核验：「最优档」结论在各抖动档内部成立（r=2 同档最高），歧义率 ≈ r² 理论与 `S8_false_match_theory`（r=2 → 0.00418 vs 实测 0.0037–0.0041）吻合；过宽的只是 ≥0.996 这个下界。
- **建议改法**：改为「零抖动 0.9963；σ_jit≥0.2 时 0.9957–0.9959」，或去掉 ≥0.996 下界。
- **所属面**：①科学性（陈述过宽）、④

### 黄-4 c* = 4.6850649 的实验腿把 route3/exp_S01 记作同值
- **文件:行**：`REPORT_paper.md:126`（同表述见 `docs/derivation_robust_weights.md:54`「两套求积互证到 7 位」）
- **证据**：`route3/exp_S01_tukey_c.json → numeric_root_c_star = 4.685317926884681`，与 4.6850649 只到**第 4 位有效数字**一致；7 位互证仅存在于 `route2/exp1` 内部（c*=4.6850649 ≡ statsmodels 4.685065，refs.md V2 同）。反方核验：S01 作独立近似复核方向无误（差 2.5e-4），核心三腿（≡ statsmodels 六位）不受影响。
- **建议改法**：实验腿改记 route2/exp1 为主，S01 注明「独立复核到 ~4 位」。
- **所属面**：④

### 黄-5 「负例 ψ=x 偏差 1.4e-12」无 results 腿
- **文件:行**：`REPORT_experiment.md:56`；`docs/derivation_robust_weights.md:54`
- **证据**：全 `results/` grep `1.4e-12` 无命中；`route2/exp1_robust_constants.json` 实际记录 `identity_deviation_from_1 = 3.7858e-11`、`eff_identity_psi_x = 0.9999999999621417`；1.4e-12 只在叙述性 `code/redo/route2/REPORT_route2.md:36`。反方核验：负例本身（ψ=x ⇒ 效率=1）成立，错的是把叙述层数字挂在 exp1 JSON 名下。
- **建议改法**：改引 JSON 实测 3.79e-11，或标注 1.4e-12 出自 route2 报告。
- **所属面**：④

### 黄-6 README §4.7 的 step8 三个耗时数与当前归档 JSON 不符
- **文件:行**：`README.md:274`（引导 2.093 s、0.004823 s·星⁻¹）、`README.md:275`（盲检 `1.751 s`）、`README.md:445`（R2 行「已修：现为 1.751 s（归档 JSON）」）
- **证据**：`results/step8_real_frame.json → guided_vs_blind_real` 实开：`guided_wall_time_s = 2.0716970049979864`、`guided_sec_per_fit = 0.0047734954032211665`、`blind_wall_time_s = 3.187498871004209`。README 三数对应 `results/REVIEW.md:451` 记录的**旧档值**（2.09298/0.0048225/1.75113）；当前 JSON 盲检耗时 3.1875 s（差 82%）。反方核验：该表结论性数字（13163、434、196、1.49%、827/827、σ_obs 0.026520、0.0068″）与当前 JSON 全部一致，受累仅三个墙钟数字与 445 行的「归档 JSON」断言。
- **建议改法**：按当前 JSON 重刷三数，或注明耗时随运行波动并修正 445 行断言。
- **所属面**：④

### 黄-7 「n=3 MAD 偏差 1.49×」的锚指错 S11
- **文件:行**：`REPORT_paper.md:104`、`REPORT_experiment.md:71`、`results/redo_summary.json:19`（均记 [route3/exp_S11]）
- **证据**：`route3/exp_S11_zp_sample_floor.json` 实开为 n=3 SE/n=2338 = 25.7154、`ratio_exact/asym = 0.9256987` 与 criterion 文本，**无 1.49 字段**。1.49 实际在：`route3/exp_S02 → n3_deficit_factor = 1.4918`、`route2/exp1 → n=3 bias 1.4884`、`route2/exp6 → 1.495`、`route1/exp1 → S13_mad_finite_sample`（E_S/σ n=3=0.6766 → 1.478，`REPORT_route1.md:294`）。反方核验：同句的 25.7×（及 7.4%，见黄-8）确在 S11，故是 1.49 这一项挂错、非整句错。
- **建议改法**：1.49× 单独改挂 `exp_S02`（或注明分挂两锚）。
- **所属面**：④

### 黄-8 「高估 7.4%」与 S11 JSON「高估 8%」并存但未注分母
- **文件:行**：`REPORT_paper.md:104`、`REPORT_experiment.md:71`（7.4%）vs `results/redo/route3/exp_S11_zp_sample_floor.json → criterion`（「高估 8%」）
- **证据**：S11 `ratio_exact_over_asym = 0.9256987`：1−0.9257 = 7.43%（以渐近式为分母，即报告/台账口径）；(1/0.9257−1) = 8.03%（以精确值为分母，即 JSON 文本口径）。同一比值两个分母，非数值冲突。反方核验：分歧台账终裁口径为 7.4%，本条**不改台账值**，仅指出同仓两文两数无分母注记。
- **建议改法**：报告处补一句分母口径（「以渐近式为分母 7.4%；以精确值为分母 8.0%」），或在 JSON criterion 旁标分母。
- **所属面**：①科学性（口径注记）

### 黄-9 n=5 σ_floor = −0.004911 的证据锚指错 step5
- **文件:行**：`REPORT_paper.md:139`、`REPORT_experiment.md:41`（均记 [实验:code/step5_calibration_gate.py]）
- **证据**：该值只在 `results/step7_negatives.json`（N5 行 `sigma_floor = -0.004911249342677107`）与 `code/step7_negatives.py`；`step5_calibration_gate.json` 无 n=5 案例。同单元 `DOC_CORRECTIONS.md:16` 与 `README.md:253` 都正确引 step7（C1）。反方核验：「n≲22 下界失效」结论另有 step7 finding 与 `PHOTOMETRY.md:403` 支撑。
- **建议改法**：两处改引 `code/step7_negatives.py / results/step7_negatives.json`。
- **所属面**：④

### 黄-10 REPORT_route1 的 H1 锚处置建议仍写「:355」（单元内新旧两说）
- **文件:行**：`code/redo/route1/REPORT_route1.md:321`（「:126 内容已漂移至 :227/:355」「05 §0.3 锚改为 PHOTOMETRY.md §16.1 ⑥/:355」）
- **证据**：`docs/science/PHOTOMETRY.md:356` 实开即 ⑥行（星等坐标系表达）；`REPORT_experiment.md:53` 已带 `<!-- 订正: 检查-跨文档冲突 黄9——原 :355，⑥ 行实为 :356 -->`。同表 `:323`（H3 行）又写「:356 命中」——同一张表内 :355/:356 并存。反方核验：`REPORT_paper.md:132` 与 redo_summary 的 A-P1-04/09 表述（正本 :13/:15-17）不受影响。
- **建议改法**：REPORT_route1:321 按 :356 订正或注明漂移。
- **所属面**：②行文逻辑（订正注新旧冲突）

### 黄-11 S15 的引注挂在 exp4/exp5（实际在 exp1）
- **文件:行**：`REPORT_experiment.md:53`（「S15：ZP_syn 下限 3 的有限样本偏差量化 …[实验:route1/exp4, exp5]」）
- **证据**：`route1/exp4_spatial_gain.json` 键 = {S12_order_sweep, S12_negative_flat_field, S9_amplitude_bound, S12b_gate_statistics}；`route1/exp5_integration_gates.json` 键 = {S14_*, S16_*, S17_*}——均无 S15。S15 量化在 `route1/exp1_robust_constants.json → S13_mad_finite_sample`（`REPORT_route1.md:29` 明记「S15 … ✅ exp1」）。反方核验：同行 S12→exp4、S16/S17→exp5 引注正确，仅 S15 漏 exp1。
- **建议改法**：行内为 S15 补 `[实验:route1/exp1]`。
- **所属面**：④

### 黄-12 求解器豁免的证据链未回指「负例未触发」披露
- **文件:行**：`REPORT_paper.md:65`、`REPORT_experiment.md:88`（豁免清单「敏度实验 1e-2→1e-12 不敏感、50 步不可达 [route2/exp1]」）
- **证据**：`route2/exp1_robust_constants.json → S3_summary`：`negative_control_fired = false`（设计负例「污染场下松容差应可改变结果（能红）」未触发）；`REPORT_route2.md:61` 自身如实登记「结果（诚实报告——负例未触发）」并在 `:68` 给出原因（固定尺度 IRLS 线性收敛快）。主报告只引不变性结果、未回指该披露。反方核验：豁免结论属台账已终裁（求解器设置豁免），本条**不重开豁免**，只是披露链不完整。
- **建议改法**：豁免清单理由栏补一句「route2 S3 设计负例未触发（REPORT_route2 §S3），豁免依据为不变性而非负例变红」。
- **所属面**：①科学性（非退化证据链披露）、②行文逻辑

---

## 三、绿级（可选）

### 绿-1 未启用路径的 degraded_reason 字符串只在脚本层、未落盘结果 JSON
- **文件:行**：`README.md:206`（`degraded_reason = "apply_photometry_disabled"`）
- **证据**：该串在 `code/step6_apply_and_units.py:87` 存在（代码腿有），但 `results/step6_apply_and_units.json` 只落了下游串 `degraded_reason_on_disabled_path = "fail-closed: 产品未归一化到测光星等坐标系，禁止继续下游"`，JSON 内无该字段。反方核验：语义与 `PHOTOMETRY.md:355`（degraded_reason + fail-closed）一致，非死键。
- **建议改法**：可选——step6 JSON 同时落盘未启用路径的 degraded_reason，README 注明两层串来源。
- **所属面**：④

---

## 四、已查无问题面（逐面说明）

### ①科学性（除黄-3、黄-8、黄-12 外已查无问题）
- 常数体系三腿逐条核对：`c=4.685`（refs V1 NIST 全文原句 + V2 statsmodels 4.685065 + route2 exp1 c*=4.6850649/ARE 0.9499974）、`0.6744897501960817 = Φ⁻¹(3/4)`（route2 exp1 相对差 1.6e-16、route3 S02 一致）、`1.482602218505602`（D3 倒数恒等式；源码 `frame_photometry_fit.cpp:292` 四位截断与 `NOISE_ESTIMATION.md:213` 冻结条款实开核对，A-P1-08 登记一致）、`1.166=√1.361`（refs V5 Crossref 书目 + Table 2 n=∞ 1.361；A-P1-01/02 口径在四份报告中一致）——公式、单位、量纲无误。
- 判据形态逐字核对：`06_photometry.md §4.1 :38-44` 与 `REPORT_paper §2.3` 的 σ_obs/σ_floor/σ_ceiling/PASS 四式逐字一致；F_syn = ∫F_λ·T·Q·λ dλ、343 点 @2 nm、336–1020 nm、无 `10^(−0.4·G)` 因子，与 `PHOTOMETRY.md:75/:133/:182`、`05_正向规格 A-5 禁止行` 一致。
- 非退化负例：step7 N0–N4 的期望/实测/通过标记逐条与 README §4.6、`REPORT_paper:138`、GATES G7 一致（N0 归零判红、N1 单调 3.52×、N2 Poisson 2.91× 且 4× 判红、N3 颜色项 7.0× 判别幅、N4 跨帧门 0.19%）；N5 未过如实记录（5/6、注入-响应 3/3）。
- 适用域：n≲22 下界失效、n=5 floor=−0.004911、`PHOTOMETRY.md:403` 与 step7 finding 三方一致；D-01…D-11 与 A-P1-01/02/04/06/08/09/12/13 终裁在本单元均为「按台账落实」表述，无重开迹象。

### ②行文逻辑（除红-3、黄-10、黄-12 外已查无问题）
- UNRESOLVED 泄漏检查：`REPORT_paper.md` 全文 grep `UNRESOLVED` 仅命中第 4 行「UNRESOLVED 项不进正文」声明；开放项只出现在 §7 诚实边界 1–8 条（含 Croux&Rousseeuw 待补脚注 `:201`、Lindegren 开放 `:182`）与 `REPORT_experiment §8`（自述「唯一登记面」）——论文主结论（§5 与 REPORT_experiment §5 结论 1–5）无 UNRESOLVED 泄漏。
- 订正注记新旧两说：全单元 `<!-- 订正` 注记逐一实开（REPORT_experiment:53、`PHOTOMETRY.md:295/:403`、`05_正向规格:265/:278`），除黄-10 一处外均为「旧文划线 + 终裁新说」的合规写法。
- 三份主文档互指：REPORT_paper §5.1–5.5、REPORT_experiment §4/§5、README §4/§5 的同数量（σ_obs 四值、obs/pred 三值、0.9859/0.2254/0.1972、13163/1.49%/30.3×、ZP_syn 28.9358/k 1.00156/σ_res 0.0493/0.00138、chain 3.7053e6/1.44e-3）逐值互洽，报告-报告层无矛盾。
- 自包含性：假说/方法/三类数据/结果/结论/诚实边界/复现命令（REPORT_paper §6、REPORT_experiment §9、README §7）/佐证来源（refs.md V1–V12 + 待补脚注）齐备；seed 纪律（20260921 历史轮、20260926 重做、route1 派生 +1…+4 → 20260927/28/29/30）三文一致且与各 JSON 头部相符。

### ③跨文档冲突（除红-1、红-2、红-3 外已查无问题）
- 与 `docs/science/PHOTOMETRY.md`：行锚 :13/:15-17（ivar′ 量纲变换、单帧门）、:126（49 帧 0.42720→0.04505）、:227（绝对归一化不可辨识）、:356（⑥行）、:360（项目约定豁免清单）、§10（c/tol/max_iter/mag_tolerance 不可接受变化）、§14 D-06 订正注（PMC6768164 锚有效）逐行实开核对，与 `REPORT_paper:30/:61/:76/:132/:180` 全部相符。
- 与 `独立审计/08_修复包/①测光星等坐标系/`：`05_正向规格.md:265-266` 已按 A-P1-05/A-P1-12 钉死条件钳位与缓冲/钳位优先语义（与 `REPORT_paper:145`、redo_summary:15 口径一致）；`01_缺陷清单.md` 的 B13/C4/C9 定性与 route2 S13/S15、REPORT_route1 S10/S15 核验结论一致；`02_已确立…:158/:183` 的 1.4826 全精度与 V-2 自证和本单元 D3 一致。
- 与 PSF 文档：`PSF_SIGNAL_WEIGHT.md:25/:145`（Var(F̂)=1/W，Horne/Naylor 口径）与 README §4.1 对「精确 Fisher 含自由背景简并项」的口径差异说明（`README:142-147`，含 C1 撤回记录）自洽，未构成两说。
- 与最高设计/SCI-PHOT 禁令：`PHOTOMETRY.md:14`（k_photo 绝对值无物理意义、禁物理闭合式）与 step6 `unit_elimination.note`、`REPORT_paper:29-30`、`README:93-94`（明列不使用项）一致；`PHOT-GATE-DROP-001`（跨帧门禁禁用）在 `README:93/165`、`REPORT_paper:80`、step7 N4 三处一致。
- 备注：`06_photometry.md:46` 的 1.166 旧标签与 `PHOTOMETRY.md:403` 并存问题，已由兄弟切片 S7 报告（S7-红-01），本面不重复计数（见文末附注）。

### ④幻觉与锚（除红-1、红-2 与黄-1…黄-7、黄-9、黄-11、绿-1 外已查无问题）
- 文献抽验 ≥5 条（web/DOI 实开）：V1 Kafadar 1983 DOI 10.6028/jres.088.006 → NIST 全文 PDF（题名/作者/J. Res. 88(2)/1983 相符）；V5 Rousseeuw&Croux 1993 DOI → Crossref 题名 `Alternatives to the Median Absolute Deviation` 相符；V4 Holland&Welsch DOI → Crossref 题名相符；V11 Aitken DOI → Crossref 题名相符；V6 arXiv:2208.00211 → arXiv 实开为 Gaia DR3 Summary，且旧号 2205.11321 实开为 `Fluke 8588A and Keysight 3458A DMM Sampling Performance`（自纠属实）；V7/V8 DOI → Crossref 卷页 A&A 674 **A33**（Synthetic photometry，…202243709）/ 674 **A3**（External calibration，…202243880）与 refs.md 完全一致；README §8.1 的 A3/A33 用法亦一致。
- 源码/文档行锚实开：`frame_photometry_fit.cpp:292`（1.4826*zmad）、`:276`（zp_vals≥kMinFitStars）、`:332`（n_matched<kMinFitStars → rc=−6）、`NOISE_ESTIMATION.md:213`（MAD→σ 常数冻结行）、`06_photometry.md` 结构（§1–§8、无 §2.1/§3.1，`README:89-90` 断言成立）、`PHOTOMETRY.md:13/15-17/126/227/356`——均与引用相符。
- 数字-结果逐值核对通过（抽样）：step1（location 16.336461198922965/rtol 0/k 3.33e-15、oversample 1.64e-4→2.04e-4、求积 8.49e-5/2.89e-4、空间 0.0372→0.000365、离群 13.7%/9.24e-4、噪声阶梯 0.013352/0.013603/0.019149/0.034587、Fisher 2.149/1.731/1.335/1.132）；step3（中位 −0.147/−0.263/0.080、逐星 MAD 0.491/0.645/0.213、色斜率 0.464/0.765/0.565、干净子样 0.255/0.104/0.049、跨滤镜 0.117、面积 44425.5/45322.9/38453.1、F657N/F502N 偏移）；step4（0.9859/0.2254/0.1972/+0.7606/+0.7887、19/16/3、19/14/5、每匹配星 0.00718/0.00792/0.00398/0.00993、3px→0.0141、阈值扫描 0.239/0.197）；step5（σ_obs 0.045344/0.057457/0.051718、floor/ceiling、obs/pred 1.009/0.727/0.390、k 2.09/1.85/3.95%、σ_pix 18.40、σ_psfsys 0.0250、σ_color 0.00574、σ_flat 0.0197、k_ratio 1.60986/0.9981、KS D 0.1227 p 0.787）；step6（逐像素 0.0、drizzle 2.060596e-17 vs 2.060581e-17、flux_ratio 2.053358e-17、MAD 0.04534→0.01973、退化族 4831.2/4865.1/4840.2 与 0.01891/0.01812/0.01344、Δlocation 0.8633228601、k 比 0.1369863014）；step7（N0–N4 全套）；step8（827/827、−0.263/−0.319 px=0.0068″、1202(871/115/216)、13.63″、1248、434 全拟合、13163、196、1.49%、n=157/151、σ_obs 0.026520∈[0.006008, 0.032456]、σ_pix 40.94、σ_psfsys 0.01367、σ_flat 0.01956、σ_color/σ_gaia=null）；route1 exp1/exp2/exp3/exp4/exp5、route2 exp1/exp6/exp8、route3 S00–S12 其余读数（窗口平移 0/+0.015299/−0.031794/−0.014441、预过滤增益 ≈0、FOV ×3.0187/×0.6923、B12 −0.146、tol 跨度 1.93e-4/17 步、S9/S14/S16/S17、m_cut 16.728548、指纹 1.83e-16/8.42e-15/8.49e-9、ZP 28.935825/k 1.0015617/σ_res 0.04932/σ_kappa 0.0013818/m5 27.1867、ARE 0.9499974/c* 4.6850649/1.496e-6、identity 3.79e-11、25.7154/0.9256987、S00 15/15、S01 4.6853179）逐值与文档相符（除本报告已列各条）。
- `results/GATES.md`/`gates.json`：8/9 PASS、G6 PARTIAL（N5 fail 如实登记）与 README §5、REPORT_experiment 判读一致；`redo_summary.json` 的 superseded_numbers 三条（2205.11321→2208.00211、:126/:400→:13/:15-17、幻觉锚降格为行号漂移）与台账 A-P1-04/06/09 一致。

---

## 附注：与兄弟切片重叠、不重复计数
- `docs/plugins/algorithms_phase1/06_photometry.md:46` 仍写「1.166 = SD(MAD)/MAD 正态渐近常数」，而台账 A-P1-01 终裁「订正解释文字」，`PHOTOMETRY.md:403` 已订正——本单元 `REPORT_paper.md:76` 恰把该文 §4.1 列为「逐字来源」，两说并存影响本单元引证面。该问题已由 S7 插件切片以 **S7-红-01** 上报（本面复核结论与其一致：行原文、PHOTOMETRY 正本、台账裁决三方证据齐全），故本报告不重复计数；请总编合并时归到 S7 红级。
