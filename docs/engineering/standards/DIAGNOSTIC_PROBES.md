# ACSD 中间数据快照与性能探针插入规范

> 工程标准正本；上游：docs/ACSD_DESIGN.md §3、§8–§10、§13 及 docs/engineering/architecture/DEV_MODE.md。已有基础设施：lib/infrastructure/observability/probes/include/acsd/probe.h、src/probe.cpp、lib/infrastructure/aio/。2026-10-14 目标规范，**实现前不声称 API/格式已存在**。
>
> 核心约定：**一套探针身份、一条共享诊断通道、一个公开编译开关 ACSD_DEV_MODE**。所有开发快照与详细性能事件**只观察科学计算，不是科学产品、不影响执行路径、永不被后续计算读取**。Phase2 正式产品仍是一个 signal HiPS 科学图像通道。

## 1. 插入点及职责分层

开发探针有两种，二者共用 stable probe ID、run_id、stage、source fingerprint：

| 类型 | 执行者 | 数据 | 持续性 |
|---|---|---|---|
| 数据快照 SnapshotProbe | 算法模块在**已有输出块完成且仍有效**时发出只读 view | 原样数值、形状、单位、坐标、有效域/掩膜、算法阶段 | 只有内部开发构建、且该项被选择时可落诊断工作区 |
| 性能事件 PerfProbe | 模块作用域、调度器、I/O 或 CPU kernel wrapper | steady time、计数、读写字节、样本数、内存等观测 | 只有内部开发构建记录详细事件；正式常规运行日志独立存在 |

业务模块只写“在此处可观察什么”，不拥有诊断文件名、日志写入、后台线程或磁盘保留策略。所有快照落盘调用既有 **aio 统一 I/O**；性能记录尽量复用现存 acsd/probe.h 的 RAII timer、count、gauge 和 thread-local 缓冲。统一基础设施可在内部变更以支持元数据和选择器，禁止第二套手工 fopen/printf/JSON logger。

调用点由持有该数值/缓冲的**唯一生产者**登记，避免上游 writer 与调度器重复对同一数据落两份数据。诊断头/API 不可把 C++/STL 类型跨外部 ABI 暴露；如从 C ABI/DSO 调用，用稳定 POD view/版本号或模块适配层桥接。

## 2. 探针命名与静态登记

每个探针须在设计/实现登记表中唯一确定以下字段，不是临时打印语句：

| 字段 | 含义 |
|---|---|
| probe_id | 全小写层级标识，推荐 namespace stage.module.quantity[.subquantity]，例如 normalize.noise.reference_snr |
| kind | snapshot/image、snapshot/table、snapshot/model、perf/span、perf/counter、perf/gauge |
| producer | **实际生产函数/节点**，使用现行公开 API 的节点身份而非代码行号 |
| observation_boundary | 数学状态：前/后背景扣除、光度缩放前/后、Drizzle 前/后、排异前/后等 |
| semantic | 物理被估计量及信号/通量/方差/权重/控制点定义，不允许只有含糊的 weight |
| unit | 原样单位或已定义无量纲；不应把 Gaia XP 相对标度误标 ADU/sr |
| coord_space | detector_pixel、ICRS、HEALPix_NESTED、HiPS_tile_FITS_rowmajor、target_WCS_pixel、none |
| shape_dtype | [rows,cols] / tile / n_records，F32/F64/I32 等 |
| valid_support | 覆盖、NaN、拒绝样本、空洞编码与统计量的真实语义 |
| lifetime | 只读 view 有效区间与调用者对采样/拷贝的约束 |
| expected_cost | 通常一个标量/每帧/每 tile/每批样本，不允许默认“每像元磁盘写” |

一个节点可以多个 probe_id，但不能为每个线程/动态输入创建任意新 ID；帧、tile、worker 是**标签**，不是 ID 本身。probe_id 稳定，便于跨版本/跨机器比较。改科学被估计量而不改 probe_id 的行为视为元数据错误，必须记录 semantic revision 或使用新 ID。

运行中实际启用的探针应从内部静态登记表筛选；不得将任意表达式/指针地址/任意路径作为通用 dump 请求。用户数值科学配置本身不应因为 dev ON 变化。

## 3. 最少应暴露的科学快照位置

