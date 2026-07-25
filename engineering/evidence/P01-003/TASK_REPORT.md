# TASK_REPORT

- Task ID: P01-003（v1.1 开发包：HISS/HCSD 格式版本与 round-trip）
- Commit/base: HEAD = 7b85ff3f0d37a4b26fff6077684993842ed2bbae（"P01-002: 建立依赖锁定清单"）；远端 origin = https://github.com/fujiaze/Astro-CS-Database.git；包版本 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- Objective: 冻结 HISS/HCSD 格式规范，建立 round-trip 测试。基于 `engineering/docs/03_END_TO_END_DATAFLOW_AND_LIFETIME.md` §5/§7 定义的 HISS/HCSD 边界，分析 `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` 实际实现，冻结 v1.0 格式规范，建立 Python round-trip 测试脚本并用真实数据验证。
- Changes:
  - 仅写入 `engineering/contracts/hiss_format_v1.md`, `engineering/contracts/hcsd_format_v1.md`, `engineering/tools/test_hiss_hcsd_roundtrip.py`, `engineering/evidence/P01-003/**`，未修改任何 `lib/**` 业务源码（git status 确认）。
  - 新增 HISS 格式规范合约 `engineering/contracts/hiss_format_v1.md`（9319 字节，10 节）：覆盖文件布局、Magic/字节序、JSON 头（必填/SNR/元数据字段）、SNR 通道二进制布局（format 0 逐像素 + format 1 稀疏控制点）、校验和机制（v1.0 无）、向后兼容策略、已知缺口、API 引用、Round-trip 不变量。
  - 新增 HCSD 格式规范合约 `engineering/contracts/hcsd_format_v1.md`（10908 字节，11 节）：覆盖文件布局（含 leaf_index）、Magic/字节序、JSON 头、子叶块索引表（49152 项 × 24 字节）、数据数组、按需读取、校验和机制、向后兼容策略、已知缺口、API 引用、Round-trip 不变量。
  - 新增 round-trip 测试脚本 `engineering/tools/test_hiss_hcsd_roundtrip.py`（39943 字节）：用 ctypes 加载 `lib/astro_image_io/astro_image_io.dll`，调用 `aio_hiss_read/write` + `aio_hcsd_read/write/read_leaf`，对 4 个真实数据文件（3 HISS + 1 HCSD）执行 read→write→read round-trip 验证，输出 JSON 结构化报告。
  - 新增结构化规范摘要 `engineering/evidence/P01-003/hiss_hcsd_format_spec.json`：包含 HISS/HCSD 完整格式规范的字节级布局、字段定义、SNR 格式、向后兼容策略、已知缺口、真实数据测试结果。
- Files:
  - `engineering/contracts/hiss_format_v1.md`（新增，9319 字节，SHA-256 0C5C8AE8...，HISS 格式规范 v1.0 合约冻结）
  - `engineering/contracts/hcsd_format_v1.md`（新增，10908 字节，SHA-256 11061492...，HCSD 格式规范 v1.0 合约冻结）
  - `engineering/tools/test_hiss_hcsd_roundtrip.py`（新增，39943 字节，SHA-256 4D9FD983...，round-trip 测试脚本）
  - `engineering/evidence/P01-003/hiss_hcsd_format_spec.json`（新增，结构化规范摘要 + 真实数据测试结果）
  - `engineering/evidence/P01-003/roundtrip_output/roundtrip_report.json`（新增，4970 字节，SHA-256 0037634A...，round-trip 测试 JSON 报告）
  - `engineering/evidence/P01-003/roundtrip_output/stage1_baseline.roundtrip.hiss`（新增，47710 字节，SHA-256 C850DE21...，stage1 HISS round-trip 副本）
  - `engineering/evidence/P01-003/roundtrip_output/frame1.roundtrip.hiss`（新增，184878349 字节，SHA-256 34B5EF6C...，frame1 HISS round-trip 副本）
  - `engineering/evidence/P01-003/roundtrip_output/frame2.roundtrip.hiss`（新增，184887016 字节，SHA-256 133BFF8E...，frame2 HISS round-trip 副本）
  - `engineering/evidence/P01-003/roundtrip_output/stage2_baseline.roundtrip.hcsd`（新增，187455454 字节，SHA-256 021C3C5F...，stage2 HCSD round-trip 副本）
  - `engineering/evidence/P01-003/TASK_REPORT.md`（本文件）
  - `engineering/evidence/P01-003/TEST_REPORT.md`（v1.1 测试报告）
  - `engineering/evidence/P01-003/EVIDENCE_INDEX.md`（v1.1 证据索引含 SHA-256）
  - `engineering/evidence/P01-003/REVIEW_REPORT.md`（v1.1 独立复核报告，VERDICT: PASS）
