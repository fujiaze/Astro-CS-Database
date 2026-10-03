# 审稿-P1 · ALG-calibration-001（对抗审稿 第 1 遍）

- 片号：`ALG-calibration-001`
- 层：`lib/algorithms/calibration`
- 基线：仓库 `/workspace/Astro CS Database`，`HEAD = 850a9edefd47434b9ab71bc907c3de1e0814b323`
- 口径：一片 = 对该片材料的一次完整重读；所有结论来自本人读完原文 + 5 个子代理的独立复核。
- 纪律遵守：零 git 写（未 add/commit/checkout/reset/stash/`git rm --cached`）；未编译、未跑 ctest/pytest/构建/任何二进制；未改任何仓内文件（唯一写入是本交付件）；未读 `/tmp/acsd_g08/`；中文路径一律 `git -c core.quotepath=false`。

---

## 1. 读完了吗

| 口径 | 数值 |
|---|---|
| 成员份数（权威清单） | 38 |
| 实际读完份数 | **38** |
| 成员总行数（权威清单 `实际行数: 9276`） | 9276 |
| 实际逐行读完行数 | **9276**（本人 `wc -l` 复核 = 9276，与清单逐份一致） |
| **覆盖率** | **38/38 份，9276/9276 行 = 100.0%** |

**未读完的：无。** 全部 38 个成员文件均存在（`wc -l` 全部成功，无 MISSING），且全部由本人用 read 工具从第 1 行读到最后 1 行。

逐份行数（本人实测，`wc -l`，降序）：

```
1503  src/module_entry.cpp                 338  cpp/cosmetic_corrector.cpp
 788  src/cosmetic_corrector.cpp            304  include/acsd/calibration/calibration_covariance.h
 745  src/calibration_covariance.cpp        297  src/master_generator.cpp
 673  tests/p1cal/p1cal_tests_core.cpp      295  tests/p1cal/p1cal_oracle.hpp
 437  src/ac_api.cpp                        257  memory.md
 411  include/astro_calibration.h           198  tests/p1cal/p1cal_fixtures.hpp
 404  tests/test_photometry_apply.cpp       191  src/calibrator.cpp
 366  src/dark_optimizer.cpp                174  integration/acsd_p1_calibration.integration.json
 172  README.md                             166  tests/p1cal/p1cal_tests_selfcheck.cpp
 147  src/photometry_apply.cpp              147  tests/p1cal/bias_influence_harness.cpp
 143  tests/p1cal/p1cal_test_main.hpp       139  tests/p1cal/check_bias_influence.py
 128  CALIBRATION_PROCESS.md                118  tests/p1cal/p1cal_tests_perf.cpp
 115  src/photometry_apply.h                 109  include/acsd/calibration/types.h
  91  CMakeLists.txt                         80  module.yaml
  70  tests/p1cal/CMakeLists.txt             70  build.ps1
  62  cpp/cosmetic_corrector.h               54  CALIBRATION_COVARIANCE.md
  28  batch_config.json                      20  .gitignore
  18  Makefile                                9  tests/p1cal/p1cal_tests_main.cpp
   6  src/module_exports.map                 3  src/acsd_p1_calibration.def
```

---

## 2. 本片判定

### **判定：阻断（BLOCK）**

理由：片内存在**一条可由合法配置值触发、把两个科学算子静默变成恒等/无剔除的通路**，且**全部既有绿灯对该通路零覆盖**；同时存在**两个"用同一个定义式既当被检量又当期望量"的恒真门**（一个在判据代码里，一个在判据测试里）。按 AGENTS.md §8「判据须在正确实现下绿、注入缺陷时红」，本片当前的绿灯对最重的一类缺陷不具备证伪能力。

### 最重 3 条

**【阻断-1】`max_structure_size` / `max_iterations` 无上界 + uint64→int 静默截断 ⇒ 坏点修复静默变恒等、母版静默不剔除离群值**
`src/module_entry.cpp:449-466`（`kv_u64` 只挡 `>UINT64_MAX`）、`:705-708`（`max_structure_size` 只校验 `!=0`）、`:663-666`（`max_iterations` **连 `!=0` 都没查**，错误文案却写 "positive integer required"）、`:968` / `:1036` / `:1046`（`(int)` 强转）。
`src/cosmetic_corrector.cpp:110` `if (labels[i] > 0 && sizes[labels[i]] >= max_size) mask[i] = 0;` —— `max_size<=0` 时第二合取项**对每个候选像素恒真** ⇒ 全部候选被清。
`src/master_generator.cpp:104` `for (int iter = 0; iter < max_iter; ++iter)` —— `max_iter<=0` 时**零次迭代** ⇒ 零离群剔除。
同函数内 `width`/`height` 确有 `u > 100000000u` 上界（`:642`/`:646`），证明上界写法在本文件已存在，唯独这两键漏了。集成合同 `integration/acsd_p1_calibration.integration.json:162` 明写「超界 → ACS_ERR_PARAM + detail=103」—— **合同已要求，代码未做**。

**【阻断-2】判据代码里的恒真门：`validate_covariance_record(make_calibration_covariance_record(r))` 对任意 `r` 恒为 true**
`src/calibration_covariance.cpp:637-641` / `:643-656` / `:663-674`（生产者）vs `:702-738`（校验者）。生产者把 `propagation`、`variance_from`、`input_covariance` 赋成**与校验者要求的字面量完全相同**的值（且与头文件 `calibration_covariance.h:279-283` 的默认成员值相同 ⇒ 字面 no-op）；`combination_coefficients` 来自定长 `double[4]`（`.h:203`）⇒ 永不为空。9 条判据全部被生产者自身覆盖。
更锋利的形态：唯一生产消费方 `lib/algorithms/integration/phase1_product/src/phase1_product.cpp:369-379` 在 validate **之前**按 `status != kOk` 提前返回 ⇒ 送进 validator 的记录永远 `avail=="available"` ⇒ **validator 在生产路径上是死代码**。

**【阻断-3】判据测试里的 NaN 盲门：`std::max(m, NaN)` 恒返回 `m`，六道 oracle 大门在实现吐 NaN 时照样全绿**
`tests/p1cal/p1cal_tests_core.cpp:40-45` `m = std::max(m, std::fabs(got[i] - want[i]));` —— `std::max(a,b)` 是 `(a<b)?b:a`，`m < NaN` 恒 false ⇒ **NaN 被静默丢弃**，`m` 保持 0。受影响的断言：`:160`、`:183`、`:257`、`:289`、`:332`、`:503`。
同文件 `near_equal:35-38` 反而正确处理 NaN，两条路径标准不一致。
具体后果：`:497-505` 的 `sigma_zero_threshold_semantics` 用 `sigma_low=sigma_high=0` 调用，判据 `dev<0||dev>0` 把全部帧剔除 ⇒ 实现与 oracle 都产出**整幅 NaN 母版** ⇒ 因 `std::max` 吞 NaN，断言以 `m=0` 通过。**该测试恰恰在实现产出纯 NaN 时转绿。**

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

