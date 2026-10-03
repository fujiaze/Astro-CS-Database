# 审稿-P1 · ENG-tests-004（对抗审稿第 1 遍）

- 仓库：`/workspace/Astro CS Database`　HEAD=`850a9ede`
- 依据：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:1271-1325`
- 纪律：零 git 写、零仓库改动、未编译、未跑 ctest/pytest/任何门或实验脚本、未读 `/tmp/acsd_g08/`。
  唯一执行的代码是：在 `/tmp` 副本中 `import` 真实模块 `cfg_anchors.py` 并直接调用其纯函数 `judge_value`，
  用于构造反例（非门/非测试脚本）。所有「会红/会绿」均由源码逐行推理 + 只读数据枚举得出。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（清单权威） | 47 |
| 我**亲自**读完的份数 | **46 整份 + 1 份部分** |
| 成员总行数（清单权威，`实际行数: 10423`） | 10423 |
| 我亲自读到的行数 | **10322** |
| **覆盖率** | **10322 / 10423 = 99.0%** |
| 47 份的覆盖（含子代理，我逐条复核） | 47 / 47 = 100% |

**未读完的部分（如实列出）**：
- `eng/tests/unit/p1snr/p1snr_fref_baseline_test.cpp` —— 读了 **200 / 302**（offset 140–299）。
  未读：第 1–139 行（`prep_case` 的 fixture 构造前半、OLD/NEW 形态写入）与 300–302 行（`main` 收尾）。
  该文件其余部分由子代理完整读毕并交回结论，我对其关键断言（N5 空集逃逸）已独立复核，见 §4-M11。
- 无其他未读完文件。

---

## 2. 本片判定：**需修**（且含 2 条必须先处置的阻断）

不是「通过」：本片有 **4 条阻断**，其中 2 条使所声称的验收域**当前不存在任何机器证据**；
另有 3 条**恒真门/恒红门**已被我构造的反例当场推翻。

最重的 3 条：

1. **B1 `p2_upm_synthetic_test.cpp` 全文不接触任何生产代码，却以「P2-003 单元测试」注册为门。**
   `:4-7` 只 include 标准库，零项目头；`eng/tests/unit/CMakeLists.txt:798` 只链 `acsd_contracts`，
   而同目录 `:804` 的 `p2_block_plan_test` 明确链 `acsd_phase2`。把 `lib/algorithms/coverage/src/upm.cpp`
   整体改成 `return -1;`，本测试 6 条 CHECK 仍全绿。

2. **B2 `p3_output_test.cpp:181-193`「原子性：无 .tmp 残留」是恒真门（前缀永不匹配）。**
   生产侧 `lib/algorithms/fits_output/p3_output.cpp:206` 的真实 tmp 名 = `<out>.<pid>.tmp`
   ⇒ `acsd_p3_out_test.fits.12345.tmp`（**无前导点**）；测试 `:189` 找 `rfind(".acsd_p3_out_test.", 0)`，
   **要求前导点，永不匹配**。`:183` 声明的 `tmp` 变量自始至终未被使用（`:189` 另写了一份字面量）。

3. **B3 夹具三件 + 三份 README：三个负例夹具是真的（红队预设未成立），但层索引 README 声称的
   两个目录是空壳，而 `test_index.csv` 仍把它们记为 PASS。**
   `eng/tests/sciencelint`、`eng/tests/traceability`、`eng/tests/glossary`、`eng/tests/results`
   磁盘上均为空目录、`git ls-files` 计数 **0**；而 `eng/tests/test_index.csv:14/20/23` 仍分别记
   `PASS,5` / `PASS,6` / `PASS,9`，共 **20 个不存在的用例被计入绿区**，且工作树干净、无门会报。

---

## 3. 逐文件清单（读了什么 / 看到什么 / 判定）

> 判定口径：**需修** = 须处置；**通过** = 本遍未发现本文件自身的问题；括号内为问题编号（见 §4）。

| 文件（成员 47） | 读到 | 关键所见 | 判定 |
|---|---|---|---|
| `config/check_cfg002_registry.py` | 1624/1624 | 12 项检查 + 47 条注入；登记册 119 行指向**已删**的 `docs/detail/algorithms_phase{1,2,3}/`；沙箱输入含全仓不存在的 `ENGINEERING_SPEC.md`；`:410-413` docstring 规则 3 从未实现 | 阻断 B4/B5 |
| `config/test_cfg003_multiblock.py` | 124/124 | 8 例真门；`:110` 用裸文本 grep `$defs/filter_name` | 通过 |
| `config/run_validation.py` | 46/46 | fail-closed 干净 | 通过 |
| `config/fixtures/negative/cpu_profile_v2_bad_kernel.json` | 48/48 | `provider:"sse4"` 确在 `$defs.kernel_v2` enum `["baseline","avx2","avx512"]` 外 → **真负例** | 通过 |
| `config/fixtures/negative/hardware_fields_in_phase_config.json` | 25/25 | `workers/isa/block_size/drizzle.isa_level` 确不在 `additionalProperties:false` 的块与 `drizzle_config` 内 → **真负例** | 通过 |
| `config/fixtures/negative/mosaic_block_weight_mode.phase_config.json` | 13/13 | `weight_mode` 不在 `mosaic_block` 属性面 → **真负例**；`docs/ACSD_DESIGN.md §3.1` 的「全链没有『权重模式』这一可选概念」逐字属实，**无伪引** | 通过 |
| `unit/calibration_adapter_test.cpp` | 810/810 | E 段 BITWISE 的期望值由同一批 `ac_*` 产生（结构对称型）；6 处丢弃返回码；`:463` 死赋值被 `:464` memset 抹掉 | 须修 M1 |
| `unit/p3_projection_test.cpp` | 660/660 | **容差取自被测生产表**（`ap->roundtrip_tol_px`），全仓无字面量钉死；G9「故障注入」是人为偏移期望值的恒假面 | 阻断 B6 |
| `unit/p3_output_test.cpp` | 568/568 | 3b/3c/3d/3e 负例设计**很好**（有配对正例）；但 §3 原子性门恒真；`:67` 版本硬编码 `0.10.0-alpha.2` 与根 `VERSION=0.1.0-alpha.1` 不符 | 阻断 B2 + 须修 M2 |
| `unit/gaia_magnitude_range_bounds_test.c` | 414/414 | A/B 段是**设计良好的双向夹逼**（36 合法 `has==1` / 20 畸形 `has==0`）；但 `GAIA_P19_BASELINE` 全仓零 CMake 注册，声称的红基线从未构建 | 须修 M3 |
| `unit/p2_upm/p2_upm_geo_diag_test.cpp` | 331/331 | B/C/D 段真门（强制不收敛、警告 JSON、小 buffer 截断、持久化往返）；A16 因 `rank_eff≡n_params` 使 `dof_eff≡0`，Andrae 公式不可判别 | 通过 |
| `unit/p2_upm_synthetic_test.cpp` | 146/146 | 零生产代码；`:81` 代数恒真式；`:131` 恒绿门；`:141` PASS 文案虚假宣传 | **阻断 B1** |
| `unit/p1snr/p1snr_fref_baseline_test.cpp` | 200/302 | N2/N3/N4 为解析式真门；N5 用 `.value(...,0.0)`，两处键皆缺时 `g_a==g_b==0` 恒绿 | 须修 M11（未读 1-139、300-302） |
| `unit/p1wcs/p1wcs_tests_properties.cpp` | 253/253 | F5a/F5b/嵌套/平移不变均真门；但 `:169` 线程数**无生效断言**、`:178-180` 配对残缺、`:219` 只查 A/B 不查 AP/BP | 须修 M4 |
| `unit/p1wcs/p1wcs_tests_selfcheck.cpp` | 232/232 | 四阶段自检**有正控**（阶段 4）且有 guard-path 三态证；`run_child` 失败返 127≠0 ⇒ 阶段 2/3 把「子进程没起来」读成「必败验证通过」 | 须修 M5 |
| `unit/p1_psfw/p1psfw_tests_psfsw.cpp` | 171/171 | `:30` `o_robust_scale_mad` 是**真独立 MAD oracle**（硬编码 1.482602218505602 vs 生产 `kMadToSigma`）；`:80-83` 用生产指数复算 ⇒ 指数值从未被钉死 | 须修 M6 |
| `unit/p1_psfw/p1psfw_fixtures.hpp` | 133/133 | `good_record()` 逐字段手写（正控由测试自造）；`:47` `quantile_shim` 是分位数的测试侧复刻 | 须修 M7 |
| `unit/cli_bench_verdict_test.cpp` | 170/170 | `:109` `kRegistry.size()==10` 验的是本地字面量（编译期恒真）；`:149` 只删 `ext.front()`；`:168` 成功信息打到 stderr | 须修 M8 |
| `unit/cpu_fallback_test.cpp` | 149/149 | **7 例无一条正向控制**，无一例 `LoadResult::OK`；`:44` `system("mkdir -p")` 返回值未检（Windows 无此命令） | 阻断 B7 |
| `unit/p1_calibration_test.cpp` | 123/123 | `ref_std`/`ref_darkopt` 是生产公式的测试侧复刻；头声称「u16/mask」，全文无 mask、无 u16 API | 须修 M9 |
| `unit/core_context_test.cpp` | 110/110 | `test_budget_lease:46-53` cap→耗尽→RAII 归还→再取是**有鉴别力的三态序列**；artifact 重复/非法/不存在/取消/指标均有真负例 | 通过（质量样板） |
| `cli/test_cli004_process_protocol.py` | 469/469 | `:33-35` `CLI_DIR` 元组**两项完全相同**，注释声称的「旧路径回退」不存在；test_10/11 是**范式级反 fail-open**（`self.fail("[不可判]")`） | 须修 M10 + 肯定 |
| `cli/test_phase3_inprocess.py` | 431/431 | docstring `:10-21` **公开声明** 8/11 用例在 CLI-002 修复前必然红（显式红标记，做法正确）；`:56-57` `_pick` 对 `.cpp` 用 `isdir` | 须修 M12 |
| `cli/test_fix208_event_stream_default.py` | 385/385 | LOG-001 字段命名混用负例**扎实**（`:246-274`）；`:326-381` C++ 探针是**货真价实的 oracle** | 通过（质量样板） |
| `cli/test_monitor.py` | 111/111 | docstring `:2-3` 声称「与 OS 工具误差在冻结范围」，**全文件无任何 OS 工具**；`:83` 2ms 与 `:106` 5ms 同引「07 §1」却不同值 | 须修 M13 |
| `abi/test_abi002_lifecycle.py` | 360/360 | `:283-306` 载入 `text` 却从未比对头文件，失败信息宣称「!= 头枚举」；`:217-223` 只 grep 注释 | 须修 M14 |
| `runtime/test_phase_lifecycle.py` | 272/272 | `:263-268` 用例名断言「不是全局缓存」，体只断言两次调用**相等**（两种实现都满足） | 须修 M15 |
| `backend/test_abi_loader.py` | 202/202 | `:87-94/104-120/160-178` 真负例 + 配对正例，**能红能绿**；`:177` f-string 无占位符 | 通过（质量样板） |
| `backend/test_noise_model_oracle.py` | 198/198 | `:166-181` 解析律（Poisson+read、scale law）是**真第一性原理校验**；`:183-194` P4 只查 iv0 且容差 ±40% | 须修 M16 |
| `backend/test_bench_harness.py` | 94/94 | `test_01` 用对抗件 `cheat_backend.cpp` 做**负控制**（ORACLE_FAIL + 零计时 + 永不获胜），结构最干净 | 通过（质量样板） |
| `pipeline/test_typed_dag_plan.py` | 186/186 | `:172-182` `assertIn("IMPLICIT_PATH")` 是**正确写法**（钉判别原因）；`:141-149` 只查非空，docstring 声称的「一致」无断言 | 须修 M17 |
| `api/test_reject_integration_oracle.py` | 162/162 | `:34-35` 注释**自承**布局「需确认」；`wmode` 恒 0 ⇒ docstring 声称的「多权重」零覆盖；`:154-158` small-N 臂与 `:134` test_01 逐字相同 | 须修 M18 |
| `unit/aio/mutations/durability_mutation_and_check.py` | 100/100 | **四重保险**：正控制 `:62-69`、变异 `:85-96`、锚点计数 fail-closed `:78-80`、构建失败判「不可判」`:88-90` | 通过（**本片最强样板**） |
| `monitoring/__init__.py` | 1/1 | 仅注释 | 通过 |
| `conformance/noop/README.md` | 78/78 | `:14/:67` 产物名 `libacsd_noop.so` **是错的**（`CMakeLists.txt:36-37` `OUTPUT_NAME acsd_noop` + `PREFIX ""` ⇒ `acsd_noop.so`，`install-tree.contract.json:11` 三方一致）；`:69` `verify_install_tree.py` 不存在；`:43` `11_MODULE_SOURCE_TEST_STANDARD.md` 不存在 | 阻断 B9 |
| `integration/p1_integrate/README.md` | 32/32 | `:12/:16` 「24 条」实为 **25**（`p1_integrate_test.cpp` `cases.push_back` 恰 25 次）；`:14` 「6/6 mutation」实为 **7/7** | 须修 M19 |
| `README.md`（eng/tests） | 25/25 | `:17/:18/:21` 三个目录为空壳或已删；`:25` 上游 `ENGINEERING_SPEC.md` 不存在 | 阻断 B3 |
| `validation/.../fix_p2b_variance_oracle/variance_oracle.cpp` | 221/221 | `:95` 期望值取自**被测 TU 内**的 `mean_model_naive_over_correct`（其 header 自称「仅自证/审计用」）；`:127` `w_raw` 全用硬编码字面量 | 须修 M20 |
| `validation/.../fix_p2b_variance_oracle/run_wc_selfcheck.sh` | 16/16 | `:5` `D=run/RELEASE-02/fix-p2b` 与归档实际落点不同处 | 建议 |
| `validation/.../q1_photometry_gradient/src/spatial_calib.py` | 156/156 | `:93-95`、`:135`、`:144-145` 三处**结论句硬编码**，与同文件算出的 `p_value` 无关 | 阻断 B10 |
| `validation/.../c_delta_composition/mult_test3.py` | 93/93 | `:4-5` 输入归档 `run/RELEASE-02/L4-rebuild/upmfix_out/` 不存在；`:63-67` 取**低半**做中位比 | 须修 M21 |
| `validation/.../c_delta_composition/controls_rank1.py` | 90/90 | `:3-4` `from layout import` 的 `layout.py` **全仓不存在**；`:82` 用 `pred>0`、`:88` 用 `m>0` 两个不同筛选 | 须修 M21 |
| `validation/.../q2_snr_smoothness/realdata/stageE2_nosearch.py` | 78/78 | `:73` `J_at_maxabs = max|J|` **按构造 ≥ |J(0)|**，不可能为负；`:65` 允许 NaN 进入 | 须修 M22 |
| `validation/.../phot_verify/sky_vignette.py` | 65/65 | `:29` 算出的 `r` 从未使用（死码）；`:43` 静默丢环 | 建议 |
| `validation/.../phot_verify/vignette_test.py` | 53/53 | `:22-23` 缺 npz → `continue`，但 `:52` **无条件**写 JSON 并打印「saved」 | 须修 M23 |
| `validation/.../phot_verify/run_pixel.py` | 37/37 | `:24-25` 无匹配 → `continue`，`:36-37` 仍写 JSON 并打印「saved … n=0」 | 须修 M23 |
| `validation/.../q3_additive_truth/src/step2_alpha.py` | 57/57 | `:2` `sys.path` 指向已删目录（靠脚本自身位置侥幸命中）；`:24` 在**因变量** `d` 上截尾后回归斜率 | 须修 M24 |

---

## 4. 发现清单

### 阻断（4）

| # | 位置 | 机制 | 为何能让真实缺陷逃逸 |
|---|---|---|---|
| **B1** | `eng/tests/unit/p2_upm_synthetic_test.cpp:1-146`；`eng/tests/unit/CMakeLists.txt:797-799` | 零项目头（`:4-7` 仅标准库）；CMake `:798` 只链 `acsd_contracts`（对照 `:804` 的 `p2_block_plan_test` 链 `acsd_phase2`）。`:78-79` 的「解」`C_B=-dAB; C_C=-dAC` 由测试自写 | `lib/algorithms/coverage/src/upm.cpp` 整体改成 `return -1;` 仍全绿。**P2 UPM 域当前零机器证据** |
| **B2** | `eng/tests/unit/p3_output_test.cpp:181-193` | `:189` 找前缀 `".acsd_p3_out_test."`（带前导点）；生产 `p3_output.cpp:206` 真实名 `acsd_p3_out_test.fits.<pid>.tmp`（无前导点）。`:183` 的 `tmp` 是死变量 | 写路径任何 `.tmp` 泄漏（未 close / 异常路径）该门都不红。**恒真门** |
| **B3** | `eng/tests/README.md:17,18,21`；`eng/tests/test_index.csv:14,20,23` | `sciencelint`/`traceability`/`glossary`/`results` 四目录磁盘空、`git ls-files` 计数 0；索引仍记 `PASS,5`/`PASS,6`/`PASS,9` | **20 个不存在的用例计入绿区**；工作树干净、无门会报；`ls eng/tests/` 看着完全正常 |
| **B9** | `eng/tests/conformance/noop/README.md:14,67`（产物名）；`:43,69-70`（悬空引用） | `CMakeLists.txt:36-37` `OUTPUT_NAME "acsd_noop"` + `PREFIX ""` ⇒ Linux 产物 `acsd_noop.so`；`install-tree.contract.json:11` 三方一致。README 写 `libacsd_noop.so` | **文档把正确实现写成缺陷**：`nm -D libacsd_noop.so` 直接 `No such file`，即 §7 验证步骤在正确实现下就红。另 `eng/packaging/verify_install_tree.py` 与 `11_MODULE_SOURCE_TEST_STANDARD.md` 全仓不存在 |

> 另两条我原列为阻断，经复核后**降级并入须修**，理由见 §7 否决记录：
> CFG002 的「门跑不通」（B4）与内容锚值锁失效（B5）。

### 须修（24，摘要）

- **M1** `calibration_adapter_test.cpp:521-702` 期望值由同一批 `ac_*` 产生（结构对称）；`:545,565,585,619,622,667,690` **丢弃返回码** —— 两侧一致失败留全 0 则 `memcmp(zeros,zeros)==0` 仍绿；文件头 `:15` 「全部 10 科学 op」不实（实为 8）。
- **M2** `p3_output_test.cpp:67` `prov.software_version = "0.10.0-alpha.2"` 注释写「来自版本单源, 非硬编码」—— 根 `VERSION` 实为 `0.1.0-alpha.1`，**既硬编码又不匹配**，直接违反 AGENTS.md §11。
- **M3** `gaia_magnitude_range_bounds_test.c:20-22` 声称的红基线（`GAIA_P19_BASELINE` 编译到改前源）**在全仓任何 CMakeLists 中零命中**，从未构建；`:150-153,276-278,304-310,361-409` 的 `#ifdef` 分支是死代码。D 段 fixture 刻意使两侧皆空（`:320-321` 自陈），`:178` `if(an==0) return 1` ⇒ 「剪枝==不剪枝」是空集比较。
- **M4** `p1wcs_tests_properties.cpp:169` `omp_set_num_threads` 无任何生效断言（可被 `num_threads()` 子句架空）；`:178-180` 三处比对缺 `rt[1].B↔rt[2].B` 等交叉对；`:219` 只查 `sip.A/B` 不查 `AP/BP`，且 `:210` `ipv::WcsFitResult w;` 未初始化。
- **M5** `p1wcs_tests_selfcheck.cpp:79,83` fork/execve 失败返 `127 ≠ 0` ⇒ 阶段 2/3 把「子进程根本没起来」读成「必败验证通过」（阶段 4 兜底故不自愈，但诊断误报）。
- **M6** `p1psfw_tests_psfsw.cpp:80-83` `wt_of` 用**生产常数** `kCompositeAlpha..Delta` 复算 ⇒ 把 alpha 由 2.0 改 0.5 全绿；`:22` `background_samples` 全为 2.0 ⇒ `:31` 的 B 断言零鉴别力；N 分量无数值断言。
- **M7** `p1psfw_fixtures.hpp:87-126` `good_record()` 正控由测试手写字面量构成（任何只回读 `n_common==30` 的门必过）；`:60,88` 无 `{}` 的默认初始化。
- **M8** `cli_bench_verdict_test.cpp:109` `kRegistry.size()==10` 验本地字面量（**编译期恒真**），而 `protocol.h` 的 `registered_event_kinds_v1()` 已 include 却未用 ⇒ 生产新增第 11 类不可见；`:149` 41 个扩展字段只对 `ext.front()` 验必填。
- **M9** `p1_calibration_test.cpp:2` 声称覆盖「u16/mask」，全文**无 mask 断言**、`astro_calibration.h` 无 u16 API；`:5-8` 未 include `<algorithm>` 却用 `std::max`。
- **M10** `test_cli004_process_protocol.py:33-35` `CLI_DIR` 元组两项**完全相同**（都 `lib/infrastructure/cli`），注释 `:32` 声称的「旧路径回退」是虚构；`:54-56` 的 `lib/astro_image_io` 回退**路径已不存在**。
- **M11** `p1snr_fref_baseline_test.cpp:290-293` N5 用 `.value("flux_adu", 0.0)`，两处键皆缺时 `g_a==g_b==0` ⇒ 「帧间独立」这一核心锁恒绿。
- **M12** `test_phase3_inprocess.py:56-57` `_pick` 用 `os.path.isdir` 但传入的是 `.cpp` **文件路径** ⇒ 第二候选永不可选，docstring `:8` 的「旧路径回退」为假（失败响亮，非 fail-open）。
- **M13** `test_monitor.py:2-3` 声称「与 OS 工具误差在冻结范围」—— 全文件无 `ps`/`getrusage`/`/proc` 直读；`:83` 2ms 与 `:106` 5ms 同引「07 §1」却给出不同冻结预算。
- **M14** `test_abi002_lifecycle.py:284` 载入 `text = open(HDR)` 后**从未使用**，`:287-288` 比的是 Python 字面量 `STATES`/`OPS`，而失败信息写「!= 头枚举」；`:217-223` 只对头文件做三段中文 `assertIn`。
- **M15** `test_phase_lifecycle.py:263-268` 用例名 `test_registry_loaded_per_call_not_cached_globally`，体只 `assertEqual(first, second)` —— **每次读盘与全局缓存都满足**，结构上不可区分。
- **M16** `test_noise_model_oracle.py:183-194` P4 只对 `iv0` 做数值检查且容差 **±40%**，iv32/iv63 仅 `>0`；`has_spatial_field`/`n_control_points` driver 打印但从不断言 ⇒ 常量填充也全绿。
- **M17** `test_typed_dag_plan.py:141-149` 只 `assertTrue(e[key])` 查非空，docstring `:6` 声称的「data_schema_id/unit/coordinate/scalar/shape 一致」**一条断言都没有**。
- **M18** `test_reject_integration_oracle.py:65` `wmode` 硬编码 0 ⇒ docstring 声称的「多权重」零覆盖；`:154-158` small-N 臂 docstring 自认「无欠定」，断言与 `:134` 逐字相同；`:34-35` 注释自承缓冲区布局「需确认」而所有 inlier ≈10.0 ⇒ 完全错误布局下亦全绿。
- **M19** `p1_integrate/README.md:12,16` 「24 条」实为 **25**（漏 `flux_factor_pixfrac2`，恰是 pixfrac² 口径钉死那条）；`:14` 「6/6 mutation」实为 **7/7**。根因：`EVIDENCE.md:49-50` 跑的是改名前路径 `tests/integration/v6_p1`，此后新增用例**从未重测**。
- **M20** `variance_oracle.cpp:8` 声称「期望值在本 TU 内以闭式独立复算（不调用被测实现）」，但 `:95` 的期望量 `mean_model_naive_over_correct` **定义在被测 TU 内**（`variance_propagation.cpp:212-219`，其 header 自称「仅自证/审计用」）；`:127` `w_raw` 全用硬编码字面量，全程不碰生产 API。
- **M21** `mult_test3.py:4-5` / `controls_rank1.py:3-7` 输入归档 `run/RELEASE-02/L4-rebuild/upmfix_out/` 与 `run/RELEASE-02/trail/layout.py` **全仓不存在**；两脚本对同一 `(SPAN,)` tile 用了**互斥索引约定**（`mult_test3.py:19` 原始下标 vs `controls_rank1.py:16,20` FITS 重排下标），且 `controls_rank1.py:82` 用 `pred>0`、`:88` 用 `m>0` 两个不同筛选 ⇒ 两条对比臂的 rms 不在同一子集上。
- **M22** `stageE2_nosearch.py:73` `J_at_maxabs = max over off of |J(off)|` **按构造 ≥ |J(0)|**，该「bias」量不可能为负，要表达的真实信息（搜索是否**降低** |J|）永远看不到；`:65` 允许 NaN 进入，`:73` 的 `max()` 遇 NaN 按顺序返回任意合法值。
- **M23** `vignette_test.py:22-23` 缺 npz → `continue`，但 `:52` **无条件**写 JSON 并打印「saved」；`run_pixel.py:24-25` 同形 ⇒ **fail-open 伪装成合规**。叠加 `run_pixel.py:27` 只覆盖不清理 ⇒ 上一轮 npz 会被当本轮结果加载。
- **M24** `step2_alpha.py:24` 按 `|d - median(d)| < 8σ_d` 在**因变量** `d` 上截尾后回归斜率 `d ~ α·L + β` ⇒ OLS 斜率系统性衰减，偏差方向恒为 α 偏小，而 α 正是 P5/P3 要量的量；**不跑无剪枝对照臂**。

