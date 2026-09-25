# 注释与 README 审计报告

被审基线 `c8f64e9a`，仓库 `Astro CS Normalization Database`。素材取自已落盘通读成稿 `通读-CR-01`…`通读-CR-35`（缺 CR-08），编号 `CR-<批>-<序>`，CR-26 批的 `F-CR26-<序>` 同号并用。全部结论以成稿原文为准，本报告不复述取证过程。

判据来源：工作包标准 01《文档权威链整治标准》§1（权威链单向、同一主题一份正本、锚可回查）与 §4（只写现行设计、正文无日期无历史、无元信息块）；标准 04《代码梳理标准》§5（注释解释为什么、清理任务编号与缺陷流水号、不保留与现行实现矛盾的注释）、§6（README 与文档索引一致、模块说明让读者据以理解职责与入口）、§7 第 3 项（本件即"注释与 README 审计报告"）；`AGENTS.md` §1.1（任何事先回权威链，答不出规范依据即违规）与 §6（不以 facade/空骨架/no-op 冒充完成、产物不散落仓库）；负责人六条文档口径：层级向下、口径唯一、只写正向约束、常数须有推导、文档内禁日期与历史、自洽——六条同时约束源码头注释与模块 README。

## 1 一页结论

注释、模块 README 与登记面和真实对象之间的失效，不是"写得少"或"写得旧"，而是**同一句话在三处说了三件事，且其中至少一处是对外承诺的那一处**。按失真主体分六类，覆盖范围内共 96 条成立实例：注释宣称的对象在代码或合同里不存在 27 条；注释/README 与合同或科学条文方向相反 19 条；锚漂移或指向无关语句 20 条；登记面把未接线、未验证的对象记为已验证（及其镜像：生效项只活在注释里）23 条；注释含日期与历史叙事 1 条独立立案加 23 处未立案位点；模块 README 或其等价登记面自称对齐依据或唯一实现，却把生产实际入口写错或漏掉 6 条。

失效的重心不在"注释与代码不合"这种无害形态，而在**读者据以做判断的那一层是错的**：对外 ABI 头、模块 README 的端口表、追溯矩阵的状态列。三类实例的后果最重：

- **PSF 模块 README 的端口表仍写"拟合失败星 9 字段全 NaN"**，而合同、签名头与实现都是 compact 语义（失败星不占行、尾部行是不承诺值的残影）。按 README 用"第 k 行是否 NaN"做星↔行映射的消费者，会把 FWHM 与测光通量贴到另一颗星上——直接污染创新点①的星等坐标系；现网生产消费者恰好走 `out_status` 映射，所以今天不炸，明天换消费者就炸（CR-32-4、CR-33-3）。
- **`lib/algorithms/psf/README.md` 与 `module.yaml` 自称"行号锚全量复测刷新"**，成稿逐条打开数过 25 条锚，全部漂移，位移从 +12 到 +61 不等。登记面用一句已完成时的主张免除读者的复核，而复核必失败；同族形态是 platesolve 的两份合同把同一实现符号钉到互斥的两套锚上、且 `ALG-WCS-001` 在追溯面据此标着 VERIFIED（CR-32-3、CR-27-05）。
- **零调用、无测试、权威文档 0 字的对象在对外合同面上带 VERIFIED**：platesolve 的 CDA Phase B 径向畸变支链 444 行零调用点却登记为对外已验证 API（F-CR26-13），k-vector 与 polygon 支链同样登记 VERIFIED 而其唯一测试载体从未进构建图（CR-27-16），SCI-PHOT-001 的一项 VERIFIED 证据宿主是一个零断言的 JSON 打印器、它调的还是模块自己头文件标为"非生产通道"的那条路（CR-07-12）。这一类使发布门按登记面审阅时把未覆盖面当已覆盖面跳过。

反向失真同样成规模：真正生效的判据与义务只在注释里存在——`psfsw.h` 的"2 倍星数豁免"OR 支路决定门是否放行，而 24 条 PSFSW 条款的 locator 命中 0/24、`GATES_AND_TOLERANCES.md` 里一个都没有（CR-05-16、CR-05-13）；`information_weight.h` 自称的冻结合同锚 `FZ-WINFO-DIAG-APPROX` 在合同与登记面零条目（CR-15-01）；逐星半径缺失导致的天空预算收缩只由一个 u8 位标承载（CR-15-10）。

日期与历史这一类的取舍是明确的：**要清，但不为它单列批次**。成稿把 12 处日期位点（`Phase1 Final Closure, 2026-08-08`、`RETIRED 2026-09-25`、`r2（PSF-001，2026-09-10）`、`P5-SNR 重定义 (2026-09-14)` 等）与 ≥10 处任务/缺陷流水号（`FIX-NOISE-*`、`FIX-STAR-*`、`M3b-*`、`B4-2`、`[P27-DEAD-PARAMS]`）一律标为"结构性不适用、登记不计数、S4"，其中 CR-16 明文把"源码头注释是否受六条口径约束"列为待裁问句、本批未据此立案。按负责人六条口径的现行适用域，这类位点是零行为收益、零判据影响、可机械清除的，处理方式是并入各模块注释清理的同一次改动，并由词面门承接（见 §4），**不为其改写任何判据、合同或门表**。真正必须整改的是它的两个变体——引用已废止条文（「宪章 §5.3」三处仍在 ABI 头与实现里充当规范出处）与保留被撤销的旧口径（README 端口表）——这两者已计入①②类。

## 2 失效形态分类登记

归计规则：一条 finding 按其失真主体只归一类，不重复计数；引用的对象与文件均取自成稿原文。

### ① 注释宣称的行为、断言、清单或条款在被指对象里不存在 —— 27 条

