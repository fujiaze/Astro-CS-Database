# HCSD 格式规范 v1.0 (合约冻结)

- 合约 ID: HCSD-FMT-V1
- 任务: P01-003
- 来源代码: `lib/astro_image_io/src/healpix/aio_healpix_io.cpp`（`aio_hcsd_write`, `aio_hcsd_read`, `aio_hcsd_read_leaf`）
- 来源头文件: `lib/astro_image_io/include/aio_healpix_io.h`
- 文档边界: `engineering/docs/03_END_TO_END_DATAFLOW_AND_LIFETIME.md` §7
- 冻结时间: 2026-07-25
- 状态: **FROZEN**（v1.0 实现现状冻结，v1.1+ 演进见末尾"已知缺口"）

## 1. 概述

HCSD (HEALPix CS Database) 是天球数据库格式，由 Stage2 Stack 模块产出，包含多个 HISS 帧叠加后的最终球面像素数据。HCSD 在 HISS 基础上新增**子叶块索引表**（leaf block index），支持按需加载指定子叶的数据，避免全天球数据全部常驻内存。

## 2. 文件总体布局

```
+--------------------------- offset=0
| Magic "HCSD"              | 4 字节
+--------------------------- offset=4
| uncomp_json_len           | uint32 LE  (JSON 头未压缩字节数)
| comp_json_len             | uint32 LE  (JSON 头 zstd 压缩后字节数)
+--------------------------- offset=12
| compressed_json           | comp_json_len 字节 (zstd level=5)
+--------------------------- offset = 12 + comp_json_len
| leaf_index[49152]         | 49152 × 24 字节 = 1179648 字节
+--------------------------- offset = 12 + comp_json_len + 1179648
| sorted_ipix[n_pix]        | uint64 LE × n_pix (按 leaf_ipix + ipix 升序)
+--------------------------- offset = 12 + comp_json_len + 1179648 + 8*n_pix
| sorted_pixel[n_pix]       | float32 LE × n_pix
+---------------------------
```

**关键区别于 HISS**：
1. Magic = `"HCSD"` 而非 `"HISS"`
2. JSON 头与 ipix 数组之间插入 `leaf_index` 子叶索引表
3. ipix/pixel 数组**按 leaf_ipix 升序、子叶内按 ipix 升序**排序（HISS 不要求排序）
4. 无 SNR 通道（`has_snr` 强制为 false）

## 3. Magic 与字节序

- Magic: 4 字节 ASCII = `"HCSD"` = `0x48 0x43 0x53 0x44`
- 字节序: 全字段小端序（x86 native）
- 解析时若 magic 不匹配返回错误码 `HIO_ERR_MAGIC = -3`

## 4. JSON 头（zstd 压缩）

### 4.1 头部结构

- 8 字节长度前缀：`uncomp_json_len` (u32 LE) + `comp_json_len` (u32 LE)
- 紧跟 `comp_json_len` 字节的 zstd 压缩数据（压缩级别 5）
- 解压后为 UTF-8 JSON 对象字符串

### 4.2 必填字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `nside` | uint32 | HEALPix nside 参数（必须为 2 的幂，且 ≥ 64） |
| `nested` | bool | true=NESTED 排序，false=RING 排序 |
| `n_pix` | uint64 | 稀疏像素总数（所有子叶像素数之和） |
| `has_snr` | bool | **HCSD 强制为 false**（当前实现不写入 SNR 通道） |

**注意**：HCSD 的 `hio_build_json` 调用强制传入 `has_snr=false, snr_format=0`，因此 HCSD 文件不会有 `snr_format` 和 `snr_n_points` 字段。

### 4.3 元数据字段（caller-supplied）

调用方通过 `meta_json` 参数传入的字段会合并到头中。P00-003 基线实际产出的 HCSD 头包含：

| 字段类别 | 字段示例 | 说明 |
|---|---|---|
| 输入 HISS 清单 | `input_hiss_files`, `input_hiss_hashes` | 输入 HISS 文件路径与 SHA-256 |
| 配置 | `config_hash`, `sigma_clip`, `weighting` | Stage2 配置 |
| 模块版本 | `module_versions`, `build_id` | 各模块版本号 |
| 覆盖统计 | `n_unique_pix`, `mean_pixel_count`, `non_empty_leaves` | 叠加覆盖统计 |
| 叠加摘要 | `sigma_clip_iterations`, `rejected_count` | sigma-clip 摘要 |
| 梯度校正 | `gradient_corrected`, `gradient_max_iter` | 梯度校正参数 |

