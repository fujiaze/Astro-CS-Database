# 检查-S14 · 实验单元 P2 absolute-snr 全量切片（全仓-01）

> 检查范围：`实验/absolute-snr/` 全部四面（①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚点）。
> 首读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表，已修复项不重复报；科学争议以分歧台账 D-01…D-11 终裁与 §2 A-* 裁决为界。
> 计数：**红 2 / 黄 8 / 绿 2**。

---

## 红级（2）

### R1（红）｜面① + 面④
- **文件:行**：`实验/absolute-snr/docs/DERIVATIONS_P2.md:27`；`实验/absolute-snr/results/AUDIT_KEY_RESULTS.json:110`
- **问题**：两处写「9216→5816.6（= (1.152×1.25/0.015)² 精确复算）」，但该式 = (1.44/0.015)² = **9216**；5816.6 的正确闭式是 (1.144/0.015)² = 5816.6044。两处均自称「精确复算」，属把 9216 的式子标成 5816.6 的出处。
- **证据（含反方核验）**：
  - 算术：(1.152×1.25) = 1.44，(1.44/0.015)² = 96² = 9216；(1.144/0.015)² = 76.2667² = 5816.6044。
  - 本单元自产证据即反证：`code/audit/results/route1/exp03_sky_budget_constant.json` 的 `budget_arithmetic` 三键并列——`"(1.44/0.015)^2": 9216.0`、`"(1.152*1.25/0.015)^2": 9216.0`、`"(1.144/0.015)^2": 5816.6044`、`"doc_claim": 5811`；脚本头注 `code/audit/route1/exp03_sky_budget_constant.py:5` 亦写「(1.144/0.015)^2 = 5811/5816 branch」。
  - 反方核验：同式同错存在于分歧台账 D-04（域外，不计本切片计数，需上呈）；`REPORT_paper.md:61`「9216→5816.6（非 5811）」不带括号故无此错——错误仅在上列两文件。
  - 影响面：5811→5816.6 的 D-04 终裁本身不受影响（裁决正确，错在括号引式）。
- **建议改法**：两处括号改为 (1.144/0.015)²；台账 D-04 同句作为域外同源错误一并上呈。
- **所属面**：①科学性（引式与数值不符）、④锚点（「精确复算」所指的复算不存在于被引式）。

### R2（红）｜面③ + 面④
- **文件:行**：`实验/absolute-snr/docs/snr-propagation-design.md:85`（记号表「F_ref 组内公共」）、`:312`（跨帧可比「需公共 F_ref」）、`:319`（硬约束2「F_ref 必须是组内公共常数，否则配对定理失效」，引 `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:34-41`）；对照 `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:34-45`、`实验/absolute-snr/docs/frame-snr-canon.md:133-134/:453/:474`、`code/reverse_verify/frame_snr/README.md:17`（C2 判据）、`实验/absolute-snr/REPORT_paper.md:33/:43`。
- **问题**：同一量 F_ref 在本单元内存在**互斥的三方口径**，且设计文档的引锚恰好反证自己：
  1. **引锚倒挂**：`snr-propagation-design.md:319` 以 `weight_chain.h:34-41` 作「F_ref 必须组内公共」的依据；实开该锚，头文件原文是「⚠ 本节曾写『F_ref 为组内公共常数…逐帧 F_ref 不得使用』—— **该表述已作废**（WEIGHT-FREF-PERFRAME-001，scope="frame_independent_fixed_magnitude"），配对性**只要求同一帧内** SNR 与 F_ref 同源…**不要求跨帧相等**」。
  2. **数学上不成立**：w = SNR²/F_ref² = (a·F_ref/σ_F)²/F_ref² = a²/σ_F²，F_ref 约掉——权重配对定理与跨帧是否公共无关；需要公共 F_ref 的只有「SNR 数值跨帧可比」这一条（设计文档 :312 表内语境），:319 把它接到「配对定理失效」上是错位。
  3. **单元内三方互斥**：`frame-snr-canon.md:133-134` 定案「F_ref 必须组内公共（同一 output_dir 所有帧同一值）」并把生产逐帧 F_ref 判为 **G11 高危缺陷**（:453/:474，13/13 多帧产品不满足，实测跨度 2.14×）；`code/reverse_verify/frame_snr/README.md:17` 的 C2 检查项仍用该旧判据；而主论文 `REPORT_paper.md:43` 与生产合同采**逐帧** F_ref,k = 10^(−0.4(m_ref−ZP_k))（:33 主张「换算权逐帧比值对 m_ref 与 ZP 完全不变——跨帧可用、不依赖参考帧」）——按 canon 判定，主论文口径即「高危缺陷」。
