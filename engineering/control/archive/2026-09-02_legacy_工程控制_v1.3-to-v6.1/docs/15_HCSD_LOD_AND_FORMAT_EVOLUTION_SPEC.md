# HCSD LOD 与格式演进（条件任务）

默认先完成 v1 的持久句柄、批量读取、异步缓存和 GPU Tile。只有 P15/P16 性能证据仍不能达标，才启动 P16-006 ADR。

候选方案：

- 不改主文件，生成可重建的 `.hcsd.lod` sidecar；
- HCSD v2 在每个叶下存多级 LOD；
- 兼容 Reader 同时支持 v1/v2；
- 旧文件可离线升级，不重跑 Stage1。

不得为了浏览器方便破坏现有 HCSD 科学数据和 CLI 兼容性。
