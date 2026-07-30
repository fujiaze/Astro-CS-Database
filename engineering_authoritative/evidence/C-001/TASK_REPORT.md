# C-001 任务执行报告 — 冻结 HISS V2 signal/support/SNR/provenance 契约

- 任务编号：C-001
- Gate：C
- 执行日期：2026-07-30
- 执行环境：PowerShell 7，Python 3（含 zstandard 0.25.0）
- 依赖：B-002（3 帧 V1 HISS 文件）、B-003（plate solve WCS）
- 状态：**完成（契约已 FROZEN）**

## 1. 任务目标

冻结 HISS V2 契约，含 signal / support / SNR / provenance 四要素，定义二进制布局、分块索引、压缩方案、校验和、batch read API，标记 FROZEN。禁止把无覆盖写成零、禁止 signal 量化 uint8、禁止只支持整文件读取。

## 2. 输入调查

### 2.1 现有 V1 实现源码

- `lib/astro_image_io/include/aio_healpix_io.h` — V1 公共 API（`aio_hiss_write`/`aio_hiss_read`/`aio_hiss_write_snr_model`/`aio_hiss_read_snr_model`）
- `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` — V1 实现，含 SNR 稀疏格式 snr_format=1
- `lib/astro_image_io/docs/HEALPIX_FORMAT_SPEC.md` — V1 格式规范（称"不含 WCS"，但实际实现已写入 wcs）
- `lib/healpix_db/healpix_stack/ahps_format.h` — `.ahps` 分块压缩格式（CHUNK_SIZE=4096，codec 枚举），V2 分块设计参考
- `lib/healpix_db/healpix_stack/hp_stack_hiss.h` — 堆叠器调用 `hiss_read`，V2 需保持兼容

### 2.2 V1 二进制结构（实测确认）

V1 `.hiss` 布局：
```
[HISS 4B][json_uncomp_len u32][json_comp_len u32][zstd JSON 头]
[ipix: n_pix×u64][pixel: n_pix×f32]
[SNR 块（snr_format=1）: n_points u32 | points n×20B{ra f64,dec f64,snr_psf f32} | snr_phot f64 | median_snr f64 | idw_power f64]
```

V1 缺陷：无版本号、无 support、无分块随机读取、无校验和、SNR 用 AoS。

### 2.3 B-002 三帧 V1 HISS 文件实测

用 Python + zstandard 实测 `output/B-002/` 三帧，确认 JSON 头实际字段：

| 帧 | magic | nside | n_pix | snr_format | snr_n_points | JSON 头字段 |
|---|---|---|---|---|---|---|
| T2_RED_LDN43 | HISS | 2048 | 1573 | 1 | 1930 | nside,nested,n_pix,has_snr,snr_format,snr_n_points,filter,exposure_s,obs_time,pixfrac,**wcs,fits_meta,source,drizzle** |
| T3_RED_NGC55 | HISS | 2048 | 1535 | 1 | 617 | 同上 |
| T4_RED_GalaxyCenter_panel1 | HISS | 512 | 3928 | 1 | 1984 | 同上 |

关键发现：V1 规范文档称"不含 WCS"，但 **V1 实现实际已写入 wcs/drizzle/source/fits_meta**。V2 provenance 直接采纳这些既有字段并补全版本号与配置字段，迁移无字段丢失风险。

实测 T2 文件大小 58076B 与结构推算一致（12 头 + 560 压缩 JSON + 1573×12 ipix+pixel + 4+1930×20+24 SNR 块 = 58076 ✓），SNR 块二进制布局经源码（aio_healpix_io.cpp:1158-1180）与文件偏移双重核实。

### 2.4 现有最小 schema

`engineering_authoritative/contracts/hiss_v2_minimum.schema.json` 已定义顶层骨架（format_version/nside/ordering/filter/signal/support/snr_model/provenance）。本次冻结的 HISS_FORMAT_V2.md 是其完整二进制契约细化，两者一致。

## 3. 交付物

| 文件 | 说明 |
|---|---|
| `engineering_authoritative/contracts/HISS_FORMAT_V2.md` | **FROZEN 契约文档**（17 章，692 行） |
| `engineering_authoritative/evidence/C-001/contract_summary.md` | 契约摘要（四要素 + 设计决策 + Gate C 对应） |
| `engineering_authoritative/evidence/C-001/TASK_REPORT.md` | 本报告 |

