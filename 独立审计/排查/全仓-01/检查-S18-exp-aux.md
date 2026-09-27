# 检查-修复验证 · S18（实验辅件单元：shared ＋ cone-search-constants ＋ m42-realdata）

> 切片：`实验/shared/` 全部（含 `data/`、`synthetic/`、`references/`、`REVERSE_VERIFY_MIGRATION.md`）、`实验/cone-search-constants/` 全部、`实验/m42-realdata/` 全部，以及其对照面 `testdata/README.md`（入库策略一致性）。
> 方法：只读静态核验。零 git 写、零构建、零测试、零写盘（唯一写盘 = 本报告）。
> 纪律：先读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（PASS 项与 D 系/A-* 终裁一律不翻案重报）；科学公式/默认容差/冻结定义只登记红级＋证据、不代拟改法；testdata 数据本体不动，涉及改/删数据一律上呈。
> 证据级别：所有 file:line 均实开核对；红/黄均附反方核验（即"试图证伪本条"的检查与其结果）。

---

## 一、问题清单

### 【红】S18-01 ｜实验/cone-search-constants/REPORT_paper.md:5,7,292,308-310,495-496,506 ｜面④
**问题**：报告声称的全部代码、机器可读结果与复现脚本在仓内**一个都不存在**，而 PROGRESS 表自标"全阶段 DONE ✅"——典型「文档说有、代码没接」。
**证据**：
- :5 称 `results/phase1_phase2.json`、`results/final_constants.json`（seed 20260926）；`ls 实验/cone-search-constants/` **只有 REPORT_paper.md 一个文件**，无 `src/`、无 `results/`、无 `build/`。
- :308-310 文件清单表列出 `src/cone_search.cpp`、`src/process_testdata.py`（:292 复现命令）、`src/ir尔斯_fit.cpp`（文件名本身已残缺乱码）——三者均不存在。
- :495-496 PROGRESS 表 P3/P4 标 ✅DONE、锚 `src/cone_search.cpp:247-312`；:506「**全阶段 DONE ✅**」；:416/:454 内嵌"结果 JSON"含 `"version": "0.11.0-alpha.3"`。
- §4.4（:205-221）"Testdata 实测结果"打印 `R_c = 3.00 ± 0.001° / θ_p = 18.50 / φ_p = 310.00`，与 §4.1 合成真值（:174）**逐位相同**——真实 testdata 帧不可能复现合成注入真值，属结果不可信的内部铁证。
- 摘要 :15 中「极轴 ±0.5° ⇒ 匹配率下降约 15%」「偏移 [-0.1,0.1] mag 才收敛」两条定量主张在全文**无任何对应小节/表/图**。
**反方核验**：`find 实验/cone-search-constants -type f` → 1 个；`grep -rn "final_constants\|process_testdata" .`（排除本单元与 .git）→ 仅命中本报告自身；已排除"结果在 run/"：`find run -name "*phase1_phase2*" -o -name "*final_constants*"` → 0。
**建议改法**：非科学性问题。二选一上呈：(a) 补交代码＋results＋seed 后复核；(b) 整篇降级为"未执行的实验计划"，撤下全部 PASS/EXPECT/结论与 PROGRESS DONE 标记。**不接受维持现状**。
**面**：④幻觉与锚。

---

### 【红】S18-02 ｜实验/cone-search-constants/REPORT_paper.md:4,48,353 ｜面④
**问题**：报告的"正本科学口径"锚与两处条款锚**不存在**。
**证据**：
- :4「`docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN；**§2「锥形搜索与常数定义」**）」——实开该文件：`## 2 符号表`(:19)，其后为 `## 2a 参考通量…`(:37)；`grep -c "锥形" docs/science/PHOTOMETRY.md` = **0**。§2「锥形搜索与常数定义」纯属捏造。
- :48「禁止硬编码…（`PHOTOMETRY.md` **§2.1**）」与 :353 A-02 锚同为 §2.1——该小节不存在（2 与 2a 之间无 2.1）。
**反方核验**：换关键词 `grep -n "cone\|常数定义\|半径" docs/science/PHOTOMETRY.md` → 命中均为 FOV/单位/符号语境，无"锥形搜索常数定义"节；逐条列章节头（7 行起）确认无 2.1 编号空档。
**建议改法**：锚须改为真实条文（或承认该单元无正本依据并上呈）；正本条款本身不可擅改。
**面**：④。

---

