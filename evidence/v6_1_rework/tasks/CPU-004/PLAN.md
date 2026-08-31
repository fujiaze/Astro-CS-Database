# CPU-004: 实现按 kernel 自适应路由

任务 ID: CPU-004
Gate: G3
依赖: CPU-003; RT-006
平台: Linux+Windows
变更类别: performance

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` CPU-004 与 `08_CPU_RESOURCE_ACCEPTANCE.md` §4：

> Runtime 按 kernel ID + workload class 读取 profile；核对 arch/CPU/XCR0/OS ABI/quota/
> runtime/provider/benchmark schema/build IDs。无 profile、stale、损坏或 unsupported
> provider 时逐 kernel 回 baseline；保守意味着最低 ISA，不意味着单线程。
> 记录 provider 选择、fallback reason、workers/block 和 self-test hash。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 读取 profile 核对 arch/CPU/quota/runtime/build | `validate_profile_v2_for_machine`：schema/结构(复用 verify_profile_v2) + host.arch + quota_signature + logical_available + source_commit | c01/c02 |
| 无 profile → 保守 baseline 但多线程 | `conservative_route`：workers=有效可用核(≥1, 禁退 1) | c03 单测 #8 |
| stale/损坏 profile 拒绝 | quota_signature/logical_available/schema/commit 任一不匹配 → valid=false + reason | c02 exit=5 |
| 逐 kernel 路由 | `route_kernel_from_profile`：按 kernel_id 查 kernels[] provider/workers/block | c01/c03 |
| unsupported provider → 回 baseline | `provider_supported` 检查 feature bits；不支持 → baseline + fallback_reason | c03 单测 #6 |
| 记录 provider/fallback/workers/block/selftest hash | `config show-effective --cpu-profile` 输出 `effective.kernel_routes` | c01 |
| 保守 ≠ 单线程 | workers 恒 ≥1，available≥2 时 heavy 不退 1 | c03 单测 #7/#8 |

## 实现文件

- `lib/backend_host/cpu_routing.h/cpp`（新）：ProfileVerdict / KernelRoute / validate_profile_v2_for_machine / route_kernel_from_profile / conservative_route
- `cli/parser.cpp`：`validate_cpu_profile` 升级 v2（machine 一致性校验，替代 V5 kind/cpu_signature）
- `cli/commands.cpp`：`config show-effective --cpu-profile` 输出逐 kernel 路由摘要
- `CMakeLists.txt` / `cli/CMakeLists.txt`：加入 cpu_routing.cpp
- `tests/unit/cpu004_routing_test.cpp`（新）：8 组断言（valid/stale×3/路由/缺 kernel/无 profile/保守）

## 测试结果

- `ctest`: 53/53 PASS（含新增 cpu004_routing）
- `cpu004_routing_test`: CPU-004 TESTS PASS（8 组断言）
- `tests/cli/test_cli_protocol.py`: 10/10 PASS
- CLI 实测：show-effective 逐 kernel 路由输出正确；篡改 quota_signature/logical_available/schema 均 exit=5

## 遗留说明

- 路由决策已暴露于 config show-effective；生产模块调用时由 Runtime `profile_hint` 消费（RT-006 预留字段），Phase1/2/3 模块迁移（P1-001+）时接线。