以下为**建议的具体插入点清单**。Agent 须按真实生产函数名对齐后落地；若数值并未在该点生成，报告缺失，不自行合成一个看似合理的值冒充原始中间量。

| probe_id | 快照内容 | 坐标/单元 | 主要科学用途 |
|---|---|---|---|
| normalize.calibration.signal | 校准后线性像素值 | detector/WCS、当前积分信号单位 | 检查 bias/dark/flat、数值范围和坏像元 |
| normalize.cosmetic.validity | 修复/否决后信号与有效掩膜 | detector、掩膜原因 | 识别坏列、宇宙线对真实结构的损失 |
| normalize.psf.model | PSF 参数、残差与抽样位置 | detector→ICRS，FWHM/积分归一 | 审查 PSF 定义和时空变化 |
| normalize.photometry.scale | Gaia XP 拟合残差、颜色项、标度字段 | star catalogue/detector，实际光度单位 | 排查 R_k/R_0、重复标度、通带差 |
| normalize.noise.reference_snr | 真正的稀疏 SNR_ref/F_ref/σ_F 控制点 | ICRS、ref flux 单位 | 对拍 P2 SNR 估计与 P4 producer |
| normalize.drizzle.geometry | 指定 drop 与 leaf 的交叠 a、A_drop、Ω_src、候选与误差 | HEALPix NESTED、sr | 源分割/曲边/极区交叠 |
| normalize.drizzle.signal_tile | S_p、有效几何分母、可得方差与覆盖状态 | HEALPix tile FITS local、BUNIT | P3 守恒/数值/相关噪声 |
| mosaic.snr.input_controls | Phase2 实际**读取**的稀疏控制点、状态 | ICRS/NESTED + frame_id | 排查 Catalogue → 重建断链 |
| mosaic.snr.reconstruction | 查询 ROI 的预测 V_F、支持域与插值统计 | NESTED、[F]² | 插值过冲/空洞/跨 tile |
| mosaic.weight.surface_precision | 与相同 ROI 匹配的 V_B、T、w_B、effective PSF/核 | NESTED、明确单位 | 对拍点源→面亮度权重与统计效率 |
| mosaic.sky.relative_field | 各连通分量差模 a_k、规范与控制点残差 | ICRS/NESTED、背景面亮度单位 | 无缝/低频保真/断连零点 |
| mosaic.rejection.sample_mask | 排异资格、实际样本数和原因摘要 | NESTED、boolean/reason | 检查排异是否误删真源 |
| mosaic.integration.signal_tile | 最终 tile 生成**之前**的 weighted signal、内部 sample support | NESTED、面亮度单位 | 核对叠加/NaN/局部台阶 |
| export.resample.footprint | WCS 目标位置、输入 leaf、有效核权/覆盖 | target WCS、sr 或无量纲 | 方向/像素中心/二次插值 |
| export.fits.signal_tile | FITS 输出前的实际未拉伸 signal 区域 | target WCS、BUNIT | 排查末端软化与数值偏差 |

这是诊断列表，**不是新的正式阶段产品清单**。尤其 mosaic.weight.*、mosaic.sky.* 与 rejection 中间快照不可变成最终 HiPS 的 variance/ivar/support/权重第二通道。

## 4. 性能探针的最小范围和命名

优先每节点/每帧/每 tile/每 kernel-batch 作用域，热点内部再分段：

| perf ID 示例 | 数据 | 解释 |
|---|---|---|
| normalize.calibrate.frame | wall_ns、processed_pixels、bytes_read | 磁盘/像元校准成本 |
| normalize.gaia.query_batch | wall_ns、n_candidates、cache_hit | Gaia 本地读取与匹配性能 |
| normalize.psf.fit_batch | wall_ns、n_stars | 拟合而不是仅帧总墙钟 |
| normalize.noise.control_estimate | wall_ns、n_controls | 稀疏 SNR 计算成本 |
| normalize.drizzle.overlap_tile | wall_ns、n_drops、n_intersections | 几何开销、候选裁剪、极区分层 |
| mosaic.snr.reconstruct_tile | wall_ns、n_queries、n_controls | P4 现场密集查询与缓存 |
| mosaic.sky.fit_component | wall_ns、n_obs、n_solver_iter | 重叠/矩阵构建/求解 |
| mosaic.integrate_tile | wall_ns、n_candidates、n_rejected | 叠加计算成本 |
| export.resample_tile | wall_ns、n_pixels、n_neighbors | 二次重采样耗时 |
| io.read_tile、io.write_tile | wall_ns、bytes_requested/actual、queue_wait_ns | 区分算子慢和 I/O 慢 |
| runtime.scratch.peak_bytes、runtime.queue.depth | 数字及其采样时刻 | 内存峰值和积压诊断 |

