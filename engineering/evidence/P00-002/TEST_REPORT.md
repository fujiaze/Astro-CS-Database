# TEST_REPORT

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| T1 源码受 git 跟踪 | `git ls-files lib/healpix_db/healpix_drizzle \| Measure-Object -Line` | 10s | 0 | PASS（18 行） | 本报告 §T1 |
| T2 源码受 git 跟踪 | `git ls-files lib/healpix_db/healpix_stack \| Measure-Object -Line` | 10s | 0 | PASS（38 行） | 本报告 §T2 |
| T3 源码受 git 跟踪 | `git ls-files lib/healpix_db/healpix_browser_qt \| Measure-Object -Line` | 10s | 0 | PASS（30 行） | 本报告 §T3 |
| T4 嵌套 .git 已移除 | `Get-ChildItem -Path <3 模块> -Filter .git -Recurse -Force` | 10s | 0 | PASS（0 命中） | 本报告 §T4 |
| T5 来源 commit 可追溯 | 比对 `engineering/evidence/P00-002/SOURCE_RECORD.md` 与 `engineering/evidence/P00-003/SOURCE_RECORD.md` | 10s | 0 | PASS | TASK_REPORT §2 |
| T6 源码哈希完整性 | `Get-FileHash -Algorithm SHA256` 对 86 文件逐个计算并比对 v1.0 抽查项 | 30s | 0 | PASS（drizzle_engine.cpp / hp_drizzle_api.h / wcs_sip.cpp 三项 MATCH） | source_lock.json |
| T7 healpix_drizzle.dll 存在 | `Get-ChildItem build\artifacts\healpix_drizzle.dll` | 5s | 0 | PASS（1,273,688 B，2026-07-24 20:10:17） | build/manifest.json |
| T8 healpix_stack.dll 存在 | `Get-ChildItem build\artifacts\healpix_stack.dll` | 5s | 0 | PASS（1,471,655 B，2026-07-24 20:10:39） | build/manifest.json |
| T9 healpix_browser_qt.exe 存在 | `Get-ChildItem build\artifacts\healpix_browser_qt.exe` | 5s | 0 | PASS（1,528,326 B，2026-07-24 20:07:40） | 本报告 §T9 |
| T10 独立构建状态 | 解析 `build/manifest.json` 中 3 模块的 `status` 字段 | 5s | 0 | PASS（drizzle=OK / stack=OK / browser_qt=OK） | build/manifest.json |
| T11 build manifest 汇总 | 解析 `build/manifest.json` `summary` 字段 | 5s | 0 | PASS（total=12, ok=12, fail=0, skip=0） | build/manifest.json |
| T12 head commit 核对 | `git log -1 --pretty=format:"%H %s"` | 5s | 0 | PASS（7b85ff3f... P01-002: 建立依赖锁定清单） | TASK_REPORT header |
| T13 第三方依赖许可证 | 检查 `lib/healpix_db/healpix_stack/gradient/nanoflann.hpp` 头部 | 5s | 0 | PASS（BSD 许可证，可保留） | P00-003 SOURCE_RECORD.md |
| T14 归档模块不参与构建 | 检查 `lib/healpix_db/archive/` 存在且不含于 build/manifest.json | 5s | 0 | PASS | lib/healpix_db/memory.md |

## Real-data metrics

### 源码跟踪规模
- healpix_drizzle：18 文件（C++ 5 对 + Python 2 + Makefile/README/memory/.gitignore + 2 测试）
- healpix_stack：38 文件（根 21 + gradient/14 + tests/3）
- healpix_browser_qt：30 文件（app/5 + core/8 + widgets/4 + tests/5 + include/1 + 顶层 7）
- 合计 86 文件受 git 跟踪

### 构建产物
| 产物 | 大小 (B) | SHA-256（来自 build/manifest.json） | 构建耗时 |
|---|---:|---|---:|
| healpix_drizzle.dll | 1,273,688 | 54DE6D78AEE963E87B4BA20D8914AB652A3E6D11A47AFC23B0BF1AA9D5A57924 | 9.08s |
| healpix_stack.dll | 1,471,655 | F99A42D5B62897D307378035D03C57612BC42BB5880E6B638874444E793C09D3 | 22.60s |
| healpix_browser_qt.exe | 1,528,326 | （deploy.ps1 部署，未在 manifest 内单独计算） | — |
| a.exe（cmake 内部测试） | 127,872 | D667664DDB718D73742A86D1A4BBFE566FFAC6D6001584405C7CE18910FFF2FF | 0.11s |

