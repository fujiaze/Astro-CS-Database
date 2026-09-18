# W2-TEST1 审计报告 —— 第二波·文件级：tests/unit · tests/api · tests/arch · tests/cli · tests/quality

## 抬头
- **代号**：W2-TEST1（文件级穷尽扫描；只判定不修复）。
- **文件域**：`tests/unit tests/api tests/arch tests/cli tests/quality`，共 **221 文件**（开工枚举 tracked 216 + untracked 5；收工枚举 tracked 219 + untracked 2——收工窗口内 `tests/cli/test_command_tree.py`、`tests/quality/test_module_map.py`、`tests/quality/fixtures/docchk002_claims_fixture.csv` 已被并行线收编；`tests/unit/p1_noise/` 2 文件仍 untracked 且被 CMake `if(EXISTS)` 门卫化〔F-CI-002-01 裁决自述〕）。
- **基线**：开工 2026-09-16T~09:10Z 实测 `git rev-parse HEAD main origin/main` = `180c8a0a / 180c8a0a / b4afc135`，三向不等；按口径等 2 分钟复测 = `59981aa9 / 59981aa9 / b4afc135`，仍不等 ⇒ **以 HEAD=main 相等开工并注明**（origin/main 落后=并行线未推送）。审计窗口内 HEAD 推进 4+ 次（180c8a0a→59981aa9→01754fab→32a5f5f3），并行线**当场改写工作树**（GAP-030 同型，本窗口实证）；多条红灯在窗口内被修又被新 churn 打回，本报告全部判定按**收工实况**定稿，逐条注明时点。
- **收工**：HEAD=main=origin/main=`32a5f5f300700b817cfa1ceafe047acaeff4ee9b`（2026-09-16T09:53Z 实测三向一致）。
- **方法**：`git ls-files`+`git status --porcelain` 穷尽枚举；221 文件全部过目（python 批量指标扫描 + 逐可疑文件精读）；真跑 UT-API（3 次）、UT-ARCH 子集（除 test_inventory 写副作用件）、UT-CLI、tests/quality 全套与四红灯子集（2 次）；注册面用 ci/checks.json(HEAD 与工作树) + ctest_baseline + CMakeLists 交叉；用例 ID 用 `git grep` 机械对账。**test_inventory.py 因会改写 tracked CSV 未执行**（静态判定，立 W2-TEST1-4）。
- **摘要（≤10 行）**：
  1. P0=0；P1=3；P2=4。域内无恒真断言（assertTrue(True) 全域 0 命中）、无"仅存在性"测试函数；多数 C++/Python 测试具实质数值断言与独立 oracle（好例：p1snr 长精度 oracle、p1psfw/p1wcs 故障注入 selfcheck WILL_FAIL、tests/backend 同族的构建戳锁在 tests/cli 缺位但部分文件有 fail-closed 守卫）。
  2. 最大问题是**迁移残径**：ARCH-001 已删目录仍被测试当前置（lib/astro_image_io、lib/backend_host）→ UT-API/UT-ARCH 收工实测仍红（errors 各 1）。
  3. **fail-open skip 家族 15 文件**：前置缺失/构建失败/运行失败→skip（UT-CLI 实测 skipped=38/77），与 GAP-027 已裁决 fail-closed 口径相悖（同源 QA-4）。
  4. **新 CI 规范与注册表脱节**：docs/ci/01_CHECKS.md §2（28 项 CHK-*）零 Python-unittest 承载，本域 47 个 test_*.py/374 方法现靠旧注册表 UT-* 存活；窗口内未提交注册表已丢 UT-* 四项致 tests/quality 接线四例再红（t13a/b/c + doc_line_anchors）。
  5. 用例 ID 对账：8 个 TEST-*-DESIGN-001 在 tests/ 零锚、7 个"可执行" TEST-P*-001 全仓零命中（文档宣称↔测试锚命名断链；非占位造假，属漂移）。
  6. tests/quality 的 GAP-028 四红灯：窗口内曾由 `35804431`（TEST-GREEN-001 收口，Ran 124 OK）转绿，收工时因注册表 churn 再红 3F+1E——按现势红灯同源标注归 F3，不单立测试缺陷条。
  7. 注册面：tests/unit 165 个 add_test 全部由 CMake 编译可达、ctest-full 兜底；40 个 v6_*/p2_rej_*/p1snr_linux_* 名不在 HEAD 注册表 ctest_targets 任何字面/正则（仅 CTEST-LINUX-FULL 全量兜住），新注册表以 `v6_*_*` 通配收编——与 QA-3 注册闭包盲区同源，不单立。

