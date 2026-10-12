# 阶段一产物面收敛与内存传递：清点与设计

本轮为**只读清点 + 设计**，不写实现。清点结果写入本文件（实验单元留档）；未改 `lib/**`、`docs/**`、`eng/**`，无 git 操作。

清点口径按负责人的两句话定：
> 「这些调试输出都要走开关，输出到 JSON 里。标准模式下真正的产物只有逐帧 HiPS 和一个给阶段二输入用的 JSON。」
> 「像开关什么的也只是中间产物额外打印，没有额外路径。」

---

## 1 读了哪些文档，以及从中得到的关键口径

| 文档 | 关键口径（判据 / 合同 / 边界） |
|---|---|
| `docs/ACSD_DESIGN.md` | §1.2 三命令各自独立启动、独立调度器、独立内存管线；**阶段之间只通过磁盘 HiPS 与 manifest 交换数据**。§4 `normalize` 单帧标准化使命与输出合同。§5 `mosaic` 消费面。§13 **不设门禁**：门禁式机制删除或改为显式诊断读数，不得阻塞正常流程。 |
| `docs/engineering/architecture/DEV_MODE.md` | **本任务的核心正本**。§5「节点之间通过内存管线传递像素面与辅助量」；§7 开发模式边界是**构建期**而非运行期（同一能力要么在二进制里要么不在）；**§34「逐帧 JSON 是标准产物」**：`p1_wcs`/`p1_sources`/`p1_phot`/`p1_psf`/`p1_snr` 不是调试缓存而是**节点之间的契约载体**（调度器据以绑定依赖边），发布版必须产出它们；§40/§42 门控分两批，第一批（清理面/坏列掩膜/测光施加面写出侧、检查点、探针）与第二批（`calibrated → cos`、`photoapplied → drz` 两条边改内存块）**必须同批落地**；§56 `block_flow.cpp` 的 `set_input`/`set_input_f64`/`product`/`run_unit` 在 `lib/` 全树**零消费者**；§58 只闸写侧会切掉读侧输入、只闸读侧会在取数处必然失败；**§60 发布构建可用性「未达成」，现状 = 写侧尚未闸、读侧已闸**。 |
| `docs/detail/normalize/pipeline.md` | §3 节点与先后关系（8 节点链）；§8.2 Phase1 只产出帧级 SNR、稀疏绝对 SNR、`point_information`，**不产生不消费任何叠加权重**；§10 **一个原子目录至少包含** signal / variance-ivar / support / coverage-validity / PSF / photometric response / WCS / 背景噪声模型 / `point_information` / `depth_m5` / source catalog / drizzle correlation / **product manifest（schema、算法模块 provider、完整 SHA、输入配置哈希、单位、参考尺度、近似与降级）**；§10 说明 `p1_products.json` 自报 `storage_form`/`index_path`/`index_sha256`/`archive_sha256`；**§11 验收：「产品从磁盘独立重开后足以执行 Phase2，不依赖进程内状态」**。 |
| `docs/engineering/contracts/CONFIG.md` | §配置面三登记**另一条**阶段一调用链（orchestrator `stage1.*.enabled` 门 → `Orchestrator::run_stage_*`），与本仓 `module_adapters` 的 8 节点链**并存**；配置门与诊断字段是写 config 时实际要敲的键。 |
| `docs/engineering/standards/CACHE.md` | 只登记**生产缓存**，入册须具备「容量 + 身份 + 失效 + 线程模型」四要素；清单里只有 UPM dense cache、Gaia 查询缓存、Drizzle geometry cache 三项 ⇒ **阶段一的帧级平面中间件未被登记为缓存**。 |
| `docs/science/unified/DATA_SEMANTICS.md` | §3.2 三个基本对象的语义；§3.7 **帧身份**是科学载荷身份的 64 位截断哈希，输入含 signal/coverage tile 像素与信噪比目录内容，**与路径、重命名、换根目录无关**；§4.4 **溯源最小集**（缺键即拒绝）：`product.type_id`、`schema_version`、`software_sha`、`run_id`、`input_product_hashes`、`config_hash`、`units.{bunit,pixel_semantics,pixel_area_power,target_pixel_area}`、`coordinate.frame`、`sampling`、`algorithm_ids`、`module`、`provider`、`approximations`、`degradations`、`normalization_version`、`correlation_summary`、`flux_conservation_factor`、`k_corr.{definition,value,domain,calibration}`、`generated_utc`、`output_hash`、`unavailable.{flag,reason,scope}`；§4.5 失败一律显名。 |
| `docs/detail/common/unified_model.md` | §2 数据对象与字段歧义消解（13 个 canonical 对象）；§2.1 参考通量基准 `F_ref`；§3 三类配置严格分离（阶段配置 / 机器画像 / 运行 manifest）；§4 精度归属；**§5 块生命周期**。 |
| `docs/detail/registry/acsd.phase1.*.md`（11 页） | 各节点端口表与产物：`calibration`（calibrated 面）、`cosmetic`（cleaned + 坏列掩膜）、`star-detection`、`star-psf`（`p1_sources.json`、`p1_psf.json`）、`wcs-platesolve`（`p1_wcs.json`）、`photometry`（`p1_phot.json`、`p1_flux.json`）、`noise-snr`（`p1_snr.json`）、`drizzle`（`p1_stack.json` + HiPS）、`writer`（`p1_final.json`、`p1_products.json`）、`hips-writer`、`session`。 |
| `lib/infrastructure/pipeline/module_ports.registry.json` | 阶段二端口声明：`:944` 注明 `p1_products.json` 的 **`hips_paths` 直接作为 mosaic 的输入路径来源**；`:1071` 注明阶段二**只取逐帧 HiPS 产品树内的 `<frame_key>/p1_stack.json`** 的 `finest_input_arcsec` 导出天光面节点几何，并按「**跨阶段唯一载体 = HiPS 产品树**」归入 `frame_hips` 块。 |
| `AGENTS.md` | §7 模块经内存管线读块、写新块、消耗旧块；不私藏大块数据长期副本；**不写未声明文件**。§13 不设门禁、**同一件事只允许一份实现**、规范优先于最小改动（允许并鼓励必要重构）。 |