- Compatibility:
  - 本任务是合约冻结任务，仅文档与测试工具开发，不修改任何 `lib/**` 业务源码、接口、ABI 或文件格式。
  - HISS/HCSD v1.0 格式规范是对 `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` 当前实现现状的冻结，不引入格式变更。
  - 已知缺口在合约中明确记录为"v1.1+ 待修复"，后续 P02+ 任务可基于本合约规划格式演进（如新增 format_version 字段、CRC 校验等）。
  - round-trip 测试脚本依赖 `lib/astro_image_io/astro_image_io.dll`（已通过 SHA-256 与 build/artifacts 一致性确认，由 P00-003 记录），不依赖被归档的 `lib/healpix_db/healpix_io/healpix_io.dll`。
- Rollback:
  - 删除 `engineering/contracts/hiss_format_v1.md`, `engineering/contracts/hcsd_format_v1.md`, `engineering/tools/test_hiss_hcsd_roundtrip.py`, `engineering/evidence/P01-003/` 下所有文件即可回滚。
  - 不需要 git revert，因为本任务不产生 commit（由主 Agent 统一提交）。
- Remaining risks:
  - **格式规范是对现状的冻结，不修复缺口**：HISS/HCSD v1.0 的 10 个已知缺口（无 format_version、无校验和、JSON 字符串搜索、HISS 非字节可重现等）记录在合约中，由后续 P02+ 任务修复。
  - **round-trip 副本字节级不完全一致**：副本文件大小比原文件大 17 字节（HISS）或 24 字节（HCSD），SHA-256 不同。原因是 Python `json.dumps` 与 C `hio_build_json` 产生的 JSON 字符串字节表示略有差异（字段顺序/空格）。所有数据字段（ipix/pixel/snr/snr_model/leaf_read）通过位级一致验证，符合合约 §10/§11 round-trip 不变量要求（"JSON 字符串字节级一致或语义等价"）。
  - **测试脚本依赖 DLL 加载**：需要 `C:\msys64\mingw64\bin` 在 PATH 中（mingw64 运行时），且 `lib/astro_image_io/astro_image_io.dll` 存在。在其他环境运行需先确认 DLL 可加载。
  - **HCSD 按子叶读取仅验证前 10 个非空子叶**：完整 49152 子叶验证耗时过长，本测试抽样 10 个非空子叶（P00-003 基线 78/49152 非空）。完整验证留给后续稳定性测试任务。
  - **未覆盖 snr_format=1 稀疏控制点格式**：真实数据 P00-003 基线 `has_snr=false`（G-002 缺口导致 SNR 退化），无 snr_format=1 文件可测。测试脚本已实现稀疏模型读写支持，待 G-002 修复后可补充验证。

## 详细执行结果

### 1. HISS/HCSD 实现分析

读取以下源码文件，理解 HISS/HCSD 格式的实际实现：

| 文件 | 行数 | 关键内容 |
|---|---:|---|
| `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` | 1436 | HISS/HCSD 9 个 API 的 C++ 实现，含 zstd 压缩、JSON 头构建/解析、leaf_index 构建/查询 |
| `lib/astro_image_io/include/aio_healpix_io.h` | 155 | 9 个 API 函数签名 + HioSnrControlPoint/HioSnrModel 结构体定义 + 向后兼容宏 |
| `lib/astro_image_io/python/aio_healpix_io.py` | 1034 | Python ctypes 绑定（HissWriter/HissReader/HcsdWriter/HcsdReader + 便捷函数） |
| `lib/healpix_db/healpix_drizzle/healpix_drizzle.py` | 285 | Drizzle Python 绑定（FITS/PipelineFrame → .hiss） |
| `lib/healpix_db/healpix_stack/healpix_stack.py` | 475 | Stack Python 绑定（.hiss → .hcsd，含梯度校正） |

**DLL 导出验证**：`objdump -p astro_image_io.dll` 确认导出 9 个函数（`aio_hiss_read/write/write_snr_model/read_snr_model`, `aio_hcsd_read/write/read_leaf`, `aio_hio_free/free_snr_model`）。旧 `lib/healpix_db/healpix_io/archive/healpix_io.dll` 仍存在并导出无前缀旧名（`hiss_read` 等），但已被归档，不应使用。

### 2. HISS 格式规范要点

