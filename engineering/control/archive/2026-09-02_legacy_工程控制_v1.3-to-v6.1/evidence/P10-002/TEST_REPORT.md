# 测试报告

- Task/ADR：P10-002 建立 T1-T4 设备档案
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

验证 P10-002 的 4 个 JSON profile 完整性与硬门限，确保：
1. 入口条件与依赖状态满足（P10-001 已完成）
2. 失败基线复现（修复前 TypeError）
3. 4 个 profile 文件存在且 JSON 可解析
4. 设备 ID 集合 == {T1, T2, T3, T4}，无 T5/unknown（禁止捷径）
5. T1 status == "no_data"，T2/T3/T4 status == "active"
6. active profile 必填字段非空（telescope/aperture/focal_length/camera/image_size/bin/filter_set/pixel_size_um）
7. 校准文件齐全且 dark 曝光覆盖 Light 范围
8. 总 Light 帧数 == 710（T2:174 + T3:151 + T4:385）
9. 滤镜别名可归一化到 FILTER_ALIAS_MAP
10. 缺失 Lum Flat 已记录（T2/T4）

## 输入与范围

- 测试数据：P10-002 生成的 4 个 JSON profile + DEVICE_PROFILE_SUMMARY.json
- 测试脚本：`scripts/validate_device_profiles.py`（20 类测试，76 个测试点）
- 测试方法：自动校验 + JSON schema 检查

## 执行/决策

### 测试矩阵

| 测试项 | 类型 | 必测项 | 状态 |
|--------|------|--------|------|
| T-01 入口条件与依赖状态 | contract | 入口条件 | PASS |
| T-02 失败基线复现 (TypeError) | contract | 修改前事实/失败基线 | PASS |
| T-03 4 个 profile 文件存在 | unit | 对应测试 | PASS |
| T-04 JSON 可解析 (4 个) | unit | 对应测试 | PASS (4/4) |
| T-05 device_id 集合 == {T1,T2,T3,T4} | contract | 全部 | PASS |
| T-06 禁止捷径: 无 T5/unknown | contract | 全部 | PASS |
| T-07 T1 status == no_data | contract | 全部 | PASS |
| T-08 T2/T3/T4 status == active | contract | 全部 | PASS (3/3) |
| T-09 active profile 必填字段非空 (21 项) | unit | 真实数据测试 | PASS (21/21) |
| T-10 pixel_size_um 非空 (3 设备) | unit | 真实数据测试 | PASS (3/3) |
| T-11 calibration.total_calib_files >= 1 (3 设备) | unit | 真实数据测试 | PASS (3/3) |
| T-12 total_light_frames >= 1 (3 设备) | unit | 真实数据测试 | PASS (3/3) |
| T-13 总 Light 帧数 == 710 | contract | 真实数据测试 | PASS |
| T-14 T2 filter_set == {LRGB+Ha+OIII} | unit | 真实数据测试 | PASS |
| T-15 T4 filter_set == {LRGB+Ha+Oiii} | unit | 真实数据测试 | PASS |
| T-16 missing_flats 已记录 (T2/T4 缺 Lum) | unit | 对应测试 | PASS (3/3) |
| T-17 滤镜别名可归一化 (18 项) | unit | 对应测试 | PASS (18/18) |
| T-18 dark_exposures 覆盖 (T2/T4) | unit | 真实数据测试 | PASS (2/2) |
| T-19 dataset_summary n_lights 类型为 int | unit | 对应测试 | PASS (3/3) |
| T-20 generated_at 存在 (4 设备) | unit | 对应测试 | PASS (4/4) |

### 测试详情

**T-01 入口条件**
- 依赖 P10-001 已完成（DONE，MASTER_TASK_REGISTER.csv 已标记）
- P10-001 交付物齐全：TESTDATA_EQUIPMENT_CATALOG.csv + TESTDATA_DATASET_CATALOG.csv + FILTER_ALIAS_MAP.json
- PASS

**T-02 失败基线复现**
- csv.DictReader 读取 TESTDATA_DATASET_CATALOG.csv，确认 `n_lights` 字段类型为 `str`
- 修复前脚本 `sum(d["n_lights"] for d in device_datasets)` 会触发 TypeError
- 修复后 `sum(int(item["n_lights"]) for item in dataset_summary)` 正常运行
- PASS（基线确认 + 修复有效）

