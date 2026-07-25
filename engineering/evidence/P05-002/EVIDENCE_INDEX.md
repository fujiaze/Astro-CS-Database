# P05-002 证据索引

## 任务信息
- **任务 ID**: P05-002
- **任务名称**: Stage1 真实数据端到端验证 (v1.1 开发包)
- **阶段**: P05
- **Gate**: G5
- **执行日期**: 2026-07-25
- **运行时间窗口**: 2026-07-25 21:14:49 ~ 21:18:12 (+08:00)
- **Commit base**: 7c4c1ae P05-001 真实参考数据集登记
- **orchestrator.exe**: `lib\orchestrator\cpp\orchestrator.exe` (2026-07-25 20:03:27 编译)
- **VERDICT**: PASS

## 验证结果摘要
- **总帧数**: 7
- **stage1 成功**: 6/7 (P05-001-C002 失败, T2 Lum flat 缺失)
- **HISS 生成**: 6 个
- **PlateSolve success**: 6/6 (RMS 0.1188" ~ 0.3788", n_pairs 31 ~ 47)
- **PSF 成功率**: 93% ~ 99%
- **HISS 大小**: 18.5 KB ~ 46.6 KB (> 10 KB 阈值)
- **WCS 完整**: 6/6 (CRVAL/CRPIX/CD + sip_order=3)
- **star_det v1 写入**: 6/6

## 证据清单

### 1. Stage1 端到端执行脚本
- **文件**: `engineering/evidence/P05-002/run_stage1_e2e.ps1`
- **描述**: 7 帧 canonical 数据集 stage1 端到端执行脚本. 加载 P05-001 帧定义, 为每帧匹配 T2/T3/T4 config, 调用 orchestrator.exe stage1, 用 Start-Process 可靠重定向 stdout (JSONL) 与 stderr (日志), 计算 HISS SHA-256, 调用 inspect --hiss 验证, 写入每帧 stage1_meta.json
- **SHA-256**: `D351FBACF2AF182E08663DF609451EF0EADB15035FE8148F841498A0AD331C7C`
- **大小**: 19290 字节

### 2. 结果汇总与 HISS 验证脚本
- **文件**: `engineering/evidence/P05-002/finalize_results.ps1`
- **描述**: 对 6 个 HISS 文件运行 inspect --hiss 验证元数据, 从 orchestrator 主日志按时间切片提取每帧完整指标 (READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE), 生成 stage1_e2e_results.json 结构化结果
- **SHA-256**: `D3E83900FF3CDF2BF1F79FC92C78CF57AE240488276D79AD6C977CA4D8CBB49F`
- **大小**: 24059 字节

### 3. 汇总脚本运行日志
- **文件**: `engineering/evidence/P05-002/finalize_results_run.log`
- **描述**: finalize_results.ps1 完整运行日志 (含每帧 inspect 调用结果与指标提取进度)
- **SHA-256**: `4658C910B7A8B2B963D38EC5DBC6E56F2EFAF49C2164B97BFC2663CAE0C5D64B`
- **大小**: 4129 字节

### 4. 结构化端到端结果 (JSON)
- **文件**: `engineering/evidence/P05-002/stage1_e2e_results.json`
- **描述**: 7 帧完整端到端结果 (机器可读). 含 validation_summary, metrics_summary, frames[] (每帧含 dataset_id/telescope/filter/exposure/success/hiss_exists/hiss_sha256/metrics/inspect/validation)
- **SHA-256**: `15D9E5F1FB921B3E71E7CB129887F58D02291CCE29077677774B135241E21219`
- **大小**: 26409 字节
- **关键字段**: task_id, task_name, started_at, completed_at, orchestrator_exe, orchestrator_log, total_frames, success_count, failed_count, hiss_count, validation_summary, metrics_summary, frames[]

### 5. 原始结果数据 (JSON)
- **文件**: `engineering/evidence/P05-002/stage1_e2e_raw.json`
- **描述**: 端到端执行原始结果 (run_stage1_e2e.ps1 输出, 含每帧 stdout/stderr 摘要)
- **SHA-256**: `E78D94E101DCC214BB52B5EACD6AA6B6B52FCE2107EE9B6C6147FB14475CEB33`
- **大小**: 16581 字节

