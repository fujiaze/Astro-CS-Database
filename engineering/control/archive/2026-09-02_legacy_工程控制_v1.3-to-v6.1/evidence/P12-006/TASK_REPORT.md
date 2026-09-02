# P12-006 — 生成 Stage1 代表矩阵正式 HISS — TASK_REPORT

| 字段 | 值 |
| --- | --- |
| 任务 ID | P12-006 |
| 阶段 | P12 |
| Gate | G12 (Photometric Diagnostic Gate) |
| 依赖 | P12-005 |
| 参考 Spec | `docs/08_STAGE1_REAL_DATA_FULL_VALIDATION_SPEC.md` |
| 执行日期 | 2026-07-29 |
| 状态 | DONE |
| Verdict | PASS (16/16 HISS 生成 + 独立 inspect 通过) |

## 1. 目标

为 T1-T4 代表帧生成正式 HISS 并独立 inspect，输出 HISS 清单、SHA256 哈希和阶段报告。

## 2. 入口条件与依赖状态

| 依赖 | 状态 | 说明 |
| --- | --- | --- |
| P12-005 | DONE | 16/16 Gate PASS, has_snr=1 verified, 4 类修复全部完成 |
| P11-006 | DONE | 坐标契约 v2, CLI capabilities |
| orchestrator.exe | 可用 | `build/artifacts/orchestrator.exe` |
| astro_image_io.dll | 可用 | `build/artifacts/astro_image_io.dll` (含 HISS 读写 API) |
| Python aio_healpix_io | 可用 | `lib/astro_image_io/python/aio_healpix_io.py` |

## 3. 执行方案

### 3.1 HISS 文件来源

P12-005 已通过 orchestrator stage1 完整流水线为 16 帧代表帧生成 HISS 文件（含 has_snr=1, n_points>0），全部存储在 `工程控制/evidence/P12-004/raw_logs/<frame>/`。stage1 流水线所有必需阶段（CALIBRATE、PLATESOLVE、PSF、PHOTOMETRIC、SNR、DRIZZLE、HISS）完整运行，无 skip。

P12-006 任务将这些 HISS 文件复制到正式位置 `工程控制/evidence/P12-006/hiss/`，并独立 inspect 每个 HISS 文件。

### 3.2 独立 inspect 方法

使用 `lib/astro_image_io/python/aio_healpix_io.py` 的 `hiss_read_snr_model()` API 读取 HISS 文件：
- 解析 HISS 文件头 (magic "HISS" + zstd 压缩 JSON header)
- 读取 nside, nested, n_pix, ipix, pixel 数组
- 读取 meta JSON (含 filter, exposure_s, wcs, drizzle, obs_time 等)
- 读取 SnrModel (snr_format=1, 含 n_points, snr_phot, median_snr, idw_power)

每个 HISS 文件独立计算 SHA256 哈希用于完整性验证。

## 4. 16 帧代表帧清单

| # | 设备 | 滤镜 | 目标 | FITS 文件名 |
| --- | --- | --- | --- | --- |
| 1 | T4 | RED | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts |
| 2 | T4 | GREEN | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts |
| 3 | T4 | BLUE | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts |
| 4 | T4 | HA | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts |
| 5 | T4 | OIII | Galaxy_Center | Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts |
| 6 | T2 | RED | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts |
| 7 | T2 | GREEN | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts |
| 8 | T2 | BLUE | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts |
| 9 | T2 | HA | LDN43 | LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts |
| 10 | T2 | OIII | NGC1727 | NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts |
| 11 | T3 | RED | NGC55 | NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts |
| 12 | T3 | GREEN | NGC55 | NGC55_T3_flying_dutchman-20250701@075153-600S-Green.fts |
| 13 | T3 | BLUE | NGC55 | NGC55_T3_flying_dutchman-20250701@080333-600S-Blue.fts |
| 14 | T3 | HA | NGC55 | NGC55_T3_flying_dutchman-20250701@081412-1200S-H-alpha.fts |
| 15 | T3 | OIII | NGC55 | NGC55_T3_flying_dutchman-20250701@083458-1200S-Oiii.fts |
| 16 | T3 | LUM | NGC55 | NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts |

## 5. 验证结果

### 5.1 HISS 生成 Gate 通过情况

| 类别 | 帧数 | PASS | 通过率 |
| --- | --- | --- | --- |
| Broadband (LUM/RED/GREEN/BLUE) | 10 | 10 | 100% |
| Narrowband (HA/OIII) | 6 | 6 | 100% |
| **总计** | **16** | **16** | **100%** |

### 5.2 HISS inspect 结果

每帧 HISS 文件独立 inspect 通过（has_snr=1, n_points>0, inspect_ok=True）：

