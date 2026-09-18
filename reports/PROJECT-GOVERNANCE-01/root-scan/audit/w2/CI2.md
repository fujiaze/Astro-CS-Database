# W2-CI2 · 第二波文件级审计 —— ci/** + .github/**（穷尽域）

- 分片：W2-CI2（文件级穷尽，每文件必判）
- 基线（开工）：'timeout 30 git rev-parse HEAD main origin/main' → HEAD=main='180c8a0a'，origin/main='b4afc135'（三 SHA 不等；按规程 2 分钟后重试一次仍不等——期间 HEAD 推进至 59981aa9/23f42ffa——依口径以 **HEAD=main 相等** 为准开工，origin 滞后已注明）。
- 基线（收工）：HEAD=main=origin/main='01754fab'（审计期间并行线 CI-001 把注册表从 146 项重写为 35 聚合项并推送，HEAD 三次移动）。
- **审计窗口污染声明**：取证期间 ci/checks.json（146→35 聚合项重写）、ci/checks.schema.json、ci/validate_registry.py、ci/validate_workflow_binding.py、ci/wf_step.py、ci/root_manifest.json、ci/impact_map.json 被并行线 CI-001 在途修改（M；改前/改后均取证），并出现 **UNTRACKED 新文件** ci/run_checks.py、ci/exemptions.json、ci/id_migration_map.json。发现表按**各条标注时刻的已提交/工作树状态**取证；凡「缺失面」在窗口末已有 CI-001 WIP 同向补齐的，逐条注明，未提交/未接线前仍判 OPEN，不判 RESOLVED。
- 权威口径：ASTROCS_DESIGN §0 权威链；ENGINEERING_SPEC §2/§7/§8；docs/ci/01_CHECKS §1–§5、02_PIPELINE §1–§3、03_GATES §1–§5、CI_SPEC；同源映射按 SHARD_BRIEF §1/§2。第一波 CI 轴（audit/CI.md）因基线核对**未开工、0 发现**——本报告为该域实质首轮取证，「第一波同源」多为 AUDIT_INDEX 跨轴共性而非 CI.md 本体。