**注意**：本合约仅冻结字段名约定，不强制要求所有字段都出现。

## 5. 子叶块索引表 (leaf_index)

### 5.1 总体结构

- 固定 `N_LEAVES = 49152` 项（= 12 × 64²，对应 nside=64 层的子叶数）
- 每项 24 字节，总计 `49152 × 24 = 1179648` 字节
- 紧跟在压缩 JSON 头之后，位于 ipix 数组之前

### 5.2 单项布局（`LeafIndexEntry`, pack=1）

```
+---------------------------
| leaf_ipix       | uint64 LE  (子叶在 nside=64 下的 ipix, = 数组索引)
| data_offset     | uint64 LE  (子叶 ipix 数据在 sorted_ipix 数组中的字节偏移)
| data_length     | uint64 LE  (子叶包含的像素数量)
+---------------------------
```

**关键约定**：
- `leaf_ipix` 等于该项在索引表中的数组下标（冗余字段，用于一致性校验）
- `data_offset` 是**字节偏移**（相对 `sorted_ipix` 数组起始），不是像素索引
- `data_length` 是**像素数量**（不是字节数）
- 空子叶：`data_offset = 0`, `data_length = 0`
- 计算 pixel 偏移：`pixel_byte_offset = data_offset / sizeof(uint64_t) * sizeof(float)` = `data_offset / 2`

### 5.3 子叶位移量计算

```c
int hio_compute_leaf_shift(uint32_t nside) {
    int shift = 0;
    uint32_t temp = nside;
    while (temp > 64) {
        shift += 2;
        temp >>= 1;
    }
    return shift;
}
// nside=64    → shift=0   (ipix 直接作为 leaf_ipix)
// nside=128   → shift=2   (leaf_ipix = ipix >> 2)
// nside=8192  → shift=14  (leaf_ipix = ipix >> 14)
// nside=32768 → shift=18  (leaf_ipix = ipix >> 18)
```

**约束**：nside 必须 ≥ 64。若 nside < 64，`shift=0`，所有 ipix 落入 leaf 0，索引表退化为单子叶。

### 5.4 排序规则

写入时 `aio_hcsd_write` 会按 `(leaf_ipix, ipix)` 升序排序输入数据：

```c
sort(order, [&](a, b) {
    uint64_t la = ipix[a] >> shift;
    uint64_t lb = ipix[b] >> shift;
    if (la != lb) return la < lb;
    return ipix[a] < ipix[b];
});
```

排序后的 `sorted_ipix` 和 `sorted_pixel` 写入文件。读取方应假设数据已排序（`aio_hcsd_read` 不重新排序）。

## 6. 数据数组

### 6.1 sorted_ipix[n_pix]

- uint64 LE × n_pix
- 按 `(leaf_ipix, ipix)` 升序
- 同一子叶内的像素连续存储

### 6.2 sorted_pixel[n_pix]

- float32 LE × n_pix
- 顺序与 `sorted_ipix` 一一对应
- 当前实现：每个像素值是 sigma-clip + SNR²加权叠加后的均值

### 6.3 按需读取（`aio_hcsd_read_leaf`）

支持只读取指定子叶的数据，无需加载全文件：

```
1. 读取 JSON 头获取 totalNPix
2. 定位到 leaf_index[leaf_ipix_at_nside64] 项 (offset = 12 + compLen + leaf_ipix * 24)
3. 读取 data_offset, data_length
4. 若 data_length=0, 返回空数组
5. 计算 ipix 位置: ipixArrayStart + data_offset
6. 计算 pixel 位置: ipixArrayStart + totalNPix*8 + data_offset/8*4
7. 读取 data_length 个 ipix 和 pixel
```

## 7. 校验和机制

**当前 v1.0 实现：无校验和。**

与 HISS 相同，HCSD 文件没有 CRC/SHA 完整性校验。损坏检测依赖：
- Magic 字段匹配
- JSON 头 zstd 解压成功
- 必填字段解析成功
- 子叶索引项的 `leaf_ipix` 与数组下标隐式一致（但代码未显式校验）

**风险**：
- 子叶索引表损坏会导致按需读取返回错误数据，且无法检测
- 建议 v1.1+ 加入索引表 CRC32 或全文件 SHA-256

## 8. 向后兼容策略

