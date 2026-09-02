# PlateSolve 权威星对导出契约

## 目的

让外部工具验证最终 WCS，而不重新求解星对对应关系。

## 每个任务级元数据

- schema_version
- frame_id / input_sha256
- solver_version / commit
- detection_hash
- image width/height
- observation epoch
- solver_rms_px
- trans_order / sip_order
- n_inliers

## 每个 pair

- pair_id（稳定整数）
- gaia_source_id（可用时）
- gaia_ra_deg / gaia_dec_deg（求解时实际使用的历元坐标）
- detector_x_px / detector_y_px（0-based像素中心约定）
- internal_pred_x_px / internal_pred_y_px（强烈要求）
- internal_residual_x/y/dist_px
- weight / inlier=true
- detection flags：saturated、edge、blend、quality

## 格式

首选 JSONL 或 Parquet/CSV + metadata JSON。字段单位和坐标基准必须写入 schema。输出只用于验证和 provenance，不进入最终 GUI 控制协议。

## 生命周期

由 PlateSolve 在最终 inlier 集冻结后产生；Orchestrator保存到任务 evidence 或可选 PipelineFrame诊断块；P11 Gate消费后可归档，不进入 HISS 正式像素数据。