### 6. T2 望远镜 stage1 配置
- **文件**: `engineering/evidence/P05-002/configs/stage1_config_T2.json`
- **描述**: T2 望远镜 (4096×4096) stage1 配置, 用于 LDN43/NGC1727/NGC247 帧. calibration_dir 指向 testdata/T2 calibration files, stages 7 阶段全开, drizzle nside_strategy=1x_to_2x_drizzle
- **SHA-256**: `2ADEC0A9C12EFC758718CD89023DA7A5DB279F26FBC2310CA5E6DED68C4081CF`
- **大小**: 1964 字节

### 7. T3 望远镜 stage1 配置
- **文件**: `engineering/evidence/P05-002/configs/stage1_config_T3.json`
- **描述**: T3 望远镜 (4096×4096) stage1 配置, 用于 NGC55/NGC83_cluster 帧. calibration_dir 指向 testdata/T3 calibration files
- **SHA-256**: `B4EFD6362AC5BC741DCA0227D4C67336B737BF43AAE7D9BA0A0463993C654311`
- **大小**: 1963 字节

### 8. T4 望远镜 stage1 配置
- **文件**: `engineering/evidence/P05-002/configs/stage1_config_T4.json`
- **描述**: T4 望远镜 (4500×3600) stage1 配置, 用于 Galaxy_Center/Victory_Nebula 帧. calibration_dir 指向 testdata/T4 calibration files
- **SHA-256**: `70208E1201E4A2D83AFA98734E42D59A34C43ACBD7F6F26E2BB508D7AB54213D`
- **大小**: 1972 字节

### 9. 任务报告
- **文件**: `engineering/evidence/P05-002/TASK_REPORT.md`
- **描述**: 任务执行报告 (任务信息, 目标, 执行摘要, 7 帧运行结果, 失败帧根因, 实现细节, 代码变更, 兼容性与回滚, 已知限制, 数值范围验证, 结论)
- **SHA-256**: `A98B2AFEA51B7EAE6CE945F76C67351A0C68A7FF5267EC5F78EF6728F94964A2`
- **大小**: 12111 字节
- **VERDICT**: PASS

### 10. 测试报告
- **文件**: `engineering/evidence/P05-002/TEST_REPORT.md`
- **描述**: 详细测试报告 (测试命令, 测试详情, 每帧 stage1 完整指标表, 汇总统计, HISS inspect 验证, PlateSolve RMS 基线对比, 失败与已知限制)
- **SHA-256**: `B700096699FF0178BB8F0F0626ACA331C25D9ECEACC359A145B45ECABFF0D6C8`
- **大小**: 15875 字节
- **结果**: 6/7 PASS, 1/7 FAIL (C002 T2 Lum flat 缺失)

### 11. 复核报告
- **文件**: `engineering/evidence/P05-002/REVIEW_REPORT.md`
- **描述**: 任务复核报告 (复核检查项 9 类, 风险评估, 数据来源可信度, 复核结论)
- **SHA-256**: `968EF510516339F9AA715091901B84873C13EA5D5B6797F354E938817737EE60`
- **大小**: 10399 字节
- **VERDICT**: PASS

## HISS 输出文件 (6 个)

| Dataset_ID | HISS 文件 | 大小 (字节) | nside | n_pix | SHA-256 |
|---|---|---:|---:|---:|---|
| P05-001-C001 | P05-001-C001_Galaxy_Center_T4_Red_180s.hiss | 47706 | 512 | 3928 | C534865F82A750D5B2F7F346EA42DCD37D10126EAC89FF902070ADE96250F064 |
| P05-001-C003 | P05-001-C003_NGC1727_T2_Red_600s.hiss | 19347 | 2048 | 1566 | C9FBB48501D664CE4E6F7EBE528AB7142077C45A791E2926556C9F1CFEB2E438 |
| P05-001-C004 | P05-001-C004_NGC247_T2_Lum_600s.hiss | 19451 | 2048 | 1575 | 417417611D445847B9B7BFF05009C9E23D1A1B3AB3D865F9A261CA80C3F58874 |
| P05-001-C005 | P05-001-C005_NGC55_T3_Red_600s.hiss | 18978 | 2048 | 1536 | DDABBF5D124C11B0CBB8C07DB824AB7F4015E3191F127C06C763B04FF1835FA2 |
| P05-001-C006 | P05-001-C006_NGC83_cluster_T3_Red_600s.hiss | 19287 | 2048 | 1561 | C50C8C55DCD98481F8DCA5274A370778F339F2B1BC0D49FC073125261F204C60 |
| P05-001-C007 | P05-001-C007_Victory_Nebula_T4_Lum_180s.hiss | 47691 | 512 | 3927 | 9D76449DEFC2F82839F0C5274007FA6847AD9D14AE0D75FFA944CD897D20ED7A |

