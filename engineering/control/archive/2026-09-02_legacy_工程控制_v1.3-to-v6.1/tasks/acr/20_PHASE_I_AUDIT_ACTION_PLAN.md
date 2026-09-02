# Phase I AuditPack 专项行动计划

审计对象：`AstroCS_ACR_AuditPack_2026-08-03_PhaseI(5).zip`  
审计记录中的分支：`feature/astrocompute-runtime`  
审计记录中的 HEAD：`a0d7783`  
用途：把当前代码从旧的 per-kernel 路由骨架纠正为 HardwareProfile 驱动的异构运行时。

## 1. 保留，不推倒重来

- `lib/acr/include/astro/compute/acr.hpp` 中可用的 Buffer/Event/RuntimeConfig；
- `core/runtime.cpp` 的 oneTBB 基础；
- `topology/` 的 hwloc 与 cpu_features；
- `backends/cpu/isa/` 的已有 ISA 代码，后续补真实独立计时；
- `backends/cuda/` 的代码骨架，修复工具链和集成；
- `diagnostics/` 和现有单测框架；
- 经典实验中正确性和coverage可复用部分。

## 2. 删除或迁移

- `routing/route_profile.hpp` 的 per-kernel `preferred_backend`；
- `qualification/profile_generator.cpp` 生成的旧 `routes.json`；
- 任何将 OperationId直接映射设备的长期规则；
- CPU模拟Mixed却命名为Mixed的结论；
- 人工利用率输入被宣称为资源控制验证；
- 未开启ASan却命名为sanitizer验证的报告。

保留旧文件只允许作为迁移测试fixture，不得参与正式运行。

## 3. 第一批提交：架构迁移

### Commit A

`docs(acr): freeze profile driven runtime architecture`

- 更新ADR、schema和README；
- 明确旧profile不兼容；
- 增加path guard。

### Commit B

`refactor(acr): replace kernel routes with hardware profiles`

- 新建 DeviceProfile/HardwareProfile；
- 删除preferred_backend；
- 旧routes加载返回明确unsupported schema。

### Commit C

`refactor(acr): connect task descriptors to cost estimator`

- TaskTraits/TaskDescriptor；
- API不得忽略OperationId/traits；
- CPU fallback明确。

## 4. 第二批提交：画像

### CPU

- Google Benchmark接入；
- 算术、STREAM、reduction、ISA/线程/NUMA；
- 原始JSON和profile拟合。

### GPU

- 解决 `nvcc 11.8 vs MinGW g++ 16.1.0` 等实际工具链问题；
- 优先采用官方支持组合；
- BabelStream、transfer、launch、reduction、卷积和不规则能力；
- 无GPU/无法编译则明确SKIPPED并不得进入合并阶段。

## 5. 第三批提交：调度和控制

- CostEstimator；
- 动态共享工作池；
- 真实CPU+GPU；
- guided尾部收缩；
- profile hash不变；
- 真实利用率读取或明确估算；
- RAM/VRAM预算。

## 6. 测试结论重新命名

- 原 `e13_mixed` 若 `enable_gpu=false`：改名为 `cpu_partition_coverage`；
- 原人工利用率单测：改名为 `controller_math_unit`；
- 原 `sanitizer_smoke` 若未开启工具：改名为 `lifecycle_smoke`；
- 只有真实硬件/实际构建证据齐全后，才能新增正式 Mixed、Utilization、Sanitizer结果。

## 7. 合并前硬门禁

- 不再生成旧routes；
- HardwareProfile schema和留出验证通过；
- Public API调用链通过集成测试；
- 至少一张真实GPU完成画像和Mixed；
- 95%控制不是人工输入；
- Sanitizer实际开启；
- Evidence单一HEAD；
- 算法目录零修改；
- 普通AstroCS无副作用。