| # | 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `src/module_entry.cpp` | 全 1503 行：JSON 微解析器、base64、cfg 校验、exec、vtable 九操作 | 阻断-1 截断链；`:917-920` `if (mkv[bi].kind != 's' \|\| !b64) continue;` 把 **类型错也当成"键缺席"**（见 F-2）；`:1226` `work_units = c.frames` 而 `c.frames` 在 `exec_run` 中**从未被使用**（见 F-1）；`:949-1055` 租约在异常路径泄漏（见 F-6）；`:1235` node_id 静默截断到 96 字符 | 阻断 |
| 2 | `src/cosmetic_corrector.cpp` | 全 788 行：坏点检测/过滤/插值 + 坏列全族 | `:110` 候选集上的恒真清除门（阻断-1）；`:574-584` `apply_wide_budget` 的 `frac_max` 越大反而越抑制（见 F-7）；`:334-336` 空输入时 `*out_status` 被写成 `OK` 后直接 return；`:162-226` `interpolate_pixels` 全坏邻域时保持原值但仍计入 `n_hot` | 阻断 |
| 3 | `src/calibration_covariance.cpp` | 全 745 行：`calibrate_pixel` + 共享门 + 记录组装/校验 | 阻断-2 恒真门；`:533-534` 尺寸不匹配的 `-1.0` 哨兵被静默折成 `0.0`；`:598-600` `naive==0` ⇒ `ratio=+inf` ⇒ 门放行（见 F-8）；`:146` 未声明 quantum 时默认 1 ADU；`:26-30` 裸 `getenv` 故障注入无编译期护栏（见 F-9）；`MasterIdentity` 无 `has_v_single_frame`，"未知"与"完美母版"不可区分（见 F-10） | 阻断 |
| 4 | `tests/p1cal/p1cal_tests_core.cpp` | 全 673 行：units/properties/negative/cosmetic 四组 | 阻断-3 NaN 盲门；`:675` 见 F-11；`:96-101` `bias_participation_gate` 被兄弟断言完全蕴含（恒真）；`:483-486` `nan_flat_reproducible` 恒真；`:650` `h2 == want_hot_count+1` 用的是**另一个 seed 的 fixture**；`:315` 测试名 "NaN 跳过" 与 `:326-327` 注释 "NaN 不跳过" 自相矛盾 | 阻断 |
| 5 | `src/ac_api.cpp` | 全 437 行：18 个 `extern "C"` 包装 | `:101-125` `ac_generate_master_bias` 与 `ac_generate_master_dark` **函数体逐字相同**；`:138` 把内层任意非零一律映成 `AC_ERR_PARAM`；`:194-260` `_f64` master 族内部转 float32（头文件已声明）；`:361` `ac_detect_bad_columns_from_master` 额外要求 `col_mask` 非空（与兄弟入口语义不一致） | 须修 |
| 6 | `include/astro_calibration.h` | 全 411 行：18 个 `AC_API` 声明 + 坏列族文档 + `optimize_dark_k` | `:9` 「12 个 legacy 符号」**实际 18 个**（见 F-4）；`:405-407` 公开声明 `ac::optimize_dark_k`，而该实现被**零个 CMake 目标编译**（见 F-3）；`:142/289/290` 三处引用行号漂移 | 须修 |
| 7 | `tests/test_photometry_apply.cpp` | 全 404 行：photometry 单测 + Writer 元数据 | `:236-248` 大动态范围门容差 `1e-5f`，float 路径给**同样结果** ⇒ 该门无法区分它自称要守的 double 精度；`:19-33` 编译说明引用已消失的 `../../astro_image_io/include`；`:370` 硬编码 `rc == -2` 耦合到无关 IO 库内部错误码；`:279/309/336/368` 在 CWD 写 `.hiss` 文件 | 须修 |
| 8 | `src/dark_optimizer.cpp` | 全 366 行：鲁棒回归 + 10 处回退 | **零 CMake 目标编译**（本人核验：根 `CMakeLists.txt:668-677` 无、模块 `CMakeLists.txt:20-28` 无；仅 `build.ps1:29` 编译，而该脚本因 `:41` 悬空 include 已不可用）；`:351` `k_est > 10.0f` 硬编码无推导；`:147-149` `GRID=8/PER_CELL=1000/MAX_TOTAL=50000` 硬编码；`:213/240/297/321` `100/50` 魔数；`:187` `dark[idx]-bias[idx]` 在 **float32** 做相减（暗项灾难性抵消） | 须修 |
| 9 | `cpp/cosmetic_corrector.cpp` | 全 338 行：`cc_*` 独立世代实现 | **不在 CMake 产品面**，但 `Makefile:8/14-15` 仍把它编成导出默认可见 `cc_*` 的 DLL（两位子代理各执一词，合并结论见 F-5）；`:100` 同样的 `>= max_structure_size` 恒真清除门；`:229-237` MAD==0 时静默回退到**标准差**（与 `src/` 的 MAD-only 定义不同，且头文件未记载） | 须修 |
| 10 | `include/acsd/calibration/calibration_covariance.h` | 全 304 行 | `:279-284` 字段默认成员值 = validator 要求的字面量（阻断-2 的根因之一）；`:100` `v_single_frame = 0.0` 无 `has_` 标志（F-10）；`:203` `double jacobian[4]` 定长 ⇒ 判据 8 不可达；`:257-260` 注释只提醒"该传什么"，未说"传错会怎样" | 须修 |
| 11 | `src/master_generator.cpp` | 全 297 行：`generate_master` + `generate_master_flat` | `:104` 零迭代（阻断-1）；`:78-81` 无效参数只 `ac_log` 后 return（void，**C ABI 无从报错**），与 `generate_master_flat` 的 `int` 返回不对称；`:231` / `:273` `if (frame_med == 0.0f) frame_med = 1.0f;` —— 与 `:234`「median<0 拒绝」自相矛盾的唯一漏网值；`:243` / `:285` 硬编码 `0.1f` flat 地板（第二处，与 `calibration_covariance.cpp:64` 同值但无共享来源）；`:38-45` 库代码直写 stderr 绕过统一 I/O | 须修 |
| 12 | `tests/p1cal/p1cal_oracle.hpp` | 全 295 行：9 个 oracle | `:205-241` `filter_structure_oracle` 与 `cosmetic_corrector.cpp:62-114` **同一算法**，`std::deque` vs `std::queue`（后者默认容器就是 deque）⇒ 文件头 `:11-12` 宣称的「非被测 std::queue 实现路径」是空的；`:244-265` / `:268-291` 与 `interpolate_pixels` 逐行同构；`:145` 注释「与实现一致: frame_med<=0 拒绝」**在 `==0` 档是错的**（见 F-12）；`:61-64` `actual_k_oracle` 是恒等桩 | 须修 |
| 13 | `memory.md` | 全 257 行 | `:11` 又一处「12 导出符号」；`:16` 行号漂移（称 `calibrator.cpp:97-99`，实为 `:122`）；`:18` 「生产调用方仅 ac_calibrate_frame」**与实测冲突**（见 F-13）；`:100-101` 指向不存在的 `testdata/Galaxy_Center_T4 全链路测试数据/`；`:35` 起已正确标注 `[ARCHIVED/NON_NORMATIVE]` | 须修 |
| 14 | `tests/p1cal/p1cal_fixtures.hpp` | 全 198 行：FIX-CAL-A..F | `:129` 注释「构造保证每帧中位数精确等于目标值」**为假**（W=H=12 ⇒ NPIX=144 ⇒ 各值 48 个，中位槽落在 `base+100` ⇒ 实际中位数 200/300/500 而非 100/200/400）；`:192-194` `fix_cal_f_buffer` 全仓零调用（死代码） | 建议 |
| 15 | `src/calibrator.cpp` | 全 191 行：`calibrate` + `calibrate_d` | `:122` `float k = k_init;` ⇒ **K 恒为入参，从不搜索**；`:14-16` 头注释仍宣称「黄金分割搜索最优 K…迭代>30」，`:103-104` 自身又写「不使用优化搜索」；`:30` 「多线程固定 16 线程」与实际 `#pragma omp parallel` 不符；`:71` `compute_mad` / `:85` `normalize_flat` 均无调用方；`:129/139/173/183` 四处硬编码 `0.1` | 须修 |
| 16 | `integration/acsd_p1_calibration.integration.json` | 全 174 行 | `:127` `work_units = config.frames` 被称作「数据事实」，而该字段对执行零影响（F-1）；`:64/78/92` 合同只允许「键缺席/JSON null」走 NULL 语义，代码把**类型错也归入该路**（F-2）；`:139` `call_count 每节点必须=1` 与 `:148` `execute 必须 2 遍（两阶段 strbuf）` 相互矛盾（F-14）；`:24` 第 4 处「12 个 legacy 符号」；`:162` 「超界→103」被代码违反 | 阻断 |
| 17 | `README.md` | 全 172 行 | `:6/22/170` 三处断言 `acsd_module_query_v1`「函数体零调用 ⇒ NOT_IMPLEMENTED」，而仓内 `eng/tests/unit/CMakeLists.txt:1191-1194` 明确 `dlopen` 该 DLL；`:53/80` 写 `actual_k`「标准分支=1.0」、回退「K=1.0」，**与 `calibrator.cpp:122` 及 `p1cal_tests_core.cpp:538-553` 相反**；`:69` 「14 AC_API 符号」+ 路径 `lib/include/astro_calibration.h` 不存在；`:97-98` 「其余导出当前无生产调用方」**与实测冲突**；`:132-133` 宣称 oracle 是 NumPy/scipy，落地是 C++；`:137` 宣称 ≥1.60 加速比，落地 `>= 0.4`；`:147` 源文件清单漏 `photometry_apply.cpp`；`:26` 行号 `321-333` vs 实测 `668-677` | 须修 |
| 18 | `tests/p1cal/p1cal_tests_selfcheck.cpp` | 全 166 行：注入必败自检 | `:100-107` 实际只注入 7 个名、全在 `units` 组，`child_argv` 恒为 `{"...","units"}` ⇒ **properties/negative/cosmetic 三组零注入证据**，三组若恒红 selfcheck 仍绿；`:8-10` 声称对齐的 7 个注入名一个都没真注入；`:95-98` `P1CAL_SELFCHECK_FAULT` 指向非 units 组名字必然假红；`:37-79` guard-path 元测试设计正确（值得保留） | 须修 |
| 19 | `src/photometry_apply.cpp` | 全 147 行 | `:60-61/71-72/131-132/142-143` **成功路径也无条件写 4 行 stderr**（绕过统一 I/O）；`:90-93` `nterm==0` 早退发生在全部参数校验**之前**；错误码是自造的 `-1..-6`，与模块 `ACSD_CAL_ECODE_*`（100+）、与 `ac_api.cpp` 的 `AC_ERR_*`、与 `calibrator.cpp` 的 void **四套并存**；`:68` 溢出到 float 得 `+Inf` 不检查 | 须修 |
| 20 | `tests/p1cal/bias_influence_harness.cpp` | 全 147 行 | **全片独立性最强的一份**：期望值全是手算字面量（`:73/78/91/95/100/108-109/130-131/141-142`），fp32 下可精确表示 ⇒ `==` 是合法强判据。`:71` 与 `:121` 是同一次调用，T1 与 T7a 重复 | 建议（正面） |
| 21 | `tests/p1cal/p1cal_test_main.hpp` | 全 143 行 | `:58-80` `fault_name_usable` 是运行期判定，正确修掉了「字面量地址恒真」的老问题（值得保留）；`:11` 示例的 fault 名与组名**双双错**（真名 `darkopt_actual_k_identity`，真组 `units`） | 建议 |
| 22 | `tests/p1cal/check_bias_influence.py` | 全 139 行 | **变异门设计正确**：`:104-109` 变异命中数 `hits==0` 判**红**而非跳过，堵死"改源码改到门失效"的自愈路径；`:81/99-111/126-127` 只写仓外 `mkdtemp`，`:130-132` `--json-out` ctest 未传 ⇒ **不写仓内任何路径**。但 `:25-26` 固定编 4 个源、**跳过 `module_entry.cpp`** ⇒ ABI 层零变异覆盖；`:30` 注释称 `astro_calibration.h:172`，实际在 `:396`（偏 224 行）；`:60` 无 `-fopenmp` ⇒ 该门跑的是串行构建 | 建议 |
| 23 | `CALIBRATION_PROCESS.md` | 全 128 行 | **整篇描述的是已不存在的算法**：`:74` 「局部统计检测（5×5 中值 + 残差 MAD）」vs 现实现全局 median+1.4826·MAD（`cosmetic_corrector.cpp:119-156`）；`:77` 「孤立性检查 max_neighbor_candidates」**该功能全仓不存在**；`:84` 「双线性插值」vs 实际 4 方向 IDW；`:56` 「黄金分割搜索最优 K」已删；`:62-63` 主帧自动匹配无 C++ 实现；`:43` 公式漏 bias。`README.md:153` 称其为「历史流程参考」，但正文无退役标注 | 须修 |
| 24 | `tests/p1cal/p1cal_tests_perf.cpp` | 全 118 行 | `:67-71` `memory_upper_bound` **恒真门**：量的��测试自己 fixture 的 `vector::capacity()`（`fixtures.hpp:106`），与被测实现零关系，右界还给了 1.5× 余量；`:4` 宣称 ≥1.60 加速比、`:98` 落地 `>=0.4`；`:97-98` 无 OpenMP 构建下 `ac_set_num_threads` 是 no-op ⇒ ratio≈1 双门必过 | 须修 |
| 25 | `src/photometry_apply.h` | 全 115 行 | `:71-73` `photo_spatial_nterm`：`order<=0 → 0`（负阶静默当"无空间增益"）、**`order>=3 → 5`**（任意 ≥3 静默按二阶多项式执行，无错无告警）；`:90-91` 除以 `x_scale`/`y_scale`，其合法性只在 `.cpp:118-123` 校验（头不声明前置条件）；`:54-55` 宣称 order=0 逐位一致由 ctest S4 锁定 | 须修 |
| 26 | `include/acsd/calibration/types.h` | 全 109 行 | **全部 67 个宏的引用点只在本文件内**（本人 grep：`lib/` 内除本文件外，零处使用 `ACSD_CAL_CFG_KEY_*` / `ACSD_CAL_OP_*` / `ACSD_CAL_PLAN_KEY_*` / `ACSD_CAL_ECODE_*`）；实现改用 `module_entry.cpp:387-403` 的字面量副本 ⇒ 「冻结词表」有两份事实源；`:88` `ACSD_CAL_PLAN_KEY_MAX_WORKERS` 与 `:101` `ACSD_CAL_ECODE_PLAN_FAIL` 声明但从不产出/从不使用 | 须修 |
| 27 | `CMakeLists.txt`（模块） | 全 91 行 | `:8` 「legacy 12 个 AC_API 符号」失准；`:19` 「与根 CMakeLists.txt acsd_calibration :333 逐文件一致」行号漂移（实测根在 `:668-677`）且 `:20-25` 缺 `photometry_apply.cpp`，与根静态库**源集不同** ⇒ 同一模块两条构建面编译不同的文件 | 须修 |
| 28 | `module.yaml` | 全 80 行 | `:62-74` `source_symbols` 列 12 个，**遗漏全部 6 个坏列符号，含唯一有生产调用者的 `ac_correct_columns_ex2`**；`:8` 第 5 处「12 个」；`:24` 「10 科学 op 与 legacy AC_API 科学导出 1:1」不成立（16 中取 10） | 须修 |
| 29 | `build.ps1` | 全 70 行 | `:41` `-I../astro_image_io/include` **目录已不存在**（IO 现为 `lib/infrastructure/aio`）⇒ 本脚本必然编译失败；而 `dark_optimizer.cpp:33` 依赖该 include 路径下的 `hiss_format.h` ⇒ **`optimize_dark_k` 被唯一一条含它的构建路径排除后，零个可用构建路径编译它**；`:38` 仍带 `-march=native`，与 `Makefile:1-3` 的 ISA-001 声明冲突 | 须修 |
| 30 | `tests/p1cal/CMakeLists.txt` | 全 70 行 | 注册 7 个 ctest，零 `#ifdef`、零 `if(EXISTS)`；由 `eng/tests/unit/CMakeLists.txt:552` **无条件**引入（本人已核验——初查只搜根 `CMakeLists.txt` 得出"未注册"的假设**不成立**，已推翻）；`:67` 只对 Python3 做 REQUIRED，未检查 `g++`，而 `:68-70` 的门在 ctest 期现场调 `g++` | 建议 |
| 31 | `cpp/cosmetic_corrector.h` | 全 62 行 | `:32` 「max_structure_size：**大于**此值被过滤」vs 实现 `:100` 用 `>=`（off-by-one，且与 `astro_calibration.h:120` 的「>=」自相矛盾）；`:31` 只写 MAD 阈值，未记载 `:229-237` 的标准差回退 | 须修 |
| 32 | `CALIBRATION_COVARIANCE.md` | 全 54 行 | `:32` fail-closed 表「共享项存在但 joint/naive ≤ 1 → 红」**在 `naive=0` 时被 `ratio=+inf` 击穿**（表欠定）；`:48` 证据 `run/v6/IMPL-P1-CAL-001/logs/` 落在被 `.gitignore` 排除的 `run/*` 下 ⇒ **从 clone 不可复现** | 须修 |
| 33 | `batch_config.json` | 全 28 行 | `:2-8` `testdata/Galaxy_Center_T4 全链路测试数据/` **不存在**（实测该目录实为 `testdata/Galaxy_Center_T4/`，只含 `素材信息.txt` 与 `lights/`）⇒ 三个路径全悬空；`:9-21` 引用的 9 个母版文件均不存在；整体与模块 15 键词表不兼容（无 `op`/`width`/`height`/`max_iterations`）⇒ 属 legacy 管线配置，无迁移标注 | 须修 |
| 34 | `.gitignore` | 全 20 行 | 未覆盖测试在 CWD 生成的 `*.hiss` / `*.hiss.partial`（`test_photometry_apply.cpp:279/309/336/368`）；未覆盖 `Makefile` 的 `clean` 在 Linux 下经 `2>nul` 产生的 `nul` 文件 | 建议 |
| 35 | `Makefile` | 全 18 行 | `:1-3` 声称「`-march=native` 已移除」——本文件确实没有，但 `build.ps1:38` 仍有 ⇒ 两条构建路径 ISA 姿态不一致；`:12-15` 仍产出导出默认可见 `cc_*` 的 `cosmetic_corrector.dll`，与 `docs/detail/registry/acsd.phase1.calibration.md` 登记的「已退役、不在任何 CMake 目标内」并存；`:17-18` `clean` 用 Windows `del /Q ... 2>nul` ⇒ 在 Linux 下 `make clean` 失败并留下 `nul` 文件 | 须修 |
| 36 | `tests/p1cal/p1cal_tests_main.cpp` | 全 9 行 | 单执行器薄封装，无问题 | 通过 |
| 37 | `src/module_exports.map` | 全 6 行 | 只导出 `acsd_module_query_v1`，与 `.def` 一致 ✓ | 通过 |
| 38 | `src/acsd_p1_calibration.def` | 全 3 行 | 只导出 `acsd_module_query_v1`，与 `.map` 一致 ✓ | 通过 |

