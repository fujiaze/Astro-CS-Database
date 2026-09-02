# 最新基线现状审计

## 1. 审计对象与可信边界

输入包：`AUDIT_PACKAGE_587fe0e341a7.zip`  
包内声明版本：`0.9.0-alpha.1`  
包内声明源码基线：`587fe0e341a780da726917f40ed77f610de0c73f`  
文件数：1699；解包体积约 15.5 MiB；`SHA256SUMS` 全部通过。

该包包含当前源码、文档、测试和审核证据，可用于静态审计与重构起点识别。但包内证据不是单一快照：除 `587fe0e` 外，还出现 `59649ed`、`b8ce366`、`b842899`、旧 `77fc48e` 等提交引用。因此：

- 源码起点只认 `587fe0e`；
- 历史 `SUMMARY.json`、HANDOVER 和 PASS 不能作为现场验收；
- 重构开始后必须在真实仓库重新生成证据；
- 审核包以后必须保证 `source_commit == document_commit == evidence_commit == artifact_commit`，否则自动失败。

## 2. 总体判定

当前状态：`ARCHITECTURE_REFACTOR_REQUIRED`，不具备 Alpha 发布条件。

程序已有可运行能力，但“单一 CLI + 完整 Pipeline Runtime + 模块注册”的架构目前只是局部外壳：Phase1、Phase2 session 并未执行文档声称的完整链；完整能力仍散落在旧 Orchestrator、旧 Stage2、AIO PipelineEngine 和 CLI 临时命令中。文档、源码和审核结论存在实质性漂移。

## 3. P0：必须在架构迁移最前面解决

| ID | 位置/符号 | 现场事实 | 风险 | 强制动作 |
|---|---|---|---|---|
| P0-001 | `code/lib/phase2_session/p2_session.cpp` | sampler 明确设置 `cpu_workers = 1`，session 只到 coverage/sample/UPM/persist | Phase2 重计算单线程；与资源硬约束直接冲突 | 在统一 Runtime 下取得线程租约；并行 sampler；合成等价与资源曲线通过后删除串行生产路由 |
| P0-002 | `code/lib/phase3_session/p3_session.cpp` | 像素二维循环为显式单线程；注释也写明 serial | Phase3 球面到平面重采样低利用率 | 先把现有代码标记 prototype/non-production；完成 SCI/ALG/DATA 后按 tile 并行实现 |
| P0-003 | `phase1_session::ImageDeleter` | 只 `std::free(image)`；正式 `aio_free_image_data()` 还需释放 `data/data_f64/keywords` | 像素与关键字内存泄漏 | 所有 AIO image 所有权统一 RAII；ASan/LSan 循环测试必须零泄漏 |
| P0-004 | Phase1 API 文档 vs `p1_session.cpp` | 文档声称 Calibration→Star→PSF→PlateSolve→Photometry→SNR→Drizzle→HiPS；代码只做 read→calibrate→cosmetic→write | 用户看到的“完整 Phase1”并不存在于 session | 以 Pipeline IR 建真实完整链，runtime trace 必须逐节点证明 |
| P0-005 | Phase2 API 文档 vs `p2_session.cpp` | 文档声称完整 Phase2；代码缺 block/apply、rejection、integration、HiPS writer/verify | 文档与生产行为不一致 | 模块化迁移旧 Stage2 能力，禁止继续用文档掩盖缺失 |
| P0-006 | `code/cli/main.cpp` 的 `run --phases 1,2,3` | 三个 session 顺序调用，但未把上一阶段 Artifact 绑定为下一阶段输入 | 只是三个命令连跑，不是真数据管道 | Runtime 用类型化端口和 Artifact ID 传递；端到端 trace 验证对象身份 |
| P0-007 | `phase3_session` 元数据 | `BUNIT=Jy/beam`、run id、版本、输出名等硬编码 | 产生错误科学语义和不可追溯产物 | BUNIT 必须由 DATA 合同传播/转换；禁止猜测单位；provenance 取真实运行上下文 |

## 4. P1：架构和并发高风险

