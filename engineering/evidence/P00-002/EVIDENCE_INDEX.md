# EVIDENCE_INDEX

| Evidence | Description | SHA-256 |
|---|---|---|
| source_lock.json | 结构化锁定清单（v1.1）：3 模块 × 86 文件逐文件 SHA-256 + 3 个 manifest_sha256 + 来源 commit/远端 + 构建产物 SHA-256（来自 build/manifest.json）+ 第三方依赖 + 归档模块清单 + verification_summary | 6b27b79f9e688299cd81020b7c556330854c5136ecd252a4e366eb666efc4b64 |
| TASK_REPORT.md | v1.1 任务报告：Task ID/Commit/Objective/Changes/Files/Compatibility/Rollback/Remaining risks + 6 节详细执行结果（源码位置/来源追溯/DLL 不代基线/独立构建/哈希/归档） | 689b1fd2312bd2b8e7be534a652833d18d891b402a0b2245095b1732abaf982f |
| TEST_REPORT.md | v1.1 测试报告：14 项验证测试表（git 跟踪/嵌套 .git/来源 commit/源码哈希/产物存在/构建状态/manifest 汇总/head commit/第三方许可证/归档隔离）+ Real-data metrics + 复现命令 | 3d0f26825c24856c81c73f789f1b1162ae5d2083badeb0e01b570c6449b30313 |
| REVIEW_REPORT.md | v1.1 独立复核报告：Reviewer mode/Diff reviewed/Tests rerun/Contract findings/Scientific regression/Risks + Scope/Acceptance/Test/Evidence/Compatibility review，VERDICT: PASS | 6f4ec29c93f97b1e3dd2ca5ea31d63c81d3ac141e010c9855b72fb8b6bd6c090 |
| EVIDENCE_INDEX.md | 本证据索引（self-referential，自身 SHA-256 不列出以避免自引用循环） | (self) |
| SOURCE_RECORD.md | v1.0 healpix_drizzle 来源记录（远端/commit/文件清单/清理清单），保留作历史参考 | 26fb48fb9ca53615373c4315c85db5c2e2c5a9079706bcf75bc0c715a060cf7b |
| commit_msg.txt | v1.0 P00-002 提交信息（"P00-002: 恢复并固定 healpix_drizzle 源码"），保留作历史参考 | f3dd11206caa26f301bb2a95e196fa0ee3e764566cb5639b9526f454630c1607 |

## 关键源证据引用

### 三个模块的源码 manifest（来自 source_lock.json）

| 模块 | 路径 | 跟踪文件数 | manifest_sha256 |
|---|---|---:|---|
| healpix_drizzle | `lib/healpix_db/healpix_drizzle/` | 18 | f80cd3e56f988b8076e350de0a1ffe9012a0244e96e180ec0ea8eb4a7a1d9056 |
| healpix_stack | `lib/healpix_db/healpix_stack/` | 38 | 10610a6ff79c26d65a7f520a77e452e3c13fea4543d16006f8ff77bb7d6eb616 |
| healpix_browser_qt | `lib/healpix_db/healpix_browser_qt/` | 30 | 770393989f4588447112b36700af6e6d60dc217d2331bcd536ad993c6adc3c07 |

manifest 算法：按 `git ls-files` 顺序取每个文件 SHA-256，UTF-8 拼接（每行后加 `\n`），对拼接结果计算 SHA-256。

### 来源 commit（healpix_drizzle / healpix_stack 有独立远端）

| 模块 | 远端 | 来源 commit | commit 时间 | 纳入主仓库 commit |
|---|---|---|---|---|
| healpix_drizzle | https://github.com/fujiaze/Healpix-Drizzle-Cpp.git | ecf8758affaf0caf0eb12faed7d2ed32623886e7 | 2026-07-16T12:27:28+08:00 | a5fc8dd（v1.0 P00-002） |
| healpix_stack | https://github.com/fujiaze/Healpix-Mosaic-Cpp.git | 027b64f51ec365a223816faf3ca9801499e2db9f | 2026-07-16T12:29:24+08:00 | dde66ba（v1.0 P00-003） |
| healpix_browser_qt | （无独立远端，自始受主仓库跟踪） | — | — | — |

