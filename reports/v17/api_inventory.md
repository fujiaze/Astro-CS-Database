# API Inventory（V17 True Final Freeze）

## Production 入口（唯一）

| 阶段 | 入口 | 状态 |
| --- | --- | --- |
| Phase1 | `orchestrator.exe <stage1.json>` | CANONICAL |
| Phase2 | `astrocs-stage2.exe <stage2.json>` | CANONICAL |
| Browser | `healpix_browser_qt.exe` | CANONICAL（只消费，不解释科学数据） |
| ACR | lib/acr（备用加速基座；phase2 经 KernelRegistry 使用同一 contract） | BACKEND_EQUIVALENT |

## V17 冻结的公共接口

### rejection.h（V15 重写 + V16/V17 收尾）

```text
P2RejectionMethod（0..10 枚举值不变；P2_REJECT_SIGMA=1 alias canonical
                  robust_mad_clip；AUTO=10 只在 planning 层，永不进 kernel）
P2RejectReason / P2RejectStatus（stack status 与 per-sample reason 分离；
                  V17：INVALID_METHOD=6 / INTERNAL_ERROR=7；Stage2/ACR 只
                  接受 OK / UNDERDETERMINED，其余必须 hard fail）
P2SigmaParams / P2LinearFitParams / P2EsdParams / P2PercentileParams /
P2MinmaxParams / P2RcrParams（method-specific typed；禁止共享 low/high）
P2LargeScaleParams / p2_large_scale_apply（V17：
                  astrocs.large_scale_rejection.v1，per-frame low/high
                  mask connected-component grow，min structure size +
                  low/high 独立半径）
P2RejectionPlan（explicit method + minimum_n + typed params +
                  large_scale 后处理参数）
P2RejectionPlanRequest（auto 解析请求；nominal contributors；profile：
                  wbpp_2_9_1 冻结 / astrocs_adaptive 独立）
p2_reject_plan_resolve / p2_rejection_semantic_id
P2EligibilityInput / P2EligibilityOutput / p2_eligibility_filter
P2EligibilityGatherInput / P2EligibilityGatherOutput /
p2_collect_candidate_stack（V16 生产 strided collector；CPU/ACR 统一路径）
p2_validate_candidate_weights（V17：SNR lookup 后统一非 finite/非正权重
                  校验）
P2CandidateStack / P2RejectionDecision / p2_reject_stack_ex
P2SampleStackView / P2RejectionResult / p2_reject_stack（COMPAT adapter，
                  生产 Stage2 不再调用）
```

> 已删除：`P2RejectionWorkspace` / `p2_rejection_workspace_create/free`
> （V17 不再存在；PUBLIC_API.md 与头文件一致）。

### integrate.h（V17）

```text
P2_INTEGRATE_OK=0 / NO_CANDIDATES / ALL_REJECTED / ZERO_VALID_WEIGHT /
INVALID_INPUT（显式状态机，不靠 n_used 猜原因）
非 finite weight/support → INVALID_INPUT，绝不返回 OK+NaN
output support 唯一 canonical reducer = max(accepted support)；Stage2/ACR
只消费 pr.support，不再二次 max/mean
weight 政策明确分开：stack.support_x_snr2.v1 / stack.equal.v1（积分权）
vs upm.robust_control_weight.v1（UPM 控制点权）——禁止再混叫 snr2_normalized
```

### stage2_common.h（V17）

```text
reject_method（默认 auto）/ reject_profile（wbpp_2_9_1 冻结；wbpp_current
仅 migration alias，解析到 2.9.1，diagnostics 序列化 wbpp_2_9_1）/
reject_underdetermined_n（2）/
reject_normalization（astrocs_median_center_v1 canonical）/
large_scale_enabled + min_structure_pixels + low/high grow radius/
各方法 typed 参数（sigma_lower 等）/
legacy low/high/max_iterations/min_samples：已从 parser 删除（V17 硬错误，
旧 config 必须 tools/migrate_stage2_config.py 显式迁移）
```

### sampler.h（V15 不变）

```text
p2_sampler_default_config()（默认值单一来源；null-config 与显式同语义）
```

## 冻结 ABI

```text
p2_coverage_build/free / p2_sample_controls / p2_upm_* / p2_block_plan /
p2_frame_id / p2_stats_median/mad
aio_*（astro_image_io 全部）
```

## 状态/错误所有权（V14 合同不变）

- 返回码 0=OK；非 0 语义各头文件独占定义；`err` 只承载日志文本；
- C ABI 不抛异常；buffer ownership/lifetime/nullable/单位逐参数注释；
- 日志与状态分离（run/logs/<module>/）；
- 不允许"API 调用成功但科学状态非法"：rejection INVALID_* 由调用方 hard
  fail，integration INVALID_INPUT 显式状态。

## 工具 CLI

```text
astrocs-stage2 / orchestrator / healpix_browser_qt / rejection_cli
（--plan JSON 模式 + --reasons；--plan 支持 normalization 与 WBPP Light
typed 默认（linear_fit 5.0/3.5、percentile 0.2/0.1），与 stage2 一致）
phase2_synthetic_gate.exe（gate 测试入口；V17 74/74 PASS，含 5 个
large_scale 单元/配置测试）
```
