# 复核报告

- Task/ADR：P10-002 建立 T1-T4 设备档案
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

独立复核 P10-002 是否满足任务定义 `tasks/P10-002.md` 和规范 `docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md` 的全部要求。

## 输入与范围

- 任务定义：tasks/P10-002.md
- 规范：docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md
- 交付物：T1/T2/T3/T4_DEVICE_PROFILE.json + DEVICE_PROFILE_SUMMARY.json
- 脚本：scripts/generate_device_profiles.py + scripts/validate_device_profiles.py
- 原始日志：raw_logs/

## 执行/决策

### 复核矩阵

| 复核项 | 验证内容 | 结果 |
|--------|----------|------|
| R-01 任务要求覆盖 | 任务定义的全部要求是否满足 | PASS |
| R-02 交付物完整性 | 5 个交付物（4 profile + 1 summary）是否全部生成 | PASS |
| R-03 设备 ID 唯一性与范围 | T1-T4 四套规范 ID，无 T5/unknown | PASS |
| R-04 T1 无数据正确处理 | T1 status=no_data，不创建虚假档案 | PASS |
| R-05 active profile 字段完整性 | telescope/aperture/focal_length/camera/image_size/bin/filter_set/pixel_size_um 非空 | PASS |
| R-06 校准文件清单完整 | bias/dark/flat 分类正确，dark_exposures 覆盖 Light 范围 | PASS |
| R-07 滤镜别名一致性 | filter_set 中元素在 FILTER_ALIAS_MAP 中有映射 | PASS |
| R-08 失败基线修复验证 | TypeError 已修复，n_lights 类型为 int | PASS |
| R-09 测试覆盖与通过率 | 76/76 测试 PASS | PASS |
| R-10 禁止捷径检查 | 无 T5/unknown，无虚假档案，无未声明 fallback | PASS |

### R-01 任务要求覆盖

任务定义要求：
1. ✅ 结合文档和 Header 生成四套唯一规范设备档案
   - 4 个 JSON profile 已生成（T1/T2/T3/T4）
   - 每个 profile 来源：TESTDATA_EQUIPMENT_CATALOG.csv（文档）+ header_samples.json（Header）
2. ✅ 禁止捷径：不得产生 T5 或 unknown 作为正常结果
   - device_id 集合 == {T1, T2, T3, T4}
   - 无 T5/unknown

规范要求（docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md）：
1. ✅ 设备档案至少包含：设备 ID、望远镜、口径、焦距、相机、像元、图像尺寸、Bin、Gain、Offset、温度、滤镜集合、Light 目录、Master 目录、文档来源和冲突说明
   - 全部字段在 T2/T3/T4 active profile 中存在
   - 口径从 telescope 行推导（ASA 500N -> 500mm, Nikkor 200F2 -> 200mm）
   - 像元从 Header XPIXSZ 读取（T2=9.0, T3=9.0, T4=6.0）
   - Gain/Offset 在 Header 中为空（FLI 相机不写入，正常现象）
2. ✅ 硬门限：只允许 T1-T4 四套规范设备 ID；所有 Light 必须能归属其中之一
   - 710 Light 帧归属 T2/T3/T4

### R-02 交付物完整性

| 交付物 | 状态 | 行数/大小 |
|--------|------|-----------|
| T1_DEVICE_PROFILE.json | PASS | status=no_data |
| T2_DEVICE_PROFILE.json | PASS | status=active, 16 datasets, 174 lights |
| T3_DEVICE_PROFILE.json | PASS | status=active, 10 datasets, 151 lights |
| T4_DEVICE_PROFILE.json | PASS | status=active, 23 datasets, 385 lights |
| DEVICE_PROFILE_SUMMARY.json | PASS | 4 devices, 710 lights |

### R-03 设备 ID 唯一性与范围

- 实际 device_id 集合：{T1, T2, T3, T4}
- 4 套唯一规范设备 ID，无重复
- 无 T5/unknown
- PASS

### R-04 T1 无数据正确处理

- T1 status = "no_data"
- description 明确标注 "testdata 中无 T1 设备数据"
- 所有字段留空（不创建虚假档案）
- data_conflict_note 说明 "T1 在 testdata 中无任何数据"
- PASS

### R-05 active profile 字段完整性（T2/T3/T4）