---

## 4. 发现清单

### 4.1 阻断（BLOCK，5 条）

**B1｜`max_structure_size` / `max_iterations` 无上界 + 静默窄化 ⇒ 坏点修复变恒等、母版不剔离群**
`module_entry.cpp:449-466`（`kv_u64` 上界 = `UINT64_MAX`）、`:705-708`（只查 `!=0`）、`:663-666`（**连 `!=0` 都没查**）、`:968`/`:1036`/`:1046`（`(int)` 强转）
→ `cosmetic_corrector.cpp:110`（候选集上 `sizes>=max_size` 恒真 ⇒ 全清）→ `correct_frame` 逐位恒等，`hot=cold=0`
→ `master_generator.cpp:104`（零次 sigma-clip）
违反自身集成合同 `acsd_p1_calibration.integration.json:162`「超界 → detail=103」。
**测试覆盖：零。** 加上下界或删掉上下界，仓内没有任何一个测试会变红（子代理实测 5 处用到该两键的用例全用合法小值）。

**B2｜`validate_covariance_record ∘ make_calibration_covariance_record` 恒真**
`calibration_covariance.cpp:637-641/643-656/663-674` vs `:702-738`；根因之一是头文件 `calibration_covariance.h:279-284` 的默认成员值就等于判据要求的字面量。
生产面更锋利：`phase1_product.cpp:369-379` 在 validate 前提前返回 ⇒ **validator 在生产路径是死代码**。
（子代理纠正了我一处过强表述：「所有正向测试都是同义反复」**被推翻**——`eng/tests/unit/p1_cal/cal_covariance_test.cpp:535-560` 有 7 条真负例，`lib/algorithms/coverage/tests/retired_object_reject_negative_test.cpp:93-115` 更是完全手写、与 make 无关。准确表述是"**库自身的生产者无法被库自身的校验者证伪**"。）

