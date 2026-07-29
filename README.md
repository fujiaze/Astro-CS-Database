# AstroCS — 天文 CCD 图像校准与标准化数据库系统

**版本**: v1.3.0 (Stage1 全量验证 + 浏览器性能修复阶段)
**状态**: G12 (Stage1 科学链) 进行中 · 22/50 任务 DONE · P13-002 IN_PROGRESS
**平台**: Windows 10/11 (64-bit) · MinGW-w64 g++ 16.1.0
**审计入口**: [工程控制/AUDIT_PACK.md](工程控制/AUDIT_PACK.md)

---

## 项目简介

AstroCS (Astro CS Normalization Database) 将多帧天文 FITS 图像通过两阶段流水线转换为标准化的 HEALPix 天球数据库，支持球面浏览、测光校准、梯度校正与多帧叠加。

```
Stage1 (单帧预处理):  FITS ──→ .hiss
  READ_FITS → CALIBRATE → PLATESOLVE → PSF → PHOTOMETRIC → SNR → DRIZZLE

Stage2 (多帧合并):    .hiss ──→ .hcsd
  GRADIENT_SPHERE → STACK
```

## 快速开始

### 方式一：使用发布包（推荐，无需安装依赖）

1. 下载 `dist/AstroCS-CLI-v1/` 目录
2. 打开 cmd.exe，切换到发布包目录
3. 运行 `verify.bat` 验证完整性
4. 参照 [dist/AstroCS-CLI-v1/README.txt](dist/AstroCS-CLI-v1/README.txt) 执行 CLI 命令

```cmd
:: 设置 PATH
set PATH=%cd%\bin;%PATH%

:: 查询能力
lib\orchestrator\cpp\orchestrator.exe capabilities

:: Stage1 单帧预处理
lib\orchestrator\cpp\orchestrator.exe stage1 --frame <input.fits> --output <out.hiss> --config config\default_stage1.json

:: Stage2 多帧合并
lib\orchestrator\cpp\orchestrator.exe stage2 --frames <hiss_dir> --output <out.hcsd> --config config\default_stage2.json

:: 检查文件
lib\orchestrator\cpp\orchestrator.exe inspect --hiss <file>
lib\orchestrator\cpp\orchestrator.exe inspect --hcsd <file>
```

### 方式二：从源码构建

```powershell
# 需要 MSYS2 mingw64 环境 (g++ 16.1.0, cmake, ninja)
cd lib\orchestrator\cpp
cmake -B build -G Ninja
cmake --build build
```

## 目录结构

```
Astro-CS-Normalization-Database/
├── lib/                       # 源码（活跃）
│   ├── orchestrator/cpp/      # CLI 编排器 (orchestrator.exe, 32MB 栈)
│   ├── astro_image_io/        # FITS/XISF/HISS/HCSD I/O
│   ├── calibration/           # CCD 校准
│   ├── plate_solve/cpp/ipv/   # PlateSolve 求解器
│   ├── dynamic_psf/           # PSF 拟合
│   ├── photometric_calib/cpp/ # 测光校准 + Gaia 客户端
│   ├── snr_estimator/cpp/     # SNR 估算 (leaf_max_size=32)
│   └── healpix_db/
│       ├── healpix_drizzle/   # 球面 Drizzle (DLL 栈 8MB)
│       ├── healpix_stack/     # 稀疏堆栈 + 球面梯度
│       └── healpix_browser_qt/# GUI 浏览器 (Qt6) + CLI 调试工具
├── 工程控制/          # 当前工程包（活跃，P09-P17）
│   ├── AUDIT_PACK.md          # 审计交付包入口
│   ├── AUTONOMOUS_ENTRY.md    # 自治执行入口
│   ├── control/               # 项目状态控制文件
│   ├── docs/                  # Spec 文档 (28 篇)
│   ├── contracts/             # 契约文档
│   ├── evidence/              # 任务证据 (P09~P13)
│   └── checklists/            # Gate 检查清单
├── engineering/               # v1.0 工程包（归档参考）
├── engineering_v1.2/          # v1.2 工程包（归档参考）
├── tools/                     # Python 工具集 (astro_toolkit)
├── testdata/                  # 测试数据 (710 帧, ~73 GB)
├── GaiaDR3SP/                 # Gaia DR3SP 数据库 (~50 GB)
├── output/                    # 运行时输出 (.hiss/.hcsd)
├── ROOT_INVENTORY.md          # 根目录清单与归档状态
└── memory.md                  # 工程记忆
```

## 核心契约

| 契约 | 描述 |
|---|---|
| [hiss_format_v1.md](engineering/contracts/hiss_format_v1.md) | HISS v1.0 单帧存储格式 (FROZEN) |
| [hcsd_format_v1.md](engineering/contracts/hcsd_format_v1.md) | HCSD v1.0 天球数据库格式 (FROZEN) |
| [cli_command_schema_v1.json](engineering/contracts/cli_command_schema_v1.json) | CLI 命令 schema |
| [cli_event_schema_v1.json](engineering/contracts/cli_event_schema_v1.json) | JSONL 事件 v1 schema (13 种事件) |
| [error_code_registry.csv](engineering/contracts/error_code_registry.csv) | 错误码注册表 (21 个退出码) |
| [config_parameter_registry.csv](engineering/contracts/config_parameter_registry.csv) | 配置参数注册表 (49 参数) |

