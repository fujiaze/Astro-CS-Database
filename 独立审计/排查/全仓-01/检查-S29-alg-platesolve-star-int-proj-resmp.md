# 检查-S29 ｜ lib/algorithms/{platesolve, star_detection, integration, projection, resample}

- 切片：platesolve/cpp/ipv（15 头 + 14 源 + 11 测试 + wrapper_phase1）、star_detection（include/src/test/tests-p1star/wrapper_phase1）、integration/v6（3 头 + 3 源 + 6 oracle + v6_phase1）、projection（p3_projection / p3_proj_v6 / p3_wcs + tests/p3wcs）、resample（p3_resample + 6 个 p3_rsmp_* 实现 + tests）。
- 交叉文档：docs/science/{ASTROMETRY, STAR_DETECTION, INTEGRATION}.md、docs/science/algorithms/{PLATESOLVE, STAR_DETECTION_ALGORITHMS, INTEGRATION_ALGORITHMS, PHASE3_RESAMPLE, PHASE3_RSMP_IMPL, PHASE3_PROJ_IMPL}.md、docs/contracts/DATA_SEMANTICS.md（§28.6 / §31.1）。
- 方法：纯静态（未编译、未跑测试、零 git 写、唯一写入 = 本报告）。行锚一律本轮 grep -n / sed 实开；涉及公式的逐项展开手算；发现先过 独立审计/实验重做/总编对账/检查-修复验证.md PASS 表与 分歧台账.md D-01…D-11、§2 A-* 裁决（已订正/已裁决不重报），再与同轮兄弟切片 S2/S4/S5/S6/S22 逐条去重。
- 结论：**红 2 / 黄 4 / 绿 2**。

---

## 一、红（必须改）

### S29-R1 ｜④幻觉与锚（并涉③）｜docs/science/STAR_DETECTION.md:14、:114、:166 与 docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:95、:142、:162、:191、:199、:206、:234 —— 生产拟合后端仍写作「GSL trust-region LM / gsl_multifit_nlinear」，代码已换成仓内自研 nls_lm 且根构建明令禁止再引入 GSL

- **问题描述**：SCI 冻结页写「基线算法 = peaker 七步候选 + **椭圆高斯 GSL trust-region LM 拟合**」（:114）、定位写「椭圆高斯 **GSL-LM** 中心」（:14）、参考写「实现对照 GSL gsl_multifit_nlinear」（:166）；ALG 页把 CPU 后端合同写成「纯 CPU；**GSL gsl_multifit_nlinear trust-region LM（trs=gsl_multifit_nlinear_trs_lm**…）」（:191）、伪代码写 fit = GSL_TR_LM_GAUSS(...)（:142）、失败分支写「**GSL≠SUCCESS** 或非有限值」（:162）、历史写「IPv 手写 LM 已被 **GSL trust-region LM** 取代」（:199）、源码表写「sdet_lm_fit（**GSL TR-LM** 7 参）」（:234）。生产实际后端 = 仓内自研 nls_lm，上述各句未随替换更新。
- **证据（含反方核验）**：
  1. 代码事实源：lib/algorithms/star_detection/src/sdet_api.cpp:28「拟合后端: **自研信赖域 Levenberg-Marquardt**（见 src/nls_lm.h；该头载有不得退回外部求解器的禁止性说明）」；:375-387 调 astrocs::star_detection::nls::solve；:40-42「**原后端** GSL 2.8 driver 的 ftol 判据实测从不触发…」＝代码自陈替换事实。
  2. 构建面：根 CMakeLists.txt:898「GSL-REPLACE-01: 自研信赖域 LM 求解器（替代外部 GSL gsl_multifit_nlinear）」、:906-908「**禁止**重新引入 GSL/gslcblas: Windows 无来源，且其为 GPL 族」；lib/algorithms/psf/tests/p1psf/CMakeLists.txt:113-115 同令；lib/algorithms/star_detection/Makefile:7「依赖: 仓内自研 trust-region LM (src/nls_lm.cpp), OpenMP; **无外部库依赖**」。全仓 CMake grep gsl 零命中（无 find_library(GSL)、无链接项）。
  3. **反方核验**：(a) nls_lm.cpp:3/:14-17 自述「与 GSL gsl_multifit_nlinear_trs_lm + solver=qr 的公开语义同类…GSL 只作为行为对齐的**实测对象**」⇒ :166「实现**对照** GSL」在“对照对象”意义上仍成立，本条不指控该句，只指控把对照对象写成**生产后端身份**的 :114/:14/:191/:142/:162/:199/:234。(b) 反方可能称“语义对齐即等价”——但 :191/:206 还指名 trs 子算法与 XTOL/GTOL/FTOL 编译期常量，而 nls_lm 另有 GSL driver 没有的 xtol_abs=1e-6（nls_lm.h:39 标注由 GSL 行为标定）与 kAcceptRho=1e-4（MINPACK）等自有常量 ⇒ 不是同一对象。(c) 非 PASS 表已核项：检查-修复验证.md 残余仅 3 条论文摘要 k_shape + UPM_SOLVER.md:26 锚；分歧台账 D-01…D-11 与 A-* 各条不涉本条；对 独立审计/排查/ 全目录 grep「GSL」「nls_lm」零命中 ⇒ 兄弟切片未报。
