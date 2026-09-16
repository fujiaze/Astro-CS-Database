# 差异审计（GAP_AUDIT）

基线提交：`a861d8f63a1f6c17dea2f201349006f6a6bd1ad2`（编制时 HEAD = main = origin/main 三者一致）
编制方式：对 `ASTROCS_DESIGN.md` / `ENGINEERING_SPEC.md` / `CONTROL_PACK_SPEC.md` / `docs/ci` / `docs/plugins` 逐条与仓库现状对照；每条给出可直接复跑的仓内证据。
复核：本文经一次独立只读复核（对照章节原文 + 逐条重跑证据），已按复核结论修正判定、补全证据并新增 GAP-021；仍无法判定的条目集中在 §3。
纪律：本表只记录偏差，**不修复**。执行 BASE-001 时补充精确 SHA 与完整命令输出；执行中各任务新发现的偏差追加为 GAP-0xx 或 `UNRESOLVED`，不得擅自选择方便口径。

## 0. 基线红灯（编制时实测，必须先记录再治理）

以下三项旧治理检查在当前提交上**已经失败**，属于「文档集已替换、检查器未跟随」的直接后果，是 GOV-001 / CI-001 的开工依据：

| 检查项 | 命令 | 实测 | 原因 |
|---|---|---|---|
| AGENTS-GOV | `python3 tools/check_agents_gov.py` | rc=1；`GOV_CHECK_FAIL missing=[main-only, amd64, 节点, cpu-only, 单入口, 资源门禁, 无硬编码, alpha/发布, 状态机, 不停工]` | 检查器把**旧 AGENTS.md 的 10 组字符串**当作硬编码期望（`tools/check_agents_gov.py` 的 `REQUIRED` 列表）；新 AGENTS.md 按新文档集重写后全部失配 |
| ENG-CONSTRAINTS | `python3 tools/doccheck/check_engineering_constraints.py` | rc=1；`verdict=CONSTRAINTS_FAIL` | 检查器仍以 `AstroCS_ENGINEERING_CONSTRAINTS.md`（旧工程约束）为权威对象 |
| DOC-INDEX | `python3 tools/doccheck/check_doc_index.py --strict` | rc=1；`verdict=DOC_INDEX_FAIL` | `docs/DOCUMENT_INDEX.yaml` 仍索引旧文档体系，与新文档集不一致 |

> 结论：**当前仓库的机器门是红的**。任何「治理完成」的声明在 GOV-001 + CI-001 PASS 之前都不成立。

## 1. 逐条偏差

### GAP-001　过时/冲突：根目录并列旧权威/旧状态入口

- **权威依据**：ENGINEERING_SPEC §7 根固定条目白名单（直接违规依据）；ASTROCS_DESIGN §0 只定义权威层级（本文 > AGENTS > ENGINEERING_SPEC > CONTROL_PACK_SPEC > docs/ci > 插件文档）。
- **证据**：以下 5 项均 tracked 且不在 §7 白名单内——`ASTROCS_PROJECT_CONSTITUTION.md`(39903B)、`AstroCS_ENGINEERING_CONSTRAINTS.md`(10061B)、`REVIEW.md`(20936B)、`HANDOVER.md`(9216B)、`VERSION`(15B)。
- **治理任务**：GOV-001、DOC-001

### GAP-002　过时：活动文档把旧权威/已删路径当现状

- **权威依据**：ASTROCS_DESIGN §0（冲突以本文为准；下级文档不得放宽或重新解释）。
- **证据**：
  - `README.md` 称根 `ASTROCS_PROJECT_CONSTITUTION.md` 是 FROZEN 唯一最高约束；
  - `REVIEW.md` 称其为「冻结宪章之下唯一目标态设计总规范」；
  - `HANDOVER.md` 引用已删控制包路径 `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909`（`ls` → 不存在）；
  - `memory.md` 称旧宪章为「唯一最高约束」，并自报旧基线 SHA `da3c4b4a`（≠ 当前 `a861d8f6`）。
- **治理任务**：DOC-001

### GAP-003　违规：目标源码根不存在，源码平铺且混合三种性质

- **权威依据**：ASTROCS_DESIGN §7.1（顶层结构唯一：`lib/algorithms/` 与 `lib/infrastructure/`）；ENGINEERING_SPEC §7 同款。
- **证据**：`ls -d lib/algorithms lib/infrastructure` → 两者均不存在；`lib/` 顶层 31 项平铺，既非 §7.1 的二分，又混有 `core`、`common`、`backend_host`、`orchestrator`、`io`、`astro_image_io`、`hips`、`hips_p2`、`healpix_db` 等无设计节点的目录；另有根 `cli/`、`runtime/`、`providers/`、`modules/`。
- **名称可对应（职责边界未验证）**：calibration↔calibration、cosmetic↔cosmetic、star_detection↔star_detector、psf↔dynamic_psf、platesolve↔plate_solve、photometry↔photometric_calib、noise_snr↔snr_estimator、drizzle↔drizzle、coverage↔hips_p2、sampling↔phase2_samp、upm↔phase2_upm、rejection↔phase2_rej、integration↔phase2_int、projection↔phase3_proj、resample↔phase3_rsmp、fits_output↔phase3_fits、shared↔common。
- **治理任务**：MOD-001、ARCH-001、INT-001

### GAP-004　漂移：可调度模块不是独立 DLL/SO，整阶段 Session 仍在编译

- **权威依据**：ASTROCS_DESIGN §7.3（每个可独立调度的算法/基建模块 = 独立 DLL/SO，单一 entrypoint，不隐藏整阶段 Session）。
- **证据**：根 `CMakeLists.txt` 共 25 条实际 `add_library`，其中 **SHARED 仅 3 个**（`lib/astrocs_runtime`、`lib/astrocs_io`、`lib/astrocs_cpu_baseline`），其余 **STATIC 22 个**；三个整阶段 Session 仍为 STATIC（`astrocs_phase1_session`、`astrocs_phase2_session`、`astrocs_phase3_session`）。
- **UNRESOLVED（U-01）**：「哪些算法模块本应可独立调度」缺权威清单，需 MOD-001 的 23 模块映射表作为判定口径。
- **治理任务**：MOD-001、ARCH-001、P1-001、P2-001、P3-001

### GAP-005　违规：命令树是旧 phase 命令，三命令名完全不存在

- **权威依据**：ASTROCS_DESIGN §6.2（唯一命令树 normalize/mosaic/export + help/--version/doctor/benchmark；phase1|2|3 仅为内部指代）；docs/plugins/infrastructure/18_cli.md。
- **证据**：
  - `cli/parser.cpp` 的 `kRules` 注册 27 条命令，含 `phase1 run`、`phase2 run`、`phase3 run`，以及 `validate/plan/inspect`、`hardware inspect`、`modules list/verify`、`selftest`、`config init/validate/show-effective`、`benchmark cpu`、`test synthetic`、`drizzle`、`verify` 等旧命令；
  - `cli/commands.cpp` 只派发 `phase1|2|3 run`，未识别命令走 `cmd_stub`；
  - 在 `cli/parser.cpp`、`cli/commands.cpp`、`cli/main.cpp` 中检索 `normalize`/`mosaic`/`export` → **零命中**（排除 `export-mode`），即三命令名根本不在 CLI 面；
  - 旧检查器 `python3 tools/check_cli_command_layer.py` 实测 **rc=0**（`CLI-001_PASS: 15 cmd_* uniform, 27 unique paths, kRules dispatch, stable exits`）：它把旧命令层当合格判据。
- **附带缺口**：§6.2 要求的 `help` 与 `<cmd> --template` 均未注册（`kValueFlags` 无 `--template`，`kRules` 无 `help`，仅有 `--help/-h`）。
- **治理任务**：CLI-001、CLI-002

### GAP-006　缺口：无 config/，且无任何等价物

- **权威依据**：ASTROCS_DESIGN §3.3（程序根 `config/filters.json`、`config/defaults.json`）、§6.2（三命令 `--template`）。
- **证据**：`ls -d config` → 不存在；4 层深度检索 `defaults.json` / `filters.json`（排除 `build`）→ 零结果；`contracts/` 下检索 `*phase_config*` / `*template*` → 零结果；CLI 未注册 `--template`。判定：**无等价物**，不是「位置不同」。
- **治理任务**：CFG-001、CLI-002

### GAP-007　漂移：合同分散在 4 个子域 + 两套文档口径

