# 全仓对抗性静态排查 · 切片 S26（lib/algorithms/drizzle/healpix_drizzle/tests ＋ fits_output/p3_output）

**检查员**: S26 只读检查员（只查不改；本文为唯一产出）
**责任域**: ① `lib/algorithms/drizzle/healpix_drizzle/tests/` 全部（含 p1drz/ 套件，C++ 26 件 + Python 4 件 + shell 2 件 + CMake 2 件）；② `lib/algorithms/fits_output/p3_output.cpp/.h`。测试仅静态审查（判据非退化 / 负例归零 / oracle 与文档常量一致），不编译不运行。
**方法**: 逐文件实开核读；六道专项 oracle 门全文通读；判据结构（CHECK/return 语义/守卫/死变量）逐条走查；与 独立审计/实验重做/总编对账/分歧台账.md D-01…D-11 终裁、检查-修复验证.md PASS 表逐项对数；p3_output 与 docs/science/algorithms/PHASE3_FITS_IMPL.md、docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md 双向互查；file:line 锚全部实开文件核对。
**先读衔接**: 检查-修复验证.md 30 项 PASS 与三报告已裁决事项本轮一律未重报；S6（docs/algorithms 切片）已报的 PHASE3_FITS_IMPL p3_wcs.h 行数（R01/R10）、S15 红-1 已报的 211034.6 五处残留均只作交叉引用、不重复计数。
**日期口径**: 当前工作树（未提交）。

---

## 一、红（必须改，4 项）

### S26-R01 ｜面①（兼④）｜ `lib/algorithms/drizzle/healpix_drizzle/tests/kcorr_matrix_test.cpp:156-161` ＋ `:8-11` —— K_CORR_DOMAIN 适用域判定是恒真门，且声称的 JSON 证据件不存在

- **问题描述**: 该件自称是 k_corr 对 Drizzle 参数适用域的判定器（"结论落入选项 A 或选项 B——证据写入 reports/v19r3/evidence/science/kcorr_matrix.json"），是生产查表值（sampler.cpp kControlCorr 六格表）署名的标定来源。但：① A/B 选项判定只 printf，**无条件 `return 0`**——不存在任何能让它退出非 0 的数值状态；② 全部 cell 被 `continue` 跳空时 `base` 保持 0（:148-150）、`max_dev` 循环空转为 0（:152-153），随后 `:156` 判 `max_dev <= 0.10` 为真，打印 **"[PASS] 差异<=10% → 选项A"** 并退出 0——空集退化反而输出"PASS/选项A"结论；③ 头注声称的证据文件 `reports/v19r3/evidence/science/kcorr_matrix.json` 全文件无任何写出代码（无 fopen/fwrite），`reports/v19r3/` 目录本身不存在（glob 实测空）。
- **证据**:
  - 源码实况: `:106` drizzle 失败 `return 1` 是唯一非 0 出口；`:125` `if (meds.size() < 200) continue;`、`:117` `if (patch.size() < 4) continue` 可把 cell 全部跳空；`:156-160` A/B 两分支均为 printf；`:161` `return 0;` 无条件。
  - 与 D-08 对数（分歧台账:245）: D-08 终裁 k_corr = k_shape×k_geo 两因子 + (ρ, pixfrac, 帧数, patch) 几何查表，**600″ 端低估 2 倍、N=5 端低估 32%**——本测试 `:75` scales={300,600} 正覆盖该维度，按其自身 10% 门（:156）600″ 档相对 300″/pixfrac=0.8 基线的偏差应远超 10% 应判"选项B"，但无论判 A 判 B 退出码恒 0，**判定结果不构成任何门**。
  - 反方核验: tests/CMakeLists.txt:165-170 将其列入 DRZ_EXTRA_TESTS 并自述"作为可执行证据件保留（可手工运行），不注册——避免把与本轮无关的历史红灯带进 ctest 基线"——定位为证据件而非 ctest 门，但证据件的退出码与声称的 JSON 缺失使"证据"二字不成立；前轮 独立审计/证据/通读-CR-13.md:312-315 已独立登记同一恒真位点与"注释虚构"（交叉一致，当前工作树未修）；08_修复包/③加性天光无缝/01_缺陷清单.md:231、03_修复顺序与验收判据.md:184 均点名该件是六格表唯一出处且仍是孤儿。