---

## 2 T1 逐项清点：标准模式下一次阶段一运行写出的文件

### 2.1 实测基准

以仓内一次真实标准模式运行的产物树为基准（`run/AUTONOMOUS-01/smoke/out/p1_phot_smoke/`，单帧）：

```text
<output_dir>/
├── calibrated_<base>.fts                    67 121 280 B ≈ 64 MiB/帧   ← 帧级平面中间件
├── photoapplied_<base>.fts                  67 121 280 B ≈ 64 MiB/帧   ← 帧级平面中间件
├── p1_flux.json                                    473 805 B
├── p1_phot.json                                      2 096 B
├── p1_products.json                                  1 301 B
├── p1_psf.json                                         457 B
├── p1_snr.json                                     405 081 B
├── p1_sources.json                                 993 433 B
├── run_context.json                                    231 B
├── astrocs_run_<hash>.json                          10 321 B
├── alloc_report.json / alloc_samples.csv / resource_summary.json /
│   resource_timeseries.csv / worker_balance.csv
├── graph/{static_graph.json,.dot,.svg, observed_graph.json,.dot,.svg,
│          l0_graph.json,.dot, graph_sidecar.json, observed_trace.json}
└── <frame_key>/                                        ← 逐帧 HiPS 产品树
    ├── manifest.json                                   671 B
    ├── p1_wcs.json                                  22 117 B
    ├── p1_final.json                                 1 144 B
    ├── p1_stack.json                                   783 B
    └── signal/ support/ variance/ ivar/ [snr/]       每 tile 1 054 080 B（512² FP32 + 头）
```

### 2.2 逐项表

**A 组：逐帧 HiPS 产品树**（`<output_dir>/<frame_key>/`）

| 路径 | 写入点（文件:行） | 读取点（文件:行） | 阶段二需要 | 大小量级 | 判定 |
|---|---|---|---|---|---|
| `<frame_key>/signal/**` | `aio_hips_writer.cpp:2515`（`finalize_image_product`）；生产入口 `module_adapters.cpp:7892` `p1_op_drizzle` → `astro_sphere_sink` | 阶段二 `lib/algorithms/coverage/src/sampler.cpp:543`（`AIO_HIPS_RD_SUPPORT` 同族）等 AIO 读；`p2_frame_id` 以 signal tile 为身份载荷 | **是** | 每 tile ≈1 MiB | **产物** |
| `<frame_key>/support/**` | 同上 `:2521` | 阶段二 `sampler.cpp:552` | **是** | 每 tile ≈1 MiB | **产物** |
| `<frame_key>/variance/**`、`<frame_key>/ivar/**` | `aio_hips_writer.cpp` 对应子产品；`p1_op_drizzle` 落盘 | 阶段二 `sampler.cpp:831`（`AIO_HIPS_RD_IVAR`） | **是** | 每 tile ≈1 MiB | **产物** |
| `<frame_key>/snr/**`（目录子产品，`tsv`） | `aio_hips_writer.cpp:2195` `finalize_snr_product`；生产入口 `lib/algorithms/drizzle/hips/src/module_entry.cpp:965-966` `aio_hips_write_snr_points` | 阶段二 `sampler.cpp:813-828`（`AIO_HIPS_RD_SNR` + `aio_hips_read_snr_catalog`） | **是** | 随源数 | **产物** |
| `<frame_key>/manifest.json` | `aio_hips_writer.cpp:2593` 段（产品集完成标记） | 阶段二/发布层（`pipeline.md:216` 记 `storage.form_source`） | **是** | <1 KiB | **产物** |
| `<frame_key>/p1_stack.json` | `module_adapters.cpp:8803`（`fdir + "/p1_stack.json"`，`p1_op_drizzle` 内；`out_path` 定义在 `:8765`） | **阶段二两处真实读取**：`module_adapters.cpp:10344`（`p2_op_upm_fit`，取 `finest_input_arcsec` 导出天光面节点几何）、`:13214`（`p2_op_integrate`，产品族声明）；阶段一 `p1_op_writer` 见 `:9019` | **是** | <1 KiB | **产物** |

**B 组：逐帧 JSON**

| 路径 | 写入点 | 读取点 | 阶段二需要 | 大小量级 | 判定 |
|---|---|---|---|---|---|
| `<frame_key>/p1_wcs.json` | `module_adapters.cpp:4934`、`:5441`（`p1_op_wcs`）；未解算分支 `:5282` | 阶段一内**三段读回**：`p1_op_star_psf_impl:3862`、`p1_op_photometry:6017`、`p1_op_drizzle:8175`；`p2_op_*` 无读取点 | 否（阶段二直接消费 HiPS 的 WCS 属性） | ≈22 KB | **调试输出**（按单一判据）；但它同时是**阶段一内部**的 typed 边载体（见 §2.3） |
| `<frame_key>/p1_final.json` | `module_adapters.cpp:9327`（`final_path` 定义 `:9271`，`p1_op_writer`） | **lib 内零读取点**（其余命中均为注释） | 否 | ≈1 KiB | **调试输出** |
| `<frame_key>/p1_photscale.json` | **未见写出点**（`lib/` 内只有读取） | `p1_op_photometry:5638`/`:6240` 作为「上游/外部标定通道」**读入**（`DATA-P1-PHOTSCALE-001`） | — | — | **不是阶段一产物**（是可选输入通道）；本轮实测运行中不存在 |

