# 任务报告

- Task/ADR：P10-002 建立 T1-T4 设备档案
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

依据 `tasks/P10-002.md` 和 `docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`，结合 P10-001 的 TESTDATA_EQUIPMENT_CATALOG.csv 与 Header 采样数据，建立 T1-T4 四套唯一规范设备档案（JSON profile）。

## 输入与范围

- 输入：P10-001 交付物（TESTDATA_EQUIPMENT_CATALOG.csv + TESTDATA_DATASET_CATALOG.csv + FILTER_ALIAS_MAP.json + raw_logs/header_samples.json）
- 参考规范：`docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`
- 工具：`engineering_v1.2/evidence/P10-002/scripts/generate_device_profiles.py`
- 依赖：P10-001（已满足）

## 执行/决策

### 阶段 1：加载 P10-001 数据

- 加载 TESTDATA_EQUIPMENT_CATALOG.csv（3 行 T2/T3/T4）
- 加载 TESTDATA_DATASET_CATALOG.csv（49 行数据集清单）
- 加载 raw_logs/header_samples.json（49 FITS + 27 XISF Header 采样）
- 加载 FILTER_ALIAS_MAP.json（6 个规范滤镜名）

### 阶段 2：修复故障基线

修复前脚本因 `csv.DictReader` 返回的 `n_lights` 字段为 `str` 类型，`sum(d["n_lights"] for d in device_datasets)` 触发 `TypeError: unsupported operand type(s) for +: 'int' and 'str'`。

修复方案：
- 在 `dataset_summary` 构造时显式 `int(d.get("n_lights", 0))`
- 在 `total_lights` 求和时使用已构造的 `dataset_summary` 列表（元素已为 int）

同时修复 `collect_device_headers` 的设备匹配正则：
- 旧：仅匹配 `_T{N}_` 和 `_T{N}素材` 和 `key.startswith("NGC83_cluster_T{N}")`，遗漏 `_T{N}/`（Galaxy_Center_T4/panel1/Red）和 `{TN}/` 开头（T2/masterBias_*.xisf）
- 新：`(?:^|_)T{n}(?:[^a-zA-Z0-9]|$)|^{device_id}/`，覆盖 4 种变体

### 阶段 3：构建 4 个 JSON profile

每个 profile 包含：
- `device_id` / `status` / `generated_at`
- 设备参数：`telescope` / `aperture_mm` / `focal_length_mm` / `camera` / `camera_from_header` / `mount` / `pixel_size_um` / `image_size` / `bin` / `gain` / `offset` / `temperature_c`
- 滤镜信息：`filter_set`（实际观测，去重）/ `filter_set_doc`（文档声明）
- 数据集信息：`datasets`（按 target/panel/filter 汇总）/ `total_light_frames`
- 校准信息：`calibration`（bias_files / dark_files / dark_exposures_s / flat_files / flat_filters / missing_flats / total_calib_files）
- 元信息：`doc_source` / `data_conflict_note` / `missing_flat_warning`

T1 无数据场景：
- `status: "no_data"`
- 所有字段留空
- `description` 明确标注 "testdata 中无 T1 设备数据"

### 阶段 4：生成汇总文件