- 宣称参数生效而实现丢弃：`drizzle_engine.h` 把 `sumWeight` 登记为 `= sumNorm` 而实现两处写 0.0（CR-10-10）；`snrData/weightData` 登记为参与计算的入参而实现按匿名形参丢弃（CR-10-15）；hips 入口的 `max_workers` 注释称"仅作回显"，实为既不消费也不回显（CR-14-1）；`iter_trans_inner` 注释称 `tolerance` 只为接口兼容保留，实现里它做首轮预过滤并钳制 σ（CR-27-12）；`sky_plane.h` 声明 `max_nodes` 默认 8192，实现默认 2048（CR-23-09）；`ipv_distortion.h:32` 的 `valid` 判据与实现不同（F-CR26-15）。
- 宣称判据/门数值存在而实现与登记面都没有：`drizzle_science_matrix_test` 头登记的 1e-10（纯 TAN）与 1e-8（交叠）两值在实现与门表两侧都不存在，实现统一 1e-6（CR-12-05）；`p1phot_tests_spatial` 头登记的 6× 噪声底、峰峰值、10× 阈值与实现的 4×、场 RMS、10× 噪声底不是同一条判据（CR-04-11）；`hips_properties` 头承诺"重复键 = 错误（无 silent override）"，8 个已知键里 3 个无哨兵、后写行静默覆盖（CR-21-07）；`image_corrector.h` 头注"OpenMP 并行加速 (16线程)"，实现不固定线程数，而固定线程数本身被 `AGENTS.md` §6 禁止（CR-01-20）。
- 宣称断言或覆盖面存在而文件里没有：`concurrency_cache_test` 头注登记三项守护、实际被编入构建图而无任何门执行（CR-10-01）；头注"并发结果与串行参考逐 tile 一致"，`close()` 只比一个全局 `sum_flux` 且容差 1e-3（CR-10-02）；头注第 0 步"验证 patch 尺度与 SKY 一致"是一段纯 `printf`，不参与退出码（CR-10-18）；`p1noise` 注释宣称的注入锚是 `P1NOISE_CHECK(cs, true, "n1_null_args_rc3")` 这类恒真式加死占位 helper（CR-16-23）；`coverage.cpp` 的 STATUS 块自称"生产可达的合同门"，唯一非测试调用点既传不进禁 token 又把返回值丢弃（CR-20-12）；ABI 越界锁宣称守"不得按大结构体写小缓冲"，全部负例传的都是全尺寸对象（CR-28-7）；`p1psf_prodpath_check` 宣称是 M3b-F-01 的"生产路径共址回归锁、未修复必红"，实际不链接任何坐标契约或 `sdet_*`，其 0.5 偏移由测试自己施加（CR-33-25）。
- 宣称范围/等价/清单为真而事实相反：`p1phot_fixgates` 头注"生产路径一律 `quality_flags=nullptr`、生产行为与 HEAD 逐行等价"，而生产节点已走带质量位的 `_qf` 入口（CR-02-12）；`p1noise/p1psf` 测试头注的组集合与磁盘与注册表不一致——注释 6 组对注册表与 CMake 8 组（CR-16-19）、注释四组对 `groups[]` 与 `add_test` 的六组（CR-03-2-03、CR-03-4-01）；`api.h` 注释"本结构体 24 个字段全部不可由用户配置影响"，结构体实为 26 成员（F-CR26-22）；`ipv_angle.cpp` 注释"用 std::printf 到 stderr 不依赖 Logger"，本文件 0 处 printf（F-CR26-10）；`drizzle_l2_test` 注释把 `65536` 称作"生产 NSIDE"，与 `KNOWN_LIMITATIONS.md` 的"生产 nside（≥2^17）"指两个网格（CR-11-7）；`precise_hardening` 采样格注释写 5×5/每 6 像素，实现是 3×3/每 4 像素、且该错数成了均值门分母（CR-12-09）；`astro_sphere_sink.h` 与 `write_hips_phase1` 的"与旧 writer 逐字节等价/`product_begin` 参数逐项一致"，既无仓内对照实现，又在 DID、flags 位、观测元数据三处异值（CR-09-11、CR-09-9）。
- 宣称合同条款或义务存在而合同面零条目：`information_weight.h` 的 `FZ-WINFO-DIAG-APPROX`（CR-15-01）；`psfsw.h:254` 的"2 倍星数豁免"支路（CR-05-16）；`sdet_angle_guard.h` 的"与冻结合同 § 一致"（§ 后无条款号）与被四处引用而全仓不存在的 `SDET-ANGLE-001`（CR-31-1）；`module_entry.cpp` 的逐星 flux/fwhm 降级只在注释里交代，产品与合同面无逐帧可审计字段（CR-15-10）；`pc_api.cpp` 里被剥成空括号 `(: …)` 的两个夹具常数，其"收紧 3.0 → 2.0"的改值在合同与配置两侧均无登记（CR-02-17）。

### ② 注释/README 与合同或科学条文方向相反 —— 19 条

- ABI 头与合同对撞：`hp_drizzle_api.h` 宣称 pixfrac 值域 `0.0~1.0`、默认 1.0 且"1.0 避免源像素固有缝隙"，与负责人裁决的默认 0.8 和"取 1.0 才产生无覆盖缝隙"的登记说明方向恰好相反，值域里的 0.0 是合同非法值（CR-13-10）；正式 ABI 头抄写权威文档明令禁抄的 `211034.6`（正确值按公式 `√(π/3)·(180/π)·3600 = 211076.28514206142`）（CR-13-09）；`photometric_calib.h:102` 把 `-3` 解释为"无 Gaia 星/PSF 星（退化 scale=1.0）"，而 README §6 与 ALG §13.1 规定退化路径 rc=0 不失败、实现里 `-3` 只出现在锥形搜索失败分支（CR-05-10）；`api.h` 对 `log_dir` 缺省语义与插件文档相反，同一参数三面三说（F-CR26-21）。
- 注释/测试自述与冻结条文相反：`p1noise_fixtures` 的 FIX-NOISE-B 注释把负预测像素期望写成 `variance=floor(1e-12)`、`ivar=1e12` 并要求逐位断言 clamp，冻结条文规定"预测 ≤ 0 ⇒ `variance==0 ∧ ivar==0`"即不可用态（CR-16-02）；`G3c` 由测试注释单方宣布 SCI:346 的正例判据"不应成立"并把断言改成两臂一致（CR-16-13）；FIX-NOISE-D 的规格注释把"掩膜半径与星亮度解耦"当期望，而冻结测试设计明文判该陈述为伪（CR-16-01）；`p1star` 注释把 flux 母函数口径冻结成 Moffat4 振幅 A，合同正本规定检测侧=椭圆高斯峰值振幅且明令两者差 1.9140×（CR-30-09）；`module_adapters.cpp` 消费点注释用高斯因子 `2.3548` 解释 Moffat4 的 σ→FWHM，正是 DISP-STAR-007 明文禁止跨块使用的那一次换算（CR-33-4）。
- 注释与代码、代码与合同三方各说一种：`p1phot_tests_units` 断言的行为与公共 ABI 头注释相反，而该矛盾已被 DISP-PHOT-001 登记为"现状缺陷、登记不改码"（CR-04-22）；`pc_api_qf.h` 头注自认"冻结 6 导出在生产路径不带质量位 ⇒ 饱和/质量位有效域过滤不生效"，而 ALG 条文以无条件句表述该门、登记面只记了双通道并存未记门失效（CR-05-1）；`spatial_gain.cpp` 注释自称"与 §2.3 一致"而收敛门取 1e-9、条文与配置三侧都是 1e-6，且三个"已登记可配"的常数在此写死成空登记（CR-06-02）；`u.cpp` 的 `workers<=0` 在注释、委托合同与实现里分别是 auto(OpenMP)/委托标记/串行 1（CR-24-08）；`n_finite` 的三处定义互不一致，实现只满足头文件注释、不满足冻结 §5 式（CR-22-02）；`at_match_lists` 实现是双向互为最近邻，而头文件与上游对照表都登记为"按距离升序贪心"并标"相同 ✓"（CR-27-11）。
- 注释之间互斥或与运维意图相反：`nls_lm.cpp` 与 `sdet_api.cpp` 两处同源注释对"同一生产输入下哪个判据触发"给出相反结论，且都自称有实测凭据而凭据在 `run/` 之后（CR-29-06）；`dpsf_log.cpp` env 越界回退到"最啰嗦"档，与同处注释声明的"默认只输出 WARN+ERROR"意图相反（CR-32-7）；模块 README 的端口表与签名头对失败星的两说（CR-32-4、CR-33-3）。

### ③ 行号锚或符号锚漂移、指向无关语句 —— 20 条