| 演进类型 | 兼容策略 |
|---|---|
| 新增 JSON 头可选字段 | 直接添加，旧读取器忽略（向前兼容） |
| 新增 SNR 通道 | 当前 `has_snr=false`，若 v1.1+ 启用需新增 `snr_format` 字段并保持向后兼容 |
| 修改 N_LEAVES | 不兼容（硬编码 49152） |
| 修改 leaf_index 项大小 | 不兼容（硬编码 24 字节） |
| 修改排序规则 | 不兼容（读取方依赖排序） |
| 修改 Magic | 不兼容 |
| 新增文件尾校验和 | 旧读取器忽略尾部（向前兼容） |

## 9. 已知缺口（v1.1+ 待修复，本合约不修复）

1. **无显式 format_version 字段**：与 HISS 相同，建议在 JSON 头加入 `"format_version": "1.0"`。
2. **无校验和**：见 §7。
3. **无 SNR 通道**：HCSD 强制 `has_snr=false`，丢失了叠加后的 SNR 信息。docs/03 §7 提到"每像素覆盖数/方差/权重/拒绝数是否作为正式通道，由 ADR 决定；未决前不得悄悄改变格式"——本合约维持现状不引入这些通道。
4. **N_LEAVES 硬编码 49152**：仅支持 nside=64 子叶划分，不支持其他 LOD 层级。
5. **nside < 64 行为未定义**：`hio_compute_leaf_shift` 在 nside<64 时返回 0，所有像素落入 leaf 0，但代码不会报错（应明确拒绝或文档化）。
6. **data_offset/data_length 单位混淆**：`data_offset` 是字节，`data_length` 是像素数，容易误用。建议统一为像素索引或字节偏移。
7. **leaf_ipix 字段冗余**：`leaf_index[i].leaf_ipix` 始终等于 `i`，浪费 8 字节/项（共 393216 字节）。可移除或用于存储其他元数据。
8. **空子叶仍占用索引项**：49152 项中绝大多数为空（P00-003 基线仅 78/49152 非空），浪费空间。可考虑稀疏索引。
9. **JSON 头解析使用字符串搜索**：与 HISS 相同的 `hio_parse_json_*` 缺陷。
10. **HCSD 字节级可重现**：P00-003 记录 HCSD SHA-256 与旧记录一致，证明 HCSD 输出可字节级重现（与 HISS 不同）。

## 10. API 引用

| 函数 | 用途 |
|---|---|
| `aio_hcsd_write(path, nside, nested, n_pix, ipix, pixel, meta_json)` | 写入（内部排序 + 构建索引表） |
| `aio_hcsd_read(path, &nside, &nested, &n_pix, &ipix, &pixel, &meta_json)` | 全量读取（跳过索引表） |
| `aio_hcsd_read_leaf(path, leaf_ipix_at_nside64, &n_pix, &ipix, &pixel)` | 按子叶读取（定位索引项 → 读取数据段） |
| `aio_hio_free(ptr)` | 释放读取器分配的内存 |

错误码：与 HISS 相同（`HIO_OK=0`, `HIO_ERR_PARAM=-1`, `HIO_ERR_FILE=-2`, `HIO_ERR_MAGIC=-3`, `HIO_ERR_ZSTD=-4`, `HIO_ERR_JSON=-5`, `HIO_ERR_MEM=-6`, `HIO_ERR_BOUNDS=-7`）。

## 11. Round-trip 不变量

对于任意 HCSD 文件 F，round-trip 操作（read → write → read）应满足：

1. **JSON 头字段等价**：第二次读取的 `nside`, `nested`, `n_pix` 与第一次一致；caller-supplied meta 字段键值对一致。
2. **ipix 集合等价**：第二次读取的 `ipix[n_pix]` 与第一次**作为集合**相等（顺序可能因 `aio_hcsd_write` 重新排序而不同，但元素相同）。
3. **pixel 值等价**：第二次读取的 `pixel[n_pix]` 与第一次按 ipix 索引后值相等（float32 位级一致）。
4. **子叶索引等价**：第二次写入的 `leaf_index` 与第一次按子叶划分后等价（每个子叶的像素数和 ipix 集合相同）。
5. **按子叶读取等价**：对任意 `leaf_ipix_at_nside64`，第二次的 `aio_hcsd_read_leaf` 返回的 (ipix, pixel) 集合与第一次相同。
6. **文件大小等价**：副本文件大小与原文件相等（HCSD 字节级可重现，P00-003 已验证）。

**特别注意**：由于 `aio_hcsd_write` 会重新排序数据，原始文件的 ipix 顺序可能与副本不同。Round-trip 测试必须按 ipix 集合（而非数组顺序）比较，或对原始数据先排序再比较。
