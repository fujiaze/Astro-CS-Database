# HEALPix DB 遗留代码归档与依赖迁移 spec

**日期**: 2026-07-16
**任务类型**: 工程重构
**触发**: 用户纠正"web和Python实现的是旧的，run_healpix.bat启动的才是最新版本"，要求归档遗留代码、迁移依赖、同步文档

## 1. 背景

`lib/healpix_db/` 下存在多个历史版本的浏览器和已废弃模块，与最新版 `healpix_browser_qt/`（Qt6+OpenGL，run_healpix.bat 启动）并存，导致：
- 目录结构混乱，新旧版本难以区分
- `healpix_browser_cpp/` 顶层与 `archive/healpix_browser_cpp/` 字节级重复
- `healpix_browser_qt` 依赖的 `../healpix_io/` 已于 2026-07-16 原位归档（源码移入 `healpix_io/archive/`），编译路径断链
- 文档严重过时（README.md/memory.md 与实际状态不符）

## 2. 目标

1. 消除 healpix_db 内部的冗余与废弃模块原位存在
2. 修复 healpix_browser_qt 编译断链：迁移依赖至 `lib/astro_image_io/`（aio 模块已完整接管 healpix_io API，含兼容宏）
3. 同步更新所有相关文档，消除 GAP-001/002/003
4. 验证独立仓库本地副本 commit 与文档记录一致

## 3. 非目标

- 不处理 orchestrator 跨模块依赖（healpix_stack/healpix_drizzle 本地存在，无断链）
- 不修改 healpix_browser_qt 的算法/功能代码，仅修改构建配置
- 不重构 astro_image_io 模块本身

## 4. 详细设计

### 4.1 删除 healpix_browser_cpp/ 顶层

- **理由**: 与 `archive/healpix_browser_cpp/` 字节级重复（已验证 browser_main.cpp 30行对比一致）
- **操作**: `Remove-Item -Recurse -Force lib/healpix_db/healpix_browser_cpp/`
- **验证**: 确认 archive/healpix_browser_cpp/ 仍在

### 4.2 归档到 archive/legacy/

创建 `lib/healpix_db/archive/legacy/` 目录，移动以下内容：

| 源路径 | 目标路径 | 理由 |
|---|---|---|
| `healpix_browser/` (PyQt5+vispy Python版) | `archive/legacy/healpix_browser_python/` | 已废弃，依赖 ahpx_io/healpix_stack/healpix_lod 全断链，被 healpix_browser_qt 替代 |
| `healpix_lod/` | `archive/legacy/healpix_lod/` | 已废弃，被 healpix_browser_qt 内存 ud_grade 替代 |
| `tests/test_e2e_integration.py` | `archive/legacy/tests/test_e2e_integration.py` | 依赖 ahpx_io/healpix_stack（ahpx_io 不存在），静默 skip |

在 `archive/legacy/` 下创建 `README.md` 说明归档原因、日期、替代方案。

### 4.3 迁移 healpix_browser_qt 依赖至 astro_image_io

**背景**: `lib/astro_image_io/` 已完整接管 healpix_io API：
- 头文件 `include/aio_healpix_io.h` 导出 9 个 C API（aio_hiss_*、aio_hcsd_*、aio_hio_*）
- 末尾兼容宏让旧源码无缝编译（`#define hiss_write aio_hiss_write` 等）
- DLL 名称从 `healpix_io.dll` 变为 `astro_image_io.dll`

**修改文件**:

#### 4.3.1 `healpix_browser_qt/CMakeLists.txt`
- 第 24 行: `set(HIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../healpix_io)` → `set(AIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../astro_image_io)`
- 第 38 行: `${HIO_DIR}/include` → `${AIO_DIR}/include`
- 第 45 行: `${HIO_DIR}` → `${AIO_DIR}`
- 第 46 行: `healpix_io` → `astro_image_io`
- test_browser_backend 同步修改

#### 4.3.2 `healpix_browser_qt/Makefile`
- 第 8 行: `HIO_DIR = ../healpix_io` → `AIO_DIR = ../../astro_image_io`
- 第 29 行: `-I$(HIO_DIR)/include` → `-I$(AIO_DIR)/include`
- 第 33 行: `-L$(HIO_DIR) -lhealpix_io` → `-L$(AIO_DIR) -lastro_image_io`
- 第 39 行: 同上

#### 4.3.3 `healpix_browser_qt/deploy.ps1`
- 第 70 行: `$healpixIoDll = ".../healpix_io/build/healpix_io.dll"` → `$aioDll = ".../astro_image_io/astro_image_io.dll"`
- 第 71-74 行: 复制逻辑改用新变量名和新 DLL 名

#### 4.3.4 `healpix_browser_qt/memory.md`
- 更新依赖路径描述（healpix_io → astro_image_io）

### 4.4 构建 astro_image_io.dll

在 `lib/astro_image_io/` 下运行构建（build.ps1 或 make），产出 `astro_image_io.dll`。

### 4.5 验证 healpix_browser_qt 可编译

在 `lib/healpix_db/healpix_browser_qt/` 下运行 `cmake --build build`（或 make），确认编译通过。

### 4.6 验证独立仓库 commit

```
cd lib/healpix_db/healpix_stack && git log -1 --oneline
cd lib/healpix_db/healpix_drizzle && git log -1 --oneline
```

对比文档记录：
- healpix_stack: 文档记录 5f6b201
- healpix_drizzle: 文档记录 e7c1d1f

如不一致，更新文档。

### 4.7 文档同步

| 文档 | 更新内容 |
|---|---|
| `lib/healpix_db/README.md` | 重写：以 healpix_browser_qt 为主，标注归档模块，移除废弃模块作为主模块的描述 |
| `lib/healpix_db/memory.md` | 修正第252-255行归档位置描述（healpix_browser_qt/archive/ → healpix_db/archive/）；记录本次归档操作；更新 healpix_browser_qt 依赖路径（healpix_io → astro_image_io） |
| `lib/healpix_db/healpix_browser_qt/memory.md` | 更新依赖路径描述 |
| 根 `memory.md` | 更新 healpix_db 章节：标注 healpix_browser/healpix_lod 已归档；healpix_browser_qt 依赖迁移至 astro_image_io |
| `docs/ARCHITECTURE.md` | 更新模块清单：标注归档状态；更新依赖关系（healpix_browser_qt → astro_image_io） |
| `docs/DESIGN_IMPL_GAP.md` | 关闭 GAP-001（healpix_browser_cpp/web 已归档）、GAP-002（healpix_io 归档+依赖迁移）、GAP-003（healpix_browser 已归档） |

## 5. 验证标准

- [ ] healpix_browser_cpp/ 顶层删除，archive/ 副本保留
- [ ] archive/legacy/ 下有 healpix_browser_python/、healpix_lod/、tests/test_e2e_integration.py + README.md
- [ ] healpix_browser_qt 的 CMakeLists.txt/Makefile/deploy.ps1 指向 astro_image_io
- [ ] astro_image_io.dll 已构建
- [ ] healpix_browser_qt 编译通过
- [ ] healpix_stack/healpix_drizzle commit 与文档一致（或文档已更新）
- [ ] 6 份文档已同步更新
- [ ] git 提交推送成功

## 6. 回滚策略

- 各模块已记录 commit hash 作为回滚点
- 归档操作为移动（非删除），可通过 git revert 回滚
- 构建配置修改可通过 git checkout 回滚

## 7. 风险

- astro_image_io 构建可能因依赖缺失失败（zstd 等）→ 构建前检查依赖
- healpix_browser_qt 编译可能因 API 不完全兼容失败 → 兼容宏已覆盖，风险低
