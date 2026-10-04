# 原子发布合同

上游：最高设计的 I/O 与原子产品一章[1]。

产品发布的步序、发布清单、校验序与错误码的正本；生产者协议的唯一正本是本文的发布流水线一节。

本合同按最高设计的 I/O 与原子产品一章[1]的发布链执行：阶段隔离由「每次运行私有临时区 + 唯一用户路径」保证，
原子性由「临时写 → 校验 → fsync → 哈希 → 原子改名 → 完成清单」保证；科学公式与图像算法按
`docs/science/` 与 `docs/science/algorithms/` 的正本执行，不在本合同范围内。

## 目标与范围

本合同在 FITS 流式接口（原子写）与 HiPS 输入读端之上建立 **原子 HiPS/manifest
输出发布** 合同（宿主基础设施，不含科学算法迁移）：把磁盘上产出的 HiPS 子产品目录
（`properties` + `NorderK/DirD/NpixN.fits` tiles + 可选 `Moc.fits`）以 **原子、可恢复、
唯一目标** 的方式发布，并在发布完成后落 **完成 manifest**（唯一完成标记）。HiPS 输入
读端 / 跨阶段消费只接受本接口发布的完整产物（`../data/PHASE_PRODUCT_EXCHANGE.md`
的 `R-DISK-ONLY`）。

发布流水线（每个产物文件）：
`临时写（run 私有 stage）→ 关闭/fsync → fitsverify（结构 + DATASUM）→ sha256 →
原子 rename → 最后原子落 manifest.json(COMPLETE) = 完成标记`。

**落盘形态**（`HIPS_STORAGE_FORM.md`[2]）：产品以裸
`<name>.hips/` 或归档 `<name>.hips.zst` 发布，二者互斥；归档形态的发布次序是
「stage 内先按裸形态写出并逐瓦片 fitsverify → 打包为逐成员独立 zstd 帧 → 写产品级
索引 → 归档与索引 fsync + 原子 rename → 完成 manifest」。**归档解压后的合法性在
写入侧即被证明**（打包前逐瓦片过 fitsverify；发布前用标准工具解压与裸形态做
`tree_hash` 等价比对）。产品身份哈希仍取**解压后内容**（落盘形态合同的「哈希口径」一节），归档字节
的 sha256 只作容器指纹记入 `storage` 段，不作产品身份。

本合同覆盖**输出端**的发布语义：tile 生成 / 投影按各命令科学模块的正本执行；
`lib/infrastructure/aio` 与 `lib/infrastructure/aio/io` 按各自现行职责运行。读端
（`../../science/IO_002_HIPS_INPUT_INTERFACE.md`）与产物交换资格
（`../data/PHASE_PRODUCT_EXCHANGE.md`[3]）是独立合同面，不在此重复。

**原子性的适用范围**：原子性覆盖全部产品，HiPS tile 也不例外——tile 写统一经
`write_fits_image` → `write_fits_atomic`（`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`）。
全部 tile 写调用点与 MOC 写自动经该路径；失败分类（`ENOSPC` / 写失败）在清理前完成
（`aio_disk_full.h` 语义）。负例须覆盖 `tile_diskfull` / `tile_write_fail` 两种注入。**判据无载体**：承载这两条负例的
测试文件在仓内不存在（`lib/infrastructure/aio/` 无 `tests/` 目录），故本条不得写成已生效。

**产物落点只有两处**：产品落块级 `output_dir`；开发与 CI 过程产物落仓库过程产物目录。
写出位置取自显式配置，进程 CWD 只标识进程自身位置。

## 模块归属与目录

| 内容 | 路径 |
| --- | --- |
| 本合同 | `ATOMIC_PUBLISH.md` |
| 原子输出发布器（Python 执行形态） | `lib/infrastructure/aio/io/hips_output_store.py` |
| FITS 独立校验器（fitsverify，与 fits_core 同算法） | `lib/infrastructure/aio/io/fits_verify.py` |
| 契约 / 负测（Python） | **判据无载体**：仓内无 `eng/tests/io/` 目录，契约与负测尚无测试文件 |
| 测试 tile fixture（复用 FITS 流式接口 fits_core） | **判据无载体**：同上 |

本接口执行形态 = `lib/infrastructure/aio/io/hips_output_store.py`（发布状态机）+
`lib/infrastructure/aio/io/fits_verify.py`（FITS 结构 + DATASUM 校验）：纯 Python 语义层，
可在 Linux 控制 / 轻合成节点完整验证。