**注**: P05-001-C002 (LDN43 T2 Lum) 失败, 未生成 HISS 文件.

## 每帧证据目录

每帧证据保存于 `engineering/evidence/P05-002/frames/P05-001-C00X/` 下, 含以下文件:

| 文件 | 描述 |
|---|---|
| stage1_stdout.jsonl | orchestrator stage1 stdout (JSONL 事件流, schema_version=1) |
| stage1_stderr.log | orchestrator stage1 stderr (运行日志) |
| stage1_meta.json | 帧级结构化元数据 (成功/失败/指标/HISS hash) |
| inspect_hiss.jsonl | orchestrator inspect --hiss stdout (HISS 元数据验证结果) |
| inspect_hiss_stderr.log | orchestrator inspect --hiss stderr (DLL 加载与验证日志) |
| stage1_full_log.txt | 从主日志切片的该帧完整运行日志 |

### P05-001-C001 (Galaxy_Center T4 Red 180s) - SUCCESS
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 2 | 7EB70257593DA06F682A3DDDA54A9D260D4FC514F645237F5CA74B08F8DA61A6 |
| stage1_meta.json | 3315 | 676EC94C93EEA5B28CA7C0C0F37C2CB40293F7065E803AFA4CC17604477B0790 |
| inspect_hiss.jsonl | 2581 | 0FA0704637004AA754CE19F0E0599704F47F275621F1CF4F62AE78BC9D20E194 |
| inspect_hiss_stderr.log | 3318 | D157DA184A67994BEB70E1886D7ACDB1661A6EB39E0DF5D91BF6D401DFCDE226 |
| stage1_full_log.txt | 12838 | 15C7D85097CF03A256927FE4D8D5ACFA1F0393C254131BF32EB408C8AE87BE72 |

### P05-001-C002 (LDN43 T2 Lum 600s) - FAILED
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 51 | CC0EA4A4AD18AC98FA2F2BED49A034C2CB9F2A78851B76A1380D34977B871ACD |
| stage1_meta.json | 2556 | 409912A9302209ECA3988B18F267BEBD725FA6ECAFB4AEFCC8C4C95E6892CD7E |
| stage1_full_log.txt | 2732 | A9BE448D054D997B9AEE04E96A54115F6D79FEEDA74058E3E18028BDA8EEB788 |

**注**: 该帧失败 (missing_master_flat_Lum), 无 HISS 文件, 无 inspect 输出.

### P05-001-C003 (NGC1727 T2 Red 600s) - SUCCESS
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 545 | C9BA824A64066AD31DDD61F4F142E507E1C6601AFF48D4A7D72157D15771AD3E |
| stage1_meta.json | 3283 | A4EB3B4D17F59D4FE810FBE29155DACF4045B7CF85FFEBC19322772DF84F4CFE |
| inspect_hiss.jsonl | 2533 | 851DD0B7E9F4BBDCF8A8A134B9A89B468CE3A11AE1FEC45784D341BBD7F0F00A |
| inspect_hiss_stderr.log | 3308 | 162EC7563F1244B4B46928BA8E58CC99E73118E43F513FF23822DE26006105BC |
| stage1_full_log.txt | 12810 | D96F77D51E22EA6C9447A681BA27DEDCFB77C8D8C67E34FAB77471D3A05925E1 |

### P05-001-C004 (NGC247 T2 Lum 600s) - SUCCESS
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 424 | 1F02C772345527D4E6A7F166CA366F76CEEB20BBD50FD60B8F19D47F281BB592 |
| stage1_meta.json | 3277 | 936EEEF5E6456EC2A28EEFCD6F2A3AAA9F57824F5256EB1896D110B381043AB2 |
| inspect_hiss.jsonl | 2517 | FD61868B805682053DFFAE5F962AAEC29AEBD6BF8ADC2B83DFF9BE5C16AFCEBA |
| inspect_hiss_stderr.log | 3306 | 5BF65385E7BDD89526F5AACD41D4C2843D922A39364A26F78ACAFB5F7D5E86E6 |
| stage1_full_log.txt | 12806 | B41D52194F3C4B8A721219F3B27E5BCFAFC4FB948C4696AE3968F13A144830C0 |