- 源码注释指向无关语句：`frame_photometry_fit.h/.cpp` 两处共用的 `snr_science.cpp:234` 实为孔径噪声式，与 `m_5` 星等约定无关，且只给裸文件名不给目录（CR-01-15）；`drizzle_engine.cpp` 两处引 `DATA_SEMANTICS.md §31.1a:2808-2810` 讲 BUNIT，而该区间实际是样本掩码消费与子产品位分配（CR-10-08）；`ipv_api.h:272` 把生产入口钉在 `module_adapters.cpp:2100`，该行是 CD 矩阵赋值，`p1_op_wcs` 在别处（CR-26 api.h 位点、F-CR26-19）。
- 权威文档锚指向别的函数或不存在的条文：`PHOTOMETRIC_FIT.md:264` 的行号区间漏掉它自己声明"同时断言"的第二条断言，而同节另一处夹具锚仍准确（CR-04-17）；`PHOTOMETRIC_FIT.md` 对 `pc_api.cpp` 的实现锚整体漂移、两处指向别的函数，其中唯一登记"入参静默失效"的 DISP-PHOT-005 按锚找不到对象，等于把未修缺陷显示为不存在（CR-04-30）；`PHOTOMETRY.md:218` 的"实际传入点"只列两处、漏掉唯一带质量位的那条生产传入（CR-06-15）；`DATA_SEMANTICS.md` 与 `PLATESOLVE.md` 把同一实现符号钉到互斥的两套锚且均已漂移，连带使 `ALG-WCS-001` 的"§11 逐符号锚 VERIFIED"失去可复核性（CR-27-05）；`mad_check` 引的 `NOISE_MODEL.md:17/:46/:135` 与 `§14.2` 四套锚全部落空（分别是无关句、章节标题、空行、不存在的节），错锚并已扩散到 `sdet_image.cpp`、`sdet_api.cpp` 与 `tests/p1star/CMakeLists.txt`（CR-29-22）；`drizzle_nonfinite_test.cpp:12-13` 引用的"不掩膜 §8 :96"在科学面上零命中（CR-11-20）；合同文档对 `dr3sp_id` 量化步长的行号锚指向别的函数（CR-04-27）；`kMaxOrder = 20` 的"ARCH-P3 §3 内存守卫"锚经核不成立（CR-21-11）。
- 锚指向跟踪面之外的载体：`psfsw.h` 自称的冻结面里 24 条条款 locator 命中 0/24，且 `reanchored_from` 指向不存在的 `PSFSW_FROZEN_THRESHOLDS.md`（CR-05-13）；独立 verifier 与 GSL 对齐证据分别锚在 gitignore 的 `run/`（CR-18-2、CR-29-03）；FWHM 因子常量的验证证据锚同样指进 `run/`，跟踪面为零（CR-33-18）；`ipv_angle.h` 的设计参考 `ipv_cda_distortion_design.md` 从未入库（F-CR26-07）；`lib/algorithms/psf/README.md:6-7` 与 `module.yaml` 自称"行号锚全量复测刷新"，实测 25 条锚逐条漂移 +12…+61（CR-32-3）。

### ④ 登记面把未接线或未验证的对象记为已验证 —— 23 条

- 状态列虚标：platesolve 的 CDA Phase B（444 行）零调用点、无测试、`PLATESOLVE.md` 0 字，而合同登记为 VERIFIED 对外 API（F-CR26-13）；`kvector_build`/`KVectorIndex` 与整条 polygon 支链在 `API_CONTRACTS.csv:310/311` 标 VERIFIED、TEST 列是不可解析的占位 `TST-GEN-001`，其唯一测试载体从未注册进构建图（CR-27-16）；SCI-PHOT-001 的一项 VERIFIED 证据宿主是零断言产生器且走"非生产通道"，真断言宿主未回写登记面（CR-07-12）；`DISP-HIPS-001/004` 被文件头、模块 README 与登记表记为"模块事务面收口"，而生产导出末端的 sink 直写 out_dir、从不触碰这四个原语（CR-09-2）；`hips_properties.h` 自称符合 ALG-P3-001 而追溯矩阵同一条把 SCI/ALG 标 MISSING，值域由代码自己定义（CR-21-10）。
- 证据与覆盖度读数失真：候选零漏选 Oracle 登记"9003 例"，真值由被审的同一面积例程定义且只有 49.3% 走穷举档（CR-09-16）；证据 JSONL 写不出来不影响退出码、`used_fallback` 只记不判，登记证据可零字节而门全绿（CR-09-18）；SAT-001 门文档登记的实测值全部不可复现且标定参数偏离 SCI 冻结的饱和域 oracle（CR-16-12）；G1 自称引用既有门 `G-P1-CENTROID-SCI` 的冻结域，实际执行域不同，属"引用既有门"的登记名不副实（CR-29-18）；`ACR_EQUIVALENCE` 把回退门证据登记在本文件上而本文件零 ACR 行使（CR-22-06）；自称"直接跑生产 Stage2"的门，被测对象是发布清单标注"非发布目标"的 compatibility 工具（CR-22-05）；编排日志把从未生效的 `maxIter=100/tol=1e-6` 当"本次调用参数"记进运行证据，与合同默认 200/1e-8 并存两套数（CR-33-6）。
- 门与豁免登记滞后：自称"科学门"的 L0 与另外三张门根本没注册进 ctest、只有编译目标，而接线基线仍写 `unbuildable_test`（CR-11-2）；`concurrency_cache_test` 已可构建而豁免条目常驻，W4 读数无法区分"真修了"与"豁免还在"（CR-10-03）；棘轮基线仍记已接线测试为 unbuildable，同目录 `CMakeLists.txt:5` 的自述与自身存在互相矛盾（CR-12-07）；自称"Gate D 验收门"的四个判据（90s / 叶数带 / 1e-4 / 4096MB）不在任何门登记面（CR-11-15）；门号 `G-P1-CENTER-CONTRACT-1` 与两条阈值在门表零登记、容差只活在测试源码里（CR-33-10）；`API_CONTRACTS.csv` 的 `error_model` 列 381/381 同文模板，本模块三行的语义列与头文件相反（F-CR26-12）。
- 模块清单面按文件计数把非实现算作交付：只 `#include` 自身头、除注释外零定义的 `async_io.cpp` 被登记面算作模块源文件（CR-19-08）；431 行、17 个原语零调用者、且是 `sdet_image.cpp` 的无登记孪生副本，却同时进根 `CMakeLists.txt` 与模块 `Makefile` 的编译登记面，而模块 README/`module.yaml`/插件文档对这些原语全部零提及（CR-32-11）。

### ⑤ 注释含日期与历史叙事 —— 独立立案 1 条，位点 25 处（其中 23 处成稿按"登记不计数"处理，未立案；取舍见下）

