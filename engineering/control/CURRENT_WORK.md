# 当前唯一工作

## Task ID

`P00-002` — 恢复并固定 healpix_drizzle 源码

## 目标

将 `lib/healpix_db/healpix_drizzle/` 源码纳入主仓库版本控制，并记录来源 commit、与 orchestrator API 的匹配版本、许可证。

## 背景（P00-001 预检发现）

- 本地 `lib/healpix_db/healpix_drizzle/` 存在完整源码（drizzle_engine.cpp/.h、hp_drizzle_api.cpp/.h、fits_reader.cpp/.h、poly_clip.cpp/.h、wcs_sip.cpp/.h、healpix_drizzle.py、pipeline_adapter.py、Makefile、tests/）
- 该目录含独立 `.git`，主仓库 `git ls-files` 返回 0 文件
- memory.md 记录远端仓库：https://github.com/fujiaze/Healpix-Drizzle-Cpp (commit ecf8758)
- .gitignore 未明确忽略该目录

## 入口条件

- P00-001 DONE ✓
- 本地 healpix_drizzle 源码存在 ✓

## 允许修改

- `lib/healpix_db/healpix_drizzle/**`（删除 .git 后纳入主仓库）
- `engineering/evidence/P00-002/**`
- `engineering/control/**`
- `.gitignore`（如需）

## 禁止修改

- healpix_drizzle 的源码内容（只纳入，不改代码）
- 其他 `lib/**` 模块
- `docs/**`
- 构建脚本与算法配置（Makefile 内容不改）

## 执行计划

1. 记录 healpix_drizzle 独立仓库的当前 commit 和远端 URL
2. 记录源码文件清单和哈希
3. 删除 `lib/healpix_db/healpix_drizzle/.git`（解除嵌套仓库）
4. 清理编译产物（*.dll、*.exe、__pycache__、.pytest_cache）
5. `git add lib/healpix_db/healpix_drizzle/` 纳入主仓库
6. 验证 `git ls-files` 返回非零
7. 生成 TASK_REPORT、TEST_REPORT、EVIDENCE_INDEX
8. 独立复核
9. 推进到 P00-003

## 完成标准

- healpix_drizzle 源码受主仓库跟踪
- 来源 commit 和远端 URL 已记录
- 编译产物未入库
- 源码内容未被修改
