# 全仓-01 · 切片 S28：lib/algorithms photometry＋calibration＋cosmetic 对抗性静态检查

- **切片责任域（逐一覆盖）**：`lib/algorithms/photometry/` 全部（cpp/src 10 个实现与头、cpp/include、cpp/test、tests/p1phot 17 文件、wrapper_phase1、README/module.yaml/memory.md/docs）；`lib/algorithms/calibration/` 全部（src 11 文件、cpp/、include/、tests/p1cal、integration、README/module.yaml/CALIBRATION_PROCESS.md/V6_CALIBRATION_COVARIANCE.md/batch_config.json）；`lib/algorithms/cosmetic/` 全部（src/include/tests p1cos/integration/README/module.yaml）；互查面 = `docs/science/PHOTOMETRY.md`、`docs/science/CALIBRATION.md`、`docs/science/algorithms/PHOTOMETRIC_FIT.md`、`CALIBRATION_ALGORITHMS.md`、`COSMETIC_ALGORITHMS.md`。
- **方法**：纯静态只读；全部 file:行 锚逐条实开文件核对；红黄必附证据与反方核验；未运行构建/测试；零 git 写；本报告为唯一写入文件。
- **基线**：HEAD `52adba89`（工作树 clean，仅 untracked 独立审计/排查/）。
- **去重声明**：① 先读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（30 项 28 PASS）与观察项，一律不重报；D-01…D-11 与 §2 A-* 裁决不翻案。② 父指派明确排除：`frame_photometry_fit.cpp:292` 的 4 位 1.4826（上轮 PASS/勿重报）——本切片不作为问题登记，仅在"已查无问题面"记录核对事实。③ 与兄弟切片对齐去重：S5（docs/algorithms 甲组，红7/黄13/绿3）、S19（修复包①测光）、S13（exp-P1）、S18（exp-aux）、S3（scienceB）、S32（infra-gaia，登记了同型"状态陈旧"问题于 gaia 模块）、S27（noise/psf）——凡其已登记者本报告标 provenance 不冒充新发现。④ 与本域相关的既有登记：FIX_LEDGER M3-C-004/M3-C-009、DISP-PHOT-001..009、DISP-COS-001..011、DISP-CAL-001..011、A-P1-08/A-P1-12（分歧台账:113/:124）。
- **严重级统计：红 1 / 黄 12 / 绿 4**（其中黄含域外附带 1 条）。

---

## 一、红级（1）

### S28-红1 ｜ lib/algorithms/calibration/CALIBRATION_PROCESS.md:1、:56、:62、:73-77、:84（另 :12/:19/:31）｜面②＋③＋④

**问题描述**：模块生产源同目录的流程文档把「未接线/与冻结实现不同」的三套算法步骤写成现行流程，且**文件自身无任何历史/非规范标记**——既有审计 FIX_LEDGER M3-C-004（P1，**OPEN**）早已立案并给出整改方案，本轮复测全部断言仍成立、方案未执行。

**证据（逐条实开）**：
1. **K 搜索步骤不存在**：CALIBRATION_PROCESS.md:56「K 的初始值为 Light曝光/Dark曝光，**通过残差最小化（背景区域 MAD 最小）搜索最优 K 值**」——生产无此步：`src/dark_optimizer.cpp`（optimize_dark_k 定义 :97）**不在任何 CMake 源清单**（模块 CMakeLists.txt:20-25 CAL_PROD_SOURCES 仅 calibrator/master_generator/cosmetic_corrector/ac_api；根 CMakeLists.txt:589-598 同四件＋photometry_apply），声明处 astro_calibration.h:399，仓内唯一调用点是 `lib/infrastructure/aio/tests/hiss_correctness_test.cpp`（测试）；模块 README:162 自记「optimize_dark_k 未编译未接线」、README:35「K=t_light/t_dark 由调用方从 EXPTIME 得出」。
2. **坏点检测算法两说**：CALIBRATION_PROCESS.md:73-77「**局部统计检测**（5×5 中值滤波 + 残差 MAD）… 热像素: 残差 > hot_sigma×MAD（**局部**异常）… 孤立性检查: 8 个邻居中候选像素数 ≤ **max_neighbor_candidates**（默认 0）」——冻结生产实现为**全局**统计：cosmetic_corrector.cpp:124-127 `med=compute_global_median(dark,n)`、`sigma=1.482602218505602f*mad`、`threshold=med+threshold_sigma*sigma`（冷像素 :145-148 同构），随后是 **8 连通结构尺寸过滤** filter_by_structure_size（:62-114，`sizes>=max_size ⇒ mask=0`），无任何"孤立邻居候选数"判据——`max_neighbor_candidates` 全仓 grep **仅 CALIBRATION_PROCESS.md:77 自身一处**（配置死键的反面：连键都不存在）。权威文档 COSMETIC_ALGORITHMS §1 与 CALIBRATION_ALGORITHMS:309-310 均按全局阈值+结构过滤登记。
3. **修复方法两说**：CALIBRATION_PROCESS.md:84「bilinear: 用**双线性插值**替换坏点像素（**NaN 边缘用最近邻回填**）」——生产 method≠0 为 4 正交方向 1/dist IDW（cosmetic_corrector.cpp:202-224，`weight=1/dist`），代码注释与全链文档以 DISP-COS-003 登记「名义 bilinear、实为 IDW」，且统计路径**不滤 NaN**（DISP-COS-002，"NaN 最近邻回填"行为不存在）。
4. **±10s 匹配规则**：CALIBRATION_PROCESS.md:62「Dark 匹配: 按 EXPTIME 匹配最接近的 Master Dark（容差 ±10s）」——两通道均无 ±10s 校验（M3-C-009 已立案，本切片复测 module_entry/ac_api 侧亦无该键）。
5. 文件首行 :1 实为裸标题「# CCD/CMOS 标准校准流程」，无 ARCHIVED/历史标记（M3-C-004 要求项）。

**证据完整性（provenance）**：`artifacts/evidence/audit-2026-01/FIX_LEDGER.csv:181`（M3-C-004，P1，状态 **OPEN**，其列明复核时点 :56/:62/:73-77/:84/:12/:19/:31 与现文件逐一对得上）与 :186（M3-C-009）。本条为**已登记未闭环的复测上呈**，不冒充新发现。