### 建议（摘）

`check_cfg002_registry.py:126` 表头只认首格「字段」；`:808` 引用正则要求 `:数字`，裸路径整类逃逸；`:390-392` `registration=="cli"` 读了 `lines` 随即丢弃、`registered_key` 从不校验；`test_cfg003_multiblock.py:110` 裸文本 grep；`p1wcs_tests_properties.cpp` 三处 CHECK 复用同名 `p1_thread_bitwise`；`cli_bench_verdict_test.cpp:168` 成功信息打到 stderr；`gaia_..._test.c:43` `INV_SCALE` 死宏、`:388` `freopen` 永久替换进程 stderr；`p2_upm_geo_diag_test.cpp:292` 硬编码 `/tmp/...json`（无 PID）。

---

## 5. 我主动构造的反例

### 5.1 【最强】内容锚「值锁」被推翻 —— `eng/tests/config/cfg_anchors.py:109` 的 D4 是恒真门

**构造**（在 `/tmp` 副本中，`import` 真实 `cfg_anchors.py` 后直接调其纯函数 `judge_value`，**未运行任何门/测试脚本**）：

1. 取 `defaults.json` 的 `noise.saturation_level`（`value=0`、`value_text='0'`）；
2. 在其**同一篇被引文档**中，挑一句**唯一命中、且与 saturation level 毫无关系**的话
   `| \`σ_bg\` | \`1.482602218505602·MAD(|x−median|)\` | \`noise_model.cpp:robust_sigma\` |`
   作为新 `quote`，并按 `fingerprint()` 重算 `sha256`；
