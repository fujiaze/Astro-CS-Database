# 证据索引

- Task/ADR：P10-002 建立 T1-T4 设备档案
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

结合 P10-001 的 TESTDATA_EQUIPMENT_CATALOG.csv 与 Header 采样数据，建立 T1-T4 四套唯一规范设备档案（JSON profile）。

## 输入与范围

- 输入：P10-001 交付物（3 行设备 CSV + 49 行数据集 CSV + 6 个滤镜别名映射 + 76 个 Header 采样）
- 依赖：P10-001（已满足）
- 参考规范：`docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md`

## 执行/决策

1. 加载 P10-001 交付物
2. 修复 `n_lights` 字段 str->int 转换 + `collect_device_headers` 设备匹配正则
3. 构建 4 个 JSON profile（T1=no_data, T2/T3/T4=active）
4. 生成 DEVICE_PROFILE_SUMMARY.json
5. 运行 76 项测试验证

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python generate_device_profiles.py` | 60s | 0 |
| `python validate_device_profiles.py` | 60s | 0 |

## 结果与证据

### 证据目录结构

```
engineering_v1.2/evidence/P10-002/
├── TASK_REPORT.md                          # 任务报告
├── TEST_REPORT.md                          # 测试报告 (76/76 PASS)
├── EVIDENCE_INDEX.md                       # 本文件
├── REVIEW_REPORT.md                        # 独立复核报告
├── T1_DEVICE_PROFILE.json                  # 主交付物: T1 设备档案 (no_data)
├── T2_DEVICE_PROFILE.json                  # 主交付物: T2 设备档案 (active)
├── T3_DEVICE_PROFILE.json                  # 主交付物: T3 设备档案 (active)
├── T4_DEVICE_PROFILE.json                  # 主交付物: T4 设备档案 (active)
├── DEVICE_PROFILE_SUMMARY.json             # 主交付物: 4 设备汇总
├── scripts/
│   ├── generate_device_profiles.py         # 生成脚本
│   └── validate_device_profiles.py         # 验证脚本 (76 测试)
└── raw_logs/
    ├── generate_device_profiles.log        # 生成脚本运行日志
    ├── validate_device_profiles.log        # 验证脚本运行日志
    └── validate_device_profiles_result.json # 测试结果 JSON
```

### 关键统计

| 指标 | 值 |
|------|-----|
| 设备总数 | 4 (T1/T2/T3/T4) |
| 活跃设备 | 3 (T2/T3/T4) |
| 无数据设备 | 1 (T1) |
| 总 Light 帧数 | 710 (T2:174 + T3:151 + T4:385) |
| 滤镜别名映射 | 6 规范名 |
| 校准文件总数 | 27 (T2:9 + T3:9 + T4:9) |
| 缺失 Lum Flat | T2, T4 |
| 测试通过率 | 76/76 (100%) |
| 硬门限 | PASS |

### 交付物 SHA-256

| 文件 | SHA-256 |
|------|---------|
| T1_DEVICE_PROFILE.json | 07B39F838AE9D515501B8A5054CB1322D80CCA78F484B46B9159EC204F5E25DF |
| T2_DEVICE_PROFILE.json | 830BE2C4E353C4DF866808265A258186D0E87CC82DD131466271F25BF001ED59 |
| T3_DEVICE_PROFILE.json | 4E9B330E97D55B3D92104D1AE4D423E55539C549154B26EDAA04AB7114B5EA52 |
| T4_DEVICE_PROFILE.json | B104BAED24E093E04D93C9AA1E21B2EA283F27D032D007E3B11E64AB5AC5886B |
| DEVICE_PROFILE_SUMMARY.json | 5D8F2B9C0D938C9BEAB1CA60D645C4A3E713CEB438A9CCE3CE18F8F5780583CA |

### 设备参数摘要

| 字段 | T2 | T3 | T4 |
|------|----|----|-----|
| 望远镜 | ASA 500N | ASA 500N | Nikkor 200F2 |
| 口径 | 500mm | 500mm | 200mm |
| 焦距 | 1900mm | 1900mm | 200mm |
| 相机 | FLI Proline 16803 | FLI Proline 16803 | FLI Microline 16200 |
| 像元 | 9.0um | 9.0um | 6.0um |
| 图像尺寸 | 4096x4096 | 4096x4096 | 4500x3600 |
| Bin | 1 | 1 | 1 |
| 温度 | -20.0°C | -20.0°C | -20.0°C |
| 滤镜数 | 6 | 6 | 6 |
| Light 帧数 | 174 | 151 | 385 |
| 校准文件 | 9 | 9 | 9 |
| 缺 Lum Flat | 是 | 否 | 是 |

### 失败基线与修复

| 问题 | 现象 | 修复 | 验证 |
|------|------|------|------|
| TypeError (n_lights str+int) | `sum()` 失败 | `int(d["n_lights"])` 显式转换 | T-02 PASS |
| 设备 Header 匹配遗漏 | Galaxy_Center_T4/ 等格式不匹配 | 正则 `(?:^\|_)T{n}(?:[^a-zA-Z0-9]\|$)\|^{device_id}/` | T-10 PASS |

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（P10-005 处理）
- OIII 别名不一致（P10-004 统一）
- T1 无数据（硬门限允许）
- Gain/Offset 字段为空（FLI 相机 Header 未写入）

## 结论

P10-002 完成。5 个交付物已生成（4 个 JSON profile + 1 个汇总），硬门限 PASS。76/76 测试通过。设备档案覆盖 T1（no_data）+ T2/T3/T4（active），总 710 Light 帧。T2/T4 缺 Lum Flat 已记录，将在 P10-005 处理。