**反方核验**：① calibration README:153 已把该文件标为「CALIBRATION_PROCESS.md（历史流程参考）」——但 ledger 明确要求**文件自身**标记，PROCESS 本体无；README 与 PROCESS 同目录但读者常直接打开 PROCESS。② CALIBRATION_ALGORITHMS/COSMETIC_ALGORITHMS 均声明权威归属，冲突在文档层可仲裁——但"照 PROCESS 实现"正是 ledger 认定的风险（K 优化、局部检测、8 邻居回填三处都会引入 §10 禁止项）。③ 3σ（:12/:19/:31）与生产 sigma-clip 默认语义一致（此三点非本条核心，列此证明确已逐点分辨）。

**建议改法**：沿用 M3-C-004 既有整改方案（文件首行加 `ARCHIVED_NON_NORMATIVE`/「历史流程参考，禁作实现输入」标记或移入归档目录，并按现行 SCI/ALG 重写 §1.2/§2.2/§2.3/§3 三节为现状描述）——方案已立案不另拟；本切片只登记上呈。

**所属面**：②（把未接线步骤写成现行流程、文件内无订正注记）；③（与 COSMETIC_ALGORITHMS/CALIBRATION_ALGORITHMS/生产源三处两说）；④（引用了 `max_neighbor_candidates`、K 搜索等不存在的键/行为）。

---

## 二、黄级（12）

### S28-黄1 ｜ pc_api.cpp:289-290/:291-323、:708-738、:978-1008（A-P1-08 代码侧）｜面④

**问题描述**：分歧台账 A-P1-08（台账:113）裁「订正清单收录」的两件事，**代码侧至今未处置**：(a) 三份同构 `static const double mag_max_arr[] = {12,...,16}` 与硬编码 `for (i=0;i<5)` / 终止条件 `i==4` 巧合耦合——向数组加第 6 档会**静默失效**（循环不进、终止条件错位）；(b) 首处注释 :289-290 仍保留终裁认定的**虚假上界**「直到星数在 2000-**10000** 范围或达 16.0 上限」（无任何 10000 实现，停止条件只有 `n_gaia>=2000 || i==4`，:314 行自注「含 > 10000 情况」与之矛盾）。

**证据**：三处实开——:291/:297/:315（run_with_gaia_f32_impl）、:708/:714/:731（f64 impl）、:978/:984/:1001（run_with_gaia_impl，v2 生产主路径）；05_正向规格.md:278 已把正向要求写死（「阶梯长度由数组尺寸推导（`std::size`），**禁止 i<5 与 i==4 两处字面量各写一遍**；注释声明的上界 10000 ⇒ 终裁确认为虚假注释，**处置定为删除该句**」）——正向规格与代码现状对表 = 要求**未落码**。01_缺陷清单 C5 与 photometry README:195 DISP-PHOT-005 已登记（登记不改码）。S19 报告:137 已就修复包侧登记「仅 05:278 半句覆盖」（provenance）。

**反方核验**：① 现阶段三份副本逐字相同（{12..16}/5/4 一致），**无现实行为差异**，故不升红；② 早停阈 2000≫拟合需求 3 星、1.0° FOV 贫场帧末档恰达阈值（路线2 附锚自洽）——当前数值语义无错；③ 05:278 同时给出正确锚 `pc_api.cpp:978-1008`，与本切片实开一致（说明修复包锚对、仓内两份文档锚错，见黄4）。

**建议改法**：按 05:278 与 M3 既有方案落码（数组尺寸推导循环上限＋删除 :289-290 的 10000 句）；整改归 P1-PHOT-IMPL，本切片只登记「未处置」。如上位认为该处置已逾期，可升红。

**所属面**：④（正向规格要求存在、代码未接线；同时是"改动静默失效"锚定缺陷）。

### S28-黄2 ｜ lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:10（头注释）、:165（锚）｜面②＋④

**问题描述**：父指派专项 A-P1-12（S07 缓冲 1.2 与钳位下界 1.0 的优先语义）——**代码侧语义已核验为自洽且与 05:266 钉死口径逐句一致**，但文件自身两处注释与之相抵：
1. 头注释 :10「4. FOV 半径 = pixel_scale * sqrt(W²+H²)/2 * 1.2, **钳位 [1,10] 度**」——无条件地宣称钳位 [1,10]，与实际**条件窗**语义不符；
2. :165 注释「与 **orchestrator.cpp:2771-2778** 同式同钳位」——行锚漂移（实测 orchestrator 同式段在 :2719-2727；:2771-2778 现为 malloc/snprintf 内存段）。

**证据（A-P1-12 代码侧优先语义，本切片核验结论）**：
- frame_photometry_fit.cpp:168-171 与 orchestrator.cpp:2722-2723：**缓冲 1.2 恒生效**（fov = scale·√(W²+H²)/2·1.2，无条件乘）；
- frame_photometry_fit.cpp:172-174 与 orchestrator.cpp:2724-2726：**仅当 fov≤0 或 fov≥30 时**才 `min(max(fov,1.0),10.0)` ⇒ 小画幅端 **0<缓冲值<1.0 时下界 1.0 不触发、缓冲值胜出**；fov≤0 时归整为 1.0；fov∈[10,30) 不触发上界。
- 与 05_正向规格.md:266「缓冲 1.2 恒生效；钳位仅在条件窗（fov≤0 或 ≥30）触发；小画幅端 0<缓冲值<1.0 由缓冲值决定、下界 1.0 不触发」**逐句一致** ⇒ A-P1-12 的优先语义在代码与 05 两侧均已钉死，无科学口径问题。
- 反方核验：05:265 的裁决编号错挂问题已由 S19 红-2 登记（不重复）；两处代码的公式/条件逐字符相同（"同式同钳位"内容为真，仅行号旧）。

**建议改法**：头注释改为「缓冲 1.2 恒生效；仅 fov≤0/≥30 条件窗内钳位 [1,10]（A-P1-12 优先语义，05 A-4b）」；:165 锚改 orchestrator.cpp:2719-2727。注释面订正，不触公式。

**所属面**：②（头注释与钉死语义两说）；④（file:line 锚漂移）。

### S28-黄3 ｜ lib/algorithms/photometry/README.md:157、:159-161、:162-167、:169、:197、:199＋module.yaml:137｜面④

**问题描述**：README §8「构建与生产接线（**实测**）」与 DISP-PHOT-007/009 的外部 file:line 锚**系统性漂移**（行为断言全部为真，锚全部为旧）。

**证据（逐条实开对照）**：

