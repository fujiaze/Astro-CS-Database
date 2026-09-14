# 历史分片报告独立抽查（spotcheck）

- 抽查人：独立抽查子代理（只读；未修改仓库既有文件，未执行任何 git 写操作）
- 抽查对象：`设计大纲/reports/history/slices/H-Sxx.md` 中 30 条随机派发提交的分片条目
- 真实变更基准：`设计大纲/_evidence/commits/index.jsonl`（1988 行，seq 1..1988 拓扑序）+ 原始 `git show --numstat --name-status` 复算
- 硬约束：全文**不粘贴 diff 或源码正文**；只引用文件路径、函数/结构体名、任务编号、状态字面量、增删行数与文件数。

---

## 一、抽样方法与核对口径

### 1.1 样本来源与 seed 说明

样本清单（seq → 分片映射）由历史溯源前台组长随机派发；本抽查只接收清单、不参与抽样过程，**原始随机 seed 与抽样实现脚本未随样本下发，故抽样过程本身不可复算**。本次能复核的是：30 个 seq 全部真实存在、sha8 与 index.jsonl 完全一致、分片归属正确、日期与分级一致。

样本特征（用 index.jsonl 复算，可复核）：

| 维度 | 结果 |
|---|---|
| 样本数 | 30（占 1988 条的 1.51%） |
| 时间窗 | 2026-07-15 .. 2026-09-14（覆盖首尾与中段） |
| 规模分级 cls | S 17 / M 10 / L 3 |
| merge 提交 | 样本内 0；但 seq 129 的组条目含 merge 子提交 seq 130，已一并核对 |
| struct_only（结构模式）标记 | 2（seq 129、seq 175） |
| 作者 | 付家泽 17 / fujiaze 13 |
| 触达代码目录（lib/ cli/ tests/ tools/ ci/ cmake/ .github/） | 11 / 30 |
| 噪声文件（nnoise>0）或二进制（nbin>0） | 0 / 30（故无"无法核对"来源） |
| index.jsonl 标记 truncated（文件列表被截断） | 1（seq 1899，已用 git 重算完整 44 文件列表后再判） |
| 落在分片数 | 20 片（H-S02..H-S35；H-S25、H-S31 两片本身缺档，未派样本） |
| 条目形式 | 独立条目 13 / 组条目 17（组条目按"该组对本 seq 的表述"判定） |

seq → 分片映射：69→S02；129→S03；175/186/189/196/207→S04；264→S05；290→S06；349→S07；425→S08；528→S10；711→S13；765→S14；791/799/818/824→S15；878→S16；1074/1089→S20；1176→S22；1212→S23；1415→S26；1438→S27；1549/1558→S29；1786→S33；1898/1899→S35。

### 1.2 三条核对动作（每条样本都做）

1. **体量与清单复算**：`git show --numstat --name-status <sha>` 与 index.jsonl 的 `add/dele/nfile/files[]` 双向对齐（30 条全一致）。
2. **符号存在性与方向检查**：把条目里的函数名/结构体名/枚举/常量/状态字面量/任务编号，在该提交的 +/- 行里查是否存在、方向是否吻合（例：seq 69 的 `LeafMap` 是否只出现在删除行；seq 186 的 `PhotometricDiag` 实际字段数；seq 349 的 `-ffast-math` 落在哪个构建文件）。
3. **关联与交叉引用反查**：条目中的"父提交 / 前后 seq / 被引用短哈希 / 编号族"逐条用 index.jsonl 的 `parents` 与邻 seq 的 subject 反查（例：seq 130 是否 merge 且双亲为 128+129；seq 69 是否 seq 74 的父；`git cat-file -t 4bc2a38` 报不存在，故 seq 824 的"哈希不符"存疑标记成立）。

### 1.3 判定口径（写死，便于复核）

- **相符**：该条目对这条提交的「做了什么 / 落点 / 内容性质」三项与真实变更面无实质冲突；纯数字口径误差记为"相符（附注）"。
- **部分相符**：三项中存在实质偏差——漏掉重要落点、把落点错置到别的模块、或对这条提交的性质归类偏差。
- **误读**：对这条提交的核心判断与事实相反或凭空虚构（把 A 域说成 B 域、编号族张冠李戴、把结构/汇总模式说成实质开发等）。
- **无法核对**：变更面不可复现（噪声、二进制遮蔽等）。