- **权威依据**：docs/design/UNIFIED_MODEL.md §2（数据对象表）、§3（三类配置严格分离）。
- **证据**：`contracts/` 顶层 4 个子域——`config/`（4 schema）、`data/`（8 文件）、`proposals/v6`、`schemas/`（6 schema 及 `schemas/v6/`）；同时 `docs/contracts/` 下的 `DATA_SEMANTICS.md`、`DATA_ARTIFACTS.md`、`INDEX.yaml` 与 `docs/contracts/v6/**` 并存两套口径。
- **UNRESOLVED（U-02）**：「单一合同链」的判定标准（唯一索引？冻结版本？）未在 UNIFIED_MODEL §2-§3 给出；本条属结构性判断，需 DATA-001 先定义判定口径。
- **治理任务**：DATA-001、CFG-001

### GAP-008　无主/漂移：23 篇插件文档与实现/注册表存在两套 id 与端口

- **权威依据**：docs/plugins/00_INDEX.md §2/§3/§5；ENGINEERING_SPEC §4（每模块必备 7 项）、§8。
- **证据**：23 篇插件文档在位（algorithms_phase1 8 + algorithms_phase2 5 + algorithms_phase3 3 + infrastructure 7）；仓库有 **23 份 module.yaml**（`lib/` 下 21 + `modules/conformance/` 的 echo/noop 2）、**26 份** `docs/modules/registry/*.md`；两侧 id 与端口词汇不同：module.yaml 用 `astrocs.p1.calibration` + 端口 `p1.frames`，registry 文档用 `astrocs.phase1.calibration` + 端口 `frames`。数量 23 ≠ 26，且无一一映射表。
- **治理任务**：MOD-001、QA-001

### GAP-009　漂移：normalize 会话仅 4 阶段，帧级 SNR 实为 5σ 深度且未入文件头

- **权威依据**：ASTROCS_DESIGN §3.2（节点流程）、§3.4（输出合同：帧级 SNR 入文件头 + 可选稀疏层 + 结构化 JSON）；docs/plugins/algorithms_phase1/07_noise_snr.md。
- **证据**：
  - `lib/phase1_session` 的会话阶段仅 4 个：`io_read`、`calibrate`、`cosmetic`、`io_write`，与 §3.2 的节点流程不同构；
  - 适配层 `lib/core/src/module_adapters.cpp` 明确注释「不再输出任何整帧 SNR 标量」，其写入的 `frame_snr` 字段实为 **5σ 点源深度**（flux5_adu / m5_mag），落 `output_dir/p1_snr.json`；
  - `lib/astro_image_io/src/hips/` 下检索 `frame_snr` / `FRAMESNR` / `sparse` → **全部零命中**（帧级 SNR 未进 HiPS 文件头，稀疏层不存在）；
  - **更正编制初稿的错误断语**：`lib/snr_estimator` **已入库**（`git ls-files` 计 24 个文件，含 `module.yaml`、`README.md`、C 源）；但根 `CMakeLists.txt` 的相关注释仍称该子图「从未入库、锁死不收编」，与其 `add_subdirectory` 被注释、仅 `cpp/src/snr_science.cpp` 以源文件方式编入的现状**注释与事实冲突**；该模块无独立 target。
- **治理任务**：P1-001、P1-002

### GAP-010　漂移（部分）：三目标权重已实现，SNR→逆方差与分块确定性未证明

- **权威依据**：ASTROCS_DESIGN §4.2（固定科学流程）、§4.3（SNR 检测与逆方差叠加）。
- **证据（已实现）**：`lib/phase2_int/v6/src/phase2_integrate.cpp` 已实现三种生产权重 token（`point_information` / `surface_gls` / `psfsw_robust`），并有逐像素 ivar 层（`ivar=1/variance`）与 `weight_obj` 输出。
- **证据（结构仍是旧形态）**：`lib/phase2_session` 仍以单阶段会话组织（`p2_session.cpp` 仅 1 处 stage `coverage`）；`lib/phase2_int`、`lib/phase2_rej`、`lib/phase2_samp`、`lib/phase2_upm` 仍为旧目录命名。
- **UNRESOLVED（U-03）**：未见「由输入帧 SNR 反算权重」的调用链证据。
- **UNRESOLVED（U-04）**：分块/worker 确定性缺可复跑机器证据（本次为只读扫描，未构建复跑）。
- **治理任务**：P2-001、P2-002

### GAP-011　缺口（确定）：两个投影 registry 都只有 4 个投影，缺 STG/MOL/CEA/ZEA

- **权威依据**：ASTROCS_DESIGN §5.3（冻结八投影 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）。
- **证据**：`lib/phase3_proj/p3_projection.cpp` 的 legacy 冻结表只有 4 行（TAN/SIN/CAR/AIT），`p3_projection_registry_table` 返回 count=4，并有 `if (i < 0 || i >= 4)` 边界；`lib/phase3_proj/p3_proj_v6.h` 的 `ProjectionId` 仅 `kTAN/kSIN/kCAR/kAIT`，`p3_proj_v6.cpp` 表同样 4 行；在 `lib/phase3_proj` 内检索 `STG`/`MOL`/`CEA`/`ZEA` → 无实现。
- **治理任务**：P3-001

### GAP-012　漂移：Phase3 重采样/FITS/WCS/原子发布分散且无自有构建

- **权威依据**：ASTROCS_DESIGN §5.2（反向映射 + 重采样、流式 FITS、独立 WCS 验证、原子发布）、§9（AIO 唯一读写边界）。
- **证据**：实现分散在 `lib/phase3_session`（`p3_resample.cpp`、`p3_wcs.cpp`、`p3_output.cpp`）、`lib/phase3_proj`、`lib/phase3_rsmp`、`lib/phase3_fits`；其中 `lib/phase3_fits` 与 `lib/phase3_proj` **无自有 CMakeLists**，装配集中在根 `CMakeLists.txt`（仅 `add_subdirectory` 了 `v6/phase3_rsmp`）。
- **治理任务**：P3-002、AIO-001

### GAP-013　漂移：I/O 所有权分散，无唯一 AIO

- **权威依据**：docs/plugins/infrastructure/17_aio.md（唯一 I/O 边界）；ASTROCS_DESIGN §9（aio 是唯一 FITS/HiPS/manifest 读写边界，禁止各自复制 reader/writer）。
- **证据**：并存 `lib/astro_image_io`（`aio_fits.cpp`、`hiss_writer.cpp`、ahpx/healpix/hips 子目录）、`lib/io`（`io_adapter.cpp`）、`lib/hips`、`lib/hips_p2`、`lib/healpix_db`、`runtime/io`（`hips_output_store.py`、`fits_verify.py`）、`runtime/artifact_store`；`lib/astro_image_io` 亦无自有 CMakeLists。
- **治理任务**：AIO-001

### GAP-014　漂移：调度/线程预算/监控双实现并存

- **权威依据**：docs/plugins/infrastructure/19_runtime.md；ASTROCS_DESIGN §7.1（scheduler/pipeline/observability 唯一归属）。
- **证据**：单 scheduler 已部分收口在 `lib/core/src/`（`scheduler.cpp`、`executor.cpp`、`runtime.cpp`、`pipeline.cpp`、`checkpoint.cpp`），但线程预算与监控另有 `runtime/pipeline/`（typed DAG）、`runtime/monitoring/`（`monitor.py`、`runner.py`、`windows_pdh_etw.py`、`linux_procfs.py`）、`runtime/core/phase_lifecycle.py`、`runtime/v6_budget.py`，以及 `cli/runtime_client.h` / `cli/runtime_client.cpp` 双实现；`lib/orchestrator` 无自有 CMakeLists。
- **治理任务**：RT-001

### GAP-015　漂移：CPU provider/profile 与资源门双实现，CI 仍绑定旧门禁文档

- **权威依据**：ASTROCS_DESIGN §8（benchmark 生成绑定 CPU/OS/版本/provider 哈希的 cpu_profile、单一线程预算源）；docs/plugins/infrastructure/20_benchmark.md、21_observability.md。
- **证据**：provider 仅 `providers/cpu/`；CPU/profile 实现集中在 `lib/backend_host`（25 文件，含 `profile_gen.cpp`、`profile_gen_v2.cpp`、`bench_harness.cpp`、`worker_advisor.cpp`）；资源门双实现：`ci/resource_monitor.py`（自述为 `tools/monitoring/run_monitored.py` 的桥接 shim）与 `cli/resource_gate.h`；CI 侧仍绑定旧门禁文档：`ci/checks.json` 的 `ENG-CONSTRAINTS` 条目 `changed_paths` 含 `AstroCS_ENGINEERING_CONSTRAINTS.md`，`AGENTS-GOV` 条目含 `memory.md`。
- **治理任务**：CPU-001、OBS-001

