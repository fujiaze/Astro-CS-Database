# HISS V2 契约摘要（C-001）

- **冻结日期**：2026-07-30
- **冻结任务**：C-001
- **契约文档**：`engineering_authoritative/contracts/HISS_FORMAT_V2.md`
- **状态**：FROZEN

## 一、四要素冻结结果

### 1. signal（信号）
- 类型：**float32**（IEEE 754 单精度，小端序）
- 维度：n_pix，与 ipix 一一对应
- 存储：与 ipix/support 同块对齐，每块独立 zstd 压缩
- **硬约束**：不得量化为 uint8 ✓

### 2. support（覆盖）
- 类型：**uint8**（取值 {0,1}）
- 维度：n_pix，与 signal 同维度、同块对齐
- 语义：1=覆盖（signal 有效）；0=无覆盖（signal 无意义，不得当零用）
- **硬约束**：无覆盖不得写成零 ✓（必须查 support 判定，禁止"零=无覆盖"歧义）

### 3. SNR（信噪比）
- 存储：**稀疏控制点**（SoA 三通道），不得全量存储
- 字段：`n_points`(u32) + `points_ra`(f64[]) + `points_dec`(f64[]) + `points_snr`(f32[])
- 全局标量：`snr_phot`(f64) + `median_snr`(f64) + `idw_power`(f64)
- 每通道独立 zstd 压缩
- **硬约束**：稀疏格式 ✓

### 4. provenance（溯源）
- 版本号字段：`format_version="HISS-V2"`（JSON）+ 固定头 `version=2`（双重标记）
- 必填字段：nside, ordering, nested, n_pix, filter, exposure_s, obs_time, pixfrac, wcs, drizzle, fits_meta, source, has_snr, chunk_size, n_chunks, codec, crc_algorithm
- wcs 含 crval/crpix/cd/sip_order/sip_a/sip_b/sip_ap/sip_bp
- 存储：zstd level=5 压缩 UTF-8 JSON

## 二、二进制布局（FROZEN）

```
[FIXED HEADER 24B] magic "HI2S" + version=2 + flags + json_len + n_pix
[JSON PROVENANCE]   zstd 压缩
[CHUNK INDEX]       n_chunks × 24B（offset/comp_size/raw_count/crc32/codec/flags/reserved）
[DATA CHUNKS]       每块 [ipix×u64][signal×f32][support×u8] 整体 zstd
[SNR SPARSE BLOCK]  n_points + 3 通道压缩 + 3 全局标量（可选）
[FOOTER 48B]        chunk_index_offset/size + snr_offset/size + global_crc32 + magic_trailer "HI2S"
```

## 三、关键设计决策

| 决策点 | 选择 | 理由 |
|---|---|---|
| magic | `HI2S`（与 v1 `HISS` 区分第 3 字节 0x32） | 读端先读 magic 分派 v1/v2，不会误判 |
| version | 固定头 uint16=2 + JSON format_version | 双重标记，单一来源不够稳健 |
| 分块大小 | CHUNK_SIZE=4096 像素/块 | 与 `.ahps` 一致，兼顾压缩率与随机访问粒度 |
| 块内布局 | ipix+signal+support 打包后整体 zstd | 块索引只记一项，定位简单；三段同像素对齐 |
| 压缩 | zstd level=5 | 与 v1 一致，无损，压缩率/速度平衡 |
| 校验和 | CRC32 IEEE 802.3（zlib 多项式） | 广泛可用、检测充分、开销可忽略；per-chunk + global 双层 |
| SNR 布局 | SoA 三通道分块压缩 | 比 v1 AoS 压缩率更高，便于按通道批量读取 |
| SNR 字段名 | points_ra/points_dec/points_snr | 对应任务要求命名（v1 snr_psf → v2 points_snr） |
| support 语义 | 稀疏模式下恒为 1 但显式存储 | 保持维度一致 + 为稠密模式预留 + 消除"零=无覆盖"歧义 |
| footer | 48B 尾部固定结构 | 支持只读尾部即可定位索引，无需扫描全文件 |

## 四、Gate C 验收项对应

| Gate C Checklist | V2 契约对应 |
|---|---|
| HISS 格式版本化 | §5.1 固定头 version + §5.2 JSON format_version + magic HI2S |
| signal float32 | §7.1（硬约束禁止 uint8） |
| support 存在 | §7.2（uint8 独立通道） |
| SNR 稀疏 | §8（SoA 稀疏控制点，禁止全量） |
| 分块随机读取 | §6 块索引 + §11 batch read API |
| 校验和损坏测试 | §12 CRC32 + §12.4 损坏测试要求 |
| 压缩往返一致 | §10.3 zstd 无损 |
| 浏览器可检查 | §11.5 read_leaf + §11.1 read_provenance |

## 五、与 V1 兼容性

- V1 magic `HISS` / V2 magic `HI2S`，读端按 magic 分派，互不干扰。
- V1 API（`aio_hiss_read` 等）保持不变。
- V1 → V2 迁移需重新序列化（见契约 §14.2）：补 support、补版本字段、SNR AoS→SoA、加 CRC32/footer。
- B-002 三帧 V1 文件 JSON 头实际已含 wcs/drizzle/source/fits_meta（V2 provenance 字段已在 V1 实现中存在），迁移无字段丢失风险。

## 六、禁止捷径复核（任务要求）

- ✓ 不得把无覆盖写成零 → support 通道 + §7.3 语义表
- ✓ 不得将 signal 量化为 uint8 → §7.1 float32 硬约束
- ✓ 不得只支持整文件读取 → §11 七个 API（provenance/chunk/chunks/range/leaf/snr/all）
- ✓ 契约 FROZEN → §0 + §17，扩展只能加可选字段或新 magic
