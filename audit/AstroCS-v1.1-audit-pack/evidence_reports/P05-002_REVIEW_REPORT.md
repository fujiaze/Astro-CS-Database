# P05-002 复核报告

## 复核信息
- **任务 ID**: P05-002
- **任务名称**: Stage1 真实数据端到端验证 (v1.1 开发包)
- **阶段**: P05
- **Gate**: G5
- **复核日期**: 2026-07-25
- **复核人**: AI Sub-agent (self-review)
- **Commit base**: 7c4c1ae P05-001 真实参考数据集登记
- **orchestrator.exe**: `lib\orchestrator\cpp\orchestrator.exe` (2026-07-25 20:03:27 编译)
- **VERDICT**: PASS

## 复核检查项

### 1. 任务范围合规性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 端到端验证任务 | PASS | 仅新增验证证据文件, 未修改业务源码 |
| lib/ 目录无变更 | PASS | lib/ 目录无任何文件改动 (仅 engineering/ 新增) |
| 使用 canonical 数据集 | PASS | 引用 P05-001 的 7 帧 canonical 数据集 (Galaxy_Center/LDN43/NGC1727/NGC247/NGC55/NGC83_cluster/Victory_Nebula) |
| 不以模块单测代替真实端到端 | PASS | 实际运行 orchestrator.exe stage1 全流程 (7 阶段), 非模块单测 |
| 失败帧不阻塞后续帧 | PASS | P05-001-C002 失败后, C003-C007 正常运行 (各帧独立 stage1 调用) |

### 2. stage1 执行完整性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 7 阶段全开 | PASS | config 中 stages=["read_fits","calibrate","platesolve","psf","photometric","snr","drizzle"] |
| 使用 P03-001 真实校准接线 | PASS | calibration_dir 指向 testdata/<T> calibration files, 真实 Master Bias/Dark/Flat |
| 使用 P02 路径 B (callback 导出) | PASS | platesolve_callback_copied=true (6/6 成功帧), sdet_detect_ex 只调用 1 次 |
| 使用 P03-004 SNR 稀疏模型 | PASS | sigma_residual<=0 时降级跳过 snr_model 块 (P03-004 设计) |
| 使用 P04 JSONL 事件输出 | PASS | stage1_stdout.jsonl 含 schema_version=1 的 JSONL 事件 |
| orchestrator.exe 路径正确 | PASS | 使用 lib/orchestrator/cpp/orchestrator.exe (正确推导项目根目录, DLL 加载成功) |

### 3. 数值范围验证 (对比 P05-001)
| 检查项 | P05-001 预期 | P05-002 实测 | 结果 |
|--------|------|--------------|------|
| PlateSolve success | true | 6/6 true (C002 失败除外) | PASS |
| PlateSolve RMS | < 1.0" | 0.1188" ~ 0.3788" | PASS (远低于阈值) |
| PlateSolve n_pairs | > 10 | 31 ~ 47 | PASS (远高于阈值) |
| PSF 有效参数 | 非 NaN | 6/6 有效 (93%-99%) | PASS |
| HISS 文件大小 | > 10KB | 18.5KB ~ 46.6KB | PASS |
| WCS 字段完整 | CRVAL/CRPIX/CD | 6/6 完整 + sip_order=3 | PASS |
| star_det v1 格式 | FLOAT64 [N,6] | 6/6 写入 | PASS |
| 测光 n_matched | [0, 5000] | 0-1 (退化) | PASS (在范围内) |
| SNR has_snr | 0_or_1 | 0 (降级) | PASS (在范围内) |

**说明**:
- 实测项全部 PASS, 符合 P05-001 预期范围
- 测光 n_matched (0-1) 与 SNR has_snr (0) 虽为退化值, 但均在 P05-001 声明的预期范围 [0, 5000] 与 0_or_1 内
- 已知限制 (测光/SNR 退化) 不阻塞 PASS, 因任务规范明确: "VERDICT 必须是 PASS (即使某些帧的测光/SNR 退化，只要 stage1 整体成功就算 PASS)"