**C 组：运行级（root）JSON**

| 路径 | 写入点 | 读取点 | 阶段二需要 | 大小量级 | 判定 |
|---|---|---|---|---|---|
| `p1_sources.json` | `module_adapters.cpp:4383`（`src_path` 定义 `:4354`，`p1_op_star_psf_impl`） | 阶段一内三段读回：`p1_op_photometry:5479`、`p1_op_noise:7176`、`p1_op_drizzle:8382` | 否 | ≈970 KB | **调试输出**（按单一判据）；同为阶段一内部 typed 边载体 |
| `p1_phot.json` | `module_adapters.cpp:6742`（`prov_path` 定义 `:6670`，`p1_op_photometry`） | 阶段一内三段读回：`p1_op_noise:7388`、`p1_op_drizzle:8012`、`p1_op_writer:8963` | 否 | ≈0.9–2 KB | **调试输出**（同上） |
| `p1_snr.json` | `module_adapters.cpp:7808`（`out_path` 定义 `:7764`，`p1_op_noise`） | `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:391`（阶段一 drz 节点内读回帧级 SNR 键） | 否 | ≈400 KB | **调试输出**（同上） |
| `p1_flux.json` | `module_adapters.cpp:5619`（`out_path` 定义 `:5617`，`p1_op_photometry`） | **lib 内零读取点**（`module_adapters.cpp:1007` 注释自陈「本节点无读取点」；`runtime_client.cpp:218` 注释「**幻边**…已删」） | 否 | ≈463 KB | **调试输出** |
| `p1_psf.json` | `module_adapters.cpp:4383`（`psf_path` 定义 `:4355`，`p1_op_star_psf_impl`） | **lib 内零读取点**（`module_adapters.cpp:5944`、`frame_photometry_fit.h:39` 均为注释；注册表自陈「生产链路零消费者」） | 否 | ≈0.4 KiB | **调试输出** |
| `p1_products.json` | `module_adapters.cpp:9411`（`products_path` 定义 `:9401`，`p1_op_writer`） | **lib 内零读取点**（其余命中为注释与注册表声明） | **注册表声明「是」但实现为「否」**（见 §2.3） | ≈1.3 KiB | **待裁决**（名义上的阶段二输入，当前无消费者） |

**D 组：帧级平面中间件（FITS）**

| 路径 | 写入点 | 读取点 | 阶段二需要 | 大小量级 | 判定 |
|---|---|---|---|---|---|
| `calibrated_<base>.fts` | `p1_write_fits_atomic` 调用点 `module_adapters.cpp:2823` 段（`p1_op_calibrate`） | `p1_calibrated_path`（`:1550`，**12 处调用**）→ cos / wcs / psf / phot / noise / drz 各节点 | 否 | **≈64 MiB/帧** | **调试输出**（中间件） |
| `cleaned_<base>.fts` | `module_adapters.cpp:3230`（`p1_write_fits_atomic`，`p1_op_cosmetic`） | `p1_cleaned_input_path`（`:1692`，4 处调用） | 否 | ≈64 MiB/帧 | **调试输出** |
| `badcol_<base>.fts` | `module_adapters.cpp:3252` | **仓内只写不读**（`DEV_MODE.md:40` 自陈） | 否 | ≈64 MiB/帧 | **调试输出** |
| `badcolsrc_<base>.fts` | `module_adapters.cpp:3268` | **仓内只写不读** | 否 | 同量级 | **调试输出** |
| `photoapplied_<base>.fts` | `module_adapters.cpp:6470`（`p1_write_fits_atomic`，`p1_op_photometry`） | `p1_photoapplied_path`（`:1706`，4 处调用）→ drz「逐帧判决分支要求消费测光施加面」（`DEV_MODE.md:42`） | 否 | **≈64 MiB/帧** | **调试输出** |

**E 组：记账 / 资源 / 图**

| 路径 | 写入点 | 读取点 | 阶段二需要 | 判定 |
|---|---|---|---|---|
| `run_context.json` | `module_adapters.cpp:16262`（B2-A10 唯一生成路径） | `module_adapters.cpp:15555` 阶段三读；`pipeline.md` 记运行上下文 | 否 | **记账（运行上下文）** |
| `astrocs_run_<hash>.json` | 运行级 manifest（CLI 侧） | 验收/审计 | 否 | **记账（运行 manifest）** |
| `alloc_report.json`、`alloc_samples.csv`、`resource_summary.json`、`resource_timeseries.csv`、`worker_balance.csv` | `lib/infrastructure/cli/commands.cpp:1088-1110` 段（MON-001/MON-002 运行收尾）；记录器 `lib/infrastructure/cli/resource_recorder.h` | 验收/性能档案 | 否 | **记账（资源证据）** |
| `graph/{static_graph.*, observed_graph.*, l0_graph.*, graph_sidecar.json, observed_trace.json}` | `lib/infrastructure/cli/commands.cpp:650`（`static_graph.json`）、`:727`（`observed_trace.json`）、`:746`（`graph_sidecar.json`） | 审计/可视化 | 否 | **记账（调度图与执行轨迹）** |

### 2.3 判定汇总与判据张力（必须请负责人裁决的一点）

