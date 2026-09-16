# 验收记录（ACCEPTANCE）

> 本包尚未执行。`PASS` 只可由前台（调度员）独立复跑后填写（见 `OPERATOR.md §3`）。
> 填写要求：证据路径必须指向本次复跑产生的日志或报告；不得复用执行 Agent 的自述。

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| BASE-001 | NOT_STARTED | — | — | — |
| GOV-001 | NOT_STARTED | — | — | — |
| DOC-001 | NOT_STARTED | — | — | — |
| DATA-001 | PASS | 门1 32 schema 全可加载 / 31 唯一 $id 0 重复 / 14 对象各恰 1 份 canonical；门2 四负例真实必判红（n1 signal 2 errs、n2 variance 13、n3 rejection 8、n4 port_contract 2）；门3 tests/contracts Ran 63 OK（基线 27 个测试名 0 消失，+35 新增）；门4 本任务足迹 100% 在允许域；门5 追加门 7 旧 ID 全判定（4 mapped / 3 no_canonical、pending=0）；门6 证据非循环自证 EVIDENCE_NOT_CIRCULAR=True；U-02 口径 4 条机器断言单跑 rc=0 | run/PROJECT-GOVERNANCE-01/DATA-001/verify-logs/（独立复跑）+ logs/（执行）+ run/PROJECT-GOVERNANCE-01/BOUNDARY-w1/（冻结基线与扰动登记） | 独立验证 门1-5+U-02+反作弊 全 PASS、属实性 PARTIAL（唯一扣分=4 条 old_contract_face 误引本次新增的 INDEX.yaml 行，属循环自证）；已按证据缺陷打回，执行者最小更正（改基线旧面真命中 + 新增 new_index_added_by_DATA001 字段 + 新增门6 机器门），调度员复跑 rc=0（legacy 7 条 / old_contract_face 0 处 INDEX.yaml / 63 tests OK）；越界=0（P-05：cli/** 归 CLI-001；lib/** 归 ARCH-001；docs/science+algorithms 为外部锚点刷新）；遗留：3 条 no_canonical（DATA-P1-SOURCES / DATA-P2-SMP / DATA-GAIA-001）需 MOD-001 侧在 MODULE_MAP.yaml 改引真实归属面 |
| CFG-001 | PASS | 门1 三模板过各自 schema rc=0（另直调 jsonschema_min 0 errors）+ 88x 键路径扫描无硬件字段；门2 四负例各 rc=1 且失败串精确命中（filter enum / required output_dir / precision enum / 4×additionalProperties）+ 12/12 模板变异被拒；门3 defaults 44==44==44、unknown=0、4 项 pending_authority value 全 null、5 项 owner_adjudicated 出处独立核实；门4 v2∩phase_config=∅（legacy {precision} 已登记）；门5 filters 45 条逐值全等 + 双 sha256 相符（76e919a0…/e0a44393…，迁移前后不变）+ 零点键路径/全文命中 0；门6 tests/config Ran 40 OK；门7 授权三处逐条属实（+config only/§7 仅 1 insertion/34→35）+ required_dirs(20)==§7 目录集；门8 足迹 28 项全在域、越界 0 | run/PROJECT-GOVERNANCE-01/CFG-001/SELF_VERIFICATION.md（执行）+ verify-logs/VERIFICATION_REPORT.md（独立复跑，HEAD 2fc19b21→58abbd19 窗口） | 独立验证 8/8 门 PASS、属实性 **CONSISTENT**（数值逐字可复现）；待负责人追认：cpu_profile legacy v1 面 5 处与实现收敛的放宽（feature_bits/xcr0/backend_sha256/block_size min/oracle_status，均带 profile_gen.cpp 行号并登记 x-astrocs-v1-drift）；另登记：created_utc 跨类同名未登记、门7 直跑受并发 CI 根产物（checks/、ci_result.json）干扰需隔离、三处域外登记面（DOCUMENT_INDEX/test_index/checks.json）、提交等 ARCH-001 停稳；ci/root_manifest.json 为与 RETIRE-001 的**共享提交**文件 |
| MOD-001 | PASS | 门1 23/23 唯一行 rc=0；门2 --selftest PASS(8 cases) + 五类负例 rc≠0（含 facade/no-op）；门3 tests/quality 失败集合 ⊆ 冻结基线（基线 3 条仍在、skip 集合逐条相同、无新增归因本任务；新增 test_05 独立复现归因 build/ 清运）；门4 正向状态 0（NOT_IMPLEMENTED 22 / CONTRACT_READY 1，真实仓库诚实红灯 rc=1） | run/PROJECT-GOVERNANCE-01/MOD-001/verify-logs/（独立复跑）+ run/PROJECT-GOVERNANCE-01/MOD-001/logs/（执行）；冻结基线 run/PROJECT-GOVERNANCE-01/BOUNDARY-w1/quality-baseline.txt（sha256 2a373529…） | 独立验证 Agent 5/5 门 PASS（锚定 HEAD=900916fb），调度员抽查一致（HEAD=b46a316f）：23 行/23 唯一、checker rc=1 且正向状态 0、ci/checks.json 0 删除且 CHK-ROOT-CLEAN 完整、越界=0（足迹=4 新文件 + 1 条登记项）；遗留：① 该登记项误随 ROOT-002 的 e5fba371 入仓，前台将做归属修正提交；② ci/known_failures.json 未登记本门当前红灯（CI-001 域，需负责人定豁免口径）；③ 映射表 7 个旧 DATA-* ID 待 DATA-001 补 canonical 映射 |
| ARCH-001 | NOT_STARTED | — | — | — |
| AIO-001 | NOT_STARTED | — | — | — |
| RT-001 | NOT_STARTED | — | — | — |
| CLI-001 | NOT_STARTED | — | — | — |
| CLI-002 | NOT_STARTED | — | — | — |
| CLI-003 | NOT_STARTED | — | — | — |
| P1-001 | NOT_STARTED | — | — | — |
| P1-002 | NOT_STARTED | — | — | — |
| P2-001 | NOT_STARTED | — | — | — |
| P2-002 | NOT_STARTED | — | — | — |
| P3-001 | NOT_STARTED | — | — | — |
| P3-002 | NOT_STARTED | — | — | — |
| CPU-001 | NOT_STARTED | — | — | — |
| OBS-001 | NOT_STARTED | — | — | — |
| INT-001 | NOT_STARTED | — | — | — |
| CI-001 | FAIL（第二轮已交，剩余项待裁决/他任务依赖） | 门1 注册表结构 rc=0/checks=38/error_count=0；门2 迁移覆盖 146/146 无静默丢弃（独立复算 missing=0，136/2/3/6/0 逐项 MATCH）；门3 run_checks.py 入口可用、--all 实跑 230.7s 出 JSON（pass=35/fail=33）；门4 旧入口 run.py plan rc=0/selected=26、wf-binding PASS；门5 三吸收项在位、linux-main 末位执行单元正确；门6 点名 P0 项（API-DOCS 缺产物 fail-closed、ctest-target）PASS 但整体 PARTIAL（抽 3 处仍静默通过）；**门7 逐门极性证据 FAIL（缺项）**；门8 边界/反作弊 PASS | run/PROJECT-GOVERNANCE-01/CI-001/verify-logs/VERIFY-REPORT.md（独立复跑）+ logs/（执行）；写窗口基线 sha256 62d051cc→f2535f47 | 独立验证总判 **FAIL**：门7 未交付逐门正负例；另发现**执行者未披露的自伤回归** ci/tests/test_ci001b_*.py rc=1（33 测试 6 失败，收敛前 PASS，根因 WIN-PACKAGE-CANDIDATE 降为 step + wf-binding 索引扩到 unit）；GAP-027 仅修点名项（另 17+ 处抽 3 处复现属实）；越界=0（足迹全在 ci/**+tools/quality/deep_ci_driver.py）；外部归因：4 个 step 因 ARCH-001 迁移在 20 分钟内翻红。已派第二轮：A 自伤回归 → B 极性证据 → C GAP-027 P0/P1 全量闭合+_repo 严格 → D --check 多值 → E 3 个 RESERVED 门 → F 元数据瑕疵 | 【第二轮结果】A 自伤回归 DONE（ci/tests/test_ci001b_*.py Ran 33 OK，修前 6 failures，逐条给等价论证）；B 门7 极性证据 PARTIAL（ci/polarity_evidence.json：31 门中 5 门本地双极实证 + 7 inconclusive + 19 not_proven 逐条原因/归属，未冒充绿）；C GAP-027 DONE（12 处 fail-closed 含真凶 check_api_docs._repo() 静默回落→严格 FAIL + --allow-repo-fallback + 机器门负例；evidence/gap027_audit.{md,json}；P2 12 条行号登记）；D --check 多值 DONE；E 3 个 RESERVED 门 PARTIAL（CHK-FMT→CI-002 工具供给、CHK-DUAL-TOL→REAL-001 Windows 产物、CHK-AGENT-HARD-RULES→GOV-001(W2)，登记 reserved_targets 不注册假绿）；F 元数据 DONE（order_in_target 3→1）；红项归因 29=P0 20+P1 4+旧权威 5，未用 waiver 掩盖；checks.json sha 未变 f2535f47。**新增违规（待路由）**：ci 运行产物 checks/（68 文件）与 ci_result.json(179KB) 落仓库根（18:12），致 CHK-ROOT-CLEAN 由绿转 FAIL（violations=2）——违反 ENGINEERING_SPEC §7；根产物的清运不属我这条线，需前台路由 |
| QA-001 | NOT_STARTED | — | — | — |
| PKG-001 | NOT_STARTED | — | — | — |
| REAL-001 | NOT_STARTED | — | — | — |
| FINAL-001 | NOT_STARTED | — | — | — |
| ROOT-001 | PASS | 账本覆盖 133/133；分类 A68/B4/C0/D4/E34/F23 | run/PROJECT-GOVERNANCE-01/ROOT-001/logs/ + reports/PROJECT-GOVERNANCE-01/root/** | 前台复跑：根条目 133→61、git ls-tree 同次执行 5367/5367 不变、删除项指纹齐全；A/B 清运已执行、设计大纲 344 tracked 由前台提交 4511712b |
| ROOT-002 | PASS | 检查器正例 rc=0（verdict PASS、violations=0）；1 正 3 负 Ran 5 OK；注册表 145→147 项错误数 1→1（无新增） | run/PROJECT-GOVERNANCE-01/ROOT-002/logs/ + run/ci/root-cleanliness/ | 前台复跑一致；tests/quality 全量 rc=1 的失败为既存/外部（归 TEST-GREEN-001 与 CI-001），不计入本任务；ci/checks.json 写入已冻结交棒 |
| ROOT-003 | PASS | RETENTION.md 221 行；27 个被引用路径 EXISTS=27/MISSING=0；70 个无引用目录清运 | run/PROJECT-GOVERNANCE-01/ROOT-003/logs/ | 前台复跑一致；run/ 102G→97G；>1G 目录（perf-fix 57.5G / release-rescue 27.6G）因被 tracked 报告引用而保留，登记 GAP-029 |
| ROOT-004 | IN_PROGRESS | 分片 26/26、条目 785/785、列数不合格 0、重复 ID 0；四态 OPEN 733/RESOLVED 41/VOID 7/UNVERIFIABLE 4；P0 93 全覆盖 | reports/PROJECT-GOVERNANCE-01/root-scan/**（CONTINUATION.md 491 行） | 分片层门已满足；剩余=合并出表（REBASE_TABLE/SUMMARY/P0_RECHECK）+ 第 8/9 列按当时树重跑；任务已移交负责人另派 agent |
| ROOT-005 | PASS | 根清洁检查器 rc=0（verdict PASS、violations=0、根条目 54） | run/PROJECT-GOVERNANCE-01/ROOT-005/logs/ + run/ci/root-cleanliness/rc2.json | 前台复跑：CS/、worktrees/ 已删；AstroCS.wiki/ 保留（待 WIKI-001）、engineering/ 保留（root_manifest required_dirs）；logs/ 旧日志移入 run/archive/ |
| ROOT-006 | PASS | 【归属更正】23acb453 的 stat 含 ARCH-001 在途 rename（git mv 入索引 + 前台提交未带 pathspec），内容合法但归属挂错，不改历史，见下方流程修正记录；排除保证自测 rc=0（8/8）；检查器正例 pack rc=0×3、tracked rc=0；负例 22 项 OK | run/PROJECT-GOVERNANCE-01/ROOT-006/logs/FINAL_EVIDENCE.txt + reports/PROJECT-GOVERNANCE-01/security/** | 前台独立复跑一致（22 tests OK、pack 连跑 3 次 rc=0/0/0）；裁决：不轮换（正文无凭据）、不 git rm --cached（52 处路径引用）；CHK-SECRET-HYGIENE 并入 CI-001 收敛；待负责人确认旧审计包是否已外发 |
| ROOT-007 | PASS | 删除 4 个作废世代根文档 + evidence/**（2799 文件）+ 9 个旧审计脚本 + 1 探活日志；artifacts/** 按裁决删除 | reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/** + git 历史 | 前台执行并提交（01db973b/7940d70e/4e95f841/b1290525/d414c3e0）；归档副本按负责人第二轮裁决一并删除；根条目 61→54 |
| WIKI-001 | NOT_STARTED | — | — | 依赖 GOV-001（权威收敛）；push 属发布类动作须负责人批准 |
| TEST-GREEN-001 | IN_PROGRESS | 原 4 项红灯已全绿（锚点 rc=0 808 锚 0 error；API-DOCS fail-closed + 3 新例；单位歧义 rc=0；R10 次序 error_count=0） | run/PROJECT-GOVERNANCE-01/TEST-GREEN-001/logs/ | 进行中：六层追溯门退役（GAP-032 裁决）+ tests/api 夹具重接线 + docs/ci 登记；前台复跑 `validate_registry --strict` → error_count=0（147→146 系 TRACEABILITY-CODE 退役） |
| RETIRE-001 | NOT_STARTED | — | — | 依赖 TEST-GREEN-001 交出 ci/checks.json 写权；对象=引用已删 artifacts 路径的旧世代打包/审计工具 |

## 控制包级门

- [ ] 所有任务 PASS（且每条 PASS 都有前台独立复跑证据）；
- [ ] GitHub Linux/Windows CI 在同一 SHA 全绿；
- [ ] Linux 最终 SHA 真实数据流通过；
- [ ] 图像审核（Agent 初审）通过，Owner 终审留白；
- [ ] Windows/Fatduck 同 SHA 复验通过（未完成则如实记 AWAITING_WINDOWS_VALIDATION）；
- [ ] FINAL-001 汇总经负责人审阅；
- [ ] 无 FAIL / BLOCKED / UNRESOLVED 冒充完成。

## 登记模板（复制到对应行）

```text
状态：PASS / FAIL / BLOCKED
机器门结果：<逐门一句话 + rc>
证据路径：run/PROJECT-GOVERNANCE-01/<TASK>/logs/…
前台结论：<复跑是否与执行 Agent 自述一致；有无越界；遗留项>
```
