# HEALPix 存储格式规范（.hiss / .hcsd）

本规范定义 Astro CS Normalization Database 系统的两种 HEALPix 存储格式，用于替代旧版 `.ahpx` / `.ahps` / `.ahpl` 三种格式：

- **`.hiss`** (HEALPix Storage System) — 单帧存储，保存 Drizzle 重投影后的单张曝光帧
- **`.hcsd`** (HEALPix CSDatabase) — 天球数据库，保存多帧 sigma-clip 叠加后的最终天图

两种格式共享同一套设计理念：稀疏存储（仅保留非零像素）、ipix 隐含球面坐标、JSON 头 zstd 压缩、像素数组不压缩以支持随机访问。

---

## 1. 通用约定

### 1.1 字节序

所有多字节整数采用 **小端序**（x86 native）。

### 1.2 数据类型

| 类型 | 大小 | 说明 |
|------|------|------|
| `uint8` | 1 字节 | Magic 字节、字面值 |
| `uint32` | 4 字节 | nside、帧数、长度字段 |
| `uint64` | 8 字节 | ipix、data_offset、data_length、n_pix |
| `float32` | 4 字节 | pixel 像素值（IEEE 754 单精度） |
| `float` | 8 字节 | JSON 头中的浮点字段（语义说明，实际由 JSON 编码） |

### 1.3 Magic 标识

| 格式 | Magic 4 字节 |
|------|--------------|
| `.hiss` | `'H','I','S','S'` (0x48 0x49 0x53 0x53) |
| `.hcsd` | `'H','C','S','D'` (0x48 0x43 0x53 0x44) |

### 1.4 JSON 头编码

- JSON 头采用 UTF-8 编码
- 使用 zstd 压缩，**压缩级别 level=5**
- 长度字段使用 4 字节 uint32，单位为字节数
- JSON 头前 8 字节固定为 `[uncompressed_len: uint32][compressed_len: uint32]`，紧接 `compressed_len` 字节的压缩后 JSON 数据

### 1.5 像素数组布局

- `ipix` 数组：连续 `n_pix` 个 `uint64`，HEALPix 像素索引
- `pixel` 数组：连续 `n_pix` 个 `float32`，与 `ipix` 一一对应的像素值
- `ipix` 必须按升序排列（便于二分查找与子叶分块）
- 仅存储非零像素（稀疏存储）

---

## 2. `.hiss` 文件格式（单帧存储）

### 2.1 用途

保存 Drizzle 重投影后**单张曝光帧**的 HEALPix 数据。每帧对应一个 `.hiss` 文件，后续由堆叠器读取多帧生成 `.hcsd`。

### 2.2 文件结构

```
┌─────────────────────────────────────────────────────────┐
│ Magic: "HISS"                          4 字节           │
├─────────────────────────────────────────────────────────┤
│ JSON 头 uncompressed_len               4 字节 (uint32)  │
│ JSON 头 compressed_len                 4 字节 (uint32)  │
│ 压缩后的 JSON 头数据                   compressed_len B  │
├─────────────────────────────────────────────────────────┤
│ ipix 数组                              n_pix × 8 字节   │
│   (连续 n_pix 个 uint64)                                │
├─────────────────────────────────────────────────────────┤
│ pixel 数组                             n_pix × 4 字节   │
│   (连续 n_pix 个 float32)                               │
└─────────────────────────────────────────────────────────┘
```

### 2.3 JSON 头字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `nside` | uint32 | 是 | HEALPix nside 参数 |
| `nested` | bool | 是 | 是否为 nested 排序（true=nested, false=ring） |
| `n_pix` | uint64 | 是 | 像素数量（ipix / pixel 数组长度） |
| `filter` | string | 是 | 滤光片名称（如 `"Red"`, `"Green"`, `"Blue"`, `"Lum"`, `"Ha"`） |
| `exposure_s` | float | 是 | 曝光时间（秒） |
| `obs_time` | string | 是 | 观测时间（ISO 8601 格式，如 `"2025-05-03T03:15:25Z"`） |
| `pixfrac` | float | 是 | drizzle pixfrac 参数 |
| `fits_meta` | object | 是 | 原始 FITS 头关键信息子集 |

#### 2.3.1 `fits_meta` 对象

保留原始 FITS 头中用于溯源与质控的关键字段，按需包含：

