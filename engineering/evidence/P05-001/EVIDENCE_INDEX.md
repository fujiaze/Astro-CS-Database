# P05-001 证据索引

## 任务信息
- **任务 ID**: P05-001
- **任务名称**: 真实参考数据集登记 (v1.1 开发包)
- **阶段**: P05
- **Gate**: G5
- **执行日期**: 2026-07-25
- **Commit base**: f98de03b203996f3c56503c0c73bd4bd044ccd7f
- **VERDICT**: PASS

## 证据清单

### 1. Canonical 数据集生成脚本
- **文件**: `engineering/evidence/P05-001/build_canonical_dataset.py`
- **描述**: 从 P02-001 manifest 中按目标天区选择 canonical 帧, 读取 FITS header 元数据, 重算 SHA-256 验证完整性, 加载 P02-001 plate solve 结果, 生成结构化 canonical 数据集与注册表
- **SHA-256**: `5D41BBC3927833046DCE64F9776FFC58A246E89D382CB7ABFE92BB848A87E7C6`
- **大小**: 17031 字节
- **覆盖**:
  - 7 个目标天区 (Galaxy_Center / LDN43 / NGC1727 / NGC247 / NGC55 / NGC83_cluster / Victory_Nebula)
  - 每目标 1 帧 canonical, 共 7 帧
  - FITS header 元数据采集 (width/height/bitpix/exposure/filter/ccd_temp/object/date_obs)
  - SHA-256 重算与 manifest 比对
  - P02-001 plate solve 结果复用

### 2. Canonical 数据集验证脚本
- **文件**: `engineering/evidence/P05-001/verify_canonical_dataset.py`
- **描述**: 验证 canonical 数据集, 检查 SHA-256 完整性、PlateSolve 指标 (success/RMS/n_pairs) 与声明性预期范围
- **SHA-256**: `7A508A4E73000BF25E794079CF6ECBB7AD2D71205CDE83C3DB3752B600F6A7AE`
- **大小**: 9296 字节
- **检查项**: 每帧 8 项 (A_SHA256/B_PLATESOLVE_SUCCESS/C_PLATESOLVE_RMS/D_PLATESOLVE_N_PAIRS/E_PSF/F_PHOTOMETRIC/G_SNR/H_HISS)

### 3. Canonical 数据集 (结构化 JSON)
- **文件**: `engineering/evidence/P05-001/canonical_dataset.json`
- **描述**: 结构化 canonical 数据集, 包含 _meta (任务元信息)、by_target (目标天区分组)、frames (7 帧详细信息)
- **SHA-256**: `BB6E0CA5A6F1B3EAF27A745320DD88EC04731E2D66B20C39D80EBA0E484FC943`
- **大小**: 12371 字节
- **字段**: dataset_id, canonical_index, p02_001_index, case_id, target, target_full, panel, filename, frame_path, frame_path_absolute, sha256, sha256_manifest, sha256_match, size_bytes, size_mb, width, height, bitpix, exposure, filter, ccd_temp, object, date_obs, p02_001_success, p02_001_rms_arcsec, p02_001_n_pairs, expected_platesolve_success, expected_rms_range, expected_n_pairs_range, expected_psf_valid, expected_n_matched_range, expected_snr_has_snr, expected_hiss_size_kb

### 4. Canonical 数据集注册表 (CSV)
- **文件**: `engineering/contracts/canonical_dataset_registry.csv`
- **描述**: canonical 数据集注册表 (工程契约文件), 后续任务 (P05-002+) 引用此注册表作为参考数据集
- **SHA-256**: `934FED9FEC25D236FD0EB0C3E209DA19B2A3F40D0F9C5106201BA343E432CB9D`
- **大小**: 2562 字节
- **列**: dataset_id, target, frame_path, sha256, size_bytes, width, height, exposure, filter, ccd_temp, object, date_obs, expected_platesolve_success, expected_rms_range, expected_n_pairs_range, p02_001_rms_arcsec, p02_001_n_pairs

### 5. Canonical 数据集验证结果 (JSON)
- **文件**: `engineering/evidence/P05-001/canonical_verification.json`
- **描述**: 验证脚本结构化输出 (机器可读)
- **SHA-256**: `51CC451BE8D81BB67B25985E75A4A4D7C028461E2D174343CD1C2DFB32F2C2A9`
- **大小**: 12209 字节
- **字段**: task_id, task_name, verdict, summary, frames[] (每帧含 8 项 checks)

### 6. Canonical 数据集验证日志
- **文件**: `engineering/evidence/P05-001/canonical_verification.log`
- **描述**: 验证脚本完整运行日志 (含每帧检查详情)
- **SHA-256**: `FFA22656443DBB555BEC0A73E8CA62109C92F44025986FA55D338FEAC22D11BC`
- **大小**: 4940 字节
- **关键结果**: 28/28 实测项 PASS + 28/28 声明性项 DECLARED