- **证据（含反方核验）**：三方原文均已逐行实开（上列行号）；`frame-snr-canon.md` 的 G11 实测数据（t3_m4_red 六帧 3140.9/3292.1/3224.5/1728.8/3274.5/3700.7）本身无误，冲突在「这是缺陷还是合法」的判定；`weight_chain.h:34-45` 注释引「负责人 GAP_AUDIT §9.49 定案 2『帧间独立』」，若该裁决在先则 canon G11 判定已过时——但 canon/README/设计文档均未登记 WEIGHT-FREF-PERFRAME-001 变更，反向亦无「组内公共」仍有效的裁决记录。上轮三份检查报告未覆盖本单元（grep 无 snr-propagation/frame-snr-canon 命中，除已 PASS 的 D_p² 两行外）。
- **建议改法**：本条为两权威口径打架（主论文+生产合同 vs 定案文档+设计文档），**上呈负责人裁决现行口径**；裁决后同步：frame-snr-canon G11/C2、设计文档 :85/:312/:319、并把 :319 的引锚改指 `weight_chain.h:34-37` 的真实论点（同帧同源）。检查员不擅自给改值。
- **所属面**：③跨文档（五权威方冲突）、④锚点（引锚反证）。

---

## 黄级（8）

### Y1（黄）｜面② + 面③
- **文件:行**：`REPORT_paper.md:65`、`REPORT_experiment.md:73`、`results/AUDIT_KEY_RESULTS.json:121/:132`、`docs/LEDGER_CORRECTIONS_P2.md:11`（同源域外：`独立审计/08_修复包/②跨帧绝对信噪比/05_正向规格.md:118` 订正注同挂）。
- **问题**：四处把 **A-P2-05/06** 与 **+1.91×10⁻⁶** 挂到 **P-CST-11 双计偏差**；台账原文两者与双计偏差无关——A-P2-05 =「P-CST-02（−0.4）是 P-CST-01 的纯数学导出，不应单列三腿齐备」（分歧台账.md:128）；A-P2-06 =「**P-CST-05**（Moffat4 FWHM/σ）的 '+1.91e-6' 是闭式−登记差（复算 +1.908e-6 精确一致）」（:129）。
- **证据（含反方核验）**：
  - 数字对不上双计：闭式 0.14500914320209946（route3/exp05 archived_reference_values）vs 登记 0.145009 ⇒ 相对差 **9.87×10⁻⁷**，非 1.91×10⁻⁶；
  - `route2/exp06_doublecount_bias.json` 全文**无 1.91e-6 字段**，其 `x_implied` 反解法（x_implied=0.5577…）使 `closed` 精确等于 `registered`（差≈0），根本不产生「闭式−登记」数；
  - route3/exp05 的 2.35×10⁻¹⁴ 是闭式↔闭式复算差，不是登记差；
  - 数字对得上 Moffat：(1.230310−1.2303076525901024)/1.23031 = **1.908×10⁻⁶**，且 `AUDIT_KEY_RESULTS.json:40` 自记 P-CST-05 = Moffat4 rel +1.908e-6；
  - 反方核验：05 规格 :118 订正注也写「A-P2-05/06」，本单元系同源继承（域外同错上呈），但 `+1.91e-6 = 闭式−登记值之差 source route2/exp06`（KEY_RESULTS:132）这一具体挂接为本单元新增且三重不符。
  - 影响面：不推翻「双计闭式逐位复现（2.35e-14）」主结论，错在编号与数字的对象。
- **建议改法**：四处将 +1.91e-6/(A-P2-06) 改挂 P-CST-05（Moffat 手抄截断），A-P2-05 从 P-CST-11 议题移除；05 规格 :118 的同款订正注一并上呈核对。
- **所属面**：②行文逻辑（订正注编号错挂）、③跨文档（与分歧台账原文冲突）。

