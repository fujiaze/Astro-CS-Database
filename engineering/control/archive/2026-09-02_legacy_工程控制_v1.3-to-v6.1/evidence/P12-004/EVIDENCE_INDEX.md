# P12-004 证据索引

- **任务编号**: P12-004
- **任务名称**: T1-T4 与滤镜类别测光矩阵验证
- **完成日期**: 2026-07-28
- **状态**: DONE (CONDITIONAL — 0/16 Gate PASS，需进入 P12-005 修复)
- **Gate**: G12 (Photometric Diagnostic)

## 1. 证据清单

| 类别 | 文件 | 说明 | 大小 | SHA256 |
|------|------|------|------|--------|
| 任务报告 | `TASK_REPORT.md` | 完整任务报告（目标/完成内容/测试文件/关键指标/失败原因/后续/约束） | - | - |
| 测试报告 | `TEST_REPORT.md` | 16 帧测试详情与关键指标 | - | - |
| 复核报告 | `REVIEW_REPORT.md` | 独立复核报告 (VERDICT: CONDITIONAL_PASS) | - | - |
| 测光矩阵 CSV | `reports/PHOTOMETRY_MATRIX.csv` | 16 帧测光矩阵（含 status/fit_used/scale_factor/sigma_residual/gate_pass/failure_category） | 3420 bytes | `58F2041F6C87E6B8DF29540DE7BFD0FCDD0076C6BC7BA7FC92C778AC006D6EF9` |
| 测光诊断汇总 | `reports/photometric_diag_summary.json` | 16 帧测光诊断汇总（Gate 标准/分类统计/按滤镜类别统计/每帧详细 diag） | 18671 bytes | `8BBB6F2E6B48E48702BD6E693A1FECC28FB52A18057978A5130C2B8818DE4D6C` |
| 失败帧分类 | `reports/failure_classification.json` | 16 帧失败分类（类别/fit_used/scale_factor/sigma_residual/exit_code/notes） | 6726 bytes | `555E44C505FCC06D84B7199C089AEAD843FD7EC08E7B5D6347B0AA2361CC667A` |
| 测试脚本 | `scripts/run_photometric_matrix.py` | 16 帧批量测光校准脚本 | 26166 bytes | `350DE1B3A8B300CEA91465441920621DBB6584B8E30589335969BD2CA5D8A067` |
| 主日志 | `raw_logs/run_photometric_matrix_main.log` | 测试主日志（16 帧运行摘要） | 6362 bytes | `9AAE5EC3181C504093EC8BBC6865C7CD81F698BC0BD2462C9FC5D4FD4F8B3EAE` |

## 2. 16 帧原始日志清单

### 2.1 T4 Galaxy_Center (5 帧)

| 帧 | 日志文件 | 大小 | SHA256 | 附加文件 |
|----|----------|------|--------|----------|
| T4 RED | `raw_logs/T4_RED_Galaxy_Center/stage1.log` | 45665 bytes | `533E4DBEC1BC0E45093AA80F7C801DA98BCA1855671AB9CB8AC125469ACB4C31` | `photometry_report.json` (706 bytes, `0EDFCA7BE57256786C5BF22713C77148FA3CBF84A49A43E71557AACADA58E9FE`) |
| T4 GREEN | `raw_logs/T4_GREEN_Galaxy_Center/stage1.log` | 46493 bytes | `345A2823A48D9959C528710913733B1919C1856675FF51913295858115065F2C` | `photometry_report.json` (710 bytes, `6F968D62D9B42022E791B385F7A431319CB51575C4B5C116F8CAA6ADFFE29CE2`) |
| T4 BLUE | `raw_logs/T4_BLUE_Galaxy_Center/stage1.log` | 47105 bytes | `0E2B513FB7A520DD9C6F9A977631AA70F47158174F58DE8B926E7E31DF300AF8` | `photometry_report.json` (708 bytes, `996C20869FB24F608D401435252B23A94FC028E259897D30F03F2718A87BFA27`) |
| T4 HA | `raw_logs/T4_HA_Galaxy_Center/stage1.log` | 35231 bytes | `C70513FA5DA9DFA08BCBB418EF5FB2BFB91EEBE3F6B399DFE7A59FB7091CBAEC` | - |
| T4 OIII | `raw_logs/T4_OIII_Galaxy_Center/stage1.log` | 35010 bytes | `7CA7086D3B63F214F2B2A44B6A6B49CA32AF375434024882974AF5AC07F77E96` | - |

