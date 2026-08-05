# AstroCS 项目交接文档

**生成时间**: 2026-08-04 23:30 (Asia/Shanghai) | **更新**: 2026-08-04 18:45 (阶段 8 完成)
**当前分支**: main
**HEAD**: `c748077` (main, 2026-08-05)
**当前阶段**: R10 纠正控制包阶段 1-8 全部完成（Wiki/README/证据/交付ZIP 已交付）

> **2026-08-04 更新**: 阶段 8 已全部完成并推送。
> 审核包: `AstroCS_Review_JSONOrchestratorTrueDualPrecision_20260804.zip`
> 证据: `工程控制/evidence/R10-001/`（验收自审 27/27 闭合）
> 统一工具链入口: 根目录 `toolchain.ps1`（check/build/run/review），规范见 AGENTS.md。
> 命名规范: 输入 `AstroCS_Control_<主题>_<日期>.zip`，输出 `AstroCS_Review_<主题>_<日期>.zip`。
> `tools/astro_toolkit.py` 已恢复为强制工具（工程批处理，git/编译/文件操作），
> 与 `toolchain.ps1` 协作（toolchain 是统一入口，astro_toolkit 是批处理执行层）。
> **ACR 控制包/审核包（ZIP 与展开目录）禁止删除**（用户 2026-08-04 明确要求）。

---

## 1. 项目总览

### 1.1 项目性质

AstroCS 是一个原生天文图像处理内核（C++/DLL），将真实 FITS 图像转换为标准化 HEALPix 球面帧数据库（HISS），并通过 SNR² 加权全局加性梯度统一、稳健排异和多帧叠加生成连续球面数据库（HCSD）。

### 1.2 仓库

- **主仓库**: https://github.com/fujiaze/Astro-CS-Database
- **Wiki 仓库**: https://github.com/fujiaze/Astro-CS-Database.wiki.git （本地已克隆到 `AstroCS.wiki/`）
- **GitHub 账号**: fujiaze (https 协议)

### 1.3 权威文档层级

1. GitHub Wiki（唯一权威总文档）
2. `工程控制/`（工程控制包：规范、任务清单、证据）
3. 根 README.md（仅作入口与导航，不再声明自己是 SSOT）
4. 源码注释和实现说明

**注意**: 根 README.md 当前仍是旧版（1575 行，声明自己是 SSOT），需要按控制包 `README_REWRITE_REQUIREMENTS.md` 精简重写（阶段 8a 待办）。

---

## 2. 当前工作上下文（R10 纠正控制包）

### 2.1 背景

用户在 2026-08-04 提供 R10 纠正控制包 `AstroCS_Phase1_JSON_Orchestrator_TrueDualPrecision_Correction.zip`（SHA256: `ef677bbcaa87be05a99a1ea6569e3d5d9e0b7d731fc16172c17ed665c4f77665`），拒绝上一轮交付，要求：

1. 纠正虚假完成状态
2. 只允许 `orchestrator.exe <json>` 单一入口
3. 彻底移除生产 Python 封装
4. 实现真正全链路 FP32/FP64
5. 用合成数据和单帧分段验收
6. **严禁触碰 ACR**

### 2.2 控制包位置

```
run/temp/r10_correction/AstroCS_Phase1_JSON_Orchestrator_TrueDualPrecision_Correction/
├── 00_START_PROMPT.txt          # 启动提示词
├── START_HERE.md
├── audit/                       # 审计报告
├── configs/                     # JSON 配置模板
├── control/                     # 执行顺序、契约、计划文档
├── delivery/                    # 交付要求（ACCEPTANCE_CHECKLIST, REQUIRED_STRUCTURE）
├── schemas/                     # stage1.schema.json (JSON Schema)
├── templates/                   # 验收模板
├── wiki/                        # 需同步到 GitHub Wiki 的页面
├── FILES_SHA256.txt
└── PACKAGE_MANIFEST.yaml
```

