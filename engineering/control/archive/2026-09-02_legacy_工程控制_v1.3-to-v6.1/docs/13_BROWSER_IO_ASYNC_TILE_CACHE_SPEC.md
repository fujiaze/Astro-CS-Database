# 浏览器异步 I/O 与 Tile Cache 规范

第一阶段不改 HCSD v1：

- `aio_hcsd_open_readonly`：一次打开并缓存 Header/索引；
- `aio_hcsd_read_leaf_batch`：批量读取相邻叶；
- 后台 I/O/解码线程池，渲染线程零磁盘访问；
- generation/request token，快速交互取消过期请求；
- 层级可见叶查询，禁止每帧扫描 49152 叶；
- CPU 与 GPU 独立的按字节 LRU；
- 当前视野周边预取；
- 拖动时低 LOD，静止后渐进细化。

线程所有权、Tile 状态机和关闭/换文件时的取消顺序必须明确，避免 use-after-free。
