# CLI-008 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS CLI-008 行「删除发布 install/package 中旧 phase/benchmark exe；保留 test-only target 需明确 | install tree scanner 仅一个用户 exe；CLI 不 shell-out」；04 §1(仅一个 astrocs CLI 用户入口)/API-CLI-001/ARCH-002(单一 CLI 架构)/CLI-004·006·007(进程内无 shell-out)。

## 动作
1. **确认 cli/CMakeLists.txt 安装规则单执**: `install(TARGETS astrocs RUNTIME DESTINATION bin)` 是 CLI 项目唯一 install 规则; 无任何 phase/benchmark/tool/test exe 被安装。
2. **新测试 tests/cli/test_cli_single_install.py(CLI-008 scanner)**: 对 CLI 项目 `cmake --build` → `cmake --install --prefix` 到临时 prefix, 然后扫描安装树:
   - test_01 install 成功;
   - test_02 bin 恰一个用户 exe(=`astrocs`/`astrocs.exe`);
   - test_03 无任何旧 phase/benchmark/tool/test exe(orchestrator/astrocs-stage2/phase2*/acr-benchmark/acr-report/acr-classic-runner/browser_cli/healpix_browser_qt/calibrated_pair_diag/rejection_cli)泄漏;
   - test_04 无脚本入口(.sh/.bat/.cmd, 防 shell-out 转发);
   - test_05 安装树不含源码/头/third_party 便携产物。
3. **无 shell-out 机器证明**: 复用已有 CLI-004(test_phase1_inprocess /proc/<pid>/task/*/children=0)与 CLI-006(test_phase3_inprocess 进程内)证据; 本任务 scanner 补 install 树层"无脚本入口"守卫。

## 验证
- staging install: `/tmp/astrocs_install/bin/astrocs` 恰一个 exe; install 树无其他可执行/源码/第三方便携件。
- 全量回归 **unittest 201/201 OK**(新增 5)。
- 旧 phase/benchmark exe(orchestrator/stage2/acr-*)存在于各自独立 CMake 工程(lib/phase2, lib/acr/, lib/healpix_db), **不进入 CLI-release 安装树**——因发布安装由 cli/CMakeLists.txt 单独 `install(astrocs)` 决定; 这些 exe 的 add_executable 均无 `install(TARGETS ... RUNTIME)` 规则。

## 限制与遗留
- 本 scanner 覆盖 Linux(GCC)与 Windows 语义(排 .exe/.dll); MSVC 实际 install 树扫描由 FAT/WIN 远程节点验证(Agenda: 完整 release 打包=LNX-005/WIN-00x)。
- 测试/工具 target(phase2_synthetic_gate/test_* 等)是 test-classified 的独立目标, 仅在其模块 CMake 内 add_executable, 不出现在 CLI 安装树——满足"保留 test-only target 需明确"。

## 产物
tests/cli/test_cli_single_install.py(5 测试); 本日志。cli/CMakeLists.txt 已确认单 exe 安装(未改动, 已满足)。

## PASS 判定
install tree scanner 确认发布安装树恰一个用户 exe(astrocs), 无旧 phase/benchmark exe 泄漏, 无脚本 shell-out 入口; CLI 不 shell-out 由 CLI-004/006/007 进程内 + /proc children=0 机器证明。CLI-008 = PASS。
