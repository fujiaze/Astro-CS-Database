# CLI-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS CLI-004 行(去除 Phase1 shell-out,进程内调用 API;传 cancel/thread budget/monitor;验收=integration test 证明无子进程、事件完整、错误映射正确); PHASE1_API_V1(API-P1-001 冻结: p1_session 四段式); 04 协议(退出码/JSONL); 05 §7 线程预算。

## 动作
1. lib/phase1_session/p1_session.{h,cpp}: API-P1-001 冻结合同实现——p1_session_create/validate/run/inspect/destroy(opaque handle, host services 注入 allocator/logger/cancel/budget); 阶段序列 io_read→calibrate(逐帧 ac_calibrate_frame, master 全可空)→cosmetic(ac_correct_frame)→io_write; **线程预算注入**: ac_set_num_threads(budget.max_workers)(V5 迁移整改点落地, 禁硬编码); **取消点=帧粒度**; 错误映射 AC_ERR_*/aio→acs_status(error_kind=input/output 标注); validate 纯读无 IO 拒绝未知键(无 silent default)。
2. CLI `phase1 run` 真接线: 进程内直调 session(**无 shell-out/无子进程**); cancel 桥接 SIGINT(cli_cancel_probe); budget=有效 affinity(sched_getaffinity, 2); monitor=session logger→stderr+JSONL stage/artifact/resource 事件; run manifest(status=complete+artifacts path/sha256/size)→`verify` 全链可用; 测试钩子 ASTROCS_TEST_SLEEP_MS(阶段间等待, 与 run/stub 同语义); validate 失败亦以 final 事件收尾(04 §3 stdout 纯 JSON)。
3. write_run_manifest 扩展 artifacts 参数; CMake: AIO(aio_fits/api/log/compressor)+vendored CFITSIO 4.6.4(61 源, -w 第三方豁免)+calibration(-fopenmp)编入 CLI; project 启用 C 语言; **无 -march 旗标**(ISA 污染防线不变)。
4. tests/backend/phase1_fixture_main.cpp: 合成 FITS fixture(bias100/dark150/flat1.25/light200, 64×64 常量域)+--mean 读回校验。
5. tests/cli/test_phase1_inprocess.py 5 集成测试: complete 事件+manifest+verify 全链/数值 Oracle 精确命中((200−100−1×50)/1.25=**40.000000**)**/运行中 /proc/<pid>/task/*/children 为空**(无子进程硬证)/错误映射(缺输入→3, 坏 JSON→2, master 全可空路径→0)/SIGINT 取消→9+final status=cancelled。

## 验证
- 端到端实测: phase1 run→exit 0; 事件 run→stage→artifact→resource→final 单调; manifest complete; verify OK; 校准输出均值=40.0 精确。
- 全量回归 unittest **159/159 OK**(新增 5)。
- 过程修复: use-after-free(p1_last_error 在 destroy 后调用, 3 处→先取后毁); fixture AIOImageData 完整字段; CMake C 语言/示例程序 main 冲突排除。

## 限制与遗留
- async_io_depth 参数 v1 收 {0,1,2} 且当前串行实现(预读 worker 预算租借随 CODE 域接线); API 合同允许。
- phase2(PHASE2_API_V1)同构接入=CLI-005(下一任务); 阶段序列与 production_call_paths 的 7 路径全集随 CLI-005/CODE 扩展。

## 产物
lib/phase1_session/p1_session.{h,cpp}; cli/main.cpp+cli/CMakeLists.txt; tests/backend/phase1_fixture_main.cpp; tests/cli/test_phase1_inprocess.py; 本日志。

## PASS 判定
Phase1 进程内调用(集成测试硬证无子进程); cancel/thread budget/monitor 全部注入且实测生效; 事件完整+错误映射正确+数值 Oracle 精确。CLI-004 = PASS。