- 位点统计：**日期字面量 12 处**（测试头冻结日期 `2026-09-07`/`2026-08-23`、`Phase1 Final Closure, 2026-08-08`、`"2026-09-07, wave W1"`、`FROZEN T104 2026-08-23`、`P5-SNR 重定义 (2026-09-14)`、「自 2026-09 负责人裁决」、`RETIRED 2026-09-25`、`ipv_api.h:14` 与 `:14` 两处 `2026-07-02`、`README.md:6-7` 的 `r2（PSF-001，2026-09-10）`、`p1psf_oracle.hpp` 的 `probe, 2026-09-09 实测`）；**任务与缺陷流水号 10 处**（`FIX-NOISE-B/D/H`、`FIX-STAR-A/B/C`、`B4-2 (M3-C-001)`、`B4-3 (M3-C-002)`、`Phase1 v2`、`[P27-DEAD-PARAMS]` 段、`SDET-ANGLE-001`、`M3b-F-01` 自称）；**已废止条文引用 3 处**（`hp_drizzle_api.cpp:978-979` 与 `hp_drizzle_api.h:76` 的「宪章 §5.3」、根 `CMakeLists.txt:843-845` 的「宪章 §10.4」，宪章一词在 `docs/design` 与最高设计已零载体）。
- 成稿处置：除 CR-13-07（引用已废止宪章，独立立案、轴 C）外，上述位点被一致标为"结构性不适用／登记不计数／S4"，CR-16 更把"源码头注释是否受六条口径约束"列为待裁问句并声明本批未据此立案。
- 取舍：**清，但不单列批次。** 理由三条，均可核对：其一，六条口径的适用域已明写含源码头注释与模块 README，废止引用与旧口径残留本来就已计入①②类，日期与流水号是同一改动的顺手项；其二，这类位点零行为后果、零判据影响（`FIX-NOISE-*` 类流水号的危害已在 CR-16-01/02 以"方向相反"立案，不必重复计），单独开一轮改动等于用一次全量构建换纯文本；其三，它的正确防法是词面机械核对（见 §4），人工逐条清必漏，故把它并入各模块注释清理的同一次改动即可，**不为其改任何判据、合同或门表**，也不接受"为达标而把与之关联的注释语义顺手改掉"。

### ⑥ 模块 README 或等价登记面自称唯一正本，却漏登/误登生产实际入口 —— 6 条

- `saturation_policy.h:7`「规则（唯一事实源=上述 SCI 条款，本头文件只是它的可测实现）」——生产第二入口 `module_adapters.cpp:6646-6659` 另写了一份优先级链且标签集合不同（`config`/`noise_config`/`fits_header` 三值 vs 合同冻结的 `NONE`/`DISABLED_NO_MASK` 形态），"唯一可测实现"的自称不覆盖真实生产者（CR-17-1，另见 CR-17-2 的 `SOURCE=NONE` 与 `FILTER=ENABLED` 并存）。
- `docs/science/CONTROL_WEIGHT_SNR.md:106` 把科学不变量的执行主体点名登记为 `p2_star_mask_caps`/`p2_star_mask_contains`/`p2_sky_patch_estimate` 三个"生产"函数，而接线台账把三者登记为 unreachable，真正履行该"必须"的是 `sampler.cpp`：文档面漏登真实载体，读者按锚复核会落到不执行的代码（CR-23-05）。
- platesolve 的"生产入口"两说互斥：`ipv_api.h:272` 与 `test/CMakeLists.txt:162-163` 说 `phase1 run → p1_op_wcs → ipv_solve_from_memory_with_callback_d`；`lib/algorithms/platesolve/README.md:54/79/93`、`module.yaml` 与 `PUBLIC_API.md:919/945` 说生产入口是 `ipv_solve_from_detections_v1`（orchestrator 实调）；代码注释又说"路径 A 仅用于 A/B 对比测试、生产路径为路径 B"，而 `DATA_SEMANTICS.md §18.1` 的节标题把路径 A 直接称作"生产通道"（F-CR26-19、CR-27-06）。四处里 README/合同这一处是对外承诺面，且与实现声明相反。
- `star_detection/README.md:170` 把"performance 组 = 批 1/N worker 缩放（OpenMP 3 处）"登记为该族测试设计的冻结面，实测 `#pragma omp parallel` 在该模块是 31 处；同族"口径冻结（README §5）"被测试注释当作权威引用，而它与合同正本 `DATA_SEMANTICS.md:901` 的规定相反（CR-30-09 及 README 计数位点）。
- `lib/algorithms/psf/README.md` §1/§5 自述"该表是 registry descriptor 的对齐依据"，其 `psf_params:FLOAT64[N,9]` 行仍记 B2-A2 之前的旧端口语义，同文件 §131、`dynamic_psf.h:95-96` 与合同都是 compact 语义（CR-32-4、CR-33-3）；同 README `:49` 立的 `sx≥sy` 约定在实现里既无排序也无断言，被点名的 θ 消歧机制在代数上不具备保持该约定的能力（CR-32-9）。

零实例声明（不为凑类而含糊）：① 的子形态"**注释宣称某条断言存在而文件里连该断言名都没出现**"在生产源里零实例，全部落在测试与门文件（CR-10-18、CR-12-05、CR-16-23、CR-10-01）；生产源的该形态一律表现为"宣称参数/字段生效而实现丢弃"。④ 的子形态"**登记面把跟踪集里不存在的文件记为存在**"零实例；登记面的失真一律是状态词（VERIFIED/收口/已刷新/unbuildable 常驻）与内容列（`error_model` 模板）失真，不存在的对象只出现在文档锚里（CR-05-13、F-CR26-07）。六类在覆盖范围内均非空。

## 3 按模块的整改清单

一行一个可独立核对的改动对象。"判据"一栏是给验收方的可核对条件，不是过程描述。与通读 finding 对应的标其编号。

### 3.1 测光 photometry

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `image_corrector.h` 头注的并行声明 | 去掉"16线程"，正向写"并行度取自 OpenMP 运行环境，本库不写死线程数" | 该 TU 无固定线程数字面量；注释不含具体线程数 | CR-01-20 |
| `frame_photometry_fit.h/.cpp` 的 `m_5` 约定锚 | 星等约定的锚改指条款（`CONTROL_WEIGHT_SNR.md §2a` 的 m_5 行 / `DATA_SEMANTICS` 对应行），删裸文件名+行号 | 注释每个引用按锚文字在被指文件命中；全仓无裸文件名锚 | CR-01-15 |
| `photometric_calib.h` 错误码语义行 | 按实现重写为「−3 = 外部锥形搜索调用失败」，并把"无参考星 ⇒ rc=0 且 `out_n_matched=0`（恒等校正）"单列为成功退化态 | 头/`README §6`/`PHOTOMETRIC_FIT §13.1` 三处错误码表逐行等值，且与 `pc_api.cpp` 的 return 集合一一对应 | CR-05-10 |
| `star_matcher.cpp` 头注与 `photometric_calib.h` 的 `unique_matches`/`rejected_ambiguous` 字段语义 | 注释改为与实现一致的语义；DISP-PHOT-001 的登记标题作用域改指发布头文件而非"内部注释" | 按头文件描述写出的判据与实测 diag 字段值一致；ALG §13.3 文字点名 `photometric_calib.h` | CR-04-22 |
| `pc_api_qf.h` 头注的"生产不带质量位" | 该事实要么由合同面新增登记条目承载，要么随接线删除；`PHOTOMETRIC_FIT.md` 的无条件句加"仅经 `_qf` 入口"限定 | 同一科学条文（`PHOTOMETRY` §4/§10）在两条通道上各有唯一登记条目，无"注释自认+登记沉默" | CR-05-1 |
| `p1phot_fixgates.cpp` TU 头的范围/影响声明 | 改为事实描述（冻结 6 导出传 nullptr；scheduler 节点经 `_qf` 传真实位），并补一条 `_qf` + 非空位数组用例 | 声明里每个"一律/逐行等价"能沿 `pc_api.cpp` 逐跳核对；新增用例存在且判据非恒真 | CR-02-12 |
| `p1phot_tests_spatial.cpp` 头登记的判据 | 头注与实现统一为同一统计量与同一倍率（按裁决取 6×/ptp 或 4×/RMS），并把该组容差入门表 | 头注数值与代码常量逐条相等；`GATES_AND_TOLERANCES.md` 有该组唯一条目 | CR-04-11 |
| `p1phot_tests_main.cpp` 头注的单跑组清单 | 组清单由 `groups[]` 与 CMake 派生，不手写；"四组"改六组 | 注释组集合 = `groups[]` = `add_test` 名集合 | CR-03-2-03、CR-03-4-01 |
| `pc_api.cpp` 的 `(: …)` 空锚与 `match_radius_px=2.0` | 在 `PHOTOMETRIC_FIT.md` 常数表补该键（含取值理由），`defaults.json` 增键（若应可配），注释换成可解析锚；F3 夹具三处判据改从同一常数派生 | 该常数在权威面有行且注释锚指向它；无空括号锚 | CR-02-17 |
| `PHOTOMETRIC_FIT.md`、`DATA_SEMANTICS.md` 对 `pc_api.cpp`/测试文件的 `:NNN` 锚 | 全改符号锚（函数名/变量名/fault 名），DISP-PHOT-005 行同步 | 文档内不再有指向代码的行号锚；每个符号锚唯一命中且命中的是被点名对象 | CR-04-30、CR-04-17、CR-04-27 |
| `spectrum_integrator.h` 的"非生产通道/历史口径"注 + `test_spectrum_integrator.cpp` 头注 + 追溯行 | 头注按实际角色改写（产生器），SCI-PHOT-001 的证据列改指含比较断言的宿主 | 该 SCI 条目的测试列指向注册目标且该目标含断言；非生产通道有显式标注并被登记引用 | CR-07-12 |