按派单的**单一判据**（阶段二是否读它）汇总：

- **产物**：逐帧 HiPS 产品树全部子产品 + 逐帧 `manifest.json` + 逐帧 `p1_stack.json`。**运行级 JSON 无一入选**——`p1_products.json` 名义上是阶段二输入，但**在 `lib/` 内零读取点**；阶段二实际的帧列表来自**配置** `doc["hips_paths"]`（`p2_op_*` 各处的 `hips_paths`），几何来自 HiPS 树内的 `p1_stack.json`。
- **调试输出**：`p1_wcs`/`p1_final`/`p1_sources`/`p1_phot`/`p1_snr`/`p1_flux`/`p1_psf` 七个 JSON + `calibrated_`/`cleaned_`/`badcol_`/`badcolsrc_`/`photoapplied_` 五个平面件。

**张力（登记，不下结论）**：`DEV_MODE.md:34` 明文写「逐帧 JSON 是**标准产物**……不是调试缓存，而是**节点之间的契约载体**：调度器把 `wcs`、`photprov`、`sources`、`snr` 声明为类型化输入端口，据此绑定依赖边，保证例如解算先落盘再 drizzle。**发布版必须产出它们**，否则依赖边无从绑定、节点顺序无保证。」

⇒ 同一批 JSON，按派单判据是「调试输出」，按 `DEV_MODE.md` 是「发布版必需的契约载体」。**两者不能同时成立**，除非把依赖边的载体从「磁盘文件」换成「内存块」——这正是 `DEV_MODE.md:42/§58` 的**第二批改造**。也就是说：**「接口用文件」与「文件是产物」是同一件事的两面**；判据要从「阶段二是否读」升级为「**是否必须落盘**」，而后者由「依赖边是否已改内存块」决定。

同理，`p1_products.json` 的处境是第三种：注册表 `:944` 与 `pipeline.md:216` 都把它写成阶段二的输入来源，但**没有任何代码读它**——它要么是「写好了但没接线」，要么是「设计上阶段二应该读它而现在用配置代替」。这一点决定它属于「保留并接线」还是「删除」，**须裁决**。

---

## 3 T2 内存传递缺口：写到磁盘又被同一进程读回的地方

**前提（已核实）**：阶段一的 8 节点链在**同一进程**内被调度执行。链定义见 `module_adapters.cpp:2032`：

```text
static const char* kChain[] = {"cal", "cos", "wcs", "psf", "phot", "snr", "drz", "wr"};
```

节点函数 `p1_op_*` 全部在 `module_adapters.cpp` 内（`:2500` cal、`:2976` cos、`:3827/4430` psf、`:4752` wcs、`:5472` phot、`:7168` snr、`:7892` drz、`:8941` wr）。因此下列每一处都是「**同进程写盘 → 同进程读回**」的往返。

| # | 数据 | 写入点（文件:行） | 读回点（文件:行） | 能否直接内存传 | 判断依据 |
|---|---|---|---|---|---|
| **R1** | `calibrated_<base>.fts`（≈64 MiB/帧） | `module_adapters.cpp:2823`（cal） | cos/wcs/psf/phot/noise/drz 经 `p1_calibrated_path`（`:1550`，12 处调用） | **能**（这是 `DEV_MODE.md:42` 的 `calibrated → cos` 边） | 两节点同进程、同一块像素面；`block_flow` 能力已存在但零消费者（`:56`）。**体量最大的一项** |
| **R2** | `cleaned_<base>.fts`（≈64 MiB/帧） | `:3230`（cos） | 经 `p1_cleaned_input_path`（`:1692`，4 处调用） | **能** | 同 R1；cos 产物本就是 cal 面的改造版，块生命周期可覆盖 |
| **R3** | `photoapplied_<base>.fts`（≈64 MiB/帧） | `:6470`（phot） | drz 经 `p1_photoapplied_path`（`:1706`，4 处调用） | **能**（`DEV_MODE.md:42` 的 `photoapplied → drz` 边） | 同 R1；`DEV_MODE.md:42` 明写该边「当前走输出目录的文件约定，关闭缓存后……会直接失败」 |
| **R4** | `p1_wcs.json`（≈22 KB/帧） | `:4934`/`:5441`（wcs） | psf `:3862`、phot `:6017`、drz `:8175` | **能** | 同进程三次读回；`DEV_MODE.md:34` 已把它声明为 typed 边（`artifact:p1_wcs`） |
| **R5** | `p1_sources.json`（≈970 KB） | `:4383`（psf） | phot `:5479`、noise `:7176`、drz `:8382` | **能** | 同上（typed 边 `artifact:p1_sources`） |
| **R6** | `p1_phot.json`（≈1–2 KB） | `:6742`（phot） | noise `:7388`、drz `:8012`、wr `:8963` | **能** | 同上（typed 边 `artifact:p1_phot`） |
| **R7** | `p1_snr.json`（≈400 KB） | `:7808`（noise） | `astro_sphere_sink.cpp:391`（drz 内） | **能** | 同上（typed 边 `artifact:p1_snr`）；注意读点在 **drizzle 算法库**内，不在编排层 |
| **R8** | `p1_stack.json`（<1 KB） | `:8803`（drz） | **跨阶段**：`p2_op_upm_fit:10344`、`p2_op_integrate:13214` | **不能**（跨命令/跨进程） | 阶段二是独立的 `acsd mosaic` 进程；`pipeline.md:228` 要求「产品从磁盘独立重开后足以执行 Phase2」⇒ 必须落盘 |

**不是往返、只是落盘留档**（写后无人读回，或只有本节点内自用）：

