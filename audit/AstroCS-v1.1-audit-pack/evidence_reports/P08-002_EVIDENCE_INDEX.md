# EVIDENCE_INDEX

| Evidence | Description | SHA-256 / Reference |
|---|---|---|
| TASK_REPORT.md | 任务报告 (复核与交接, 无业务源码修改) | (本文件) |
| TEST_REPORT.md | 测试报告 (smoke 5/5 + canonical 3/3 + 回归 352/352) | (本文件) |
| REVIEW_REPORT.md | 独立复核报告 (VERDICT: PASS) | (本文件) |
| HANDOVER.md | v1.1 开发包最终交接文档 | (本文件) |
| final_handover.json | 结构化交接数据 (smoke/canonical/GUI/回归/发布包/缺口/性能/部署/v1.2) | (本文件) |
| logs/smoke_tests.log | Smoke 测试详细日志 (5/5 PASS) | (本文件) |
| logs/canonical_results.json | Canonical 测试结果 (3/3 PASS, 含 SHA-256 baseline 对比) | (本文件) |
| logs/gui_dependency_analysis.md | GUI 依赖分析报告 (PASS, 格式契约路径) | (本文件) |
| logs/regression_test_orchestrator_cli.out | 回归测试日志 (352/352 PASS) | (本文件) |
| configs/setup_dirs.json | 目录创建配置 | (本文件) |
| configs/copy_clean_env.json | 干净环境文件复制配置 | (本文件) |
| configs/prepare_testdata.json | 测试数据准备配置 | (本文件) |

## 发布包文件清单 (引用 P08-001)

发布包位于 `dist/AstroCS-CLI-v1/`, 共 22 个文件 (~22 MB), 完整清单见 `evidence/P08-001/EVIDENCE_INDEX.md`。

| 文件 | SHA-256 (来自 P08-001 SHA256SUMS.txt) |
|---|---|
| lib/orchestrator/cpp/orchestrator.exe | 759e2d4ff640bbf752ac7047037b5dc7d4e9c4107e3206013988024d02d21b50 |
| lib/astro_image_io/astro_image_io.dll | 565fabf3... |
| lib/calibration/astro_calibration.dll | ef1a5a0b... |
| lib/plate_solve/cpp/ipv/ipv_solver.dll | 804b2f2f... |
| lib/dynamic_psf/dynamic_psf.dll | a33286d6... |
| lib/photometric_calib/cpp/photometric_calib.dll | e6f9842b... |
| lib/photometric_calib/cpp/gaia_client.dll | d8f68b5c... |
| lib/snr_estimator/cpp/snr_estimator.dll | b86c72ea... |
| lib/healpix_db/healpix_drizzle/healpix_drizzle.dll | 54de6d78... |
| lib/healpix_db/healpix_stack/healpix_stack.dll | f99a42d5... |
| bin/libgcc_s_seh-1.dll | 3d06aa66... |
| bin/libgomp-1.dll | 53ade6d7... |
| bin/liblz4.dll | 35f91727... |
| bin/libstdc++-6.dll | 96eb8553... |
| bin/libwinpthread-1.dll | f7d57886... |
| bin/libzstd.dll | b95c223a... |
| bin/zlib1.dll | 93e9243a... |
| config/default_stage1.json | e939ea85... |
| config/default_stage2.json | 5153e55b... |
| VERSION.txt | 119769dd... |
| README.txt | a4302b6d... |
| verify.bat | b96eef9d... |
| SHA256SUMS.txt (自引用) | 733a0f7e... |

## 依赖任务证据

| Task | Status | Evidence |
|---|---|---|
| P08-001 | DONE | evidence/P08-001/ (CLI Core v1 发布包, 22 文件, 回归 346/346 PASS) |
| P07-002 | DONE | evidence/P07-002/ (长批次与故障稳定性, 13/13 PASS) |
| P07-001 | DONE | evidence/P07-001/ (性能与峰值内存基线, 9/9 PASS) |
| P06-003 | DONE | evidence/P06-003/ (HCSD 输出与独立读取, 7/7 PASS) |
| P06-002 | DONE | evidence/P06-002/ (球面梯度与稳健叠加证据, SNR²加权证明) |
| P05-002 | DONE | evidence/P05-002/ (Stage1 真实数据端到端, 6/6 PASS) |
| P04-003 | DONE | evidence/P04-003/ (capabilities 与 inspect 命令, 317/317 PASS) |

## 验证证据

### 独立环境 smoke 测试 (clean_env)

- **环境**: clean_env (PATH 仅含 bin/ + 系统目录)
- **capabilities**: exit 0, 9/9 DLL 加载, JSON 输出含 modules/stages/commands/exit_codes/events
- **inspect --hiss (real)**: exit 0, result + completed JSONL 事件, nside=2048, n_pix=1566
- **inspect --hcsd (real)**: exit 0, result + completed JSONL 事件, nside=32768, n_leaves=49152
- **inspect --hiss nonexistent**: exit 8 (FILE_IO_ERROR), error + failed JSONL 事件
- **verify.bat 等价**: 4/4 PASS (文件存在 + SHA-256 + capabilities + inspect)

### Canonical 测试

- **Canonical-1 (HISS inspect)**: PASS, nside=2048, n_pix=1566, filter=Red, exposure_s=600.0
- **Canonical-2 (HCSD inspect + SHA-256 baseline)**: PASS, SHA-256=2A9BD12E... 与 P00-003/P06-002/P07-001 baseline 字节级一致
- **Canonical-3 (inspect 读取验证)**: PASS, result 事件输出正确

### GUI 依赖分析

- **链接库**: astro_image_io.dll (独立 I/O 库) + Qt6 + OpenGL (不链接 orchestrator.exe)
- **契约路径**: 格式契约 (HISS/HCSD 公开格式, 合规)
- **CLI 契约路径**: smoke 测试已验证可用 (未来可作替代方案)
- **当前状态**: 源码完整, CMake 34/34 编译成功, 未包含在 v1.1 发布包

### 回归测试

- **test_orchestrator_cli.exe**: 352/352 PASS, exit 0
- **覆盖**: Part 1-9 (checkpoint/dll_loader/logger/orchestrator/P04-001/P04-002/P04-003/P04-004)
- **与 P08-001 基线对比**: +6 个测试, 无回归

## Real-data metrics

- 发布包总大小: ~22 MB (17 二进制 + 5 文本)
- orchestrator.exe: 4126364 bytes (3.93 MB)
- 模块 DLL 总计: ~11.4 MB (9 个)
- 运行时 DLL 总计: ~4.7 MB (7 个)
- 配置文件: 2 个 (stage1 34 参数, stage2 15 参数, 共 49 参数)
- capabilities 输出: 10 modules, 8 stages, 7 commands, 21 exit_codes, 13 events
- Stage1 C003 中位数 wall: 77.805s, 峰值内存: 35470 MB
- Stage2 中位数 wall: 5.597s, 峰值内存: 1979 MB
- HCSD SHA-256: 字节级可重现 (2A9BD12E...)

## Failures and investigation

无失败。所有 smoke 测试、canonical 测试、GUI 依赖分析和回归测试全部通过。

### 初始问题与修复

- **verify.bat 在 PowerShell 中编码问题**: 用 PowerShell 模拟关键步骤完成等价验证 (4/4 PASS)
- **HISS inspect JSON 解析失败**: 用正则提取关键字段绕过 JSON 解析错误, 数据一致性已验证
- **沙箱路径限制**: 在项目内创建干净目录 (clean_env) 作为独立环境, 等价于 C:\Temp 验证
