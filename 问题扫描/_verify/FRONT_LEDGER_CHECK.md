# 前台独立复核 · 账本与落位纪律（V 层之外，我自己跑的机械面）

## 一、20 条声称的回归锁：两阶检索结果
- **第一阶（我只数 `add_test(NAME …)`）报"14 处找不到"——这个结论是错的，已作废**：本仓大量用例走 `gtest_discover_tests`/套件名，不写 `add_test(NAME)`（E2 实测 `phase2_*` 113 例即此形态）。教训：**"缺失"必须先确认抽取面与被测对象的生成方式匹配**，这正是审计本身反复登记的簇 1 第四机制的反面教材。
- **第二阶（全真源 1,644 文件词边界检索）**：**13 在位 / 1 查无**。逐条：
  | 声称的锁 | 宿主 | 状态 |
  |---|---|---|
  | `by_coords_partial_miss` | `tests/unit/gaia_adapter_test.c` | 在位 |
  | `corner_exact` | `lib/phase2/tests/kcorr_lookup_test.cpp` | 在位 |
  | `P2EntryParity` / `zero_anchor_default` / `ControlGridMismatchRejected` / `Phase2Upm` | `lib/phase2/tests/synthetic_gate.cpp` | 在位（同文件四枚）|
  | `Phase2IvarWiring` / `IvarTileMissingFailClosed` | `lib/phase2/tests/ivar_wiring_test.cpp` | 在位（**ivar fail-closed 有专门测试文件**）|
  | `case_avg_085_boundary` / `case_alloc4_observed_0p9_core_fails` | `tests/unit/p2_workers_test.cpp` | 在位（**资源门分母改判后新增的边界用例，含 0.9 核必须失败** —— 直接对上 M5a-G-002）|
  | `validate_registry` | `ci/validate_registry.py` + `ci/tests/test_negative_guards.py` + `test_windows_ci.py` | 在位 |
  | `failure_sets_nonzero_exit` | `tests/unit/io_ownership_test.cpp` | 在位（**正是审计 M8-F-002 的根因件：failures 以前不进退出码**）|
  | `test_cli_single_install` | `ci/steps/linux_build_root_graph.sh` | 在位（但宿主是 **shell 步骤**不是 ctest 用例，锁的强度另判）|
  | **`ci_missing_prereq_fails`** | **全仓零命中** | **查无 → 账本失真**（登记在 `M8-F-003`）|

## 二、根目录落位纪律（AGENTS.md「目录规范（强制）」）
- **`astrocs_p1sess_{neg,perf,props,test}` 四个可执行文件在根目录且**未被 gitignore****（`git check-ignore` 判未 ignore）** ⇒ 既是"运行产物落项目根目录"的直接违规，又会永久污染 `git status`（隔壁自己写的 `UT-CLI` 基线 reason 承认的正是这一族）。mtime 09-12 17:16。
- `alloc_report.json` / `alloc_samples.csv`（今天 05:28）虽被 ignore，但 AGENTS.md 明确 "`resource_samples.csv`、`resource_summary.json`、`worker_balance.csv` … 落 `run/resource/`" ⇒ **同类资源产物落根目录 = 落位违规**（不是"ignore 了就合规"）。
- 根目录 `astrocs_run_*.json` 共 **56** 份，其中**今天新增 2**；同期 `run/cli_runs/` 有 96 份 ⇒ 规则**大体被遵守但在漏**，属"修复即扩散"型残余，建议给 CI 加一条"根目录出现 `astrocs_run_*.json`/`astrocs_p1sess*` 即红"的门（成本一次性）。
- `Testing/` 在根（09-10 起，已 ignore）⇒ AGENTS.md 已规定归 `run/Testing_archive/`，未归位。