### 2.3 执行顺序（control/EXECUTION_ORDER.md）

| 阶段    | 内容                                                        | 状态        |
| ----- | --------------------------------------------------------- | --------- |
| 0     | 读取 Wiki 与 audit，自检 astro_toolkit，纠正项目状态                   | ✅ 已完成     |
| 1     | 清理源码树生成产物                                                 | ✅ 已完成     |
| 2     | 删除 REPL/run/run-batch，实现 `orchestrator.exe <json>`        | ✅ 已完成     |
| 3     | 全仓扫描 Python，删除生产 wrapper，加非生产标记                           | ✅ 已完成     |
| 4     | 建立公共 AstroScalarType、typed PipelineFrame、PrecisionContext | ✅ 已完成     |
| 5     | 逐模块实现双精度（AIO→...→Browser）                                 | ✅ 已完成     |
| 6     | HISS/SNR 双 dtype、FP64 query、全 Tile verify、SNR 点原因分类       | ✅ 已完成     |
| 7     | 合成模块测试 + 单帧逐 Gate                                         | ✅ 已完成     |
| **8** | **更新 Wiki、生成真实证据、清理源码、完整哈希并交付**                           | **❌ 未开始** |

### 2.4 阶段 7 验证结果（最新）

**FP32 模式** (`run/logs/r10/fp32_snr_fix_verify_20260804.log`):

- HISS_VERIFY: 285/285 Tile 全部通过
- SNR 控制点: 全部有效（修复后）
- stage1 exit_code=0 成功

**FP64 模式** (`run/logs/r10/fp64_snr_fix_verify_20260804.log`):

- HISS_VERIFY: 285/285 Tile 全部通过
- SNR 控制点: n_total=2000, n_valid=1947, n_dropped=53（全部归因 INVALID_PSF，零 NaN/越界/radec2pix 失败）
- stage1 exit_code=0 成功
- 输出: `run/temp/r10_validation/output/fp64/frame.hiss`

### 2.5 R10 关键修复（已提交）

| Commit    | 内容                                            |
| --------- | --------------------------------------------- |
| `85fa651` | 编排器 JSON 唯一入口重构（删除 REPL/run/run-batch）        |
| `8c616f1` | 手写 JSON 解析器替换为 nlohmann/json                  |
| `8653bd5` | Python 生产层彻底清理（421 文件删除，非生产标记）                |
| `bd036f3` | 真正全链路 FP32/FP64 双精度 ABI 改造                    |
| `f654766` | FP64 HISS_VERIFY 修复（精度感知 signal 读取）           |
| `c6efe31` | HISS_VERIFY 全 Tile 验证 + SNR 点原因分类             |
| `8cb22a9` | SnrControlPoint 结构体打包 bug 修复（根因：1609 SNR 点丢失） |

### 2.6 R10 关键 Bug 修复记录（供下一个 agent 参考）

#### Bug 1: NSIDE 计算常数错误

- **症状**: 自动 NSIDE 选择 nside=512（412"/px），与 6.3"/px 源像素不匹配
- **根因**: `calculate_nside` 用常数 `1186.18`，应为 `211034.6`（差约 178 倍）
- **位置**: `lib/orchestrator/cpp/src/orchestrator.cpp`
- **修复**: 用 `HEALPIX_SCALE_PER_NSIDE_ARCSEC = π/(3·NSIDE²)` 推导

#### Bug 2: HEALPix 边细分不收敛

- **症状**: nside=65536 时 drizzle 超时（332B 操作）
- **根因**: 阈值 `hp_res_rad * 1e-12` 对非大圆弧 HEALPix 边永不收敛，导致 1024 顶点/像素
- **位置**: `lib/healpix_db/healpix_drizzle/spherical_overlap.cpp`
- **修复**: 阈值改为 `hp_res_rad * 1e-6`，HEALPix 边用角距离法