| README 声称锚 | 实测位置 | 实开核对 |
|---|---|---|
| :162 `orchestrator.cpp:2474` run_stage_photometric | **:2502** | :2474 现为 PSF 段语句 |
| :164 fn `..._f64_v2（:2714）`、`_v2（:2790）` | **:2750**（get_function f64）、**:2826**（get_function v2）；调用 :2784/:2859 | :2714 现为 read_wcs lambda 段 |
| :165 读 psf 块「:2560 起」 | **:2590** `psf_block = fn_get_block(frame_,"psf")` | :2560 现为 data 块读取 |
| :166-167 写 photo_stats KV「:2902-2935」 | **:2949-2971**（:2949 注释、:2950 STATUS 起、17 诊断字段 :2955 起） | :2902-2935 现为 photometric_match 权威块 |
| :169 `snr_estimator.h:47-48` / `noise_model.cpp:276` | 声明 **snr_estimator.h:77**、定义 **noise_model.cpp:1177** | h:47-48 与 cpp:276 现为 patch-R 统计注释，与该函数无关 |
| :197 DISP-PHOT-007 `orchestrator :2714-2831` | 实际双通道段 ≈**:2750-2863** | 起点错 36 行 |
| :197/:module.yaml:137 `module_adapters.cpp:469-486`（descriptor 占位 ID） | :469-486 现为 TAN 投影参考函数；photometry descriptor 行 `sci_id="SCI-P1-PHOT-001"` 在 **:948**（descriptor 区 :703-1033） | 偏 ~480 行 |
| :159-161 `dll_loader.cpp:40/:54/:271-281` | **:58**（photometric_calib 文件名表）、**:72**（子目录表）、**:289-305**（gaia 预加载） | 三处均旧 |

另 :157「根 CMakeLists.txt 无 photometric_calib 目标，**grep rc=1**」——「无目标」仍真（无 add_library），但 grep 现返回 1 命中（根 CMake:760 注释行），"rc=1"表述已不成立。

**反方核验**：上述每条行为断言（stage 必需、双通道、psf [N,9]、photo_stats 17 字段、QA 下游落位 snr_phot_cal_quality、dll 装载表与 gaia 预加载）**逐一在实测位置成立**——纯锚漂移，无口径错；§8 其余锚（Makefile:11、build.ps1:9、README:22 entrypoint 零调用、module_entry.cpp:970-998 分派段）实开相符。兄弟切片 S5 未覆盖 lib 模块 README（其域为 docs/algorithms），S3/S7/S8 亦未登记本组。

**建议改法**：按实测位置整体重锚 §8 与 DISP-PHOT-007/009（一次性重锚，行为句不动）；「grep rc=1」改为「grep 仅命中 :760 注释、无构建目标」。

**所属面**：④（file:line 锚逐条实开不符）。

### S28-黄4 ｜ README.md:195 与 docs/science/algorithms/PHOTOMETRIC_FIT.md:224-225、:247-249（DISP-PHOT-005 锚族）｜面②＋④

**问题描述**：DISP-PHOT-005 在两份文档中的四个锚全部指错位置，且 PHOTOMETRIC_FIT **同文两说**（:206 给出正确锚、:224/:248 给出错误锚）。

**证据**：
- 「自适应 mag_max_arr（**pc_api.cpp:836-866**）」（README:195、PHOTOMETRIC_FIT:224、:248）——实开 pc_api.cpp:836-866 = f64 legacy 的 matchWithKdTree→cleanAndScale→correctImage 段，**无阶梯**；三处阶梯实际在 **:291-323 / :708-738 / :978-1008**（生产主路径 :978-1008）。同文 PHOTOMETRIC_FIT:206 却写「自适应锥搜 mag_max_arr{12..16}×5 **:977-1006**」（正确）⇒ 同一文档对同一对象两个锚（面②）；05:278 亦为 :978-1008（修复包侧正确）。
- 「:758 形参注释 /*mag_max*/」（PHOTOMETRIC_FIT:248）——:758 现为 `fprintf("警告: DB光谱点数...")`；`/*mag_max*/` 实际在 :217/:899 等 impl 签名。
- 「QE 三参数 (void) 丢弃（**:103**，头注释 **:85-88** 声明保留）」（README:195、PHOTOMETRIC_FIT:249）——`(void)qe_wl...` 实际在 **:117**，声明保留的头注释在 **:114-116**；:103 位于 no-PSF 退化分支 `*out_scale_factor=1.0`，:85-88 位于 n_gaia 参数校验（且均在 run_simple_impl :43 内、pc_calibrate_simple 符号 :175 之前，锚不可能落在其"头注释"上）。

**反方核验**：① 缺陷本体（mag_max 入参被阶梯覆盖、QE 被丢弃）**属实**（签名 `double /*mag_max*/`、`(void)qe_*` 实开为证）——错的只是锚与行段；② S5-A5-Y9 已登记 PHOTOMETRIC_FIT §13.5 的**另一组**四符号锚（:103/:185/:153/:201→:175/:469/:510/:635），与本条的 §13.3/§13.2 载体不同、不重复计数；③ module.yaml:129 同缺陷无行锚，不受影响。

**建议改法**：两份文档的 DISP-PHOT-005 行统一改锚（阶梯 :978-1008；QE :117/:114-116），并删去 PHOTOMETRIC_FIT:224/:248 的 :836-866（保留 :206 正确锚，消除同文两说）。

**所属面**：④（锚错）＋②（同文两说）。

### S28-黄5 ｜ docs/science/algorithms/CALIBRATION_ALGORITHMS.md:174、:318、:325、:326＋模块 CMake 注释（calibration CMakeLists.txt:12/:19/:51、cosmetic CMakeLists.txt:38-39/:71）｜面④

**问题描述**：CALIBRATION_ALGORITHMS §13 表的构建/调用方/声明锚漂移；同源的"根 astrocs_calibration :333/:338"注释锚在两个模块 CMakeLists 里同样漂移。

