# AstroCS 项目记忆（GOV-005 收敛）

> 本文件是仓库**唯一项目记忆**，自 GOV-005（SA-GOV-01，2026-09-02）起只保留：
> 稳定目标、当前 SHA/版本、模块索引、开放问题。
> 历史开发过程记录（V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1 轮次、11 子仓合并、
> 旧 F 盘路径、旧线程数、Python 调 DLL 等失效现状）已整体归档至
> `docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md`
> （ARCHIVED_NON_NORMATIVE，仅追溯用，不代表当前状态）。
> 科学/算法/架构/发布权威一律以 `docs/`（science|algorithms|architecture|
> standards|modules|contracts）、冻结宪章 `ASTROCS_PROJECT_CONSTITUTION.md`
> （ASTROCS-CONSTITUTION-001，唯一最高约束；`AstroCS_ENGINEERING_CONSTRAINTS.md`
> 已降 ARCHIVED_NON_NORMATIVE 仅作历史参照）与
> `REVIEW.md` + `docs/owner/`（L0）为准；本文件不复制长文、不承载权威判定。

## 1. 稳定目标（不随轮次变化）

- 天文 CCD 图像校准与标准化数据库：Phase1 单帧 light + masters/catalog/config →
  单帧标准化 IVOA HiPS + manifest；Phase2 一组合同兼容 HiPS → 马赛克 HiPS +
  UPM/rejection/integration provenance；Phase3 任一合同兼容 HiPS → 平面 FITS +
  WCS/coverage/validity/provenance（约束 §A）。
- 三 Phase 是**三个隔离产品命令**，非固定顺序流水线；跨 Phase 仅磁盘产品/manifest
  交换；禁止同进程 `--phases 1,2,3`（约束 §A.4）——CLI-002 已删除该入口
  （DOC-CONV-001 实测 `astrocs run --phases 1,2,3` → rc=2 unknown command，见 §5-1）。
- 正式开发/客户端/发布平台 = **Windows x64**（Win10 22H2 下限 / Win11 主验证）；
  Windows 用户只面对 `astrocs.exe`，运行时/I/O/科学模块/CPU provider 以 DLL 交付；
  Linux amd64 仅控制/静态分析/轻量编译/小合成（约束 §B）。
- ACR = DORMANT：保留源码与隔离测试；生产构建/加载/路由/benchmark/发布不含
  ACR/CUDA；当前唯一生产计算后端是纯 CPU（约束 §C）。
- 科学定义 = 算法 = 接口 = 代码 = 测试；科学公式与默认容差不得改动（约束 §E）。
- 仅 `main` 形成正式历史；SubAgent 不直接 commit/push；最终发布裁定权只属项目
  负责人（约束 §G/H）。

## 2. 当前 SHA / 版本

- 当前收敛基线（DOC-CONV-001 实测，2026-09-12）：HEAD = main = origin/main 三 SHA
  一致 = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`。
- 历史基线：GOV-005 检出基线 `6affe3009985452f5bc0bdf654aa95a4b61b2d2e`；
  L0 建立基线 `caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（GOV-004）。
