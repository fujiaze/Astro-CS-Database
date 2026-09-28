# 插件文档：aio（I/O 与原子提交）

> 上游：ASTROCS_DESIGN.md §10（I/O 与原子产品）

## 1. 职责与边界

- **职责**：FITS/HiPS/manifest 的**唯一 I/O 边界**：读、写、校验、原子提交、缓存；**形态解析**（裸 HiPS 在役；zstd 归档包读侧属待实现项，见 §4）。
- **不是**：不做科学计算；不做重采样/投影；Phase1/2/3 复用同一套 AIO（reader/writer 只有这一套）。
- **形态解析层**：按落盘名判定形态并向上层提供统一读语义——裸 `<name>.hips/` 直接按路径读（现行读路径）；归档形态 `<name>.hips.zst`（按产品级索引定位表 `pread` 单帧解压交付瓦片、覆盖查询走不压缩索引、不解压整包、不落中间文件）的**读侧未接线**：`storage_form` 在生产写出侧与读侧均无读取点，归档形态的读路径属待实现项，按变更流程推进。调用方（Phase2/Phase3）读面消费裸形态。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §8.4（顶层结构：aio 模块位）、§10（I/O 与原子产品：aio 是文件级唯一 I/O 边界）
- FITS Standard v4.0（IAU FITS Working Group, 2016；Pence et al. 2010, A&A 524, A42）、[IVOA HiPS 1.0 Recommendation](https://www.ivoa.net/documents/HiPS/)

## 3. 输入/输出数据合同

- **输入**：FITS/HiPS/manifest 路径、数据对象（signal/variance/coverage/...）、配置。
- **输出**：原子提交的产品目录/文件 + manifest + 校验记录。
- 参考：`eng/contracts/schemas/*.schema.json`（全部数据产品）。

## 4. 算法与公式要点

- 形态（`docs/design/PRODUCT_STORAGE_FORM.md`）：归档形态 = 产品在 stage 内先按裸形态写出并逐瓦片校验，再按成员边界切分为独立 zstd 帧串接；归档、索引、完成 manifest 依次 fsync 后原子落位，manifest 最后落；
- **形态来源与登记**：Phase1 的落盘形态由输入配置键 `storage_form`（`archive` 默认 / `bare`；键缺失或留空 ⇒ 默认 + warn）选定（合同 `eng/contracts/schemas/phase_config_normalize.schema.json`#/$defs/storage_form，CLI 可达性已闭合：`lib/infrastructure/cli/parser.cpp` session_keys + `session_commands.h` config_fields）。该键在**生产写出侧无读取点**，**现行产出 = 裸形态**：按形态落盘（裸/归档两形态、逐成员独立 zstd 帧）、产品级索引 `<name>.hips.index.json` 与完成清单 `manifest.json` `storage` 段（`storage_form`/`index_path`/`index_sha256`/`archive_sha256`/`archive_bytes`/`tree_hash`）的写出均为**待实现项**，按变更流程推进；形态事实登记落点的合同口径见 `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` §10；
- 所有产品：临时文件/目录 + 校验 + fsync + 原子 rename 提交；
- **裸形态体积削减（文件系统打洞）**：`aio_sparse_punch.h` 是打洞机制的**唯一实现**——只对 4 KiB 对齐的**字面全零**块调 `fallocate(FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE)`（Windows：`FSCTL_SET_SPARSE`+`FSCTL_SET_ZERO_DATA`，64 KiB 对齐），打洞后丢页缓存**读回复算**整文件 `sha256`；不一致 ⇒ 硬错误、不发布；卷不支持 ⇒ `trim=skipped(reason)` 的 warn 后照常发布。判据是**字节级**（`-0.0` 与 NaN 的位型含非零字节 ⇒ 不可打洞）。打洞在 `write_fits_atomic` 内、`fsync` 之后与原子 rename 之前完成；归档容器不做打洞（`docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md` §7）；
- **发布终态恰有三态（P-174 取代旧口径「失败/取消的产物面 = 无正式产品」）**：① **已发布且持久化已确认**（`PublishDurability::kDurable`：`status=kOk`、`renamed=true`、rename 后目录 fsync 成功）；② **已发布但持久化未确认**（`kNotDurable`：目标**已可见且不可回滚** —— rename 后目录 fsync 失败（`status=kErrIo`），或本平台无目录 fsync 等价物 / 调用方显式关闭 `fsync_directory`（`status=kOk`））；③ **未发布**（`kNotPublished`：rename 未发生，或 rename 后验证失败且撤销成功 ⇒ 目标根无正式产品）。判据 = `PublishResult::durability`（与 `status` 正交；恒等式 `renamed == (durability != kNotPublished)`；`PublishStatus` 数值域 0..15/70/71 仍由 `static_assert` 冻结，本态**不新增取值**）；**调用方处置：② 不得回滚删除**（产品已可见，删掉即毁掉已发布对象）、**不得静默当成功**（该目录项在崩溃后可能消失），须显式可见（日志/manifest 标注「持久化未确认」）并允许对父目录重跑 fsync 确认；只有 ③ 才按「无正式产品」清理/重发；
- **单写者前提**：跨进程不取文件锁，同一产品路径同一时刻只允许一个写者（双写者同目标 = 原子 rename 的 last-writer-wins）；并发读安全，并发写由调用方在更高层串行化；
- 读：格式校验、哈希校验、单位/形状/所有权检查；
- 写：分层写（先数据后 manifest）、flush/close/fsync、checksum、原子 rename；
- 缓存：只缓存不改变科学值；容量有界、可失效。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `cache_mb` | —— | MB | 缓存上限 |
| `fsync` | true | —— | 原子提交 fsync |
| `checksum` | true | —— | DATASUM/CHECKSUM |

## 6. 接口/ABI

- 公共读写 API（C ABI 版本化），供 lib/infrastructure/cli/scheduler/科学模块调用；
- 是唯一允许触碰磁盘产品的模块。

## 7. 错误与边界

- 哈希/格式不匹配 → 拒绝读取；
- 缺 tile/非有限 → validity 标记，不以零填充；
- 取消 → 无半成品；
- I/O 错误 → exit 7。

## 8. 测试与 Oracle

- 原子提交测试：中途失败/取消无半成品；
- 重开独立验证与写入一致；
- 哈希/校验失败拒绝读取；
- 缓存正确性（不改变科学值）；
- 长路径、>2 GiB、Windows/Linux 双平台。
