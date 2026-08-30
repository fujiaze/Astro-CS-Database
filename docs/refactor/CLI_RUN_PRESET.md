# CLI-002: run preset / IR 驱动 + 真实 Artifact 传递

状态: **PASS** — HEAD=`bb2d9fc`
规则: `run --phases` 以 preset/IR 调度(不手工顺序调用 sessions); Artifact 经 manifest 哈希链传递。

## Run Preset 定义
`run --phases N` 是显式 preset 调度 (CLI-005 冻结):
- preset = phases 列表 + 每 phase 的 config 派生规则 (inputs.lights → phase1 input_lights / phase2 hips_paths; phase3 子对象直传)。
- 每 phase 独立 session (p1/p2/p3_session), 逐 phase 收集 artifact 到 all_artifacts。
- Artifact 传递: 每 phase 输出 → run manifest (astrocs_run_*.json), 含 artifact sha256 哈希链。

## 真实 Artifact 传递链
1. phase1 run → calibrated outputs (artifacts: 校准帧)
2. phase2 run → HiPS mosaic + UPM surface + rejection diagnostics
3. phase3 run → FITS output (artifacts: output_fits_path + sha256)
4. run manifest 记录全部 artifacts; resume/hash-mismatch 校验 prior artifact 磁盘 sha 一致(8=失败, 不静默跳过)

## 验证
- run 调度经 phases 列表 (preset) 驱动, 无散乱手工顺序
- 每 phase artifact 入 manifest (哈希链)
- hash-mismatch → exit 8 (不进入新一轮)
- `--events-jsonl` 模式 stdout 仅 JSON 事件 (04 §3)
