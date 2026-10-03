# 审稿-P1 · ENG-tests-009（第 1 遍 · 对抗审稿）

- 片号：`ENG-tests-009`（层 `eng/tests`，SRS-1 层内 LPT 均衡装箱）
- 仓：`/workspace/Astro CS Database`，HEAD = `850a9ede`
- 口径声明：按负责人裁定，**判据代码不构成正确性证据**。本报告所有结论来自对原文的阅读与独立重算；凡「判据绿」一律不作为「实现正确」的依据。
- 计数口径：本片以**成员文件**为单位计数；涉及判据时另注明「门实例 / 去重门 / 整改分母」哪一层（见 §4 各条）。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（权威版清单） | **47** |
| 成员总行数 | **10424** |
| **我本人完整逐行读完** | **27 份 / 7098 行** |
| 我本人部分读（定向取证，非全文） | 2 份 / 已读 31 行（`abi004_registry_probe.c` 读 92-104；`stageD2_frames.py` 读 15-32） |
| **我本人直接覆盖率** | **7129 / 10424 = 68.4%** |
| 子代理全文覆盖 | 47 份 / 10424 行（4 个子代理，两两不重叠覆盖全片） |

**未由我本人读完的 20 份 / 3326 行（如实列出）**：

```
 352  eng/tests/monitoring/test_frozen_gate.py
 300  eng/tests/unit/p1snr/p1snr_noise_cfg_wiring_test.cpp
 263  eng/tests/validation/release02/q2_snr_smoothness/synth_q2.py
 256  eng/tests/cpu/baseline/provider_kernel_oracle_main.cpp
 246  eng/tests/unit/p2_sky/p2_sky_geometry_test.cpp
 211  eng/tests/unit/core_block_frame_test.cpp
 202  eng/tests/unit/p1_noise_test.cpp
 183  eng/tests/cli/test_fix203_promoted_keys.py
 176  eng/tests/quality/test_compare_products.py
 164  eng/tests/cli/test_parallel_queue.py
 155  eng/tests/validation/release02/q2_snr_smoothness/realdata/stageE_final.py
 150  eng/tests/abi/abi004_registry_probe.c              （部分读：13 行）
 143  eng/tests/backend/kernel_oracle_main.cpp
 108  eng/tests/backend/test_phase1_hotspot.py
 101  eng/tests/validation/release02/fix_p2b_variance_oracle/realdata_evidence.py
  79  eng/tests/validation/release02/q1_photometry_gradient/src/star_precision.py
  76  eng/tests/validation/release02/c_delta_composition/delta_vs_C.py
  64  eng/tests/validation/release02/phot_verify/run_all_pairs.py
  50  eng/tests/validation/release02/c_delta_composition/Ck_robust.py
  47  eng/tests/validation/release02/q2_snr_smoothness/realdata/stageD2_frames.py（部分读：18 行）
```

这 20 份的结论**仅来自子代理**，我没有独立重读；其中凡进入 §4 阻断/须修的条目，我都另外用只读命令做了**独立取证**（见 §7 自证段），但**没有**逐行重读全文。若负责人要求「一遍 = 本人全文重读」的严格口径，本片需补读这 3326 行后才算闭合。

---

## 2. 本片判定

### 判定：**阻断**

理由：本片同时命中「恒真门」「恒红门」「fail-open」「悬空引用」「伪引」五类，且**不是个别文件的问题，而是全片性的**——包括 2 条永久判红的门、1 条判据的期望量就是被检量本身的恒真门、1 整族因目录删除而失效的机器门、以及一个把自造字典值写成公式的**证据文件反向错述**。

### 最重的 3 条

1. **恒真门（代数恒等式型）：被检量即期望量，判据零生产接触**
   `eng/tests/contracts/test_unified_object_contract.py:372-373` —— `node_reconstruction(..., "absolute")` 直接 `return list(vals)`，即**返回落盘值本身**，不含任何重建算子。于是 `:390 node_ok` 是「列表与自身比」、`:391-392 frame_invariant` 是「同一函数在绝对分支下根本不读 `frame_snr`、两次必然相同」。`verdict()` 只要 `frame_snr != median` 就**永远返回 GREEN**。
   反例（我独立构造）：把稀疏层语义改成相对解释 `frame_snr × v/median(v)`（该类自称要防的缺陷），本文件**一条断言都不会红**。

2. **恒红门（双向体检漏的一侧）：两条门自建仓以来从未产出过有效判定**
   `test_unified_object_contract.py:431-439` 的 `AUTHORITY_DOCS` 含 `docs/detail/algorithms_phase1/07_noise_snr.md` 与 `docs/detail/algorithms_phase2/13_integration.md`；我实测**这两个目录均不存在**（`ls docs/detail/` 只有 `00_INDEX.md anchors common.md infrastructure LOG_AND_ERROR_SYSTEM.md merged_TROUBLESHOOTING.md PHASE{1,2,3}_DETAILED_DESIGN.md PRODUCT_STORAGE_FORM.md README.md registry STAR_DETECTION_IMPL_DESIGN.md UNIFIED_MODEL.md`）。`:448 assertTrue(p.is_file())` 与 `:458 read_text()` 因此使 `test_authority_docs_declare_absolute_not_relative` 与 `test_doc_drift_judge_is_live` **永久判红**。
   这是本轮「判据体检必须双向」最该查而没查的一类：现行结论「此门在防文档漂移」是错的，它从未绿过、也从未红得有意义。

3. **fail-open（被检能力整段静默消失）+ 往返自证（写进去再读出来）**
   `eng/tests/unit/calibration_integration_test.cpp:362` `trace_open()` 在未注入路径时 `return 0`，`:724 CHECK(trc >= 0)` 对 0 判绿，随后 `:937 if (tw.f)`、`:945 if (f)` 把 **T2–T12 共 12 条断言**（RT-006 重放、dll_sha256 实测、artifact_size 真实字节）整体跳过，测试仍打印 `ALL PASS` 并 `return 0`。
   叠加：`:920` 把 `call_count` 写成**字面量 1**，`:966 T8` 再读回来比 1——**恒真**，且与同文件 `:884 CHECK(ec==2)`、`:913 CHECK(ec2==4)` 的真实 execute 计数自相矛盾；`:919` 写入的 `granted_workers` 是测试自己在 `:773` 设的常量 3（不是 stub 的授予观测，`ex_acquire` `:257-262` 恒返回 0、从不真正授予或拒绝）。

---

## 3. 逐文件清单（我本人读完的 27 份）