**已知缺口登记**：mosaic 阶段直写输出目录、无 staging 环节，缺口登记面 =
`../../detail/registry/acsd.phase2.write.md`。该缺口的闭合条件与判定口径以该登记面为准。

**契约登记**：原子 I/O 与发布契约 = `ENG-IO-001`（登记面 = `../governance/TRACEABILITY.md`[4] 的合同 ID 登记面）。

## 唯一目标与 run 隔离

### 目标布局

```
{output_root}/runs/{run_id}/stage/ # run 私有临时写区（未发布）
{output_root}/runs/{run_id}/products/{user_path}/ # 已发布目标（唯一用户路径）
{output_root}/runs/{run_id}/products/{user_path}/manifest.json # 完成 manifest
```

- `run_id` 词法：`^[A-Za-z0-9._-]+$`（唯一；两个不同 run 的目录树完全分离，
 **并发不同 run 绝不共享目标路径** → 不互相覆盖）。
- `user_path` = 用户路径（相对；可含子目录 `signal`、`signal/Norder0/…` 等
 目录型产物），段词法 `^[A-Za-z0-9._-]+$`。
- **产物面划分**：本接口的产物面 = `runs/{run_id}/products/{user_path}/`（HiPS 目录产物 +
 完成 manifest）；typed artifact 面（`objects/` + `manifests/`）的正本 =
 「模块归属与目录」一节。两面目录不重叠，各由其正本约束。

### 词法拒绝（路径穿越）

`user_path` / 发布文件名 / run_id 含以下任一 → `PathTraversalError`（发布前拒绝）：

| 违例 | 例子 |
| --- | --- |
| 绝对路径 | `/abs/path` |
| 父目录穿越 | `../evil`、`a/../../b` |
| 空段 / 当前段 | `a//b`、`a/./b`、尾 `/` |
| 反斜杠（Windows 分隔符） | `a\b` |
| 非法字符段 | `a b`、`a*b`（段外 ASCII 可见字符一律拒） |

文件名（products 内相对名）额外约束为 HiPS 目录产物形态：
`properties` \| `Moc.fits` \| `NorderK/DirD/NpixN.fits`（K/D/N 为十进制数字）\| `coverage.index.json`
（**数据集级**覆盖索引，只认**根层**这一个名）——
其它文件名（如 `notes.txt`、任意 `.fits` 布局、带子目录或改名的覆盖索引）发布前拒绝（`PublishError`）。

`coverage.index.json` 为何在白名单里：它是数据集级产物，按 `HIPS_STORAGE_FORM.md`「数据集级覆盖索引 `coverage.index.json`」一节与归档容器的布局规则 A5 位于**归档之外**（与产品同级、运行输出根）；发布它的清单仍是同一份「HiPS 目录产物」清单（`properties` 必在，见「发布流水线（原子语义）」一节），故必须与产品文件同名法登记，否则同一发布路径上「文档允许而发布器拒绝」。
**仍只有一条判据、无豁免名单**：白名单按**形态**判定，不为任何产品 / 场景开逐名豁免；产品树内的 `metadata.fits`、产品集 `manifest.json`、`snr/**`（`.tsv` / `metadata.xml`）面的登记以各自正本为准（落盘形态合同的「形态的输入配置与输出清单字段」一节，以及统一工程对象正本 `../UNIFIED_OBJECTS.md`）。

**只读数据集**：`testdata/` 保持只读，打开模式限于只读；本合同的发布器不对其发起任何写操作。

### 文件系统层拒绝（权限 / 符号链接）

发布器在每次写 / rename 前对目标路径组件做符号链接检查：任何已存在组件为
符号链接 → `PermissionError_`（防符号链接逃逸 stage / 目标根）。底层 I/O 层
把 `EACCES` / `EPERM` / `EROFS` / `EISDIR` 统一映射为 `PermissionError_`；发布产物目录
权限收紧 `0o750`、文件 `0o640`（阶段隔离，绝对用户路径 / 凭据不落盘）。

## 发布流水线（原子语义）

一次 `publish_directory(user_path, files, overwrite=…)`：

1. **先检后写**：`user_path` 词法 → 目标存在性 → 文件清单词法（properties 必在；
 文件名形态）→ 符号链接检查。全部通过才进入写。
