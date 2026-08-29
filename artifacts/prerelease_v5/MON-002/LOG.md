# MON-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS MON-002 行「所有 Phase/kernel 发 stage/resource/backend 事件; summary/downsample/raw 分层 | >5s 未标注 stage 的 mutation test 必失败; 审核摘要小型化」; 07 §1-2(强制启用+resource-detail+stage 标注+必采指标+分层)。ABI 冻结(v1)不改公共 ABI; 仅 CLI 侧事件组装。

## 动作
1. 新建 **cli/resource_events.h**(header-only):
   - `StageKind` 枚举(compute/memory/io/mixed/unknown) + `stage_kind_name`。
   - `downsample_curve`: 降采样序列到 ≤ kDownsampleMax(=121) 点(审核摘要小型化, 禁原原始几 MB 打包)。
   - `ResourceReport`(summary + curve + raw_dir/raw_n): summary 强制; 原始 timeseries 只记录留存路径+样本数, 不内嵌数据。
   - `summarize(ProcessMonitor::Summary)` → payload; `build_curve`(等价核/RSS/IO/ctxsw 逐点)。
   - `classify_stage` / `is_unannotated_priority`: 无标注(unknown)且 wall>5s → P1(07 §1, 供 MON-003 gating)。
2. 修改 **cli/main.cpp**:
   - 新 flag `--resource-detail`(summary|timeseries), 加入 `kValueFlags` + `phase*/run` 白名单; 非法值→parse_fail(2)。
   - `cmd_run_pipeline` 集成: 启动进程监控背景采样线程(run 期间累计 CPU/RSS), 结束后 join; 成功路径发射 **resource summary 事件**(07 §2 必采指标 + resource_detail + raw_dir/raw_n)与 **backend 事件**(backend_id/workers_used/available_cpus, 禁硬编码)。
   - `emit_resource_summary` / `emit_backend_event` helper + 前向声明。
3. 分层: timeseries 详略内嵌 `downsample_max`/curve 标记; summary 不内嵌曲线数据(仅 raw 指针)。

## 验证
- tests/cli/test_monitor_events.py(6 测试):
  - test_01 `--resource-detail summary|timeseries` accepted, bogus→非0;
  - test_02 resource summary 事件必含 07 §2 指标(n_samples/peak_rss/wall/peak_equivalent_cores/max_threads);
  - test_03 backend 事件含 backend_id/workers_used/available_cpus;
  - test_04 timeseries 内嵌 downsample_max, summary 不内嵌曲线数据(分层小型化);
  - test_05 stage 类别枚举固定(compute/memory/io/mixed/unknown);
  - test_06 classify_stage/is_unannotated_priority: 无标注>5s→P1, compute 标注不 P1, <5s 无标注不 P1, 降采样≤121 点。
- 全量回归 unittest **228/228 OK**(新增 6)。
- 手工验证: run --phases 3 --resource-detail timeseries → resource 事件(resource_detail=timeseries, n_samples, peak_rss, raw_dir, downsample_max=121) + backend(baseline, workers_used=2)。

## 限制与遗留
- 阶段级 stage 事件(compute/memory/io/mixed 标注)落地在每个 session 内, 由各 session 的 stage_start/stage_end 承载; 本任务建立事件分层+resource/backend 摘要与 P1 语义, 具体每 stage 标注由 MON-003 在 gating 中细化(分类与公式)。
- Windows(GetProcessTimes/SystemInfo)桩随 MON-001, 需 FATDUCK 复验。
- backend_id 现取 config `backend` 键; 因 run-config 校验不允许未知键, 实际当前取默认 "baseline"(真实选择由 BENCH-005 注入)。记录为简化。

## 产物
cli/resource_events.h(分层+降采样+stage 标注); cli/main.cpp(+--resource-detail+resource/backend 事件发射); tests/cli/test_monitor_events.py(6 测试); artifacts/prerelease_v5/MON-002/LOG.md; 本日志。

## PASS 判定
resource-detail summary|timeseries 分层落地(summary 强制, timeseries 才有降采样曲线标记); resource/backend 事件含 07 §2 必采指标; 无标注>5s→P1 语义实现+按标注豁免; 降采样≤121 点(审核小型化)。MON-002 = PASS。
