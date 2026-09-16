# ADR-010: 删除 per-kernel 固定路由，改为 HardwareProfile 驱动动态调度

**状态**: Accepted
**日期**: 2026-08-03
**决策者**: 用户授权（控制包 20_PHASE_I_AUDIT_ACTION_PLAN.md）

## 背景

Phase I AuditPack 审计发现以下问题：

1. `routing/route_profile.hpp` 定义了 per-kernel `preferred_backend` 固定路由模型
2. `qualification/profile_generator.cpp` 生成 `routes.json`，按 (kernel_id, precision) 预选 backend
3. `routing/static_router.cpp` 实现了 `StaticRouteResolver`，从 `routes.json` 查表得 backend
4. 但运行时调度路径（`runtime.cpp::submit_range_with_desc` → `CostEstimator` → `Dispatcher`）**完全不读 routes.json**
5. `preferred_backend` 字段在 6 个数据结构中存在，但对调度结果**无任何实际影响**（GPU 路径占位 + 字段不被消费）

两套并行路由模型存在：
- **旧静态路由**：`StaticRouteResolver`（读 `routes.json`，per-kernel 固定路由）
- **新动态决策**：`CostEstimator`（读 `hardware-profile.json`，根据 TaskDescriptor + HardwareProfile 实时估算）

## 决策

1. **删除旧静态路由模型**：
   - 删除 `routing/route_profile.hpp`（RouteEntryView/RouteProfile 数据结构）
   - 删除 `routing/static_router.hpp` / `routing/static_router.cpp`（StaticRouteResolver）
   - 删除 `schemas/route_profile.schema.json` / `schemas/route_profile.example.json`
   - 清理 `qualification/profile_generator.cpp` 中 `routes.json` 生成逻辑
   - 清理 `qualification/profile_schema.hpp` 中 `RouteEntry` / `ProfileBundle` 结构
   - 删除 `tools/acr_invalidate/`（仅用于删除 `routes.json`）

2. **删除 `preferred_backend` 字段**：
   - `DispatcherConfig::preferred_backend` — 删除
   - `CostAwareResult::preferred_backend` — 删除（改用 `preferred_device`）
   - `MixedRunnerConfig::preferred_backend` — 删除
   - `CostEstimate::preferred_backend` — 保留为派生字段（从 `preferred_device` 推导），或删除
   - `runtime.cpp` 中 `cfg.preferred_backend = "cpu"` 硬编码 — 删除

3. **保留**：
   - `OperationId` 作为诊断/缓存标识（不作为固定路由键）
   - `TaskTraits` / `TaskDescriptor` 作为任务特征输入
   - `CostEstimator` 读取 `hardware-profile.json` 的动态决策路径
   - `Dispatcher` 的 work-conserving 调度模型

4. **接通调用链**（Commit C）：
   - `submit_tiles_with_desc` / `submit_batch_with_desc` / `submit_reduce_with_desc` 接通 `CostEstimator` → `Dispatcher`

## 影响

- 运行时调度完全由 `CostEstimator` + `HardwareProfile` 驱动
- 无画像时走明确 CPU fallback（不悄悄伪造 GPU 路由）
- 旧 `routes.json` 不再生成；已有 `routes.json` 加载返回 unsupported schema 错误
- 工具（`acr-status` / `acr-report`）改为基于 `hardware-profile.json` 展示状态

## 风险

- 旧 `routes.json` 用户需重新生成 `hardware-profile.json`（通过 `acr-benchmark`）
- `StaticRouteResolver` 的三态处理（Missing/Stale/Corrupt）由 `HardwareProfileReader` 替代