### Y2（黄）｜面① + 面②
- **文件:行**：`REPORT_paper.md:61`、`REPORT_experiment.md:60`、`results/AUDIT_KEY_RESULTS.json:98`。
- **问题**：c_eff 一句三个数互相不自洽：①「与 1.152×1.2533=1.444 相差 **+2.4%**」——(1.4751−1.444)/1.444 = **+2.15%**，+2.4% 是对 1.44 的差，同句两个基准混用；②「≈**1.1σ**」——(1.4751−1.444)/0.023 = **1.35σ**、对 1.44 = 1.53σ，两个基准都不是 1.1σ；③ `AUDIT_KEY_RESULTS:98` 更写「理论链 1.152×1.2533=1.444 **恰为实测中心**」——实测 value=1.4751，1.444 距中心 1.35σ，「恰为中心」不成立。
- **证据（含反方核验）**：±0.023 本身合法（route1/exp03 的 c_eff 由 Mf=2000 帧池化，SE = c/√(2·2000) = 0.0233，与所记 0.023 相符）；两遍独立复算 1.35σ/1.53σ/2.15%/2.4% 结果相同；三处同源同错，前两处文句一致。
- **建议改法**：统一比较基准（对 1.44 或对 1.444），σ 数值按所选基准重给（或注明 1.1σ 的另一不确定度来源）；「恰为实测中心」改「落在实测 ±1.4σ 带内的带内点」。
- **所属面**：①科学性（σ 不可复算）、②行文逻辑（一句两基准）。

### Y3（黄）｜面① + 面④
- **文件:行**：`REPORT_paper.md:61`、`REPORT_experiment.md:60`、`README.md:39`、`results/AUDIT_KEY_RESULTS.json`（budget 相关条）；分歧台账 D-04 同现（域外）。
- **问题**：**N_min≈9321** 标注来源 `code/audit/route3/exp01_mad_sigma_budget.py`，但该实验的 JSON（`route3/exp01_mad_sigma_budget.json`：`c_median_of_patch_vars` n9216=1.4488817、`implied_n_sky_at_eps0015` 三值 9264.93/9136.70/9207.10、c_known=1.16639、pooled 1.148/1.165/1.168）与全部 `code/**/*.py` **均无 9321**（grep 0 命中）。按其引数复算：(1.449/0.015)² = 9331.6、(1.4488817/0.015)² = 9330.0——9321 与两者差约 0.1%，不可复现。
- **证据（含反方核验）**：全仓 grep `9321` 仅命中文档四处；结论「与 9216 相差 1.2% 阈值自洽」在 9330 下为 1.23%，方向与结论不变（故不升红）。
- **建议改法**：补出 9321 的具体推导（若取了别的 c 或 SE，注明），或将读数统一为 9330/9332 并同步四处。
- **所属面**：①科学性（读数不可复算）、④锚点（引实验文件无此数）。

### Y4（黄）｜面④
- **文件:行（成组，逐条实开核对）**：
  | # | 引用处 | 声称内容 | 实况 |
  |---|---|---|---|
  | a | `snr-propagation-design.md:128` | CALIBRATION.md:65-75 = 生产校准式（dark_opt 方程） | :65-75 是标度一致性门/3a/4；dark_opt 方程在 **:90-95** |
  | b | `snr-propagation-design.md:166` | NOISE_MODEL.md:51-63 含「variance=max(σ_bg²,1e-12)；ivar=1/variance」 | :62 有 σ_bg 公式✓；floor/ivar 实义在 **:30/:289（variance_floor 配置默认 1e-12）与 :73**，字面式不在 51-63 |
  | c | `snr-propagation-design.md:178` | NOISE_MODEL.md:134,139 =「生产唯一基线 empirical MAD(source==0)」 | :134 是噪声项登记面、:139 空行；该句实际在 **:327/:332** |
  | d | `snr-propagation-design.md:179` | NOISE_MODEL.md:66-69 =「gain 模型仅诊断不入生产」 | :66-69 是 §5 几何退化/全局兜底行；诊断块在 **:79-82** |
  | e | `snr-propagation-design.md:182-183` | NOISE_MODEL.md:183（SExtractor 不等价）/:202（PixInsight 禁互换） | :183 空行、:202 秩论述；实际在 **:377 / :396** |
  | f | `snr-propagation-design.md:197` | NOISE_MODEL.md:93 = 实测 6.93%/1.11%/14.8%→0 | :93 空行；实际在 **:108** |
  | g | `snr-propagation-design.md:316-317` | star_detector.cpp:41-67「噪声估计按框架公式」、:63「全帧裁剪中位数」 | :41-67 是 STATUS/回归锁定注释，估计式在 **:68 起**；:63 是注释行 |
  | h | `results/DOC_CORRECTIONS.md:15-16` | module_adapters.cpp:4258/:3944-3945 = D1 双计调用点 | :4258=median_fwhm_y_px、:3944-3945=catalog_guided 背景；EMPIRICAL_TOTAL_RMS 声明实际在 **:7159** |
  | i | `results/DOC_CORRECTIONS.md:106` | NOISE_MODEL.md:86 =「1.44/√N 预算行」 | :86 是「5a 掩膜半径物理导出」标题；预算推导在 **:99-100** |