- **建议改法**：按 §8 变更流程统一「后端身份」表述——SCI:14/:114 与 ALG:95/:142/:162/:191/:199/:206/:234 改为「仓内自研信赖域 LM（src/nls_lm.{h,cpp}，行为对齐 GSL gsl_multifit_nlinear_trs_lm；GSL 仅作对照实测对象、不链接）」；对齐证据 run/GSL-REPLACE-01/REPORT.md §3 为 run 产物不入库，需补库内可复现登记（同 S29-Y2 类风险）。本检查不代改冻结页。
- **所属面**：④（主）+ ③（SCI/ALG 两方写 GSL、GATES 无对应句 ⇒ 同一“后端”在三处仅两处有断言）。

### S29-R2 ｜④幻觉与锚｜docs/contracts/UNIFIED_OBJECTS.md:120 与 lib/algorithms/integration/v6/oracle/CMakeLists.txt:5 —— 声明的「合同机器门」recon_contract_gate.py 全仓零调用方、未注册任何门；oracle CMake 自述的 ctest 归属也不存在

- **问题描述**：UNIFIED_OBJECTS.md:120 明写「**合同机器门**：eng/tests/contracts/test_unified_object_contract.py（对象级）与 lib/algorithms/integration/v6/oracle/recon_contract_gate.py（本次新增声明面）」。实际 recon_contract_gate.py 是独立脚本（自带 argparse，--out 默认写回本目录），**没有任何调用方**；同时 v6/oracle/CMakeLists.txt:5 注释称「本文件只登记 Oracle 目标，**不注册 ctest（eng/tests/ 归测试分片）**」，而 eng/tests/unit/CMakeLists.txt 对 weight_chain_selfcheck、recon_dump、recon_contract_gate、weight_chain_oracle **零命中**；eng/ci/checks.json（14206 行）对上述名字零命中 ⇒ 声明为“机器门”的东西在门注册表与 ctest 两面都不存在。
- **证据（含反方核验）**：
  1. grep -rn "recon_contract_gate" eng/ lib/ docs/ 实验/ ⇒ 仅 4 条自述（docstring/用法/out 路径）+ UNIFIED_OBJECTS.md:120 声明行，零调用方。
  2. grep -n "weight_chain|recon_" eng/tests/unit/CMakeLists.txt ⇒ 空；grep -n "weight_chain|recon" eng/ci/checks.json ⇒ 空。对照：**v6_p3_rsmp_*** 通配在 checks.json:2613/:3535/:3571 在册 ⇒ 登记机制本身有效，本条不是“机制不存在”。
  3. **反方核验**：(a) Oracle 目标本体存在且可构建（add_executable(weight_chain_selfcheck):12、add_executable(recon_dump):25，且 docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:27-28 登记为 tool/非发布目标）⇒ 本条指控的是**门声明与注册面**，不是“目标不存在”。(b) 反方可能称“可手工运行”——但 :120 措辞与同句 test_unified_object_contract.py 并列为“合同机器门”，读者据以认为 CI 生效。(c) 非 PASS 表已核项（该表无此条目）。