- 产品版本唯一源：根 `VERSION` = `0.11.0-alpha.2`（GOV-003 单源；生成串
  `0.11.0-alpha.2+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）。
- 文档索引：`docs/DOCUMENT_INDEX.yaml`（GOV-002；`base_product_version` 为机器
  修订关系字段，版本检查器豁免）。
- 权威入口：`REVIEW.md` + `docs/owner/` 5 份 L0 文档（GOV-004 建立，
  DOC-CONV-001 状态收敛；状态词阶梯权威 = `docs/owner/RELEASE_STATUS.md` §0）。
- 主线关键提交链：GOV-001 冻结约束 → GOV-002 归档 → GOV-003 版本单源 →
  GOV-004 L0 骨架 → GOV-005 文档收敛 → … → GOV-001 宪章冻结（`d8c821db`）→
  STD-REG-001（`fb7f232a`）→ CI 线（`778fe98e`/`a67bbc83`/`d3097319`）→
  `da3c4b4a`（DOC-CONV-001 收敛基线）。

## 3. 模块索引（权威：docs/architecture/MODULE_MAP.md、docs/modules/、module.yaml）

- **产品骨架**（唯一根 CMake，BLD-002）：`astrocs` 可执行 = cli/ + 显式静态库图
  （astrocs_contracts / astrocs_core / astrocs_io / astrocs_common /
  astrocs_cfitsio 等）；`lib/core`（runtime/pipeline/module registry）、
  `lib/io`（IO-001 io_adapter）、`lib/common`（sha256/healpix）。
- **阶段会话**：`lib/phase1_session`（p1_session）、`lib/phase2_session`
  （p2_session）、`lib/phase3_session`（p3_session + wcs/resample/output/
  hips_properties）。
- **科学模块**（legacy lib/ 树，模块文档见 docs/modules/*.md）：
  astro_image_io / calibration / dynamic_psf / star_detector / plate_solve /
  photometric_calib / snr_estimator / gaia_xpsd_client / healpix_db
  （healpix_drizzle + healpix_browser_qt）/ phase2 / orchestrator / acr。
- **模块服务**：`modules/services/io`（fits_stream_v1 头 + 自测）。
- **CLI 命令**（docs/api/CLI_PROTOCOL_V1.md）：`astrocs phase1/2/3 run`、
  `phase1/2/3 validate|plan|inspect`（CLI-001 九条）、`verify`、
  `benchmark cpu`、`doctor` 等；遗留 `run --phases 1,2,3` 已删除（见 §5-1）。
- 每模块 README.md + module.yaml + 公共头 + CMake + 共址测试是模块规范（约束
  §F.5）；docs/modules/registry/ 为机器生成模块 README（GENERATED）。

## 4. 已归档/不再活跃（history，勿当现状）

- V1–V19/V18R2/V19R2/V19R8/V19R6R2-W1 等历史轮次：只进 CHANGELOG.md 历史节与
  `docs/archive/**`、`engineering/control/archive/**`（ARCHIVED_NON_NORMATIVE）。
- 旧 11 子仓合并记录、F 盘本地路径、16 线程硬编码、Python 脚本调 DLL 时代：
  全部为失效现状，已随 GOV-005 迁 history（见 §1 归档位置）。
- 开发过程操作日志本体：`docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md`。

## 5. 开放问题（如实，不冒充已解决）

1. ~~`astrocs run --phases 1,2,3` 遗留未删~~ → **已删除**（CLI-002；DOC-CONV-001
   BASE=`da3c4b4a` 实测 `build/cli/astrocs run --phases 1,2,3` → rc=2
   `unknown command 'run'`；kRules 无 run/graph）。
2. ~~约束 §F.1 每节点唯一真实模块 operation 未达成~~ → **三 Phase 已完成**
   （P1-001 `9e09941a` / P2-001 `439f9f20` / P3-002 `1a56ffb7`；
   `lib/core/src/module_adapters.cpp`:4257 P1 八节点 / :4282 P2 七节点 /
   :4309 P3 五节点；ctest `p1001_real_nodes`/`p2001_real_nodes`/
   `p3002_real_nodes`/`p3002_uncertainty` 在 BASE=`da3c4b4a` 实测 4/4 PASS）。
3. **Windows 发布执行面未完成（NOT_VERIFIED）**：Windows 侧 DLL 化发布安装树
   （astrocs.exe + runtime/io/科学模块/provider DLL）与 MSVC 编译/测试/32R/真实数据
   最终验收未跑（Fatduck 侧）；**Linux 技术预览安装面已 INSTALLED**
   （MOD-001 `59fdeab3`：`cmake/install_layout.cmake` 五科学模块 +
   `packaging/astrocs.product.json` units=10；DOC-CONV-001 实测
   `tests/abi/mod001_install_load_check.py` 64/64 PASS）。
4. **Phase3 扩展（DOC-CONV-001 更正口径）**：负责人裁决 §18.1 冻结四投影为
   **TAN+SIN+CAR+AIT**（旧 `SIN/ZEA/CAR/AIT` 表述有误）——registry v1 已在
   `lib/phase3_proj/p3_projection.cpp`（:267-273）IMPLEMENTED 并通过 ctest
   `p3_projection_units`/`p3_projection_fault` + 独立 numpy Oracle；**生产会话/DLL
   挂载未切换**（`lib/phase3_proj/module.yaml` `entrypoint: MISSING`），
   故未 INSTALLED；`healpix_interp4`（当前 nearest/bilinear）与流式 FITS 输出接入
   （IO-001 接口在位未接入 p3 writer）为 NOT_IMPLEMENTED。
5. **版本收敛遗留（他人路径，非 GOV-005 写域）**：docs/VERSIONING.md 的旧基线
   版本号字样、CMake `project(... VERSION …)` 字面量、tests/tools 硬编码旧版本
   断言 —— 已由 check_version_namespaces.py `out_of_scope` 登记，待前台/QA
   协调（GOV-005 不改他人写域；旧号段见该检查器输出与 CHANGELOG 历史节）。
6. **运行环境事实**：当前控制节点为 Linux amd64（vm-bj Debian 13），正式验证
   依赖 Windows/Fatduck 节点恢复；本提交后如需在 main 上继续，前台需 fetch 并
   验证三 SHA 一致（约束 §G.2）。
7. **控制包执行模式（本周期沉淀，打包审核包）**：控制包线 cp_run 串行单飞（DOC→TEST→IMPL 链依赖）+ 修复批次线按文件域互斥并行（多批同飞+挂账机制）；前台=调度员+验收官（completion≠PASS，五门机器复跑+diff 纯度抽验+禁区复核）；SubAgent 零 git、前台守则化提交（ledger append→精确 add→原子 commit→reconcile --strict rc=0）。全文：`run/local/execution_mode/CONTROL_PACK_EXECUTION_MODEL.md`（含下一控制包结构要求：parallel_group/domain_files/deps 扩展字段）。
8. **会话交接（2026-09-09）**：控制包 disarm 收尾，38/140 闭环、6 TEST+HIPS-IMPL 中断待重派、fatduck 真实数据拉取进行中（GaiaDR3/DR3SP/testdata 132G）。全文：`run/local/HANDOFF_CONTROL_PACK_20260909.md`（新对话必读，含挂账 10 项与续作指引）。
9. **旧控制包作废归档（2026-09-09 owner 指令）**：V7 MODULAR REFOUNDATION 控制包线（140 任务队列） disarm 终止于 42/140（41 submit + NOISE-IMPL 沿用旧包收尾）；队列 session-ccad3cec 台账与 TASK_STATE/CSV/COMMIT_LEDGER 保持一致性（reconcile --strict rc=0），未完成任务状态保留 NOT_STARTED 作历史。归档：已交付任务闭环清单见 git log 与台账；根目录整理两批（19ef3722 散落归位+目录规范强制基线；009ee419 launch→packaging/launch、graph→artifacts/graph、schemas→contracts/schemas 含9处引用更新、CI资源csv→run/resource/）。**owner 将重新修订工程包（新控制包），本条为交接锚**。已登记未修候选：DISP-WCS-007(deg/rad 成对抵消)、DISP-WCS-008(AP/BP 逆 42px vs 冻结 1e-4px)、iter_trans_solve 空对 UB、DISP-PSF-007(dpsf_fit_batch 无 w/h 校验 size 下溢)、p1hips make_tmp_dir 无清理 tmpfs 假败风险、挂账 10 项（HANDOFF_CONTROL_PACK_20260909.md §6）。
10. **控制包活动状态统一（GOV-002，ASTROCS-CONSTITUTION-ALIGNMENT-V1）**：活动状态唯一登记源 = `工程控制/ACTIVITY_STATE.md`（ACTIVE 唯一 = ASTROCS-CONSTITUTION-ALIGNMENT-V1；V6.1/V8.1=ARCHIVED_SUPERSEDED、V7=ARCHIVED_DISARMED，附唯一性/路径存在性机器自检与负向注入验证）。归档动作：V8.1 tracked 镜像自 `engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/` 原样移入 `engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/`（56 文件移动前后 SHA-256 零差异，git R100 56/56；BASE-001 冻结镜像台账 3 份中 CONTROL_TASK_LEDGER/CURRENT_CHECKPOINT 与冻结值一致，baseline/V7_1_STATIC_TASK_LEDGER 镜像 32e3e414 与冻结快照工作区口径 524ab719 存在历史口径差=F4，登记 ACTIVITY_STATE §5/§6）+ 目录级 README_ARCHIVED.md；`工程控制/` 解压原件原地不动仅登记。V7 线中断态残留如实登记不清理不收编：lib/snr_estimator/ 三文件 + tests/unit/p1_noise/ 未跟踪、根 CMakeLists.txt 追加块未提交（预存 8 modified 之一）。DOCUMENT_INDEX.yaml archived 区段新增 V8.1 目录聚合条目并修正 2026-09-02 条目断链 replacement（工作根 control/active/…V7… → 当前包）。REVIEW.md §3 活动包与任务状态源同步收敛到当前包 TASK_LEDGER.csv（工作根台账已不存在）。findings：F1 已闭环——ci/reconcile_state.py V71_LEDGER 常量由前台 d1ac4dfc 改指 archive 新路径，--current-first --strict 复跑 9/9 PASS rc=0（负向注入可复现修复前 rc=1）；F4 BASE-001 冻结快照（工作区口径）与 a4fdee3f 入库 blob 对 V7_1 台账登记值不一致（524ab719 vs 32e3e414），历史口径差交 owner/域外裁决；F2 预存 test_impact_map schemas/** 域失败（009ee419 遗留，与本任务无关，两文件对 HEAD 零 diff 证明）；F3 evidence/v8_1_ci_control/ 历史证据含当时路径引用按历史事实保留。
11. **DOC-CONV-001 L0 与模块状态文档收敛（2026-09-12，cprun run `Rmtxvlrtfa66eb7` rev23 / dispatch `7c0b15abaee56640`）**：BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`（HEAD=main=origin/main 三 SHA 一致）；`scientific_change=false`（不动公式/容差/冻结门/负责人裁决/生产源码）。① **状态词统一**：全 L0 文档由历史三级口径（合同冻结/源码在位/执行验收）切换为 `CONTRACT_READY`/`IMPLEMENTED`/`INSTALLED`/`VERIFIED` 阶梯（`NOT_IMPLEMENTED`/`NOT_VERIFIED`/`DEFERRED`/`DORMANT`/`FAIL` 为负向词），权威定义 = `docs/owner/RELEASE_STATUS.md` §0，并在 REVIEW.md §3 复述；与 `docs/standards/STANDARDS_REGISTRY.md` §1.4 的标准符合性取值域（CONFORMANT/PARTIAL/…）显式区分为两个独立轴。② **删除已修复项的 NOT_VERIFIED**：`run --phases` 遗留（CLI-002 已删，实测 rc=2）、§F.1 节点化（P1-001/P2-001/P3-002 已完成）、Phase3 四投影（P3-001 已实现）三处陈旧陈述以当前提交证据替换。③ **错误事实更正**：冻结四投影由旧 `SIN/ZEA/CAR/AIT` 更正为宪章 §18.1 的 `TAN+SIN+CAR+AIT`；`cli/runtime_client.cpp` Phase1 IR 由"单节点"更正为 cal→cosmetic 两节点。④ **收敛文件**：`REVIEW.md`、`docs/owner/{RELEASE_STATUS,SCIENCE_OVERVIEW,PIPELINE_OVERVIEW,ARCHITECTURE_OVERVIEW,CHANGE_REVIEW}.md`、`docs/architecture/MODULE_MAP.md`（重写为"模块/路径/交付状态/证据锚"）、`docs/review/` 五份治理评审层文档、本文件。⑤ **实测证据**（日志 `run/docconv001/logs/`）：`ninja -C build` rc=0（28/28）；ctest `p1001_real_nodes`/`p2001_real_nodes`/`p3002_real_nodes`/`p3002_uncertainty` 4/4 PASS、`p3_projection_units`/`p3_projection_fault` 2/2 PASS、`rt001_unique_executor` PASS；`tests/abi/mod001_install_load_check.py --build-dir build` 64/64 PASS（含 5 类负向注入必败）；`pytest tests/cli/test_cli001_vpi.py` 15/15 PASS；doccheck 八项（doc_index/eng_constraints/version_namespaces/l0_docs/standards_registry/api_docs/glossary/doc_symbols）全 rc=0。⑥ **域外 findings（写白名单 = REVIEW.md / docs/ / memory.md 之外均未改）**：F-DOC-CONV-001-01 `lib/*/README.md`（任务目标提及）不在写白名单 → 未改，移交 lib/ 写域；F-DOC-CONV-001-02 跨 L0 文档状态词一致性**无 CI 检查项** → 建议 tools/+ci/ 域补 checker 并同提交注册 `ci/checks.json`；F-DOC-CONV-001-03 05 号 findings 登记册与 `ci/checks.json` 在写白名单外 → 由前台并入；F-DOC-CONV-001-04 `lib/drizzle/module.yaml` `entrypoint: MISSING` 与已安装的 `MOD-P1-DRIZZLE`（`lib/drizzle/src/module_entry.cpp` 在位、安装树实测装配 PASS）矛盾，属 lib/ 写域；F-DOC-CONV-001-05 预存根目录散落运行产物（`astrocs_run_*.json`、`alloc_report.json`、`alloc_samples.csv` 等 untracked）违反 AGENTS.md 目录规范，预存 dirty 零覆盖未处置；**F-DOC-CONV-001-06（CI 红灯，P1，域外）**：`tests/version` UT-VERSION 在 HEAD=`da3c4b4a` 即红——`tools/check_version_consistency.py` rc=1，19 条 findings **全部**落在 `docs/standards/STANDARDS_REGISTRY.md`（该文件相对 HEAD 零 diff，非本任务改动；19 条均为标准**条款编号**（Paper I / HiPS 的 §a.b.c 节号）被误判为"未知版本字面量"），疑似 STD-REG-001（`fb7f232a`）引入的检查器口径缺口；本任务写白名单外（tools/ 与 registry 冻结面），只登记不修，建议 CI-REPAIR 常驻线处置（检查器排除 `§` 前缀条款号，或 registry 冻结表加机器豁免字段）。日志：`run/docconv001/logs/version_consistency_head.log`。
