# 复核报告

- Task/ADR：P10-006 T1-T4 真实校准代表帧验证
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

独立复核 P10-006 是否满足任务定义 `tasks/P10-006.md` 和规范 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md` 的全部要求。

## 输入与范围

- 任务定义：tasks/P10-006.md
- 规范：docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md
- 依赖：P10-005 LIGHT_TO_MASTER_RESOLUTION.csv（710 Light 帧 + Bias/Dark/Flat 匹配结果）
- 交付物：REPRESENTATIVE_CALIBRATION_REPORT.csv + CALIBRATION_VALIDATION_SUMMARY.json + 16 个 calibrated FITS
- 脚本：scripts/validate_representative_calibration.py + scripts/test_calibration_outputs.py
- 原始日志：raw_logs/

## 执行/决策

### 复核矩阵

| 复核项 | 验证内容 | 结果 |
|--------|----------|------|
| R-01 任务要求覆盖 | 任务定义的全部要求是否满足 | PASS |
| R-02 交付物完整性 | 3 个数据交付物 + 2 个脚本交付物是否齐全 | PASS |
| R-03 代表帧数 16 (T2:5+T3:6+T4:5) | 预期代表帧组合完整 | PASS |
| R-04 T1 无数据未参与 | T1 不在 16 个代表帧中 | PASS |
| R-05 T2/T4 缺 Lum Flat 未参与 | T2/T4 各 5 帧（无 Lum），未用其他滤镜替代 | PASS |
| R-06 T3 含 Lum 代表帧 | T3/LUM 1 帧参与（T3 有 Lum Flat） | PASS |
| R-07 校准公式一致性 | (Light - Dark) / NormalizedFlat 与项目 calibrator.py 一致 | PASS |
| R-08 Flat 归一化策略 | median=1.0, 最小值裁剪 0.1 | PASS |
| R-09 尺寸一致性 | 16 帧 Light/Bias/Dark/Flat 形状全部相同 | PASS |
| R-10 无 NaN | 16 帧校准后 nan_count=0 | PASS |
| R-11 无 Inf | 16 帧校准后 inf_count=0 | PASS |
| R-12 无极端值 | 16 帧校准后 extreme_count=0 | PASS |
| R-13 坏点比例 0% | 16 帧坏点比例全部 = 0.0 | PASS |
| R-14 统计量有限 | 16 帧 mean/std 均为有限值 | PASS |
| R-15 校准后 mean > 0 | 16 帧 mean 全部 > 0（符合物理预期） | PASS |
| R-16 校准后 FITS 文件存在 | 16 个 .fits 全部生成 | PASS |
| R-17 FITS 含 CALIBRAT/SRCFRAME 关键字 | FITS 头信息完整 | PASS |
| R-18 测试覆盖与通过率 | 25/25 测试 PASS | PASS |
| R-19 禁止捷径: 无 T1 伪造 | 未生成 T1 校准结果 | PASS |
| R-20 禁止捷径: 无 Lum 替代 | T2/T4 缺 Lum Flat 未用其他滤镜替代 | PASS |
| R-21 禁止捷径: Light 路径真实 | 16 个 light_path 全部指向真实文件 | PASS |
| R-22 禁止捷径: Master 文件真实 | 48 个 Master 路径全部指向真实文件 | PASS |
| R-23 与 P10-005 一致性 | 代表帧全部来自 P10-005 resolved=YES | PASS |
| R-24 与 P10-002/P10-003 一致性 | 设备/滤镜/Master 信息与档案一致 | PASS |
| R-25 与 P10-004 一致性 | 滤镜规范名使用 LUM/RED/GREEN/BLUE/HA/OIII | PASS |

### R-01 任务要求覆盖

任务定义要求：
1. ✅ 每套设备和滤镜类选代表帧实际校准
   - 16 个 (device, filter_canonical) 组合各选 1 个代表帧
   - 每个代表帧实际执行标准 CCD 校准
2. ✅ 验证尺寸/统计/坏点
   - 尺寸：Light/Bias/Dark/Flat 形状一致性（5 项校验）
   - 统计：min/max/mean/std/median 全部记录
   - 坏点：NaN/Inf/饱和/极端值 全部检查
3. ✅ 校准报告 + 输出统计 + Gate 复核
   - REPRESENTATIVE_CALIBRATION_REPORT.csv（16 行）
   - CALIBRATION_VALIDATION_SUMMARY.json（汇总）
   - 16 个 calibrated FITS（输出统计示例）
   - TASK/TEST/EVIDENCE/REVIEW 4 报告完整

规范要求：
- ✅ 每套设备（T2/T3/T4，T1 无数据）和滤镜类选代表帧
- ✅ 实际校准（标准公式）
- ✅ 验证尺寸/统计/坏点

### R-02 交付物完整性

| 交付物 | 状态 | 内容 |
|--------|------|------|
| REPRESENTATIVE_CALIBRATION_REPORT.csv | PASS | 16 行，30 字段，含路径/统计/坏点/通过判定 |
| CALIBRATION_VALIDATION_SUMMARY.json | PASS | 汇总（按设备/按滤镜 + 通过率 + 16 详情） |
| calibrated/*.fits (16 个) | PASS | 16 个 float32 FITS（CALIBRAT=TRUE 关键字） |
| scripts/validate_representative_calibration.py | PASS | 校准主脚本 475 行 |
| scripts/test_calibration_outputs.py | PASS | 测试套件 332 行，25 测试 |

### R-07 校准公式一致性

任务实现采用项目标准公式（与 `lib/calibration/python/calibrator.py` 一致）：

```python
# 标准模式: (Light - Dark) / NormalizedFlat
# Dark 已含 Bias, 直接减 Dark
calibrated = (light - dark) / flat_norm
```

与规范 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md` 一致：
- Dark 已含 Bias（P10-003 masterDark 由 dark 帧堆叠且未减 Bias）
- Flat 归一化到 median=1.0，最小值裁剪 0.1