**证据（实开对照）**：
- :325 构建锚「**CMakeLists.txt:503-523**」（astrocs_calibration 目标）——根 CMake :503-523 实为 zlib/cfitsio 配置段；目标实际在 **:589-609**（`add_library(astrocs_calibration STATIC` :589、include :599、OpenMP :606-608）。
- :326 生产调用方：`p1_session.cpp:372` → 实测 ac_calibrate_frame 调用 **:378**；`module_adapters.cpp:2220` → `p1_op_calibrate` 实际 **:2285**；（`p1_session.cpp:462-465`/ `module_adapters.cpp:2355` 的 cosmetic 侧 → 实测 :481 / :2749——**同源锚 S5-A5-R3 已就 COSMETIC_ALGORITHMS:214-217 侧登记**，此处登记 CALIBRATION_ALGORITHMS 载体，不重复计数）。同段 `module_entry.cpp:970-998`（master 分派）实开**相符** ✓、`README.md:22`（entrypoint 零调用）实开**相符** ✓。
- :318 FP64 ABI「头文件 **105-115** 声明」——astro_calibration.h:105-115 为 f32 `ac_calibrate_frame` 尾部/坏点注释头；f64 声明实际在 **:330-355**（generate_master_*_f64 :330/:336/:342、correct_frame_f64 :355）。同格 ac_api.cpp:147-263 实测覆盖 f32 correct+f64 masters+calibrate_f64（:147 起点偏早、:263 恰为 ac_calibrate_frame_f64 定义行）——行段近似可用。
- :174「C API 层先校验并返回 AC_ERR_PARAM（**ac_api.cpp:100-101**）」——:101 是 `ac_generate_master_bias(` 签名行，校验在 **:106-107**（dark 在 :118-119）。
- 模块注释：calibration CMakeLists :12/:19 引「根 astrocs_calibration **:333**」、:51 引「**:338**」；cosmetic CMakeLists :38-39/:71 同款——实际 **:589/:599**（两份注释同源同错，偏 ~256 行）。

**反方核验**：算法主体锚全对——`master_generator :67-164/:84-88/:91-97/:49-60/:106-118/:38-45`、`calibrator.cpp:50-64`、`cosmetic_corrector :119-135/:140-156/:127/:131/:148/:152/:61-113/:85-97/:103-106/:108-113/:110` 抽验**逐条命中**（含 ±1 行段宽窄）；COSMETIC_ALGORITHMS:8 引 `calibration/CMakeLists.txt:20-25` 实开为 CAL_PROD_SOURCES ✓（S5 黄组亦判两模块算法锚 100% 命中）——漂移仅限 §13 的构建/调用方/声明格与 CMake 注释。

**建议改法**：§13 表按实测重锚；两份模块 CMakeLists 注释的 :333/:338 → :589/:599（纯注释锚，不触构建）。

**所属面**：④。

### S28-黄6 ｜ docs/science/algorithms/COSMETIC_ALGORITHMS.md:9-10｜面④

**问题描述**：文档头「C ABI 导出 `ac_api.cpp:108,228`，签名权威 `astro_calibration.h:97,142`」四个锚全错。

**证据**：cosmetic 的 C ABI 导出实际为 `ac_correct_frame` **ac_api.cpp:155**、`ac_correct_frame_f64` **ac_api.cpp:277**（:108 = ac::generate_master 调用行、:228 = out_n 计算行）；签名权威实际为 `ac_correct_frame` **astro_calibration.h:124-130**、`ac_correct_frame_f64` **:355-361**（h:97 = ac_calibrate_frame 注释「dark_optimization: 0=关闭」、h:142 = ac_correct_columns_ex 注释「neighbor_k」）。行段整体偏移 +47/+49（ac_api）与 +27/+213（h）——典型旧版行号残留。

**反方核验**：同文件 :23-24 的导出**符号名**正确（`ac_correct_frame`/`ac_correct_frame_f64`），:8 的 CMake 源清单锚（calibration/CMakeLists.txt:20-25）实开正确；S5 全对组覆盖的是 cosmetic_corrector.cpp 内锚，不含本组——不与 S5 重复。

**建议改法**：改锚 :155/:277 与 :124/:355（或改为符号名+文件，免行号漂移）。

**所属面**：④。

### S28-黄7 ｜ COSMETIC_ALGORITHMS.md:6-11（:11）、:411＋lib/algorithms/cosmetic/module.yaml:6｜面③＋④

**问题描述**：cosmetic 文档两处「尚未建立」陈述已被实况反证，且与同模块 README 的 LEDGER-DOC 复测注记两说：
1. COSMETIC_ALGORITHMS:11「迁移目标目录 lib/algorithms/cosmetic/（落码由 P1-COS-IMPL 执行，**尚未存在生产符号**）」——实况：`src/module_entry.cpp` 1262 行已在位，**astrocs_module_query_v1 定义于 :1246**，CMake SHARED 目标 astrocs_p1_cosmetic（cosmetic CMakeLists:50）由根 :309 add_subdirectory 注册，def/version-script 双白名单在位；README:5/§15/:24 以 LEDGER-DOC 注记明文「已由 P1-COS-IMPL 落地在位…『仅合同文件/无源码/尚未存在』等表述已被实测反证」——**README 自己点名"尚未存在"为已反证旧基线，而 COSMETIC_ALGORITHMS:11 原句仍在且无同款注记**。
2. COSMETIC_ALGORITHMS:411「可执行 TEST-P1-COS-001 **由 P1-COS-TEST 建立**（将来时）」与 module.yaml:6「可执行测试由 P1-COS-TEST 建立」——实况：`lib/algorithms/cosmetic/tests/p1cos/` 12 文件在位，其 CMakeLists 头自记「P1-COS-TEST (TEST-P1-COS-001) — 可复用验证面」，并已由 `eng/tests/unit/CMakeLists.txt:564` add_subdirectory 注册（ctest 面 p1cos_units/properties/negative/cosmetic/performance/selfcheck）。module.yaml 自身还两说：:2-4「模块化迁移**已由 P1-COS-IMPL 交付**」vs :6「测试**由…建立**（未建立语气）」。

**反方核验**：① "尚未存在生产符号"若读作"cosmetic 目录内无 AC_API 科学导出"仍真（科学实现居 calibration/src，7 个 source_symbols 在 module.yaml:55-62 如实登记）——但"生产符号"对模块目录的自然读法已被 query_v1 反证，且 README 注记明文把它列为已反证表述；② module_status=CONTRACT_READY 本身合规（状态词口径=docs/ASTROCS_DESIGN §12.5，不声明 IMPLEMENTED——**非本条指控对象**）；③ 同型问题 S32 已在 gaia 模块登记（provenance of pattern），本条为 cosmetic 实例。
- **域外附带**：同 unit 在 `eng/packaging/astrocs.product.json:14-15` 标 `status:"IMPLEMENTED"`（note 称 MOD-001 安装加载验证），与 README「机读 NOT_IMPLEMENTED（noop_entrypoint）」并立——两机读源判据不同（安装加载 vs entrypoint 零调用），README 把后者冠以「MOD-001 检查器」而 product.json 把 MOD-001 写成安装加载验证，判据归属需上位澄清（eng/packaging 域外，登记不改）。