3. 调用 `judge_value(repo, trial, "0")`。

**结果**：`True  |  line=20`。

四条判据全部通过：D1（长度 ≥2）、D2（引文在文档内唯一命中）、D3（指纹自洽）、D4（`'0' ⊂ quote`）。

**期望推翻什么**：`check_cfg002_registry.py:751` 明文声称
「`value_text` 必须落在引文内（**锚锁住的是那个值/判据文本，而不是任意 token**）」。

**是否推翻**：**推翻成功**。`'0'` 落在 `1.4826**0**2185…` 里 —— D4 对该字段退化成的命题是
「引文里含有数字字符 0」，而**不是**「文档声明 saturation_level 的默认是 0」。

**杀伤面**：`defaults.json` 59 个 field 中 53 个带 `source_ref`，其中 **20 个** 的 `value_text`
是**长度 ≤2 的裸数字串**（`'0','1','2','4','5','6','8','10','50','64','9216'`…）。
最极端的三个（引文长度 / 该数字在引文中出现次数）：

| key | value | value_text | 引文长 | 出现次数 | 上下文（真实语义） |
|---|---|---|---|---|---|
| `noise.saturation_level` | 0 | `'0'` | 800 | **7** | 全部来自 `h>0,w>0` |
| `noise.mask_radius_scale` | 6 | `'6'` | 490 | **9** | 来自 `6**0** px`、`92**16**`、`1.5` |
| `noise.spatial_field_enabled` | 1 | `'1'` | 259 | **5** | 来自 `>=**1**`、`1/16` |