### 2.2 T2 LDN43/NGC1727 (5 帧)

| 帧 | 日志文件 | 大小 | SHA256 |
|----|----------|------|--------|
| T2 RED (LDN43) | `raw_logs/T2_RED_LDN43/stage1.log` | 4783 bytes | `1B480DD459BC95EAA85CF382B6AB44B055FE13C8A2F5F25721C071C78B88E1F2` |
| T2 GREEN (LDN43) | `raw_logs/T2_GREEN_LDN43/stage1.log` | 4799 bytes | `BDF8898DCB0C264B3CB8E99026BD4D09EB94949BB14DD658EF36C6CA4B560205` |
| T2 BLUE (LDN43) | `raw_logs/T2_BLUE_LDN43/stage1.log` | 4791 bytes | `4119E5AF4880E3D4A82C38726A71FED371D43E6D78016EFE2732F519EF9F7ABE` |
| T2 HA (LDN43) | `raw_logs/T2_HA_LDN43/stage1.log` | 4795 bytes | `E26D676771ABD532357E39ABDF34C77B10E556C2ED5D8B161915BE4CC5821389` |
| T2 OIII (NGC1727) | `raw_logs/T2_OIII_NGC1727/stage1.log` | 12153 bytes | `E26E0DB790437EF903946173D2922FCD1F18E5360C801230C5D5620834754155` |

### 2.3 T3 NGC55 (6 帧)

| 帧 | 日志文件 | 大小 | SHA256 |
|----|----------|------|--------|
| T3 RED | `raw_logs/T3_RED_NGC55/stage1.log` | 12074 bytes | `C0F6A8D3FA4D6586783DE724C5263AEB914A89C733D1D4E35C8880C99CDF70A1` |
| T3 GREEN | `raw_logs/T3_GREEN_NGC55/stage1.log` | 12102 bytes | `F3CEDEA622A986700B99116051A7D2226182E9AB440D77BA6BA9724CBAA319F1` |
| T3 BLUE | `raw_logs/T3_BLUE_NGC55/stage1.log` | 12088 bytes | `DC09002F481FE538F03106A65A738431BC9476548DDFFD8F2EAA4C5E7F58278D` |
| T3 HA | `raw_logs/T3_HA_NGC55/stage1.log` | 12118 bytes | `1F131023BD135DEBE3CFA6768B73CF11D6A36021B077E1B4DDADD6D8C3573967` |
| T3 OIII | `raw_logs/T3_OIII_NGC55/stage1.log` | 12096 bytes | `DB0155296C2E81E34B0D77770DBD640F996F7AD99D3B1AA029383105AE4AAAE7` |
| T3 LUM | `raw_logs/T3_LUM_NGC55/stage1.log` | 12074 bytes | `85CFA3CB9CE7481FE49A07B1A7186EF9A2AF2F3F6885EBFDBB7B9CB4515657A4` |

## 3. 新增文件清单（本次任务生成）

### 3.1 证据文件（4 个）

- `工程控制/evidence/P12-004/TASK_REPORT.md`
- `工程控制/evidence/P12-004/TEST_REPORT.md`
- `工程控制/evidence/P12-004/EVIDENCE_INDEX.md` (本文件)
- `工程控制/evidence/P12-004/REVIEW_REPORT.md`

### 3.2 测试产物（8 个）

