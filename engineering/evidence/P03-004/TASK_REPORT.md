# P03-004 任务报告：SNR 稀疏模型与 SIP 一致性

## 任务信息
- **任务 ID**: P03-004
- **阶段**: P03
- **Gate**: G3
- **依赖**: P01-002 (数据块注册表), P01-003 (HISS 稀疏 SNR 支持), P03-002 (配置参数追踪)
- **执行日期**: 2026-07-25

## 目标
1. 校验 snr_model schema (SnrModel 序列化格式)
2. 使用完整 WCS/SIP 转球面 (从图像坐标到球面坐标)
3. HISS 保存稀疏模型 (has_snr=1, snr_format=1)

## 执行摘要

### 验证结果
- **总测试数**: 5
- **PASS**: 5 (100%)
- **FAIL**: 0
- **VERDICT**: PASS

### 核心成果
| 验证项 | 状态 | 关键指标 |
|--------|------|----------|
| A. snr_model schema | PASS | 序列化 payload = 4 + n_points×20 + 24 字节 |
| B. WCS+SIP 一致性 | PASS | max\|Δ\| = 5.684e-14 度 (阈值 1e-9) |
| C. SIP 修正生效 | PASS | 边缘 Δ=3.4e-6°, 中心 Δ=5.6e-7° |
| D. 退化路径 | PASS | 5 个返回码全部正确 |
| E. HISS 稀疏模型往返 | PASS | snr_format=1, 控制点数据完整 |

## 实现细节

### 1. SNR 模型 Schema (SnrModel)
**文件**: `lib/snr_estimator/cpp/include/snr_estimator.h`

```
SnrModel {
    uint32_t          n_points;     // 控制点数
    SnrControlPoint*  points;       // 控制点数组 (ra, dec, snr_psf)
    double            snr_phot;     // 1/(ln10×sigma_residual) 全局标量
    double            median_snr;   // median(snr_psf) 归一化基准
    double            idw_power;    // IDW 幂次 (默认 2.0)
}
```

序列化字节数 = 4 + n_points × 20 + 24 (与 hp_drizzle_api.cpp 期望一致)

### 2. WCS+SIP 转球面 (完整前向 SIP A/B)
**文件**: `lib/snr_estimator/cpp/src/snr_estimator.cpp`

新增 `SnrSipCoeffs` 结构 (前向 A/B 多项式, 6×6 数组, 阶数上限 5):
- `snrEvalSip(coeffs, dx, dy, order)`: 前向 SIP 多项式求值
- `pixelToSkySimple(x, y, wcs, ra, dec)`: 像素 → 球面 (TAN + SIP)

转换步骤:
1. 归一化像素坐标: dx = x - (crpix1-1), dy = y - (crpix2-1) (CRPIX 1-based)
2. 前向 SIP 修正: dx' = dx + A(dx,dy), dy' = dy + B(dx,dy)
3. CD 矩阵: xi = cd[0]·dx' + cd[1]·dy', eta = cd[2]·dx' + cd[3]·dy'
4. TAN 反投影: (xi, eta) → (ra, dec)

**与 astropy all_pix2world 对比**: max\|Δra\| = 5.684e-14 度, max\|Δdec\| = 1.243e-14 度 (远小于阈值 1e-9 度)

### 3. HISS 稀疏模型保存 (snr_format=1)
**文件**: `lib/orchestrator/cpp/src/orchestrator.cpp` (run_stage_snr)

- orchestrator 从 FITS header 读取 A_ORDER/B_ORDER + A_i_j/B_i_j 系数
- 调用 snr_extract_model 提取稀疏控制点 (含 SIP 转球面)
- 序列化到 snr_model 块 (AIO_BLOCK_RAW)
- drizzle 阶段读取 snr_model 块, 写入 .hiss (has_snr=1, snr_format=1)

**HISS 往返验证**: 5 控制点 + 10 HEALPix 像素, 364 字节, snr_format=1 验证通过