**建议改法**：COSMETIC_ALGORITHMS:11 补 LEDGER 同款现状注记（或改为「迁移目标目录，adapter 已落码、算法源仍在 calibration」）；:411 与 module.yaml:6 改为「已建立（tests/p1cos，注册于 eng/tests/unit/CMakeLists.txt:564）」。

**所属面**：③（模块内 README/module.yaml/算法文档三处状态口径不一）＋④（说没有、实际有）。

### S28-黄8 ｜ lib/algorithms/calibration/V6_CALIBRATION_COVARIANCE.md:8-20（§1 权威锚表）｜面④

**问题描述**：§1 标题为「权威锚（冻结，逐条符合）」，但表中 3 行锚向**仓内不存在的文档**、2 行使用**非注册条款 ID**——"逐条符合"不可核。

**证据**：
1. `ALG-P1-001 §2.1 / §2.2 / §2.4`（:12/:13/:23 行）——全仓检索（排除 run/独立审计/artifacts）：ALG-P1-001 仅出现于 DATA_SEMANTICS:3388（owner 字段）、QA_MATRIX（owner）、v6_clause_registry（anchor/owner 字符串）、v6 代码/文档自引——**无任何定义该条款的在库文档**（docs/ 下零命中为定义文；05_正向规格亦无该 ID）。注册表自身的 open-item 锚（OI-02「ALG-P1-001 §2.2」、OI-03「§2.4」）同样悬空 ⇒ 若 ALG-P1-001 属 W3 待交付件，此为**链级缺件**；若属幻觉锚，V6 核心公式面的三行"权威锚"失效。
2. `CAL-UNIT`（:19）、`CAL-NO-CLIP`（:20）——非任何合同层条款：docs/contracts、eng/contracts/data/v6_clause_registry_v1.json 均无此 ID；仅 v6 模块自引（V6 文档、v6_calibration_covariance.cpp:189、测试 v6_cal_covariance_test.cpp:8/:10）。真实对应条款是 `FZ-UNIT-WINFO`（单位律）与 CAL-NO-CLIP 想表达的"无 clamp"语义对应的合同行。
3. **与 05_正向规格对表（父指派）**：05（315 行，§0–§A-8）grep `covariance|协方差|C_out|propagation|ALG-P1-001|FZ-FORMULA` **零命中**——05 对表面**不存在**协方差条款（其 §4–§7 截断缺失系 S19 红-1 已立案），故 v6 侧对表面只能落在本文件 §1；而 §1 中可核的行（FZ-FORMULA-COV-PROP、ADJ-OBS-01、FZ-PROV-SHARED-SYSTEMATIC、FZ-CAL-FLOOR、FZ-CAL-QUANTUM-DEFAULT、OI-02、OI-03）**全部在册** ✓（DATA_SEMANTICS:2692/:3294-3295/:3367/:3439-3443、registry:1965/:1990/:2500/:2508、adjudications SUMMARY:13）——即 11 行中 6 行可核、3 行悬空、2 行本地标签。

**反方核验**：实现侧与可核行**逐项相符**——J=[1/f,−(1−α)/f,−α/f,−y/f] 折叠式实开于 v6_calibration_covariance.cpp:307-334（同 master 折叠 :324、双独立项 :327/:398）；`max(f,0.1)`+flat_floor_applied、q²/12（:146-147/:271-276）、validate_covariance_record(:697)、evaluate_shared_gate(:526) 均与 §1-§3 描述一致；FZ-CAL-QUANTUM-DEFAULT 在 registry:1990/:3159 在册 ✓ ⇒ 问题只在"锚的可核性"，非实现正确性。

**建议改法**：§1 表加"锚源"列（registry 条款 vs 本地标签）；ALG-P1-001 §2.x 三行**上呈**——若该规格未入库属缺件，登记交付欠账；CAL-UNIT/CAL-NO-CLIP 改指真实合同条款（FZ-UNIT-WINFO 等）或标注"本文件局部标号"。涉及条款体系，只登记上呈不代改。

**所属面**：④（幻觉/悬空锚专项）。

### S28-黄9 ｜ V6_CALIBRATION_COVARIANCE.md:4（状态行）｜面③

**问题描述**：「状态：实现完成，**未接线 Phase session**（lib/phase1/、lib/phase1_session/ 未改）」未登记该实现的**唯一生产消费点**——读者易读作"无人消费"。

**证据**：`lib/algorithms/integration/v6_phase1/src/phase1_product.cpp:32`（using CovarianceRecord）、**:367**（`calibration::v6::calibrate_pixel`）、**:373-376**（`make_calibration_covariance_record`+`validate_covariance_record`）——即 V6 Phase1 产品装配链已消费 v6 协方差；构建在位：v6_phase1/CMakeLists:13-15 `add_library(astrocs_v6_phase1_product STATIC src/phase1_product.cpp)`、根 CMake **:1116** `add_subdirectory(lib/algorithms/integration/v6_phase1)`；integration/v6_phase1/README 自记「状态：IMPLEMENTED（本任务只做接线）」且职责第 1 条即「校准 covariance（astrocs::calibration::v6，IMPL-P1-CAL-001）」。

**反方核验**：① 状态行括注**限定了** lib/phase1/、lib/phase1_session/ 两目录，字面为真（该两目录确未改）——故不升红；② 消费点在审计证据 独立审计/证据/通读-CR-54.md:103 已登记（非未发现，而是文档状态行未更新）；③ legacy session（p1_session）确实零调用 v6 头（grep calibration/v6_calibration_covariance.h 全 lib 仅 phase1_product.cpp 消费）✓。

**建议改法**：状态行补一句「唯一生产消费点 = integration/v6_phase1/phase1_product.cpp:367/:373（legacy lib/phase1_session 未接）」。

**所属面**：③（状态口径与接线实况两说）＋④（"文档说没接、代码已接"反向专项）。

### S28-黄10 ｜ lib/algorithms/calibration/README.md:12、:69＋lib/algorithms/cosmetic/README.md:14｜面④

**问题描述**：两份模块 README 把权威签名源写作 `lib/include/astro_calibration.h`——**该文件不存在**。

**证据**：`ls lib/include/` 仅含 astrocs/ 目录；`lib/include/astro_calibration.h` 实开 not found；工作树 clean（git status 仅 untracked 排查目录）⇒ HEAD 亦无。唯一真身 = `lib/algorithms/calibration/include/astro_calibration.h`（405 行）——CALIBRATION_ALGORITHMS:17 与 COSMETIC_ALGORITHMS:10 用的正是正确路径 ⇒ **同库两说**。构建侧用的也是真路径（模块 CMakeLists:54、根 :600 include 到 calibration/include）。
（cosmetic README:13 同句还带旧锚「CMakeLists.txt:321-333」→ 根 astrocs_calibration 实际 :589-609，一并纳入。）

