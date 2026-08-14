# API Inventory（V15）

## Production 入口（唯一）

| 阶段 | 入口 | 状态 |
| --- | --- | --- |
| Phase1 | `orchestrator.exe <stage1.json>` | CANONICAL |
| Phase2 | `astrocs-stage2.exe <stage2.json>` | CANONICAL |
| Browser | `healpix_browser_qt.exe` | CANONICAL（只消费，不解释科学数据） |
| ACR | lib/acr（备用加速基座；phase2 经 KernelRegistry 使用同一 contract） | BACKEND_EQUIVALENT |

## V15 变更的公共接口

### rejection.h（V15 重写）

```text
P2RejectionMethod（0..10 枚举值不变；P2_REJECT_SIGMA=1 alias canonical
                  robust_mad_clip）
P2RejectReason / P2RejectStatus（stack status 与 per-sample reason 分离）
P2SigmaParams / P2LinearFitParams / P2EsdParams / P2PercentileParams /
P2MinmaxParams / P2RcrParams（method-specific typed；禁止共享 low/high）
P2RejectionPlan（explicit method + minimum_n + typed params）
P2RejectionPlanRequest（auto 解析请求；nominal contributors；profile）
p2_reject_plan_resolve / p2_rejection_semantic_id
P2EligibilityInput / P2EligibilityOutput / p2_eligibility_filter
P2CandidateStack / P2RejectionDecision / p2_reject_stack_ex
P2RejectionWorkspace / p2_rejection_workspace_create/free
P2SampleStackView / P2RejectionResult / p2_reject_stack（COMPAT）
```

### sampler.h（V15）

```text
p2_sampler_default_config()（默认值单一来源；修复 null-config 未初始化）
```

### stage2_common.h（V15）

```text
reject_method（默认 auto）/ reject_profile（wbpp_current）/
reject_underdetermined_n（2）/ 各方法 typed 参数（sigma_lower 等）/
deprecation_warnings（legacy low/high/max_iterations/min_samples adapter）
```

## 未变（冻结 ABI）

```text
p2_coverage_build/free / p2_sample_controls / p2_upm_* / p2_block_plan /
p2_integrate_pixel / p2_frame_id / p2_stats_median/mad
aio_*（astro_image_io 全部）
```

## 状态/错误所有权（V14 合同不变）

- 返回码 0=OK；非 0 语义各头文件独占定义；`err` 只承载日志文本；
- C ABI 不抛异常；buffer ownership/lifetime/nullable/单位逐参数注释；
- 日志与状态分离（run/logs/<module>/）。

## 工具 CLI

```text
astrocs-stage2 / orchestrator / healpix_browser_qt / rejection_cli
（--plan JSON 模式 + --reasons；旧位置参数模式为 COMPAT）
phase2_synthetic_gate.exe（gate 测试入口，V15 新增 12 个 rejection/config 测试）
```