### 3.2 噪声与 SNR noise_snr

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `information_weight.h` 自称的 `FZ-WINFO-DIAG-APPROX` | 在 `v6_clause_registry_v1.json` + `DATA_SEMANTICS` 正式立此条款并把偏差比接入产品字段，或删除这条注释里的假合同义务 | 注释引用的每个 rule_id 在合同与登记面可解析；登记面不引用不存在的义务 | CR-15-01 |
| `psfsw.h` 头注的 24 条 `PSFSW-T-*` 锚 | 把值与判据真正写进 `GATES_AND_TOLERANCES.md`/SCI 正文（现仅 id 清单无值）；删指向不存在的 `PSFSW_FROZEN_THRESHOLDS.md` 的 `reanchored_from` | 24/24 locator 命中；`contains` 命中 24/24；无指向跟踪集外文件的迁移记录 | CR-05-13 |
| `psfsw.h` 的"2 倍星数豁免"支路 | 补条款承载 `2×`，或删该 OR 支路；注释不再单独承载判据 | `psfsw.cpp` 每个阈值常数带 rule_id 且值与 rule_id 登记的常数等值 | CR-05-16 |
| `snr_estimator.h` 的 `P5-SNR 重定义 (2026-09-14)` 注与 `noise_snr/README.md` 同族行 | 去日期与"重定义"叙事，改为现行定义的正向陈述，两处同批同措辞 | 头与 README 对应行不含日期、不含历史标签，且逐字一致 | CR-17（⑤ 位点）＋ CR-17-13 |
| `saturation_policy.h` 的"唯一事实源/唯一可测实现"自称 + `module_adapters.cpp` 第二份优先级链 | 优先级链只留一份实现；若两入口并存属实，注释须点名第二入口与其标签集差异，且两处标签集合同一 | `sat_source`/`noise_saturation_source` 赋值点仅一处，或注释如实枚举且枚举与 `git grep` 结果相等；两入口标签集合逐项相等 | CR-17-1、CR-17-2 |
| `module_entry.cpp` 的 MASK-002 降级注释 | 把"生产噪声面恒用 rmax 天空预算"的方差影响登记为逐帧可审计的产品字段（或写进 `variance_audit` 必落字段清单） | 合同面有该降级字段；注释不再是一条降级的唯一载体 | CR-15-10 |
| 拟合后端的引文锚（`NOISE_MODEL.md:17/:46/:135`、`§14.2`） | 五处同源错锚统一改小节锚（§5/§9），禁行号式 | `git grep -n "NOISE_MODEL.md:"` 的行号式命中 0；新锚按内容命中 | CR-29-22、CR-29-01 |

### 3.3 Drizzle / healpix_drizzle（mosaic）

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `drizzle_engine.h` 的 `sumWeight`/`sumSnrSq` 与 `snrData`/`weightData` 注释 | 字段要么实现按注释算，要么删字段并删注释；两形参改成正向说明（当前不参与） | 不存在"登记为入参而实现匿名丢弃"的签名；头字段等式与实现写入值一致 | CR-10-10、CR-10-15 |
| `hp_drizzle_api.h` 的 pixfrac 行与 `211034.6` 抄写值 | pixfrac 改「(0,1]；生产默认 0.8（`defaults.json drizzle.pixfrac`）」；常数改公式值 `211076.28514206142` | ABI 头默认/值域与 `defaults.json`、`DRIZZLE.md` 逐项等；全仓 `211034.6` 命中 0（含 `drizzle/README.md`、`orchestrator.cpp` 注释） | CR-13-10、CR-13-09 |
| `hp_drizzle_api.h/.cpp` 与根 `CMakeLists.txt` 的「宪章 §5.3 / §10.4」引用 | 改引 `DRIZZLE_GEOMETRY` RESCUE-FD-02 与现行线程纪律条文 | `git grep -n 宪章 -- lib eng docs` 命中 0 | CR-13-07、F-CR26-01 形态 |
| `drizzle_engine.cpp` 两处 BUNIT 锚与 `DRIZZLE_GEOMETRY.md:276` | 行号锚改条款号锚（§31.1/§31.1a 的 signal 单位表） | 被引条文按锚文字命中；三处不再有 `:NNNN` 式锚 | CR-10-08 |
| `drizzle_nonfinite_test.cpp` 头注与 `drizzle_engine.cpp` 的"旧行为已作废/已被反转"叙事 | 删历史叙述，直接引现行 `DRIZZLE.md` 掩膜条款与 `DATA-002 §2a` | 两文件不含"旧/作废/已反转"；锚指向的条文内容与本文件断言一致 | CR-11-20 |
| `drizzle_science_matrix_test` 头登记的 1e-10/1e-8 与该族 5 个门值；`tests/CMakeLists.txt:5` 的自述 | 只登记实际执行的阈值，5 个门值入 `GATES_AND_TOLERANCES.md`；删与自身存在矛盾的目录自述 | 头注门值与代码常量逐条相等；门表有该门唯一条目；自述与文件树事实一致 | CR-12-05、CR-12-07 |
| `concurrency_cache_test`/`drizzle_l0_test` 头注的门身份与"逐 tile 一致"承诺；`prod_wiring_baseline.json` 的三条死豁免 | 头注改指真实执行载体（`DRIZZLE_GEOMETRY §9` 的已注册件）；判据要么升级为逐 tile 比较、要么注释如实降级；豁免条目随对象消失回收 | 同一测试的 ctest 注册状态、基线豁免项、头注声称三者互不矛盾；`close()` 的比较集合与头注承诺的范围一致 | CR-10-01、CR-10-02、CR-10-03、CR-11-2、CR-12-07 |
| `control_median_mc_test` 头注的第 0 步 | 改成具名断言（按 pixfrac 口径带 `×pixfrac²` 或用 `sumNorm`），或从步骤清单里删除该声明 | 头注列出的每一步都对该 TU 退出码有非零贡献 | CR-10-18 |
| `aio_publish.cpp` 头、`hips/README.md`、`STANDARDS_REGISTRY` 的 DISP-HIPS-001/004"收口" | 改写为"能力已在库内、生产末端未接线"，或把 sink 末端真正接入 staging 后再保留原措辞 | 状态词与 `astro_sphere_sink.cpp` 的实际调用集一致；`aio_publish_*` 调用点包含生产末端，或登记面明写旁路 | CR-09-2 |
| `spherical_overlap` 族与 p1drz 门族 | 本轮未读，不列整改项 | 见 §5 边界 | — |