| # | 文件 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `unit/calibration_integration_test.cpp` (982) | 全文 | 12 条 trace 断言可静默消失 `:362/:724/:937/:945`；`call_count` 字面量 1 自证 `:920`→`:966`；`granted_workers` 写常量 `:920`；SHA-256「独立 oracle」只与自己比 `:726-729`，无 KAT（对照姊妹件 `cosmetic_integration_test.cpp:229-234,746` 有 `"abc"→ba7816bf` 锚）；`span_find(g, g+400, …)` 越界读 `:615`；C6 `ex.max_workers==3` 与自设常量比 `:845`；C9 往返自证 `:861`；`acquire_fail` 死旋钮 `:253` | 阻断 |
| 2 | `contracts/test_unified_object_contract.py` (868) | 全文 | 恒真 verdict `:372-398`；恒红两门 `:431-439/:448/:458`；`:455-462` 自造注入串再在内存里 grep；`:46` 声称「逐字照抄 UNIFIED_MODEL §2」但全文件从不打开该文档（我核实 `docs/detail/UNIFIED_MODEL.md` 存在且可读）；`:823` 损坏字面量 `"...TRACEABILITY_SPEC.mdTRACEABILITY_SPEC §9"`；`:232-233` 正例缺失时静默拿 `signal.example.json` 顶替；`:255` 或字面量空字典恒真 | 阻断 |
| 3 | `cli/test_phase123_pipeline.py` (783) | 全文 | `:91-94` 三处 CMake 行号伪引（我实测：`acsd_common` 在 `CMakeLists.txt:530`、`acsd_aio` 在 `:618`、`acsd_hips` 在 `:646`，注释写 378-381/449-459/480-491，**三处全错**）；`:308-320` `_complete_manifests` 的 `except Exception: pass`，负例三处（`:424/:455/:527`）用它断言「不得写 complete」——CLI 若写出语法非法的 complete manifest 会被静默跳过⇒假绿；`:686` `"acsd::" in s` 把非 `acsd::` 缺失符号滤掉；`:774-779` 通过时零断言且无负控；`:519` `sleep(8.0)` 时序竞态；`:344` 小节标题称「校准数值 Oracle」但 `:374-378` 已委派 | 须修 |
| 4 | `unit/p3_export_stream_prod_test.cpp` (586) | 全文 | **判据⑤用常量平面** `:106-107`（`kSigVal=100.0f`，`const_px` 每像素同值）⇒ 子块顺序/索引错误的字节流完全相同，该门对整类子块编排缺陷**恒绿**；`:412 canon_stream==canon_frame` 在 writer 停止输出 `canonical_sha256` 时两侧同为 `""` ⇒ 空真通过（`:406` 只校验了 `sha_*` 非空）；`:248-266` 注释称「卡其余部分保留」，代码 `out.replace(off,80,kw+72个'#')` **抹掉整卡**（我本人发现，注释与实现不符）；`:17` 引 `eng/tools/arch/check_p3_export_stream_prod.py` —— 我实测该文件**不在现行树**（`eng/tools/arch/` 只有 `cmake_graph.py`、`gen_build_graph_doc.py`、`README.md`、`__pycache__`），仅存于 `run/FINAL-07*/` 归档；`README.md:13` 仍把它登记为现存工具 | 阻断 |
| 5 | `cli/test_fix406_sigterm_cancel.py` (519) | 全文 | **缓存目录名自相矛盾**（我本人核对）：`_build_fixtures:121` 建 `<root>/p1_1`、`<root>/p1_2`，而 `_FixtureCache.get:231-232` 缓存分支读 `<env>/p1a/light_1`、`<env>/p1b/light_2` ⇒ 文档 `:44` 承诺的 `ACSD_FIX406_FIXTURES` 复用路径**必然指向不存在目录**；`_annotate_p3_input:191-202` 从生产诊断 `expected '…'` 里**回灌**夹具 BUNIT ⇒ 夹具跟着生产门走，检出不了 BUNIT 守卫回归；`:391-392` 前序失败即 `skipTest`；`:358` 全文子串搜 `"cancel"`；`:263-264` `except OSError: pass`；`:297` 固定 `sleep(1.2)` 时序竞态 | 阻断 |
| 6 | `unit/p1wcs/p1wcs_std_f1_bridge_cross.py` (467) | 全文 | `:74-75` `repo_root()` 判 `(parent/"tests").is_dir()` —— 我实测仓根**只有 `eng/tests`，无 `tests/`** ⇒ 恒返 `None`；`:126` 找 `eng/tests/unit/p3wcs` 下 `p3_wcs_test`，而该目标已于 `CMakeLists.txt:830` 注释所述迁至 `lib/algorithms/projection/tests/p3wcs/` ⇒ 无 env 时必抛 `EnvError`；`:15` 文档串引 `eng/tests/unit/p3_wcs_test.cpp`（我实测**已不存在**）；`:246` 阈值命名误导；`:143-144` 把任何非零 rc 一律归为 `EnvError`（产品缺陷被报成「环境缺失」rc=2） | 须修 |
| 7 | `unit/mon002_gate_test.cpp` (419) | 全文 | `:6` 规格依据 `04_TASK_SPECIFICATIONS §MON-002(v6_1_rework)` —— 我实测**全仓不存在**；`:357` 引证据 `run/release-rescue/real-chain-fix/cli/F78_t4_green/alloc_report.json` —— 我实测**不存在**（数字被硬编码，测试不读该档）；`:222 CHECK(acsd::RESOURCE == 10)` 只比头常量与字面量，从不验证「门失败确实映射该退出码」；`:212/:215/:218` 2–4 字符子串门；`:189` 弱否定断言；`:299/:400` 写固定相对路径 | 须修 |
| 8 | `unit/p1wcs/p1wcs_astropy_cross.py` (395) | 全文 | **空 fixtures 恒绿**（我本人核对）：`:286 all_ok=True` 起，`:287 for fx in data["fixtures"]` 无长度守卫 ⇒ 导出空数组即打印 PASS、rc=0（姊妹件 `:453-457` 有 `n_cells!=18` 守卫，**同族一个有、一个没有**）；**证据反向错述**（我本人发现）：`:296` 计算 `crpix + 1.0`，而 `:300` 写进落盘 JSON 的 `report["semantic_bridge"]` 却是「bridged via astropy **crpix'=crpix-1**」——**符号相反**，证据文件记录的操作与代码执行的操作相反；`:79-88` `repo_root()` 同 6 号恒返 `None`，而 `:241-245` 的守卫在 `root is None` 时恰好把 tempfile 兜底判为合规 ⇒ **守卫与失效同向失效**；`:337` `rt_max` 算出但**不参与** `:339` 的 `ok`；`:16-17` 声明的判据 (c)「与 onestep 表一致」全文件不存在 | 阻断 |
| 9 | `integration/p3_export/p3_export_oracle.py` (376) | 全文 | `:338-343` `except Exception: astropy_ok=False` → `:363-364` 只打印 `ORACLE_ASTROPY_UNAVAILABLE … not a failure` 并**照常 return 0** ⇒ 第三方独立复核整块可静默消失而门仍绿；`:228` `allof = pschema.get("allOf",[])` **读进来后再未使用**，`:301-305` 的规则是手抄 ⇒ 改 schema 的 `pixel_area_power` 本 oracle 毫无察觉；`:6` 自称「用生产 schema 的 required/allOf 反查」为伪；`:316` 条件断言 k_corr **等于** 1，失败文案却写「k_corr != 1」（极性反了）；`:18 EXCLUDE` 死常量；`:231` 的 `"quadratic_law"` 载入后全文未用；像素值比对 `:289` 的期望来自 C++ 侧 `expected.json`（非独立） | 须修 |
| 10 | `realdata/test_match_plan.py` (309) | 全文 | **空测试体**（我本人核对 `:242-246`）：`bucket = t1_masters.get("T1"); if bucket is not None: assert …` ⇒ `None` 时**零断言通过**；**自证**（`:258-265`）：`plan` 是测试现场写的字面量，断言的也是它，`empty(PASS)` 从不由生产产出 ⇒ 类注释 `:237` 宣称的结论**零证据**；`:255` 或空字典恒真；`:98` 该参数化下 `rec["flat"]["filter"]` 与 `filt` 取自同一字面量集合 ⇒ `f(x)==f(x)`（真正的 OIII/Oiii 覆盖在 `:113-114`）；`:268-269` 固定 `$TMPDIR/acsd_real000_t1` 并发不安全 | 阻断 |
| 11 | `unit/p1_cal/cal_oracle.hpp` (135) | 全文 | `dense_jcj:25-35` 用显式 4×4 矩阵，生产用闭式标量 ⇒ **结构异构，是真判据**（我采纳子代理的证伪）。`analytic_jacobian:38-43` 只覆盖 4 分量 J 的单一形态，未覆盖生产 `calibration_covariance.cpp:325-351` 的 `same_bias_master`/`uses_bias_light`/`uses_bias_dark`/`flat_floor`/`!gain_ok` 分支。`:14 #include <array>` 死 include | 通过（有覆盖缺口） |
| 12 | `unit/p1_psfw/p1psfw_tests_oracle.cpp` (129) | 全文 | `:69-78`（生产快路径 vs oracle 稠密 long double）、`:86-93`（逐 trial GLS 1e-9）、`:118-121`（MC 5%）是真判据；`:123-127` 负向 mutation 有判别力。`:64` 注释的 SE≈0.022 与实测量级不符，容差相对量宽 | 通过 |
| 13 | `unit/rt003_context_test.cpp` (119) | 全文 | **伪宣称**（我实测）：`:5`「本测试在 TSan 下运行（`-fsanitize=thread`）」，但 `eng/tests/unit/CMakeLists.txt:134-137` 的注册**无任何 sanitizer 标志**、无独立 TSan target；全仓 `-fsanitize=thread` 只出现在无关的 `aio/mutations/tmp_path_tsan_negative.py` 与 `executor_provider_race_test.cpp:2-3` 的注释里（项目 TSan 属手工重配）。`:100 (void)ctx.cancelled();` 并发读结果被丢弃。`:106 cancels.load()==2` 是测试自控分支的恒真 | 须修 |
| 14 | `unit/p15_artifact_collect_test.cpp` (111) | 全文 | `:62-67` 回归锁有判别力（旧 `res` 过滤）；`:87-98` 畸形输入跳过是**把「静默跳过」锁成合格**，而文件头 `:5-6` 声称要根除的正是「artifacts 恒空而 status 仍 complete」；`:66-67` 在 `:65` 已失败时仍执行 `paths[0]`（同文件 `:56/:83/:97` 都写了 `!paths.empty()` 短路，**唯独此处漏写**）⇒ 可能 UB 而非干净 FAIL；`:74/:84` `std::system` 固定 `/tmp` 路径并发互踩；`:74/:84` 用 `std::system` 但未 include `<cstdlib>` | 须修 |
| 15 | `unit/aio/oracle/aio_oracle.py` (25) | 全文 | 25 行驱动干净：`:11-13` 参数不符 `return 2`；`:14-19` 真判据；`:24-25` `__main__` 守卫 + `SystemExit`。我实测 `aio_oracle_lib.py`（19803 B，git 跟踪）存在 ⇒ **无悬空引用**。已注册于 `eng/tests/unit/aio/CMakeLists.txt:53`（无守卫） | 通过 |
| 16 | `unit/p1wcs/p1wcs_tests_perf.cpp` (97) | 全文 | `iter_trans_solve` 第 5 参是**多项式阶数不是线程数**（我实测 `lib/algorithms/platesolve/cpp/ipv/include/ipv_itertrans.h:94-99`），线程只经 `omp_set_num_threads` ⇒ 我**否决**了「线程参数被写死导致恒绿」的假设。但：`:86 perf_parity_2t` 只有 `<4.0` 上界、**无下界**，而 `:6-7` 注释自称「并行不倒退」；`:11-12` 称「不设绝对阈值」而 `:86-88` 硬编码 4.0/4.0/0.25；`:50-60` 取 3 次 min，系统性把比值压低；`:3` 引的两个「先例」文件我实测全仓不存在 | 须修 |
| 17 | `unit/p3_rsmp/CMakeLists.txt` (65) | 全文 | **fail-open**（我本人核对 `:59-64`）：`find_program(PYTHON3_EXECUTABLE python3)` + `if(...)` ⇒ python3 缺失时 `p3_rsmp_mutation_driver` **在 ctest 面里根本不存在**，ctest 退出 0，20 条注入一条不跑；而 `:43-48` 的注释白纸黑字说打开它的理由正是「驱动根本没有执行单元」。`:26/:46/:48` 引 `eng/ci/checks.json`、`eng/ci/ctest_face.py`、`check_mutation_gates.py` —— 我实测 **`eng/ci/` 已于 `e5f589a6` 物理删除**。`:52` 称「21 变体 × 3 用例 = 63 次」，我实测 `MUTATIONS` 实为 **20** | 阻断 |
| 18 | `unit/p1snr/CMakeLists.txt` (89) | 全文 | **孤儿注释块**（我本人核对 `:38-46`）：P14-N-08/N-09 的 A/B/C 三条判据描述详尽，但 `:47` 之后**没有任何 `add_executable`/`add_test`** ⇒ 三条判据无注册载体。`:89` 只有最后一个测试带 `WORKING_DIRECTORY`+`TIMEOUT 900`，前 9 个两者皆无。`:58/:68/:84` 路径含双斜杠 | 须修 |
| 19 | `integration/p1_integrate/CMakeLists.txt` (41) | 全文 | `:4` 广告独立构建；`:16-17` 拉入 `phase1_product`，而我实测 `lib/algorithms/integration/phase1_product/CMakeLists.txt:57` `target_link_libraries(… PUBLIC acsd_platform_math)` **无 `if(TARGET)` 守卫**，`acsd_platform_math` 全仓唯一定义在根 `CMakeLists.txt:329` ⇒ 独立 configure 下会被展开成 `-lacsd_platform_math`。**仓内已有同族修复与实测记录**：`phase2_integrate/CMakeLists.txt:112-115` 的 `foreach()+if(TARGET)` 守卫，注释逐字记录了同一失败模式 ⇒ FINAL-07 修复漏了 phase1。无 fail-open（`:25` python3 裸字符串会响亮地红，正确） | 须修 |
| 20 | `api/test_p1_api.py` (91) | 全文 | **悬空引用（我本人核对）**：`:6 DOC = REPO/docs/api/PHASE1_API_V1.md`，我实测 **`docs/api/` 不存在**（权威件已迁 `docs/engineering/PHASE1_API_V1.md`）⇒ `:40 setUpClass` 必 `FileNotFoundError`，**5 个 test 全 error**。`:2` docstring 声称「doc-symbol-signature checker 合同」，而 `:48` 只是 `assertIn(fn, text)` 子串包含，**全文件零签名/参数比对**。`:57` 按「test ID 为 `—`」**预先丢弃不合规行**，`:60` 永远看不到它们；`:61 any(k in l for k in ("yes","no"))` 中 `"no"` 是 `node`/`not`/`none` 的子串 | 阻断 |
| 21 | `monitoring/test_log_contract.py` (199) | 全文 | **恒真门 + 自相矛盾**（我本人核对 `:194-195`）：类名 `TestNoRawTestdata`、docstring A7「无 testdata 依赖」、`:190` 注释「不引用仓库 testdata/ 目录」，而 `:194` **恰恰引用** `REPO/testdata/index.json` 并在 `:195` 断言它存在，且 `or td.parent.exists()` 让 `index.json` 缺失也通过。header `:8/:12/:13` 承诺的 A1 schema **负测**、A5 错误 **正测**、A6 **schema 检查和** **全文件零覆盖**（`:63-70` 只有两个负测；`:63` 起的 A6 只覆盖行大小上限）。`TestCheckerCli:172-182` 从不打开文档且断言是 `:55-61` 的真子集 ⇒ 零新增覆盖。`:27/:29` 导入未用 | 须修 |
| 22 | `cli/test_cli001_vpi.py` (166) | 全文 | **伪引（我本人核实）**：`:4-5` 称「权威: docs/ACSD_DESIGN §6.2（唯一七行命令树）、§6.3（stdout/退出码）」；我实测 `docs/ACSD_DESIGN.md:370` = `### 6.2 流程`（mermaid 流程图，**不含命令树**）、`:383` = `### 6.3 投影算法`、命令树实为 `:397 ### 7.1`、机器输出与退出码实为 `:419 ### 7.2` ⇒ **两处章节均指错**。`:39-47 HELP_LINES` 是**手抄字面量**，测试从不打开 §6.2/§7.1 ⇒ 文档改了不会红。`:131-138` 测「`-y` 不得越过预检」，但**真正的旁路 `-force` 从未被测**。`:117` 快照只取 `self.out`，CLI 往 `run/` 写东西全绿 | 阻断 |
| 23 | `validation/release02/phot_verify/radius_test.py` (32) | 全文 | docstring `:2-3` 声称 "**bounds** the seeing-induced systematic"，但全文**只有 print**、无 assert/无 `sys.exit`/无半径间比较 ⇒ 宣称无载体，恒绿。`:23-30` 空匹配时 `z` 空 → `:29 k.all()` 对空数组为 True → 跳出 → `:31 np.median(空)` 得 nan 并照常打印 | 须修 |
| 24 | `config/fixtures/negative/cpu_profile_v1_missing_required.json` (41) | 全文 | 逐键核对：顶层 7 键齐备、**唯一缺 `verdict`**，与文件名语义一致；`cli_sha256`/`commit` 长度、`fingerprint` 16 位、`block_size:0`、`oracle_status:"pass"` 均在允许域内 ⇒ **不是因无关理由非法**。红队先验被推翻 | 通过 |
| 25 | `config/fixtures/negative/mosaic_blocks_mixed_flat.phase_config.json` (16) | 全文 | `:3-11 blocks` 与 `:12-15` 平铺 `hips_paths`+`output_dir` **确实互斥**，与文件名语义一致；`blocks[0]` 具备 `name` ⇒ 判红理由正是 blocks/flat 互斥 | 通过 |
| 26 | `config/fixtures/positive/export_legacy_contract.phase_config.json` (23) | 全文 | 逐字段对 `$defs/export_config`：`output_dir`/`precision:"fp64"`/`output_mode`/`wcs` 五键（`projection:"tan"`、`center_deg:[0,0]`、`s_out_deg` 满足 `exclusiveMinimum:0`、`width/height 512` 在 `[1,20000]`）/`inputs` 均合法 | 通过 |
| 27 | `unit/fixtures/core_pipeline/valid_pipeline.json` (15) | 全文 | 逐条对生产解析器 `lib/infrastructure/scheduler/src/pipeline.cpp:60-183`：`acsd.pipeline/v1` 在白名单、`nodes`/`inputs`/`outputs` 非空且全为 `artifact:` 引用、`resources.class:"cpu_heavy"` + `parallel:true`（`:150-156` 要求 cpu_heavy 不得 parallel=false）⇒ 无键名漂移 | 通过 |

