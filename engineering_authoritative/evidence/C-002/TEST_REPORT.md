# C-002 测试报告 — HISS V2 读写器

- **任务**：C-002
- **日期**：2026-07-30
- **测试脚本**：`engineering_authoritative/evidence/C-002/test_hiss_v2.py`
- **测试日志**：`engineering_authoritative/evidence/C-002/test_run.log`
- **结果**：**37/37 通过，0 失败**

---

## 1. 测试环境

- Python 3.x + numpy 2.2.6 + zstandard 0.25.0 + zlib（标准库）
- 测试数据：B-002 产出的 3 帧 V1 HISS 文件（`output/B-002/*.hiss`）
- V2 产物：`output/C-002/*.hiss2`

## 2. 测试矩阵

| # | 测试项 | 帧数 | 通过 | 验证内容 |
|---|---|---|---|---|
| 1 | V1→V2 转换 | 3 | 3 | ret=0，V2 文件生成，n_pix 一致 |
| 2 | 全局 CRC32 + 结构 | 3 | 3 | Reader 打开即校验 magic/version/footer/global_crc/n_pix/文件大小 |
| 3 | read_provenance (§11.1) | 3 | 3 | format_version=HISS-V2，codec=ZSTD，crc_algorithm 正确 |
| 4 | read_chunk (§11.2) | 3 | 3 | zstd 往返，ipix/signal 字节级一致，support 全 1，signal=float32 |
| 5 | read_chunks (§11.3) | 3 | 3 | 多块拼接 total==n_pix |
| 6 | read_ipix_range (§11.4) | 3 | 3 | 全范围/子范围/无交集 三种场景 |
| 7 | read_leaf (§11.5) | 3 | 3 | nside=64 子叶定位，count 与掩码期望一致 |
| 8 | read_snr_model (§11.6) | 3 | 3 | n_points/ra/dec/snr/scalars 一致，字节级往返，含 NaN 正确处理 |
| 9 | read_all (§11.7) | 3 | 3 | 整文件读取，n_pix/ipix/signal/support/snr 一致 |
| 10 | 损坏测试 | 3 | 3 | 翻转数据块字节 → code=-4（CRC 失败），不返回错误数据 |
| 11 | 多块分块 | 1 | 1 | chunk_size=512 触发 4 块，块边界正确 |
| 12 | 错误码 | 3 | 3 | -1 文件不存在 / -2 magic 不匹配 / -8 footer 截断 |
| 13 | V1/V2 magic 区分 | 3 | 3 | V1=HISS / V2=HI2S |

## 3. 关键验证详情

### 3.1 zstd 压缩往返一致性（契约 §10.3）

每帧 V1→V2→读取，验证四要素字节级一致：
- **ipix**：`np.array_equal(v1_ipix, v2_ipix)` = True
- **signal**：`np.array_equal(v1_pixel, v2_signal)` = True，dtype=float32（未量化为 uint8）
- **support**：`np.all(support == 1)` = True（V1 迁移后所有像素标记覆盖）
- **SNR**：`v1.tobytes() == v2.tobytes()` = True（ra/dec/snr 三通道字节级一致，含 NaN）

### 3.2 CRC32 校验（契约 §12）

- **全局 CRC32**：Reader 打开时计算 `[0, filesize-48)` CRC32，与 footer `global_crc32` 比对。3 帧均通过。
- **per-chunk CRC32**：读取数据块时校验压缩数据 CRC32。
- **损坏测试**：翻转第一块内 1 字节，`hiss2_read_chunk` 返回 -4（CRC 失败），不返回错误数据。3 帧均通过。

### 3.3 batch read API 完整性（契约 §11）

| API | 测试场景 | 结果 |
|---|---|---|
| read_provenance | 仅读头部，不读数据 | ✓ |
| read_chunk | 单块随机读取 | ✓ |
| read_chunks | 多块批量拼接 | ✓ |
| read_ipix_range | 全范围/子范围/无交集 | ✓ |
| read_leaf | nside=64 子叶（位运算 ipix>>shift） | ✓ |
| read_snr_model | SoA 三通道 + 3 标量 | ✓ |
| read_all | 整文件（=read_chunks 全部 + read_snr_model） | ✓ |

### 3.4 多块分块测试

对 T2_RED_LDN43（n_pix=1573）用 chunk_size=512 转换：
- n_chunks = ceil(1573/512) = 4（与期望一致）
- 4 块拼接 total=1573，与原数据一致
- chunk 0 raw_count=512（首块满），末块 raw_count=1573-3×512=37

### 3.5 read_leaf 子叶定位

| 帧 | nside | shift | leaf_ipix | 命中像素 | 期望 | 一致 |
|---|---|---|---|---|---|---|
| T2_RED_LDN43 | 2048 | 10 | 30727 | 415 | 415 | ✓ |
| T3_RED_NGC55 | 2048 | 10 | 35501 | 78 | 78 | ✓ |
| T4_RED_GalaxyCenter_panel1 | 512 | 6 | 29111 | 12 | 12 | ✓ |

## 4. 禁止项验证

| 禁止项 | 验证方法 | 结果 |
|---|---|---|
| 不得把无覆盖写成零 | V1 迁移 support 全 1，显式存储 | ✓ `support_all1=True` |
| 不得将 signal 量化为 uint8 | 检查 `signal.dtype == float32` | ✓ `signal_dtype=float32` |
| 不得只支持整文件读取 | 7 个 batch API 均独立测试 | ✓ 37/37 |
| CRC32 必须实现 | per-chunk + global + 损坏测试 | ✓ code=-4 |
| zstd 往返必须一致 | 字节级 tobytes() 比较 | ✓ `bytes_exact=True` |

## 5. 测试输出摘要

```
======================================================================
C-002 HISS V2 测试
======================================================================
测试汇总: 37/37 通过, 0 失败
======================================================================
```

## 6. 复现命令

```powershell
cd "f:\Astro dev\Astro CS Normalization Database"
python "engineering_authoritative/evidence/C-002/test_hiss_v2.py"
```

退出码 0 表示全部通过。

## 7. 已知限制

1. **LZ4 codec**：契约允许但未实现（无 LZ4 依赖），遇 LZ4 返回 -6。ZSTD（默认）与 NONE 已实现。
2. **SNR NaN**：V1 源数据 SNR 含 NaN，V2 忠实保留。`np.allclose` 默认对 NaN 返回 False，测试用 `equal_nan=True` + 字节级比较。
3. **read_leaf nside<64**：契约未定义专用错误码，复用 -5。
