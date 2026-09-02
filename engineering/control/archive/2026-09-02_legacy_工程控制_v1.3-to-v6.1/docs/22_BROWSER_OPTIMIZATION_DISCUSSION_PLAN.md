# AstroCS 球面坐标浏览器性能优化方案 v1

- 文档状态：讨论稿，不作为已批准开发包
- 审计基线：AstroCS v1.1，`main` at `a3b468d`
- 目标组件：`lib/healpix_db/healpix_browser_qt`
- 目标数据：HISS / HCSD，优先验证银心三片 Red 马赛克 HCSD

## 1. 目标与边界

浏览器仍是 AstroCS CLI 底层的结果查看器，未来可被 GUI 壳层复用。它不负责 Stage 1/Stage 2 算法，只负责：

1. 快速打开 HISS/HCSD；
2. 按视场加载必要球面数据；
3. 平滑缩放、拖动和 STF；
4. 显示大尺度马赛克、接缝与局部细节；
5. 为后续 JavaScript/GUI 控制层提供稳定的数据浏览接口。

本方案不要求用户安装 PowerShell/Python，不把脚本作为产品运行时依赖。

## 2. 当前性能瓶颈（源码事实）

### 2.1 HCSD 打开时全量读取

`BrowserBackend::open_file()` 对 HCSD 调用 `hcsd_read()`，读取全部 `ipix/pixel` 后立即释放。大文件打开阶段产生无意义的全量 I/O 和内存峰值。

### 2.2 每个子叶读取都重新打开并解析文件

`BrowserBackend::load_leaf()` 调用 `hcsd_read_leaf(path, leaf)`。当前 API 每次重新打开文件、读取/解压 JSON、定位索引、读取数据、关闭文件。连续加载多个叶时重复成本很高。

### 2.3 每帧扫描全部 49152 个 nside=64 子叶

`get_required_leaves()` 每帧遍历 `12×64²=49152` 个叶，逐个执行 `pix2ang` 和球面角距离，再排序候选。拖动和缩放会重复该过程。

### 2.4 I/O、降采样和缓存更新阻塞渲染线程

`render_sphere()` 同步调用 `load_leaf()` 和 `ud_grade()`。文件读取、内存分配、哈希聚合都发生在 OpenGL/GUI 线程。

### 2.5 CPU 每顶点查值并整块更新 VBO

当前渲染方案为“CPU 端每顶点查值”：

1. 动态生成最多 2048×2048 级别球面网格；
2. 每帧对每个顶点执行 HEALPix `ang2pix`；
3. 在叶内排序数组中多级二分查找；
4. 重新构建 `vertex_data`；
5. `glBufferSubData` 上传整个 VBO。

相机移动本应只更新矩阵，当前却重复执行球面重采样。

### 2.6 `ud_grade` 使用 `unordered_map` 并转换为 uint8

实时 LOD 降采样成本高，同时将科学 float32 数据量化为 uint8。STF 调整无法恢复量化丢失的动态范围。

### 2.7 缓存缺少稳定的预算、异步状态和视场预取

当前缓存主要按当前 required 集合立即删除，缺少：

- 按字节预算的 LRU；
- 当前视野外一圈预取；
- 请求取消和代际编号；
- CPU/GPU 两级缓存；
- 交互期间低 LOD 保底。

## 3. 目标架构

```text
Qt/未来 GUI
    │ ViewRequest(center, fov, viewport, channel)
    ▼
View Controller
    ├── 可见叶层级查询
    ├── LOD + 滞回决策
    └── 请求代际/取消
          │
          ▼
Tile Scheduler
    ├── CPU Tile Cache (LRU)
    ├── I/O 线程池
    ├── Decode/LOD 线程池
    └── GPU Upload Queue
          │
          ▼
Persistent HCSD/HISS Dataset Handle
    ├── 文件句柄或 mmap
    ├── Header 常驻
    └── 49152 叶索引常驻
          │
          ▼
GPU Tile Renderer
    ├── R32F 数据纹理
    ├── R8 有效掩码
    ├── 静态/可复用叶网格
    └── Shader STF + 相机矩阵
```

## 4. 第一阶段：消除无意义 I/O 和主线程阻塞

### 4.1 新增持久读取句柄

在 `astro_image_io/healpix_io` 增加 C ABI：