计时用 monotonic/steady clock，`ts_utc` 仅作阅读/对齐，不能用墙上时钟差计算 duration。wall_ns 是作用域历时、cpu_ns 为**可测时才记录**的线程/进程 CPU 时间，二者不能互换；多个并发 worker 的 wall_ns 相加不是整次运行 wall 时间。统计吞吐时必须附实际工作量 n_drops/n_pixels/bytes 和 worker 宽度/CPU backend，否则不同任务结构无法比较。

性能探针应帮助定位负载，不参与科学线程预算选择的数值路径，不改优先级、不阻塞热点、不按采样耗时“动态调整精度”。调度的真实资源事件与正式 run_graph 若属于正常产品观测，继续保留；开发细分计时与正式事件只关联不互相替代。

## 5. 开发运行选择器：只控制观测多少

`ACSD_DEV_MODE=ON` 时允许读取开发专用诊断策略（配置位置由现有机器/运行 config 约定落地，不引入新业务科学模式）。以下**拟议内部配置**是语义示例，**不是当前已经实现的 JSON schema**：

```json
{
  "diagnostics": {
    "perf": {"enabled": true, "detail": "node", "sample_every_n_tiles": 1},
    "snapshots": {
      "enabled": true,
      "probe_ids": [
        "normalize.noise.reference_snr",
        "mosaic.snr.reconstruction",
        "mosaic.weight.surface_precision",
        "mosaic.sky.relative_field",
        "mosaic.integration.signal_tile"
      ],
      "frame_ids": [],
      "tile_ids": [],
      "max_frames": 2,
      "max_tiles": 4,
      "max_total_bytes": 1073741824,
      "max_inflight_bytes": 67108864
    }
  }
}
```

- 数值只是一个**示例选择器**，默认策略不要把整幅巨型 HiPS 或真实大 FITS 全量输出；
- `probe_ids`、frame_id、tile、ROI 与 run_id 决定身份，选择逻辑应当确定性；按随机数/线程完成先后选样会破坏两次运行对照；
- 大数组取指定 ROI/tile；如带 frame_ids 查询但当前没有该帧，记 `NOT_SEEN`，不要静默造填充图；
- `max_total_bytes`、`max_inflight_bytes`、每项样本上限、异步队列深度由一个配置面冻结，不在模块里硬编码多套全局默认；
- 更完整的性能探针可由 perf.enabled 独立打开而快照关闭；它们**都受同一个编译期 ACSD_DEV_MODE 的最终约束**；
- 发布构建读取相同科学 JSON 时不因为存在 diagnostics 扩展而改变科学计算；由配置解析层明确声明 diagnostics 为开发私有字段/另独立运行配置，避免默认 schema 误拒合法科学任务。

## 6. 中间数据格式：科学原样、可独立解读

禁止把中间浮点数据写成经过拉伸、量化、clamp、NaN→0、取绝对值的“方便看”图后再把它用于数值比较。默认选择：

- **二维线性像元/天空面**：浮点 FITS 图像（与原数组 F32/F64 类型对应）+ 符合所在阶段的 WCS/HEALPix 意义的头/同名 JSON metadata；可以写选定的 cutout，不必默认全图。
- **HEALPix tile**：保留 tile order、NESTED ipix、512 local FITS row-major 手性和本地/球面像元索引映射；单 tile 的数值若写普通 FITS 必须在 metadata 标出它**不是一个新的标准 HiPS 产品**。
- **稀疏点/拟合残差/模型系数**：小 JSON/表结构，至少携带列名、单位、空间定义、数据顺序与 frame_id；不能只保存未标注列含义的二进制 double 向量。
- **掩膜/覆盖**：如会影响 NaN/有效样本解析，保存具名 mask 及原因/有效域，并注明是否是**真实输入模型的一部分**还是开发派生统计。
- **性能时序**：JSONL；样本量过大时按 thread-local 聚合并在结束时给聚合读数，原始事件可选择性采样。

