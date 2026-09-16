# ACCEPTANCE_FINAL —— 「项目做完」的验收判据（前台定义，逐条须有实测证据）

> 依据负责人指令：**「把整个项目按照最高文档全部做好。在全部完成前不允许结束。」**
> 本文件把"做好"翻译成**可复跑的判据**。全部满足 ⇒ 目标达成；任一条不满足 ⇒ 目标保持 active。

## A. 构建与测试（阶段 0/4）

| # | 判据 | 当前值 | 命令 |
|---|---|---|---|
| A1 | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` rc=0 | ✅ rc=0 | 同左 |
| A2 | `ninja -C build -k 0` → **0 FAILED** | ✅ 0 | 同左 |
| A3 | `ctest --test-dir build` → **426/426 通过** | ⏳ 425/426（`core_pipeline`，QA-001 在修） | 同左 |
| A4 | `tests/cli` 全绿（或每条例外有依据与登记） | ⏳ CLI-002 在跑 | `pytest tests/cli -q` |
| A5 | `tests/backend`、`tests/quality`、`tests/contracts`、`tests/config`、`tests/artifact` 全绿 | 部分待 W4 线收口 | 各目录 runner |

## B. 机器门（阶段 0）

| # | 判据 | 当前值 |
|---|---|---|
| B1 | `check_doc_line_anchors.py --root .` rc=0 | ✅ `PASS: 38 docs, 842 anchors, EXEMPT:8, OK:834` |
| B2 | `ci/validate_registry.py --registry ci/checks.json --strict` → PASS / errors 0 | ✅ `error_count=0, verdict=PASS` |
| B3 | `check_registry_doc_sync` rc=0（注册表 ↔ `docs/ci/01_CHECKS.md §2` 双向差集空） | ✅（CI-003 交付） |
| B4 | `polarity_probe --check` rc=0（每门有极性记录） | ✅ 19/4/19（42 门） |
| B5 | `ci/run_checks.py --all` → 无未登记红；**豁免零新增** | ⏳ W4-A3 在收 28 个执行单元 |
| B6 | `CHK-ROOT-CLEAN` PASS（根条目=45，无未登记条目） | ✅ 45 条目 / violations 0 |

## C. 台账清零（阶段 1）

| # | 判据 |
|---|---|
| C1 | `reports/PROJECT-GOVERNANCE-01/scan-clear/REBASE_VERDICT.csv` 覆盖 **785/785** 行 |
| C2 | 四类计数之和 = 785，且与既有 `DISPATCH_MATRIX.md` 计数逐项对差已解释 |
| C3 | **`OPEN` 计数 = 0**，或每一项都已转入已派任务（含根因/方案/归属/验收门） |
| C4 | `CRITERION-WRONG` 清单每条含「为什么错 + 正确口径 + 依据」 |

## D. 冒烟测试（阶段 2，逐命令）

| # | 判据 |
|---|---|
| D1 | `normalize` / `mosaic` / `export` 各：`--help` rc=0 且参数与 `CLI_PROTOCOL_V1.md` 一致 |
| D2 | 各：最小合法输入 rc=0，产物落 `output_dir`，**同输入两次运行产物 sha256 一致** |
| D3 | 各：失败路径（缺参/坏配置/缺输入/非法组合）rc 符合 `ENGINEERING_SPEC §9`，诊断结构化，**不留半成品** |

## E. 端到端（阶段 3）

| # | 判据 |
|---|---|
| E1 | 真实数据全链路（normalize→mosaic→export）rc=0，产物 sha256 记录完整 |
| E2 | `export` 产物与 **astropy 7.0.1** 逐点对拍在冻结容差内（给 max 偏差与分布） |
| E3 | 台账精度数字**重锚**完成（旧值 → 新值 → 差异原因），不再有"修复前产品"的数字冒充当前值 |
| E4 | 确定性：同输入两次全链路产物哈希一致 |

## F. 文档与治理（贯穿）

| # | 判据 |
|---|---|
| F1 | 所有科学订正有 claim 登记（`SCIENCE_CORRECTNESS.md` 逐条含证据/影响面） |
| F2 | 无"已废止宪章"等悬空引用（活动树内 0 命中，或已加非规范声明） |
| F3 | 无"以代码为准"类权威倒置表述 |
| F4 | 根目录条目=45 且每一项在 `ROOT_LEDGER.md` 有归属与处置 |
| F5 | 工作区无我方未提交残留（隔壁 `问题扫描/**`、`reports/**/root-scan/**` 除外）；`HEAD = main = origin/main`，未推送 0 |

## G. 交付与可复现

| # | 判据 |
|---|---|
| G1 | 每个 PASS 均有**前台独立复跑**的证据（不接受执行行自述） |
| G2 | 每条 FAIL 均有最小复现命令 + 根因 + 归属 |
| G3 | `run/PROJECT-GOVERNANCE-01/**` 保留各线证据索引（自证摘要 + 日志） |