#### Bug 3: 跨 DLL 精度上下文不共享

- **症状**: FP64 模式但 AIO 仍按 FP32 读取 data 块
- **根因**: `PrecisionContext` 单例在 EXE 和 DLL 各有一份副本
- **位置**: `lib/astro_image_io/include/astro_image_io.h`, `lib/astro_image_io/src/aio_api.cpp`
- **修复**: 新增 `aio_set_precision_mode(int is_fp64)` 显式 API

#### Bug 4: SnrControlPoint 结构体未打包（最关键）

- **症状**: 1979 个 SNR 控制点只有 370 有效，1609 个"ra/dec 越界"丢失
- **根因**: `SnrControlPoint` 未 `#pragma pack(1)`，sizeof=24（4 字节尾部填充），但 orchestrator 序列化用 `memcpy(dst, points, n*20)` 按 20 字节连续拷贝，从第 2 个点起 ra/dec 错位
- **位置**: `lib/snr_estimator/cpp/include/snr_estimator.h`
- **修复**: 添加 `#pragma pack(push, 1)` 和 `static_assert(sizeof==20)`

#### Bug 5: FP64 模式 PlateSolve 失败

- **症状**: FP64 模式星点检测失败（median=5）
- **根因**: 星点检测器收到 double* 但按 float* 解析
- **位置**: `lib/orchestrator/cpp/src/orchestrator.cpp`
- **修复**: 添加 FP64→FP32 显式转换缓冲区

---

## 3. 项目编译与运行

### 3.1 编译环境

- **编译器**: MSYS2 MinGW64 g++ (C:\msys64\mingw64\bin)
- **C++ 标准**: C++17
- **依赖库**: nlohmann/json（通过 `pacman -S mingw-w64-x86_64-nlohmann-json` 安装）
- **并行**: OpenMP (-fopenmp)
- **静态链接**: -static-libgcc -static-libstdc++ -static

### 3.2 编译产物清单（当前已编译）

| 模块                 | 产物                                                               | 大小      | 编译时间             |
| ------------------ | ---------------------------------------------------------------- | ------- | ---------------- |
| orchestrator       | `lib/orchestrator/cpp/orchestrator.exe`                          | 4297 KB | 2026-08-04 17:22 |
| astro_image_io     | `lib/astro_image_io/astro_image_io.dll`                          | 3578 KB | 2026-08-04 16:16 |
| calibration        | `lib/calibration/astro_calibration.dll`                          | 987 KB  | 2026-08-04 11:47 |
| calibration        | `lib/calibration/cosmetic_corrector.dll`                         | 669 KB  | 2026-08-02 23:27 |
| dynamic_psf        | `lib/dynamic_psf/dynamic_psf.dll`                                | 338 KB  | 2026-08-04 11:47 |
| photometric_calib  | `lib/photometric_calib/cpp/photometric_calib.dll`                | 1075 KB | 2026-08-04 12:51 |
| plate_solve (ipv)  | `lib/plate_solve/cpp/ipv/ipv_solver.dll`                         | 760 KB  | 2026-08-02 23:29 |
| star_detector      | `lib/star_detector/star_detector.dll`                            | 964 KB  | 2026-08-02 23:29 |
| snr_estimator      | `lib/snr_estimator/cpp/snr_estimator.dll`                        | 956 KB  | 2026-08-04 17:21 |
| gaia_xpsd_client   | `lib/gaia_xpsd_client/gaia_client.dll`                           | 276 KB  | 2026-08-02 23:26 |
| healpix_drizzle    | `lib/healpix_db/healpix_drizzle/healpix_drizzle.dll`             | 1314 KB | 2026-08-04 17:03 |
| healpix_stack      | `lib/healpix_db/healpix_stack/healpix_stack.dll`                 | 1437 KB | 2026-08-03 11:39 |
| healpix_browser_qt | `lib/healpix_db/healpix_browser_qt/build/healpix_browser_qt.exe` | 1563 KB | 2026-07-31 15:55 |

