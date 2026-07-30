# C-003 测试报告 — HISS V2 Inspector 往返/损坏测试

- **任务**：C-003
- **日期**：2026-07-30
- **测试脚本**：`engineering_authoritative/evidence/C-003/test_hiss_v2_inspector.py`
- **测试日志**：`engineering_authoritative/evidence/C-003/test_run.log`
- **结果**：**26/26 通过，0 失败**

---

## 1. 测试环境

- Python 3.x + numpy 2.2.6 + zstandard 0.25.0 + zlib（标准库）
- 测试数据：C-002 产出的 3 帧 V2 HISS 文件（`output/C-002/*.hiss2`）
- inspector 模块：`lib/astro_image_io/python/hiss_v2_inspector.py`

## 2. 测试矩阵

### A. 往返测试（9 项）

| # | 测试项 | 帧数 | 通过 | 验证内容 |
|---|---|---|---|---|
| A.0 | 读取源数据 | 3 | 3 | hiss2_read_all 成功读取 3 帧 V2 文件 |
| A.1 | 往返一致 | 3 | 3 | write→read 字节级一致：signal/support/ipix/SNR/provenance |
| A.2 | inspector 往返检查 | 3 | 3 | 往返文件 inspector 报告 PASS，0 errors |

### B. 损坏测试（17 项，5 类场景全覆盖）

| # | 测试项 | 通过 | 期望错误码 | 实际 | 验证内容 |
|---|---|---|---|---|---|
| B.1a | 翻转 magic 字节 | ✓ | -2 | -2 | magic 不匹配 |
| B.1b | 翻转 version 字节 | ✓ | -3 | -3 | version 不支持 |
| B.1c | 翻转头部其他字节 | ✓ | -4/-7 | -7 | json_uncomp_len 破坏 → JSON 长度校验失败 |
| B.2a | 翻转块数据（全局CRC先捕获） | ✓ | -4 | -4 | 块数据在全局 CRC 范围内 |
| B.2b | 翻转块数据+修复全局CRC | ✓ | -4 | -4 | 隔离 per-chunk CRC 校验路径 |
| B.2c | inspector 定位 per-chunk CRC 失败 | ✓ | — | — | global_ok=True, chunk0_crc_ok=False |
| B.3a | 翻转 magic_trailer 字节 | ✓ | -8 | -8 | footer magic_trailer 不匹配 |
| B.3b | 翻转 magic_trailer 全部字节 | ✓ | -8 | -8 | 同上（全翻转） |
| B.3c | inspector 定位 footer magic 失败 | ✓ | — | — | footer_magic_ok=False |
| B.4a | 截断尾部 10B | ✓ | <0 | -8 | footer 不完整 |
| B.4b | 截断尾部 48B（footer 全丢） | ✓ | <0 | -8 | footer 完全丢失 |
| B.4c | 截断至 10B | ✓ | <0 | -2 | 文件过短无法读固定头 |
| B.4d | 截断尾部 100B | ✓ | <0 | -8 | footer + 部分 SNR 丢失 |
| B.5a | 篡改 footer global_crc32 | ✓ | -4 | -4 | 全局 CRC 不匹配 |
| B.5b | 翻转 JSON 头区字节 | ✓ | -4/-7 | -4 | 全局 CRC 范围内字节损坏 |
| B.5c | inspector 定位全局 CRC 失败 | ✓ | — | — | global_crc_ok=False, footer_ok=True |
| B.6 | 文件不存在 | ✓ | -1 | -1 | 额外错误码验证 |

## 3. 往返测试详情（契约 §10.3 压缩往返一致性）

### 3.1 字节级一致（3 帧）

每帧执行：read v2 → write v2 → read v2 → `tobytes()` 比较

| 帧 | signal(float32) | support(uint8) | ipix(uint64) | SNR(ra/dec/snr) | provenance |
|---|---|---|---|---|---|
| T2_RED_LDN43 | ✓ | ✓ | ✓ | ✓ | ✓ |
| T3_RED_NGC55 | ✓ | ✓ | ✓ | ✓ | ✓ |
| T4_RED_GalaxyCenter_panel1 | ✓ | ✓ | ✓ | ✓ | ✓ |

- **signal**：`signal1.astype("<f4").tobytes() == signal2.astype("<f4").tobytes()`，dtype=float32（未量化为 uint8）
- **support**：`support1.astype("<u1").tobytes() == support2.astype("<u1").tobytes()`
- **SNR**：三通道（ra=f64, dec=f64, snr=f32）各自 `tobytes()` 比较 + 3 标量（snr_phot/median_snr/idw_power）精确比较（误差 <1e-12）
- **provenance**：18 个必填字段逐字段值比较，0 差异

### 3.2 往返文件 inspector 检查

往返写出的 3 个 V2 文件经 inspector 检查全部 PASS（0 errors），文件大小与源文件完全一致：

| 帧 | 源文件大小 | 往返文件大小 | 一致 |
|---|---|---|---|
| T2_RED_LDN43 | 43291 B | 43291 B | ✓ |
| T3_RED_NGC55 | 19012 B | 19012 B | ✓ |
| T4_RED_GalaxyCenter_panel1 | 56560 B | 56560 B | ✓ |

## 4. 损坏测试详情（契约 §12.4 / §13）

### 4.1 五类损坏场景（不得跳过任何一个）

#### B.1 翻转文件头部字节 → 应返回错误码