**反方核验**：README:4-9 的 LEDGER-DOC 注记覆盖词表为「仅合同文件/无源码/无 CMake target/entrypoint=MISSING/尚未存在」，**不含路径类失效引用** ⇒ 不构成已登记豁免；三处引用均为"以其为准"的权威指向，路径失效会使核对者找不到文件。

**建议改法**：三处路径改 `lib/algorithms/calibration/include/astro_calibration.h`（calibration README:69 的"14 AC_API 符号"表述随真身头复核）。

**所属面**：④。

### S28-黄11 ｜ lib/algorithms/photometry/tests/p1phot/p1phot_fixgates.cpp:184-201（对照 :22-29 头声明）｜面①

**问题描述**：B4-2 门 2（`|r_inliers| ≥ 2 才估 sigma_residual，否则 =0`）的**判别分支在当前管线序下不可达**，回落分支 `CHECK(sigma >= 0.0, ...)` 为结构恒真——与文件头「判别力：每个必拒用例都配同构对照，**故不是恒真/恒假断言**」的声明相抵；该冻结门（SCI-PHOT-001 §4/§8 的第二半）在套件内**无可执行证据**。

**证据**：
- :184-201「B4-2 门 2 直接判别」：构造 m[2].f_instr=1000·10^2.90 使 r_2=2.90——**可执行路径只可能走 else**：该构造下 median=0.1、MAD=0 ⇒ S=0 ⇒ IRLS 全收（:563 `S<=0 → all inliers`）⇒ out.size()=3 ≥2 ⇒ 断言落到 `sigma >= 0.0`，而 sigma_residual=`mad_in>0? ...:0` 结构 ≥0（仅 NaN 为假）⇒ 恒真。若分支真被触发（inliers<2）才断言 sigma==0——**恰是测不到的那一半**。注释自认「m[2]…**数学上不可达**（mag_tolerance 未命中，构造很难给 <2）」。
- 门 2 的可达性疑为结构性：门 1 先要求 `r_consistent ≥ 3`（star_matcher.cpp:518-535，可达，b42_two_stars_rejected/three_stars_pass 对照实证 ✓），而 Tukey 固定尺度掩码对 n=3 最多剔 1、对含中位支配的样本组剔不到 n−2（MAD=中位支配）⇒「≥3 进、<2 出」疑不可达——若属实，代码里的该门是**冻结条款死分支**，测试的自适应断言只是把它包装成绿。

**反方核验**：① 该 TU 其余断言（ABI 地址锁 :96-134、门1 双向对照 :146-182、B4-3 质量位三组+对照 :203-265）均非恒真、判别力实证 ✓；② 文件头对门 2 的不可达性有诚实自注（非隐瞒）；③ p1phot N1-N9、p1cal、p1cos 的负例与自检面非退化（见"已查无问题面①"）——问题仅此一处。

**建议改法**：二选一上呈：(a) 给 `|r_inliers|<2` 构造可达注入（若能）；(b) 若数学上不可达，把该门登记为冻结条款死分支（SCI 死条款属科学面，**只上呈不代改**），测试注释把 `b42_sigma_needs_two_inliers` 的 else 分支明示为"结构断言、非门证据"。判据非退化纪律：恒真断言没有证据资格。

**所属面**：①（负例与判据非退化）＋②（头声明与实际断言力两说）。

### S28-黄12（域外附带）｜ docs/science/algorithms/NOISE_ESTIMATION.md:213｜面①

**问题描述**：MAD→σ 常数冻结行「11 位简写 `1.4826022185` 与 4 位 `1.4826` 只作约等于语境（**实测绝对差 5.602e-12、相对 3.779e-12**）」——括注数值**只对 11 位简写成立**，把两简写并列后括注按联合读法把 4 位的截断差写成 3.779e-12，**低估 6 个数量级**（4 位实差：绝对 2.218505602e-6、相对 1.4963e-6）。

**证据**：独立复算 1.482602218505602−1.4826=2.218505602e-6（rel 1.4963e-6）；1.482602218505602−1.4826022185=5.601963337653615e-12（rel 3.7785e-12）——后者与该行括注逐位吻合 ⇒ 括注实为 11 位的量。旁证三方一致：分歧台账 A-P1-08 自述「**1e-6 量级**」；wrapper_phase1/photometer.cpp:91 注释「相对差 **-1.50e-6**」；实验 redo_summary（S13:134 引）「1.496e-6」。git 追溯该句由 `c4af4136`（文档全仓正向化）写入——其修复计划行原文为「截断值只保留在明确标注 **11 位简写 / 7 位简写** 的约等于语境并给出实测差（5.602e-12 / 3.779e-12）」，落稿时"7 位简写"被替换为"4 位 1.4826"而括注数值未换。

**反方核验**：① 若括注只修饰"11 位简写"（语法上可作此读），则 4 位的实测差在该冻结行**缺失**——两读法各有缺陷；② 该行的**规范性指令**（4 位只作约等于语境、与本层冻结值不可互换）本身无误，S5-A5-Y3 只登记了该行**依据列**（SCI 行号/GLOSSARY 条款）失据、未覆盖本括注 ⇒ 不与 S5 重复；③ 本切片互查清单外文件（NOISE_ESTIMATION 属 S5 责任域），按"域外但确凿"登记。

**建议改法**：括注限定为「11 位简写实测差 5.602e-12/3.779e-12；4 位实测差 2.2185e-6/1.4963e-6」或拆成两句——常数量级订正属冻结行文本，随 S5-A5-Y3 一并上呈，不代拟定稿。

**所属面**：①（常数量级）＋②（并列结构歧义）。

---

## 三、绿级（4）

### S28-绿1 ｜ star_matcher.cpp:26 ｜面②
`// IRLS 收敛阈值 (|scale_new - scale_old| < 1e-6)`——实际收敛判据是 **location**：:577 `diff = fabs(new_location - prev_location)`、:580 `diff < _IRLS_CONVERGE`；文档 PHOTOMETRIC_FIT:133 按 location 记载（文档对、注释旧）。可不改；随下次触碰该文件顺手改为 location。

### S28-绿2 ｜ wrapper_phase1/photometer.cpp:90-91 ｜面②
注释「MAD→σ 冻结常数（…；**原 4 位截断 1.482602218505602** 相对差 -1.50e-6…）」——1.482602218505602 是全精度值不是 4 位截断，句式漏掉旧值 `1.4826`；数值 -1.50e-6 正确（与黄12 旁证一致）、代码用全精度 ✓。补一个"1.4826→"即可。

