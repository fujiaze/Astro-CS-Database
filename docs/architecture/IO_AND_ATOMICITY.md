# I/O & Atomicity

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> ⚠ **原子性覆盖与产物落点**：
> ① **原子性覆盖全部产品（含 HiPS tile）是最高设计 §9 的强制条款**；HiPS tile 当前的非原子路径
>    属**已登记的待修缺口**（见下），**未修完前不得声称「原子发布已全覆盖」**；
> ② **产物落点**：产品**只落块级 `output_dir`**；`run/` 只放**临时产物与日志**
>    （最高设计 §9；**禁止**把 `run/` 说成「唯一运行输出目录」）。

- science product 写盘协议：temp write → validate → atomic promote（最高设计 §9）。
- UPM sparse 模型：aio_upm_write_sparse 已于 V19R6R2 改为 temp+rename（lib/infrastructure/aio/src/aio_upm.cpp:60-97 temp write → validate → atomic promote，F-V19R2-IO-001 已修复）。
- ⚠ **待修缺口登记（R13，未闭合）——HiPS tile 非原子**：
  现状 = `remove → fits_create → write_chksum → close`（`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:330/:399`
  的 `std::remove`），partial-file 策略：abort 尽力清理、finalize verify（失败清理 temp/partial，
  交付前 verify CHECKCODE/CHECKDATASUM）；**不是** temp+rename 原子发布。
  **闭合判据** = tile 走「临时区 → 校验 → 哈希 → 原子改名」且负例可红。
  **未闭合期间**本行保持「缺口」语义：禁止任何文档声称 HiPS tile 已原子发布
  （最高设计 §9「本期例外（如实登记）」）。
- ⚠ **待修缺口登记（R13 关联，未闭合）——阶段二直写无暂存区**：
  阶段二马赛克当前直写输出目录、无 staging（`docs/modules/registry/astrocs.phase2.write.md` §已知限制）；
- dense cache：固定 512B 头部 + 二进制块 + streaming checksum；打开校验
  checksum 与 source_hash。
- HiPS 写：先 tiles/properties 到目标目录，最后 properties/index；
  verify（CHECKCODE/CHECKDATASUM）后交付。
- 只读：testdata/ 禁止写入；**产品落块级 `output_dir`，`run/` 只放临时产物与日志**
  （最高设计 §9；**禁止**以进程 CWD 作隐式缺省）。

## 非生产 / 诊断接口登记

最高设计 §9 把 aio 定为**文件级唯一 I/O 边界**，并**穷举**允许的文件读写点。
下列符号是穷举点之外的「块 ↔ 文件」读写面，**已降级为非生产 / 诊断接口**
（代码侧归属声明见 `lib/infrastructure/aio/include/aio_pipeline.h:205-247`）：

| 符号 | 性质 | 处置 |
|---|---|---|
| `aio_frame_save_cache` | 整帧写缓存文件（`.aio` 自定义二进制） | **非生产 / 诊断**；**禁止**任何阶段内节点用其搬运数据 |
| `aio_frame_load_cache` | 整帧读缓存文件 | 同上 |
| `aio_frame_export_block_fits` | 单块导出 FITS | 同上（调试导出） |
| `aio_frame_export_block_xml` | 单块导出 XML | 同上 |
| `aio_frame_export_all_xml` | 全块导出 XML | 同上 |
| `aio_pipeline_export_xml` | 旧名包装（= `aio_frame_export_all_xml`） | 同上 |

- 符号签名登记面 = `docs/architecture/api_inventory.csv`（该表**只登记函数签名**，不含生产性分级）；
  **生产性分级以本表为准**；
- 本表是文档侧登记面，**不得**据本表推断它们可用于生产。

## 契约

ENG-IO-001..003（S2 注册）。