**B3｜判据测试 NaN 盲门：`std::max(m, NaN)` 恒返回 `m`**
`p1cal_tests_core.cpp:40-45` ⇒ `:160/:183/:257/:289/:332/:503` 六门在实现吐 NaN 时全绿。
最刺眼的一处：`:497-505` 的 `sigma_zero_threshold_semantics` 传入 `sigma_low=sigma_high=0`，判据把全部帧剔除 ⇒ 实现与 oracle 都产出**整幅 NaN**，该门以 `m=0` 通过。
（我先独立发现，第 5 个子代理独立复核后给出同一结论并补全受影响清单——**双向印证**。）

**B4｜`module_entry.cpp:917-920` 把 manifest 平面"类型错"静默归入"键缺席"**
```c
if (mkv[bi].kind != 's' || !b64) continue;   /* null → 缺席 */
```
`kind != 's'` 同时吞掉 number / bool / array / object。后果：`{"master_flat": 12345}` 与 `{"master_flat": null}` 走**完全相同**的 legacy NULL 恒等路径，校准在缺 flat 的情况下静默产出错值并返回 `ACS_OK`，产物 manifest 无任何字段可区分。
违反 `integration/...json:64/78/92`（合同只允许「键缺席/JSON null」）。
**测试覆盖：零。**

**B5｜`work_units` 是"配置预测"却被合同称作"数据事实"**
`integration/...json:127` + `module_entry.cpp:1226` 都取 `c.frames`；但 `exec_run` 的帧数来自 manifest（`:734-758` `decode_stack` → `:896`），`c.frames` **在 `exec_run` 中一次都没出现**。
⇒ plan 报的并行工作量与 execute 实际处理量无关，而 `types.h:84` 与合同都把它标为 work-units 事实，调度器据此配资源。配置写 `frames:1` 而 manifest 给 64 帧，全链路绿灯。
**测试覆盖：零。**

### 4.2 须修（MUST-FIX，13 条）

