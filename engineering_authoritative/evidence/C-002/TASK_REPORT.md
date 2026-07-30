# C-002 任务报告 — HISS V2 分块索引、压缩、校验与 batch read

- **任务**：C-002
- **Gate**：C
- **状态**：完成
- **日期**：2026-07-30
- **依赖**：C-001（HISS_FORMAT_V2.md 契约冻结）

---

## 1. 目标

实现 HISS V2 读写器（Python 参考实现），满足冻结契约 `engineering_authoritative/contracts/HISS_FORMAT_V2.md` 的全部规范：
- 分块索引（CHUNK_SIZE=4096，O(1) 随机读取）
- 每块独立 zstd 压缩（signal float32 + support uint8 打包）
- CRC32 IEEE 802.3 双层校验（per-chunk + global）+ footer magic_trailer
- 7 个 batch read API
- V1→V2 迁移转换器

## 2. 交付物

| 文件 | 说明 |
|---|---|
| `lib/astro_image_io/python/hiss_v2.py` | V2 读写器主实现（1006 行） |
| `output/C-002/T2_RED_LDN43.hiss2` | 转换后 V2 文件（43291 B） |
| `output/C-002/T3_RED_NGC55.hiss2` | 转换后 V2 文件（19012 B） |
| `output/C-002/T4_RED_GalaxyCenter_panel1.hiss2` | 转换后 V2 文件（56560 B） |
| `engineering_authoritative/evidence/C-002/test_hiss_v2.py` | 测试脚本 |
| `engineering_authoritative/evidence/C-002/test_run.log` | 测试日志 |
| `engineering_authoritative/evidence/C-002/test_results.csv` | 测试结果 CSV |
| `engineering_authoritative/evidence/C-002/TEST_REPORT.md` | 测试报告 |

## 3. 实现概要

### 3.1 二进制布局（严格遵循契约 §4）

```
FIXED HEADER (24B): magic "HI2S" + version(2) + flags + json_uncomp_len + json_comp_len + n_pix
JSON PROVENANCE (zstd level=5): 18 个必填字段 (契约 §5.2)
CHUNK INDEX (n×24B): offset/comp_size/raw_count/crc32/codec/flags/reserved
DATA CHUNKS: 每块 [ipix×u64][signal×f32][support×u8] 打包后整体 zstd
SNR BLOCK: n_points + 3 通道 SoA (ra/dec/snr 各自 zstd) + 3 标量
FOOTER (48B): chunk_index_offset/size + snr_block_offset/size + global_crc32 + magic_trailer "HI2S"
```

### 3.2 关键设计决策

1. **固定头布局以契约为 SSOT**：任务描述中的固定头布局（含 header_size/n_chunks/nside 等）与冻结契约 §5.1 不一致。实现以 FROZEN 契约 §5.1 为准（magic+version+flags+json_uncomp_len+json_comp_len+n_pix = 24B）。

2. **全局 CRC32 增量计算**：写入时用 `zlib.crc32(data, crc)` 增量更新，避免回读文件（Windows 下 "wb" 模式不支持 read）。覆盖范围 `[0, filesize-48)`，footer 自身不参与（契约 §12.3）。

3. **SNR SoA 三通道**：V1 AoS（n×20B {ra,dec,snr_psf}）→ V2 SoA（ra/dec/snr 三通道独立 zstd）。每通道前置 8B 头 `[comp_len u32][raw_len u32]`（契约 §8.3），解压时用 comp_len 推进位置。

4. **ipix 范围二分查找**：块索引项不含 ipix 范围（契约 §6.2 FROZEN），`read_ipix_range`/`read_leaf` 通过解压块边界 `(_chunk_bounds)` 做二分定位，仅解压 O(log n_chunks) 个块。解压结果缓存以便重复访问。

5. **V1 纯 Python 读取**：`v1_read_snr_model` 直接解析 V1 二进制（不依赖 DLL），支持 snr_format=1 稀疏控制点（packed 20B/点）。文件大小校验确认 V1 点结构为 20B（非 24B）。