- **建议改法**：二择一并写实——① 真接线：把 recon_contract_gate.py 纳入 eng/ci/checks.json（或加 add_test），使“机器门”名副其实；② 改措辞：UNIFIED_OBJECTS.md:120 降级为“可手工运行的声明面校验脚本”，并把 v6/oracle/CMakeLists.txt:5 的“eng/tests/ 归测试分片”改成实际归属（现状＝无归属）。
- **所属面**：④。

---

## 二、黄（建议改）

### S29-Y1 ｜④幻觉与锚｜lib/algorithms/platesolve/module.yaml:4-16 头注 —— 12 个 C ABI 导出行锚全部失效（自称“grep 实测”），行数声明亦不符

- **问题描述**：module.yaml 头注逐符号给出行锚：ipv_solve_create :237 / ipv_solve_destroy :249 / ipv_set_gaia_handle :260 / ipv_set_detector_handle :273 / ipv_get_default_params :286 / ipv_get_last_inlier_count :314 / ipv_get_last_inliers :328 / ipv_solve :345 / ipv_solve_from_memory :377 / ipv_solve_from_detections_v1 :524 / ipv_solve_from_memory_with_callback :566 / _d :610，并自述「12 个 C ABI 导出符号 **grep 实测**，禁止手抄他版」。
- **证据（含反方核验）**：本轮 grep -n '^IPV_API' ipv_entry.cpp 实开 = **:369 / :381 / :392 / :405 / :418 / :459 / :473 / :490 / :525 / :675 / :720 / :767**，12/12 全不中，漂移 +132…+157 **且非均匀**（逐函数伸长后未重刷，非整体平移）。行数声明「内核 ipv_solver/select/triangle/itertrans/robust_refine/wcs/sip 共 13821 行」：该 7 件 src+include 实测 **9650**（8524+1126），全部 src+include = **14648**（含 ipv_entry.cpp 809；14648−809=13839 也 ≠13821）⇒ 该数既非所列 7 件、也非现状全量。**反方核验**：exports 清单本身正确——12 项与实际 12 个 IPV_API 定义逐一对应、名字零缺漏（导出**词表**面③无问题）；source_symbols（W1 收缩后 7 项）与 exports（12 项）分列结构自洽。
- **建议改法**：重刷头注 12 个行锚（或改为“按符号名定位、行号不作依据”的弱锚写法）；行数改为 wc -l 可复算口径并注明统计范围。
- **所属面**：④。

### S29-Y2 ｜④幻觉与锚｜lib/algorithms/integration/v6/include/astrocs/v6/variance_propagation.h:4-8 —— 「权威依据（只读，不改）」两份报告全仓不存在，upm.h 行锚亦漂移

- **问题描述**：头注把科学形式的权威依据列为 ①reports/RELEASE-02/unc-prop-audit.md §3、②reports/RELEASE-02/q2-snr-smooth.md §2/§5/§7、③lib/algorithms/coverage/include/astro/phase2/upm.h:225-228、④docs/plugins/algorithms_phase2/11_upm.md §4.1。
- **证据（含反方核验）**：ls reports/ ⇒ 仅 v19r2；find . -name unc-prop-audit* -o -name q2-snr-smooth*（排除 build）⇒ **零命中**，即被标“权威依据（只读，不改）”的两份文件库内不存在（应为已回收的 run 产物）。③行锚：upm.h 现行 C_theta=(J^T W J)^-1 / C_out=C_stat+J_out C_theta J_out^T 在 **:256-257**（另 :360-365 为 API 注释），:225-228 处内容是“V6 目标态：UPM 乘法/加性分离求解器”段首 ⇒ 锚偏 31 行；④文件存在 ✓。**反方核验（科学面）**：本条**不指控公式**——本轮手算核过：P=I−H 下 Var_i=Σ_j P_ij²σ_j²（对角 Σ）成立；N 帧均值正确支 σ²(1−1/N)、朴素支 σ²(1+1/N)，N=8 比值 1.125/0.875=1.285714 与注释 1.2857× 一致；corrected_pixel_variance=[PΣPᵀ_ii+param_var]/g² 与“÷g² 硬要求”一致 ⇒ 面①该项通过。
- **建议改法**：①②改为库内可复现锚（条款/文献号或 docs/ 条文）或注明“run 产物（不入库）及回收策略”；③重锚 :256-257。
- **所属面**：④。