### 1.4 系统性发现（影响全部 30 条的读法，先声明）

各分片报告在片头声明其基底为 `_evidence/commits/compact/*.md` 与 `cards/*.md`、**未自行执行 `git show`**。复算结果：卡片的"目录聚合行数"与 `git numstat` 一致，但卡片的**"每文件变更行数"小清单存在偏小**，例如：

- seq 1415：卡片 `tests/backend/test_p3001_science_freeze.py +58 -0`，git 实为 **+66 -0**；
- seq 1898：卡片 `问题扫描/findings/H_NUMERIC/p0/M9_L24_L26.md +44 -0`，git 实为 **+49 -0**（同一卡片的目录聚合行 `findings : 3 文件 +57 -3` 反而是对的，卡片自相矛盾）。

故本报告中的行类小误差应理解为**证据底稿口径被报告继承**，而非条目作者编造；但"报告未对卡片数字做二次校验"本身属可核对性缺口，已计入附注/部分相符。

---

## 二、30 条逐条判定

| # | seq | sha8 | 分片 | 形式 | 报告条目要点（一句） | 真实变更（一句） | 判定 |
|---|---|---|---|---|---|---|---|
| 1 | 69 | 86047733 | H-S02 | 独立 | 浏览器 LOD 动态网格 + uint8 降采样 + 双击部署，13 文件 +1053/-187，新增仅 deploy.ps1 | 13 文件全在 `lib/healpix_db/healpix_browser_qt/`（1 A + 12 M，+1053/-187）；`build_sphere_mesh_dynamic`/`need_rebuild_mesh`/`LeafIndexEntry`/`CachedLeaf` 在新增行，`LeafMap` 只在删除行 | **相符** |
| 2 | 129 | 55e6d6d2 | H-S03 | 组 129–130 | 代码面很小（3 文件 +717/-99），体量来自 evidence 740 文件 +295185（"710 帧 × 3 次重复"）；130 为 merge 且数字与 129 相同 | 代码面 3 文件（orchestrator 1 + engineering/tools 2）+717/-99；evidence 740 文件 +295185；seq 130（f8097dfc）双亲 = 128+129 且 add/dele/nfile 与 129 全同；`MERGE_PATH_B`、99.86%（709/710）在证据正文；128 确有 `ipv_solve_from_memory_with_callback`，163 确有 `INTERNAL_DETECTION_SHARED_EXPORT` | **相符**（附注：重复证据实为 710 帧各 1 次 + 21 个 `_run2/_run3` 文件，"710 帧 × 3"与其自报 740 文件总数不自洽） |
| 3 | 175 | f4ec8b24 | H-S04 | 独立 | P11-004 双层闭环 + v1.3 控制包整体挂载（结构模式 L），452 文件；并指出"变更面显示消息未提及 v1.3 整包安装" | 452 文件 = `engineering_v1.3/` 446 新（evidence 310/+420962、tasks 50、docs 29、control 13、checklists 12、contracts 8、templates 8、agent 7、tools 3、根件 6）+ `lib/plate_solve` 5 文件 +369 + `memory.md` +55/-13；`SolveInlierCache`/`ipv_get_last_inliers`/(N,9) ndarray/`--authoritative-pairs` 均在 diff | **相符**（附注：plate_solve 5 文件状态均为 M，"+369"为纯新增行；诊断工具 v3.4 系随包新增而非原地升级） |
| 4 | 186 | b6d19e56 | H-S04 | 组 186..190 | `lib/photometric_calib` 5 文件 +351/-22，`PhotometricDiag` 带 17 个阶段计数器进测光 DLL | 5 文件名与行数全对（+351/-22）；结构体实为 8 个阶段分组、20 字段（12 个整型计数器），"17"系首行自述且条目已标"未证实" | **相符** |
| 5 | 189 | 8ccdf362 | H-S04 | 组 186..190 | `engineering_v1.3/evidence` 11 文件 +1456 四件套 | 11 文件 +1456/-0，全在 `engineering_v1.3/evidence/P12-001/`（spec.md、TASK_REPORT、TEST_REPORT、EVIDENCE_INDEX、scripts/test_contract.py、raw_logs 5 个） | **相符** |
| 6 | 196 | 38470d2b | H-S04 | 独立 | memory.md 追加 P12-004/P12-005 完成记录，1 文件 +47 | 1 文件 `memory.md` +47/-0；seq 194/195 正是 P12-004/P12-005 | **相符** |
| 7 | 207 | 3c219af8 | H-S04 | 独立 | Gate A 收口：A-001~A-004 全 DONE、报告入库、台账进 B-001；control 2 + evidence 1 | 3 文件：`engineering_authoritative/control/MASTER_TASK_REGISTER.csv` 2/2、`PROJECT_STATE.yaml` 2/2、`evidence/GATE_A_REPORT.md` +62；A-004→DONE、B-001→IN_PROGRESS 均在 diff，报告内 A-001..A-004 行为 DONE | **相符** |
| 8 | 264 | 37ed737d | H-S05 | 独立 | R05-B07：photscal<=0 返 -5；测试改名并新增用例；附 tools/ 草稿入库 | 4 文件 +69/-15：`lib/calibration/src/photometry_apply.cpp` 6/1、`lib/calibration/tests/test_photometry_apply.cpp` 19/14、`tools/_r05_photapply_commit_msg.txt` 6/0、`tools/_r05_rebuild_photapply.json` 38/0；`test_photscal_0`/`test_photscal_negative`/-5 坐实 | **相符** |
| 9 | 290 | e48bd601 | H-S06 | 独立 G15 | R08 移植自动 NSIDE，1 文件 +300/-59，生产代码 | 1 文件 `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` +300/-59；`MAX_DEPTH`、`make_tangent_basis`、去魔数 210960→211034.6 均在 diff；18/18 属消息自述（条目已声明） | **相符** |
| 10 | 349 | ea62d787 | H-S07 | 独立 G18 | r11 修复 + 移除 Phase1 fast-math + Schema 规范化（CFG-103），4 文件 +29/-16；落点写 `lib/orchestrator/{Makefile, configs/stage1.schema.json, src/orchestrator.cpp}`、`lib/calibration(1)`、`lib/plate_solve(1)` | 真实 4 文件为 `lib/calibration/Makefile`(1/1)、`lib/orchestrator/configs/stage1.schema.json`(1/1)、`lib/orchestrator/cpp/src/orchestrator.cpp`(25/12)、`lib/plate_solve/cpp/ipv/build.ps1`(2/2)；该提交**不含任何 orchestrator 的 Makefile**（仓库里 orchestrator 的 Makefile 在 `lib/orchestrator/cpp/Makefile`，本提交未触）；`-ffast-math` 的删除行只在 calibration Makefile 与 plate_solve build.ps1 | **部分相符**（落点错置：把 `lib/calibration/Makefile` 记到 `lib/orchestrator` 名下，凭空多出 1 项，与自称"4 文件"自相矛盾；做了什么与性质归类正确；"CFG-103 晚于 345 的 CFG-105..109"经反查成立） |
| 11 | 425 | 8114a1f3 | H-S08 | 独立 | wiki 指针更新（Phase1 冻结闭合），AstroCS.wiki gitlink +1/-1 | 1 文件 `AstroCS.wiki` +1/-1，diff 为 `Subproject commit` 行，gitlink 判定正确；seq 422/428/437 同族指针链成立 | **相符** |
| 12 | 528 | bf93e71a | H-S10 | 独立 | ACR 稳健性四修，`lib/acr` 4 文件 +50/-11 | 4 文件全名一致（`examples/weighted_integration/route_profile_calibration.cpp/.hpp`、`routing/benchmark_route_estimator.cpp`、`scheduler/dispatcher.cpp`）+50/-11；父提交即 seq 527 | **相符**（附注：`dispatcher.cpp` 实为 0/1 纯删除；宪章 §3.3/§10.1 下 ACR 属休眠非目标域，条目未加此语境） |
| 13 | 711 | ce8ec527 | H-S13 | 独立 | v19r3-s3 目标几何缓存：TargetGeomCache 有界 LRU 8192 + 代际清空 + 新计数进 DrizzleStats | 4 文件 `lib/healpix_db/healpix_drizzle/{drizzle_engine.h/.cpp, spherical_overlap.h/.cpp}` +243/-34；`TargetGeomCache`、`compute_overlap_area_g_ctx_cached`、`geometry_cache_hit`、`drizzleTiled` 与 generation 语义均在 diff；k_corr=1.3883 属消息自述 | **相符** |
| 14 | 765 | 78615aae | H-S14 | 独立 | B2-11 文档锚点：REJECTION_ALGORITHMS.md 加 large_scale trail 锚（rejection.cpp:1501-1592），+1/-1 | 1 文件 `docs/algorithms/REJECTION_ALGORITHMS.md` +1/-1，新增行原文含 `rejection.cpp:1501-1592`；seq 758（线锚）与 767（标 B2-01/11/12 DONE）关系成立 | **相符** |
| 15 | 791 | a4452d89 | H-S15 | 组 791-792 | cli_command.h 注释订正 + cli_command.cpp 补 deprecated shim 转发到 `astrocs::crypto::sha256_hex`；792 台账 B4-02 TODO→DONE、报 43/95 | 2 文件：`cli_command.h` 1/1、`cli_command.cpp` 12/6（合计 +13/-7）；shim 与 `namespace sha256_impl` 在新增行；seq 792 台账行 TODO→DONE 与 43/95 坐实 | **相符**（附注：关联把 seq 771 与 778 并列作"crypto 单源预告"，771 实为 HEALPix 映射权威 [B2-08]，crypto 单源预告只见 seq 778；"+12/-6"为 .cpp 单文件口径） |
| 16 | 799 | 60915265 | H-S15 | 组 799-800 | docs 补 AioHipsDataType 契约（aio_hips.h:46-49、aio_hips_writer.cpp:222-225、FP64 oracle 双模式）；800 台账 B4-04 TODO→DONE、46/95 | 1 文件 +8/-1，两个行锚均在新增行原文；seq 800 台账行与 46/95 坐实；条目正确声明 test_precision_dual 只有文档引用、无测试文件 | **相符** |
| 17 | 818 | 126223cd | H-S15 | 独立 | V19R8 checklist(+185) + tasks(+421)，S0–S6 分解（S5 28c） | 2 文件均 A：`工程控制/checklists/QA_V19R8_QUALITY.md` 185、`工程控制/tasks/QA-V19R8-QUALITY-OPTIMIZATION.md` 421；S5-01..S5-28 计数命中；与 816(212)/820(44)/821-822(trio 口径) 关系成立 | **相符** |
| 18 | 824 | 56a99645 | H-S15 | 组 823-824 | 824 台账 QA-V19R7-B4-14（dynamic_psf 数值）TODO→DONE、54/95 | 1 文件 `工程控制/control/MASTER_TASK_REGISTER.csv` +1/-1，正是 B4-14 行 TODO→DONE；同组对 823 的描述（+6/+14 纯注释、`MOFFAT4_FWHM_FACTOR=1.230310`、守卫行锚 45/71-74/89-91/114/169-170,326/333-341/343-344）逐项吻合；"4bc2a38 与 823 哈希不符"的存疑标记经查成立（该短哈希在仓库不存在） | **相符** |
| 19 | 878 | 68aed09b | H-S16 | 独立 | 台账销账 QA-V19R7-B5-05 TODO→DONE，1 文件 | 1 文件 `MASTER_TASK_REGISTER.csv` +1/-1，B5-05 行 TODO→DONE；seq 877（+1/-0 追加 QA-V19R8-S3-11 行）与"两代编号族"观察成立（S3-11 不属 878） | **相符** |
| 20 | 1074 | 7818e663 | H-S20 | 组 1074+1075 | ThreadBudget 架构文档 +46、静态 checker +68（7 处命中登记）、测试 +62（5 mutation）、LOG +20、台账 NOT_STARTED→PASS；代码面 2 | 5 文件与四组行数全对；ARCH-004 行 NOT_STARTED→PASS 坐实；"7 处命中逐一定性登记"在 ARCH-004/LOG.md 新增行内；TB-ARCH-004 确在 seq 1082 面上；条目未把架构文档误说成运行时实现 | **相符**（附注：把 `tools/` 下静态 checker 归"生产代码"偏宽，但同条目并列"CI 或机器门"；"5 mutation""41/41"为消息自述） |
| 21 | 1089 | efe0c66f | H-S20 | 组 1088+1089 | 1089 = 表行登记 CAP-CLI-001；1088 = cli/main.cpp(+80)、CMakeLists(+40)、version_generated.h.in(+3)、tests/cli 2 文件 +83、LOG、台账；代码面 5 | seq 1089 实为 2 文件 +1/+1（`artifacts/prerelease_v5/tables/COMMITS.csv`、`REVIEW_CAPSULE_INDEX.csv`）；seq 1088 7 文件各行数全对（`__init__.py` 为 0/0） | **相符**（附注：合并落点行里的 `02_TASK_LEDGER.csv` 只属 1088，不在 1089 变更面） |
| 22 | 1176 | 266f0815 | H-S22 | 组 11 | 跨语言测试交付：oracle 测试 +277（10 tests）、C++ 探针 +71、fixture main +2、SYN007_verification.md +59、台账 SYN-007 NOT_STARTED→PASS；不改 lib/ | 5 文件与四组行数全对；`def test_` 计数 = 10；台账行与依赖 ALG-007\|CLI-006\|ABI-003 坐实；确无 lib/ 文件；大端 FITS reader 在测试内 | **相符** |
| 23 | 1212 | 242a42f1 | H-S23 | 组 1211–1214 | 1212 改 `tests/cli/test_cli_protocol.py` 的 test_07 cancel，用 CTRL_BREAK_EVENT + CREATE_NEW_PROCESS_GROUP 替代 send_signal | 1 文件 +7/-2；`CTRL_BREAK_EVENT`、`CREATE_NEW_PROCESS_GROUP` 在新增行，方法为 `test_07_cancel_exit_9_no_fake_artifacts`；同组 1211（cli/CMakeLists.txt 6/5）、1213（verification.md 61 + 台账 1/1）、1214（zip + 两表行）亦全对 | **相符** |
| 24 | 1415 | 9e8ba103 | H-S26 | 组 1415–1416 | 极区条件统一 abs(dec)<=85°：docs 两处 2/2、p3_session.cpp/.h 各 1/1、p3_wcs.cpp/.h 各 3/3、新增测试 +58、包面 tasks/P3-001、台账 P3-001→PASS；11 文件 +259/-13 | 11 文件 +259/-13 与四个代码文件行数全对；`kMaxAbsDec = 85.0`、`fabs(dec) > 85.0`、`ACS_ERR_PARAM`、"85°"均在 diff；**新增测试实为 +66/-0**；台账 NOT_STARTED→PASS 坐实 | **相符**（附注：单文件 +58 与 numstat +66 不符，系 1.4 节所述卡片口径被继承；条目自称总数与落点无碍） |
| 25 | 1438 | c0927374 | H-S27 | 组 4 | 1438 = COMMITS.csv 绑定行（49 任务）；1437 = 15 个代码文件任务号注释清零、保留合同 ID，增删对称 | seq 1438 为 1 文件 `evidence/v6_1_rework/COMMITS.csv` +1/-0；seq 1437 20 文件 +196/-42：代码 15 文件全部 +n/-n 对称（phase3_session 7 文件 12/12、phase2 4 文件 25/25、phase1 2 文件 2/2、healpix_db 1、phase2_session 1），evidence 5 文件 +155/-1 | **相符** |
| 26 | 1549 | 7e974087 | H-S29 | 组 1547–1550 | 轮 4（1549）：run.py probe_prerequisite 跳过 run/ 前缀、CMakePresets 去 CMAKE_GENERATOR_INSTANCE 与 version pin、verify_toolchain.py 契约同步、ci-windows.yml utf-8 + 上传多路径；并断言"轮 3 集中 ci/tests、轮 4 集中 presets/run.py/verify_toolchain.py，修复轮与文件族一一对应" | 10 文件 +823/-18：除上述 4 项外，本条**还含 `ci/tests/test_run_prefix_probe.py` 新增 +124 与 `ci/tests/test_workflow_lock.py` +17/-2（合计 +141，为该提交最大代码面）**，另有 4 个 V8-CI-010 evidence 文件；即 ci/tests 并非只属轮 3，"一一对应"不成立 | **部分相符**（漏本条最大代码落点 ci/tests +141 行；1547/1548/1550 的轮次描述与文件族经复算均成立） |
| 27 | 1558 | 06e288ab | H-S29 | 独立 | ci-linux.yml deep 安装步补 python3-numpy/astropy/scipy/yaml，policy 登记 apt_packages，PYTEST_TAIL_CAP 50→120 | 3 文件 +14/-5：`.github/workflows/ci-linux.yml` 6/1、`ci/toolchain.policy.json` 3/1、`tools/quality/ci_coverage_runner.py` 5/3；四个包名与 `PYTEST_TAIL_CAP`、`apt_packages`、50→120 均在 diff | **相符** |
| 28 | 1786 | d8c821db | H-S33 | 独立 | 宪章冻结：新增 `ASTROCS_PROJECT_CONSTITUTION.md` +674/-0，另三处小改；代码面 0 | 4 文件 +713/-15：宪章 A 674/0、`AstroCS_ENGINEERING_CONSTRAINTS.md` 21/7、`AGENTS.md` 7/6、`docs/DOCUMENT_INDEX.yaml` 11/2；确无代码文件；§18 四项裁决与 supersession 为消息自述且与正文标题吻合 | **相符** |
| 29 | 1898 | cbb07805 | H-S35 | 组 RQS-G | 1898 = M6b 二次复核回执入档 + 新增通则"问题扫描/** 不计入真源统计"，整组归为"汇总层 INDEX/SUMMARY" | 6 文件 +83/-27：`SUMMARY.md` 2/2、`_merge/00_COORDINATION.md` 15/18、`_merge/M6b.md` 9/4、两个 M6b findings 小改（7/2、1/1），以及**新增 `findings/H_NUMERIC/p0/M9_L24_L26.md` +49/-0——占本提交新增行 59% 的最大落点，为 M9 域两条 P0 定稿条目**；通则文字确在 diff；该提交不含 INDEX.md（条目未误称，INDEX 归属 1891/1892/1915/1916 正确） | **部分相符**（漏最大落点；把含新增 P0 定稿条目的提交笼统归入"汇总层"，性质归类偏轻；条目照抄消息口径而未用变更面补正） |
| 30 | 1899 | b72da8a4 | H-S35 | 组 RQS-F | 1899 = M2b 定稿 29 条（P0 10）+ 三条裁决入 40 文件 + 对已收工域移交改道；落点记 `findings/`（1889 新增 32、1899 新增 34、1901 新增 11）、`_merge/M*.md`、`40_OWNER_DECISIONS.md`（1895 新建 +58、1899/1901 增项）；零代码面 | 44 文件 +2367/-89（该 seq 的 index 记录 truncated，已用 git 重算）：新增(A) 恰 34 个且全在 `findings/`；修改(M) 10 个 = `_merge/M2b.md` 156/15、`_merge/M8.md` 188/10、`_merge/00_COORDINATION.md` 21/14、`40_OWNER_DECISIONS.md` 4/0 + 6 个 findings；seq 1889 A=32、1901 A=11、1895 台账 +58 均复算命中；确零代码面 | **相符**（附注：34 个新增文件按文件名多属 M7/M8/M8a/M9 域的类别×优先级总述件，M2b 侧以修改为主，条目沿用消息口径但数字无误） |