2. **覆盖清理**：默认不覆盖——目标为**成功对象**（含 COMPLETE manifest）且
 `overwrite=False` → `PublishError`。中断 / cancel 残留（目录存在但无 COMPLETE
 manifest）= **非成功对象**（`../data/ARTIFACT_STORE.md` 语义）→ 自动清残重发；成功对象仅在显式
 `overwrite=True` 时清除后重建。
3. **stage 临时写**：每个文件写入 `{run}/stage/`（`O_EXCL`；临时名带内容 sha256
 前缀防碰撞）；写后 `fsync` 关闭（关闭 = 内容完整落盘）。

4. **fitsverify**：`.fits` 科学平面 tile（`NpixN.fits`）必须通过结构 + DATASUM
 校验（`fits_verify.py`，与 FITS 流式接口 fits_core `fits_verify_file` 同一算法族，
 可由 C verifier / astropy 交叉验证）。失败 → `PublishError`，无成功对象。
 `Moc.fits` 为 BINTABLE（FITS 流式接口支持域外，HiPS 输入读端 MOC optional）→ 只做 sha256
 + 原子落盘，不阻塞发布。
5. **sha256**：每个文件内容 sha256/64hex 记录。
6. **原子 rename**：逐个 `os.replace`（同文件系统原子）从 stage → 目标；
 每步前已 fsync。
7. **完成 manifest**：最后原子写 `manifest.json`（`status=COMPLETE`；= 唯一完成
 标记）。成功对象 = 内容 + COMPLETE manifest 齐全。

任一步失败 / 中断（`KeyboardInterrupt` / `SystemExit` / 异常）→ 无 COMPLETE manifest、
无成功对象；可恢复（`cleanup` 清 stage / 新 Store `start` 不索引残留 /
同 run 重发）。

**走同一发布序的非 tile 产品**：

| 产品 | 写路径 | 校验与完整性 |
|---|---|---|
| UPM sparse 模型 | `aio_upm_write_sparse` = temp write → validate → atomic promote（`lib/infrastructure/aio/src/aio_upm.cpp`） | 内容 hash 记入产物清单 |
| dense cache | 固定 512B 头部 + 二进制块 + streaming checksum | 打开时校验 checksum 与 `source_hash` |
| HiPS 目录 | 先 tiles / properties 落目标目录，最后 `properties` / `index` | verify（CHECKCODE / CHECKDATASUM）后交付 |

### 机制层步序与「两序关系」的唯一正本

上表 6 步是**生产者协议正本**：P3 写路径与该序一致，校验 / 哈希**先于** rename，
故校验失败时目标位置从不出现半成品。**机制层**（`lib/infrastructure/aio/product_io/` 的
`atomic_publish_file` / `atomic_publish_directory`，即 `atomic_publish.h`）执行同一组步骤，
但**重开验证读的是已发布对象**（rename 之后，由调用方的 `verify_fn` 提供），验证失败即撤销。
两序的保证关系（本处是**唯一**声明处；`atomic_publish.h` 只引用不复写）：

- **内容面等价**：同目录（同文件系统）rename 不改变字节，机制层校验的 inode 内容与
 tmp 阶段写入的内容相同；
- **检测面：机制层更强**——它能检出「rename 之后才发生」的损坏（外部进程截断 / 改写目标、
 回写上出现的坏块），生产者序（先校验后 rename）在这一窗口无观测点；
- **可见性 / 破坏面：机制层更弱**——存在「未验证对象短暂可见」的窗口，且撤销会删除
 已被 rename 替换掉的目标位置。故机制层**必须**用 `PublishResult::durability` 三态约束调用方
 （定义见 `atomic_publish.h`）：`kNotDurable` = 已发布但持久化未确认，**不得回滚删除**、
 不得静默当成功。

**能区分两序的判据**：在 rename **成功之后**破坏目标内容，机制层必须**检出并撤销**
（`status=ERR_*` + `renamed=false`）；生产者序在该情形下无法检出（校验点已在 rename 之前）。
**判据未接线**：仓内没有实现该注入的加载器（全仓只有本行提到 `ACSD_TEST_CORRUPT_AFTER_RENAME`），
故这条判据当前不可执行，不得写成已有可执行证据。

## 完成 manifest 形态

