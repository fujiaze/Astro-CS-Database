# W2-LIB2 · 第二波文件级审计 — lib/phase2_upm · phase2_rej · phase2_samp · phase2_int · phase2 · phase2_session · drizzle · coverage · hips_p2 · healpix_db

- 开工基线：git rev-parse HEAD main origin/main = 180c8a0a / 180c8a0a / b4afc135。三 SHA 不等 → 按指令等 2 分钟复取（fetch 后）仍不等 → **以 HEAD=main=180c8a0a 为准开工**并注明 origin/main 滞后未同步。
- 收工基线：HEAD=main=32a5f5f300700b817cfa1ceafe047acaeff4ee9b（窗口内并行线持续推进 180c8a0a→59981aa9→23f42ffa→01754fab→32a5f5f3；origin/main=b4afc135 全程滞后未同步）。开工 SHA=180c8a0a。
- 文件域口径：覆盖集 = git ls-tree -r 180c8a0a --name-only <10 prefix> = 239 文件，与 reports/PROJECT-GOVERNANCE-01/root-scan/_gen/coverage_plan.tsv 的 LIB2 行双向差集为空（239=239，本轮实测 set equal: True，未跟踪文件 0）。
  - lib/phase2_upm / phase2_rej / phase2_samp / phase2_int / lib/coverage 五前缀 0 个 tracked 文件（旧路径已于开工前 commit f776da71 移除；实现现位于 lib/algorithms/{upm,rejection,sampling,integration}/** 与 lib/algorithms/integration/v6/**，属 moved 后继/他域，无覆盖行）。
  - moved-wip（索引/工作树已移、HEAD 旧路径计入覆盖，共 132）：lib/drizzle/**→lib/algorithms/drizzle/**（8）；lib/healpix_db/healpix_browser_qt/**→lib/infrastructure/hips_browser/healpix_browser_qt/**（48）；lib/healpix_db/healpix_drizzle/**→lib/algorithms/drizzle/healpix_drizzle/**（76）。
  - 跨线风险提示（非本域发现，移交主控）：移动已同步根 CMakeLists（astrocs_drizzle 指新路径），但 cli/CMakeLists.txt:151 DRIZZLE_DIR=lib/healpix_db/healpix_drizzle 与 lib/healpix_db/README.md 模块表仍指旧路径——该批合入前须同批更新，否则 main 构建断。
- 重点面结论（本轮实测）：
  1. SCI 锚定：rejection 冻结阈值（4.0/3.0/8、5.0/3.5/8、ESD 0.05/10、percentile 0.2/0.1、minmax 1/1/4）与 docs/science/REJECTION.md 声明逐项一致（stage2_common.cpp:307-360 默认值 vs rejection.cpp:11-13 「阈值权威实现」锚）；UPM production 权重=quality×control_ivar、缺 ivar→rc=2 显式失败禁静默回退（upm.h:129-139 与 docs/algorithms/UPM_SOLVER.md 伪码一致）；drizzle 移动面抽查 drizzle_engine.cpp:2 锚 docs/science/DRIZZLE.md α²v 方差传播（SCI-DRZ-014/ALG-DRZ-VAR）——未见公式漂移。
  2. support/coverage→权重残留：ivar 主面 fail-closed+计数入诊断（stage2.cpp:549-578 产品缺失 exit7；1106-1121 tile 失败 fail=2 除显式 legacy 旗标；1749-1753 诊断字段 ivar_product_missing/legacy_allow_weight_fallback/ivar_tile_read_fallback_pixels）；FZ 门函数（coverage.h:147-165；forbidden tokens 含 support/coverage，coverage.cpp:371-377）已由 v6 集成面消费（lib/algorithms/integration/v6/src/phase2_integrate.cpp:1804-1814）；残留=发现 W2-LIB2-2。
  3. 并发正确性：stage2.cpp omp 区（1293-1316）每 worker 独立栈 scratch、独立计数槽、共享计数为 std::atomic、按 thread-id 定序归并（确定性）、cpu_fail 原子标志→rc 传播——未发现共享栈对象进并行区竞争；sampler/upm 以 std::thread+Runtime lease（sampler.cpp:881-884、upm.cpp:513-522，无 hardware_concurrency 自取），写区间不相交+join；async_io.h BoundedAsyncQueue 有容量/背压/取消/错误传播（§10 合规）。未发现 P0/P1 并发缺陷；OpenMP 硬禁用的 0xC0000005 崩溃史以串行 hotfix 压制、无根因登记→并入 W2-LIB2-4。

## ① 发现表

| ID | 定位 path:line | 违反的最新权威条款 | 当前证据（命令+本轮真跑输出≤3行） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
| W2-LIB2-1 | lib/hips_p2/README.md:98；lib/hips_p2/module.yaml:74-75；lib/phase2/README.md:59,82-83,189-190；lib/phase2/module.yaml:66-67；lib/phase2_session/module.yaml:76-77 | ASTROCS_DESIGN §0（权威链不含 docs/contracts；冲突以 DESIGN 为准）+ ENGINEERING_SPEC §4-7（端口引用有效 DATA 合同，contracts/schemas/ 唯一事实源） | 命令：grep -n "唯一权威|docs/contracts" 上述文件；输出：README.md:98「唯一权威 = DATA-P2-HIPS（docs/contracts/DATA_SEMANTICS.md §20）」；module.yaml:74「data: docs/contracts/DATA_SEMANTICS.md#§20」；DATA_SEMANTICS.md:3「权威：本文档。任何模块/文档不得出现第二套定义」 | P1 | 三模块合同层（README+module.yaml=MOD 事实载体）把端口/单位/invalid 语义锚到已出链旧文档并自称唯一权威，绕开 contracts/schemas 与 UNIFIED_MODEL 数据对象表；DATA-001 合同链在 phase2 域断链，双口径持续漂移 | module.yaml data/api 链接切至 contracts/schemas/（unified schema 落库后）并保留 DATA-* 为别名；README 删「唯一权威」句式（改为别名层，权威=UNIFIED_MODEL/contracts）；docs/contracts 头部权威声明归 GOV/DOC 线收敛 | contracts/ + 本域三模块元数据 | bash -c "! grep -q 'data:.*docs/contracts' lib/hips_p2/module.yaml lib/phase2/module.yaml lib/phase2_session/module.yaml" | 与 GAP-007（两套合同口径并存）重复·本条补代码域证据清单；DOC2 轴同族（悬空 schema 引用）；第一波未登记 |
| W2-LIB2-2 | lib/phase2/tools/stage2.cpp:1123-1141（weights[s]=support_v[s]×snr_v²）与 1119-1121（support 冒充 ivar 面）；lib/phase2/src/stage2_common.cpp:381-386（配置接受 weight_mode="support_x_snr2"→mode0 并照常写正式产品） | docs/design/UNIFIED_MODEL.md §2（support 可否作权重=否）+ docs/science/v6/frozen/01_SEMANTIC_FREEZE.md FZ-FIELD-WEIGHTMODE/FZ-GATE-SUPPORT-COVERAGE（:33-38，进科学权重面即 REJECT）+ docs/plugins/algorithms_phase2/09_coverage.md:6 + ASTROCS_DESIGN §4.4/§9（产品 provenance） | 命令1：grep -n "support_v\[s\] \* snr_v" lib/phase2/tools/stage2.cpp → 1141: weights[s] = support_v[s] * snr_v * snr_v; 命令2：grep -rn "p2_weight_mode_check" lib/phase2/src lib/phase2/tools → 空（门零接线）；命令3：grep -n "FZ-FIELD-WEIGHTMODE" lib/phase2/include/astro/phase2/coverage.h → 76 行禁进科学+157-165 提供门函数（唯一生产消费者=lib/algorithms/integration/v6） | P1 | stage2 是 Phase2 全链（reject/integrate/write）唯一可跑通的产品写出面（p2_session 自报 reject/integrate/write=unavailable），显式 legacy 配置即产出 support 派生权重的 HiPS 马赛克；产品面元数据无 degraded/baseline 标记（仅旁挂 diagnostics.json 记 weight_mode=0），下游/验收面不可区分——仓库自设 FZ 门在该面未执行 | 在 p2_stage2_parse_config 或写产品前调用本库 p2_weight_mode_check：rc=1→拒绝写 output.hips；rc=2（equal/pixel_ivar baseline）→强制写 provenance/baseline 标记；ablation 限 diagnostics-only。附带：diagnostics.json 打开失败不得 log「written」（stage2.cpp:1768-1770）、support 回读缺失不得静默跳过（1688-1693） | 本域 lib/phase2/{src,tools}/（新域 integration 门已在位） | bash -c "grep -q p2_weight_mode_check lib/phase2/src/stage2_common.cpp"（配套负例：config weight_mode=support_x_snr2 → rc!=0） | 与 GAP-010/U-03（SNR→逆方差权重链未证明）同族·本条为 support 入权重面的执行残留；第一波 SCI2 在跑未登记，合并对表；归属 P2-002 |
| W2-LIB2-3 | 23 个源文件堆积任务号/修复流水注释；12 文件 14 处引废止权威面。代表：lib/phase2/src/rejection.cpp:2503-2512（「V15 修复记录」9 条）；include/astro/phase2/block.h:5、coverage.h:5（34A532A2...B2EB308 哈希链+wiki 语义锚）；rejection.h:3（TRACEABILITY_MATRIX）；lib/phase2_session/p2_session.cpp:245（宪章 §16.2/§18.3）；src/stage2_common.cpp:376,391（SNR-008/B4-28/ACR-IVAR-001）；upm.h:196（宪章 §6.3） | ENGINEERING_SPEC §2（注释禁堆积历史版本号/任务编号/审计流水/代码复述）+ ASTROCS_DESIGN §0（语义锚须在链上 docs/science·docs/algorithms·UNIFIED_MODEL） | 命令：grep -rln "宪章|TRACEABILITY_MATRIX|wiki Phase2|34A532A2" lib/phase2/ lib/phase2_session/ --include="*.cpp" --include="*.h" | wc -l → 12（命中 14 处）；python 标签正则（CON-0xx/W*/V1x/M4-C-*/P0-01/SNR-0xx/B*-A*/B2-A*）扫描 → 23 个 cpp/h 命中（明细在日志） | P2 | 注释面被旧治理化石占据且锚指向已废止账本（旧宪章/追溯矩阵/wiki/旧哈希链），后续订正按旧判据找错对象；同型问题批量命中归并一条 | 删流水块；锚统一改 FZ-*/SCI-*/ALG-*（本轮验证对应内容均在 docs/science/v6/frozen/01_SEMANTIC_FREEZE.md 与 docs/algorithms/* on-chain 在位）；模块沿革保留 memory.md（其定位即模块记忆，不在本条范围） | 本域 lib/phase2/**、lib/phase2_session/**（纯注释、零功能改动，宜随 ARCH-001 迁移顺带） | bash -c "test 0 = \$(grep -rl '宪章 §|TRACEABILITY_MATRIX|wiki Phase2|34A532A2' lib/phase2/ lib/phase2_session/ --include='*.cpp' --include='*.h' | wc -l)" | 第一波 GOV 轴「旧账本引用」同因不同面（根文档/引用域）；无 GAP 同号 |
| W2-LIB2-4 | lib/phase2/CMakeLists.txt:11-12（project(astro_phase2) 第二入口）、:14（CMAKE_CXX_STANDARD 20）、:21-23（Windows 默认 MinGW Makefiles 生成器）、:26-39（「Hotfix: OpenMP 硬禁用…避免 0xC0000005」强制串行+显式禁链）、:80-94（测试目标直链 ../infrastructure/aio/astro_image_io.dll 产物字面路径）；lib/phase2/tests/sanitize_driver.cpp:4（-std=c++20 编译指令，与生产 C++17 不同标准） | ENGINEERING_SPEC §1（C++17 双平台；MSVC v143；唯一根 CMake、不引入第二套构建入口）+ AGENTS §5（不以环境/工具手段掩盖失败）+ ASTROCS_DESIGN §8（CPU-heavy 必须多线程——该面整体 hotfix 串行） | 命令：grep -n "CMAKE_CXX_STANDARD 20|MinGW|astro_image_io.dll|Hotfix" lib/phase2/CMakeLists.txt → 6 命中（set(CMAKE_CXX_STANDARD 20)/set(CMAKE_GENERATOR "MinGW Makefiles")/# Hotfix: OpenMP 硬禁用/...astro_image_io.dll）；grep -n "std=c++20" lib/phase2/tests/sanitize_driver.cpp → 1 命中 | P2 | 文件自声明 COMPATIBILITY 非产品事实源（生产 astrocs_phase2 由根 CMake 声明、C++17、std::thread 并行不经 OpenMP），故降 P2；但同一源树双标准双入口积累不兼容漂移；.dll 字面路径依赖外部残留产物；0xC0000005 崩溃以串行压制、无缺陷登记即无修复计划 | 兼容入口删除或收编为根 CMake 的 option 化子图并统一 C++17/MSVC；.dll 改 target 级依赖；崩溃登记缺陷台账（复现包+根因）后恢复并行门（含 1worker/Nworker 一致性，§5.1） | 本域 lib/phase2/ + 归属 CI-001/ARCH-001 | bash -c "! grep -q 'CMAKE_CXX_STANDARD 20' lib/phase2/CMakeLists.txt" | 与 GAP-004/013（构建双轨/兼容面并存）同族；第一波 ARCH 未涉此文件 |
| W2-LIB2-5 | lib/phase2/include/astro/phase2/sampler.h:54-55；lib/phase2/include/astro/phase2/upm.h:180；lib/phase2_session/p2_session.h:4 | ENGINEERING_SPEC §10+§2（线程/所有权注释须与实现一致，错误合同注释=失真文档）+ ASTROCS_DESIGN §8（线程预算单源） | 对照实现（本轮 sed 实测）：sampler.cpp:881-884「无 hardware_concurrency…生产默认 N-worker 并行(std::thread…); OpenMP 条件已移除」+ const int workers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;（0≠auto、并行不再依赖 P2_ENABLE_OPENMP）；upm.cpp:1482 const int nw = (workers > 0) ? workers : 1;（upm.h:180 称 workers<=0=>auto(omp_get_max_threads)）；p2_session.cpp:155 实传 sc.cpu_workers=budget.max_workers（p2_session.h:4 称「sampler=1(串行 reference)」） | P2 | 公共头即 ABI/合同面：按注释传 0 期待自动线程池的调用方实际得串行；过期 OpenMP 前提误导并行改造与门禁核对 | 三处注释按实现改写（workers≤0→1 串行；并行=std::thread×lease>1，跨平台一致） | 本域 include/ + lib/phase2_session/ | bash -c "! grep -q '0=auto' lib/phase2/include/astro/phase2/sampler.h" | 无（本域新增） |

统计：发现 5 条 = P0:0 / P1:2（W2-LIB2-1、W2-LIB2-2）/ P2:3（W2-LIB2-3、-4、-5）。
查有实据但不立条（宁缺毋滥留档）：configs 的 "version":1 与 module.yaml module_version 0.11.0-alpha.2（ENGINEERING §4 要求版本字段；§7 只禁程序/发布产物带版本）；diag["stage2_version"]=1（兼容工具旁挂诊断，非正式 manifest）；lib/healpix_db/README.md:4「版本：v2.0」（文档非产物）；archive/legacy 的 SNR 加权堆栈（不入构建：grep archive/legacy CMakeLists.txt ci/checks.json 0 命中）；execution_options.h hardware_concurrency 默认（standalone 工具预算单源注释合法）；p2_session 双调用 coverage probe 协议与取消点（合同内正确）。

## ② 覆盖清单

口径：每行 相对路径<TAB>VERDICT；基线=开工 HEAD 180c8a0a 的 git ls-tree（239），moved-wip 新路径映射见文末；FINDING 同 ID 按规模纪律复用（次要命中文件在发现行内已列举）。

lib/drizzle/CMakeLists.txt	NA:moved-wip
lib/drizzle/README.md	NA:moved-wip
lib/drizzle/include/astrocs/drizzle/types.h	NA:moved-wip
lib/drizzle/memory.md	NA:moved-wip
lib/drizzle/module.yaml	NA:moved-wip
lib/drizzle/src/astrocs_p1_drizzle.def	NA:moved-wip
lib/drizzle/src/module_entry.cpp	NA:moved-wip
lib/drizzle/src/module_exports.map	NA:moved-wip
lib/healpix_db/.gitignore	OK
lib/healpix_db/README.md	OK
lib/healpix_db/archive/legacy/healpix_stack/.gitignore	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/Makefile	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/README.md	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/ahps_format.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/ahps_reader.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/ahps_reader.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/ahps_writer.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/ahps_writer.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/build.ps1	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/corrected_stacker.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/corrected_stacker.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_fitter.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_sampler.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/gradient_sampler.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/nanoflann.hpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/snr_evaluator.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/test_gradient_sampler.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/gradient/test_snr_evaluator.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/healpix_core.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/healpix_core.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/hp_stack_api.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/hp_stack_api.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/hp_stack_hiss.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/hp_stack_hiss.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/memory.md	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/simple_test.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/stack_db.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/stack_db.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/stack_engine.cpp	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/stack_engine.h	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/tests/test_gradient_synthetic.py	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/tests/test_healpix_stack.py	NA:archived
lib/healpix_db/archive/legacy/healpix_stack/tests/test_hp_stack_hiss.py	NA:archived
lib/healpix_db/docs/healpix_drizzle_overview.md	OK
lib/healpix_db/healpix_browser_qt/CMakeLists.txt	NA:moved-wip
lib/healpix_db/healpix_browser_qt/Makefile	NA:moved-wip
lib/healpix_db/healpix_browser_qt/README.md	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/main.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/main_window.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/main_window.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/stf_bar.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/stf_bar.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/stf_panel.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/app/stf_panel.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/browser_backend.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/gl_renderer.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/gl_renderer.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/healpix_math.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/healpix_math.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/hips_sky_view.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/hips_sky_view.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/logger.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/core/stf_engine.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/deploy.ps1	NA:moved-wip
lib/healpix_db/healpix_browser_qt/include/healpix_browser_core.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/memory.md	NA:moved-wip
lib/healpix_db/healpix_browser_qt/run_healpix.bat	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/debug_healpix.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/debug_trace.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/gen_hips_browser_test.py	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/gen_tiny_hiss.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/test_browser_backend.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/test_browser_dual_dtype.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/test_geometry_truth.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/test_healpix_math.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/test_hips_browser_backend.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/test_stf_engine.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tests/trace_browser_lineage.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tools/gen_geometry_truth.py	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tools/gen_ref_source.py	NA:moved-wip
lib/healpix_db/healpix_browser_qt/tools/hips_tile_oracle.py	NA:moved-wip
lib/healpix_db/healpix_browser_qt/widgets/abstract_view.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/widgets/abstract_view.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/widgets/hips_view.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/widgets/hips_view.h	NA:moved-wip
lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp	NA:moved-wip
lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/.gitignore	NA:moved-wip
lib/healpix_db/healpix_drizzle/Makefile	NA:moved-wip
lib/healpix_db/healpix_drizzle/README.md	NA:moved-wip
lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/astro_sphere_sink.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/drizzle_engine.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/drizzle_engine.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/fits_reader.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/fits_reader.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/healpix_core.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/healpix_core.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/hp_drizzle_api.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/hp_drizzle_hips_api.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/hp_drizzle_internal.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/memory.md	NA:moved-wip
lib/healpix_db/healpix_drizzle/nanoflann.hpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/poly_clip.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/poly_clip.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/reverse_drizzle.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/reverse_drizzle.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/snr_evaluator.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/snr_evaluator.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/spherical_overlap.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/spherical_overlap.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/acceptance_drizzle.py	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/bench_drizzle.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/bench_write.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/candidate_oracle_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/concurrency_cache_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/control_median_mc_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_acceptance_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_freeze_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_l0_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_l2_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_l3_full_fp32.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_nonfinite_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_science_completion_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/drizzle_science_matrix_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/hiss_write_probe.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/kcorr_matrix_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/mini_sip1000.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/oracle_edge_crossing_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/oracle_independent_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p0_sip_order_guard_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/CMakeLists.txt	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_fixtures.hpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_geom.hpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_merge_pipeline_lock.sh	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_oracle.hpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_test_main.hpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_main.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_perf.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_selfcheck.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_thread_probe.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/probe_reverse_steps.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/reference_overlap.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/representative_probe.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/reverse_api_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/reverse_drizzle_science_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/reverse_drizzle_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/test_drizzle.py	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/test_nside_pixfrac.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/test_pipeline_adapter.py	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/test_spherical_overlap.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/trace_g4_validate.py	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/variance_propagation_test.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/tests/verify_bench.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/v6_drizzle_science.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/v6_drizzle_science.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/v6_spherical_overlap.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/v6_spherical_overlap.h	NA:moved-wip
lib/healpix_db/healpix_drizzle/wcs_sip.cpp	NA:moved-wip
lib/healpix_db/healpix_drizzle/wcs_sip.h	NA:moved-wip
lib/healpix_db/healpix_io/ARCHIVED.md	OK
lib/healpix_db/memory.md	OK
lib/hips_p2/README.md	FINDING:W2-LIB2-1
lib/hips_p2/memory.md	OK
lib/hips_p2/module.yaml	FINDING:W2-LIB2-1
lib/phase2/CMakeLists.txt	FINDING:W2-LIB2-4
lib/phase2/README.md	FINDING:W2-LIB2-1
lib/phase2/configs/stage2_full.example.json	OK
lib/phase2/configs/stage2_gc_3panel_red.json	OK
lib/phase2/configs/stage2_overlap.example.json	OK
lib/phase2/configs/stage2_real_3frame.json	OK
lib/phase2/configs/stage2_real_overlap.json	OK
lib/phase2/configs/stage2_real_overlap_cpu.json	OK
lib/phase2/configs/stage2_t4_true_overlap.json	OK
lib/phase2/configs/stage2_tiny_memory.example.json	OK
lib/phase2/include/astro/phase2/acr_kernels.h	OK
lib/phase2/include/astro/phase2/async_io.h	OK
lib/phase2/include/astro/phase2/block.h	FINDING:W2-LIB2-3
lib/phase2/include/astro/phase2/coverage.h	FINDING:W2-LIB2-3
lib/phase2/include/astro/phase2/execution_options.h	OK
lib/phase2/include/astro/phase2/integrate.h	OK
lib/phase2/include/astro/phase2/rejection.h	FINDING:W2-LIB2-3
lib/phase2/include/astro/phase2/sampler.h	FINDING:W2-LIB2-5
lib/phase2/include/astro/phase2/stage2_common.h	OK
lib/phase2/include/astro/phase2/upm.h	FINDING:W2-LIB2-3
lib/phase2/memory.md	OK
lib/phase2/module.yaml	FINDING:W2-LIB2-1
lib/phase2/src/acr_kernels.cpp	FINDING:W2-LIB2-3
lib/phase2/src/async_io.cpp	OK
lib/phase2/src/block.cpp	OK
lib/phase2/src/coverage.cpp	FINDING:W2-LIB2-3
lib/phase2/src/cuda_bridge_stub.cpp	OK
lib/phase2/src/integrate.cpp	OK
lib/phase2/src/rejection.cpp	FINDING:W2-LIB2-3
lib/phase2/src/sampler.cpp	FINDING:W2-LIB2-3
lib/phase2/src/stage2_common.cpp	FINDING:W2-LIB2-2
lib/phase2/src/upm.cpp	FINDING:W2-LIB2-3
lib/phase2/tests/async_io_test.cpp	OK
lib/phase2/tests/execution_options_test.cpp	OK
lib/phase2/tests/ivar_wiring_test.cpp	FINDING:W2-LIB2-3
lib/phase2/tests/kcorr_lookup_test.cpp	OK
lib/phase2/tests/routing_test.cpp	OK
lib/phase2/tests/sampler_parallel_consistency_test.cpp	OK
lib/phase2/tests/sanitize_driver.cpp	FINDING:W2-LIB2-4
lib/phase2/tests/synthetic_gate.cpp	FINDING:W2-LIB2-3
lib/phase2/tools/calibrated_pair_diag.cpp	OK
lib/phase2/tools/config_smoke.py	OK
lib/phase2/tools/controlled_rejection_metrics.py	OK
lib/phase2/tools/controlled_rejection_truth.py	OK
lib/phase2/tools/hips_compare.py	OK
lib/phase2/tools/linear_fit_oracle.py	OK
lib/phase2/tools/memory_acr_compare.py	OK
lib/phase2/tools/overlap_photometry.py	OK
lib/phase2/tools/rcr_oracle_compare.py	OK
lib/phase2/tools/rejection_cli.cpp	OK
lib/phase2/tools/rejection_matrix.py	OK
lib/phase2/tools/rejection_oracle_compare.py	FINDING:W2-LIB2-3
lib/phase2/tools/satellite_gate_build.py	OK
lib/phase2/tools/satellite_gate_metrics.py	OK
lib/phase2/tools/satellite_gate_real_metrics.py	OK
lib/phase2/tools/stage2.cpp	FINDING:W2-LIB2-2
lib/phase2/tools/weight_runtime_gate.py	OK
lib/phase2_session/README.md	OK
lib/phase2_session/memory.md	OK
lib/phase2_session/module.yaml	FINDING:W2-LIB2-1
lib/phase2_session/p2_session.cpp	FINDING:W2-LIB2-3
lib/phase2_session/p2_session.h	FINDING:W2-LIB2-5

files_total: 239
verdict_counts: FINDING:W2-LIB2-1=5, FINDING:W2-LIB2-2=2, FINDING:W2-LIB2-3=13, FINDING:W2-LIB2-4=2, FINDING:W2-LIB2-5=2, NA:archived=37, NA:moved-wip=132, OK=46, SUM=239
枚举命令: cd "/workspace/Astro CS Database" && git ls-tree -r 180c8a0a --name-only lib/phase2_upm lib/phase2_rej lib/phase2_samp lib/phase2_int lib/phase2 lib/phase2_session lib/drizzle lib/coverage lib/hips_p2 lib/healpix_db
枚举命令2(untracked 补集, 0 命中): git status --porcelain --untracked-files=all -- <同前缀> | grep "??"

moved-wip 新路径映射（旧→新，逐目录前缀）：
lib/drizzle/  ->  lib/algorithms/drizzle/  （8 文件；含 types.h/module_entry.cpp/def/map/CMakeLists/README/memory/module.yaml）
lib/healpix_db/healpix_browser_qt/  ->  lib/infrastructure/hips_browser/healpix_browser_qt/  （48 文件；app/core/widgets/tools/tests 全量）
lib/healpix_db/healpix_drizzle/  ->  lib/algorithms/drizzle/healpix_drizzle/  （76 文件；根 CMake astrocs_drizzle 生产源已指新路径，spot-check：drizzle_engine.cpp:2 SCI-DRZ-014 方差传播锚在位、support=面积比注释 1034 行，无 support→权重面）
