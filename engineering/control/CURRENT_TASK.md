# 当前任务：v1.1 开发包交付完成, 进入 v1.2 规划

## v1.1 开发包状态

**状态**: DELIVERED (2026-07-27)
**VERDICT**: PASS (G8 Release Gate PASSED)
**版本**: v1.1.0

### v1.1 完成情况

- **总任务数**: 31 (P00-001 ~ P08-002, 全部 DONE)
- **Gate 通过**: 9/9 (G0-G8 全部 PASSED)
- **发布包**: dist/AstroCS-CLI-v1/ (22 文件, ~22 MB, 自包含)
- **回归测试**: test_orchestrator_cli.exe 352/352 PASS
- **交接文档**: engineering/evidence/P08-002/HANDOVER.md + final_handover.json

### P08-002 最终独立复核结论

- 独立环境 smoke 测试: 5/5 PASS
- Canonical 测试: 3/3 PASS (HCSD SHA-256 字节级一致)
- GUI 依赖分析: PASS (走格式契约路径, 不依赖 orchestrator.exe 内部逻辑)
- 回归测试: 352/352 PASS (比 P08-001 基线 +6 测试无回归)
- **VERDICT: PASS**

## 上一任务完成情况

- P08-002 最终独立复核与交接: DONE (VERDICT: PASS, 2026-07-27)
  - 证据: evidence/P08-002/
  - 交接文档: HANDOVER.md + final_handover.json + 四份标准报告
  - 独立环境验证: clean_env (PATH 仅含 bin/+系统) capabilities + inspect 全部 PASS
  - GUI 依赖分析: healpix_browser_qt 通过 astro_image_io.dll 走格式契约路径, 不依赖 orchestrator.exe
  - 回归测试: 352/352 PASS
  - 控制文件已更新: MASTER_TASK_REGISTER.csv + PROJECT_STATE.yaml + CURRENT_TASK.md

## v1.2 规划范围 (下一阶段)

1. **GUI 发布包**: healpix_browser_qt + Qt6 运行时 + astro_image_io.dll
2. **G-002 修复**: HISS has_snr 持久化, SNR²加权真实生效
3. **GAP-015 完成**: STACK stage 完整实现 (替代骨架)
4. **DLL 版本号补充**: capabilities 输出模块版本号
5. **CLI 契约路径 GUI 原型**: BrowserBackendCli 类, 验证完全解耦可行性

## 已知缺口和风险 (不阻塞 v1.1)

| ID | 描述 | 状态 |
|---|---|---|
| G-002 | HISS has_snr=0 → SNR²加权退化为等权 | P06-002 用合成数据证明数学正确, 待 P03-004 修复后回归 |
| GAP-015 | STACK stage 为骨架 | 工作在 GRADIENT_SPHERE 完成, v1.2+ 完整实现 |
| Gaia-memory | 南天天区 Gaia 内存需求 32-35 GB | 部署需 64 GB RAM |
| HISS-reproducibility | HISS 非字节级可重现 (zstd 时间戳) | 数据一致, HCSD 字节级可重现 |
| DLL-version-unknown | 模块 DLL version 字段为 unknown | 留待 v1.2+ 补充 |
| GaiaDR3SP-not-included | GaiaDR3SP 数据库 (~50GB) 不包含 | 用户需单独获取 |
| testdata-not-included | 测试数据 (~73GB) 不包含 | 用户需单独获取 |

## 部署要求

- **操作系统**: Windows 10/11 (64-bit)
- **RAM**: 4 GB 最低 (Stage1 单帧), 64 GB 推荐 (南天天区)
- **Python**: 不需要 (发布包自包含)
- **PowerShell**: 不需要 (verify.bat 用 cmd.exe)
- **VC++ Runtime**: 不需要 (orchestrator.exe -static 编译)
- **MinGW 运行时**: 已包含在 bin/ (7 个 DLL)
- **GaiaDR3SP 数据库**: 需单独获取 (~50GB)
- **测试数据**: 需单独获取 (~73GB)

## 快速开始

```cmd
:: 1. 切换到发布包目录
cd <path>\AstroCS-CLI-v1

:: 2. 运行验证脚本
verify.bat

:: 3. 设置 PATH
set PATH=%cd%\bin;%PATH%

:: 4. 查询能力
lib\orchestrator\cpp\orchestrator.exe capabilities

:: 5. Stage1 (单帧预处理)
lib\orchestrator\cpp\orchestrator.exe stage1 --frame <fits> --output <hiss> --config config\default_stage1.json

:: 6. Stage2 (多帧合并)
lib\orchestrator\cpp\orchestrator.exe stage2 --frames <dir> --output <hcsd> --config config\default_stage2.json

:: 7. 检查文件
lib\orchestrator\cpp\orchestrator.exe inspect --hiss <file>
lib\orchestrator\cpp\orchestrator.exe inspect --hcsd <file>
```

详见: `dist/AstroCS-CLI-v1/README.txt` 和 `engineering/evidence/P08-002/HANDOVER.md`
