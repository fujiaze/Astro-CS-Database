# I/O & Atomicity

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> ⚠ **原子性覆盖与产物落点**：
> ① **原子性覆盖全部产品（含 HiPS tile）是最高设计 §10 的强制条款**；HiPS tile 写路径统一经
>    `write_fits_atomic`（临时文件 → 内容 → CHECKSUM 校验 → 原子改名，见下），**原子发布覆盖全部产品**；
> ② **产物落点**：产品**只落块级 `output_dir`**；`run/` 只放**临时产物与日志**
>    （最高设计 §10；`run/` 定位为临时产物与日志目录）。

- science product 写盘协议：temp write → validate → atomic promote（最高设计 §10）。
- UPM sparse 模型：aio_upm_write_sparse 为 temp+rename（lib/infrastructure/aio/src/aio_upm.cpp:64-116 temp write → validate → atomic promote）。
- **HiPS tile 原子发布**：tile 写统一入口 `write_fits_image` → `write_fits_atomic`
  （`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:630/:539`：同目录临时文件 → 内容写出 →
  CHECKSUM 校验 → fsync → 原子 rename → 父目录 fsync），全部 tile 写调用点
  （`aio_hips_writer.cpp:1018/:1028/:1082/:1092/:1369/:1377/:1585/:1595/:1747`）与 MOC 写（:657）
  自动经该路径；失败分类（ENOSPC/写失败）在清理前完成（`aio_disk_full.h` 语义）；
  负例可红：`lib/infrastructure/aio/tests/test_hips_atomic_publish.cpp`
  （`tile_diskfull` / `tile_write_fail` 注入）。
- ⚠ **待修缺口登记（未闭合）——阶段二直写无暂存区**：
  阶段二马赛克当前直写输出目录、无 staging（`docs/modules/registry/astrocs.phase2.write.md` §已知限制）；
- dense cache：固定 512B 头部 + 二进制块 + streaming checksum；打开校验
  checksum 与 source_hash。
- HiPS 写：先 tiles/properties 到目标目录，最后 properties/index；
  verify（CHECKCODE/CHECKDATASUM）后交付。
- 只读：testdata/ 的打开模式限于只读；**产品落块级 `output_dir`，`run/` 只放临时产物与日志**
  （最高设计 §10；写出位置取自显式配置，CWD 只标识进程自身位置）。

## 非生产 / 诊断接口登记

最高设计 §10 把 aio 定为**文件级唯一 I/O 边界**，并**穷举**允许的文件读写点。
下列符号是穷举点之外的「块 ↔ 文件」读写面，**已降级为非生产 / 诊断接口**
（代码侧归属声明见 `lib/infrastructure/aio/include/aio_pipeline.h:232-264`）：

| 符号 | 性质 | 处置 |
|---|---|---|
| `aio_frame_save_cache` | 整帧写缓存文件（`.aio` 自定义二进制） | **非生产 / 诊断**；阶段内节点的数据搬运只走命名块管线 |
| `aio_frame_load_cache` | 整帧读缓存文件 | 同上 |
| `aio_frame_export_block_fits` | 单块导出 FITS | 同上（调试导出） |
| `aio_frame_export_block_xml` | 单块导出 XML | 同上 |
| `aio_frame_export_all_xml` | 全块导出 XML | 同上 |
| `aio_pipeline_export_xml` | 旧名包装（= `aio_frame_export_all_xml`） | 同上 |

- 符号签名登记面 = `docs/architecture/api_inventory.csv`（该表**只登记函数签名**，不含生产性分级）；
  **生产性分级以本表为准**；
- 本表是文档侧登记面；生产可用性判据以各符号在役调用链为准。

## 契约

ENG-IO-001..003（S2 注册）。
