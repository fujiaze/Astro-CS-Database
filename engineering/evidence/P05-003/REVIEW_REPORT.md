# P05-003 复核报告

## 复核信息
- **任务 ID**: P05-003
- **任务名称**: Stage1 负面与恢复测试 (v1.1 开发包)
- **阶段**: P05
- **Gate**: G5
- **复核日期**: 2026-07-25
- **复核人**: AI Sub-agent (self-review)
- **Commit base**: e7dccd9 P05-002 Stage1 真实数据端到端
- **orchestrator.exe**: `lib\orchestrator\cpp\orchestrator.exe` (2026-07-25 20:03:27 编译, 复用 P05-002 同一构建)
- **VERDICT**: PASS

## 复核检查项

### 1. 任务范围合规性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 负面测试任务 | PASS | 仅新增测试证据文件与 fixture, 未修改业务源码 |
| lib/ 目录无变更 | PASS | lib/ 目录无任何文件改动 (仅 engineering/ 新增) |
| 不修改业务源码 | PASS | 本任务为负面测试, lib/ 目录零变更, 仅新增工程证据文件 |
| 不以模块单测代替真实端到端 | PASS | 实际运行 orchestrator.exe stage1 全流程, 非模块单测 |
| 负面测试不用真实好数据 | PASS | 使用模拟 fixture (black/tiny) 和修改的配置 (size_mismatch/no_calib_dir/timeout/allow_no_calib) |
| 取消和超时测试不影响系统稳定性 | PASS | 取消测试用独立控制台隔离, 超时测试用极短阈值 (0.001s) 触发, 均不影响系统 |
| DLL 缺失测试确保恢复 | PASS | try/finally 确保 gaia_client.dll 恢复, 测试后验证 DLL 存在 |

### 2. 负面场景覆盖完整性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 错误输入场景 (3 个) | PASS | s1_1_1 (frame 不存在), s1_1_2 (config 不存在), s1_1_3 (output 目录不存在) |
| DLL 缺失场景 (1 个) | PASS | s1_2 (gaia_client.dll 缺失, 触发 PHOTOMETRIC 模块加载失败) |
| 校准失败场景 (2 个) | PASS | s1_3_1 (尺寸不匹配), s1_3_2 (校准目录不存在) |
| PlateSolve 失败场景 (2 个) | PASS | s1_4_1 (全黑图像无星点), s1_4_2 (极小图像 10×10) |
| 取消场景 (1 个) | PASS | s1_5 (Ctrl+C via Win32 API GenerateConsoleCtrlEvent) |
| 超时场景 (1 个) | PASS | s1_6 (READ_FITS=0.001s 超时, P04-004 watchdog 触发) |
| 恢复测试 (1 个) | PASS | s2_recovery (所有负面测试后运行 stage1, exit=0, HISS 生成) |

### 3. 退出码验证 (对比 error_code_registry.csv)
| 场景 | 预期退出码 | 实际退出码 | 匹配 | 结果 |
|---|---:|---:|---|---|
| s1_1_1_frame_not_exist | 8 (FILE_IO_ERROR) | 1 (GENERIC_ERROR) | ✗ | PASS (偏离已记录, 仍非零失败) |
| s1_1_2_config_not_exist | 7 (CONFIG_ERROR) | 7 (CONFIG_ERROR) | ✓ | PASS |
| s1_1_3_output_dir_not_exist | 8 (FILE_IO_ERROR) | 6 (DRIZZLE_FAILED) | ✗ | PASS (偏离已记录, 仍非零失败) |
| s1_2_dll_missing | 2 (DLL_LOAD_FAILED) | 2 (DLL_LOAD_FAILED) | ✓ | PASS |
| s1_3_1_size_mismatch | 4 (CALIBRATE_FAILED) | 4 (CALIBRATE_FAILED) | ✓ | PASS |
| s1_3_2_no_calib_dir | 4 (CALIBRATE_FAILED) | 4 (CALIBRATE_FAILED) | ✓ | PASS |
| s1_4_1_black_image | 5 (PLATESOLVE_FAILED) | 5 (PLATESOLVE_FAILED) | ✓ | PASS |
| s1_4_2_tiny_image | 5 (PLATESOLVE_FAILED) | 5 (PLATESOLVE_FAILED) | ✓ | PASS (重跑后) |
| s1_5_cancelled | 10 (CANCELLED) | 10 (CANCELLED) | ✓ | PASS |
| s1_6_timeout | 9 (TIMEOUT) | 9 (TIMEOUT) | ✓ | PASS |
| s2_recovery | 0 (SUCCESS) | 0 (SUCCESS) | ✓ | PASS |

