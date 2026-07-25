# 决策注册表

## 已锁定

- ADR-001：当前产品里程碑是 CLI Real-Data Core v1，GUI 后续通过脚本/进程协议控制 CLI。
- ADR-002：Orchestrator 是控制层、PipelineFrame 和算法模块的唯一连接中心。
- ADR-003：PlateSolve 正确性优先于模块解耦；共享上游 detections 仅在全量 TestData 无退化时启用，否则保留原 PlateSolve 内部检测路径并导出同次检测结果给 PSF。
- ADR-004：Astrometry Gaia 查询与 DR3SP Photometry 查询语义不同，不强制共用三列 gaia_cat。

## 运行期待决策（由任务按规则自动形成）

- ADR-RUNTIME-001：P02-003 结束后必须记录 `UPSTREAM_SHARED_DETECTIONS` 或 `PRESERVE_INTERNAL_DETECTION_EXPORT`，不得保持未决后进入 G2。

## 待决策

- ADR-PENDING-001：PipelineFrame 最终归属 data_pipeline 还是 astro_image_io。
- ADR-PENDING-002：HCSD 是否正式保存覆盖数、方差、权重和拒绝数通道。
- ADR-PENDING-003：`.aio` 是否作为 Stage1 正式中间恢复格式。