**⇒ 这是本轮最有价值的产出：一个「用同一个定义式既当被检量又当期望量」的变体 ——
D4 把「锚文本」同时当作「期望值」的容器，而期望值本身从未从文档中被提取。**

### 5.2 其余反例（逐条推演，**均未推翻**）

| # | 注入缺陷 | 期望推翻 | 会不会仍绿 |
|---|---|---|---|
| R1 | `upm.cpp` 整体 `return -1` | `p2_upm_synthetic_test` 6 CHECK | **仍绿**（CMake:798 不链 UPM） |
| R2 | 生产 `roundtrip_tol_px` 由 1e-8 放宽到 1e-3 | `p3_projection_test` G2 四投影往返门 | **仍绿**（`:88` 从生产取值，全仓无字面量钉死；已核实 `1e-8/1e-6` 只出现在 `:12,13,77,81,82` 的**注释**里） |
| R3 | 写侧泄漏 `acsd_p3_out_test.fits.<pid>.tmp` | `p3_output_test` §3 原子性门 | **仍绿**（前缀不匹配） |
| R4 | `preflight_entry` 无条件 `return FALLBACK_BASELINE` | `cpu_fallback_test` 7 CHECK | **仍绿**（无一条 OK 正例） |
| R5 | `p3_projection_registry_find("tan")` 误返非空（真缺陷） | 该缺陷本身 | **在 fault 模式下反而返 0 并打印「注入缺陷被捕获」**（`:641-648`）——**真实红灯被自愈成绿灯** |
| R6 | `ACSD_P3PROJ_FAULT=TAN`（大写 typo） | 必败面 | **报正常 PASS**（`:566-569` 无匹配 ⇒ `fault_mode` 保持 false ⇒ 落到 `:650`） |
| R7 | `GAIA_P19_BASELINE` 红基线从未构建 | 「能红」证据 | **仍绿**（`:276-278,304-310` 永久走 `#ifndef` 分支） |
| R8 | `p1_snr.json` 删掉 `flux_adu` | N5 帧间独立 | **仍绿**（`.value(...,0.0)` ⇒ `0==0`） |
| R9 | `kCompositeAlpha` 2.0 → 0.5 | psfsw 复合权重全部断言 | **仍绿**（`:80-83` 用同一生产常数复算） |
| R10 | 生产注册第 11 个 kind | `cli_bench_verdict` 「恰 10 类」 | **仍绿**（`:109` 只数测试本地数组） |
| R11 | 写 `var=2.0 / ivar=0.5`（乘积仍为 1） | `p3_output_test:153` 的 `ivar==1/var` 不变量 | **仍绿**（只查乘积，`vbuf==4.0` 从未断言） |
| R12 | `curve_json::map_filter_name` 改大小写不敏感 | `CFG002-04`「exact + 无别名」 | **仍绿**（`:554-621` 全读 `filters.json` 自述字段，一个字节不读生产实现） |
| R13 | 摘掉 normalize/mosaic 的 `filter_name` `$ref` + 登记册同步改 `[]` | `CFG002-04` 红绿双向循环 | **仍绿**（`:685-688` 分母来自登记册，`want=exercised=[]`，循环零次执行） |
| R14 | 把 `noise.saturation_level.source_ref` 重锚到离题句（= §5.1） | `CFG002-09` / `-12` 的值锁 | **仍绿（实测通过）** |
| R15 | `clip_sigma`/`max_clip_rounds` 等 20 个裸数字锚全部重锚 | 同上 | **仍绿（同族）** |