| # | 问题 | 证据 |
|---|---|---|
| M1 | `ACSD_V6_CAL_FAULT` 8 个注入点无任何编译期护栏，`add_pedestal` 给**每个**像素 +100 ADU 且连带污染 `jac[3]`，无任何痕迹 | `calibration_covariance.cpp:26-30`（裸 getenv）、`:301-303`、`:338-343`、`:416`；编入 `phase1_product`（该 target 无 `target_compile_definitions`）与 `phase2_integrate`（`:82-83` 只有 3 个无关宏） |
| M2 | 「12 个 legacy 符号」在 **6 处**失准（实际 18） | `astro_calibration.h:9`、`CMakeLists.txt:8`、`module.yaml:8`、`integration/...json:24`、`memory.md:11`、`README.md:69`（后者还说 14） |
| M3 | `module.yaml:62-74` `source_symbols` 漏掉全部 6 个坏列符号，**含唯一有生产调用者的 `ac_correct_columns_ex2`**（`module_adapters.cpp:3027`，`col_enabled` 默认 true） | 子代理 A(1) 两次独立复核一致 |
| M4 | `types.h` 的"冻结词表"零使用，实现用字面量副本；`ACSD_CAL_PLAN_KEY_MAX_WORKERS` / `ACSD_CAL_ECODE_PLAN_FAIL` 声明即死 | `types.h` 全 67 宏的引用点只在自身；`module_entry.cpp:387-403` |
| M5 | README 三处「`acsd_module_query_v1` 函数体零调用 ⇒ NOT_IMPLEMENTED」与仓内 `dlopen` 证据冲突；`:53/80` 的 `actual_k`/`K=1.0` 描述**与代码相反**；`:97-98` 「其余导出无生产调用方」与 M3 相反 | `README.md:6/22/53/80/97-98/170` vs `calibrator.cpp:122`、`p1cal_tests_core.cpp:538-553`、`eng/tests/unit/CMakeLists.txt:1191-1194` |
| M6 | `CALIBRATION_PROCESS.md` 整篇描述已不存在的算法，且有一处（孤立性检查）在全仓**无任何实现** | `:56/74/77/84` vs `calibrator.cpp:103-104`、`cosmetic_corrector.cpp:119-156` |
| M7 | `build.ps1:41` 指向已删除的 `../astro_image_io/include` ⇒ 脚本不可用 ⇒ **`optimize_dark_k` 被零个可用构建路径编译**；`:38` `-march=native` 与 `Makefile:1-3` 的 ISA-001 声明冲突 | 本人 `ls` 实测目录不存在；`dark_optimizer.cpp:33` 依赖该路径 |
| M8 | `batch_config.json:2-8` 三个路径全悬空（实测目录名实为 `testdata/Galaxy_Center_T4/`）；9 个母版文件均不存在；与 15 键词表不兼容 | 本人 `ls` 实测 |
| M9 | 注释与文档引用漂移：`cosmetic_corrector.cpp:549` 称 `module_adapters.cpp:3002` 调 ex2（实为 `:3027`，`:3002` 是 `ac_correct_frame`，**指错函数**）；`:550`「会话侧同」被正本 `COSMETIC_ALGORITHMS.md:339-340` 证伪；`astro_calibration.h:142/289/290` 引用 off-by-one / 文件归属错；`check_bias_influence.py:30` 偏 224 行；`p1cal_test_main.hpp:11` fault 名与组名双双错；`p1cal_tests_selfcheck.cpp:8-10` 声称注入的 7 个名一个都没真注入 | 子代理逐条核验 |
| M10 | 证据文件全部落在 `.gitignore` 排除的 `run/*` 下（`.gitignore:17`），从 clone 不可复现：`run/LINDEF-IMPL-01/`、`run/LINDEF-CLOSE-01/`、`run/v6/IMPL-P1-CAL-001/logs/`。本人实测 `git ls-files run/` 仅 2 个文件 | `cosmetic_corrector.cpp:275/323`、`astro_calibration.h:231`、`CALIBRATION_COVARIANCE.md:48` |
| M11 | `photo_spatial_nterm`：`order>=3` 静默按二阶多项式执行（`>=3 不启用`只写在注释里），`order<=0` 静默当"无空间增益" | `photometry_apply.h:71-73` |
| M12 | 失败语义四套并存：模块 ECODE(100+，`types.h:96-107`) / `AC_ERR_*`(-1..-3) / photometry 自造 `-1..-6` / `calibrate` 返回 void 且无效路径只 `ac_log` | `master_generator.cpp:78-81`、`ac_api.cpp:138`、`photometry_apply.cpp:36-56` |
| M13 | 恒真门 4 条 + 容差宽于冻结值 2~100 倍 + 冻结性能门(≥1.60)落地为 ≥0.4 + 内存门量的是自己的 fixture | `p1cal_tests_core.cpp:96-101/483-486`、`p1cal_tests_perf.cpp:67-71/98`、`p1cal_oracle.hpp:8`、`README.md:137/141-142` |

### 4.3 建议（ADVISORY，8 条）

- S1 `calibrator.cpp:14-16` 头注释仍宣称「黄金分割搜索最优 K…迭代>30」，`:30` 宣称「多线程固定 16 线程」，均与 `:103-104`/实际 pragma 不符。
- S2 `p1cal_fixtures.hpp:129` 「构造保证每帧中位数精确等于目标值」为假（W=H=12 ⇒ 实际中位数 200/300/500）；`fix_cal_f_buffer:192-194` 零调用。
- S3 `p1cal_oracle.hpp:11-12` 宣称的 oracle 独立性（deque vs queue）不成立；`:61-64` `actual_k_oracle` 是恒等桩；9 个 oracle 中 8 个是"同定义、异数值域/异容器"重写 ⇒ **能抓实现 bug、抓不到定义错误**。
- S4 `p1cal_oracle.hpp:145` 注释「与实现一致: frame_med<=0/NaN 拒绝」**在 `==0` 档是错的**（实现 `master_generator.cpp:231` 置 1.0 继续）；两侧都无该输入的测试。
- S5 selfcheck 只覆盖 `units` 组 7 名，`properties`/`negative`/`cosmetic` 三组零注入证据；`P1CAL_SELFCHECK_FAULT` 指向非 units 名必然假红。
- S6 库代码在成功路径无条件写 stderr（`master_generator.cpp:38-45`、`photometry_apply.cpp:60-61/71-72/131-132/142-143`），绕过统一 I/O。
- S7 `cpp/cosmetic_corrector.h:32` 「**大于**此值」vs 实现 `:100` 用 `>=`；`:31` 未记载 MAD==0 时回退标准差。`Makefile:12-15` 仍产出导出默认可见 `cc_*` 的 DLL，与仓内「已退役」登记并存；`:17-18` `clean` 用 Windows `del /Q ... 2>nul`，Linux 下必失败并留 `nul` 文件。
- S8 `test_large_dynamic_range`（`test_photometry_apply.cpp:236-248`）容差 `1e-5f`，float 路径给同样结果 ⇒ 无法区分它自称要守的 double 精度；`:19-33` 编译说明引用已消失路径；`:370` 硬编码 `rc==-2` 耦合无关 IO 库内部码。

### 4.4 已排除的怀疑项（如实记录，避免下轮重复）