### 【红】S18-03 ｜实验/cone-search-constants/REPORT_paper.md:30,31-32,352 ｜面④
**问题**：「四项常数在 `frame_photometry_fit.cpp:172-173` 中被显式配置」为**张冠李戴＋键名不存在**，且与紧邻下一段自相矛盾。
**证据**：
- 实开 `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:168-174`：`:168-171` 算 `fov_radius_deg = pixel_scale·√(w²+h²)/2·1.2`，`:172-173` = `if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) { fov_radius_deg = std::min(std::max(fov_radius_deg,1.0),10.0); }`——**FOV 半径条件钳位**，与四键无关。
- `git grep -n "polar_theta_deg\|polar_phi_deg\|constant_offset_mag" HEAD`（*.py/*.cpp/*.h/*.json）→ **零命中**（报告本体除外）；`cone_radius_deg` 全仓唯一命中在 `lib/algorithms/platesolve/tools/diag_projection_plot.py:156`（`= fov_radius_deg * 1.05`，绘图工具局部变量，非配置键、非本文件）。
- :31-32 报告自己下一段即写「锥角半径必须 ≥1.0° 且 ≤10.0°」，正是该 FOV 钳位窗数值——自证其把 FOV 常数误认作"锥形搜索常数"。
- :352 A-01「FOV 钳位范围 | frame_photometry_fit.cpp:172-173 | ✅确认」：锚行对 FOV 而言是对的，与 :30 的"四常数锚"两说并存、互相打架。
**反方核验**：逐行读 :160-180 排除"四键在邻行"；用 HEAD 而非工作区 grep，排除未提交文件；`grep -rln` 全树扫（含 run/）仅 platesolve 一处。
**建议改法**：撤下 :30/:352 的"✅确认"并改锚至真实条文；键名与常数定义属科学口径，**只上呈不代拟**。
**面**：④。

---

### 【红】S18-04 ｜实验/cone-search-constants/REPORT_paper.md:342-344 ｜面④
**问题**：§8.2 三行"开源对照（无差异）"锚**三行全错**（路径错 / 目录错 / 行内容错）。
**证据**：
1. :342 `/实验/shared/scia_common.py:247` → 文件**不存在**；真实文件 `实验/photometric-magnitude/code/scia_common.py`，其 :247 = `Qg = np.interp(...)`、`f_syn` 梯形积分在 **:248**（`np.trapezoid`），路径与行号双错。
2. :343 `lib/algorithms/spectroscopy/cpp/src/spectrum_integrator.cpp:89`（称"复合 Simpson 求积"）→ `lib/algorithms` 下**无 spectroscopy 目录**，真实在 `lib/algorithms/photometry/cpp/src/`；:89 = `double w1 = std::fabs(m_right - m_cur);`（单调三次 Hermite 权重），**不是**复合 Simpson。
3. :344 `star_matcher.cpp:156`（称"Tukey-IRLS 权重 (Huber 2011)"）→ :156 = `destroy(node->left);`（KD-tree 析构），与 Tukey 权重无关。
**反方核验**：三条都先按报告给的路径/行"按图索骥"再失败，改用全树 `find`/`grep -n` 反查真实位置，排除"我找错文件"；`ls lib/algorithms/` 证实 spectroscopy 不存在。
**建议改法**：三行必须改锚或删行；"无差异"结论在锚修正前不成立。
**面**：④。

---

### 【红】S18-05 ｜实验/cone-search-constants/REPORT_paper.md:134-136,147,208,292,456 ｜面①＋④
**问题**：被引科学数据对象三处错误/不存在。
**证据**：
- :134「HST/**MACS J1689+6149** HLSP drz 图像（**M16 星云**）」——MACS J1689+6149 是 Abell 星系团（强透镜场），与 M16 鹰状星云不同天区，一句话缝合两个对象；:135「4096×4096 px」——实开 `testdata/HST_M16/*.fits` 头部 `NAXIS1=8000 / NAXIS2=8400`（三帧一致）；:136「WCS 由 FLASK 提供」——M16 HLSP 的 WCS 来自 Hubble Heritage drizzle（`CTYPE=RA---TAN`、`CRVAL=274.721587/-13.841549`、`ORIENTAT≈-35`，与 `实验/shared/data/real/m16_scene_index.json` 逐项一致），无 FLASK 迹象（FLASK 是模拟星表生成器）。
- :147/:208/:292/:456 `testdata/HST_M16/obs_001_f435w_drz.fits` → 目录仅 3 个 `hlsp_heritage_hst_wfc3-uvis_m16_{f502n,f657n,f673n}_v1_drz.fits`，**无 obs_001、无 F435W**（F435W 为 ACS 通道，本数据为 WFC3/UVIS）。
**反方核验**：直接读 FITS 头（不依赖 README），三帧 NAXIS/CRVAL/CTYPE 全一致；`ls testdata/HST_M16` 全列；`grep -rn "MACS J1689" .` 全仓仅此一处，排除"别处同口径"。
**建议改法**：对象与尺寸错属事实错误须订正（可给题录级改法）；"FLASK WCS"等科学归属**只登记上呈**。
**面**：①（并④）。

---

### 【红】S18-06 ｜实验/cone-search-constants/REPORT_paper.md:512 ｜面④
**问题**：参考文献 [1] 题录错误——把 Gaia DR3 主论文的卷页安在 Brown 名下。
**证据**：:512 `[1] Brown, A. G. A., et al. (2023). "Gaia Data Release 3". *A&A* 674, A2.`
- web 核验：Gaia DR3 数据描述主论文 = **Vallenari et al. 2023, A&A 674, A1**（aanda.org 论文页 aa43940-22）；Brown, A. G. A. 是 **Gaia DR2** 主论文第一作者（A&A 615, A1, 2018）；A&A 674 **A2** 是 De Angeli 等内标定论文（见 `独立审计/证据/AUD-301` 与 `核验-CIT-11.md:25` 的成对绑反专检）。
**反方核验**：以仓内已核题录 `核验-CIT-12.md:14,31-35`（DOI 10.1051/0004-6361/202243709 → vol 674, page A33）与 `AUD-301:195-197`（202243880 ↔ A3 ↔ 2206.06205）交叉，排除"DR3 确有 Brown 署名卷"与"A2 归 Brown"两种可能。
**建议改法**：题录按 web 一手源改正（题录订正允许给出）。
**面**：④。

---

### 【红】S18-07 ｜实验/cone-search-constants/REPORT_paper.md:6,416,454,524-525 ｜面④
**问题**：仓库版本锚 `VERSION = 0.11.0-alpha.3` **在 git 历史上从未存在**，且与所附 HEAD 自相矛盾。
**证据**（只读 git 历史）：`VERSION` 演变为 0.9.0-alpha.1 → 0.10.0-alpha.1 → 0.10.0-alpha.2 → 0.11.0-alpha.1 → 0.11.0-alpha.2 → **0.1.0-alpha.1**（现行）；**0.11.0-alpha.3 全程零命中**；commit `d8495a65`（2026-09-22，commit 本身存在）当时 `VERSION` = **0.11.0-alpha.2** ⇒ :6/:524-525 的「0.11.0-alpha.3 + d8495a65」组合不可能成立。
**反方核验**：`git log --all --oneline -- VERSION` 遍历全部引用后对每版取值检索 "0.11.0-alpha.3" → 0；单独 `git cat-file -e d8495a65` 确认 commit 有效（否则应改判 HEAD 错）。
**建议改法**：版本锚改正（版本不存在 ⇒ 只能改版本值为真实值）。
**面**：④。

---

### 【红】S18-08 ｜实验/cone-search-constants/REPORT_paper.md:15 vs 192-195 vs 265 ｜面②
**问题**：摘要与正文**两套数字**，同一量三处口径互斥，且无订正注记。
**证据**：
- 相对误差：:15「**0.12% / 0.08% / 0.05%**」vs :192-195 表「**0.013% / 0.0005% / 0.00006%**」——摘要值是表值的约 9× / 160× / 830×，非舍入差。
- 精化精度：:15「细化至 **±0.01°**」vs :265「细化至 **0.001°**」vs :211-213 实测 `± 0.001°`。
**反方核验**：独立复算表值：`(3.0004−3)/3 = 0.0133%` ✓、`(18.5001−18.5)/18.5 = 0.00054%` ✓、`(309.9998−310)/310 = 0.000065%` ✓ —— **表内自洽，摘要是错的一方**；:184 迭代增量 1e-4 与 :265 的 0.001° 一致，:15 的 ±0.01° 为离群说法。
**建议改法**：两说必须统一；因无 results 佐证，**取哪个数由上呈裁定，此处不代定**。
**面**：②。

---

### 【红】S18-09 ｜实验/cone-search-constants/REPORT_paper.md:195 ｜面①
**问题**：对**零真值**给出非零"相对误差 0.003%"——零分母下相对误差无定义，该列是伪精度。
**证据**：:195 `C_const | 真值 0.000 mag | 估计值 -0.00003 mag | 相对误差 0.003% | ✅`。复算 `|−0.00003−0| / 0` 发散；0.003% 只有在分母被偷换成 0.001 mag 时才成立，但真值列明写 0.000（与 :174 注入 `C=0.00 mag`、§3.3 T01「常数偏移 0.0 mag」一致）。且 :152 正例 Oracle「相对误差 <1%」确实把该行当 PASS 依据。
**反方核验**：排除"真值是显示截断"——同表已给出估计值全精度 −0.00003，注入真值为 0；再按 ±0.0021 的标准误差反推，0.003% 也无法由任何常规相对误差式得到。
**建议改法**：属科学判据问题——**只登记红级＋证据，不给改法**（须由上呈决定改用绝对误差或不确定度覆盖判据；公式与判据不得由本检视代拟）。
**面**：①。

---

### 【红】S18-10 ｜实验/m42-realdata/README.md:13,59-64,80-82 ＋ REPORT_paper.md:18,64-70 ｜面③
**问题**：C1 判红的核心结论与仓内**已定案的根因订正**正面冲突，单元内无任何订正注记。
**证据**：
- 本单元现状：README:13 判据 C1-G1 `2.5·sigma_residual_dex ≤ 0.057457 mag`，README:80-82 与 REPORT:18 断言「49/49 帧超界、中位 7.8 倍」，README:59-64/REPORT:66-70 把红解读为「**生产测光链在真实数据上的已确认缺陷**……帧间系统项没有被物理模型吸收」。
- 别批已定案：`实验/photometric-magnitude/RESOLUTION_m42_curve_resolve.md:12-26,131-141`——根因 = **生产曲线解析缺陷（"Baader R" 文本搜索命中转录版 provenance 段 → 取到 Antlia V Pro Series B，53 点 420–524 nm）＋配置缺 QE ⇒ Q(λ)≡1**；修好通带后 49 帧中位散度 **0.04505 mag，落回量级参照内**；§5.3 明文「m42 C1 判红保留为**被缺陷放大的读数，不得表述为'生产测光链在真实数据上的已确认缺陷'**（根因已定位并已在代码层修复）」；§5.4 明文 0.057457 是 EXP-04 帧 B 的 **σ_obs（观测值）**，"band 上界"须改称量级参照。
- 正本同步：`docs/science/PHOTOMETRY.md:126` 已把该根因写成**定案句**（非待定）。
- 本单元零同步：`grep -n "订正\|根因\|曲线解析\|Baader\|Antlia" 实验/m42-realdata/*.md` → 唯一"根因"命中 CRITERIA:298（说的是另一条 C2-G1），**无任何 C1 订正注记**。
**反方核验**：确认 RESOLUTION 比本单元新且**主动选择不动 m42**（其 :5-6 自述"不改 实验/m42-realdata/（只读）"）⇒ 不是我没找到注记，而是注记确实缺；确认代码层修复真实存在（RESOLUTION :298-300 自述唯一改动即 `frame_photometry_fit.cpp` 曲线解析修复与 QE 告警），即"已确认缺陷"表述已过时。
**建议改法**：本单元须加**订正注记**（引 RESOLUTION 根因与 §5.3/§5.4 禁表述条款），C1 结论改述为"被缺陷放大的读数；修复后待重跑"。是否重跑 RELEASE-05 属负责人决定，本检视只登记。注意改 README/REPORT 将破坏 `results/SNAPSHOT.sha256`（须同步重签）。
**面**：③。

---

### 【黄】S18-11 ｜实验/shared/data/README.md:63 ｜面④
**问题**：索引重建入口死链——`run/reverse_verify/m16_scene/make_index.py` 不存在，`m16_scene_index.json` 的"可复跑重建"承诺落空。
**证据**：`ls run/reverse_verify/m16_scene/make_index.py` → 不存在（该目录本身也不存在）；`find run -name "make_index.py"` → 0；`grep -rn "make_index" 实验/` → 仅 README:63 一处。
**反方核验**：核 `m16_scene_index.json` 自述 `generated_by` 三脚本（m16_mask.py / m16_scene.py / exp_a6_seeing_aperture.py）均存在，排除"README 指的其实是这三者之一"；即被登记的第四个入口确实无对应物。
**先前登记**：`独立审计/证据/AUD-101-DB-19.md:1369` 已登记，**至今未修**（本条 = 仍开放确认）。
**建议改法**：改锚至真实生成脚本或补交该脚本（入口脚本属代码，可给建议）。
**面**：④。

---

### 【黄】S18-12 ｜实验/shared/synthetic/README.md:45 ｜面④
**问题**：渲染报告死链——`reports/RELEASE-02/m16-scene.md` 不存在。
**证据**：`实验/shared/` 下无 `reports/` 目录（同级仅 data/ synthetic/ references/）；`find . -name "m16-scene.md"` → 0；`find . -path "*RELEASE-02/m16*"` → 0。
**反方核验**：确认非改名/搬移（全树按文件名与按 RELEASE-02 路径两种查法都空）。
**先前登记**：`AUD-101-DB-19.md:1376` 已登记，**未修**。
**建议改法**：删引用或补交报告（产物补交，可给）。
**面**：④。

---

### 【黄】S18-13 ｜实验/shared/data/real/README.md:22 ｜面③
**问题**：登记表与数据本体不符——F657N 的 DARKTIME 记为「—」（无），实际 FITS 头与本目录索引都有该值。
**证据**：README:22 `| DARKTIME | — | 903.84 s | 1003.72 s |`；
- 头部实测：`hlsp_..._f657n_v1_drz.fits` → `DARKTIME=           603.715296`（`head -c 40000 | grep -a -o`），EXPTIME=9600.0 与表一致，唯 DARKTIME 缺；
- `实验/shared/data/real/m16_scene_index.json` frames.F657N `"darktime_s": 603.715296` 也已记录；
- F673N=903.843296、F502N=1003.715296 两列与 README 一致（仅 F657N 列为 "—"）。
**反方核验**：三帧各读一次头部，排除"某帧真缺该卡"；再核索引 JSON 三帧全有 darktime_s ⇒ README 是唯一与数据不符的一方。
**建议改法**：登记表补值（数据**登记**订正，不涉及改动数据本体）。
**面**：③。

---

### 【黄】S18-14 ｜实验/m42-realdata/README.md:13,80-82 ＋ REPORT_paper.md:18 ｜面③
**问题**：C1 判据参照物口径与上位裁决冲突——称 0.057457 为「EXP-04 三帧仿真 **band 上界** / **已发表量级上界**」，裁决要求改称「EXP-04 量级参照（三帧观测值 0.045344/0.057457/0.051718 的最大者）」。
**证据**：本单元三处现文（README:13、README:82「band 上界的 7.8 倍」、REPORT:18「已发表量级上界」）；`RESOLUTION_m42_curve_resolve.md:237-243` §5.4：「0.057457 是 EXP-04 帧 B 的 σ_obs（**观测值，不是带上界**）…**必须在任何后续引用中改称**量级参照」；`docs/science/PHOTOMETRY.md:126` 同口径。
**反方核验**：查 EXP-04 一手值 `实验/photometric-magnitude/results/gates.json:9`、`GATES.md:7`：三帧 σ_obs = 0.045344 / **0.057457** / 0.051718，0.057457 恰为帧 B 观测值；`README.md:151` 表同值 ⇒ "上界"说法在源头即无支撑（不是本单元抄错数，是性质定错）。
**先前登记**：`AUD-101-DB-18.md:37-38` 已登记（处置建议"按上位订正 §1/§4.1/§8"），**至今未修**。
**建议改法**：措辞订正（与 S18-10 一并处理、注意重签 SNAPSHOT）。
**面**：③。

---

### 【黄】S18-15 ｜实验/m42-realdata/README.md:218、docs/CRITERIA.md:104、code/c2_absolute_snr.py:9 ｜面④
**问题**：`docs/science/SNR_CHAIN.md` 全仓不存在，却在单元正本、判据台账与代码头注三处被引（三方同错）。
**证据**：`ls docs/science/SNR_CHAIN.md` → 不存在；`find docs -name "*SNR*"` → 只有 PSF_SIGNAL_WEIGHT / CONTROL_WEIGHT_SNR / NOISE_MODEL 等，无同名改名物；`grep -rn "SNR_CHAIN" docs/` → 0。
**反方核验**：README:218 出现在"佐证来源"表、CRITERIA:104 出现在规范出处行、c2_absolute_snr.py:9 在文件头依赖声明 —— 三处语义都是"必读上游"，非装饰性提及。
**先前登记**：`AUD-101-DB-18.md:37,39` 已登记（悬空引用列），**未修**。
**建议改法**：改锚至真实 SNR 权威文档（选哪份由作者定，可给候选）。
**面**：④。

---

### 【黄】S18-16 ｜testdata/README.md:73-75 ｜面④（m42 切片对照面）
**问题**：入库策略文档的"消费方"名单用已退役单元名，三条路径全部不存在。
**证据**：README:73-75 列 `实验/SCI-A/code/step2_hst_sim.py`、`实验/SCI-B/code/{sci_b_common.py,b3_domain_map.py}`、`实验/SCI-C/code/sci_c_common.py`——`ls 实验/SCI-A` 等 → 目录不存在（现名 photometric-magnitude / absolute-snr / additive-sky-seamless）；`find 实验 -name step2_hst_sim.py` → 在 `实验/photometric-magnitude/code/`；`sci_b_common.py`、`b3_domain_map.py`、`sci_c_common.py` 均在新名目录下。
**反方核验**：`git grep -n "实验/SCI-" -- docs testdata` 全仓仅命中 testdata/README.md:74 一处（与 AUD-101-DB-13:329 结论互证）⇒ 排除"仓内仍通用 SCI-* 别名"。
**先前登记**：`AUD-101-DB-18.md:162` 已登记（"只登记不处置"），**未修**。
**建议改法**：名单改新名路径（纯路径订正，可给）。
**面**：④。

---

### 【黄】S18-17 ｜testdata/README.md:28-29 ｜面④
**问题**：自称"可复跑"的两条机器校验，现在会给出**错误/过期的判定**。
**证据**（逐条原样实跑，只读）：
- :28 `git check-ignore -q testdata/HST_M16/*.fits && echo OK || echo BAD` → glob 展开成 3 个路径，**git 2.47.3 下 `-q` 多路径直接 exit 128** ⇒ 打印 **BAD**；而数据其实被正确 ignore（`git check-ignore -v` 三条均命中 `.gitignore:169:testdata/HST_M16/`；逐文件 `-q` 逐个 exit 0）。
- :29 `git status --porcelain --untracked-files=all -- testdata/BASS_DR3 | wc -l  # 期望 367` → 实测 **0**；`git ls-files testdata/BASS_DR3 | wc -l = 367`，367 个元数据文件已全部提交跟踪，`git status` 不再列出 ⇒ 期望值过期。
**反方核验**：原样复制 README 两行整串执行（排除我抄错命令）；单文件 `-q` 验证 ignore 规则本身有效（即 :28 的 BAD 是命令形态问题，不是数据违规）；`git --version` = 2.47.3。
**建议改法**：校验命令改可跑形态（:28 逐文件或去 `-q`；:29 改为 `git ls-files … | wc -l` 期望 367，或更新期望值）。属文档内命令订正。
**面**：④。

---

### 【黄】S18-18 ｜实验/m42-realdata/docs/DATA-SOURCES.md:34 ｜面④
**问题**：`sample_mask` 寻址公式与代码实现不符，且与同文件 :28 字段表自相矛盾。
**证据**：:34 称按 `tile_index·262144 + slot·262144 + p` 寻址；实现侧是**逐 tile 偏移**：`c3_seam_additive.py:183-184`、`c4_leaf_allocation.py:199-201` 用 `sample_mask_offset`（= 此前各 tile `depth×262144` 累计和，`Σ depth = 5609`）＋ tile 内 slot/p；而本文件 **:28 自己**列出的 tile 字段即含 `sample_mask_offset` ⇒ 同一文档两种寻址说法。按字面公式 `slot` 与 `tile_index` 等权相加还会在 `slot ≥ tile_index` 时越界。
**反方核验**：独立复算字节数 `1,470,365,696 = 262,144 × 5,609` ✓、`Σ depth = 5609` 与 :28 一致 ⇒ 真实布局是"逐 tile 连排"，:34 公式漏用 offset；`grep -rn "sample_mask_offset" 实验/m42-realdata/code/ lib/` 确认实现变量名。
**建议改法**：文档公式订正为实现口径（产品字段说明，不涉冻结公式/容差，可给）。
**面**：④。

---

### 【黄】S18-19 ｜实验/m42-realdata/README.md:193,243-247 ｜面④
**问题**："一键复现"声明的输入产品已不在，README 未声明输入不可得（复现承诺当前无法兑现）。
**证据**：README:193 `bash 实验/m42-realdata/code/run_all.sh`、:243 「run_all.sh 一键复现」；全部输入来自 `run/RELEASE-05/vis/out/m42_p{1_t2,1_t3,2,3}`（:5,:51-60）—— `ls run/RELEASE-05` → **不存在**；`find run -maxdepth 3 -name "*RELEASE-05*"` → 0；现存仅 `run/M42-REALDATA-01/logs`。产品树曾按 `run/ROOT-CONSOLIDATION/RUN_CLEANUP.md` 回收，属预期 GC。
**反方核验**：反向核 results/ 15 文件仍在且 sha256 全过 ⇒ 缺的是输入不是结果；也排除"输入在别的轮次目录"（全树按 `m42_p1_t2` 文件名查 → 0）。
**建议改法**：README 加一行输入前置声明（恢复 75 GiB 输入属负责人决定；**不建议**为满足复现恢复或改动任何数据）。
**面**：④。

---

### 【黄】S18-20 ｜独立审计/实验重做/总编对账/五单元成稿简报.md:38 ｜面④（域外附带）
**问题**：五篇论文收口依据里的 Montegriffo arXiv 号错——指向了另一篇论文。
**证据**：:38「Montegriffo et al. 2023（**arXiv:2206.06205**）」；
- web 实取 abs 页 `<title>`：2206.06205 = *Gaia Data Release 3: **External calibration** of BP/RP low-resolution spectroscopic data*（A&A 674 A3，外标定）；
- 合成测光论文 = **arXiv:2206.06215**（*…The Galaxy in your preferred colours. Synthetic photometry…*），其 abs 页 `citation_doi = 10.1051/0004-6361/202243709`；
- 本切片文献库 `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md` 条目 7.6 引 **2206.06215 + DOI …202243709 = 正确**，与简报冲突；仓内 `P1通量积分拟合/路线1/refs.md:50,53` 两号分挂 A33/A3 也正确 ⇒ 06205 是简报单点抄错。
**反方核验**：两个 abs 页各取一次 title 逐字比对；DOI 用 arXiv meta `citation_doi` 交叉；再对 `独立审计/证据/核验-CIT-12.md:14,35`（190 号条目 = A33 ↔ 2206.06215）一致。
**建议改法**：简报改号（域外文件，只登记不处置）。
**面**：④（域外附带）。

---

### 【绿】S18-21 ｜实验/shared/REVERSE_VERIFY_MIGRATION.md:48 ｜面②
**问题**：硬边界仍以 `实验/SCI-*/code/reverse_verify/**` 描述路径，而迁移后该形目录已不存在（现为 `实验/{photometric-magnitude,absolute-snr,additive-sky-seamless}/code/reverse_verify/**`）。
**证据**：`ls -d 实验/SCI-*` → 不存在；同文件 :14-:30 迁移映射表自己写明目标是具体单元名 ⇒ :48 与本文件映射表口径不一。
**反方核验**：`find 实验 -maxdepth 3 -path "*reverse_verify*" -type d` 全部落在三个现名单元下，无 SCI-* 残留；文义仍可读（"三个 SCI 单元的 reverse_verify"）⇒ 不致误执行。
**建议改法**：可不改；若改，统一为 `实验/*/code/reverse_verify/**`。
**面**：②。

---

### 【绿】S18-22 ｜实验/absolute-snr/code/exp03/e1_analytic.py:74 ｜面④（域外附带）
**问题**：`glob("*F657N*.fits")` 用大写滤镜名，而 testdata 文件名全小写 `f657n` ⇒ `m16_template` 臂**静默缺席**（死分支）。
**证据**：`testdata/HST_M16/` 三文件名全小写；`实验/absolute-snr/results/exp03_e1_analytic.json` 含 `m42_template` 键、**无 `m16_template` 键**；EXP-03 文档 :329-337 结果表也只列 m42_template ⇒ 文档与结果自洽，只有代码分支是死的（同目录 `HST_GLOB` 的 `*.fits` 兜底臂可用）。
**反方核验**：`grep -rn "F657N" 实验/absolute-snr/code/exp03/` 仅此一处大写用法；确认文档**未**主张 m16_template 结果（否则应升黄/红），不影响任何已发布结论。
**建议改法**：域外只登记；若后续要跑该臂需改大小写不敏感匹配。
**面**：④（域外附带）。

---

## 二、已查无问题面（逐面说明＋抽查方式）

### ① 科学性（对数 分歧台账 D-01…D-11 与 A-* 终裁；负例与判据非退化）
- **shared 常数与 seed 纪律——干净**。抽查：`m16_scene.py:172` `M16_DEFAULTS["stack_n"]=32`、`:499` `sc.get("stack_n",32)`、4 个 m16 场景 JSON `stack_n=32`＋`read_noise_e=3.1`、`p7lib.py:312` 默认 32 —— 与 STACKN32-001 一致；独立复算 `√32×3.1 = 17.536 e-` ✓、`full_well 2.24e6` ✓；`before_stackn32/scenes/*.json` 全为 `stack_n=1` 且与 CHANGES.md 所述差异逐字段 diff 一致；23 个场景全有顶层 `seed`（render 20260919、m16_scene 20260924），无隐式全局随机态；`datasets.json` 登记的 18 个场景文件全部存在。
- **零点表——逐位复算通过**：`实验/shared/data/real/README.md` §2 的 ZP_AB 22.6350/22.5649/22.2889、ZP_ST 23.0297/23.0245/22.0960、r20 = 11.32/10.62/8.23 e/s，按 FITS 头 PHOTFLAM/PHOTPLAM 独立复算全部一致（含 −2.408 常数推导），与 `m16_scene_index.json` 同值。
- **cone 报告的公式常数本身未越界**：`S = MAD/0.6744897501960817`、`c = 4.685`（:112）与正本 PHOTOMETRY.md:30-31,:205-213 逐字一致（D-06 维持）；`σ_floor = (1−3·1.166/√n)·σ_fit`、`σ_ceiling = (1+3·1.166/√n)·√(Σbudget²)`（:123-124）与 `docs/plugins/algorithms_phase1/06_photometry.md:41`、`photometric-magnitude/README.md:78` 同式；`1.166 = √1.361` 标签与 A-P1-01 终裁一致。**本面红条只有 S18-09（零分母相对误差）与 S18-05（数据对象），均只登记不改公式。**
- **m42 数值抽查——独立复算全过**：`A_cell = 4π/(12·262144²) = 1.52387e-11 sr` ✓；`hp_res = √(4π/12/262144) = 0.805192″` ✓；`A_drop 2.1979e-11` ✓；`1,470,365,696 = 262144×5609`、`1,096,810,496 = 137,101,312×8` ✓；`0.44746/0.057457 = 7.79 ≈ 7.8` ✓；由 CD 矩阵反推 `0.967″/px` ✓。
- **负例非退化**：m42 CRITERIA 的注入类负例（C3-N1 注入 A*=2e-06、A=0 判绿）、EXP-04 正负例（帧 A/C 在带内、B 超界）均有真值且能红能绿；cone 负例矩阵（N01-N05）设计合规但因 S18-01 无载体、无证据资格（该结论归④，本面不重复登记）。

### ② 行文逻辑（断裂、前后矛盾、订正注记新旧两说、UNRESOLVED 混入正文）
- **cone**：全 525 行通读，仅两处两说（S18-08），无断裂、无 UNRESOLVED 混入正文。
- **m42 四文件**（README 248 / REPORT 192 / CRITERIA 301 / DATA-SOURCES 60 行）：CRITERIA 对未定项写「未定位/待定」并给判定方法（:298 即例）；README §4.1-§6 与 REPORT §2/§4 数字集重叠但同值（49/43/6 与 level 分布全等），无两说。
- **shared**：`REVERSE_VERIFY_MIGRATION.md`（104 行）通读，映射表与 §4 偏离说明自洽，仅 :48 路径形残留（S18-21 绿）；文献库 §0 规则与表体格式一致。
- **testdata/README.md**：三条入库策略（BASS 入库 / HST_M16 不入库 / 其余 T2-T4 不入库）与 `git ls-files testdata = 369`（367 BASS + README + index）、`git check-ignore` 实况逐条相符（命令形态缺陷另记 S18-17）。
- **PASS 回护**：上轮 PASS 项（红 8 中 7 条 PASS、黄 16、行漂移 6）与 D 系/A-* 终裁（D-01/02/04/05/06/08、A-P1-01、STACKN32-001）本报告一律未重开。

### ③ 跨文档冲突（五方权威链）
- **m42 ↔ testdata 登记一致性——通过**：m42「T2 16 帧 + T3 33 帧 = 49」与 `testdata/M42_T2T3_mosaic_Flying_dutchman` 实盘计数逐目录吻合：`find T2 -iname "*300s*red*" | wc -l = 16`、`T3 = 33`（Red 档恰为 16/33；文件名 `-20251128@050425-` 亦与 DATA-SOURCES「@ 分隔时刻」一致）；`testdata/index.json` 在 T2/T3/T4 三望远镜条目均注册 `M42_T2T3_mosaic_Flying_dutchman`。
- **m42 ↔ SNAPSHOT/README 计数——通过**：`results/SNAPSHOT.sha256` 15 文件 `sha256sum -c` 全 OK；README §3.5 的 49 行、43 绿/6 红、level 分布（data18/ext-cons3/control2/pos4/neg17/degen3/honest2）与 `results/*.json` 逐项相等。
- **shared 索引一致性——通过（除 S18-13 一格）**：`m16_scene_index.json` 三帧 path/bytes/shape/bitpix/NDRIZIM/EXPTIME/CRVAL 与 FITS 头及实文件一致；5 个未登记场景中 4 个由自有 CLI 驱动（`m16_sampling_*`）、`hst_m16_realbase.json` 由 `datasets.json` 的 $scene 参数引用（非孤儿）。
- **本面红/黄冲突已全部单列**：S18-10、S18-13、S18-14。

### ④ 幻觉与锚（文献 web 核验、file:line 实开、"文档说有、代码没接"）
- **cone 集中爆点**：S18-01/02/03/04/05/06/07。
- **shared**：除 S18-11/12 两处死锚外其余实开通过——`synthetic/README` CLI（`load_scene` 回退 `SCENES_DIR/p.name`，仓库根可跑）、`data/real/README` 校验块、`REVERSE_VERIFY_MIGRATION` §5 三份 `REVERSE_VERIFY_CANON.md` 与 `p1_spatial_gain/build_oracle.sh` 均存在（`ls` 全 OK）、文献库 7.6 的 DOI+arXiv 与 abs 页 `citation_doi` 逐字一致。
- **m42**：S18-15/18/19；其余 §8 佐证来源锚抽开 12 处（docs/ASTROCS_DESIGN §12.2、PHASE2_UPM.md:9-10/:186-190、integrate.cpp、DRIZZLE_GEOMETRY.md、healpix_core.cpp:155-226 等）路径存在、行内容相符。
- **testdata**：S18-16/17；`index.json` v1.2 的 T1-T4 四组与实盘目录一一对应（M42/HST_M16/BASS_DR3 三键抽查存在）。
- **文献 web 核验抽样**：shared 7.6（2206.06215 + DOI 202243709）✅；cone [1]（Brown/A2）❌ = S18-06；简报 06205 ❌ = S18-20；1.1-1.4/4.1/6.1 等 DOI 形态与既有已核台账（`核验-CIT-*`、`AUD-301`）一致，未发现新题录错误。
- **域外附带**：S18-20、S18-22（均标注域外，不计入切片整改面）。

---

## 三、统计
- 红 10 条（S18-01…S18-10）、黄 10 条（S18-11…S18-20）、绿 2 条（S18-21…S18-22）；合计 22 条。
- 按面：① 2 条（S18-05 兼、S18-09）；② 2 条（S18-08、S18-21）；③ 3 条（S18-10、S18-13、S18-14）；④ 15 条（含 2 条域外附带）。
- 先前批次已登记、本轮复核**仍未修复**：S18-11、S18-12（AUD-101-DB-19）、S18-14、S18-15、S18-16（AUD-101-DB-18）——本报告为"仍开放"确认，未重复计功。
- 纪律确认：未提出任何删除/修改 testdata 数据本体的要求；未对科学公式、默认容差、冻结定义给出改法（S18-09 只登记）；未重开 PASS 表已验项与 D 系/A-* 终裁；全程零 git 写、零构建、零测试。