`DEVICE_PROFILE_SUMMARY.json` 包含：
- `total_devices: 4`（T1/T2/T3/T4）
- `active_devices: [T2, T3, T4]`
- `no_data_devices: [T1]`
- `total_light_frames: 710`
- `hard_gate: "PASS"`（无 T5/unknown）

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python generate_device_profiles.py` | 60s | 0 |
| `python validate_device_profiles.py` | 60s | 0 |

## 结果与证据

### 交付物

1. **T1_DEVICE_PROFILE.json** — `status: no_data`，硬门限允许但实际无数据
2. **T2_DEVICE_PROFILE.json** — `status: active`
   - 望远镜：Chilescope T2 (ASA 500N), 焦距 1900mm
   - 口径：500mm，相机：FLI Proline 16803，像元 9.0um
   - 图像尺寸：4096x4096，Bin 1
   - 滤镜集合：Blue/Green/H-alpha/Lum/OIII/Red（6 项）
   - 总 Light 帧数：174（LDN43 + NGC1727 + NGC247）
   - 校准文件：9 个（1 bias + 3 darks + 5 flats）
   - **缺失 Lum Flat**（已在 missing_flats 记录）
3. **T3_DEVICE_PROFILE.json** — `status: active`
   - 望远镜：Chilescope T3 (ASA 500N), 焦距 1900mm
   - 口径：500mm，相机：FLI Proline 16803，像元 9.0um
   - 图像尺寸：4096x4096，Bin 1
   - 滤镜集合：Blue/Green/H-alpha/Lum/Oiii/Red（6 项）
   - 总 Light 帧数：151（NGC55 + NGC83_cluster）
   - 校准文件：9 个（1 bias + 2 darks + 6 flats）
   - 无缺失 Flat（含 Lum）
4. **T4_DEVICE_PROFILE.json** — `status: active`
   - 望远镜：Chilescope T4 (Nikkor 200F2), 焦距 200mm
   - 口径：200mm，相机：FLI Microline 16200，像元 6.0um
   - 图像尺寸：4500x3600，Bin 1
   - 滤镜集合：Blue/Green/H-alpha/Lum/Oiii/Red（6 项）
   - 总 Light 帧数：385（Galaxy_Center 3 panels + Victory_Nebula 2 panels）
   - 校准文件：9 个（1 bias + 3 darks + 5 flats）
   - **缺失 Lum Flat**（已在 missing_flats 记录）
5. **DEVICE_PROFILE_SUMMARY.json** — 4 设备汇总（710 Light 帧，hard_gate PASS）

### 硬门限检查

- 只允许 T1-T4 四套规范设备 ID：**PASS**（实际 T2/T3/T4 active + T1 no_data，无 T5/unknown）
- 所有 Light 必须能归属 T1-T4：**PASS**（710 帧归属明确）

### 关键发现

1. **T1 无数据**：testdata 中无 T1 设备的任何 Light/校准/说明文档，profile 标注 `no_data`，不创建虚假档案
2. **T2/T3 设备参数高度相似**（同为 ASA 500N + FLI Proline 16803 + 4096x4096），但望远镜标识和 Light 目录独立，视为两套独立设备
3. **T4 设备参数独立**（Nikkor 200F2 + FLI Microline 16200 + 4500x3600）
4. **T2/T4 缺 Lum Flat**：T2/T4 校准文件仅有 5 个 Flat（Blue/Green/H-alpha/OIII/Red），无 Lum Flat，但 LDN43_T2、NGC247_T2、Victory_Nebula_T4 有 Lum Light 帧 — P10-005 校准阶段须特殊处理
5. **OIII 别名不一致**：T2 profile 使用 "OIII"，T3/T4 profile 使用 "Oiii" — 与 P10-001 冲突报告一致，P10-004 统一
6. **所有 Light 帧温度 -20.0°C，Bin=1**
7. **T2 dark 覆盖 600/1200/1800s**（覆盖 Light 曝光范围）
8. **T4 dark 覆盖 180/300/600s**（覆盖 Light 曝光范围）

## 风险/回滚/残留

- **T2/T4 缺 Lum Flat**：已在 profile.missing_flats 和 missing_flat_warning 字段标注，P10-005 须特殊处理（可能借用其他设备 Flat 或跳过 Lum 校准）
- **OIII 别名不一致**：profile 暂保留实际观测拼写，P10-004 冻结规范名后统一
- **T1 无数据**：硬门限允许，不影响通过；后续任务无 T1 相关处理
- **口径/像元/Gain/Offset 字段来源**：口径从 telescope 行推导（ASA 500N -> 500mm），像元从 Header XPIXSZ 读取（T2=9.0, T3=9.0, T4=6.0），Gain/Offset 在 Header 中为空（相机厂商未写入，FLI 相机通常不写入 Gain/Offset Header）

## 结论

P10-002 完成。5 个交付物已生成（4 个 profile + 1 个汇总），硬门限 PASS。T1 标注为 no_data，T2/T3/T4 标注为 active 并包含完整设备参数。T2/T4 缺 Lum Flat 已记录，将在 P10-005 处理。OIII 别名不一致将在 P10-004 统一。禁止捷径检查通过（无 T5/unknown）。
