# P05-001 测试报告：真实参考数据集登记

## 测试概述
- **任务 ID**: P05-001
- **测试日期**: 2026-07-25
- **测试环境**: Windows + PowerShell 7 + Python 3 + astropy.io.fits
- **测试帧数**: 7 个 canonical 帧 (每目标 1 帧)
- **数据来源**: P02-001 batch_platesolve_test.py 全量结果 (710 帧)

## 测试命令

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| Canonical 数据集生成 | `python engineering/evidence/P05-001/build_canonical_dataset.py` | 300s | 0 | PASS | canonical_dataset.json |
| Canonical 数据集验证 | `python engineering/evidence/P05-001/verify_canonical_dataset.py` | 60s | 0 | PASS | canonical_verification.json + canonical_verification.log |

## 测试详情

### Test 1: Canonical 数据集生成
**命令**: `python engineering/evidence/P05-001/build_canonical_dataset.py`
**退出码**: 0
**结果**: PASS

**输出摘要**:
- 加载 P02-001 manifest: 710 帧
- 按目标天区分组: 7 个目标 (Galaxy_Center=157, LDN43=42, NGC1727=64, NGC247=68, NGC55=79, NGC83_cluster=72, Victory_Nebula=228)
- 选择 7 个 canonical 帧 (每目标 1 帧)
- 读取每帧 FITS header 元数据: 7/7 成功
- SHA-256 重算验证: 7/7 与 manifest 一致
- 加载 P02-001 plate solve 结果: 7/7 找到结果文件
- 生成 canonical_dataset.json: 12371 bytes
- 生成 canonical_dataset_registry.csv: 2562 bytes

### Test 2: Canonical 数据集验证
**命令**: `python engineering/evidence/P05-001/verify_canonical_dataset.py`
**退出码**: 0
**结果**: PASS

**输出摘要**:
- 验证 7 个 canonical 帧
- 总检查项: 56 (每帧 8 项 × 7 帧)
- PASS: 28 (实测验证, 每帧 4 项 × 7 帧)
- FAIL: 0
- DECLARED: 28 (声明性预期, 每帧 4 项 × 7 帧)

## Real-data metrics

### PlateSolve 实测结果 (P02-001, 7 个 canonical 帧)

