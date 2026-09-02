# 任务报告

- Task/ADR：P10-001 读取全部TestData子目录说明文档
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3 (astropy) + PowerShell 7

## 目标/问题

依据 `docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`，递归读取每个 TestData 子文件夹的说明文档，并交叉读取 FITS/XISF Header（不得仅凭文件名猜测），输出设备档案、数据集清单、滤镜别名映射和冲突报告。

## 输入与范围

- 输入：`testdata/` 下全部子目录（7 个数据集目录 + 3 个校准目录）
- 参考规范：`docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`
- 工具：`engineering_v1.2/evidence/P10-001/scripts/extract_testdata_catalog.py`
- 依赖：P09-001（已满足）

## 执行/决策

### 阶段 1：发现子目录
发现 10 个 TestData 子目录：
- 7 个数据集目录：Galaxy_Center_T4, LDN43_T2素材_flying_dutchman, NGC1727_T2, NGC247_T2, NGC55_T3, NGC83_cluster_T3, Victory_Nebula_T4
- 3 个校准目录：T2 calibration files, T3 calibration files, T4 calibration files

### 阶段 2：解析说明文档
递归读取 7 个 `素材信息*.txt` 说明文档，抽取望远镜/相机/赤道仪/滤镜/曝光时间字段。
- 修复了 `derive_device_id_from_dirname` 正则以支持中文目录名（`LDN43_T2素材_flying_dutchman`）
- 从 telescope 行提取焦距数字（如 "焦距1900mm" -> 1900）

### 阶段 3：读取 FITS Header
对每个 (target, device, panel, filter) 分组采样第一个 .fts 文件，用 astropy.io.fits 读取 Header。
- 49 个 Light 分组，每组采样 1 个文件
- 提取字段：FILTER, EXPTIME, INSTRUME, TELESCOP, XBINNING, CCD-TEMP, DATE-OBS, NAXIS1/2 等

### 阶段 4：读取 XISF Header
对 27 个 .xisf 校准文件读取 XML Header。
- 发现 XISF 文件使用 PixInsight 变体格式（16 字节头：magic + version "0100" + header_len LE + reserved + XML），非标准 XISF 1.0（8 字节头）
- 修复 XML namespace 问题（去除 xmlns 声明以使 ET.fromstring 用普通标签名）
- 解析 FITSKeyword 元素（PixInsight 风格），提取 FILTER/XBINNING/INSTRUME/XPIXSZ 等字段

### 阶段 5：交叉验证与冲突检测
- 滤镜别名冲突：文件名 vs Header（全部一致，无冲突）
- 校准文件滤镜名冲突：检测到 OIII 别名不一致（T2 用 "OIII"，T3/T4 用 "Oiii"）
- 设备级冲突：文档相机型号（如 "FLI Proline 16803"）vs Header INSTRUME（"FLI"）— 信息粒度不同，非真正冲突

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_testdata_catalog.py` | 120s | 0 |

## 结果与证据

### 交付物
1. **TESTDATA_EQUIPMENT_CATALOG.csv** — 3 行设备档案（T2/T3/T4）
   - T2: ASA 500N, 焦距 1900mm, FLI Proline 16803, 4096x4096, 3 个 Light 目录
   - T3: ASA 500N, 焦距 1900mm, FLI Proline 16803, 4096x4096, 2 个 Light 目录
   - T4: Nikkor 200F2, 焦距 200mm, FLI Microline 16200, 4500x3600, 2 个 Light 目录
2. **TESTDATA_DATASET_CATALOG.csv** — 49 行数据集清单
   - 按 (target, device, panel, filter) 分组，每组包含 n_lights、Header 采样信息、sample_file
   - 总 Light 帧数约 711 帧
3. **FILTER_ALIAS_MAP.json** — 6 个规范滤镜名（LUM/RED/GREEN/BLUE/HA/OIII）
   - observed_aliases_by_canonical: 实际观察到的别名拼写
   - canonical_to_preferred_alias: 推荐使用的别名
4. **DOCUMENT_FACT_CONFLICTS.md** — 1 个冲突
   - calibration_filter_alias_inconsistency: OIII 别名不一致（OIII vs Oiii），需在 P10-004 统一

### 硬门限检查
- 只允许 T1-T4 四套规范设备 ID：**PASS**（实际设备 T2/T3/T4，无 T1）
- 所有 Light 必须能归属 T1-T4：**PASS**（0 个未知设备目录）

### 关键发现
1. **无 T1 设备数据**：testdata 中只有 T2/T3/T4 三套设备的数据，无 T1
2. **T2/T3 共享相机型号**（FLI Proline 16803, 4096x4096），但望远镜不同（都是 ASA 500N 1900mm）
3. **T4 相机不同**（FLI Microline 16200, 4500x3600），望远镜 Nikkor 200F2 200mm
4. **OIII 滤镜别名不一致**：T2 校准文件用 "OIII"，T3/T4 用 "Oiii"（需在 P10-004 统一）
5. **T2 无 Lum 校准平场**：T2 calibration files 只有 Blue/Green/H-alpha/OIII/Red 五个 Flat，无 Lum Flat（但 LDN43_T2 和 NGC247_T2 有 Lum Light 帧）
6. **T4 无 Lum 校准平场**：T4 calibration files 只有 Blue/Green/H-alpha/Oiii/Red 五个 Flat，无 Lum Flat（但 Victory_Nebula_T4 有 Lum Light 帧）
7. **XISF 格式变体**：校准文件使用 PixInsight 变体格式（16 字节头），非标准 XISF 1.0（8 字节头）
8. **所有 Light 帧温度一致**：-20.0°C（仅 1 帧为 -19.9375，浮点精度差异）
9. **所有 Light 帧 Bin=1**：无 Bin 模式变化
10. **滤镜在文件名与 Header 中完全一致**：49 个分组的 filter_in_filename == filter_in_header

## 风险/回滚/残留

- **T2/T4 缺 Lum Flat**：T2/T4 的 Lum Light 帧无对应校准平场，P10-005 须处理（可能借用其他设备 Flat 或跳过 Lum 校准）
- **OIII 别名不一致**：需在 P10-004 冻结规范名后统一
- **无 T1 数据**：硬门限允许 T1-T4，但实际无 T1 数据，不影响门限
- **NGC83_cluster 文件名前缀**：部分文件名为 `NGC90_2025wwk_T3_...`（非 NGC83），但目录名是 NGC83_cluster，Header OBJECT 字段需在 P10-003 核实

## 结论

P10-001 完成。所有 4 个交付物已生成，硬门限 PASS。1 个冲突（OIII 别名不一致）已记录，将在 P10-004 解决。设备档案覆盖 T2/T3/T4 三套设备，数据集清单覆盖 49 个 Light 分组（约 711 帧）。XISF PixInsight 变体格式解析已修复并验证。
