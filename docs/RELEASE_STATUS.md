# AstroCS 发布状态（Release Status）

> 上游：ASTROCS_DESIGN.md §12.5（状态阶梯）、§13（版本与发布权）

> 状态词唯一口径：`ASTROCS_DESIGN.md` §11.3 —— `CONTRACT_READY` / `IMPLEMENTED` /
> `INSTALLED` / `VERIFIED`；负向 `NOT_IMPLEMENTED` / `NOT_VERIFIED` / `DEFERRED` /
> `DORMANT` / `FAIL`。**合成测试或历史可用节点不等于真实数据/Windows VERIFIED**（§11.3 末条）。

当前结论（DOC-001 复检，2026-09-16）：

```text
发布结论:            NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）
真实数据面:          NOT_VERIFIED（FINAL_REAL_DATA_VALIDATION 未达成）
Windows x64 复验面:  NOT_VERIFIED（VERIFIED 要求正式平台 + 真实数据验收通过）
ACR（CPU/GPU 异构）: DORMANT（不进生产构建/加载/路由/发布）
psf_snr_power:       DEFERRED（生产拒绝）
```

- 逐面状态与证据锚以 `docs/owner/RELEASE_STATUS.md`（§0 词表 + 分面表）与
  `docs/modules/MODULE_MAP.yaml` 为准；本页只登记发布结论，不复制分面表。
- 最终发布决定只属项目负责人；Agent 至多声明 `READY_FOR_OWNER_REVIEW`
  （`ASTROCS_DESIGN.md` §12）。Alpha 前程序/代码/产物内不存在版本信息（§12）；
  根 `VERSION` 仅为内部助记符，不进入程序与发布产物（`ENGINEERING_SPEC.md` §7）。
- 历史轮次（V19R2/V19R8 等 quality-closure 节点、`PRE_RELEASE_ENGINEERING_FOUNDATION`
  一类自述门）**不是当前状态**，只作追溯；原始记录见 `git log`（历史归档树已随 CLEAN-402 清理，
  见下 §历史控制包验收结论）。

## 历史控制包验收结论（CLEAN-402 并入，2026-09-21）

> 六个历史控制包（RELEASE-01/02/03、PROJECT-GOVERNANCE-01/02、SCI-RES-01）已从工作区移除，
> 过程由 git 历史承载；下列**验收结论**为清理时并入的现行口径，逐条去向见
> `run/CLEAN-402/CONCLUSION_MIGRATION.md`。

| 控制包 | 收口时的验收结论 | 备注 |
|---|---|---|
| RELEASE-01 | **不满足 `READY_FOR_OWNER_REVIEW`**：L1 有条件通过（合成 ctest 442 全绿，但存在空/恒真断言与未链产品库的"假测试"）；**L2 合成/真实性能不通过**（4/4 normalize、3/4 mosaic 违约，mosaic 近单线程利用率 5.6–6.4%，Phase1 峰值 RSS 无界增长）；L3 通过；**L4 视觉判定 FAIL**（接缝/背景均匀不通过）；P0 16 条全 OPEN | Windows 腿未构建/未验收 |
| RELEASE-02 | `SUMMARY.md` 为**未填写空模板**（未宣布收口）；其目标（接缝、逆方差链、性能、测试、科学订正）由 RELEASE-03 承接，收口基线见下 | 接缝结论见 `docs/KNOWN_LIMITATIONS.md` §15 |
| RELEASE-03 | **机器门全绿**：`eng/ci/run_checks.py --all` = `verdict=PASS entries=52 steps=99 pass=99 fail=0`（timeout/prereq/skip=0）；`ctest` 472/472；`ninja` rc=0 0 error；**零豁免**（`eng/ci/exemptions.json`=`[]`）。开工基线为 `entries=40 steps=86 pass=81 fail=5`。遗留 18 项如实登记（已由当前控制包差距清单承接） | 未覆盖面：Windows 腿、linux-main/deep 重型档、`tests/cli` 余 5 红（非本包）、SIGTERM exit 9 路径 |
| PROJECT-GOVERNANCE-01/02 | 治理包只完成编制/部分执行，未宣布收口；未闭合项见 `docs/KNOWN_LIMITATIONS.md` §C 与当前控制包 | — |
| SCI-RES-01 | **从未执行**（5 课题全 `NOT_STARTED`，`run/SCI-RES-01/` 不存在）⇒ 无研究结论可沉淀 | 研究课题去向由前台按当前控制包登记 |

**CLEAN-402 治理工件清理前后机器门实测（2026-09-21，Linux）**：

| 轮次 | `eng/ci/run_checks.py --all --profile fast` | 红项 |
|---|---|---|
| 清理前（基线） | `entries=52 steps=99 pass=95 fail=4` | `CHK-ROOT-CLEAN` / `AGENTS-GOV` / `ENG-CONSTRAINTS` / `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`（**四项均为既有红**） |
| 清理后（已删未提交） | `entries=52 steps=99 pass=86 fail=13` | 上述 4 项既有红 + 2 项"已删未提交"暂态（`DOC-INDEX`、`CHK-SECRET-HYGIENE`：二者以 `git ls-files` 枚举，提交后即消失，临时索引模拟实测 **PASS**）+ 7 项并发任务在飞改动（`CTEST-REGISTRATION`/`UNIT-CLOSURE`/`DOC-LINE-ANCHORS`/`UT-VERSION`/`CHK-NO-WEIGHT-MODE`/`CHK-AIO-IO-BOUNDARY`/`CHK-FIX208-DISK-GATE`） |

逐条归因、临时索引复跑证据与 13 项红的分派见 `run/CLEAN-402/CONCLUSION_MIGRATION.md` §F。
清理本身**零代码改动**（只删治理工件 + 改 3 个文档），不引入任何新的结构性红灯。