PASS。

### R-09 尺寸一致性

16 个代表帧全部满足 Light/Bias/Dark/Flat 形状一致性：

| 设备 | 尺寸 | 代表帧数 |
|------|------|---------|
| T2 | 4096x4096 | 5 |
| T3 | 4096x4096 | 6 |
| T4 | 3600x4500 | 5 |

PASS。

### R-10/R-11/R-12 坏点检查

| 检查项 | 16 帧汇总 |
|--------|----------|
| NaN 总数 | 0 |
| Inf 总数 | 0 |
| Extreme 总数（abs > 1e6） | 0 |
| 坏点比例 | 0.0% (全部 16 帧) |

PASS。

### R-14/R-15 统计验证

| 验证项 | 16 帧汇总 |
|--------|----------|
| mean 范围 | 1062.33 ~ 2365.90 ADU（全部 > 0） |
| std 范围 | 271.79 ~ 1085.43 ADU |
| 有限值 | 16/16 PASS |

PASS。

### R-18 测试覆盖与通过率

- 测试脚本：scripts/test_calibration_outputs.py
- 测试点数：25（contract 5 + unit 12 + e2e 4 + forbidden_shortcut 4）
- 通过率：25/25 (100%)
- 失败数：0
- 跳过数：0
- PASS

### R-19/R-20/R-21/R-22 禁止捷径检查

- 无 T1 伪造数据（T1 不在 16 帧中）✅
- 无 T2/T4 Lum 替代（T2/T4 各 5 帧无 Lum，未用其他滤镜替代）✅
- 全部 16 个 Light 路径真实存在 ✅
- 全部 48 个 Master 路径真实存在（16 帧 × 3 master） ✅
- PASS

### R-23 与 P10-005 一致性

代表帧选择策略：从 `P10-005/LIGHT_TO_MASTER_RESOLUTION.csv` 中，每个 (device_id, filter_canonical) 组合选第一个 `resolved=YES` 的帧。

- 16 个代表帧全部来自 P10-005 resolved=YES
- 与 P10-005 的 587 resolved 帧一致
- 无伪造、无虚构
- PASS

### R-24/R-25 与上游任务一致性

- R-24: 设备 ID（T2/T3/T4）+ Master 路径与 P10-002/P10-003 一致
- R-25: 滤镜规范名（LUM/RED/GREEN/BLUE/HA/OIII）与 P10-004 一致
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python validate_representative_calibration.py` | 300s | 0 |
| `python test_calibration_outputs.py` | 60s | 0 |

## 结果与证据

- 25/25 复核项 PASS
- 3 个数据交付物完整（CSV 16 行 + JSON + 16 个 FITS）
- 2 个脚本交付物完整（校准脚本 475 行 + 测试套件 332 行）
- 16/16 代表帧 PASS（100% 通过率）
- 25/25 测试 PASS（contract/unit/e2e/forbidden_shortcut 四维度）
- 0 NaN / 0 Inf / 0 极端值（坏点比例 0%）
- 校准公式与项目 calibrator.py 一致
- 与 P10-002/P10-003/P10-004/P10-005 上游任务全部一致

## 风险/回滚/残留

- 123 Lum Light 帧（T2 25 + T4 98）缺 Lum Flat Master，本任务跳过 Lum 代表帧校准。后续 P12-004 测光矩阵验证前需用户决策。
- 16 个 calibrated FITS 共约 1 GB，可用作后续 P11/P12 阶段输入示例。
- 未启用暗场优化（dark_optimization=True），后续 P12-004 测光验证可启用。

## 结论

P10-006 独立复核通过。25 项复核全部 PASS。3 个数据交付物完整（REPRESENTATIVE_CALIBRATION_REPORT.csv 16 行 + CALIBRATION_VALIDATION_SUMMARY.json + 16 个 calibrated FITS）。2 个脚本交付物完整（校准脚本 + 测试套件）。16/16 代表帧 PASS（T2:5 + T3:6 + T4:5），100% 通过率。25/25 测试通过，覆盖 contract/unit/e2e/forbidden shortcut 四个维度。校准公式 (Light - Dark) / NormalizedFlat 与项目 calibrator.py 一致。0 NaN/0 Inf/0 极端值，坏点比例 0%。禁止捷径检查通过（无 T1 伪造、无 Lum 替代、全部 Light/Master 路径真实存在）。与 P10-002/P10-003/P10-004/P10-005 上游任务全部一致。

P10 Gate 关键证据齐备：T1-T4 设备档案、滤镜规范、Master 盘点、Light-to-Master 解析、实际校准全链路贯通。

VERDICT: PASS