### GAP-016　缺口：CI 入口名不符，且 145 项注册中 CHK-* 为 0

- **权威依据**：docs/ci/01_CHECKS.md §2/§5（27 个 `CHK-*` 检查项；运行方式 `python3 ci/run_checks.py --all/--check/--json-out`）；docs/ci/CI_SPEC.md §5。
- **证据**：
  - `ls ci/` 无 `run_checks.py`；实际入口 `ci/run.py`（自述「唯一 CI 入口」），workflow 只调用 `ci/run.py`、`ci/wf_step.py`、`ci/bootstrap.py`；
  - `ci/checks.json`：`schema_version=1`，`checks` 共 **145** 项，其中 `CHK-*` 注册 **0 项**——与 01_CHECKS.md 的 27 个 `CHK-*` 完全不是同一 ID 空间；
  - 仍绑定旧治理文本的检查项至少 6 项：`VERSION-CONSISTENCY`（`--expected 0.11.0-alpha.2`，changed_paths 含 VERSION / REVIEW.md）、`AGENTS-GOV`（`tools/check_agents_gov.py`，含 memory.md）、`ENG-CONSTRAINTS`（含 `AstroCS_ENGINEERING_CONSTRAINTS.md`）、`VERSION-NAMESPACES`（`tools/doccheck/check_version_namespaces.py`）、`UT-VERSION`（`tests/version`）、`DOC-INDEX`（`docs/DOCUMENT_INDEX.yaml`）。
- **治理任务**：CI-001、QA-001

### GAP-017　冲突：Alpha 前存在版本信息，且已进入程序与产物

- **权威依据**：ASTROCS_DESIGN §12（Alpha 之前程序与代码不包含任何版本信息）；ENGINEERING_SPEC §7 同款。
- **证据**：
  - `VERSION` 内容 `0.11.0-alpha.2`；
  - `ci/checks.json` 的 `VERSION-CONSISTENCY` 以 `--expected 0.11.0-alpha.2` 硬绑定旧版本号，**实测 rc=0**——属「旧值自洽」掩盖，不是干净；
  - `CMakeLists.txt` 的 `project(astrocs VERSION 0.11.0)` 与读取 `VERSION` 生成 `ASTROCS_VERSION_STRING`；
  - 版本串已进入程序面（`cli/commands.cpp` 多处使用 `ASTROCS_VERSION_STRING`，含 `astrocs %s` 输出）与 `module.yaml` 的 `module_version: 0.11.0-alpha.2`。
- **治理任务**：GOV-001、PKG-001

### GAP-018　漂移：产品名与安装树仍为旧形态

- **权威依据**：ASTROCS_DESIGN §10.1（唯一入口 `ACSD Cli.exe` / `acsd_cli`；解压目录 = 唯一 exe + 各 dll + schemas + manifest）、§10.2。
- **证据**：
  - **可执行名硬冲突**：设计要求 `ACSD Cli.exe` / `acsd_cli`，实际根 `CMakeLists.txt` 的 `add_executable(astrocs …)` 与 `cmake/install_layout.cmake` 的安装名均为 `astrocs` / `astrocs_p1_*.so`；在 `packaging/`、`cmake/`、`docs/`、`ci/` 内检索 `acsd` → **零命中**；
  - `packaging/astrocs.product.json`：`product_version=0.11.0-alpha.1`（≠ VERSION 的 `0.11.0-alpha.2`）、`source_commit=9f6b72b5…`（≠ 当前基线）、units=10，其中 `PLATFORM-RUNTIME` / `PLATFORM-IO` 状态仍为 `SKELETON`；
  - `packaging/install-tree.contract.json` 的 `target_version` 同为 `0.11.0-alpha.1`，`root_layout` 仍写「Linux 技术预览」。
- **治理任务**：PKG-001

### GAP-019　过时：活动面仍引用已删除的控制包路径与归档目录

- **权威依据**：CONTROL_PACK_SPEC §3（现状须可复核，不得凭印象）。
- **证据**：被引用的两个控制包目录 `ls` 均不存在（`工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909`、`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915`）；`engineering/` 现为**空目录**，`engineering/control` 不存在；但活动面仍有 30+ 处引用，例如 `tests/contracts/v6/tools/gen_data_dictionary.py`、`tests/integration/v6_p2/EVIDENCE.md`、`tests/unit/core_pipeline_test.cpp`、`tools/realdata/README.md`、`tools/config_consistency_check.py`、`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json`。
- **UNRESOLVED（U-05）**：哪些引用属「活动合同/测试输入」（必须修），哪些只是历史溯源注记（可保留），缺判定口径。
- **治理任务**：GOV-001、DOC-001、QA-001

### GAP-020　未验证：新文档集对应的终验证据尚未产生

- **权威依据**：ASTROCS_DESIGN §11（验证体系；VERIFIED = 正式平台 Windows x64 + 真实数据）。
- **证据**：`reports/v6/release-review/` 内自述「Agent 图像初审 NOT_MET」「Windows/Fatduck 复验 AWAITING（Fatduck 不可达、Windows CI 无候选）」；`REVIEW.md` 自述「GitHub Linux CI 仍红（THREAD-BUDGET / CTEST-REGISTRATION）…NOT_READY，未发布」。
- **UNRESOLVED（U-06）**：Linux 真实数据终验的当前实际状态（本次只读扫描未复跑真实数据流）。
- **治理任务**：QA-001、REAL-001、FINAL-001

### GAP-021　违规：仓库根被运行产物与未登记目录污染

- **权威依据**：ASTROCS_DESIGN §6.3（运行产物只落配置 `output_dir`，不得以进程 CWD 作隐式缺省写出）；ENGINEERING_SPEC §7（任何新产物必须落位到对应目录，禁止散落根目录；新根条目须先登记并经负责人确认）。
- **证据**：工作区根顶层 **126** 个条目，其中 tracked **46**、untracked **80**。对照 §7 白名单：
  - 「文档允许但不存在/为空」：`engineering/` 存在但**为空**；
  - 「存在但文档未列」（tracked）：`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`cli/`、`runtime/`、`providers/`、`modules/`、`设计大纲/`、`问题扫描/`、`run/`；
  - 「未跟踪产物类条目」（untracked）：`astrocs_run_*.json` **62** 个、`astrocs_p1sess_neg`、`astrocs_p1sess_perf`、`astrocs_p1sess_props`、`astrocs_p1sess_test`、`alloc_report.json`、`alloc_samples.csv`、`resource_samples.csv`、`resource_summary.json`、`worker_balance.csv`、`run_context.json`、`p8-files.patch` 等 patch、`build`、`out`、`logs`、`Testing`、`.pytest_cache`、`worktrees` 等。
- **UNRESOLVED（U-07）**：`run/` 在 §7 被标为 gitignore，但实际 **tracked**——文档与既成事实冲突，需负责人裁决口径。
- **治理任务**：OBS-001（产物归位）、GOV-001（根条目处置）、BASE-001（预存边界登记）

### GAP-022　违规：仓库根被大量散落条目污染（不止 git 脏，物理上就脏）

- **权威依据**：ENGINEERING_SPEC §7（禁止散落根目录；新根条目须先登记并经负责人确认）；ASTROCS_DESIGN §6.3（运行产物只落 output_dir）。
- **证据（逐项实测）**：工作区根顶层 **133** 个条目（tracked 46 / untracked 87）。相对 §7 白名单：
  - **一次性运行产物落根**：`astrocs_run_*.json` **62 个**、`astrocs_p1sess_neg`、`astrocs_p1sess_perf`、`astrocs_p1sess_props`、`astrocs_p1sess_test`、`alloc_report.json`、`alloc_samples.csv`、`resource_samples.csv`、`resource_summary.json`、`worker_balance.csv`、`run_context.json`；
  - **构建/缓存残留**：`build/` 6.0G、`out/`、`Testing/`、`.pytest_cache/`、`logs/`；
  - **补丁文件散落**：`p8-files.patch`、`p9-files.patch`、`p10-files.patch`、`p11-files.patch`、`p15a-files.patch`（合计约 105KB）；
  - **未登记目录**：`graph/`、`worktrees/`、`CS/`、`Database/`、`AstroCS.wiki/`、`engineering/`（空）、`BASS DR3/`、`GaiaDR3/`、`GaiaDR3SP/`（后三者属用户数据，已在 .gitignore 内）；
  - **tracked 但不在白名单**：`设计大纲/`（101M）、`问题扫描/`（20M）、`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`run/`（§7 标 gitignore 却 tracked `run/.gitkeep`）；
  - **磁盘影响**：`run/` **102G / 634993 文件**（`run/perf-fix` 59G、`run/release-rescue` 29G），/workspace 503G 已用 293G。
