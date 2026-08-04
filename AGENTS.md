

# 项目根目录结构

Astro CS Normalization Database/
├── lib/                    # 项目源码 (C++/Python/Go) — 唯一修改代码处
├── 工程控制/                # 权威工程规范、任务清单、证据
├── toolchain.ps1           # 统一工具链唯一入口（环境/编译/运行/审核包）
├── testdata/                # 测试数据 + index.json 索引（只读，禁止写入运行产物）
├── run/                     # 运行目录（校准/Drizzle/HISS/日志/截图统一写此，gitignored）
├── AGENTS.md                # 本文件
├── README.md
├── .gitignore

### 关键原则

- **`lib/` 是项目源码**：所有代码修改仅在此目录
- **`工程控制/` 是工程控制包**：权威工程规范、任务清单、证据
- **`testdata/` 是只读原始数据目录**：禁止向其写入任何运行产物
- **`run/` 是唯一运行输出目录**：所有程序运行时产物（校准后亮场、Drizzle 产物、HISS 文件、日志、截图等）统一写入此目录，禁止散落到 `output/`、`testdata/results/` 等位置
- **根目录只保留临时交付包和必要文件**：不堆积旧交付
- **根目录 ACR 控制包/审核包禁止删除**：`AstroCS_ACR_Control_Package(2).zip`、`AstroCS_ACR_Fix_Review_2026-08-03.zip`、`AstroCS_ACR_Fix_Review_2026-08-04.zip`、`AstroCS_ACR_Fix_Review/`、`_new_control_pack/` 一律保留，任何会话不得删除

### run/ 子目录约定

| 子目录                                  | 用途                          |
| ------------------------------------ | --------------------------- |
| `run/calibrated/<dataset>/<filter>/` | 校准后亮场（01_calibrated.fits 等） |
| `run/drizzle/<dataset>/nside<N>/`    | Drizzle 产物（HISS 文件、统计）      |
| `run/hiss/<dataset>/`                | HISS 输出文件                   |
| `run/logs/<module>/<YYYYMMDD>/`      | 各模块日志                       |
| `run/screenshots/`                   | 调试截图                        |
| `run/temp/`                          | 临时中间文件                      |

---

## 权威文档位置 — GitHub Wiki

**项目唯一权威文档维护在 GitHub Wiki 仓库**，避免本地文档不一致。

- **主仓库**: https://github.com/fujiaze/Astro-CS-Database
- **Wiki 仓库**: https://github.com/fujiaze/Astro-CS-Database.wiki.git

---

## 外部数据目录（本地保留，不入仓库）

**以下目录在 `.gitignore` 中排除，仅供本地使用，AI 不得删除：**

| 目录             | 大小      | 用途                                   |
| -------------- | ------- | ------------------------------------ |
| `GaiaDR3/`     | 41.9 GB | Gaia DR3 星表本体，plate solving 与测光参考星来源 |
| `GaiaDR3SP/`   | 64.7 GB | Gaia DR3 光谱数据库，测光定标用                 |
| `siril-1.4.3/` | 55.2 MB | Siril 天文图像处理软件源码，开发时参考其实现            |

### testdata 目录（只读，详细索引见 `testdata/index.json`）

- **7 个数据集**覆盖 T2/T3/T4 三个望远镜（T1 数据未入库，保留占位符）
- **原始亮场**：`<dataset>/lights/` 下，共 710 个 `.fts` 文件（约 22.3 GB）
- **校准母版**：`T2/T3/T4 calibration files/` 下，共 27 个 `.xisf` 文件
  - 每个望远镜含：1 个 masterBias + 2-3 个 masterDark（按曝光时长）+ 5-6 个 masterFlat（按滤镜）
- **滤镜品牌**：T2/T3 用 Astrodon（LRGB / LRGBHO 50mm，Halpha/OIII 3nm），T4 用 Baader（RGBHaOIII 50mm，Halpha 7nm / OIII 8.5nm）
- **禁止向 testdata/ 写入任何运行产物**（已校准亮场、Drizzle 输出等统一写入 `run/`）

---

## 4. 工程控制包合并规则

新的工程控制包合并到 `工程控制/`：