文件始终保存原始数值与显式 `dtype`、`shape`、`endianness`、`BUNIT`/units、`frame_id`、`tile order/ipix`、`coordinate_frame`、`validity_policy` 和所在算法状态。若做 **不可逆降采样/统计摘要**，明确 `representation=summary`，不能冒充原始像素逐一快照。FITS/WCS 的头信息如无法准确生成，应写旁挂元数据并标明未提供完整 WCS，**不得编造天区中心与变换矩阵**。

## 7. 唯一工作区、命名与 debug manifest

典型开发目录（`<dev_root>` 由现有 AIO/runtime 生成，必须与正式产品树隔离）：

```text
<dev_root>/<run_id>/
  debug_manifest.json
  snapshots/
    <probe_id>/<frame_or_component>/<tile_or_roi>.<fits|json>
    <probe_id>/<frame_or_component>/<tile_or_roi>.meta.json
  perf/
    events.jsonl
    summary.json
```

内部路径规范：probe_id 是白名单枚举，frame/tile 只用可安全编码的短标识，禁止用户输入构造 `../`、绝对路径、符号链接逃逸。重复写相同探针身份时必须增加确定的 sequence/attempt ID，不覆盖先前采集的版本。诊断文件写到私有暂存名、写完计算 hash/size 再原子改名；掉电/取消留下 incomplete 状态，不冒充成功样本。

`debug_manifest.json` 建议结构（**拟议科学旁挂元数据，不是项目正式 run_manifest schema**）：

```json
{
  "schema": "acsd.dev-diagnostics/v1",
  "non_scientific_product": true,
  "run_id": "sample_run_id",
  "build": {"dev_mode": true, "git_commit": "<actual_commit>", "source_digest": "<actual_digest>"},
  "selection": {"snapshot_enabled": true, "perf_enabled": true},
  "entries": [{
    "probe_id": "mosaic.weight.surface_precision",
    "stage": "mosaic",
    "producer": "actual.module.operation",
    "frame_id": "actual_frame_id",
    "tile": {"order": 10, "ipix": 1234},
    "semantic": "surface_brightness_inverse_variance",
    "unit": "sr^2 / selected_flux_unit^2",
    "dtype": "float64",
    "shape": [128, 128],
    "coordinate_frame": "HEALPix_NESTED",
    "upstream_product": "actual_source_fingerprint",
    "file": "snapshots/.../actual.fits",
    "sha256": "<computed_hash>",
    "status": "WRITTEN"
  }]
}
```

以上例子的 `unit` 只是**具名示意**，真实单位必须按所选 BUNIT 和物理传递因子填写。下划线占位字段不得被程序拿去写成事实。允许状态 `WRITTEN`、`NOT_SELECTED`、`NOT_SEEN`、`SKIPPED_BUDGET`、`IO_ERROR`、`CANCELLED`、`INVALID_VIEW`，每类都记录数量与原因；未执行的节点不能伪装成成功写出。

正式 science run manifest 可以可选记录开发诊断路径的**引用**，但不得把 snapshot 文件当成 phase2 mosaic 子产品、phase3 输入数据或签名的正式 artifact。诊断数据默认不上传 Git，也不上传 /testdata 原始大型数据。运行目录与隐私脱敏规则见 LOG_AND_ERROR.md。

## 8. 性能事件记录语义与格式

复用现有性能探针的 `ACSD_PROBE_SCOPE`、`ACSD_PROBE_COUNT`、`ACSD_PROBE_GAUGE` 等公共 API；目标由 ACSD_DEV_MODE 统一控制是否编译。若旧实现 `ACSD_PROBES` 与 `ACSD_PROBE_LOG` 仍存在，则迁移为 dev-only 兼容内部别名/经过工作区限制的 sink，不允许在 DEV_MODE=OFF 时以独立旧环境变量激活。

性能事件（建议 `acsd.dev-perf/v1`）最小字段：

| 字段 | 科学/工程定义 |
|---|---|
| schema、run_id、stage、probe_id、kind | 跨分支可对比的稳定身份 |
| ts_utc、t_start_steady_ns 或 duration_ns | 墙时只是展示，耗时只来自稳态/单调计时 |
| thread_id、worker_id、node_id、parent_span_id（有则给） | 并发归因，可缺则记 null，不能猜 |
| wall_ns、cpu_ns（可用时）、count/bytes/alloc | 显式单位，大小单位为 bytes 而非混合 MB/MiB |
| backend、n_pixels/n_drops/n_stars/n_samples | 作性能对比的真实工作量 |
| probe_status、dropped_events、flush_overhead | 已丢失的探针样本不能当零耗时 |