| ID | 位置/符号 | 现场事实 | 后果 | 重构归属 |
|---|---|---|---|---|
| P1-001 | `code/cli/main.cpp` | 约 1521 行，混合解析、配置、运行、benchmark、monitor、manifest、Drizzle、Phase1/2/3 | CLI 成为第二套编排器，难测试 | CLI 薄化，只调用 Runtime public API |
| P1-002 | `code/lib/orchestrator/cpp/src/orchestrator.cpp` | 约 5405 行旧硬编码 stage 与 DLL loader | 与新 Runtime 竞争 | 先适配、影子验证，再移除生产入口 |
| P1-003 | `aio_pipeline_engine.cpp` | I/O 模块内含 5-stage PipelineEngine，默认线程数硬编码 16，并调用 `omp_set_num_threads` | I/O 越权编排、线程过订阅 | I/O 仅保留格式适配与 Artifact；删除其生产调度职责 |
| P1-004 | `phase2/tools/stage2.cpp` | 约 1762 行旧完整 Phase2，手工编排并注册 ACR kernel | 真能力与新 session 分裂；ACR 污染首发 | 按模块逐步迁移；纯 CPU 后端；旧工具最终下线 |
| P1-005 | CLI `drizzle` 命令 | 手工解析 FITS 头并直接调用 `hp_drizzle_run_hips`；默认 NSIDE/pixfrac/FP32 硬编码 | 绕过 Phase1 与注册表，重复 I/O/WCS 语义 | 变为 Pipeline preset 或模块测试入口，不得直接调用内部符号 |
| P1-006 | `phase2/src/sampler.cpp` | 全局 `g_aio_mu` 包住 HiPS tile 读取，曾以并发 SIGSEGV 为理由 | 所有 worker 在 I/O 处串行等待 | CFITSIO reentrant 构建；每 worker 独立打开；压力/TSan 后消除全局锁 |
| P1-007 | `code/cli/CMakeLists.txt` | `file(GLOB)` 拉取大量内部源；大范围 `-w`/`/w`；生产路径包含 ACR registry | 构建边界不可审查、警告被隐藏 | 显式 target graph；目标级警告；`ASTROCS_ENABLE_ACR=OFF` |
| P1-008 | 多线程所有权 | session、AIO、模块各自设置线程或 workers | 低利用/过订阅无法统一诊断 | 只有 Runtime `ThreadBudget` 可创建/分配重计算 worker |

## 5. P1：文档与治理漂移

1. `docs/architecture/PIPELINE.md` 仍把 `orchestrator.exe`、`astrocs-stage2.exe` 作为生产入口；另一份架构文档又宣称它们已被统一 CLI 替代。
2. 根 README/HANDOVER 残留旧版本、PowerShell 主流程和不同发布代号。
3. `PHASE1_API_V1.md`、`PHASE2_API_V1.md` 描述的链路超出实际 session 实现。
4. `test synthetic` 当前未接到真实测试执行器，参数验证后落入 stub。
5. 审核证据跨多个 commit；旧任务依赖曾被修改以让图通过。任务 DAG 以后必须视为受控输入，禁止靠改依赖消除 blocker。
6. 顶层 L0 负责人文档尚未按冻结规范形成唯一入口。

## 6. 当前有效设计：必须保留并迁移

- `PipelineFrame` 及显式帧身份思路；
- AIO 作为统一图像 I/O 入口的方向；
- 已有模块 SCI/ALG/API/TEST 文档资产中经核实有效的部分；
- Drizzle FP64 累积/FP32 产品、负值保留、加性 UPM 等冻结语义；
- ACR 源码、独立合同和独立测试，但保持 dormant；
- 现有科学模块实现，通过适配器迁移，不直接重写公式；
- 已有 benchmark/resource monitor 的可复用代码，但归并到 Runtime 服务。

## 7. 对 Phase3 的判定

最高工程约束写明 Phase3 尚未开发。当前包虽然出现 `phase3_session`，但它是单线程、单位硬编码且缺少完整 SCI/ALG/TEST，因此只能认定为 `PROTOTYPE_NOT_PRODUCTION`，不能以“文件已存在”改写冻结结论。V6 将 Phase3 作为独立、受科学合同约束的开发迁移，不得把现状直接标 PASS。

## 8. 本次重构边界

本包不要求反复运行历史版本，不以旧输出为科学 Oracle。迁移前只为每个模块建立一次小型 characterization fixture，用于定位重构引入的行为漂移；最终正确性由 SCI 推导、独立合成 Oracle、性质测试和少量真实数据验证判定。

本审计是静态与结构审计，不冒充现场 Linux/Windows 编译结果。所有构建、测试、资源和真实数据结论必须由执行 Agent 在目标 commit 上重新生成。