### 3.3 编译流程（每个模块独立 Makefile）

```powershell
# 设置环境
$env:Path = "C:\msys64\mingw64\bin;$env:Path"

# 编译顺序（有依赖关系）
# 1. 公共头（无产物）
# 2. 各 DLL 模块（独立编译）
cd lib\astro_image_io; make        # astro_image_io.dll
cd lib\calibration; make           # astro_calibration.dll
cd lib\dynamic_psf; make           # dynamic_psf.dll
cd lib\plate_solve\cpp\ipv; make   # ipv_solver.dll
cd lib\star_detector; make         # star_detector.dll
cd lib\snr_estimator\cpp; make     # snr_estimator.dll
cd lib\photometric_calib\cpp; make # photometric_calib.dll
cd lib\healpix_db\healpix_drizzle; make  # healpix_drizzle.dll
cd lib\healpix_db\healpix_stack; make    # healpix_stack.dll

# 3. 编排器（最后编译，依赖所有 DLL 头文件）
cd lib\orchestrator\cpp; make      # orchestrator.exe

# 4. 浏览器（独立 CMake，可选）
cd lib\healpix_db\healpix_browser_qt; mkdir build; cd build
cmake .. -G Ninja; ninja
```

### 3.4 运行方式（唯一正式入口）

```powershell
# 唯一正式入口
.\lib\orchestrator\cpp\orchestrator.exe <stage1.json>

# 示例（FP64 验证用配置）
.\lib\orchestrator\cpp\orchestrator.exe run\temp\r10_validation\fp64\stage1.json
```

**Stage1 JSON 配置模板**:

- 控制包模板: `run/temp/r10_correction/.../configs/stage1.template.json`
- 控制包示例: `run/temp/r10_correction/.../configs/stage1.minimal_read_example.json`
- R10 验证用: `run/temp/r10_validation/fp32/stage1.json` 和 `fp64/stage1.json`
- JSON Schema: `run/temp/r10_correction/.../schemas/stage1.schema.json`

### 3.5 Stage1 流水线（8 阶段）

```
READ_FITS → CALIBRATE → PLATESOLVE → PSF → PHOTOMETRIC → SNR → DRIZZLE → HISS_VERIFY
```

每阶段都有 `stage_timeout_sec` 保护，DRIZZLE 阶段建议 1800s（nside=65536 时约 190s）。

---

## 4. 源码模块结构

### 4.1 lib/ 模块清单

| 模块                 | 路径                                   | 职责                                             |
| ------------------ | ------------------------------------ | ---------------------------------------------- |
| common             | `lib/common/include/`                | 公共头：AstroScalarType, PrecisionContext（双精度 ABI） |
| astro_image_io     | `lib/astro_image_io/`                | FITS/XISF 读取、HISS 格式读写、PipelineFrame           |
| calibration        | `lib/calibration/`                   | Bias/Dark/Flat 校准、坏点修复                         |
| plate_solve        | `lib/plate_solve/cpp/ipv/`           | 向量匹配 WCS/SIP 解算                                |
| star_detector      | `lib/star_detector/`                 | 星点检测（PlateSolve 内部调用一次）                        |
| dynamic_psf        | `lib/dynamic_psf/`                   | PSF 拟合                                         |
| photometric_calib  | `lib/photometric_calib/cpp/`         | Gaia 光谱积分、全局光度尺度                               |
| snr_estimator      | `lib/snr_estimator/cpp/`             | SNR 模型提取（稀疏控制点）                                |
| healpix_drizzle    | `lib/healpix_db/healpix_drizzle/`    | Drizzle 投影、球面重叠                                |
| healpix_stack      | `lib/healpix_db/healpix_stack/`      | Stage2 梯度+叠加（**当前禁止修改**）                       |
| healpix_browser_qt | `lib/healpix_db/healpix_browser_qt/` | Qt 球面浏览器                                       |
| orchestrator       | `lib/orchestrator/cpp/`              | 编排器（JSON 唯一入口）                                 |
| gaia_xpsd_client   | `lib/gaia_xpsd_client/`              | Gaia DR3/DR3SP 客户端                             |
| data_pipeline      | `lib/data_pipeline/`                 | 历史 PipelineFrame（与 astro_image_io 重叠，待 ADR 明确） |