| 源目录           | 目标                 | 合并策略                                       |
| ------------- | ------------------ | ------------------------------------------ |
| `agent/`      | `工程控制/agent/`      | 覆盖旧文件                                      |
| `checklists/` | `工程控制/checklists/` | 新增不覆盖                                      |
| `contracts/`  | `工程控制/contracts/`  | 新增不覆盖                                      |
| `control/`    | `工程控制/control/`    | 更新 CURRENT_TASK.md, DECISION_REGISTER.md 等 |
| `docs/`       | `工程控制/docs/`       | 新增不覆盖                                      |
| `evidence/`   | `工程控制/evidence/`   | **永不覆盖已有证据，只新增**                           |
| `tasks/`      | `工程控制/tasks/`      | 新增不覆盖                                      |
| `templates/`  | `工程控制/templates/`  | 新增不覆盖                                      |

### 提交规则

- **精细化 commit**：完成最小任务后必须 commit 留痕
- **完成一阶段子任务后 push 一次**

### 环境约束

- **GitHub CLI**：`C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe`
- **MSYS2 MinGW64**：`C:\msys64\mingw64\bin`

### 模块化与日志

- 每个模块独立建立文件夹与开发文档
- 程序中加入详细日志输出，每个模块建立日志目录（统一写入 `run/logs/<module>/`）
- **禁止在单个文件堆叠上千行代码**：职责单一、模块化

---

## 编译环境（已确定 2026-08-04，已验证）

### 工具链版本

| 工具 | 版本 | 说明 |
| --- | --- | --- |
| g++ / gcc（MSYS2 MinGW64） | 16.1.0 (Rev4) | `C:\msys64\mingw64\bin`，C++17 |
| GNU Make | 4.4.1 | 各模块 Makefile 驱动 |
| CMake | 4.3.2 | 仅 healpix_browser_qt |
| Ninja | 1.13.2 | 浏览器 Qt 构建用 |
| nlohmann/json | 已安装 | `C:\msys64\mingw64\include\nlohmann\json.hpp`（pacman: `mingw-w64-x86_64-nlohmann-json`） |
| OpenMP | 已安装 | `C:\msys64\mingw64\lib\libgomp.a`，编译用 `-fopenmp` |
| Git | 2.53.0 | 系统 Git |
| PowerShell | 7.6.3 | 项目强制使用 PowerShell 7，禁用 WSL |
| GitHub CLI | 2.63.2 | `C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe` |
| Python（工具/测试用，规范） | 3.12.2 | 唯一规范 Python：`C:\Users\fujia\AppData\Local\Programs\Python\Python312\python.exe`（`py -3.12`，numpy 1.26.4） |

### PATH 与 Python 坑点（重要，勿重复踩）

- 编译前必须设置：`$env:Path = "C:\msys64\mingw64\bin;$env:Path"`
- **注意**：加上 MSYS2 PATH 后 `python` 会解析到 MSYS2 自带的 Python 3.14.5，**不要用它运行任何 Python 脚本**
- 跑 Python 脚本时一律用规范 Python：`py -3.12` 或 `C:\Users\fujia\AppData\Local\Programs\Python\Python312\python.exe`（`toolchain.ps1` 里已固化，用 `Get-AstroCSPython`）
- orchestrator.exe 为静态链接（`-static`），**运行不依赖 MSYS2 PATH**；DLL 模块由 DllLoader 按相对路径动态加载，重新编译后无需手工复制

### 编译顺序（模块依赖）

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"

# 1. 公共头（header-only，仅跑测试）：lib/common
# 2. 各 DLL 模块（相互独立，可并行；产物已存在时 make 为 no-op）
cd lib\astro_image_io; make                 # astro_image_io.dll
cd lib\calibration; make                    # astro_calibration.dll / cosmetic_corrector.dll
cd lib\dynamic_psf; make                    # dynamic_psf.dll
cd lib\plate_solve\cpp\ipv; make            # ipv_solver.dll
cd lib\star_detector; make                  # star_detector.dll
cd lib\snr_estimator\cpp; make              # snr_estimator.dll
cd lib\photometric_calib\cpp; make          # photometric_calib.dll
cd lib\healpix_db\healpix_drizzle; make     # healpix_drizzle.dll
cd lib\gaia_xpsd_client; make               # gaia_client.dll

# 3. 编排器（最后编译，依赖所有模块头文件；动态加载 DLL 不链接）
cd lib\orchestrator\cpp; make               # orchestrator.exe

