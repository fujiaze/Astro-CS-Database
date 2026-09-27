# 检查-S31 — lib/infrastructure scheduler ＋ orchestrator ＋ module_loader（纯静态，只读）

- 切片：S31。域 = `lib/infrastructure/scheduler/src/`（24 impls）＋ `lib/infrastructure/pipeline/orchestrator/cpp/`（10 头 / 7 src / 11 测试源 + tests/CMakeLists.txt）＋ `lib/infrastructure/pipeline/module_loader/`（2 头）＋ `lib/infrastructure/pipeline/typed_dag_contract.h`；互查文档 = `docs/contracts/SCHEDULER_CONTRACT.md`、`docs/contracts/PIPELINE_BLOCK_CONTRACT.md`、`docs/contracts/CONFIG_CONTRACT.md`、`docs/modules/orchestrator.md`、`docs/modules/phase1_session.md`、`docs/architecture/PIPELINE.md`、`docs/architecture/DATA_FLOW.md`（另核 ERROR_MODEL/RESOURCE_MONITORING_CONTRACT 作旁证）。
- 方法：不构建、不测试、不编译、零 git 写；全部 file:line 锚逐条实开文件核对；科学口径对照 `独立审计/实验重做/总编对账/分歧台账.md` D-01…D-11（不重开裁决）；已过 PASS 表（检查-修复验证.md）与往期排查报告（S1/S10/S12/S15 等）去重。
- 计数：**红 3 / 黄 6 / 绿 6**。

---

## 一、红（必须改；只登记，不在本任务内修改）

### R1【红｜①面·A-P3-09 残留核零】禁抄值 211034.6 残留 4 处（本切片域内）

- **文件:行**：`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:180`、`:181`、`:198`；`lib/infrastructure/scheduler/src/module_adapters.cpp:7603`。
- **问题**：HEALPix 每 NSIDE 角分辨率常数的**注释值**仍写「211034.6」（真值 ≈ 211076.3）。例：`orchestrator.cpp:197-198` 实算 `std::sqrt(PI / 3.0) * (180.0 / PI) * 3600.0`（正确），紧随注释 `// ≈ 211034.6 "/nside`；`module_adapters.cpp:7602-7603` 同构：`const double HEALPIX_SCALE_PER_NSIDE_ARCSEC = std::sqrt(M_PI / 3.0) * (180.0 / M_PI) * 3600.0;  // ≈ 211034.6 "/nside`。
- **证据（含反方核验）**：真值 √(π/3)·(180/π)·3600 = 648000/√3 ≈ **211076.31**；同族文件 `lib/algorithms/drizzle/cpp/src/drizzle_engine.cpp:148` 已订正为 `// ≈211076.3`（反方核验：全仓 grep `211034.6`，域内命中恰为上述 4 处注释，域外 `hp_drizzle_api.h:114`、`drizzle/README.md:93` 归 S25 域）。**行为面无损**：4 处均为注释，代码一律按公式求值——但按切片指令【A-P3-09 残留核零＝发现即红】登记。
- **建议改法**：仅改 4 行注释为 `// ≈ 211076.3`（与 drizzle_engine.cpp:148 对齐），不动任何表达式；改后重跑全仓 grep，域内残留必须为 0。
- **去重声明**：S15 报过同一五点清单（含域外两点）；本条为切片指令强制的域内复核登记，不新增域外重复项。

### R2【红｜④面·文档说有、代码没接】phase1_session.md 的 P1Api 委托关系主张为假（＋3 处行锚全漂）

- **文件:行**：`docs/modules/phase1_session.md:15-17` vs `lib/infrastructure/scheduler/src/module_adapters.cpp:1193-1199 / :15462-15478 / :15447 / :15455`。
- **问题**：模块合同页声明「五函数经 P1Api（module_adapters.cpp:**755-762**）被 8 个 Phase1 descriptor 工厂委托（**:728-735 / :755-770**）」「B 线 registry 通道经 SessionModule ThreadLease 租借（module_adapters.cpp:**156-162**）」——实测三组锚与机制**全部不成立**：
  1. `P1Api` 实定义于 **:1193-1199**；grep `P1Api` 全文件仅命中定义行——`make_session_module<P1Api>` **零调用点**（仅 `make_session_module<P2Api>` :15447、`<P3Api>` :15455 各 1 处）；
  2. 8 个 Phase1 节点工厂实际全部注册为 `make_p1_node_module(d, spec)`（**:15462-15478**，p1_nodes[] 恰 8 条，走 P1NodeSpec 单节点 operation 委托）；
  3. **:755-770 实为 P2/P3 descriptor 工厂**（phase3_descriptor / p3_properties_descriptor），**:156-162 实为 #include 区**；ThreadLease 租借实点为 **:598-599**（`ThreadLease lease = ctx.acquire_lease(...)`）。