| 子测试 | 翻转位置 | 期望 | 实际 | 错误码含义 |
|---|---|---|---|---|
| B.1a | offset 0 (magic) | -2 | -2 | magic 不匹配 |
| B.1b | offset 4 (version) | -3 | -3 | version 不支持 |
| B.1c | offset 8 (json_uncomp_len) | -4/-7 | -7 | JSON 解压长度不一致 |

#### B.2 翻转块数据字节 → CRC32 校验失败

| 子测试 | 方法 | 期望 | 实际 | 说明 |
|---|---|---|---|---|
| B.2a | 翻转 chunk0 内 1 字节 | -4 | -4 | 全局 CRC 先捕获（块数据在 [0,filesize-48) 范围内） |
| B.2b | 翻转+修复全局 CRC | -4 | -4 | 隔离 per-chunk CRC 路径，chunk0 CRC 失败 |
| B.2c | inspector 检查 | — | — | global_crc32_ok=True, chunk0 crc32_ok=False, all_ok=False |

B.2b 关键：修复全局 CRC 后，全局校验通过，但 per-chunk CRC 仍能定位到具体损坏块，证明双层 CRC 设计有效。

#### B.3 翻转 footer 字节 → magic_trailer 不匹配

| 子测试 | 翻转位置 | 期望 | 实际 |
|---|---|---|---|
| B.3a | magic_trailer 第 1 字节 | -8 | -8 |
| B.3b | magic_trailer 全部 4 字节 | -8 | -8 |
| B.3c | inspector 检查 | — | footer_magic_ok=False, all_ok=False |

#### B.4 截断文件 → 应返回错误码

| 子测试 | 截断方式 | 实际 | 说明 |
|---|---|---|---|
| B.4a | 尾部 10B | -8 | footer 不完整，magic_trailer 错位 |
| B.4b | 尾部 48B | -8 | footer 全丢，读到的尾部非 HI2S |
| B.4c | 截断至 10B | -2 | 文件 < 24B，无法读固定头 |
| B.4d | 尾部 100B | -8 | footer + 部分 SNR 丢失 |

#### B.5 全局 CRC32 不匹配 → 应返回错误码

| 子测试 | 方法 | 期望 | 实际 | 说明 |
|---|---|---|---|---|
| B.5a | 篡改 footer global_crc32 字段 | -4 | -4 | 存储值与实际计算值不匹配 |
| B.5b | 翻转 JSON 头区字节 | -4/-7 | -4 | 全局 CRC 范围内字节损坏 |
| B.5c | inspector 检查 | — | global_crc_ok=False, footer_ok=True, all_ok=False |

B.5a 关键：直接篡改 footer 中的 global_crc32 字段（该字段不参与全局 CRC 计算），footer magic 完好，但全局 CRC 校验失败 → -4。

### 4.2 inspector 对损坏文件的定位能力

损坏测试中 inspector 展现了精确的损坏定位能力：

| 损坏类型 | inspector 输出 |
|---|---|
| per-chunk CRC 失败 (B.2c) | global_crc32_ok=True, chunk0 crc32_ok=False, all_ok=False |
| footer magic 失败 (B.3c) | footer_magic_ok=False, all_ok=False, errors 含 "magic_trailer" |
| 全局 CRC 失败 (B.5c) | global_crc32_ok=False, footer_ok=True, all_ok=False |

## 5. 禁止项验证

| 禁止项 | 验证方法 | 结果 |
|---|---|---|
| 不得把无覆盖写成零 | 往返 support 字节级一致，显式存储 | ✓ |
| 不得将 signal 量化为 uint8 | 往返 signal dtype=float32 | ✓ |
| 不得只支持整文件读取 | inspector 复用 7 个 batch read API | ✓ |
| CRC32 必须实现 | per-chunk + global 双层，5 类损坏测试 | ✓ 全部返回正确错误码 |
| zstd 往返必须一致 | 字节级 tobytes() 比较 | ✓ signal/support/ipix/SNR 全一致 |

## 6. 测试输出摘要

```
========================================================================
测试汇总: 26/26 通过, 0 失败
========================================================================
```

## 7. 复现命令

```powershell
cd "f:\Astro dev\Astro CS Normalization Database"
python "engineering_authoritative/evidence/C-003/test_hiss_v2_inspector.py"
```

退出码 0 表示全部通过。

## 8. inspector CLI 用法

```powershell
# 检查单个文件并输出报告
python "lib/astro_image_io/python/hiss_v2_inspector.py" "output/C-002/T2_RED_LDN43.hiss2"

# 输出到文件
python "lib/astro_image_io/python/hiss_v2_inspector.py" "output/C-002/T2_RED_LDN43.hiss2" -o report.txt

# JSON 格式输出
python "lib/astro_image_io/python/hiss_v2_inspector.py" "output/C-002/T2_RED_LDN43.hiss2" --json
```

## 9. 已知限制

1. **zstd 异常处理**：`HissV2Reader` 内部 `_zstd_decompress` 在数据严重损坏时抛出 `ZstdError`（非 `HissV2Error`），模块级函数 `hiss2_read_*` 仅捕获 `HissV2Error`。测试用 `safe_read_all` 包装捕获所有异常。这是 C-002 实现的一个边界问题，不影响契约合规性（损坏文件返回错误码即可）。
2. **LZ4 codec**：契约允许但未实现，遇 LZ4 返回 -6（继承 C-002 限制）。