### 源码 manifest 摘要
- healpix_drizzle manifest_sha256 = `f80cd3e56f988b8076e350de0a1ffe9012a0244e96e180ec0ea8eb4a7a1d9056`
- healpix_stack manifest_sha256 = `10610a6ff79c26d65a7f520a77e452e3c13fea4543d16006f8ff77bb7d6eb616`
- healpix_browser_qt manifest_sha256 = `770393989f4588447112b36700af6e6d60dc217d2331bcd536ad993c6adc3c07`

### 测试明细
- **T1/T2/T3**：`git ls-files <module> | Measure-Object -Line` 输出分别为 18 / 38 / 30，全部 MATCH 预期。
- **T4**：`Get-ChildItem -Path "lib/healpix_db/healpix_drizzle","lib/healpix_db/healpix_stack","lib/healpix_db/healpix_browser_qt" -Filter ".git" -Recurse -Force -ErrorAction SilentlyContinue` 输出空，DONE。三个模块均无嵌套 .git 目录。
- **T6**：86 个文件全部计算 SHA-256 并写入 `source_lock.json`。其中 3 个 healpix_drizzle 文件哈希与 v1.0 SOURCE_RECORD.md 记录逐字符比对一致：
  - `drizzle_engine.cpp` = `c00503bcbfa07fe3cf21380010354a04010d844108d4d9bae68d1cd26e2b700f` MATCH
  - `hp_drizzle_api.h` = `cf4ed666d2f049239386aa94e9f8ffd3d6f03eac445778d15d6018a060d0198d` MATCH
  - `wcs_sip.cpp` = `a6d2d001e7967bc722b41011c0db1f2d3240b16e93460650c24f3c4a73b4ff7f` MATCH
- **T7-T11**：构建验证全部基于 `build/manifest.json`（clean_build=true，2026-07-24T20:11:20+08:00 生成），三个目标模块 status=OK，全局 summary total=12 ok=12 fail=0 skip=0。
- **T13**：nanoflann.hpp 头部注明 BSD 许可证（Jose Luis Blanco et al.），允许保留与重分发。
- **T14**：`lib/healpix_db/archive/` 含 `healpix_browser_cpp/`、`healpix_browser_web/`、`legacy/healpix_browser_python/`、`legacy/healpix_lod/`，均未出现在 build/manifest.json 的 build_order 与 results 中。

## Failures and investigation

无失败项。14 项测试全部 PASS。

### 复现命令（PowerShell 7）

```powershell
# T1-T3 跟踪数
git ls-files lib/healpix_db/healpix_drizzle  | Measure-Object -Line
git ls-files lib/healpix_db/healpix_stack    | Measure-Object -Line
git ls-files lib/healpix_db/healpix_browser_qt | Measure-Object -Line

# T4 嵌套 .git
Get-ChildItem -Path "lib/healpix_db/healpix_drizzle","lib/healpix_db/healpix_stack","lib/healpix_db/healpix_browser_qt" -Filter ".git" -Recurse -Force -ErrorAction SilentlyContinue

# T6 86 文件 SHA-256
git ls-files lib/healpix_db/healpix_drizzle lib/healpix_db/healpix_stack lib/healpix_db/healpix_browser_qt |
  ForEach-Object { "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -Path $_).Hash.ToLower(), $_ }

# T7-T9 产物存在性
Get-ChildItem build\artifacts\healpix_drizzle.dll,build\artifacts\healpix_stack.dll,build\artifacts\healpix_browser_qt.exe |
  Select-Object Name,Length,LastWriteTime

# T10-T11 manifest 解析
Get-Content build\manifest.json -Raw | ConvertFrom-Json | Select-Object -ExpandProperty results |
  Where-Object { $_.module -in @("healpix_drizzle","healpix_stack","healpix_browser_qt") }
Get-Content build\manifest.json -Raw | ConvertFrom-Json | Select-Object -ExpandProperty summary

# T12 head commit
git log -1 --pretty=format:"%H %s"
```

## VERDICT: PASS