- `工程控制/evidence/P12-004/scripts/run_photometric_matrix.py`
- `工程控制/evidence/P12-004/reports/PHOTOMETRY_MATRIX.csv`
- `工程控制/evidence/P12-004/reports/photometric_diag_summary.json`
- `工程控制/evidence/P12-004/reports/failure_classification.json`
- `工程控制/evidence/P12-004/raw_logs/run_photometric_matrix_main.log`
- `工程控制/evidence/P12-004/raw_logs/T4_RED_Galaxy_Center/photometry_report.json`
- `工程控制/evidence/P12-004/raw_logs/T4_GREEN_Galaxy_Center/photometry_report.json`
- `工程控制/evidence/P12-004/raw_logs/T4_BLUE_Galaxy_Center/photometry_report.json`

### 3.3 16 帧原始日志（16 个）

- `工程控制/evidence/P12-004/raw_logs/T4_RED_Galaxy_Center/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T4_GREEN_Galaxy_Center/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T4_BLUE_Galaxy_Center/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T4_HA_Galaxy_Center/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T4_OIII_Galaxy_Center/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T2_RED_LDN43/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T2_GREEN_LDN43/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T2_BLUE_LDN43/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T2_HA_LDN43/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T2_OIII_NGC1727/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T3_RED_NGC55/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T3_GREEN_NGC55/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T3_BLUE_NGC55/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T3_HA_NGC55/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T3_OIII_NGC55/stage1.log`
- `工程控制/evidence/P12-004/raw_logs/T3_LUM_NGC55/stage1.log`

## 4. 关键验证结果

### 4.1 测试结果总览

| 指标 | 实际值 | Gate 标准 | 状态 |
|------|--------|-----------|------|
| 总帧数 | 16 | - | - |
| Gate PASS | 0 | ≥ 1 | FAIL |
| Gate FAIL | 16 | - | - |
| Gate 通过率 | 0.0% | - | FAIL |
| Broadband PASS | 0/10 | - | FAIL |
| Narrowband PASS | 0/6 | - | FAIL |

### 4.2 失败分类统计

| 失败类别 | 帧数 | 帧列表 |
|----------|------|--------|
| INVALID_SCALE | 3 | T4 RED/GREEN/BLUE (scale_factor ≈ 0.0026-0.0028, valid_fsyn=0) |
| STAGE1_ERROR (滤光片曲线) | 2 | T4 HA/OIII (加载滤光片曲线失败) |
| STAGE1_ERROR (中文路径) | 4 | T2 RED/GREEN/BLUE/HA-LDN43 (filesystem error) |
| STAGE1_ERROR (无 Master) | 7 | T2 OIII-NGC1727, T3 全部 6 帧 (无 Master 文件) |

### 4.3 T4 RED/GREEN/BLUE 关键诊断（stage1 成功但 Gate 失败）

| 指标 | T4 RED | T4 GREEN | T4 BLUE | Gate 标准 |
|------|--------|----------|---------|-----------|
| fit_used | 1670 | 1619 | 1231 | ≥ 20 (PASS) |
| scale_factor | 0.002836 | 0.002696 | 0.00261 | ∈ [0.01, 100.0] (FAIL) |
| sigma_residual | 0.181595 | 0.157614 | 0.128533 | > 0 (PASS) |
| valid_fsyn | 0 | 0 | 0 | > 0 (异常) |
| unique_matches | 1673 | 1623 | 1237 | - |
| rejected_ambiguous | 0 | 0 | 0 | - |

## 5. 依赖与后续

- **依赖**: P12-002 (DONE) — KD-tree 方向 bug 修复 + 双向最近邻唯一配对
- **依赖**: P12-003 (DONE) — 光谱积分与响应曲线无回归验证
- **依赖**: P10-006 (DONE) — T1-T4 真实校准代表帧验证（提供 16 帧代表帧清单）
- **后续**: P12-005 (修复 SNR 模型与 HISS 持久化) — 需修复本任务发现的 4 类问题
- **Gate**: G12 进行中 (P12-001/002/003 DONE, P12-004 DONE-CONDITIONAL, P12-005/006 TODO)