- `badcol_<base>.fts`、`badcolsrc_<base>.fts`：**仓内只写不读**（`DEV_MODE.md:40`），无读回点 ⇒ **纯额外输出**。
- `p1_flux.json`、`p1_psf.json`、`p1_final.json`：`lib/` 内零读取点 ⇒ 纯额外输出。
- `p1_products.json`：零读取点 ⇒ 目前纯额外输出（但其**设计意图**是阶段二输入，见 §2.3）。
- E 组记账/资源/图：不参与计算，写后由外部（验收/审计）读取。

**影响量化（先量，不下结论）**：

- 磁盘体量：R1–R3 三项平面件按 **≈64 MiB/帧** 计，单帧一次运行即写出 **≥128 MiB**（实测该运行同时存在 `calibrated_` 与 `photoapplied_` 各 67 121 280 B）；49 帧量级下为 **≈3 GiB/次运行**（仅这两项），全部落盘中间件还要加上 `cleaned_`/`badcol_`/`badcolsrc_`。
- 往返次数：R1（12 处读）× R2/R3（各 4 处）× R4–R7（各 3–4 处）——读回是**每次下游节点取数都发生**，不是一次性。
- 结论边界：本轮**只定位与量化**；「哪些必须改」留给 §5 与负责人裁决。

---

## 4 T3 调试开关现状核对

### 4.1 声明面

四个开关是 **CMake 选项**（构建期），定义与传播点：

| 开关 | 定义 | 传播 | 代码消费者 |
|---|---|---|---|
| `ACSD_DEV_MODE` | `CMakeLists.txt:29`（默认 `OFF`） | `CMakeLists.txt:1442-1444` → `ACSD_DEV_MODE=1`（`PUBLIC`，目标 `acsd_module_adapters`） | **零**（`lib/` 内无命中） |
| `ACSD_DEV_INTERMEDIATE_CACHE` | `CMakeLists.txt:30`（默认 = `ACSD_DEV_MODE`） | `CMakeLists.txt:1445-1447` | **2 处**：`module_adapters.cpp:1553`、`:1648` |
| `ACSD_DEV_CHECKPOINT` | `CMakeLists.txt:31` | `CMakeLists.txt:1448-1450` | **零** |
| `ACSD_DEV_PROBES` | `CMakeLists.txt:32` | `CMakeLists.txt:1451-1453` | **零** |

实测默认构建（`build/CMakeCache.txt`）**四个开关均未定义** ⇒ 当前二进制走「`ACSD_DEV_INTERMEDIATE_CACHE` 未定义」分支。

### 4.2 逐个：它现在实际控制什么

| 开关 | 实际控制的东西（文件:行） | 是「额外输出」还是「改变执行路径」 |
|---|---|---|
| `ACSD_DEV_MODE` | **无**（只被 `CMakeLists` 读来给子开关取默认值；`lib/` 内无 `#if defined(ACSD_DEV_MODE)`） | **不适用**——当前不控制任何行为 |
| `ACSD_DEV_CHECKPOINT` | **无**（`lib/` 内零消费者；`DEV_MODE.md:28` 声明的「检查点记录每帧已完成阶段」在本仓未见实现） | **不适用** |
| `ACSD_DEV_PROBES` | **无**（`lib/` 内零消费者；`DEV_MODE.md:30` 声明的性能探针未见 `#if` 包裹的探针代码） | **不适用** |
| `ACSD_DEV_INTERMEDIATE_CACHE` | ① `p1_calibrated_path`（`module_adapters.cpp:1550-1558`）：**定义时**若 `calibrated_<base>` 缺失 ⇒ `return light;` **回退到原帧**；**未定义时** ⇒ `return cand;`（那个不存在的产物路径）。② `p1_prerun_calibrated_check`（`:1642-1663`）：**定义时**⇒把 `fell_out`/`why` 写进 manifest 后**继续跑**；**未定义时**⇒ `Result<void>::fail(...)` **fail-closed** | **改变执行路径（两处，均为缺陷）** |

### 4.3 与负责人口径的对照（逐条列出缺陷）

负责人口径：**开关只允许决定「中间产物是否额外打印/落盘」，不得制造第二条路径。**

| # | 缺陷 | 证据 | 后果 |
|---|---|---|---|
| **D1** | 开关切换了**取数来源**（产物面 vs 原帧） | `:1553-1557` —— 定义时 `return light`（原帧），未定义时 `return cand`（产物路径） | 同一份 config 在两种构建下**消费不同的数据**；「回退原帧」让校准结果不参与下游，属**第二条数据路径** |
| **D2** | 开关决定了**运行是否继续**（fail-closed vs 留痕继续） | `:1648-1660` —— 定义时写 `calibrated_fell_out` 后 `continue`，未定义时 `return fail` | 开关**控制流程存续**，属门禁式机制（`AGENTS.md` §13「不设门禁」）；且同一输入两种构建下**一个失败一个成功** |
| **D3** | 三个子开关与主开关**均无实现**（`DEV_MODE.md` 声明的检查点与探针能力在本仓不存在） | `CMakeLists.txt:1442-1453` 定义了 4 个宏，`lib/` 内只有 `ACSD_DEV_INTERMEDIATE_CACHE` 有消费者 | 文档声明的「开发期能力」与实现不一致；读文档的人会以为有检查点/续跑与探针 |
| **D4** | 写侧**未闸**，读侧**已闸** ⇒ 不可逆不平衡 | `DEV_MODE.md:60`；写侧 `p1_write_fits_atomic` 调用点（`:2823`、`:3230`、`:3252`、`:3268`、`:6470`）**无任何 `#if` 包裹** | 当前能跑通只因产物文件恰好存在；一旦按声明闸上写侧，`calibrated` 取数处立即 fail-closed，直到第二批改造完成 |

