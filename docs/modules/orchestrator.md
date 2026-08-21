# Module: orchestrator

## 职责

Phase1 编排：stage1.json 驱动 READ/CALIBRATE/STAR/PSF/PLATESOLVE/
PHOTOMETRIC/NOISE/DRIZZLE/HIPS_WRITE，DllLoader 动态加载模块 DLL。

## 非职责

不实现科学算法。

## Public API

orchestrator.exe <stage1.json>；模块头文件契约。

## Data contract

stage1.json（输入/校准/输出/参数）；运行产物 run/。

## Ownership

模块句柄生命周期由 orchestrator 管理。

## Thread safety

stage 顺序；模块内 OpenMP。

## Errors

模块加载失败/阶段失败 → 显式 exit code + 日志。

## Config

stage1 JSON schema（configs/stage1.schema.json）。

## Science IDs

全链（阶段委托模块）。

## 性能特征

资源监控（resource_monitor）；spill/checkpoint（V13+）。

## Diagnostics

阶段日志 run/logs/orchestrator/；已知 bug：cpp/ 下嵌套 logs 目录（非阻断）。

## Tests

单帧端到端验证；DLL smoke。

## Source files

lib/orchestrator/cpp/。

## 构建/契约锚点

C++17 (`-std=c++17`, 见 `lib/orchestrator/cpp/Makefile:CXXFLAGS`)；C ABI 经 `DllLoader` 纯 C 调用（`docs/standards/C_ABI_STANDARD.md`）；退出码与 `docs/architecture/ERROR_MODEL.md` 全集合一致（`tools/docs_machine_consistency.py` 校验）[B4-24]。
