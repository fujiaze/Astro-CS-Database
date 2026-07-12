# Astro Data Pipeline

内存管线C++实现 - 提供PipelineFrame、PipelineEngine和命名块容器接口。

## 模块职责

- PipelineFrame: 命名块容器，存储像素数据、WCS、星表等
- PipelineEngine: 管线引擎，注册和调度阶段handler
- 命名块容器: 通过name索引的数据块，支持KV存储

## 接口规范

### PipelineFrame
- aio_frame_new() - 创建新frame
- aio_frame_free() - 释放frame
- aio_frame_get_block() - 获取命名块数据
- aio_frame_set_block() - 设置命名块数据
- aio_frame_kv_get_double() - 获取KV双精度值
- aio_frame_kv_set_double() - 设置KV双精度值
- aio_frame_kv_get_string() - 获取KV字符串
- aio_frame_kv_set_string() - 设置KV字符串

### PipelineEngine
- aio_engine_new() - 创建新引擎
- aio_engine_register() - 注册阶段handler
- aio_engine_run_single() - 运行单帧
- aio_engine_run_batch() - 批量运行（OpenMP并行）

### 命名块容器
- 支持的数据类型: FLOAT32, FLOAT64, INT32, INT64, UINT8, STRING
- 块索引: 通过name字符串索引
- KV存储: 键值对存储WCS、统计等信息

## 依赖
- C++17
- OpenMP (批量并行)
- 无外部库依赖

## 编译
参考 astro_image_io/Makefile

## 许可
MIT