### S29-Y3 ｜④幻觉与锚｜p3_rsmp_kernel_registry.cpp:2、p3_rsmp.h:126、:263、eng/tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h:167 —— 代码四处引用的合同文件「ALG-P3-001_KERNEL_REGISTRY §1-§5 / §3.1 / §3.3」全仓不存在

- **问题描述**：registry 源首行「采样核 registry（FZ-P3-KERNEL-REGISTRY；**ALG-P3-001_KERNEL_REGISTRY §1-§5**）」、p3_rsmp.h:126「bilinear_4quad 注册证据（**… §3.3**）」、p3_rsmp.h:263「四象限最近中心双线性（**… §3.1**）」、p3_rsmp_oracle.h:167「bilinear_4quad 独立 Oracle（**… §3**）」——四条都以该文件为段级权威。
- **证据（含反方核验）**：grep -rln 'ALG-P3-001_KERNEL_REGISTRY' docs/ eng/ 独立审计/ 实验/ lib/ ⇒ 只有上述 4 处**代码引用**，无任何 .md/.json 合同文件；grep 'bilinear_4quad' docs/ ⇒ **零命中**（:3.1/:3.3 所指段落在 docs/ 无处可查）。**反方核验**：同一行的 FZ-P3-KERNEL-REGISTRY 是**真条款**（DATA_SEMANTICS.md:2665-2677，且贯通 v6_clause_registry_v1.json / schema / tests）⇒ 本条不是“冻结条款不存在”，而是**代码把段级权威指向一份不存在的文档**，段引无法回溯；独立审计/证据/通读-CR-60.md:102 曾把该引当证据源引用，也未发现文件缺失。
- **建议改法**：4 处段引改指真实权威（DATA_SEMANTICS FZ-P3-KERNEL-REGISTRY §28.6 相应小节），或补建该合同并入 traceability；二择一，勿留悬空段引。
- **所属面**：④。

### S29-Y4 ｜③跨文档冲突（并涉④）｜p3_rsmp_kernel_registry.cpp:205/:226 ↔ docs/contracts/DATA_SEMANTICS.md:2669-2673 ↔ eng/contracts/schemas/product_family_field_constraints.schema.json —— registry 唯一「生产科学默认」id 不在冻结核词表内

- **问题描述**：registry 把 bilinear_area_overlap_exact 登记为 production_science_default = true（:226，全表唯一 true），而 FZ-P3-KERNEL-REGISTRY 词表只有 nearest / bilinear_4quad / higher_order 三行（DATA_SEMANTICS.md:2671-2673），schema 的 family 枚举同集合 ⇒ 同一“产品语义取值”在注册表与冻结词表两处口径不一致（词表既无该 id，也无 exact 族）。
- **证据（含反方核验）**：(a) **实现/选路侧已另行立案、本条不重复**：独立审计/08_修复包/④面积交叠与分配/01_缺陷清单.md:151-155/:518/:525 与 03:16（缺-9/F₃）已把“零实现 + 从不查 registry + production_science_default 零使用者”立案，S22-修复包④亦逐条实开 :203-227 ⇒ 本条**只登记词表漂移**这一未见登记的面。(b) 反方可能称“frozen() 是实现登记、FZ 是产品族，粒度不同”——但 FZ:2667 明写「取值来源 = **registry 登记项**」把词表义务指回 registry，两向对不上必有一处要改。(c) D-10 方向核验：weight_chain.h:85-126 / weight_chain.cpp:139-155 默认 = natural_bicubic_spline_clip_v1、bilinear_regular_grid_v1 明注「对照/回退」⇒ P4 侧词表**写法正确**，与本条（P3 重采样核）不是同一对象，不构成 D-10 违规；Phase3 导出 bilinear 默认已有裁决，不重报。
- **建议改法**：词表择一对齐——要么 FZ 词表/schema 枚举补 bilinear_area_overlap_exact（连同 08_修复包④ 03 的注册证据验收），要么撤下 registry 的 production_science_default=true；与缺-9 同批闭环，勿只改一头。
- **所属面**：③（并④）。