### 7. 任务报告
- **文件**: `engineering/evidence/P05-001/TASK_REPORT.md`
- **描述**: 任务执行报告 (目标, 执行摘要, Canonical 帧清单, 实现细节, 代码变更, 兼容性与回滚, 数据来源, 结论)
- **SHA-256**: `B2A8F63834F0EF2950C1DC8CC11D6CA29AC8A5242E9E22787E90D686C6848A6B`
- **大小**: 6653 字节
- **VERDICT**: PASS

### 8. 测试报告
- **文件**: `engineering/evidence/P05-001/TEST_REPORT.md`
- **描述**: 详细测试报告 (测试命令, 测试详情, Real-data metrics, SHA-256 完整性验证, 预期数值范围验证汇总)
- **SHA-256**: `3317C99EC53B650B32883F0FB99BC207F650657CA83BFB39C26989CD74EFB47E`
- **大小**: 5964 字节
- **结果**: 28/28 实测 PASS, 28/28 声明性 DECLARED, 0 FAIL

### 9. 复核报告
- **文件**: `engineering/evidence/P05-001/REVIEW_REPORT.md`
- **描述**: 任务复核报告 (复核检查项 + 风险评估 + 复核结论)
- **VERDICT**: PASS

## 数据来源证据

### P02-001 manifest (输入)
- **文件**: `engineering/evidence/P02-001/testdata_manifest.json`
- **描述**: 710 帧 testdata manifest (含路径、SHA-256、元数据)
- **manifest_sha256**: `2A9BE035D326B735D6E3C751CFB5342ED9183DCC22FE83322E445F97877E7DCF`

### P02-001 plate solve 结果 (输入)
- **目录**: `engineering/evidence/P02-001/results/`
- **描述**: 710 个 frame_XXXX.json (含 success/RMS/n_pairs/WCS 等)
- **本任务引用的 7 个文件**:
  - `frame_0001.json` (Galaxy_Center, P05-001-C001)
  - `frame_0158.json` (LDN43, P05-001-C002)
  - `frame_0200.json` (NGC1727, P05-001-C003)
  - `frame_0264.json` (NGC247, P05-001-C004)
  - `frame_0332.json` (NGC55, P05-001-C005)
  - `frame_0411.json` (NGC83_cluster, P05-001-C006)
  - `frame_0483.json` (Victory_Nebula, P05-001-C007)

## Canonical 帧 SHA-256 完整性

| Dataset_ID | 目标天区 | manifest SHA-256 (前 16) | 重算 SHA-256 (前 16) | 一致 |
|---|---|---|---|---|
| P05-001-C001 | Galaxy_Center | EC34DD3DB9E90314 | EC34DD3DB9E90314 | ✓ |
| P05-001-C002 | LDN43 | D67D56BB142DD1BC | D67D56BB142DD1BC | ✓ |
| P05-001-C003 | NGC1727 | F0AADA0594B8475D | F0AADA0594B8475D | ✓ |
| P05-001-C004 | NGC247 | 6B0A2D2D0C330870 | 6B0A2D2D0C330870 | ✓ |
| P05-001-C005 | NGC55 | AA5172C6BB652E95 | AA5172C6BB652E95 | ✓ |
| P05-001-C006 | NGC83_cluster | 72F3AD2487D0F201 | 72F3AD2487D0F201 | ✓ |
| P05-001-C007 | Victory_Nebula | E43B88A4BDD8C930 | E43B88A4BDD8C930 | ✓ |

## 关键指标

| 指标 | 值 | 阈值 | 结果 |
|------|-----|------|------|
| Canonical 帧数量 | 7 | 7-14 | PASS (达到下限) |
| 目标天区数量 | 7 | 7 | PASS (全覆盖) |
| SHA-256 完整性 | 7/7 一致 | 7/7 | PASS |
| PlateSolve success rate | 7/7 = 100% | 7/7 | PASS |
| PlateSolve RMS 范围 | 0.1174" ~ 0.3975" | < 1.0" | PASS |
| PlateSolve n_pairs 范围 | 32 ~ 45 | > 10 | PASS |
| PSF 有效参数 | DECLARED | 非 NaN | DECLARED |
| 测光 n_matched | DECLARED | [0, 5000] | DECLARED |
| SNR has_snr | DECLARED | 0_or_1 | DECLARED |
| HISS 文件大小 | DECLARED | > 10KB | DECLARED |

## 业务源码变更
- **无**: 本任务为数据集登记, 不修改任何业务源码 (lib/ 目录无变更)
- **仅新增工程文件**: engineering/evidence/P05-001/ + engineering/contracts/canonical_dataset_registry.csv

## 兼容性与回滚
- **兼容性**: 完全兼容, 不影响现有功能
- **回滚**: 删除 `engineering/evidence/P05-001/` 目录与 `engineering/contracts/canonical_dataset_registry.csv` 即可回滚
- **残留风险**: 无