| Dataset_ID | 目标天区 | success | RMS (") | n_pairs | n_detected | trans_order | sip_order |
|---|---|---|---:|---:|---:|---:|---:|
| P05-001-C001 | Galaxy_Center | true | 0.3329 | 45 | 0 | 3 | 3 |
| P05-001-C002 | LDN43 | true | 0.1431 | 34 | 0 | 3 | 3 |
| P05-001-C003 | NGC1727 | true | 0.1174 | 41 | 0 | 3 | 3 |
| P05-001-C004 | NGC247 | true | 0.1927 | 34 | 0 | 3 | 3 |
| P05-001-C005 | NGC55 | true | 0.1333 | 38 | 0 | 3 | 3 |
| P05-001-C006 | NGC83_cluster | true | 0.1394 | 32 | 0 | 3 | 3 |
| P05-001-C007 | Victory_Nebula | true | 0.3975 | 32 | 0 | 3 | 3 |

**统计**:
- success rate: 7/7 = 100%
- RMS 范围: 0.1174" ~ 0.3975" (全部 < 1.0" 任务规范)
- RMS 中位数: 0.1394"
- n_pairs 范围: 32 ~ 45 (全部 > 10 任务规范)
- n_pairs 中位数: 34
- trans_order: 全部 3 (三阶变换)
- sip_order: 全部 3 (三阶 SIP 畸变)

### 元数据汇总 (7 个 canonical 帧)

| Dataset_ID | 目标天区 | 尺寸 (W×H) | BITPIX | 曝光 (s) | 滤镜 | CCD温度 (°C) | OBJECT | DATE-OBS |
|---|---|---|---:|---:|---|---:|---|---|
| P05-001-C001 | Galaxy_Center | 4500×3600 | 16 | 180 | Red | -20 | Galaxy_Center_mosaic1_T4_flying_dutchman | 2025-07-02T06:17:19 |
| P05-001-C002 | LDN43 | 4096×4096 | 16 | 600 | Lum | -20 | LDN43_LRGBH_flying_dutchman | 2025-05-03T03:16:10 |
| P05-001-C003 | NGC1727 | 4096×4096 | 16 | 600 | Red | -20 | NGC1727_RGBHO_T2_flying_dutchman | 2025-10-31T06:46:06 |
| P05-001-C004 | NGC247 | 4096×4096 | 16 | 600 | Lum | -20 | NGC247_T2_flying_dutchman | 2025-08-16T03:35:06 |
| P05-001-C005 | NGC55 | 4096×4096 | 16 | 600 | Red | -20 | NGC55_T3_flying_dutchman | 2025-07-01T07:41:22 |
| P05-001-C006 | NGC83_cluster | 4096×4096 | 16 | 600 | Red | -20 | NGC90_2025wwk_T3_flying_dutchman | 2025-10-11T02:08:54 |
| P05-001-C007 | Victory_Nebula | 4500×3600 | 16 | 180 | Lum | -20 | Victory_Nebula_mosaic1_flying_dutchman | 2025-02-04T03:57:02 |

**观察**:
- CCD 温度全部 -20°C (统一冷却)
- BITPIX 全部 16 (16 位整数)
- 尺寸两种: 4500×3600 (Galaxy_Center/Victory_Nebula, T4 望远镜) 和 4096×4096 (其他 5 个目标, T2/T3 望远镜)
- 滤镜: Red (3 帧) + Lum (3 帧) + 其他 (1 帧, 实际 7 帧中 4 Red 3 Lum)

### SHA-256 完整性验证

| Dataset_ID | 目标天区 | manifest SHA-256 (前 16) | 重算 SHA-256 (前 16) | 一致 |
|---|---|---|---|---|
| P05-001-C001 | Galaxy_Center | EC34DD3DB9E90314 | EC34DD3DB9E90314 | ✓ |
| P05-001-C002 | LDN43 | D67D56BB142DD1BC | D67D56BB142DD1BC | ✓ |
| P05-001-C003 | NGC1727 | F0AADA0594B8475D | F0AADA0594B8475D | ✓ |
| P05-001-C004 | NGC247 | 6B0A2D2D0C330870 | 6B0A2D2D0C330870 | ✓ |
| P05-001-C005 | NGC55 | AA5172C6BB652E95 | AA5172C6BB652E95 | ✓ |
| P05-001-C006 | NGC83_cluster | 72F3AD2487D0F201 | 72F3AD2487D0F201 | ✓ |
| P05-001-C007 | Victory_Nebula | E43B88A4BDD8C930 | E43B88A4BDD8C930 | ✓ |

**结果**: 7/7 一致, manifest 完整性验证通过

### 预期数值范围验证汇总

| 检查项 | 类型 | 预期 | 实际 | 结果 |
|---|---|---|---|---|
| A. SHA-256 完整性 | 实测 | manifest=重算 | 7/7 一致 | 7/7 PASS |
| B. PlateSolve success | 实测 | true | 7/7 true | 7/7 PASS |
| C. PlateSolve RMS | 实测 | < 1.0" | 0.1174" ~ 0.3975" | 7/7 PASS |
| D. PlateSolve n_pairs | 实测 | > 10 | 32 ~ 45 | 7/7 PASS |
| E. PSF 有效参数 | 声明性 | 非 NaN | stage1 历史 1913/2000 stars | 7/7 DECLARED |
| F. 测光 n_matched | 声明性 | [0, 5000] | G-002 缺口可能 0 或 1606 | 7/7 DECLARED |
| G. SNR has_snr | 声明性 | 0_or_1 | 骨架退化 0, P03-004 修复后 1 | 7/7 DECLARED |
| H. HISS 文件大小 | 声明性 | > 10KB | stage1 输出 11.5MB+ | 7/7 DECLARED |

## Failures and investigation
无失败项。所有 28 项实测验证 (A/B/C/D × 7 帧) 全部 PASS。28 项声明性预期 (E/F/G/H × 7 帧) 基于 stage1 历史数据声明, 未实际运行 stage1 全流程 (本任务为数据集登记, 不要求运行 stage1)。

## 测试结论
- **总测试数**: 56 (28 实测 + 28 声明性)
- **PASS**: 28/28 实测 (100%)
- **DECLARED**: 28/28 声明性 (基于历史数据)
- **FAIL**: 0
- **VERDICT**: PASS