### 4.2 双精度 ABI 关键文件

- `lib/common/include/astro_scalar.h` — AstroScalarType 枚举（FP32/FP64）和精度 traits
- `lib/common/include/precision_context.h` — PrecisionContext 单例
- `lib/astro_image_io/include/astro_image_io.h` — `aio_set_precision_mode(int is_fp64)` API
- `lib/astro_image_io/src/aio_api.cpp` — AIO 模块级精度全局变量
- `lib/astro_image_io/include/aio_healpix_io.h` — HISS FP64 读写 API（`aio_hiss_read_tile_signal_f64`, `aio_hiss_query_pixel_f64`）
- `lib/snr_estimator/cpp/include/snr_estimator.h` — SnrControlPoint（`#pragma pack(1)`, sizeof=20）

### 4.3 Python 边界

- **生产层已删除**：所有生产 wrapper/adapter/CLI 已删除（R10 阶段 3）
- **保留 Python**：仅测试/研究脚本，全部带 `NON_PRODUCTION_TOOL_ONLY` 标记
- **无 Python 环境可运行**：orchestrator.exe + DLLs 独立运行

---

## 5. 阶段 8 待办事项（下一个 agent 重点）

### 5.1 阶段 8a: Wiki + README

**Wiki 同步**（控制包 `wiki/` 目录 → GitHub Wiki）:

- 控制包中有 15 个 Wiki 页面需同步到 `AstroCS.wiki/`
- 其中 3 个新增: `PipelineFrame_and_ABI.md`, `Python_Boundary.md`, `SNR.md`, `Stage1_JSON_Config.md`
- 11 个修改: `Acceptance.md`, `ACR_Isolation.md`, `Drizzle_PRECISE.md`, `Execution_Model.md`, `FAST_Research_Status.md`, `Governance.md`, `HISS.md`, `Home.md`, `Numeric_Precision.md`, `Project_Status.md`, `Validation_Strategy.md`
- **本地 Wiki 已克隆**: `AstroCS.wiki/` (最近提交: `0b27344 docs(wiki): Phase1编排器与双精度闭合Wiki基线`)

**Wiki 推送流程**:

```powershell
cd AstroCS.wiki
git pull --rebase
# 复制控制包 wiki/*.md 覆盖本地
git add . && git commit -m "docs(wiki): R10 纠正控制包 Wiki 同步" && git push
```

**Wiki 安全规则**:

- 不删除未知现有页面
- 更新 `_Sidebar.md` 前检查原内容
- 不得自行把 `WAITING_FOR_USER_REVIEW` 改为 `USER_APPROVED`

**README 重写**（按 `control/README_REWRITE_REQUIREMENTS.md`）:

- **根 README.md**: 当前 1575 行（声明自己是 SSOT），需精简为：项目简介、当前状态、唯一 Wiki 链接、构建入口、唯一运行示例。删除"README 是 SSOT"声明。
- **Orchestrator README** (`lib/orchestrator/README.md`): 当前仍引用 REPL、run/run-batch、stage1/stage2 CLI。第一屏必须是 `orchestrator.exe <stage1.json>`，随后只给 Schema、模板、字段、路径解析、校验和错误说明。删除长参数命令、run/run-batch、REPL、Python 正式用法、Stage2。

### 5.2 阶段 8b: 证据收集

**已完成的验证证据**（位于 `run/logs/r10/`）:

