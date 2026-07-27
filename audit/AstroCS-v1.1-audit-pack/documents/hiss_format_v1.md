# HISS 格式规范 v1.0 (合约冻结)

- 合约 ID: HISS-FMT-V1
- 任务: P01-003
- 来源代码: `lib/astro_image_io/src/healpix/aio_healpix_io.cpp`（`aio_hiss_write`, `aio_hiss_read`, `aio_hiss_write_snr_model`, `aio_hiss_read_snr_model`）
- 来源头文件: `lib/astro_image_io/include/aio_healpix_io.h`
- 文档边界: `engineering/docs/03_END_TO_END_DATAFLOW_AND_LIFETIME.md` §5
- 冻结时间: 2026-07-25
- 状态: **FROZEN**（v1.0 实现现状冻结，v1.1+ 演进见末尾"已知缺口"）

## 1. 概述

HISS (HEALPix Storage System) 是单帧稀疏球面像素存储格式，由 Stage1 Drizzle 模块产出，作为 Stage2 Stack 模块的输入单位。每个 HISS 文件对应一个原始输入帧，包含 ipix 索引、像素值、可选 SNR 通道（逐像素或稀疏控制点）和 JSON 元数据头。

## 2. 文件总体布局

```
+--------------------------- offset=0
| Magic "HISS"              | 4 字节
+--------------------------- offset=4
| uncomp_json_len           | uint32 LE  (JSON 头未压缩字节数)
| comp_json_len             | uint32 LE  (JSON 头 zstd 压缩后字节数)
+--------------------------- offset=12
| compressed_json           | comp_json_len 字节 (zstd level=5)
+--------------------------- offset = 12 + comp_json_len
| ipix[n_pix]               | uint64 LE × n_pix
+--------------------------- offset = 12 + comp_json_len + 8*n_pix
| pixel[n_pix]              | float32 LE × n_pix
+--------------------------- offset = 12 + comp_json_len + 12*n_pix
| [snr 通道]                | 可选，见 §5
+---------------------------
```

## 3. Magic 与字节序

- Magic: 4 字节 ASCII = `"HISS"` = `0x48 0x49 0x53 0x53`
- 字节序: 全字段小端序（x86 native，写入端不显式记录字节序标记，由 magic 隐式约定）
- 解析时若 magic 不匹配返回错误码 `HIO_ERR_MAGIC = -3`

## 4. JSON 头（zstd 压缩）

### 4.1 头部结构

- 8 字节长度前缀：`uncomp_json_len` (u32 LE) + `comp_json_len` (u32 LE)
- 紧跟 `comp_json_len` 字节的 zstd 压缩数据（压缩级别 5）
- 解压后为 UTF-8 JSON 对象字符串（无 null 终止）

### 4.2 必填字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `nside` | uint32 | HEALPix nside 参数（必须为 2 的幂） |
| `nested` | bool | true=NESTED 排序，false=RING 排序 |
| `n_pix` | uint64 | 稀疏像素数（ipix/pixel 数组长度） |

### 4.3 SNR 通道字段（可选）

| 字段 | 类型 | 出现条件 | 说明 |
|---|---|---|---|
| `has_snr` | bool | 总是（旧文件缺失则默认 false） | 是否包含 SNR 通道 |
| `snr_format` | uint32 | 仅 `has_snr=true` 时写入 | 0=逐像素 float32[n_pix]（旧格式）；1=稀疏控制点（新格式） |
| `snr_n_points` | uint32 | 仅 `snr_format=1` 时写入 | 稀疏控制点数量 |

**向后兼容规则**：
- 旧文件无 `has_snr` 字段 → 默认 `false`（无 SNR 通道）
- 旧文件 `has_snr=true` 但无 `snr_format` 字段 → 默认 `snr_format=0`（逐像素）

### 4.4 元数据字段（caller-supplied）

调用方通过 `meta_json` 参数传入的任意 JSON 对象字段会合并到头中。当前 P00-003 基线实际产出的 HISS 头包含以下字段（由 Drizzle 模块填充）：

| 字段类别 | 字段示例 | 说明 |
|---|---|---|
| 原始输入标识 | `input_fits_path`, `input_fits_sha256` | 原始 FITS 路径与 SHA-256 |
| 生效配置 | `config_hash` | Stage1 配置哈希 |
| 校准摘要 | `calibration_summary` | CALIBRATE 阶段摘要 |
| WCS 摘要 | `wcs_summary`, `ctype1`, `crval1`, ... | PLATESOLVE 输出 |
| PSF 摘要 | `psf_summary`, `psf_method`, `fwhm_median` | PSF 拟合结果 |
| 测光摘要 | `photometric_summary`, `n_matched` | PHOTOMETRIC 输出 |
| Drizzle 摘要 | `nside`, `pixfrac`, `n_source_pixels` | Drizzle 参数与统计 |
| 模块版本 | `module_versions`, `build_id` | 各模块版本号 |
| 滤镜/曝光 | `filter`, `exposure_sec`, `obs_time` | 观测元数据 |
| 设备信息 | `telescope`, `camera`, `pixel_size_um` | 设备元数据 |

**注意**：本合约仅冻结字段名约定，不强制要求所有字段都出现。具体字段集由 Stage1 Drizzle 模块的 `meta_json` 构造逻辑决定。P00-003 基线 `has_snr=0`（因 G-002 缺口导致 SNR 退化）。

## 5. SNR 通道二进制布局

### 5.1 snr_format=0（逐像素）

紧跟 `pixel[n_pix]` 数组之后：

```
| snr[n_pix] | float32 LE × n_pix |
```

总长度 = `n_pix * 4` 字节。

### 5.2 snr_format=1（稀疏控制点）

紧跟 `pixel[n_pix]` 数组之后：