- **证据（含反方核验）**：逐条 `awk`/`grep` 实开被引文件行区间比对；**被引内容全部存在，仅行号/区间漂移**（非幻觉）；上轮 PASS 表的行漂移 6 处清单不含本组（其面为 upm/sampler/drizzle/noise_model/sdet_api），三份旧检查报告对本组文件零命中（grep `snr-propagation` 仅 D_p² 已 PASS 条）。
- **建议改法**：按现行行号批量重钉（或引改到章节号防再次漂移）；h 涉 D1 闭环叙述，行号更正后「声明在 :7159」应保留以支撑闭环。
- **所属面**：④幻觉与锚点（file:line 锚失效组）。

### Y5（黄）｜面③ + 面④
- **文件:行**：`REPORT_paper.md:65`（挂 `[实验:code/b2_noise_terms.py]`）、`results/AUDIT_KEY_RESULTS.json:139-141`（`source: "unit_b/b2_noise_terms.json"`）。
- **问题**：偏置三档值「+12.8%（基准）~ +34.0%（RN=50）~ +36.6%（B=0 暗源）」单挂 b2，但 **b2_noise_terms.json 全文无 0.366**（实含 0.12879/0.34308/0.34398，即 12.9%/34.3%/34.4%）；**+36.6% 实为 `results/b1_sky_scan.json:1120-1121` 的 `G4c_empirical_rn_rel_dev_at_B0 = 0.366199`**（B=0 处、字段语义吻合）；+34.0% 与 `results/ctest_evidence.md:54` 对拍 `G4c_max_bias_over_all_points = 0.33954` 吻合，而 b2 自身最大是 34.4%。
- **证据（含反方核验）**：b2/b1/ctest 三处 grep 与 python 解析实测；`results/REVIEW.md:231` 记有历史审稿建议「'+34.0%'→'+36.6%'」，说明 36.6 是按审稿意见改入正文的，但 source 未随之补 b1；论文与 JSON 的 source 同错。
- **建议改法**：source 补 `results/b1_sky_scan.json`；区分 34.0（ctest 对拍）与 34.4（b2 实测），三档值各自挂源。
- **所属面**：③跨文档（报告↔结果 JSON）、④锚点（证据文件错挂）。

### Y6（黄）｜面② + 面③
- **文件:行**：`results/REVERSE_VERIFY_CANON.md:35` vs `docs/snr-propagation-design.md:53-56,62`（§13.7 :1472 同）。
- **问题**：CANON 写「`σ_MAD/σ_clippedRMS = 1.3127` 是比值，σ 相对误差 31.27%（方差高 76.8%、**SNR 低 23.8%**）」并置于「当前生产链成品 σ 相对误差」句中；而设计文档复核块**明删**了这一归因（:56「原『⇒ 帧级 SNR 偏低 23.8%』的归因已删」，:53-56 指出 1.3127 是**两个噪声生产者互相矛盾**的比值（整帧未裁剪 MAD vs frame_snr 裁剪 RMS），不是 frame_snr 对真值的偏差，真实电平偏差为 EXP-05 表 A「偏低 15%~77%」）。CANON 未带该限定，保留了设计文档已作废的推论。
- **证据（含反方核验）**：两文原文实开对照；1−1/1.3127 = 23.8% 算术无误，问题在**归因对象**（把生产者间比值的倒数直接称成品 σ 的 SNR 偏差）。
- **建议改法**：CANON 删「SNR 低 23.8%」或加「系两生产者比值、非对真值偏差；frame_snr 真值偏差另见 EXP-05 表 A」限定，与 §13.7 对齐。
- **所属面**：②行文逻辑（同一文件包内两份文档结论互斥）、③跨文档。

