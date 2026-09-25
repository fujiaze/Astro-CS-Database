# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emit import flush

ROWS = [
    ("76", "cpp/ipv/test/ipv_dead_params_lock.py :: R5-85.md::A-85-19",
     "cpp/ipv/test/ipv_dead_params_lock.py", "R5-85.md::A-85-19", "S1/S2/降级",
     "同文件的对比判据拿「收敛后的 IPv」对「Siril 的初始 RMS」判 ✅，并把在册死实现列为 IPV 优势",
     "cpp/ipv/test/ipv_dead_params_lock.py:9-11",
     "lib/algorithms/platesolve/cpp/ipv/REPORT.md:757/822; cpp/ipv/SIRIL_COMPARISON.md:231/472/522; lib/algorithms/platesolve/IPV_PIPELINE.md:160/171/245; cpp/ipv/include/ipv_types.h:133; cpp/ipv/src/ipv_solver.cpp:6; cpp/ipv/test/ipv_dead_params_lock.py:9-11",
     "仍在",
     "对比口径的行本身逐字未改（`git log -- …/ipv/REPORT.md` 最新 `ee99ca26`(09-21)、`SIRIL_COMPARISON.md` 同批未动）：`REPORT.md:822` 现读「| 精度 (RMS) | 双成功帧一致 | RMS 中位 0.487\" (Siril 40.28\" 是**初始 RMS**, 收敛后 IPv 更优) | ✅ |」，同件 `:757` 自己登记「RMS 中位: 40.28\" (Siril 初始 RMS, 未收敛前)」⇒ 一侧取收敛终值、另一侧取未收敛初值，仍以 ✅ 结案；`SIRIL_COMPARISON.md:472`「| RMS 中位 | 0.487\" | 40.28\" (初始 RMS, 收敛后 IPv 更优) | ✅ |」与 `:522`「**0.489\"** | 40.28\" … | ✅ |」两处同型（其中 :522 的 0.489\" 即 ROW 75 点名的 V4.28 行）。第二半「在册死实现列为优势」：`SIRIL_COMPARISON.md:231`「| 16 | **多组采样** | 无 (单组 top 6 对) | 3 组 (vote 0-5, 6-11, 12-17) | **IPV 优势** | 保留 |」所依的 vote/匹配器链在生产解算路径上零调用点 —— `cpp/ipv/src/ipv_solver.cpp:6` 现读「2. triangle_match (三角形匹配, **替代 polygon_match**)」，`git grep -n 「polygon_match|prosac_verify|geometric_vote|extract_consensus」 -- src/ipv_solver.cpp src/ipv_entry.cpp` 只命中该行注释（0 调用），这些名字仅在各自定义文件（`ipv_polygon.cpp:442/450/610`、`ipv_ransac.cpp:311`）与 test/ 出现；本模块自带的锁件 `test/ipv_dead_params_lock.py:9-11` 亦自陈同一事实（「另有 polygon_match / polygon_match_adaptive / geometric_vote / extract_consensus / prosac_verify 等匹配器在 src/ 内**零调用点**(仅 test/ 与 include 声明)」，并指出生产链 `module_adapters.cpp:2100 → ipv_solve_from_memory_with_callback_d → solve_from_memory_with_callback_f64 → solve_post_select` 里 `triangle_match(U, W, 60, 60, 0.002, s0)` 传字面量常数）。文档侧仍以现在时叙述已被源码声明移除的对象：`lib/algorithms/platesolve/IPV_PIPELINE.md:160`「阶段 3: 4 flip_mode 循环 (`solve_flip_mode`)」、`:171` 带 `ipv_solver.cpp#L95` 锚、`:245`「全失败: 返回 fail_result (best_mode=-1)」，而 `include/ipv_types.h:133`「// 移除 best_mode (统一求解, 无 flip_mode 区分)」、`src/ipv_entry.cpp:16`「best_mode (flip_mode) 已移除, 替换为 trans_order」、`src/ipv_wcs.cpp:222` 同。⇒ 锁件（P27 裁决 A：只标注 + 机器锁，不改解算行为）不触及本条被点名的判据行，判据与优势记述原样存续。",
     "高",
     "是（S2）：`REPORT.md:822` 与 `SIRIL_COMPARISON.md:472/:522` 的精度对比须改为同阶段口径（两侧同为收敛终值，或并列初值/终值并撤 ✅），并把 `IPV_PIPELINE.md:160/171/245` 的 flip_mode/best_mode 叙述按 `ipv_types.h:133` 的实际形态改写；零调用匹配器不得计入「IPV 优势」"),

    ("77", "data/gates_analytic.json :: R5-53.md::R553-02",
     "data/gates_analytic.json", "R553-02" if False else "R5-53.md::R553-02", "S1/S2/降级",
     "同一物理不变量在两个门里登记成互逆判据，且都不等于实现口径",
     "data/gates_analytic.json:186-192; data/gates_mc.json:167-173; qa_oracle.py:117",
     "artifacts/evidence/v6/qa-design/data/gates_analytic.json:163-191; data/gates_mc.json:144-172; oracle/qa_oracle.py:109-117; data/mutations.json:179/:270",
     "仍在",
     "两个门仍把同一不变量（跨帧共享模式下方差传播）登记成两套**互为倒数**的度量与相反算子：`gates_analytic.json:163-170` G-ANA-03「title: 相关帧必须用联合 C（简单求和被拒）」→ `criterion: {「metric」: 「var_naive/var_joint」, 「op」: 「<」, 「thresh」: 1, 「status」: 「frozen」}`；`gates_mc.json:144-150` G-MC-03「title: 共享系统项：联合 vs 朴素求和比值 > 1」→ `criterion: {「metric」: 「var_joint/var_naive」, 「op」: 「>」, 「thresh」: 1, 「status」: 「frozen」}`。两条 ref 指向同一实测值（`gates_analytic` 「SCI-P2-001 Oracle C3 实测 1.575；F-OBS-04 实测 3.48x」 vs `gates_mc` 「SCI-P2-001 C3 1.575；SCI-OBS-001 C4 3.48」）⇒ 同一物理事实的两套登记面。实现口径两侧都不等于：`qa_oracle.py:109-117` `@check(「CHK-ANA-03」, [「G-ANA-03」], …)` 实际断言是 `subj = naive[「var」] if (B.get(「M01_rho0」) or B.get(「I01_shared_indep」)) else ref[「var」]` → `return dict(measured=subj/ref[「var」], op=「>=」, thresh=1.0, ok=bool(ok))` —— 度量对象是「被试输出方差 / 联合方差 ≥ 1」（不得低估），既非登记的 `var_naive/var_joint < 1` 亦非 `var_joint/var_naive > 1`；且无注入的基线臂下 `subj = ref[「var」]` ⇒ `measured = 1.0`、`ok = not(subj < ref*(1-1e-9)) = True`，即基线恒过（与 ROW 66 的「恒真基线臂计入 executed_cases」同源）。互逆登记还被 mutation 面双向钉住：`data/mutations.json:179 「rule」: 「G-ANA-03/G-MC-03」`（targets 含两门）与 `:270 「rule」: 「G-MC-03/G-ANA-03」` ⇒ 改任一侧度量方向都要同批改另一侧登记与 mutation 名册。存续性：`git log --oneline -5 -- artifacts/evidence/v6/qa-design/` ⇒ 唯一命中 `9a2b5d11`(09-21，早于工单)，本批整改（`c4af4136`/`139a2bc4`/`5f8c237b`）未触及该证据树。",
     "高",
     "是（S2）：同一不变量只留一套具名度量（建议 `var_naive/var_joint < 1` 或 `var_joint/var_naive > 1` 择一），门登记、mutation 名册与 `qa_oracle` 断言三方同源；基线臂须给出可失败的判据而非恒 1"),

    ("78", "docs/TRACEABILITY.c :: L6-发布与验收证据链.md::L6-14",
     "docs/TRACEABILITY.c", "L6-发布与验收证据链.md::L6-14", "S1/S1/确认",
     "追踪表的 `VERIFIED` 由「测试名字符串出现在测试文件里」生成，67/67 行全 VERIFIED，与 §12.5 及发布页互斥",
     "eng/tools/quality/v19r3_traceability.py:409-411; eng/tools/quality/check_traceability.py; eng/tools/traceability/check_traceability_matrix.py; docs/traceability/TRACEABILITY_MATRIX…",
     "docs/TRACEABILITY.csv（67 行，status 全 VERIFIED）; eng/tools/quality/v19r3_traceability.py:396-411",
     "仍在",
     "生成规则未改：`v19r3_traceability.py:396-401` 现读 `hit = any(test_ids_in_file(p, [tid])[tid] for p in tfile_list)` → 不命中才 `broken.append({「id」: rid, 「reason」: f「test ID {tid} 不在测试文件」})`，随后 `:409-411` 与 `:415-417` 两处 `「status」: 「VERIFIED」 if not any(b[「id」] == rid for b in broken) else 「BROKEN」` ⇒ 状态只由「测试 ID 字符串是否出现在所列测试文件文本里」决定，与是否构建、是否注册、是否跑过全无关。读数未变：以 csv 读数（`docs/TRACEABILITY.csv`）**67 行、status 计数 Counter({VERIFIED: 67})**，与发现时一致；文件历史 `git log -- eng/tools/quality/v19r3_traceability.py docs/TRACEABILITY.csv` 最新 `ee99ca26`(09-21) ⇒ 工单之后无订正（`5f8c237b` 的「自造状态词取消 / 状态词表实时解析最高设计验收章节」未落到本生成器）。与 §12.5 及发布面互斥的现场证据：被记为 VERIFIED 的行其 test_files 指向**未进构建图**的 TU —— `TRACEABILITY.csv:17` `DATA-HIPS-IVAR-001 … TEST-SNR-001, lib/algorithms/noise_snr/cpp/test/noise_model_science_test.cpp … VERIFIED`，而该行文件名出现在 ROW 74 的可复算清单里（39 个 tracked-but-unbuilt 测试 TU 之一，任何受跟踪 CMake 非注释行 0 命中）；同表 `:15/:16` 的 `G3ManifestOrderCanonical` 指向 `lib/algorithms/coverage/tests/synthetic_gate.cpp`（该件确在构建图，见 `eng/tests/quality/test_sci_evidence_registration.py:52-56` 阳性对照）⇒ 同一列里「已建」与「从未编译」两类文件得到同一个 VERIFIED 词，与 `docs/ci/03_GATES.md` §2 转写的 §12.5 阶梯（VERIFIED 需真实可执行证据）与发布页状态互斥。",
     "高",
     "是（S1）：`VERIFIED` 的生成条件须改为「测试在构建图且登记为 ctest/CHK 且有实测通过记录」三合取（读 `eng/ci/cmake_graph.py` 的真实构建图），否则追踪表不得记 VERIFIED；并按新口径重生成 67 行"),

    ("79", "docs/architecture.md :: Q2-26.md::Q2-26-01",
     "docs/architecture.md", "Q2-26.md::Q2-26-01", "S1/S3/降级",
     "[S1] `orchestrator/docs/architecture.md` 整篇以现在时叙述一套**跟踪内容中不存在的 Python 编排实现**，并与同模块正本对同一对象给出互斥说法",
     "docs/architecture.md:5; docs/modules/astro_image_io.md; lib/infrastructure/aio/include/astro_image_io.h; tests/test_orchestrator_e2e.py; _adapter.py; lib/infrastructure/aio/include/aio_pipeline.h:34-3…",
     "lib/infrastructure/pipeline/orchestrator/docs/architecture.md:5/95-103/120-128（128 行全篇）; tests/test_orchestrator_e2e.py:1-14",
     "仍在",
     "被点名件逐字未改（`git log -- …/orchestrator/docs/architecture.md` 唯一命中 `eaf32aad`(09-16)，早于工单），全篇 128 行仍以现在时讲一套 Python 编排实现：`:5`「Orchestrator 是天文图像处理管线的编排引擎，串联 5 个标准阶段和 1 个自定义阶段，通过 `PipelineFrame` 命名块容器实现零临时文件的内存数据流」、`:16-24`「### 2. PipelineStageHandlerC (ctypes 回调包装器) … `handler_c = PipelineStageHandlerC(python_callback)`」、`:95-103`「适配器文件使用 `register_*_handler(engine, params)` 模式…`Orchestrator` 不使用真正的 `PipelineEngine`，而是用 `_HandlerExtractor` 捕获 handler」+ python 代码块 + 「通过 `_load_module_from_path` 动态加载适配器文件（避免多个 `pipeline_adapter.py` 命名冲突）」、`:120-128`「内存管线优势 / 日志体系: 每个适配器有独立的 logging.Logger…`platesolve_adapter` 初始化时创建文件日志」。跟踪集中这些对象**没有实现体**：`git grep -ln 「_HandlerExtractor」` ⇒ **仅本 doc 自身**；`git ls-files | grep -c pipeline_adapter` ⇒ 1（`lib/algorithms/drizzle/healpix_drizzle/tests/test_pipeline_adapter.py`，与本文所述 `pipeline_adapter.py` 非同一物）；`git ls-files | grep -E 「orchestrator.*\\.py$」` ⇒ 只有 `tests/__init__.py` 与 `tests/test_orchestrator_e2e.py`。与同模块正本互斥：本模块的在册入口是 C++（`cpp/src/main.cpp` 走 `AstroCsExitCode`，根 `CMakeLists.txt:473-485` ORCH-001 已把 `pipeline/orchestrator/cpp` 纳入构建图），而 `tests/test_orchestrator_e2e.py:1-4` 自标「# NON_PRODUCTION_TOOL_ONLY … The production pipeline uses **orchestrator.exe** <stage1.json> exclusively」⇒ 同一模块对「编排器是什么」给出 Python 引擎（doc 现在时）与 C++ 可执行（正本 + 测试自述）两种互斥说法；阶段数亦与 `docs/architecture/ERROR_MODEL.md:12-15` 的 P1/P2 阶段清单（8 个 P1 + 5 个 P2 条目）不可通约。",
     "高",
     "是（S3，写法面）：本 doc 须限定为「历史 Python 侧脚本说明」并删去现在时的能力叙述，或按在册 C++ 编排器重写；不得与 `orchestrator.exe` 正本并存两说"),

    ("80", "docs/architecture/DEPENDENCY_RULES.md :: 质量-架构与设计.md::Q-ARCH-17",
     "docs/architecture/DEPENDENCY_RULES.md", "质量-架构与设计.md::Q-ARCH-17", "S1/S3/降级",
     "`DEPENDENCY_RULES.md` 把「从未进构建图的 orchestrator + DllLoader 动态加载」写成现行依赖规则",
     "docs/architecture/DEPENDENCY_RULES.md:13; CMakeLists.txt:441-453; cpp/include/dll_loader.h; cpp/src/dll_loader.cpp; docs/architecture/DEPENDENCY_RULES.md; docs/modules/star_detector.md:23; lib/algorit…",
     "docs/architecture/DEPENDENCY_RULES.md:13; CMakeLists.txt:473-485; lib/infrastructure/pipeline/orchestrator/cpp/src/dll_loader.cpp:30/52/273/290; CMakeLists.txt:273 与 lib/algorithms/*/CMakeLists.txt 的 OUTPUT_NAME; orchestrator/cpp/tests/CMakeLists.txt:66/:83",
     "仍在（「从未进构建图」前提已被推翻，剩下的实质是所声明的加载面与构建产物不同源）",
     "被点名的行本身未被改写：`c4af4136`(09-25，工单之后) 确实重写了本文件若干条（`git show c4af4136 -- docs/architecture/DEPENDENCY_RULES.md` 的 ± 行为 common 方向、healpix_drizzle 去重、ACR 链接面、循环依赖、healpix_stack 冻结五条），其中**不含** orchestrator 那条 ⇒ `:13` 现读「- orchestrator 依赖所有模块头文件，通过 DllLoader 动态加载 DLL（不静态链接）。」原样。前提半已变：根 `CMakeLists.txt:473-485`「ORCH-001 批次 1: 编排层进构建图（结构性孤岛收口）… 事实（改前）: 根 CMake 对 orchestrator 命中 0 ⇒ `lib/infrastructure/pipeline/orchestrator/cpp/**` 从未进任何构建图」+ `:485 add_subdirectory(lib/infrastructure/pipeline/orchestrator/cpp)`，该改动由 `git log -S` 定为 `9902c788`(09-17，工单之前) ⇒ 「从未进构建图」在当前树不成立，故本条不再按该半上报。仍成立的实质：规则所述加载面与在册产物不同名同源 —— `dll_loader.cpp:52`「模块基名与平台后缀分离：Windows astro_image_io.dll / Linux astro_image_io.so」、`:273` `std::string aio_path = aio_dir + 「astro_image_io.dll」;`、`:289-290`「预加载 gaia_client.dll (PHOTOMETRIC 依赖…)… photometric_calib.dll 依赖 gaia_client.dll」；而构建图产物名为 `CMakeLists.txt:273 set_target_properties(acsd_io PROPERTIES OUTPUT_NAME 「acsd_io」)`、`lib/algorithms/{calibration,drizzle,noise_snr,cosmetic…}/CMakeLists.txt` 一律 `astrocs_p1_*`、`lib/infrastructure/gaia_xpsd_client/CMakeLists.txt:36` `astrocs_catalog_gaia` ⇒ loader 所需基名在生产图中无对应产物；仅有的两名由**测试桩**满足：`orchestrator/cpp/tests/CMakeLists.txt:66 OUTPUT_NAME 「astro_image_io」 PREFIX 「」`、`:83 OUTPUT_NAME 「snr_estimator」`。`lib/algorithms/coverage/CMakeLists.txt:92/:108/:126` 亦自证「`../../infrastructure/aio/astro_image_io.dll` 是 ARCH-001 迁移前的历史构建 / 未跟踪的…源树内**未跟踪**的产物」⇒ 现行规则指向一个只有测试桩满足的动态加载面。消费者面：`git grep -ln DllLoader|dll_loader` 除本模块外只命中文档与证据件（无第二个代码消费者）。",
     "高",
     "是（S3）：`:13` 须改写为「按当前生产链接面」（静态/`acsd_io`+`astrocs_p1_*` 具名目标，或把 DllLoader 基名与构建产物 OUTPUT_NAME 收敛到同一登记表），不得把只有测试桩支撑的动态加载写成现行依赖规则"),
]

flush(ROWS)
print("batch4 ok:", [r[0] for r in ROWS])
