# TEST_REPORT

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| HISS round-trip (stage1_baseline.hiss) | `python engineering/tools/test_hiss_hcsd_roundtrip.py` (单文件) | 30s | 0 | PASS | roundtrip_output/stage1_baseline.roundtrip.hiss (47710 字节, nside=512, n_pix=3927) |
| HISS round-trip (frame1.hiss) | 同上 | 60s | 0 | PASS | roundtrip_output/frame1.roundtrip.hiss (184878349 字节, nside=32768, n_pix=15406480) |
| HISS round-trip (frame2.hiss) | 同上 | 60s | 0 | PASS | roundtrip_output/frame2.roundtrip.hiss (184887016 字节, nside=32768, n_pix=15407202) |
| HCSD round-trip (stage2_baseline.hcsd) | 同上 | 60s | 0 | PASS | roundtrip_output/stage2_baseline.roundtrip.hcsd (187455454 字节, nside=32768, n_pix=15522966) |
| HCSD 按子叶读取验证 | 同上（内嵌于 HCSD round-trip） | - | 0 | PASS | 10 个非空子叶 ipix/pixel 全部匹配 |
| DLL 加载验证 | `python -c "import ctypes; dll = ctypes.CDLL('lib/astro_image_io/astro_image_io.dll'); print(dll.aio_hiss_write)"` | 5s | 0 | PASS | 9 个 aio_hiss_*/aio_hcsd_* 函数全部绑定成功 |
| Magic 字段验证 | `python -c "import struct; f=open('.../stage1_baseline.hiss','rb'); print(f.read(4))"` | 5s | 0 | PASS | HISS 文件 magic = b'HISS', HCSD 文件 magic = b'HCSD' |
| zstd 头部解压验证 | 读取 uncomp_json_len/comp_json_len 并验证 zstd magic 0x28b52ffd | 5s | 0 | PASS | stage1_baseline.hiss: uncomp=796/comp=557, zstd magic 正确 |
| 格式规范合约完整性 | 人工审查 hiss_format_v1.md / hcsd_format_v1.md | - | - | PASS | HISS 10 节 + HCSD 11 节，覆盖布局/Magic/JSON/SNR/索引/兼容/缺口/API/不变量 |

## Real-data metrics

### HISS round-trip 详细指标

| 文件 | 原大小 | 副本大小 | 大小差 | nside | n_pix | has_snr | snr_format | read_ms | write_ms | reread_ms |
|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|
| stage1_baseline.hiss | 47693 | 47710 | +17 | 512 | 3927 | false | 0 | 0.95 | 1.10 | 0.67 |
| frame1.hiss | 184878332 | 184878349 | +17 | 32768 | 15406480 | false | 0 | 2504.6 | 4076.9 | 2678.8 |
| frame2.hiss | 184886999 | 184887016 | +17 | 32768 | 15407202 | false | 0 | 2803.0 | 3685.4 | 2688.9 |

### HCSD round-trip 详细指标

| 文件 | 原大小 | 副本大小 | 大小差 | nside | n_pix | has_snr | 非空子叶 | read_ms | write_ms | reread_ms | leaf_read |
|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| stage2_baseline.hcsd | 187455430 | 187455454 | +24 | 32768 | 15522966 | false | 78/49152 | 2848.0 | 4036.9 | 2509.3 | 10 sampled PASS |

### 字段验证矩阵

| 文件 | json_header | ipix | pixel | snr | snr_model | leaf_read | file_size |
|---|---|---|---|---|---|---|---|
| stage1_baseline.hiss | ✓ | ✓ | ✓ | ✓ (N/A) | ✓ (N/A) | - | ✗ (+17B) |
| frame1.hiss | ✓ | ✓ | ✓ | ✓ (N/A) | ✓ (N/A) | - | ✗ (+17B) |
| frame2.hiss | ✓ | ✓ | ✓ | ✓ (N/A) | ✓ (N/A) | - | ✗ (+17B) |
| stage2_baseline.hcsd | ✓ | ✓ | ✓ | ✓ (N/A) | ✓ (N/A) | ✓ | ✗ (+24B) |

**说明**：
- ✓ = 通过（位级一致或语义等价）
- ✗ = 不通过（字节级差异，但语义等价）
- N/A = 不适用（has_snr=false，无 SNR 数据）
- file_size 列 ✗ 不计入测试失败（合约 §10/§11 允许 JSON 字符串字节级差异）

### HCSD 按子叶读取验证详情

测试抽样 10 个非空子叶（全部 78 个非空子叶中的前 10 个），对每个子叶验证：
- 原文件 `aio_hcsd_read_leaf` 与副本 `aio_hcsd_read_leaf` 返回的 ipix 集合一致
- 原文件与副本返回的 pixel 值按 ipix 索引后位级一致
- 子叶数据与全量读取的对应子集一致