- **建议改法**: 上呈（涉及标定证据链与 D-08 执行面，不越权代改）: 让 A/B 判定进入退出码（如差异>10% 判 B 时非 0，或对空 cells 直接判红），并补齐头注承诺的 JSON 落盘；或删除头注的证据文件承诺并按 08_修复包"二选一"处置。恒真门在修复前不得再被引用为六格表的证据。
- **所属面**: ①（恒真门、负例归零失败）＋④（声称存在而实不存在的证据件）。

### S26-R02 ｜面④（兼①）｜ `lib/algorithms/fits_output/p3_output.cpp:440` ＋ `docs/science/algorithms/PHASE3_FITS_IMPL.md:300-301` —— 声称"独立重开 verify 覆盖 BUNIT"，两套 verify 实现均无 BUNIT 对拍

- **问题描述**: `p3_output_verify_ex` 注释 :440 写"独立重开读回验证 (dimensions/WCS/**BUNIT**/checksum/mask/uncertainty HDU 面)"，PHASE3_FITS_IMPL §12 T2 写"独立 verify：重开 dims/WCS/**BUNIT**/checksum/mask 一致 → reopen_ok=1"。实测: verify_ex 的对拍键集仅 skeys（:502-504 CTYPE1/2、CUNIT1/2）与 dkeys（:515-519 CRPIX1/2、CRVAL1/2、CD1_1..CD2_2），**零 BUNIT 读回**；流式 `P3FitsVerifyStream` 同样（skeys :929、dkeys :942，无 BUNIT）；`reopen_ok = ok && covok && uncok && wcsok`（:602-604 / :1066-1068）**无 buok 子项**。全文件 BUNIT 仅出现于写侧（:285/:371/:719/:772，grep 实证）。结果: 写侧传入错误 bunit（面亮度量纲错）时，最后一道独立验证门零鉴别力、reopen_ok 照样为 1，而文档/注释两处都说它验过 BUNIT。
- **证据**:
  - 锚实开: `grep -n BUNIT p3_output.cpp` 14 行命中全部落在写侧与注释，:457-619 verify 区间无一命中；skeys/dkeys 锚行逐行核对如上。
  - 同型先例: verify 对 WCS 零鉴别力曾被裁为审计缺陷（AUD-COORD F-05，见 p3_output.cpp:464-466 注释自述），B2-A9 已补 WCS 对拍，BUNIT 面未补——注释却已按"已补"书写。
  - 反方核验（测试面）: eng/tests/unit/p3_output_test.cpp:128/:133 确有 `fits_read_key(... "BUNIT" ...)` 断言，但属 2b 段**写侧键值检查**（FIX-402 二次律），:79-86 的 T2 段只断言 reopen_ok=1——reopen_ok 不携带 BUNIT 信息，T2 声称的"BUNIT 一致"无判据承载；p3_output.h:68-73 的 API 注释只写"读回 header 数字+数据回环"未点名 BUNIT（承诺在 cpp 注释与文档 §12，两处均已点名）。
- **建议改法**: 二选一并同步三处（cpp:440 注释、PHASE3_FITS_IMPL §12 T2、实现）: ① 在 verify_ex / VerifyStream::open 读回 BUNIT（主 HDU 及 VARIANCE/IVAR 扩展）与传入值对拍并入 reopen_ok；② 明确撤销"BUNIT 由 verify 覆盖"的两处声明、把 BUNIT 一致性归到 2b 段写侧检查名下。涉及验证合同口径，上呈后统一。
- **所属面**: ④（文档/注释说有、代码没接）＋①（科学量纲字段的独立验证证据资格）。

### S26-R03 ｜面①（兼③④）｜ `lib/algorithms/drizzle/healpix_drizzle/tests/control_median_mc_test.cpp:15-16` vs `:216-223` —— 头注承诺的"冻结值 vs 经验值"对差断言未实现，且 PASS 文案与 D-08 终裁相悖