---

## 三、误读清单

**按 1.3 节口径，本 30 条样本中误读条数 = 0。** 未发现把 A 域说成 B 域、编号族张冠李戴、把结构/汇总模式说成实质开发、或把文档提交说成生产开发的条目。

典型例子（**擦边条目**，已计入部分相符，供组长复核判级）：

1. **seq 349（H-S07）— 落点目录错置。** 条目写出 `lib/orchestrator/Makefile`，而该提交的 Makefile 属 `lib/calibration/Makefile`；后果是读者会以为 Phase1 的 `-ffast-math` 移除发生在编排器构建面，实际发生在标定库与 plate_solve/ipv 构建脚本。同时落点列 5 项却自称"4 文件 +29/-16"，内部不自洽。
2. **seq 1549（H-S29）— 组条目轮次叙述漏掉本条最大代码落点。** 该组明确声称"修复轮与文件族一一对应"，把 `ci/tests` 划给轮 3；但 seq 1549 自身含新增 `ci/tests/test_run_prefix_probe.py` +124 与 `test_workflow_lock.py` +17/-2（+141，为该提交最大代码面），轮 4 叙述未提。
3. **seq 1898（H-S35）— 漏最大落点 + 归类偏轻。** 条目把 1898 叙述为"M6b 复核回执 + 通则"并归入审计汇总层，但 +83 新增行中 49 行（59%）来自新文件 `问题扫描/findings/H_NUMERIC/p0/M9_L24_L26.md`（M9 域两条 P0 定稿）。消息正文确实未提 M9，条目照抄消息口径而未以变更面补正——正是本次抽查要抓的"以消息代变更面"缺口。

