# 任务报告

- Task/ADR：P10-006 T1-T4 真实校准代表帧验证
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

依据 `tasks/P10-006.md` 和 `docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`，每套设备 (T2/T3/T4) 和滤镜类选 1 个代表帧，实际执行标准 CCD 校准，验证尺寸/统计/坏点（NaN/Inf/饱和/极端值）。

T1 无数据（跳过）；T2/T4 缺 Lum Flat（跳过 Lum 代表帧）；T3 全部 6 滤镜均有 Flat（包含 Lum 代表帧）。

总代表帧数 = T2 (5 滤镜) + T3 (6 滤镜) + T4 (5 滤镜) = **16**。

## 输入与范围

- 依赖（已满足）：P10-005（Light-to-Master 解析结果）
- 数据源 1：`P10-005/LIGHT_TO_MASTER_RESOLUTION.csv`（710 Light 帧 + Bias/Dark/Flat 匹配结果）
- 数据源 2：`P10-003/CALIBRATION_MASTER_INVENTORY.csv`（27 个 Master 元数据）
- 数据源 3：`testdata/**/*.fts`（710 Light 帧，T2 174 + T3 104 + T4 432）
- 数据源 4：`testdata/T{2,3,4} calibration files/*.xisf`（27 个 Master XISF）
- 参考规范：`docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md`
- 工具：`engineering_v1.2/evidence/P10-006/scripts/validate_representative_calibration.py`
- 验证：`engineering_v1.2/evidence/P10-006/scripts/test_calibration_outputs.py`（25 测试）

## 执行/决策

### 阶段 1：选择代表帧

从 `P10-005/LIGHT_TO_MASTER_RESOLUTION.csv` 中，对每个 (device_id, filter_canonical) 组合选第一个 `resolved=YES` 的帧。

预期 16 个组合（与 P10-002 设备档案和 P10-004 滤镜规范名一致）：

| 设备 | 滤镜 | 代表帧目标 | 曝光 | 尺寸 |
|------|------|-----------|------|------|
| T2 | BLUE | LDN43 | 1200s | 4096x4096 |
| T2 | GREEN | LDN43 | 1200s | 4096x4096 |
| T2 | HA | LDN43 | 1200s | 4096x4096 |
| T2 | OIII | NGC1727 | 1800s | 4096x4096 |
| T2 | RED | LDN43 | 1200s | 4096x4096 |
| T3 | BLUE | NGC55 | 600s | 4096x4096 |
| T3 | GREEN | NGC55 | 600s | 4096x4096 |
| T3 | HA | NGC55 | 1200s | 4096x4096 |
| T3 | LUM | NGC55 | 600s | 4096x4096 |
| T3 | OIII | NGC55 | 1200s | 4096x4096 |
| T3 | RED | NGC55 | 600s | 4096x4096 |
| T4 | BLUE | Galaxy_Center | 180s | 4500x3600 |
| T4 | GREEN | Galaxy_Center | 180s | 4500x3600 |
| T4 | HA | Galaxy_Center | 300s | 4500x3600 |
| T4 | OIII | Galaxy_Center | 600s | 4500x3600 |
| T4 | RED | Galaxy_Center | 180s | 4500x3600 |

### 阶段 2：标准 CCD 校准

采用项目标准校准公式（与 `lib/calibration/python/calibrator.py` 一致）：

```
Calibrated = (Light - Dark) / NormalizedFlat
```

- Dark 已含 Bias（P10-003 的 masterDark 由 dark 帧堆叠且未减 Bias），直接减 Dark 即可
- Flat 归一化到 median=1.0，最小值裁剪 0.1（防止除以接近 0 的值导致极端像素）

实现细节：
- Light 帧读取使用 `lib/astro_image_io` 的 `ImageReader.read()`（自动检测 FITS/XISF）
- Master 文件读取使用 `ImageReader.read_xisf()`（27 个 Master 全部为 XISF 格式）
- 16-bit FITS Light 帧自动应用 BSCALE=1.0/BZERO=32768.0 转换为物理 ADU