最小 JSONL 示例：

```json
{"schema":"acsd.dev-perf/v1","run_id":"sample_run_id","stage":"mosaic","probe_id":"mosaic.snr.reconstruct_tile","kind":"span","thread_id":"worker_2","node_id":"p2.snr","wall_ns":3225000,"n_queries":262144,"backend":"cpu","probe_status":"RECORDED"}
```

多线程 flush：线程本地缓冲、批量写，统一 AIO sink；最多在有界 flush 点加锁，绝不在每个 leaf 内写文件。探针自身的 wall/CPU 开销单独计数。环境中没有实际采集 `cpu_ns` 就不应写虚假的零。

系统 CPU 利用率、进程 CPU 时间、I/O queue wait、页面缓存命中/物理读、各 worker busy/idle 是不同量；不得拿 `/proc/stat` 系统 iowait 归因成 ACSD 自己的块 I/O 等待。输出的 summary 需要包含运行机器、处理帧数、tile/leaf 规模、线程租约和实际可用 CPU；中位数/分位数说明按事件还是按工作量加权。

## 9. 可编译的 API 形态约束（不是当前已实现函数签名）

伪接口示意（用现有 acsd/probe.h 作为性能主入口；Snapshot API 由 Agent 依现有 AIO/ABI 分层实现）：

```cpp
void compute_weight_tile(const Input& input, MutableOutput& output) {
#if ACSD_DEV_MODE
  ACSD_PROBE_SCOPE("mosaic", "weight.compute_tile");
#endif
  compute_scientific_weights(input, output);     // 两档调用完全相同
#if ACSD_DEV_MODE
  ACSD_DEV_SNAPSHOT_VIEW("mosaic.weight.surface_precision",
                         output.readonly_view()); // 示例宏，待实现
#endif
}
```

代码约束：
- 宏为完整语句，并保证 `ACSD_DEV_MODE=0` 下**不求值传入表达式**，尤其禁止额外为 dump 调用昂贵的 `reconstruct_dense()`；同一个输出缓冲在编译 OFF 时没有额外引用/构造/拷贝；
- 借用 view 的数据所有权、异步复制与生命周期必须有明确合同；不得将共享 mutable buffer 地址异步延迟使用；
- 不允许在探针中偷偷改变科学数据的精度档位、随机种子、迭代次数、浮点舍入模式、线程数、tile 顺序或 C++ exception 传播；
- 生产模块可只填 POD 数据与标签，图像/FITS/JSON 编码均由基础设施决定；禁止 C++ 科学模块直接创建任意文件路径；
- 逐像元热点的 detailed event 应按确定性采样或 per-thread 聚合，不因探针采集本身超过算法耗时而误判优化方向；
- 诊断异常不穿透科学 API；普通产品/正式日志错误仍使用正式错误模型。

## 9A. 现有实现的独立审查更正：观测身份与非阻塞写盘

> 来源：实验/engineering-evidence/acsd-science-docs-p5/SCIENCE_V2_AGENT_INDEPENDENT_REVIEW_F8C011BAD.md 的 F02、F05、F06。这是**正向目标规范**，不宣称当前 C++ 全部实现。

### 9A.1 探针必须保存真正的被观测量

当前阶段二 writer 的内部量 flux_sum = signal × covered_area，是归一前的累加分子，不是面亮度 signal。若 probe_id 为 mosaic.integration.signal_tile，其值应是最终 signal = flux_sum / covered_area（或同义上游 signal，须与 HiPS 读回值对拍）。**禁止**把 flux_sum 直接标为 ADU/sr 和 signal。若三个量都要观察，明确分别命名 mosaic.integration.flux_sum、mosaic.integration.covered_area（单位 sr）、mosaic.integration.signal_tile（面亮度单位），说明无覆盖 NaN、真零与有限负数，检查实际 NESTED ↔ HiPS FITS-local 手性转换、F32 量化和除法次序。