- **不成立**：`lib/session/`、`lib/scheduler/`、`lib/orchestrator/`、`p1_op_cosmetic*` 这四条我最初假想的路径在本仓**不存在**（真实位置：`lib/phase1_session/`、`lib/infrastructure/scheduler/`、`module_adapters.cpp:2821` 的函数体）。
- **不成立（我自己推翻）**：「p1cal 测试套件未注册」——只搜根 `CMakeLists.txt` 得出，实际由 `eng/tests/unit/CMakeLists.txt:552` 无条件 `add_subdirectory` 引入，7 个 ctest 全部在册。
- **不成立（子代理推翻）**：「整个坏列子系统在生产 ABI 上不可达」——`ac_correct_columns_ex2` 在 `module_adapters.cpp:3027` 有真实在役调用（`col_enabled` 默认 `true`）。
- **不成立（子代理推翻）**：「`cpp/` 是死代码」——准确表述是**不在 CMake 产品面，但 `Makefile:8/14-15` 仍有一条活的构建配方**（两位复核者各执一词，合并为此）。
- **不成立（子代理推翻）**：「`validate_covariance_record` 的所有正向测试都是同义反复」——存在 7 条真负例与一份完全手写的记录测试。
- **机制被更正**：负 `hot_sigma` 到不了检测判据（`cosmetic_corrector.cpp:243` 短路）⇒ 后果是**静默恒等**而非过检测；负 `max_iterations` 是**零次迭代**而非死循环。
- **未发现**：任何「写文件→读回自己写的文件」型自愈锚（`tests/p1cal/` 无 golden 产物，`check_bias_influence.py` 只写仓外 `mkdtemp`）；任何结构性**恒红**断言；任何「先筛子集再取极值掩盖最差值」型掩护（全部极值都是全像素遍历）。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| E1 | config `max_structure_size = 4294967296`（2³²）→ `(int)` = 0 → `cosmetic_corrector.cpp:110` 候选集恒真 | 「配置层 `u != 0` 校验（`module_entry.cpp:705`）+ 既有测试已挡住 ≤0」 | **推翻成功**。`cfg_fill:705` 明文把 `0` 判为非法（"positive required"），截断是**唯一**把这个非法值偷运进来的通道；产物与「真的没有坏点」逐位相同，manifest 只写 `hot:0,cold:0`（`:823-828`）不可区分。子代理独立复现并补证：同仓另两条宿主路径（`module_adapters.cpp:1410-1417` 的 `p1_int`、`p1_session.cpp:103-114` 的 `cosmetic_int`）**都有区间校验并回退默认值**，只有 DSH 适配层缺这道闸 ⇒ 定性为**新适配层的回归**，非全仓系统性缺陷 |
| E2 | config `max_iterations = 4294967296` → `(int)` = 0 → `master_generator.cpp:104` 零次迭代 | 「`max_iterations` 至少会被 `cfg_fill` 拦住」 | **推翻成功，且比预期更严重**：`cfg_fill:663` **连 `u == 0` 都不查** ⇒ `max_iterations: 0` **无需任何大整数**即可达到同样效果；错误文案（`:665`）却写 "positive integer required"。母版在零剔除下产出，无错、无日志进 manifest |
| E3 | `c={1.0}`、`d={0.0}`、`alpha={1.0}`、`kCommonMasterId`、`master_variance=1.0` 送 `evaluate_shared_gate` | 「ratio>1+1e-9 是 fail-closed 门」 | **推翻成功**。逐行：`naive=0.0`（`:467`）→ `naive_variance=0.0`（`:534`）→ `shared=1.0`（`:565`）→ `joint=1.0` → `naive>0` 假 ⇒ **`ratio=+inf`**（`:598-600`）→ `!(inf>1+1e-9)` 为假 ⇒ **门放行 `ok=true`**。即：**在没有任何方差信息可比的退化输入上给绿灯**，并主动把 `inf` 写进公共字段 `SharedGateResult.ratio` |
| E4 | E3 的变体：`c={1.0,0.0}`、`d={1.0}`（**尺寸不匹配**）、`alpha={1.0,0.0}` | 「尺寸错配会与退化输入可区分」 | **推翻成功**。`naive_diagonal_variance:467` 返回 `-1.0`，`:534` 无条件折成 `0.0` ⇒ 返回值与 E3 **逐字段完全相同**（`{ok=true, shared_present=true, shared_variance=1.0, naive_variance=0.0, joint=1.0, ratio=+inf, reason=kNone}`）⇒ **硬输入错误被静默吞掉且与退化输入不可区分** |
| E5 | config `sigma_high = -1.0` 送 `generate_master` | 「sigma 有范围校验」 | **推翻成功（比原链更严重）**。`cfg_fill:659-662` 只查 `isfinite` ⇒ `master_generator.cpp:127` 的 `dev > sigma_high*sigma` 右侧为负 ⇒ 每轮剔除面扩大到全部 NaN ⇒ `:154-156 out[idx]=NAN` ⇒ **整幅母版静默变 NaN**，零守卫、零痕迹。子代理独立复现并补出对比：`hot_sigma` 负值被 `:243` 短路（静默恒等），**两类 sigma 失效模式不同**，须分开记 |
| E6 | 跑 `p1cal_tests negative` 组，观察 `sigma_zero_threshold_semantics` | 「该门会在实现产出 NaN 时变红」 | **推翻成功（最刺眼）**。`:498` 传 `sigma_low=sigma_high=0` ⇒ 判据 `dev<0||dev>0` 全剔 ⇒ 实现与 oracle 都产出整幅 NaN；`:503` 的 `m` 因 `std::max(m, NaN)==m`（`:40-45`）保持 `0.0` ⇒ 断言以 `m=0` 通过。**该测试恰在实现产出纯 NaN 时转绿** |
| E7 | 逐条比对 `CovarianceRecord` 默认成员值、`make_...` 的每个赋值、`validate_...` 的 9 条判据 | 「存在能证伪库自身生产者的记录」 | **推翻成功（恒真）**。9 条判据全部被生产者自身覆盖；`combination_coefficients` 来自定长 `double[4]` ⇒ 永不为空 ⇒ 判据 8 不可达。极端例 `CalResult{}`（默认 `kRejected`）仍通过 |
| E8 | 穷举 `fix_cal_d_flat_frames(12,12)` 的逐帧中位数 | 「fixture 注释声称的中位数不变式成立」 | **推翻成功**。NPIX=144 ⇒ 各值 48 个 ⇒ 中位槽落在 `base+100` ⇒ 实际中位数 **200/300/500**，注释声称的 100/200/400 为假。**且无任何断言检查这个不变式** ⇒ 一条只存在于注释里的"断言" |

---

## 6. 盲复算（遮住既有判定独立取证）

口径：先不看 `审稿-RR*.md` / `审稿-R2-*.md` / `审稿-R3-*.md` / `审稿-P1-*.md` 的结论，只按本片固化检查项（静默降级 / 自愈判据锚 / 恒红门 / 筛掉真信号 / 退役对象活调用者 / 悬空引用 / 私建线程池 / 硬编码 / 错误码语义 / 数值稳定性）逐项独立取证，再与既有判定比对。

