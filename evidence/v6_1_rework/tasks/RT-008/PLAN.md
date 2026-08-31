# RT-008 计划: 将 CLI 接入唯一 Runtime

## 目标
拆分 1604 行 cli/main.cpp；CLI 不再 include/调用 session、CFITSIO、AIO 内部、Drizzle 或科学 header；
`run --phases` 解析 preset→IR→Runtime，一次运行的 P1/P2/P3 通过 ArtifactStore 连续；
单 phase 命令走同一 IR 子图（不是第二条路径）；cmd_drizzle 只能选择测试 preset，不得手工重读 FITS。
验收以 CHK-001 调用图 + 实际 observed trace 为准（不以文件行数单独判定）。

## 依赖
- RT-006: 唯一 Scheduler+Runtime DAG 调度（节点级失败传播/内存回压）
- RT-007: 类型化 ArtifactStore（role 绑定 P1→P2→P3）
- CHK-001: 生产可达图检查器（本任务完成后必须 REACH_PASS）

## 步骤
1. 拆分 main.cpp → cli/parser.cpp、cli/commands.cpp、cli/main.cpp（入口壳）、cli/runtime_client.h/.cpp
2. Runtime 扩展: IModule::last_manifest（节点执行后收集 session inspect 摘要）、Runtime::node_manifests
3. SessionModule::execute: plan 保存 config → validate→run→inspect 捕获 manifest（成功/失败都捕获）；
   validate 失败保留 last_error 细节（CLI 04 合同: PARAM/UNSUPPORTED → 2）
4. runtime_client: build_pipeline_ir（preset→IR v1，兼容 run/phase 两种 config 格式）、
   run_pipeline（Runtime 唯一执行入口，退出码按 04 映射: DATA→2/input→3/IO→7/取消→9/其余→70）
5. commands.cpp: cmd_run_pipeline/cmd_phase1/2/3_run 改走 runtime_client；cmd_drizzle 限制为测试 preset
6. Scheduler: 保留节点原始错误域（不再统一压成 BACKEND）
7. 删除 CLI 对 session/CFITSIO/AIO/Drizzle 的直接 include 与调用（fits helpers/spawn_frame_from_fits 移除）
8. 修复: cfitsio 全局表（FptrTable/handleTable/fits_already_open）并行访问非线程安全 →
   aio_fits/aio_hips_reader/p3_output 共用进程级互斥串行化（aio_cfitsio_mutex.h）；
   register_phase_modules 单线程阶段预初始化 cfitsio。TSan 0 race 验证

## 验收
- CHK-001 REACH_PASS: CLI 全部 *.cpp 无 banned include/直接调用；Runtime owner 符号可达
- 根 CMake + build/cli 构建成功
- tests.cli.test_cli_protocol + phase1/2/3_inprocess + phase123_pipeline 全 PASS
- core 单元 15/15 PASS；TSan rt002/003/005/006/008 PASS
- 真实 fixture 冒烟: run --phases 1/2/3 独立 + 事件链/产物/退出码符合 04 合同