- **问题描述**: 头注 :15-16 声明本测试流程"4) 冻结值写入 sampler.cpp kControlCorrDefault；**UPMW-005 断言 |k_corr_frozen − k_corr_empirical| 在容差内**"。实测 main 全程**不读取 kControlCorrDefault、无任何 frozen-对差断言**，唯一切断判据是 `:217` `if (!(k_corr >= 0.98 && k_corr <= 2.0))` 的宽区间合理性门；`:221-222` 判过即打印 **"[PASS] k_corr 科学合理，可用作 sampler 冻结值"**——把单一几何（300″/pixfrac=0.8/20×20）实测值宣称为"可作冻结值"，与 D-08 终裁（分歧台账:245: "1.4 在声明域两端低估 32%/2 倍；已批改表"，域内走 k_gauss×k_geo 逐帧查表、1.4 仅代码默认/域外回退）直接冲突：D-08 已裁定几何专属实测值不得作普适冻结值。
- **证据**:
  - 源码实况: `:90-223` 全函数无 kControlCorrDefault、无 sampler.h 引用、无容差对差；`:214-215` 注释自述门是"科学合理性门…k_corr < 2.0，20×20/512 采样下足迹重叠有限"——门的域是合理性而非标定回归。
  - 反方核验: 全仓 grep `kcorr_lookup_test` 仅命中 reports/ 清单与 AUD-404-退役与死代码清单.csv:98，**源文件不在当前工作树**——冻结表值对差在别处实现的说法不成立；sampler.cpp 冻结值 1.4 与本件经验值之间不存在任何可红的回归锁（引擎漂移到 k=1.9 仍绿、冻结值不动也无红灯）。
  - 与 D-08 一致面（正方）: 本件的 MC 方法（:91 NMC=2000、:122 1.482602218505602、:136 (π/2)σ²/N）与公式口径本身正确——问题在断言缺位与输出文案，不在数值方法。
- **建议改法**: 上呈（断言口径涉 D-08 执行面）: 补 `|kControlCorrDefault − k_corr_empirical|` 对差断言（含容差来源声明），或把头注 :16 的承诺改为现状"合理性区间门"；`:222` PASS 文案按 D-08 改为"实测值仅作标定几何专属记录，非冻结值推荐"。
- **所属面**: ①（声明的判据未接线＝恒宽门）＋③（文案与 D-08 终裁冲突）＋④（头注说有、代码没接）。

### S26-R04 ｜面①②④｜ `lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_science_matrix_test.cpp:11-13` vs `:306-318` —— Gate 门限同文件两说（差 4 个数量级）、逐点门声明未接线（worst_case 死变量）

- **问题描述**: 头注 :11-13 声明 "per drop 记录 … raw absolute/relative error（**不得用 Σoverlap 归一化掩盖**）；**Gate: |computed-ref|/ref < 1e-6 (SIP) / < 1e-10 (纯 TAN)**；**|Σoverlap−computed|/computed < 1e-8**"。实测: `:309` `gate_ref = 1e-6` 对全部 WCS 单档（纯 TAN 档比头注松 4 个数量级）；`:314/:318` 门作用于 `mean_ref`/`mean_ov`（9 个采样点**平均**，:306）而非 per-drop 逐点；`:242` 初始化、`:295` 赋值的 `worst_case` **此后从未被读取**（grep 全文件仅 2 行命中）——头注的逐点 Gate 与最坏点记录双双未接线，平均聚合可让单点 5e-6 的坏例被 8 个零误差点稀释到 5.6e-7 而判绿，正是头注 :11 自己禁止的"掩盖"。
- **证据**:
  - 锚实开: :11-13（头注 Gate）/ :11（禁止掩盖句）/ :242 `int worst_case = -1;` / :295 `if (rel_ref > 1e-9 || rel_ov > 1e-9) worst_case = npx;` / :306-318 门段——四处行号逐行核对；`grep worst_case` 仅 :242/:295 两命中，无消费者。
  - 反方核验: :307-308 有注释解释"门限 1e-6: 生产 drop_area 已尺度感知…误差 ~1e-9/~1e-12"——即实现侧认为 1e-6 足够；但该解释不能同时让头注的 1e-10/1e-8 与"per drop"语义成立，**同文件两说必居其一**；正确口径的权威在 DRIZZLE_GEOMETRY §9 TEST-DRZ-DESIGN-001 冻结容差（本件两侧均未引其锚），须以该冻结面核定后统一。门本身非恒真（mean 也能红），故不定为恒真，但声明的判据未接线使现有绿灯的证据资格低于其自述。
- **建议改法**: 上呈（判据容差属冻结面，不越权定数）: 按 DRIZZLE_GEOMETRY §9 核定门限归属后，统一头注与 :309/:314/:318；把 `worst_case` 接入判据（或删除死变量并把头注 Gate 语义改为 mean），使"per drop 不得掩盖"的承诺可红可绿。
- **所属面**: ②（同文件新旧两说）＋①（判据弱于声明、聚合掩盖单点）＋④（头注声称的逐点门未接线）。

---

## 二、黄（建议改，4 项）

### S26-Y01 ｜面①｜ `lib/algorithms/drizzle/healpix_drizzle/tests/test_nside_pixfrac.cpp:165/:185/:205/:227/:245` —— 期望推导注释用被实现明文禁止的魔数 210960″/nside