6. **NaN 处理**：SNR 数据含 NaN（`has_nan=True`），比较时用 `np.allclose(equal_nan=True)` + 字节级 `tobytes()` 校验，确保 zstd 无损往返字节一致。

### 3.3 禁止项遵守（契约 §2.1）

| 禁止项 | 遵守情况 |
|---|---|
| 不得把无覆盖写成零 | ✓ V1 迁移时 support[i]=1（契约 §14.2），稀疏模式仅存覆盖像素 |
| 不得将 signal 量化为 uint8 | ✓ signal 为 float32，测试验证 `signal_dtype=float32` |
| 不得只支持整文件读取 | ✓ 实现 7 个 batch read API，read_ipix_range 仅读必要块 |
| CRC32 必须实现 | ✓ per-chunk + global 双层，损坏测试返回 -4 |
| zstd 往返必须一致 | ✓ ipix/signal/support/snr 字节级一致（`bytes_exact=True`） |

## 4. V1→V2 转换结果

| 帧 | nside | n_pix | n_points | V1 大小 | V2 大小 | 压缩比 |
|---|---|---|---|---|---|---|
| T2_RED_LDN43 | 2048 | 1573 | 1930 | 58076 | 43291 | 0.745 |
| T3_RED_NGC55 | 2048 | 1535 | 617 | 31352 | 19012 | 0.606 |
| T4_RED_GalaxyCenter_panel1 | 512 | 3928 | 1984 | 87433 | 56560 | 0.647 |

V2 含分块索引（24B/块）+ footer（48B）+ per-chunk CRC32 + support 通道开销，但 zstd 压缩 signal/support/SNR 后总体积小于 V1（V1 signal/pixel 未压缩）。

## 5. 测试结果

**37/37 全部通过**（详见 TEST_REPORT.md）。覆盖：
- V1→V2 转换 + 数据一致性（ipix/signal/support/snr 字节级一致）
- 全局 CRC32 + 结构校验（magic/version/footer/n_pix 一致性/文件大小）
- 7 个 batch read API（provenance/chunk/chunks/ipix_range/leaf/snr_model/all）
- 损坏测试（翻转字节 → -4 CRC 失败）
- 多块分块（chunk_size=512 触发 4 块）
- 错误码（-1/-2/-8）
- V1/V2 magic 区分（HISS vs HI2S）

## 6. 限制与说明

1. **LZ4 codec 未实现**：契约 §10.1 允许 LZ4，但本项目未引入 LZ4 依赖。遇到 `codec=LZ4` 报 -6（契约要求遇未知 codec 报错不崩溃）。ZSTD（默认）与 NONE 已实现。

2. **read_leaf 错误码**：契约 §11.8 未为 "nside<64 不支持子叶读取" 定义专用错误码。实现复用 -5（chunk 索引越界，最近语义）。

3. **C++ 移植**：本任务为 Python 参考实现，C++ 移植（契约 §16）属后续任务。Python 实现已验证契约可实施性。

4. **SNR NaN**：V1 SNR 数据本身含 NaN（源数据特性），V2 忠实保留，无损往返。

## 7. 复现命令

```powershell
cd "f:\Astro dev\Astro CS Normalization Database"
python "engineering_authoritative/evidence/C-002/test_hiss_v2.py"
```

依赖：`zstandard`、`zlib`（标准库）、`numpy`（均已就绪）。

## 8. 契约合规性声明

本实现严格遵循 `engineering_authoritative/contracts/HISS_FORMAT_V2.md`（FROZEN）：
- 固定头 24B / 块索引项 24B / footer 48B 布局一致
- magic `HI2S` + version `2` + format_version `HISS-V2`
- signal=float32 / support=uint8 / SNR SoA 三通道
- CRC32 IEEE 802.3（zlib）per-chunk + global
- 7 个 batch read API 函数签名与错误码（§11.8）
- 读端校验规则（§13 全部 10 项）