- `fp32_snr_fix_verify_20260804.log` (620 KB) — FP32 单帧验证
- `fp64_snr_fix_verify_20260804.log` (596 KB) — FP64 单帧验证
- 两者均显示 285/285 Tile 通过，stage1 exit_code=0

**需生成的证据**（按 `delivery/REQUIRED_STRUCTURE.md`）:

- 合成模块测试日志
- 单帧 FP32/FP64 各 Gate 原始日志（已有，需整理）
- Python 审计 CSV（模板: `templates/PYTHON_AUDIT.csv`）
- 自动 precision trace（模板: `templates/PRECISION_TRACE.jsonl`）
- ABI 报告
- SNR 点核算
- 实现报告 + 科学正确性报告 + 已知问题报告
- 最终自审（按 `delivery/ACCEPTANCE_CHECKLIST.md` 29 项）

### 5.3 阶段 8c: 交付 ZIP

**交付结构要求**（`delivery/REQUIRED_STRUCTURE.md`）:

- 完整最新 Wiki
- main Phase1 必要源码
- Schema 与 JSON 模板
- Git refs
- Python 审计
- 自动 precision trace
- ABI 报告
- SNR 点核算
- 完整文件 SHA256
- 合成测试
- 单帧 FP32/FP64 各 Gate 原始日志
- 实现/科学正确性/已知问题报告
- 最终自审

**source 只包含必要最新源码、构建文件和必要文档**，不包含运行日志、生成图片、编译输出、测试产物、缓存或大型数据。

### 5.4 验收门禁（ACCEPTANCE_CHECKLIST.md 29 项）

关键项：

- [ ] main 工作，无新 Stage1 分支
- [ ] ACR ref 前后一致
- [ ] FAST 和 Stage2 未修改
- [ ] 正式运行只有 `orchestrator.exe <json>`
- [ ] 无 REPL、run、run-batch、复杂科学 CLI
- [ ] FP32 各阶段实际 float32
- [ ] FP64 各阶段实际 float64
- [ ] HISS signal/SNR 双 dtype
- [ ] HISS_VERIFY 遍历全部 Tile
- [ ] SNR 全部点完成原因分类
- [ ] Wiki 与 README 无权威冲突
- [ ] source 无生成产物
- [ ] SHA256 列出全部交付文件
- [ ] 未宣称整个 Phase1 已完成

---

## 6. 根目录清理建议

### 6.1 当前根目录散落文件（未跟踪）

| 文件                                                                  | 类型          | 处理建议                         |
| ------------------------------------------------------------------- | ----------- | ---------------------------- |
| `AstroCS_ACR_Control_Package(2).zip`                                | 旧控制包        | 删除（已合并到工程控制）                 |
| `AstroCS_ACR_Fix_Review/`                                           | ACR 审查目录    | 保留（含源码和证据，未跟踪）               |
| `AstroCS_ACR_Fix_Review_2026-08-03.zip`                             | 旧交付         | 删除                           |
| `AstroCS_ACR_Fix_Review_2026-08-04.zip`                             | 旧交付         | 删除                           |
| `AstroCS_Phase1_JSON_Orchestrator_TrueDualPrecision_Correction.zip` | R10 控制包     | 保留（本地参考）                     |
| `AstroCS_Phase1_Orchestrator_Precision_Closure.zip`                 | 旧交付         | 删除                           |
| `AstroCS_Phase1_Precision_Closure_Delivery_2026-08-03.zip`          | 旧交付（44.5MB） | 删除                           |
| `_new_control_pack/`                                                | ACR 新控制包    | 保留（待合并）                      |
| `astro_image_io.dll`                                                | 编译产物（根目录散落） | 删除（应在 lib/astro_image_io/）   |
| `orchestrator.exe`                                                  | 编译产物（根目录散落） | 删除（应在 lib/orchestrator/cpp/） |
| `orchestrator.legacy.exe.bak`                                       | 旧版备份        | 删除                           |
| `hello_test.exe`                                                    | 测试产物        | 删除                           |
| `wph_test.hiss`                                                     | 测试产物        | 删除                           |
| `git-graph.html`                                                    | 可视化脚本       | 删除或移到 tools/                 |
| `git_graph_horizontal_png (1).py`                                   | 可视化脚本       | 删除或移到 tools/                 |
| `tools/_r10_*.txt` / `*.json`                                       | R10 临时文件    | 删除（已使用完毕）                    |