**补充事实**：`ACSD_DEV_INTERMEDIATE_CACHE` 是**编译期**宏，不是运行期环境变量——按 `DEV_MODE.md:7` 的口径这本身是正确的（「同一个能力要么在二进制里，要么不在」）；问题不在「构建期 vs 运行期」，而在**它改变了数据来源与流程存续**，超出了「额外输出」的授权范围。

---

## 5 T4 方案、必须动的位置、JSON 字段清单、风险

> 本节为回报与共裁决用，**不含实施**。

### 5.1 目标形态

标准模式下一次阶段一运行，磁盘上只留：
1. **逐帧 HiPS 产品树**（含其内部 `manifest.json`、`p1_stack.json`——后者按注册表 `:1071` 属「跨阶段唯一载体 = HiPS 产品树」的一部分，阶段二确要读）；
2. **一个给阶段二输入用的 JSON**；
3. 运行级记账（manifest / 资源 / 图）——非科学产物，是否随之收敛另议（见 §5.6 风险 K6）。

阶段一各节点之间的像素面与辅助量走**内存块**，不再经磁盘。

### 5.2 必须动的位置（清点，不含实施顺序）

| # | 位置 | 动什么 | 依据 |
|---|---|---|---|
| M1 | `lib/infrastructure/scheduler/src/module_adapters.cpp` `p1_op_calibrate`/`p1_op_cosmetic`/`p1_op_photometry` 的写出侧（`:2823`、`:3230`、`:3252`、`:3268`、`:6470`） | 把平面件写出改为「额外输出」语义（由单一开关控制），或直接改内存块产出 | `DEV_MODE.md:40`、§4.3 D4 |
| M2 | 同文件 `p1_calibrated_path`（`:1550`）、`p1_cleaned_input_path`（`:1692`）、`p1_photoapplied_path`（`:1706`）及其 20 处调用 | 取数改为内存块；**删掉** `#if defined(ACSD_DEV_INTERMEDIATE_CACHE)` 两条分支 | §4.3 D1/D2；`AGENTS.md` §13「同一件事只允许一份实现」 |
| M3 | `p1_prerun_calibrated_check`（`:1642`） | 从「构建期决定 fail-closed 与否」改为**单一的显式诊断读数**（记录缺失，但不因构建形态改变流程存续） | `AGENTS.md` §13；§4.3 D2 |
| M4 | `lib/infrastructure/scheduler/src/block_flow.cpp` 的 `set_input`/`set_input_f64`/`product`/`run_unit` | **接线**（当前零消费者）：把 R1/R3（`calibrated → cos`、`photoapplied → drz`）与 R4–R7 的边绑到块流 | `DEV_MODE.md:42`、§56、§58 |
| M5 | 同文件 `p1_op_star_psf_impl`（`:4383`）、`p1_op_wcs`（`:4934`/`:5441`）、`p1_op_noise`（`:7808`）、`p1_op_drizzle`（`:8803`/`:8765`）、`p1_op_writer`（`:9327`/`:9411`） | 各 JSON 的**写出侧**改为「额外输出」或删除（逐个定，见 §5.4） | 派单口径 |
| M6 | 同文件各 JSON 的**读回点**（R4–R7 共 11 处） | 改为内存块读取 | §3 |
| M7 | `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:391` | 帧级 SNR 键的读回改为块传（读点在算法库内，需与编排层同步） | §3 R7 |
| M8 | `lib/infrastructure/cli/runtime_client.cpp` 的 DAG 注释与边声明（`:152-236` 段） | 依赖边声明随载体变更同步（含删掉已不存在的幻边注释） | `DEV_MODE.md:34`、§13 API 文档义务 |
| M9 | 那个「阶段二输入 JSON」的**写入侧**（现在最近的对应物是 `p1_op_writer` 的 `:9411`）与**阶段二读取侧** | 见 §5.3；若要保留 `p1_products.json` 作为该 JSON，需**新增阶段二读取点**（当前零消费者） | §2.3 |
| M10 | `CMakeLists.txt:29-32`、`1442-1453` | 开关收敛为一个（`ACSD_DEV_INTERMEDIATE_CACHE` → 统一为「中间产物额外输出」），删掉零消费者的三个 | §4.2、§4.3 D3 |
| M11 | `docs/engineering/architecture/DEV_MODE.md` §34、§40、§42、§60 | 文档与实现同步（§34 的「逐帧 JSON 是标准产物」在载体改块后须改写） | `AGENTS.md` §4 文档随代码维护 |

### 5.3 那个 JSON 必须装什么

**先分清「谁读它」**。现状证据：

- 阶段二**实际**读的跨阶段内容有两类：① 逐帧 HiPS 树（AIO 读 signal/support/variance/ivar/snr）；② `<hips_paths[i]>/p1_stack.json` 的 **`finest_input_arcsec`**（`module_adapters.cpp:10344`、`:13214`）。
- 阶段二的**帧列表**来自**配置** `doc["hips_paths"]`，**不是**任何阶段一 JSON。
- `p1_products.json` 被注册表 `:944` 与 `pipeline.md:216` 写成阶段二输入来源，但**零代码读取点**。

⇒ 「那个 JSON」目前**还没有消费者**；它的字段清单因此必须以**设计意图 + 验收要求**为依据，并**新增阶段二读取点**（M9）。依据来源两条，均为正本：