阶段一 normalize.noise.reference_snr 也必须依据真实 snr_caliber、gain、F_ref、PSF/noise model 写动态 metadata：upper_bound_no_gain 是源散粒项未计算的条件上界，不是绝对参考通量方差。frame_snr 快照与实际稀疏空间控制点 catalogue **不能混同**。

### 9A.2 快照 I/O 不应在全局锁内堵住科学热点

当前 dev_diagnostics.cpp 在持有全局 mutex 时执行 FITS/JSON 写盘和 SHA-256，mosaic 的 tile 循环同步调用，违反已有“有界低干扰”目标。统一实现应当：

1. 很短的临界区只进行 selector/预算/sequence 预留；没有选中或 byte-budget/queue 已满立即登记 SKIPPED_BUDGET，不等待磁盘。
2. 在数据 view 有效时，做有界只读复制并将**有所有权**的缓冲交独立低优先 I/O worker；任何异步借用必须显式证明生命周期，不允许 use-after-free。
3. 经 AIO 写私有临时数据、完整 sidecar、实际 SHA-256 并原子发布；**都成功才记录 WRITTEN**。任一必要元数据写失败，记录 IO_ERROR 和未完成状态，不以主文件存在替代一个可解读的快照。
4. 多工作线程的 dev writer 自身异步队列、流量及内存均有界；取消/科学错误时有明确的 bounded drain/flush 与丢失读数，不损害正式产物。
5. 同一进程内连续 begin_run/finish_run 必须重置全局 session、预算、run_id、计数和输出工作区，避免上轮诊断串入下一轮；测试跨线程退出和科学内存复用。
6. ON+none、ON+perf、ON+snapshot 与 OFF 同科学算法结果；其独立开销、IO waits、写入字节数分别实测。**异步不能保证零开销**，需明确限制与真实测量。

本项不改变单信号 HiPS 的最终产品合同，也不把诊断异常当成科学处理的失败或流程门禁。

## 10. Agent 实施时的对照和证据

优先交付实际编译与行为事实，而非“开关已添加”四个字：

1. **编译配置**：CMakeCache 能证明只有 ACSD_DEV_MODE 是公开诊断构建控制，旧独立 ACSD_PROBES 无法单独激活；两档的 target compile definitions 对所有有关目标一致。
2. **静态/符号差异**：OFF 包不链接 dev snapshot/perf 实现、不采开发时钟、不访问 ACSD_PROBE_LOG/DEV 变量；ON 才有诊断符号。
3. **科学等价**：相同输入 ON/OFF 与 ON(no dump) 的实际信号、SNR 控制点、HiPS 坐标、阶段二单信号与最终 FITS 相同或给出严格浮点原因和数值误差；禁止因诊断选项走原始帧/改用另一缓存。
4. **快照真实性**：选中某个真实 P1 控制点/Drizzle tile、P2 V_F/T/w_B、P5 背景差模和 P3 输出像元，数值确实是调用点的那个缓冲，而非图像终态复制；单位/坐标/状态/来源实际可读。
5. **生命周期并发**：tile buffer 被重用、工作线程退出、取消、诊断 I/O queue 过载、诊断磁盘满等，正式图像不损坏、不死锁；被抛弃事件留下统计。
6. **性能开销**：在相同机器/科学配置/工作量/编译优化下，记录 OFF、ON(none)、ON(perf)、ON(selected snapshots) 的 CPU/墙钟/写盘量与统计散度。快照落盘慢不代表科学内核慢。
7. **正式输出隔离**：DEV ON 产生的 snapshots/perf/debug_manifest 只在 dev workspace；不进入 Phase2 单 signal HiPS、不进入 Phase3 FITS、也不变成下一阶段科学输入。
8. **失败域**：诊断写失败只记录诊断问题；科学必需文件损坏仍是科学/IO 错误，不能混淆。
9. **参考旧路径**：ACSD_DEV_INTERMEDIATE_CACHE、ACSD_DEV_PROBES、ACSD_DEV_CHECKPOINT、ACSD_PROBES、ACSD_NODE_TRACE/LEASE_TRACE/P1CAP_TRACE、ACSD_UPM_PROFILE 与 ACSD_*_FAULT 逐项给出迁移/隔离结果，不必为旧 ABI 字段误删科学功能。

实验按科学问题安排，不设审批式门禁。前台负责本标准后续修订；Agent 可提出有代码/实测支撑的建议，但不得自行发明第二套规范或修改统一科学模型。