```json
{
 "manifest_schema": "acsd.hips-output-manifest/v1",
 "manifest_version": 1,
 "status": "COMPLETE",
 "run_id": "run-abc123",
 "user_path": "signal",
 "product": "signal",
 "publisher": "acsd.hips-output/v1",
 "tree": [
 {"path": "Norder0/Dir0/Npix0.fits", "size": <bytes>, "sha256": "<64hex>"},
 {"path": "properties", "size": <bytes>, "sha256": "<64hex>"}
 ],
 "tree_hash": "<64hex = sha256(规范 tree)>",
 "fitsverify": {"performed": true, "checksum": "datasum", "tile_count": <n>},
 "created_utc": "<ISO-8601 UTC>",
 "producer": {"module_id": "...", "module_build_id": "..."}
}
```

- `tree` 条目 = `{path, size, sha256}`（稳定排序）。

- **`storage` 段（加性，运行级形态事实）**：`storage` 段属**运行级完成清单** `manifest.json#storage`，
 记录该次运行生效的落盘形态与容器 / 索引指纹：`storage_form`（`archive` \| `bare`）、`form_source`
 （`config` = 输入配置显式给出 / `default` = 键缺失或留空 ⇒ 取默认 `archive` 并报 warn）、`products[]`
 （逐产品 `product` / `storage_form` / `index_path` / `index_sha256` / `archive_bytes` /
 `archive_sha256` / `tree_hash`）与 `coverage_index`。字段与不变式（M1..M4）的唯一正本 =
 `HIPS_STORAGE_FORM.md`「运行完成清单 `manifest.json#storage`（加性）」一节，机器事实源 =
 `eng/contracts/schemas/hips_storage_form.schema.json#/$defs.manifest_storage`。
 本接口的**产品级**完成 manifest（`products/{user_path}/manifest.json`，执行形态
 `lib/infrastructure/aio/io/hips_output_store.py`）承载 `tree` / `tree_hash` / `fitsverify` 三项与形态无关的事实。

- **形态事实只落输出清单面**：`storage_form` / `archive_sha256` / `index_sha256` 出现在三处——normalize
 输出清单 `p1_products.json#frames[]` 的逐帧四字段、上述运行级 `storage` 段、产品级索引
 （正本 = `HIPS_STORAGE_FORM.md`「normalize 输出清单 `p1_products.json`（加性）」与「运行完成清单 `manifest.json#storage`（加性）」两节）；归档内 `properties` 与裸形态
 逐字节一致；`properties` 只含标准 `hips_tile_format` token。
- **产品身份仍取解压后内容**：`tree` / `tree_hash` 记录解压后 HiPS 的条目（与裸形态相同）；`storage.archive_sha256` 只是容器指纹，产品身份判据 = `tree_hash`。
- `tree_hash` = **归一后**的 sha256 → **可重算**：
 归一规则（唯一实现 = `lib/infrastructure/aio/io/hips_output_store.py` 的 `tree_hash`，与
 `HIPS_STORAGE_FORM.md`[2] 的哈希口径同源）：① 每个 `tree` 条目 `{path,size,sha256}`
 归一为**三元组** `(path, size, sha256)`（非对象数组）；② 按 `(path, size, sha256)`
 字典序升序排序；③ 序列化为紧凑 JSON（UTF-8、`ensure_ascii=false`、分隔符 `(",",":")`
 无空白）；④ 对该字节串取 sha256。
 判据：同内容重算一致；任何文件改动 / 增删 → hash 变化。
 重算入口 = `tree_hash(tree_entries)` / `recompute_tree_hash(manifest)` /
 按磁盘实际文件重算 `verify_tree_hash`。
- `fitsverify` 记录发布时已执行校验（performed / tile_count）——机器证据。
- `producer`（可选）由调用方注入；不含绝对路径 / 凭据（privacy：任何绝对
 Unix / Windows 路径不进入 manifest —— 结构上字段均为词法受限标识）。

## 错误语义

| 情形 | 结果 |
|---|---|
| 路径穿越（词法） | `PathTraversalError`（发布前；无写入） |
| 目标为成功对象且 `overwrite=0` | `PublishError`（默认不覆盖） |
| 符号链接组件 / 权限拒绝 | `PermissionError_`（无成功对象） |
| tile fitsverify 失败 | `PublishError`（无成功对象） |
| 中断（进程 / 注入） | 无 COMPLETE manifest（可恢复） |
| 成功 | COMPLETE manifest 唯一完成标记 |

任何失败都不产生成功对象：无 COMPLETE manifest、不入索引、不可消费、可恢复。

## 非生产 / 诊断接口登记

下列符号是穷举点之外的「块 ↔ 文件」读写面，**降级为非生产 / 诊断接口**
（代码侧归属声明见 `lib/infrastructure/aio/include/aio_pipeline.h`）：