- **反方核验**：①「8 个」数量对（p1_nodes[] 恰 8）；②代码注释 **:15458-15461** 自证「P1-001 attempt 2: ARCH-P0-001 整改——子节点不再委托 phase_session_run」——即文档描述的恰是**已被整改废弃的旧链路**，行号是在旧版文件上记的；③委托链本身健全（P1NodeModule→operation→p1_op_*，与 module_ports.registry.json 冻结绑定一致），错的是文档不是代码；④去重：全仓-01 排查目录 grep `phase1_session` 仅 S10:134 提到目录存在，未报过本问题。
- **建议改法**：改写 phase1_session.md:15-17 的 registry 关系句为现行事实（五函数经 P1NodeModule 的 P1NodeSpec 单节点 operation 委托，锚 :1193-1199/:15462-15478；或如实注明 P1Api 现无调用者、为 P2/P3 保留面），ThreadLease 锚改 :598-599。
- **所属面**：④（幻觉与锚·文档声称的行为在源码不存在）兼②。

### R3【红｜④面】「docs/ASTROCS_DESIGN.md:373 逐字」引文在最高设计中不存在（ORCH-001 归属判定证据链悬空）

- **文件:行**：`docs/modules/orchestrator.md:60-61` 与 `lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt:9-14`。
- **问题**：两处均以「`docs/ASTROCS_DESIGN.md:373`（:374）**逐字**『scheduler/  注册、资源预算、执行、取消、checkpoint』『pipeline/   typed DAG、artifact、内存/数据管线』」作为 ORCH-001「职责家 = lib/infrastructure/scheduler/**」归属判定与 ORCH-HOME-01（位置职责分离登记）的**逐字依据**。实测：
  - `docs/ASTROCS_DESIGN.md:373-374` 实为 §4.5 预检流程 **mermaid 图**行（:373 = `PAGE --> E{"存在 error?"}`，:374 = `E -->|是| BLOCK[...]`）；
  - L1 全文 grep **无**「注册、资源预算、执行、取消、checkpoint」此句；grep「资源预算」L1 仅 :634「一个进程只有一个资源预算源（§9）」，语义不同；
  - L1 现行目录树为 **:656-657**：「pipeline/  PipelineFrame、命名块、块生命周期、typed DAG」「scheduler/  三个阶段调度器（异步编排 / 窗口并行 / 子块流式）」——与所引「逐字」措辞不同。
- **反方核验**：①orchestrator.md:60 说「§7.1 目标家」而 L1 §7.1 现行=命令树（章节漂移与 S1-L1:141 已登记的 L1 漂移族同源——但 :373 引文**整句不存在**属新证据，S1 报告未覆盖 orchestrator 这两文件）；②归属结论本身（编排层归 scheduler 职责面）与 L1:657 大体相容，错在「逐字依据」为假；③CMakeLists:12-14 还把该假引文展开成「五个词逐条对应」的判定链（资源预算=admission_controller/resource_monitor——恰是 Y2 的无实现悬挂头），证据链双重失真。
- **建议改法**：两处同步改为现行锚与现行引文（L1:656-657 目录树、或 §8.1 :571-574 独立调度器原则），删去「逐字」措辞或照抄现行文本；ORCH-HOME-01 登记句附注证据已更新。
- **所属面**：④（锚）兼③（跨文档冲突：模块合同页/构建脚本 vs 最高设计）。

---

## 二、黄（建议改）

### Y1【黄｜③④】共址测试计数三方漂移：文档 6 / CMakeLists 自注 7 / 实际 add_test 9

- **文件:行**：`docs/modules/orchestrator.md:81-82`（「共址测试 6 条 ctest（cpp/tests/CMakeLists.txt）」并列举 5 类）；`cpp/tests/CMakeLists.txt:5`（头注「本文件注册 7 条 ctest」）；实测 `add_test` **9 条**：orchestrator_logger_units / orchestrator_checkpoint_units / orchestrator_cli_integration / orchestrator_legacy_cli_smoke / orchestrator_legacy_cli_validate / orchestrator_saturation_wiring_gate / orchestrator_curve_resolve_gate / orchestrator_curve_resolve_gate_fault_inject / p1phot_passband_identity_gate。
- **反方核验**：`eng/ci/checks.json:1130-1135` 登记 6 个 orchestrator_* ——若 orchestrator.md:81 指「CI 登记数」则与 checks.json 一致，但其括号锚指向 tests/CMakeLists.txt，且 CMakeLists 自注 7、实测 9（漏数 curve_fault 与 p1phot 两条后加门），三方仍互斥；orchestrator.h:7-9 的「checks.json 登记 6 个 ctest」另算一致面。
- **建议改法**：orchestrator.md:81 拆写「CI 登记 6（checks.json:1130）/ ctest 全集 9（tests/CMakeLists.txt）」；CMakeLists:5 注释同步为 9。
- **所属面**：③（跨文档数字两说）兼④（锚名不符）。

### Y2【黄｜④面】spill_manager / resource_monitor / admission_controller 三悬挂头：无实现、无调用、规范锚整目录不存在

- **文件:行**：`cpp/include/spill_manager.h`（211 行）、`cpp/include/resource_monitor.h`（214 行，:138「实现于 resource_monitor.cpp」）、`cpp/include/admission_controller.h`。
- **证据**：①`resource_monitor.cpp` / `spill_manager.cpp` / `admission_controller.cpp` **不存在**（src/ 实为 checkpoint/cli_command/dll_loader/json_config/logger/main/orchestrator 7 个 TU）；②全 lib/eng grep 三头 include——**仅头文件互引，零 .cpp 包含、零调用者**，CMake target 也不含其「实现」；③三头引用的规范源 `engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md`、`engineering_authoritative/contracts/resource_profile.schema.json`、`engineering_authoritative/evidence/H-001…H-003/*.py`——`engineering_authoritative/` **全仓不存在**（find 0 命中）；④`docs/modules/orchestrator.md:44` 把「资源监控（resource_monitor）；spill/checkpoint（V13+）」写成**现行**「性能特征」，:63 再写「资源预算 = admission_controller.h + resource_monitor.h」。
- **反方核验**：orchestrator.h:1-29 RETIRED 块与 CMakeLists:13 均把该目录标为 legacy/ORCH-001 登记面（「保留则注释」），**运行行为零影响**；问题在模块合同页把无实现组件写成现行能力、且头文件注释给出成套不存在的规范/原型锚。
- **建议改法**：orchestrator.md:44/:63 改为如实表述（「声明保留、无实现，资源预算实际由 executor/plan_estimator 承接」，与 orchestrator.h:17 口径统一）；三头顶部注明 SPEC 锚为历史外部资料或删除失效锚。
- **所属面**：④（doc claims but code not wired）兼②。

### Y3【黄｜②面】orchestrator.h 陈旧注释两处（run_stage2 / stage_timeouts）与实现两说

- **文件:行**：`cpp/include/orchestrator.h:352-357`「run_stage2(…)：success=true 表示 GRADIENT_SPHERE + STACK 全部成功」——同文件 **:107** 注明 GRADIENT_SPHERE 分支已移除，`orchestrator.cpp:5499-5500` 对 stage2 **强制置失败**（两说）；`:241` stage_timeouts 键注释仍列已归档 GRADIENT_SPHERE/STACK 阶段键（json_config 实测 `stage_timeout_sec` required 10 键、模板 11 键，不含该两者）。
- **反方核验**：均纯注释，编译与行为不受影响；legacy 目录整体 RETIRED（ORCH-001），但注释是现行读者理解 run_stage2 语义的唯一说明面。
- **建议改法**：:352-357 改为「stage2 已归档，恒失败（见 :107 / orchestrator.cpp:5499）」；:241 键清单对齐 stage_timeout_sec 现行 10 键。
- **所属面**：②（行文逻辑·同文件前后两说）。

### Y4【黄｜④面】ExportOutcome.exit_code 是写而不读的死字段，且注释只登记 0/10（漏 1/2/130）

- **文件:行**：`lib/include/astrocs/core/export_stream.h:86`（注释「0 = 正常；10 = 磁盘满（已修语义）」）vs `scheduler/src/export_stream.cpp:572`（=1，header 缺失）、`:577`（=2，sink 落盘失败 I/O）、`:582/:587`（=130，未发布/取消）。
- **证据**：全 lib grep `.exit_code` 排除赋值行后**无任何读取者**——module_adapters 消费 `ExportOutcome` 只用 `so.ok / so.error`（:14794-15011 生产接线段）；进程真实退出码走 `lib/include/astrocs/core/contracts.h:42-46` 映射枚举（与唯一源 `exit_codes.h` 11 码**逐值一致**）＋ pipeline_exit_code_from_error（module_adapters:8291 注释「映射为 exit 10」）。
- **反方核验**：①SCHEDULER_CONTRACT §5「disk-full exit 10」经 error 映射**真通路成立**（非死配置）；②取消出口的 130 不会成为进程码（合同 §5 写 SIGTERM=9，130 仅是对象内标记）——**行为无违约**；③死字段本身是「声明了语义但没接线」的登记面；S10/S12 只覆盖 CLI 侧与 orchestrator 数值表，未报此点。
- **建议改法**：二选一——删 `exit_code` 字段，或补读取者并在 :86 注释补齐 1/2/130 的对象内语义与「非进程码」声明。
- **所属面**：④（幻觉与锚·dead 字段）兼②（注释不全）。

### Y5【黄｜②③】PIPELINE.md 节点序同文档两说（:27-29 vs :33 枚举序）

- **文件:行**：`docs/architecture/PIPELINE.md:27-29` 主张「解算在检测与 PSF 建模之前」（= `DATA_FLOW.md:14-16` 流程、注册表声明序 `module_ports.registry.json` idx2=wcs-platesolve → idx3=star-psf、`PIPELINE_BLOCK_CONTRACT` §7.1 C6「pos(psf) > pos(wcs)」四方一致）；同文档 **:33** 却把生产 IR normalize 枚举为 `calibrate/cosmetic_correct/`**`detect_sources`**`/`**`plate_solve`**`/measure_flux/estimate_snr/drizzle_stack/write_hips`——detect_sources 在 plate_solve **之前**，恰是 `module_adapters.cpp:15463-15470` p1_nodes[] 的**注册调用序**，与本文件 :27-29 及注册表序相反。
- **反方核验**：若 :33 读作**节点集合**（点数 8）则非序主张，冲突可降为措辞歧义；但紧邻 :29「节点序 = 注册表端口图 DAG 的拓扑序」并读，两种解读都未被文内消歧，且枚举序与 C6 冻结判据相逆。
- **建议改法**：:33 改为集合写法（「8 节点：{…}」或按拓扑序重排为 …plate_solve/detect_sources…），并注明「p1_nodes[] 注册序 ≠ 执行序，执行序以注册表拓扑为准」。
- **所属面**：②（同文档前后两说）兼③（PIPELINE.md vs 注册表/C6）。

### Y6【黄｜②面】dll_loader.h 注释称「实现仍为 Win32 专属」，与本仓 Linux dlopen 实现两说

- **文件:行**：`cpp/include/dll_loader.h:27-31`（「DllLoader 实现仍为 Win32 专属（cpp/src/dll_loader.cpp），此 typedef 仅允许 Linux sanitizer 编译头文件」）vs `cpp/src/dll_loader.cpp:513-537`（完整 `#else` 分支：`dlopen(path, RTLD_NOW|RTLD_GLOBAL)` :520、错误串 `code=dlopen/dlsym` :537）；旁证 `tests/gate/orchestrator_saturation_wiring_gate.cpp:14`（「真实加载器；Linux dlopen 分支」）、`CMakeLists.txt:47`、`docs/modules/orchestrator.md:87-88`。
- **反方核验**：实现早已跨平台、gate 测试在 Linux 真跑 dlopen——纯注释滞后，行为无损；危害是误导读者以为 Linux 侧只有头可编译。
- **建议改法**：dll_loader.h:27-31 改为「实现双平台：Win32 LoadLibraryExA / POSIX dlopen（见 dll_loader.cpp:468/:513 分支）」。
- **所属面**：②。

---

## 三、绿（低危记录，可不改）

- **G1** `docs/modules/orchestrator.md:48`「已知 bug：cpp/ 下嵌套 logs 目录（非阻断）」——`find cpp -type d` 无嵌套 logs 目录（bug 已消），登记未销项（②）。
- **G2** FNV 种子值 **1469598103934665603** 非标准 FNV-1a 64 位 offset basis（标准 = 14695981039346656037，少一位）：全仓 10 处一致使用（block_flow.cpp:244、normalize_workflow.cpp:220 标注「FNV offset basis」、mosaic_window.cpp:188、export_stream.cpp:544、artifact.cpp:51、io_adapter.cpp:53/:97、io_adapter.h:45、p1hips 测试×2）——内部互算自洽、无外部 FNV 标准复算合同，行为零影响；注释标注不准，建议改注释为「FNV-1a 式自定义 seed（全仓冻结）」或改值并同步 10 处（①，非科学常数、非 D 系裁决域）。
- **G3** `orchestrator.cpp:365` `stage_name_v2` switch 缺 NSIDE/HISS_VERIFY/BROWSER_VERIFY 三个枚举 → 返回 "UNKNOWN"；grep 无调用者（死函数，晚补）（④）。
- **G4** normalize/mosaic/export 三个 scheduler 的 `run()` 均不重置 `cancel_`（实例复用时上一轮取消态残留）；现有生产接线每命令新建实例，无触发面（②）。
- **G5** `scheduler/README.md:21` 与 `module_adapters.cpp:3300/:4503` 均写 `ipv_select.cpp:57`——实际常量在 **:58**（`static constexpr double IPV_ARCSEC_PER_UM_PER_MM = 206.265`），行号差 1（④微漂）。
- **G6** `orchestrator.md:48` 邻近的「C ABI：dll_loader.h:8」锚（:8 实为「- 5 个模块: CALIBRATE/…」注释行）与 :60 的 :373 锚同族微漂（主证已计入 R3，此为同页次要锚）（④）。

### 已确认与往期报告同源、按纪律不重复计数

- **退出码数值表冲突**（`orchestrator.h:143-154` AstroCsExitCode TIMEOUT=9/CANCELLED=10 vs 唯一源 `exit_codes.h` CANCELLED=9/RESOURCE=10；`:40`「与 ERROR_MODEL 全集合一致」假声明；`main.cpp:383-391` 传播链）——**S10-architecture.md:15-30 已报**（含 TRACEABILITY ENG-ERR-001）；本切片复核域内成立，不另立红黄。
- **211034.6 五点清单**——S15 已报（域外 hp_drizzle_api.h / drizzle README 归 S25）；域内 4 点按 A-P3-09 指令登记为 R1。
- **L1 章节/行号漂移族**——S1-L1:141 已登记 7 处；R3 为该族在 orchestrator 域的新证据（引文整句不存在），已注明。
- **RESOURCE_MONITORING_CONTRACT:43-53 自相矛盾**——S10 已报，不重复。

---

## 四、已查无问题面（checked & clean；附抽样方法）

1. **SCHEDULER_CONTRACT §4 探针事件 schema/字段**：`eng/contracts/schemas/scheduler_probe_event.schema.json` 实开——required = ts/stage/kind/value/unit（与合同「必带」逐字），properties 11 字段与合同 §4 字段全集逐项相等；实现端 ProbeSink（normalize_workflow / mosaic_window / export_stream 三处 flush 段精读）逐字段写出含 stage/frame_id/window_id/worker；stage 默认值 "normalize"（normalize_workflow.h:54）与 mosaic/export 显式置值核对无缺。
2. **SCHEDULER_CONTRACT §2.1/§5 冻结归约与门**：mosaic_window.cpp 全读——E1a/E1b/E2/E3 四门按「常驻数据 = 退化」实现（非恒真）、权重 = SNR²、积分序冻结；normalize_workflow run() 以 frame_id 稳定排序（stable）＋ PrefetchCache 单飞；export_stream disk-full=10（:566）与合同 §5 一致。
3. **三命令不串链**：normalize_workflow/mosaic_window/export_stream 之间 grep 互相**零引用**、无 include/调用关系；scheduler/src 无任何入口可一次拉起两阶段；隐式链监测仅 jsonl_event_v1 的 implicit_phase_chain 登记字段（CLI 域）。
4. **json_config（stage1 内嵌 schema）**：python 逐字节比对内嵌 STAGE1_SCHEMA_JSON vs 盘上 `configs/stage1.schema.json` = **identical: True**；stage_timeout_sec required 10 键 / 模板 11 键、panel×3 键集、additionalProperties 逐项程序化校验通过；compute_config_sha256 的 -999.0 null 哨兵语义清楚。
5. **CONFIG_CONTRACT 死键检查**：三类注册表（全局/会话/阶段）中**无 stage1/orchestrator 键**——stage1.json 属 legacy 作业数据（恰是 CONFIG_CONTRACT:88 区分的「数据」类），不存在可登记的 dead key；ParseConfigCached 所读键（run/stages/timeouts）均在模板内。
6. **RT-001 §2.1/§2.5 接线与 checkpoint**：context.h:191/:192/:218/:229（add_metric/record_tick/store_artifact/mark_checkpoint）与 runtime.h:44-59、runtime.cpp:133/:420 实调存在；生产 `scheduler/src/checkpoint.cpp`（RT-007）全读——空 run 拒、mid-run 换 run/scope 拒、commit_node 空 artifact 拒（半成品拒绝）、validate_resume_inputs hash/schema 门（:93-100），与「原子提交后才可恢复」一致。
7. **PIPELINE_BLOCK_CONTRACT 机器判据实体**：`eng/tools/quality/check_block_flow_ports_vs_code.py`、`eng/ci/check_registry_ir_parity.py`、`eng/contracts/block_flow/stage_block_flow.json`、`eng/tools/quality/check_block_flow_spec.py` **全部实存**；block_flow.cpp 全读——四条非法图判据构建期接（graph_issues_）、EXTERNAL_IN→optional=true、EXTERNAL_OUT 豁免销毁与规格 R7 注释对得上、生命周期映射自洽。
8. **注册表节点序 vs 冻结判据**：module_ports.registry.json phase1 idx0-7 = calibration→cosmetic→wcs-platesolve→star-psf→photometry→noise→drizzle→writer——与 C6「pos(psf)>pos(wcs)」、C5 拓扑序、DATA_FLOW/PIPELINE 流程图一致（仅 PIPELINE.md:33 枚举措辞两说，见 Y5）。
9. **科学常数抽样（①面）**：板尺度 206.265 三处逐位一致（`kP1GuidedAsecPerUmPerMm` module_adapters:3302、`kP9AsecPerUmPerMm` :4506、`IPV_ARCSEC_PER_UM_PER_MM` ipv_select.cpp:58），量纲推导注释（206.265 = 648000/π×1e-3，显式规避 206264.806 的 μm/mm 错用）正确；与 D-01…D-11 台账无交集；scheduler/src 除 A-P3-09 残留外**无**其他硬编码科学常数、无 `omp_set_num_threads(<字面量>)`（grep 反证：线程数一律 lease 注入，仅 ICV 保存/恢复与 lease 派生调用）。
10. **module_loader / typed_dag**：module_registry.h:148/:164、secure_loader.h:150/:156 锚符号实开相符（C ABI 面 S10 已核）；typed_dag_contract.h 锚 `typed_dag.schema.json`/`typed_dag.py`/`module_ports.registry.json` 三文件实存。
11. **退出码唯一源（生产侧）**：contracts.h:43-46 `ExitCode` 枚举 11 值与 exit_codes.h **逐值一致**，注释声明「只做映射表不重定义数值表」；export 路径 disk_full → pipeline_exit_code_from_error → exit 10 有实链（module_adapters:8291）。
12. **orchestrator 11 测试源抽查**：tests/ 9 源 + gate/ 2 源逐一点名——gate 均为接线/故障注入型（curve_resolve_gate_fault_inject 具备能红能力），无 SKIP 充数式空测试迹象；tests/CMakeLists.txt 真实 add_test 逐条列出（计数问题单列 Y1，测试内容本身无退化面证据）。
13. **抽样方法声明**：24 个 scheduler impls——16 个精读关键段（normalize/mosaic/export/checkpoint/block_flow/artifact_store/runtime/scheduler/plan_estimator 等），8 个中型文件按头注释＋退出码/硬编码/TODO 三项 grep 全扫（TODO 残留 0 命中）；orchestrator 5522 行 orchestrator.cpp 按锚段（180-198/365/409-441/492-498/5301-5305/5499-5500）精读，7 个 TU 全部结构性核验；6 份互查文档全文读；关键 file:line 锚（phase1_session×4、orchestrator.md×4、CMakeLists×2、README×1、RESOURCE×2、L1×3）**逐条实开**——失效锚均给出实测内容。