### Y7（黄）｜面② + 面③
- **文件:行**：`REPORT_paper.md:91`（§4.8 末句）vs `REPORT_experiment.md:110`（§7.9）；连带 `docs/LEDGER_CORRECTIONS_P2.md:22`。
- **问题**：论文写「SNR 路径 fail-closed 状态语义**经转写自检与静默降级注入判红验证**」，未带实验报告 §7.9 的范围声明「**lib/ 实现侧无对应符号，不构成对实现的验证**」；而 LEDGER:22 声称「历史正本失败状态（I2）在**论文与 README** 均按实际实现范围降级」——实况：README.md 速览 43 行无 fail-closed 条目，论文正文亦未写实现侧无对应符号，降级声明与两文实况不完全对应。同文件包内两份报告的诚实边界不对称。
- **证据（含反方核验）**：两文实开对照；反方：论文措辞已用「转写自检」限定、非裸称「实现已验证」，故不升红；`results/REVIEW.md:226` 历史审稿同样建议降级，实验报告已落、论文只落了「转写自检」四字。
- **建议改法**：论文 §4.8 括注「（转写自检；lib/ 实现侧无对应符号）」；LEDGER:22 表述改为与两文实况一致。
- **所属面**：②行文逻辑、③跨文档（双报告诚实边界一致性）。

### Y8（黄）｜面③
- **文件:行**：`docs/snr-propagation-design.md:777`（自注引 `docs/science/PHASE2_UPM.md:47-75`）。
- **问题**：写「control_variance = k_corr·(π/2)·σ_bg²/N_retained , **k_corr 冻结 1.4，定义域 1 ≤ k_corr**」，与 D-08 终裁后的正本不一致：`PHASE2_UPM.md:24`「定义域 **1 < k_corr**；公式面 = 两因子 k_gauss(N)×k_geo 几何查表（D-08），代码默认 1.4 为实现记录」、`:43`「k_corr<1 rc=1、**k_corr=1 rc=2 显式拒**」、`:125` 订正注「D-08 原『k_corr = 1.4 保守冻结』…改写如上」。设计文档保留的正是 D-08 已废弃的「冻结 1.4」措辞，且定义域 1≤ 把代码显式拒绝的 k_corr=1 算进域内。
- **证据（含反方核验）**：PHASE2_UPM.md 全文 grep k_corr（:20/:24/:42-68/:85/:113/:125/:183/:288）；「缺省 1.4」（:42）与公式本身仍含 k_corr 属合法现状描述，非法的是「冻结」与「1≤」两处；设计文档全文无 D-08/订正注（grep 0 命中）；上轮三报告未查本文件。
- **建议改法**：随 D-08 同步为「k_corr 缺省 1.4（代码默认/实现记录），公式面 = k_gauss(N)×k_geo 查表（D-08，P3 承载）；定义域 1 < k_corr」。涉及公式定义域，只登记不擅改。
- **所属面**：③跨文档（设计文档 vs docs/science 正本 vs D-08 终裁）。

---

## 绿级（2）

### G1（绿）｜面②
- **文件:行**：`REPORT_paper.md:73`、`results/AUDIT_KEY_RESULTS.json:161`。
- **问题**：「旧启发式 1+0.75ρ̄ 给 12.5%」——字面 (1+0.75×0.19)−1 = 14.25%，12.5% 实为欠估口径 (x−1)/x = 0.1425/1.1425 = 12.47%。数值与 `route1/exp10` 输出 `0.1247264` 相符（**数值正确**），但式与数并列缺一步换算，易被误读为 14.25%。
- **建议改法**：补写「欠估 = 1−1/(1+0.75ρ̄) = 12.5%」。
- **所属面**：②行文逻辑（可读性，非错误）。

### G2（绿）｜面④
- **文件:行**：`results/REVERSE_VERIFY_CANON.md:53`（及 :47-50 的产物锚）。
- **问题**：「实测与退役登记见 `run/ROOT-CONSOLIDATION/logs/migration_rerun.md`」——`run/` 为 gitignore 不入库目录，该文件实测不存在（ls 报不存在）；同文产物锚亦全部指向 `run/`。属登记指针失效，非结论依据。
- **建议改法**：此类「实测与退役登记」锚改指向入库副本（如 results/ 内文件）；同文 :47 已声明 run/ 不入库，两处自洽化。
- **所属面**：④幻觉与锚点。

