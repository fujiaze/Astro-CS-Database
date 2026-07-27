# AstroCS CLI Core v1.1 开发包 - 最终交接文档

**版本**: v1.1.0
**发布阶段**: P08-002 最终独立复核与交接
**Gate**: G8 (Release) - PASSED
**交接日期**: 2026-07-27
**VERDICT**: PASS

---

## 1. 项目概述

### 1.1 项目目标

AstroCS (Astro CS Normalization Database) 是天文 CCD 图像校准与标准化数据库系统。
核心目标: 将多帧天文 FITS 图像通过两阶段流水线转换为标准化的 HEALPix 天球数据库,
支持球面浏览、测光校准、梯度校正与多帧叠加。

### 1.2 系统架构

**两阶段流水线** (C++ CLI + 模块化 DLL):

```
Stage1 (单帧预处理): FITS → .hiss
  READ_FITS → CALIBRATE → PLATESOLVE → PSF → PHOTOMETRIC → SNR → DRIZZLE

Stage2 (多帧合并): .hiss → .hcsd
  GRADIENT_SPHERE → STACK
```

**模块化 DLL 架构** (10 个模块):
- `astro_image_io.dll` - FITS/XISF/HISS/HCSD I/O
- `astro_calibration.dll` - CCD 校准
- `ipv_solver.dll` - PlateSolve 求解器
- `dynamic_psf.dll` - PSF 拟合
- `photometric_calib.dll` - 测光校准
- `gaia_client.dll` - Gaia DR3/DR3SP 客户端
- `snr_estimator.dll` - SNR 估算
- `healpix_drizzle.dll` - 球面 Drizzle 重投影
- `healpix_stack.dll` - 稀疏堆栈 + 球面梯度

### 1.3 技术栈

- **核心语言**: C++17 (MinGW-w64 g++ 16.1.0)
- **并行**: OpenMP (16 线程)
- **压缩**: zstd + LZ4 + zlib
- **CLI**: orchestrator.exe (-static 编译, 无外部 DLL 依赖)
- **GUI** (v1.2+): Qt6 + OpenGL 3.3 Core
- **格式**: HISS v1.0 (单帧) + HCSD v1.0 (天球数据库) + JSONL 事件 v1

---

## 2. 完成任务汇总 (v1.1 开发包)

**总计 31 个任务, 全部 DONE, 9 个 Gate 全部 PASSED**

### 2.1 P00 - 项目基线与契约 (G0)

| 任务 | 标题 | 状态 |
|---|---|---|
| P00-001 | 仓库与环境事实基线 | DONE |
| P00-002 | 恢复并锁定全部模块源码 | DONE |
| P00-003 | 旧 CLI 真实数据基线 | DONE |
| P00-004~008 | 仓库整合/合并/JSON 修复/基线清单 | DONE |

### 2.2 P01 - 核心数据结构与 FITS I/O (G1)

| 任务 | 标题 | 状态 |
|---|---|---|
| P01-001 | PipelineFrame 唯一归属决策 (ADR-005) | DONE |
| P01-002 | 数据块注册表与 schema 校验器 | DONE |
| P01-003 | HISS/HCSD 格式版本与 round-trip | DONE |

### 2.3 P02 - Gaia 客户端与匹配 (G2)

| 任务 | 标题 | 状态 |
|---|---|---|
| P02-001~004 | PlateSolve 全量 TestData/路径决策/实施 | DONE |
| P02-005 | Dynamic PSF float32 API | DONE |
| P02-006 | Gaia 查询边界与缓存 | DONE |
| P02-007 | PlateSolve 无退化与单次检测专项 Gate | DONE |

### 2.4 P03 - 星检测与测光定标 (G3)

| 任务 | 标题 | 状态 |
|---|---|---|
| P03-001 | 真实校准输入接线 | DONE |
| P03-002 | 配置参数端到端追踪 (49 参数) | DONE |
| P03-003 | 严格失败与禁止静默跳过 (9 退出码) | DONE |
| P03-004 | SNR 稀疏模型与 SIP 一致性 | DONE |

### 2.5 P04 - CLI 骨架与取消/超时 (G4)

| 任务 | 标题 | 状态 |
|---|---|---|
| P04-001 | CLI request 与 effective config | DONE |
| P04-002 | JSONL 事件与稳定错误码 (21 码) | DONE |
| P04-003 | capabilities 与 inspect 命令 | DONE |
| P04-004 | 取消、超时与 partial 输出 | DONE |

### 2.6 P05 - Stage1 真实数据验证 (G5)

| 任务 | 标题 | 状态 |
|---|---|---|
| P05-001 | 真实参考数据集登记 (7 帧) | DONE |
| P05-002 | Stage1 真实数据端到端 (6/6 帧 PASS) | DONE |
| P05-003 | Stage1 负面与恢复测试 | DONE |

### 2.7 P06 - Stage2 叠加验证 (G6)

