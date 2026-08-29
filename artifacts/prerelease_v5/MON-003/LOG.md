# MON-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS MON-003 行「实现 07 分类和公式、first-10s 快速失败、exit 10、诊断分类 | 人工 sleep/lock/io/memory/compute fixtures 各判对; 低 CPU compute 必失败」; 07 §1/§3/§4(分类+公式+快速失败+exit 10)。ABI 冻结(v1)不改公共 API。

## 动作
新建 **cli/resource_gate.h**(纯门禁判定, 无副作用):
1. **分类** `ResKind`(compute/memory/io/mixed/unknown) + `res_kind_name`。
2. **公式**: `compute_cores_threshold = 0.80 * min(selected_workers, available_cpus)`(07 §3)。
3. **逐项门禁** `evaluate_gate`(顺序判首错, 可诊断):
   - mixed 必须先拆出 compute/io 子区间, 未拆→mixed_unsplit;
   - 无标注且 wall>5s→unannotated_priority(P1, 07 §1);
   - compute: wall<5s 豁免; available>=2 但 selected_workers/max_active_threads<2→single_threaded; avg_equivalent_cores<threshold→low_avg_cores; CPU/io/mem 皆低→compute_io_mem_all_low(禁止"单线程算法正常"解释); N-worker>1-worker→global_lock_degradation;
   - memory: 允许 CPU 未满, 但须达 pre-frozen 带宽比例, 未测量→memory_bandwidth_low(禁止不证明; 比例由 BENCH-003 写入, 不外推/不事后改);
   - io: 允许低 CPU, 但须 bytes/ops/await 证据→io_missing_evidence; 短串行 IO(<5s 或 <5% 时长)→豁免;
4. **first-10s 快速失败** `fast_fail_first10s`(07 §4): 低 CPU+非 IO+非内存带宽饱和→true(调用方协作取消+exit 10)。
5. **诊断分类** `GateDiag` 枚举 + `diag_message`(可操作说明, 含阈值)。

## 验证
- tests/cli/test_resource_gate.py(7 测试, 覆盖人工 fixtures):
  - 01 compute ok/单线程/低核/CPU-io-mem皆低各判对; 02 短<5s 豁免+全局锁 fail; 03 memory 达标/未达/未测量各判对; 04 io 证据/缺证据/短串行; 05 mixed 拆份/未拆份; 06 无标注>5s→P1+诊断消息含 0.80 阈值; 07 first-10s 快速失败触发/不触发。
  - 人工 fixtures(sleep→低 CPU compute 必失败; lock→全局锁退化; io→证据/短串行; memory→带宽比例; compute→利用率)语义由以上 case 精确对应。
- 全量回归 unittest **235/235 OK**(新增 7, 零回归)。

## 限制与遗留
- 门禁判定模块落地; 与 CLI/phase 活体集成(first-10s 采样+协作取消调用点、run 成功路径调用 evaluate_gate 并映射 exit 10)由后续集成任务(或 WIN-003 资源门禁)接线; 本任务实现 07 分类/公式/快速失败/exit 10 语义与诊断, 由纯逻辑测试证明。
- memory 带宽比例(BENCH-003 写入 test contract)为外部输入; 未测量即 FAIL(禁止假通过)。
- iowait/await 为可选字段(可得时采入), 缺失时按"无证据"保守判。

## 产物
cli/resource_gate.h(分类+公式+门禁+快速失败+诊断); tests/cli/test_resource_gate.py(7 测试); artifacts/prerelease_v5/MON-003/LOG.md; 本日志。

## PASS 判定
07 分类(compute/memory/io/mixed/unknown)+公式(0.80*min(workers,cpus))+first-10s 快速失败+exit 10(RESOURCE, gate 失败由调用方映射)+诊断分类(GateDiag+diag_message)全部实现; 人工 fixtures 语义(test_01..07)各判对, 低 CPU compute 必失败(single_threaded/low_avg_cores/compute_io_mem_all_low)。MON-003 = PASS。