**说明**:
- 11/11 场景退出码行为正确 (失败场景非零, 恢复场景零)
- 8/10 负面场景退出码与预期完全匹配
- 2/10 负面场景退出码偏离 (s1_1_1/s1_1_3), 但均为非零失败, 原子性 OK, 已记录根因与修复建议
- 任务规范明确: "VERDICT 必须是 PASS (所有负面场景都按预期失败就算 PASS)", 偏离不阻塞 PASS

### 4. 原子性验证 (无 HISS 残留)
| 场景 | HISS 残留 | 原子性 | 结果 |
|---|---|---|---|
| s1_1_1_frame_not_exist | 无 | ✓ | PASS |
| s1_1_2_config_not_exist | 无 | ✓ | PASS |
| s1_1_3_output_dir_not_exist | 无 | ✓ | PASS (AtomicOutputGuard 清理) |
| s1_2_dll_missing | 无 | ✓ | PASS |
| s1_3_1_size_mismatch | 无 | ✓ | PASS |
| s1_3_2_no_calib_dir | 无 | ✓ | PASS |
| s1_4_1_black_image | 无 | ✓ | PASS |
| s1_4_2_tiny_image | 无 | ✓ | PASS |
| s1_5_cancelled | 无 | ✓ | PASS |
| s1_6_timeout | 无 | ✓ | PASS |
| s2_recovery | ✓ 生成 | N/A (成功) | PASS |

**结论**: 10/10 负面场景原子性验证通过 (无 HISS 残留), AtomicOutputGuard 在所有失败路径正确清理.

### 5. 恢复测试验证
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 恢复测试 exit=0 | PASS | s2_recovery 退出码 0 (SUCCESS) |
| HISS 文件生成 | PASS | 47706 字节, 路径 engineering/evidence/P05-003/hiss/s2_recovery.hiss |
| HISS 大小匹配 P05-002 C001 | PASS | 47706 字节一致 (与 P05-002 C001 同一帧同一配置) |
| HISS SHA-256 计算 | PASS | 98683E67AF3FEE38B0FABDED2CB99A585DC9446A9BEBE0AA0ACDB34A03EC2ECB |
| inspect --hiss 验证 | PASS | format=HISS, nside=512, n_pix=3928, WCS 完整, sip_order=3 |
| 7 阶段全部成功 | PASS | READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE 全部 success=true |
| 系统状态可恢复 | PASS | 所有负面测试后系统仍能正常运行 stage1 |

### 6. 交付物完整性
| 交付物 | 路径 | 状态 |
|--------|------|------|
| TASK_REPORT.md | engineering/evidence/P05-003/TASK_REPORT.md | PASS (v1.1 模板格式) |
| TEST_REPORT.md | engineering/evidence/P05-003/TEST_REPORT.md | PASS (v1.1 模板格式, 含每个负面场景结果) |
| EVIDENCE_INDEX.md | engineering/evidence/P05-003/EVIDENCE_INDEX.md | PASS (v1.1 模板格式) |
| REVIEW_REPORT.md | engineering/evidence/P05-003/REVIEW_REPORT.md | PASS (v1.1 模板格式, 本文件) |
| negative_test_results.json | engineering/evidence/P05-003/negative_test_results.json | PASS (结构化: 11 场景完整数据) |
| 每场景日志 | engineering/evidence/P05-003/logs/<scenario>/ | PASS (11 场景完整日志) |
| HISS 文件 (恢复测试) | engineering/evidence/P05-003/hiss/s2_recovery.hiss | PASS (47706 字节) |
| Fixture 文件 | engineering/evidence/P05-003/fixtures/ | PASS (black_4096x4096.fits, tiny_10x10.fits) |
| 配置文件 | engineering/evidence/P05-003/configs/ | PASS (4 个测试配置) |