---

## 4. 发现清单

### 4.1 阻断（6 条）

| ID | 类型 | 位置 | 一句话 |
|---|---|---|---|
| **B-1** | 恒真门（代数恒等式型） | `test_unified_object_contract.py:372-373`（波及 `:390/:391-392/:400-413`） | `node_reconstruction(...,"absolute")` 就是 `return list(vals)`；`node_ok`/`frame_invariant` 构造性恒真，`verdict()` 只要 `frame_snr != median` 永远 GREEN。**判据的期望量就是被检量本身，且零生产代码接触。** |
| **B-2** | 恒红门（双向体检漏侧） | `test_unified_object_contract.py:431-439` → `:448`、`:457-458` | `AUTHORITY_DOCS` 列的两个目录实测不存在，两条门**永久判红**，从未产出过有效判定。 |
| **B-3** | fail-open + 往返自证 | `calibration_integration_test.cpp:362/:724/:937/:945`；`:920`→`:966`；`:726-729` | 12 条 trace 断言可整段消失仍 `ALL PASS`；`call_count` 字面量 1 自证且与 `:884/:913` 真实计数矛盾；SHA-256「独立 oracle」只与自己比（姊妹件有 KAT，此件无）。 |
| **B-4** | 悬空引用（整族） | `api/test_p1_api.py:6`（我实测 `docs/api/` 不存在） | 权威件已迁 `docs/engineering/`，`eng/tests/api/` 整族指向已消失路径 ⇒ 5 个 test 全 error；且 `:2` 宣称的签名比对**从未实现**（`:48` 只是子串 `assertIn`）。 |
| **B-5** | 判据读的是桩（恒真） | `p3_export_stream_prod_test.cpp:106-107` + `:415-427`；`:412` | 用**每像素同值**平面做「流式 vs 整幅逐字节相同」对拍 ⇒ 子块顺序/索引错误的字节流完全相同；`:412` 在两侧同为 `""` 时空真通过。 |
| **B-6** | fail-open（测试可静默消失） | `unit/p3_rsmp/CMakeLists.txt:59-64` | `find_program`+`if(...)` ⇒ python3 缺失时变异驱动不在 ctest 面内，ctest 仍绿，20 条注入一条不跑；而 `:43-48` 说打开它的理由正是「驱动没有执行单元」。 |

