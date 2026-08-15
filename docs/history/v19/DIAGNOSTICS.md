# AstroCS 诊断与故障定位 (V19)

## 轻量工具

```powershell
py -3.12 tools/astrocs_diagnose.py <run_dir> [--json out.json] [--timeout 30]
```

- 扫描 `<run_dir>` 下 `*.log` (>2MiB 只扫尾部 2000 行, 上限 400 文件)
- 按 `docs/ERROR_TAXONOMY.md` 的模式归类: symptom → stage → evidence →
  likely cause → command → fix path
- 默认不读大 FITS/HiPS; 外部命令一律带 timeout

## 统一 stage 记录

每个可诊断阶段记录:

```text
run_id           本次运行唯一 ID
frame_id         帧内容哈希 (Phase1 V4 frame identity)
stage_id         READ_FITS/CALIBRATE/.../STAGE2_HIPS
input_hash       阶段输入块内容哈希 (如 data/psf/photo_stats)
config_hash      生效配置哈希 (stage1/stage2 JSON 规范化)
output_hash      阶段产物哈希 (HiPS properties/tile datasum)
wall_cpu         wall 与 CPU 耗时
RSS              峰值内存
IO bytes         读写字节约 (可关闭)
threads          实际线程数
status/error     状态码/错误码 (ERROR_TAXONOMY.md)
upstream_cause   上游根因 (前序阶段错误传播)
artifacts        阶段产物路径清单
```

## 诊断开销

- 默认关闭 per-pixel 计时 (需要时 `ASTROCS_DRIZZLE_FINE_PROFILE=1`)
- 默认关闭逐 Tile 日志 (HISS_VERIFY 等用 `*_DLOG` 门控)
- 操作计数 (Drizzle `operation_counts.json`) 为整帧整数累加, 开销可忽略

## 故障定位流程

```text
1. py -3.12 tools/astrocs_diagnose.py run/logs --json diag.json
2. 按 E 代码定位阶段 → 读该阶段日志原文 (证据优先)
3. 对照 docs/TROUBLESHOOTING.md 的 symptom 表
4. 修复后最小回归 (模块测试 → 阶段 E2E → 完整 gate)
```