### 7. 脚本质量
| 检查项 | 结果 | 说明 |
|--------|------|------|
| run_negative_tests.ps1 | PASS | 模块化设计, Invoke-Stage1 函数封装, 9 个场景清晰, DLL 重命名 try/finally 安全 |
| run_cancel_test.py | PASS | Python + Win32 API, CREATE_NEW_CONSOLE 隔离, GenerateConsoleCtrlEvent 精准发送 |
| finalize_results.py | PASS | 结果聚合, s1_4_2 退出码修正, comparison 对比表生成 |
| make_fixtures.py | PASS | astropy 生成 black/tiny fixture, 可重复 |
| 错误处理 | PASS | DLL 恢复 try/finally, 超时设置, 帧不存在 continue |
| 日志输出 | PASS | 每场景 stdout/stderr/meta.json 完整, 含 main_run.log 主脚本日志 |

### 8. 已知偏离与限制分析
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 偏离 1 根因分析 | PASS | s1_1_1: orchestrator.cpp 预检查未设 exit_code, 兜底返回 1 (GENERIC_ERROR) |
| 偏离 1 影响 | PASS | 仍非零失败, 原子性 OK, stdout 输出错误信息 |
| 偏离 1 修复建议 | PASS | 在预检查失败路径设置 result.exit_code = FILE_IO_ERROR (后续任务) |
| 偏离 2 根因分析 | PASS | s1_1_3: 跑完整流水线, DRIZZLE 阶段写 HISS 时目录不存在失败 (rc=-2) |
| 偏离 2 影响 | PASS | 仍非零失败, 原子性 OK (AtomicOutputGuard 清理), stdout 输出错误信息 |
| 偏离 2 修复建议 | PASS | 在 stage1 入口预检查 --output 父目录, 不存在则返回 FILE_IO_ERROR (后续任务) |
| 限制 1 (tiny_image 需 allow_no_calib) | PASS | 测试配置调整, 非代码缺陷, 重跑后 exit=5 匹配 |
| 限制 2 (取消测试需 Python) | PASS | PowerShell 无法精准发送 Ctrl+C, 改用 Python + Win32 API, 测试工具限制 |

### 9. 兼容性与回滚
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 业务源码无变更 | PASS | lib/ 目录零变更 |
| 工程文件新增 | PASS | 仅 engineering/evidence/P05-003/ 新增 |
| 回滚方案 | PASS | 删除新增目录即可回滚, 无副作用 |
| DLL 恢复验证 | PASS | gaia_client.dll 已恢复 (try/finally 确保), 测试后 DLL 存在 |
| 残留风险 | PASS | 无 (纯测试任务, 不影响运行时行为; DLL 缺失测试已确保 DLL 恢复) |

## 风险评估

### 已知偏离 (非缺陷, 不阻塞 PASS)

1. **s1_1_1_frame_not_exist 退出码偏离 (exit=1 而非 8)**: orchestrator.cpp run_stage1() 预检查 `if (!fs::exists(fits_path))` 返回时未设置 `result.exit_code`, 兜底返回 1 (GENERIC_ERROR). 这是退出码精细化问题, 非功能缺陷 (失败行为正确, 原子性 OK, stdout 输出错误信息). 任务规范明确: "VERDICT 必须是 PASS (所有负面场景都按预期失败就算 PASS)". 修复建议: 在预检查失败路径设置 `result.exit_code = AstroCsExitCode::FILE_IO_ERROR` (后续任务).

2. **s1_1_3_output_dir_not_exist 退出码偏离 (exit=6 而非 8)**: orchestrator 跑完整流水线, 在 DRIZZLE 阶段写 HISS 时因目录不存在失败 (`hiss_write rc=-2`). 这是预检查缺失, 非功能缺陷 (失败行为正确, 原子性 OK, AtomicOutputGuard 清理了部分输出). 修复建议: 在 stage1 入口预检查 --output 目录的父目录是否存在, 不存在则提前返回 FILE_IO_ERROR (后续任务).

### 已知限制 (非缺陷, 不阻塞 PASS)

1. **s1_4_2_tiny_image 需 allow_no_calibration 配置**: 10×10 极小图像与 T3 master (4096×4096) 尺寸不匹配, 初次测试在 CALIBRATE 阶段失败 (exit=4). 改用 `allow_no_calibration=true` 配置重跑后, 跳过校准直达 platesolve, 正确返回 exit=5. 这是测试配置调整, 非代码缺陷 (极小图像本就不是正常使用场景).

2. **取消测试需 Python + Win32 API**: PowerShell 的 GenerateConsoleCtrlEvent 在子进程无独立控制台时无法精准发送 Ctrl+C. 改用 Python subprocess + CREATE_NEW_CONSOLE (0x10) 启动 orchestrator, 通过 FreeConsole/AttachConsole/GenerateConsoleCtrlEvent 发送 Ctrl+C. 这是测试工具限制, 非被测系统缺陷.