**R11/R12/R13/R14 经我独立复核确认成立**；R1–R10、R15 为源码逐行推演。

---

## 6. 盲复算（遮住既有判定独立取证）

口径：遮住 `审稿-RR*.md` / `审稿-R2-*` / `审稿-R3-*` / `审稿-P1-*` 的结论，只依原始源码 + 只读数据枚举独立取证，再比对。

| 项 | 既有判定倾向 | 我的独立取证 | 一致性 |
|---|---|---|---|
| `check_cfg002_registry.py` 健康度 | 「rc=0 全绿」 | 登记册 119 行 → 已删目录（49+25+12=86 行指向三个已删目录，33 行指向现存 `infrastructure`）；`test_index.csv:8` **自己记 `FAIL,78,5`**，失败明细含「CFG002-01 缺登记 6 行」 | **判一致，但均偏松** —— 既有把「绿」当基线，实际机器记录已是红 |
| 内容锚机制 | 「R-54 内容锚 >> 行号，值锁已实现」 | D4 对 20/53 锚退化为「含某数字字符」 | **我偏严，且是本片新问题** |
| 三个 config 负例夹具 | 可能合法 / 校验器不查 | 全部真负（逐条核对 schema enum 与 `additionalProperties:false`） | **我偏松** —— 我推翻了「夹具可能是假负」的预设 |
| `eng/tests/README.md` 目录清单 | 目录存在 | 4 个空壳/已删，索引仍记 20 个 PASS | **我偏严** |
| 实验域（release02）归档可信 | 「代码改了归档没重跑」 | 确认：`spatial_calib.py:93-95,144-145` 三处结论句硬编码；`variance_oracle.cpp:95` 期望量在被测 TU 内 | **判一致** |
| 判据失败方向 | — | `vignette_test.py:52` / `run_pixel.py:36`「continue 后仍写 JSON 并打印 saved」= fail-open | **我偏严** |

