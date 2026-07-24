# astro_image_io - 模块开发memory

## 模块职责
统一天文图像I/O层 + Pipeline管线引擎，提供FITS/XISF图像读写、命名块容器模型与PipelineFrame+PipelineEngine管线框架，作为所有C++模块的底层依赖。

## 当前版本
- 版本号：v1.0 C++原生实现
- 最新commit：a33d167
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Astro-Image-IO-C
- 默认分支：master

## 依赖列表
- C++17
- OpenMP（多线程并行）
- 无外部库（零依赖，纯C++原生实现）

## 关键决策记录
- **C++原生FITS/XISF解析**：零外部依赖，避免cfitsio等库引入，自行实现FITS关键字解析、BZERO/BSCALE处理与XISF格式解析
- **命名块容器模型**：PipelineFrame采用"命名块"数据结构（add_block/get_block_data/kv_set/kv_get），支持像素块、星表块、KV元数据块统一管理，避免跨模块数据格式耦合
- **PipelineFrame+PipelineEngine架构**：管线引擎以PipelineStageHandler回调为执行单元，通过frame指针在阶段间传递，Python层仅做编排
- **BZERO/BSCALE根因修复**：fits_write_file关键字过滤列表增加BZERO/BSCALE，从源头避免float32数据携带无符号16位关键字导致的二次偏移

## 进度日志
### 2026-07-12 C++迁移完成与性能修复
- 完成C++迁移，15/15测试通过
- 修复动态缓冲区问题（图像读写越界）
- 修复PSF性能问题（详见dynamic_psf模块记录）
- 关键API：ImageReader/FITSWriter读写、PipelineFrame命名块容器、PipelineEngine管线调度
- 推送至GitHub：commit a33d167

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 文档刷新并重新推送
- 最新commit: c7ae15f

### 2026-07-16 healpix_io 合并入 aio (architecture-refactor spec G1+G2, Phase 1 完成)

**变更背景**: 按 architecture-refactor spec，healpix_io 源码合并入 aio，aio.dll 统一提供 FITS+XISF+HEALPix I/O。

**Phase 1 完成项**:
1. **源码迁移** (1.1-1.2): healpix_io.cpp/h → aio_healpix_io.cpp/h，API 前缀 hiss_/hcsd_/hio_ → aio_hiss_/aio_hcsd_/aio_hio_，添加向后兼容宏（`#define hiss_write aio_hiss_write` 等）
2. **选择编译机制** (1.3-1.4): aio_build_config.json + 条件编译宏（AIO_ENABLE_FITS/XISF/HEALPIX/COMPRESSOR/PIPELINE），build.ps1 动态构建源文件列表
   - **重大 bug 修复**: PowerShell 7 解析无 BOM UTF-8 文件时中文注释导致行解析失败（aio_log.cpp 被丢弃）。根因：中文注释 `# 核心必需源文件` 后的 `$srcFiles += "src/aio_log.cpp"` 被吞。修复：所有中文注释改为英文
3. **healpix_io 归档** (1.5): healpix_io/ 整目录移入 archive/，创建 ARCHIVED.md，FORMAT_SPEC.md/test_*/healpix_io.py 复制到 aio 对应位置
4. **依赖调整** (1.6): healpix_drizzle (6 源文件 + Makefile) + healpix_stack (build.ps1) 改为链接 aio.dll，移除 healpix_io.dll 依赖，添加 -DAIO_ENABLE_HEALPIX 宏
5. **编译验证** (1.7): V1-V4+V10 全部通过

**验证结果**:
| 验证项 | 结果 |
|--------|------|
| V1 | ✅ aio.dll 默认配置 2923.7 KB，67 aio_* + 9 HEALPix I/O 符号 |
| V2 | ✅ minimal 配置 893.9 KB，25 符号，无 aio_hiss_* |
| V3 | ✅ healpix_drizzle.dll 1243.8 KB，2 hp_drizzle_* 符号，只依赖 astro_image_io.dll |
| V4 | ✅ healpix_stack.dll 1430.8 KB，10 hp_stack_* 符号，只依赖 astro_image_io.dll |
| V10 | ✅ 向后兼容宏工作（旧名 hiss_read/hiss_write 编译通过）|

**新文件**:
- `include/aio_healpix_io.h` - HEALPix I/O API + 向后兼容宏
- `src/healpix/aio_healpix_io.cpp` - HEALPix I/O 实现
- `aio_build_config.json` (默认/full/minimal/healpix 4 套配置)
- `build.ps1` - 选择编译脚本
- `docs/HEALPIX_FORMAT_SPEC.md` - 格式规范（从 healpix_io 复制）
- `tests/test_healpix_io*.py` - 测试（从 healpix_io 复制）
- `python/aio_healpix_io.py` - Python 绑定（从 healpix_io 复制）

**修改文件**:
- `src/aio_api.cpp` - 添加 AIO_ENABLE_FITS/XISF 条件编译宏
- `lib/healpix_db/healpix_drizzle/` - drizzle_engine.cpp/h, hp_drizzle_api.cpp, Makefile
- `lib/healpix_db/healpix_stack/` - hp_stack_api.cpp, hp_stack_hiss.cpp, gradient/gradient_sampler.cpp, build.ps1
