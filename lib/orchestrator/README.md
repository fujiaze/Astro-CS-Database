# Pipeline Orchestrator

版本：v2.0 两段流水线 10 节点 C++ CLI | 2026-07-16

## 模块职责

管线编排引擎模块。统一管理两段流水线 10 节点全链路流水线，提供单帧预处理（stage1）和多帧合并（stage2）两个 CLI 命令。

通过 `DllLoader` 动态加载各 C++ DLL 模块，以 `PipelineFrame` 命名块容器在阶段间传递数据，实现零临时文件的内存管线。

## GitHub仓库
- https://github.com/fujiaze/Orchestrator-Cpp-Python

## 架构设计（spec §2.3 两段流水线 10 节点）

### 第一段：单帧预处理（FITS → .hiss, stage 0-7）

```
orchestrator stage1 --frame <fits> --output <hiss> [options]
```

| Stage | 名称 | DLL | 职责 |
|-------|------|-----|------|
| 0 | READ_FITS | astro_image_io.dll | 读取 FITS 到 PipelineFrame |
| 1 | CALIBRATE | astro_calibration.dll | dark/bias/flat 校准 + 坏点修复 |
| 2 | PLATESOLVE | ipv_solver.dll | WCS/SIP 解析 |
| 3 | PSF | dynamic_psf.dll | PSF 拟合 |
| 4 | PHOTOMETRIC | photometric_calib.dll | F_syn 积分 + 全局 scale |
| 5 | GRADIENT_2D | gradient_2d.dll | step4 C++化: 乘性梯度曲面拟合 + 图像校正 |
| 6 | SNR | snr_estimator.dll | 异常值剔除 + 测光不确定度 + 帧SNR基准 |
| 7 | DRIZZLE | healpix_drizzle.dll | nside 1-2x, SNR同步转换, 落盘 .hiss |

### 第二段：多帧合并（.hiss → .hcsd, stage 8-9）

```
orchestrator stage2 --frames <hiss_dir> --output <hcsd> [options]
```

| Stage | 名称 | DLL | 职责 |
|-------|------|-----|------|
| 8 | GRADIENT_SPHERE | healpix_stack.dll | 球面梯度校准 (hp_stack_gradient_corrected) |
| 9 | STACK | healpix_stack.dll | Winsorized sigma clip + SNR²加权叠加 → .hcsd |

## 功能列表

### C++ CLI（v2.0, 两段流水线）
- `orchestrator stage1` - 单帧预处理 (stage 0-7 串行)
- `orchestrator stage2` - 多帧合并 (stage 8-9 串行)
- `orchestrator run` - 旧版单帧处理 (5 阶段, 向后兼容)
- `orchestrator run-batch` - 旧版批量处理
- `orchestrator status` - 状态查询
- DllLoader: 动态加载 10 个模块 DLL (AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE/GRADIENT_SPHERE/STACK)
- CheckpointManager: 断点续传
- Logger: 统一日志系统 (DEBUG/INFO/WARN/ERROR)
- 交互式 REPL 模式

### Python 调试层（v1.0, 保留）
- Orchestrator类：封装PipelineEngine，提供run_single/run_batch接口
- 5个管线适配器（pipeline_adapters/）
- 端到端测试

## 目录结构
- `cpp/include/` - C++ 头文件 (orchestrator.h, dll_loader.h, cli_command.h, checkpoint.h, logger.h, cli_repl.h)
- `cpp/src/` - C++ 源文件 (orchestrator.cpp, dll_loader.cpp, cli_command.cpp, checkpoint.cpp, logger.cpp, cli_repl.cpp, main.cpp)
- `cpp/tests/` - 单元测试 (test_dll_loader, test_checkpoint, test_logger, test_orchestrator_cli)
- `cpp/Makefile` - 编译配置 (g++ -O2 -std=c++17 -Wall -fopenmp -static)
- `configs/` - 配置文件 (stage1_config.json, stage2_config.json, galaxy_center_t4.json)
- `python/` - Python 调试层 (orchestrator.py + pipeline_adapters/)
- `tests/` - Python 端到端测试
- `archive/scripts/` - 归档批处理脚本
- `docs/architecture.md` - 架构说明
- `logs/` - 运行时日志

