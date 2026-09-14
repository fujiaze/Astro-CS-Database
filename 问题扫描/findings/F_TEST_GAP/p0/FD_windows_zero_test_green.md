# 前台定稿（producer=FD）· F_TEST_GAP × P0 · E4 证据由前台落档（E 层被限制只可追加不得立条，故由我承接）

> 取证主体：E4（执行层，白名单只读）。条目化与定级：前台。E4 未自开条目，已在 §「需前台裁决」点名移交。

### FD-F-003 唯一跑 Windows C++ 单测的门连续 10 次"0 用例 PASS"，且空输出被 `EMPTY_OUTPUT_SILENCE_EXEMPT` 制度化为豁免
- **ID**：FD-F-003 ｜ **类别**：F_TEST_GAP ｜ **优先级**：**P0**（§17.1 发布门禁面：Windows 是正式支持平台，§15.3/§15.4 把 Windows 复验列为发布前置）
- **位置**：`ci_windows_driver.py::stage_plan:344-356`（缺"用例数>0"断言）；`ci/run.py:103-105 EMPTY_OUTPUT_SILENCE_EXEMPT`；`ci/checks.json::WIN-TEST-UNIT`、`::API-DOCS`、`::WARNING-SUPPRESSION`（其 `detect_build_tree()` 不认 `build/win-msvc-17.14.39-x64`）；对照 `deep_ci_driver.py:248`（持相反说法）
- **证据（E4 从 28 份真机 `CI_RESULT.json` 留痕复原，全部 `platform_runtime=windows`）**：最新一份 68 项运行 = `total 68 / pass 64 / fail 4`，**4 个 FAIL 全在 Windows 产物链**（configure 缺 ZLIB/GSL、install 失败、`wf_step fail-closed: 登记必需产物缺失`）；
  而 `WIN-TEST-UNIT` 最近 **10 次真机连续 PASS**，对应 stage 日志逐字为 `ctest --preset win-rel` → **`No tests were found!!!`**、junit **`tests="0"`**、`<testcase>` 元素计数 **0**。
  ⇒ **Windows 上最后一次真正跑过 C++ 用例的运行是 2026-09-07（57 用例全绿，且该清单内无 publish 用例）**。另两条同族：`API-DOCS` PASS 而 **stdout 与 stderr 双空**（被 `EMPTY_OUTPUT_SILENCE_EXEMPT` 写成豁免），`WARNING-SUPPRESSION` 自宣 `QA-001_SKIP: 无可用构建树` **仍 PASS**。
- **问题说明（三种失守合流，是簇 1 四机制里"④子串/空断言"的最重实例）**：①**采集数>0 缺断言** ⇒ 0 用例等于全绿；②**空输出被制度化豁免** ⇒ 门连"我检查了什么"都不留；③**自宣跳过仍记 PASS** ⇒ SKIP 与 PASS 在记账面上同色。
  三者叠加的效果：**Windows 侧的"已验证"是一句无人否证的声明**，而它在 `ctest_baseline`、`CI_RESULT`、审核包与 `RELEASE_STATUS` 类文本里被反复当作已完成的验证证据引用。**这与 M9 §4 平台盲区是同一失守，但本条是它的记账面，比"没跑"更糟：账面显示跑过。**
- **依据条款**：宪章 §15.3/§15.4（Fatduck 是正式验证节点、验证职责矩阵）、§17.1（发布前全部偏差须登记）、§17.10（P0/P1 清零才可发布）、§12.3-4（测试须能证伪）；AGENTS.md「状态机」要素（waiver 唯一入口、严禁用 waiver 掩盖红灯）
- **建议处置**：①在 `stage_plan`/`run_ctest` 两侧都加"用例数>0 否则 FAIL"（**与 C-02 第四向同批实现**，且 `deep_ci_driver.py` 那句相反断言必须同时删，否则两门互斥）；
  ②废除 `EMPTY_OUTPUT_SILENCE_EXEMPT` 或把它改成"必须显式登记豁免理由到 checks.json 的 waived 字段"（**注意：废除会让一批静默门立刻变红，需一轮修门排期，故须你拍 A-38②**）；③引入第四态 `SKIPPED` 并计入覆盖率，禁止与 PASS 同色；④Windows C++ 单测面**在恢复"用例数>0"之前不得作为发布证据引用**。
- **取证口径**：E4 只读解析 28 份 `run/**/CI_RESULT.json` + stage 日志 + junit（未跑任何 ctest、未连远端）；前台独立复验 `find run -name CI_RESULT.json -newermt 2026-09-01` = **169 份**（含非 windows）⇒ 留痕量充足，不是孤例
- **置信度**：高（真机留痕三类互相印证：CI_RESULT + stage 日志 + junit 计数）｜ **related**：**M9.md §4**（平台盲区总述，本条补"记账面"这一层，不并档）、**FD-F-001/F-002**（子串断言守卫）、**FD-G-002/G-004**（红被豁免 / 三态混记）、**M5b-G-01**（门验错对象）、C-02 第四向、A-38
