# aio（I/O 与原子提交）

> 上游：docs/ACSD_DESIGN.md §10（I/O 与原子产品）、§8.4（顶层结构：aio 模块位）、
> §8.5（模块与 ABI）
> 数据正本：docs/science/DATA_SEMANTICS.md（§11.1 帧内命名块表，仅作引用；
> DATA-HIPS-SIGNAL-001 等 HiPS tile 语义）、docs/engineering/CONFIG_SCHEMA.md
> 合同面：docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md（§7 归档容器不打洞、
> §10 形态事实登记落点）、docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md
> （IO_002 读合同、IO_003 原子发布）
> API 正本：docs/engineering/PUBLIC_API.md（API-AIO-001..）
> 科学 ID：SCI-DRZ-014/016（产品语义）
> 精度：docs/engineering/PERFORMANCE_MODEL.md（FP64 reference）
> 落盘形态说明：docs/detail/PRODUCT_STORAGE_FORM.md
> 引用文献：见文末「参考文献」（角标用全角 `［N］`）

## 1. 职责与边界

- **职责**：FITS / XISF / AHPIX / HiPS / manifest 的**唯一 I/O 边界**：读、写、
  校验、原子提交、缓存；zstd / lz4 压缩；UPM 模型文件容器（`aio_upm` sparse /
  dense）；`PipelineFrame` / 引擎；**形态解析**（裸 HiPS 在役；zstd 归档包读侧属
  待实现项，见 §4）。
- **不是**：不做科学计算（几何 / 统计）；不做重采样 / 投影；不写 `testdata/`。
  Phase1/2/3 复用同一套 AIO（reader / writer 只有这一套）。
- **形态解析层**：按落盘名判定形态并向上层提供统一读语义 —— 裸 `<name>.hips/`
  直接按路径读（现行读路径）；归档形态 `<name>.hips.zst`（按产品级索引定位表
  `pread` 单帧解压交付瓦片、覆盖查询走不压缩索引、不解压整包、不落中间文件）
  的**读侧未接线**：`storage_form` 在生产写出侧与读侧均无读取点，归档形态的读
  路径属待实现项。调用方（Phase2 / Phase3）读面消费裸形态。

### Production callers

所有科学模块、编排层、Phase2（`aio_upm` / `aio_hips_reader`）、浏览器组件
（hips reader）。

## 2. 权威依据

- 最高设计 `docs/ACSD_DESIGN.md` §8.4（顶层结构：aio 模块位）、§10（I/O 与
  原子产品：aio 是文件级唯一 I/O 边界）、§9（原子发布的本期例外如实登记）
- 机器可校验 schema：`eng/contracts/schemas/*.schema.json`（全部数据产品）

## 3. 输入输出数据合同

- **输入**：FITS / HiPS / manifest 路径、数据对象（signal / variance /
  coverage / ...）、配置。
- **输出**：原子提交的产品目录 / 文件 + manifest + 校验记录。
- **HiPS tile 精度**：`AioHipsDataType` 枚举 `AIO_HIPS_FLOAT32 = 0` /
  `AIO_HIPS_FLOAT64 = 1`（lib/infrastructure/aio/include/aio_hips.h）透传至
  `aio_hips_product_begin` 的 `data_type`，写盘 `BITPIX -32 / -64` 对应 CFITSIO
  `TFLOAT / TDOUBLE`（src/hips/aio_hips_writer.cpp）。**科学精度优先 FP64
  reference，FP32 仅显式等价路径**（见 docs/engineering/PERFORMANCE_MODEL.md）。
- **UPM sparse format** = `acsd-upm-v2`（DATA-UPM-MODEL-001）。

## 4. 形态与原子提交

- 形态（docs/detail/PRODUCT_STORAGE_FORM.md）：归档形态 = 产品在 stage 内先按裸
  形态写出并逐瓦片校验，再按成员边界切分为独立 zstd 帧串接；归档、索引、完成
  manifest 依次 fsync 后原子落位，**manifest 最后落**；