## 4. 契约核心内容

### 4.1 四要素

- **signal**：float32，n_pix，分块 zstd，禁止 uint8 量化
- **support**：uint8{0,1}，n_pix，与 signal 同块对齐，禁止"无覆盖=零"歧义
- **SNR**：稀疏 SoA（n_points + points_ra/dec/snr 三通道 + 3 全局标量），禁止全量
- **provenance**：JSON（format_version="HISS-V2" + nside/nested/n_pix/filter/exposure_s/obs_time/wcs/drizzle/pixfrac/fits_meta/source + chunk/codec/crc 配置）

### 4.2 二进制布局

```
[FIXED HEADER 24B: magic "HI2S" + version=2 + flags + json_len + n_pix]
[JSON PROVENANCE: zstd level=5]
[CHUNK INDEX: n_chunks × 24B (offset/comp_size/raw_count/crc32/codec/flags/reserved)]
[DATA CHUNKS: 每块 [ipix×u64][signal×f32][support×u8] 整体 zstd]
[SNR SPARSE BLOCK: n_points + 3 通道压缩 + 3 标量（可选）]
[FOOTER 48B: chunk_index_offset/size + snr_offset/size + global_crc32 + magic_trailer "HI2S"]
```

### 4.3 关键机制

- **magic HI2S**：与 V1 HISS 区分（第 3 字节 0x32 vs 0x53），读端按 magic 分派
- **分块索引**：CHUNK_SIZE=4096 像素/块，每块独立 zstd，O(1) 随机读取
- **batch read API**：7 个函数（read_provenance/read_chunk/read_chunks/read_ipix_range/read_leaf/read_snr_model/read_all），禁止只支持整文件读取
- **校验和**：CRC32 IEEE 802.3（zlib），per-chunk + global 双层 + footer magic_trailer
- **FROZEN**：固定头/块索引项/footer 布局/四要素类型/magic/version 不可改；仅允许 JSON 可选字段与 codec 枚举扩展

## 5. 禁止捷径复核

| 禁止项 | 契约对应 | 复核 |
|---|---|---|
| 不得把无覆盖写成零 | §7.2 support 通道 + §7.3 语义表 | ✓ support=0 的 signal 不得当零用 |
| 不得将 signal 量化为 uint8 | §7.1 float32 硬约束 | ✓ |
| 不得只支持整文件读取 | §11 七个 API | ✓ read_chunk/chunks/range/leaf 均支持随机读取 |
| 契约 FROZEN 不可更改 | §0 + §17 | ✓ 扩展只能加可选字段或新 magic |

## 6. Gate C 验收项对应

| Gate C Checklist | 契约对应章节 |
|---|---|
| HISS 格式版本化 | §5.1 version=2 + §5.2 format_version + magic HI2S |
| signal float32 | §7.1 |
| support 存在 | §7.2 |
| SNR 稀疏 | §8 |
| 分块随机读取 | §6 + §11 |
| 校验和损坏测试 | §12（实现需提供损坏测试，返回 -4） |
| 压缩往返一致 | §10.3 zstd 无损 |
| 浏览器可检查 | §11.5 read_leaf + §11.1 read_provenance |

## 7. 未解决问题与后续

- 本任务为**契约冻结**，不含代码实现。V2 读写器实现（`aio_hiss2_*`）属后续任务（C-002/C-003 等）。
- 契约 §12.4 损坏测试、§10.3 压缩往返测试、§6.4 随机读取测试均需在实现任务中提供可重跑证据。
- V1 → V2 迁移工具需在实现阶段开发，迁移测试输入可用 B-002 三帧 V1 文件。
- 本报告未修改 V1 任何代码或文件；V1 API 保持不变。

## 8. 可复现性

- 契约文档：`engineering_authoritative/contracts/HISS_FORMAT_V2.md`（FROZEN，可直接审阅）
- V1 文件实测命令：见本报告 §2.3（Python + zstandard 读取 magic/JSON 头/SNR 块偏移）
- 源码核实：`lib/astro_image_io/src/healpix/aio_healpix_io.cpp:1158-1180`（SNR 写入顺序）
- 无静默降级：契约明确禁止项均以"硬约束"标注，读端遇未知 codec/版本/CRC 失败必须报错（§11.8 错误码）