```
+---------------------------
| n_points                 | uint32 LE
+---------------------------
| points[n_points]         | 每项 20 字节（pack=1）
|   ra: double LE          | 8 字节，球面赤经（度）
|   dec: double LE         | 8 字节，球面赤纬（度）
|   snr_psf: float32 LE    | 4 字节，(A-B)/mad 无量纲
+---------------------------
| snr_phot                 | double LE  (1/(ln10×sigma_residual) 全局标量)
| median_snr               | double LE  (median(snr_psf) 归一化基准)
| idw_power                | double LE  (IDW 幂次，默认 2.0)
+---------------------------
```

总长度 = `4 + 20*n_points + 24` 字节。

**SNR 重建公式**：`SNR(ra,dec) = snr_phot × (IDW_spherical(points, query) / median_snr)`

### 5.3 无 SNR 通道（`has_snr=false`）

`pixel[n_pix]` 之后无任何 SNR 数据，文件结束。

## 6. 校验和机制

**当前 v1.0 实现：无校验和。**

文件没有 CRC32、SHA-256 或任何完整性校验字段。损坏检测依赖：
- Magic 字段匹配（4 字节）
- JSON 头 zstd 解压成功
- JSON 头必填字段（nside/nested/n_pix）解析成功
- 文件读取长度匹配（`fread` 返回值检查）

**风险**：静默位翻转无法检测，建议 v1.1+ 在文件尾加入 CRC32 或 SHA-256 校验字段。

## 7. 向后兼容策略

| 演进类型 | 兼容策略 |
|---|---|
| 新增 JSON 头可选字段 | 直接添加，旧读取器忽略未知字段（向前兼容） |
| 新增 `snr_format` 值 | 旧读取器遇到未知值应返回错误（不向前兼容，需版本协商） |
| 修改 Magic | 不兼容，必须新建格式（如 HISS2） |
| 修改字节序 | 不兼容 |
| 修改数组元素类型 | 不兼容 |
| 新增文件尾校验和 | 旧读取器忽略尾部多余字节（向前兼容，但旧写入器产出的文件无校验和） |

## 8. 已知缺口（v1.1+ 待修复，本合约不修复）

1. **无显式 format_version 字段**：当前仅靠 magic 区分 HISS/HCSD，无法区分 v1.0/v1.1+。建议在 JSON 头加入 `"format_version": "1.0"`。
2. **无校验和**：见 §6。
3. **JSON 头解析使用字符串搜索**：`hio_parse_json_*` 用 `find()` 而非真正的 JSON 解析器，对包含特殊字符（如 `"nside":` 出现在字符串值内）可能误解析。
4. **HISS 非字节级可重现**：同一输入两次运行 hash 不同（P00-003 已记录，疑似 zstd 元数据或并行浮点非确定性）。
5. **`hiss_read` 与 `hiss_read_snr_model` 行为不一致**：`hiss_read` 在 `snr_format=1` 时跳过稀疏块但不解析；`hiss_read_snr_model` 在 `snr_format=0` 时跳过逐像素 snr 但不返回。调用方必须根据 `snr_format` 选择正确的读取函数。
6. **n_pix=0 时无 SNR 数据**：`hiss_write` 在 `n_pix=0` 时不写入 ipix/pixel/snr，但 `has_snr` 仍可为 true（语义不清）。

## 9. API 引用

| 函数 | 用途 |
|---|---|
| `aio_hiss_write(path, nside, nested, n_pix, ipix, pixel, snr, meta_json)` | 写入 snr_format=0 |
| `aio_hiss_read(path, &nside, &nested, &n_pix, &ipix, &pixel, &snr, &meta_json)` | 读取（兼容 format 0/1，snr 仅 format=0 时填充） |
| `aio_hiss_write_snr_model(path, nside, nested, n_pix, ipix, pixel, snr_model, meta_json)` | 写入 snr_format=1 |
| `aio_hiss_read_snr_model(path, &nside, &nested, &n_pix, &ipix, &pixel, &snr_model, &meta_json)` | 读取（兼容 format 0/1，snr_model 仅 format=1 时填充） |
| `aio_hio_free(ptr)` | 释放读取器分配的 ipix/pixel/snr/meta_json 内存 |
| `aio_hio_free_snr_model(model)` | 释放 HioSnrModel |

错误码：`HIO_OK=0`, `HIO_ERR_PARAM=-1`, `HIO_ERR_FILE=-2`, `HIO_ERR_MAGIC=-3`, `HIO_ERR_ZSTD=-4`, `HIO_ERR_JSON=-5`, `HIO_ERR_MEM=-6`, `HIO_ERR_BOUNDS=-7`。

## 10. Round-trip 不变量

对于任意 HISS 文件 F，round-trip 操作（read → write → read）应满足：

1. **JSON 头字段等价**：第二次读取的 `nside`, `nested`, `n_pix`, `has_snr`, `snr_format`, `snr_n_points` 与第一次一致；caller-supplied meta 字段键值对一致（JSON 字符串字节级一致或语义等价）。
2. **ipix 数组等价**：第二次读取的 `ipix[n_pix]` 与第一次逐元素相等（uint64）。
3. **pixel 数组等价**：第二次读取的 `pixel[n_pix]` 与第一次逐元素相等（float32 位级一致，NaN 按 IEEE 754 全等处理）。
4. **SNR 通道等价**：
   - `snr_format=0`：第二次读取的 `snr[n_pix]` 与第一次逐元素相等（float32 位级一致）。
   - `snr_format=1`：第二次读取的 `snr_model` 与第一次字段相等（n_points, points[], snr_phot, median_snr, idw_power）。
5. **文件大小等价**：副本文件大小与原文件相等（字节级可重现可选，受 zstd 压缩确定性影响）。
