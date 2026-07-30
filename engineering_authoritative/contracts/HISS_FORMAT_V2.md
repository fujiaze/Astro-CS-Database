# HISS Format V2 — FROZEN Contract

> **状态：FROZEN（冻结）**
> **冻结日期：2026-07-30**
> **冻结任务：C-001**
> **格式版本：HISS-V2 (version=2)**
> **文件扩展名：`.hiss`（v2，与 v1 通过 magic 区分）**
> **权威源（SSOT）：本文件。任何实现必须以本文件为唯一基准。**

---

## 0. FROZEN 声明

本契约自 2026-07-30 起冻结。冻结后**不得修改**已有字段语义、二进制布局、magic、版本号、校验算法或分块结构。如需扩展：

- **允许**：在 JSON provenance 头中新增可选字段（读端必须忽略未知字段，向前兼容）。
- **允许**：新增 codec 枚举值（读端遇未知 codec 应报错而非崩溃）。
- **禁止**：改动固定头布局、magic、version、chunk index 项布局、footer 布局、CRC32 算法、四要素（signal/support/SNR/provenance）的数据类型与语义。
- **不兼容变更**：必须升级 magic 与 version（如 `HI3S`/version=3），不得复用本契约的 magic。

任何对本文件的修订必须保留 FROZEN 原文，并以新版本号另起契约。

---

## 1. 概述

HISS（HEALPix Storage System）V2 是 AstroCS 项目单帧曝光的球面像素存储格式，保存 Drizzle 重投影后单张曝光帧的 signal、support、稀疏 SNR 模型与完整 provenance。

V2 在 V1 基础上冻结以下改进，解决 V1 的四类缺陷：

| V1 缺陷 | V2 冻结方案 |
|---|---|
| 无版本号字段（仅靠 magic 区分，magic 未升级） | 固定头显式 `version` 字段 + 新 magic `HI2S` |
| 无 support 通道，"无覆盖"与"零信号"语义混淆 | 独立 `support` 通道（uint8），硬约束：无覆盖不得写成零 |
| signal 为整块未压缩连续数组，无法分块随机读取 | signal/ipix/support 按固定像素数分块，每块独立 zstd，块索引支持 O(1) 随机读取 |
| 无校验和，文件损坏只能靠长度推断 | per-chunk CRC32 + 全局 CRC32 + footer 校验 |

V2 仍保持**稀疏存储**（仅存被覆盖像素），并保持 ipix 隐含球面坐标的设计。

---

## 2. 设计目标与硬约束

### 2.1 硬约束（禁止项，源自 Gate C / 任务 C-001）

1. **不得将 signal 量化为 uint8**。signal 必须为 float32（IEEE 754 单精度）。
2. **不得把无覆盖写成零**。无覆盖像素必须由 `support` 通道标记，读端不得将 `support=0` 的 signal 值当作零信号使用。
3. **不得只支持整文件读取**。必须支持基于分块索引的随机读取与批量读取（batch read）。
4. **SNR 不得全量存储**。SNR 必须以稀疏控制点格式存储（仅存非零/采样点）。
5. **契约冻结后不可更改**。

### 2.2 设计目标

- **随机访问**：任意子天区 / 任意像素区间可在不读取整文件的前提下获取。
- **压缩率**：数值数组按块独立 zstd 压缩，兼顾体积与随机访问。
- **完整性校验**：CRC32 覆盖头部、每块、全局，损坏可定位。
- **可演进**：JSON 头可加字段；codec 可扩展；二进制骨架不可变。
- **与 V1 可区分**：magic 与 version 双重区分，读端不会把 V2 当 V1。

---

## 3. 四要素总览

| 要素 | 数据类型 | 维度 | 存储方式 | 硬约束 |
|---|---|---|---|---|
| **signal** | float32 | n_pix（与 ipix 一一对应） | 分块 zstd 压缩 | 不得量化为 uint8 |
| **support** | uint8（0/1） | n_pix（与 signal 同维度） | 分块 zstd 压缩（与 signal 同块对齐） | 无覆盖不得写成零 |
| **SNR** | 稀疏控制点 | n_points 个采样点 | 独立稀疏块，分通道 zstd 压缩 | 不得全量存储 |
| **provenance** | JSON 对象 | 1 份 | zstd 压缩 UTF-8 JSON | 必须含版本号字段及 §5.2 全部必填字段 |

---

## 4. 文件二进制布局总图