```c
typedef struct AioHcsdReader AioHcsdReader;

int aio_hcsd_open_readonly(const char* path, AioHcsdReader** out);
int aio_hcsd_get_header(AioHcsdReader* r, AioHcsdHeader* out);
int aio_hcsd_get_leaf_index(AioHcsdReader* r,
                            const AioLeafIndexEntry** out_entries,
                            uint32_t* out_count);
int aio_hcsd_read_leaf(AioHcsdReader* r, uint64_t leaf,
                       AioLeafData* out);
int aio_hcsd_read_leaf_batch(AioHcsdReader* r,
                             const uint64_t* leaves, uint32_t count,
                             AioLeafBatch* out);
void aio_hcsd_release_leaf(AioLeafData* leaf);
void aio_hcsd_close(AioHcsdReader* r);
```

要求：

- `open` 只读取 Magic、压缩 Header 和固定叶索引；
- 文件句柄在数据集打开期间保持；
- 支持相邻偏移合并读取；
- 所有返回内存有明确释放函数；
- Reader 在一个专用 I/O 线程或内部锁下使用；
- 旧 `hcsd_read`/`hcsd_read_leaf(path,...)` 保留兼容。

### 4.2 异步 Tile Scheduler

定义：

```cpp
struct TileKey {
    uint64_t leaf64;
    uint32_t nside;
    uint16_t channel;
};

enum class TileState {
    Missing, Queued, Loading, ReadyCPU, QueuedGPU, ReadyGPU, Failed
};
```

线程边界：

- GUI/OpenGL 线程：只提交请求、上传已完成数据、绘制；
- I/O 线程池：2 个线程，读取叶数据；
- LOD/Decode 线程池：`max(1, min(4, hw_threads-2))`；
- 任何外部等待和任务必须有超时与取消标志。

使用 `view_generation`：视角每次明显变化递增。旧代际尚未开始的任务取消；已经读取的结果可进入 LRU，但不得阻塞最新视图。

### 4.3 交互降级

鼠标拖动或滚轮连续输入期间：

- 保留上一帧可用 tile；
- 请求低 1–2 级 LOD；
- 合并输入事件，只渲染最新视角；
- 交互停止后再请求目标 LOD；
- 不允许因缺少高 LOD 显示黑屏。

## 5. 第二阶段：层级可见性查询和稳定 LOD

### 5.1 不再扫描 49152 叶

预计算 nside=64 每个叶的：

- 中心单位向量；
- 球面包围半径；
- 12 个 base face 的层级父子关系。

查询从 12 个 base face 开始，使用球面视锥/圆锥相交测试递归到 nside=64。只访问可能相交的节点。

若暂不实现完整递归，可先把 49152 中心向量常驻连续数组，使用点积代替 `pix2ang + acos`，并建立经纬度分桶；但该方案仅是过渡。

### 5.2 屏幕空间误差 LOD

目标 LOD 由屏幕像素角尺度决定：

```text
healpix_pixel_angle(nside) <= k × screen_pixel_angle
```

建议 `k=1.0~1.5`。增加 20% 滞回：只有理想 LOD 超过当前档位的上下阈值时才切换，防止缩放边界反复重载。

### 5.3 预取

加载顺序：

1. 当前视场中心；
2. 当前视场其余叶；
3. 视场外 0.5–1 个 tile 环；
4. 更高 LOD 细化。

## 6. 第三阶段：GPU Tile Renderer

不继续优化“CPU 每顶点查值”主路径，改为按 HEALPix 叶渲染。

### 6.1 Tile 数据布局

HCSD 使用 NESTED。对一个 nside=64 叶，在目标 nside 下，子像素构成边长：

```text
tile_side = target_nside / 64
```

的 Morton/NESTED 网格。解交织本地 ipix 位，生成：

- `R32F` 或 `R16F` 数据纹理；
- `R8` 有效掩码；
- 可选 coverage/SNR/variance 通道。

无数据像素使用 mask，不用特殊浮点值参与 STF。

### 6.2 叶网格

每个可见叶绘制一个球面 patch：

- 叶边界和内部采样点由 HEALPix 几何生成；
- 同一 LOD 的索引缓冲共享；
- 顶点位置在 tile 首次进入 GPU 时生成一次；
- 相机移动只更新 View/Projection uniform；
- 不再逐帧重建百万顶点 VBO。

### 6.3 Shader

Fragment shader 完成：

- float 纹理采样；
- 有效掩码；
- STF shadows/highlights/midtones；
- 可选通道切换；
- tile 边缘无缝处理。

### 6.4 GPU 缓存

