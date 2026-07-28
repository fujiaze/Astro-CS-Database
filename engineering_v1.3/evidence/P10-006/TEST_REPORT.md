# 测试报告

- Task/ADR：P10-006 T1-T4 真实校准代表帧验证
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

独立验证 P10-006 校准输出（CSV/JSON/16 个 FITS）满足任务定义 `tasks/P10-006.md` 和规范 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md` 的全部要求，覆盖 contract/unit/e2e/forbidden shortcut 四个维度。

## 输入与范围

- 任务定义：tasks/P10-006.md
- 规范：docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md
- 交付物：REPRESENTATIVE_CALIBRATION_REPORT.csv + CALIBRATION_VALIDATION_SUMMARY.json + 16 个 calibrated FITS
- 脚本：scripts/validate_representative_calibration.py + scripts/test_calibration_outputs.py
- 原始日志：raw_logs/

## 执行/决策

### 测试矩阵

| 测试维度 | 测试点数 | 通过 | 失败 | 跳过 |
|---------|---------|------|------|------|
| contract | 5 | 5 | 0 | 0 |
| unit | 12 | 12 | 0 | 0 |
| e2e | 4 | 4 | 0 | 0 |
| forbidden_shortcut | 4 | 4 | 0 | 0 |
| **总计** | **25** | **25** | **0** | **0** |

### contract 测试详情

| 测试 | 验证内容 | 结果 |
|------|---------|------|
| C-01 CSV 文件存在 | REPRESENTATIVE_CALIBRATION_REPORT.csv 存在 | PASS |
| C-02 JSON 文件存在 | CALIBRATION_VALIDATION_SUMMARY.json 存在 | PASS |
| C-03 CSV 行数 | 16 行 (T2:5 + T3:6 + T4:5) | PASS |
| C-04 CSV 字段完整 | 30 个必需字段全部存在 | PASS |
| C-05 JSON 汇总字段完整 | 10 个必需字段全部存在 | PASS |

### unit 测试详情

| 测试 | 验证内容 | 结果 |
|------|---------|------|
| U-01 全部 16 个组合存在 | (device, filter) 组合完整覆盖 | PASS |
| U-02 无多余组合 | 无非预期组合 | PASS |
| U-03 T1 无数据未参与 | T1 行数=0 | PASS |
| U-04 T2 缺 Lum Flat 未参与 | T2/LUM 行数=0 | PASS |
| U-05 T4 缺 Lum Flat 未参与 | T4/LUM 行数=0 | PASS |
| U-06 T3 有 Lum Flat 参与 | T3/LUM 行数=1 | PASS |
| U-07 校准后无 NaN | 16 帧 nan_count=0 | PASS |
| U-08 校准后无 Inf | 16 帧 inf_count=0 | PASS |
| U-09 Light/Calibrated 尺寸一致 | light_shape == calibrated_shape | PASS |
| U-10 坏点比例 < 1% | 16 帧 bad_pixel_ratio=0.0 | PASS |
| U-11 统计量有限 | 16 帧 mean/std 均为有限值 | PASS |
| U-12 校准后 mean > 0 | 16 帧 mean 全部 > 0 | PASS |

### e2e 测试详情

| 测试 | 验证内容 | 结果 |
|------|---------|------|
| E-01 全部 16 帧通过 | passed=YES 全部 16 行 | PASS |
| E-02 JSON 通过率 100% | pass_rate=100.0 | PASS |
| E-03 success_count | success_count=16 | PASS |
| E-04 全部 16 个校准后 FITS 存在 | 16 个 .fits 文件全部存在 | PASS |

### forbidden_shortcut 测试详情

| 测试 | 验证内容 | 结果 |
|------|---------|------|
| F-01 无 T1 伪造数据 | 未生成 T1 校准结果 | PASS |
| F-02 无 T2/T4 Lum 替代 | T2/T4 缺 Lum Flat 未用其他滤镜替代 | PASS |
| F-03 全部 Light 路径真实存在 | 16 个 light_path 全部指向真实文件 | PASS |
| F-04 全部 Master 文件真实存在 | 48 个 Master 路径（16 × 3）全部指向真实文件 | PASS |

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python test_calibration_outputs.py` | 60s | 0 |

## 结果与证据

- 25/25 测试 PASS
- contract: 5/5 PASS（交付物完整性）
- unit: 12/12 PASS（数据合法性）
- e2e: 4/4 PASS（端到端通过率 100%）
- forbidden_shortcut: 4/4 PASS（禁止捷径）

## 风险/回滚/残留

- 无失败测试，无需回滚
- 测试规模与 P10-005（23 测试）一致，覆盖维度相同

## 结论

P10-006 测试套件全部通过。25/25 测试 PASS，覆盖 contract/unit/e2e/forbidden shortcut 四个维度。校准输出（CSV/JSON/16 个 FITS）完整性、合法性、端到端通过率、禁止捷径全部通过。