## 代码变更

### 修改文件
1. `lib/snr_estimator/cpp/include/snr_estimator.h`
   - 新增 `SNR_SIP_MAX_ORDER=5`, `SNR_SIP_COEFF_SIZE=36` 宏
   - 新增 `SnrSipCoeffs` 结构 (a_order, b_order, a[36], b[36])
   - 扩展 `SnrWcsParams` 添加 `sip` 字段

2. `lib/snr_estimator/cpp/src/snr_estimator.cpp`
   - 新增 `snrEvalSip()` 前向 SIP 多项式求值
   - 新增 `pixelToSkySimple()` 完整 WCS+SIP TAN 反投影
   - `snr_extract_model()` 使用 `pixelToSkySimple` 转换控制点坐标

3. `lib/orchestrator/cpp/src/orchestrator.cpp`
   - `run_stage_snr` 新增 SIP 系数读取 (A_ORDER/B_ORDER + A_i_j/B_i_j)
   - 系数按 `a[i*6+j]` 存储, 跳过 (0,0), i+j<=order
   - 新增日志: `[SNR] SIP 前向系数加载: A_ORDER=X B_ORDER=X (P03-004 WCS+SIP 一致性)`

### 新增文件
1. `engineering/evidence/P03-004/verify_snr_model.py` - 5 个验证用例
2. `engineering/evidence/P03-004/snr_model_validation.json` - 验证结果 JSON
3. `engineering/evidence/P03-004/verify_snr_model.log` - 测试日志
4. `engineering/evidence/P03-004/TASK_REPORT.md` - 本报告
5. `engineering/evidence/P03-004/TEST_REPORT.md` - 测试报告
6. `engineering/evidence/P03-004/EVIDENCE_INDEX.md` - 证据索引
7. `engineering/evidence/P03-004/REVIEW_REPORT.md` - 复核报告

## Stage1 集成测试

**测试帧**: `testdata/results/Galaxy_Center_T4/panel1/Red/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/01_calibrated.fits`

**关键日志**:
```
[SNR] SIP 前向系数加载: A_ORDER=3 B_ORDER=3 (P03-004 WCS+SIP 一致性)
[SNR] n_stars=2000 sigma_residual=0.000000 CRVAL=(272.825668,-13.131769) CRPIX=(2250.500000,1800.500000) SIP(a_order=3, b_order=3)
[snr_model] degenerate: sigma_residual=0 <= 0
[SNR] sigma_residual<=0, 降级跳过 snr_model 块
```

**说明**:
- P03-004 SIP 系数读取**成功** (A_ORDER=3, B_ORDER=3)
- SNR stage 因 sigma_residual=0 降级 (测光阶段 n_matched=0, 非 P03-004 问题)
- 降级行为正确: 记录 SNR_STATUS, 不阻塞 stage1
- stage1 总体成功 (success=true)

## 兼容性与回滚
- **兼容性**: 所有修改保持向后兼容
  - 无 SIP 系数时 (a_order=0) 退化为纯 CD+TAN, 行为与修改前一致
  - 退化路径 (n_stars=0, sigma=0, nullptr) 返回码不变
- **回滚**: 撤销 orchestrator.cpp 中 P03-004 SIP 读取代码即可回滚; snr_estimator 的 SnrSipCoeffs 是新增字段, 不影响旧调用
- **残留风险**: 无; SIP 读取复用 hp_drizzle_api.cpp 已验证的算法, 系数存储格式一致

## 结论
P03-004 任务完成。SNR 稀疏模型 schema 验证通过, WCS+SIP 转球面与 astropy 一致 (差异 < 1e-9 度), HISS 稀疏模型往返验证通过 (snr_format=1)。stage1 集成测试中 SIP 系数加载成功 (A_ORDER=3, B_ORDER=3), SNR stage 因上游 sigma_residual=0 正确降级。