| 任务 | 标题 | 状态 |
|---|---|---|
| P06-001 | Stage2 真实输入兼容检查 | DONE |
| P06-002 | 球面梯度与稳健叠加证据 (SNR²加权证明) | DONE |
| P06-003 | HCSD 输出与独立读取 (字节级可重现) | DONE |

### 2.8 P07 - 性能与稳定性 (G7)

| 任务 | 标题 | 状态 |
|---|---|---|
| P07-001 | 性能与峰值内存基线 (Stage1 77.8s + Stage2 5.6s) | DONE |
| P07-002 | 长批次与故障稳定性 (13/13 PASS) | DONE |

### 2.9 P08 - 发布包与交接 (G8)

| 任务 | 标题 | 状态 |
|---|---|---|
| P08-001 | CLI Core v1 发布包 (22 文件, 自包含) | DONE |
| P08-002 | 最终独立复核与交接 (本任务) | DONE |

---

## 3. 关键交付物清单

### 3.1 代码仓库

- **主仓库**: https://github.com/fujiaze/Astro-CS-Database
- **分支**: main
- **最新 commit**: 889944a (P08-001 CLI Core v1 发布包) → P08-002 commit (待提交)

### 3.2 契约文档 (engineering/contracts/)

| 契约 | 描述 |
|---|---|
| `hiss_format_v1.md` | HISS v1.0 单帧存储格式 |
| `hcsd_format_v1.md` | HCSD v1.0 天球数据库格式 |
| `jsonl_event_schema.json` | JSONL 事件 v1 schema (13 种事件) |
| `error_code_registry.csv` | 错误码注册表 (21 个退出码) |
| `config_parameter_registry.csv` | 配置参数注册表 (49 参数) |

### 3.3 发布包 (dist/AstroCS-CLI-v1/)

- **22 个文件** (~22 MB)
- **自包含**: 不依赖 Python/PowerShell/.NET/VC++ Runtime
- **orchestrator.exe**: -static 编译, 9/9 模块 DLL 加载
- **SHA-256 清单**: SHA256SUMS.txt (22 文件完整性)
- **验证脚本**: verify.bat (Windows 原生, 不依赖 Python/PowerShell)
- **配置文件**: default_stage1.json (34 参数) + default_stage2.json (15 参数)

### 3.4 证据文档 (engineering/evidence/)

- **31 个任务证据目录** (P00-001 ~ P08-002)
- **每个任务**: TASK_REPORT.md + TEST_REPORT.md + EVIDENCE_INDEX.md + REVIEW_REPORT.md
- **回归测试**: test_orchestrator_cli.exe 352/352 PASS

### 3.5 控制文件 (engineering/control/)

- `MASTER_TASK_REGISTER.csv` - 31 个任务状态 (全部 DONE)
- `PROJECT_STATE.yaml` - 项目状态 (G8 PASSED, v1.1 完成交付)
- `CURRENT_TASK.md` - 当前任务 (v1.1 完成, 指向 v1.2 规划)

---

## 4. 已知缺口和风险

| ID | 描述 | 状态 | 阻塞 v1.1 |
|---|---|---|---|
| G-002 | HISS has_snr=0 → SNR²加权退化为等权 | P06-002 用合成数据证明数学正确 | 否 |
| GAP-015 | STACK stage 为骨架 (工作在 GRADIENT_SPHERE 完成) | 已知缺口 | 否 |
| Gaia-memory | 南天天区 Gaia 内存需求 32-35 GB | 部署需 64 GB RAM | 否 |
| HISS-reproducibility | HISS 非字节级可重现 (zstd 时间戳) | 数据一致, HCSD 字节级可重现 | 否 |
| DLL-version-unknown | 大多数模块 DLL 的 version 字段为 unknown | 留待未来版本补充 | 否 |
| GaiaDR3SP-not-included | GaiaDR3SP 数据库 (~50GB) 不包含 | 用户需单独获取 | 否 |
| testdata-not-included | 测试数据 (~73GB) 不包含 | 用户需单独获取 | 否 |

**结论**: 无阻塞 v1.1 发布的缺口, 所有已知缺口均为已知限制或未来版本计划。

---

## 5. 性能基线汇总 (P07-001)

| 指标 | Stage1 (C003) | Stage2 |
|---|---|---|
| 中位数 wall 时间 | 77.805s | 5.597s |
| 峰值内存 | 35470 MB (~35 GB) | 1979 MB (~2 GB) |
| HCSD SHA-256 可重现 | N/A | 字节级可重现 (2A9BD12E...) |
| 内存泄漏 | 无 (3 次峰值差异 <2 MB) | 无 |
| 取消测试 | PASS (进程退出无残留) | PASS |

**性能异常说明** (非回归):
- C001 南天内存 3.6GB vs C003 35.5GB: 根因 Gaia xpsd 分区覆盖
- HISS 非字节级可重现: zstd 压缩含时间戳 (P00-003 已记录)

---

## 6. 部署要求