### 4.2 须修（11 条）

| ID | 类型 | 位置 |
|---|---|---|
| M-1 | **证据反向错述** | `p1wcs_astropy_cross.py:296`（`crpix+1.0`）vs `:300`（写入 JSON 的「crpix'=crpix**-1**」）——落盘证据记录的操作与代码执行的**符号相反** |
| M-2 | **伪引（章节指错）** | `test_cli001_vpi.py:4-5` §6.2/§6.3 → 实为 §7.1/§7.2（我实测 `ACSD_DESIGN.md:370/383/397/419`） |
| M-3 | **伪引（行号）** | `test_phase123_pipeline.py:91-94` 三处 CMake 行号全错（我实测 530/618/646） |
| M-4 | **自证 + 空测试体** | `test_match_plan.py:242-246`（`None` 时零断言）、`:258-265`（`plan` 为自写字面量） |
| M-5 | **伪宣称（能力无载体）** | `rt003_context_test.cpp:5` 称 TSan，`CMakeLists.txt:134-137` 无任何 sanitizer 标志（我实测） |
| M-6 | **恒真门 + 自相矛盾** | `test_log_contract.py:194-195`（「不使用 raw testdata」的类反过来断言 testdata 存在，且 `or` 让 `index.json` 缺失也过） |
| M-7 | **fail-open（第三方复核消失）** | `p3_export_oracle.py:338-343`+`:363-364` astropy 不可用仍 `return 0` |
| M-8 | **伪引 + 判据读的是手抄** | `p3_export_oracle.py:6` 称用 schema `allOf`，`:228` 读入即弃、`:301-305` 手抄；`test_cli001_vpi.py:39-47` `HELP_LINES` 与权威文档无绑定 |
| M-9 | **悬空引用（已删目录）** | `p3_export_stream_prod_test.cpp:17`（`eng/tools/arch/check_p3_export_stream_prod.py` 已删，`README.md:13` 仍登记）；`p1wcs_std_f1_bridge_cross.py:15`（`eng/tests/unit/p3_wcs_test.cpp` 已迁 lib）；`:126` 搜索路径未随迁移更新 |
| M-10 | **FINAL-07 修复漏改** | `phase1_product/CMakeLists.txt:57` 缺 `if(TARGET)` 守卫（同族 `phase2_integrate:112-115` 已修且留有实测记录） |
| M-11 | **孤儿注释块 + 事实漂移** | `p1snr/CMakeLists.txt:38-46`（A/B/C 三判据无注册载体）；`:52` 「21 变体」实测为 20；`:26/:46/:48` 引已删除的 `eng/ci/`（我实测 `e5f589a6` 删除） |

