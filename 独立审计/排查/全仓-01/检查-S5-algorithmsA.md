# 全仓排查 · S5 · docs/algorithms 甲组（15 份）检查报告

- 切片：docs/science/algorithms/{ACR_EQUIVALENCE, CALIBRATION_ALGORITHMS, COSMETIC_ALGORITHMS, DRIZZLE_GEOMETRY, GAIA_QUERY, GATES_AND_TOLERANCES, HEALPIX_MAPPING, HIPS_WRITER, NOISE_ESTIMATION, PHOTOMETRIC_FIT, PLATESOLVE, REJECTION_ALGORITHMS, STAR_DETECTION_ALGORITHMS, STAR_PSF_ALGORITHMS, UPM_SOLVER}.md
- 视角：只读对抗式静态核查；四面 = ①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚。
- 基线：HEAD `52adba89`（工作树 clean，仅 untracked 独立审计/排查/）。所有 file:行 锚均为本轮 grep/sed 实测复开。
- 去重规则（严格执行）：
  1) `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表已 PASS 项、观察项（GATES bgnoise 行、UPM/PHASE2_UPM 嵌套注释、NOISE_MODEL NaN 行锚、08_修复包 k_shape 记号、§4a D_p² 等价参数化换算注）、D-01..D-11 终裁值、§2 A-* 裁决、`frame_photometry_fit.cpp:292` 作为违规 —— 均不重报；
  2) 独立审计/证据/ 既登记项（AUD-101-DA02 / D1残余 / DB-09 / DB-11 等）→ 本报告登记为「已登记未闭环」并给出本轮复测证据，标注 provenance，不冒充新发现；
  3) 科学公式 / 默认容差 / 冻结定义变更类问题只登记上呈（红级＋证据），不给越权改法。
- 严重级统计：**红 7 / 黄 13 / 绿 3**。

---

## 一、红级（7）

### A5-R1  DRIZZLE_GEOMETRY.md:43-59 与 :332 —— 正本权重定义文内两说，且 Σ_p w=1 与 N_p 分母链在 :43 口径下不成立（登记上呈）
- 文件:行：docs/science/algorithms/DRIZZLE_GEOMETRY.md:43（`w_jp = a_jp / A_pixel,j`）、:48（「核按 drop 面积归一」D-03 订正注）、:50/:55（`Σ_p w_jp = 1`）、:56-57（`N_p = Σ_j w_jp·A_pixel,j` 与「改用 D_p 偏 1/pixfrac²」警告）、:59（等价参数化句）、:332（§10 DISP-DRZ-009 「canonical 核 `w_jp = a_jp/A_drop,j`」）。
- 所属面：①科学性 + ②行文逻辑 + ③跨文档冲突（登记上呈，不给改法）。
- 问题描述：同一份正本给出两个互相矛盾的 canonical 权重：:43 写 `a/A_pixel`，:332 写 `a/A_drop`。在 :43 口径下：(a) `Σ_p w_jp = Σ_p a_jp/A_pixel = A_drop/A_pixel = pixfrac²`（pf<1 时 ≠1），与同文 :50/:55「Σ_p w_jp = 1 ⇒ Σ_p F_p = Σ_j x_j 与 pixfrac 无关」直接矛盾；(b) 代入 :56 得 `N_p = Σ_j (a/A_pixel)·A_pixel = D_p ≡ 覆盖面积`，于是 :57「改用 D_p 作分母偏 1/pixfrac²」的警告在该定义下**不可能发生**（警告与定义互斥）；(c) :59「等价参数化 `w'=a/A_pixel` 配分母 `Σ_j w'_jp = D_p/pixfrac²` 给出同一个 c_jp」在代数与量纲上均不成立——`Σ_j w'_jp = D_p/A_pixel`（无量纲）≠ `D_p/pixfrac²`（sr），且同 c 要求 `N'_p = D_p`（因 w'=pixfrac²·w ⇒ N'=pixfrac²·N_p=D_p），与所给分母相差 pixfrac² 因子。
- 证据（含反方核验）：
  - 代码事实：`drizzle_engine.cpp:1617 Scalar weight = overlap_area / drop_area;`（= a/A_drop 口径，与 :332 一致、与 :43 不一致）；`:1644 acc.sumNorm += overlap_area * (pixel_area / drop_area);`（A_pixel 只经 sumNorm 因子进入 N_p）。
  - 上位：`docs/science/DRIZZLE.md:202` 把 a/A_pixel 参数化列为**红标负例**（FZ-COND-FLUX-CONSERV），:21/:45 为 drop 归一。
  - 反方核验：`pixfrac==1` 时 A_drop≡A_pixel（:43 段内自注），故 pf==1 路径两种写法逐位等价——矛盾仅在 pf<1 时显现，正是生产参数域。
  - provenance：AUD-101-DA02-算法推导.md:1582-1591 **已登记**同一族代数矛盾（L43/L50/L51-53），本轮复测 HEAD 仍开；:59 分母/量纲面向为本轮补充。
- 建议改法：**登记上呈**（涉及冻结权重参数化，属科学公式类）。携带上述推导与 D-03/SCI 负例三方对照上呈裁决；本报告不提修改方案。

### A5-R2  DRIZZLE_GEOMETRY.md:349-355 —— §11「实现口径」把权重分母说成 overlap/pixel_area，与代码及本文件 §10 自相矛盾
- 文件:行：docs/science/algorithms/DRIZZLE_GEOMETRY.md:349-355（「processPixelSharedTiled 内权重恒为 `weight = overlap_area / pixel_area`，其中 pixel_area = 未收缩源像素球面面积……pixfrac==1 时传 nullptr ⇒ 分母直接取 drop_area」）。
- 所属面：②行文逻辑 + ④幻觉与锚。
- 问题描述：§11 断言 pf<1 时权重分母 = 未收缩 pixel_area；实际代码中权重分母**恒为 drop_area**，未收缩 pixel_area 只以 `overlap*(pixel_area/drop_area)` 因子进入 sumNorm（:1535-1537 定义、:1644 使用、:1642 注释「pixfrac==1 时比值恰为 1.0」）。同时本文件 §10 DISP-DRZ-009 行（:332-340）明确写「正向 weight = overlap_area / drop_area（drop 面积归一）」——同一文档 §10 与 §11 对同一行代码给出两种互斥描述。
- 证据（含反方核验）：
  - 代码实测（drizzle_engine.cpp）：`:1415` 函数定义；`:1617 weight = overlap_area / drop_area`；`:1535 Scalar pixel_area = drop_area; :1537 pixel_area = polygon_area_consistent(pixel_corners_v,4)`（仅 pf<1 时改值）；`:1644` pixel_area 唯一消费点 = sumNorm。全文件 weight 分母无 pixel_area 分支。
  - 反方核验：`:1494/:1512` 代码注释自述「会系统性偏置 weight = overlap/drop_area」——代码注释与 §10 同说、与 §11 异说。
  - provenance：AUD-101-DA02:1585 已登记该「实现口径 weight = overlap/pixel_area」表述（当时位于 §10 L338-340），本轮复测：表述迁至 §11 且仍与代码不符，仍开。
- 建议改法：以 §10/代码为准改写 §11「实现口径」段（把 pixel_area 的作用点从 weight 更正为 sumNorm 因子），或整段改为指针引用 §10——属实现事实订正，建议随 DA02 既有处置一并执行。

### A5-R3  COSMETIC_ALGORITHMS.md:209-222 与 DISP-COS-009 —— 「两条生产路径均传 NULL 检测源 ⇒ 模块恒等 pass」在 HEAD 已被 WIRING-AUDIT-01 整改推翻（已登记未闭环）
- 文件:行：docs/science/algorithms/COSMETIC_ALGORITHMS.md:213-222（「现网生产调用点（两处，均传 NULL 检测源）……检测全部关闭，out_hot=out_cold=0，模块退化为恒等 pass」）、:214-217（p1_session.cpp:462-465/:471/:472/:477-478 锚组）、:218-219（module_adapters.cpp:2355/:2364/:2370/:2394-2395 锚组）。
- 所属面：②行文逻辑 + ③跨文档冲突 + ④幻觉与锚。
- 问题描述：核心「生产现状」结论已失效：(1) 调度器路径 `module_adapters.cpp:2749 p1_op_cosmetic`、调用 `ac_correct_frame` 在 **:2930-2933**，实参为 `p_dark_ok ? m_dark.px() : nullptr` / `p_bias_ok ? m_bias.px() : nullptr`——**母版已接线**，且 :2796 注释明写「master_dark/master_bias 同时是坏点检测（ac_correct_frame 的形参）」，:2925-2929 新增尺寸不符 fail-closed 判红；(2) legacy 路径 `p1_session.cpp:470-484` 同样经 WIRING-AUDIT-01 整改（dark_plane/bias_plane 实传 + "cosmetic master size mismatch" 判红），文档所引 :462-465 现为 probe 宏与 read_image；(3) 行锚组 :2355/:2364/:2370/:2394-2395 与 :462-465/:471/:472/:477-478 全部漂移（实测函数体 :2749 起、调用 :2930、memcpy :2943）。
- 证据（含反方核验）：
  - 代码 sed 实测（本轮）：`:2749`、`:2796-2816`（WIRING-AUDIT-01/LINDEF-IMPL-01 整改注释、col_enabled 默认 true）、`:2930-2933`（实传母版）；p1_session :470-484（判红 + 实传）。
  - 反方核验：文档结论对**整改前**的快照为真；两处代码注释均标注整改编号与 A/B 取证（run/LINDEF-IMPL-01/REPORT.md §5），非注释空谈。
  - provenance：**AUD-101-D1残余.md:86-87 已登记**（「全文核心生产现状结论在 HEAD 已失效」，含 p1_session/module_adapters 实测行号），处置建议①未执行；本轮 HEAD 复测仍开。不在 PASS 表、不在 06_实施 tasks。
- 建议改法：按 D1残余处置建议①作废改写 :209-222 与 DISP-COS-009（结论改为「已接线母版 + 尺寸一致门」），§9 非退化断言立论从「现状」改「正向条款」；跨文件锚整组重锚。执行归文档整改任务，本报告只登记。

### A5-R4  COSMETIC/CALIBRATION 双正本对坏列（bad column）生产族零登记；CALIBRATION「14 个 AC_API 符号」与头文件实测 18 不符
- 文件:行：docs/science/algorithms/COSMETIC_ALGORITHMS.md:5-11/:21-24/:30-38（「实现唯一生产源……逐公式权威登记」「唯一插值核即本节两条」）；docs/science/algorithms/CALIBRATION_ALGORITHMS.md:17-21（「astro_calibration.h（唯一公共头，14 个 AC_API 符号）」）、:614（「SRC-CAL-001（astro_calibration.h 14 符号）」）、:22-23（「负责……热/冷像素检测与插值修复」）。
- 所属面：④幻觉与锚（「文档声称覆盖、代码一半没登记」）+ ③跨文档冲突。
- 问题描述：生产文件 cosmetic_corrector.cpp 后半段（:324 detect_bad_columns / :459 repair_bad_columns / :516 correct_columns / :580 correct_columns_ex_impl / :701 correct_columns_ex / :727 column_variance_inflate，约占该文件 40%）经 ac_api.cpp 以 AC_API 导出 6 个符号（astro_calibration.h :161/:184/:207/:248/:311/:365），并在生产调度器默认启用（module_adapters.cpp:2786-2816 `col_enabled = p1_flag(c,"bad_column_enabled", true)`、bad_column_sigma=5.0、neighbor_k=3、max_seg_len=1，:2955 调 `ac_correct_columns_ex2`）。本轮 `grep -rn "坏列|bad_column" docs/` = **0 命中**：docs/algorithms 两份「权威登记」均未收录该族；SCI（docs/science/CALIBRATION.md:208-223）、合同（DATA_SEMANTICS §10.4/§10.5）、配置（defaults.json cosmetic.bad_column_* 6 键）、代码、测试（p1cos_tests_badcol.cpp）四层在册，唯第三层（算法推导）断档。同时公共头实测 **18 个 AC_API 函数声明**（grep 计数 23 处 AC_API 减 5 处宏/注释），两处「14」均错（既非 12 legacy 也非 18 总数）。
- 证据（含反方核验）：
  - grep docs/ 坏列|bad_column = 0（本轮，含 docs/algorithms、docs/plugins）。
  - astro_calibration.h AC_API 函数清单实测 18 条（:60-:379，其中坏列族 6 条）；头注释 :9 自称「12 个 legacy 符号」——14 不对应任何分组。
  - 生产消费实证：module_adapters.cpp:2955 `ac_correct_columns_ex2(`、:2727/:2793/:2798 注释链。
  - 反方核验：合同层 DATA_SEMANTICS §10.4 确有「列状缺陷路径（bad column）」行（代码注释引用）——故**不是全仓未文档化，而是算法层正本缺条目**；COSMETIC:26-29 还自称「cosmetic 域逐公式的权威登记」，覆盖声明与事实不符。
  - provenance：AUD-101-D1残余.md:86 第 4 点**已登记**「cosmetic 主题正本缺一整族算法（第三层无正本）」；「14 个 AC_API 符号」计数错误为**本轮新增**（DA02/D1残余 grep「14 个 AC_API」「18 个」= 0）。
- 建议改法：登记上呈——①按 D1残余建议④在算法层补坏列正本条目（或并入 COSMETIC 并收缩 CALIBRATION §3.4 为指针），②「14 个」更正为「18 个（含坏列族 6）」。增补属权威链断档修补，具体条目结构与 ID 归属由整改任务裁定。

### A5-R5  NOISE_ESTIMATION.md §13.1 + §13.4 源码锚表系统性失效（§13.4 构建锚为本轮新增证据）
- 文件:行：docs/science/algorithms/NOISE_ESTIMATION.md:129-130（头「noise_model.cpp 938 行」「snr_estimator.h 624 行」）、§13.1 锚表（default_config :696-719、noise_model_impl :347-621、fill :776-866、free :903-917、scale_law :919-926、gain_variance :928-937、ABI :746-767、DISP-007 :377-382）、§13.4 构建接入行（根 CMakeLists.txt:767-770（noise 源）、:876（noise OpenMP）、eng/tests/unit/CMakeLists.txt:619-623（p1_noise_test））。
- 所属面：④幻觉与锚（文档自declare「行号均 grep 实测」）。
- 问题描述：noise_model.cpp 实测 **1482 行**（文档 938，+544）；§13.1 表内逐条复开多数错位：default_config 实际 :1243（floor :1256）、noise_model_impl 实际 :783、fill :1323、free :1447、scale_law :1463、gain_variance :1472、ABI 段 :1293/:1305、DISP-007 ~:815；snr_estimator.h 实测 647 行（文档 624，+23）。§13.4 三条构建锚**全错**：根 CMakeLists 的 add_library astrocs_calibration 源清单实际在 **:807-810**（:767-770 为 photometry 源区）、noise OpenMP 实际 **:822**（:876 为 dpsf OpenMP）、eng/tests/unit/CMakeLists p1_noise_test 实际 **:722-728**（:619-623 非该目标）。正确的锚（:33/:92/:100/:113/:122/:133 等）仍准——「模块内新锚准、结构区旧锚漂」形态。
- 证据（含反方核验）：逐条 grep/sed 实测（本轮）；反方核验——文档中不依赖行号的常数与公式（kLn10、方差合成式）本报告未判错。provenance：§13.1 主体（938→1482、§13.1 系统漂移）**AUD-101-DA02:384/:734 已登记**且不在 PASS 表/任务台账，本轮复测仍开；**§13.4 三条构建锚为本轮新增**（DA02「13.4」6 处命中均为 PHOTOMETRIC_FIT §13.4 语境，非本节）。
- 建议改法：§13.1 表与头行数整体重锚（或改符号绑定走 anchor_contract）；§13.4 三条构建锚更正为实测行。登记为整改任务，随 DA02 既有处置执行。

### A5-R6  STAR_PSF_ALGORITHMS.md §11 逐符号锚表系统性失效 + 表内自相矛盾
- 文件:行：docs/science/algorithms/STAR_PSF_ALGORITHMS.md:150（头「dpsf_psf.cpp 934 行」）、§11.1 表（LM 调用 :320-321、compute_trimmed_mad :190-220、moffat4_fit_tmpl :225-407、C API :427、batch :482、f :580、free :599、d :612、f32 :694、MOFFAT4_FWHM_FACTOR :24）、§11.1/§11.3 OpenMP 锚（:528,635,738,**876** vs :528,635,738,**866**）、§1.2 snr_estimator.h :57-74、§11.1 orchestrator 布局B :2428-2464/row :2443-2451/dims :2453-2455/note :2456-2458。
- 所属面：④幻觉与锚 + ②行文逻辑。
- 问题描述：dpsf_psf.cpp 实测 **1146 行**（文档 934，+212）。本轮复开：§11.1 表多数行错位——LM 调用实际 **:374-375**（该文档 §3/脚注自己也写 :374-375，表与自注矛盾）、compute_trimmed_mad :222、moffat4_fit_tmpl :260、C API dpsf_fit :481、batch :556、f :683、free :703、d :716、f32 :814、MOFFAT4_FWHM_FACTOR 实际 **:25**（§2 写 :25 ✓、§11.1 写 :24 ✗）；OpenMP 四处实际 **:615/:740/:873/:1047**，而 §11.1 尾锚 876 与 §11.3 尾锚 866 **互相矛盾且都错**；orchestrator 布局 B 实测 row :2387-2395、dims :2398、note :2400-2402（文档 +45~+66）；snr_estimator.h PsfFitQualityRow 结构实测止于 :105（文档 :57-74）。§2 公式本身（FWHM=1.230307652590102、flux=2πA·sxsy/3、闭式 0.7316730952806134）本轮重推一致，不在本条。
- 证据（含反方核验）：逐条 grep/sed 实测（本轮）；反方核验——:25/:374-375 等「同一文档两处两说」为文内证据，非口径之争。provenance：**DA02:385/:735 已登记**「STAR_PSF_ALGORITHMS.md:150 dpsf_psf.cpp 934→1146 (+212)」核心事实，不在 PASS 表/任务台账；逐行表错位、:24/:25 与 876/866 内部矛盾为**本轮补全**。
- 建议改法：§11 逐符号表整体重锚（同一任务窗口内一并处理 OpenMP 尾锚两说），或改为符号绑定交 C4 门管。登记整改。

### A5-R7  STAR_DETECTION_ALGORITHMS.md:59-61 —— 99% 召回阈表与本文件 §11.4 及 GATES:58 三方不一致（门值口径冲突）
- 文件:行：docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:59-61（「召回域下限 = §11.4 F1 的逐档 99% 召回阈表（σ_psf = 1.0/1.27/1.5/2.0/2.5/3.0 px 对应 **46.0 / 24.0 / 19.0 / 16.0 / 10.0 / 10.0**）」）对照同文 §11.4:319-321（「∈**[52,65]** / 24.0 / **20.0** / 16.0 / **12.0** / 10.0」，并注「64 样本值已撤销、区间标定」）与 GATES_AND_TOLERANCES.md:58（G-P1-STAR-RECALL 同 [52,65]/24/20/16/12/10，「引用须同报区间」）。
- 所属面：②行文逻辑（单文档前后矛盾）+ ③跨文档冲突 + ①（门值口径）。
- 问题描述：:59-61 引用「§11.4 F1」却给出与 §11.4 正本**三格不同**的数值串（1.0px 档：单值 46.0 vs 区间 [52,65]，且 46 低于区间下限 52；1.5px：19.0 vs 20.0；2.5px：10.0 vs 12.0）。GATES 门表与 §11.4 一致 ⇒ :59-61 为孤立旧值。若下游按 §2 串引用，将放宽 1.0/1.5/2.5 三档召回门，且违反 GATES:58「引用须同报区间」；两处均自称同出 §11.4 F1，属「同一正本两处转引不一致」。
- 证据（含反方核验）：三方原文均本轮 sed 实测（:59-61、§11.4:317-321、GATES:58）；反方核验——§2 前句「50% 过渡点 6.22±0.50 不是召回域下限」的告诫本身正确，错在紧随的数值串；本文件 §11.4 自身明写「n=64 标定值系统性偏乐观，已撤销」，§2 串恰为被撤销形态。provenance：AUD-101-DA02 中「46.0」grep=0（已登记 GATES:58 但未抓本处转引错值）⇒ **本轮新发现**；总编对账对 STAR_DETECTION_ALGORITHMS 0 提及。
- 建议改法：:59-61 数值串改为直接指针引用 §11.4 表（不复抄数字），或逐格与 §11.4 对齐（[52,65]/20.0/12.0）。属引用对齐而非公式变更；若判定需改值，走上呈。**建议优先对齐方案**：因 §11.4 与 GATES 已互相印证，无需新裁决。

---

## 二、黄级（13）

### A5-Y1  GATES_AND_TOLERANCES.md:126 —— 表外自设「残余阶跃门 = ≤ 2× 参考水平」违自身 R1
- 面：②④。证据：§3 表内无该门（无门ID/统计量/证据ID/UNJUSTIFIED 标注）；所标依据 docs/ASTROCS_DESIGN §5.4 与 PHASE2_UPM 两处 grep 均无「2× 参考水平」阈值；:126 描述的 P1 三分支残差与 SCI-C 接缝场景口径不一。provenance：AUD-101-DA02 §1.12（:538 起）**已登记未闭环**（不在 PASS 表/任务台账），本轮复测仍开。
- 建议改法：按 R1 补门表行（门ID/统计量/来源）或标 UNJUSTIFIED；登记整改。

### A5-Y2  GATES_AND_TOLERANCES.md:127 —— 「0.1028″（典型）/0.3242″（最坏）」全仓无出处且与 §4a 表不一致
- 面：④②。证据：DA02:555-557 已 grep 确证 0.1028/0.3242 在全 docs/ 仅此一处；与 §4a 表内数（0.1584/0.1472/0.3588 等）不同族；未按自身 R2 标 UNJUSTIFIED。provenance：**DA02 已登记未闭环**，本轮复测仍开。
- 建议改法：补证据出处或标 UNJUSTIFIED；登记整改。

### A5-Y3  NOISE_ESTIMATION.md:213 —— 依据列两处失据：SCI 行号错 + GLOSSARY 条款幻觉
- 面：④（幻觉锚）。证据：①「SCI-NOISE-001 §9:91」——docs/science/NOISE_MODEL.md §9 实际在 :310（§5:58 正确、§9:91 错）；②依据「docs/GLOSSARY.md 禁两套定义」——grep GLOSSARY「两套/不可互换/唯一权威写法」= 0 命中，该条款不存在（唯一含「两套」的行即引用句本身）。provenance：本轮新增（DA02 对 :213 的记录是 1.4826 常数讨论语境，非本两处依据）。
- 建议改法：依据列更正为真实出处；不存在的条款删除或给出真实锚。

### A5-Y4  NOISE_ESTIMATION.md:216 —— defaults 引用括注 6/6 行号与 defaults.json source_ref 不符
- 面：④。证据：文档括注 patch_grid→:46、clip_sigma→:48、max_clip_rounds→:48、min_patch_samples→:37、spatial_field_enabled→:39、variance_floor→:21；defaults.json 实际 source_ref 行 = 54/56/56/52/47/23，6/6 不符；所指 NOISE_MODEL 行现为 :46 标题/:21 表头/:37 表行/:39 表行。provenance：本轮新增。
- 建议改法：逐键重锚到 defaults.json source_ref 行。

### A5-Y5  NOISE_ESTIMATION.md 生产 4 站点仍用 4 位 1.4826（域外事实登记）
- 面：④（事实提示）。证据：`lib/infrastructure/scheduler/src/module_adapters.cpp:10807`（const double sigma = 1.4826 * mad;）、`hips_browser/.../hips_sky_view.cpp:54`、`app/browser_cli.cpp:340`（1.4826f）、`baseline_provider_v1.h:52` 注释自称与 ABI-003/ALG-NOISE F1 同等——与全仓 1.482602218505602 条款并存。反方核验：这几处不属本切片 15 文档文本；PASS 表 1.4826 条款的既往整改域未含这四处。
- 所属面：④。建议：**登记上呈**（常数精度条款域归属，红/黄交界——本条按黄登记事实，是否红线由上位裁决）。

### A5-Y6  GAIA_QUERY.md §2 各节行锚系统性漂移（内容与常数全对）
- 面：④。证据（实测）：gaia_client.c 3141 行；§2.1 角距判定文档 :1242-1249/:1371-1377/:1510-1516/:2082-2088 → acos 受控点实测 **:1853/:1988/:2132**（各 +1 或更多）；§2.4 bbox :648-659 → 定义 **:1054**；§2.5 prune :661-734 → **:1108**；inv_scale 三处文档 :1282/:1411/:1550 → 实测 **:1817/:1952/:2096**；§2.3 :1212-1240 → :1830+。反方核验：magG 公式 prose 锚 **:1827 ✓**（double magG = mag_raw*0.001 - 1.5）、inv_scale 值 1/(3600·1000·500) 三处 ✓、C45=π/(2√2)≈1.1107 ✓、0.001/−1.5 ✓——**语义零错、行号漂**。provenance：AUD-101-DB-11 对本组锚号 grep=0、DA02「1853」=0 ⇒ 本轮新增。
- 建议改法：§2 各节锚整体重锚或改符号绑定。

### A5-Y7  HIPS_WRITER.md:22-26 行号锚声明失真 —— 「§4/§5/§9 已按修订后行号重标、漂移 +57 起最大约 +147」与实测不符
- 面：②④。证据：实测 aio_hips_writer.cpp 2874 行；`aio_hips_product_begin` 定义 **:1165**（§1 锚 :386-422，属声明的符号优先豁免区，但漂移 +779 远超声明上界 +147）；`fmt_sky_fraction` 定义 **:1805**、使用点 **:1892/:2056/:2412**（§5 锚 :1051/:1115/:1673 —— §5 属**声明已重标区**，仍漂 +754~+777）。声明把重标区描述为已按修订行号校准、C4 门不覆盖（anchor_contract 未声明 aio 符号绑定），实际重标不彻底。反方核验：§1-§3 裸锚有「以符号名为准」的显式豁免，该部分**合法**；本条只针对「已重标」声明区。
- provenance：DA02 有 fmt_sky_fraction 命中（格式唯一性主题），锚行号失真为本轮实测补证。
- 建议改法：修正声明（如实列未重标节）或把 §4/§5/§9 锚按当前行号真正重标；为 aio 文件在 anchor_contract 补 symbol binding。

### A5-Y8  PHOTOMETRIC_FIT.md —— snr_phot_cal_quality 锚文内两说且均错
- 面：②④。证据：§4 写 `noise_model.cpp:630-641`、§13.3 DISP-PHOT-009 写 `noise_model.cpp:305`；实测该函数定义在 **noise_model.cpp:1177**（:630-641 落在无关区）。反方核验：star_matcher.cpp 锚组（:21/:23/:25/:27、:545/:555/:558/:580/:597/:621、:616-621、:680-704、:486-501、:513-536 等）与 spectrum_integrator（:46/:130/:172 区间）本轮抽测**全部命中**，仅此一处错。provenance：AUD-101-DB-09 列有该符号上下文（本组两锚号为本轮实测）。
- 建议改法：两处统一更正为 :1177。

### A5-Y9  PHOTOMETRIC_FIT.md §13.5 与 §13.1 —— 旧 ABI 四符号行号文内矛盾（实测=§13.1）
- 面：②④。证据：§13.5（:210 上下）写 pc_calibrate_simple/:103、_f64/:185、_with_gaia/:153、_with_gaia_f64/:201；§13.1（:206）写 :175/:469/:510/:635。实测 pc_api.cpp（1336 行）定义 = **:175/:510/:469**（f64 定义 :510 ✓ 与 §13.1 一致）——§13.5 四值全错（偏 -72~-325），且 §13.5 的 :103 实为内部实现注释区。provenance：本轮新增。
- 建议改法：§13.5 改指针引用 §13.1 锚组或更正四值。

### A5-Y10  COSMETIC_ALGORITHMS.md §6 表与 §4/代码矛盾 —— 计数归约写成「omp reduction(+)」
- 面：②④。证据：§6（:265）「归约 | 仅 out_hot/out_cold 计数归约（**omp reduction(+)**，顺序不确定但整数加法可交换→结果确定）| §4」与 §6 并行列（:264「判定/**合并/清零**/插值/**计数**」）vs §4（:201-202）「计数是…单线程 O(n) 计数循环，**不是** omp reduction」。实测 cosmetic_corrector.cpp correct_frame（:230 起）：合并+计数单 for 循环**无任何 pragma**（串行）——§4 与代码对、§6 表错。附带：§1 判定 pragma 锚 :126,147 → 实测 :129/:150（偏 3 行）。provenance：本轮新增（D1残余未含本条；其「reduction」grep=0）。
- 建议改法：§6 归约/并行两行改为「合并与计数串行；并行仅判定/清零/插值」，锚随改。

### A5-Y11  CALIBRATION_ALGORITHMS.md:17/:614 —— 「14 个 AC_API 符号/14 符号」与实测 18 不符
- 面：④。证据：astro_calibration.h 实测 18 个 AC_API 函数（坏列族 6 个 + 12 legacy，头注释 :9 自证 12 legacy）；两处「14」既非 12 也非 18。provenance：本轮新增（与 A5-R4 同根，计数错误单列黄级）。
- 建议改法：两处更正为 18（或按「12 legacy + 6 坏列」分组表述，与 :9 注释一致）。

### A5-Y12  DRIZZLE_GEOMETRY.md —— §1 内两处行锚漂移
- 面：④。证据：:56「`acc.sumNorm`，:1629 前后」→ 实测 sumNorm 累加在 **:1644**；:69（§2 内）sumFlux 锚 :1628 → 实测 **:1639** 区域。反方核验：disp009 回归门名与 p1drz 相关引用本报告未判错。provenance：DA02 登记的是公式族，行锚漂移为本轮实测补证。
- 建议改法：随 A5-R1/R2 的 DRIZZLE 整改一并重锚。

### A5-Y13  STAR_DETECTION_ALGORITHMS.md:9 与 §11.1 —— 「sdet_api.cpp（2973 行实测）」实为 2962 行，§11.1 锚组系统 +11 漂移
- 面：④。证据：实测 sdet_api.cpp **2962** 行（文档 2973，−11）；符号锚实测：sdet_compute_bgnoise def **:451**（表 :462-518）、sdet_detect_impl **:1738**（表 :1749-2499）、sdet_dedup_stars **:853**（:864-982）、sdet_sort_stars **:972**（:983-995）、阈值组装 **:1771/:1772/:1779**（:1782-1792）——统一 +11 偏移；star_detector.h 110 行 ✓ 对。反方核验：reject_star :189-239（含 :214 def）✓、sdet_lm_fit :290 在 :285-460 内 ✓。provenance：**DA02:920 已登记**（且注 ALG-LINE-ANCHORS 门对该条已判红——机器门在管），本轮复测仍开；+11 组化证据为本轮补全。
- 建议改法：随 ALG-LINE-ANCHORS 门整改重锚（已有机器门跟踪，无需新台账）。

---

## 三、绿级观察（3，不计入违规）

1. **A5-G1** DRIZZLE_GEOMETRY.md:53-54 —— 行尾多余右括号「…另行分列引用）：」（纯行文小疵，无语义影响）。面②。建议顺手删括号。
2. **A5-G2** 事实提示（**不作为本切片违规重报**）：PASS 表把 `frame_photometry_fit.cpp:292` 的 1.4826 列为已修，HEAD 实测该行仍为 `out.zero_point_scatter_mag = 1.4826 * zmad;`——是否为「整改域外/以全精度条款另处覆盖」需上位确认，本报告仅如实呈报给总编。
3. **A5-G3** 文献核验抽查通过：Maples et al. 2018（REJECTION 引）经 arXiv 实查 = **arXiv:1807.05276**（作者 Maples/Reichart/Konz 等，Robust Chauvenet Rejection）✓；同批结果含 Konz & Reichart 的 RCR 后续文 ✓；Lindegren et al. 2021 DOI 10.1051/0004-6361/202039709 ✓；Rousseeuw & Croux 1993 DOI 10.1080/01621459.1993.10476408 与 CALIBRATION:624 的「该文非本仓 1.4826 来源」自注相互印证 ✓；其余（Huber 1964 DOI 10.1214/aoms/1177703732、Holland & Welsch 1977 DOI 10.1080/03610927708827533、Rosner 1983、Zackay 2016、Moffat 1969、Goldberg/Higham/Demmel&Nguyen、Fernique 2015、Górski 2005）为文献常识级 plausible，文档自标「文章级」核验状态，未发现虚构条目。

---

## 四、已查无问题面（逐面负面报告 + 抽查方式）

### 面① 科学性（对数/分歧台账/非退化判据）
**结论：除 A5-R1（DRIZZLE 权重参数化，登记上呈）与 A5-R7（召回门值引用冲突）外，未发现新的科学性问题。**
抽查方式：
- D-系列落位复核：全 15 文档 grep `0.104369 / 0.1043885 / 8.094e-2 / 1.152` = 0 残留（旧值仅存在于订正注/禁抄值语境）；`WCS_ADAPTIVE_MAX_DEPTH = 12`（spherical_overlap.cpp:859）与 D-02 一致；idw_power=1.0（rejection path）、c=4.685（PHOTOMETRIC F1-F7 全用 c=4.685 且注明 D-06）、N=5、k_shape（残留 3 处属 PASS 表 FAIL 项，不重报）均与终裁一致。
- 公式独立重推：STAR_PSF §2 FWHM=2√(2ln2)·s、flux=2πAsxSy/β(1,1/β)（β=4→2π/3）、Moffat 闭式 0.7316730952806134（与 1.230307652590102 关系重算 rel-diff 4.13e-7 自洽）；PHOTOMETRIC F1-F7（Tukey c=4.685、MAD/0.6744897501960817）；PLATESOLVE F1-F6（CD/3600、Y-down 符号、C45=π/(2√2)）；REJECTION 档界表（n<6 percentile / ≤15 winsorized / ≥16 + linear_fit 按 M3 改投）与代码 rejection.cpp 档界一致；GATES UNJUSTIFIED 行按 R2 正确标注（无伪装有据行）。
- 非退化判据抽查：COSMETIC 标度不变性负例（撤 1 热像素掩码变 1 像素）、PLATESOLVE bias 参与 oracle（删 bias 项须判红）、GATES 99% 召回负例、GAIA §5 「本文不声明任何测试已 PASS」——均符合「真值无效应⇒判红/不冒功」要求。

### 面② 行文逻辑（论证链断裂/单文档前后矛盾/订正注两说/UNRESOLVED 当结论）
**结论：发现 5 处（已全部入账）**：A5-R1/R2（DRIZZLE 文内两说）、A5-R7（STAR_DETECTION :59 vs §11.4）、A5-Y8/Y9（PHOTOMETRIC 双说）、A5-Y10（COSMETIC §6 vs §4）、A5-Y13 内部 876/866（归 R6 组）。其余文档抽查方式：
- 逐文档节内互指核对：GAIA（§2.6 引用 §2 各节自洽）、NOISE（§13.1/§13.2/§13.3 互指、DISP-PHOT 表与正文一致——除 Y8 所列）、UPM（订正注新旧两说并列且标注「原锚→现锚」，属合法订正注形态，D 系与行漂移订正注均新旧分明）、HEALPIX/GATES/REJECTION/ACR/PLATESOLVE/CALIBRATION（含 DISP-CAL-001「部分关闭+残留」式诚实登记）——未见「UNRESOLVED 被当结论」与断裂论证链。
- 订正注形态检查：DRIZZLE D-03 注、UPM 行漂移注均「<!-- 订正: ... --> 内嵌、旧对照保留」，与 PASS 表裁决形态一致。

### 面③ 跨文档冲突（五方权威链 L1 ↔ docs/ ↔ 实验/ ↔ 独立审计/ ↔ 源码）
**结论：发现 3 处（R1 含链③、R3、R7），其余抽查通过。**
抽查方式：
- L1↔docs：docs/ASTROCS_DESIGN §4.2 节点流程 vs 15 文档头部上游声明——15/15 对应；COSMETIC/CALIBRATION 与 SCI-CAL-001 参数表（hot_sigma/cold_sigma/method/max_structure_size）逐值一致（1.482602218505602、rtol/atol、掩码极性与 SCI :318/:346/§9a 三处逐值同——与 AUD-101-D1残余第 5 点独立互证）。
- docs↔docs：GATES↔STAR_DETECTION（发现 R7）、GATES↔PLATESOLVE/STAR_PSF 容差互指一致（0.05″ 等，PLATESOLVE §9 把非生产 IRLS 路径正确标 UNJUSTIFIED）、REJECTION↔PHASE2_REJECTION 档界两文一致（DA02 §1.4 亦判非重复）、UPM↔REJECTION 权重归属互证（DA02 §1.2/1.3 判自洽正面样例）、HIPS↔DRIZZLE（:130/:145 引用语义与 SCI-DRZ 一致）。
- docs↔源码：见面④；REJECTION 枚举（P2_REASON 0-3/P2_STATUS 0-7 ↔ rejection.h）、PLATESOLVE ipv_entry 7 个 API 锚 100% 命中、COSMETIC/CALIBRATION 模块内锚（:119/:127/:131/:230/:236/:242/:247/:196-197/:218-221）100% 命中、UPM（upm.cpp:286/:761-763/:778-780、sampler.cpp:855-859）100% 命中。
- docs↔独立审计：本轮每条红黄均回查 独立审计/证据/（DA02/D1残余/DB-03/DB-09/DB-11）标注 provenance；PASS 表与 06_实施 tasks 去重后才定级。

### 面④ 幻觉与锚（文献核验/file:line 逐条实开/「文档说有代码没接」）
**结论：锚问题是本切片最大弱点——发现 R4/R5/R6、Y3/Y4/Y6/Y7/Y8/Y9/Y11/Y12/Y13 共 11 条；文献与「代码有没接」反向检查未发现新虚构。**
抽查方式（实开统计）：
- 本轮实开/复开 file:line 锚 ≈190 条，覆盖全部 15 文档；命中率分层：**全对组**（PLATESOLVE ipv_entry/ipv_wcs、COSMETIC+CALIBRATION 模块内、UPM+sampler、PHOTOMETRIC star_matcher/spectrum/pc_api §13.1 组、GAIA 常数 prose 锚、GATES ctest/evidence 路径、HEALPIX D 系值）；**系统漂移组**（NOISE §13.1/§13.4、STAR_PSF §11、STAR_DETECTION §11.1、GAIA §2、HIPS 声明区、DRIZZLE §1 两处）——逐条行号与实测对照已列红/黄各条。
- 「文档说有、代码没接」反向核验：GATES:16 check_gates_and_tolerances.py 存在 ✓；PHOTOMETRIC run/SCI-FIX-PHOTFIT-01 与 run/SCI-PHOT-FORMULA-01 目录**均存在** ✓（AUD-101 曾记 ABSENT 的是其快照期状态，本轮不重报）；GATES ctest 证据名 8/8 在 CMake 注册 ✓；COSMETIC §8 「python/ 与 dll 不存在」自注与树一致 ✓；反向（代码有、文档没接）= A5-R4 坏列族。
- 文献：1 篇 arXiv 实查（Maples 1807.05276）+ DOI 常识级核验组（见 G3），未见虚构引用；全部 15 文档的参考文献节结构与许可证标注一致。

---

## 五、覆盖与方法附录
- 15/15 文档**全文通读**（含被 read 输出截断段的补读）；大表段落以符号名 grep 复核。
- 关键源文件实测：drizzle_engine.cpp、cosmetic_corrector.cpp、ac_api.cpp、astro_calibration.h、module_adapters.cpp、p1_session.cpp、sdet_api.cpp、dpsf_psf.cpp（行数/符号表）、noise_model.cpp（行数/符号表）、star_matcher.cpp、spectrum_integrator.cpp、pc_api.cpp、gaia_client.c、aio_hips_writer.cpp、upm.cpp、sampler.cpp、ipv_entry/ipv_wcs/ipv_solver/ipv_select.cpp、master_generator.cpp、calibrator.cpp、CMakeLists（根/子）、defaults.json。
- 未执行任何构建/测试/git 写操作；本报告为本切片唯一写入文件。
- 统计：红 7 / 黄 13 / 绿 3。其中已登记未闭环（DA02/D1残余 provenance）红 4、黄 3；本轮新发现红 3、黄 10。