---

## 三、绿（可不改）

### S29-G1 ｜②行文逻辑｜docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:58 与 :353 —— κ₅₀ 同文两值（6.22±0.50 vs 6.21±0.50）

- §2 正文「实测系数为 6.22 ± 0.50」，§11.4 冻结表「50% 行的单参数式为 κ₅₀ = 6.21 ± 0.50」。差 0.01，两处均**非验收判据**（§11.4:351 明写“单参数闭式不足以定义召回域，验收一律用上表”），不影响任何门；下次改文统一为一处引用即可，非必改。

### S29-G2 ｜④幻觉与锚｜docs/science/STAR_DETECTION.md:146 ↔ wrapper_phase1/star_detector.cpp:30-70 —— 锚落在说明注释块而非函数体

- :146 用 star_detector.cpp:30-70 指「StarDetector::estimate_background 的 2 轮 median±3σ 裁剪后 RMS」：实开 :30-70 是 RETIRED-CODE-RETAINED 说明块，函数本体在 **:75-118**（def :75、sigma-clip 2 轮 :94、1.482602218505602*mad :105、3.0*(s>0?s:1e-9) :110）。注释块与被指对象**同题且可核**（块内复述函数行为与缺陷史）故判绿；顺带记录该注释块**自指**行号（“:67 的裁剪后 RMS”“:50/:51”“detect()（:120-123）”）也已漂至 :117-118/:105/:135 一带，属同源行号老化。S4-scienceC 的 SCI 锚表列了 :21/:66/:68/:79/:117/:140，**未含 :146** ⇒ 不算重复。

---

## 四、同轮已覆盖 / 已裁决 —— 本切片查到但不重复计数

| 项 | 本切片复核事实 | 不重复报的依据 |
|---|---|---|
| ALG STAR :59-61 逐档 99% 召回阈内联值 46.0/19.0/10.0 与冻结表 ∈[52,65]/20.0/12.0 冲突 | 已实开 ALG §2:60-61 vs §11.4:321 vs SCI:28 vs GATES:58 | S5-algorithmsA **A5-R7** 已报 |
| STAR 两文档 sdet_api.cpp 锚系统性 +11、2973→2962 | 已复核实际 :451/:1772/:1779/:1761/:1845/:1823/:1738（SCI:117 更落后 139 行） | S4-scienceC 表 + S5 **A5-Y13** 已报 |
| PHASE3_RSMP_IMPL §4 表结构断裂 / FIFO-LRU 四说 / 锚两代 / 201-202 行 | 已复核 :103-117 断裂、:73/:218/:347/:382 FIFO vs :277-284 LRU、代码 :64-122 LRU | S6-algorithmsB **S6B-R07/R08/R09** 已报 |
| PHASE3_PROJ_IMPL 行数与锚自相矛盾（166 vs 373；p3_session 329 vs 441） | 已复核 p3_wcs.h 373、p3_session.cpp 441（RSMP_IMPL 同量 441 ✓） | S6 **S6B-R10** 已报 |
| ASTROMETRY §5a:107「标签错误」vs §14a:265「与 §5a 一致」互斥 | 已复核 ipv_wcs.h:43/:57-60/:70-71 现文确为 0-based | S2-scienceA **红-3** 已报 |
| area_overlap 零实现 + production_science_default 生产不可达 | 已复核仅 registry 两处命中、make_*_neighborhood 只有 4quad/nearest | 08_修复包④ 缺-9 + S22 锚实开 |
| ipv test_synthetic.cpp 未进 CMake/ctest | 已复核 Makefile 仅 test_kvector.exe、CMake 零注册 | 独立审计 **CR-46-10** 已立案 |
| D-10 生产默认算子、Phase3 导出 bilinear 默认 | 已核 weight_chain 默认 token 正确 | 分歧台账 D-10 + 检查-跨文档冲突 条目6 已裁 |

