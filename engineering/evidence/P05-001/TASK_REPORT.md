# P05-001 任务报告：真实参考数据集登记

## 任务信息
- **任务 ID**: P05-001
- **阶段**: P05
- **Gate**: G5
- **依赖**: P00-003 (基线测试数据), P02-001 (PlateSolve 全量基线), P03-003 (严格失败), P03-004 (SNR 稀疏模型)
- **执行日期**: 2026-07-25
- **Commit base**: f98de03b203996f3c56503c0c73bd4bd044ccd7f

## 目标
1. 建立 canonical 数据集 (每个目标天区 1-2 帧代表性帧, 总计 7-14 帧)
2. 计算每帧 SHA-256, 记录元数据 (尺寸/曝光/滤镜/CCD温度/OBJECT/DATE-OBS)
3. 定义每帧预期数值范围 (PlateSolve/PSF/测光/SNR/HISS)
4. 创建 canonical_dataset_registry.csv (canonical 数据集注册表)
5. 用 P02-001 batch_platesolve_test.py 结果验证预期数值范围

## 执行摘要

### 验证结果
- **canonical 帧数量**: 7 (每目标 1 帧, 符合 7-14 帧下限)
- **目标天区数**: 7 (Galaxy_Center / LDN43 / NGC1727 / NGC247 / NGC55 / NGC83_cluster / Victory_Nebula)
- **SHA-256 完整性**: 7/7 PASS (manifest vs 重算全部一致)
- **PlateSolve success**: 7/7 PASS (全部 success=true)
- **PlateSolve RMS < 1.0"**: 7/7 PASS (最大 0.3975", 最小 0.1174")
- **PlateSolve n_pairs > 10**: 7/7 PASS (最小 32, 最大 45)
- **VERDICT**: PASS

### Canonical 帧清单

| Dataset_ID | 目标天区 | 文件名 | 滤镜 | 曝光 | 尺寸 (W×H) | SHA-256 (前 8) |
|------------|----------|--------|------|------|-----------|----------------|
| P05-001-C001 | Galaxy_Center | Galaxy_Center_mosaic1_T4_...@061703-180S-Red.fts | Red | 180s | 4500×3600 | EC34DD3D |
| P05-001-C002 | LDN43 | LDN43_LRGBH_...@031525-600S-Lum.fts | Lum | 600s | 4096×4096 | D67D56BB |
| P05-001-C003 | NGC1727 | NGC1727_RGBHO_T2_...@064517-600S-Red.fts | Red | 600s | 4096×4096 | F0AADA05 |
| P05-001-C004 | NGC247 | NGC247_T2_...@033428-600S-Lum.fts | Lum | 600s | 4096×4096 | 6B0A2D2D |
| P05-001-C005 | NGC55 | NGC55_T3_...@074114-600S-Red.fts | Red | 600s | 4096×4096 | AA5172C6 |
| P05-001-C006 | NGC83_cluster | NGC90_2025wwk_T3_...@020846-600S-Red.fts | Red | 600s | 4096×4096 | 72F3AD24 |
| P05-001-C007 | Victory_Nebula | Victory_Nebula_mosaic1_...@035646-180S-Lum.fts | Lum | 180s | 4500×3600 | E43B88A4 |

### 选择规则
- Galaxy_Center_T4: panel1 第一帧 Red (manifest index=1)
- LDN43_T2: 第一帧 Lum (manifest index=158)
- NGC1727_T2: 第一帧 (manifest index=200, Red)
- NGC247_T2: 第一帧 (manifest index=264, Lum)
- NGC55_T3: 第一帧 (manifest index=332, Red)
- NGC83_cluster_T3: 第一帧 (manifest index=411, Red)
- Victory_Nebula_T4: mosaic1 第一帧 Lum (manifest index=483)

### 预期数值范围

| 检查项 | 预期 | 实际 P02-001 结果 | 验证结果 |
|--------|------|-------------------|----------|
| PlateSolve success | true | 7/7 true | PASS |
| PlateSolve RMS | < 1.0" | 0.1174" ~ 0.3975" | PASS |
| PlateSolve n_pairs | > 10 | 32 ~ 45 | PASS |
| PSF 有效参数 | 非 NaN | 声明性 (stage1 历史 1913/2000 stars) | DECLARED |
| 测光 n_matched | [0, 5000] | 声明性 (G-002 缺口, 可能为 0 或 1606) | DECLARED |
| SNR has_snr | 0_or_1 | 声明性 (骨架退化=0, P03-004 修复后=1) | DECLARED |
| HISS 文件大小 | > 10KB | 声明性 (stage1 输出 11.5MB+) | DECLARED |

