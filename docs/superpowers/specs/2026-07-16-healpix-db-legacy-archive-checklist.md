# HEALPix DB 遗留代码归档与依赖迁移 checklist

**spec**: docs/superpowers/specs/2026-07-16-healpix-db-legacy-archive.md
**日期**: 2026-07-16

## Phase 0: 准备与回滚点

- [ ] 0.1 记录 healpix_db 当前 commit hash（git rev-parse HEAD）
- [ ] 0.2 记录 healpix_browser_qt 当前 commit hash
- [ ] 0.3 记录 astro_image_io 当前 commit hash
- [ ] 0.4 记录 healpix_stack/healpix_drizzle 当前 commit hash（验证用）

## Phase 1: 删除冗余 + 归档遗留代码

- [ ] 1.1 删除 `lib/healpix_db/healpix_browser_cpp/` 顶层（archive 已有副本）
- [ ] 1.2 创建 `lib/healpix_db/archive/legacy/` 目录
- [ ] 1.3 移动 `healpix_browser/` → `archive/legacy/healpix_browser_python/`
- [ ] 1.4 移动 `healpix_lod/` → `archive/legacy/healpix_lod/`
- [ ] 1.5 移动 `tests/test_e2e_integration.py` → `archive/legacy/tests/test_e2e_integration.py`
- [ ] 1.6 创建 `archive/legacy/README.md`（归档原因/日期/替代方案）
- [ ] 1.7 验证：archive/healpix_browser_cpp/ 仍在；archive/legacy/ 结构正确

## Phase 2: 迁移 healpix_browser_qt 依赖至 astro_image_io

- [ ] 2.1 读取 `healpix_browser_qt/CMakeLists.txt` 确认当前 HIO_DIR 用法
- [ ] 2.2 修改 CMakeLists.txt：HIO_DIR → AIO_DIR，路径指向 astro_image_io
- [ ] 2.3 读取 `healpix_browser_qt/Makefile` 确认当前 HIO_DIR 用法
- [ ] 2.4 修改 Makefile：HIO_DIR → AIO_DIR，路径指向 astro_image_io
- [ ] 2.5 读取 `healpix_browser_qt/deploy.ps1` 确认当前 healpix_io 引用
- [ ] 2.6 修改 deploy.ps1：healpix_io.dll → astro_image_io.dll，路径更新
- [ ] 2.7 验证：三处文件无遗漏的旧路径引用

## Phase 3: 构建 astro_image_io.dll

- [ ] 3.1 检查 astro_image_io 构建依赖（zstd 等）
- [ ] 3.2 运行 build.ps1 或 make 构建 astro_image_io.dll
- [ ] 3.3 验证：astro_image_io.dll 已生成在 lib/astro_image_io/

## Phase 4: 验证 healpix_browser_qt 编译

- [ ] 4.1 在 healpix_browser_qt/ 下运行 cmake --build build（或 make）
- [ ] 4.2 确认编译通过，无错误
- [ ] 4.3 如编译失败，排查原因并修复

## Phase 5: 验证独立仓库 commit

- [ ] 5.1 git log -1 healpix_stack，对比文档记录 5f6b201
- [ ] 5.2 git log -1 healpix_drizzle，对比文档记录 e7c1d1f
- [ ] 5.3 如不一致，更新文档记录

## Phase 6: 文档同步

- [ ] 6.1 重写 `lib/healpix_db/README.md`（以 healpix_browser_qt 为主）
- [ ] 6.2 更新 `lib/healpix_db/memory.md`（修正归档位置+记录本次操作+依赖路径）
- [ ] 6.3 更新 `lib/healpix_db/healpix_browser_qt/memory.md`（依赖路径）
- [ ] 6.4 更新根 `memory.md`（healpix_db 章节标注归档+依赖迁移）
- [ ] 6.5 更新 `docs/ARCHITECTURE.md`（模块清单+依赖关系）
- [ ] 6.6 更新 `docs/DESIGN_IMPL_GAP.md`（关闭 GAP-001/002/003）

## Phase 7: git 提交推送

- [ ] 7.1 healpix_db 仓库 git add + commit
- [ ] 7.2 healpix_browser_qt 仓库 git add + commit（如独立仓库）
- [ ] 7.3 astro_image_io 仓库 git add + commit（如有变更）
- [ ] 7.4 推送到 GitHub 远端
- [ ] 7.5 验证推送成功

## Phase 8: 最终验证

- [ ] 8.1 目录结构核对：healpix_browser_cpp/ 顶层已删；archive/legacy/ 结构正确
- [ ] 8.2 编译验证：healpix_browser_qt 可编译
- [ ] 8.3 文档核对：6 份文档已更新，GAP-001/002/003 已关闭
- [ ] 8.4 git 历史核对：提交记录清洁
