# REVIEW_REPORT

- Reviewer mode: 独立复核 (self-review, P08 阶段最终复核, v1.1 开发包交付门限)
- Diff reviewed: engineering/evidence/P08-002/ (新增证据文档 + 日志 + 配置 + 干净环境副本), 控制文件更新 (MASTER_TASK_REGISTER.csv + PROJECT_STATE.yaml + CURRENT_TASK.md)
- Tests rerun: 独立环境 smoke 测试 (5/5) + canonical 测试 (3/3) + GUI 依赖分析 + 回归测试 (352/352)

## 复核内容

### 1. 独立环境 smoke 测试

**验证**: 从干净目录 (clean_env, PATH 仅含发布包 bin/ + 系统目录) 验证 capabilities/smoke/inspect 命令。

**结果**: PASS (5/5)
- capabilities: exit 0, 9/9 模块 DLL 加载, JSON 输出含 modules/stages/commands/exit_codes/events
- inspect --hiss (真实 HISS): exit 0, result + completed JSONL 事件, nside=2048, n_pix=1566
- inspect --hcsd (真实 HCSD): exit 0, result + completed JSONL 事件, nside=32768, n_leaves=49152
- inspect --hiss (不存在): exit 8 (FILE_IO_ERROR), error + failed JSONL 事件
- verify.bat 等价验证: 4/4 PASS (文件存在 + SHA-256 + capabilities + inspect)

**关键发现**: 发布包在完全独立的环境中功能正常, 不依赖 Python/PowerShell/MSYS2/VC++ Runtime。

### 2. Canonical 测试

**验证**: 用 P05-002 真实 HISS 和 P07-001 baseline HCSD 验证端到端功能。

**结果**: PASS (3/3)
- Canonical-1 (HISS inspect): nside=2048, n_pix=1566, filter=Red, exposure_s=600.0, 与 P05-002 记录一致
- Canonical-2 (HCSD inspect + SHA-256 baseline): SHA-256=2A9BD12E... 与 P00-003/P06-002/P06-003/P07-001 baseline **字节级一致** (确定性保证)
- Canonical-3 (inspect 读取验证): inspect 命令能正确读取真实 HISS/HCSD 文件并输出 result+completed JSONL 事件

**关键发现**: HCSD 字节级可重现, 跨任务 (P00-003 → P06-002 → P06-003 → P07-001 → P08-002) SHA-256 一致, 确定性保证成立。

### 3. GUI 依赖分析

**验证**: GUI (healpix_browser_qt) 可只依赖 CLI 契约/格式契约, 不依赖 orchestrator.exe 内部逻辑。

**结果**: PASS
- **链接库分析**: GUI 链接 astro_image_io.dll (独立 I/O 库) + Qt6 + OpenGL, **不链接** orchestrator.exe 或其内部库
- **数据读取路径**: GUI 通过 astro_image_io.dll 的 aio_hiss_read/aio_hcsd_read/aio_hcsd_read_leaf 直接读 HISS/HCSD 公开格式 (格式契约路径)
- **CLI 契约路径**: smoke 测试已验证可用 (orchestrator inspect 输出 JSONL), 未来可作为替代方案
- **当前状态**: 源码完整, CMake 34/34 编译成功, 未包含在 v1.1 发布包 (v1.2+ 计划)

**关键发现**: GUI 走格式契约路径 (合法路径), 不依赖 orchestrator.exe 内部逻辑, 满足"GUI 可只依赖 CLI 契约"的要求。

### 4. 回归测试

**验证**: 运行 test_orchestrator_cli.exe 集成测试确认无回归。

**结果**: PASS (352/352)
- 总测试数: 352, 通过: 352, 失败: 0, exit code: 0
- 覆盖 Part 1-9: checkpoint/dll_loader/logger/orchestrator/P04-001/P04-002/P04-003/P04-004
- 与 P08-001 基线 (346/346) 对比: +6 个测试 (新增边界用例), 无回归

**关键验证项**:
- capabilities 退出码 0
- inspect --hiss 不存在文件退出码 8 (FILE_IO_ERROR)
- inspect --hiss 无效 magic 退出码 25 (HISS_INVALID)
- inspect --hcsd 无效 magic 退出码 26 (HCSD_INVALID)
- inspect --frame 无效 FITS 退出码 28 (INPUT_INVALID)
- inspect --hiss 真实文件退出码 0 (result + completed 事件)
- inspect --hcsd 真实文件退出码 0 (n_leaves=49152)
- 超时测试退出码 9 (TIMEOUT)
- 原子性: stage1 失败后部分输出文件已删除
- JSONL 有效性: 所有非空行均为有效 JSONL
- stdout/stderr 严格分离

### 5. 发布包自包含性

**验证**: 发布包不得依赖用户安装 Python/PowerShell/.NET/Visual C++ Runtime。

**结果**: PASS
- orchestrator.exe 用 -static 编译, 自身无外部 DLL 依赖 (仅 msvcrt.dll 系统库)
- MinGW-w64 运行时 DLL (7 个) 已包含在 bin/
- verify.bat 纯 Windows 原生 (cmd.exe + certutil), 不调用 Python/PowerShell
- 模块 DLL (9 个) 已包含在 lib/<module>/
- 配置文件 (2 个) 已包含在 config/

### 6. 版本与 SHA-256 清单完整性

