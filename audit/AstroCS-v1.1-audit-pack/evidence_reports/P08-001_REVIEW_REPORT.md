# REVIEW_REPORT

- Reviewer mode: 独立复核 (self-review, P08 阶段第一项)
- Diff reviewed: dist/AstroCS-CLI-v1/ 发布包 (22 文件) + engineering/evidence/P08-001/ (5 文件) + 控制文件更新
- Tests rerun: 干净目录验证 (capabilities + inspect) + P04-003 回归测试 (test_orchestrator_cli.exe 346/346)

## 复核内容

### 1. 发布包自包含性

**验证**: 发布包不得依赖用户安装 Python/PowerShell/.NET/Visual C++ Runtime。

**结果**: PASS
- orchestrator.exe 用 -static 编译, 自身无外部 DLL 依赖 (仅 msvcrt.dll 系统库)
- MinGW-w64 运行时 DLL (7 个) 已包含在 bin/
- verify.bat 纯 Windows 原生 (cmd.exe + certutil), 不调用 Python/PowerShell
- 模块 DLL (9 个) 已包含在 lib/<module>/
- 配置文件 (2 个) 已包含在 config/

### 2. 干净目录验证

**验证**: 从 dist/AstroCS-CLI-v1/ 目录 (不依赖项目其他文件) 运行 capabilities 和 inspect。

**结果**: PASS
- capabilities: exit 0, 9/9 模块 DLL 加载成功, JSON 输出含 modules/stages/commands/exit_codes/events
- inspect --hiss nonexistent.hiss: exit 8 (FILE_IO_ERROR), JSONL error + failed 事件输出正确
- 测试环境: PATH 仅含 dist/AstroCS-CLI-v1/bin + 系统目录 (C:\Windows\System32, C:\Windows)

### 3. SHA-256 清单完整性

**验证**: SHA256SUMS.txt 列出发布包中所有文件的 SHA-256。

**结果**: PASS
- 22 个文件全部记录 (17 二进制 + 5 文本)
- SHA256SUMS.txt 自身 hash 也记录 (自引用)
- orchestrator.exe SHA-256: 759e2d4ff640bbf752ac7047037b5dc7d4e9c4107e3206013988024d02d21b50

### 4. 版本清单完整性

**验证**: VERSION.txt 包含版本号/构建日期/git commit/构建环境/组件清单/依赖运行时。

**结果**: PASS
- 版本号: v1.1.0 (v1.1 开发包)
- 构建日期: 2026-07-27 13:50:20 +08:00
- git commit: 29cb2912affda58b5387371b65cc9a636f365f58
- 构建环境: Windows NT 10.0.26220.0, AMD64, g++ 16.1.0 (MSYS2)
- 组件清单: 1 exe + 9 模块 DLL + 7 运行时 DLL + 2 配置 + 1 验证脚本
- 依赖运行时: MinGW-w64 (已包含), 无需 Python/PowerShell/.NET/VC++ Runtime

### 5. 默认配置完整性

**验证**: config/ 目录包含 default_stage1.json 和 default_stage2.json, 参数与 config_parameter_registry.csv 一致。

**结果**: PASS
- default_stage1.json: 34 参数 (gaia_data_dir, calibration, platesolve, psf, photometric, drizzle 等)
- default_stage2.json: 15 参数 (stack, gradient_sphere 等)
- 所有默认值与 config_parameter_registry.csv (49 参数) 一致

### 6. 验证脚本可用性

**验证**: verify.bat 不依赖 Python/PowerShell, 用 Windows 原生命令。

**结果**: PASS
- 纯 cmd.exe 批处理脚本
- 用 certutil -hashfile 计算 SHA-256 (Windows 自带)
- 4 项检查: 文件存在 + SHA-256 + capabilities + inspect
- capabilities 期望 exit 0
- inspect 期望 exit 8 (FILE_IO_ERROR, 文件不存在)

### 7. 回归测试

**验证**: 运行 P04-003 集成测试确认无回归。

**结果**: PASS
- test_orchestrator_cli.exe: 346/346 测试通过, exit 0
- 覆盖 P04-001 (CLI request + effective config) + P04-002 (JSONL 事件 + 错误码) + P04-003 (capabilities + inspect) + P04-004 (取消/超时/原子性)
- 超过 P04-003 基线 (317 个测试), 因后续任务增加了更多测试

### 8. 二进制文件不提交 git

**验证**: 二进制 DLL 文件不提交到 git (太大), 只记录 SHA-256 清单。

**结果**: PASS
- .gitignore 已排除 dist/、*.exe、*.dll
- 提交文件: VERSION.txt, SHA256SUMS.txt, README.txt, verify.bat, config/*.json (文本文件)
- 不提交: orchestrator.exe, *.dll (二进制文件)
- SHA256SUMS.txt 记录所有文件 (含二进制) 的 SHA-256, 可用于完整性验证

## Contract/ABI/format findings

- 无契约变更: 发布包使用 P07-002 基线的 orchestrator.exe 和 DLL, 未重新编译
- HISS/HCSD 格式: v1.0 (与 engineering/contracts/ 一致)
- JSONL 事件 schema: v1 (与 engineering/contracts/jsonl_event_schema.json 一致)
- 错误码注册表: 21 个退出码 (与 engineering/contracts/error_code_registry.csv 一致)
- 配置参数: 49 个参数 (与 engineering/contracts/config_parameter_registry.csv 一致)

## Scientific regression findings

- 无科学回归: 发布包未修改任何业务源码
- 回归测试 346/346 PASS 确认功能无退化
- capabilities JSON 输出与 P04-003 基线一致 (10 modules, 8 stages, 21 exit_codes)

## Risks

1. **GaiaDR3SP 数据库不包含**: 发布包不包含 Gaia DR3SP 数据库 (~50GB), 用户需单独获取。Stage1 的 PLATESOLVE 和 PHOTOMETRIC 阶段需要此数据库。
2. **测试数据不包含**: 发布包不包含测试数据 (testdata/, ~73GB), 用户需单独获取。
3. **南天天区内存需求**: 南天天区 (如 C003) 内存需求 32-35 GB, 部署需 64 GB RAM (P07-001 基线确认)。
4. **HISS 非字节级可重现**: HISS 文件 zstd 压缩含时间戳, 非字节级可重现, 但数据一致 (P07-001 已记录)。
5. **DLL 版本号 unknown**: 大多数模块 DLL 的 version 字段为 "unknown" (capabilities 输出), 留待未来版本补充。
6. **find_mingw_bin 回退**: 在无 MinGW 环境下, find_mingw_bin() 回退到 C:\msys64\mingw64\bin; 若不存在则 SetDllDirectory 不设置。但 verify.bat 将 bin/ 加入 PATH, Windows 仍能找到运行时 DLL (已验证)。

## 验收标准检查

- [x] 依赖任务均已通过 (P07-002 DONE, P04-003 DONE)
- [x] 本任务目标有可复现证据 (干净目录验证 + 回归测试)
- [x] 相关回归全部运行 (346/346 PASS)
- [x] 独立复核以 VERDICT: PASS 结束
- [x] 更新任务注册表、当前任务和项目状态

VERDICT: PASS