| 符号 | 性质 | 处置 |
|---|---|---|
| `aio_frame_save_cache` | 整帧写缓存文件（`.aio` 自定义二进制） | **非生产 / 诊断**；阶段内节点的数据搬运只走命名块管线 |
| `aio_frame_load_cache` | 整帧读缓存文件 | 同上 |
| `aio_frame_export_block_fits` | 单块导出 FITS | 同上（调试导出） |
| `aio_frame_export_block_xml` | 单块导出 XML | 同上 |
| `aio_frame_export_all_xml` | 全块导出 XML | 同上 |
| `aio_pipeline_export_xml` | 旧名包装（= `aio_frame_export_all_xml`） | 同上 |

- 符号签名登记面 = `../api/PUBLIC_API.md`（该表**只登记函数签名**，不含生产性分级）；
 **生产性分级以本表为准**；
- 本表是文档侧登记面；生产可用性判据以各符号在役调用链为准。

## 验收映射

合同条款 → 验收要求的对应关系如下。**当前状态：下列九项验收全部判据无载体** ——
承载它们的测试类（`TestUniqueRunDirIsolation` / `TestAtomicPublishPipeline` /
`TestNoOverwriteDefault` / `TestConcurrentRunsIsolated` / `TestInterruptNoCompleteMark` /
`TestPathTraversalRejected` / `TestPermissionRejected` / `TestTreeHashRecomputable` /
`TestFitsVerifyCrossOracle`）在 `lib/` 与 `eng/` 下均无实现，故本表登记的是**验收要求**，
不是已生效的判据。

| 验收 | 合同条款（现行口径） | 状态 |
|---|---|---|
| 每个输出以唯一用户路径或 run ID 目录 | 「唯一目标与 run 隔离」一节 | 判据无载体 |
| 临时写、关闭、fitsverify、SHA256、原子 rename、最终完成 manifest | 「发布流水线」一节 | 判据无载体 |
| 默认不覆盖，显式 overwrite 才可 | 「发布流水线」一节的覆盖清理步 | 判据无载体 |
| 并发不同 run 不互相覆盖 | 「唯一目标与 run 隔离」一节 | 判据无载体 |
| 中断后无完成标记 | 「发布流水线」一节的完成 manifest 步 | 判据无载体 |
| 文件权限 / 路径穿越拒绝 | 「词法拒绝」与「文件系统层拒绝」两节 | 判据无载体 |
| tree hash 可重算 | 「完成 manifest 形态」一节的 `tree_hash` 归一规则（实现 = `tree_hash` / `recompute_tree_hash` / `verify_tree_hash` 三个入口） | 判据无载体 |
| fitsverify 与 C verifier 同一判定 | 「发布流水线」一节的 fitsverify 步（实现 = `lib/infrastructure/aio/io/fits_verify.py`[2]） | 判据无载体 |
| 非生产 / 诊断接口不被生产路径引用 | 「非生产 / 诊断接口登记」一节 + 在役调用链扫描 | 可人工复跑 |

## 已知限制

1. 本接口交付 Python 语义层 + 全部契约 / 负测；Windows 侧交付形态 `acsd_io.dll` 按同一发布
 状态机由同语义 C 接线复刻（Linux 侧对应产物 `libacsd_io.so`）。
2. `Moc.fits`（BINTABLE 扩展）不做内容校验（FITS 流式接口合同的表扩展条款：表扩展 UNSUPPORTED；
 HiPS 输入读端 MOC optional hint 语义：缺失 / 损坏不阻塞读 / 写）。
3. 并发安全以 run 目录隔离 + 单次发布单线程为前提；单 run 内并发发布同一
 `user_path` 由调用方串行化（与 `../data/ARTIFACT_STORE.md` 的 writer 唯一 producer 同纪律）。
4. CHECKSUM 卡写路径沿用 FITS 流式接口默认关闭；fitsverify 校验 DATASUM（写入侧恒写
 DATASUM 卡），CHECKSUM 卡存在且非占位时同样校验。
5. mosaic 阶段直写输出目录、无 staging 环节（登记面见 「模块归属与目录」一节）。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/contracts/HIPS_STORAGE_FORM.md`，同层相关正本。
[3] 内部文档 `docs/engineering/data/ARTIFACT_STORE.md`，同层相关正本。

[4] 内部文档 `../governance/TRACEABILITY.md`，合同 ID 登记面（ENG-IO-001 登记处）。
