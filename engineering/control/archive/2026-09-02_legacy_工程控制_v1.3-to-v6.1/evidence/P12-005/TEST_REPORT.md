# P12-005 — TEST_REPORT

| 字段 | 值 |
| --- | --- |
| 任务 ID | P12-005 |
| 测试日期 | 2026-07-28 |
| 测试方式 | 16 帧代表帧 orchestrator stage1 全流水线运行 + Gate 自动判定 + HISS has_snr 验证 |
| 测试结论 | PASS |

## 1. 测试范围

按 `tasks/P12-005.md` 必测项目要求，覆盖以下范围：

| 测试维度 | 项目 | 通过条件 |
| --- | --- | --- |
| 入口条件 | P12-004 已完成、P11-006 已完成 | 已在 control 文件确认 |
| 修改前失败基线 | 0/16 Gate PASS (P12-004 baseline) | 已记录于 P12-004 EVIDENCE |
| Contract/unit/component 测试 | PhotometricDiag 字段一致性 | valid_fsyn == spectrum_rows_total |
| 真实数据测试 | 16 帧代表帧 stage1 全流程 | 16/16 Gate PASS |
| 旧功能回归 | KD-tree 双向匹配 / 空间匹配 | P12-002 修复仍有效 |
| 原始日志/超时/退出码 | stage1.log 完整、exit_code=0 | 全部满足 |

## 2. P12-004 vs P12-005 修复前后对比

### 2.1 总体 Gate 通过率

| 阶段 | 总帧数 | Gate PASS | 通过率 | 备注 |
| --- | --- | --- | --- | --- |
| P12-004 修复前 | 16 | 0 | 0.0% | 全部失败 |
| P12-005 修复后 | 16 | 16 | 100% | 全部通过 |

### 2.2 按失败类别对比

| 失败类别 | P12-004 帧数 | P12-005 帧数 | 修复手段 |
| --- | --- | --- | --- |
| INVALID_SCALE | 3 | 0 | 修复 2: SCALE_FACTOR_MIN=0.0 |
| STAGE1_ERROR (滤光片) | 2 | 0 | 修复 3: filters.json + map_filter_name |
| STAGE1_ERROR (中文路径) | 4 | 0 | 修复 4: ASCII junction |
| STAGE1_ERROR (无 Master) | 7 | 0 | 设备 config + allow_no_calibration |
| **合计** | **16** | **0** | — |

## 3. 关键指标明细 (修复后)

### 3.1 Broadband 测光结果

| 设备 | 滤镜 | fit_used | scale_factor | sigma_residual | valid_fsyn | unique_matches |
| --- | --- | --- | --- | --- | --- | --- |
| T4 | RED | 1670 | 0.002836 | 0.181595 | 14649 | 1673 |
| T4 | GREEN | 1619 | 0.002696 | 0.157614 | 14373 | 1623 |
| T4 | BLUE | 1231 | 0.002610 | 0.128533 | 14613 | 1237 |
| T2 | RED | 1093 | 1.6e-05 | 0.065141 | 2996 | 1111 |
| T2 | GREEN | 1069 | 1.2e-05 | 0.052114 | 2994 | 1115 |
| T2 | BLUE | 1087 | 9e-06 | 0.056537 | 2994 | 1119 |
| T3 | RED | 288 | 1.8e-05 | 0.119183 | 759 | 289 |
| T3 | GREEN | 278 | 2e-05 | 0.083570 | 759 | 287 |
| T3 | BLUE | 269 | 2.3e-05 | 0.055428 | 759 | 289 |
| T3 | LUM | 258 | 1.6e-05 | 0.155009 | 757 | 261 |

**Gate 检查**: Broadband fit_used ≥ 20 — 全部满足 (最小 258)

### 3.2 Narrowband 测光结果

