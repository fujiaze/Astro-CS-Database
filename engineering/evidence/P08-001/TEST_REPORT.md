# TEST_REPORT

## 验证矩阵

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| 文件完整性 | 文件存在性检查 | 10s | 0 | PASS | 22/22 文件存在 |
| SHA-256 完整性 | Get-FileHash SHA256 | 10s | 0 | PASS | SHA256SUMS.txt 22 条目 |
| 干净目录 capabilities | orchestrator.exe capabilities (PATH 仅含 bin/+系统) | 30s | 0 | PASS | 9/9 模块加载, JSON 输出含 modules/stages/exit_codes |
| 干净目录 inspect | orchestrator.exe inspect --hiss nonexistent.hiss | 10s | 8 | PASS | exit 8=FILE_IO_ERROR, JSONL error 事件输出 |
| 回归测试 | test_orchestrator_cli.exe | 120s | 0 | PASS | 346/346 测试通过 |

## 干净目录验证详情

### 环境
- PATH = dist/AstroCS-CLI-v1/bin; C:\Windows\System32; C:\Windows (仅发布包 bin/ + 系统目录)
- 工作目录 = dist/AstroCS-CLI-v1/
- 不依赖项目其他文件

### [1/4] 文件检查
- orchestrator.exe: OK (lib/orchestrator/cpp/orchestrator.exe)
- 运行时 DLL: OK (bin/libgomp-1.dll 等 7 个)
- 配置文件: OK (config/default_stage1.json, default_stage2.json)

### [2/4] SHA-256 验证
- orchestrator.exe SHA-256: 759e2d4ff640bbf752ac7047037b5dc7d4e9c4107e3206013988024d02d21b50
- 与 SHA256SUMS.txt 记录一致

### [3/4] capabilities 命令
- exit code: 0 (SUCCESS)
- DLL 加载: 9/9 模块全部成功
  - AIO (astro_image_io.dll) OK
  - CALIBRATE (astro_calibration.dll) OK
  - PLATESOLVE (ipv_solver.dll) OK
  - PSF (dynamic_psf.dll) OK
  - PHOTOMETRIC (photometric_calib.dll) OK
  - SNR (snr_estimator.dll) OK
  - DRIZZLE (healpix_drizzle.dll) OK
  - GRADIENT_SPHERE + STACK (healpix_stack.dll) OK
- JSON 输出验证:
  - schema_version: 1
  - modules: 10 个 (含 version + capabilities)
  - stages: 8 个 (READ_FITS ... STACK)
  - commands: 7 个 (run, run-batch, stage1, stage2, inspect, capabilities, status)
  - exit_codes: 21 个 (0=SUCCESS ... 100=MODULE_SPECIFIC_BASE)
  - events: 13 种 (accepted ... completed)
  - stdout_format: jsonl
  - stderr_format: human_readable_log

### [4/4] inspect 命令
- 命令: inspect --hiss nonexistent.hiss
- exit code: 8 (FILE_IO_ERROR) - 符合预期
- JSONL 输出:
  - error 事件: {"type":"error", "error":{"code":"ASTROCS_FILE_IO_ERROR","numeric_code":8,"message":"hiss file not found"}}
  - failed 事件: {"type":"failed", "error":{"code":"ASTROCS_FILE_IO_ERROR","numeric_code":8}}
- 验证 inspect 命令能正常解析参数、加载 DLL、输出 JSONL 事件

## 回归测试详情

### 测试套件
- test_orchestrator_cli.exe (lib/orchestrator/cpp/tests/test_orchestrator_cli.exe)
- 运行环境: 项目根目录 (lib/orchestrator/cpp/), PATH 含 C:\msys64\mingw64\bin

### 结果
- 总测试数: 346
- 通过: 346
- 失败: 0
- exit code: 0

### 测试覆盖 (9 个 Part)
- Part 1-4: 基础功能 (checkpoint/dll_loader/logger/orchestrator)
- Part 5: 日志系统集成测试 (16 个子测试)
- Part 6: P04-001 CLI request 与 effective config (46 个子测试)
- Part 7: P04-002 JSONL 事件与稳定错误码 (38 个子测试)
- Part 8: P04-003 capabilities 扩展与 inspect --hiss/--hcsd/--frame (73 个子测试)
- Part 9: P04-004 取消/超时/原子性 (21 个子测试)

### 关键验证项
- capabilities 退出码 0
- inspect --hiss 不存在文件退出码 8 (FILE_IO_ERROR)
- inspect --hiss 无效 magic 退出码 25 (HISS_INVALID)
- inspect --hcsd 无效 magic 退出码 26 (HCSD_INVALID)
- inspect --frame 无效 FITS 退出码 28 (INPUT_INVALID)
- inspect --hiss 真实文件退出码 0 (result + completed 事件)
- inspect --hcsd 真实文件退出码 0 (n_leaves=49152)
- inspect --frame 真实文件退出码 0 (SIMPLE/BITPIX/NAXIS/EXPTIME)
- 超时测试退出码 9 (TIMEOUT)
- 原子性: stage1 失败后部分输出文件已删除
- JSONL 有效性: 所有非空行均为有效 JSONL
- stdout/stderr 严格分离

## Real-data metrics

- 发布包总大小: ~22 MB (17 二进制 + 5 文本)
- orchestrator.exe: 4126364 bytes (3.93 MB)
- 模块 DLL 总计: ~11.4 MB (9 个)
- 运行时 DLL 总计: ~4.7 MB (7 个)
- 配置文件: 2 个 (stage1 34 参数, stage2 15 参数, 共 49 参数)
- capabilities 输出: 10 modules, 8 stages, 7 commands, 21 exit_codes, 13 events

## Failures and investigation

无失败。所有验证和回归测试全部通过。

### 初始问题与修复
- 问题: verify.bat 初版使用 `inspect --help` 验证, 但 inspect 不支持 --help 参数 (返回 exit 7=CONFIG_ERROR)
- 修复: 改用 `inspect --hiss nonexistent.hiss` 验证, 期望 exit 8=FILE_IO_ERROR (验证 inspect 能正常解析参数和输出 JSONL)
- 复验: 修复后 verify.bat 4/4 检查全部 PASS