### 6.2 根目录应保留

- `AGENTS.md` — AI 操作指南（用户已手动精简）
- `README.md` — 项目入口（待重写）
- `HANDOVER.md` — 本文件
- `memory.md` — 项目记忆
- `.gitignore`, `.gitattributes`
- `lib/`, `工程控制/`, `tools/`, `testdata/`, `run/`, `docs/`
- `AstroCS.wiki/` — 本地 Wiki 克隆
- 当前轮次的交付 ZIP（完成后）

### 6.3 源码树生成产物清理

源码树中仍有大量未跟踪的编译产物（.exe, .dll, .o）和测试输出。各模块 `.gitignore` 已覆盖这些模式，但本地文件未清理。阶段 8c 交付前需清理：

- `lib/orchestrator/cpp/tests/*.exe`
- `lib/astro_image_io/tests/*.exe`
- `lib/healpix_db/healpix_drizzle/tests/*.exe`
- `lib/healpix_db/healpix_browser_qt/build/`（整个目录）
- `lib/orchestrator/cpp/logger_*/`（测试临时目录，约 25-30 个）
- `lib/orchestrator/cpp/lib/`（路径解析 bug 产生的嵌套目录）
- `lib/*/logs/`（应写入 `run/logs/<module>/`）
- `lib/*/stdout.txt`, `stderr.txt`, `test_output*`, `compile_*`

**注意**: `lib/orchestrator/cpp/lib/orchestrator/logs/` 这个嵌套路径是 orchestrator.exe 路径解析 bug 的产物，需要修复代码中的日志路径解析（相对路径 vs 绝对路径）。

---

## 7. 关键科学约束（红线，违反即返工）

详细见 GitHub Wiki，以下是核心：

- **HISS/Stage1**: signal=累计通量（不除面积）/ support=面积比[0,255] uint8 / Tile 叶像素数=4^d / 球面重叠用 Girard 定理 / pixfrac∈(0,1] / 自动 NSIDE 上限 2^22
- **PlateSolve**: 向量匹配用 gnomonic 投影 / Y 轴反转 / 候选半径 0.5×FOV / RANSAC+Umeyama SVD / 尺度因子 s 限制±10%
- **性能**: C++ 版本性能不低于 Python / Gaia 客户端 60s TTL 内存缓存 / 星检测串行
- **测试**: 契约不满足必须真正失败，禁止 `ASSERT_TRUE(true)` 软通过

---

## 8. 工具集与提交规则

### 8.1 工具集（已删除 2026-08-04）

`tools/`（astro_toolkit.py / gen_audit_pack.py / README.md / 可视化脚本）已整体删除。
原因为：该工具集是上个 Agent 为减少沙箱确认而做的批处理封装，当前环境已无沙箱审核，且 `lib/` 无任何依赖。
后续 git/编译/文件操作直接用系统命令即可。

**commit 仍建议用 `-F` 从文件读取 message**（避免长 message 触发扫描超时），临时文件用后删除。

### 8.2 提交规则

- **精细化 commit**：完成最小任务后必须 commit 留痕
- **完成一阶段子任务后 push 一次**
- **conventional commits**：`feat/fix/docs/chore/refactor`
- **禁止 `git add -A`**：明确指定文件
- **commit message 用中文**（项目惯例）

### 8.3 环境约束