**(a) `pipeline.md:216` 已规定 `p1_products.json` 自报的字段**（逐帧产品清单）：

- 逐帧：`storage_form`、`index_path`、`index_sha256`、`archive_sha256`；
- 运行级：`coverage_index`；
- 加上注册表 `:944` 的用途声明：**`hips_paths` 直接作为 mosaic 的输入路径来源** ⇒ 必须含**逐帧产品树路径清单**。

**(b) `DATA_SEMANTICS.md:231-249` 溯源最小集**（缺键即拒绝）——产品的 provenance 必须齐备：

```text
product.type_id, product.schema_version, software_sha, run_id,
input_product_hashes, config_hash,
units.{bunit, pixel_semantics, pixel_area_power, target_pixel_area},
coordinate.frame, pixel_semantics, sampling,
algorithm_ids, module, provider, approximations, degradations,
normalization_version, correlation_summary, flux_conservation_factor,
k_corr.{definition, value, domain, calibration},
generated_utc, output_hash, unavailable.{flag, reason, scope}
```

**(c) `pipeline.md:228` 的验收要求**：「产品从磁盘独立重开后足以执行 Phase2，不依赖进程内状态」⇒ 该 JSON **加上 HiPS 树**必须自足，不得依赖进程内残留。

**逐字段清单（拟，待裁决处标注）**：

| 字段组 | 字段 | 用途 / 依据 | 来源 |
|---|---|---|---|
| 运行身份 | `run_id`、`software_sha`、`config_hash` | 溯源最小集；`run_context.json` 已有（`module_adapters.cpp:16255`） | 已有 |
| 帧清单（**核心**） | `frames[]`：`frame_key`、`frame_id`（64 位截断哈希，`DATA_SEMANTICS §3.7`）、`hips_path`、`status`、`n_leaf_tiles` | 阶段二据此枚举帧集与定位产品树（现在靠配置 `hips_paths`） | **新增**（`p1_products.json` 现含逐帧条目，字段名待核） |
| 产品形态 | `storage_form`、`index_path`、`index_sha256`、`archive_sha256`、`coverage_index` | `pipeline.md:216` 已规定 | 已有 |
| 单位与坐标 | `units.{bunit,pixel_semantics,pixel_area_power,target_pixel_area}`、`coordinate.frame`、`pixel_semantics`、`sampling` | 溯源最小集；阶段二 `DATA_SEMANTICS` 单位层判据 | 部分已有 |
| 算法与模块 | `algorithm_ids`、`module`、`provider`、`approximations`、`degradations`、`normalization_version` | 溯源最小集；降级必须显名 | 部分已有 |
| 相关与标度 | `correlation_summary`、`flux_conservation_factor`、`k_corr.{definition,value,domain,calibration}` | 溯源最小集；`k_corr` 缺域或跨域外推时拒绝 | 部分已有 |
| 参考通量口径 | `reference_mag`（`m_ref`）、逐帧 `reference_flux`（`F_ref`）、`reference_baseline.scope` | `unified_model.md §2.1`：`F_ref` 是阶段二叠加权重换算的分母 | 已有（散在 `p1_snr.json`/header） |
| 可用性 | `unavailable.{flag,reason,scope}`、逐帧 `unavailable` | 溯源最小集三条附加约束 | **需补** |
| 摘要 | `generated_utc`、`output_hash` | 溯源最小集 | 已有 |

**注意**：以上 (b) 的多数键目前散落在**多个文件**（HiPS `properties`、逐帧 `p1_final.json`、`p1_stack.json`、运行 manifest）。收敛为**一个 JSON** 意味着把它们**汇总**到一处——这是「一个 JSON」的实质工作量，不只是删文件。

### 5.4 现有 6+3 个 JSON 的处置（拟，逐条待裁决）

| JSON | 拟处置 | 理由 | 待裁决点 |
|---|---|---|---|
| `p1_wcs.json` | 转调试输出（走开关） | 阶段一内部边（R4），阶段二不读 | 峰值下 WCS 由 HiPS `properties` 承载，是否已足够 |
| `p1_sources.json` | 转调试输出 | 阶段一内部边（R5），阶段二不读 | 阶段二掩膜读的是 HiPS `snr/` 目录，非此文件 |
| `p1_phot.json` | 转调试输出 | 阶段一内部边（R6） | 测光 provenance 是否需并入那个 JSON |
| `p1_snr.json` | 转调试输出 | 阶段一内部边（R7，读点在算法库） | 帧级 SNR 已写入 HiPS 头（`ACSD_FRAME_SNR`），是否重复承载 |
| `p1_flux.json` | **建议删除** | lib 内零读取点，且已被判为「幻边」 | 是否有实验单元/外部脚本读它（见 §5.6 K4） |
| `p1_psf.json` | **建议删除** | lib 内零读取点，注册表自陈「生产链路零消费者」 | 同上 |
| `p1_final.json` | 转调试输出或删除 | lib 内零读取点 | 其字段是否应并入那个 JSON |
| `p1_stack.json` | **保留**（且属 HiPS 树） | 阶段二**实际读取**（`finest_input_arcsec`） | 是否把它并入那个 JSON 而不再读树内文件 |
| `p1_products.json` | **保留并接线** | 它是「那个 JSON」最接近的现成物 | 是否改名/重构，以及阶段二读取点（M9） |

### 5.5 内存传递怎么接（方向，不含实施）

