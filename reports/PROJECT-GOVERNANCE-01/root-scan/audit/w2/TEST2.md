# W2-TEST2 文件级审计报告 —— tests/** 测试域（ROOT-004 第二波）

## 抬头
- 文件域：tests/backend(63) tests/cpu(21) tests/io(5) tests/abi(9) tests/contracts(14+1U) tests/integration(14) tests/monitoring(5) tests/glossary(2) + tests 其余目录（unit 167+2U、cli 21+1U、api 11、testkit 11、traceability 9、quality 9+2U、runtime 8、arch 8、system 5、artifact 4、version 3、realdata 3、sciencelint 2、pipeline 2、oracle 1、test_index.csv、__init__.py）= tracked 399 + UNTRACKED 6 = 405；收工时窗口内并行线新落地 tests/config（CFG-001）14 个 UNTRACKED 文件已补读加录，覆盖清单终 419 行。
- 基线：开工 'git rev-parse HEAD main origin/main' = 180c8a0a/180c8a0a/b4afc135——三向不等；按指令等 2 分钟 fetch 重试一次仍不等（origin/main 落后本地），以 HEAD=main=180c8a0ad9755e513670c7a1feb8fa215a87e1d7 开工并注明。收工 HEAD=main=32a5f5f300700b817cfa1ceafe047acaeff4ee9b、origin/main=20d86b7994a1364044a88cdef4a0e291a4be8f1c（窗口内并行线推进；本报告 path:line 为工作树实况，关键条已按 HEAD 版本复核，TEST2-1/2/4/6 不依赖 M 态内容成立）。
- 方法：必读=AGENTS.md、ASTROCS_DESIGN.md §0/§1.2-1.3/§6.2、ENGINEERING_SPEC.md §3-§5/§8、SHARD_BRIEF §1/§2；405 文件逐文件 meta+断言密度机械扫描 + 分类批读 + 重点深读（abi 探针结构/cpu oracle/io 契约/backend skip 面/api 前置/cli 命令树/contracts evidence）+ 真跑 6 组（abi002=18/18 OK 0.6s；iso_acr=OK(skipped=6)；UT-CLI discover=77 tests 2F/11E/38S；UT-API=45 tests 2F/1E/16S；UT-ARCH=33 tests 4F/1E；p1003 单例实测能红）。
- 摘要（≤8 行）：
  1. ABI-002 生命周期语义=探针自证三角：判定函数唯一实现住在 tests/abi 探针 C 内，生产（runtime/modules/lib）零链接，18 用例 0.6s 全绿且未触及任何产品（TEST2-2，P1）。
  2. tests/cli ISO-001 ACR/GPU 隔离套件类级 skipUnless 指向 BLD-002 前死路径 build/cli/astrocs → 产品二进制在位时仍永久 OK(skipped=6)，隔离合同运行面无验收（TEST2-1，P1）。
  3. tests/api 4 个 SYN/PAR oracle 门维持「缺 build/linux-openmp-on/libphase2.a → 整类 skip」，与仓库自身 RESCUE-P0-08 裁决（test_seam_metric_gate.py:25-45「前置缺失必须变红，绝不 skip」）直接矛盾；g++ 链接行还引用 Linux 不存在的 astro_image_io.dll（TEST2-4，P1）。
  4. GAP-027 fail-closed 传导残留实测 11 处 skipTest（backend CLI 二进制缺失 ×7=第一波 QA-4 在册 + cli p1003×2 + mon003「编译失败」×2）（TEST2-3，P1）。
  5. tests/cli 11 套件仍以 phase1/2/3 旧命令树为验收真值（67 命中）并锚死 build/cli 旧布局，对 §6.2 冻结树实测 2F/11E/38S；新树 pin（test_command_tree.py）审计时未入库、收工前刚随并行提交收编（TEST2-5，P1）。
  6. tests/arch/test_budget_contract.py 编译已删除的 lib/backend_host 源 → UT-ARCH 永久红挂 main（在册 FD-R1-010）（TEST2-6，P1）；另 P2×3：contracts/v6 evidence 入库（TEST2-7）、恒真断言单点（TEST2-8=QA-9 同源）、astropy 交叉 oracle 静默降级（TEST2-9）。
  7. 达标面：oracle 独立性（integration v6 三件套磁盘重开+第一性原理复算、cpu provider f64 独立参考、backend g++ 现场编译独立参考、loader/registry/monitoring/glossary 真调生产+故障注入必败）与确定性（12 处随机全固定 seed、容差 2e-4/1e-4 预冻结常量、1w-Nw 逐位一致断言）本轮深读通过。
  8. 观察（未立条，移交）：现行构建二进制 nm 含 hp_drizzle_run_hips（p1003 test_01 实测能红=门有牙齿，属生产/并行线问题，非测试文件缺陷）；tools/check_legacy_exit.py:20 对 build/root-cmake/astrocs if-exists 静默跳符号面（域外）。