---

## 已查无问题面（四面覆盖陈述）

### 面① 科学性（常数/公式/单位/域/负例）
- **已核对的数值链（复算或对源 JSON 相符）**：κ_MAD=1.482602218505602、Gauss FWHM/σ=2.3548200450309493、Moffat4 闭式 1.2303076525901024 与截断差 1.908×10⁻⁶、截尾均值 0.7316730952806134（−4.13e-7）、√(π/2)=1.2533141373155001（−2.5e-4）、c=1.1508=1.1507905（route1/exp01，20 万行 MC）、1.152 vs 1.1508 相对差 +0.19%、9216=(1.44/0.015)²、36.31%=(0.19×3)/1.57、23.3% 同族（ρ=0.101→23.25%、M_eff=2.6→23.31%）、12.47%（exp10 输出 0.1247264）、D-07 的 9.53%/±1.5%/1.3–3.2%/偶N +5.0%@N=20、γ=2 恒等 4.4e-16=2ulp、ulp 门规则（A-P2-10）、双计闭式 2.35e-14、SNR_comb²=2.2e-16、Q/W 偏差 (1275.1233−1260.4095)/1260.4095=+1.167%≈+1.17%、b5 的 0.997989 与 0.32463（32.5%）、c_eff SE 0.023=c/√4000——全部与报告一致。
- **负例非退化抽查**：route1/exp03 `negative_sigma0` 与 `negative_constant_patchvar` 均 0.0（非恒真）；route3/exp05 `negative_rn0_identical=true`、`negative_control.max_rel_dev`=0；route2/exp06 `negative_control` bias=0；b1 打乱/算术 null 4.44e-16=0、z 判红 16.36σ；EXP-04 恒真门（帧级臂 RMSE≤K·s_field）明确「无证据资格」登记、EXP-06 三处门退化全部如实登记——未发现把恒真门当证据的用法。
- **D-series/A-series 落地**：D-04（1.152 终裁、5816.6、校准出处仍在诚实边界 :102）、D-07、D-08（论文/实验报告/LEDGER 三处订正注）、D-11、A-P2-01…11 的转写与 REPORT_paper §4/结论一致；分歧台账本身不越权翻案（1.152 选择与 k_corr 复现系已裁事项，未重报）。
- 本面唯一问题即 R1（引式括号）、Y2、Y3。

### 面② 行文逻辑（主链/互斥/订正注/UNRESOLVED）
- REPORT_paper（133 行全读）与 REPORT_experiment（110 行全读）主链数字互证：摘要 25 常数/2.35e-14/5811→5816.6 ↔ §4.1-4.8 ↔ 结论 ↔ 诚实边界 9 条，四档偏置、N_min、k_corr 订正注、ulp 规则、A-P2 系列全文自洽；README.md 速览与两报告一致；refs.md 19 条 VERIFIED 表内部（题录、页码、正确/错误 DOI 对照）自洽。
- `<!-- 订正: -->` 两声明注抽查（REPORT_paper:81/:107、LEDGER:9/:13、REPORT_experiment:73、PHASE2_UPM 交叉核对处）格式与内容合规，旧对照保留。
- 五份正式报告正文无 UNRESOLVED 混入；UNRESOLVED 仅存在于 snr-propagation-design §7/§11/§13 差距表与 EXP 文档的元登记（方案设计文档合法用法）。
- docs/ 下 EXP-01…06、EXP-06-SUMMARY、f-instr-canon、frame-snr-canon、surveys 扫描（恒真/订正/9321/5816/1.91e-6 关键词）无扩散：错挂数字未进入这些文档。
- 本面问题：Y2、Y6、Y7（及 Y1 的编号错挂）。