若组长按最严口径把第 1、3 条上调为误读，则误读率上界为 **2/30 = 6.7%**；本报告正式口径给 **0/30 = 0.0%**，并同时给出部分相符率 **3/30 = 10.0%** 作为质量底线指标。

另有 8 条**相符但带附注**的细节偏差（数字口径、关联引用、组条目并表），已在第二节逐条标注：seq 129、175、528、791、1074、1089、1415、1899。

---

## 四、统计与结论

| 判定 | 条数 | 占比 |
|---|---|---|
| 相符 | 27 | 90.0% |
| 部分相符 | 3 | 10.0% |
| **误读** | **0** | **0.0%** |
| 无法核对 | 0 | 0.0% |

- **误读率 = 0 / 30 = 0.0%**（部分相符单列，不计入误读）；最严口径上界 2/30 = 6.7%（见第三节）。
- 部分相符名单：**seq 349、seq 1549、seq 1898**。
- 数字面准确率：30 条中 29 条的"文件数 + 增删行数 + 文件清单"与 git 三方一致；**报告侧只有一条行数字面误差**（seq 1415 把新增测试写成 +58，git numstat 为 +66）。另有 2 处卡片底稿自身偏小（seq 1415 +58/+66、seq 1898 +44/+49），后者未被报告引用因而未扩散——根因同为 1.4 节的"每文件变更行数"清单口径，报告未对卡片做二次校验。