**结论**：既有判定在「实验域归档滞后」与「自愈/自证」两类上与我一致；但
**(a) 低估了 README/索引层的一致性破损（4 个空目录 vs 20 个 PASS）**，
**(b) 未察觉内容锚 D4 的值锁在短数字上恒真**，
**(c) 把三个 config 负例夹具列为可疑 —— 实为真负，我推翻该预设**。

---

## 7. 子代理派发记录与复核

派发 **5 个子分片**（实际启动 7 次：其中 cfg002 与 python 两片各被重复派发一次，
重复件结论我同样逐条复核，未采信其「首次自述」）。全程零写、零编译、零测试执行。

| 子代理 | 范围 | 行数 | 我复核结论 |
|---|---|---|---|
| A1 / A2（重复） | `check_cfg002_registry.py` | 1624 | **采纳 B1/B2/B3/B5**；**修正**其「phase_props 漏 16 多 2」为**「漏 17、幽灵 2」**（我用 5 个 `$defs` 并集重算）；**否决**其「`ENG-TEST` 门跑不通算阻断」的措辞 → 降为须修（机器记录本已是红，属「已知红」而非新阻断） |
| B1 / B2（重复） | C++ 单测簇（15 份） | 4572 | **采纳 B-1/B-2/B-3（p2_upm_synthetic / p3_output tmp / cpu_fallback）** —— 我逐条独立复核：CMake:798、p3_output.cpp:206、cpu_fallback 无 OK 正例，三条**全部成立**。**采纳** B3（roundtrip_tol 生产自评）、**采纳** S-1/S-3/S-5/S-6 |
| C1 / C2（重复） | Python 测试簇（15 份） | 3141 | **采纳 B1/B3/B5/B6/B7/B8（ABI-002 无生产实现 / Store 桩 / cancel 空流 / oracle 未实现 / reject 布局不变 / monitor 无 OS 参照 / test_cli004 缺 kind 检查）**。**部分否决 B11**（见下） |
| D | release02 实验域（10 份） | 866 | **采纳 B1/B2/B3（悬空归档 / 归档早于代码 12 天 / spatial_calib 三处硬编码结论句）**、**采纳 S1/S5/S6/S7/S8** |
| E | 夹具三件 + 三份 README | 220 | **采纳 B1（sciencelint 空目录）/ B2（24→25）/ B3（6/6→7/7）/ B4（`libacsd_noop.so` 错）**；**采纳 B5-B10** |