## 三、我对隔壁修复工作的一句话评价（待 V1–V6 补完）
- 机械面看，**这批修复与以往最大的不同是"真的带锁"**：13/14 声称的回归锁可在真源定位，且锁的分布恰好落在本审计点名的根因件上（`io_ownership_test.cpp` 的退出码、`p2_workers_test.cpp` 的 0.9 核必须失败、新文件 `ivar_wiring_test.cpp`）。
- 唯一查无的那条 + 一条锁住在 shell 步骤而非用例 ⇒ 记为**账本失真/锁强度不足**两小条，不推翻整体。
- **但锁在位 ≠ 锁会红**：审计已证"在册≠执行"（M8-F-004、FD-F-001/002、L28e-E-001 三型）。这些新用例是否真进 ctest 采集、断言是否进退出码，由 V4/V6 与各域复核给结论。

## 四、我把上一节的"账本失真"改判了（自己纠自己一次）
- 我原写 `M8-F-003` 的锁"查无 ⇒ 账本失真"。**复验后改判**：`tests/cli/test_cli_single_install.py` **存在且修复真实**（`:35-39` 缺前置产物 `raise AssertionError`，注释直引"该门恒 SKIP = 未执行"；`:64/:81/:90/:100` 四处 `self.fail`）。真查无的只是**我按隔壁写的锁名 `ci_missing_prereq_fails` 检索**——真名是 `test_01_install_succeeds`…`test_05_…` 五枚。⇒ **定性从"虚假声称"降为"锁名记法不可检索"**，须订正账本记法（一条小账，不是门失效）。
- 同因：`test_cli_single_install` 命中的是 `ci/steps/linux_build_root_graph.sh`，属**步骤级锁**而非用例级锁，强度另计。

## 五、抽查三条最关键的（逐字读码）
1. **`M5a-G-002` 判 PARTIAL，且这是本批唯一的机制性残余**：生产者 `cli/resource_recorder.h:252` 把 `normalized_cpu_100pct_all_allocated_cores` 由 `true` 改 `false`、新增 `cpu_pct_units:"percent_of_one_core"`（注释坦承"旧字段无条件自证 true 与采集事实相反""保留旧键名但置 false(下游解析兼容)"）——**诚实且兼容，是进步**；`tests/unit/mon001_recorder_test.cpp:100` **同步改成断言 `false`，没有留下断链**（我特意查了这一条，因为修生产者不改守卫就是新红）。**但守卫仍是 `js.find(子串)` 断言字面量** ⇒ **FD-F-002 的机制一分未动，只是极性翻转**：值仍不可证伪、仍测文本而非行为。残余 = 改成值断言（或删掉这个自证字段，改由门禁侧的 `cpu_percent_of_allocated_capacity` 单面出数）。
2. **`M5a-G-001` 判 VERIFIED**：`cli/resource_gate.h:98` `kCpuMeanMinPercent = 85.0`（注 §18.2 冻结）、`:108` `kMon001UtilSampleMinPercent = 85.0`、`:40` `LowAvgCores` 用 `0.85*min(selected_workers,available)`、`:49` `CpuMeanLow` 85%；全文件未见 0.80/0.75/0.50 旧值残留，`:106` 还写明"旧 D.6 的"已被废弃。**他们同时把三个门都拉回宪章值，这是本审计最重的一条 P0 被正面处理**。
3. **他们自己提出一个未决口径并请求编号（做对了）**：`fix_note` 写"分母取义 `granted_workers` vs `min(selected,available)` 按观测优先落地，**待 A 组补登编号**"⇒ 说明隔壁**没有自行裁决科学口径**、而是把选择连同实现交回。**我已登记为 A-39**（现行代码取 `min(selected_workers, available)` 即"观测优先"，而宪章 §10.5 原文是"不低于**已分配容量**的 85%"，两者在 `selected < granted` 时给出不同判定）。
4. **顺带发现一条 C-12 的新实例**（轻微，记档不立条）：`evidence/v6_1_rework/tasks/P1-004/logs/c02_resource_summary.json` 里仍写着 `normalized_cpu_100pct_all_allocated_cores: true` ⇒ **证据目录里存着与新生产者矛盾的旧产物**；evidence 属免报区，但**一旦有文档把它当"已归一化"的证据引用，就是 C-12（证据新鲜度门）要拦的事**。