### 4.3 建议（8 条）

1. `calibration_integration_test.cpp:253` `acquire_fail` 死旋钮（租借失败路径零覆盖）；`:845` `ex.max_workers==3` 与自设常量比。
2. `calibration_integration_test.cpp:610/619-650` 在**整个文件**做 `tjson_contains` 无锚定（同文件 `:613/:628` 用 `span_find` 限定段落，口径不一）。
3. `calibration_integration_test.cpp:615` `span_find(g, g+400, …)` 越界读（对比 `:631` 是有界的）。
4. `p3_export_stream_prod_test.cpp:248-266` 注释称「卡其余部分保留」，代码抹掉整卡 —— 注释与实现不符（我本人发现）。
5. `mon002_gate_test.cpp:6` 规格文件 `04_TASK_SPECIFICATIONS` 实测不存在；`:357` 引的证据档实测不存在（数字被硬编码）。
6. `test_phase123_pipeline.py:308-320` `except Exception: pass` 在**负例**方向使用 ⇒ 坏 manifest 被当「没写 complete」。
7. `p15_artifact_collect_test.cpp:66-67` 缺 `!paths.empty()` 短路（同文件其余三处都有）；`:74/:84` 缺 `<cstdlib>`。
8. `radius_test.py` / `run_all_pairs.py` / `Ck_robust.py` / `star_precision.py` / `delta_vs_C.py` / `stageD2_frames.py` / `synth_q2.py` / `realdata_evidence.py` —— 这批 validation 脚本**全部只有 print、无 assert、无退出码**（子代理一致结论），且输入 `run/RELEASE-02/**` 已随 `run/*`（`.gitignore:17`）消失；其中 `pair_results.json`、`evidence_summary.json`、`stageD2.log` **是 git 跟踪的** ⇒ 仓内提交了一批不可复现的「证据」。

### 4.4 全片横切（我本人独立取证）

- **无任何 CI 配置**：我实测 `.github/` 目录**不存在**；全仓无 `pytest.ini`/`conftest.py`/`pyproject.toml`/`setup.cfg`/`tox.ini`。
- **Python 判据面无自动红灯载体**：逐个检索 11 个 Python 测试，被 CMake 或 CI 引用的只有 `test_compare_products`；其余 10 个（`test_unified_object_contract`、`test_phase123_pipeline`、`test_frozen_gate`、`test_match_plan`、`test_log_contract`、`test_cli001_vpi`、`test_parallel_queue`、`test_fix406_sigterm_cancel`、`test_p1_api`、`test_phase1_hotspot`）**零引用** ⇒ 只能人工调用 docstring 里的 `python3 -m unittest ...`。
  （我一度用脚本自动统计「未注册」，得出 47/47 未注册，**与已核实事实矛盾**——`calibration_integration` 在 `CMakeLists.txt:1224` 有注册。判定该脚本正则失效，**结果作废**，改用上述逐项检索。）