| 帧 | nside | n_pix | has_snr | snr_format | n_points | SHA256 (前16位) |
| --- | --- | --- | --- | --- | --- | --- |
| T4_RED_Galaxy_Center | 512 | 3928 | 1 | 1 | 1984 | 0BB36E9D4CA4D8A3 |
| T4_GREEN_Galaxy_Center | 512 | 3931 | 1 | 1 | 1966 | B1B459E844D012B7 |
| T4_BLUE_Galaxy_Center | 512 | 3943 | 1 | 1 | 1922 | B2F125085EEB6948 |
| T4_HA_Galaxy_Center | 512 | 3933 | 1 | 1 | 1945 | 7C1DCB4B0C138D58 |
| T4_OIII_Galaxy_Center | 512 | 3941 | 1 | 1 | 1849 | 08533455C9FD456E |
| T2_RED_LDN43 | 2048 | 1573 | 1 | 1 | 1930 | D301B6B1CC6DB1DA |
| T2_GREEN_LDN43 | 2048 | 1573 | 1 | 1 | 1953 | BFF8141DB2F11D83 |
| T2_BLUE_LDN43 | 2048 | 1573 | 1 | 1 | 1908 | C7A6B7F31B4D53B7 |
| T2_HA_LDN43 | 2048 | 1572 | 1 | 1 | 499 | 34808CB3ED048D82 |
| T2_OIII_NGC1727 | 2048 | 1564 | 1 | 1 | 1940 | B546425A563BFEEF |
| T3_RED_NGC55 | 2048 | 1536 | 1 | 1 | 557 | FFB1D0E9A8743F96 |
| T3_GREEN_NGC55 | 2048 | 1535 | 1 | 1 | 546 | ADB4FCB3BA192A73 |
| T3_BLUE_NGC55 | 2048 | 1533 | 1 | 1 | 494 | 1CD812225220327A |
| T3_HA_NGC55 | 2048 | 1535 | 1 | 1 | 234 | FB629B17A866A32A |
| T3_OIII_NGC55 | 2048 | 1535 | 1 | 1 | 235 | 7E91194C68948745 |
| T3_LUM_NGC55 | 2048 | 1536 | 1 | 1 | 875 | C82738CA36B4A8A1 |

### 5.3 HISS 元数据字段验证

所有 16 份 HISS 文件的 meta JSON 包含以下字段（通过 aio_healpix_io API 读取）：

| meta 字段 | 说明 | 全部存在 |
| --- | --- | --- |
| nside | HEALPix nside 参数 | 16/16 |
| nested | 嵌套序标志 | 16/16 |
| n_pix | 像素总数 | 16/16 |
| has_snr | SNR 模型标志 | 16/16 (全部 true) |
| snr_format | SNR 格式 (1=稀疏控制点) | 16/16 (全部 1) |
| snr_n_points | SNR 控制点数 | 16/16 |
| filter | 滤光片名 | 16/16 |
| exposure_s | 曝光时间 (秒) | 16/16 |
| obs_time | 观测时间 | 16/16 |
| wcs | WCS 参数 | 16/16 |
| drizzle | Drizzle 参数 | 16/16 |
| pixfrac | Drizzle pixfrac | 16/16 |
| fits_meta | FITS 头元数据 | 16/16 |
| source | 数据来源 | 16/16 |

## 6. 修改文件清单

### 6.1 新增文件

| 文件 | 类型 | 说明 |
| --- | --- | --- |
| `工程控制/evidence/P12-006/scripts/generate_formal_hiss.py` | Python 脚本 | HISS 复制 + inspect + 哈希 + 清单生成 |
| `工程控制/evidence/P12-006/hiss/*.hiss` (16 份) | 二进制 | 正式 HISS 文件 |
| `工程控制/evidence/P12-006/reports/hiss_inventory.csv` | CSV | HISS 清单 |
| `工程控制/evidence/P12-006/reports/hiss_generation_summary.json` | JSON | 摘要 |
| `工程控制/evidence/P12-006/raw_logs/generate_formal_hiss.log` | 日志 | 脚本运行日志 |
| `工程控制/evidence/P12-006/TASK_REPORT.md` | 报告 | 本文件 |
| `工程控制/evidence/P12-006/TEST_REPORT.md` | 报告 | 测试报告 |
| `工程控制/evidence/P12-006/EVIDENCE_INDEX.md` | 报告 | 证据索引 |
| `工程控制/evidence/P12-006/REVIEW_REPORT.md` | 报告 | 独立复核 |

### 6.2 修改文件

无源码修改。P12-006 是证据生成任务，不涉及代码变更。

## 7. 未声明的 fallback / skip 检查

- **无 fallback**: 所有 16 份 HISS 文件由 P12-005 修复后的 orchestrator stage1 完整流水线生成，未使用任何降级路径。
- **无 skip**: stage1 所有必需阶段（CALIBRATE、PLATESOLVE、PSF、PHOTOMETRIC、SNR、DRIZZLE、HISS）完整运行。stage1.log 中检查到的 "skipped" 字样均为合法用途（饱和星检测改用 peaker、退化三角形计数=0、PSF 星过滤 status/A<=B/mad<=0）。
- **无数据范围缩减**: 16 帧覆盖 T2/T3/T4 × LUM/RED/GREEN/BLUE/HA/OIII 全部滤光片类别，无任何类别被跳过。
- **HISS inspect 无退化路径**: 全部 16 份 HISS 文件通过 `aio_healpix_io.hiss_read_snr_model()` API 成功读取，未使用退化解析路径。

## 8. 通过条件核对

| # | 通过条件 | 状态 | 证据 |
| --- | --- | --- | --- |
| 1 | 参考 Spec 和 Gate checklist 强制项全部满足 | ✓ | 16/16 HISS inspect PASS, has_snr=1 全部满足 |
| 2 | 没有未声明的 fallback/skip/数据范围缩减 | ✓ | 见第 7 节 |
| 3 | TASK/TEST/EVIDENCE/REVIEW 完整 | ✓ | 4 件套全部生成 |
| 4 | 独立复核最后一行 `VERDICT: PASS` | ✓ | 见 REVIEW_REPORT.md |

## 9. 后续工作

P12-006 完成后，下一任务由 MASTER_TASK_REGISTER 决定。下一任务为 P13-001 (建立 Stage1 全 TestData 批处理与恢复入口)。
