# P12-005 — EVIDENCE_INDEX

## 1. 任务元数据

| 字段 | 值 |
| --- | --- |
| 任务 ID | P12-005 |
| 阶段 | P12 |
| Gate | G12 |
| 完成日期 | 2026-07-28 |
| Verdict | PASS |
| Git HEAD (修复后) | 待提交 (上一提交 b854d9a: P12-004) |

## 2. 证据 4 件套

| 文件 | 路径 | 说明 |
| --- | --- | --- |
| TASK_REPORT | `工程控制/evidence/P12-005/TASK_REPORT.md` | 4 类根因修复 + 16/16 PASS 验证 |
| TEST_REPORT | `工程控制/evidence/P12-005/TEST_REPORT.md` | Gate 通过情况 + 关键指标对比 + SNR 持久化验证 |
| EVIDENCE_INDEX | `工程控制/evidence/P12-005/EVIDENCE_INDEX.md` | 本文件 — SHA256 文件清单 |
| REVIEW_REPORT | `工程控制/evidence/P12-005/REVIEW_REPORT.md` | 独立复核 (VERDICT: PASS) |

## 3. 修改源码文件 SHA256

| 文件 | SHA256 | 大小 |
| --- | --- | --- |
| `lib/photometric_calib/cpp/src/star_matcher.cpp` | 5AEB4823B46A2CDFC407B45A3195147D4203AFB4A36C295A977EEACBEE16E7B7 | 26145 |
| `lib/photometric_calib/cpp/src/pc_api.cpp` | FD73BD9D06513D2376BEA338042C777CB547E3CE9C71AAC97EE0B5319CE1F692 | 16308 |
| `lib/photometric_calib/data/response_curves/filters.json` | A1D76D7FC46330311EC31B076F217A2DE902A41F14FD2E45E72A0F6047AB65AD | 197842 |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | 06C00EA4BF420EBB277F1C7ED65BA6892228279FBA0235A274CD112AA44B549F | 186282 |
| `lib/snr_estimator/cpp/src/snr_estimator.cpp` | AC2334444D2877E17F9B59CAA421C22AF8670B7BF0E6A7B6F490FE4BFCE9DE7B | 15593 |

## 4. 测试脚本 SHA256

| 文件 | SHA256 | 大小 |
| --- | --- | --- |
| `工程控制/evidence/P12-004/scripts/run_photometric_matrix.py` | 0528B1A58AAF1FBE1A9966DA941E927B4705553AA237A448BE67246FA7AB7ACD | 27263 |

## 5. 测试结果报告 SHA256

| 文件 | SHA256 | 大小 |
| --- | --- | --- |
| `工程控制/evidence/P12-004/reports/PHOTOMETRY_MATRIX.csv` | E8B1F4FDBB48F9405A041234058575385F3BB20DB196DF8A4ED4D751DED33F33 | 2635 |
| `工程控制/evidence/P12-004/reports/photometric_diag_summary.json` | E6FBA70967901D258C23C9244AD7A7A138CB0F025A024D9DD9AD4A6D1BA6AA87 | 18292 |
| `工程控制/evidence/P12-004/reports/failure_classification.json` | 5918D5890FBB6F0D9685E3414AED7E1DA9FAEFA40164EF5541577A3E78E83292 | 357 |

## 6. 单帧 photometry_report.json SHA256

### 6.1 T4 (5 帧)

| 帧 | SHA256 | 大小 |
| --- | --- | --- |
| T4_RED_Galaxy_Center | 7AF1AAADFD76F327A27566041BB584A76BC17890084093208DA194A86F1C65DC | 714 |
| T4_GREEN_Galaxy_Center | 3360337CFDB91A1395F43D17533B89F28EA5B30B646335DE5531CE4506B24F3C | 718 |
| T4_BLUE_Galaxy_Center | B0165505978365BBD1B325D7126603F1F4131E116891F63C233EE05D861AEF18 | 716 |
| T4_HA_Galaxy_Center | D5741F0720045C86923829C4BC94272F5137F048C10C28766D907DE5B7CDF837 | 721 |
| T4_OIII_Galaxy_Center | EF0E0B5E5FC1410AFA181D78D993885F18C7EE675BDFA556668BBC235522C102 | 719 |

### 6.2 T2 (5 帧)

| 帧 | SHA256 | 大小 |
| --- | --- | --- |
| T2_RED_LDN43 | A7C1763ADBB08922D0EBCD0BA409820121F1E7DE795E331438FB514B693F3F77 | 706 |
| T2_GREEN_LDN43 | 943718937FE1B4F21B324BEBBF73B31628CADCFE417E3672DD13C58D91DBBAAC | 707 |
| T2_BLUE_LDN43 | 4BFDF92DF9A43ACFA97D84A868DF22ADF4FB13DE09A3C0D9F25A8684ACEE2B16 | 707 |
| T2_HA_LDN43 | E9AAE37346A8725BAFDAAFFF03D4653E035664E6D56CB68F434945674ED542E1 | 706 |
| T2_OIII_NGC1727 | 3DAFDDB2C9688E74D0A6626451C31E1B7C60B20145BC3DC41975528041E031C0 | 711 |

### 6.3 T3 (6 帧)