| 检查项 | 我的独立取证结论 | 与既有判定比对 |
|---|---|---|
| 静默降级 | **命中 4 处**：B1（截断→恒等）、B4（类型错→键缺席）、`master_generator.cpp:231/273`（median==0→魔数 1.0）、`cpp/cosmetic_corrector.cpp:229-237`（MAD==0→静默换标准差） | 一致 |
| 自愈判据/锚 | **未命中写回-读回型自愈**；但命中两条"读自己写的东西"：集成合同 `:127` 的 `work_units` 取自一个对执行零影响的配置旋钮（B5）；`p1cal_tests_core.cpp:650` 的期望值取自**另一个 seed** 的 fixture 实例 | **偏松**——既有判定未把这两处登记为自愈锚 |
| 恒红门 | 未发现结构性恒红。恒**真**门 5 条（`:96-101`、`:483-486`、`perf:67-71`、`filter_structure_oracle` 复制实现、`validate∘make`） | **偏松**——既有判定漏报 `perf:67-71` 与 oracle 复制实现两条 |
| 筛掉真信号 | 命中 1 处：`apply_wide_budget`（`cosmetic_corrector.cpp:580-583`）`frac_max` **越大反而抑制越多**——超出 int 范围的 `frac_max` 使 `budget` 变负，`n_mark <= budget` 假 ⇒ 全部宽标记被丢弃并记为"已抑制" ⇒ **配置越宽松，结果越严格**，方向反了，且 `ac_api.cpp:389-419` 对 `master_wide_frac_max` 无任何范围校验 | **新发现（既有判定未覆盖）** |
| 退役对象仍有活调用者 | 命中 2 处，方向相反：① `astro_calibration.h:405` 的 `optimize_dark_k` = 声明+366 行实现+**零个可用构建路径编译**+零调用者（M7）；② `module.yaml:62-74` 把**唯一有活调用者**的 `ac_correct_columns_ex2` 列为不在册（M3）。另 `compute_mad`/`normalize_flat`/`fix_cal_f_buffer` 零调用 | 一致（且我把"活调用者"方向也补上了） |
| 悬空引用 | 命中 12 条（M9 + M8 + M10）。其中 `cosmetic_corrector.cpp:549` **指错了函数**（`:3002` 是 `ac_correct_frame`），比漂移行号更误导 | 一致 |
| 私建线程池 | **未命中**。全部并行走 OpenMP pragma + `ac_set_num_threads` 注入的 ICV，无 `std::thread`/自建池。**但发现一个相关并发缺陷**：`ac_set_num_threads` 改的是**进程级全局 ICV**（`ac_api.cpp:175`），而 `module_entry.cpp:953` 设、`:1054` 还原；同文件 `:34` 只保证「同实例 execute 互斥」⇒ **不同实例并发 execute 会互相踩 ICV**。另：本模块无并行租约违规，但 `module_entry.cpp:949-1055` 的**租约在异常路径泄漏**（acquire 后若 legacy 调用或 `bytes out_plane(plane_bytes)` 抛异常，`:1052` 的 release 被跳过，而 `:1398/:1402` 只重置 state） | **新发现（租约泄漏 + 全局 ICV 竞态，既有判定未覆盖）** |
| 硬编码 | 命中：`0.1` flat 地板 **4 处**（`calibrator.cpp:129/139/173/183`、`master_generator.cpp:243/285`、`calibration_covariance.cpp:64`、`p1cal_oracle.hpp:56/148/164`）无推导无配置；`k_est > 10.0f`（`dark_optimizer.cpp:351`）；`GRID/PER_CELL/MAX_TOTAL`（`:147-149`）；`100/50` 魔数（`:213/240/297/321`）；`1e-12`（`:251`，带 ADU² 量纲）；`1.482602218505602`（有科学推导，可保留） | 一致 |
| 错误码与失败语义 | 命中 M12（四套并存）+ `generate_master` 返回 void 导致 C ABI 无从报错（`ac_api.cpp:101-125` 恒返回 `AC_OK`）+ `correct_columns_ex_impl:619` 的 `AC_COLSTAT_FRAME_TOO_SMALL` 初值虽正确但 `detect_bad_columns:334-336` 的空输入路径**把 status 写成 `OK`** | 一致 |
| 数值稳定性 | 命中：E3/E4 的 `ratio=+inf` 恒真门；`calibration_covariance.cpp:587` 的 `system_error_budget_variance` **负值不校验**（对比 `:499` 同类有 `master_variance<0 → -1`）⇒ 可输出 `ok=true` 且 `joint_variance=-1.0`；`dark_optimizer.cpp:187` 在 float32 做 `dark-bias`（暗项灾难性抵消）；`MasterIdentity` 无 `has_v_single_frame` ⇒ "未知母版方差"与"完美母版"不可区分（M10），后者**低报总方差、高报 SNR** | 部分一致，既有判定未覆盖后三条 |

**盲复算总判定：一致，但既有判定偏松**——我在既有口径下额外命中 5 条既有判定未登记的问题（`apply_wide_budget` 反向、租约异常泄漏、全局 ICV 竞态、负 system_error_budget 未校验、float32 暗项相减）。未发现既有判定**偏严**（无既有判定被我推翻）。

---

## 7. 子代理派发记录

共派发 **5 个**子代理，全部只读（已核验：均未改仓内文件、未 git 写、未编译、未跑 ctest/pytest/任何二进制）。派发时**只给指控、不给结论**，要求逐条「确认/推翻/部分确认 + `文件:行` + 自己的推理」。

| # | 派发范围 | 结果 | **否决/更正了我的什么** |
|---|---|---|---|
| S-A / S-A' | `module_entry.cpp` uint64→int 截断链 + sigma 负值后果（两个独立代理同一范围，互为对照） | 7 环中 6 确认、1 部分确认 | **否决 3 处**：① 我说「负 `max_iterations` 会死循环」→ 实为**零次迭代**（`0 < INT_MIN` 为假）；② 我说「负 `hot_sigma` 会过检测」→ `cosmetic_corrector.cpp:243` 已短路，后果是**同一族的静默恒等**；③ 我说「`cfg_fill` 对 `max_iterations` 查 `!=0`」→ **它连 0 都不查**（比我说的更糟）。另**补强**：定性应从「发现未知 legacy 语义」改为「**适配层无检查的窄化，路由进自己声明为非法的分支**」；指出另两条宿主路径 `p1_int`/`cosmetic_int` **已有区间校验**，故这是**新适配层的回归**；指出孪生模块 `lib/algorithms/cosmetic/src/module_entry.cpp:579/810/819` 同病 |
| S-B / S-B' | 坏列族可达性 + 悬空引用逐条核对（两个独立代理同一范围） | 主命题被推翻；成立 6 条子命题 | **否决 3 处**：① 「整个坏列子系统生产不可达」→ **错**，`ac_correct_columns_ex2` 在 `module_adapters.cpp:3027` 在役且默认开启；② 「`cpp/` 是死代码」→ 准确表述是**不在 CMake 产品面但 `Makefile` 仍有活配方**；③ 我给的 `DATA-P1-COS` 引用位置（`astro_calibration.h:290` + 归 `docs/detail/`）→ 实际在 `:273`，且该 ID 属 `docs/science/DATA_SEMANTICS.md §10`。**补强**：`module_entry.cpp:514` 的「科学导出 1:1 (10)」同样失准；`run/` 下的证据文件被 `.gitignore:17` 排除 ⇒ **从 clone 不可复现**（我本人随后用 `git ls-files run/` 独立复核 = 仅 2 文件，确认） |
| S-C | `calibration_covariance` 自洽性 + 共享门恒真 + 故障注入护栏 | A 恒真（2 例外）、B 确认、C 三项全确认 | **否决 1 处**：我把「所有正向测试都是同义反复」说得过强 → 存在 7 条真负例与一份完全手写的记录测试。**补强**：① 我只推到「库自证」，它推出更强的「**生产路径 9 条判据全部不可达、validator 是死代码**」（`phase1_product.cpp:369-379` 提前返回）；② 补出**负 `system_error_budget` 未校验**与**负对角方差被折 0** 两条同类静默失败；③ 补出 `ACSD_V6_CAL_FAULT` 编入两个**生产**静态库且 `phase1_product` 那个 target **完全没有** `target_compile_definitions`；④ 核实「先例」`ACSD_RT001_FAULT` 在生产 scheduler 源里同样裸 `getenv` ⇒ **证明的是反模式被复制两次，不是已被接受的安全实践** |
| S-D | `tests/p1cal/` 判据独立性全面审计 | 5 节报告，2 条推翻 | **否决 2 处**：① 我说「`max_structure_size`/`max_iterations` 零测试覆盖」→ 仓内别处（`eng/tests/unit/calibration_adapter_test.cpp:230/238`、`calibration_integration_test.cpp:688-758`）**有 5 处**用到，准确表述是「**越界路径零覆盖**，加/去掉上界都不会让任何测试变红」；② 我担心「自愈锚」→ 明确**无写回-读回型自愈**。**独立印证**：**与我各自发现的 `std::max(m,NaN)` NaN 盲门结论一致**（它补全了受影响断言清单）；另补出「`cold_sigma` 阈值推导出期望检出≈1.1e-4 ⇒ 冷像素像素级路径实质零覆盖」与「selfcheck 只覆盖 1/4 组」 |

