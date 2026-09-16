# 差异审计（GAP_AUDIT）

基线提交：`a861d8f63a1f6c17dea2f201349006f6a6bd1ad2`（HEAD = main = origin/main，2026-09-16 编制时核对）
编制方式：对 `ASTROCS_DESIGN.md` / `ENGINEERING_SPEC.md` / `CONTROL_PACK_SPEC.md` / `docs/ci` / `docs/plugins` 逐条与仓库现状对照；每条给出可直接复跑的仓内证据。
纪律：本表只记录偏差，**不修复**。执行 BASE-001 时补充精确 SHA 与完整命令输出；执行中各任务新发现的偏差追加为 GAP-0xx 或 `UNRESOLVED`，不得擅自选择方便口径。

## 0. 基线红灯（编制时实测，必须先记录再治理）

以下三项旧治理检查在当前提交上**已经失败**，属于"文档集已替换、检查器未跟随"的直接后果，是 GOV-001 / CI-001 的开工依据：

| 检查项 | 命令 | 实测 | 原因 |
|---|---|---|---|
| AGENTS-GOV | `python3 tools/check_agents_gov.py` | rc=1，`GOV_CHECK_FAIL missing=[main-only, amd64, 节点, cpu-only, 单入口, 资源门禁, 无硬编码, alpha/发布, 状态机, 不停工]` | 检查器把**旧 AGENTS.md 的 10 组字符串**当作硬编码期望；新 AGENTS.md 按新文档集重写后全部失配 |
| ENG-CONSTRAINTS | `python3 tools/doccheck/check_engineering_constraints.py` | rc=1，`verdict=CONSTRAINTS_FAIL` | 检查器仍以 `AstroCS_ENGINEERING_CONSTRAINTS.md`（旧工程约束）为权威对象 |
| DOC-INDEX | `python3 tools/doccheck/check_doc_index.py --strict` | rc=1，`verdict=DOC_INDEX_FAIL` | `docs/DOCUMENT_INDEX.yaml` 仍索引旧文档体系，与新文档集不一致 |

> 结论：**当前仓库的机器门是红的**。任何"治理完成"的声明在 GOV-001 + CI-001 PASS 之前都不成立。

## 1. 逐条偏差