### S28-绿3 ｜ README.md:191（DISP-PHOT-001 行）｜面④
登记的失实注释锚「star_matcher.cpp:4-6/:34-38(头)」已不含该句（:4-6 现为功能清单、:34-38 为 percentileOf；:177 注释已更新为"双向最近邻唯一配对"）；失实注释**仅存 photometric_calib.h:32/:34**（"当前无双向过滤 / rejected_ambiguous 保持 0"）而 rejected_ambiguous 实际在 star_matcher.cpp:343 赋值。缺陷本身已按 DISP-PHOT-001 登记（登记不改码）——只需把行内锚裁剪到 :32/:34 两处。

### S28-绿4 ｜ docs/science/algorithms/PHOTOMETRIC_FIT.md:206 ｜面④
§13.1 主路径行段两处宽窄近似：「run_with_gaia_impl<T> **:896-1210**」实为 **:895-1186**（:1188 namespace 收、:1193 起为 v2 wrapper）；「参数校验 **:941-963**」实为 **:926-946**（起点偏 15 行、终点落进 no-PSF 退化段）。同行其余 8 个锚（:1011 无光谱星、:1054 滤光片-QE、:1071-1096 F_syn OpenMP、:1113-1122 匹配清洗、:1127-1167 逐星记录、:1168-1173 像素校正、:884-894 make_dr3sp_id、:977-1006 阶梯）实开**全部命中**；S5 已判该组命中，本切片仅记录行段宽窄差异，不立黄。

---

## 四、计数与登记台账

| 级别 | 条数 | 编号 |
|---|---|---|
| 红 | **1** | S28-红1 |
| 黄 | **12** | S28-黄1…黄12（黄12 为域外附带） |
| 绿 | **4** | S28-绿1…绿4 |

- 新发现：红1、黄1-7、黄9-12、绿1-4（黄8 含链级缺件嫌疑）；已登记未闭环复测：红1（M3-C-004 P1/OPEN）、黄1（A-P1-08＋01 C5＋S19 provenance）；与兄弟切片交叉注记：黄5 的 cosmetic 调用方锚（S5-R3 同源）、黄4 与 S5-Y9 同族不同载体、黄7 与 S32 同型不同模块、黄12 与 S5-Y3 同行不同点。

---

## 五、已查无问题面

### ① 科学性（除已列黄11/黄12 外查净）

- **常数与公式对数（D 系终裁不翻案）**：`_TUKEY_C=4.685`（star_matcher.cpp:23、spatial_gain kTukeyC）↔ PHOTOMETRY.md:31/:207/:268/:295（D-06 维持，Kafadar 一手锚本切片不复核）；`_MAD_SCALE=0.6744897501960817`（:21）↔ PHOTOMETRY:221 逐位一致；IRLS 50 次/收敛 1e-6（:25/:27）↔ PHOTOMETRIC_FIT:18；mag_tolerance=3.0（调用点 pc_api:427/:597、star_matcher.h:72 默认）↔ PHOTOMETRIC_FIT:18；match_radius=2.0（pc_api:137/:427/:592/:835/:1115 五点＋star_matcher.h:71 默认）↔ PHOTOMETRIC_FIT:231 **全对**；F_syn 积分步 1.0 nm（spectrum_integrator.cpp:250）↔ PHOTOMETRIC_FIT:218；`1.482602218505602` 全精度写法（cosmetic_corrector:126/:147、calibrator/median、photometer:92、spatial_gain:198）↔ NOISE_ESTIMATION:213 唯一全精度写法 ✓；`1.166=√1.361` ↔ PHOTOMETRY:403/:428（A-P1-01 裁决口径一致）。**1.2533/√n：本域代码零消费**（lib/algorithms grep 无命中；其定义与消费在 NOISE_MODEL:301/02 式-4/修复包，属 S3/S19 域）——任务提示的"与 PHOTOMETRY.md 一致"在本域为空真、未发现冲突可报（PHOTOMETRY.md 全文亦无 1.2533，与分工一致）。
- **S07 缓冲 1.2/钳位 1.0（A-P1-12）**：代码侧优先语义核验通过（详见黄2 证据），frame_photometry_fit 与 orchestrator 两处**逐字符同式同钳位**、与 05:266 一致——除头注释（黄2）外无口径问题。
- **cosmetic/calibration 算术语义**：掩码 1=坏点（cosmetic_corrector:131/:152 严格不等号）↔ CALIBRATION_ALGORITHMS:321/COSMETIC §1 ✓；median 偶数取双中位（cosmetic:35-44、calibrator:50-64）↔ 文档 :147 ✓；IDW 1/dist 4 方向 + sum_w>0 回退原值（:202-224）↔ DISP-COS-003 登记口径 ✓（文档明示"名义 bilinear 实 IDW"，无两说）；结构过滤 sizes≥max_size ⇒ 清除（:108-113）↔ "size<max_size 才保留" ✓；calibrator dark_opt 双分支与 flat floor 0.1 ↔ CALIBRATION_ALGORITHMS §3.1 ✓；F64 降级位型 ↔ DISP-COS-004 冻结口径 ✓。
- **v6_calibration_covariance 公式抽验**：J=[1/f,−(1−α)/f,−α/f,−y/f]（cpp:307-334，同 master 折叠 −(1−α)/f :324、双独立 bias 项 :327/:398、K 两分支作用于 dark :333-334）↔ V6 文档 §1 :12-:16 ✓；`max(f,0.1)`+flat_floor、q²/12 缺省 1 ADU（:146-147/:271-276）、CAL-NO-CLIP 无 clamp（:189 自引处语义）↔ §1 :17-:20 ✓——**实现面与可核条款相符**（锚可核性问题另立黄8）。
- **负例/判据非退化**：p1phot tests_negative N1-N9 逐条具名断言 rc 与退化量（N4 零星→nm==0、sg==0.0；N5 全拒→status=3/reason=6；N6 全无效→fit_used=0/scale=1.0——**真值无效应⇒归零形态齐备**）✓；p1cal test_negative（FIX-F 参数域＋:218 `negative_actually_present` 自证非空）＋check_bias_influence.py **M1/M2/M3 三变异必红且"模式找不到=判红防放行"**（真值无效应型可执行负例）✓；p1cos selfcheck 基线必 PASS＋注入必 FAIL 双向排除恒常（:66 明示恒 PASS 告警）✓；p1phot fixgates 门1 双向对照 ✓——**唯一例外为黄11（门2）**。
- **判据不退化的反面核查**：三模块 grep 未发现"永真门包装成验收判据"（性能哨兵、oracle、selfcheck 均有必红侧）；D-07/D-08 等终裁相关表述在本域文档无翻案。