- `OBJCTRA` / `OBJCTDEC` — 目标中心赤经赤纬（HMS 字符串）
- `IMAGETYP` — 图像类型（如 `LIGHT`）
- `SITELAT` / `SITELONG` — 观测站地理坐标
- `XPIXSZ` / `FOCALLEN` — 像素尺寸与焦距
- `OBJECT` — 目标名称
- `INSTRUME` — 相机型号

### 2.4 不包含的字段

- **不含 WCS**：`ipix` 已隐含球面位置，无需重复存储 WCS
- **不含 SNR / weight 块**：Drizzle 阶段已处理能量滴落与权重分配，单帧存储不再保留这些中间量

### 2.5 JSON 头示例

```json
{
  "nside": 8192,
  "nested": true,
  "n_pix": 23664,
  "filter": "Lum",
  "exposure_s": 600.0,
  "obs_time": "2025-05-03T03:15:25Z",
  "pixfrac": 0.6,
  "fits_meta": {
    "OBJCTRA": "12 34 56.7",
    "OBJCTDEC": "+45 67 89.0",
    "IMAGETYP": "LIGHT",
    "SITELAT": "30.0",
    "SITELONG": "120.0",
    "OBJECT": "LDN43",
    "INSTRUME": "ASI 6200MM"
  }
}
```

---

## 3. `.hcsd` 文件格式（天球数据库）

### 3.1 用途

保存多帧 sigma-clip 叠加后的**最终天球数据库**。一个 `.hcsd` 文件对应一个滤光片的全天（或大天区）叠加结果。支持浏览器按子叶分区（leaf tile）按需加载，无需全量读取。

### 3.2 文件结构

```
┌─────────────────────────────────────────────────────────┐
│ Magic: "HCSD"                          4 字节           │
├─────────────────────────────────────────────────────────┤
│ JSON 头 uncompressed_len               4 字节 (uint32)  │
│ JSON 头 compressed_len                 4 字节 (uint32)  │
│ 压缩后的 JSON 头数据                   compressed_len B  │
├─────────────────────────────────────────────────────────┤
│ 子叶块索引表                           49152 × 24 字节  │
│   (每个索引项 24 字节，见 3.4)                           │
│   总大小: 1,179,648 字节                                 │
├─────────────────────────────────────────────────────────┤
│ ipix 数组                              n_pix × 8 字节   │
│   (连续 n_pix 个 uint64)                                │
├─────────────────────────────────────────────────────────┤
│ pixel 数组                             n_pix × 4 字节   │
│   (连续 n_pix 个 float32)                               │
└─────────────────────────────────────────────────────────┘
```

### 3.3 JSON 头字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `nside` | uint32 | 是 | HEALPix nside 参数（通常 8192） |
| `nested` | bool | 是 | 是否为 nested 排序 |
| `n_pix` | uint64 | 是 | 像素数量（ipix / pixel 数组长度） |
| `filter` | string | 是 | 滤光片名称 |
| `n_frames` | uint32 | 是 | 叠加帧数 |
| `total_exposure_s` | float | 是 | 总曝光时间（秒，等于各帧 exposure_s 之和） |
| `sigma_clip` | object | 是 | sigma-clip 参数 |
| `stack_stats` | object | 是 | 堆叠统计信息 |

#### 3.3.1 `sigma_clip` 对象

```json
{
  "sigma": 3.0,
  "max_iter": 5
}
```

- `sigma` (float): 离群剔除阈值（MAD-based，sigma 倍数）
- `max_iter` (uint32): 最大迭代次数

#### 3.3.2 `stack_stats` 对象

```json
{
  "mean_pixel_count": 12.4,
  "median_exposure": 600.0
}
```

- `mean_pixel_count` (float): 每像素平均叠加帧数
- `median_exposure` (float): 单帧曝光时间中位数（秒）

### 3.4 子叶块索引结构

子叶块索引是 `.hcsd` 区别于 `.hiss` 的核心特性，用于支持浏览器按需加载。

#### 3.4.1 分区方案

- 按 **nside=64** 子叶分区，全球共 **49152 个子叶**（12 × 64² = 49152）
- 每个子叶对应 nside=64 的一个像素区域
- 该区域在 nside=8192 下包含 `(8192/64)² = 128² = 16384` 个子像素
- ipix 嵌套位运算：`leaf_ipix = ipix_fine >> (2 × log2(8192/64))`，即 `ipix_fine >> 14`

