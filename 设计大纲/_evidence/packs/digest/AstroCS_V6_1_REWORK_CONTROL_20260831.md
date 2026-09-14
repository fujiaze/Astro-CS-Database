# 包取证摘要：AstroCS_V6_1_REWORK_CONTROL_20260831

身份由脚本归一（去复原前缀与 .zip 后缀）。共 2 个实例。

## 【worktree】工程控制/AstroCS_V6_1_REWORK_CONTROL_20260831
- 文件 46 | 204.2 KB
- git 事件: {'first': ('8c71f7aba356092784f83c5e8f44f43a15623271', '2026-09-02T02:12:13+08:00'), 'last': ('8c71f7aba356092784f83c5e8f44f43a15623271', '2026-09-02T02:12:13+08:00'), 'dels': [], 'adds_n': 1, 'mod_n': 0, 'touch_last': None}
- 规格文件: 00_READ_FIRST.md, 01_ASTROCS_ENGINEERING_CONSTRAINTS.md, 01_INDEPENDENT_AUDIT_CONCLUSION.md, 04_TASK_SPECIFICATIONS.md, 05_GATE_CHECKLISTS.md, 06_ARCHITECTURE_ACCEPTANCE.md, 07_SCIENCE_AND_TEST_ACCEPTANCE.md, 08_CPU_RESOURCE_ACCEPTANCE.md, 09_DOCUMENTATION_AND_MACHINE_CHECKS.md, 10_LINUX_WINDOWS_RELEASE_FLOW.md, 11_GIT_MAIN_ONLY.md, 12_AUDIT_PACKAGE_SPEC.md, 13_OWNER_REVIEW.md, 14_PRIMARY_REFERENCES.md
- scripts/: known_failure_scan.py, package_audit.py, selftest.py, validate_control.py, validate_return_audit.py, validate_task_graph.py
- schemas/: audit_summary.schema.json, cpu_profile.schema.json, data_artifact.schema.json, module_descriptor.schema.json, pipeline_ir.schema.json, resource_summary.schema.json, task_result.schema.json
- templates/: ALG_CONTRACT.md, COMMITS.csv, FINDINGS.csv, LARGE_ARTIFACT_MANIFEST.csv, MODULE_README.md, OWNER_REVIEW_CHECKLIST.md, PREVIEW_MANIFEST.csv, RESOURCE_SUMMARY.csv, SCI_CONTRACT.md, SOURCE_IDENTITY.template.json, SOURCE_INDEX.csv, SUMMARY.template.json, TASK_RESULT.template.json, TEST_SUMMARY.csv
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 43, "mismatch": 1, "missing": 1, "mismatch_list": ["scripts/validate_return_audit.py"]}
- MANIFEST.json: {"schema": "astrocs.control-manifest/v2", "control_id": "AstroCS-V6.1-REWORK", "target_version": "0.10.0-alpha.2", "created_utc": "2026-08-31T03:21:20Z", "reviewed_audit_sha256": "137e3ed14ef85142f5897c93ac718f4f22713977282815e2ddd7288f62e6696a", "reviewed_audit_reported_commit": "b16d422a40fefedbdedab1749cfb8ebc06189736", "prior_control_sha256": "fdad8e40123a757cfcdd51429261ca2e9c915f640e69adba8eb449ee2850f689", "engineering_constraints_sha256": "dc47fbbbeccb239fc1834a985b357d6ded50080977708b6ea5c9811bf4de4b92", "task_ledger_sha256": "c6f21304d1835ca0816af600c565223cbab129ada5ce7d5a84caf3d285736822", "files": "[45 项] [{'path': '00_READ_FIRST.md', 'size': 5195, 'sha256': 'f08e7c5a13a2ae0ebd1
- START_PROMPT 开头: 解压并校验 AstroCS V6.1 返工控制包，严格读取 00_READ_FIRST.md、冻结工程约束、FINDINGS、TASK_LEDGER、TASK_SPECIFICATIONS 与全部 Gate checklist；只在 main 从 R0-001 连续执行到 REL-003，逐 Task 原子 commit 并立即 push。不得自行删改任务、放宽容差或设置普通人工停点。当前仅纯 CPU，ACR 不接生产；所有持续≥10秒的 heavy 节点必须使用 Runtime 多 worker并自动通过 CPU/内存/I-O资源门，低利用率或单线程立即失败。科学验证使用调用生产路径的独立合成 Oracle，不做历史版本全量对比。Linux 完成所有可做工作；Fatduck 离线仅标 WAITING_WINDOWS 并继续，在线后完成 MSVC、benchmark、少量真实、一次32R和六张真实视图。最终只能交付 validate_return_audit.py 通过的白名单审核包并声明 READY_FOR_OWNER_REVIEW，禁止代替负责人批准发布。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS V6.1 Alpha 重构打回控制包 ; ## 1. 结论与目标 ; ## 2. 最高优先级 ; ## 3. 启动动作（必须按顺序） ; ## 4. 不得自由发挥 ; ## 5. 每任务闭环 ; ## 6. 当前必须消除的事实 ; ## 7. 完成出口

## 【zip】工程控制/_control_packs/AstroCS_V6_1_REWORK_CONTROL_20260831.zip
- 文件 47 | 209.4 KB
- sha256: 903018212bfb584b1aaf0dd05b318d8171d1a5b3af43014ab020834e9359ede6
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: AstroCS_V6_1_REWORK_CONTROL_20260831
- zip 内 marker: 00_READ_FIRST.md, MANIFEST.json, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_ASTROCS_ENGINEERING_CONSTRAINTS.md, 01_INDEPENDENT_AUDIT_CONCLUSION.md, 02_FINDINGS.csv, 03_REWORK_TASK_LEDGER.csv, 04_TASK_SPECIFICATIONS.md, 05_GATE_CHECKLISTS.md, 06_ARCHITECTURE_ACCEPTANCE.md, 07_SCIENCE_AND_TEST_ACCEPTANCE.md, 08_CPU_RESOURCE_ACCEPTANCE.md, 09_DOCUMENTATION_AND_MACHINE_CHECKS.md, 10_LINUX_WINDOWS_RELEASE_FLOW.md, 11_GIT_MAIN_ONLY.md, 12_AUDIT_PACKAGE_SPEC.md, 13_OWNER_REVIEW.md, 14_PRIMARY_REFERENCES.md
- 00_READ_FIRST 开头:

# AstroCS V6.1 Alpha 重构打回控制包

控制包：`AstroCS-V6.1-REWORK`  
目标版本：`0.10.0-alpha.2`  
上轮控制包 SHA-256：`fdad8e40123a757cfcdd51429261ca2e9c915f640e69adba8eb449ee2850f689`  
被审审核包 SHA-256：`137e3ed14ef85142f5897c93ac718f4f22713977282815e2ddd7288f62e6696a`  
审核包自报 commit：`b16d422a40fefedbdedab1749cfb8ebc06189736`（尚未被接受为基线）  
开发分支：仅 `main`

## 1. 结论与目标

上轮结果正式打回。不得继承“81 PASS、Linux 侧完成、G11 PASS”等状态。本包不是要求回退到历史版本，也不是要求重复比较旧版；它要求在当前 `main` 上完成真正的生产架构、纯 CPU 并行、科学 Oracle、文档—代码闭环及双平台发布验证。

只有执行完本包全部非 Owner 任务，Windows 最终验证完成，审核包自验证通过后，Agent 才能输出 `READY_FOR_OWNER_REVIEW`。Agent 永远无权输出 `ALPHA_RELEASE_APPROVED`。

## 2. 最高优先级

冲突时依次服从：

1. 仓库根 `AstroCS_ENGINEERING_CONSTRAINTS.md`；
2. 本文件和 `03_REWORK_TASK_LEDGER.csv`；
3. 本包各专项合同；
4. 已核实的当前 SCI/ALG/DATA 合同；
5. 当前源码仅是待审对象，不能因为已存在就成为正确设计；
6. 历史报告、自报 PASS、旧测试日志仅作线索。

## 3. 启动动作（必须按顺序）

1. 在独立控制目录解压本包，运行 `python3 scripts/validate_control.py .`，必须得到 `CONTROL_PASS`。
2. 阅读 `01_INDEPENDENT_AUDIT_CONCLUSION.md`、`02_FINDINGS.csv`、`03_REWORK_TASK_LEDGER.csv` 和当前任务规格。
3. `git fetch origin --prune`；确认当前分支仅为 `main`，且 `HEAD == main == origin/main`。
4. 验证 `b16d422...` 是否为当前 `main` 祖先；若不是，记录为 `SOURCE_IDENTITY_FAIL`，禁止修改代码并汇报。
5. 创建仓库内本轮证据目录 `evidence/v6_1_rework/`，复制任务台账；不得