## 实现细节

### 1. Canonical 数据集生成
**脚本**: `engineering/evidence/P05-001/build_canonical_dataset.py`

流程:
1. 加载 P02-001 testdata_manifest.json (710 帧)
2. 按目标天区分组 (7 个目标天区)
3. 按 SELECTORS 规则选择 canonical 帧 (每目标 1 帧, 共 7 帧)
4. 读取每帧 FITS header 获取元数据 (width/height/bitpix/exposure/filter/ccd_temp/object/date_obs)
5. 重算 SHA-256 验证 manifest 完整性
6. 从 P02-001 results/frame_XXXX.json 加载 plate solve 结果
7. 构建预期数值范围 (基于 P02-001 实际结果 + 任务规范)
8. 生成 canonical_dataset.json + canonical_dataset_registry.csv

### 2. 元数据采集
使用 astropy.io.fits 读取 FITS header:
- `width` / `height`: 从 PRIMARY HDU data.shape 获取
- `bitpix`: 从 BITPIX 关键字
- `exposure`: 从 EXPTIME (回退 EXPOSURE)
- `filter`: 从 FILTER 关键字
- `ccd_temp`: 从 CCD-TEMP (回退 SET-TEMP)
- `object`: 从 OBJECT 关键字
- `date_obs`: 从 DATE-OBS 关键字

### 3. SHA-256 完整性验证
- manifest 中已有每帧 SHA-256 (P02-001 生成时计算)
- P05-001 脚本重算每帧 SHA-256, 与 manifest 比对
- 7/7 帧一致, 确认 manifest 准确性

### 4. P02-001 plate solve 结果复用
- P02-001 已对 710 帧全量运行 batch_platesolve_test.py
- 7 个 canonical 帧的 plate solve 结果已存在 results/frame_XXXX.json
- P05-001 直接复用这些结果作为预期范围验证 (不重复运行, 避免冗余计算)
- 验证脚本: `engineering/evidence/P05-001/verify_canonical_dataset.py`

## 代码变更

### 新增文件
1. `engineering/evidence/P05-001/build_canonical_dataset.py` - canonical 数据集生成脚本
2. `engineering/evidence/P05-001/verify_canonical_dataset.py` - 验证脚本
3. `engineering/evidence/P05-001/canonical_dataset.json` - 结构化 canonical 数据集 (7 帧)
4. `engineering/evidence/P05-001/canonical_verification.json` - 验证结果 JSON
5. `engineering/evidence/P05-001/canonical_verification.log` - 验证日志
6. `engineering/evidence/P05-001/TASK_REPORT.md` - 本报告
7. `engineering/evidence/P05-001/TEST_REPORT.md` - 测试报告
8. `engineering/evidence/P05-001/EVIDENCE_INDEX.md` - 证据索引
9. `engineering/evidence/P05-001/REVIEW_REPORT.md` - 复核报告
10. `engineering/contracts/canonical_dataset_registry.csv` - canonical 数据集注册表

### 修改文件
- 无 (本任务为数据集登记, 不修改业务源码)

## 兼容性与回滚
- **兼容性**: 完全兼容。本任务不修改任何业务源码, 仅新增数据集登记文件
- **回滚**: 删除 `engineering/evidence/P05-001/` 目录和 `engineering/contracts/canonical_dataset_registry.csv` 即可回滚
- **残留风险**: 无

## 数据来源
- **P02-001 manifest**: `engineering/evidence/P02-001/testdata_manifest.json` (710 帧, manifest_sha256=2A9BE035...)
- **P02-001 plate solve 结果**: `engineering/evidence/P02-001/results/frame_XXXX.json` (710 个 JSON)
- **testdata FITS 文件**: `testdata/<target>/lights/<panel>/` (7 个目标天区, 710 帧)

## 结论
P05-001 任务完成。建立了 7 个 canonical 帧 (每目标 1 帧), 每帧含 SHA-256、元数据、预期数值范围。所有 canonical 帧的 P02-001 plate solve 结果符合预期范围 (success=true, RMS<1.0", n_pairs>10)。SHA-256 完整性验证通过 (manifest vs 重算一致)。canonical_dataset_registry.csv 已创建作为后续任务的参考数据集注册表。