```
┌──────────────────────────────────────────────────────────────┐
│ FIXED HEADER                                24 字节           │
│   magic "HI2S"              4B                               │
│   version (uint16 LE)       2B   = 2                         │
│   flags (uint16 LE)         2B   bit0=_dense_mode            │
│   json_uncomp_len (uint32)  4B                               │
│   json_comp_len (uint32)    4B                               │
│   n_pix (uint64 LE)         8B   存储像素数（稀疏=覆盖像素数）│
├──────────────────────────────────────────────────────────────┤
│ COMPRESSED JSON PROVENANCE HEAD             json_comp_len B  │
│   zstd level=5 压缩的 UTF-8 JSON（见 §5）                    │
├──────────────────────────────────────────────────────────────┤
│ CHUNK INDEX TABLE                           n_chunks × 24B   │
│   每项 24B（见 §6），n_chunks = ceil(n_pix / CHUNK_SIZE)     │
├──────────────────────────────────────────────────────────────┤
│ DATA CHUNKS（ipix + signal + support，每块独立 zstd）        │
│   chunk 0: [ipix×u64][signal×f32][support×u8] → zstd         │
│   chunk 1: ...                                               │
│   ...                                                        │
│   chunk n-1: （末块 raw_count 可 < CHUNK_SIZE）              │
├──────────────────────────────────────────────────────────────┤
│ SNR SPARSE BLOCK（可选，has_snr=true 时存在）                │
│   见 §8                                                      │
├──────────────────────────────────────────────────────────────┤
│ FOOTER                                      48 字节          │
│   chunk_index_offset (uint64)                                │
│   chunk_index_size (uint64)                                  │
│   snr_block_offset (uint64)   =0 表示无 SNR 块               │
│   snr_block_size (uint64)     =0 表示无 SNR 块               │
│   global_crc32 (uint32)       覆盖 FIXED HEADER→SNR 块末尾   │
│   reserved (uint32)           =0                             │
│   magic_trailer "HI2S"        4B                             │
└──────────────────────────────────────────────────────────────┘
```

**字节序**：所有多字节整数小端序（LE）。
**对齐**：段间不做显式对齐，紧跟前一段。读取按自然边界解析（x86 原生支持非对齐访问）。

**读端定位顺序**（支持只读尾部即可定位索引）：
1. 读 magic（偏移 0）→ 确认 `HI2S`。
2. seek 到 `filesize - 48` 读 footer。
3. 由 `chunk_index_offset` 读块索引表。
4. 按需读取指定 chunk 或 SNR 块。

---

## 5. FIXED HEADER 与 JSON PROVENANCE HEAD

### 5.1 FIXED HEADER（24 字节，固定布局，FROZEN）

| 偏移 | 长度 | 字段 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | 4 | `magic` | char[4] | `"HI2S"` (0x48 0x49 0x32 0x53) |
| 4 | 2 | `version` | uint16 LE | `=2`（V2 冻结值） |
| 6 | 2 | `flags` | uint16 LE | bit0=`dense_mode`（1=稠密存储全天 nside²×12 像素；0=稀疏存储仅覆盖像素，默认 0）；bit1..15 保留，必须为 0 |
| 8 | 4 | `json_uncomp_len` | uint32 LE | JSON 头压缩前字节数 |
| 12 | 4 | `json_comp_len` | uint32 LE | JSON 头压缩后字节数 |
| 16 | 8 | `n_pix` | uint64 LE | 存储像素数（稀疏模式=被覆盖像素数；稠密模式=nside²×12） |

固定头之后紧接 `json_comp_len` 字节的 zstd 压缩 JSON provenance head。

### 5.2 JSON PROVENANCE HEAD 必填字段（FROZEN）

JSON 头为 UTF-8 JSON 对象，zstd level=5 压缩。必填字段如下：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `format_version` | string | 是 | `"HISS-V2"`（与固定头 version=2 对应） |
| `nside` | uint32 | 是 | HEALPix nside 参数（必须为 2 的幂） |
| `ordering` | string | 是 | `"NESTED"` 或 `"RING"`（V2 默认 NESTED） |
| `nested` | bool | 是 | 与 ordering 对应（true=NESTED）；为兼容 V1 读端保留 |
| `n_pix` | uint64 | 是 | 存储像素数（与固定头 n_pix 必须一致） |
| `filter` | string | 是 | 滤光片名称（如 `"Red"`,`"Green"`,`"Blue"`,`"Lum"`,`"Ha"`） |
| `exposure_s` | float | 是 | 单帧曝光时间（秒） |
| `obs_time` | string | 是 | 观测时间 ISO 8601（UTC，如 `"2025-05-03T03:27:21Z"`） |
| `pixfrac` | float | 是 | Drizzle pixfrac 参数 |
| `wcs` | object | 是 | WCS 信息（见 §5.3） |
| `drizzle` | object | 是 | Drizzle 参数（见 §5.4） |
| `fits_meta` | object | 是 | 原始 FITS 头关键信息子集（见 §5.5） |
| `source` | object | 是 | 溯源信息（见 §5.6） |
| `has_snr` | bool | 是 | 是否含 SNR 稀疏块 |
| `chunk_size` | uint32 | 是 | 分块像素数（FROZEN 默认 4096；见 §6） |
| `n_chunks` | uint32 | 是 | 分块数 = ceil(n_pix / chunk_size) |
| `codec` | string | 是 | 数据块压缩编码（`"ZSTD"`/`"NONE"`/`"LZ4"`，默认 `"ZSTD"`） |
| `crc_algorithm` | string | 是 | 校验算法（FROZEN `"CRC32_IEEE8023"`） |

