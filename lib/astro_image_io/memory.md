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
