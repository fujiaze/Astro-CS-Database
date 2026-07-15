# 编排器架构说明

## 概述

Orchestrator 是天文图像处理管线的编排引擎，串联 5 个标准阶段和 1 个自定义阶段，通过 `PipelineFrame` 命名块容器实现零临时文件的内存数据流。

## 核心组件

### 1. PipelineEngine (C++ 引擎)

来自 `astro_image_io` 模块，提供阶段注册和执行能力：
- `engine.register(stage_id, handler)` - 注册阶段处理器
- `engine.run_single(frame, from_stage, to_stage)` - 按阶段范围执行

### 2. PipelineStageHandlerC (ctypes 回调包装器)

将 Python 闭包 handler 包装为 C 函数指针：
```python
handler_c = PipelineStageHandlerC(python_callback)
engine.register(STAGE_CALIBRATE, handler_c)
```

回调签名: `int handler(c_frame_ptr, params_ptr, err_buf, err_cap) -> int (0=成功, -1=失败)`

### 3. PipelineFrame (命名块容器)

C++ 端的 `PipelineFramePy` 通过命名块存储数据：
- `frame.add_block(name, numpy_array, description)` - 添加数据块
- `frame.get_block_data(name)` - 获取块数据 (零拷贝 numpy view)
- `frame.kv_set(namespace, key, value)` - 设置 KV 块
- `frame.kv_get(namespace, key)` - 获取 KV 值
- `frame.remove_block(name)` - 删除块

### 4. 命名块数据流

```
读取FITS
  │ data (FLOAT32[H,W])
  │ header (KV: SOURCE_PATH, EXPTIME, FILTER, OBJCTRA, OBJCTDEC, ...)
  ▼
CALIBRATE (calibrate_adapter)
  │ data → 校准后像素 (替换)
  │ cal_stats (KV: DARK_K)
  ▼
PLATESOLVE (platesolve_adapter)
  │ header += WCS (CD1_1, CRVAL1, CRPIX1, CTYPE1, ...)
  │ header += SIP (A_ORDER, A_0_0, B_ORDER, ...)
  │ star_det (FLOAT32[N,4]: x, y, flux, mag)
  │ gaia_cat (FLOAT64[N,3]: ra, dec, mag)
  ▼
PSF_FIT (psf_adapter) [非标准阶段，手动插入]
  │ psf (FLOAT64[N,9]: status, B, flux, cx, cy, fwhm, A, mad, eccentricity)
  ▼
PHOTOMETRIC (photometric_adapter)
  │ data → 光度校准后像素 (替换, 全局scale校正)
  │ photo_stats (KV: N_MATCHED, SCALE_FACTOR)
  │ [清理] star_det, gaia_cat, psf 块被丢弃
  ▼
DRIZZLE (drizzle_adapter)
  │ → .ahpx 输出文件
```

## 5个标准阶段

| 阶段常量 | 值 | 适配器 | 输入 | 输出 |
|----------|---|--------|------|------|
| STAGE_CALIBRATE | 0 | calibrate_adapter | data, header | data(替换), cal_stats |
| STAGE_PLATESOLVE | 1 | platesolve_adapter | data, header | header+=WCS, star_det, gaia_cat |
| STAGE_PHOTOMETRIC | 2 | photometric_adapter | data, header, gaia_cat, psf | data(替换), photo_stats |
| STAGE_DRIZZLE | 3 | drizzle_adapter | data, header | .ahpx 文件 |
| STAGE_STACK | 4 | (未实现) | - | - |

## 非标准阶段: PSF_FIT

PSF 拟合不是 `PipelineEngine` 的标准阶段，而是在 PLATESOLVE 后由 `Orchestrator` 手动调用：

```python
# Orchestrator.run_single() 中的调用顺序
self._call_handler(self._calib_handler, frame, "CALIBRATE")
self._call_handler(self._solve_handler, frame, "PLATESOLVE")
self._call_handler(self._psf_handler, frame, "PSF_FIT")       # 手动插入
self._call_handler(self._photo_handler, frame, "PHOTOMETRIC")
self._call_handler(self._drizzle_handler, frame, "DRIZZLE")
```

## Orchestrator 类设计

### 初始化流程

1. `_setup_paths()` - 添加各模块 Python 路径到 `sys.path`
2. `_extract_handler()` - 通过 `_HandlerExtractor` 伪 engine 从各适配器提取 handler
3. `make_psf_fit_handler()` - 创建 PSF 拟合 handler (始终创建)

### Handler 提取机制

适配器文件使用 `register_*_handler(engine, params)` 模式注册 handler。`Orchestrator` 不使用真正的 `PipelineEngine`，而是用 `_HandlerExtractor` 捕获 handler：

```python
class _HandlerExtractor:
    def register(self, stage, handler, params=None):
        self.handler = handler  # 捕获 handler
```

然后通过 `_load_module_from_path` 动态加载适配器文件（避免多个 `pipeline_adapter.py` 命名冲突）。

### 运行单帧

`run_single(fits_path, output_dir)` 返回:
- `success: bool` - 是否成功
- `timings: dict` - 各阶段耗时
- `blocks: dict` - 各阶段后的块名列表
- `output_files: list` - 输出文件路径 (.ahpx)
- `error: str` - 错误信息
- `photo_stats: dict` - 光度校准统计
- `wcs: dict` - WCS 关键字段

## 内存管线优势

- **零临时文件**: 所有数据通过 `PipelineFrame` 命名块在内存中传递
- **零拷贝**: `get_block_data` 返回 numpy view，直接操作 C 端内存
- **高性能**: 相比临时文件方案，总耗时减少约 30-50%

## 日志体系

- 每个适配器有独立的 `logging.Logger`
- `platesolve_adapter` 初始化时创建文件日志 (UTF-8 编码)
- `Orchestrator._export_debug()` 可导出各阶段后的 frame XML 调试信息
- 日志目录: `lib/orchestrator/logs/`