- **C++ 侧确有注册**（我逐个读到的）：`calibration_integration:1224`、`mon002_gate:381`、`p3_export_stream_prod:72`、`rt003_context:137`、`p1wcs_std_f1_bridge_cross`（p1wcs/CMakeLists:176）、`p1wcs_astropy_cross`(:146)、`p3_export_oracle`（p3_export/CMakeLists:75）、`aio_oracle`（aio/CMakeLists:53）、`p1_*` 四个（p1_integrate/CMakeLists:27-41）、`p1snr_*` 十个（p1snr/CMakeLists:23-31/61/71/87）。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| **R-1** | 把 `node_reconstruction(pts, fs, "absolute")` 的实现改成恒等 `return list(vals)`（**已经是这样**），再把正例 `sparse_snr_layer.example.json` 的控制点值改成任意数 | 期望「绝对 vs 相对解释」数值判据转红 | **未推翻** ⇒ `test_absolute_reconstruction_is_green_and_relative_is_red`、`test_frame_scalar_does_not_change_reconstruction`、`test_degenerate_point_is_not_evidence` 对数据缺陷零判别力。`verdict()` 的 GREEN 只由 `frame_snr != median` 决定（B-1 成立） |
| **R-2** | 把 `Phase2` 稀疏层重建改成相对解释 `frame_snr × v/median(v)`（`:375` 就是这个被否决的分支） | 期望该类判红 | **未推翻** ⇒ 同 B-1：判据从不调用生产重建算子 |
| **R-3** | 从 `eng/tests/unit/CMakeLists.txt:1225` 删掉 `ACSD_CAL_INT_TRACE_OUT=...` 注入（或裸跑二进制） | 期望 T2–T12 判红 | **未推翻** ⇒ 12 条断言整体跳过，`ALL PASS` / rc=0（B-3 成立） |
| **R-4** | 把 `trace_module_call(..., call_count=1, ...)`（`:920`）改成写 `4`（真实计数）或写 `2` | 期望 T8（`:966`）判红 | **未推翻** ⇒ 字面量写、字面量读，恒真；且与 `:884 ec==2`/`:913 ec2==4` 自相矛盾 |
| **R-5** | 把 `sha256_block`（`:152-164`）K 表任意两个常数对调 | 期望 T1/T5 判红 | **未推翻** ⇒ T1（`:726-729`）只与自身第二次计算比，T5（`:962`）也与同一未验证函数比。姊妹件 `cosmetic_integration_test.cpp:229-234,746` 有 `"abc"→ba7816bf` KAT 锚，此件没有 ⇒ **可移植的修法已有先例** |
| **R-6** | 让 `p1wcs_tests apbp` 导出的 `wcs003_cross_input.json` 里 `fixtures` 为 `[]` | 期望 `p1wcs_astropy_cross.py` 判红 | **未推翻** ⇒ `:287` 循环零次、`:352` 打印 `P1WCS ASTROPY CROSS PASS`、rc=0。同族姊妹件 `:453-457` 有守卫，此件没有 |
| **R-7** | 让 `p3_export_stream_prod_test.cpp` 的 writer 停止输出 `canonical_sha256` 键 | 期望 `:412` 判红 | **未推翻** ⇒ 两侧同为 `""`，`"" == ""` 为真（B-5 后半成立） |
| **R-8** | 在子块流式路径里把第 k 个子块写到第 j 个位置（编排顺序 bug） | 期望判据⑤（逐字节相同）判红 | **未推翻** ⇒ 夹具每像素同值（`:106-107`），任意子块置换产生**完全相同**的字节流 ⇒ 该门对整类子块编排缺陷恒绿（B-5 前半成立） |
| **R-9** | 从 `wcs003_cross_input.json` 的 `fixtures[0]` 删掉所有条目 | 期望 `test_authority_docs_declare_absolute_not_relative` 判红 | **构造不成立**（该门读的是仓内 md 不是 JSON）；但 `:448` 的 `assertTrue(p.is_file())` 因两个目录不存在而**必然抛红** ⇒ 转为恒红门（B-2 成立） |
| **R-10** | 把 `eng/tests/unit/p3_rsmp/CMakeLists.txt:60` 的 `if(PYTHON3_EXECUTABLE)` 条件改为恒假（如 PATH 去掉 python3） | 期望 ctest 判红 | **未推翻** ⇒ `add_test` 不执行、`p3_rsmp_mutation_driver` 从 ctest 面消失、ctest 退出 0。且 `:26/:46/:48` 依赖的 `eng/ci/` 检查已随 `e5f589a6` 删除 ⇒ **已无任何机器面能发现这件事**（B-6 成立） |
| **R-11** | 跑 `cmake -S eng/tests/integration/p1_integrate -B build/x` | 期望独立构建成功（`:4` 广告） | **未执行**（受只读约束）⇒ 但我已核实 `phase1_product/CMakeLists.txt:57` 无守卫、目标唯一定义在根 `:329`、同族 `phase2_integrate:112-115` 有守卫且注释逐字记录同一失败 ⇒ **列为待前台执行项**，不冒充已证 |
| **R-12** | 把 `cpu_profile_v1_missing_required.json` 里某个 sha 改成非法长度 | 期望夹具因「缺 `verdict`」判红 | **构造不成立** ⇒ 我逐键核对后该夹具确实只缺 `verdict` 且其余字段合法 ⇒ **红队先验被推翻**，判为干净 |

---

## 6. 盲复算

方法：先遮住子代理结论与既有 `审稿-RR*/R2-*/R3-*/P1-*`，只用本片原文 + 只读取证命令重推，再比对。

| 层 | 独立取证得到 | 与既有结论比 | 判定 |
|---|---|---|---|
| 恒真门 | 6 条（B-1、B-3 后半、B-5、R-6、R-7、R-8） | 一致 | **判一致** |
| 恒红门 | 1 条（B-2）+ `p3_rsmp:59-64` 的「消失」侧 | 既有登记表未见此条 | **本片新发现（偏严的一侧）** |
| fail-open / fail-closed 伪装 | 6 处（`p3_export_oracle:338`、`calibration_int:362`、`p3_rsmp:60`、`test_log_contract:195`、`test_match_plan:244`、`test_fix406:391`） | 一致 | **判一致** |
| 悬空引用 | 4 处（`docs/api/`、`eng/tools/arch/check_p3…py`、`eng/tests/unit/p3_wcs_test.cpp`、`eng/ci/`） | 既有登记表只记了「整目录已删」的一般形态 | **偏严**：本片把它们逐条定位到 `文件:行` |
| 伪引 | 5 处（§6.2/§6.3 章节、三处 CMake 行号、`UNIFIED_MODEL §2`、`module_adapters` 行号） | 一致 | **判一致** |
| 全片横切 | 无 CI、无 pytest 配置、Python 判据面零自动红灯载体 | 既有登记表未覆盖 | **本片新发现** |

**口径**：以上均为**门实例**层，未做去重；`eng/tests/` 全层的去重门与整改分母不在本片授权范围内。

---

## 7. 子代理派发记录

派发 **4 个**（`subagent`，两两文件集不重叠，合覆盖 47/47）：

| 子代理 | 文件范围 | 份数/行数 | 状态 |
|---|---|---|---|
| A（`f8586721`） | 本片第 1–12 份（大头 12 份） | 12 / 6356 | 已交回完整报告 |
| B（`6c62bec3`） | 第 13–24 份 | 12 / 2371 | 已交回完整报告 |
| C（`ddf920ca`） | 第 25–36 份 | 12 / 1278 | 已交回完整报告 |
| D（`2e47fc44`） | 第 37–47 份 | 11 / 419 | 已交回完整报告 |

