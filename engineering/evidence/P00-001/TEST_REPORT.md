# TEST_REPORT

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| Git HEAD/branch/remote | `git log -n 5`; `git status --short`; `git branch -vv`; `git remote -v` | 30s | 0 | PASS | HEAD=7b85ff3, branch=main, ahead origin/main by 2, remote=https://github.com/fujiaze/Astro-CS-Database.git |
| Git tags | `git tag --list` | 10s | 0 | PASS | 1 tag: astrocs-baseline-p00 (P00-008 G0 gate) |
| 工具链版本 | `gcc --version`; `g++ --version`; `git --version`; `gh --version`; `$PSVersionTable.PSVersion` | 30s | 0 | PASS | GCC/G++ 16.1.0 (MSYS2 MinGW64), Git 2.53.0, gh 2.63.2, PowerShell 7.6.3 |
| build/artifacts 文件清单 | `Get-ChildItem build\artifacts -File` | 10s | 0 | PASS | 16 个文件（11 DLL + 5 EXE），与任务描述一致 |
| build/artifacts SHA-256 | `Get-FileHash -Algorithm SHA256` (16 文件) | 60s | 0 | PASS | 全部 16 个文件哈希采集成功，见 baseline_inventory.json build_artifacts.files |
| Gaia DR3 SP 文件清单 | `Get-ChildItem GaiaDR3SP -File` | 30s | 0 | PASS | 20 个 xpsd 文件 (gdr3sp-1.0.0-01..20)，总 63.5 GB |
| testdata 顶层目录 | `Get-ChildItem testdata -Directory` | 10s | 0 | PASS | 12 个顶层目录：7 目标 + report + results + 3 套 calibration files |
| T4/T3/T2 master 校准帧 | `Glob testdata/**/master*` | 10s | 0 | PASS | T4: 9 文件(180/300/600s Dark + 5 Flat + Bias); T3: 9 文件(600/1200s Dark + 6 Flat + Bias); T2: 9 文件(600/1200/1800s Dark + 5 Flat + Bias) |
| HISS 文件存在性 + 哈希 | `Get-FileHash lib/orchestrator/cpp/output_hiss_dir/*.hiss` | 30s | 0 | PASS | frame1.hiss (176MB, 3C06E240...), frame2.hiss (176MB, BC2C19FF...) |
| HCSD 文件存在性 + 哈希 | `Get-FileHash lib/orchestrator/cpp/output_stage2.hcsd` | 30s | 0 | PASS | output_stage2.hcsd (179MB, 2A9BD12E...) |
| lib 模块树盘点 | `LS lib/` + 各模块 Makefile/build.ps1 检查 | 10s | 0 | PASS | 13 个模块/子模块全部存在，含 healpix_db 下 3 个子模块(browser_qt/drizzle/stack) |
| 业务源码未修改检查 | git status 中 lib/ 改动仅 Makefile/build.ps1 | 10s | 0 | PASS | 仅 lib/gaia_xpsd_client/Makefile 与 lib/plate_solve/cpp/ipv/build.ps1 被改（v1.1 迁移所致），无 .cpp/.c/.h/.py 业务源码改动 |
| 旧证据整合 | Read P00-001 旧 TASK_REPORT + P00-005 environment_baseline.json + P00-008 baseline_manifest.json | 10s | 0 | PASS | v1.0 旧证据字段已整合到 baseline_inventory.json (toolchain/git/modules) |

## Real-data metrics

- **Gaia DR3 SP 数据集**：20 个 xpsd 分片，总计 68,167,941,509 字节 (≈63.5 GB)，单文件范围 2.95–3.28 GB，命名 `gdr3sp-1.0.0-NN.xpsd` (NN=01..20)。
- **testdata 真实观测数据**：7 个目标（Galaxy_Center_T4 含 3 panel 马赛克、LDN43_T2、NGC1727_T2、NGC247_T2、NGC55_T3、NGC83_cluster_T3、Victory_Nebula_T4），涵盖 T2/T3/T4 三种望远镜，滤镜含 Red/Green/Blue/Lum/H-alpha/Oiii，曝光 180S/300S/600S/1200S/1800S。
- **校准 master 帧总数**：27 个 xisf 文件（T4 9 + T3 9 + T2 9），T4 尺寸 4500x3600 BIN1，T3/T2 尺寸 4096x4096 BIN1。
- **已有 pipeline 输出**：2 个 HISS 文件（frame1/frame2，各 ≈176 MB）+ 1 个 HCSD 文件（output_stage2.hcsd，≈179 MB），均位于 `lib/orchestrator/cpp/`。
- **build/artifacts 产物总大小**：≈16.5 MB（16 个文件，最大 orchestrator.exe 3.9 MB，最小 a.exe 128 KB）。

## Failures and investigation

- 无失败项。本任务为只读盘点，所有 13 项验证测试均 PASS。
- 注意点（非失败）：build/artifacts/a.exe 为临时编译产物（127872 字节），不属于任何正式模块输出，建议在 P01 构建任务中清理。