**版本号字段**：`format_version`（JSON）与固定头 `version`（=2）双重标记。读端必须两者一致，不一致视为损坏。

### 5.3 `wcs` 对象

```json
{
  "crval": [248.6096556109, -15.7591304856],
  "crpix": [2048.5, 2048.5],
  "cd": [3.19778e-06, 2.68598e-04, -2.68577e-04, 3.26440e-06],
  "sip_order": 3,
  "sip_a": null,
  "sip_b": null,
  "sip_ap": null,
  "sip_bp": null
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `crval` | float[2] | 中心赤经赤纬（度） |
| `crpix` | float[2] | 参考像素（1-based） |
| `cd` | float[4] | CD 矩阵（行优先 2×2） |
| `sip_order` | uint32 | SIP 多项式阶数（0=无 SIP） |
| `sip_a`/`sip_b` | float[][]\|null | SIP 正向系数（A_ORDER/B_ORDER），无则为 null |
| `sip_ap`/`sip_bp` | float[][]\|null | SIP 逆向系数（AP_ORDER/BP_ORDER），无则为 null |

V1 实际已写入 wcs（尽管 V1 规范文档称"不含 WCS"）；V2 将其纳入 provenance 必填字段。

### 5.4 `drizzle` 对象

```json
{
  "n_healpix_pixels": 1573,
  "n_source_pixels": 16777216,
  "elapsed_sec": 14.5278,
  "pixfrac": 1.0
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `n_healpix_pixels` | uint64 | Drizzle 命中的输出 HEALPix 像素数（= 稀疏模式下 support=1 的像素数） |
| `n_source_pixels` | uint64 | 输入图像参与投影的源像素数 |
| `elapsed_sec` | float | Drizzle 阶段耗时（秒） |
| `pixfrac` | float | Drizzle pixfrac（与顶层 pixfrac 一致） |

### 5.5 `fits_meta` 对象

保留原始 FITS 头用于溯源与质控的关键字段子集（按需，至少含下列键之一存在时的值）：

`OBJCTRA`, `OBJCTDEC`, `OBJECT`, `IMAGETYP`, `INSTRUME`, `TELESCOP`, `SITELAT`, `SITELONG`, `XPIXSZ`, `YPIXSZ`, `XPIXSZ`, `FOCALLEN`, `XBINNING`, `YBINNING`, `GAIN`, `RADESYS`, `EQUINOX`, `EXPTIME`, `DATE-OBS`。

读端必须忽略未知 fits_meta 键。

### 5.6 `source` 对象

```json
{
  "fits_path": "testdata/NGC55_T3_.../NGC55_T3_...-600S-Red.fts",
  "frame_id": "T3_RED_NGC55",
  "n_source_pixels": 16777216,
  "orchestrator_version": "2.0",
  "pipeline_stage": "stage1"
}
```

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `fits_path` | string | 是 | 原始 FITS 路径（可为空字符串，如中文路径规避场景） |
| `frame_id` | string | 是 | 帧标识 |
| `n_source_pixels` | uint64 | 是 | 源像素数 |
| `orchestrator_version` | string | 否 | 产生该文件的 orchestrator 版本 |
| `pipeline_stage` | string | 否 | 产生阶段（如 `"stage1"`） |

### 5.7 JSON 头示例

```json
{
  "format_version": "HISS-V2",
  "nside": 2048,
  "ordering": "NESTED",
  "nested": true,
  "n_pix": 1573,
  "filter": "Red",
  "exposure_s": 1200.0,
  "obs_time": "2025-05-03T03:27:21Z",
  "pixfrac": 1.0,
  "wcs": { "crval": [248.609, -15.759], "crpix": [2048.5, 2048.5],
           "cd": [3.197e-06, 2.685e-04, -2.685e-04, 3.264e-06],
           "sip_order": 3, "sip_a": null, "sip_b": null, "sip_ap": null, "sip_bp": null },
  "drizzle": { "n_healpix_pixels": 1573, "n_source_pixels": 16777216, "elapsed_sec": 14.5278, "pixfrac": 1.0 },
  "fits_meta": { "OBJECT": "LDN43", "INSTRUME": "FLI", "IMAGETYP": "Light Frame",
                 "XPIXSZ": "9", "EXPTIME": "1200", "RADESYS": "ICRS", "EQUINOX": "2000" },
  "source": { "fits_path": "", "frame_id": "T2_RED_LDN43", "n_source_pixels": 16777216 },
  "has_snr": true,
  "chunk_size": 4096,
  "n_chunks": 1,
  "codec": "ZSTD",
  "crc_algorithm": "CRC32_IEEE8023"
}
```

---

## 6. CHUNK INDEX TABLE（分块索引，FROZEN）

### 6.1 分块策略

- **CHUNK_SIZE**：固定每块 `chunk_size` 个像素（FROZEN 默认 `4096`，与 `.ahps` 一致）。
- `n_chunks = ceil(n_pix / chunk_size)`，末块 `raw_count` 可小于 `chunk_size`。
- ipix / signal / support 三数组按**相同像素分块对齐**：块 `i` 覆盖像素索引区间 `[i*chunk_size, min((i+1)*chunk_size, n_pix))`。
- 每块的 ipix+signal+support 打包成连续缓冲后**整体 zstd 压缩为单个数据块**，块索引只记一项。

### 6.2 块索引项布局（24 字节/项，FROZEN）

| 偏移 | 长度 | 字段 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | 8 | `offset` | uint64 LE | 该块压缩数据在文件中的字节偏移 |
| 8 | 4 | `comp_size` | uint32 LE | 压缩后字节数 |
| 12 | 4 | `raw_count` | uint32 LE | 该块原始像素数（末块可 < chunk_size） |
| 16 | 4 | `crc32` | uint32 LE | 该块**压缩后数据**的 CRC32（IEEE 802.3） |
| 20 | 1 | `codec` | uint8 | 0=NONE, 1=ZSTD, 2=LZ4（见 §10） |
| 21 | 1 | `flags` | uint8 | 块级标志，保留，必须为 0 |
| 22 | 2 | `reserved` | uint16 LE | 保留，必须为 0 |

块索引表总大小 = `n_chunks × 24` 字节，紧接 JSON 头之后存放。

### 6.3 数据块内部布局（解压后）

单个数据块解压后的连续缓冲布局（按像素数 `raw_count`）：

```
[ipix:    raw_count × 8B  (uint64 LE, 升序)]
[signal:  raw_count × 4B  (float32 LE)]
[support: raw_count × 1B  (uint8, 0/1)]
```

块内解压缓冲大小 = `raw_count × 13` 字节。读端按偏移切分三段。

### 6.4 随机读取语义

- 给定像素索引 `p`，其所在块 `chunk_idx = p / chunk_size`，块内位置 `p % chunk_size`。
- 读块 `i`：seek 到 `chunk_index[i].offset`，读 `comp_size` 字节，校验 CRC32，按 `codec` 解压，切分 ipix/signal/support。
- **batch read**：见 §11。
- ipix 在块内仍按升序排列，块间全局升序（块 i 的最大 ipix < 块 i+1 的最小 ipix），支持跨块二分查找。

---

## 7. SIGNAL 与 SUPPORT 语义（FROZEN）

### 7.1 signal

- 类型：float32（IEEE 754 单精度），小端序。
- 维度：n_pix，与 ipix 一一对应。
- **禁止量化为 uint8**（硬约束 §2.1.1）。
- 语义：球面像素的信号值（Drizzle 重投影后的像素值）。
- 分块压缩存储，每块独立 zstd。

### 7.2 support

- 类型：uint8，取值 `{0, 1}`。
- 维度：n_pix，与 signal 同维度、同块对齐。
- 语义：
  - `support=1`：该像素被观测覆盖（Drizzle 命中），signal 有效。
  - `support=0`：该像素无覆盖，signal 值**无意义**，读端**不得将其当作零信号使用**（硬约束 §2.1.2）。
- **稀疏模式（flags.dense_mode=0，默认）**：数组仅含 `support=1` 的像素（ipix 升序）。此时 support 数组值恒为 1，但**必须显式存储**，以保持维度一致并为稠密模式预留。
- **稠密模式（flags.dense_mode=1）**：数组含全天 `nside² × 12` 像素，support 标记覆盖，无覆盖像素 signal 可为任意值（读端必须查 support）。
- 读端规则：使用 signal 前必须先检查对应 support；`support=0` 的像素不得参与统计、叠加或显示。

### 7.3 无覆盖与零信号的区分

| 场景 | support | signal | 含义 |
|---|---|---|---|
| 像素被覆盖且有信号 | 1 | 任意 float32 | 有效信号 |
| 像素被覆盖但信号为 0 | 1 | 0.0 | 有效零信号（真实测量值） |
| 像素无覆盖（稀疏模式） | 不存储 | 不存储 | 不在数组中 |
| 像素无覆盖（稠密模式） | 0 | 任意（无意义） | 必须查 support 判定 |

此设计彻底消除 V1 中"零=无覆盖"的歧义。

---

## 8. SNR SPARSE BLOCK（稀疏信噪比，FROZEN）

### 8.1 设计原则

SNR **不得全量存储**（硬约束 §2.1.4）。V2 采用稀疏控制点 + IDW 插值模型，仅存储非零采样点的球面坐标与 SNR 值，外加全局标量参数。读端按需插值还原逐像素 SNR。

### 8.2 SNR 块二进制布局

SNR 块紧接最后一个数据块之后存放（仅 `has_snr=true` 时存在）。布局：

```
┌────────────────────────────────────────────────────────────┐
│ n_points                   4B   (uint32 LE)  控制点数      │
├────────────────────────────────────────────────────────────┤
│ points_ra  压缩块          [zstd: n_points × f64 LE]       │
│   布局: [comp_len u32][raw_len u32][zstd 数据 comp_len B]  │
├────────────────────────────────────────────────────────────┤
│ points_dec 压缩块          [zstd: n_points × f64 LE]       │
│   布局: [comp_len u32][raw_len u32][zstd 数据 comp_len B]  │
├────────────────────────────────────────────────────────────┤
│ points_snr 压缩块          [zstd: n_points × f32 LE]       │
│   布局: [comp_len u32][raw_len u32][zstd 数据 comp_len B]  │
├────────────────────────────────────────────────────────────┤
│ snr_phot     8B  (f64 LE)  1/(ln10×sigma_residual)         │
│ median_snr   8B  (f64 LE)  median(snr_psf) 归一化基准      │
│ idw_power    8B  (f64 LE)  IDW 幂次（默认 2.0）            │
└────────────────────────────────────────────────────────────┘
```

**字段命名**（FROZEN，对应任务要求）：
- `n_points`：控制点数（uint32）
- `points_ra`：各点赤经（度，f64）— SoA（Structure of Arrays）布局
- `points_dec`：各点赤纬（度，f64）
- `points_snr`：各点 SNR 值（(A-B)/mad，无量纲，f32）

**与 V1 区别**：V1 用 AoS（`points: n×20B {ra,dec,snr}`）；V2 改用 SoA 三通道分块压缩，压缩率更高且便于按通道批量读取。V1 的 `snr_psf` 字段在 V2 重命名为 `points_snr`（语义不变）。

### 8.3 每通道压缩块格式

points_ra / points_dec / points_snr 各自独立 zstd 压缩，前置 8 字节头：

| 偏移 | 长度 | 字段 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | 4 | `comp_len` | uint32 LE | 压缩后字节数（=0 表示未压缩，raw_len 字节直接跟） |
| 4 | 4 | `raw_len` | uint32 LE | 压缩前字节数（= n_points × elem_size，elem_size: ra/dec=8, snr=4） |
| 8 | `comp_len` 或 `raw_len` | 数据 | bytes | zstd 压缩数据（comp_len=0 时为未压缩原始数据） |

读端：先读 8B 头，若 `comp_len=0` 则直接读 `raw_len` 字节；否则读 `comp_len` 字节并 zstd 解压到 `raw_len` 字节。

### 8.4 SNR 块在 footer 中的定位

footer 的 `snr_block_offset` 与 `snr_block_size` 记录 SNR 块在文件中的偏移与总字节数。`has_snr=false` 时两者均为 0。

---

## 9. FOOTER（48 字节，FROZEN）

文件尾固定 48 字节，位于文件末尾。读端可先 seek 到 `filesize - 48` 读 footer 定位所有结构。

| 偏移 | 长度 | 字段 | 类型 | 说明 |
|---|---|---|---|---|
| 0 | 8 | `chunk_index_offset` | uint64 LE | 块索引表在文件中的字节偏移 |
| 8 | 8 | `chunk_index_size` | uint64 LE | 块索引表字节数（= n_chunks × 24） |
| 16 | 8 | `snr_block_offset` | uint64 LE | SNR 块字节偏移（无 SNR=0） |
| 24 | 8 | `snr_block_size` | uint64 LE | SNR 块字节数（无 SNR=0） |
| 32 | 4 | `global_crc32` | uint32 LE | 全局 CRC32（见 §12.3） |
| 36 | 4 | `reserved` | uint32 LE | =0 |
| 40 | 4 | `magic_trailer` | char[4] | `"HI2S"`（与头部 magic 一致） |
| 44 | 4 | _pad_ | bytes | =0x00×4（对齐到 48B） |

**校验**：读端必须校验 `magic_trailer == "HI2S"`，否则文件不完整或被截断。

---

## 10. 压缩方案（FROZEN）

### 10.1 压缩算法

- **主算法：zstd**，默认压缩级别 `level=5`（与 V1 JSON 头一致）。
- 允许的 codec 枚举（块索引 `codec` 字段与 JSON 头 `codec` 字段）：

| codec 值 | 名称 | 说明 |
|---|---|---|
| 0 | NONE | 不压缩（raw_len 字节直接存） |
| 1 | ZSTD | Zstd（默认，高压缩率） |
| 2 | LZ4 | LZ4（快速，可选） |

- 读端遇未知 codec 值必须返回错误，不得静默降级。
- 不同数据块可使用不同 codec（块索引逐块记录），但同一文件默认全部 ZSTD。

### 10.2 压缩对象

| 数据区 | 压缩方式 |
|---|---|
| JSON provenance head | 整体 zstd level=5 |
| 每个数据块（ipix+signal+support） | 整体 zstd level=5（块内三段打包后压缩） |
| SNR points_ra / points_dec / points_snr | 各通道独立 zstd level=5 |
| 块索引表 | 不压缩（便于直接读取） |
| footer | 不压缩 |

### 10.3 压缩往返一致性

写端必须保证：解压后的数据与压缩前逐字节一致。Gate C 验收要求"压缩往返一致"：写后立即读回，signal/support/ipix/snr 与原数据逐字节相等。zstd 为无损压缩，满足此要求。

---

## 11. BATCH READ API（FROZEN）

V2 必须支持以下读取模式，**不得只支持整文件读取**（硬约束 §2.1.3）。API 以 C 风格声明（实现语言无关，函数签名 FROZEN，参数语义 FROZEN）：

### 11.1 读 provenance（只读头部，不读数据）

```c
// 只读 FIXED HEADER + JSON provenance head，返回解析后的 provenance 与 n_pix/n_chunks。
// 不读取任何数据块。用于快速元数据扫描。
int hiss2_read_provenance(const char* path,
                           uint16_t* version, uint16_t* flags,
                           uint64_t* n_pix,
                           char** provenance_json,  // malloc, 调用者 free
                           uint32_t* chunk_size, uint32_t* n_chunks);
```

### 11.2 读单个块（随机读取）

```c
// 读取第 chunk_idx 个数据块，返回该块的 ipix/signal/support。
// raw_count 为该块像素数（末块可能 < chunk_size）。
// 调用者负责 free ipix/signal/support。
int hiss2_read_chunk(const char* path, uint32_t chunk_idx,
                      uint32_t* raw_count,
                      uint64_t** ipix, float** signal, uint8_t** support);
```

### 11.3 批量读多个块（batch read）

```c
// 批量读取多个块，返回拼接后的 ipix/signal/support（按 chunk 顺序、全局 ipix 升序）。
// chunk_indices: 欲读取的块号数组；n: 块数。
// total_count: 返回的总像素数。
// 实现应合并对同一文件的多次 seek，或并行解压多块。
int hiss2_read_chunks(const char* path,
                       const uint32_t* chunk_indices, uint32_t n,
                       uint64_t* total_count,
                       uint64_t** ipix, float** signal, uint8_t** support);
```

### 11.4 按 ipix 区间读取（范围查询）

```c
// 读取 ipix 落在 [ipix_lo, ipix_hi] 区间内的所有像素。
// 内部通过块索引二分定位相关块，仅读取必要块。
int hiss2_read_ipix_range(const char* path,
                           uint64_t ipix_lo, uint64_t ipix_hi,
                           uint64_t* count,
                           uint64_t** ipix, float** signal, uint8_t** support);
```

### 11.5 按 HEALPix 子叶读取（leaf tile，可选）

```c
// 读取 nside=64 子叶 leaf_ipix 内的所有像素（基于 nested 位运算 ipix>>shift 定位）。
// 适用于浏览器视口按需加载。nside<64 时返回错误。
int hiss2_read_leaf(const char* path, uint64_t leaf_ipix,
                     uint64_t* count,
                     uint64_t** ipix, float** signal, uint8_t** support);
```

### 11.6 读 SNR 稀疏模型

```c
typedef struct {
    uint32_t n_points;
    double*  points_ra;    // malloc, n_points
    double*  points_dec;   // malloc, n_points
    float*   points_snr;   // malloc, n_points
    double   snr_phot;
    double   median_snr;
    double   idw_power;
} Hiss2SnrModel;

int hiss2_read_snr_model(const char* path, Hiss2SnrModel** out_model);
void hiss2_free_snr_model(Hiss2SnrModel* model);
```

### 11.7 整文件读取（兼容，仅便利用途）

```c
// 整文件读取，等价于 read_chunks(0..n_chunks-1) + read_snr_model。
// 仅用于小文件或调试；大文件应使用分块/范围 API。
int hiss2_read_all(const char* path,
                    uint64_t* n_pix,
                    uint64_t** ipix, float** signal, uint8_t** support,
                    Hiss2SnrModel** snr_model,
                    char** provenance_json);
```

### 11.8 错误码

| 值 | 含义 |
|---|---|
| 0 | 成功 |
| -1 | 文件不存在 / 无法打开 |
| -2 | magic 不匹配（非 `HI2S`） |
| -3 | version 不支持 |
| -4 | CRC32 校验失败（文件损坏） |
| -5 | chunk 索引越界 |
| -6 | codec 不支持 |
| -7 | JSON 解析失败 / 必填字段缺失 |
| -8 | footer magic_trailer 不匹配（文件截断） |
| -9 | 内存分配失败 |
| -10 | has_snr=false 但请求读 SNR |

---

## 12. 校验和（CRC32，FROZEN）

### 12.1 算法

- **算法：CRC32 IEEE 802.3**（多项式 `0xEDB88320`，与 zlib `crc32()` 一致）。
- 初始值 `0xFFFFFFFF`，最终异或 `0xFFFFFFFF`，输入输出均不反转。
- JSON 头 `crc_algorithm` 字段 FROZEN 为 `"CRC32_IEEE8023"`。
- 选 CRC32 而非 XXHash 的理由：检测能力对单/多 bit 错误充分，实现广泛可用（zlib 内置），与多数工具链兼容，校验开销可忽略。

### 12.2 Per-chunk CRC32

块索引项 `crc32` 字段记录该块**压缩后数据**（`comp_size` 字节）的 CRC32。读端解压前先校验，损坏可定位到具体块。

### 12.3 全局 CRC32

footer `global_crc32` 覆盖范围：从文件偏移 0（FIXED HEADER 起始）到 SNR 块末尾（即 footer 之前的所有字节）。计算顺序：

1. 写端写完 FIXED HEADER → JSON 头 → 块索引 → 数据块 → SNR 块后，对 `[0, filesize - 48)` 区间计算 CRC32。
2. 将结果写入 footer `global_crc32`，再写 footer（footer 自身不参与全局 CRC32）。

读端校验顺序：
1. 读 footer，校验 `magic_trailer`。
2. 对 `[0, filesize - 48)` 计算 CRC32，与 footer `global_crc32` 比对。
3. 逐块校验 per-chunk CRC32（按需，读取某块时校验）。

### 12.4 损坏测试（Gate C 验收）

实现必须提供损坏测试：人为翻转文件中某字节，验证 `hiss2_read_chunk` 返回 `-4`（CRC 失败），且不返回错误数据。Gate C checklist "校验和损坏测试" 项由此验收。

---

## 13. 读端校验规则（FROZEN）

读端必须在以下环节校验，任一失败返回对应错误码：

1. **Magic**：偏移 0 四字节 == `"HI2S"`（否则 -2）。
2. **Version**：固定头 `version == 2`（否则 -3）。
3. **Footer magic**：`filesize - 4` 处四字节 == `"HI2S"`（否则 -8，文件截断）。
4. **JSON 头解压**：解压后字节数 == `json_uncomp_len`（否则 -7）。
5. **JSON 必填字段**：§5.2 全部必填字段存在（否则 -7）。
6. **n_pix 一致性**：固定头 `n_pix` == JSON `n_pix` == `sum(chunk_index[i].raw_count)`（否则 -7）。
7. **全局 CRC32**：`[0, filesize-48)` CRC32 == footer `global_crc32`（否则 -4）。
8. **Per-chunk CRC32**：读取某块时，该块压缩数据 CRC32 == `chunk_index[i].crc32`（否则 -4）。
9. **块索引越界**：`chunk_idx < n_chunks`（否则 -5）。
10. **文件大小**：`filesize == chunk_index_offset + chunk_index_size + sum(comp_size) + snr_block_size + 48`（否则文件损坏）。

---

## 14. 与 V1 的兼容性与迁移

### 14.1 区分 V1 / V2

| | V1 | V2 |
|---|---|---|
| magic | `HISS` (0x48 0x49 0x53 0x53) | `HI2S` (0x48 0x49 0x32 0x53) |
| version 字段 | 无（magic 即版本） | 固定头 `version=2` |
| support 通道 | 无 | 有（uint8） |
| 分块随机读取 | 不支持（整块连续） | 支持（chunk index） |
| 校验和 | 无 | CRC32（per-chunk + global） |
| SNR 布局 | AoS `{ra,dec,snr}` 20B/点 | SoA 三通道分块压缩 |
| SNR 字段名 | `points[].snr_psf` | `points_snr[]` |
| footer | 无 | 48B |
| provenance | JSON 头（部分字段） | JSON 头（必填字段完整 + 版本号） |

读端先读 magic：`HISS` → V1 路径（`aio_hiss_read`）；`HI2S` → V2 路径（`hiss2_read_*`）。

### 14.2 V1 → V2 迁移

V1 文件迁移到 V2 必须重新序列化（非原地升级）：

1. 读 V1：`aio_hiss_read_snr_model` 获取 ipix/pixel/snr_model/meta。
2. 构造 support：V1 无 support，迁移时 support[i]=1（所有存储像素均为覆盖像素）。
3. 构造 provenance：V1 JSON 头已有 wcs/drizzle/source/fits_meta（实际已写入），补全 `format_version`、`ordering`、`chunk_size`、`n_chunks`、`codec`、`crc_algorithm`、`has_snr`。
4. SNR 模型：V1 AoS `{ra,dec,snr_psf}` 拆为 V2 SoA `points_ra/points_dec/points_snr`（`snr_psf` → `points_snr`，语义不变）。
5. 写 V2：分块、压缩、计算 CRC32、写 footer。

迁移不得丢失 V1 的 wcs/drizzle/source/fits_meta 字段。

### 14.3 现有 V1 文件现状（B-002 三帧）

B-002 产出的 3 帧 V1 HISS 文件（`output/B-002/T2_RED_LDN43.hiss` 等）实际 JSON 头已含 `wcs`/`drizzle`/`source`/`fits_meta` 字段（尽管 V1 规范文档称"不含 WCS"），SNR 为 `snr_format=1` 稀疏控制点。这些文件可作为 V2 迁移测试输入。

| 帧 | magic | nside | n_pix | has_snr | snr_format | snr_n_points | JSON 头实际字段 |
|---|---|---|---|---|---|---|---|
| T2_RED_LDN43 | HISS | 2048 | 1573 | true | 1 | 1930 | nside,nested,n_pix,has_snr,snr_format,snr_n_points,filter,exposure_s,obs_time,pixfrac,wcs,fits_meta,source,drizzle |
| T3_RED_NGC55 | HISS | 2048 | 1535 | true | 1 | 617 | 同上 |
| T4_RED_GalaxyCenter_panel1 | HISS | 512 | 3928 | true | 1 | 1984 | 同上 |

---

## 15. 字节序与对齐

- 所有多字节整数小端序（LE，x86 native）。
- 段间不显式对齐，紧跟前一段。
- 块索引项固定 24 字节，footer 固定 48 字节，固定头固定 24 字节——这些尺寸 FROZEN，不得改动。
- 浮点数使用 IEEE 754（float32 单精度，float64 双精度），小端序。

---

## 16. 参考实现位置

V2 参考实现应位于 `lib/astro_image_io/`，与 V1 共存：

- `include/aio_hiss2.h` — V2 公共 API（§11）
- `src/healpix/aio_hiss2_writer.cpp` — V2 写入器（分块、压缩、CRC32、footer）
- `src/healpix/aio_hiss2_reader.cpp` — V2 读取器（provenance/chunk/range/leaf/snr/all）
- `src/healpix/aio_hiss2_crc32.cpp` — CRC32 IEEE 802.3 实现（或复用 zlib `crc32`）

压缩层复用 `astro_image_io` 的 `aio_compress`/`aio_decompress` C API（codec=1=zstd）。

V1 API（`aio_hiss_read` 等）保持不变，通过 magic 分派。

---

## 17. FROZEN 摘要

以下要素一经冻结不可更改：

- magic `HI2S`、version `2`
- 固定头 24B 布局
- JSON provenance 必填字段集合与 `format_version="HISS-V2"`
- 块索引项 24B 布局、CHUNK_SIZE 默认 4096
- 数据块内部 `ipix+signal+support` 打包布局
- signal=float32、support=uint8、四要素语义
- SNR SoA 三通道布局与字段名 `n_points/points_ra/points_dec/points_snr`
- footer 48B 布局
- CRC32 IEEE 802.3 校验算法
- batch read API 函数签名与错误码
- 读端校验规则

扩展只能通过：JSON 可选字段新增、codec 枚举新增（不得删除已有值）、或新 magic/version 另起契约。

---

**— END OF FROZEN CONTRACT —**