（本人注：A 的任务书里我误把 12 份写成「约 4470 行」，实际 6356 行；A 自行更正。）

### 逐条复核：采信 / 否决

**采信并由我独立复现（15 条）**——每条我都用只读命令或本人重读复核过：

1. `docs/detail/algorithms_phase1|algorithms_phase2` 不存在 → 我 `ls docs/detail/` 确认（B-2）
2. `docs/api/` 不存在、`docs/engineering/PHASE1_API_V1.md` 存在 → 我 `ls` 确认（B-4）
3. `eng/tools/arch/check_p3_export_stream_prod.py` 不存在 → 我 `ls eng/tools/arch/` 确认（M-9）
4. `eng/tests/unit/p3_wcs_test.cpp` 不存在、已迁 lib → 我 `ls` 确认（M-9）
5. `ACSD_DESIGN.md` §6.2=流程(:370)/§6.3=投影算法(:383)/§7.1 命令树(:397)/§7.2 退出码(:419) → 我 grep 标题确认（M-2）
6. 「键名一律以命令行实际认的键为准」「唯一声明」在 ACSD_DESIGN 零命中 → 我 `grep -c` 得 0（M-3 邻项）
7. 根 `CMakeLists.txt` 三目标实际在 530/618/646 → 我 grep 确认（M-3）
8. `eng/ci/` 已随 `e5f589a6` 删除 → 我 `ls -d eng/ci` + `git log -- <file>` 确认（B-6/M-11）
9. `run_mutations.py` 的 `MUTATIONS` = 20（非注释所称 21） → 我用 `ast` 解析确认（M-11）
10. `phase1_product/CMakeLists.txt:57` 无 `if(TARGET)` 守卫、目标唯一定义在根 `:329`、同族 phase2 已修 → 我 `grep`+`sed` 确认（M-10）
11. `psfsw.h:308-314` 三个字段是硬编码成员默认值、`propagate_covariance`（`psfsw.cpp:619-641`）从不赋值 → 我 `sed` 读原文确认（C 的 ①，我采信）
12. `abi004_registry_probe.c:97 if (rc != 0) return 0;` + `:99-100` 丢弃返回值致 `issues` 恒 0 → 我 `sed -n '92,104p'` 确认
13. `stageD2_frames.py:20` 键集只取 `oa[:50]` 的 A 侧、`:24` 却比全量 → 我 `sed -n '15,32p'` 确认
14. `test_match_plan.py:242-246` 空测试体 / `:258-265` 自证 → 我 `sed -n '238,266p'` 确认（M-4）
15. `stageF_assemble.py:83` 覆写 `realdata_seam.json`、`:72` `if False else None` 死代码 → 我 `grep` 确认（覆盖 stageE 被取代的论断）

**否决 / 降级的（7 条）**——我复核后**不成立或需改写**，特记以免误伤：

1. ❌ **「`iter_trans_solve` 第 5 参写死 1 导致线程数不生效」** —— 否决。我查 `lib/algorithms/platesolve/cpp/ipv/include/ipv_itertrans.h:94-99`，第 5 参是 `int order`（多项式阶数），线程只经 `omp_set_num_threads(nthreads)`（测试 `:52`）。**结构上并非恒真**，我把它改写为「`:86` 无下界 + `:11-12` 注释与阈值矛盾 + min-of-3 偏置」。
2. ❌ **「`phases` 字段两个门形状矛盾（`[1]` vs `[{"normalize":1}]`）」** —— 否决，是我自己的误读。`{"normalize":1,...}[phase]` 求值得到整数，`test_fix406:355-357` 断言的是 `[1]`，与生产 `commands.cpp:2126-2127`（`ph.is_number_integer()`）一致。
3. ❌ **「`test_match_plan.py:98` 与 A 的『f(x)==f(x)』不完全成立」** —— 部分否决。该参数化下 `T4_MASTERS` 用 `"OIII"`、参数也用 `"OIII"`，确为同字面量 ⇒ 恒真成立；但真正的 OIII/Oiii 大小写覆盖在 `:113-114`。我按「局部恒真、另有覆盖」记录。
4. ⚠️ **「`p3_export_stream_prod_test.cpp:25-26` 头注『integrity sha256 == canonical sha256 == 逐字节比对』与实现矛盾」** —— 降级为「建议」。确系头注强于实现，但 `:408-411` 的注释已说明原因（CHECKSUM 含写入时刻），且 `:426` 的 DATASUM 比对补上了数据面证据 ⇒ 属表述过强，不是判据缺陷。
5. ⚠️ **「`calibration_integration_test.cpp:893` `err` 未 `err_init` 可能继承残留 detail_code」** —— 降级为「未证实」。`:576` 那次 describe 失败后 `:673` 首次 validate 前确无重置，但 B1 断言的是 `ACSD_CAL_ECODE_PARAM_MISSING` 且当前实现不返回该码，故不足以判红。
6. ⚠️ **「7 个判据完全未进 CTest」** —— 部分否决。Python 侧确无 CI 载体（我用 11 项逐个检索确认，见 §4.4），但 C++ 侧我逐个读到的 CMake 注册（`calibration_integration:1224` 等）说明子代理的「未注册」说法若被理解成「整片无注册面」则**过宽**。
7. ⚠️ **子代理的自动注册统计** —— 我自己跑过一版「47/47 未注册」的脚本，**与已核实事实直接矛盾**（`calibration_integration` 有注册），判定正则失效并**作废该结果**，改用逐项检索。子代理同类脚本结论一律不采信为独立证据。

**未证实项（承上，不冒充结论）**：`p2_sky_geometry_test.cpp:182` 单指向分支是否同源赋值；`compare_products.py` 的 `tolerance_rationale` 在非 tolerance 模式是否存在；`abi004_registry_probe.c:27` 欠对齐是否真被触发；`test_cli001_vpi.py:14-15` 所引两个 `test_01` 方法名是否精确存在；`p1wcs_std_f1_bridge_cross.py:143-144` 把产品缺陷报成 rc=2 的实际外层消费方式。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# S0 基线
git -c core.quotepath=false log --oneline -1          # 850a9ede

# S1 片清单与成员清单（权威版）
sed -n '1557,1612p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"

# ---- B-2 恒红门：两个权威文档目录不存在 ----
ls docs/detail/
for f in docs/detail/algorithms_phase1/07_noise_snr.md \
         docs/detail/algorithms_phase2/13_integration.md; do [ -e "$f" ] && echo EXIST "$f" || echo MISSING "$f"; done

# ---- B-1 恒真门：absolute 分支就是 return list(vals) ----
sed -n '367,398p' eng/tests/contracts/test_unified_object_contract.py