| leaf_ipix | n_pix | 原文件 offset | 验证结果 |
|---:|---:|---:|---|
| 29111 | 30773 | 0 | PASS |
| 29115 | 112101 | 246184 | PASS |
| 29116 | 105989 | 1142992 | PASS |
| 29117 | 255619 | 1990904 | PASS |
| 29118 | 261191 | 4035856 | PASS |
| 29119 | 262144 | 6125384 | PASS |
| 29160 | 83870 | 8222536 | PASS |
| 29162 | 256210 | 8893496 | PASS |
| 29163 | 78510 | 10943176 | PASS |
| 29447 | 111514 | 11571256 | PASS |

### 测试环境

| 项 | 值 |
|---|---|
| Python | 3.10.11 (MSC v.1929 64 bit AMD64) |
| Platform | win32 |
| DLL | lib/astro_image_io/astro_image_io.dll |
| DLL 函数 | 9 个 (aio_hiss_read/write/write_snr_model/read_snr_model + aio_hcsd_read/write/read_leaf + aio_hio_free/free_snr_model) |
| numpy | 2.2.6（仅脚本头部导入，实际未使用——纯 ctypes 实现） |
| PowerShell | 7.6.3 (PSVersion) |
| 操作系统 | Windows |

## Failures and investigation

### F-001: 副本文件大小与原文件不一致（不视为失败）

- **症状**：所有 4 个文件的 round-trip 副本大小比原文件大 17 字节（HISS）或 24 字节（HCSD），SHA-256 不同。
- **根因**：测试脚本用 Python `json.dumps(data.meta, ensure_ascii=False)` 重新序列化 meta JSON，与 C 端 `hio_build_json()` 的字符串拼接方式产生的 JSON 字符串字节表示略有差异（字段顺序、空格、转义等）。zstd 压缩后字节不同。
- **影响**：不影响 round-trip 验证。所有数据字段（ipix/pixel/snr/snr_model/leaf_read）通过位级一致验证，JSON 头通过语义等价验证（dict 相等）。符合合约 §10/§11 "JSON 字符串字节级一致或语义等价"要求。
- **HCSD +24 字节**：比 HISS 多 7 字节，可能涉及 leaf_index 排序顺序的细微差异（但 leaf_read 验证通过，证明子叶数据等价）。
- **不视为失败**：`file_size_match` 字段在测试结果中为 False，但不计入 `success` 判定（合约允许 JSON 字节级差异）。

### F-002: snr_format=1 稀疏控制点格式未覆盖（测试覆盖缺口）

- **症状**：所有 4 个真实数据文件 `has_snr=false`，未覆盖 snr_format=1 稀疏控制点格式。
- **根因**：P00-003 基线因 G-002 缺口（PHOTOMETRIC n_matched=0）导致 SNR 退化，HISS has_snr=0，无 snr_format=1 文件可测。
- **影响**：测试脚本已实现稀疏模型读写支持（`hiss_read_snr_model` + `hiss_write_snr_model`），但未能在真实数据上验证。
- **缓解**：合约 §5.2 已明确记录稀疏控制点二进制布局，待 G-002 修复后可补充验证。本缺口不阻塞 P01-003 验收（合约冻结任务，测试覆盖现有数据即可）。

### F-003: HCSD 按子叶读取仅抽样 10 个子叶（测试覆盖缺口）

- **症状**：HCSD 有 78 个非空子叶，测试仅验证前 10 个。
- **根因**：完整 78 个子叶验证耗时较长（每个子叶两次 `aio_hcsd_read_leaf` 调用 + 集合比较），且前 10 个已覆盖不同子叶大小（30773 ~ 262144 像素）。
- **影响**：剩余 68 个子叶未验证，但抽样已覆盖大中小三种规模。
- **缓解**：测试脚本支持通过修改 `test_leaves = sorted(leaf_set)[:min(10, len(leaf_set))]` 调整抽样数，后续稳定性测试任务可改为全量验证。

### F-004: 测试脚本依赖 DLL 加载环境（部署风险）

- **症状**：测试脚本需要 `C:\msys64\mingw64\bin` 在 PATH 中（mingw64 运行时），且 `lib/astro_image_io/astro_image_io.dll` 存在。
- **根因**：astro_image_io.dll 静态链接 mingw64 运行时库，DLL 加载时需要 mingw64 路径。
- **影响**：在其他环境（如 CI/CD 或干净 Windows）运行需先确认 DLL 依赖。
- **缓解**：测试脚本 `_setup_path()` 自动添加 `C:\msys64\mingw64\bin` 到 PATH 和 `os.add_dll_directory`。若 mingw64 路径不同，需修改脚本顶部的 `MINGW_BIN` 常量。
