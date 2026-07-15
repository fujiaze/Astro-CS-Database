# Pipeline Orchestrator

版本：v1.0 Python原型 | 规划中：C++ CLI版本 | 2026-07-12

## 模块职责

管线编排引擎模块。统一管理校准→解析→PSF→测光→Drizzle全链路流水线，提供单帧和批量处理能力。

通过 `PipelineStageHandlerC` 将各 C++ DLL 模块封装为统一的阶段处理器，以 `PipelineFrame` 命名块容器在阶段间传递数据，实现零临时文件的内存管线。

## GitHub仓库
- 暂未上传（规划中）

## 功能列表
- Orchestrator类：封装PipelineEngine，提供run_single/run_batch接口
- 5个管线适配器（pipeline_adapters/）：
  - calibrate_adapter.py - STAGE_CALIBRATE（调用ac_calibrate_frame C++ DLL）
  - platesolve_adapter.py - STAGE_PLATESOLVE（调用ipv_solve_from_memory C++ DLL）
  - psf_adapter.py - PSF_FIT非标准阶段（调用dpsf_fit_batch C++ DLL）
  - photometric_adapter.py - STAGE_PHOTOMETRIC（调用pc_calibrate_simple C++ DLL）
  - drizzle_adapter.py - STAGE_DRIZZLE（调用hp_drizzle_run C++ DLL）
- 端到端测试（15项验证）
- 批处理脚本（step1-4、batch、report、visualize）

## 目录结构
- python/orchestrator.py - 编排器核心
- python/pipeline_adapters/ - 5个管线适配器
- tests/test_orchestrator_e2e.py - 端到端测试
- scripts/ - 批处理脚本（从integration_test迁移）
- docs/architecture.md - 架构说明
- logs/ - 运行时日志

## 依赖列表

### C++ DLL依赖（5个）
- astro_image_io.dll - PipelineFrame + PipelineEngine
- ipv_solver.dll - plate solving
- dynamic_psf.dll - PSF拟合
- photometric_calib.dll - 测光校准
- healpix_drizzle.dll - HEALPix drizzle

### Python依赖
- numpy, ctypes

## 编译说明

本模块为Python模块，无需编译。依赖的C++ DLL请参考各模块的编译说明。

所有 DLL 依赖 MinGW 运行时 (`C:\msys64\mingw64\bin`)。

## 使用示例

```python
from orchestrator import Orchestrator
from pipeline_adapters import (
    register_calibrate_handler,
    register_platesolve_handler,
    register_psf_handler,
    register_photometric_handler,
    register_drizzle_handler,
)

# 创建编排器
orch = Orchestrator()

# 注册handler
register_calibrate_handler(orch.engine, CalibrateParams(...))
register_platesolve_handler(orch.engine, PlateSolveParams(...))
register_drizzle_handler(orch.engine, DrizzleParams(nside=8192, output_dir="./output"))

# 运行单帧
result = orch.run_single(frame, STAGE_CALIBRATE, STAGE_DRIZZLE)
```

### 批量处理

```python
scripts/run_all.py          # 全链路批量
scripts/batch_calibrate.py  # 仅校准
scripts/batch_solve.py      # 仅解析
scripts/batch_step34.py     # 步骤3+4 (积分+估计)
```

## 接口说明

### Orchestrator类
- run_single(frame, from_stage, to_stage) -> dict
  返回: {success, timings, blocks, output_files, wcs, photo_stats, error}

### PipelineStageHandlerC
C回调函数签名: int (*)(void* frame, void* params, char* err_buf, int err_cap)

### 5个适配器
每个适配器提供register_xxx_handler(engine, params)函数和XxxParams数据类。

## 迁移说明

本模块从以下位置迁移而来（使用 copy，源文件保留）：

- `lib/astro_image_io/python/orchestrator.py` → `python/orchestrator.py`
- `lib/astro_image_io/python/tests/test_orchestrator_e2e.py` → `tests/test_orchestrator_e2e.py`
- `lib/calibration/python/pipeline_adapter.py` → `python/pipeline_adapters/calibrate_adapter.py`
- `lib/plate_solve/python/pipeline_adapter.py` → `python/pipeline_adapters/platesolve_adapter.py`
- `lib/photometric_calib/flux_calibrator/python/pipeline_adapter.py` → `python/pipeline_adapters/photometric_adapter.py`
- `lib/healpix_db/healpix_drizzle/pipeline_adapter.py` → `python/pipeline_adapters/drizzle_adapter.py`
- `lib/dynamic_psf` 的 PSF handler 代码从 orchestrator.py 提取 → `python/pipeline_adapters/psf_adapter.py`
- `lib/integration_test/python/*` → `scripts/`

## 未来规划

### C++ CLI 版本

计划将编排器迁移为 C++ CLI 可执行程序，实现：
- **JSON 处理**: 用 nlohmann/json 替代 Python dict 传递参数和结果
- **命令行交互**: 支持 `--stage`, `--input`, `--output`, `--config` 等参数，可交互查询状态、中断程序
- **断点续传**: 记录每帧处理状态，失败后可从断点恢复
- **配合前端**: 输出结构化 JSON 进度，供 Web/GUI 前端实时展示
- **多线程**: 利用 16 线程 CPU 并行处理多帧
- **集成日志系统**: 统一的日志输出与分析

## 版本历史
- v1.0 (2026-07-12): 从各模块迁移编排代码，Python原型完成