---

## 五、已查无问题面

### ①科学性（常数 / 公式 / 单位 / 定义域）
- **BUNIT 单位正字**：p3_rsmp_units.cpp 规范串 signal_sb="ADU/sr"、sb_variance="ADU^2/sr^2"、ivar="sr^2/ADU^2"、sr=px_power/2 ⇒ 与 DATA_SEMANTICS §31.1 冻结集逐字符一致。*方法*：逐行 sed 对读两处字符串。
- **Q/W 重算公式**：p3_rsmp_propagation.cpp 实现 W=a*a*pi_cinv_pi、Q=a*pi_cinv_f、F_hat=Q/W、var_F_hat=1/W ⇒ 与 DATA_SEMANTICS.md:2681-2684 逐项同式（含“重算面在输出帧”取值来源）。*方法*：符号级对照 + 单例手算。
- **协方差传播**：p3_rsmp_covariance.cpp:22-46 按 M=R C_x、C_y=M Rᵀ 两遍稀疏累加，与头注 FZ-FORMULA-COV-PROP 同式；DenseMatrix(r,c) 零初始化（p3_rsmp.h:281，专查 += 是否踩未初始化）⇒ 无隐患。
- **重建算子数学**：weight_chain.cpp:206-227 自然边界二阶导（M[0]=M[n-1]=0 的 Thomas 递推，b0=4、cp/dp 前代回代）与标准方程组 M[i-1]+4M[i]+M[i+1]=6(y[i+1]-2y[i]+y[i-1]) 逐项吻合；natural_eval_1d_strided 的 y0+b*h+m0*h²/2+(m1-m0)h³/6、b=(y1-y0)-(2m0+m1)/6 展开三次 Hermite 后**逐幂次一致**（t⁰=y0、t¹=b、t²=m0/2、t³=(m1−m0)/6）。
- **D-10 默认算子**：weight_chain.h:85-126 + weight_chain.cpp:153-155 sparse_recon_operator_default_token() ⇒ natural_bicubic_spline_clip_v1；bilinear_regular_grid_v1 注释明为“对照/回退；无钳制、无滤波”；未知 token parse 返回 false ⇒ fail-closed。**词表方向未写反**。
- **方差传播对照式**：variance_propagation.h:19-25 朴素式/正确式与 N=8 比值 1.2857 手算一致（详见 S29-Y2 反方核验）。
- **背景噪声估计**：sdet_api.cpp:451-504 行差分 → 3 轮 5σ clip（MAD×1.482602218505602）→ 行标准差 → 行中位 → ×0.70710678，与 ALG §2:34-49 同序同常数；阈值 img_median+T(5.0)*bgnoise（:1779）、σ_smooth=2.0（:1761/:1763）、norm=65535.0f（:1823）逐项对得上（行号漂移已由 S4/S5 计）。
- **platesolve 科学常量**：CONV_THRESH_ARCSEC=0.01（ipv_solver.cpp:199，注释自带 PLATESOLVE.md 反向锚）、三角匹配 5.0″（:660/:670）、δ=1.345×median_abs_r（ipv_sip.cpp:242）、IRLS_MAX_ITER=15（:408）⇒ 与 PLATESOLVE.md:185-200 同值同锚。
- **TAN 公式**：wrapper_phase1/wcs_tan.cpp:47-52 标准 gnomonic 分子分母 + 弧度域→度域 ×180/π 注释；p3_wcs.cpp:515-542 正反映射入口 ⇒ 与 ASTROMETRY §5 式一致，未见符号/单位错。