**验证**: VERSION.txt 和 SHA256SUMS.txt 包含完整的版本和 hash 信息。

**结果**: PASS
- VERSION.txt: v1.1.0, git commit 29cb291, 构建环境 g++ 16.1.0 MSYS2, 组件清单完整
- SHA256SUMS.txt: 22 个文件 SHA-256 完整记录 (含自引用)
- orchestrator.exe SHA-256: 759e2d4ff640bbf752ac7047037b5dc7d4e9c4107e3206013988024d02d21b50, 与 SHA256SUMS.txt 一致

### 7. 交接文档完整性

**验证**: HANDOVER.md 和 final_handover.json 包含完整交接信息。

**结果**: PASS
- HANDOVER.md: 项目概述 + 完成任务汇总 (31 任务) + 交付物清单 + 已知缺口 (7 项) + 性能基线 + 部署要求 + 快速开始 + v1.2 路线图 + 复核结论
- final_handover.json: 结构化数据 (smoke/canonical/GUI 分析/回归/发布包/已知缺口/性能/部署/v1.2 规划)
- 控制文件: MASTER_TASK_REGISTER.csv (31 任务全部 DONE) + PROJECT_STATE.yaml (G8 PASSED) + CURRENT_TASK.md (v1.1 完成, 指向 v1.2)

## Contract/ABI/format findings

- **无契约变更**: P08-002 为复核与交接任务, 未修改任何业务源码
- **HISS/HCSD 格式**: v1.0 (与 engineering/contracts/ 一致)
- **JSONL 事件 schema**: v1 (与 engineering/contracts/jsonl_event_schema.json 一致)
- **错误码注册表**: 21 个退出码 (与 engineering/contracts/error_code_registry.csv 一致)
- **配置参数**: 49 个参数 (与 engineering/contracts/config_parameter_registry.csv 一致)
- **GUI 契约合规**: 走格式契约路径 (通过 astro_image_io.dll 直接读 HISS/HCSD), 不依赖 orchestrator.exe 内部逻辑

## Scientific regression findings

- **无科学回归**: P08-002 未修改任何业务源码
- **回归测试 352/352 PASS** 确认功能无退化
- **HCSD SHA-256 字节级一致** (2A9BD12E... 与 P00-003/P06-002/P06-003/P07-001 baseline 一致), 确定性保证成立
- **HISS 数据一致性** (nside/n_pix/filter/exposure_s 与 P05-002 记录一致)
- **capabilities JSON 输出** 与 P04-003/P08-001 基线一致 (10 modules, 8 stages, 21 exit_codes, 13 events)

## Risks

1. **GaiaDR3SP 数据库不包含** (~50GB): Stage1 PLATESOLVE/PHOTOMETRIC 必需, 用户需单独获取。**不阻塞 v1.1**。
2. **测试数据不包含** (~73GB): 用于验证, 用户需单独获取。**不阻塞 v1.1**。
3. **南天天区内存需求 32-35 GB**: 部署需 64 GB RAM (P07-001 基线确认)。**不阻塞 v1.1**。
4. **HISS 非字节级可重现** (zstd 时间戳): 数据一致, HCSD 字节级可重现 (P07-001 已记录)。**不阻塞 v1.1**。
5. **DLL 版本号 unknown**: 大多数模块 DLL version 字段未填充, 留待 v1.2+。**不阻塞 v1.1**。
6. **GUI 未包含在 v1.1 发布包**: healpix_browser_qt 源码完整, 留待 v1.2+ 发布。**不阻塞 v1.1**。
7. **G-002 缺口**: HISS has_snr=0 导致 SNR²加权退化为等权, P06-002 已用合成数据证明数学正确, 待 P03-004 修复后回归。**不阻塞 v1.1**。
8. **GAP-015 缺口**: STACK stage 为骨架 (工作在 GRADIENT_SPHERE 完成), 留待 v1.2+。**不阻塞 v1.1**。

**结论**: 所有风险均为已知限制或未来版本计划, **无阻塞 v1.1 发布**的缺口。

## 验收标准检查

- [x] 依赖任务均已通过 (P08-001 DONE, 所有前置任务 DONE)
- [x] 本任务目标有可复现证据 (smoke 5/5 + canonical 3/3 + GUI 分析 + 回归 352/352)
- [x] 相关回归全部运行 (352/352 PASS, 比 P08-001 基线 +6 测试无回归)
- [x] 独立复核以 VERDICT: PASS 结束
- [x] 更新任务注册表、当前任务和项目状态

## 最终裁决

**VERDICT: PASS**

v1.1 开发包 (AstroCS CLI Core v1.1.0) 交付完成。

- 发布包自包含, 不依赖 Python/PowerShell/.NET/VC++ Runtime
- 从干净目录验证 capabilities/inspect 全部通过 (5/5 smoke + 3/3 canonical)
- GUI 只依赖格式契约 (通过 astro_image_io.dll 直接读 HISS/HCSD), 不依赖 orchestrator.exe 内部逻辑
- 所有回归测试通过 (352/352), 无功能退化
- HCSD 字节级可重现 (SHA-256 与历史 baseline 一致)
- 无阻塞 v1.1 发布的缺口
- 交接文档完整 (HANDOVER.md + final_handover.json + 四份标准报告 + 控制文件更新)

v1.1 开发包 (31 个任务, 9 个 Gate 全部 PASSED) 交付完毕, 进入 v1.2 规划阶段。