- **强制使用 PowerShell 7**（WSL 无代理，禁止 WSL 远程推送）
- **GitHub CLI**: `C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe`
- **MSYS2 MinGW64**: `C:\msys64\mingw64\bin`
- **备用 commit 脚本**: `C:\Users\fujia\bin\vq-commit.ps1`

### 8.4 任务执行流程

1. 任何任务开始前必须调用 `iterative-discussion` 技能走确认流程（用户明确给文档的除外）
2. 分析任务依赖关系，先统一公共约定
3. 分配给 Subcoding Agent 执行，最大化安全并行
4. 子任务完成后统一集成与验收
5. 任务完成后用 AskUserQuestion 追问"是否有下一阶段任务/补充/调整"

---

## 9. 已知问题与风险

### 9.1 待修复（非阻断）

1. **orchestrator 日志路径解析 bug**: 运行时在 `cpp/` 目录下创建 `lib/orchestrator/logs/` 嵌套目录，应使用绝对路径或固定到 `run/logs/orchestrator/`
2. **源码树生成产物未清理**: 大量 .exe/.dll/.o 散落在 lib/ 子目录（已 .gitignore，但本地未清理）
3. **根目录散落文件**: 多个旧 ZIP、编译产物、测试文件散落在根目录
4. **data_pipeline 模块重叠**: 与 astro_image_io 的 PipelineFrame 重叠，待 ADR 明确

### 9.2 隔离边界（严禁触碰）

- **ACR**（Astro Compute Runtime）: 在 `feature/astrocompute-runtime` 分支独立开发，当前分支 main 不涉及
- **FAST**: 实验性研究，保持暂停状态
- **Stage2**: 当前禁止修改（`healpix_stack` 模块）

### 9.3 阻断性约束

- **Phase1 未闭合**: 不能宣称"Phase1 已完成"
- **单帧验证 ≠ 全量验证**: 当前仅验证了一张 T4 Red 帧，不代表 710 帧全量回归
- **浏览器性能**: 不能用 CLI 模拟值当真实 GUI 性能

---

## 10. 下一个 Agent 启动指南

### 10.1 必读文件

1. **本文件** (`HANDOVER.md`)
2. `AGENTS.md` — AI 操作指南
3. `run/temp/r10_correction/AstroCS_Phase1_JSON_Orchestrator_TrueDualPrecision_Correction/START_HERE.md`
4. `run/temp/r10_correction/.../control/EXECUTION_ORDER.md`
5. `run/temp/r10_correction/.../delivery/ACCEPTANCE_CHECKLIST.md`

### 10.2 启动步骤

1. 读取本文件了解项目状态
2. 读取控制包 `START_HERE.md` 和 `EXECUTION_ORDER.md`
3. 确认阶段 8 待办事项
4. 与用户确认后开始执行：
   - 8a: Wiki 同步 + README 重写
   - 8b: 证据收集
   - 8c: 交付 ZIP
5. 完成后用 AskUserQuestion 追问是否有下一阶段任务

### 10.3 验证当前状态

```powershell
# 确认 git 状态
git status
git log --oneline -5

# 确认编译产物存在
Test-Path lib\orchestrator\cpp\orchestrator.exe
Test-Path lib\astro_image_io\astro_image_io.dll

# 运行 FP32 验证
.\lib\orchestrator\cpp\orchestrator.exe run\temp\r10_validation\fp32\stage1.json

# 运行 FP64 验证
.\lib\orchestrator\cpp\orchestrator.exe run\temp\r10_validation\fp64\stage1.json

# 检查日志
Get-Content run\logs\r10\fp64_snr_fix_verify_20260804.log -Tail 20
```

---

## 11. 联系与决策

- **用户决策权**: 用户对项目有最终决策权
- **不确定时**: 强制使用 AskUserQuestion 提问工具向用户提问
- **用户授权**: 用户的自主执行授权仅一次对话有效，不得长期默认授权
- **冻结决策**: 已冻结页面变更必须在 commit message 中说明改了哪项用户决策

---

**文档结束**
