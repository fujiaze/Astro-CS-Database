# data_pipeline - 模块开发memory

## 模块职责
内存管线C++实现，提供PipelineFrame、PipelineEngine与命名块容器接口，作为各C++模块共享的管线框架与数据传递载体。

## 当前版本
- 版本号：v1.0
- 最新commit：e81717e
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Astro-Data-Pipeline
- 默认分支：main

## 依赖列表
- C++17
- OpenMP（多线程并行）

## 关键决策记录
- **命名块容器模型**：PipelineFrame以"命名块"为核心数据结构，支持像素块（FLOAT32/FLOAT64数组）、星表块、KV元数据块统一管理，通过add_block/get_block_data/kv_set/kv_get接口访问
- **PipelineFrame+PipelineEngine架构**：PipelineEngine以PipelineStageHandler回调为执行单元，frame指针在阶段间传递，避免数据拷贝
- **C API设计**：导出纯C接口（pipeline_frame_create/add_block/get_block_data/kv_set等），便于ctypes绑定与跨语言调用
- **从astro_image_io迁移独立**：原管线实现位于astro_image_io，为职责单一拆分为独立仓库，astro_image_io保留I/O层

## 进度日志
### 2026-07-12 从astro_image_io迁移C++实现，新建独立仓库
- 从astro_image_io模块迁移PipelineFrame、PipelineEngine、命名块容器实现
- 新建独立GitHub仓库：Astro-Data-Pipeline
- 推送至GitHub：commit e81717e
- C API设计完成，支持ctypes绑定