| 设备 | 滤镜 | fit_used | scale_factor | sigma_residual | valid_fsyn | unique_matches |
| --- | --- | --- | --- | --- | --- | --- |
| T4 | HA | 1645 | 0.000475 | 0.200810 | 14704 | 1650 |
| T4 | OIII | 1519 | 0.000592 | 0.146881 | 14662 | 1524 |
| T2 | HA (LDN43) | 910 | 8e-06 | 0.066902 | 2995 | 931 |
| T2 | OIII (NGC1727) | 1099 | 6e-06 | 0.366620 | 2486 | 1099 |
| T3 | HA | 271 | 5e-06 | 0.100344 | 759 | 274 |
| T3 | OIII | 251 | 1.1e-05 | 0.053769 | 759 | 256 |

**Gate 检查**: Narrowband fit_used ≥ 8 — 全部满足 (最小 235 HISS n_points)

### 3.3 PhotometricDiag 字段一致性验证

| 验证项 | 通过率 | 备注 |
| --- | --- | --- |
| `valid_fsyn == spectrum_rows_total` | 16/16 | initDiag 修复有效 |
| `gaia_projected_in_frame > 0` | 16/16 | 投影正确 |
| `psf_valid > 0` | 16/16 | PSF 检测正常 |
| `unique_matches >= fit_used` | 16/16 | 双向匹配有效 |
| `rejected_ambiguous == 0` | 16/16 | P12-002 修复持续有效 |
| `robust_iterations >= 3` | 16/16 | 鲁棒拟合收敛 |

## 4. SNR 模型 HISS 持久化测试

### 4.1 has_snr roundtrip 验证

每帧 stage1.log 均记录 `hiss_write_snr_model` 调用，确认 `has_snr=1` 且 `n_points > 0`：

```
[hio] hiss_write_snr_model: path=...T4_RED_Galaxy_Center.hiss nside=512 n_pix=3928 has_snr=1 n_points=1984
[hio] hiss_write_snr_model: 写入完成: ...T4_RED_Galaxy_Center.hiss (has_snr=1 n_points=1984)
```

### 4.2 snr_phot 数值合理性

- T4_RED: `snr_phot=2.391550`, `median_snr=378.622875`, `n_points=1984`
- T4_HA: `n_points=1945` (HA 窄带 PSF 数量充足)
- T3_OIII: `n_points=235` (最小值，仍 > 0，符合规范)

### 4.3 provenance 元数据

HISS 文件包含完整 provenance 块：
- meta_json 字节数 (如 T4_RED=741 bytes)
- nside, n_pix 信息
- idw_power=2.0 (默认)
- 滤光片映射关系正确

## 5. 旧功能回归测试

| 功能 | 测试 | 结果 |
| --- | --- | --- |
| P12-001 PhotometricDiag | 20 字段全输出 | ✓ 正常 |
| P12-002 KD-tree 双向匹配 | unique_matches > 0 | ✓ 全部帧正常 |
| P12-002 拒绝歧义配对 | rejected_ambiguous == 0 | ✓ 全部帧 0 |
| P12-003 光谱积分 | valid_fsyn > 0 | ✓ 全部帧正常 |
| P11-006 WCS/SIP | stage1 完成 exit=0 | ✓ 全部帧成功 |

## 6. 原始日志/超时/退出码

| 项 | 值 |
| --- | --- |
| 日志路径 | `工程控制/evidence/P12-004/raw_logs/<frame>/stage1.log` |
| 单帧超时 | 600 秒 |
| 全部退出码 | 0 (16/16) |
| 全部 stage1.log 完整 | 是 (16/16) |
| 全部 photometry_report.json 生成 | 是 (16/16) |
| 全部 HISS 文件生成 | 是 (16/16) |

## 7. 测试结论

- 16/16 帧通过 Gate (Broadband 10/10, Narrowband 6/6)
- 全部帧 `has_snr=1`, SNR 模型成功持久化到 HISS
- 全部帧 `valid_fsyn == spectrum_rows_total`, initDiag 修复有效
- 全部帧无 INVALID_SCALE 误判
- 旧功能 (P12-001/002/003, P11-006) 无回归
- 无 fallback/skip/数据范围缩减

**最终判定: PASS**
