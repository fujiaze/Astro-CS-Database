# AstroCompute Runtime (ACR)

独立底层计算运行时，为 AstroCS 未来密集计算模块提供 CPU 多线程、CPU ISA、GPU 和 CPU+GPU 混合执行。

**当前状态**: DORMANT（ACR-001 固化首发休眠边界；源码/接口/隔离测试保留，
生产构建/加载/路由/benchmark/发布包不含 ACR/CUDA）。阶段历史：Phase A 完成
（审计 + ADR + dependency-lock + CMake 骨架 + path guard）。

## 休眠边界（ACR-001，冻结约束 §C.1）

AstroCS_ENGINEERING_CONSTRAINTS.md §C.1/C.2：ACR 是正式发布后的 CPU/GPU 异构
更新；本轮保留源码和隔离测试，但**生产构建、加载、路由、benchmark、发布包均
不得依赖或包含 ACR/CUDA**；当前唯一生产计算后端是纯 CPU。

机器守卫（`lib/acr/CMakeLists.txt` 顶部 guard + `lib/acr/ci/` 校验器）：

1. **生产构建不引入**：根 CMakeLists.txt 从不 `add_subdirectory(lib/acr)`；
   若任何生产构建尝试引入，lib/acr CMakeLists 顶部 guard 检测到
   `CMAKE_SOURCE_DIR != CMAKE_CURRENT_SOURCE_DIR` 即 `FATAL_ERROR`。
2. **实验 target 只能隔离构建**：显式实验 configure 以 ACR 树自身为
   `-S` 根（见下），产出 target 全部位于独立 build 目录，不与产品图交集。
3. **release preset 不接受 ON**：`win-msvc-17.14.39-x64` 与 `linux-control`
   preset 冻结 `ASTROCS_ENABLE_ACR=OFF`；`cmake/toolchain/verify_toolchain.py`
   对 formal path 强制 OFF（ON → FAIL）。
4. **install/product manifest 零 ACR**：唯一 install 源
   `cmake/install_layout.cmake` 与 `packaging/*.json` 均无 ACR/CUDA 条目
   （机器校验见 `lib/acr/ci/check_acr_dormant.py`）。
5. **生产二进制不加载**：`include/astrocs/core/runtime.h` 声明"ACR 不注册不链接"，
   `lib/core/src/module.cpp` 拒绝 `astrocs.acr.*` 模块注册；ACR 实验入口
   （tools/qualification/scheduler）仅供独立实验构建。

验收命令（Linux 控制节点）：

```bash
python3 lib/acr/ci/check_acr_dormant.py --repo .            # 全部 PASS
python3 lib/acr/ci/check_acr_dormant.py --repo . --selftest # 负测自检
```

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

```bash
# 隔离实验构建（唯一合法入口；本树自身为 -S 根 → ACR-001 guard PASS）
cmake -S lib/acr -B build/acr -DCMAKE_BUILD_TYPE=Release -DACR_BUILD_CUDA=OFF
cmake --build build/acr -j

# 负例（机器负测 NEG-ACR-001）：从产品根尝试 add_subdirectory 引入会被
# lib/acr/CMakeLists.txt FATAL guard 拒绝 —— 本树不可并入产品构建图。
```

```powershell
# CPU-only（默认，不依赖 GPU SDK）—— Windows 实验验证
cmake -S lib/acr -B build/acr -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/acr -j

# 启用 CUDA（本机 RTX 3060 Ti 真实验证；仅实验树，生产不含 CUDA）
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