- 复用调度器**已有**的块流能力（`block_flow.cpp` 的 `set_input`/`set_input_f64`/`product`/`run_unit`），**不新建第二套块机制**（`AGENTS.md` §13 复用条）。
- 按数据形态分两类：**像素面**（calibrated/cleaned/photoapplied）按块注入与取产物；**小辅助量**（wcs/sources/phot/snr 的 JSON 结构）按块内的结构化字段传递，避免 JSON 序列化往返。
- **R8 必须保持落盘**：`p1_stack.json` 是跨阶段载荷（阶段二在另一个进程里读），且 `pipeline.md:228` 要求磁盘可独立重开。
- 写侧与读侧**同批落地**（`DEV_MODE.md:58`），否则出现 §4.3 D4 的不可逆不平衡。

### 5.6 风险与不可逆点（逐个列出，拿不准的一律登记）

| # | 风险 | 证据 / 影响 | 处置 |
|---|---|---|---|
| **K1** | **写侧闸掉后立即不可用** | `DEV_MODE.md:60` 明写：写侧一闸，同一构建就在校准取数处 fail-closed，直到第二批完成 | 严格同批落地（M1+M2+M4）；不得分两次提交 |
| **K2** | `DEV_MODE.md:34` 与负责人判据**正面冲突** | 同一批 JSON 在两份依据下判定相反 | **登记为待裁决**：先裁「依赖边载体是否已改内存块」，再定 JSON 是产物还是调试输出 |
| **K3** | `p1_products.json` 名义是阶段二输入、实际零消费者 | 注册表 `:944`、`pipeline.md:216` vs `lib/` 零读取点 | **待裁决**：接线（并新增阶段二读取点）还是删除 |
| **K4** | 历史产物 / 实验单元可能读这些 JSON | 已知 `实验/absolute-snr` 读 `p1_sources.json`/`p1_snr.json`（其 README:69 记 `norm/<tile>/{calibrated_*.fts, p1_sources.json, p1_snr.json}`）；`lib/phase1_session` README 提到 `p1_final.json` | **未穷尽**：删除或改语义前须逐单元清点消费方；本轮**未完成该清点**，登记 UNRESOLVED |
| **K5** | 阶段一内部还有**第二条**调用链 | `CONFIG.md` 配置面三登记 orchestrator `stage1.*.enabled` → `Orchestrator::run_stage_*`（`run_stage_calibrate`/`run_stage_drizzle`/`run_stage_snr` 等），与 `module_adapters` 的 8 节点链并存 | **待裁决**：两条链的产权与收敛归属；本轮未追踪 orchestrator 链的文件写出面 |
| **K6** | 运行级记账（资源 CSV / 图 / run manifest）**不是科学产物**但是既有验收面 | `commands.cpp:1088-1110`、`:650/:727/:746` | 建议**不并入**本次收敛（属验收/性能面）；若负责人要一并收敛须另裁 |
| **K7** | `block_flow` 是**公开 API 面新增接线** | `DEV_MODE.md:42` 明写这是「新增接线而非切换既有绑定」；`AGENTS.md` §13 要求「同一件事一份实现」 | 接线时须核对 `block_flow.h` 的公开签名与角色/生命周期，避免复制实现 |
| **K8** | 精度/内存副作用 | 平面件不再落盘 ⇒ 峰值内存上升（当前靠磁盘换内存）；R1–R3 单项 ≈64 MiB/帧 | 需在改后测峰值 RSS 与墙钟（本轮未测） |
| **K9** | `p1_photscale.json` 是**输入**通道而非产物 | `module_adapters.cpp:5638`/`:6240` 读取；`lib/` 内无写出点 | 收敛产物面时**不得**误删该输入通道（它是「上游/外部标定」的第二通道） |

### 5.7 UNRESOLVED（本轮未能确证，不猜）

1. **K2 的裁决前置**：`DEV_MODE.md:34` 的「逐帧 JSON 是标准产物」是否随第二批改造而失效——需负责人一句。
2. **K3**：`p1_products.json` 的定位（接线 vs 删除），以及「那个 JSON」是它还是新建。
3. **K4**：全仓消费方清点**未完成**。本轮只确证 `lib/` 内的读取点；`实验/**`、`tools/**`、`eng/**`、`run/**` 下的历史脚本与本单元以外的实验单元**未逐一提查**。
4. **K5**：orchestrator `stage1.*` 链是否也写产物面文件（`run_stage_*` 的落盘面），**未追踪**。
5. **K6**：运行级记账文件是否属于「标准模式产物」的定义域，**未裁决**。
6. **内存传递的接口形态**：块内「结构化辅助量」是按现有 `set_input_f64` 传递还是需要扩签名，**未核** `block_flow.h` 的完整公开面。
7. 本轮**未运行任何构建或端到端**，所有结论为静态清点；R1–R8 的往返判定以「同进程 + 编排层文件约定」为依据，**未经运行期验证**。

---

## 6 一句话汇总

标准模式一次运行的磁盘产物面现状是：**逐帧 HiPS 树 + 7 个 JSON（其中 6 个在 `lib/` 内零阶段二消费者）+ 5 类帧级平面中间件（3 项各 ≈64 MiB/帧，且都是同进程写盘后读回）+ 记账/资源/图**；阶段二真正需要的跨阶段载荷只有**逐帧 HiPS 树**与树内的 `<frame_key>/p1_stack.json`（`finest_input_arcsec`）。四个构建期开关里**三个零消费者**，唯一有消费者的那个**改变执行路径两次**（取数来源 + 流程存续），与「开关只决定额外输出」的口径冲突。收敛到「逐帧 HiPS + 一个 JSON」的关键不在删文件，而在**把阶段一内部依赖边的载体从磁盘换成内存块**（`DEV_MODE.md` 第二批），且必须与写侧门控**同批落地**。