### 残留风险
- **无阻塞性风险**: P05-003 为负面测试任务, 不修改业务源码, 不影响运行时行为. 所有新增文件均为工程证据文件, 可独立删除回滚. DLL 缺失测试已用 try/finally 确保 DLL 恢复, 无残留影响.
- **后续待修复**: 2 个退出码偏离 (s1_1_1/s1_1_3) 已记录根因与修复建议, 可在后续任务中精细化退出码设置.

## 数据来源可信度

| 数据来源 | 文件 | 可信度 |
|----------|------|--------|
| P05-001 canonical 数据集 | engineering/evidence/P05-001/canonical_dataset.json | 高 (P05-001 已验证, SHA-256 完整) |
| P05-002 配置 | engineering/evidence/P05-002/configs/stage1_config_T4.json | 高 (P05-002 已验证, 复用同一配置) |
| P05-002 C001 HISS 基线 | engineering/evidence/P05-002/hiss/P05-001-C001_*.hiss | 高 (47706 字节, 用于恢复测试大小匹配验证) |
| orchestrator.exe | lib/orchestrator/cpp/orchestrator.exe | 高 (2026-07-25 20:03:27 编译, 复用 P05-002 同一构建, DLL 加载成功) |
| 错误码注册表 | engineering/contracts/error_code_registry.csv | 高 (P03-003 定义, 退出码与名称映射完整) |
| testdata FITS 文件 | testdata/Galaxy_Center_T4/lights/panel1/ | 高 (P05-001 SHA-256 重算验证通过) |
| Fixture 文件 | engineering/evidence/P05-003/fixtures/ | 高 (astropy 生成, 可重复) |

## 复核结论

P05-003 任务完成质量良好:

1. **范围合规**: 严格遵循"负面测试不修改业务源码"约束, lib/ 目录零变更, 使用真实 orchestrator.exe 运行完整 stage1 流程. 负面测试使用模拟 fixture 和修改的配置, 不用真实好数据. DLL 缺失测试用 try/finally 确保 DLL 恢复, 取消和超时测试用独立控制台/极短阈值, 不影响系统稳定性.

2. **场景完整**: 10 个负面场景覆盖 6 大类 (错误输入/DLL 缺失/校准失败/PlateSolve 失败/取消/超时), 全部按预期失败 (非零退出码). 1 个恢复测试在所有负面测试后成功运行 stage1, 生成 HISS 文件 (47706 字节, 与 P05-002 C001 大小一致), inspect 验证通过, 证明系统状态完全可恢复.

3. **原子性保证**: 10/10 负面场景原子性验证通过 (无 HISS 残留), AtomicOutputGuard 在所有失败路径正确清理, 包括 s1_1_3 的 DRIZZLE 阶段写 HISS 失败时也正确清理了部分输出.

4. **退出码验证**: 8/10 负面场景退出码与预期完全匹配 (s1_1_2/s1_2/s1_3_1/s1_3_2/s1_4_1/s1_4_2/s1_5/s1_6), 2 个偏离 (s1_1_1/s1_1_3) 已记录根因与修复建议, 均为非零失败, 符合任务规范"所有负面场景都按预期失败就算 PASS".

5. **技术正确**: 取消测试通过 Python + Win32 API GenerateConsoleCtrlEvent 精准发送 Ctrl+C, 验证 P04-004 --cancel-on-signal 机制. 超时测试复用 P04-004 watchdog 自适应轮询机制. DLL 缺失测试验证 dll_loader 必需模块检测.

6. **交付齐全**: 5 项必需交付物全部完成 (TASK_REPORT/TEST_REPORT/EVIDENCE_INDEX/REVIEW_REPORT/negative_test_results.json), 含每场景完整日志 (stdout/stderr/meta.json) 和恢复测试 HISS 文件.

7. **已知偏离清晰**: 2 个退出码偏离 (s1_1_1/s1_1_3) 均为非阻塞性问题, 根因已定位 (预检查未设 exit_code/预检查缺失), 修复建议已记录 (后续任务精细化). 2 个已知限制 (tiny_image 需 allow_no_calib/取消测试需 Python) 均为测试配置/工具限制, 非代码缺陷.

8. **兼容性**: 完全兼容, 回滚方案清晰 (删除新增目录即可), DLL 缺失测试已确保 DLL 恢复, 无残留风险.

**VERDICT: PASS**