# 4. 浏览器（独立 CMake，可选，需 Qt）
cd lib\healpix_db\healpix_browser_qt; cmake -G Ninja -B build; ninja -C build
```

编译标志：各模块 `-O2/-O3 -std=c++17 -fopenmp`，DLL 带 `-static-libgcc -static-libstdc++`，orchestrator 带 `-static -Wl,--stack,33554432`（nanoflann 递归需要 32MB 主线程栈）。

### 已知环境问题（无需重复排查）

1. **healpix_stack（Stage2，冻结禁止修改）**：其 Makefile 仍依赖已归档的 `../healpix_io/healpix_io.dll`，当前 `make` 必然报错。这是已知状态——**不要修复、不要重建**，直接使用现有 `healpix_stack.dll`（2026-08-03 构建）。归档源码/旧 DLL 在 `lib/healpix_db/healpix_io/archive/`，I/O 功能已并入 astro_image_io（`aio_healpix_io`）。
2. **orchestrator 日志路径 bug（非阻断待修）**：运行时会在 `cpp/` 下创建 `lib/orchestrator/logs/` 嵌套目录，应固定到 `run/logs/orchestrator/`。
3. **外部数据目录禁止删除**：`GaiaDR3/`、`GaiaDR3SP/`、`siril-1.4.3/`（gitignored，仅供本地）。
4. **Python 生产层已删除**：正式运行只有 `orchestrator.exe <stage1.json>`，Python 仅保留带 `NON_PRODUCTION_TOOL_ONLY` 标记的测试/研究脚本。

### 环境验证命令

```powershell
.\toolchain.ps1 check   # 一键自检全部工具版本
```

---

## 统一工具链（唯一入口，2026-08-04 确定）

整个项目只用**一条**工具链，入口是根目录 `toolchain.ps1`。任何会话不得自建工具脚本
（旧的 `tools/astro_toolkit.py` 已删除，不再使用），避免不同对话各自搭环境造成冲突。

```powershell
.\toolchain.ps1 check              # 自检工具版本（g++/make/cmake/ninja/git/gh/python/nlohmann/orchestrator）
.\toolchain.ps1 build              # 按依赖顺序编译全部模块（healpix_stack 冻结跳过）
.\toolchain.ps1 run <stage1.json>  # 运行 orchestrator（唯一正式入口）
.\toolchain.ps1 review -Topic <主题> [-Date <YYYYMMDD>]   # 生成审核交付包
```

规范值（已固化在脚本内，勿改）：

- C++ 工具链：MSYS2 MinGW64 `C:\msys64\mingw64\bin`（g++ 16.1.0 / GNU Make 4.4.1 / CMake 4.3.2 / Ninja 1.13.2）
- 规范 Python：`py -3.12` = `C:\Users\fujia\AppData\Local\Programs\Python\Python312\python.exe`（numpy 1.26.4）
- GitHub CLI：`C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe`

## 审核包工作流（控制包 → 执行 → 审核包）

用户把 GPT 工程控制包放到根目录作为启动提示词，执行完成后提交新的审核包。命名规范：

| 类型 | 命名 | 示例 |
| --- | --- | --- |
| 输入控制包（GPT→项目） | `AstroCS_Control_<主题>_<YYYYMMDD>.zip` | `AstroCS_Control_JSONOrchestratorDualPrecision_20260804.zip` |
| 输出审核包（项目→GPT） | `AstroCS_Review_<主题>_<YYYYMMDD>.zip` | `AstroCS_Review_JSONOrchestratorTrueDualPrecision_20260804.zip` |

执行流程：

1. 用户把 GPT 工程包放根目录（内含 `00_START_PROMPT.txt` / `START_HERE.md`）
2. 解压到 `run/temp/<包名>/`，先读 `START_HERE.md` + `control/EXECUTION_ORDER.md` + `delivery/ACCEPTANCE_CHECKLIST.md`
3. 先读 Wiki 与 audit 再执行；最小任务 commit、阶段完成 push 一次
4. 全部完成后 `.\toolchain.ps1 review -Topic <主题>` 生成审核包到根目录（含 Wiki/源码/证据/日志/git refs/SHA256）
5. 回复中给出：审核包路径 + SHA256 + 关键验证结果；不宣称超出包内范围的内容

**禁止删除清单**（根目录）：ACR 控制包与审核包（ZIP 与展开目录）、R10 控制包、
当前审核包、外部数据目录 `GaiaDR3/` `GaiaDR3SP/` `siril-1.4.3/`。