# ---- B-3 fail-open：trace 门可整段消失 ----
sed -n '360,367p;723,725p;936,946p' eng/tests/unit/calibration_integration_test.cpp
sed -n '920,922p;961,968p' eng/tests/unit/calibration_integration_test.cpp
grep -n "ACSD_CAL_INT_TRACE_OUT" eng/tests/unit/CMakeLists.txt    # 1225 注入点
# 姊妹件有 SHA-256 已知向量锚，此件没有：
grep -n "abc\|ba7816bf\|已知向量" eng/tests/unit/cosmetic_integration_test.cpp
grep -c "abc\|ba7816bf" eng/tests/unit/calibration_integration_test.cpp

# ---- B-4 悬空引用：docs/api/ 整族失效 ----
ls -d docs/api 2>&1; ls docs/engineering/PHASE1_API_V1.md
grep -n "docs.*api" eng/tests/api/*.py

# ---- B-5 常量平面 + 空真通过 ----
sed -n '105,107p;405,427p' eng/tests/unit/p3_export_stream_prod_test.cpp
grep -n "check_p3_export_stream_prod" eng/tools/arch/README.md   # README 仍登记已删工具
ls eng/tools/arch/

# ---- B-6 fail-open：变异驱动可静默消失 + eng/ci 已删 ----
sed -n '43,48p;52p;57,65p' eng/tests/unit/p3_rsmp/CMakeLists.txt
ls -d eng/ci 2>&1
git -c core.quotepath=false log --oneline -1 -- eng/ci/checks.json
python3 -c "import ast;t=ast.parse(open('eng/tests/unit/p3_rsmp/run_mutations.py',encoding='utf-8').read());print([len(n.value.elts) for n in ast.walk(t) if isinstance(n,ast.Assign) and any(getattr(x,'id','')=='MUTATIONS' for x in n.targets)])"

# ---- M-1 证据反向错述：代码 +1，JSON 写 -1 ----
sed -n '293,302p' eng/tests/unit/p1wcs/p1wcs_astropy_cross.py
sed -n '74,78p;291,295p' eng/tests/unit/p1wcs/p1wcs_astropy_cross.py   # repo_root 判 tests/
ls -d tests 2>&1; ls -d eng/tests
sed -n '453,457p' eng/tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py  # 同族有守卫

# ---- M-2 伪引：§6.2/§6.3 指错 ----
grep -n "^#\{2,3\} " docs/ACSD_DESIGN.md | grep -E "6\.|7\."
grep -c "键名一律以命令行实际认的键为准\|唯一声明" docs/ACSD_DESIGN.md

# ---- M-3 伪引：三处 CMake 行号 ----
grep -n "add_library(acsd_common\|add_library(acsd_aio\|add_library(acsd_hips" CMakeLists.txt
sed -n '91,94p' eng/tests/cli/test_phase123_pipeline.py

# ---- M-4 空测试体 + 自证 ----
sed -n '238,266p' eng/tests/realdata/test_match_plan.py

# ---- M-5 TSan 伪宣称 ----
sed -n '1,6p' eng/tests/unit/rt003_context_test.cpp
sed -n '134,137p' eng/tests/unit/CMakeLists.txt
grep -rn "fsanitize=thread" eng lib CMakeLists.txt 2>/dev/null

# ---- M-6 恒真 + 自相矛盾 ----
sed -n '185,196p' eng/tests/monitoring/test_log_contract.py

# ---- M-7 第三方复核静默消失 ----
sed -n '337,364p' eng/tests/integration/p3_export/p3_export_oracle.py
sed -n '224,231p;298,310p' eng/tests/integration/p3_export/p3_export_oracle.py

# ---- M-10 FINAL-07 修复漏改 ----
sed -n '55,59p' lib/algorithms/integration/phase1_product/CMakeLists.txt
grep -n "add_library(acsd_platform_math" CMakeLists.txt
sed -n '112,115p' lib/algorithms/integration/phase2_integrate/CMakeLists.txt

# ---- M-11 孤儿注释块 + 变体数漂移 ----
sed -n '38,53p' eng/tests/unit/p1snr/CMakeLists.txt

# ---- 恒真门：psfsw 三个字段是硬编码默认值且从不被赋值 ----
sed -n '308,314p' lib/algorithms/photometry/include/acsd/psfsw.h
sed -n '619,641p' lib/algorithms/photometry/cpp/src/psfsw.cpp

# ---- fail-open：abi004 open 失败返回 0；check 返回值被丢弃 ----
sed -n '95,105p' eng/tests/abi/abi004_registry_probe.c

# ---- 键集只采 A 侧前 50 条，却比全量 ----
sed -n '15,32p' eng/tests/validation/release02/q2_snr_smoothness/realdata/stageD2_frames.py

# ---- 修复方向失败而被锁成 PASS：缓存目录名不一致 ----
sed -n '119,141p;229,234p' eng/tests/cli/test_fix406_sigterm_cancel.py

# ---- 夹具自行适应生产门（自愈面） ----
sed -n '180,205p' eng/tests/cli/test_fix406_sigterm_cancel.py

# ---- §4.4 横切：无 CI、无 pytest 配置、Python 判据零引用 ----
ls -d .github 2>&1
ls pytest.ini conftest.py pyproject.toml setup.cfg tox.ini 2>&1
for n in test_unified_object_contract test_phase123_pipeline test_frozen_gate test_match_plan \
         test_log_contract test_cli001_vpi test_parallel_queue test_fix406_sigterm_cancel \
         test_p1_api test_phase1_hotspot; do
  echo -n "$n -> "
  grep -rl "$n" .github/ CMakeLists.txt eng/tests/unit/CMakeLists.txt eng/tests/*/*/CMakeLists.txt 2>/dev/null || echo NONE
done

# ---- 覆盖统计（复算本报告 §1） ----
wc -l $(cat /tmp/slice009.txt 2>/dev/null) 2>/dev/null | tail -1
```

**注**：R-11（`cmake -S eng/tests/integration/p1_integrate -B build/x`）与任何需要编译/ctest/pytest 的验证，**本轮一律未执行**，列为待前台执行项。

---

## 9. 待负责人裁决（3 项）

1. **`docs/api → docs/engineering` 的迁移是否有意？** 若是，`eng/tests/api/` 整族 + `PHASE1_API_V1.md:50` 声明的「机器门含签名一致性」需同批订正；且 `:2` 宣称的签名比对从未实现，需补签名解析（AGENTS.md §3：代码与文档冲突以文档为准，但此处是文档内部引用与代码同时失配）。
2. **本片 20 份 / 3326 行未由本人全文重读**，是否需要补读后才计入「一遍完成」口径。
3. **validation 域脚本（8 个只有 print、无退出码、输入已随 `run/*` 消失、但产物被 git 跟踪）的定位**：若为历史快照，应与生成脚本的关系明确标注「不可复现」；若要继续当判据，须把输入搬进跟踪区（AGENTS.md §6）。
