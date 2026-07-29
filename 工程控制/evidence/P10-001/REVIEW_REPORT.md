# 复核报告

- Task/ADR：P10-001 读取全部TestData子目录说明文档
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3 (astropy) + PowerShell 7

## 目标/问题

独立复核 P10-001 是否满足任务定义 `tasks/P10-001.md` 和规范 `docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md` 的全部要求。

## 输入与范围

- 任务定义：tasks/P10-001.md
- 规范：docs/03_TESTDATA_T1_T4_INVENTORY_SPEC.md
- 交付物：TESTDATA_EQUIPMENT_CATALOG.csv, TESTDATA_DATASET_CATALOG.csv, FILTER_ALIAS_MAP.json, DOCUMENT_FACT_CONFLICTS.md
- 脚本：scripts/extract_testdata_catalog.py
- 原始日志：raw_logs/

## 执行/决策

### 复核矩阵

| 复核项 | 验证内容 | 结果 |
|--------|----------|------|
| R-01 任务要求覆盖 | 任务定义的全部要求是否满足 | PASS |
| R-02 交付物完整性 | 4 个交付物是否全部生成 | PASS |
| R-03 说明文档递归读取 | 7 个说明文档是否全部读取 | PASS |
| R-04 FITS Header 交叉验证 | 49 个分组是否都有 Header 采样 | PASS |
| R-05 XISF Header 交叉验证 | 27 个校准文件是否都有 Header 采样 | PASS |
| R-06 硬门限 | T1-T4 设备 ID + Light 归属 | PASS |

### R-01 任务要求覆盖

任务定义要求：
1. ✅ 递归发现说明文件并抽取器材/滤镜/面板/校准事实，记录来源
2. ✅ 输出 DOCUMENT_FACTS.csv（实现为 TESTDATA_EQUIPMENT_CATALOG.csv + TESTDATA_DATASET_CATALOG.csv）
3. ✅ 输出冲突报告（DOCUMENT_FACT_CONFLICTS.md）
4. ✅ 不只扫描文件名（读取说明文档内容 + FITS/XISF Header）

规范要求：
1. ✅ 设备档案至少包含：设备 ID、望远镜、口径、焦距、相机、像元、图像尺寸、Bin、Gain、Offset、温度、滤镜集合、Light 目录、Master 目录、文档来源和冲突说明
   - 注：口径/像元/Gain/Offset 部分字段在说明文档中未提及，Header 中部分字段为空（相机厂商未写入），已在设备档案中留空
2. ✅ 硬门限：只允许 T1-T4 四套规范设备 ID；所有 Light 必须能归属其中之一

### R-02 交付物完整性

| 交付物 | 行数 | 状态 |
|--------|------|------|
| TESTDATA_EQUIPMENT_CATALOG.csv | 3 (T2/T3/T4) | PASS |
| TESTDATA_DATASET_CATALOG.csv | 49 | PASS |
| FILTER_ALIAS_MAP.json | 6 规范名 | PASS |
| DOCUMENT_FACT_CONFLICTS.md | 1 冲突 | PASS |

### R-03 说明文档递归读取

| 目录 | 说明文档 | 设备 ID |
|------|----------|---------|
| Galaxy_Center_T4 | 素材信息.txt | T4 |
| LDN43_T2素材_flying_dutchman | 素材信息与版权约定.txt | T2 |
| NGC1727_T2_flying_dutchman | 素材信息与版权约定.txt | T2 |
| NGC247_T2_flying_dutchman | 素材信息与版权约定.txt | T2 |
| NGC55_T3_flying_dutchman | 素材信息与版权约定.txt | T3 |
| NGC83_cluster_T3_Flying_Dutchman | 素材信息与版权约定.txt | T3 |
| Victory_Nebula_T4_Flying_Dutchman | 素材信息与版权约定.txt | T4 |

7/7 说明文档已读取。PASS。

### R-04 FITS Header 交叉验证

49 个 Light 分组，每组采样 1 个 .fts 文件读取 Header。
- 文件名 filter 与 Header FILTER 一致：49/49
- 文件名 exposure 与 Header EXPTIME 一致：49/49
- 所有 Header 采样记录在 raw_logs/header_samples.json

PASS。

### R-05 XISF Header 交叉验证

27 个 .xisf 校准文件全部读取 XML Header。
- 修复 PixInsight 变体格式（16 字节头：magic + version "0100" + header_len LE + reserved + XML）
- 修复 XML namespace 问题（去除 xmlns 声明）
- Flat 帧 FILTER 与文件名一致：18/18
- image_size 与文件名一致：27/27

PASS。

### R-06 硬门限

- 只允许 T1-T4 四套规范设备 ID：PASS（实际 T2/T3/T4，无 T1，但门限允许）
- 所有 Light 必须能归属 T1-T4：PASS（0 个未知设备目录）

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python extract_testdata_catalog.py` | 120s | 0 |

## 结果与证据

- 6/6 复核项 PASS
- 4 个交付物完整
- 硬门限 PASS
- 1 个冲突已记录（OIII 别名不一致）

## 风险/回滚/残留

- T2/T4 缺 Lum Flat（P10-005 须处理）
- OIII 别名不一致（P10-004 统一）
- NGC83_cluster 文件名前缀 NGC90（P10-003 核实）
- 口径/像元/Gain/Offset 部分字段为空（说明文档未提及，Header 中部分为空）

## 结论

P10-001 独立复核通过。6 项复核全部 PASS。4 个交付物完整，硬门限满足。1 个冲突已记录并将在后续任务解决。说明文档递归读取 + FITS/XISF Header 交叉验证均已完成，未仅扫描文件名。

VERDICT: PASS