### P05-001-C005 (NGC55 T3 Red 600s) - SUCCESS
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 480 | 924A2C5E83D992672653178E84278572ABFE7BC19E569700BC94401D99ACFA23 |
| stage1_meta.json | 3247 | E2F83E07B62B3A71B27938C766B68147A9C96633E14F954EC7C64B9825569921 |
| inspect_hiss.jsonl | 2515 | 9FA77D3FB0DD8BE3CF0A2E8D74000F26209A2157C0752141BFD0B580F7B77228 |
| inspect_hiss_stderr.log | 3304 | E5F094DA9A9C560A17A6E05BA4C0C381B9795823EC9B3E7E7AC3B0AC73FDC77A |
| stage1_full_log.txt | 12773 | 463C34D763080A8B11365F4E30FA3F568FF9E2558E1E3BFDA508FF59A0D6A26F |

### P05-001-C006 (NGC83_cluster T3 Red 600s) - SUCCESS
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 314 | A0A1380E7AD38C5915664C7A47D2FADF782133D29C76B75551A238E7F20F358E |
| stage1_meta.json | 3295 | 1F9C8AD5D92CDDE19340B3B162A2CFC2710ADE88571C26319587B13E195C37F1 |
| inspect_hiss.jsonl | 2545 | A817BDF11DAB1F1E1C2C0518D48701105B63B9120765505DAD47C2DA4B79C65B |
| inspect_hiss_stderr.log | 3320 | A28A16EFA1AD3B7950CE7821599E6FF317D863B758095617D0242AA377D6C7F8 |
| stage1_full_log.txt | 12819 | B049BFC64D77398EBCA37596DD1776D68B4A9D3F0AE259A7C321C8AAE13DE3F7 |

### P05-001-C007 (Victory_Nebula T4 Lum 180s) - SUCCESS
| 文件 | 大小 (字节) | SHA-256 |
|---|---:|---|
| stage1_stderr.log | 363 | 090F8E16FA73A8C74FA219E0E70C845E6D1433912D61124B2EBFBBF06253E64F |
| stage1_meta.json | 3340 | 944E44EA7A030F59A10050201AD711F5ECE0D0EBF4588C5454E87B75026B8F72 |
| inspect_hiss.jsonl | 2583 | 057B5925D78DCC43F661A5E7443DFADDC366D8B4807F1FCBCA43F79968C9DDA2 |
| inspect_hiss_stderr.log | 3320 | 6A055A3A86B79354E5193B047868140BE958ED96FF7F1EF7E5A05ABD8C1B7943 |
| stage1_full_log.txt | 12674 | F59627D3A73F0211FF72442FF1DE58B9A1991563CF39096668635410131E30F6 |

## 关键指标汇总

| 指标 | 值 | 阈值 | 结果 |
|------|-----|------|------|
| stage1 成功帧数 | 6/7 | 7 (允许 1 失败) | PASS |
| PlateSolve success rate | 6/6 = 100% (成功帧) | 6/6 | PASS |
| PlateSolve RMS 范围 | 0.1188" ~ 0.3788" | < 1.0" | PASS |
| PlateSolve n_pairs 范围 | 31 ~ 47 | > 10 | PASS |
| PSF 成功率范围 | 93% ~ 99% | 非 NaN | PASS |
| HISS 文件大小范围 | 18.5 KB ~ 46.6 KB | > 10 KB | PASS |
| WCS 完整 | 6/6 (CRVAL/CRPIX/CD+SIP) | 完整 | PASS |
| star_det v1 写入 | 6/6 | 写入 | PASS |
| 测光 n_matched | 0-1 (退化) | [0, 5000] | PASS (在范围内) |
| SNR has_snr | false (6/6 降级) | 0_or_1 | PASS (在范围内) |
| PlateSolve RMS 回归 | 差值 ≤ 0.021" | 无回归 | PASS |

## 业务源码变更
- **无**: 本任务为端到端验证, 不修改任何业务源码 (lib/ 目录零变更)
- **仅新增工程文件**: engineering/evidence/P05-002/ (脚本/配置/日志/HISS/报告)

## 兼容性与回滚
- **兼容性**: 完全兼容, 不影响现有功能
- **回滚**: 删除 `engineering/evidence/P05-002/` 目录即可回滚, 无副作用
- **残留风险**: 无 (纯验证任务, 不影响运行时行为)
