# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emit import flush

ROWS = [
    ("66", "artifacts/evidence/v6/qa-design/evidence/rc_summary.json :: 前台复验-20260924.md::V-23",
     "artifacts/evidence/v6/qa-design/evidence/rc_summary.json", "前台复验-20260924.md::V-23", "S1/S1/确认",
     "成立（S1 两条：Oracle 自反 + 文档一致性步在不存在的路径上记 rc=0）",
     "run_mutations.py:19; artifacts/evidence/v6/qa-design/evidence/rc_summary.json; run_mutations.py; qa_matrix.json",
     "artifacts/evidence/v6/qa-design/oracle/render_docs.py:13-14; oracle/run_all.py:14/93; evidence/rc_summary.json:20-23; docs/validation/v6/QA_MATRIX.md:323 ↔ qa-design/qa_matrix.json:882",
     "仍在",
     "文档一致性步仍指向不存在的路径：`render_docs.py:13-14` 现读 `DEFAULT_DOCS = os.path.join(dirname(dirname(dirname(dirname(HERE)))), 「docs」, 「validation」, 「v6」)`，HERE=`artifacts/evidence/v6/qa-design/oracle` ⇒ 四级上跳落在 `artifacts`，即 `artifacts/docs/validation/v6`；`git ls-files artifacts/docs` ⇒ **0 命中**（跟踪集中不存在）。`run_all.py:93` 调用 `check_docs.py` **不带** `--docs-dir`，故实际判定面就是这条空路径；`check_docs.py:17-19 read()` 直接 `open()` ⇒ 该步既读不到真发布稿也无「不一致」可言。在册证据件 `evidence/rc_summary.json:20-23` 仍记 `「name」: 「check_docs」, 「rc」: 0`（另有 :33 `「rc_total」: 0`）⇒ 洗绿结论未被订正（全目录 `git log -- artifacts/evidence/v6/qa-design/` 最新 `9a2b5d11`(09-21，早于工单)，本批整改未触及）。发布稿现处失同步且门看不见：真稿 `docs/validation/v6/QA_MATRIX.md:323` 已按 `DRZ-FLUX-FIX-01` 改写为「通量守恒为**严格**不变量…flux_conservation_factor 恒 1」，而其机读正本 `qa-design/qa_matrix.json:882` 仍是「通量守恒为条件不变量：pf=1 严格…pf<1 总输出通量=pixfrac^2*Σx」⇒ 渲染一致性步若真读该稿即应判红，现它读空目录 ⇒ 漂移无承载可判。Oracle 自反半：`run_all.py:33-40 build_case_ledger()` 仍把 `oracle_baseline.json` 里**每一条** `r[「ok」]` 无差别计入所属门的 `executed_cases`，未见「恒真基线臂不计入」的排除逻辑（前台复验要求的修法①未落地）；`case_ledger.json` 的 `zero_case_red_rule: 「rc=2 if executed_cases==0 or skipped_cases>=executed_cases」` 仍由同一份自证计数喂入。",
     "高（空路径与失同步为现读；「Oracle 自反」按计数实现现读判定，未实跑）",
     "是（S1，两条须同批）：①一致性步改指仓库真稿 `docs/validation/v6`，并把真稿与 `qa_matrix.json` 的既有漂移判红；②`build_case_ledger` 排除恒真基线臂，`rc_summary.json` 的 `check_docs rc=0` 按当前树重记"),

    ("67", "artifacts/evidence/v6/qa-design/oracle/qa_oracle.py :: 复核-DRZ-前台自证-20260924.md::D-03",
     "artifacts/evidence/v6/qa-design/oracle/qa_oracle.py", "复核-DRZ-前台自证-20260924.md::D-03", "S2/S2/确认",
     "键集出现在三处「必须齐全」清单 ⇒ 裁任一侧都要与合同/生产/Oracle 同批改，不能只改一处",
     "qa_oracle.py:506,:511; eng/tests/integration/v6_p2/oracle/v6_p2_oracle.py:77; eng/tests/integration/v6_p3/p3_v6_export_e2e_test.cpp:502",
     "qa_oracle.py:506/:511; v6_p2_oracle.py:77; p3_v6_export_e2e_test.cpp:502；另 4 处新增独立清单（见取证）",
     "仍在（耦合面比发现时更宽）",
     "原三处锚逐字复现：`qa_oracle.py:506` `[…「pixel_area_power」,「correlation_kernel」,「flux_conservation_factor」,「k_corr」,「output_hash」]`、`:511` 同键集的第二条「必须齐全」循环、`eng/tests/integration/v6_p2/oracle/v6_p2_oracle.py:77` `「correlation_summary」, 「flux_conservation_factor」, 「k_corr」, 「generated_utc」, 「output_hash」`、`eng/tests/integration/v6_p3/p3_v6_export_e2e_test.cpp:502` `void m_prov_key_flux(ExportInputs* in) { in->omit_provenance_key = 「flux_conservation_factor」; }`。且 `git grep -n flux_conservation_factor` 显示独立清单面**已从 3 处增到 8 处**：另有 `eng/tests/contracts/product_family/field_constraints_oracle.py:745`、`eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp:344/472/479`、`eng/tests/integration/v6_p2/v6_p2_integrate_test.cpp:419`、`eng/tests/integration/v6_p3/p3_v6_export_oracle.py:311`、`eng/contracts/data/examples/v6/provenance.example.json:21` + `signal.example.json:14` ⇒ 「改一处须同批改多处」的隐患规模不回退、反而放大。口径分叉已发生一处（佐证该隐患非理论风险）：科学正本侧 `docs/science/DRIZZLE.md:115` 与 `docs/plugins/algorithms_phase1/08_drizzle.md:53` 已改为「`flux_conservation_factor` **恒为 1**」（DRZ-FLUX-FIX-01），而证据侧 `qa-design/qa_matrix.json:882/:922`、`data/gates_analytic.json:650/:689`、`data/gates_inj.json:115` 仍写「= pixfrac² / 条件不变量」。",
     "高",
     "是（S2）：provenance 键集须有单一事实源（合同 schema/常量表），Oracle 与三阶段测试改为引用它；本批同时订正证据侧残留的 pixfrac² 口径"),

    ("68", "c_delta_composition/map_test2.py :: R5-29.md::G-29-01",
     "c_delta_composition/map_test2.py", "R5-29.md::G-29-01", "S2/S2/确认",
     "本批 15 个跟踪文件 0 判据：全部只 print，不具备「能红能绿」资格",
     "…/c_delta_composition/map_test2.py:10",
     "eng/tests/validation/release02/c_delta_composition/map_test2.py:10（同目录 28 个跟踪件 / 21 个 .py）",
     "仍在",
     "被点名行逐字未改，现读 `map_test2.py:10`：`print(「model frames == corrected frames order?」, mf==cf)` —— 判据位上只是一行 print，无 assert/无退出码。目录规模与判据面：`git ls-files eng/tests/validation/release02/c_delta_composition` ⇒ 28 个跟踪文件（含 21 个 `.py`）；`git grep -ln 「assert|sys.exit|raise SystemExit|CHECK(」` 在该目录 ⇒ **仅 1 个文件**（`delta_vs_C.py` 含 1 处 assert）⇒ 「全部只 print、0 判据」整体成立。可跑性亦无：`map_test2.py:3-8` 从 `run/RELEASE-02/trail`（`sys.path.insert`）与 `run/RELEASE-02/L4-rebuild/upmfix_out/p2_upm_model.bin|p2_samples.json|p2_corrected.json` 取输入，而 `run/` 是 gitignore 的临时产物树 ⇒ 干净检出处直接 FileNotFoundError。承载面亦无：`git grep -n 「c_delta_composition」 -- eng/ci/checks.json eng/tests/unit/CMakeLists.txt CMakeLists.txt` ⇒ 0 命中（`release02` 只有 `fix_p1_photometry_apply/p1_apply_oracle.cpp`、`p1_qf_oracle.cpp` 两处进 CMake，`:1789/:1797`）⇒ 这批文件既不被任何门跑，也无判据，属 tracked-but-inert 的验证件。",
     "高",
     "是（S2，验证件判据化）：逐文件补具名判定与退出码（或改为 pytest/unittest 用例并被 CHK-TEST-DISCRIMINATIVE 一类门看见），不可判定者从验证语料除名，不得继续充当 RELEASE 证据"),

    ("69", "checkpoints/round32_machine_consistency.json :: R5-56.md::E-56-02",
     "checkpoints/round32_machine_consistency.json", "R5-56.md::E-56-02", "S1/S2/降级",
     "退出码一致性门读第二套退出码表，从不打开文档声明的唯一源 ⇒ 绿灯是错对象洗白",
     "checkpoints/round32_machine_consistency.json:16-18; eng/tools/docs_machine_consistency.py:334-343; lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:143-166; docs/architecture/ERROR_MODEL.md",
     "eng/tools/docs_machine_consistency.py:199-201/334-343; lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:40/143-166; lib/infrastructure/cli/exit_codes.h:1-21; docs/architecture/ERROR_MODEL.md:24-34; artifacts/evidence/v19r7-quality/checkpoints/round32_machine_consistency.json:16-18",
     "仍在（该门在当前 HEAD 已由「记绿」转为「记红」，守错对象未变）",
     "门的读入面未改：`docs_machine_consistency.py:199-201` SOURCES 只有两项与退出码有关 —— `「error_model」→ERROR_MODEL.md（must 含 token AstroCsExitCode）`、`「orchestrator_h」→orchestrator.h（must 含 namespace AstroCsExitCode）`；判据体 `:334-343` 为 `doc_exit = extract_enum(tax)` vs `code_exit = extract_enum(orc_h, 「namespace AstroCsExitCode」)`，合取式 `「AstroCsExitCode」 in tax and bool(code_exit) and doc_exit == code_exit` ⇒ **`SOURCES` 全表中没有 `lib/infrastructure/cli/exit_codes.h`**（`git grep -n 「exit_codes」 -- eng/tools/docs_machine_consistency.py` 仅命中上述行号，无该头文件）。唯一源侧现读：`ERROR_MODEL.md:24-34`「退出码唯一源 = `lib/infrastructure/cli/exit_codes.h`（11 码…）：OK=0 ARGS=2 INPUT=3 SCIENCE=4 BACKEND=5 COMPUTE=6 IO=7 INTEGRITY=8 **CANCELLED=9 RESOURCE=10** INTERNAL=70」+ `exit_codes.h:2`「本文件是 11 个退出码在仓库内的唯一定义处」、`:16-17` `CANCELLED = 9 / RESOURCE = 10`。第二套表现读在位且被生产使用：`orchestrator.h:143-166` `namespace AstroCsExitCode { … TIMEOUT = 9; CANCELLED = 10; …}`、`:40` 头注释仍自称「与 ERROR_MODEL.md 全集合一致(AstroCsExitCode 0-10进程码… TIMEOUT=9/CANCELLED=10)，由 eng/tools/docs_machine_consistency.py error_taxonomy 全集合校验」，实现侧 `cpp/src/orchestrator.cpp:492` `result.exit_code = AstroCsExitCode::CANCELLED;`、`:498` `= AstroCsExitCode::TIMEOUT` ⇒ 门校验的正是这套与唯一源互斥的码值。归档锚 `round32_machine_consistency.json:16-18` 仍逐字保留该 PASS 明细（doc 与 code 两个 dict 完全等于 orchestrator 旧表）。形态差登记：当前 HEAD 上 `AstroCsExitCode` 在 ERROR_MODEL.md 已 0 命中 ⇒ 该项与 `source_paths_alive` 转红，原「记绿洗白」表现为「记红」，但被守的对象仍是错的那一套。",
     "高",
     "是（S2）：门的对照面改为 `ERROR_MODEL.md ↔ lib/infrastructure/cli/exit_codes.h`（逐名逐值），orchestrator 的 `AstroCsExitCode` 旧表改为 include 唯一源或整体除名；归档 PASS 明细按 ROW 63 一并盖失效"),

    ("70", "common/crypto/sha256.cpp :: R5-62.md::Q-62-16",
     "common/crypto/sha256.cpp", "R5-62.md::Q-62-16", "S2/S2/确认",
     "cpp/Makefile 全部相对路径按移动前布局写死，模块现深两级 ⇒ 无法构建",
     "PENDING.md; trace_replay.py; ../../common/crypto/sha256.cpp; lib/algorithms/shared/crypto/sha256.cpp",
     "lib/infrastructure/pipeline/orchestrator/cpp/Makefile:16/22/58/136/151-152/165-166/178-179/193-194/206-207（14 处 ../../common/）; lib/algorithms/shared/crypto/sha256.cpp; lib/algorithms/shared/healpix/healpix_core.cpp",
     "仍在",
     "`git grep -c 「../../common/」 -- lib/infrastructure/pipeline/orchestrator/cpp/Makefile` ⇒ **14 处**，逐条为编译闭包与头路径：`:22 -I../../common/include`、`:58` `../../common/healpix/healpix_core.cpp ../../common/crypto/sha256.cpp`、`:136/:151-152/:165-166/:178-179/:193-194/:206-207` 同型重复（各 target 的 DEPS/OBJS）。该 Makefile 现位于 `lib/infrastructure/pipeline/orchestrator/cpp/`，`../../` 解析到 `lib/infrastructure/pipeline/` ⇒ 需要 `lib/infrastructure/pipeline/common/**`；`git ls-files lib/infrastructure/pipeline | grep -c common` ⇒ **0**（跟踪集中不存在，磁盘残留不作仓库事实）。移动前的布局 `lib/common/**` 亦已从跟踪集消失（`git ls-files lib/common` ⇒ 0），真实文件现为 `lib/algorithms/shared/crypto/sha256.cpp` 与 `lib/algorithms/shared/healpix/healpix_core.cpp`（`git ls-files lib/algorithms/shared` 命中，另有 `lib/algorithms/shared/include/{astro_scalar.h,precision_context.h}` 对应那条 `-I../../common/include`）⇒ 相对深度差两级，该 Makefile 在当前树不可能构建。文件历史：`git log --oneline -- …/orchestrator/cpp/Makefile` 最新为 `eaf32aad`（ARCH-001「35 个旧目录 → lib/algorithms/** + lib/infrastructure/**」等价迁移本身），此后无订正 ⇒ 迁移未同步该构建面。",
     "高",
     "是（S2，与 ROW 71 同族并批处理）：要么按新布局改写路径并纳入构建图反查，要么随第二套构建面一并除名；不得留「看着能跑其实不可构建」的 Makefile"),
]

flush(ROWS)
print("batch2 ok:", [r[0] for r in ROWS])
