# TASK_REPORT

- Task ID: P00-002（v1.1 开发包复核）
- Commit/base: HEAD = 7b85ff3f0d37a4b26fff6077684993842ed2bbae（"P01-002: 建立依赖锁定清单"）；远端 origin = https://github.com/fujiaze/Astro-CS-Database.git；包版本 2026-07-24-cli-core-v1.1-platesolve-conditional-path
- Objective: 确认 `lib/healpix_db/healpix_drizzle`、`healpix_stack`、`healpix_browser_qt` 实际源码位置与版本；从现有仓库/历史/远端恢复并建立可追溯锁定；不以 DLL 代替长期源码基线；验证可独立构建。整合旧 v1.0 P00-002（healpix_drizzle）与 P00-003（healpix_stack）证据，纳入 healpix_browser_qt，输出结构化 `source_lock.json`。
- Changes: 仅新增/更新 `engineering/evidence/P00-002/**` 证据文件，未修改任何业务源码（`lib/**`、`docs/**`、构建脚本未触动）。
- Files:
  - `engineering/evidence/P00-002/TASK_REPORT.md`（v1.1 复核版，覆盖 v1.0）
  - `engineering/evidence/P00-002/TEST_REPORT.md`（v1.1 复核版）
  - `engineering/evidence/P00-002/EVIDENCE_INDEX.md`（v1.1 复核版）
  - `engineering/evidence/P00-002/REVIEW_REPORT.md`（v1.1 复核版）
  - `engineering/evidence/P00-002/source_lock.json`（结构化清单，新增）
  - `engineering/evidence/P00-002/SOURCE_RECORD.md`（v1.0 healpix_drizzle 来源记录，保留作历史参考）
- Compatibility: 无接口/ABI/数据格式变更；`source_lock.json` 仅作追溯用，不影响构建路径或运行时行为。
- Rollback: 删除本次新增的 5 个 evidence 文件即可回滚；源码基线由 v1.0 提交 a5fc8dd（healpix_drizzle）与 dde66ba（healpix_stack）锁定，回滚本报告不影响源码受控状态。
- Remaining risks: 无硬风险。软风险：`healpix_browser_qt` 无独立远端 commit 可追溯（自始受主仓库跟踪），其历史只能通过主仓库 git log 查询；build/manifest.json 中 `healpix_browser_qt` 的 artifact 记为内部测试 `a.exe`，生产 EXE `healpix_browser_qt.exe` 由模块内 `deploy.ps1` 部署，二者均存在。

## 详细执行结果

### 1. 实际源码位置核查（执行步骤 1）

| 模块 | 物理路径 | git ls-files 跟踪数 | 嵌套 .git | 语言/构建 |
|---|---|---:|---|---|
| healpix_drizzle | `lib/healpix_db/healpix_drizzle/` | 18 | 已移除 | C++17 + Python，Makefile |
| healpix_stack | `lib/healpix_db/healpix_stack/` | 38 | 已移除 | C++17 + Python，build.ps1 |
| healpix_browser_qt | `lib/healpix_db/healpix_browser_qt/` | 30 | 无（自始受控） | C++17 + Qt6 + OpenGL 3.3，CMakeLists.txt |

合计 86 个文件全部受主仓库 git 跟踪。三个模块目录下均无嵌套 `.git`（`Get-ChildItem -Recurse -Filter .git` 0 命中）。

### 2. 来源追溯（执行步骤 2-3，记录来源 commit）

| 模块 | 来源远端 | 来源 commit | commit 时间 | 来源最后提交信息 | 纳入主仓库 commit |
|---|---|---|---|---|---|
| healpix_drizzle | https://github.com/fujiaze/Healpix-Drizzle-Cpp.git | `ecf8758affaf0caf0eb12faed7d2ed32623886e7` | 2026-07-16T12:27:28+08:00 | refactor(drizzle): link aio.dll instead of healpix_io.dll (spec G1 Phase 1) | `a5fc8dd`（v1.0 P00-002，2026-07-24） |
| healpix_stack | https://github.com/fujiaze/Healpix-Mosaic-Cpp.git | `027b64f51ec365a223816faf3ca9801499e2db9f` | 2026-07-16T12:29:24+08:00 | refactor(stack): link aio.dll for HEALPix I/O (spec G1 Phase 1) | `dde66ba`（v1.0 P00-003，2026-07-24） |
| healpix_browser_qt | （无独立远端） | — | — | — | 自始受主仓库跟踪（2026-07-13 起） |