## 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据（本轮真跑） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
| W2-TEST1-1 | tests/api/test_seam_metric_gate.py:21-23（AIO=lib/astro_image_io/astro_image_io.dll）；tests/arch/test_budget_contract.py:12（HOST=lib/backend_host） | ENGINEERING_SPEC §8「每项检查能红能绿」；§7 lib/algorithms+lib/infrastructure 二分（ARCH-001 目标态） | 命令：`python3 -B -m unittest discover -s tests/api -t tests/api`；输出：`Ran 45 / FAILED (errors=1, skipped=16)`，seam 报 `lib/astro_image_io/... 前置缺失 (fail-closed)`；`ls lib/astro_image_io lib/backend_host`→均不存在，而 `lib/infrastructure/aio/astro_image_io.dll` 实存——前置路径永不满足，门永不绿 | P1 | UT-API/UT-ARCH 两枚 waivable=false 门永久红挂 main（收工 32a5f5f3 复测仍红）；SYN-008 接缝独立验证与预算合同探测失能 | 两文件前置路径改指迁移后实存产物（AIO→lib/infrastructure/aio；backend_host→其迁移去向），OMP 归档构建行同订 | tests/api/, tests/arch/ | `python3 -B -m unittest tests.api.test_seam_metric_gate tests.arch.test_budget_contract; echo rc=$?` 期望 rc=0 | 第一波 QA-4（fail-closed 正确、路径滞后至死红）；GAP-027 裁决口径的 ARCH-001 迁移残留；GAP-030（窗口内订正未覆盖此二处） |
| W2-TEST1-2 | tests/cli/test_cli_build.py:54；tests/cli/test_iso_acr_gpu_isolation.py:159,175；tests/cli/test_p1003_drizzle_path.py:25,35,43,53,59；类级 skipUnless 九处（test_monitor_events.py:23、test_phase123_pipeline.py:81、test_cli003_semantics.py:76、test_monitor.py:48、test_memory_growth.py:10、test_parallel_queue.py:10、test_resource_gate.py:10、tests/api test_upm_parallel.py:55/test_upm_recovery_oracle.py:68/test_reject_parallel.py/test_reject_integration_oracle.py） | GAP-027 已裁决「前置缺失必须 fail-closed 不得静默跳过」；docs/ci/01_CHECKS.md §1「能绿能红」；ENGINEERING_SPEC §8 | 命令：`python3 -B -m unittest discover -s tests/cli -t tests/cli`；输出：`Ran 77 / FAILED (failures=2, errors=11, skipped=38)`——近半用例面 skip 即绿；test_cli_build.py:54 把 configure 失败 raise SkipTest 且自述「cli/ 独立图 configure 失败（ARCH-001 迁移未落盘）」；iso:175 把 `phase3 run` 非零退出也 skipTest（吞产品运行失败） | P1 | 无产物/构建破/产品 run 失败三类硬故障可被静默为绿；UT-CLI 名义通过率虚高 | 按同仓好例（test_cli_single_install.py:36-40：CI 已构建仍缺产物→AssertionError；phase*_inprocess 缺 EXE→setUpClass error）统一改 fail-closed：run 失败→fail，前置缺失→error | tests/cli/, tests/api/ | `grep -rnE "skipTest\((\"CLI 二进制缺失\"|f\"phase3 run 未成功)" tests/cli \| wc -l` 期望 0 | 第一波 QA-4 同族扩散入本域（backend 7 处已立条，本条为 cli/api 15 文件 17+ 处计数）；GAP-027 传导未达 |
| W2-TEST1-3 | docs/ci/01_CHECKS.md §2（28 行 CHK-*，CHK-UNIT=`ctest --test-dir build`）；ci/checks.json@HEAD 146 项含 UT-API/UT-ARCH/UT-CLI/UT-QUALITY；工作树未提交版曾为 38 项 CHK-*（丢 UT-*） | ENGINEERING_SPEC §11「CI 必跑…单测」+§8（唯一注册表）；DESIGN §0（docs/ci 为权威链⑤，其清单应覆盖现存活门）；01_CHECKS §1 注册表原则 | 命令：`git show HEAD:docs/ci/01_CHECKS.md \| grep -cE "UT-API\|UT-CLI\|UT-ARCH\|UT-QUALITY\|unittest"`；输出：`0`（收工时 WT 版仍 0，CHK 行 28）；本域 47 个 test_*.py/374 def-test 全靠旧注册表 UT-* 承载；窗口内真跑 tests/quality 子集 `Ran 60 / FAILED (failures=3, errors=1)`（t13a/b/c+line_anchors），直接根因是未提交注册表改名 CHK-KNOWN-FAILURES-BASELINE 与 VERIFY 缺登记 | P1 | 权威链⑤与唯一注册表两套事实源：CI-001 若按现 spec 收口，tests/{api,arch,cli,quality}（含 thread-budget/known-failures/secret-hygiene/doc-anchors 等机器门）从 CI 面消失；churn 期门时红时绿不可判 | CI-001 收口前在 01_CHECKS.md §2 登记 Python 面门（新 CHK-UT-* 或声明 UT-* 保留集），并让 KNOWN-FAILURES-BASELINE-VERIFY/CHECK 注册与末位序一次到位 | docs/ci/, ci/checks.json, tests/quality/ | `grep -cE "UT-API\|unittest discover" docs/ci/01_CHECKS.md` ≥1 且 `python3 -B -m unittest discover -s tests/quality -t tests/quality; echo rc=$?` rc=0 | 第一波 QA-6（CHK-* 口径 2/147 脱节）同源、面扩至 Python 承载；GAP-028/TEST-GREEN-001（四红灯窗口内 35804431 收口转绿→注册表 churn 再红，现势红灯以本条注记）；GAP-030 |
| W2-TEST1-4 | tests/arch/test_inventory.py:35-37（test_05_regeneration_idempotent） | ENGINEERING_SPEC §7「一切输出落 output_dir/run/」+ AGENTS.md §5「不把运行产物散落/写回仓库」；§8（测试不得改跟踪文件） | 命令：`sed -n 35,37p tests/arch/test_inventory.py`；输出：`before=open(INV,"rb").read(); subprocess.run([sys.executable, GEN], cwd=REPO); assertEqual(before, open(INV,"rb").read())`——GEN 在仓内执行并**重写 tracked docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv**（并行线窗口内该测试即可弄脏工作树；故本轮未执行该文件，静态定案） | P2 | UT-ARCH 执行带写副作用：与 GAP-030 并发提交叠加时可制造假 dirty/覆盖他线在途改动 | test_05 改为「生成到 tempdir 后与 tracked CSV 比对」（生成器加 --out 或复制树），tracked 文件只读 | tests/arch/, tools/arch/ | `python3 -B -m unittest tests.arch.test_inventory -v; git status --porcelain docs/architecture \| wc -l` 期望第二值 0 | 第一波 QA-7 自述面（矩阵自报写副作用）本条给出实码行号与最小改法 |
| W2-TEST1-5 | tests/arch/test_single_cli.py:16 | ENGINEERING_SPEC §8「每项检查正例与负例（能红能绿）」——断言可被静默即失效 | 命令：`sed -n 16p tests/arch/test_single_cli.py`；输出：`self.assertNotIn("orchestrator.exe", s) if os.path.exists(MODULE) else None`——MODULE_MAP.md 缺失时整条断言不执行且测试记 PASS | P2 | 「删除旧第二入口」负例在文件缺失场景下不可红；该守卫可静默失效 | 缺文件改 `self.fail("MODULE_MAP missing")`（fail-closed 同款 test_cli_single_install 先例） | tests/arch/ | `grep -c "else None" tests/arch/test_single_cli.py` 期望 0 | 新立（第一波 QA-9 恒真残留同族但形态为「条件静默」） |
| W2-TEST1-6 | docs/algorithms/{PHASE2_COVERAGE,PHASE2_INTEGRATION,PHASE2_REJECTION,PHASE2_SAMPLER,PHASE2_SESSION,PHASE3_FITS_IMPL,PHASE3_PROJ_IMPL}.md §11.x ↔ tests/ 全域 | ENGINEERING_SPEC §8「检查器覆盖：…无悬空引用、能红能绿」；DESIGN §11.1（Oracle/不变量锚须可归因） | 命令：`git grep -lE "TEST-P2-COV-001\|TEST-P2-REJ-001\|TEST-P3-WR-001\|TEST-P3-WCS-001" HEAD -- tests/`；输出：7 个「可执行」用例 ID 全 NONE（另 8 个 TEST-*-DESIGN-001 锚 ID 零命中）；而同模块可执行面已入库（p2_rej_v6_units、v6_p2_upm_ma_*、v6_p2_samp_all 等）却未回锚该 ID——宣称↔实现命名断链 | P2 | docs 宣称的可执行用例 ID 不可被任何机器门解析消费；追踪面按 ID 查询落空（DESIGN-001 型对账失败） | 已落地模块的测试文件头补 `合同锚: TEST-P2-REJ-001` 类回锚注释并把 ID 登记进注册表/矩阵；未落地的显式标 MISSING（先例：TRACEABILITY_MATRIX 显式 MISSING 不冒认） | docs/algorithms/, tests/unit/, docs/traceability/ | python 断言：`每个 docs 声明的可执行 TEST-*-001 ∈ tests/ 锚 ∪ 显式 MISSING 表` | 第一波 QA-1（TST-GEN-001 占位铸造）同族（形态不同：此为漂移断链非造假）；GAP-016（命名脱节）同源不删条 |
| W2-TEST1-7 | tests/cli/test_cli_v7_surface.py:1-21, 全文 | ASTROCS_DESIGN §6.2（唯一命令树 normalize/mosaic/export+help/--version/doctor/benchmark；modules*/selftest 已删且 rc=2——docs/api/CLI_PROTOCOL_V1.md@HEAD 同述「全部删除」） | 命令：`sed -n 1,30p tests/cli/test_cli_v7_surface.py`；输出：docstring 仍按「V7 统一命令面(03 §3)」断言 `astrocs version/modules list\|verify/selftest` 存在与退出码，并引用已废止控制包路径 tasks/02_ABI_BUILD_CLI_TASKS.md；本轮 UT-CLI 中该类 3 例 setUpClass ERROR（cli/ 独立图 configure 失败）故偏差未显形 | P2 | 测试守护已被最高设计废止的命令面；cli/ 独立图修复后要么永久红、要么诱导保留旧命令——与 CLI-001 权威冲突 | CLI-001 收口时整文件退役或改写为「旧命令必 rc=2」负例（同 tests/api/test_cli_protocol.py 窗口内改法：只改期望、不放宽语义） | tests/cli/ | `grep -c "modules list" tests/cli/test_cli_v7_surface.py` 期望 0（或文件不存在） | tests/api/test_cli_protocol.py 同型问题已在窗口内被并行线修复（开工实测 FAIL 2 例→收工复跑转绿），本条为同族残留面 |