| ID | 类型 | 权威依据 | 当前证据（可复跑） | 治理任务 |
|---|---|---|---|---|
| GAP-001 | 过时/冲突 | ASTROCS_DESIGN §0；ENGINEERING_SPEC §7 | 根目录仍并列 `ASTROCS_PROJECT_CONSTITUTION.md`(39903B)、`AstroCS_ENGINEERING_CONSTRAINTS.md`(10061B)、`REVIEW.md`(20936B)、`HANDOVER.md`(9216B)、`VERSION`；ENGINEERING_SPEC §7 的根固定条目表中不含这些文件（`ls -la` 可复核） | GOV-001、DOC-001 |
| GAP-002 | 过时 | ASTROCS_DESIGN §0 | `README.md`、`REVIEW.md`、`HANDOVER.md`、`memory.md` 仍以旧宪章/旧控制包/已删除路径描述现状 | DOC-001 |
| GAP-003 | 违规 | ASTROCS_DESIGN §7.1；ENGINEERING_SPEC §7 | `ls -d lib/algorithms lib/infrastructure` → 两者均不存在；`ls lib/` 实为 31 个扁平旧目录（calibration、cosmetic、star_detector、plate_solve、photometric_calib、snr_estimator、drizzle、phase1/2/3_*、orchestrator、io、hips* …），另有根 `cli/`、`runtime/`、`providers/`、`modules/` | MOD-001、ARCH-001、INT-001 |
| GAP-004 | 漂移 | ASTROCS_DESIGN §7.3 | 根 `CMakeLists.txt`(43614B, 9月16日) 仍集中装配大量旧命名 target；算法模块未统一为独立 DLL/SO + 单 entrypoint | MOD-001、ARCH-001、P1-001、P2-001、P3-001 |
| GAP-005 | 违规 | ASTROCS_DESIGN §6.2；plugins/18_cli | `cli/parser.cpp`、`cli/commands.cpp` 注册并派发 `phase1/phase2/phase3 …`；未提供唯一命令树 `normalize/mosaic/export`（旧检查器 `tools/check_cli_command_layer.py` 反而以 15 个 `cmd_*` 旧命令层为 PASS 判据，rc=0） | CLI-001 |
| GAP-006 | 缺口 | ASTROCS_DESIGN §3.3/§6 | `ls config` → 不存在；无 `config/defaults.json`、`config/filters.json`、三命令 `phase_config` schema 与模板/预检合同 | CFG-001、CLI-002 |
| GAP-007 | 漂移 | UNIFIED_MODEL §2-§3 | 合同分散于 `contracts/config`、`contracts/data`、`contracts/schemas/v6`；`docs/contracts/{INDEX.yaml,DATA_ARTIFACTS.md,DATA_SEMANTICS.md}` 与 `docs/contracts/v6/**` 并存两套口径；未形成新对象词汇 + 三类配置分离的单一合同链 | DATA-001、CFG-001 |
| GAP-008 | 无主/漂移 | plugins/00_INDEX；ENGINEERING_SPEC §4 | 23 篇插件文档与现有 `modules/*/module.yaml`、`lib/*`、`docs/modules/registry/*`、根 CMake target、产品清单之间没有一一机器映射 | MOD-001、QA-001 |
| GAP-009 | 漂移 | ASTROCS_DESIGN §3 | normalize 生产路径仍由 `lib/phase1_session` + `lib/phase1` + 适配层组织；`frame_snr`、稀疏 SNR 层、结构化 JSON 输出未按新 §3.4 合同核验（`lib/snr_estimator` 仍未入库） | P1-001、P1-002 |
| GAP-010 | 漂移 | ASTROCS_DESIGN §4 | mosaic 仍以 `lib/phase2_session`、`lib/phase2_int/rej/samp/upm` 旧端口命名组织；按需 SNR→逆方差、三目标权重与分块确定性未按新 §4.3 核验 | P2-001、P2-002 |
| GAP-011 | 缺口 | ASTROCS_DESIGN §5.3 | 新设计冻结八投影 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA；现有 `lib/phase3_proj` 实现/测试/生产接入与统一 registry 未逐项核对到位 | P3-001 |
| GAP-012 | 漂移 | ASTROCS_DESIGN §5/§9 | Phase3 重采样（`lib/phase3_rsmp`）、FITS 输出（`lib/phase3_fits`）、模式语义、WCS Oracle、原子发布分散在旧模块与 Session | P3-002、AIO-001 |
| GAP-013 | 漂移 | plugins/17_aio；ASTROCS_DESIGN §9 | I/O 所有权分散在 `lib/astro_image_io`、`lib/io`、`lib/hips*`、`lib/healpix_db`、`runtime/` 与各阶段 writer；无唯一 AIO 入口 | AIO-001 |
| GAP-014 | 漂移 | plugins/19_runtime | 调度/管线/监控分散于 `lib/orchestrator`、`lib/core`、`runtime/`、`cli/runtime_client.cpp`；未收敛为单 scheduler + 统一线程预算 + typed DAG | RT-001 |
| GAP-015 | 漂移 | ASTROCS_DESIGN §8；plugins/20/21 | CPU provider/profile（`lib/backend_host`、`providers/`）与资源门（`ci/resource_monitor.py`、`cli/resource_gate.h`）仍绑定旧命名与旧门禁文档 | CPU-001、OBS-001 |
| GAP-016 | 缺口 | docs/ci/01_CHECKS；CI_SPEC §5 | 文档要求 `python3 ci/run_checks.py --all/--check/--json-out`；实际入口是 `ci/run.py`，`ci/checks.json` 有 145 项、ID 为旧语义（如 `AGENTS-GOV`、`ENG-CONSTRAINTS`、`VERSION-CONSISTENCY`），与 `CHK-*` 注册语义不一致 | CI-001、QA-001 |
| GAP-017 | 冲突 | ASTROCS_DESIGN §12 | `VERSION` 内容为 `0.11.0-alpha.2`；`ci/checks.json` 的 `VERSION-CONSISTENCY` 以 `--expected 0.11.0-alpha.2` 硬绑定旧版本号（当前实测 rc=0，即红灯被"旧值自洽"掩盖）；新设计规定 Alpha 前程序与产物不含版本信息 | GOV-001、PKG-001 |
| GAP-018 | 漂移 | ASTROCS_DESIGN §10 | `packaging/astrocs.product.json` 与安装树仍描述旧模块/命令形态 | PKG-001 |
| GAP-019 | 过时 | CONTROL_PACK_SPEC §3 | 活动 `tests/**`、`tools/**`、`docs/**` 中仍存在指向已删除旧控制包路径的引用（旧 `工程控制/*`、`engineering/control/archive/**` 已随 a861d8f6 删除） | GOV-001、DOC-001、QA-001 |
| GAP-020 | 未验证 | ASTROCS_DESIGN §11 | 新文档集对应的双平台 CI、Linux 真实数据终验、Windows/Fatduck 复验与图像审核证据尚未产生 | QA-001、REAL-001、FINAL-001 |

## 2. 未决项处理

- 执行中发现的文档互相冲突、科学定义歧义、或无法从实际代码证明的状态，追加为 `UNRESOLVED` 并写明"缺什么才能判定"；
- 不得为通过检查而改写权威文档，不得用 waiver 掩盖红灯（CONTROL_PACK_SPEC §7.3）。