## 依赖列表

### C++ DLL依赖（10个模块, spec §2.3.2）
- astro_image_io.dll - PipelineFrame + PipelineEngine (stage 0)
- astro_calibration.dll - 校准 (stage 1)
- ipv_solver.dll - plate solving (stage 2)
- dynamic_psf.dll - PSF拟合 (stage 3)
- photometric_calib.dll - 测光校准 (stage 4)
- gradient_2d.dll - step4 C++化: 梯度曲面拟合 (stage 5)
- snr_estimator.dll - SNR估算 (stage 6)
- healpix_drizzle.dll - HEALPix drizzle (stage 7)
- healpix_stack.dll - 球面梯度校准 + 堆叠 (stage 8-9, 共用)

### Python依赖
- numpy, ctypes

## 编译说明

```powershell
# 需要 MinGW g++ (C:\msys64\mingw64\bin)
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cd lib\orchestrator\cpp
make            # 编译 orchestrator.exe
make clean      # 清理
```

## 使用示例

### stage1: 单帧预处理

```powershell
# 从项目根目录执行 (DLL 路径为相对路径)
.\lib\orchestrator\cpp\orchestrator.exe stage1 `
    --frame testdata\results\...\01_calibrated.fits `
    --output output\frame1.hiss `
    --gaia-data GaiaDR3SP `
    --filter Red `
    --config lib\orchestrator\configs\stage1_config.json `
    --log-level INFO
```

### stage2: 多帧合并

```powershell
.\lib\orchestrator\cpp\orchestrator.exe stage2 `
    --frames output\hiss_output\ `
    --output output\stacked.hcsd `
    --config lib\orchestrator\configs\stage2_config.json `
    --log-level INFO
```

### 旧版命令（向后兼容）

```powershell
orchestrator run <fits> [--config <json>] [--threads <N>] [--fresh]
orchestrator run-batch <dir> [--config <json>] [--threads <N>] [--fresh]
```

## 配置文件

### stage1_config.json (spec §2.3.3)
```json
{
  "project_root": ".",
  "gaia_data_dir": "GaiaDR3SP",
  "calibration_dir": "testdata/calibration",
  "stages": ["read_fits", "calibrate", "platesolve", "psf",
             "photometric", "gradient_2d", "snr", "drizzle"],
  "frame": {"id": "panel1_Red", "filter": "Red", "qe_curve": "GSENSE2020BSI"},
  "drizzle": {"nside_strategy": "1x_to_2x_drizzle", "nside_override": 0}
}
```

### stage2_config.json (spec §2.3.3)
```json
{
  "frames_dir": "output/hiss_output/",
  "output_hcsd": "output/stacked.hcsd",
  "stages": ["gradient_sphere", "stack"],
  "stack": {
    "sigma_clip_method": "winsorized",
    "sigma_clip_sigma": 3.0,
    "weighting": "snr_squared"
  }
}
```

## 接口说明

### Orchestrator类 (C++)
- `run_stage1(fits_path, output_hiss, config_json)` -> TaskResult
- `run_stage2(hiss_dir, output_hcsd, config_json)` -> TaskResult
- `run_single(fits_path)` -> TaskResult (旧版 5 阶段)
- `run_batch(dir_path)` -> vector<TaskResult> (旧版批量)
- `init_dlls(lib_base_dir, error_msg)` -> bool
- `load_config(config_path, error_msg)` -> bool
- `pause() / resume() / interrupt()` - 状态控制
- `save_checkpoint() / load_checkpoint()` - 断点续传

### TaskResult 结构
```json
{
  "success": true,
  "frame_name": "...",
  "timings": [{"stage": "...", "name": "...", "duration_sec": 0.0, "success": true}],
  "wcs_fields": {},
  "photo_stats": {},
  "output_ahpx_path": "...",
  "error_msg": ""
}
```

## 版本历史
- v2.0 (2026-07-16): 两段流水线 10 节点 C++ CLI (stage1/stage2) + DllLoader 10 模块 + 配置文件
- v1.0 (2026-07-12): 从各模块迁移编排代码，Python原型完成