**合计：5 个子代理，否决/更正我的转述 12 处，其中 3 处推翻了「阻断级」指控的原始表述（死循环→零迭代、过检测→静默恒等、整个子系统不可达），2 处推翻了我对既有判定「偏严」的怀疑。**

---

## 8. 自证段（可复跑命令）

全部只读。路径含中文者已用 `git -c core.quotepath=false`。

```bash
cd "/workspace/Astro CS Database"

# S0 基线
git -c core.quotepath=false rev-parse HEAD          # → 850a9edefd47434b9ab71bc907c3de1e0814b303
git -c core.quotepath=false diff --stat HEAD -- lib/  # → 空（lib/ 与 HEAD 逐字节一致）

# S1 片成员与行数（38 份 / 9276 行，与权威清单一致）
sed -n '37,82p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
# 逐份 wc -l 见本件 §1 的代码块

# S2 B1 截断链
sed -n '449,466p;663,666p;705,708p;968p;1036p;1046p' lib/algorithms/calibration/src/module_entry.cpp
sed -n '62,114p' lib/algorithms/calibration/src/cosmetic_corrector.cpp   # :110 恒真清除门
sed -n '100,120p' lib/algorithms/calibration/src/master_generator.cpp    # :104 零迭代
# 对照：仓内另两条路径已有区间校验
sed -n '1406,1417p' lib/infrastructure/scheduler/src/module_adapters.cpp
sed -n '99,115p'    lib/phase1_session/p1_session.cpp

# S3 B2 恒真门
sed -n '635,677p' lib/algorithms/calibration/src/calibration_covariance.cpp   # 生产者
sed -n '697,741p' lib/algorithms/calibration/src/calibration_covariance.cpp   # 校验者
sed -n '278,288p' lib/algorithms/calibration/include/acsd/calibration/calibration_covariance.h  # 默认值=判据字面量
sed -n '203p'      lib/algorithms/calibration/include/acsd/calibration/calibration_covariance.h  # double jacobian[4] 定长
sed -n '369,379p' lib/algorithms/integration/phase1_product/src/phase1_product.cpp             # 生产提前返回

# S4 B3 / E6 NaN 盲门
sed -n '35,45p;493,506p' lib/algorithms/calibration/tests/p1cal/p1cal_tests_core.cpp

# S5 B4 类型错→键缺席；B5 work_units 预测
sed -n '909,924p;1223,1248p' lib/algorithms/calibration/src/module_entry.cpp
sed -n '64p;78p;92p;127p' lib/algorithms/calibration/integration/acsd_p1_calibration.integration.json

# S6 E3/E4 共享门恒真 + 静默吞 -1.0
sed -n '465,473p;526,536p;584,607p' lib/algorithms/calibration/src/calibration_covariance.cpp

# S7 M1 故障注入无护栏
sed -n '26,30p;148p;216p;298,303p;395p;546p;616p;657p' lib/algorithms/calibration/src/calibration_covariance.cpp
grep -rn "target_compile_definitions" lib/algorithms/integration/phase1_product/CMakeLists.txt   # → 无命中

# S8 M2/M4 符号计数与词表零使用
grep -c "^AC_API" lib/algorithms/calibration/include/astro_calibration.h      # → 18
grep -rn "ACSD_CAL_CFG_KEY_\|ACSD_CAL_OP_\|ACSD_CAL_PLAN_KEY_\|ACSD_CAL_ECODE_" lib   # → 仅 types.h 自身
grep -rn '"12 个' lib/algorithms/calibration/ docs/ 2>/dev/null              # → 6 处

# S9 M3/M5 活调用者 vs 声明
grep -n "ac_correct_columns_ex2" lib/infrastructure/scheduler/src/module_adapters.cpp   # → :3027
sed -n '2853p' lib/infrastructure/scheduler/src/module_adapters.cpp                        # col_enabled 默认 true
sed -n '62,74p' lib/algorithms/calibration/module.yaml                                    # source_symbols 漏 6 个
sed -n '97,98p' lib/algorithms/calibration/README.md                                      # 「无生产调用方」

# S10 M7/M8 悬空路径
ls lib/astro_image_io lib/algorithms/calibration/../astro_image_io            # → 均 No such file
sed -n '41p'  lib/algorithms/calibration/build.ps1
ls -d "testdata/Galaxy_Center_T4 全链路测试数据"; ls testdata/Galaxy_Center_T4   # → 前者不存在；后者仅 素材信息.txt + lights/
grep -n "dark_optimizer" CMakeLists.txt lib/algorithms/calibration/CMakeLists.txt  # → 均无命中

# S11 M10 run/ 未受版本控制
sed -n '17,18p' .gitignore
git -c core.quotepath=false ls-files run/ | wc -l        # → 2

# S12 孪生模块同病（S-A 指出的扩散面）
sed -n '579,582p;810p;819p' lib/algorithms/cosmetic/src/module_entry.cpp

# S13 我推翻过的假设（供下轮免重复）
grep -n "add_subdirectory.*p1cal" eng/tests/unit/CMakeLists.txt               # → :552 存在（初查"未注册"不成立）
grep -n "ac_correct_columns_ex2" lib/infrastructure/scheduler/src/module_adapters.cpp  # → :3027（"不可达"不成立）
ls lib/session lib/scheduler lib/orchestrator 2>&1                            # → 均不存在
grep -n "calibration_covariance\|photometry_apply" CMakeLists.txt              # → photometry 在 :676；covariance 零命中
```

**未执行（按纪律禁止，留给前台）**：任何编译、ctest、pytest、CMake 配置、二进制运行、故障注入实跑。所有"实测量"（如 `want_cold_count≈1.1e-4` 的推导）均已在正文标注为**推导**而非实测。

---

## 9. 给负责人的三条裁决请求（UNRESOLVED）

1. **`max_iterations` / `max_structure_size` 的正确上界取值无仓内依据。** `batch_config.json:26` 的实验默认是 4，但"上界应为多少"无推导。两条候选修法：**① 判红 detail=103**（与 `integration/...json:162` 合同一致，fail-loud，与 `p1_int`/`cosmetic_int` 的既有先例不同——那两条是**回退默认**）或 **② 钳位回退默认**（与两条宿主路径一致）。倾向 ①，但这是仓级政策，需负责人裁定。
2. **`cpp/`（`cc_*` 世代）的去留。** 不在 CMake 产品面，但 `Makefile:12-15` 仍产出导出默认可见符号的 DLL，且 `docs/science/CALIBRATION.md:412` 已把它登记为「已退役通道」。按 AGENTS.md §6「退役代码从代码库删除，或保留统一注释块写明原因」——删 Makefile 配方 + 保留统一注释块，还是整目录删除？
3. **故障注入环境变量是否允许无编译期护栏地存在于生产源。** `ACSD_V6_CAL_FAULT`（8 注入点，含 `add_pedestal` 全帧 +100 ADU）与 `ACSD_RT001_FAULT`（生产 scheduler 源裸 getenv）目前都靠"生产环境不设该变量 ⇒ 不可达"这一**假设**而非护栏。AGENTS.md §8 未明文禁止。需负责人给出仓级口径。