### 阶段 3：尺寸/统计/坏点验证

每个代表帧执行 5 项验证：

1. **尺寸一致性**：Light/Bias/Dark/Flat 形状必须全部相同
2. **NaN 检查**：校准后图像中 NaN 像素数必须为 0
3. **Inf 检查**：校准后图像中 Inf 像素数必须为 0
4. **极端值检查**：校准后 abs(pixel) > 1e6 的像素比例必须 < 1%（通常是 Flat 异常导致）
5. **统计量有限性**：校准后 mean/std 必须是有限值（非 NaN/Inf）

### 阶段 4：坏点判定

每个代表帧的坏点定义为：`NaN + Inf + 极端值` 总数。坏点比例 = 坏点数 / 总像素数。

通过条件：`size_ok AND nan_ok AND inf_ok AND extreme_ok AND stats_ok`

### 阶段 5：写出校准后 FITS

使用 `lib/astro_image_io` 的 `FITSWriter.write()` 写出 16 个 float32 FITS 文件作为代表帧示例：

- `engineering_v1.2/evidence/P10-006/calibrated/<device>_<filter>_calibrated.fits`
- 每个 FITS 包含 `CALIBRAT=TRUE` 和 `SRCFRAME=<原始 Light 文件名>` 关键字

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python validate_representative_calibration.py` | 300s | 0 |
| `python test_calibration_outputs.py` | 60s | 0 |

## 结果与证据

### 交付物

1. **REPRESENTATIVE_CALIBRATION_REPORT.csv** — 16 行，每行一个代表帧校准结果（含 Light/Bias/Dark/Flat 路径 + 校准前后统计 + 坏点数 + 通过判定）
2. **CALIBRATION_VALIDATION_SUMMARY.json** — 汇总统计（按设备/按滤镜分布 + 通过率 + 16 个代表帧详情）
3. **calibrated/*.fits** — 16 个校准后 FITS 文件（float32，含 CALIBRAT/SRCFRAME 关键字）

### 关键统计

| 指标 | 值 |
|------|-----|
| 预期代表帧数 | 16 |
| 实际校准数 | 16 |
| 成功 | 16 (100%) |
| 失败 | 0 |
| 按设备分布 | T2:5 T3:6 T4:5 |
| 按滤镜分布 | BLUE:3 GREEN:3 HA:3 LUM:1 OIII:3 RED:3 |
| 总坏点 | 0 (NaN=0, Inf=0, Extreme=0) |
| 校准后尺寸一致性 | 16/16 PASS |
| 校准后统计量有限 | 16/16 PASS |

### 16 个代表帧校准后统计

| 设备/滤镜 | 目标 | 曝光 | 尺寸 | mean | std | median | 饱和 |
|----------|------|------|------|------|-----|--------|------|
| T4/RED | Galaxy_Center | 180s | 3600x4500 | 1564.02 | 1085.43 | 1471.60 | 1688 |
| T4/GREEN | Galaxy_Center | 180s | 3600x4500 | 1506.16 | 881.55 | 1442.75 | 1162 |
| T4/BLUE | Galaxy_Center | 180s | 3600x4500 | 1324.54 | 754.79 | 1273.47 | 864 |
| T4/HA | Galaxy_Center | 300s | 3600x4500 | 1268.54 | 413.01 | 1235.92 | 173 |
| T4/OIII | Galaxy_Center | 600s | 3600x4500 | 1379.68 | 432.94 | 1347.29 | 224 |
| T2/RED | LDN43 | 1200s | 4096x4096 | 2365.90 | 1009.05 | 2313.28 | 2043 |
| T2/GREEN | LDN43 | 1200s | 4096x4096 | 1965.18 | 926.31 | 1920.42 | 1744 |
| T2/BLUE | LDN43 | 1200s | 4096x4096 | 1700.56 | 785.82 | 1663.95 | 1260 |
| T2/HA | LDN43 | 1200s | 4096x4096 | 1100.70 | 338.11 | 1084.59 | 174 |
| T2/OIII | NGC1727 | 1800s | 4096x4096 | 1152.55 | 499.09 | 1120.10 | 318 |
| T3/RED | NGC55 | 600s | 4096x4096 | 1299.34 | 469.39 | 1279.74 | 424 |
| T3/GREEN | NGC55 | 600s | 4096x4096 | 1253.63 | 487.52 | 1232.39 | 476 |
| T3/BLUE | NGC55 | 600s | 4096x4096 | 1191.50 | 449.62 | 1171.41 | 396 |
| T3/HA | NGC55 | 1200s | 4096x4096 | 1074.79 | 271.79 | 1059.44 | 85 |
| T3/OIII | NGC55 | 1200s | 4096x4096 | 1062.33 | 302.63 | 1044.46 | 90 |
| T3/LUM | NGC55 | 600s | 4096x4096 | 2072.09 | 808.64 | 2032.72 | 1495 |

### 关键发现

1. **16 个代表帧全部 PASS**：尺寸一致、NaN=0、Inf=0、极端值=0、统计量有限
2. **T2/T4 缺 Lum Flat 正确跳过**：T2/T4 各 5 个代表帧（无 Lum），T3 6 个代表帧（含 Lum）
3. **T1 无数据正确跳过**：未伪造 T1 校准结果
4. **校准后 mean 全部 > 0**：天体信号 + 背景大于暗噪声扣除，符合物理预期
5. **饱和像素全部 < 0.013%**：最大 T3/LUM 1495/16777216=0.0089%，正常星象
6. **尺寸分布**：T2/T3 均为 4096x4096（FLI 16803），T4 为 4500x3600（FLI 16803 裁切）
7. **曝光分布**：T2 用 600-1800s（深空长曝光），T3 用 600-1200s（中曝光），T4 用 180-600s（短曝光，银心广角）

## 风险/回滚/残留

- **T2/T4 Lum Light 缺 Flat Master**：123 帧 unresolved 已在 P10-005 记录，本任务跳过 Lum 代表帧校准。后续 P12-004 测光矩阵验证前需用户决策（提供 Lum Flat 或批准 flat-skip）。
- **代表帧选择策略**：本任务选每个 (device, filter) 的第一个 resolved 帧，未做"最优代表帧"筛选。对当前 16 个组合足够，但后续若需代表帧多样性可改为按 target/exposure 分布选择。
- **校准后 FITS 文件大小**：16 个 FITS 共约 1 GB（4096x4096 帧 ~64 MB + 4500x3600 帧 ~62 MB），可用作后续 P11/P12 阶段的输入示例。
- **未启用暗场优化**：本任务用标准模式 (Light - Dark) / Flat，未启用 `dark_optimization=True`。后续 P12-004 测光验证时可启用以追求更优 SNR。

## 结论

P10-006 完成。3 个交付物已生成（REPRESENTATIVE_CALIBRATION_REPORT.csv 16 行 + CALIBRATION_VALIDATION_SUMMARY.json + 16 个 calibrated FITS）。16/16 代表帧全部 PASS（通过率 100%）。校准公式与 `lib/calibration/python/calibrator.py` 一致（(Light - Dark) / NormalizedFlat）。尺寸一致性、NaN/Inf/极端值检查、统计量有限性全部通过。25/25 测试 PASS，覆盖 contract/unit/e2e/forbidden shortcut 四个维度。禁止捷径检查通过（T1 无伪造、T2/T4 Lum 未替代、全部 Light/Master 路径真实存在）。

T1-T4 真实校准代表帧验证完成，可作为 P10 Gate 的关键证据：T1-T4 设备档案、滤镜规范、Master 盘点、Light-to-Master 解析、实际校准全链路贯通。