- **为什么单列一条**：GAP-021 只登记了「根条目与未跟踪产物」，本条明确其**规模、分类与磁盘后果**，并作为 ROOT-001/002/003 的开工依据；治理口径是「物理根目录干净」，不是「git status 干净」——事实上 `run/` 与多数根产物已被 ignore，git 看不出它们的存在。
- **治理任务**：ROOT-001（账本 + 清运）、ROOT-002（长效机器门）、ROOT-003（run/ 保留策略）
## 2. 基线门复核记录

| 命令 | 编制时实测 | 说明 |
|---|---|---|
| `python3 tools/check_agents_gov.py` | rc=1 | 见 §0 |
| `python3 tools/doccheck/check_engineering_constraints.py` | rc=1 | 见 §0 |
| `python3 tools/doccheck/check_doc_index.py --strict` | rc=1 | 见 §0 |
| `python3 ci/check_version.py --expected 0.11.0-alpha.2` | rc=0 | **不是干净**：红灯被旧版本号自洽掩盖（GAP-017） |
| `python3 tools/check_cli_command_layer.py` | rc=0 | **不是干净**：以旧命令层为合格判据（GAP-005） |
| `python3 tools/check_l0_docs.py` | rc=0 | 绑定旧活动文档集合（GAP-001 / GAP-016 范围） |
| `python3 tools/check_module_readmes.py` | rc=0 | 只查少量模块 README 链接（GAP-008） |

## 3. UNRESOLVED 清单（需负责人或后续任务给判定口径）

| ID | 无法判定的内容 | 缺什么才能判定 | 归属任务 |
|---|---|---|---|
| U-01 | 哪些算法模块本应可独立调度 | 23 模块映射表（MOD-001 产物）才能逐模块判定 | MOD-001 |
| U-02 | 「单一合同链」的唯一性判定标准 | UNIFIED_MODEL 或 contracts 的索引/冻结口径 | DATA-001 |
| U-03 | 是否存在「由输入帧 SNR 反算权重」的调用链 | 需在 phase2 生产路径定位具体函数与行 | P2-002 |
| U-04 | 分块/并行确定性是否有机器证据 | 需可复跑的确定性测试项与运行输出 | P2-002 |
| U-05 | 哪些已删路径引用属活动合同输入、哪些属历史注记 | 负责人给出「活动引用」判定口径 | GOV-001 |
| U-06 | Linux 真实数据终验的实际状态 | 真实数据流的可复跑命令与产物路径 | REAL-001 |
| U-07 | `run/` 应 gitignore 还是 tracked | 负责人对 §7 与既成事实的裁决 | GOV-001 |

## 4. 未决项处理

- 任何执行中发现的文档互相冲突、科学定义歧义、或无法从实际代码证明的状态，追加为 `UNRESOLVED` 并写明「缺什么才能判定」；
- 不得为通过检查而改写权威文档，不得用 waiver 掩盖红灯（CONTROL_PACK_SPEC §7.3）。

---

### 5.6 执行期新发现（追加为 GAP-024 / GAP-025）

**GAP-024　缺口：设计点名的配置默认值缺权威数值**

