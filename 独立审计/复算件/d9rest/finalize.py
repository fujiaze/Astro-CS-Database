# -*- coding: utf-8 -*-
"""追加 D9 补派成稿的两节收口内容与基线差异记录。"""
import io, os

MD = r"独立审计/证据/D9-工单对账-补.md"

TEXT = u"""
---

## 收工基线核对与差异

- **收工状态**：`git -C "F:/Astro dev/Astro CS Normalization Database" status --porcelain` ⇒ `?? ACSD整治工作包_AUDIT-06.zip`、`?? site/`；HEAD 仍 `c8f64e9a`。
- **与开工逐字相同**（两行、同序、同内容），本批发起期间工作区无第三方改动、本批自身对仓库树**零写入、零 git 写操作**（未执行任何 `git config`/add/commit/checkout）。
- **自记偏差（一次，已即改）**：中途用 shell 重定向做取证时误把一次性片段文件落在仓库根 `_d9rest_row86_view.txt`，随即 `rm -f` 删除并复跑 `status --porcelain` 确认回到上述两行；此后所有工作件只写 `独立审计/复算件/d9rest/`（`emit.py`、`batch1-6.py`、`unbuilt_tus.py`）。
- **未验证项（统一登记）**：本阶段禁构建/ctest/`run_checks.py`/任何 `eng/**` 脚本与端到端 ⇒ (i) ROW 61 的 runner 自测实跑结果、(ii) ROW 63 的「11 查 5 红」具体红数（只静态定得 ≥2）、(iii) ROW 66 的两步实测 rc、(iv) ROW 70/71 两套构建脚本的真实失败形态（按 CMake/Makefile 文本判）、(v) ROW 78/90 相关门实跑读数 —— 均**未复算**，判定基于静态现读，符合工包「只读找错先行」的阶段口径。

## 仍成立且需进 RELEASE-06 实施任务书（按被点名对象聚合）

> 本批 30 条中 29 条判「仍在」。为免同一对象被拆成多条上报，按**被点名对象/同一收口动作**聚合为 13 项；括号内为源行号。

1. **退出码唯一源贯穿**（69、81、82，连带 63 的归档明细）：`eng/tools/docs_machine_consistency.py:199-201/334-343` 的对照面由「ERROR_MODEL ↔ orchestrator.h `AstroCsExitCode`」改为「ERROR_MODEL ↔ `lib/infrastructure/cli/exit_codes.h` 逐名逐值」；`orchestrator.h:40/143-166` 的旧码表改为 include 唯一源或只留 20-28 非进程码段；`docs/architecture/EXECUTION_MODEL.md:50/:52` 删去 `CANCELLED=10`/`TIMEOUT=9` 字面码值。三处同批，否则门与文档各说一套。
2. **v19r7 归档机器门证据族盖失效**（63、64、65）：`artifacts/evidence/v19r7-quality/checkpoints/round32_machine_consistency.json`、`evidence/QA-V19R7-A1-01/EVIDENCE_INDEX.md:11/:17-18`、`TASK_REPORT.md:7`、`TEST_REPORT.md:3` 的「9/9 PASS、broken 0、可进 A2」结论与恒真通过条件须按现役 v2.0.0/11 判据族标注失效或改写；`audit_stats.json:32` 的 `B2_unblock_condition`「0 broken」重写指。
3. **CI 自证承载**（61）：`run_checks.py --self-test`（S6 并行等价 + N1–N7 负例面）在全仓 441 条注册命令里 0 承载；须登记为一条 CHK 或撤下「自测即证据」的台账措辞（`G2-9_CI_INCREMENTAL_STATUS.md:9`）。
4. **v6 qa-design 证据族两处收口**（66、77）：`oracle/render_docs.py:13-14` 与 `run_all.py:14/:93` 的一致性步改指真稿 `docs/validation/v6`（现指不存在的 `artifacts/docs/validation/v6`）并让真稿与 `qa_matrix.json` 的既有漂移判红；`build_case_ledger` 排除恒真基线臂、`rc_summary.json` 的 `rc=0` 重登；`data/gates_analytic.json:163-170` 与 `data/gates_mc.json:144-150` 的互逆度量与 `qa_oracle.py:109-117` 的第三种口径统一为单一具名判据。
5. **provenance 键集单一事实源**（67）：`flux_conservation_factor` 等同名键的「必须齐全」清单现存 8 处（Oracle/P1/P2/P3 测试与合同样例），且证据侧仍留 `pixfrac²` 旧口径（`qa_matrix.json:882`、`gates_analytic.json:650`）与正本「恒 1」相左 ⇒ 改为引用同一常量/合同面并同批订正。
6. **验证语料判据化**（68）：`eng/tests/validation/release02/c_delta_composition/` 21 个 `.py` 只 print 不判定（被点名行 `map_test2.py:10`），且输入取自 gitignore 的 `run/RELEASE-02/**`、不被任何 CHK/ctest 承载 ⇒ 补具名判据或从证据面除名。
7. **第二套构建面收敛**（70、71，连带 74 的未建 TU）：`lib/infrastructure/pipeline/orchestrator/cpp/Makefile`（14 处 `../../common/…`）与 `lib/infrastructure/aio/Makefile:55/:57`、`aio/build.ps1:147`（`../common/…`）按 ARCH-001 后布局改路径或除名；与 `eng/tools/HANDOVER.md:219`「8 个模块 Makefile 是无任何检查项覆盖的独立构建路径」一并给负责人结论。
8. **测试 TU 进构建图 + 追踪表状态词生成规则**（74、78）：当前仍有 39 个受跟踪测试 TU 不在任何受跟踪 CMake 非注释行（可复算口径见 `独立审计/复算件/d9rest/unbuilt_tus.py`）；`eng/tests/quality/v19r3_traceability.py:396-417` 的 `VERIFIED` 仍只由「测试 ID 字符串出现在测试文件里」生成 ⇒ `docs/TRACEABILITY.csv` 现 67/67 VERIFIED 中含指向未建 TU 的行（如 `:17` 的 `noise_model_science_test.cpp`）；证据门 `test_sci_evidence_registration.py:46-74` 的 watched 手工表须改为从构建图自动生成。
9. **HIPS 浏览器 LOD 语义统一**（72、73）：`browser_backend.h:124` 与 `browser_backend.cpp:593` 的「求均值」注释与 B20 求和实现相反（`:619/:629/:649`）；同模块 `HealpixMath::ud_grade`（`healpix_math.cpp:84-99`，除计数）与 `BrowserBackend::ud_grade`（求和）并存且各有活调用点（`gl_renderer.cpp:1365` / `browser_backend.cpp:548/:582`、`browser_cli.cpp:1211`）⇒ 单一实现 + 通道语义具名 + 两路一致性判据。
10. **platesolve IPv 文档口径**（75、76）：`REPORT.md:884/914/916` 与 `SIRIL_COMPARISON.md:544/554` 仍把 `-ffast-math` 记为在册启用而两构建脚本无该旗标；`ipv/build.ps1:27/:72` 的 `-march=native` 站点仍不在 `eng/tools/quality/isa_sites.json`（该件 0 命中）；`REPORT.md:822`、`SIRIL_COMPARISON.md:472/:522` 以「收敛后 IPv」对「Siril 初始 RMS」判 ✅；`IPV_PIPELINE.md:160/171/245` 仍以现在时叙述源码已声明移除的 `solve_flip_mode`/`best_mode`，零调用点匹配器计入「IPV 优势」。
11. **架构文档口径唯一（五处写法缺陷，各一处收口）**：`orchestrator/docs/architecture.md`（79，整篇现在时叙述跟踪集中不存在的 Python 编排实现，与 `orchestrator.exe` 正本互斥）；`DEPENDENCY_RULES.md:13`（80，DllLoader 动态加载作为现行规则，而生产产物名为 `acsd_io`/`astrocs_p1_*`，loader 基名只由 `orchestrator/cpp/tests/CMakeLists.txt:66/:83` 两个测试桩满足）；`MODULE_MAP.md:8/:9` 与四张表（83，宣布「不写状态字段」而 11 行填 INSTALLED、`:102` 填 DORMANT）；`PERFORMANCE_MODEL.md:178/:181`（84，用自标「数据不可用」的行推出 73%/1.42× 并写成「更硬的边界」）；`THREAD_BUDGET_ARCH.md:34/:41` + `EXECUTION_MODEL.md:18` + `calibrator.cpp:30`（85，同行两说、按名不存在的 `THREAD_BUDGET_EXEMPT` 登记表、禁硬编码却写死 16）。
12. **合同层三处**：`03_GATES.md:18` 把 `VERIFIED` 的正式平台写窄为 Windows x64，比 §12.5 正本更严且已扩散到 `README.md:105`、`KNOWN_LIMITATIONS.md:13`、`RELEASE_STATUS.md:18`（87）；`DATA-002` 强制计数四个字段名 + 「不合格样本」三种范围 + 输出面无「权重非正」分解（88）；`IO-002:102` 的「只读回退」与六处「禁止父 order 静默回退」同词两义并留「M2b-B-01 前」历史句（89）。
13. **ACR 生产可达与链接闭包**（86）：`PRODUCTION_EXECUTION_INVENTORY.csv:300` 记 `acr_kernels.cpp` 为 `production,yes`，与同件 `:62-:69`（ACR 例子/测试一律 `no`）、`production_call_paths_stage2.csv:10`（DORMANT 非生产）、`stage2_common.h:178-182`/§8（生产不可达）对撞；根因在生产库 `lib/algorithms/coverage/CMakeLists.txt:41-58` 无条件把 `src/acr_kernels.cpp`、`acr/api/kernel_registry.cpp`、`acr/scheduler/device_executor.cpp`、Windows 侧 `cuda_bridge_loader.cpp` 编入 `phase2`，而 `ASTROCS_ENABLE_ACR` 在 `lib/algorithms/**` 内 0 引用 ⇒ 二选一：源集真进守卫，或承认「链接可达、运行期恒不路由」并改两份清单取值。另 `docs/modules/hips_p2.md:28/:65` 把 §12.5 最高状态词 `VERIFIED` 用于「登记面=设计冻结」且同句承认 `TEST-P2-HIPS-001` 可执行测试 MISSING（90）—— 与追踪表 VERIFIED 生成规则（第 8 项）同族，须一并降格。

## 已修 / 锚失效（不得再上报，标「作废不得再引用」）

### 整条已修

- **ROW 62 — `artifacts/evidence/v19r7-quality/audit_stats.json` :: R4-09.md::RC-05**：**已修（作废不得再引用）**。`gate_before.file_audit_coverage_ok` 由 `true` 就地翻为 `false` 并附 `file_audit_coverage_ok_correction`（`5f8c237b` 09-25）；分母改由同一条 `git ls-files -z` 产出，工具 `eng/tools/file_audit.py` 入库、产物 `artifacts/evidence/truthful-conclusion-01/file_audit.json` 落 `shipping_total 3249 / standard_scanned 3095 / coverage 0.9526` 与 `denominator_definition`。矛盾并存消失且能力面不回退。

### 半条已修（该半作废，不得再作为上报理由；主条目按「仍在」的剩余半上报）

- ROW 65 的「机器一致性门从未登记进 `eng/ci/checks.json`」——**作废**：`CHK-DOCS-MACHINE-CONSISTENCY`（linux-main/prerelease，waivable=false）与 `CHK-DOCS-MACHINE-CONSISTENCY-SELFTEST`（fast/linux-main/windows-main）两条在册。剩余上报面 = A1-01 报告结论句仍写「9/9 PASS、可进 A2」。
- ROW 74 的「`docs/science/DRIZZLE.md` 以现在时把三个未建 TU 当作科学门证据」——**作废**：`candidate_oracle_test`（`tests/CMakeLists.txt:91-102`）、`variance_propagation_test`（`:291 add_test`）、`p1drz_disp009_gate`（`tests/p1drz/CMakeLists.txt:85-86` + `docs/ci/01_CHECKS.md:103`）均已入构建图并被门看见（`a84844cb` 09-25）。剩余面 = 全量 39 个未建 TU 与 watched 手工表。
- ROW 80 的「orchestrator 从未进构建图」——**作废**：根 `CMakeLists.txt:473-485`（ORCH-001，`9902c788` 09-17）已 `add_subdirectory(.../orchestrator/cpp)`，且该改动早于工单生成时刻，故「发现时即错」。剩余面 = `:13` 的 DllLoader 加载面与生产产物名不同源。
- ROW 81/82 的「权威未声明唯一源」——**作废**：`ERROR_MODEL.md:24-34` 已明文唯一源为 `lib/infrastructure/cli/exit_codes.h`。剩余面 = 门与文档仍守/仍写第二套码值。

### 锚漂移与锚失效（本批 0 条整条锚失效；以下按新址复算，旧锚不得再引用）

- ROW 70、71 的对象列写 `common/crypto/sha256.cpp`、`common/healpix/healpix_core.cpp`：迁移前路径，当前跟踪集中 `lib/common/**` 与 `lib/infrastructure/common/**` 均 **0** 命中（`eaf32aad` ARCH-001 迁移所致）⇒ 现址为 `lib/algorithms/shared/crypto/sha256.cpp`、`lib/algorithms/shared/healpix/healpix_core.cpp`，而被点名的**引用方**（两套模块级构建脚本里的 `../../common/…`、`../common/…`）原样在位 ⇒ 判「仍在」，不得以旧路径上报，也不得以「本机磁盘仍有残留副本」当仓库事实。
- ROW 86 的原锚「`production_call_paths_stage2.csv` 自称 `production_reachable=yes`」——**该文件无此列**（列头 `entry_symbol,…,notes`），其 ACR 行发现时即已写 DORMANT；可复算的 `production_reachable=yes` 只存在于 `PRODUCTION_EXECUTION_INVENTORY.csv:300` ⇒ 旧锚作废，按新址上报。
- ROW 88 的「『权重非正』完全无承载面」的绝对形态——**作废**：实现侧有 `drizzle_engine.cpp:288 rejected_nonpositive_weight`；按「合同输出面无原因分解、四字段名不可核」的具体缺口上报。
- ROW 63/69/81 引用的 `docs/TRACEABILITY.c`、`cpp/.h/.h`、`checkpoints/…` 等队列件对象名为工单表截断/聚合写法，本批一律按 `D9-rest.csv` 原文字条截取键、再以跟踪集重锚（重锚结果见各条「现在位点」）。
"""


def main():
    with io.open(MD, "r", encoding="utf-8") as f:
        old = f.read()
    marker = u"<!-- PROGRESS: "
    idx = old.find(marker)
    head = (old[:idx].rstrip() if idx >= 0 else old.rstrip())
    if head.endswith(u"---"):
        head = head[:-3].rstrip()
    with io.open(MD, "w", encoding="utf-8", newline="\n") as f:
        f.write(head + u"\n" + TEXT + u"\n---\n\n<!-- PROGRESS: 30/30 -->\n")
    print("sections appended")


main()