## ① 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据 | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门 | GAP/任务·第一波·旧账本同源 |
|---|---|---|---|---|---|---|---|---|---|
| W2-CI2-1 | ci/checks.json（开工基线全表）＋ ci/checks.schema.json:17-27 ＋ docs/ci/01_CHECKS.md:75-79(§5) | docs/ci/01_CHECKS §1/§4（唯一注册表；登记含门禁级与正负例）、§5（运行方式 ci/run_checks.py）；ENGINEERING_SPEC §8 | 命令：python3 解析对比（本轮真跑）；输出1：「docs CHK not in registry (26): [CHK-BUILD-LINUX … CHK-RESOURCE, CHK-PACKAGE]」（开工 WT 146 项中按 docs 28 行同名注册的仅 CHK-MODULE-MANIFEST/CHK-ROOT-CLEAN/API-DOCS=3）；输出2：「ls: 无法访问 ci/run_checks.py: 没有那个文件或目录」；输出3：注册项键集无 gate/severity 字段 | P1 | 声明层 27 CHK-*（含 P0 的 ORACLE/INVARIANT/RESOURCE/PACKAGE）与执行层 146 项两套账；门禁分级无机器表示；§5 入口在 HEAD 不存在；「docs 声明⇒注册」无对账门 | 按 CI-001 在途方向收口：聚合 CHK-* 注册（run_checks.py+steps 已现形）＋新增对账门「01_CHECKS §2 每行 ⇒ checks.json 同名 id 或 migration_map 覆盖」单命令 rc 断言；schema 增 gate 字段 | ci/** | 对账脚本（docs CHK ⊆ registry ids ∪ migration_map）rc=0 | CI-001/QA-001；AUDIT_INDEX 跨轴共性1（PKG-1/RT-1/QA-8 同形）；SHARD_BRIEF §2「旧 145/147 项 CHK-*」行；第一波 CI.md 未开工无本条同源 |
| W2-CI2-2 | .github/workflows/fatduck-admin.yml:20-22（对照 :3 注释） | ASTROCS_DESIGN §11.2/§11.3（VERIFIED 只认真实验收锚）；docs/ci/02_PIPELINE §1/§5（fatduck 面触发仅 workflow_run/schedule；真实数据终验由负责人触发）；AGENTS.md §5（凭据纪律）；ci/toolchain.policy.json fatduck 节（compile_allowed=false, checkout_allowed=false） | 命令：read 本文件；输出1：shell: cmd ＋ run 体直接插值 inputs.cmd（workflow_dispatch 输入零约束进入 self-hosted fatduck-realdata 节点）；输出2：注释自称「以 pwsh 执行」与实际 shell:cmd 不符；输出3：无 environment/approval、无步骤白名单、无审计产物 | P0 | 任何具 write 权限者可令真实数据验证节点执行任意命令：伪造/篡改 fatduck-public 产物与 FATDUCK_PUBLIC_SUMMARY.json、并向负责人评审 issue（fatduck.yml:191-229 直连 gh api）投毒 ⇒ VERIFIED 证据链与节点凭据面双失效；绕过 toolchain.policy fatduck 边界 | 删除该 workflow，或收口为：inputs 仅接受枚举 step_id（经 ci/wf_step.py 声明派发）＋ GitHub environments 人工 approval ＋命令审计落盘 | .github/** | grep inputs 该文件零命中（或文件删除） | ROOT-006（Fatduck 凭据入仓）同源叠加；CI-001/OBS-001；GAP 无同号（新立）；旧账本无同条 |
| W2-CI2-3 | ci/checks.json::CI-BINDING-TESTS（command 末段 -p test_ci001b_*.py）＋ ci/tests/（21 个 test 文件中 19 个无门消费） | ENGINEERING_SPEC §8（每项检查能红能绿；修改后复跑对应检查）；docs/ci/01_CHECKS §1 | 命令1：注册表命令全扫——引用 ci/tests 的仅 1 条且带 -p test_ci001b_*.py 过滤；命令2：PYTHONDONTWRITEBYTECODE=1 python3 -m pytest ci/tests -q（17:0x 近净窗）；输出：「14 failed, 365 passed in 74.04s」（红项含 strict 注册表校验、fast 计数 67≠61、impact_map schemas 域缺失、ctest 注册闭包、V6-DETERMINISM requires_monitor=true 但 command 无监控包装器）；命令3（registry 重写后复跑）：64 failed, 315 passed | P1 | CI 元防线（365 项回归守注册表/绑定/画像漂移）基本不在任何 profile 执行面上 ⇒ 漂移可合入 main 而门全绿；红测试长期无人见，「能红能绿」只剩 rc 面 | 将 ci/tests 全量注册为门（去掉 -p 过滤或新增聚合 CHK-CI-SELFTEST），红项随 CI-001 注册表重写一次性对表 | ci/**（tests 目录内） | ci/run.py --profile fast 的选中集含全量 ci/tests 消费，且 pytest ci/tests -q rc=0 | CI-001/QA-001；旧账本 V9-b（validate_registry 未注册同族）；第一波 AUDIT_INDEX QA-2 同「现势红」族 |
| W2-CI2-4 | ci/checks.json::UT-GAIA-ZLIB（开工 HEAD=180c8a0a 在册，profiles fast/linux-main/windows-main）＋ ci/validate_registry.py（未接线） | ENGINEERING_SPEC §7（修改代码/测试后同步订正 ci/checks.json）、§8 | 命令1：ls -d lib/gaia_xpsd_client → 「没有那个文件或目录」（lib/ 17 项无该模块）；命令2：python3 ci/run.py --check UT-GAIA-ZLIB --output-root /tmp/… → 「FAIL(prerequisite)；prerequisite 未满足：command 引用的仓库路径不存在：lib/gaia_xpsd_client/tests」rc=1 ⇒ 三 profile push 恒红；命令3：validate_registry --strict → 「R4 checks[106].command -s: dir not found … verdict: FAIL」rc=1，而该 strict 校验器在 checks.json/workflows/impact_map 0 引用（grep -c 全 0） | P1 | 悬空注册使三 profile 现势恒红（狼来了吞真红）；能抓它的结构校验器本身不成门 ⇒ 注册表完整性无人守。窗口末 CI-001 重写后 strict 对 35 聚合项 PASS（未提交），方向一致但已提交状态仍 OPEN | checks.json 同步删改该 step 目标（随 ARCH/INT 迁移对表）；validate_registry --strict 注册进 fast 门 | ci/** | validate_registry --strict rc=0 且该检查经 runner 非 FAIL(prerequisite) | CI-001/INT-001（ARCH 迁移配套）；第一波 QA-2 同「main 现势红灯」族 |
| W2-CI2-5 | docs/ci/01_CHECKS.md:7 ↔ ci/**（全树）；ci/checks.json waivable 7 项；ci/run.py:82-83,1043-1066 | docs/ci/01_CHECKS §1、03_GATES §1/§4（豁免登记 ci/exemptions.json 只减不增、负责人批准；禁 waiver 掩盖红灯）、CI_SPEC:53 | 命令1（开工）：ls ci/exemptions.json → 「没有那个文件或目录」；命令2：grep exemptions ci/*.py ci/*.json → 0 命中（无消费者）；命令3：waivable=True 共 7 项（TESTKIT-LIST/WORKSPACE-ADOPTION/RECONCILE-STATE/DEEP-CLANG-BUILD/DEEP-SAN-TSAN/DEEP-COV-PY/DEEP-COMPLEXITY）；run.py 中 prereq 缺失/platform 不符/exit77 → SKIPPED(waivable) 且「passed and skipped ⇒ 总 verdict=PASS」（:1064-1066 逐字在位） | P1 | 豁免治理合同无机器落位：waivable 布尔即事实豁免通道，P0/P1「无豁免/须登记」不可执行；SKIPPED 并入 PASS ⇒「环境性永跳」不可见（fatduck profile 0 检查 → FATDUCK_PENDING rc=0 同族，:1043-1045） | 落地 ci/exemptions.json 为唯一豁免源（waivable 由 ledger 驱动、只减不增断言）；SKIPPED 独立计数入报告门；（窗口末 UNTRACKED ci/exemptions.json high_water.max_entries=0 与 validate_registry R14 已在途——提交接线前仍 OPEN） | ci/**、docs/ci/** | validate_registry --strict rc=0（含 R14 豁免对账）且 ci/exemptions.json tracked | CONTINUATION.md:239 #18（前任复核实证候选）；旧账本 V9-b；CI-001 |
| W2-CI2-6 | ci/run.py:103-106 EMPTY_OUTPUT_SILENCE_EXEMPT={API-DOCS, UNIT-CLOSURE} ＋ :164-178 silent_failure | ENGINEERING_SPEC §8（每项能红能绿）；docs/ci/01_CHECKS §1/§2（API-DOCS=声明 P0 门）；03_GATES §2（IMPLEMENTED=当前提交实际执行通过） | 命令1：本轮真跑直测 → 「tools/check_api_docs.py rc= 0 stdout_bytes= 0 stderr_bytes= 0」「tools/check_unit_closure.py rc= 0 stdout_bytes= 0 stderr_bytes= 0」（双零字节静默）；命令2：白名单逐条核=2 项，均源码写死、未登记任何豁免台账，注释证据引 run/local/agent_ciqa_p2/*（run/ 为 gitignore 易失目录，仓库内无锚）；命令3：test_gap4 套件在场且负例能红（防线本体有效），但 :176 对这两 id 恒跳过内容级判定 | P1 | 唯一内容级反静默防线对两个常驻 fast 门永久关闭；API-DOCS 为 P0 声明的 doc↔code 命令树门，rc=0+零输出即 PASS ⇒「永远绿」形态固化（checker 主体在 build 缺失时 if-isfile 静默跳过的旧证=GAP-027；主体属 tools/** 他域，本条按机制面立） | 白名单迁入 ci/exemptions.json（负责人逐条批准）或强制两门输出 verdict 行（非零字节留痕）后删名单 | ci/**（联动 tools/check_api_docs.py，他域） | 装载 run.py 后 len(EMPTY_OUTPUT_SILENCE_EXEMPT)==0，或两 id 有 ledger 批准记录且直跑 stdout>0 | 旧账本 FD-F-003(P0)/V9-N-11(P2)/REBASE_TABLE:462,713 同源；GAP-027 同源不删条；第一波 CI.md 未开工 |
| W2-CI2-7 | .github/workflows/ci-windows.yml:24（对照 :20-23 注释与 ci-linux.yml:30-38） | docs/ci/02_PIPELINE §1（push 到 main：全量——每 SHA 应可归因）；ASTROCS_DESIGN §11.2（Windows x64 正式验证层） | 命令：read 两文件比对；输出1：linux concurrency group 含 github.sha（V3 B3-A7 修复注记逐字：「近 30 次 15 cancelled 且 total_jobs=0, 从未获得 runner → 每个候选 SHA 无归因证据」）；输出2：windows group 仅 ref+event_name、无 github.sha，其注释却自称「与 ci-linux.yml 同款口径」 | P1 | 连续 push 下 Windows run 在 pending 槽被后来者取代 ⇒ 部分 SHA 零执行零归因；fatduck 候选链依赖 windows success run（select_candidate 要求），缺口向真实数据终验/VERIFIED 面传导（与 FD-F-003 Windows 久跑无证据同族互证） | ci-windows.yml group 追加 github.sha（与 linux 同款）；validate_workflow_binding 增规则断言两平台 group 同构 | .github/** | grep -F github.sha 该文件 concurrency group 行命中 | 旧账本 RESCUE-P1-22/AUD-CI-LIVE F9 同族（linux 已修 win 未修）；FD-F-003 互证；QA-001 |
| W2-CI2-8 | .github/workflows/ci-linux.yml:21-24,44；ci-windows.yml:11-14,30；ci/select_profile.py:36-37 | docs/ci/02_PIPELINE §1（push=全量；schedule=每日一次全量）、§2（8-job 依赖 DAG）、§3（build 30/测试 45/打包 15 分钟分级超时） | 命令：read 对照；输出1：push→linux-main(136)、schedule(每日 19:17)→linux-deep（仅 6 项 DEEP-*＋BUILD-GCC），非「每日全量」；输出2：windows 无 schedule 触发；输出3：两平台均单 job timeout-minutes: 330，非 §2/§3 拓扑 | P2 | 「每日全量」实为每日 deep-only；Windows 无周期全量；流水线拓扑/超时与权威文档双账漂移（profile 承载 job 属可辩护实现，但 02_PIPELINE 未订正） | 订正 02_PIPELINE 如实描述（push=main 画像/schedule=deep 巡检/单 job profile 化），或按 §1 补每日全量与 win schedule；统一 timeout 口径 | .github/**、docs/ci/** | 工作流触发面/profile 选择与 02_PIPELINE §1 文本一致性断言（或订正后人工 diff 记录） | DOC-001/CI-001；§1「PR 若有 PR 流程」按 main-only 直推判 VOID（AGENTS §5/ENG §6） |
| W2-CI2-9 | ci/INVENTORY_REPORT.md（全文）、ci/WORKFLOW_BINDING_AUDIT.md（抬头）；ci/run.py:3-21 等注释密度；ci/known_failures.json:9-10,62-68；ci/tests/test_ci_repair_round.py:5；.github/workflows/fatduck.yml:221 | ENGINEERING_SPEC §2（注释禁堆历史版本号/任务编号/审计流水）、§7（报告落位 reports/）；ASTROCS_DESIGN §6.2/§10.1（唯一命令树；产品名 acsd_cli/ACSD Cli.exe）；SHARD_BRIEF §2（旧宪章非链上权威） | 命令1：read+解析 → INVENTORY 自称「70 项、fast=57」，实测开工注册表 146 项/fast=67（失效快照仍留 ci/ 并以现势口吻书写）；命令2：grep astrocs-cli fatduck.yml → 「astrocs-cli review open --run」（命令树无 review 子命令）；命令3：ci/** 通篇 V8-CI-00x/F-CI-002-04/07_CI_MACHINE_CONTRACT.md/05_FINDINGS_REGISTER/「冻结宪章 §17」引用（旧控制包文档与链外旧宪章） | P2 | 陈旧审计快照冒充现行状态误导读者、报告错置目录；任务编号/审计流水注释噪声违反 §2；通知文案铸造不存在的 CLI 合同面 | 两份 MD 移 reports/（加历史快照抬头）或删；注释按 §2 收敛只留设计原因；fatduck 通知改真实命令树文案 | ci/**、.github/** | git ls-files ci/*.md 为空（或文件首行含 ARCHIVED）且 grep astrocs-cli review .github/ 零命中 | DOC-001/GOV-001；旧账本 V9 注释流水同族；第一波无 |

**计数**：P0=1，P1=6，P2=2（共 9 条）。

## ② 覆盖清单

口径：开工枚举（HEAD=180c8a0a）与收工枚举逐字一致（git ls-tree -r 180c8a0a -- ci/ .github/ 与 git ls-files ci/ .github/ diff 空）= tracked 61（ci/ 57 + .github/ 4）；另按 git status --porcelain 补 3 个 UNTRACKED（并行线 CI-001 在途）。M 文件按取证时刻工作树判。

覆盖清单（TSV，tab 分隔）：

ci/INVENTORY_REPORT.md	FINDING:W2-CI2-9
ci/WORKFLOW_BINDING_AUDIT.md	FINDING:W2-CI2-9
ci/actions.lock.json	OK
ci/bootstrap.py	OK
ci/check_version.py	OK
ci/checks.json	FINDING:W2-CI2-1
ci/checks.schema.json	FINDING:W2-CI2-1
ci/ci_repair_round.py	FINDING:W2-CI2-9
ci/ci_result.schema.json	OK
ci/ctest_baseline.json	OK
ci/impact_map.json	FINDING:W2-CI2-3
ci/known_failures.json	FINDING:W2-CI2-9
ci/prepare_linux_fixtures.py	OK
ci/reconcile_state.py	OK
ci/resource_monitor.py	OK
ci/root_manifest.json	OK
ci/run.py	FINDING:W2-CI2-6
ci/select_candidate.py	OK
ci/select_profile.py	FINDING:W2-CI2-8
ci/steps/collect_bootstrap_diag.py	OK
ci/steps/linux_build_root_graph.sh	OK
ci/steps/linux_prepare_fixtures.sh	OK
ci/tests/__init__.py	OK
ci/tests/_helpers.py	OK
ci/tests/test_bootstrap_utf8.py	OK
ci/tests/test_ci001_failclosed.py	FINDING:W2-CI2-3
ci/tests/test_ci001b_binding.py	OK
ci/tests/test_ci001b_wf_step.py	OK
ci/tests/test_ci_repair_round.py	FINDING:W2-CI2-9
ci/tests/test_ctest_registration.py	FINDING:W2-CI2-3
ci/tests/test_deep_profiles.py	FINDING:W2-CI2-3
ci/tests/test_fatduck_workflow.py	OK
ci/tests/test_gap4_empty_outputs_guard.py	OK
ci/tests/test_impact_map.py	FINDING:W2-CI2-3
ci/tests/test_negative_guards.py	OK
ci/tests/test_resource_monitor_shim.py	OK
ci/tests/test_resource_probe.py	OK
ci/tests/test_run_monitored.py	OK
ci/tests/test_run_prefix_probe.py	OK
ci/tests/test_runner_contract.py	OK
ci/tests/test_runner_execution.py	OK
ci/tests/test_runner_selection.py	OK
ci/tests/test_verify_remote_run.py	OK
ci/tests/test_windows_ci.py	FINDING:W2-CI2-3
ci/tests/test_workflow_lock.py	FINDING:W2-CI2-3
ci/toolchain.lock.json	OK
ci/toolchain.policy.json	OK
ci/validate_candidate.py	OK
ci/validate_registry.py	FINDING:W2-CI2-4
ci/validate_workflow_binding.py	OK
ci/verify_actions_lock.py	OK
ci/verify_ciqa_report.py	OK
ci/verify_remote_run.py	OK
ci/verify_toolchain.py	OK
ci/verify_workspace_adoption.py	OK
ci/wf_step.py	OK
ci/workflow_binding.json	OK
.github/workflows/ci-linux.yml	FINDING:W2-CI2-8
.github/workflows/ci-windows.yml	FINDING:W2-CI2-7
.github/workflows/fatduck-admin.yml	FINDING:W2-CI2-2
.github/workflows/fatduck.yml	FINDING:W2-CI2-9
ci/run_checks.py	NA:moved-wip-UNTRACKED
ci/exemptions.json	NA:moved-wip-UNTRACKED
ci/id_migration_map.json	NA:moved-wip-UNTRACKED

统计：
- files_total = 64（tracked 61 + UNTRACKED 3）
- verdict_counts = OK:40 / FINDING:21（W2-CI2-1:2, -2:1, -3:7, -4:1, -6:1, -7:1, -8:2, -9:6）/ NA:3
- 枚举命令逐字：cd "/workspace/Astro CS Database" && timeout 30 git ls-files ci/ .github/ ；cd "/workspace/Astro CS Database" && timeout 30 git status --porcelain -- ci/ .github/

## 合规声明

本轮零修复、零 git 写（git 仅只读 rev-parse/ls-files/status/show/log/diff）；除本报告与 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/w2_CI2.log 外未写任何仓库路径（执行验证一律 PYTHONDONTWRITEBYTECODE=1、-p no:cacheprovider、产物根重定向 /tmp）；未读取/打印/复制 FATDUCK_ACCESS.md（本域仅文件名级引用，内容未触）；ci/tests 真跑临时物均落 tempfile//tmp；未触其他线工作区。