#### 3.4.2 索引项布局

每个子叶索引项固定 **24 字节**：

```
┌────────────────────────────────────────┐
│ leaf_ipix       8 字节 (uint64)        │  子叶在 nside=64 下的 ipix
│ data_offset     8 字节 (uint64)        │  子叶数据在 ipix/pixel 数组区内的字节偏移
│ data_length     8 字节 (uint64)        │  子叶包含的像素数量（uint64 个数）
└────────────────────────────────────────┘
```

#### 3.4.3 索引表大小

- 索引项大小：24 字节
- 子叶总数：49152
- **索引表总大小：49152 × 24 = 1,179,648 字节（约 1.13 MB）**

#### 3.4.4 索引项语义

- `leaf_ipix`：子叶在 nside=64 nested 下的 ipix，范围 `[0, 49152)`
- `data_offset`：该子叶的像素数据在 ipix 数组 / pixel 数组区内的字节偏移（相对 ipix 数组起始）
- `data_length`：该子叶包含的非零像素数量（用于定位 pixel 数组区间：`pixel 起始 = data_offset / 8 × 4`）

#### 3.4.5 访问模式

- **O(1) 定位**：给定 `leaf_ipix`，直接读索引表第 `leaf_ipix` 项即可拿到该子叶数据的偏移与长度
- **按需加载**：浏览器可只读索引表 + 指定子叶的数据区间，无需全量加载文件
- **稀疏子叶**：若无像素落入该子叶，`data_length = 0`，`data_offset` 可为任意值（约定为 0）

### 3.5 数据排序约定

- ipix 数组按升序排列
- 同一子叶内的像素在数组中连续存放
- 子叶在数组中的排列顺序与 `leaf_ipix` 升序一致
- 子叶内部按 nside=8192 的 ipix 升序

---

## 4. 二进制布局细节

### 4.1 完整字节级布局

#### `.hiss`

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | Magic | `0x48 0x49 0x53 0x53` |
| 4 | 4 | `header_uncompressed_len` | uint32 LE，JSON 头压缩前字节数 |
| 8 | 4 | `header_compressed_len` | uint32 LE，JSON 头压缩后字节数 |
| 12 | `header_compressed_len` | 压缩 JSON 头 | zstd level=5 压缩的 UTF-8 JSON |
| 12 + `header_compressed_len` | `n_pix × 8` | ipix 数组 | uint64 LE，升序 |
| 12 + `header_compressed_len` + `n_pix × 8` | `n_pix × 4` | pixel 数组 | float32 LE |

#### `.hcsd`

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | Magic | `0x48 0x43 0x53 0x44` |
| 4 | 4 | `header_uncompressed_len` | uint32 LE |
| 8 | 4 | `header_compressed_len` | uint32 LE |
| 12 | `header_compressed_len` | 压缩 JSON 头 | zstd level=5 压缩的 UTF-8 JSON |
| 12 + `header_compressed_len` | 1,179,648 | 子叶索引表 | 49152 × 24 字节 |
| 12 + `header_compressed_len` + 1,179,648 | `n_pix × 8` | ipix 数组 | uint64 LE，升序 |
| 12 + `header_compressed_len` + 1,179,648 + `n_pix × 8` | `n_pix × 4` | pixel 数组 | float32 LE |

### 4.2 长度字段编码

JSON 头长度采用 **双 uint32** 编码：

- `header_uncompressed_len`：压缩前 JSON 字节数，用于读端预分配解压缓冲区
- `header_compressed_len`：压缩后字节数，用于读端确定从文件读取多少字节

此编码同时支持校验：解压后字节数应等于 `header_uncompressed_len`，否则文件损坏。

### 4.3 数组对齐

- 数组**不做显式对齐**，紧跟前一段数据
- 读取时按 uint64 / float32 的自然边界解析（小端 x86 平台原生支持非对齐访问）

---

## 5. 设计理由

### 5.1 ipix 隐含球面位置

HEALPix 像素索引 `ipix` 与 `(nside, nested, ordering)` 一起唯一确定球面位置，无需额外存储 RA/DEC 坐标。这相比旧 `.ahpx` 格式省去坐标数组，减小文件体积并消除坐标 / ipix 不一致的风险。