- 发现计数：P0 = 0 ｜ P1 = 6 ｜ P2 = 3（共 9 条；P0=0 判词：域内无科学正确性直接失真实据——oracle 独立性与容差冻结核查通过；最重为门禁结构性失效，TEST2-1/2/4 建议 CI-001 按 known_failures never_waivable（ACR_DORMANT/ABI/SCI_ALG_ORACLE）口径升格处置）。

## ① 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据(命令+本轮真跑输出≤3行) | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门(单命令) | GAP/TASK/第一波/旧账本 同源 |
|---|---|---|---|---|---|---|---|---|---|
| W2-TEST2-1 | tests/cli/test_iso_acr_gpu_isolation.py:8-9,42（类级 skipUnless isfile(build/cli/astrocs)）; tests/cli/test_monitor_events.py:8-9,23（同） | DESIGN §1.3「ACR 仅保留源码 DORMANT」+§7.1；ESPEC §5.3 测试纪律+§8 能红能绿；GAP-027 裁决口径（缺前置 fail-closed） | 命令：python3 -B -m unittest -v tests.cli.test_iso_acr_gpu_isolation；输出：Ran 6 tests in 0.000s / OK (skipped=6)；而 build/astrocs 在位、build/cli/astrocs 不存在（BLD-002 后唯一产物=build/astrocs） | P1 | ISO-001 六用例（含不依赖二进制的静态扫描 test_01/02）被类级门连坐永久跳过，UT-CLI 面上「隔离合同」与「隔离已验证」不可区分；test_05/06 另走已撤命令 phase3 run+失败→skip 双 fail-open | EXE→build/astrocs（ASTROCS_CLI_BIN 同款）；前置缺失→self.fail（复用本仓 seam_metric_gate._require_prerequisites 先例）；phase3 run→export run | tests/cli/ | python3 -B -m unittest tests.cli.test_iso_acr_gpu_isolation 2>&1 | tail -1 期望 OK 且无 skipped=6 | GAP-027 同模式残留；第一波 QA-4 同族（backend 侧）；归 CI-001/QA-001；known_failures never_waivable 含 ACR_DORMANT |
| W2-TEST2-2 | tests/abi/abi002_lifecycle_probe.c:19-30（acs_lc_*_v1 判定族唯一实现）; tests/abi/test_abi002_lifecycle.py:67-356（探针↔Python 镜像↔schema 三方自洽） | ESPEC §4-3/4-4（公开头版本化 C ABI、单一实现）+§5.1（Oracle 面向生产实现）+§8；DESIGN §11.2（ABI 测试层测产品） | 命令：grep -rln acs_lc_transition_allowed_v1 tests/abi include runtime modules lib cli；输出：命中=探针.c+测试.py+lifecycle_v1.h 声明，生产目录零命中；python3 -B -m unittest tests.abi.test_abi002_lifecycle → Ran 18 tests in 0.597s OK（不触产品即全绿）；noop_handshake_test.c:41「只握手不触发 execute」 | P1 | 冻结生命周期语义=测试自证闭环；echo/科学模块手写状态机与判定函数无链接/对拍关系，生产漂移无门可红 | reference 判定函数移入 runtime/ 生产 target 供模块链接消费；或增设对真实 .so 注入非法序列（double destroy/execute-after-destroy→STATE 码）的 ctest 用例 | tests/abi/, runtime/, modules/conformance/ | grep -rln acs_lc_transition_allowed_v1 runtime/ modules/ lib/ | wc -l ≥1 且 abi002 用例引用产品构建产物 | 旧宪章 §12.3-3→ESPEC §3/§4 映射族；本轮新立；归 MOD-001/ARCH-001 |
| W2-TEST2-3 | tests/backend/test_p1004_joint_gate.py:135,152,179; test_p2001_parallel_sampler.py:104,120; test_p2002_parallel_upm.py:111,122; tests/cli/test_p1003_drizzle_path.py:25,43; tests/backend/test_mon003_synthetic.py:51,64 | GAP-027 裁决「缺构建产物必须 fail-closed 不得静默跳过」+ESPEC §5.3；docs/ci/01_CHECKS §1 能红能绿 | 命令：grep -rn 'skipTest("CLI 二进制缺失")' tests/backend/ | wc -l → 7；同型 cli+mon003 合计 11（本轮定位）；对照：其余 8 个 EXE 依赖门（p2003/p2006/p2007/p3003-6）无 skip 守卫=缺二进制 subprocess 抛错 fail-closed | P1 | 无 build/astrocs 节点上 UT-BACKEND/UT-CLI 联合科学门集体静默绿；mon003「编译失败→skip」还会掩盖测试自身腐化 | 11 处 skipTest→self.fail("prerequisite build missing")，与 seam_metric_gate/RESCUE-P0-08 批传；归一「缺依赖即红」检查项 | tests/backend/, tests/cli/ | grep -rn 'skipTest("CLI 二进制缺失")\|skipTest("编译失败")' tests/ | wc -l 期望 0 | 第一波 QA-4 同源在册（backend 7 处不删条）；与 GAP-027 重复不合并；归 CI-001 |
| W2-TEST2-4 | tests/api/test_reject_integration_oracle.py:75; test_reject_parallel.py:16,75; test_upm_parallel.py:14,55; test_upm_recovery_oracle.py:17,68（skipUnless isfile(build/linux-openmp-on/libphase2.a)；g++ 链接行含 lib/astro_image_io/astro_image_io.dll） | ESPEC §5.1（必备独立 Oracle 必须执行）+§5.3；仓库内裁决先例 tests/api/test_seam_metric_gate.py:25-45（RESCUE-P0-08：「缺 g++/phase2 OpenMP 归档/AIO 共享库必须变红，绝不 skip——缺库与接缝已消除在 CI 面上不可区分」）；known_failures.json never_waivable=SCI_ALG_ORACLE | 命令：ls build/linux-openmp-on/libphase2.a lib/astro_image_io/astro_image_io.dll；输出：双双「没有那个文件或目录」；discover -s tests/api → Ran 45 tests / FAILED(failures=2, errors=1, skipped=16)（4 套 oracle 全跳；seam 门因 fail-closed 变 ERROR=对照组）；CI 步骤 linux_build_root_graph.sh:52-55 仅在 CI 建 .a，非 CI 节点永久静默 | P1 | SYN-005/006、PAR-003/005 oracle 在审计/开发节点零执行而报 OK；同类缺陷已有 P0-rescue 裁决但未传导至这 4 文件；.dll 名在 Linux 为第二重死路 | 4 文件复用 _require_prerequisites fail-closed；.dll→.so/构建变量；并入 CI-001「缺依赖即红」批传 | tests/api/ | python3 -B -m unittest discover -s tests/api -t . 2>&1 | grep -o 'skipped=[0-9]*' 期望 skipped=0 或预置齐备真跑 | GAP-027 同族不同目录（本轮新面）；归 CI-001/QA-001 |
| W2-TEST2-5 | tests/cli/test_cli001_vpi.py:35,139-244; test_phase123_pipeline.py:22,81; test_phase1_inprocess.py:6; test_phase2_inprocess.py:6; test_phase3_inprocess.py:6; test_cli_v7_surface.py:38; test_bench_cli.py; test_cli_build.py; test_cli_protocol.py; test_cli003_semantics.py; test_cli004_process_protocol.py（phase1/2/3 run|validate|plan 调用合计 67 处） | DESIGN §6.2 唯一命令树（normalize/mosaic/export+help/--version/doctor/benchmark）+§1.2；SHARD_BRIEF §2 旧→新映射（命令树 phase1/2/3→§6.2） | 命令：./build/astrocs phase3 run --config x → unknown command 'phase3 run'（产品旧面已撤）；discover -s tests/cli → Ran 77 / FAILED (failures=2, errors=11, skipped=38)；新树 pin tests/cli/test_command_tree.py 审计时仍 UNTRACKED（收工时已随 32a5f5f3 收编入库） | P1 | UT-CLI 对 §6.2 命令树合同整体失真：一半套件靠死路径永久跳（与 TEST2-1 交叉）、一半对已撤命令报错/红；不能作命令树验收证据；部分红与工作树 M 态 cli/** 混流，需并行线收编后在干净 HEAD 复测 | CLI-001 收编提交同批：旧面套件删除或改指新树；test_command_tree.py 入库；build/cli 路径全清 | tests/cli/, docs/api/ | python3 -B -m unittest discover -s tests/cli -t . 期望 OK（0 旧树红+0 死路径跳） | 归 CLI-001/CLI-002/DOC-001；05 账本 FD-R1-010（UT-CLI 红在册）；第一波 QA-4/本表 TEST2-1 交叉 |
| W2-TEST2-6 | tests/arch/test_budget_contract.py:12（HOST=REPO/lib/backend_host，setUpClass 编译 host_services.cpp） | ESPEC §6（验证后提交）+§5；DESIGN §11.2（PAR-007 线程预算合同须可执行验收）；AGENTS §5 禁私有线程池面依赖此门 | 命令：python3 -B -m unittest tests.arch.test_budget_contract；输出：cc1plus: fatal error: .../lib/backend_host/host_services.cpp: 没有那个文件或目录；git ls-files lib/backend_host | wc -l → 0（ARCH-001 迁移已删，测试未同步） | P1 | UT-ARCH 永久红挂 main（本轮 discover=4F+1E）且按 known_failures 契约不得入基线→阻塞「全部 P0/P1 绿」判据；线程预算不超标合同实际无验收 | HOST 改指迁移后等价源（backend_host 新址），与迁移侧同提交联动（同提交注册精神） | tests/arch/ | python3 -B -m unittest tests.arch.test_budget_contract 期望 OK | 05 账本 FD-R1-010 同源在册（UT-ARCH 红）；归 ARCH-001/RT-001；第一波 QA-2 同族（红压 main） |
| W2-TEST2-7 | tests/contracts/v6/evidence/oracle_report.json, mutations.json, rc_summary.json（tracked）; tests/contracts/v6/run_all.py:92,102,112,141（write_text 重写 tracked 文件） | AGENTS.md §5「一切输出落 output_dir 或 run/，不入库」+ESPEC §7 目录规范；DESIGN §6.3 | 命令：git ls-files tests/contracts/v6/evidence/；输出：3 行 tracked JSON（某次历史运行产物）；run_all.py 复跑即改仓内容、停跑即陈旧自证 | P2 | 仓内固化历史运行证据易被机器读者当现势（QA-1 台账失真同族）；运行带写副作用 | evidence 输出改 run/contracts-v6/（同目录 write_evidence.py 即正确先例：落 run/v6/）；3 文件出库 | tests/contracts/v6/ | git ls-files tests/contracts/v6/evidence/ | wc -l 期望 0 | 第一波 QA-7 同族（写副作用/自报停更）；ROOT-003 同旨 |
| W2-TEST2-8 | tests/backend/test_isa_avx.py:109 | ESPEC §5.3 断言可证伪；01_CHECKS §1 能红能绿 | 命令：grep -rn 'assertTrue(True)' tests/；输出：1 处（tests/backend/test_isa_avx.py:109 比值断言在 LOG 人工判读; 这里是结构守卫） | P2 | AVX 比值判定外移人工判读 LOG，机械面空转（全域唯一残留点） | LOG 行存在且可解析为数值比值断言（结构守卫机械化） | tests/backend/ | grep -rn 'assertTrue(True)' tests/ | wc -l 期望 0 | 第一波 QA-9 同源在册（本轮逐文件定位复核仍成立，不删条） |
| W2-TEST2-9 | tests/io/test_fits_stream_contract.py:230,246,260; tests/cli/test_phase3_inprocess.py:248 | ESPEC §5.1（astropy 交叉 oracle 必备执行）；机制先例：HEAD ci/checks.json 已支持 deps 声明（python3:astropy 见于 p1wcs_astropy_cross 行），UT-IO/UT-CLI 未声明 | 命令：grep -rn 'astropy unavailable' tests/ --include='*.py' | wc -l → 4；本节点 astropy 7.0.1 在位=skip 不触发，干净节点静默降级无人见 | P2 | FITS/phase3 交叉 oracle 在缺第三方库节点静默缺席且无登记 | UT-IO/UT-CLI deps 增 python3:astropy（照 p1wcs_astropy_cross 格式）或前置缺失 fail-closed | tests/io/, tests/cli/, ci/ | grep -rn 'skipTest(f"astropy' tests/ | wc -l 期望 0 | GAP-027 同族（第三方依赖面）；本轮新立 |

> 未复现/不立条（宁缺毋滥）：①tests/cpu AVX512 缺 ISA→exit 77=与 ctest SKIP_RETURN_CODE 同码治理设计非违规；②skipUnless(g++) 工具链门 ~15 处第一波已计未立条维持原状；③「存在性-only 测试」「自写自读 continue 空转」抽样 0 命中（维持第一波未复现记录）；④glossary test:17 roots 三元恒为全量=死参数；⑤tests/arch/test_inventory 再生成幂等比较机制正确（失败时留脏树属低于立门槛槛）。

## ② 覆盖清单


tests/__init__.py	OK
tests/abi/abi002_lifecycle_probe.c	FINDING:W2-TEST2-2
tests/abi/abi003_loader_probe.c	OK
tests/abi/abi004_registry_probe.c	OK
tests/abi/mod001_install_load_check.py	OK
tests/abi/test_abi002_lifecycle.py	FINDING:W2-TEST2-2
tests/abi/test_abi005_echo.py	OK
tests/abi/test_mod001_install_load_check.py	OK
tests/abi/test_module_registry.py	OK
tests/abi/test_secure_loader.py	OK
tests/api/__init__.py	OK
tests/api/test_cli_protocol.py	OK
tests/api/test_common_abi.py	OK
tests/api/test_p1_api.py	OK
tests/api/test_p2_api.py	OK
tests/api/test_p3_api.py	OK
tests/api/test_reject_integration_oracle.py	FINDING:W2-TEST2-4
tests/api/test_reject_parallel.py	FINDING:W2-TEST2-4
tests/api/test_seam_metric_gate.py	OK
tests/api/test_upm_parallel.py	FINDING:W2-TEST2-4
tests/api/test_upm_recovery_oracle.py	FINDING:W2-TEST2-4
tests/arch/__init__.py	OK
tests/arch/test_backend_arch.py	OK
tests/arch/test_budget_contract.py	FINDING:W2-TEST2-6
tests/arch/test_inventory.py	OK
tests/arch/test_inventory_generator.py	OK
tests/arch/test_phase3_module_arch.py	OK
tests/arch/test_single_cli.py	OK
tests/arch/test_thread_budget.py	OK
tests/artifact/test_manifest_schema_negative.py	OK
tests/artifact/test_phase_product_exchange.py	OK
tests/artifact/test_production_store.py	OK
tests/artifact/test_provenance.py	OK
tests/backend/__init__.py	OK
tests/backend/abi_selftest_main.cpp	OK
tests/backend/bench_candidates_main.cpp	OK
tests/backend/bench_harness_main.cpp	OK
tests/backend/candidates_probe_main.cpp	OK
tests/backend/cheat_backend.cpp	OK
tests/backend/fixture_backend.cpp	OK
tests/backend/fixture_common.py	OK
tests/backend/hips_properties_probe_main.cpp	OK
tests/backend/kernel_bench_main.cpp	OK
tests/backend/kernel_oracle_main.cpp	OK
tests/backend/loader_probe_main.cpp	OK
tests/backend/mon003_synthetic_main.cpp	OK
tests/backend/p3_output_fsync_interposer.cpp	OK
tests/backend/p3_output_fsync_probe.cpp	OK
tests/backend/p3_output_probe_main.cpp	OK
tests/backend/p3_resample_probe_main.cpp	OK
tests/backend/p3_session_probe.cpp	OK
tests/backend/p3_wcs_main.cpp	OK
tests/backend/phase1_fixture_main.cpp	OK
tests/backend/phase2_fixture_main.cpp	OK
tests/backend/profile_gen_main.cpp	OK
tests/backend/syn008_seam_main.cpp	OK
tests/backend/test_abi_kernels.py	OK
tests/backend/test_abi_loader.py	OK
tests/backend/test_abi_v1.py	OK
tests/backend/test_bench_candidates.py	OK
tests/backend/test_bench_harness.py	OK
tests/backend/test_calibration_oracle.py	OK
tests/backend/test_cpu_profile.py	OK
tests/backend/test_drizzle_oracle.py	OK
tests/backend/test_drizzle_parallel.py	OK
tests/backend/test_hardware_inspect.py	OK
tests/backend/test_hips_properties.py	OK
tests/backend/test_isa_avx.py	FINDING:W2-TEST2-8
tests/backend/test_isa_avx2_fma.py	OK
tests/backend/test_isa_avx512.py	OK
tests/backend/test_isa_bit_manip.py	OK
tests/backend/test_isa_variants.py	OK
tests/backend/test_mon003_synthetic.py	FINDING:W2-TEST2-3
tests/backend/test_noise_model_oracle.py	OK
tests/backend/test_p1002_gaps.py	OK
tests/backend/test_p1004_joint_gate.py	FINDING:W2-TEST2-3
tests/backend/test_p2001_parallel_sampler.py	FINDING:W2-TEST2-3
tests/backend/test_p2002_parallel_upm.py	FINDING:W2-TEST2-3
tests/backend/test_p2003_seam_oracle.py	OK
tests/backend/test_p2004_reject_integrate.py	OK
tests/backend/test_p2005_block_io.py	OK
tests/backend/test_p2006_canonical_pipeline.py	OK
tests/backend/test_p2007_joint_gate.py	OK
tests/backend/test_p3001_science_freeze.py	OK
tests/backend/test_p3002_properties_order_unit.py	OK
tests/backend/test_p3003_parallel_resampler.py	OK
tests/backend/test_p3004_spherical_oracle.py	OK
tests/backend/test_p3005_fits_output.py	OK
tests/backend/test_p3006_production_pipeline.py	OK
tests/backend/test_p3_output.py	OK
tests/backend/test_p3_projection_oracle.py	OK
tests/backend/test_p3_resample.py	OK
tests/backend/test_p3_wcs.py	OK
tests/backend/test_phase1_hotspot.py	OK
tests/backend/test_phase3_reproject_oracle.py	OK
tests/backend/test_wcs_psf_oracle.py	OK
tests/cli/__init__.py	OK
tests/cli/cli_test_hygiene.py	OK
tests/cli/test_bench_cli.py	FINDING:W2-TEST2-5
tests/cli/test_cli001_vpi.py	FINDING:W2-TEST2-5
tests/cli/test_cli003_semantics.py	FINDING:W2-TEST2-5
tests/cli/test_cli004_process_protocol.py	FINDING:W2-TEST2-5
tests/cli/test_cli_build.py	FINDING:W2-TEST2-5
tests/cli/test_cli_protocol.py	FINDING:W2-TEST2-5
tests/cli/test_cli_single_install.py	OK
tests/cli/test_cli_v7_surface.py	FINDING:W2-TEST2-5
tests/cli/test_iso_acr_gpu_isolation.py	FINDING:W2-TEST2-1
tests/cli/test_memory_growth.py	OK
tests/cli/test_monitor.py	OK
tests/cli/test_monitor_events.py	FINDING:W2-TEST2-1
tests/cli/test_p1003_drizzle_path.py	FINDING:W2-TEST2-3
tests/cli/test_parallel_queue.py	OK
tests/cli/test_phase123_pipeline.py	FINDING:W2-TEST2-5
tests/cli/test_phase1_inprocess.py	FINDING:W2-TEST2-5
tests/cli/test_phase2_inprocess.py	FINDING:W2-TEST2-5
tests/cli/test_phase3_inprocess.py	FINDING:W2-TEST2-5
tests/cli/test_resource_gate.py	OK
tests/contracts/test_contract_graph_negative.py	OK
tests/contracts/test_data_artifacts_negative.py	OK
tests/contracts/v6/__init__.py	OK
tests/contracts/v6/evidence/mutations.json	FINDING:W2-TEST2-7
tests/contracts/v6/evidence/oracle_report.json	FINDING:W2-TEST2-7
tests/contracts/v6/evidence/rc_summary.json	FINDING:W2-TEST2-7
tests/contracts/v6/jsonschema_min.py	OK
tests/contracts/v6/run_all.py	FINDING:W2-TEST2-7
tests/contracts/v6/run_all.sh	OK
tests/contracts/v6/test_v6_negative_mutations.py	OK
tests/contracts/v6/test_v6_schema_integration.py	OK
tests/contracts/v6/tools/gen_data_dictionary.py	OK
tests/contracts/v6/tools/gen_production_schemas.py	OK
tests/contracts/v6/v6_oracle.py	OK
tests/cpu/avx2/provider_avx2_capability_gate_test.c	OK
tests/cpu/avx2/provider_avx2_handshake_test.c	OK
tests/cpu/avx2/provider_avx2_oracle_main.cpp	OK
tests/cpu/avx2/provider_avx2_so_load_test.c	OK
tests/cpu/avx2/run_provider_avx2_checks.py	OK
tests/cpu/avx512/check_avx512_illegal_instr.py	OK
tests/cpu/avx512/provider_avx512_capability_gate_test.c	OK
tests/cpu/avx512/provider_avx512_handshake_test.c	OK
tests/cpu/avx512/provider_avx512_oracle_main.cpp	OK
tests/cpu/avx512/provider_avx512_so_load_test.c	OK
tests/cpu/avx512/run_provider_avx512_checks.py	OK
tests/cpu/baseline/provider_capability_gate_test.c	OK
tests/cpu/baseline/provider_handshake_test.c	OK
tests/cpu/baseline/provider_kernel_oracle_main.cpp	OK
tests/cpu/baseline/provider_so_load_test.c	OK
tests/cpu/baseline/run_provider_oracle_checks.py	OK
tests/cpu/dispatch/cpu005_route_decision_test.cpp	OK
tests/cpu/dispatch/cpu_capability_matrix_test.c	OK
tests/cpu/dispatch/cpu_capability_probe_main.c	OK
tests/cpu/dispatch/run_cpu005_route_checks.py	OK
tests/cpu/dispatch/run_cpu_capability_checks.py	OK
tests/glossary/__init__.py	OK
tests/glossary/test_glossary.py	OK
tests/integration/v6_p1/CMakeLists.txt	OK
tests/integration/v6_p1/EVIDENCE.md	OK
tests/integration/v6_p1/README.md	OK
tests/integration/v6_p1/oracle/v6_p1_reopen_oracle.py	OK
tests/integration/v6_p1/v6_p1_integrate_test.cpp	OK
tests/integration/v6_p2/CMakeLists.txt	OK
tests/integration/v6_p2/EVIDENCE.md	OK
tests/integration/v6_p2/README.md	OK
tests/integration/v6_p2/oracle/v6_p2_oracle.py	OK
tests/integration/v6_p2/v6_p2_integrate_test.cpp	OK
tests/integration/v6_p3/CMakeLists.txt	OK
tests/integration/v6_p3/EVIDENCE.md	OK
tests/integration/v6_p3/p3_v6_export_e2e_test.cpp	OK
tests/integration/v6_p3/p3_v6_export_oracle.py	OK
tests/io/hips_output_fixture.py	OK
tests/io/make_hips_fixture.py	OK
tests/io/test_fits_stream_contract.py	FINDING:W2-TEST2-9
tests/io/test_hips_input_contract.py	OK
tests/io/test_hips_output_contract.py	OK
tests/monitoring/__init__.py	OK
tests/monitoring/test_frozen_gate.py	OK
tests/monitoring/test_log_contract.py	OK
tests/monitoring/test_monitor_contract.py	OK
tests/monitoring/test_run_graph_render.py	OK
tests/oracle/gaia_oracle.py	OK
tests/pipeline/__init__.py	OK
tests/pipeline/test_typed_dag_plan.py	OK
tests/quality/test_compare_products.py	OK
tests/quality/test_doc_line_anchors.py	OK
tests/quality/test_doc_machine_check.py	OK
tests/quality/test_docchk002_mutation.py	OK
tests/quality/test_known_failures_baseline_ci.py	OK
tests/quality/test_linux_release.py	OK
tests/quality/test_resource_monitor.py	OK
tests/quality/test_root_cleanliness.py	OK
tests/quality/test_secret_hygiene.py	OK
tests/realdata/__init__.py	OK
tests/realdata/test_index_v12.py	OK
tests/realdata/test_match_plan.py	OK
tests/runtime/__init__.py	OK
tests/runtime/test_phase_lifecycle.py	OK
tests/runtime/test_rt003_budget_wiring.py	OK
tests/runtime/test_rt004_executor.py	OK
tests/runtime/test_rt005_plan_estimator.py	OK
tests/runtime/test_rt006_trace.py	OK
tests/runtime/test_rt007_cancel_resume.py	OK
tests/runtime/test_typed_dag_negative.py	OK
tests/sciencelint/__init__.py	OK
tests/sciencelint/test_sciencelint.py	OK
tests/system/v6_runtime/CMakeLists.txt	OK
tests/system/v6_runtime/README.md	OK
tests/system/v6_runtime/v6_runtime_contract_test.cpp	OK
tests/system/v6_runtime/v6_runtime_determinism_test.cpp	OK
tests/system/v6_runtime/v6_runtime_resource_record_test.cpp	OK
tests/test_index.csv	OK
tests/testkit/examples/check_constant.py	OK
tests/testkit/examples/fixture_hash.py	OK
tests/testkit/examples/negative_bad_input.py	OK
tests/testkit/examples/oracle_sum.py	OK
tests/testkit/examples/perf_linear.py	OK
tests/testkit/examples/property_invariant.py	OK
tests/testkit/examples/registry.example.json	OK
tests/testkit/fixtures/demo_fixture.txt	OK
tests/testkit/registry.json	OK
tests/testkit/schemas/test_metadata.schema.json	OK
tests/testkit/testkit.spec.md	OK
tests/traceability/__init__.py	OK
tests/traceability/fixtures/bad_id_format.json	OK
tests/traceability/fixtures/chain_break.json	OK
tests/traceability/fixtures/dangling_ref.json	OK
tests/traceability/fixtures/duplicate_module.json	OK
tests/traceability/fixtures/empty_cell.json	OK
tests/traceability/fixtures/missing_column.json	OK
tests/traceability/test_traceability.py	OK
tests/traceability/test_traceability_matrix.py	OK
tests/unit/CMakeLists.txt	OK
tests/unit/aio_abi_omp.hpp	OK
tests/unit/aio_abi_selfcheck.cpp	OK
tests/unit/aio_abi_test_main.hpp	OK
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
tests/unit/drizzle_adapter_impl.cpp	OK
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
tests/unit/p1_hips/adapter_entry_impl.cpp	OK
tests/unit/p1_hips/adapter_test.c	OK
tests/unit/p1_hips/publish_atomic_test.c	OK
tests/unit/p1_hips_writer_test.cpp	OK
tests/unit/p1_ir_facade_test.cpp	OK
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
tests/unit/p1wcs/p1wcs_fixtures.hpp	OK
tests/unit/p1wcs/p1wcs_oracle.hpp	OK
tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py	OK
tests/unit/p1wcs/p1wcs_test_main.hpp	OK
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
tests/unit/v6_aio/oracle/v6_aio_oracle_lib.py	OK
tests/unit/v6_aio/oracle/v6_aio_oracle_negative.py	OK
tests/unit/v6_aio/v6_aio_test.cpp	OK
tests/unit/v6_p1_cal/CMakeLists.txt	OK
tests/unit/v6_p1_cal/v6_cal_covariance_test.cpp	OK
tests/unit/v6_p1_cal/v6_cal_oracle.hpp	OK
tests/unit/v6_p1_cal/v6_cal_test_support.hpp	OK
tests/unit/v6_p1_drz/CMakeLists.txt	OK
tests/unit/v6_p1_drz/v6_p1_drz_oracle.hpp	OK
tests/unit/v6_p1_drz/v6_p1_drz_test.cpp	OK
tests/unit/v6_p1_psfw/CMakeLists.txt	OK
tests/unit/v6_p1_psfw/p1psfw_fixtures.hpp	OK
tests/unit/v6_p1_psfw/p1psfw_oracle.hpp	OK
tests/unit/v6_p1_psfw/p1psfw_test_main.hpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_anea.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_gates.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_negative.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_oracle.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_psfsw.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_record.cpp	OK
tests/unit/v6_p1_psfw/p1psfw_tests_winfo.cpp	OK
tests/unit/v6_p1_psfw/tests_main.cpp	OK
tests/unit/v6_p2_rej/CMakeLists.txt	OK
tests/unit/v6_p2_rej/oracle_expected.inc	OK
tests/unit/v6_p2_rej/oracle_rej.py	OK
tests/unit/v6_p2_rej/p2_rej_v6_test.cpp	OK
tests/unit/v6_p2_samp/CMakeLists.txt	OK
tests/unit/v6_p2_samp/v6_p2_samp_test.cpp	OK
tests/unit/v6_p2_upm/CMakeLists.txt	OK
tests/unit/v6_p2_upm/oracle/anchors.json	OK
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
tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h	OK
tests/unit/v6_p3_rsmp/p3_rsmp_oracle_test.cpp	OK
tests/unit/v6_p3_rsmp/p3_rsmp_scenarios.h	OK
tests/unit/v6_p3_rsmp/p3_rsmp_test_util.h	OK
tests/unit/v6_p3_rsmp/run_mutations.py	OK
tests/unit/v6_p3_rsmp/run_verification.sh	OK
tests/unit/v6_p3_rsmp/write_evidence.py	OK
tests/unit/xpsd_spectrum_count_bounds_test.c	OK
tests/version/__init__.py	OK
tests/version/test_adopt006_version_gate.py	OK
tests/version/test_version_consistency.py	OK
tests/cli/test_command_tree.py	OK	UNTRACKED→TRACKED@32a5f5f3(收编于窗口内)
tests/contracts/test_unified_object_contract.py	OK	UNTRACKED→TRACKED@32a5f5f3(收编于窗口内)
tests/quality/fixtures/docchk002_claims_fixture.csv	OK	UNTRACKED→TRACKED@32a5f5f3(收编于窗口内)
tests/quality/test_module_map.py	OK	UNTRACKED→TRACKED@32a5f5f3(收编于窗口内)
tests/unit/p1_noise/adapter_entry_impl.cpp	OK	UNTRACKED
tests/unit/p1_noise/adapter_test.cpp	OK	UNTRACKED
tests/config/cfg_common.py	OK	UNTRACKED
tests/config/fixtures/negative/cpu_profile_v1_missing_required.json	OK	UNTRACKED
tests/config/fixtures/negative/hardware_fields_in_phase_config.json	OK	UNTRACKED
tests/config/fixtures/negative/missing_output_dir.phase_config.json	OK	UNTRACKED
tests/config/fixtures/negative/precision_out_of_domain.phase_config.json	OK	UNTRACKED
tests/config/fixtures/negative/run_manifest_hardware_field.json	OK	UNTRACKED
tests/config/fixtures/negative/unknown_filter.phase_config.json	OK	UNTRACKED
tests/config/fixtures/positive/cpu_profile_v1_legacy.json	OK	UNTRACKED
tests/config/fixtures/positive/cpu_profile_v2.json	OK	UNTRACKED
tests/config/fixtures/positive/run_manifest.example.json	OK	UNTRACKED
tests/config/gate_assertions.py	OK	UNTRACKED
tests/config/run_validation.py	OK	UNTRACKED
tests/config/test_cfg001_contracts.py	OK	UNTRACKED
tests/config/test_cfg001_negative.py	OK	UNTRACKED

files_total=419（开工基线180c8a0: tracked 399 + UNTRACKED 6；收工窗口内新增加录 tests/config 14 个 UNTRACKED 文件[CFG-001 并行线 mid-window 落地, 已补读]; 原 6 UNTRACKED 中 4 个已随 32a5f5f3 转 tracked）
verdict_counts: OK=374; FINDING=31（W2-TEST2-1:2 -2:2 -3:5 -4:4 -5:11 -6:1 -7:4 -8:1 -9:1）; NA=0
枚举命令逐字: cd "/workspace/Astro CS Database" && timeout 30 git ls-files tests/ ; cd "/workspace/Astro CS Database" && timeout 30 git ls-files --others --exclude-standard tests/（开工 399+6；收工复算 403+16，差集全部补录如上）