- **问题描述**: 五处注释均以 `210960/nside` 推导各档期望 NSIDE（如 :205 `210960/60 = 3516 → 2^12=4096`）。真值正本为 `HEALPIX_SCALE_PER_NSIDE_ARCSEC = √(π/3)·(180/π)·3600 = 211076.28514206142″`（docs/science/algorithms/DRIZZLE_GEOMETRY.md:157/:168），且被测实现 drizzle_engine.cpp:639 注释明文 **"禁止魔数 210960/1186.18"**。测试的期望推导以被禁止魔数为依据，与 A-P3-09 禁抄值体系同族（211034.6 相对差 2e-4 已判"必须清除"，210960 相对差 5.5e-4 更大）。
- **证据**:
  - 实现侧权威: drizzle_engine.cpp:709-710 `std::sqrt(M_PI/3.0)*(180.0/M_PI)*3600.0` 求值 211076.2851，与 DRIZZLE_GEOMETRY:157 逐位一致。
  - 反方核验（不升红的理由）: 逐档复算 210960 与 211076.285 推导的 2 次幂结论**全部同档**（0.1″→2^22、1″→2^18、60″→2^12、0.01″→2^22 钳位、20000″→16 钳位），当前断言（:165-263 各 ASSERT 区间）不因该魔数产生分歧；同目录 drizzle_science_completion_test.cpp:439-440 已用正解 {0.0503245, 4194304}/{12.883074, 16384}（与 A-P3-08 正解名单一致），说明正解在本目录已有先例。
- **建议改法**: 五处注释的推导常数改 211076.28514206142（与 drizzle_engine.cpp:639 禁令和 DRIZZLE_GEOMETRY:157 同步）；顺带核 :7-8 头注"60″ → 在合理范围（不一定触底）"与 :205-208 断言 `nside >= 16` 的口径（见 G 级 A3 条）。
- **所属面**: ①（几何常数与正本/实现禁令不一致）。

### S26-Y02 ｜面①②｜ `lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_freeze_test.cpp:316-346` —— T5 第二段死调用＋返回值忽略＋无 total>0 守卫的退化绿通道

- **问题描述**: T5 reverse 的第二段（:321-322 `CaseResult r; run_case(img, 65536, 0.8, r);`）结果 **r 从未被消费**（死调用，其内部失败也只 printf 不计 g_fail）；紧接着 :329 `engine.drizzleTiled_f64(...)` **返回值未检查**，随后 :346 `CHECK(out_of_range == 0, msg)` 无 `total > 0` 守卫——drizzle 失败或零输出时 total=0、out_of_range=0，该门判绿（0/0 语义下"越界 0/0"）。另 :334 硬编码 `t.parent_ipix << (2 * 9)` 与同文件 :105/:148 统一使用的 `hiss::compute_tile_depth(nside)` 不一致，depth 常量若变动将静默错位。
- **证据**: 锚四处实开核对（:316-346 段全文读过）；:329 行无 `if` 判定、:346 判据仅 out_of_range。反方核验: 同段坐标往返（:330-341）是 WCS 层独立判据仍有效；且 T2/T3/T4 的 rel64 门在 drizzle 整体失败时会红（run_case 默认 rel64=1.0，:83），**全局不恒真**——故退化面限于 T5 覆盖检查单项。
- **建议改法**: 判定 :329 返回值并在失败时判红；`:346` 加 `total > 0` 守卫；删除或消费 :321-322 的 r；:334 改用 compute_tile_depth 与 :105/:148 同源。
- **所属面**: ①（单项判据可退化绿）＋②（死代码/不一致）。

### S26-Y03 ｜面①｜ `candidate_oracle_test.cpp:90-125`、`oracle_independent_test.cpp:146-187`、`drizzle_freeze_test.cpp:150-182` —— 三道 oracle 门缺"真集合非空"守卫（对照 edge_crossing 合格范式）

- **问题描述**: 三门的判据均为"false negative 计数 == 0 即绿"，均无 `truth/oracle 非空` 守卫:
  - candidate_oracle_test: `:90-125` truth 由 `compute_overlap_area_g > 0` 筛出，fn==0 即 g_pass++——若 overlap 计算整体退化为恒 0，truth 全空、fn 恒 0、门恒绿（JSONL 会记 truth=0 但无断言）；
  - oracle_independent_test: `:147-187` 穷举/采样真集同理，`:185-186` `else { g_pass++; }` 无任何命中数下界；
  - drizzle_freeze_test T1: `:175-182` oracle 与 out 同空时 `fh/ff` 全空 → `CHECK(fh.empty() && ff.empty())` 绿，`oracle=%zu` 只进 printf 不进判据；且其 oracle 用 `query_candidate_pixels_fast`（:169）与被测 drizzle 共用候选函数——候选整体坏时 oracle 与输出一起变空。