### 面③ 跨文档冲突（五权威方 + 08_修复包② + reverse_verify）
- **08_修复包②**：`05_正向规格.md:118` P-CST-11 行已带 A-P2-05/06 订正注与「旧注无关闭式作废」，本单元 REPORT_paper:65「05 曾标注…按台账订正」的过去时转述与被引文档现状一致；`02_已确立.md:124/:166` 的引用纪律（头条引闭式、实现值带 N_MC）与论文 §4.3 一致。
- **docs/science 正本**：NOISE_MODEL、CALIBRATION、CONTROL_WEIGHT_SNR（:11-14 重定义注记与设计文档一致）、PHASE2_UPM（k_corr/D-08 落地完整）、07_noise_snr 锚（:39/:40/:41/:51/:82/:83/:107 抽查内容相符）。
- **reverse_verify 三子块 vs 主报告**：f_instr README（−0.72 mag、假增益 0.52×、孔径无关 ≤0.005 mag）与 CANON §2（−0.72 等、0.516×、M_seeing ≤0.004 mag）相容（0.005 为 0.004 的保守上界）；frame_snr README 判据先行纪律/P8 解析断言例外/UNAVAILABLE 登记与 CANON 诚实边界一致；三 README「无哈勃数据、49 帧 L4 实测帧作底、GAIN/RDNOISE 声明式取值」与 CANON:41、论文 §6 一致；CANON §3.6 修正表述（0.063%→0.18%/4.42%、131%→+31%、eps 20.5%/609%）与设计文档 §0/§13.7 定量一致。
- **evidence_web.json/refs 文献**：见面④。
- 本面问题：R2、Y1、Y5、Y6、Y7、Y8。

### 面④ 幻觉与锚点
- **文献 web 抽验 5 条全部命中**：[10.1086/338393](https://api.crossref.org/works/10.1086/338393) = Fruchter & Hook, *Drizzle…*, PASP 114(792), 144-152, 2002 ✓；[10.1086/341773](https://api.crossref.org/works/10.1086/341773) = *Viral Etiology…*（CLIN INFECT DIS，儿科病毒学）——refs.md「旧 DOI 为他文」的否定性陈述属实 ✓；[10.1093/mnras/17.1.12](https://api.crossref.org/works/10.1093/mnras/17.1.12) = Pogson 1856, MNRAS 17, 12-15 ✓；[10.1080/01621459.1993.10476408](https://api.crossref.org/works/10.1080/01621459.1993.10476408) = Rousseeuw & Croux 1993, JASA 424, 1273-1283 ✓；Horne 1986 [10.1086/131801](https://api.crossref.org/works/10.1086/131801) = PASP 98, 609（由 evidence_web.json 实抓 ✓）。Aitken DOI 沿上轮已 PASS 记录不重报。
- **源码锚抽验命中**：`snr_science.cpp:167-182`（Horne 最优提取与 SCI-B D1 读噪双计防护）✓、`:231-235`（孔径组 f_in/aperture_correction/sigma_f_aperture 输出 = P-CST-25 产生处）✓、`module_adapters.cpp:7159`（EMPIRICAL_TOTAL_RMS 声明，D1 闭环属实）✓、`star_detector` NaN fail-closed 注释与 detect(:120-123) 联动 ✓、`parser.cpp:319-325` phase2 键集 ✓。
- **文件/产物存在性**：code/ 全部脚本（b1–b7、exp01–06、audit 三路 35 脚本、supplement 两脚本、reverse_verify 四子块与 CMakeLists）与 results/ 对应 JSON 逐一在列；testdata 49 帧声明与 RELEASE-02 实测数一致；08_修复包/② 子目录实存。
- **结果 JSON 数字抽查**：b1（−0.4879107、2.7805、3.4449、max|z|=2.6856、G4c_B0=0.366199）、b2（0.12879/0.34308/0.34398）、b4（2.2204e-16、1.52257、1275.1233/1260.4095）、b5（0.997989、0.32463）均与报告 H/结果表相符。
- 本面问题：R1、Y3、Y4（锚组）、Y5、G2；其余锚抽查命中。

---

## 边界声明
- 本切片为只读静态检查：未运行任何构建/测试/实验脚本，所有「实测」均指对既有入库文件的读取与只读复算（awk/grep/python 解析 JSON）。
- 未重报 `检查-修复验证.md` PASS 表内已验证项；k_shape 于机器 JSON 记录、D_p² 等价参数化（含 snr-propagation-design:386/:1179 两处）、k_corr 复现与 1.152 选择之争、Y6 Aitken DOI、G1 k=0.1 豁免均按既有裁决不重开。
- 分歧台账自身（独立审计域）出现的同源错误（D-04 括号、9321、05 规格:118 挂接）仅作域外注明，不计入本切片计数。