GPU cache 按字节预算 LRU：

- 默认 512 MiB，可配置；
- 当前视场 tile pin；
- 预取 tile 可优先淘汰；
- 纹理上传在渲染线程执行，但数据准备在后台完成；
- 每帧限制上传字节数，避免卡顿尖峰。

## 7. 第四阶段：多分辨率磁盘缓存

实时 `ud_grade` 只能作为兼容路径。建议增加 HCSD LOD sidecar 或 HCSD v2：

```text
sky.hcsd
sky.hcsd.lod
```

sidecar 对每个 leaf 保存多级 nside tile，并带版本、源文件 hash、生成参数。优点：

- 旧 HCSD 不必重建；
- 大视场无需实时哈希降采样；
- sidecar 可删除重建；
- 后续 GUI/本地服务可直接按 tile/LOD 请求。

正式稳定后再决定是否把 LOD 合并进 HCSD v2。

## 8. HISS 模式

当前 HISS 全量加载并排序。短期保留兼容，但统一进入 Tile Scheduler：

- 打开后建立/读取叶索引；
- 数据量小时允许全量常驻；
- 超过阈值时使用 sidecar 索引或转换为临时 tile cache；
- 不在渲染线程执行 `stable_sort`。

## 9. 缓存与生命周期

### DatasetHandle

- 产生：打开文件成功后；
- 持有：Header、索引、文件句柄/mmap；
- 销毁：关闭数据集或程序退出；
- 关闭前取消该数据集全部任务。

### CPU Tile

- 产生：I/O + 解码完成；
- 格式：float32 + mask，带 TileKey 和来源 hash；
- 持有：CPU LRU；
- 销毁：LRU 淘汰且无上传/绘制引用。

### GPU Tile

- 产生：渲染线程上传；
- 持有：GPU LRU；
- 销毁：淘汰时在有效 GL context 中删除纹理/VBO。

### View Request

- 产生：相机/视口/通道改变；
- 销毁：新 generation 替换或数据集关闭。

## 10. 性能验收

在固定参考机器和固定 HCSD 上记录，不允许只写“感觉流畅”。

最低门槛：

1. 200–500 MiB HCSD 打开到首个低 LOD 画面 ≤ 1.5 s；
2. 打开阶段不得全量读取所有 pixel；
3. 缓存命中时拖动 p95 帧时间 ≤ 16.7 ms；
4. 缓存缺失时 UI 线程单次阻塞 ≤ 5 ms；
5. 连续拖动期间 p95 ≤ 33 ms；
6. 放大停止后目标 LOD p95 就绪 ≤ 500 ms（本地 SSD）；
7. 无每帧 49152 叶扫描；
8. 无每帧全 VBO 数据重建；
9. 主线程磁盘 I/O 次数为 0；
10. float32 数据在 STF 前不得量化为 uint8；
11. CPU/GPU 缓存峰值在配置预算 ±10% 内；
12. 连续 10 分钟拖动缩放无泄漏、崩溃和明显缓存抖动。

输出性能报告必须包含：

- open/header/index/I/O/decode/upload/render 分项耗时；
- cache hit/miss/eviction；
- visible/requested/ready tile 数；
- CPU/GPU 内存；
- FPS 和 p50/p95/p99 frame time。

## 11. 银心三片马赛克可视化验收

浏览器修复后必须打开 Stage 2 生成的银心三片 Red HCSD，并交付：

1. 全三片视场截图；
2. panel1↔panel2 接缝放大截图；
3. panel2↔panel3 接缝放大截图；
4. 星点局部高分辨率截图；
5. 梯度关闭/开启同视角对比图；
6. 性能 overlay 截图；
7. 可运行浏览器与 HCSD 文件路径；
8. 可选 30–60 秒拖动/缩放录屏。

截图不能替代数值验收，但必须让用户能够直观看到最终马赛克结果。

## 12. 推荐实施顺序

```text
B0 性能基线与 trace
→ B1 持久 Reader + metadata-only open
→ B2 异步 Tile Scheduler + LRU + 取消
→ B3 层级可见性查询 + LOD 滞回
→ B4 GPU Tile Renderer + float STF
→ B5 LOD sidecar
→ B6 银心 HCSD 可视化与压力验收
```

B1–B3 可先显著改善卡顿；B4 是彻底移除 CPU 每顶点查值瓶颈的主改造。不得在现有同步渲染路径上仅增加线程数或继续扩大网格上限。