### 5.2 稀疏存储

天文图像经 Drizzle 重投影后，仅覆盖天球的一小部分（FOV 通常 < 30°），绝大部分 HEALPix 像素为 0。稀疏存储（仅存非零像素的 `ipix + 值`）相比稠密存储可减少 90%+ 的体积。

### 5.3 子叶块索引支持按需加载

`.hcsd` 用于全天数据库，单文件可能达数百 MB 到 GB 级。子叶块索引（1.13 MB 固定开销）将天球划分为 49152 个区域，浏览器可：

1. 一次性读取 1.13 MB 索引表
2. 根据视口计算可见子叶的 `leaf_ipix` 列表
3. 用 HTTP Range 请求或 `pread` 仅读取对应子叶的 ipix + pixel 数据
4. 无需全量下载文件即可浏览任意天区

### 5.4 JSON 头压缩 + 数组不压缩

- **JSON 头压缩**：头部含大量文本元数据（fits_meta 字段名等），zstd level=5 压缩比通常 > 5:1，且头仅占文件极小比例，解压开销可忽略
- **数组不压缩**：ipix / pixel 数组是数值数据，zstd 压缩比有限（2:1 左右），但会破坏随机访问能力。保持未压缩使浏览器可对任意子叶区间做 `pread` 而无需解压整个文件
- 如需进一步压缩体积，可在传输层（HTTP gzip）或归档层（外层 zstd 包）处理，文件格式本身保持随机访问友好

### 5.5 嵌套排序（nested）

两种格式默认采用 nested 排序：

- nested 排序下，子叶位运算 `ipix_fine >> 14` 即可得 `leaf_ipix`，O(1) 计算无查找开销
- ring 排序下需查表转换，性能差
- nested 排序保证同一子叶的像素在 ipix 数组中连续，是子叶块索引能 O(1) 定位的前提

---

## 6. 与旧格式的对应关系

| 旧格式 | 新格式 | 关系 |
|--------|--------|------|
| `.ahpx` (单帧 Drizzle 输出) | `.hiss` | 替代。`.hiss` 去掉 WCS/SNR/weight 块，简化为 ipix + pixel 两个数组 |
| `.ahps` (多帧叠加) | `.hcsd` | 替代。`.hcsd` 增加子叶块索引，支持浏览器按需加载 |
| `.ahpl` (LOD 金字塔) | — | 不在本次迁移范围。LOD 金字塔后续可基于 `.hcsd` 在线计算或单独设计 |

---

## 7. 版本与扩展

### 7.1 版本号

本规范为 **v1.0**。当前格式未在文件中显式存储版本号，版本通过 Magic 标识区分：

- `"HISS"` → `.hiss` v1.0
- `"HCSD"` → `.hcsd` v1.0

未来若格式发生不兼容变更，可通过新增 Magic（如 `"HIS2"` / `"HCS2"`) 区分。

### 7.2 向前兼容策略

- JSON 头新增字段：读端应忽略未知字段（向前兼容）
- 二进制布局变更：必须升级 Magic
- 子叶索引结构变更：必须升级 Magic

### 7.3 校验

读端应在以下环节做校验：

1. Magic 4 字节匹配
2. `header_compressed_len` 解压后字节数 == `header_uncompressed_len`
3. JSON 解析成功且含必填字段
4. `n_pix × 8 + n_pix × 4` 与文件剩余大小一致（`.hiss`）
5. `n_pix × 8 + n_pix × 4 + 1,179,648` 与文件剩余大小一致（`.hcsd`）
6. `.hcsd` 子叶索引表中所有 `data_offset + data_length × 8` 不超过 ipix 数组总字节数

---

## 8. 参考实现

参考实现位于 `lib/healpix_db/healpix_io/`（待实现）：

- `hiss_writer.h/.cpp` — `.hiss` 写入器
- `hiss_reader.h/.cpp` — `.hiss` 读取器
- `hcsd_writer.h/.cpp` — `.hcsd` 写入器（含子叶索引构建）
- `hcsd_reader.h/.cpp` — `.hcsd` 读取器（含按子叶随机访问）
- `hio_api.h/.cpp` — C API 导出层

压缩层复用 `astro_image_io` 的 `aio_compress` / `aio_decompress` C API（codec=1=zstd）。