- **形态来源与登记**：Phase1 的落盘形态由输入配置键 `storage_form`
  （`archive` 默认 / `bare`；键缺失或留空 ⇒ 默认 + warn）选定（合同
  `phase_config_normalize.schema.json` `#/$defs/storage_form`；CLI 可达面 =
  lib/infrastructure/cli/parser.cpp 的 `session_keys` +
  `session_commands.h` 的 `config_fields`）。该键在**生产写出侧无读取点**，
  **现行产出 = 裸形态**：按形态落盘（裸 / 归档两形态、逐成员独立 zstd 帧）、
  产品级索引 `<name>.hips.index.json` 与完成清单 `manifest.json` 的 `storage`
  段（`storage_form` / `index_path` / `index_sha256` / `archive_sha256` /
  `archive_bytes` / `tree_hash`）的写出均为**待实现项**；形态事实登记落点的合同
  口径见 docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md §10；
- 所有产品：临时文件 / 目录 + 校验 + fsync + 原子 rename 提交；
- **裸形态体积削减（文件系统打洞）**：`aio_sparse_punch.h` 是打洞机制的**唯一
  实现** —— 只对 4 KiB 对齐的**字面全零**块调
  `fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)`（Windows：
  `FSCTL_SET_SPARSE` + `FSCTL_SET_ZERO_DATA`，64 KiB 对齐），打洞后丢页缓存
  **读回复算**整文件 `sha256`；不一致 ⇒ 硬错误、不发布；卷不支持 ⇒
  `trim = skipped(reason)` 的 warn 后照常发布。判据是**字节级**（`-0.0` 与 NaN
  的位型含非零字节 ⇒ 不可打洞）。打洞在 `write_fits_atomic` 内、`fsync` 之后与
  原子 rename 之前完成；**归档容器不做打洞**（HIPS_STORAGE_FORM_CONTRACT §7）；
- **发布终态恰有三态**：① **已发布且持久化已确认**
  （`PublishDurability::kDurable`：`status = kOk`、`renamed = true`、rename 后
  目录 fsync 成功）；② **已发布但持久化未确认**（`kNotDurable`：目标**已可见且
  不可回滚** —— rename 后目录 fsync 失败（`status = kErrIo`），或本平台无目录
  fsync 等价物 / 调用方显式关闭 `fsync_directory`（`status = kOk`））；
  ③ **未发布**（`kNotPublished`：rename 未发生，或 rename 后验证失败且撤销成功
  ⇒ 目标根无正式产品）。判据 = `PublishResult::durability`（与 `status` 正交；
  恒等式 `renamed == (durability != kNotPublished)`；`PublishStatus` 数值域
  0..15/70/71 仍由 `static_assert` 冻结，本态**不新增取值**）；
  **调用方处置：② 不得回滚删除**（产品已可见，删掉即毁掉已发布对象）、**不得
  静默当成功**（该目录项在崩溃后可能消失），须显式可见（日志 / manifest 标注
  「持久化未确认」）并允许对父目录重跑 fsync 确认；只有 ③ 才按「无正式产品」
  清理 / 重发；
- **单写者前提**：跨进程不取文件锁，同一产品路径同一时刻只允许一个写者（双写者
  同目标 = 原子 rename 的 last-writer-wins）；并发读安全，并发写由调用方在更高
  层串行化。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `cache_mb` | —— | MB | 缓存上限 |
| `fsync` | true | —— | 原子提交 fsync |
| `checksum` | true | —— | DATASUM / CHECKSUM |
| `compression` | —— | —— | 压缩级别（zstd / lz4，块级） |
| `order` | —— | —— | HiPS order 参数 |

无全局 config（模块不读全局配置）。

## 6. 接口/ABI

- 公共读写 API（C ABI 版本化，`API-AIO-001..`），供 lib/infrastructure/cli、
  scheduler 与科学模块调用；是**唯一允许触碰磁盘产品**的模块。符号族 =
  `aio_fits` / `aio_xisf` / `aio_hips_reader` / `aio_hips_writer` / `aio_upm` /
  `aio_compressor` / `aio_pipeline`。
- **`PipelineFrame`** = 纯命名块容器（按块名索引）。**块词表的唯一登记处 =
  `lib/infrastructure/aio/include/aio_pipeline.h` 的「标准块定义表」**（aio 是
  文件级唯一 I/O 边界，块词表属 aio 的内存块合同）；其它文档只作引用 ——
  编排层的 6 个名字**不是块词表**，而是 `stage_trace.jsonl` 的**跟踪子集**；
  DATA_SEMANTICS §11.1 的「帧内命名块」表**只作引用**。当前实现状态：标准块定义
  表**尚未收录** `variance` 块；`aio_pipeline.h` **仍允许**未列出的自定义块名；
  「块名 ∉ 标准表 ⇒ 判红」的机器判据**尚未建立**。