### 构建产物（来自 build/manifest.json，clean_build=true，2026-07-24T20:11:20+08:00）

| 产物 | 大小 (B) | SHA-256 | 构建入口 | 状态 |
|---|---:|---|---|---|
| healpix_drizzle.dll | 1,273,688 | 54DE6D78AEE963E87B4BA20D8914AB652A3E6D11A47AFC23B0BF1AA9D5A57924 | makefile | OK |
| healpix_stack.dll | 1,471,655 | F99A42D5B62897D307378035D03C57612BC42BB5880E6B638874444E793C09D3 | build.ps1 | OK |
| healpix_browser_qt.exe | 1,528,326 | （deploy.ps1 部署，未在 manifest 单独计算；磁盘存在性已验证） | cmake + deploy.ps1 | OK |
| a.exe（cmake 内部测试） | 127,872 | D667664DDB718D73742A86D1A4BBFE566FFAC6D6001584405C7CE18910FFF2FF | cmake | OK |

build/manifest.json 全局 summary：total=12, ok=12, fail=0, skip=0, total_build_time_sec=103.29。

### 第三方依赖

| 依赖 | 版本 | 许可证 | 路径 | SHA-256 |
|---|---|---|---|---|
| nanoflann | master (header-only) | BSD | lib/healpix_db/healpix_stack/gradient/nanoflann.hpp | a0bcef4159846bef2f84abea93ccdb666ae918fb89b67b3f03f661bd66eecef3 |

## 旧证据来源（v1.0，已整合到本任务交付物）

| Source | Description | 引用方式 |
|---|---|---|
| engineering/evidence/P00-002/SOURCE_RECORD.md | v1.0 healpix_drizzle 来源记录（远端/commit/文件清单/清理清单） | source_lock.json `modules[healpix_drizzle].origin` 与 `source_intact_evidence` 字段整合 |
| engineering/evidence/P00-002/TASK_REPORT.md（v1.0） | v1.0 任务执行报告（删除嵌套 .git + 清理编译产物 + git add 18 文件） | 本任务 TASK_REPORT.md §2 引用，已被 v1.1 版本覆盖 |
| engineering/evidence/P00-002/TEST_REPORT.md（v1.0） | v1.0 5 项验证（哈希/git 跟踪/编译产物排除/.gitignore/来源记录） | 本任务 TEST_REPORT.md T1-T14 扩展覆盖 |
| engineering/evidence/P00-003/SOURCE_RECORD.md | v1.0 healpix_stack 来源记录（远端/commit/第三方依赖/清理清单） | source_lock.json `modules[healpix_stack].origin` 与 `third_party_deps` 字段整合 |
| engineering/evidence/P00-003/TASK_REPORT.md（v1.0） | v1.0 任务执行报告（删除嵌套 .git + 清理编译产物 + git add 38 文件） | 本任务 TASK_REPORT.md §2 引用 |
| engineering_archive_v1.0/evidence/P00-002/ | v1.0 P00-002 证据归档副本 | 回滚备份来源 |
| engineering_archive_v1.0/evidence/P00-003/ | v1.0 P00-003 证据归档副本 | 回滚备份来源 |

## 归档模块（不参与构建，仅历史保留）

| 模块 | 归档路径 | 归档依据 |
|---|---|---|
| healpix_browser_cpp | `lib/healpix_db/archive/healpix_browser_cpp/` | HTTP+base64 通讯开销大，由 healpix_browser_qt core/ 替代 |
| healpix_browser_web | `lib/healpix_db/archive/healpix_browser_web/` | 与 C++ HTTP 后端配套，随 healpix_browser_cpp 一并归档 |
| healpix_browser_python（legacy） | `lib/healpix_db/archive/legacy/healpix_browser_python/` | PyQt5+vispy 依赖重，由 healpix_browser_qt 替代 |
| healpix_lod（legacy） | `lib/healpix_db/archive/legacy/healpix_lod/` | LOD 应为内存数据结构，由 healpix_browser_qt ud_grade 动态生成 |