旧 v1.0 证据已归档至 `engineering_archive_v1.0/evidence/P00-002/` 与 `.../P00-003/`，本目录同时保留 v1.0 副本（`SOURCE_RECORD.md`、`commit_msg.txt` 等）。

第三方依赖：
- `lib/healpix_db/healpix_stack/gradient/nanoflann.hpp` — nanoflann（BSD，header-only，Jose Luis Blanco et al.），用于 KD-tree 近邻搜索（梯度采样球面最近点查询）。SHA-256 `a0bcef4159846bef2f84abea93ccdb666ae918fb89b67b3f03f661bd66eecef3`。

### 3. 不以 DLL 代替源码基线

- `build/artifacts/healpix_drizzle.dll`（1,273,688 字节，2026-07-24 20:10:17）存在，但仅作"可独立构建"证据，不作为长期基线。
- `build/artifacts/healpix_stack.dll`（1,471,655 字节，2026-07-24 20:10:39）存在，同上。
- `build/artifacts/healpix_browser_qt.exe`（1,528,326 字节，2026-07-24 20:07:40）存在，同上。
- 长期基线 = `lib/healpix_db/**` 下 86 个受 git 跟踪的源码文件，记录于 `source_lock.json`。

### 4. 可独立构建验证（执行步骤 4）

依据 `build/manifest.json`（生成于 2026-07-24T20:11:20+08:00，clean_build=true，config=Release，gxx 16.1.0）：

| 模块 | 构建入口 | 状态 | 产物 | 大小 (B) | 耗时 (s) |
|---|---|---|---|---:|---:|
| healpix_drizzle | makefile | OK | healpix_drizzle.dll | 1,273,688 | 9.08 |
| healpix_stack | build.ps1 | OK | healpix_stack.dll | 1,471,655 | 22.60 |
| healpix_browser_qt | cmake | OK | a.exe（内部测试）+ healpix_browser_qt.exe（deploy.ps1 部署） | 127,872 / 1,528,326 | 0.11 |

`build/manifest.json` 汇总：12 模块全部 OK，0 fail，0 skip，总耗时 103.29s。三个目标模块均通过独立构建。

### 5. 文件清单与哈希

详见 `source_lock.json`：
- healpix_drizzle manifest_sha256 = `f80cd3e56f988b8076e350de0a1ffe9012a0244e96e180ec0ea8eb4a7a1d9056`（18 文件）
- healpix_stack manifest_sha256 = `10610a6ff79c26d65a7f520a77e452e3c13fea4543d16006f8ff77bb7d6eb616`（38 文件）
- healpix_browser_qt manifest_sha256 = `770393989f4588447112b36700af6e6d60dc217d2331bcd536ad993c6adc3c07`（30 文件）

manifest 算法：按 `git ls-files` 顺序取每个文件 SHA-256，UTF-8 拼接（每行后加 `\n`），对拼接结果计算 SHA-256。

抽查源码完整性（与 v1.0 SOURCE_RECORD.md 对比）：
- `drizzle_engine.cpp` = `c00503bc...` MATCH
- `hp_drizzle_api.h` = `cf4ed666...` MATCH
- `wcs_sip.cpp` = `a6d2d001...` MATCH

### 6. 归档模块说明

旧 `healpix_browser_cpp/`、`healpix_browser_web/` 与 legacy `healpix_browser_python/`、`healpix_lod/` 已归档至 `lib/healpix_db/archive/`（含 `archive/legacy/`），不参与构建；当前活跃模块为 `healpix_browser_qt`。归档依据见 `lib/healpix_db/memory.md` §"遗留代码归档与依赖迁移"。