| 帧 | SHA256 | 大小 |
| --- | --- | --- |
| T3_RED_NGC55 | 12A3DE3767AE69C5EA7B1DCA5AEC45C2C985B8B9E40994AC3B7FBFA6A33CBE7B | 689 |
| T3_GREEN_NGC55 | 9DDE79028F1D5D5998F86B5D02563ECC5CA9DA11043D2AADF5E49ECC6D10E47F | 692 |
| T3_BLUE_NGC55 | 381D43900642C99CC2BE46DFBBF22AA5B521EB6FB5779EFF963FF9D5F88F4A03 | 692 |
| T3_HA_NGC55 | 24F2F0BD6FCA33F39E10B2D00AF993132879A759D0B8A999F6E58755C2F776EC | 693 |
| T3_OIII_NGC55 | ED64528B184EEA75E2FC01A69FD1ED92F90B6E6D9FEAD70A89309F733808B905 | 695 |
| T3_LUM_NGC55 | D58047A240C0548B017643B773317FE41C290DA0019E0BB89C96428ABED7A244 | 693 |

## 7. HISS 文件清单

每帧 stage1 输出 HISS 文件，全部包含 `has_snr=1`：

| 设备/帧 | HISS 路径 | nside | n_pix | has_snr | n_points |
| --- | --- | --- | --- | --- | --- |
| T4_RED | `工程控制/evidence/P12-004/raw_logs/T4_RED_Galaxy_Center/T4_RED_Galaxy_Center.hiss` | 512 | 3928 | 1 | 1984 |
| T4_GREEN | `工程控制/evidence/P12-004/raw_logs/T4_GREEN_Galaxy_Center/T4_GREEN_Galaxy_Center.hiss` | 512 | 3931 | 1 | 1966 |
| T4_BLUE | `工程控制/evidence/P12-004/raw_logs/T4_BLUE_Galaxy_Center/T4_BLUE_Galaxy_Center.hiss` | 512 | 3943 | 1 | 1922 |
| T4_HA | `工程控制/evidence/P12-004/raw_logs/T4_HA_Galaxy_Center/T4_HA_Galaxy_Center.hiss` | 512 | 3933 | 1 | 1945 |
| T4_OIII | `工程控制/evidence/P12-004/raw_logs/T4_OIII_Galaxy_Center/T4_OIII_Galaxy_Center.hiss` | 512 | 3941 | 1 | 1849 |
| T2_RED | `工程控制/evidence/P12-004/raw_logs/T2_RED_LDN43/T2_RED_LDN43.hiss` | 2048 | 1573 | 1 | 1930 |
| T2_GREEN | `工程控制/evidence/P12-004/raw_logs/T2_GREEN_LDN43/T2_GREEN_LDN43.hiss` | 2048 | 1573 | 1 | 1953 |
| T2_BLUE | `工程控制/evidence/P12-004/raw_logs/T2_BLUE_LDN43/T2_BLUE_LDN43.hiss` | 2048 | 1573 | 1 | 1908 |
| T2_HA | `工程控制/evidence/P12-004/raw_logs/T2_HA_LDN43/T2_HA_LDN43.hiss` | 2048 | 1572 | 1 | 499 |
| T2_OIII | `工程控制/evidence/P12-004/raw_logs/T2_OIII_NGC1727/T2_OIII_NGC1727.hiss` | 2048 | 1564 | 1 | 1940 |
| T3_RED | `工程控制/evidence/P12-004/raw_logs/T3_RED_NGC55/T3_RED_NGC55.hiss` | 2048 | 1536 | 1 | 557 |
| T3_GREEN | `工程控制/evidence/P12-004/raw_logs/T3_GREEN_NGC55/T3_GREEN_NGC55.hiss` | 2048 | 1535 | 1 | 546 |
| T3_BLUE | `工程控制/evidence/P12-004/raw_logs/T3_BLUE_NGC55/T3_BLUE_NGC55.hiss` | 2048 | 1533 | 1 | 494 |
| T3_HA | `工程控制/evidence/P12-004/raw_logs/T3_HA_NGC55/T3_HA_NGC55.hiss` | 2048 | 1535 | 1 | 234 |
| T3_OIII | `工程控制/evidence/P12-004/raw_logs/T3_OIII_NGC55/T3_OIII_NGC55.hiss` | 2048 | 1535 | 1 | 235 |
| T3_LUM | `工程控制/evidence/P12-004/raw_logs/T3_LUM_NGC55/T3_LUM_NGC55.hiss` | 2048 | 1536 | 1 | 875 |

## 8. stage1.log 清单

每帧 stage1.log 完整保留，记录了:
- 命令行参数
- 退出码 (全部 0)
- 完整 stdout/stderr
- PhotometricDiag KV 块
- SNR 模型构建日志
- HISS 写入日志 (含 has_snr=1, n_points)

路径模式: `工程控制/evidence/P12-004/raw_logs/<frame_name>/stage1.log`

## 9. 哈希计算工具

- 工具: `tools/astro_toolkit.py` (sha256 step)
- 配置: `工程控制/evidence/P12-005/scripts/compute_hashes.json`
- 日志: `工程控制/evidence/P12-005/scripts/compute_hashes.log`
- 哈希算法: SHA-256 (大写)
- 文件大小单位: 字节

## 10. 证据完整性声明

- 所有 SHA256 哈希由 astro_toolkit.py 在 2026-07-28 计算
- 所有 stage1.log 文件由 orchestrator.exe 在执行期间生成，未被人工编辑
- 所有 photometry_report.json 由 orchestrator.exe 在执行期间生成，未被人工编辑
- 所有 HISS 文件由 drizzle_engine 通过 `hiss_write_snr_model` 写入
- PHOTOMETRY_MATRIX.csv 由 run_photometric_matrix.py 自动生成
