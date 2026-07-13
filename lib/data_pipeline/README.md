# Astro Data Pipeline

版本：v1.0 C++实现 | 2026-07-12

## 模块职责

内存管线C++实现模块。提供PipelineFrame、PipelineEngine和命名块容器接口，为astro_image_io和其他模块提供底层数据管线支撑。

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Astro-Data-Pipeline
- 默认分支：main
- 最新commit：e81717e

## 功能列表
- PipelineFrame：命名块容器，存储像素数据、WCS、星表等
- PipelineEngine：管线引擎，注册和调度阶段handler
- 命名块容器：通过name索引的数据块，支持KV存储
- OpenMP并行批量处理

## 目录结构
- src/aio_pipeline.cpp - PipelineFrame实现
- src/aio_pipeline_engine.cpp - PipelineEngine实现
- include/aio_pipeline.h - PipelineFrame C API
- include/aio_pipeline_engine.h - PipelineEngine C API
- docs/api_spec.md - 详细API规范文档（14KB）

## 依赖列表
- C++17
- OpenMP（批量并行）
- 无外部库依赖

## 编译说明

```bash
g++ -O2 -std=c++17 -fopenmp -shared -o data_pipeline.dll \
    src/aio_pipeline.cpp src/aio_pipeline_engine.cpp \
    -Iinclude -static-libgcc -static-libstdc++
```

## 使用示例

```c
#include "aio_pipeline.h"

// 创建PipelineFrame
PipelineFrame* frame = aio_frame_new();

// 设置命名块
float* pixels = ...;
aio_frame_set_block(frame, "data", AIO_FLOAT32, dims, 2, pixels);

// 设置KV
aio_frame_kv_set_double(frame, "header", "CRVAL1", 180.0);
aio_frame_kv_set_string(frame, "header", "OBJECT", "M31");

// 获取命名块
float* data = (float*)aio_frame_get_block(frame, "data", dims);

// 释放
aio_frame_free(frame);
```

```c
#include "aio_pipeline_engine.h"

// 创建引擎
PipelineEngine* engine = aio_engine_new();

// 注册阶段handler
aio_engine_register(engine, STAGE_CALIBRATE, calibrate_handler);

// 运行单帧
aio_engine_run_single(engine, frame, STAGE_CALIBRATE, STAGE_DRIZZLE);

// OpenMP并行批量运行
aio_engine_run_batch(engine, frames, n_frames, STAGE_CALIBRATE, STAGE_DRIZZLE);

aio_engine_free(engine);
```

## 接口规范

详见 [docs/api_spec.md](docs/api_spec.md)（14KB详细API规范）

### PipelineFrame C API
- aio_frame_new() / aio_frame_free()
- aio_frame_get_block() / aio_frame_set_block()
- aio_frame_kv_get_double() / aio_frame_kv_set_double()
- aio_frame_kv_get_string() / aio_frame_kv_set_string()
- aio_frame_export_block_fits() / aio_frame_export_xml()

### PipelineEngine C API
- aio_engine_new() / aio_engine_free()
- aio_engine_register()
- aio_engine_run_single() / aio_engine_run_batch()
- aio_engine_set_num_threads()

### 命名块容器
- 支持的数据类型: FLOAT32, FLOAT64, INT32, INT64, UINT8, STRING
- 块索引: 通过name字符串索引
- KV存储: 键值对存储WCS、统计等信息

## 性能指标
- 4096×4096 float32帧处理：端到端20.90s（PLATESOLVE=2.17s, PSF=0.26s, PHOTOMETRIC=0.03s, DRIZZLE=18.44s）
- OpenMP 16线程并行批量处理

## 版本历史
- v1.0 (2026-07-12): 从astro_image_io迁移C++实现，新建独立仓库

## 许可
MIT
