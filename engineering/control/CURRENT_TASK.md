# 当前任务：P08-002 最终独立复核与交接

读取 `tasks/P08-002.md` 并执行。独立环境执行 smoke/canonical, 确认 GUI 可只依赖 CLI 契约。

## 上一任务完成情况

- P08-001 CLI Core v1 发布包: DONE (VERDICT: PASS)
  - 证据: evidence/P08-001/
  - 发布包: dist/AstroCS-CLI-v1/ (22 文件, ~22 MB, v1.1.0)
  - 自包含: orchestrator.exe -static 编译, MinGW 运行时 DLL 7 个内含, verify.bat 纯 cmd.exe+certutil
  - 干净目录验证: capabilities exit 0 (9/9 DLL 加载) + inspect --hiss nonexistent.hiss exit 8 (FILE_IO_ERROR)
  - 回归测试: test_orchestrator_cli.exe 346/346 PASS
  - SHA-256 清单: 22 文件完整性 (SHA256SUMS.txt)
  - 默认配置: default_stage1.json (34 参数) + default_stage2.json (15 参数)
  - 残留: GaiaDR3SP/测试数据不包含 (需单独获取); 南天天区内存 32-35 GB; DLL 版本号 unknown

## P08-002 依赖

- P08-001 (DONE, CLI Core v1 发布包)

## 执行步骤

1. 发布包不得依赖用户安装 Python/PowerShell
2. 从干净目录验证 capabilities、smoke、inspect
3. 生成版本与 SHA-256 清单
4. 独立复核以 VERDICT: PASS 结束

完成独立复核后, 更新状态并进入依赖满足的下一任务。