### 我明确**否决/修正**的子代理结论（5 条）

1. **否决 C1/C2 的 B11「`from cli_test_hygiene import run_cwd` 不可导入」。**
   理由：两个代理都只看了「无 conftest / 无 pytest.ini」就推断 pytest prepend 失败。我查了
   `eng/tests/test_index.csv:7`，官方调用方式是
   `python3 -B -m unittest discover -s eng/tests/cli -t eng/tests/cli` ——
   `-t` 指定 top-level dir 即 `eng/tests/cli`，该目录被插入 `sys.path`，**导入成立**。
   ⇒ 该「全片多数门无执行保证」的推论**证据不足**，降为待核（真正成立的是「`durability_mutation_and_check.py`
   文件名不匹配 `test_*.py`，仓库内无任何文件引用它 ⇒ 收集面未知」）。

2. **修正 A1/A2 的 B3 计数**：其称 `phase_props`「漏 16 个真实块属性、**多 2 个幽灵键**」。
   我以 `normalize_block ∪ mosaic_block ∪ export_block` 重算：**漏 17**
   （新增 `source`；其漏列 `source` 与 `coverage_index` 等），幽灵键 `algorithm_upm_gauge`、`precision`
   **确认存在**。结论方向不变，数字以我的为准。

3. **否决 A1/A2 把「CFG002 门当前必红」列为阻断。** `test_index.csv:8` 已把 `eng/tests/config`
   记为 `FAIL,78,5`（2026-09-27），失败明细逐条列出 —— 这是**已被登记的已知红**，
   不是本轮新发现的阻断。降为须修 M（登记册迁移未完成），但我**保留**其指出的
   「`--self-test` 在 `:1550` 早于 `build_sandbox` ⇒ 前置红掩盖全部 35 类注入」为有效须修项。

4. **否决 B1/B2 的「`test_abi002` 五个判定函数在 `lib/` 下定义数 = 0 ⇒ ABI-002 无生产实现」。**
   该结论虽可能成立，但属**跨片断言**（`lifecycle_v1.h` / `abi002_lifecycle_probe.c` 都不在本片），
   我未亲自核实，故**不计入本片发现**，列入待核。

5. **否决 E 的「`eng/tests/results/` 空残留目录应删」为问题项。** 索引未登记 `results/`，
   属未登记残留而非索引谎报，降为建议。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"   # HEAD=850a9ede
# —— 覆盖率口径：47 成员 / 10423 行（清单权威）
sed -n '1279,1325p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml \
  | sed 's/^ *- "//; s/"$//' > /tmp/engtests004.txt
while IFS= read -r f; do printf "%6d %s\n" "$(wc -l < "$f")" "$f"; done \
  < /tmp/engtests004.txt | sort -rn        # 合计 10423，与清单 实际行数 一致

# —— B3：四个空目录 + 索引仍报 20 个 PASS
for d in sciencelint traceability glossary results; do
  echo "$d disk=$(ls -A eng/tests/$d | wc -l) tracked=$(git ls-files eng/tests/$d | wc -l)"; done
grep -n "sciencelint\|traceability\|glossary" eng/tests/test_index.csv