### 6.1 硬件要求

| 配置 | 最低 | 推荐 |
|---|---|---|
| RAM | 4 GB (Stage1 单帧) | 64 GB (南天天区) |
| 磁盘 | 5 GB (发布包 + 临时) | 200 GB+ (GaiaDR3SP + testdata) |
| CPU | 4 核 | 16 核 (OpenMP 并行) |

### 6.2 软件要求

- **操作系统**: Windows 10/11 (64-bit)
- **Python**: 不需要 (发布包自包含)
- **PowerShell**: 不需要 (verify.bat 用 cmd.exe)
- **.NET**: 不需要
- **Visual C++ Runtime**: 不需要 (orchestrator.exe -static 编译)
- **MinGW 运行时**: 已包含在 bin/ (7 个 DLL)

### 6.3 外部数据

- **GaiaDR3SP 数据库** (~50GB): 需单独获取, Stage1 PLATESOLVE/PHOTOMETRIC 必需
- **测试数据** (testdata/, ~73GB): 需单独获取, 用于验证

---

## 7. 快速开始指南

### 7.1 验证发布包

```cmd
:: 1. 打开命令提示符 (cmd.exe)
:: 2. 切换到发布包目录
cd <path>\AstroCS-CLI-v1

:: 3. 运行验证脚本
verify.bat
```

验证脚本会:
- 检查文件完整性 (SHA-256)
- 运行 capabilities 命令 (9/9 DLL 加载)
- 运行 inspect 命令 (exit 8 = FILE_IO_ERROR)

### 7.2 CLI 命令

```cmd
:: 设置 PATH (MinGW 运行时 DLL)
set PATH=%cd%\bin;%PATH%

:: 1. 查询能力
lib\orchestrator\cpp\orchestrator.exe capabilities

:: 2. Stage1 (单帧预处理)
lib\orchestrator\cpp\orchestrator.exe stage1 --frame <fits> --output <hiss> --config config\default_stage1.json

:: 3. Stage2 (多帧合并)
lib\orchestrator\cpp\orchestrator.exe stage2 --frames <dir> --output <hcsd> --config config\default_stage2.json

:: 4. 检查文件
lib\orchestrator\cpp\orchestrator.exe inspect --hiss <file>
lib\orchestrator\cpp\orchestrator.exe inspect --hcsd <file>
```

详见: `dist/AstroCS-CLI-v1/README.txt`

---

## 8. 后续路线图 (v1.2 计划)

### 8.1 v1.2 规划范围

1. **GUI 发布包**: healpix_browser_qt + Qt6 运行时 + astro_image_io.dll
2. **G-002 修复**: HISS has_snr 持久化, SNR²加权真实生效
3. **GAP-015 完成**: STACK stage 完整实现 (替代骨架)
4. **DLL 版本号补充**: capabilities 输出模块版本号
5. **CLI 契约路径 GUI 原型**: BrowserBackendCli 类, 验证完全解耦可行性

### 8.2 长期愿景

- 多波段 RGB 合成浏览
- 窄带滤光片支持 (Hα/OIII/SII)
- 梯度校正迭代优化
- Drizzle 性能优化 (当前 26s 占总管线 43%)
- 跨平台支持 (Linux/macOS)

---

## 9. P08-002 最终独立复核结论

### 9.1 复核内容

1. **独立环境 smoke 测试**: 5/5 PASS (capabilities + inspect --hiss + inspect --hcsd + inspect nonexistent + verify.bat 等价)
2. **Canonical 测试**: 3/3 PASS (C003 HISS inspect + HCSD inspect + SHA-256 baseline)
3. **GUI 依赖分析**: PASS (GUI 只依赖格式契约 + 独立 I/O 库, 不依赖 orchestrator.exe 内部逻辑)
4. **回归测试总览**: 全部 PASS (test_orchestrator_cli 352/352 + 合成数据 3/3 + 端到端 6/6 + 长批次 13/13 + 性能 9/9)

### 9.2 验收标准检查

- [x] 依赖任务均已通过 (P08-001 DONE, 所有前置任务 DONE)
- [x] 本任务目标有可复现证据 (smoke + canonical + GUI 分析 + 回归)
- [x] 相关回归全部运行 (352/352 PASS)
- [x] 独立复核以 VERDICT: PASS 结束
- [x] 更新任务注册表、当前任务和项目状态

### 9.3 最终裁决

**VERDICT: PASS**

v1.1 开发包 (AstroCS CLI Core v1.1.0) 交付完成。
发布包自包含, 不依赖 Python/PowerShell/.NET/VC++ Runtime,
从干净目录验证 capabilities/inspect 全部通过,
GUI 只依赖格式契约不依赖 orchestrator.exe 内部逻辑,
所有回归测试通过, 无阻塞缺口。

---

**交接人**: AstroCS 工程项目子 Agent
**交接日期**: 2026-07-27
**版本**: v1.1.0
**Gate**: G8 (Release) - PASSED