**未立条记录（宁缺毋滥）**：① 恒真断言：`assertTrue(True)\|assert True` 全五域 0 命中；② tests/unit 165 个 add_test 编译可达性全通过（无孤儿源文件；未引用者均为头文件/夹具，逐一核对 include 关系）；③ 「40 个 add_test 名不在 HEAD 注册表字面/正则」由 CTEST-LINUX-FULL 全量兜底，且新注册表已用 glob 收编——归第一波 QA-3 闭包议题不重复立条；④ oracle_rej.py/upm_ma_oracle.py 为离线真值发生器（.inc/anchors.json 冻结物入 tests/ 属设计内），未强制 CI 重推导记为风险不立条；⑤ tests/quality 其余 6 文件与 docchk002/line_anchors 测试本体功能正常（红灯根因归 F3）。

## 覆盖清单（221 行，制表符分隔：路径<TAB>VERDICT）

tests/api/__init__.py	NA:package-init
tests/api/test_cli_protocol.py	OK
tests/api/test_common_abi.py	OK
tests/api/test_p1_api.py	OK
tests/api/test_p2_api.py	OK
tests/api/test_p3_api.py	OK
tests/api/test_reject_integration_oracle.py	FINDING:W2-TEST1-2
tests/api/test_reject_parallel.py	FINDING:W2-TEST1-2
tests/api/test_seam_metric_gate.py	FINDING:W2-TEST1-1
tests/api/test_upm_parallel.py	FINDING:W2-TEST1-2
tests/api/test_upm_recovery_oracle.py	FINDING:W2-TEST1-2
tests/arch/__init__.py	NA:package-init
tests/arch/test_backend_arch.py	OK
tests/arch/test_budget_contract.py	FINDING:W2-TEST1-1
tests/arch/test_inventory.py	FINDING:W2-TEST1-4
tests/arch/test_inventory_generator.py	OK
tests/arch/test_phase3_module_arch.py	OK
tests/arch/test_single_cli.py	FINDING:W2-TEST1-5
tests/arch/test_thread_budget.py	OK
tests/cli/__init__.py	NA:package-init
tests/cli/cli_test_hygiene.py	NA:helper-imported
tests/cli/test_bench_cli.py	OK
tests/cli/test_cli001_vpi.py	OK
tests/cli/test_cli003_semantics.py	FINDING:W2-TEST1-2
tests/cli/test_cli004_process_protocol.py	OK
tests/cli/test_cli_build.py	FINDING:W2-TEST1-2
tests/cli/test_cli_protocol.py	OK
tests/cli/test_cli_single_install.py	OK
tests/cli/test_cli_v7_surface.py	FINDING:W2-TEST1-7
tests/cli/test_command_tree.py	FINDING:W2-TEST1-2
tests/cli/test_iso_acr_gpu_isolation.py	FINDING:W2-TEST1-2
tests/cli/test_memory_growth.py	FINDING:W2-TEST1-2
tests/cli/test_monitor.py	FINDING:W2-TEST1-2
tests/cli/test_monitor_events.py	FINDING:W2-TEST1-2
tests/cli/test_p1003_drizzle_path.py	FINDING:W2-TEST1-2
tests/cli/test_parallel_queue.py	FINDING:W2-TEST1-2
tests/cli/test_phase123_pipeline.py	FINDING:W2-TEST1-2
tests/cli/test_phase1_inprocess.py	OK
tests/cli/test_phase2_inprocess.py	OK
tests/cli/test_phase3_inprocess.py	OK
tests/cli/test_resource_gate.py	FINDING:W2-TEST1-2
tests/quality/fixtures/docchk002_claims_fixture.csv	NA:fixture-data
tests/quality/test_compare_products.py	OK
tests/quality/test_doc_line_anchors.py	OK
tests/quality/test_doc_machine_check.py	OK
tests/quality/test_docchk002_mutation.py	OK
tests/quality/test_known_failures_baseline_ci.py	OK
tests/quality/test_linux_release.py	OK
tests/quality/test_module_map.py	OK
tests/quality/test_resource_monitor.py	OK
tests/quality/test_root_cleanliness.py	OK
tests/quality/test_secret_hygiene.py	OK
tests/unit/CMakeLists.txt	OK
tests/unit/aio_abi_omp.hpp	NA:helper-header
tests/unit/aio_abi_selfcheck.cpp	OK
tests/unit/aio_abi_test_main.hpp	NA:helper-header
tests/unit/aio_abi_tests.cpp	OK
tests/unit/calibration_adapter_test.cpp	OK
tests/unit/calibration_integration_test.cpp	OK
tests/unit/cli_bench_verdict_test.cpp	OK
tests/unit/cli_process_test.cpp	OK
tests/unit/cli_wideconv_test.cpp	OK
tests/unit/core_artifact_test.cpp	OK
tests/unit/core_checkpoint_test.cpp	OK
tests/unit/core_context_test.cpp	OK
tests/unit/core_contracts_test.cpp	OK
tests/unit/core_logging_test.cpp	OK
tests/unit/core_module_test.cpp	OK
tests/unit/core_pipeline_test.cpp	OK
tests/unit/core_scheduler_test.cpp	OK
tests/unit/cosmetic_adapter_test.cpp	OK
tests/unit/cosmetic_integration_test.cpp	OK
tests/unit/cpu001_negative_test.cpp	OK
tests/unit/cpu001_provider_selftest.cpp	OK
tests/unit/cpu003_profile_v2_test.cpp	OK
tests/unit/cpu004_routing_test.cpp	OK
tests/unit/cpu006_bench_report_test.cpp	OK
tests/unit/cpu007_profile_store_test.cpp	OK
tests/unit/cpu008_worker_advisor_test.cpp	OK
tests/unit/cpu_abi_test.cpp	OK
tests/unit/cpu_backend_exception_test.cpp	OK
tests/unit/cpu_bench_test.cpp	OK
tests/unit/cpu_fallback_test.cpp	OK
tests/unit/cpu_features_test.cpp	OK
tests/unit/cpu_lease_test.cpp	OK
tests/unit/cpu_monitor_test.cpp	OK
tests/unit/cpu_profile_test.cpp	OK
tests/unit/cpu_provider_test.cpp	OK
tests/unit/drizzle_adapter_impl.cpp	NA:helper-impl
tests/unit/drizzle_adapter_test.cpp	OK
tests/unit/drizzle_precision_default_test.cpp	OK
tests/unit/executor_provider_race_test.cpp	OK
tests/unit/gaia_adapter_test.c	OK
tests/unit/gaia_cat_test.c	OK
tests/unit/gaia_integration_test.c	OK
tests/unit/gaia_magnitude_range_bounds_test.c	OK
tests/unit/gaia_module_manifest_bounds_test.c	OK
tests/unit/gaia_unshuffle_test.c	OK
tests/unit/gaia_xpsd_fixture_gen.c	OK
tests/unit/io_adapter_test.cpp	OK
tests/unit/io_ownership_test.cpp	OK
tests/unit/io_reentrant_test.cpp	OK
tests/unit/ipv_platform_binding_test.cpp	OK
tests/unit/master_flat_median_test.cpp	OK
tests/unit/mon001_gate_test.cpp	OK
tests/unit/mon001_recorder_test.cpp	OK
tests/unit/mon002_alloc_test.cpp	OK
tests/unit/mon002_gate_test.cpp	OK
tests/unit/mon004_enforcement_test.cpp	OK
tests/unit/p1001_modules_test.cpp	OK
tests/unit/p1001_real_nodes_test.cpp	OK
tests/unit/p15_artifact_collect_test.cpp	OK
tests/unit/p1_calibration_test.cpp	OK
tests/unit/p1_hips/adapter_entry_impl.cpp	NA:helper-entry
tests/unit/p1_hips/adapter_test.c	OK
tests/unit/p1_hips/publish_atomic_test.c	OK
tests/unit/p1_hips_writer_test.cpp	OK
tests/unit/p1_ir_facade_test.cpp	OK
tests/unit/p1_noise/adapter_entry_impl.cpp	NA:untracked-wip-gated
tests/unit/p1_noise/adapter_test.cpp	NA:untracked-wip-gated
tests/unit/p1_noise_test.cpp	OK
tests/unit/p1_nside_test.cpp	OK
tests/unit/p1_resource_test.cpp	OK
tests/unit/p1_stars_test.cpp	OK
tests/unit/p1_wcs_phot_test.cpp	OK
tests/unit/p1snr/CMakeLists.txt	OK
tests/unit/p1snr/p1snr_frame_parity_test.cpp	OK
tests/unit/p1snr/p1snr_linux_test.cpp	OK
tests/unit/p1wcs/CMakeLists.txt	OK
tests/unit/p1wcs/p1wcs_astropy_cross.py	OK
tests/unit/p1wcs/p1wcs_fixtures.hpp	NA:helper-header
tests/unit/p1wcs/p1wcs_oracle.hpp	NA:oracle-header
tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py	OK
tests/unit/p1wcs/p1wcs_test_main.hpp	NA:helper-main
tests/unit/p1wcs/p1wcs_tests_apbp.cpp	OK
tests/unit/p1wcs/p1wcs_tests_main.cpp	OK
tests/unit/p1wcs/p1wcs_tests_negative.cpp	OK
tests/unit/p1wcs/p1wcs_tests_oracle.cpp	OK
tests/unit/p1wcs/p1wcs_tests_perf.cpp	OK
tests/unit/p1wcs/p1wcs_tests_properties.cpp	OK
tests/unit/p1wcs/p1wcs_tests_selfcheck.cpp	OK
tests/unit/p1wcs/p1wcs_tests_units.cpp	OK
tests/unit/p2001_real_nodes_test.cpp	OK
tests/unit/p2002_unc_rej_prov_test.cpp	OK
tests/unit/p2_block_plan_test.cpp	OK
tests/unit/p2_ir_facade_test.cpp	OK
tests/unit/p2_output_semantics_test.cpp	OK
tests/unit/p2_rejection_test.cpp	OK
tests/unit/p2_seam_gate_test.cpp	OK
tests/unit/p2_upm_synthetic_test.cpp	OK
tests/unit/p2_workers_test.cpp	OK
tests/unit/p3002_real_nodes_test.cpp	OK
tests/unit/p3002_uncertainty_test.cpp	OK
tests/unit/p3_assembly_test.cpp	OK
tests/unit/p3_coverage_test.cpp	OK
tests/unit/p3_interp_test.cpp	OK
tests/unit/p3_output_test.cpp	OK
tests/unit/p3_projection_test.cpp	OK
tests/unit/p3_sampler_cache_test.cpp	OK
tests/unit/p3_wcs_test.cpp	OK
tests/unit/rt001_abi_test.cpp	OK
tests/unit/rt001_unique_executor_test.cpp	OK
tests/unit/rt002_budget_test.cpp	OK
tests/unit/rt003_context_test.cpp	OK
tests/unit/rt005_registry_test.cpp	OK
tests/unit/rt006_scheduler_test.cpp	OK
tests/unit/rt007_artifact_store_test.cpp	OK
tests/unit/rt008_runtime_client_test.cpp	OK
tests/unit/rt009_node_trace_test.cpp	OK
tests/unit/v6_aio/CMakeLists.txt	OK
tests/unit/v6_aio/mutations/mutate_and_check.py	OK
tests/unit/v6_aio/oracle/v6_aio_oracle.py	OK
tests/unit/v6_aio/oracle/v6_aio_oracle_lib.py	NA:oracle-lib-imported
tests/unit/v6_aio/oracle/v6_aio_oracle_negative.py	OK
tests/unit/v6_aio/v6_aio_test.cpp	OK
tests/unit/v6_p1_cal/CMakeLists.txt	OK
tests/unit/v6_p1_cal/v6_cal_covariance_test.cpp	OK
tests/unit/v6_p1_cal/v6_cal_oracle.hpp	NA:oracle-header
tests/unit/v6_p1_cal/v6_cal_test_support.hpp	NA:helper-header
tests/unit/v6_p1_drz/CMakeLists.txt	OK
tests/unit/v6_p1_drz/v6_p1_drz_oracle.hpp	NA:oracle-header
tests/unit/v6_p1_drz/v6_p1_drz_test.cpp	OK
tests/unit/v6_p1_psfw/CMakeLists.txt	OK
tests/unit/v6_p1_psfw/p1psfw_fixtures.hpp	NA:helper-header
tests/unit/v6_p1_psfw/p1psfw_oracle.hpp	NA:oracle-header
tests/unit/v6_p1_psfw/p1psfw_test_main.hpp	NA:helper-main
tests/unit/v6_p1_psfw/p1psfw_tests_anea.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_gates.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_negative.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_oracle.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_psfsw.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_record.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_winfo.cpp	OK
tests/unit/v6_p1_psfw/tests_main.cpp	NA:helper-main
tests/unit/v6_p2_rej/CMakeLists.txt	OK
tests/unit/v6_p2_rej/oracle_expected.inc	NA:oracle-data
tests/unit/v6_p2_rej/oracle_rej.py	OK
tests/unit/v6_p2_rej/p2_rej_v6_test.cpp	OK
tests/unit/v6_p2_samp/CMakeLists.txt	OK
tests/unit/v6_p2_samp/v6_p2_samp_test.cpp	OK
tests/unit/v6_p2_upm/CMakeLists.txt	OK
tests/unit/v6_p2_upm/oracle/anchors.json	NA:oracle-data
tests/unit/v6_p2_upm/oracle/upm_ma_oracle.py	OK
tests/unit/v6_p2_upm/v6_p2_upm_ma_test.cpp	OK
tests/unit/v6_p3_proj/CMakeLists.txt	OK
tests/unit/v6_p3_proj/p3_proj_legacy_deviation.py	OK
tests/unit/v6_p3_proj/p3_proj_wcs_oracle.py	OK
tests/unit/v6_p3_proj/v6_p3_proj_legacy_probe.cpp	OK
tests/unit/v6_p3_proj/v6_p3_proj_probe.cpp	OK
tests/unit/v6_p3_proj/v6_p3_proj_test.cpp	OK
tests/unit/v6_p3_rsmp/CMakeLists.txt	OK
tests/unit/v6_p3_rsmp/p3_rsmp_core_test.cpp	OK
tests/unit/v6_p3_rsmp/p3_rsmp_gate_test.cpp	OK
tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h	NA:oracle-header
tests/unit/v6_p3_rsmp/p3_rsmp_oracle_test.cpp	OK
tests/unit/v6_p3_rsmp/p3_rsmp_scenarios.h	NA:helper-header
tests/unit/v6_p3_rsmp/p3_rsmp_test_util.h	NA:helper-header
tests/unit/v6_p3_rsmp/run_mutations.py	OK
tests/unit/v6_p3_rsmp/run_verification.sh	NA:dev-harness
tests/unit/v6_p3_rsmp/write_evidence.py	NA:dev-harness
tests/unit/xpsd_spectrum_count_bounds_test.c	OK


## 统计
- files_total = 221
- verdict_counts = {"OK": 172, "FINDING": 20, "NA": 29}（FINDING 分布：W2-TEST1-1×2 / -2×15 / -4×1 / -5×1 / -7×1；NA 分布：helper-header×12, helper-impl/entry/main×5, oracle-header×2 已并入 helper-header 计数, oracle-data×2, dev-harness×2, oracle-lib-imported×1, package-init×3, fixture-data×1, untracked-wip-gated×2, helper-imported×1）
- 枚举命令逐字：cd "/workspace/Astro CS Database" && { git ls-files tests/unit tests/api tests/arch tests/cli tests/quality; git ls-files --others --exclude-standard tests/unit tests/api tests/arch tests/cli tests/quality; } | sort -u（tracked 219 + untracked 2 @收工 32a5f5f3；tracked 216 + untracked 5 @开工 180c8a0a；两版并集 221，逐文件判定见上表，无文件缺失）