### 3.4 AIO / HiPS writer 与 Phase2 交付面（export）

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `astro_sphere_sink.h` 的"逐字节等价"要点与 `write_hips_phase1` 的"参数逐项一致" | 给出仓内对照实现并登记为产品级门，或删除该主张、逐项列明 DID/flags/观测元数据三处差异并写进合同 | 等价主张要么带对照符号名与门名，要么不存在；两档 flags 与 DID 差异在 `DATA_SEMANTICS` 有登记行 | CR-09-9、CR-09-11 |
| hips 入口的 `max_workers` 字段与其"仅作回显"注释 | 删字段与解析（本模块事务恒串行），或真正回显进 inspect/plan 并订正注释；三处同批改 | 结构体每字段有读取点，或注释明写"未消费"且无第三种说法；对照 drizzle 入口同参数行为一致 | CR-14-1 |
| `ivar_wiring_test.cpp` 头的"直接跑生产 Stage2" | 改述为"跑 compatibility 工具"，并把门改接发布目标（`integrate.h` 自述的唯一生产调用方所经入口） | 门的被测 target 在 `PRODUCTION_EXECUTION_INVENTORY.csv` 标为发布目标；头注与之一致 | CR-22-05 |
| `ACR_EQUIVALENCE` 的证据登记行 | 证据行改指真正行使 ACR 的测试，或在本文件补 ACR 行使与断言 | 证据行的文件列内含 ACR 调用点与非恒真断言 | CR-22-06 |
| `coverage.cpp` 的 STATUS"生产可达的合同门" | 改为"库层门，生产调用点不传禁 token 且返回值未用"，或接通后保留原声明 | STATUS 里每个调用点能沿生产路径逐跳写出并被消费 | CR-20-12 |
| `async_io.cpp` 空 TU 与 `PHASE2_API_V1.md` 的 async_io 行 | 把非模板工厂真正落到该 TU，或删 TU 并在登记面标 header-only；不保留"预留扩展"注释 | 模块源清单内无"除注释与自包含外零定义"的 TU | CR-19-08 |
| `CONTROL_WEIGHT_SNR.md` 点名的三个"生产"采样/掩膜函数 | 生产锚改 `sampler.cpp` 的实际实现，或把不可达的重复实现删除/标注为只作 Oracle 参考 | 条款里每个"生产 X"的 X 在接线台账非 unreachable；该科学不变量只有一个执行主体 | CR-23-05 |
| `sky_plane.h` 的 `max_nodes` 默认与 `sky_plane.cpp`、`module_adapters.cpp` 的"注释承认漂移" | 头注与实现取同一默认值；取值依据从 `run/` 搬进跟踪面常数表；调用侧不再用注释弥合声明 | `default_config()` 值 = 头注声明值 = 权威文档值；无调用侧覆写来掩盖默认差异 | CR-23-09 |
| `u.cpp` 的 `workers` 注释、委托合同与实现 | 三面取同一语义：正向写"公共入口恒串行，需并行走 `_n` 入口并显式传值"，或让 `0=auto` 真正落地 | 同一参数在注释、合同、实现的取值语义逐字一致；公共入口能取得并行或有明文条款说明不能 | CR-24-08 |

### 3.5 PSF 模块

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `psf/README.md` §2 端口表 invalid 列 | 改为 compact 语义："仅前 `n_valid` 行为成功星参数，其余行不承诺值；星↔行映射只可由 `out_status` 判定" | README 端口表与 `DATA_SEMANTICS` 布局 B 条款、`dynamic_psf.h` 签名头、`dpsf_psf.cpp` 实现四侧逐字对齐；"失败星 9 字段全 NaN"全仓命中 0 | CR-32-4、CR-33-3 |
| `psf/README.md:49` 的 `sx≥sy` 约定与 `fwhm_x/fwhm_y` 语义 | 要么删约定、要么在 θ 消歧后真正归一并加断言；README/`PSF.md` 两处/消费者注释与代码对同一列的语义统一为一句 | 若约定保留则存在排序或断言；`fwhm = 1.230310·s` 在 README、SCI、消费者注释三处一致且无高斯因子套用 | CR-32-9、CR-33-4 |
| `psf/README.md:6-7` 与 `module.yaml:2-4` 的"r2（PSF-001，2026-09-10）行号锚全量复测刷新" | 去日期与轮次标签；锚清单改符号锚；未完成复测前不写"全量刷新" | README 与 module.yaml 的锚 100% 按符号命中；不含日期、轮次号与"复测刷新"叙事 | CR-32-3 |
| `dpsf_log.cpp` 的 env 越界回退与其注释 | 越界与非数字一律回退 `LOG_WARN`（用 `strtol`+尾字符区分 0 与垃圾），注释与之一致；合法域与缺省入合同面 | 越界用例判绿；条文、注释、代码三处缺省同档 | CR-32-7 |
| `dpsf_image.cpp` 的 17 个原语与 `dpsf_psf.cpp` 的死 include | 删孪生副本、保留唯一实现；模块 `exports` 面与实际对外能力一致 | 同一能力一份实现；根 `CMakeLists.txt`/模块 `Makefile` 不再编译零调用 TU；README 不列不存在的能力 | CR-32-11 |
| `p1psf_prodpath_check` 头注与 `tests/p1psf/CMakeLists.txt` 的"共址回归锁"名分 | 头注改述本锁实际覆盖命题（"批 API 从带偏移初值仍能收敛"），M3b-F-01 的共址锁名只留给真链接坐标契约/target 的件 | 锁 target 的链接清单包含被锁命题涉及的符号；CMake 注释与 target 源列表一致 | CR-33-25 |
| `p1psf_oracle.hpp` 容差表的"probe, 2026-09-09 实测"与证据锚 | 闭式精确值给可执行核对（同文件或跟踪面脚本），去日期，证据落跟踪面 | 容差的来源行不含日期且指向跟踪面；恒等复核不复用同一圆整副本 | CR-33-18 |