**T-03/T-04 4 个 profile 文件存在且可解析**
- T1_DEVICE_PROFILE.json: 存在，JSON 可解析
- T2_DEVICE_PROFILE.json: 存在，JSON 可解析
- T3_DEVICE_PROFILE.json: 存在，JSON 可解析
- T4_DEVICE_PROFILE.json: 存在，JSON 可解析
- PASS (4/4)

**T-05/T-06 设备 ID 与禁止捷径**
- device_id 集合 == {T1, T2, T3, T4}
- 无 T5/unknown 设备
- PASS

**T-07/T-08 status 字段**
- T1: status = "no_data"（硬门限允许，实际无数据）
- T2/T3/T4: status = "active"
- PASS (4/4)

**T-09 必填字段非空（21 项）**
- T2: telescope/aperture_mm/focal_length_mm/camera/image_size/bin/filter_set 全部非空（7/7）
- T3: 同上（7/7）
- T4: 同上（7/7）
- PASS (21/21)

**T-10 pixel_size_um**
- T2: 9.0（FLI Proline 16803, 9.0um 像元）
- T3: 9.0（FLI Proline 16803）
- T4: 6.0（FLI Microline 16200, 6.0um 像元）
- PASS (3/3)

**T-11/T-12 校准文件与 Light 帧数**
- T2: 9 calib files, 174 Light frames
- T3: 9 calib files, 151 Light frames
- T4: 9 calib files, 385 Light frames
- PASS (3/3)

**T-13 总 Light 帧数 == 710**
- T2(174) + T3(151) + T4(385) = 710
- PASS

**T-14/T-15 filter_set 完整性**
- T2: {Blue, Green, H-alpha, Lum, OIII, Red}（6 项）
- T4: {Blue, Green, H-alpha, Lum, Oiii, Red}（6 项）
- PASS

**T-16 missing_flats 记录**
- T2: missing_flats = ["Lum"]（实际缺 Lum Flat，已记录）
- T3: missing_flats = []（5 个滤镜 Flat 全覆盖，包括 Lum）
- T4: missing_flats = ["Lum"]（实际缺 Lum Flat，已记录）
- PASS (3/3)

**T-17 滤镜别名可归一化（18 项）**
- T2/T3/T4 的 filter_set 共 18 个滤镜实例（含重复设备间同名的）
- 每个滤镜都能在 FILTER_ALIAS_MAP 的 `_canonical_to_aliases` 中找到映射
- PASS (18/18)

**T-18 dark_exposures 覆盖**
- T2: dark_exposures_s = [600.0, 1200.0, 1800.0]（覆盖 Light 曝光范围 600/1200/1800s）
- T4: dark_exposures_s = [180.0, 300.0, 600.0]（覆盖 Light 曝光范围 180/300/600s）
- PASS (2/2)

**T-19 n_lights 类型**
- T2/T3/T4 的 datasets 列表中 n_lights 字段类型为 `int`（修复后）
- PASS (3/3)

**T-20 generated_at 时间戳**
- T1/T2/T3/T4 四个 profile 都有 generated_at 字段（ISO 8601 UTC）
- PASS (4/4)

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python generate_device_profiles.py` | 60s | 0 |
| `python validate_device_profiles.py` | 60s | 0 |

## 结果与证据

- **76/76 测试 PASS**（详见 raw_logs/validate_device_profiles.log）
- 5 个交付物完整（4 profile + 1 summary）
- 硬门限 PASS（无 T5/unknown）
- 失败基线确认（TypeError 已修复）
- 真实数据验证（710 Light 帧，18 个滤镜别名可归一化）
- 旧功能回归（P10-001 交付物可读，header_samples.json 可加载）

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（已在 missing_flats 记录，P10-005 处理）
- OIII 别名不一致（P10-004 统一）
- Gain/Offset 字段为空（FLI 相机 Header 未写入，属正常现象）
- T1 无数据（硬门限允许）

## 结论

P10-002 测试全部通过。76/76 测试 PASS。4 个 JSON profile + 1 个 summary 完整生成，硬门限满足。失败基线（TypeError）已修复并验证。禁止捷径检查通过（无 T5/unknown）。所有 active profile 的必填字段非空，校准文件齐全，dark 曝光覆盖 Light 范围，滤镜别名可归一化。