- `PipelineStageFn` 签名 = `const input / output / params` + `error_msg` /
  `error_capacity`（可为 NULL，> 0 保证 NUL 终止 / 截断）；
  `aio_frame_add_block_move` 为 move 语义（成功接管后调用方不再拥有 `aio_alloc`
  的 buffer）。

## 7. 错误、日志、指标、取消和 checkpoint

- 错误类别 = IO / CONFIG / INPUT_CORRUPT；`aio_upm_last_error` 提供消息；
  dense stale cache = rc 2；
- 哈希 / 格式不匹配 → 拒绝读取；
- 缺 tile / 非有限 → validity 标记，不以零填充；
- 取消 → 无半成品；
- I/O 错误 → exit 7（退出码唯一源 = lib/infrastructure/cli/exit_codes.h；域→码
  映射唯一源 = docs/engineering/LOG_AND_ERROR_CONTRACT.md §5）；
- **日志落点** = 块级 `log_dir`（默认 `<output_dir>/logs`），节点事件经
  observability 汇聚（最高设计 §7.3）；落点之外的位置（含 `run/`、源码树目录、
  安装目录、用户家目录、进程 CWD 相对路径）均不在处置面内，机器判据见
  docs/engineering/LOG_AND_ERROR_CONTRACT.md。日志内容 = 错误类别 + 消息。

## 8. 执行类、并行轴、ThreadBudget lease、确定性

- 性能特征：流式读写；compression 块级；HiPS 写**先 tile 后 properties**；
- 线程：独立句柄可并行；同一句柄顺序访问；dense cache 写 / 读分离；
- 缓存：**无进程级缓存**（读路径为句柄级）；缓存只缓存不改变科学值，容量有界、
  可失效；
- 确定性：只读路径 = 校验 + 哈希重算，无求和序变化。

## 9. 测试与 Oracle

- 原子提交测试：中途失败 / 取消无半成品；
- 重开独立验证与写入一致；
- 哈希 / 校验失败拒绝读取；
- 缓存正确性（不改变科学值）；
- 长路径、>2 GiB、Windows/Linux 双平台；
- 现状执行面（相邻证据，引用不冒认）：`hiss_correctness`、
  `pipeline_frame_contract`、`checksum`、`drizzle_integration`、fuzz / sanitize
  driver、Python oracle（`hips_mapping_oracle`）；`test_precision_dual.cpp` 覆盖
  FP64 精度 oracle（DATA_TYPE FLOAT32/FLOAT64 双模式：`precision_mode` /
  `signal_dtype` 元数据与 `acsd_signal_dtype` 一致性）。

## 10. 已知限制

- UPM sparse 走 temp + rename 原子写（IO_003）；
- **HiPS tiles 非原子 —— 已登记的未闭合缺口**：partial-file 策略 = abort 尽力
  清理、finalize 写 CHECKSUM / DATASUM 后交付；单 tile 为 remove → create →
  write_chksum → close（lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 的
  `std::remove`），**不是** temp + rename 原子发布。**HiPS tile 原子发布的宣称以
  该缺口闭合为前提**（最高设计 §10 的原子发布条款；缺口如实登记）；
- 归档形态的写出与读取、产品级索引与完成清单 `storage` 段写出未落地，生产只落
  裸形态；
- 标准块定义表未收录 `variance` 块，且无块名越表的机器判据；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。

## 11. 源文件

`lib/infrastructure/aio/{include, src}/`。

## 12. 参考文献

- ［1］ IAU FITS Working Group. (2016). *FITS Standard*, Version 4.0.
  永久链接 [fits.gsfc.nasa.gov/fits_standard.html](https://fits.gsfc.nasa.gov/fits_standard.html)
- ［2］ Pence, W. D.; Chiappetti, L.; Page, C. G.; Shaw, R. A.; Stobie, E. (2010).
  "Definition of the Flexible Image Transport System (FITS), Version 3.0".
  *Astronomy and Astrophysics* 524, A42.
  DOI [10.1051/0004-6361/201015362](https://doi.org/10.1051/0004-6361/201015362)
- ［3］ IVOA. (2017). *HiPS - Hierarchical Progressive Survey*, Version 1.0,
  IVOA Recommendation, 19 May 2017.
  永久链接 [ivoa.net/documents/HiPS](https://www.ivoa.net/documents/HiPS/)