### 4. PlateSolve 回归验证 (与 P05-001 基线对比)
| Dataset_ID | P05-001 RMS (") | P05-002 RMS (") | 差值 (") | 一致性 |
|---|---:|---:|---:|---|
| P05-001-C001 | 0.3329 | 0.3460 | +0.013 | PASS (一致) |
| P05-001-C003 | 0.1174 | 0.1240 | +0.007 | PASS (一致) |
| P05-001-C004 | 0.1927 | 0.1927 | 0.000 | PASS (完全一致) |
| P05-001-C005 | 0.1333 | 0.1270 | -0.006 | PASS (一致) |
| P05-001-C006 | 0.1394 | 0.1188 | -0.021 | PASS (一致, 略优) |
| P05-001-C007 | 0.3975 | 0.3788 | -0.019 | PASS (一致, 略优) |

**结论**: 6 帧 PlateSolve RMS 与 P05-001 基线高度一致 (差值绝对值 ≤ 0.021"), 无回归. n_pairs 范围 (31-47) 与 P05-001 基线 (32-45) 一致.

### 5. HISS 文件验证
| 检查项 | 结果 | 说明 |
|--------|------|------|
| HISS 文件存在 | PASS | 6/6 成功帧生成 HISS (C002 失败无 HISS) |
| HISS 大小 > 10KB | PASS | 18.5KB ~ 46.6KB (T2/T3 约 19KB, T4 约 47KB) |
| HISS magic 校验 | PASS | inspect 输出 magic="HISS" (6/6) |
| HISS nside 合理 | PASS | T4=512 (4500×3600), T2/T3=2048 (4096×4096), 符合 1x_to_2x_drizzle 策略 |
| HISS nested=true | PASS | 6/6 nested=true |
| HISS has_snr=false | 已知限制 | 6/6 false (SNR 降级, 见已知限制) |
| HISS SHA-256 计算 | PASS | 6/6 SHA-256 已记录 |
| inspect --hiss 验证 | PASS | 6/6 inspect 输出 result 事件, 元数据完整 |

### 6. 交付物完整性
| 交付物 | 路径 | 状态 |
|--------|------|------|
| TASK_REPORT.md | engineering/evidence/P05-002/TASK_REPORT.md | PASS (v1.1 模板格式) |
| TEST_REPORT.md | engineering/evidence/P05-002/TEST_REPORT.md | PASS (v1.1 模板格式, 含每帧指标) |
| EVIDENCE_INDEX.md | engineering/evidence/P05-002/EVIDENCE_INDEX.md | PASS (v1.1 模板格式) |
| REVIEW_REPORT.md | engineering/evidence/P05-002/REVIEW_REPORT.md | PASS (v1.1 模板格式, 本文件) |
| stage1_e2e_results.json | engineering/evidence/P05-002/stage1_e2e_results.json | PASS (结构化: 7 帧完整指标) |
| 每帧 stage1 日志 | engineering/evidence/P05-002/frames/P05-001-C00X/ | PASS (7 帧完整日志) |
| HISS 文件 | engineering/evidence/P05-002/hiss/*.hiss | PASS (6 个 HISS 文件) |

### 7. 脚本质量
| 检查项 | 结果 | 说明 |
|--------|------|------|
| run_stage1_e2e.ps1 | PASS | 模块化设计, 7 帧定义清晰, Start-Process 可靠重定向 |
| finalize_results.ps1 | PASS | 日志切片提取指标, JSON 修复 (snr_format:unknown 引号修复), inspect 验证 |
| 错误处理 | PASS | 帧文件不存在时 continue, 失败帧不阻塞后续帧 |
| 日志输出 | PASS | 每帧完整 stdout JSONL + stderr 日志 + inspect 输出 |
| 指标提取 | PASS | 正则匹配 11 类指标 (READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE) |

### 8. 失败帧根因分析
| 检查项 | 结果 | 说明 |
|--------|------|------|
| P05-001-C002 失败根因 | PASS (已记录) | missing_master_flat_Lum: T2 校准目录缺 Lum flat 文件 |
| 失败阶段定位 | PASS | CALIBRATE 预检查 (exit_code=3) |
| 失败影响范围 | PASS | 仅该帧, 不影响其他 6 帧 |
| 失败性质判定 | PASS | 测试数据缺口 (非代码缺陷), 需补充 T2 Lum flat 文件 |

### 9. 兼容性与回滚
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 业务源码无变更 | PASS | lib/ 目录零变更 |
| 工程文件新增 | PASS | 仅 engineering/evidence/P05-002/ 新增 |
| 回滚方案 | PASS | 删除新增目录即可回滚, 无副作用 |
| 残留风险 | PASS | 无 (纯验证任务, 不影响运行时行为) |

## 风险评估

### 已知限制 (非缺陷, 不阻塞 PASS)

1. **P05-001-C002 失败 (T2 Lum flat 缺失)**: T2 校准目录缺少 Lum 滤镜的 masterFlat 文件, 导致 LDN43 Lum 帧 stage1 失败. 这是测试数据缺口 (非代码缺陷), 需在后续补充 T2 Lum flat 文件后重跑该帧. 其余 6 帧不受影响. 任务规范明确允许: "如果某帧失败，记录失败原因，不阻塞后续帧".

2. **测光 n_matched 极低 (0-1)**: 6 个成功帧的 photometric_n_matched 均为 0 或 1 (P05-001 预期范围 [0, 5000]). 根因: photometric_sigma_residual=0.0, 测光定标在 sigma_residual<=0 时降级. 这是 v1.1 开发包的已知限制 (测光定标模块在稀疏匹配场景下退化). 任务规范明确: "VERDICT 必须是 PASS (即使某些帧的测光/SNR 退化，只要 stage1 整体成功就算 PASS)".

3. **SNR has_snr=false (降级跳过)**: 6 个成功帧的 has_snr 全部为 false. 根因: SNR 阶段依赖 photometric_sigma_residual, 当 sigma_residual<=0 时 SNR 模块降级跳过 snr_model 块写入 (P03-004 设计: "sigma_residual<=0, 降级跳过 snr_model 块"). 这是测光退化传导的副作用, 非独立缺陷. 修复测光 sigma_residual 后 SNR 将自动恢复.

4. **PlateSolve n_detected=0 (callback 模式)**: 所有成功帧的 platesolve_n_detected 显示为 0, 但 platesolve_callback_copied=true 且 platesolve_star_det_written=true. 这是因为 v1.1 路径 B 使用 callback 导出 star_det (从 platesolve 内部复用), n_detected 字段在日志中未单独计数, 但 star_det 块已正确写入 HISS (inspect 验证通过). 这是日志字段计数问题, 非功能缺陷.

### 残留风险
- **无阻塞性风险**: P05-002 为端到端验证任务, 不修改业务源码, 不影响运行时行为. 所有新增文件均为工程证据文件, 可独立删除回滚.
- **后续待修复**: 测光定标 sigma_residual=0.0 问题需在后续任务中修复, 届时 SNR has_snr 将自动恢复. P05-001-C002 需补充 T2 Lum flat 文件后重跑.

## 数据来源可信度

| 数据来源 | 文件 | 可信度 |
|----------|------|--------|
| P05-001 canonical 数据集 | engineering/evidence/P05-001/canonical_dataset.json | 高 (P05-001 已验证, SHA-256 完整) |
| testdata FITS 文件 | testdata/<target>/lights/<panel>/ | 高 (P05-001 SHA-256 重算验证通过) |
| 校准文件 | testdata/<T> calibration files/ | 高 (P03-001 真实校准接线, T2 Lum flat 缺失已记录) |
| Gaia DR3 星表 | GaiaDR3SP/ | 高 (P02-001 已全量验证) |
| orchestrator.exe | lib/orchestrator/cpp/orchestrator.exe | 高 (2026-07-25 20:03:27 编译, DLL 加载成功) |

## 复核结论

P05-002 任务完成质量良好:

1. **范围合规**: 严格遵循"端到端验证不修改业务源码"约束, lib/ 目录零变更, 使用真实 orchestrator.exe 运行完整 stage1 流程 (非模块单测代替)

2. **执行完整**: 7 帧 canonical 数据集全部运行 stage1, 6 帧成功生成 HISS 并通过 inspect 验证, 1 帧失败已记录根因 (T2 Lum flat 缺失, 测试数据缺口)

3. **数值符合**: 所有成功帧的 PlateSolve/PSF/WCS/star_det 指标符合 P05-001 预期范围; PlateSolve RMS 与 P05-001 基线高度一致 (差值 ≤ 0.021"), 无回归

4. **技术正确**: 使用 P03-001 真实校准接线 + P02 路径 B callback 导出 + P03-004 SNR 稀疏模型 + P04 JSONL 事件输出, 符合任务规范

5. **交付齐全**: 7 项交付物全部完成 (TASK_REPORT/TEST_REPORT/EVIDENCE_INDEX/REVIEW_REPORT/stage1_e2e_results.json/每帧日志/HISS 文件)

6. **已知限制清晰**: 3 项已知限制 (测光退化/SNR 降级/n_detected 计数) 均为非阻塞性问题, 任务规范明确允许测光/SNR 退化时仍判 PASS

7. **兼容性**: 完全兼容, 回滚方案清晰 (删除新增目录即可)

**VERDICT: PASS**