## 退出码

| 码 | 含义 |
|---|---|
| 0 | SUCCESS |
| 1 | GENERIC_ERROR |
| 2 | DLL_LOAD_FAILED |
| 3 | BLOCK_MISSING |
| 4 | CALIBRATE_FAILED |
| 5 | PLATESOLVE_FAILED |
| 6 | DRIZZLE_FAILED |
| 7 | CONFIG_ERROR |
| 8 | FILE_IO_ERROR |
| 9 | TIMEOUT |
| 10 | CANCELLED |
| 20-28 | 模块特定错误 |
| 100 | MODULE_SPECIFIC_BASE |

## 性能基线 (P07-001)

| 指标 | Stage1 (C003) | Stage2 (2 帧) |
|---|---|---|
| 中位数耗时 | 77.8s | 5.6s |
| 峰值内存 | 35 GB (南天) | 2 GB |
| 输出可重现 | 数据一致 | 字节级可重现 |
| 内存泄漏 | 无 | 无 |

> 南天天区 (如 NGC1727, dec=-70°) 因 Gaia DR3SP 分区较大，需 64 GB RAM；赤道天区 (如 Galaxy Center, dec=-13°) 仅需 4 GB。

## 部署要求

| 配置 | 最低 | 推荐 |
|---|---|---|
| RAM | 4 GB | 64 GB (南天) |
| 磁盘 | 5 GB | 200 GB+ (含 Gaia + 测试数据) |
| CPU | 4 核 | 16 核 (OpenMP) |
| OS | Windows 10/11 64-bit | Windows 11 |

发布包自包含，**无需**安装 Python / PowerShell / .NET / Visual C++ Runtime。

## 工程工具集

项目内置 [tools/astro_toolkit.py](tools/astro_toolkit.py)：Python + JSON 配置驱动的批量操作工具，一次调用执行多步操作（git + orchestrator + hash + 文件操作），减少交互确认。

```powershell
python tools/astro_toolkit.py tools/my_task.json --log tools/my_task.log
```

详见 [tools/README.md](tools/README.md)。

## 项目管理

- **审计入口**: [工程控制/AUDIT_PACK.md](工程控制/AUDIT_PACK.md) (v1.3 交付包)
- **任务注册表**: [工程控制/control/MASTER_TASK_REGISTER.csv](工程控制/control/MASTER_TASK_REGISTER.csv) (50 任务)
- **项目状态**: [工程控制/control/PROJECT_STATE.yaml](工程控制/control/PROJECT_STATE.yaml)
- **当前任务**: [工程控制/control/CURRENT_TASK.md](工程控制/control/CURRENT_TASK.md)
- **自治入口**: [工程控制/AUTONOMOUS_ENTRY.md](工程控制/AUTONOMOUS_ENTRY.md)
- **根目录清单**: [ROOT_INVENTORY.md](ROOT_INVENTORY.md)

## 已知限制

| ID | 描述 |
|---|---|
| G-002 | HISS has_snr=0 → SNR²加权退化为等权（合成数据已证明数学正确，待 v1.2 修复） |
| GAP-015 | STACK stage 为骨架（工作在 GRADIENT_SPHERE 完成） |
| HISS-reproducibility | HISS 非字节级可重现（zstd 时间戳），HCSD 字节级可重现 |
| Gaia-memory | 南天天区 Gaia 内存需求 32-35 GB |
| External-data | GaiaDR3SP (~50GB) 和 testdata (~73GB) 不含在发布包中 |

## v1.3 路线图（当前）

1. ✅ P09-P12: 基线冻结 + TestData + WCS + 测光修复 (22 任务 DONE)
2. ⏳ P13: Stage1 全量验证 (P13-002 IN_PROGRESS, 用户调整范围)
3. ⏳ Stage1 性能优化 (80s/帧 → 目标 <30s/帧)
4. ⏳ P14: 银心三片 32 帧马赛克 (8 任务 TODO)
5. ⏳ P15: 浏览器异步 I/O (8 任务 TODO)
6. ⏳ P16: 浏览器 GPU Tile Renderer (6 任务 TODO)
7. ⏳ P17: 统一回归与发布 (3 任务 TODO)

## v1.3 关键修复（2026-07-29）

- 栈溢出修复 (snr_evaluator nanoflann 递归 + EXE 栈 32MB)
- 浏览器 CLI 后台调试工具 (DLL 诊断 + 性能基准 + 交互模拟)
- 浏览器部署修复 (libgomp-1.dll + liblz4.dll)
- Stage2 全流程验证 (银心 5 帧 + 胜利 20 帧 LUM)

## 文档导航

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — 系统架构
- [docs/PROJECT_OVERVIEW.md](docs/PROJECT_OVERVIEW.md) — 项目概述
- [docs/PIPELINE_OVERVIEW.md](docs/PIPELINE_OVERVIEW.md) — 流水线概述
- [docs/DESIGN_IMPL_GAP.md](docs/DESIGN_IMPL_GAP.md) — 设计与实现差距
- [engineering/evidence/P08-002/HANDOVER.md](engineering/evidence/P08-002/HANDOVER.md) — v1.1 最终交接文档

## 许可

私有项目。© 2026 AstroCS Engineering.
