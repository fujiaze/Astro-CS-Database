# -*- coding: utf-8 -*-
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emit import flush

ROWS = [
    ("61", "artifacts/evidence/release-05/G2-9_CI_INCREMENTAL_STATUS.md :: R5-59.md::E-R5-59-08",
     "artifacts/evidence/release-05/G2-9_CI_INCREMENTAL_STATUS.md", "R5-59.md::E-R5-59-08", "S2/S2/降级",
     "`run_checks.py --self-test`（含 S6 并行等价与 N1–N7 负例面）不在任何注册项命令里 ⇒ G2-9 的判据无自动承载",
     "G2-9_CI_INCREMENTAL_STATUS.md:9; eng/ci/checks.json; eng/ci/run_checks.py:760",
     "artifacts/evidence/release-05/G2-9_CI_INCREMENTAL_STATUS.md:9（未改）; eng/ci/run_checks.py:1042/1611-1626/1636/1717; eng/ci/checks.json（441 条 command）",
     "仍在",
     "解析 `eng/ci/checks.json` 的全部 command 面（顶层与 step 级同计）⇒ **441 条命令，0 条同时含 `run_checks.py` 与 `--self-test`**；71 条调用 run_checks.py 的命令一律形如 `python3 eng/ci/run_checks.py --check <ID> --quiet`，53 条 `--self-test` 全部指向别的检查器（check_build_graph.py / docs_machine_consistency.py / check_frozen_gate.py …）。全仓 `git grep -ln 「run_checks.py --self-test」` 只命中两份证据件（G2-9、GATE-501_VERIFICATION），无脚本或测试以参数列表调用它（`eng/tests/config/check_cfg002_registry.py:1033 run_checks()` 是同名局部函数，非该 CLI）。自测面本身在实现里活着：`run_checks.py:1717 add_argument(「--self-test」)`、`:1636 run_self_test()`、`:1677-1681` 汇总 scheduler/evidence_verdict/declared_inputs/crash_tristate/gate_trust 五族用例、`:1042 cases.append({「case」: 「S6_serial_parallel_equivalence」…})`、`:1611-1626` 与 `:1523-1537` 的 N1–N7 负例名册。台账件自 `25c8227d`(09-22，早于工单) 未改，`:9` 仍以「③ fixture 级用例 S6_serial_parallel_equivalence（run_checks.py --self-test，17 例全绿）」充当已完成证据 ⇒ 该结论无在册承载命令，改坏 runner 调度/负例面不会被任何 CHK 判红。",
     "高",
     "是（低优先，CI 自证承载）：把 runner 自测登记为一条 CHK（带 --self-test 的独立步），或把台账中「自测即证据」的措辞改为明示「无自动承载」"),

    ("62", "artifacts/evidence/v19r7-quality/audit_stats.json :: R4-09.md::RC-05",
     "artifacts/evidence/v19r7-quality/audit_stats.json", "R4-09.md::RC-05", "S2/S2/确认",
     "`audit_stats.json` 在审计工具缺失的情况下把覆盖门记为绿：`file_audit.tool_missing=true` 与 `gate_before.file_audit_coverage_ok=true` 并存，分母 「~713+」 未定义",
     "audit_stats.json:60-62; audit_findings.md:17; file_audit_before.json",
     "artifacts/evidence/v19r7-quality/audit_stats.json:60-64 与 :105-111; eng/tools/file_audit.py:13/39/77-79; artifacts/evidence/truthful-conclusion-01/file_audit.json",
     "已修",
     "被点名的行本身在 `5f8c237b`(09-25 13:30，工单之后) 按正确口径改写。`git show 5f8c237b -- artifacts/evidence/v19r7-quality/audit_stats.json` 的 ± 行现读：`- 「file_audit_coverage_ok»: true` ⇒ `+ 「file_audit_coverage_ok»: false`，并新增 `file_audit_coverage_ok_correction`（现件 `:107`「TRUTHFUL-CONCLUSION-01：原值 true 与同一结论件里 file_audit.tool_missing=true 自相矛盾（工具不存在却宣称覆盖达标）。现改为 false，并把可核结论落到 artifacts/evidence/truthful-conclusion-01/file_audit.json（分子分母同一条命令产出、可复算）」）；`:63-64` 新增 `denominator_defined: false` + `denominator_note`。分母不再口头：`eng/tools/file_audit.py`（同批入库，`git ls-files` 命中）`:39 SOURCE_COMMAND = [「git」, 「ls-files」, 「-z」]`、`:13`「用**一条** git ls-files -z 同时取得分子与分母（同一命令、同一快照）」、`:77-79` shipping/scanned/unscanned 三集合同源、`:87` 把 `denominator_definition` 随产物落盘；产物 `artifacts/evidence/truthful-conclusion-01/file_audit.json` 现读 `counts: {shipping_total: 3249, standard_scanned: 3095, coverage: 0.9526008002462296, tracked_total: 3873}`。历史值 `coverage: 「873/~713+」`(`:62`) 与 `tool_missing: true`(`:60`) 保留原样并注明为历史记录 —— 未以删除或改词规避，取而代之的是可复算的同源分子分母 ⇒ 矛盾并存消失、能力面不回退（覆盖比由 0.9526 具名给出且分母定义随件在盘）。",
     "高",
     "否（作废不得再引用）：矛盾门值已就地翻正、分母改由同一条命令产出并落盘定义；归档件中 `~713+` 已标注为「无定义的历史值」"),

    ("63", "artifacts/evidence/v19r7-quality/checkpoints/round32_machine_consistency.json :: R4-09.md::RC-04",
     "artifacts/evidence/v19r7-quality/checkpoints/round32_machine_consistency.json", "R4-09.md::RC-04", "S2/S2/降级",
     "v19r7 归档机器门「9/9 PASS / broken 0/9」在当前 HEAD 同一判据族上 11 查 5 红；归档快照未标失效，且其冻结解除条款指向已被重写的工具",
     "checkpoints/round32_machine_consistency.json; evidence/QA-V19R7-A1-01/EVIDENCE_INDEX.md:17-28; audit_findings.md:59; eng/tools/docs_machine_consistency.py",
     "checkpoints/round32_machine_consistency.json:1-52（全件未改）; audit_stats.json:32/72-79; evidence/QA-V19R7-A1-01/EVIDENCE_INDEX.md:17-18; eng/tools/docs_machine_consistency.py:199/315-412/696",
     "仍在（分叉已在汇总件登记；失效标注与冻结解除条款两处未落）",
     "已收口的一半：`5f8c237b` 在 `audit_stats.json:73-76` 新增 `checks_live: 11` / `version_live: 「2.0.0」` / `superseded_by: 「eng/tools/docs_machine_consistency.py（现役 version=2.0.0 / checks=11；本件 checks=9 为工具 v1.0.0 时代快照…）」` ⇒ 分叉不再是隐性的。未收口的三半：①被点名的归档快照件逐字未改（`git log -- artifacts/evidence/v19r7-quality/checkpoints/round32_machine_consistency.json` 最新 `9a2b5d11`(09-21，早于工单)），52 行仍是 9 项全 `「pass»: true` + `「version»: 「1.0.0」`，件内无任何失效标注；②索引件 `EVIDENCE_INDEX.md:17-18` 仍写「退出码: 0 (PASS) / 检查项: 9/9 PASS, broken: 0」，:16-28 整段无失效声明；③冻结解除条款 `audit_stats.json:32 「B2_unblock_condition」: 「B2需…经 docs_machine_consistency 0 broken + candidate_oracle 9003例 + synthetic_gate UPMW/PR 复核通过」` 未改写，仍指向已被重写的工具（现件 `:696 「version」: 「2.0.0」`；判据族 `run_checks():315-412` 共 11 项 = 原 9 项同名 + `source_paths_alive` + `numeric_constants_single_spelling`）。「5 红」这一具体数本阶段未实测（禁跑 `eng/**`）；静态可判红 ≥2：`:199` 要求 ERROR_MODEL.md 含 token `AstroCsExitCode`，实测 `git grep -c AstroCsExitCode -- docs/architecture/ERROR_MODEL.md` 无命中（rc=1）⇒ 该项与依赖它的 `error_taxonomy_exit_codes`(`:339-343` 合取式含 `「AstroCsExitCode」 in tax`) 必红，`source_paths_alive`(`:393-397`) 亦因 `r.missing` 非空 fail-closed 判红。",
     "中高（存续与标注为静态现读；「11 查 5 红」的具体红数未复算，只静态定得 ≥2）",
     "是（低优先，归档面）：给 v19r7 归档件（checkpoint 件 + EVIDENCE_INDEX 结果段）盖失效标注，并把 `B2_unblock_condition` 的「0 broken」重写到 v2.0.0 判据族"),

    ("64", "artifacts/evidence/v19r7-quality/evidence/QA-V19R7-A1-01/EVIDENCE_INDEX.md :: R5-56.md::E-56-01",
     "artifacts/evidence/v19r7-quality/evidence/QA-V19R7-A1-01/EVIDENCE_INDEX.md", "R5-56.md::E-56-01", "S2/S2/降级",
     "「机器一致性」A1 门的通过条件接受 rc 0 与 rc 1 ⇒ 恒真判据",
     "EVIDENCE_INDEX.md:11; eng/tools/docs_machine_consistency.py; audit_stats.json",
     "artifacts/evidence/v19r7-quality/evidence/QA-V19R7-A1-01/EVIDENCE_INDEX.md:10-14",
     "仍在",
     "被点名的行本身逐字未改，现读 `EVIDENCE_INDEX.md:11`：「- `python3 eng/tools/docs_machine_consistency.py` 在 vm-bj Linux 上可运行（**退出码 0/1 均可，JSON 已生成**）」。同段 `:10 ## 通过条件` 下四条（:11-14）无一条约束判定值，而 `:17-18` 又把该门结果记为「退出码: 0 (PASS) / 检查项: 9/9 PASS, broken: 0」⇒ 该 A1 门的通过条件对工具输出真值不具判别力（跑得通即过）。文件历史 `git log -- …/EVIDENCE_INDEX.md` 最新 `9a2b5d11`(09-21，早于工单)，本批两个整改提交（`c4af4136`/`5f8c237b`）未触及该件。收缩说明：现役在册门 `CHK-DOCS-MACHINE-CONSISTENCY`（`eng/ci/checks.json`，profiles=linux-main/prerelease，`waivable: false`）由 runner 按 rc 判，不继承此恒真口径 ⇒ 本条只剩归档证据面这一处成立。",
     "高",
     "是（低优先，归档证据面）：该 A1 门通过条件须改写为具名判定（broken=0 且 checks 数与在册工具版本同源），“能跑”不得充当通过"),

    ("65", "artifacts/evidence/v19r7-quality/evidence/QA-V19R7-A1-01/TASK_REPORT.md :: R4-10.md::Q-10-01",
     "artifacts/evidence/v19r7-quality/evidence/QA-V19R7-A1-01/TASK_REPORT.md", "R4-10.md::Q-10-01", "S2/S2/确认",
     "证据件 A-Gate 的「机器一致性 PASS」前提所依赖的门从未登记进 `eng/ci/checks.json`，且其快照检查数已与工具分叉",
     "TASK_REPORT.md:7; TEST_REPORT.md:3; REVIEW_REPORT.md:3; QA-V19R7-A2-05/EVIDENCE_INDEX.md:18; eng/tools/docs_machine_consistency.py; artifacts/e…",
     "eng/ci/checks.json（`CHK-DOCS-MACHINE-CONSISTENCY` + `CHK-DOCS-MACHINE-CONSISTENCY-SELFTEST`）; TASK_REPORT.md:7; TEST_REPORT.md:3; audit_stats.json:73-76",
     "仍在（仅存「快照与工具分叉」半；「未登记」半已修）",
     "登记半**已收口**：解析 `eng/ci/checks.json` 现得两条在册项 —— `CHK-DOCS-MACHINE-CONSISTENCY`（command `python3 eng/tools/docs_machine_consistency.py --json-out run/ci/docs-machine-consistency/docs_consistency.json --quiet`，profiles=linux-main/prerelease，waivable=false）与 `CHK-DOCS-MACHINE-CONSISTENCY-SELFTEST`（command `… --self-test`，profiles=fast/linux-main/windows-main）⇒ 「从未登记」在当前树不成立。分叉半仍在：被点名的行本身逐字未改 —— `TASK_REPORT.md:7`「结论：DONE，9/9 PASS，0 broken，可进 A1-02/A2。」、`TEST_REPORT.md:3`「验证：`python3 eng/tools/docs_machine_consistency.py` 退出码 0，JSON 生成，9 项检查全 PASS。人工复核 10 项检查逻辑未改动，仅 ROOT 计算变更」，而工具现为 `:696 version=2.0.0` / 11 项判据（`run_checks():315-412`）。分叉只在 `audit_stats.json:73-76`（`checks 9` / `checks_live 11` / `superseded_by`，`5f8c237b` 新增）被登记，A1-01 报告内的结论句无一行改写或加注 ⇒ 以该证据件读 A-Gate 前提，仍会取到一个已被现役判据族否定的通过值。",
     "高",
     "是（低优先）：A1-01 的 TASK_REPORT/TEST_REPORT 结果句须按 `checks_live` 与现役版本改写为「历史快照」或就地标注失效，不得保留「9/9 PASS、可进 A2」作为在册结论"),
]

flush(ROWS)
print("batch1 ok:", [r[0] for r in ROWS])