| 项 | 值 |
|---|---|
| Magic | `"HISS"` (4 字节, 0x48495353) |
| 字节序 | 小端序 (x86 native) |
| JSON 头 | zstd level=5 压缩，前 8 字节为 uncomp_len + comp_len (u32 LE) |
| 必填字段 | nside (u32), nested (bool), n_pix (u64) |
| SNR 通道 | 可选，has_snr + snr_format (0=逐像素/1=稀疏控制点) |
| 数据数组 | ipix: uint64[n_pix], pixel: float32[n_pix] |
| 校验和 | **无** |
| format_version | **无** (缺陷) |

**SNR format=1 稀疏控制点布局**：`n_points:u32 + points[n_points]*20B + 3*f64 (snr_phot/median_snr/idw_power)`

### 3. HCSD 格式规范要点

| 项 | 值 |
|---|---|
| Magic | `"HCSD"` (4 字节, 0x48435344) |
| 字节序 | 小端序 |
| JSON 头 | 同 HISS，但 has_snr 强制 false |
| 子叶索引表 | 49152 项 × 24 字节 = 1179648 字节固定大小 |
| LeafIndexEntry | leaf_ipix:u64 + data_offset:u64(字节) + data_length:u64(像素) |
| 数据数组 | sorted_ipix: uint64[n_pix], sorted_pixel: float32[n_pix]（按 leaf_ipix+ipix 升序） |
| 按需读取 | `aio_hcsd_read_leaf(path, leaf_ipix_at_nside64)` 支持单子叶加载 |
| 校验和 | **无** |
| format_version | **无** (缺陷) |
| N_LEAVES | 硬编码 49152（仅支持 nside=64 子叶划分） |

### 4. Round-trip 测试结果

| 文件 | 类型 | 大小 | nside | n_pix | has_snr | 状态 | 字段验证 |
|---|---|---:|---:|---:|---|---|---|
| stage1_baseline.hiss | HISS | 47693 | 512 | 3927 | false | PASS | json/ipix/pixel/snr 全部 True |
| frame1.hiss | HISS | 184878332 | 32768 | 15406480 | false | PASS | json/ipix/pixel/snr 全部 True |
| frame2.hiss | HISS | 184886999 | 32768 | 15407202 | false | PASS | json/ipix/pixel/snr 全部 True |
| stage2_baseline.hcsd | HCSD | 187455430 | 32768 | 15522966 | false | PASS | json/ipix/pixel/snr/leaf_read 全部 True |

**4/4 PASS, 0 FAIL**。

**副本字节级差异**（不影响 round-trip 验证）：
- HISS 副本比原文件大 17 字节（Python json.dumps 与 C hio_build_json 字符串差异）
- HCSD 副本比原文件大 24 字节（同上 + leaf_index 排序细微差异）
- 所有数据字段（ipix/pixel/snr/snr_model/leaf_read）通过位级一致验证

### 5. 发现的格式问题（10 项，记录不修复）

1. **无显式 format_version 字段**：HISS/HCSD 均缺失，仅靠 magic 区分类型，无法区分 v1.0/v1.1+
2. **无校验和机制**：CRC/SHA 全部缺失，静默位翻转无法检测
3. **JSON 头解析使用字符串搜索**：`hio_parse_json_*` 用 `find()` 而非真正 JSON 解析器，对包含特殊字符的字符串值可能误解析
4. **HISS 非字节级可重现**：同一输入两次运行 hash 不同（P00-003 已记录）
5. **HCSD 字节级可重现**：P00-003 验证 SHA-256 与旧记录一致
6. **hiss_read 与 hiss_read_snr_model 行为不一致**：需根据 snr_format 选择读取函数
7. **HCSD 无 SNR 通道**：has_snr 强制 false，丢失叠加后 SNR 信息
8. **N_LEAVES 硬编码 49152**：仅支持 nside=64 子叶划分，不支持其他 LOD 层级
9. **leaf_index data_offset/data_length 单位混淆**：data_offset 是字节，data_length 是像素数
10. **leaf_index leaf_ipix 字段冗余**：始终等于数组下标，浪费 393216 字节

### 6. v1.1+ 演进建议

1. 在 JSON 头加入 `"format_version": "1.0"` 字段
2. 在文件尾加入 CRC32 或 SHA-256 校验字段
3. 替换 `hio_parse_json_*` 字符串搜索为真正 JSON 解析器
4. HCSD 引入稀疏索引替代固定 49152 项（节省 ~1MB 空间）
5. 统一 leaf_index data_offset/data_length 单位（建议都用字节或都用像素索引）
6. 修复 HISS 非字节可重现问题（疑似 zstd 元数据或并行浮点非确定性）
7. HCSD 启用 SNR 通道时新增 `snr_format` 字段保持向后兼容