### ②行文逻辑（除已报与同轮已报外）
- docs/science/algorithms/PLATESOLVE.md（381 行）通读：§1-§11 论证链完整，DISP-WCS-001..006 与 module.yaml notes 对得上；无“把 UNRESOLVED 写成结论”、无订正前后两说并存。
- docs/science/algorithms/PHASE3_RESAMPLE.md（202 行）G1-G5 公式段与 §3 伪代码、§8 实测锚内部自洽（586 行头注偏差已由 S6 计）。
- docs/science/algorithms/INTEGRATION_ALGORITHMS.md（118 行）F1-F6a 与伪代码逐行对应、§1a 状态枚举表自标“代码事实抄录”并给机器门名，无断链。
- integration/v6/README.md、projection/README.md、platesolve/module.yaml 结构段（exports/source_symbols 分列 + W1 收缩注记）语义自洽。

### ③跨文档冲突（除 S29-Y4 与同轮已报外）
- 召回表四源：SCI:28 / ALG §11.4:321 / GATES:58 **三处逐字一致**，冲突只在 ALG §2 内联句（S5 计）；台账无该量裁决。
- 重建算子默认三源（分歧台账 D-10 / weight_chain.h / integration v6 README:37）**一致**。
- LRU：PHASE3_RESAMPLE.md:99/:156（有界 LRU + splice 表头）与 p3_resample.cpp:64-122（lru.splice(lru.begin(),…)）一致；冲突只在 RSMP_IMPL（S6 计）。
- Q/W、Ω、BUNIT、variance 传播四组量在 DATA_SEMANTICS ↔ p3_rsmp_* ↔ PHASE3_RESAMPLE 三处同词同式。
- p3_session.cpp 行数在 RSMP_IMPL（441）与实测（441）一致；冲突只在 PROJ_IMPL（S6 计）。

### ④幻觉与锚（含文献）
- **failclosed 测试非退化**：ipv/test/test_extract_wcs_sip_failclosed.cpp 断言1/2 = 共线 trans + det=1e-30 ⇒ success==false 且 error 非空（负例），断言3 = 非奇异正例 success==true（防“恒失败门”）⇒ **非恒真非恒假**；test/CMakeLists.txt:28-29 已 add_test，hook 在 eng/tests/unit/CMakeLists.txt:1347。
- **阴性对照在册**：ipv_dead_params_lock_selfcheck（P1–P4 负对照自检）、ipv_abi_layout_lock_selfcheck 均 add_test；w34_ipv_last_inlier_reset 在 eng/tests/unit:1638-1646 注册。
- **resample 门在册**：v6_p3_rsmp_{core,oracle,gate,mutation_driver} 四 add_test，且 eng/ci/checks.json:2613/:3535/:3571 以 v6_p3_rsmp_* 通配在册（与 S29-R2 的“无登记”对照，可见登记机制本身有效）。
- **PLATESOLVE.md 行锚抽样 8/8 命中**：ipv_solver.cpp:199（CONV_THRESH）、:660（5.0″）、ipv_sip.cpp:242（1.345）、:408（IRLS 15）、ipv_wcs.cpp:229（extract_wcs_sip 段首）/:157-165（build_wcs 清零区）/:161（CRPIX=width/2+0.5）、wrapper_phase1/wcs_tan.cpp:48-51（TAN 式）、ipv_wcs.h:57-60（x=u+CRPIX 注释）。
- **ipv 导出词表**：module.yaml exports 12 项 vs 源 12 个 IPV_API 定义，名字零缺漏（行锚漂移另计 S29-Y1）。
- **文献 DOI 回读**：INTEGRATION_ALGORITHMS.md:107 的 Aitken 1935 DOI 10.1017/S0370164600014346 实际回读 ⇒ “On Least Squares and Linear Combination of Observations”, Proceedings of the Royal Society of Edinburgh（Cambridge Core, HTTP 200）⇒ 书目与 DOI 一致；本切片**未新增**任何 DOI/arXiv 主张（ALG:407 的 Levenberg/Marquardt/Moré 1978 为标准教科书条目，仅作存在性目视、未据以立论）。
- **第三方代码**：nanoflann / cfitsio / nlohmann-json 只做接口面确认，未进入其内部实现评判。
