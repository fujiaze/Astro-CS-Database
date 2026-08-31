# RT-009 计划: 生成真实静态与 observed 运行图

## 目标
每个正式 preset 生成 static JSON/DOT/SVG；每次 run 生成 observed JSON/DOT/SVG 和
sidecar（IR hash、source commit、profile ID、input manifest hash）。L0 简图从相同
JSON 派生。必须能从图看到 Artifact 传递、节点耗时、provider、workers、状态。路径脱敏。

## 依赖
- RT-004: PipelineIR（静态图来源）
- RT-006: Runtime 调度（节点执行）
- RT-008: CLI 经 Runtime 唯一执行；node manifest 收集
- CHK-002: 静态 IR 与 observed trace 双向比较器（验收）

## 步骤
1. Runtime 扩展: `Runtime::NodeTrace`（node_id/status/started_utc/ended_utc/duration_ms/
   workers/provider/error）— run 时在节点 fn 捕获时间戳，run 后 `node_trace()` 返回。
2. CLI runtime_client: `collect_node_trace()` + `last_pipeline_ir_json()`（保留最近 IR）。
3. CLI commands: `write_run_graphs()` — 每次成功 run 生成
   - `graph/static_graph.json`（= PipelineIR）
   - `graph/observed_trace.json`（CHK-002 输入: 节点 + module_id/status/时间/inputs/
     outputs/output_artifacts(id+sha256)/resources）
   - `graph/graph_sidecar.json`（ir_sha256/source_commit(git HEAD)/profile_id/
     input_manifest_sha256/config_path）
   - 调用 Python 渲染器生成 DOT/SVG/L0（best-effort；路径脱敏）。
4. 新命令 `astrocs graph --preset 1,2,3 --config --output`：不执行科学计算，
   为每个正式 preset 预生成静态图 JSON/DOT/SVG + L0。
5. 路径脱敏: `sanitize_path()`（绝对路径 → `<root>/末2组件`；相对原样）应用于
   observed 产物路径与 sidecar config_path。注意 run manifest 的 config_path 保持真实
   （verify 需重读配置，不得脱敏）。

## 验收
- `run` 每次生成 9 个图产物（static/observed JSON+DOT+SVG、L0 JSON+DOT、sidecar）。
- `graph --preset` 生成静态图（无科学计算）。
- CHK-002 PIPELINE_GRAPH_PASS：静态与 observed 双向 node/edge/artifact/unit/resource 一致。
- L0 显示 Artifact 传递（cal→res artifact:cal）。
- 路径脱敏：无绝对前缀（/home/ 等）。
- CLI battery 45/45 PASS；core ctest 16/16；TSan rt009/rt006/rt008 0 race；
  CHK-001 REACH_PASS；渲染器 selftest PASS。