# —— B4：登记册 119 行指向三个已删目录
python3 -c "import json,collections;r=json.load(open('eng/packaging/config/config_registry.json'));\
print(collections.Counter('/'.join(x['doc'].split('/')[:3]) for x in r['plugin_knobs']))"
ls -d docs/detail/*/            # 仅 anchors / infrastructure / registry

# —— B5/§5.1：内容锚 D4 值锁被推翻（在 /tmp 副本内调用纯函数，不跑任何门）
mkdir -p /tmp/ce && cp eng/tests/config/cfg_anchors.py /tmp/ce/
python3 - <<'PY'
import json,copy,importlib.util
s=importlib.util.spec_from_file_location("a","/tmp/ce/cfg_anchors.py")
a=importlib.util.module_from_spec(s); s.loader.exec_module(a)
R="/workspace/Astro CS Database"
f=[x for x in json.load(open(R+"/eng/packaging/config/defaults.json"))["fields"]
   if x["key"]=="noise.saturation_level"][0]
sr=f["source_ref"]; doc=open(R+"/"+sr["path"],encoding="utf-8").read()
q='| `σ_bg` | `1.482602218505602·MAD(|x−median|)` | `noise_model.cpp:robust_sigma` |'
t=copy.deepcopy(sr); t["quote"]=q; t["sha256"]=a.fingerprint(q)
print("judge_value(离题句) =", a.judge_value(R,t,sr["value_text"]))
print("value_text =",repr(sr["value_text"]),"原引文中出现次数 =",a.norm_quote(sr["quote"]).count(sr["value_text"]))
PY

# —— B1：p2_upm_synthetic 不链任何 UPM 代码
sed -n '4,7p'   eng/tests/unit/p2_upm_synthetic_test.cpp   # 仅标准库，零项目头
sed -n '797,804p' eng/tests/unit/CMakeLists.txt             # :798 acsd_contracts / :804 acsd_phase2

# —— B2：.tmp 前缀永不匹配
sed -n '205,206p' lib/algorithms/fits_output/p3_output.cpp # 真实名 <out>.<pid>.tmp（无前导点）
sed -n '181,193p' eng/tests/unit/p3_output_test.cpp         # 找 ".acsd_p3_out_test."（有前导点）

# —— B6：roundtrip_tol 全仓无字面量钉死
grep -n "roundtrip_tol\|1e-8\|1e-6" eng/tests/unit/p3_projection_test.cpp   # 全在注释或取生产值

# —— B7：cpu_fallback 无 OK 正例
grep -n "LoadResult::OK" eng/tests/unit/cpu_fallback_test.cpp  # 仅 :72 出现，且是否定式

# —— B9：产物名
sed -n '34,37p' eng/tests/conformance/noop/CMakeLists.txt     # OUTPUT_NAME acsd_noop + PREFIX ""
grep -n "acsd_noop" eng/packaging/install-tree.contract.json

# —— M2：版本硬编码且不匹配
cat VERSION                                                   # 0.1.0-alpha.1
sed -n '67p' eng/tests/unit/p3_output_test.cpp                # 硬编码 0.10.0-alpha.2

# —— M3：GAIA 红基线从未构建
grep -rn "GAIA_P19_BASELINE" --include=CMakeLists.txt .       # 零命中
grep -rn "GAIA_P19_BASELINE" eng/tests/unit/CMakeLists.txt

# —— B3 补充：CFG002 机器记录本已是红
sed -n '8p' eng/tests/test_index.csv                          # FAIL,78,5 + 失败明细

# —— 悬空引用逐条核实（不得读 /tmp/acsd_g08）
for p in docs/detail/algorithms_phase1 docs/detail/algorithms_phase2 docs/detail/algorithms_phase3 \
         ENGINEERING_SPEC.md eng/packaging/verify_install_tree.py 11_MODULE_SOURCE_TEST_STANDARD.md \
         lib/astro_image_io run/RELEASE-02/trail/layout.py \
         run/RELEASE-02/L4-rebuild/upmfix_out ; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "DANGLING $p"; done
```

---

## 9. 待核清单（需他人/前台确认）

1. `p2_upm_synthetic_test` 的定位裁决：有意为之的纯算法自检（则应改名 + 改 `:141` 的 PASS 文案 + 移出 `unit/`），还是误登记为 P2-003 单测（则是覆盖空洞）。**本片最需人工裁决的一条。**
2. `docs/detail/algorithms_phase{1,2,3}/` 迁移是否已有在途修订；`config_registry.json` 的 `doc` 字段何时同步。
3. `eng/tests/sciencelint`/`traceability`/`glossary`/`results` 四个空目录：补内容、撤索引，还是删目录？
4. `docs/engineering/CONFIG_CONTRACT.md §5:88`（取消须写 incomplete manifest）与 `test_cli004` docstring `:22-26`（FIX-406 后不落 manifest）**冲突未裁决**；`cancelled` 一词在 `CLI_PROTOCOL_V1.md` 中零出现，FIX-406 未见正本。
5. `roundtrip_tol_px` 的 1e-8/1e-6 是否应有一份**仓内字面量钉死**（现在完全依赖生产表自评）。
6. `spatial_calib.json` 中 frame 3 四个指标同时为精确 `0.0`（`bg_before/bg_after/absorbed_pct/star_err_pct`）是否为空掩膜静默失败；若是，该表整行作废重跑。
7. `run/RELEASE-02/trail/layout.py`（`leaf_to_fits` / `SPAN`）已全仓缺失 —— `mult_test3.py` 与 `controls_rank1.py` 的索引约定谁对，无法定案。
8. `docs/engineering/CONFIG_CONTRACT.md:100` 冒号后的**裸从句**（「机器门断言此类旋钮只出现在 `runtime_policy` 面（phase_config 属性面零命中）」）**逐字存在且无引号对**，是主项不是脚注；该声称对 47 个真实块属性中的 17 个不成立。已按「裸从句」口径纳入发现。