- **证据**: 同目录合格范式对照——`oracle_edge_crossing_test.cpp:272` `CHECK(n_missed == 0 && n_true > 0, msg)`（n_true 全局非空守卫，:159/:211/:239/:258 累计）证明本套件有此意识但未推及其他三门。反方核验: 正常路径下三门均能红（fast 漏选时 fn>0 判红），非恒真；candidate_oracle 头注 :15 "9003 例"与实际矩阵例数（111×5×4×4=8880 + 高 NSIDE 48 + 极限档 75 = 9003）逐位相符，规模声明可信。
- **建议改法**: 三门各加 `truth/oracle 非空`（或累计命中数 > 0）守卫，参照 edge_crossing:272 范式；freeze T1 另在头注注明其 oracle 与被测共用候选函数、独立性由 candidate_oracle_test 承担（分工已事实存在，补一句声明即可）。
- **所属面**: ①（判据非退化：真值无效应时门应判红）。

### S26-Y04 ｜面②（兼①）｜ `lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_oracle.hpp:11/:129-131/:230-231` —— 公式文字仍是 D_p 分母旧口径，与同文件实现（N_p）及 R-1 订正后正本两说

- **问题描述**: 头注 :11 写核心 oracle 恒等式 "**S_p=F_p/D_p、var_p=Σv_j·w_jp²/D_p²**"；O4 注释 :129-131 推导写 `S_p = Σ x_j w_jp / Σ a_jp`；O7 注释 :230-231 写 `var_p·D_p² = Σ v_j w_jp² ⇒ var_p = σ²/A_drop²`。而同文件 `:68-74` extract_leafs 实现是 `signal = sumFlux/sumNorm`、`variance = sumVarNum/sumNorm²`（**N_p 分母**，与 R-1 订正后正本 UNCERTAINTY_AND_COVARIANCE:12 `var_p = Σ v w²/N_p²`、DRIZZLE.md §5 S_p=F_p/N_p 一致）。O7 的期望式 `σ²/A_drop²` 是 D_p 口径推导，在 w=a/A_drop 约定下与实现的 N_p 口径（期望 σ²/A_pixel²）**差 pf⁴ 因子**——当前唯一调用点 p1drz_tests_core.cpp:384 `make_cfg(NSIDE, 1.0, ...)` 为 **pixfrac=1.0**，两口径同值故判据绿；pf<1 下该期望式与实现不同值，而注释未声明此前提。
- **证据**:
  - 等价参数化排除（反方核验）: DRIZZLE.md:88-90 的"逐位相同"等价前提是 w=a/**A_pixel** 参数化（检查-修复验证.md 红-1 判 §4a 合法等价亦明示该前提）；本文件 :68 明写 w_jp=a_jp/A_drop,j——w=a/A_drop 配 D_p² 与 w=a/A_pixel 配 D_p² 不构成等价，单源推导（O7 :260-261 自己给出 a_jp=A_leaf、D_p=A_leaf）实算: D_p 口径 σ²/A_drop² vs 实现 N_p 口径 σ²/A_pixel²，pf=0.8 时差 0.8⁴=0.4096。
  - 正面范例对照: drizzle_pf_sb_gate.cpp:21-24 同一物理量用 `sumNorm=D_p/pf²`、`sumVarNum=σ²D_p²/(n·pf⁴)` 显式推出发布值 σ²/n，口径自洽——证明 N_p 口径的正确写法在本目录已有。
- **建议改法**: 头注 :11、O4 :129-131、O7 :230-231 的公式文字统一为 N_p 口径（S_p=F_p/N_p、var=Σv w²/N_p²），或在 O7 注释显式声明"期望式仅在 pf=1（当前唯一调用点）下与实现同值，pf<1 需按 sumNorm 重推"；不改任何判据数值。
- **所属面**: ②（同文件公式新旧两说）＋①（期望式的适用域前提未声明）。

---

## 三、绿（可不改，6 项）

### S26-G01 ｜面④｜ `p3_output.h:28-32` uncertainty_source / uncertainty_missing_pixels 为不被本模块消费的死字段；PHASE3_FITS_IMPL.md:47 "provenance 八字段子集"计数过期

- P3Provenance 两字段的长注释（:28-30 "uncertainty 子产品来源…provenance 计数不中断"）暗示由本模块写出，但 `grep "prov->" p3_output.cpp` 仅命中 :288-298/:722-729 共 6 键 + HISTORY，两字段零消费；实际 provenance 载体是 manifest（p3_session.cpp:416-417 自写，eng/tests/unit/p3002_uncertainty_test.cpp:360-371 断言 manifest 面），行为无缺口。结构体已 10 字段而 PHASE3_FITS_IMPL:47 仍写"八字段子集"。
- 建议: h:28-30 注一句"本字段由 session 层落 manifest，不入 FITS 头"；:47 改"字段子集"或更新计数。面④。

### S26-G02 ｜面④｜ `drizzle_science_matrix_test.cpp:19` 声称 ULP"按信号幅值分桶"输出，bucket 计算后从未写入 `ulp_distribution.json`

- :388-405 `bucket` map 聚合后未出现在 :446-454 的 JSON 输出（仅 method/count/p50/p95/max/variants）——死计算＋声称输出缺一维；主判据 p95<10/max<64（:411-413）不受影响。建议: 删死计算或把桶写入 JSON。面④。

### S26-G03 ｜面②｜ `p3_output.cpp:467-468` 注释"1e-12(度/像素) / 1e-15(CD deg/px)"两套容差 vs 实现单档

- verify_ex 对全部 8 个数值键统一 `> 1e-12`（:520-524），无 1e-15 档；CD 实际比注释声明松 1000×。行为是"更松"且 round-trip 位精确本可容纳，注释与实现取齐即可。面②。

### S26-G04 ｜面②｜ `drizzle_pf_sb_gate.cpp:116` 注释"同一 tile 内 4 个叶" vs `:119` `const uint32_t locals[1] = {0};` 单叶

- 注释与代码不一致（1 叶）；该门判据（1)(2)(3) 非退化合格（:270-273 有 `g_pass == 0` 归零守卫；负例经 `ASTROCS_DRZ_SB_FAULT` 注入已接线——astro_sphere_sink.cpp:152-186 实开确认 sink 侧消费该变量，CMakeLists:325-330 注册 WILL_FAIL 负例）。改注释即可。面②。

### S26-G05 ｜面②｜ `candidate_oracle_test.cpp:147` 注释"(12288/49152 像素)"与 nside≤32 穷举档错位

- `oracle_exhaustive` 对 nside≤32 生效（:90-92），npix=12·nside² 实为 3072/12288；注释数字 12288/49152 对应 32/64 档。结论不受影响（穷举范围由 :90 的 `<=32` 决定）。改注释即可。面②。

### S26-G06 ｜面②｜ `drizzle_nonfinite_test.cpp:12-13` 与 `p1drz_oracle.hpp:291/:296` 引 DRIZZLE.md ":96" 行锚漂移、状态叙述过期

- DRIZZLE.md 现 250 行，:96 = Van Oosterom 行；非有限口径已按 rule_id=NAN-SAMPLE-MASK-COVERAGE-NAN 反转并落 **DRIZZLE.md:144**（"样本级掩膜+重归一+覆盖级 NaN+强制计数"，实开核对）——与两测试的断言方向已一致。残留的只是旧行锚 :96 与"文档面订正属 DOC 域（尚未订）"的过期状态句。改锚即可。面②。

---

## 四、域外附带登记（确凿且重大，注明域外）

### S26-O01 ｜面①（域外）｜ `lib/infrastructure/aio/tests/hiss_correctness_test.cpp:501-570` —— 期望值**进入判据计算**的魔数 210960（6 处）

- 与 S26-Y01 同族但更实质: `:502` `nside_min_real = 210960.0 / finest_arcsec`、`:522/:534/:548/:570` `hp_res = 210960.0 / nside` 直接参与期望值计算（test_nside_pixfrac 仅在注释），`:564` "nside >= 2109600" 结论同。真值 211076.285 相对差 5.5e-4，落入 A-P3-09 所指"决策窗口"量级。**域外**（aio 测试目录不在本片责任域），仅登记。
- 关联交叉引用: A-P3-09 点名的 211034.6 五处残留（hp_drizzle_api.h:114、orchestrator.cpp:180-181/198、module_adapters.cpp:7603、drizzle/README.md:93）本轮 grep 复核实证仍在，**已由排查 S15 红-1 报出**，此处不重复计数；210960 在 aio/tests 的 6 处为 S15 未覆盖的新位点。
- 建议: 随 A-P3-09 清除动作一并按 DRIZZLE_GEOMETRY:157 正本换算式重锚（走登记上呈）。面①。

---

## 五、已查无问题面（逐面说明）

### 面① 科学性（常数/公式/单位/量纲/适用域；与 D 系终裁对数；判据非退化）

- **六道专项 oracle 门逐个审**: ① candidate_oracle_test——9003 例规模声明逐位复算相符，fast ⊇ oracle 断言结构正确（缺非空守卫已报 Y03）；② oracle_edge_crossing_test——独立大圆弧交叉/点包含真值**不调用生产 overlap/候选**（:49-101 实现通读），:272 `n_missed==0 && n_true>0` 非退化守卫合格，负例（切线 band 不相交计入 n_tangent、F-POS 判红 :242）能红能绿；③ oracle_independent_test——采样真集独立于生产 overlap（:107-120），无重大量/口径错误（守卫缺位已报 Y03）；④ drizzle_freeze_test——T2/T3/T4 通量闭合 1e-6/1e-5 门、T7 均匀背景 `n>0 && rel_std<1e-3` 守卫、负例经默认值 rel64=1.0 兜底均可红（T5 局部已报 Y02）；⑤ drizzle_science_matrix——Part B leaf 集合逐位一致 + p95<10/max<64 ULP 门非退化（Part A 门限两说已报 R04）；⑥ drizzle_pf_sb_gate——正例 + 负例（注入 legacy 口径必判红）双臂齐备，`g_pass==0 → return 2` 归零守卫合格，头注物理推导（sumNorm=D_p/pf²、sumVarNum=σ²D_p²/(n·pf⁴)、发布 σ²/n）与 DRIZZLE.md §5/R-1 订正后口径逐式复算一致。
- **kcorr_matrix / control_median 与 D-08 对数**: 方法公式（1.482602218505602 有仓内锚 CALIBRATION_ALGORITHMS.md:89/:624、(π/2)σ²/N、MAD-σ̂）核对无误；断言与文案问题已分别报 R01/R03，不重复。
- **test_nside_pixfrac 与 D-01/D-02 对数**: 该件不含外接半径（1.0415/1.25）与 depth 矢高常量（全目录 grep `1.0415|1.1284|4.0067|0.4443|sag0|8.094` 零命中，DRIZZLE.md:98-101 缓冲三层 1.25/3.0 与 D-01 终裁"sup≈1.0415+裕量≥20%"兼容）；D-01/D-02 相关常量在本责任域内无冲突面；hp_res 常数问题已报 Y01。
- **主域节点常量**: drizzle_science_completion_test T13 {0.0503245, 4194304}/{12.883074, 16384} 与 A-P3-08 正解名单（211076.285/2^k）逐值一致；oracle_independent:252 hi_scales {0.0503,…,12.883} 同源 ✓。
- **负例归零专项**: drizzle_acceptance_test 头注自述 `--inject-legacy-pixfrac2` 恒真门自省（:16-18）+ CMakeLists:66-71 WILL_FAIL 注册 + acceptance_drizzle.py:331-341 "该模式必须 exit != 0, 否则说明正例门是恒真门"三重自检链——**本目录最高质量的非退化设计**；p1drz_disp009_gate 双判据 P（正例 <1e-3）+ N（独立几何投影回错误分母必判红，:24-31）缺一不可；p1drz_tests_selfcheck 双阶段（baseline 必 PASS + 9 注入必 FAIL，:7-10）排除恒败/恒败两侧——三件均合格。
- **DRIZZLE.md:96 非有限口径**: 已反转落 :144 与测试断言同向（锚漂移报 G06，无口径冲突）。

### 面② 行文逻辑（论证链/前后矛盾/订正注新旧两说/UNRESOLVED 混入）

- 全责任域 grep "UNRESOLVED" **0 命中**——无未解析登记混入测试断言或 p3_output 正文充当结论。
- p3_output.cpp/.h 头注与实现逐段核（原子写序 flush→fsync→rename :387-423、失败清理 unlink :400-432、R10-C 注释 :388-392）: 论证链完整，无新旧两说（容差注释小漂移报 G03）。
- 六件专项门与 p1drz/disp009/acceptance 头注的"判据—负例—证据"三层自述与实现逐条对照，除 R01/R03/R04/Y02/Y03/Y04 点名处外无断裂。
- 已知历史叙述（drizzle_nonfinite "原断言已作废"、DISP 登记句）均为合法订正导言形态，无"以旧为新"。

### 面③ 跨文档冲突（五方权威链）

- **p3_output ↔ PHASE3_FITS_IMPL**: 逐符号锚抽验 40+ 处（BUNIT 缺省 :284-285、WCS 6 键 :252-270、provenance 6 键 :288-295、DATASUM/CHECKSUM :142-149、rc 语义表 :207/:236-245/:307-312/:446/:475、tmp 形态 :151-157 DISP-P3FITS-002 已登记）与源实况相符；BUNIT verify 面不符已报 R02。行数声明（1082/166 行）实测相符（`wc -l` 1082/166 ✓，S6 亦独立核过）。S6 已报的 p3_wcs.h 166 vs 373 不重复。
- **p3_output ↔ HIPS_STORAGE_FORM_CONTRACT**: N8（Phase3 平面 FITS 不使用 `.hips` 中缀）与实现无冲突——p3_output 输出路径由 session 给定（默认 output_phase3.fits，无 .hips 中缀）；§7 打洞/TRIM 生效面限裸 .hips 目录（Phase3 不涉）；§10.5 storage_form 键 REJECT 属 schema/CLI 面（域外），p3_output 无该键。**两文件对 p3_output 的直接约束全部满足**。
- **D 系终裁对数**: D-01/D-02（外接半径/depth）在本责任域无消费点；D-03 F&H 双锚定与 CMakeLists:10-11（§7.2 式(7) 下方定义句锚）+ disp009 头注（§2 权重式）双锚并存兼容 ✓；D-08 已对数（R01/R03）；D-09/D-10/D-11 本责任域不涉。
- **PASS 表衔接**: 上轮 PASS 的 k_gauss 1.316 表、k_corr 记法、QA_MATRIX 措辞、DRIZZLE.md:108 旧值 1.044 等 30 项逐项确认未重报。

### 面④ 幻觉与锚（文献题录/file:line 锚/文档说有代码没接）

- **file:line 锚**: 本报告全部红黄绿条目的源码锚（约 60 处）逐一实开核对，无一虚锚；PHASE3_FITS_IMPL §3 逐符号锚抽验 40+ 处相符（R02 点名的 verify 段除外——该段文档本身未承诺 BUNIT，承诺在 :300-301 §12 与源码注释，均已在 R02 点名）。
- **"文档说有、代码没接"专项**: ① 声称 JSON 证据不存在（R01）；② verify BUNIT 声称未接（R02）；③ frozen 对差断言未接（R03）；④ 逐点 Gate/worst_case 未接（R04）；⑤ ULP 幅值分桶未接（G02）；⑥ 死字段（G01）——**除上述六项外，其余头注声称的覆盖（A..F/H、T1..T13、O1..O8、FIX-DRZ-A..F、SNR-011/012、DRZ-014/016）逐条在源码中找到对应实现**（批量 grep CHECK/ASSERT 与头注清单比对）。
- **ctest 注册面**: tests/CMakeLists.txt 通读——10 个门注册（acceptance_core/full/negative、freeze、candidate_oracle、science_matrix、science_completion、control_median_mc、pf_sb_gate、pf_sb_gate_legacy_injection）与 DRZ_EXTRA（编入不注册）声明自洽；kcorr/cmedian 曾为"构建孤儿"的历史问题（FIX_LEDGER M4-F-01/M8-F-013、R-2 research）已由 DRZ_EXTRA 收编——注册面无谎报。
- **文献题录**: 本责任域代码内文献引用仅 F&H 2002（CMakeLists:10-11、disp009 头注、acceptance 头注引 PASP 114/§7 定义句）——与 D-03 终裁双锚定口径逐字一致，无 DOI/arXiv 新题录需联网核验；IAU FITS 4.0 §4.4.2.5（p3_output.cpp:50-51）为标准条目。
- **第三方代码**: nanoflann.hpp / cfitsio / nlohmann 仅查对接（p3_output 经 fitsio.h 调 fits_write_chksum/fits_flush_file/fits_write_subset 返回值均检查；无第三方内部审查）✓。

---

## 六、统计

- **红: 4 条**（S26-R01～R04）
- **黄: 4 条**（S26-Y01～Y04）
- **绿: 6 条**（S26-G01～G06）
- **域外附带: 1 条**（S26-O01；另交叉引用 S15 红-1 的 211034.6 残留，不重复计数）
- 责任域文件全覆盖: tests/ 40 个源文件（含 p1drz 11 件）+ CMake 2 件全部打开或结构化扫描；p3_output.cpp（1082 行）/p3_output.h（166 行）全文逐行读毕；PHASE3_FITS_IMPL.md（442 行）、HIPS_STORAGE_FORM_CONTRACT.md（268 行）全文读毕。
