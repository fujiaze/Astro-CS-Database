# AstroCompute Runtime (ACR)

独立底层计算运行时，为 AstroCS 未来密集计算模块提供 CPU 多线程、CPU ISA、GPU 和 CPU+GPU 混合执行。

**当前状态**: Phase A 完成（审计 + ADR + dependency-lock + CMake 骨架 + path guard）

## 范围

本支线 `feature/astrocompute-runtime` **只开发底层**，不修改任何现有 AstroCS 算法实现。详见 `工程控制/tasks/acr/spec.md`。

## 目录结构

```
lib/acr/
├── CMakeLists.txt          # 独立 CMake（CPU-only 默认，CUDA backend 可选）
├── include/astro/compute/  # 公共 API 头
├── api/                    # parallel_for/tiles/reduce/batch 实现
├── core/                   # runtime lazy singleton、error、status
├── buffers/                # BufferView/Buffer/DeviceBuffer
├── topology/               # hwloc + cpu_features
├── backends/{cpu,alpaka,cuda,hip,sycl,starpu_optional}/
├── qualification/          # Benchmark + 固定路由生成
├── routing/                # 静态路由解析（只读）
├── scheduler/              # 工作保持调度器
├── utilization/            # 95% 软占用控制
├── diagnostics/            # 日志 + 设备报告
├── tests/{unit,classic,fault}/
├── tools/{acr_benchmark,acr_status,acr_report,acr_invalidate}/
├── docs/                   # ADR + 审计报告 + 禁止路径 + dependency-lock
├── schemas/                # route_profile schema
├── examples/               # minimal_parallel_for 等
└── ci/path_guard.ps1       # 提交前路径检查
```

## 构建

```powershell
# CPU-only（默认，不依赖 GPU SDK）
cmake -S lib/acr -B build/acr -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/acr -j

# 启用 CUDA（本机 RTX 3060 Ti 真实验证）
cmake -S lib/acr -B build/acr-cuda -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DACR_BUILD_CUDA=ON
cmake --build build/acr-cuda -j
```

## 文档

- 工程控制/tasks/acr/spec.md — 实现规格
- 工程控制/tasks/acr/checklist.md — 验收检查表
- 工程控制/tasks/acr/tasks.md — 任务拆分
- docs/audit-report.md — 仓库审计
- docs/forbidden-paths.md — 禁止修改路径
- docs/dependency-lock.json — 第三方依赖锁定
- docs/ADR-00*.md — 架构决策记录
