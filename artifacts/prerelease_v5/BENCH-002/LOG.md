# BENCH-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS BENCH-002 行(harness 对每 backend 先调用独立 scalar Oracle/selftest,再预热/计时;捕获错误;验收=故意错误 backend 被禁用且不得测速获胜); 06 §1(顺序: 正确性筛选→预热→多次计时→稳健统计→选择)。

## 动作
1. lib/backend_host/bench_harness.{h,cpp}: bench_kernel 流程合同——①正确性筛选(独立 scalar Oracle=double 逐元素参考, 与 kernel f32 实现不同路径; 失败→ORACLE_FAIL+理由, **不预热不计时不进候选**) ②预热(不计时) ③9 次计时(steady_clock 单调) ④稳健统计(median/MAD/p05/p95) ⑤correctness_hash(sha256 输出缓冲, 06 §4)。select_winner: 仅 verdict==OK 候选可胜出(结构性保证速度不使错误路径获胜)。
2. fixture: tests/backend/cheat_backend.cpp——cheat DSO: handshake/self_test 全过(仅 Oracle 门可拦), calibration kernel 恒返 0(错误)但零成本(必"最快"); bench_harness_main.cpp 驱动(--cheat dlopen 装载, 双候选 baseline vs cheat)。
3. tests/backend/test_bench_harness.py 4 测试: **cheat ORACLE_FAIL+无计时数据+SELECT baseline**(验收落锤)/稳健统计序(p05≤median≤p95+MAD>0+hash 格式)/correctness hash 跨运行一致/单候选可选。

## 验证
- 实测: cheat "mismatch at 0: got=0.000000 ref=1.787571" 被 Oracle 拦截(禁用), baseline SELECT——尽管 cheat 计算成本趋零。
- 全量回归 unittest **135/135 OK**(新增 4)。
- 过程修复: cheat TU 缺 baseline_kernels include/缺 -shared; 解析正则组号。

## 限制与遗留
- samples=9(≥7 满足 06 §4); 置信区间自适应采样与资源监控联动采样(thermal/throttle 标记)属 BENCH-004/07。
- 选路矩阵(profile 逐 kernel 写出 backend/workers/block/证据/备选)属 BENCH-005。

## 产物
lib/backend_host/bench_harness.{h,cpp}; tests/backend/{cheat_backend.cpp,bench_harness_main.cpp,test_bench_harness.py}; 本日志。

## PASS 判定
harness 顺序合同(Oracle→预热→计时→统计→选择)实现且机器验证; 故意错误 backend 被 Oracle 禁用且结构性不可测速获胜; correctness hash 在案。BENCH-002 = PASS。