**三点结论（可直接进总报告）：**

1. **分片报告没有编造式误读。** 落点文件集合、行数、文件数、merge 标记、日期、分级、任务编号在 git 复算下基本全中；对"结构模式"型提交（seq 129、175）不但标了结构模式，还明确写出"变更面证据显示消息未提及的事实"，说明基底对整包挂载/巨型证据类提交具备识别力。
2. **主要风险是"以消息自述代替变更面"的三类残留**：(a) 组条目在轮次/文件族归纳时漏掉某条提交自己的落点（seq 1549、1898）；(b) 落点行的目录前缀错置（seq 349）；(c) 卡片行数字被直接继承（seq 1415、1898）。建议归纳层统一补一次"逐 seq 落点反查"（一次 numstat 比对即可同时暴露 349/1549/1898 三处）。
3. **交叉引用整体可信。** 30 条中出现的父提交、前后 seq、被引用短哈希、编号族断言逐条反查，仅 1 处不成立（seq 791 把 seq 771 也算作 crypto 单源预告）；其余 20+ 处（如 seq 69 是 seq 74 之父、seq 130 双亲为 128+129、seq 824 引用的 `4bc2a38` 不存在、seq 1082 点名 TB-ARCH-004）全部为真，其中"引用哈希对不上"这类反向存疑标记也是对的。

### 复核命令（任何人可重放）

```bash
# 单条体量与文件清单（本抽查的主核对手段）
git show --numstat --name-status <sha>

# 与机器索引对齐（30 条 seq 一次性列出）
python3 - <<'PY'
want = {69,129,175,186,189,196,207,264,290,349,425,528,711,765,791,799,
        818,824,878,1074,1089,1176,1212,1415,1438,1549,1558,1786,1898,1899}
import json
for line in open('设计大纲/_evidence/commits/index.jsonl', encoding='utf-8'):
    d = json.loads(line)
    if d['seq'] in want:
        print(d['seq'], d['sha'][:8], d['adate'][:10], d['cls'], d['nfile'], d['add'], d['dele'], d['merge'])
PY

# 条目定位（分片内按 seq 反查）
grep -n "seq <N>" 设计大纲/reports/history/slices/H-Sxx.md

# 被引用短哈希是否存在
git cat-file -t 4bc2a38
```

---

*本文件为本次抽查唯一新增产物；过程只读，未改动任何既有文件。*