- 发现者：工程包调度线（W1，CFG-001 开工前扫描）。权威依据：ASTROCS_DESIGN §3.3 点名 `config/defaults.json` 应含这些值。
- 证据：docs/science/** 与 docs/algorithms/** 全库检索**无数值**——「暗场-亮场曝光容差」（`docs/science/CALIBRATION.md` 只有 `K=t_light/t_dark` 语义）、「稀疏层控制点密度」（`docs/plugins/algorithms_phase1/07_noise_snr.md` 只有字段名与单位「点/度²」）、「标量门」阈值（同文件只说失败要升级空间模型）。另：drizzle `pixfrac` 的**语义**权威在 `docs/science/DRIZZLE.md`（(0,1]、half=0.5*pixfrac）与 `docs/algorithms/DRIZZLE_GEOMETRY.md`，但**数值默认 0.8 无权威出处**（仅见旧文档 `docs/development/CONFIG_SCHEMA.md`）。
- 判定：**权威数值缺口**，不是实现缺口。按 ENGINEERING_SPEC §3 与 AGENTS.md 硬禁令，Agent 不得编造数值。
- 处置：CFG-001 照常执行——有来源的字段逐条转录并标 source；无来源项以 `pending_authority` 显式占位登记，不编造、不放宽 schema。
- 治理任务：CFG-001（标注）+ 下一轮工程包（补齐数值定义或负责人裁决）

**GAP-025　漂移：滤镜曲线库 provenance 自述未验证**

- 发现者：工程包调度线（W1）。权威依据：ENGINEERING_SPEC §4（端口引用有效 DATA 合同）、docs/plugins/algorithms_phase1/06_photometry.md。
- 证据：`lib/photometric_calib/data/response_curves/filters.json`（183763 B；name/channel/wavelength_nm[]/value[]/n_points）配套 `lib/photometric_calib/cpp/test/filter_qe_provenance.json`，其 source 自述 `unverified: original curve source not recorded in repository`，即**滤镜响应曲线原始出处未记录**；而 photometry 绝对定标锚在模型通带 T(λ)·Q(λ)·λ 上（docs/science/PHOTOMETRY.md:7）。
- 判定：曲线可用但**溯源链缺失**，属可复现性与科学溯源性风险（非当前实现错误）。
- 处置：CFG-001 只做转录与 provenance 标注；补 provenance（原始来源/版本/获取方式/校验和）登记为下一轮工程包候选任务。
- 治理任务：下一轮工程包（provenance 补齐专项）
**GAP-026　冲突/漂移：项目 Wiki 自称唯一权威，且本地 clone 失踪（§7 亦未列 Wiki）**

- **发现者**：负责人质询「wiki 为啥变成空目录」后，前台实测（2026-09-16）。
- **权威依据**：ASTROCS_DESIGN.md §0 权威链（**Wiki 不在链上**；唯一权威是仓库内文档集）；ENGINEERING_SPEC §7（根目录固定条目清单）。
- **证据**：
  1. 远端 Wiki 可达：`git ls-remote https://github.com/fujiaze/Astro-CS-Database.wiki.git` → `HEAD = refs/heads/master = 9017258…`，**34 页 / 148K**；
  2. 其 `Home.md` 第 3 行写「**本 Wiki 是项目唯一权威标准**」——与权威链**直接冲突**；
  3. **已漂移**：22 页含 `HISS`（废止名）、4 页 `HICS`、2 页 `Phase3`；现行三命令名中 `normalize` 与 `export` 在 Wiki 中**零命中**；
  4. 本地 `AstroCS.wiki/` 为**空目录**且被 `.gitignore:122` 忽略；成因非本轮清运——`25259880`（2026-09-06）移除失效 gitlink 时说明「磁盘目录保留（owner 本地 wiki clone，转为未跟踪）」，即它从来不是生成物，本地那份后被清掉，**无内容丢失**（内容都在 wiki 仓库）；
  5. ENGINEERING_SPEC §7 的根目录清单**未列 Wiki**，与 `.gitignore` 中保留的 `/AstroCS.wiki/` 规则自相矛盾。
- **判定**：①Wiki 事实源漂移（内容陈旧且自称权威）；②工程规范清单与 .gitignore 不一致；③本地 clone 消失（不阻塞，可恢复）。
- **处置**：立 **WIKI-001**（改为**由权威文档生成 Wiki** + `CHK-WIKI-SYNC` 一致性门 + 消除权威自称；push 属发布类动作须负责人批准）；立 **ROOT-005**（空目录与未登记产物目录处置，含 `AstroCS.wiki/`、`evidence/` 登记建议）；§7 清单缺项归下一轮工程包（工程规范对齐）。
- **治理任务**：WIKI-001、ROOT-005、下一轮工程包（§7 对齐）
**GAP-027　违规：清运删除 build/ 后 API-DOCS 门退化为恒绿（静默跳过）**

- **发现者**：工程包调度线 W1 中期（独立复现，非执行 Agent 自述）；前台已复核其口径与证据落点。
- **权威依据**：ENGINEERING_SPEC §8（每项检查必须能红能绿）；docs/ci/01_CHECKS.md §1（检查项语义与自检要求）；ASTROCS_DESIGN §12（不得用空输出/文档声明冒充验证）。
- **证据**：`tools/check_api_docs.py` 的 `check_command_tree`（约 132-133 行）**仅在 "入参 build/cli/astrocs" 存在时**才比对命令树，否则**静默跳过**。调度线的 mutation 复现：在临时 docs 目录里给 `docs/api/CLI_PROTOCOL_V1.md` 的命令行加上 `--min-obs 9`，跑 `python3 tools/check_api_docs.py --repo . --docs-dir <tmp>/api` → **rc=0，mutation 未被发现**；而 build/ 存在时同一 mutation 必红。
- **直接后果**：`tests/quality/test_doc_machine_check.test_05_command_tree_mutation_fails` 由 PASS 转 FAIL（外部原因，不计入 DATA-001/MOD-001）；检查项 API-DOCS **失去「能红能绿」**。
- **判定**：这是**「机器门结构性不红」的又一实例**（SUMMARY.md 簇 1 的形态：校验对象≠交付对象／判据来自被检物自身／缺失即跳过）。**注意：它不是本包新引入的缺陷——build/ 长期存在时它被掩盖了；恰恰是物理清运把它暴露出来**，属清运的正向副作用。
- **处置（已裁决）**：①修复归 **CI-001**，口径「缺构建产物时必须 fail-closed（判 FAIL），不得静默跳过」；②追加**同源全量审计**：逐个检查器审计其对「构建产物/外部文件」的存在性依赖，凡「缺失即跳过」一律改 fail-closed，产出「检查器 → 依赖物 → 缺失时行为（前/后）」清单，负例自测必须证明能红。
- **治理任务**：CI-001（含依赖物恒绿全量扫描）
## 6. 科学课题研究层（2026-09-16 设立）

负责人指示：**权威文档里点名存在但缺数值/缺定义的科学量，排 Agent 作课题研究，研究清楚后落到科学文档**。据此设立独立研究包 `工程控制/SCI-RES-01/`，与重构包并行、边界互斥。

| 课题 | 缺什么 | 现状处置 |
|---|---|---|
| R-001 稀疏帧内 SNR 层控制点密度 | 定义与取值（点/度²） | defaults 以 `pending_authority` 占位 |
| R-002 标量门判据与阈值 | 判定式与阈值 | 同上 |
| R-003 暗场-亮场曝光容差 | 定义式与适用域（值已裁决 5 s） | 5 s 直接采用；定义待落文档 |
| R-004 已裁决值落文档 | 逐字修订 | 依赖 R-003 与负责人批准 |
| R-005 drizzle pixfrac 默认值权威出处 | 数值依据 | defaults 收录字段、数值标 `pending_authority` |

纪律：研究期间 `docs/science/**` 与 `docs/algorithms/**` **只读**；结论以 `CONCLUSION_READY` 交付、经负责人批准后才落文档；禁止编造数值。
## 5. BASE-001 证据补全（2026-09-16 实测；**只补证据，不改判定口径、不删条目**）

原始输出：`reports/PROJECT-GOVERNANCE-01/baseline/01..22_*` + `SHA256SUMS`；执行日志：`run/PROJECT-GOVERNANCE-01/BASE-001/logs/`；复跑脚本：`run/PROJECT-GOVERNANCE-01/BASE-001/evidence-scan{,2,3,4}.sh`。

### 5.1 基线三 SHA 不一致（新增 U-08）

| 引用 | SHA |
|---|---|
| `HEAD` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` |
| `main` | `ecf6ad6fe55102c08a156564cc0dd30cea3a7cdc` |
| `origin/main` | `f96dff61e6ae9cc052a925713f9864f5d6bac3bb` |

- 本地 main 领先 origin/main **1 个提交**：`ecf6ad6f docs(governance): 新增根目录清洁任务 ROOT-001/002/003（29 任务）`；origin/main 无领先提交。
- 本文件抬头声明基线 `a861d8f6…`、派发提示词声明 `f96dff61`，**均 ≠ 当前 HEAD**；三者互不相同 → 新增 `U-08`（见 §5.4）。
- 证据：`reports/PROJECT-GOVERNANCE-01/baseline/{01,05}_*` 与 `19_gap_evidence.txt` 末节。

### 5.2 §0 三项红灯独立复跑（结论与编制时一致）

| 检查项 | 命令 | 编制时 | BASE-001 实测 | 证据 |
|---|---|---|---|---|
| AGENTS-GOV | `python3 tools/check_agents_gov.py` | rc=1 | **rc=1** | `17_gap_sec0_redlights.txt` |
| ENG-CONSTRAINTS | `python3 tools/doccheck/check_engineering_constraints.py` | rc=1 | **rc=1** | 同上 |
| DOC-INDEX | `python3 tools/doccheck/check_doc_index.py --strict` | rc=1 | **rc=1** | 同上 |

`§2` 复核表 7 条亦逐条复跑，结果与编制时一致；新增实测：`ls ci/run_checks.py` **rc=2（文件不存在）**，`python3 ci/run_checks.py --help` **rc=2** → GAP-016「入口名不符」证据升级为文件级（`18_gap_sec2_gates.txt`）。

### 5.3 逐条 GAP 证据（含实测数字修正）

| GAP | BASE-001 实测证据（可复跑） | 与编制时差异 |
|---|---|---|
| GAP-001 | 5 项 tracked 且不在 §7 白名单：`ASTROCS_PROJECT_CONSTITUTION.md`(39903B)、`AstroCS_ENGINEERING_CONSTRAINTS.md`(10061B)、`REVIEW.md`(20936B)、`HANDOVER.md`(9216B)、`VERSION`(15B)；另 `CHANGELOG.md`(16478B)、`FATDUCK_ACCESS.md`(2153B)、`VISUAL_CHECK_README.md`(2271B) | 一致 |
| GAP-002 | `README.md:12`、`REVIEW.md:118`、`HANDOVER.md:7`、`memory.md:10` 引旧宪章；`HANDOVER.md:22` 引 `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909`（`exists=NO`）；`memory.md:36/48/81/87/112` 自报旧基线 `da3c4b4a` | 一致（补行号） |
| GAP-003 | `lib/algorithms`/`lib/infrastructure`/`config` 均不存在；`lib/` 顶层 **31** 项；根 `cli/`(22 tracked)、`runtime/`(32)、`providers/`(10)、`modules/`(16) 在位 | 一致 |
| GAP-004 | 非注释 `add_library` **29** 条 = SHARED **3** + STATIC **25** + `INTERFACE` **1**（`astrocs_contracts`，CMakeLists.txt:95）；三 Session 仍 STATIC（:573/:582/:588） | **数字修正**：编制时写「25 条 / STATIC 22」与实际口径不符（是否含 INTERFACE 与注释行）；结论（整阶段 Session 仍 STATIC、SHARED 仅 3）不变 |
| GAP-005 | kRules 唯一命令路径 **27**；`phase1/2/3 run` 注册于 `cli/parser.cpp:57/62/68`；`normalize`/`mosaic`/`export` 在 `cli/{parser,commands,main}.cpp` 排除 `export-mode` 后 **零命中**（全部 6 处命中均为 `--export-mode` 旗标子串，原文见 `21_gap_evidence_scan3.txt §S1`）；`--template` 注册数 **0**；`cmd_stub` 兜底 `commands.cpp:202/2627` | 一致（补命中原文） |
| GAP-006 | `config` 不存在；4 层内无 `defaults.json`/`filters.json`；`contracts/` 无 `*phase_config*`/`*template*` | 一致 |
| GAP-007 | `contracts/` 4 子域：`config data proposals schemas`（含 `schemas/v6`）；`docs/contracts/` 并存 `DATA_SEMANTICS.md`、`DATA_ARTIFACTS.md`、`INDEX.yaml`、`v6/` | 一致 |
| GAP-008 | `module.yaml` **23** 份（`lib/` 21 + `modules/conformance/{echo,noop}` 2）；`docs/modules/registry/*.md` **26** 份；id 词汇：module.yaml `id: MOD-astrocs-phase1-calibration` + `module_id: astrocs.p1.calibration` + 端口 `p1.frames`；registry 文件名 `astrocs.phase1.*`、正文引用 `astrocs.p1.*`/`p2.*`/`p3.*` 端口词 | 一致（补双向词汇实样，见 `22_gap_evidence_scan4.txt §T1/§T2`） |
| GAP-009 | `p1_session` 四段 `io_read→calibrate→cosmetic→io_write`（阶段字符串计数 8/9/16/10）；`lib/astro_image_io/` 内 `FRAMESNR` **0 命中**；`lib/snr_estimator` tracked **24** 文件；根 CMakeLists 注释 :210/:239/:492-498 与 `git ls-files` 事实冲突；`lib/core/src/module_adapters.cpp:2620-2621,2795-2796,2826` 自述「不再输出整帧 SNR 标量」 | **证据细化**：`lib/astro_image_io` 内 `sparse` 命中 **72** 处，全部属 UPM sparse 模型（`aio_upm.cpp`）与 HiPS occupancy `SPARSE_LIST` 模式（`hiss_writer.cpp`/`hiss_reader.cpp`），**不是** §3.4 的帧级稀疏层 → 「帧级 SNR 未入文件头、帧级稀疏层不存在」判定加强而非削弱 |
| GAP-010 | 三权重 token 在 `lib/phase2_int/v6/src/phase2_integrate.cpp:193-195,208-209`；`ivar=1/variance` 在 :516-518；`p2_session` 有 `s->stage(` 调用 **18** 次、stage 名 **4** 个（coverage/persist/sample/upm_build） | **数字修正**：编制时写「仅 1 处 stage coverage」；仍为单会话组织，判定不变 |
| GAP-011 | `p3_projection_registry_table` `*count = 4`（`p3_projection.cpp:280`）、边界 `if (i < 0 \|\| i >= 4)`（:294/:300）；`p3_proj_v6.h:52-55` `ProjectionId` 仅 kTAN/kSIN/kCAR/kAIT；`STG/MOL/CEA/ZEA` 在 `lib/phase3_proj` 内 **5 处命中全在注释/文档**（module.yaml:31、README.md:28、memory.md:21/101/155），零实现 | 一致；**附带宽差**：module.yaml/memory.md 自述「TAN/SIN/**ZEA**/CAR/AIT」与实测四投影（TAN/SIN/CAR/AIT）不一致，且 ZEA 无实现 |
| GAP-012 | `lib/phase3_session`/`lib/phase3_proj`/`lib/phase3_fits` 均**无自有 CMakeLists**；根仅 `add_subdirectory(lib/phase3_rsmp)`（CMakeLists.txt:697，`lib/phase3_rsmp` 有 CMakeLists） | 一致 |
| GAP-013 | 并存：`lib/astro_image_io`（`aio_fits.cpp`/`hiss_writer.cpp` 在位、**无 CMakeLists**）、`lib/io/src/io_adapter.cpp`（tracked；注意路径在 `src/` 下）、`lib/hips`（有 CMakeLists）、`lib/hips_p2`、`lib/healpix_db`、`runtime/io`（`hips_output_store.py`/`fits_verify.py`/`fits_core.c`/`hips_core.c`）、`runtime/artifact_store` | 一致（补精确路径） |
| GAP-014 | `runtime/pipeline`、`runtime/monitoring`（monitor.py/runner.py/windows_pdh_etw.py/linux_procfs.py/trace_feed.py）、`runtime/core`、`cli/runtime_client.{h,cpp}`、`lib/core/src/scheduler.cpp` 全部在位；`lib/orchestrator` **无 CMakeLists** | 一致 |
| GAP-015 | `providers/` 仅 `cpu`；`lib/backend_host` **27** 文件；`ci/resource_monitor.py` 头注释自述为 `tools/monitoring/run_monitored.py` 的桥接 shim（单一事实源在 tools/monitoring）；`ci/checks.json` 的 `ENG-CONSTRAINTS.changed_paths` 含 `AstroCS_ENGINEERING_CONSTRAINTS.md`、`AGENTS-GOV.changed_paths` 含 `memory.md` | 一致（补 shim 原文） |
| GAP-016 | `ci/run_checks.py` **不存在**（rc=2）；`ci/run.py` 在位；`ci/checks.json` `schema_version=1`、`checks` **145** 项、`CHK-*` **0** 项；`VERSION-CONSISTENCY.command = [python3, ci/check_version.py, --expected, 0.11.0-alpha.2]` | 一致 |
| GAP-017 | `VERSION=0.11.0-alpha.2`；`CMakeLists.txt:14 project(astrocs VERSION 0.11.0 …)`、:58 生成 `ASTROCS_VERSION_STRING`、:797 打印；`cli/commands.cpp` 5 处使用（:174/:344/:366/:419/:681）；`module.yaml` `module_version: 0.11.0-alpha.2` | 一致（补行号） |
| GAP-018 | `packaging/astrocs.product.json`：`product_version=0.11.0-alpha.1`、`source_commit=9f6b72b5eec1f585506e57fd4fb55c15dda1bbb3`、`units=10`、status ∈ {IMPLEMENTED, SKELETON}；`packaging/install-tree.contract.json`：`target_version=0.11.0-alpha.1`、root_layout 含「Linux 技术预览」；`add_executable(astrocs` 于 CMakeLists.txt:612 | **证据修正**：`acsd` 在 packaging/cmake/docs/ci 内 **2 处命中，均为文献 URL**（`docs/science/CALIBRATION.md:129`、`docs/references/SCIENTIFIC_REFERENCES.md:11` 的 “HST ACS Data Handbook”），**不是产品名** → 「无 ACSD Cli 命名」判定成立（原写“零命中”属检索口径差异，非实质冲突） |
| GAP-019 | `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909` 与 `工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915` 均 `exists=NO`；`engineering/` 存在且 **0 条目（空）**；仓内引用计数 **337**（排除 `run/`、`build/`） | **数字修正**：编制时写「30+ 处」（方向一致，实测 337） |
| GAP-020 | `reports/v6/release-review/` 6 份 md + `tools/`；`00_导读.md:34-35` 自述 §14.5 六步 `NOT_MET ×4 / AWAITING / NOT_MET`、`NOT_READY（NOT_RELEASED）` | 一致 |
| GAP-021 | `engineering/` 空；tracked 但不在 §7 白名单：`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`cli/`、`runtime/`、`providers/`、`modules/`、`设计大纲/`(344 tracked)、`问题扫描/`(1011 tracked)、`run/`(仅 `run/.gitkeep`)、`工程控制/`(36 tracked，§7 白名单内) | 一致（补 tracked 文件数） |
| GAP-022 | 顶层 **133** 条目 = tracked 顶层 **47** + untracked 顶层 **86**；`astrocs_run_*.json` **56** 个（合计 36109 B，mtime 2026-09-12 18:49 ~ 2026-09-14 05:28）；`p*-files.patch` **5** 个（104929 B）；`build/` 6.0G；`run/` 102G；`/workspace` 503G/293G 已用 | **数字修正**：编制时写 tracked 46 / untracked 87 / `astrocs_run_*.json` 62 个。tracked 顶层 +1 因 `工程控制/` 已随 `ecf6ad6f` 入库；`astrocs_run_*.json` 由 62 → 56（有 6 个已在本包编制后被移走/删除，本任务未删任何根条目） |

> 全部数字口径：`tracked 顶层 = git -c core.quotepath=false ls-tree HEAD --name-only`；`顶层 untracked = ls -A - tracked 顶层`；`astrocs_run_*.json` 用 `ls -1 astrocs_run_*.json | wc -l`。避免用 `git status --porcelain` 的 `??` 行数当顶层计数（它按目录折叠且含子路径）。

### 5.4 UNRESOLVED 增补

| ID | 无法判定的内容 | 缺什么才能判定 | 归属任务 |
|---|---|---|---|
| U-08（**已于 5f891080 由前台 push 消除**） | 本次治理应以哪个 SHA 为基线：`HEAD=main=ecf6ad6f` 与 `origin/main=f96dff61` 不一致（本地领先 1 个提交），且与本文件抬头 `a861d8f6` 亦不同。**处置：前台 2026-09-16 提交并 push `5f8910804691ba9e64a3426d5bd10f2deb848c30`，此后 HEAD=main=origin/main=5f891080，三 SHA 一致，本条关闭。** | 已消除；后续治理以 `5f891080` 为基线 | GOV-001 / 前台 |
| U-07（补证据，判定仍待裁决） | `run/` 应 gitignore 还是 tracked | 实测 `git ls-files run` **仅 `run/.gitkeep`**，`.gitignore` 有 `run/*` + `!run/.gitkeep`；§7 白名单本身列有 `run/（gitignore：临时产物/日志）`。故冲突点仅为「目录存在 vs 被 gitignore」，证据已足；仍需负责人裁决是否保留 `.gitkeep` 占位口径 | GOV-001 |

### 5.5 本轮新发现（追加为 GAP-023，不修改既有条目）

**GAP-023　缺口：部分散落根条目未被 `.gitignore` 覆盖，长期污染 `git status`**

- **权威依据**：ENGINEERING_SPEC §7（禁止散落根目录）、§8（每项检查能红能绿）；ROOT-002 步骤 3。
- **证据（`git check-ignore -v` 实测，见 `22_gap_evidence_scan4.txt §T7`）**：以下顶层条目 **ignored=NO** 且 untracked，会长期显示在 `git status`：`run_context.json`、`p8-files.patch`、`p9-files.patch`、`p10-files.patch`、`p11-files.patch`、`p15a-files.patch`、`astrocs_p1sess_neg`、`astrocs_p1sess_perf`、`astrocs_p1sess_props`、`astrocs_p1sess_test`、`worktrees/`、`CS/`、`Database/`、`engineering/`（空目录，git 不跟踪空目录故无影响）、`run/`（目录本身）。
- **已覆盖对照**：`build/`(`.gitignore:22`)、`out/`(121)、`logs/`(18)、`Testing/`(120)、`.pytest_cache/`(91)、`graph/`(130)、`AstroCS.wiki/`(122)、`astrocs_run_*.json`(119)、`alloc_report.json`(128)、`alloc_samples.csv`(129)、`resource_samples.csv`(123)、`resource_summary.json`(124)、`worker_balance.csv`(125)、`BASS DR3/`(7)、`GaiaDR3/`(3)、`GaiaDR3SP/`(4) —— 均已 ignored。
- **治理任务**：ROOT-002（补 `.gitignore` + 机器门）。

### 5.7 ROOT 线（BASE-001 / ROOT-001 / ROOT-002 / ROOT-003）收口新发现

> 编号顺延：GAP-023 在 §5.5，GAP-024/025/026 在 §5.6，GAP-027 由调度线登记（build 清运副作用），本节从 **GAP-028** 起。§5.6 的 GAP-024/025 本次未改动、未复核。

**GAP-028　违规（既有红灯）：注册表校验与 `tests/quality` 在执行前就是红的**

- **权威依据**：ENGINEERING_SPEC §8（每项检查能红能绿；`ci/checks.json` 是唯一注册表）。
- **证据（ROOT-002 实测）**：
  1. `python3 ci/validate_registry.py --registry ci/checks.json --strict` → **rc=1 / 1 条错误**；错误为 R10「linux-main 末位必须是 KNOWN-FAILURES-BASELINE-CHECK」，实测末位是 `CTEST-COMPARE-PRODUCTS-QUALITY`。以 HEAD 版注册表复跑得**同样 1 条错误**（`run/PROJECT-GOVERNANCE-01/ROOT-002/logs/g2-registry-{before,after}.json`），证明与 ROOT-002 新增项无关。
  2. `python3 -m unittest discover -s tests/quality -t tests/quality` → **rc=1（4 failed / 6 skipped / 99 tests）**：`test_docchk002_mutation.test_01_real_repo_passes`、`test_doc_machine_check.test_05_command_tree_mutation_fails`、`test_doc_line_anchors.test_t01_real_repo_all_anchors_green`、`test_known_failures_baseline_ci.test_t13b_check_gate_is_last_linux_main_entry`。移除本任务新增的 `tests/quality/test_root_cleanliness.py` 后复跑为**同样 4 failed（94 tests）**，差分证明这 4 项为**预存红灯**。
- **影响**：ROOT-002 验收门「`tests/quality` discover rc=0」在当前基线上无法达成，只能达成「本任务新增 5 例全过 + 不新增任何失败」。
- **治理任务**：CI-001（注册表末位口径）、DOC-001（三个 doccheck 红灯）。

**GAP-029　结构：`run/` 的体量与「被引用证据」高度重合，可无损清运空间远小于体积本身**

- **权威依据**：CONTROL_PACK_SPEC §3（现状须可复核）；ENGINEERING_SPEC §7（`run/` 为临时产物家）。
- **证据（ROOT-003 精确引用扫描 `run/PROJECT-GOVERNANCE-01/ROOT-003/logs/refs-by-target.txt`）**：按 `run/<dir>/` 字面引用扫描 `reports/ evidence/ docs/ ci/ tests/ contracts/ tools/`（排除 `run/` 自身与本次新产物），**27 个目录被引用 / 116 个未引用**；被引用集合占 **100,283,248,730 B（93.40 GiB）**。其中 `run/perf-fix`（61,762,795,655 B = 57.52 GiB）被 `reports/review-package-20260915/08_算法推导_drizzle.md` 当作数值来源引用，`run/release-rescue`（29,597,850,726 B = 27.57 GiB）被 `reports/review-package-20260915/01_工作总览_V3至今.md`、`05_验收与证据索引.md` 当作裁决记录事实源引用。
- **结论**：`run/` 102G 中本次可无损清运的只有 **5,430,815,754 B（5.06 GiB）**（70 个未引用且可确证为一次性临时数据的目录，已执行）。要回收更多，必须先把被引用证据**归档进 `reports/`/`artifacts/` 并改写引用**，属 ROOT-003 复评 / QA-001。
- **治理任务**：ROOT-003 复评、QA-001。

**GAP-030　流程：控制包执行期内前台仍向 main 提交，冻结基线随时失效**

- **权威依据**：CONTROL_PACK_SPEC §6.2（前台统一提交）、§7.1（独立验证三层）；ENGINEERING_SPEC §6。
- **证据**：BASE-001 冻结时 `HEAD=ecf6ad6f`（tracked 5,366）；执行期间前台提交 `5f891080`（tracked 5,367，且改写 `tasks/ROOT-001.md`/`tasks/ROOT-003.md`、新增 ROOT-004），使 BASE-001 的 `git status`/`ls-tree` 快照过期，并直接造成 U-08。ROOT-001 的「tracked 行数不变」门因此改为「同一次脚本执行内 before/after 相等」的相对口径（5367/5367）。
- **影响**：任何「基线冻结」类验收门在并发提交下都会静默失效；`OPERATOR.md §2` 未禁止执行期提交。
- **治理任务**：GOV-001 / OPERATOR（建议明确：BASE-001 冻结期内禁止 commit，或每次 commit 后强制重跑 BASE-001 差量登记）。

### 5.8 ROOT 线交付摘要（供前台验收引用）

| 任务 | 状态 | 关键产物 | 关键数字 |
|---|---|---|---|
| BASE-001 | IN_PROGRESS（自证完备） | `reports/PROJECT-GOVERNANCE-01/baseline/00_MANIFEST.md` + `01..23_*` + `SHA256SUMS` | §0 三红灯 rc=1 复现一致；新增 U-08（已消除）与 GAP-023 |
| ROOT-001 | IN_PROGRESS（自证完备） | `工程控制/PROJECT-GOVERNANCE-01/ROOT_LEDGER.md`（344 行）+ `reports/PROJECT-GOVERNANCE-01/root/{ROOT_LEDGER.tsv,ledger-stats.json,deleted-manifest/**}` | 顶层 133 → 60；删除 74 项 / 6,451,428,033 B |
| ROOT-002 | IN_PROGRESS（自证完备；1 门受预存红灯阻塞，见 GAP-028） | `ci/root_manifest.json`、`tools/quality/check_root_cleanliness.py`、`tests/quality/test_root_cleanliness.py`、`ci/checks.json`（+CHK-ROOT-CLEAN，146 项）、`.gitignore` | 检查器正例 rc=0；3 负例 rc=1；真实根探针 2 次 rc=1 后复绿；注册表校验无新增错误 |
| ROOT-003 | IN_PROGRESS（自证完备） | `工程控制/PROJECT-GOVERNANCE-01/RETENTION.md`（221 行）+ `run/PROJECT-GOVERNANCE-01/ROOT-003/logs/**` | `run/` 102G → 97G；删除 70 目录 / 5,430,815,754 B；27 个被引用目录全部保留 |

---

**GAP-031　违规（SECURITY）：凭据类文件入仓并被审计包白名单收录**

- **发现者**：ROOT-004 前任执行 Agent（结构级判定，未读内容）；前台复核 + 负责人授权后读取内容完成最终判定。
- **权威依据**：AGENTS.md §5（不读取/打印密钥与凭据；Fatduck 密钥只允许 `-i` 路径引用）；ENGINEERING_SPEC §6/§7；docs/ci 的检查项注册要求。
- **证据（结构与登记层面）**：
  1. `FATDUCK_ACCESS.md` **处于 tracked**，首次且唯一入库 `2f20a99d`（2026-08-29），此后未改；
  2. **未被 .gitignore 覆盖**（`git check-ignore -v` 无匹配）；
  3. 被 `tools/pack_audit_package.py` 的 `ROOT_FILES` 白名单收录（与 `VERSION`、`memory.md`、`HANDOVER.md` 等并列）⇒ **会被打进审计包 zip 外发**；
  4. **负责人授权后读取内容的最终判定**：该文件是**接入说明**（Windows 验证节点 Fatduck 的跳板与身份、常用 ssh/scp 命令、在线时间窗、离线处理策略、使用范围），**不含明文密钥**——私钥实体 `vm-bj:/root/.ssh/id_ed25519` **不在仓库内**；全仓无 `BEGIN OPENSSH PRIVATE KEY` 形态、无 `id_ed25519`/`id_rsa`/`.pem`/`.key` 文件入库；
  5. 残留暴露面：主机 Tailscale IP（`100.104.10.71`）、用户名、`sshd/0.0.0.0:22` 配置、公钥落点路径——属**基础设施侦察信息**，非可直接利用的凭据；
  6. 约 30 个 tracked 文件提及该路径（多为路径引用）。
- **判定**：①无凭据泄露，**不需要轮换**；②但「凭据访问说明」入仓 + 被审计包白名单收录仍属违规（扩大暴露面、且与 §5 口径冲突）；③`运营必需` 性成立（Windows 验证节点的接入文档被 CI/review 文档引用），不宜删除。
- **处置（已裁决）**：立 **ROOT-006**（SECURITY）：**保留文件但缩小暴露面**——从 `tools/pack_audit_package.py` 的 `ROOT_FILES` 移除并加排除保证；`.gitignore` 加精确条目仅作为「误加回」护栏（已 tracked，忽略规则对已跟踪文件无效，故不替代白名单修复）；新增 `CHK-SECRET-HYGIENE`（白名单全量敏感形态扫描 + fail-closed + 能红能绿）；不再把「轮换」列为交付项。
- **治理任务**：ROOT-006（前台提交）；不轮换凭据（无凭据）。


---

**GAP-032　结构：追溯门的权威输入存放于构建产物目录，随清运被断供（已裁决退役）**

- **发现者**：TEST-GREEN-001 执行线（artifacts 删除的爆炸半径排查）；前台复核并裁决。
- **证据**：
  1. `tools/check_traceability.py:16` 的 `DEFAULT_TABLES` **唯一默认输入**是 `artifacts/prerelease_v5/tables/TRACEABILITY.csv`（13 列六层 claim 表，67 行 / 66 claim）——即**门的权威输入存放在构建产物目录** `artifacts/`；
  2. 该目录按负责人裁决（「没必要归档、不保留」）于 `b1290525` 整体删除 ⇒ CI 项 `TRACEABILITY-CODE`（三 profile、`waivable=false`）变 rc=1；`tests/quality/test_docchk002_mutation.py` 5 项红；`tests/api/test_cli_protocol.py` import 期 `FileNotFoundError`（Ran 0 / FAILED）；
  3. 表的语义锚在 `docs/VERSIONING.md` 的版本串匹配上，而新设计 §12 明令版本信息下线；
  4. 新 CI 规范 `docs/ci/01_CHECKS.md` 与 `ASTROCS_DESIGN.md`/`ENGINEERING_SPEC.md` **对「追溯」零命中** ⇒ 新世代未要求该门；
  5. 内容未丢：`git show b1290525^:artifacts/prerelease_v5/tables/TRACEABILITY.csv`。
- **判定**：①门的输入位置本身即设计缺陷（产物目录承载权威数据）；②该门口径属**已废止世代**（版本串匹配）；③新规范不要求 ⇒ **裁决退役**，并保留文件加退役抬头而非删除（可复跑性）。
- **处置**：TEST-GREEN-001 执行退役（仅退役确认坏掉的那一个，其余 5 个追溯类注册项逐个复核后保留）；旧世代打包/审计工具（`tools/{assemble_audit,make_capsule,make_rev2_capsule}.py` 引用已删路径）另立 **RETIRE-001**；`tests/api/test_cli_protocol.py` 夹具重接线到 tracked 的 `docs/api/CLI_PROTOCOL_V1.md`；该文档本身随命令树更新归 CLI-001。
- **备注**：若后续确需追溯能力，必须**在新文档集下重新立项**（给出规范条款、tracked 数据位置、能红能绿的负例），不得复活旧表。


---

**GAP-033　规范：`ENGINEERING_SPEC §7` 根白名单漏列 `config/`（设计 §3.3 要求该目录）**

- **发现**：ROOT-005/CHK-ROOT-CLEAN 在 CFG-001 新建 `config/`（`defaults.json`）后判红 `unregistered_root_entry`。
- **冲突**：`ASTROCS_DESIGN.md §3.3` 要求 `config/` 存在并承载默认配置；`ENGINEERING_SPEC.md §7` 的根目录白名单**未列** `config/` ⇒ 规范与设计不一致；
- **另**：`ci/root_manifest.json` 的 `registered_local_retention` 仍列 7 条已不存在的条目（`CHANGELOG.md`/`REVIEW.md`/`ASTROCS_PROJECT_CONSTITUTION.md`/`AstroCS_ENGINEERING_CONSTRAINTS.md`/`evidence/`/`CS/`/`worktrees/`）—— ROOT-005 报告 §4 有改法建议；
- **裁决**：放行把 `config/` 补进 `ENGINEERING_SPEC.md §7` 白名单 + `ci/root_manifest.json`（同一提交，依据设计 §3.3）；`registered_local_retention` 的 7 条悬空登记由 RETIRE-001 清理；
- **同类**：`run/` 在 §7 同时出现于「固定条目」与「gitignore」两处，口径需在下一轮文档收敛中一并订正（登记待办）。


---

**GAP-034　产品：export 的预检与会话层对 `source` 形态要求互斥（模板不可直接运行）**

- **发现**：TEST-CLI-SYNC 执行线实测；前台核实源码后确认。
- **要点**：`subcommand.h` 把 `source` 只当 array/string 计数（对象恒 0 → error 阻断），`p3_session.cpp` 要求 `source.hips_dir`；`export --template` 产出与会话合同不一致 ⇒ 无合法配置可运行，仅 `-force` 可绕过。
- **违反**：`ASTROCS_DESIGN §3.5`（force 白名单语义）、`§6.1`（模板必须可直接运行）、`§6.2`（唯一命令树）。
- **归属**：**CLI-002**（模板/预检/确认/force 语义；卡内已写入验收门）。
- **影响**：TEST-CLI-SYNC 的 export 链路用例保持**显式红**（不 skip、不掩盖），修复后转绿。


---

**GAP-035　工具：打包器「<10MB 目标」不参与退出码（静默超限）**

- **发现**：RETIRE-001 实测 + 前台复跑确认（`legacy_main()` 输出 `zip_MIB=10.72` 与 `<10MB target: FAIL`，但 rc=0）。
- **影响**：审计包体积上限是外发评审件的可用性约束，现为「打印告警但不判失败」⇒ 与 `ENGINEERING_SPEC §8` 的「不允许静默坏掉」不符。
- **处置**：现任打包入口已退役；**后继打包能力（若重立）必须把体积上限变成硬门**（超限即 rc≠0 并给出条目级最大贡献者）。
- **关联**：`reports/PROJECT-GOVERNANCE-01/security/EXPOSURE_NOTE.md`（审计包白名单与排除保证）；PKG-001（审计包白名单评审）。

