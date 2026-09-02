# 测试报告

- Task/ADR：P10-001 读取全部TestData子目录说明文档
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3 (astropy) + PowerShell 7

## 目标/问题

验证 P10-001 的 4 个交付物完整性与准确性，确保：
1. 说明文档递归读取（非仅文件名扫描）
2. FITS/XISF Header 交叉验证
3. 硬门限满足（T1-T4 设备 ID + Light 归属）
4. 冲突报告完整

## 输入与范围

- 测试数据：testdata/ 下全部子目录
- 测试脚本：extract_testdata_catalog.py
- 测试方法：人工核验 + 自动校验

## 执行/决策

### 测试矩阵

| 测试项 | 类型 | 必测项 | 状态 |
|--------|------|--------|------|
| T-01 入口条件与依赖状态 | contract | 入口条件 | PASS |
| T-02 说明文档递归读取 | unit | 修改前事实/失败基线 | PASS |
| T-03 FITS Header 交叉验证 | unit | 对应测试 | PASS |
| T-04 XISF Header 交叉验证 | unit | 对应测试 | PASS |
| T-05 设备档案完整性 | unit | 真实数据测试 | PASS |
| T-06 数据集清单完整性 | unit | 真实数据测试 | PASS |
| T-07 滤镜别名映射准确性 | unit | 对应测试 | PASS |
| T-08 冲突报告完整性 | unit | 对应测试 | PASS |
| T-09 硬门限: T1-T4 设备 ID | contract | 全部 | PASS |
| T-10 硬门限: Light 归属 | contract | 全部 | PASS |
| T-11 禁止捷径: 不得仅扫描文件名 | contract | 全部 | PASS |
| T-12 原始日志完整性 | contract | 原始日志 | PASS |

### 测试详情

**T-01 入口条件与依赖状态**
- 依赖 P09-001 已完成（DONE）
- 当前任务 PROJECT_STATE.yaml = P10-001
- PASS

**T-02 说明文档递归读取**
- 7 个 `素材信息*.txt` 说明文档全部读取
- 每个文档抽取：望远镜/相机/赤道仪/滤镜/曝光时间
- 不是仅扫描文件名：脚本读取文档内容并用正则提取字段
- PASS

**T-03 FITS Header 交叉验证**
- 49 个 Light 分组，每组采样 1 个 .fts 文件读取 Header
- 提取字段：FILTER, EXPTIME, INSTRUME, TELESCOP, XBINNING, CCD-TEMP, DATE-OBS, NAXIS1/2
- 文件名 filter 与 Header FILTER 完全一致（49/49）
- 文件名 exposure 与 Header EXPTIME 完全一致（49/49）
- PASS

**T-04 XISF Header 交叉验证**
- 27 个 .xisf 校准文件全部读取 XML Header
- 修复 PixInsight 变体格式（16 字节头）+ XML namespace 问题
- 提取字段：FILTER, XBINNING, INSTRUME, XPIXSZ, geometry
- Bias/Dark 帧 FILTER 为空（正常）
- Flat 帧 FILTER 与文件名一致（18/18）
- image_size 与文件名一致（27/27）
- PASS

**T-05 设备档案完整性**
- 3 行设备档案（T2/T3/T4）
- 每行包含：device_id, telescope, focal_length_mm, camera, mount, filter_set_doc, image_size, light_dir, master_dir, doc_source
- focal_length 从文档提取（T2=1900, T3=1900, T4=200）
- light_dir 包含所有 Light 目录（分号分隔）
- master_dir 指向校准目录
- PASS

**T-06 数据集清单完整性**
- 49 行数据集清单
- 每行包含：target_name, device_id, panel_id, filter, exposure, n_lights, light_dir, image_size, bin, temp, camera, date_obs, sample_file
- 总 Light 帧数约 711 帧
- 所有分组都有 Header 采样
- PASS

**T-07 滤镜别名映射准确性**
- 6 个规范滤镜名（LUM/RED/GREEN/BLUE/HA/OIII）
- observed_aliases_by_canonical 记录实际观察到的别名
- canonical_to_preferred_alias 推荐使用别名
- OIII 有两个别名：OIII（T2）和 Oiii（T3/T4）
- PASS

**T-08 冲突报告完整性**
- 1 个冲突：calibration_filter_alias_inconsistency（OIII 别名不一致）
- 冲突报告包含：冲突类型、规范名、别名列表、处理建议
- 硬门限检查结果：PASS
- PASS

**T-09 硬门限: T1-T4 设备 ID**
- 实际设备：T2, T3, T4（无 T1，但门限允许）
- 所有设备 ID 在 T1-T4 范围内
- PASS

**T-10 硬门限: Light 归属**
- 0 个未知设备目录
- 所有 Light 帧归属 T2/T3/T4
- PASS

**T-11 禁止捷径: 不得仅扫描文件名**
- 脚本读取说明文档内容（非文件名）
- 脚本读取 FITS/XISF Header（非文件名）
- 文件名仅用于分组和初步 filter/exposure 提取，Header 用于交叉验证
- PASS

**T-12 原始日志完整性**
- raw_logs/extract_testdata_catalog.log：脚本运行日志
- raw_logs/run_extract_final.log：最终运行日志
- raw_logs/header_samples.json：所有 Header 采样（FITS + XISF + doc_records）
- PASS

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_testdata_catalog.py` | 120s | 0 |

## 结果与证据

- 12/12 测试 PASS
- 4 个交付物完整
- 硬门限 PASS
- 1 个冲突已记录

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（P10-005 须处理）
- OIII 别名不一致（P10-004 统一）
- NGC83_cluster 文件名前缀 NGC90（P10-003 核实）

## 结论

P10-001 测试全部通过。12/12 测试 PASS。4 个交付物完整，硬门限满足。
