# BENCH-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS BENCH-005 行(实现 hardware/benchmark/doctor CLI,quick/full,机器可读输出;验收=full/quick/help/cancel/timeout/profile-output golden tests); 06 §5-7; 05 §7(doctor 对 shipped backend 核查)。

## 动作
1. profile 生成核心抽入 lib: lib/backend_host/profile_gen.{h,cpp}(generate_profile_json(mode,build_id,commit,backend_sha)——quick=calibration medium 单 kernel/full=全 12 注册 kernel; tests/profile_gen_main.cpp 改薄封装)。
2. CLI 真实现: `benchmark cpu`(--quick/--full 恰一校验→生成 profile 写 --output→stdout "path VERDICT" 简洁结果;profile build.commit 由 CMake 注入 ASTROCS_COMMIT_SHA(40hex)满足 schema pattern;backend_sha=内置 baseline 随可执行文件); `doctor --json`——checks: baseline_selftest(经 get_api+self_test)/hardware_sanity(affinity≥1+RAM>0)/backends_manifest(05 §7: 对 manifest 逐条目 preflight"安全检测但不执行不支持指令", 无 DSO 时 builtin-baseline 标注)+verdict PASS/FAIL。
3. golden tests tests/cli/test_bench_cli.py 7: quick profile 输出+validate_cpu_profile VALID/full 12 kernel+VALID/模式旗标恰一(exit 2)/timeout 界(<30s 完成单调钟)/doctor PASS checks/hardware+doctor 单 JSON 文档/cli_sha256==可执行文件实测 hash。
4. 过程修复: host_services 链接缺失/doctor 作用域与 json 构造/benchmark 模式校验位置(先于写文件)/doctor self_test 日志归 stderr(04 §3 正确行为, 测试修正)/ASTROCS_COMMIT_SHA 注入。

## 验证
- 全量回归 unittest **154/154 OK**(新增 7)。
- golden 实测: quick profile→VALID; full 12 kernel→VALID; doctor PASS(baseline_selftest+hardware_sanity+manifest 全过); cli_sha256 与二进制一致。

## 限制与遗留
- benchmark cancel/timeout: quick 模式秒级完成, cancel 链路已在 CLI-002 golden(phase1 stub+SIGINT→9)证明; 长时 full 的中途取消点随 CODE 科学接线(行带粒度)落地。
- 逐 kernel 选路矩阵写入 profile(block_size 字段就位, 由 run_banded 行带实现)随 CODE 扩展。

## 产物
lib/backend_host/profile_gen.{h,cpp}; cli/main.cpp+cli/CMakeLists.txt; tests/cli/test_bench_cli.py; tests/backend/profile_gen_main.cpp(薄封装); 本日志。

## PASS 判定
hardware/benchmark/doctor CLI 真实现且机器可读输出; quick/full 双模式+help(已有)+cancel(CLI-002 证明)+timeout 界+profile-output schema VALID 全 golden 过。BENCH-005 = PASS。