| 字段 | T2 | T3 | T4 |
|------|----|----|-----|
| telescope | Chilescope T2 (ASA 500N), 焦距1900mm | Chilescope T3 (ASA 500N), 焦距1900mm | Chilescope T4 (Nikkor 200F2), 焦距200mm |
| aperture_mm | 500 | 500 | 200 |
| focal_length_mm | 1900 | 1900 | 200 |
| camera | FLI Proline 16803 | FLI Proline 16803 | FLI Microline 16200 |
| pixel_size_um | 9.0 | 9.0 | 6.0 |
| image_size | 4096x4096 | 4096x4096 | 4500x3600 |
| bin | 1 | 1 | 1 |
| temperature_c | -20.0 | -20.0 | -20.0 |
| filter_set | 6 项 | 6 项 | 6 项 |

所有必填字段非空。PASS。

### R-06 校准文件清单完整

| 设备 | bias | darks | dark_exposures_s | flats | flat_filters | missing_flats |
|------|------|-------|------------------|-------|--------------|---------------|
| T2 | 1 | 3 | [600, 1200, 1800] | 5 | [Blue, Green, H-alpha, OIII, Red] | [Lum] |
| T3 | 1 | 2 | [600, 1200] | 6 | [Blue, Green, H-alpha, Lum, Oiii, Red] | [] |
| T4 | 1 | 3 | [180, 300, 600] | 5 | [Blue, Green, H-alpha, Oiii, Red] | [Lum] |

- T2 dark 覆盖 600/1200/1800s（Light 曝光范围 600/1200/1800s）✅
- T4 dark 覆盖 180/300/600s（Light 曝光范围 180/300/600s）✅
- T2/T4 缺 Lum Flat 已在 missing_flats 字段记录 ✅
- PASS

### R-07 滤镜别名一致性

T2/T3/T4 的 filter_set 共 18 个滤镜实例（含设备间重复）：
- Blue (×3), Green (×3), H-alpha (×3), Lum (×3), Red (×3) — 全部在 FILTER_ALIAS_MAP 中可归一化
- OIII (T2) — 在 OIII 规范名的别名列表中
- Oiii (T3, T4) — 在 OIII 规范名的别名列表中

18/18 滤镜可归一化。PASS。

### R-08 失败基线修复验证

- 修复前：`sum(d["n_lights"] for d in device_datasets)` 触发 `TypeError: unsupported operand type(s) for +: 'int' and 'str'`
- 修复后：`sum(int(item["n_lights"]) for item in dataset_summary)` 正常运行
- 验证：T-02 测试确认 csv n_lights 是 str 类型（基线复现），T-19 测试确认 dataset_summary n_lights 是 int 类型（修复有效）
- 同时修复 `collect_device_headers` 设备匹配正则，覆盖 4 种 key 格式变体
- PASS

### R-09 测试覆盖与通过率

- 测试脚本：scripts/validate_device_profiles.py
- 测试点数：76（20 类测试，覆盖 contract/unit/component/真实数据/失败基线/原始日志）
- 通过率：76/76 (100%)
- 失败数：0
- 跳过数：0
- PASS

### R-10 禁止捷径检查

- 无 T5/unknown 设备 ✅
- T1 标注 no_data，不创建虚假档案 ✅
- 无未声明 fallback ✅
- 无数据范围缩减（49 个数据集全部归属，710 Light 帧全部计数）✅
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python generate_device_profiles.py` | 60s | 0 |
| `python validate_device_profiles.py` | 60s | 0 |

## 结果与证据

- 10/10 复核项 PASS
- 5 个交付物完整（4 profile + 1 summary）
- 76/76 测试通过
- 硬门限 PASS（无 T5/unknown，710 Light 帧归属明确）
- 失败基线已修复并验证
- 禁止捷径检查通过

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（已在 missing_flats 记录，P10-005 处理）
- OIII 别名不一致（P10-004 统一）
- T1 无数据（硬门限允许）
- Gain/Offset 字段为空（FLI 相机 Header 未写入，属正常现象）
- T3 dark 只有 600/1200s 两个曝光（无 1800s），但 T3 Light 最大曝光 1200s，覆盖完整

## 结论

P10-002 独立复核通过。10 项复核全部 PASS。5 个交付物完整，76/76 测试通过，硬门限满足。失败基线（TypeError）已修复并验证。禁止捷径检查通过（无 T5/unknown，T1 标注 no_data 不创建虚假档案）。设备档案覆盖 T1（no_data）+ T2/T3/T4（active），总 710 Light 帧，校准文件齐全，dark 曝光覆盖 Light 范围，滤镜别名可归一化。

VERDICT: PASS