### ② 行文逻辑（除已列黄2/黄4/黄7/黄11/黄12 外查净）

- **订正注（新旧两说）抽查**：PHOTOMETRY:295（D-06 划线式订正）、:403/:428（A-P1-01）、05:265/:278（内嵌 `<!-- 订正 -->`）结构合法、旧说有对照、新说单一；05:265 裁决编号错挂已由 S19 红-2 登记（不重报）。三模块 README 头部 LEDGER-DOC「现状复测」注记自洽闭合（旧基线失效声明以注记为准）。
- **UNRESOLVED 混入正文**：photometry/calibration/cosmetic 三目录 grep `UNRESOLVED` = **0 命中** ✓；「未决/OPEN」项均在登记位（V6 文档 §5「未决（只登记，不擅改）」、module.yaml notes 段）——无把未决写成结论的段落。
- **状态词纪律**：三模块 module_status 均 CONTRACT_READY、无违规自宣 IMPLEMENTED（cosmetic/calibration README 均明写"不声明 IMPLEMENTED"）✓；COSMETIC_ALGORITHMS:15「IMPLEMENTED 只由验收签发」自守 ✓（其陈旧陈述问题另立黄7）。
- **单文档前后矛盾**：除黄4（PHOTOMETRIC_FIT :206 vs :224/:248 同文两说）、黄7（module.yaml :2-6 自相）外，photometry README §4/§8、CALIBRATION_ALGORITHMS §3/§13、COSMETIC_ALGORITHMS §0/§8 内部口径抽查一致。

### ③ 跨文档冲突（除已列红1/黄7/黄8/黄9/黄10 外查净）

- **五方权威链抽验**：SCI-CAL-001 参数表 ↔ COSMETIC_ALGORITHMS §1 ↔ 生产源阈值/极性逐项一致；ALG-COS-001..005 ↔ cosmetic README §2 ↔ p1cos 测试设计一致；「非生产双实现」界定（COSMETIC §0/§8 ↔ calibration README §9 ↔ cpp/Makefile 实存）三处一致；optimize_dark_k「未编译未接线」在 CALIBRATION_ALGORITHMS:316、README:162、CMake 源清单三处一致 ✓；DISP-COS-009 恒等通道在 module.yaml:31-33、integration json :95/:100、module_entry 注释三处一致 ✓。
- **端口/词表同名**：cosmetic module.yaml input/output_ports ↔ integration/astrocs_p1_cosmetic.integration.json typed_ports 逐一同名（p1.calibrated/master_dark/master_bias/cleaned）✓；2 op 词表 ↔ types.h ASTROSCOS_OP ↔ operations 块 ✓。
- **配置死键专项**：batch_config.json 13 键**无任何生产读者**（grep 仅 README/CALIBRATION_ALGORITHMS 引述为"实验配置"，README:153 已登记遗留）——非伪装接线的死键 ✓；p1cal/p1cos/p1phot 三套件均已由 eng/tests/unit/CMakeLists.txt:554/:564/:637 注册（"文档说有、测试没接"反向为无）✓。
- **docs/science/CALIBRATION.md、PHOTOMETRY.md**：作为对照面使用；其文档侧问题归 S2/S3 切片（S2 红-1 等已立案），本切片与其对照的常数/极性/参数表未发现第三说。

### ④ 幻觉与锚（除已列黄1-黄10/绿3/绿4 外查净）

- **file:line 锚逐条实开（本切片抽验 ≈160 处）**：photometry README（§3-§12 全表）、PHOTOMETRIC_FIT（§13.1/13.2/13.3/13.5 抽 35 锚）、CALIBRATION_ALGORITHMS（§2-§13 抽 40 锚，算法锚全对组见黄5 反方核验）、COSMETIC_ALGORITHMS（§1-§8 抽 30 锚，cosmetic_corrector 内锚全对）、V6 文档（§1-§5 11 行条款）、05 A-4/A-5（h:31-82/:110-171/:142、cpp:59-90/:250-252/:951-972/:1010-1032/:1034-1046/:1049-1065/:978-1008 抽验**全部命中**，05 侧锚质量高）、三模块代码注释锚——除登记项外相符；行段 ±2 行内的宽窄差异按绿不计。
- **文献题录抽验（web 实核）**：COSMETIC_ALGORITHMS:420 两条 DOI 经 Crossref API 落地核验——Pych 2004 *A Fast Algorithm for Cosmic-Ray Removal from Single Images*, PASP **116**(816), 148-153, 10.1086/381786 ✓；van Dokkum 2001 *Cosmic-Ray Rejection by Laplacian Edge Detection*, PASP **113**(789), 1420-1427, 10.1086/323894 ✓（DOI 链同时解析至 iopscience，注册真实）。Rousseeuw & Croux 1993 DOI（D 系已裁）不复核；PHOTOMETRIC_FIT 文献节与 S5-G3 组重合，不重复核。
- **「文档说有、代码没接」专项（正反双向）**：正向——05 A-4/A-5 的正向要求逐条对代码（守卫/通带身份门/阶梯/退化分支/精度档/10^(-0.4G) 禁令均在位 ✓，仅黄1 的 std::size 化与 10000 删除未执行）；反向（代码有、文档没接）——spatial_gain（fit_spatial_gain 调用 frame_photometry_fit:383）有 SCI 层登记（PHOTOMETRY:354 §16④、PHASE1_DETAILED_DESIGN:58）✓；psfsw.cpp（photometry/cpp/src）由 eng/tests/unit/CMakeLists.txt:29 引入 v6_p1_psfw 测试面、integration/v6_phase1 README 职责 4 登记 ✓；**仅 V6 文档状态行漏登消费点（黄9）**。
- **第三方对接**：本域不含 nanoflann/cfitsio/json.hpp 内部（pc_api 自持 KD-tree 实现；aio_file_io 仅走 read_all 接口，CLEAN-403 注释与 aio 侧一致）——未深入第三方内部，符合只查对接纪律。

### 纪律自查
纯静态：未编译、未测试、未运行任何写盘命令；git 只读（status/log/show/grep）；除本报告外零写入；科学公式/默认容差/冻结定义类问题（红1、黄8、黄11、黄12）只登记上呈、未代拟越权改法。