### 3.6 星点检测 star_detection

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `nls_lm.cpp` 与 `sdet_api.cpp` 两处互斥的"哪个判据触发"结论（含 `LM_FTOL` 取值论证） | 在生产 regime 真实帧重测终止码分布并落跟踪面，然后统一两处措辞与容差论证 | 两处注释引用同一份跟踪面凭据且结论同向；凭据路径在跟踪集内 | CR-29-06、CR-29-01 |
| `sdet_angle_guard.h` 的 `SDET-ANGLE-001`、"与冻结合同 § 一致"（空条款号）及同源 5 处引用 | 要么在登记面立该缺陷号并与 `M3b-I-01` 对齐严重度，要么全部改引现行 ID；条款号补齐 | 注释里的缺陷 ID 全仓可解析且严重度与登记面一致；空"§"命中 0 | CR-31-1、CR-31-2 |
| `sdet_angle_guard.h` 的 `theta` 值域（半开区间只在注释） | 值域写进合同 [6] 列并由校验实现，注释只作指针 | 合同面声明值域，且实现有对应用例判红越界 | CR-31-2 |
| `p1star_tests_core.cpp` 的 flux/mag 口径注释（Moffat4 振幅 A + FIX-STAR-B 佐证） | 按合同正本改为"检测侧椭圆高斯峰值振幅 A，两列各按自身母函数口径（DISP-STAR-007）"；佐证改用非饱和星并从跟踪面标定件取 | 注释口径与 `DATA_SEMANTICS` flux 行一致；引用的标定值能在跟踪面复算 | CR-30-09 |
| `star_detection/README.md` 的 §5 口径、`:53` 列序、`:170-171` 的"OpenMP 3 处 / 1/N worker 缩放" | 可机械核对的计数由源码派生（不手写），列序由 schema 派生，性能条目明写"无 OpenMP 时该族判据无证据" | README 内计数与 `git grep -c` 实测相等；列序与合同列定义一致；perf 族有 OpenMP 可用性前置断言 | CR-30（README 位点）、CR-30-13 |
| `docs/science/STAR_DETECTION.md`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md`、`docs/plugins/algorithms_phase1/03_star_detection.md` 的 GSL 后端登记 | 三级文档统一改为现行 `nls_lm` 后端及其失败语义（`Status::MaxIterations` 等），与代码侧"禁 GSL"声明同向 | 拟合后端语境下 `git grep -n GSL -- docs/science docs/algorithms docs/plugins` 命中 0 或与实现一致 | CR-29-04 |

### 3.7 天测解算 platesolve / IPV

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| "生产入口"的四处说法：`ipv_api.h:272-277`、`test/CMakeLists.txt:162-163`、`platesolve/README.md:54/79/93`+`module.yaml`、`PUBLIC_API.md:919/945`+`DATA_SEMANTICS §18.1`+`ipv_entry.cpp:673-674/718-719` | 由调用图定案唯一生产入口，四处同批改；错锚改符号锚；路径 A/B 的"仅测试/生产通道"两说消除 | 四处文档与代码对实调函数名同值；`ipv_entry.cpp` 注释与 `DATA_SEMANTICS` 节标题不再互斥；行号锚 0 | F-CR26-19、CR-27-06 |
| `DATA_SEMANTICS.md:956/972/985/994`、`PLATESOLVE.md §11/§18.2`、`TRACEABILITY_MATRIX.csv:13` 的逐符号行号锚与 VERIFIED | 改符号锚并两文档同批同值；VERIFIED 的可复核性恢复以锚命中+测试注册为前置 | 两文档锚集合同构且逐条命中；`ALG-WCS-001` 的 VERIFIED 有可执行载体 | CR-27-05、CR-29-22 |
| `ipv_kvector.h` 的"供 IPV 各阶段复用"与 `API_CONTRACTS.csv:310/311` 的 VERIFIED+占位 TEST 列 | 零调用符号撤登记或补测试并注册构建图；`error_model` 列的 381/381 同文模板改为逐 API 语义（本模块三行与头文件相反先订正） | VERIFIED 行的 TEST 列指向构建图内的注册目标且含比较断言；`error_model` 列不再整表同值 | CR-27-16、F-CR26-12、F-CR26-13 |
| `ipv_angle.h:5/7` 的 `CDA §5.3`/`ipv_cda_distortion_design.md`、`ipv_angle.cpp:28` 的 printf 声明、`:404` 的"设计文档默认" | 不存在的文档锚删除或把该设计文档入库到 `docs/algorithms/`；日志出口声明与实现取同一机制；常数锚随权威落点补齐 | 注释引用的文档全部在跟踪集；日志出口声明与实现一致；`alpha=0.5` 有可解析锚或标待确认 | F-CR26-07、F-CR26-10 |
| `ipv_distortion.h:32` 的 `valid` 判据与 `undistort_stars` 的静默恒等回退 | 头注判据按实现改写；回退须显式登记原因（错误传播合同） | 声明判据与实现条件逐项等值；无效分支在产品面可见 | F-CR26-15 |
| `api.h` 的字段计数注释、`log_dir` 缺省三说、`:14` 日期、`[P27-DEAD-PARAMS]` 段、`:284/:310` 的证据载体 | 成员数与判据按实现改写；`log_dir` 缺省语义单一化并入合同；日期与流水号段清理；`问题扫描/_cache/v20_keys.json` 与 `run/perf-fix/...` 的引用改落跟踪面或删除 | 字段数/判据/缺省三处与代码同一；被引证据 `git cat-file -e` 通过；注释无日期无流水号段 | F-CR26-22、F-CR26-21、CR-26 api.h 位点、CR-27-12 |
| `ipv_itertrans.cpp:336-337` 的 `tolerance` 说明 | 按实际承重改写（首轮预过滤 + σ 钳制 + 相对阈），并把 `5*tolerance²` 与实现的 `10·tol²` 差异订正 | 注释描述的参数角色与实现三处使用一致；系数文字与代码一致 | CR-27-12 |
| ABI 越界锁与 `ipv_api.h:72` 的"绝不按大结构体写小缓冲"契约 | 门的负例改为真实小于 `sizeof(IpvParams)` 的缓冲，或头注缩小契约措辞到门实际所验 | 存在一个能检出越界的负例（当前形态应判红）；头注措辞不强于门所证 | CR-28-7 |

### 3.8 登记面（跨模块）

| 哪个文件的哪一面 | 要改成什么 | 怎么算改好 | 出处 |
|---|---|---|---|
| `TRACEABILITY.csv`、`TRACEABILITY_MATRIX.csv`、`API_CONTRACTS.csv` 的状态列 | 每条 VERIFIED 带可执行载体（注册目标名 + 门名 + 证据路径），非生产通道与产生器不得充当证据 | 逐行可指到注册进构建图且含比较断言的目标；无占位 TEST 标识符 | CR-07-12、CR-27-16、F-CR26-13、F-CR26-12、CR-21-10 |
| 以 `run/` 为载体的证据锚（GSL 对齐、FWHM 因子、独立 verifier、`used_fallback` 计数） | 证据落跟踪面（`实验/`），不可写证据即非零退出；回退路径计数补断言 | 每个证据锚路径在跟踪集内可解析；证据文件 0 字节时该件判红（实跑核对 **需复测**） | CR-18-2、CR-29-03、CR-33-18、CR-09-18 |
| `eng/ci/prod_wiring_baseline.json` 的 unbuildable/冻结项与 `CMakeLists` 自述 | 豁免条目随被豁免对象消失而回收；同目录自述与文件树一致 | 基线内不存"已可构建"项；W4 读数与逐条核对结果相等（收紧由前台执行，本审计不跑 `eng/**`） | CR-10-03、CR-11-2、CR-12-07 |
| 门表 `GATES_AND_TOLERANCES.md` 的缺项（Gate D 四判据、L0/L2 阈值、p1psf 五容差、SAT-001 标定参数、门号 `G-P1-CENTER-CONTRACT-1`） | 全部入表并由门文件引用；测试源码不单独持有门号与阈值；登记的"实测值"配可复现证据 | 测试里每个门号与阈值在门表有唯一行且数值相等；引用的实测值可由跟踪面复现代码重算 | CR-11-15、CR-11-29、CR-11-24、CR-33-10、CR-16-12、CR-11-11 |
| 注释与 README 内的日期、任务/缺陷流水号、废止条文引用（25 处位点，见 ⑤） | 在各模块注释清理的同一次改动里去掉，不改任何判据数值；实验编号作为证据指针可保留并指向实验单元 | `lib/**` 与模块 README 内 `20YY-MM-DD`、`wave W1`、`RETIRED/作废/已废止`、`FIX-*/M3b-*/B4-*` 命中 0（除指针用法） | ⑤ 位点表、CR-13-07 |
| 测试组名/注入注册表的注释副本（`p1phot`、`p1noise`、`p1psf`、`p1drz` 族） | 清单从单一来源（`constexpr` 名字数组 / CMake）派生，注释不再手写第二份；未知组名与零断言执行不得返回 0 | 注释清单 = 注册表 = 构建面清单；未消费注入名与未知组名报错（判据方向须实跑确认，**需复测**） | CR-03-2-03、CR-03-4-01、CR-16-19、CR-34-2、CR-34-3、CR-34-1 |

## 4 须与门禁联动的部分

以下失真形态靠人工评审不可防复发，只能由机器门承接；此处只登记"需要哪种判据被机器门覆盖"，门的实现、注册与判据设计归门禁重建件（`05_门禁/`），本报告不设计门。

| 失真类型 | 机器门须覆盖的判据方向 | 成稿出处 |
|---|---|---|
| 锚漂移、锚指向无关语句、锚指向跟踪面外 | 注释与文档里的每个引用锚必须按内容命中；行号锚不作为定位凭据；证据锚必须落在跟踪集 | CR-32-3、CR-27-05、CR-29-22、CR-04-30、CR-10-08、CR-18-2、CR-29-03、CR-33-18、CR-21-11 |
| 登记状态虚标 | 状态词（VERIFIED/收口/已刷新/已闭合/已接线）必须能指到可执行证据：注册进构建图的目标名 + 该目标内的非恒真断言 | CR-07-12、CR-27-16、F-CR26-13、F-CR26-12、CR-09-2、CR-21-10、CR-22-06、CR-09-16 |
| 豁免与棘轮常驻 | 登记条目必须随被登记对象的状态变化而回收，且回收动作可核对（现存量与逐条核对结果相等） | CR-10-03、CR-12-07、CR-11-2 |
| 清单多副本漂移（组名、门名、测试名、模块源文件、字段数） | 同一清单只允许一个来源，其余面由其派生并比对；按文件/成员计数不得把零定义 TU 与零调用符号算作交付 | CR-03-2-03、CR-16-19、CR-34-2、CR-34-3、CR-19-08、CR-32-11、F-CR26-22 |
| README 端口表与合同异源 | 对外端口表（形状、列序、失效语义）须与 schema/合同双向比对；注释宣称的约定（如 `sx≥sy`）须有对应断言或从注释移除 | CR-32-4、CR-33-3、CR-32-9、CR-05-10、CR-04-22、CR-30（README 列序与计数位点） |
| 生产入口声明与调用图不符 | "生产 X"的每一处声明须与调用图核对；被点名函数在生产不可达即判红；同一入口的文档面与注释面不得互斥 | CR-17-1、CR-23-05、CR-22-05、CR-27-06、F-CR26-19、CR-15-02 |
| 注释/README 承载只有它一处的判据、降级或义务 | 注释里出现的 rule_id/条款号/阈值语义须在合同与登记面命中；降级只在注释即判红 | CR-15-01、CR-05-13、CR-05-16、CR-02-17、CR-31-1、CR-15-10、F-CR26-07 |
| 日期与流水号 | 词面核对即可（零行为后果，故必须机器防：人工逐条必漏） | ⑤ 位点表（12 日期 / 10 流水号 / 3 废止引用） |

配套事实供门禁重建引用：`eng/ci/checks.json:1022/1055` 已注册 `docs/algorithms/anchors/check_doc_line_anchors.py` 与 `eng/tools/doccheck/check_alg_line_anchors.py`，但 CR-32-3（25 条锚全漂）、CR-27-05（两文档互斥锚）与 CR-29-22（错锚扩散到生产源与构建文件）三族在该基线上仍成立——其适用域与判据强度是否覆盖 README/module.yaml/CMakeLists 三类载体，须由门禁侧定案（**需复测**：本审计不跑任何 `eng/**`）。

## 5 边界

- **覆盖**：通读成稿 `CR-01`…`CR-35`（缺 CR-08；CR-26 用 `F-CR26-nn` 编号，已并入同号体系）。全部 555 条 finding 中按失真主体取 96 条入本件分类，其中 ①27 ②19 ③20 ④23 ⑤1（该类位点共 25 处，另 23 处成稿按"登记不计数"未立案）⑥6。
- **未覆盖批次**：`CR-36`…`CR-44` 现为派单骨架或半读成稿（`spherical_overlap.*`、`p1drz_*` 族、`ipv_select.cpp`、`ipv_sip.cpp` 及 platesolve 其余生产实现文件）。因此**球面面积交叠族（创新点④几何）与 platesolve 的 IPv 生产实现族**是本报告的注释/README 盲区，其登记面（`PUBLIC_API.md` 的 platesolve 行、`API_CONTRACTS.csv` 的 IPV 段）只按已读部分作结。
- **未覆盖模块族**：`lib/infrastructure/benchmark`、`hips_browser`、`acr` 客户端本体、CLI 三命令入口层（`lib/infrastructure/cli/{normalize,mosaic,export}`）仅在 CR-05/CR-12/CR-20 顺带触及，未逐文件通读；根 README、`docs/**` 文档集与 `docs/DOCUMENT_INDEX.yaml` 的失真属 D6 前两份交付件范围，本件只在"与代码注释相接"处引用其条文。
- **凡需实跑定案的条目**（本审计零构建、零 ctest、零 `eng/**`，一律标 `需复测`）：
  1. 已注册锚门对上述三族错锚是否判红（CR-32-3、CR-27-05、CR-29-22）。
  2. `p1phot`/`p1noise`/`p1drz` 的组名与 ctest 注册实跑差、未知组名与零断言执行的返回码（CR-03-2-03、CR-34-1、CR-34-2）。
  3. `_qf` 通道在 CI 是否有已注册绿证据（CR-02-12 自标 PARTIAL 的缺口）。
  4. phase1 与 direct 两档 HiPS flags 的 SNR 位差异是否产生"声明 snr 位而子产品集为空"的产品（CR-09-9 待实跑项）。
  5. SAT-001 门与 L0/L2 冻结容差的"文档登记实测值"可复现性（CR-16-12、CR-11-11）。
  6. 证据文件 0 字节时相关门是否仍全绿（CR-09-18、CR-18-2）。
  7. ⑤ 类清理（去日期与流水号）是否触碰任何门的读数字面——若有，属门须改而非注释须留。
- **不判定为缺陷的东西**（沿用成稿口径，避免本报告造第二套标准）：测试期望值与夹具内容差、单位换算定义常数、已由登记面显式声明退役的对象、以及同一文件前后两段各有适用域的重复表述。
