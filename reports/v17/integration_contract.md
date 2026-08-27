# Integration / Eligibility 最终契约（V17 True Final Freeze）

## Candidate eligibility（唯一 collector）

```text
finite(value)
valid
finite(support) && support > threshold（stage2 默认 0.0）
quality policy（control 级；像素级无 quality 数组 → nullptr 并记录）
finite(weight) && weight > 0（SNR lookup 后经 p2_validate_candidate_weights）
```

CPU / ACR / compat 全部经 `p2_collect_candidate_stack`（V16 strided
collector）；V17 新增 `p2_validate_candidate_weights` 堵住 SNR lookup 后
的漏检。

## Rejection status 契约

```text
可继续：P2_STATUS_OK / P2_STATUS_UNDERDETERMINED
hard fail：P2_STATUS_INVALID_INPUT / P2_STATUS_INVALID_CONFIGURATION /
          P2_STATUS_INVALID_METHOD / P2_STATUS_INTERNAL_ERROR
```

Stage2 CPU 对非法状态 return 6；ACR 同契约；CLI --plan 校验组合。

## Integration status（显式枚举）

```text
P2_INTEGRATE_OK
P2_INTEGRATE_NO_CANDIDATES
P2_INTEGRATE_ALL_REJECTED
P2_INTEGRATE_ZERO_VALID_WEIGHT
P2_INTEGRATE_INVALID_INPUT
```

任一非 finite weight/support → INVALID_INPUT（绝不 OK+NaN）。

## Support 唯一 canonical reducer

```text
output support = max(accepted support)
```

历史语义确定依据：V13/V11 冻结 product 回归（覆盖并集保守下界）。
Stage2 CPU / ACR / large_scale 两遍路径只消费 `pr.support`，不再第二次
max/mean。

## 权重命名（不再混名）

```text
stack.support_x_snr2.v1  —— 积分权重（stage2 weight_mode=auto）
stack.equal.v1           —— 等权（weight_mode=equal 或 weights=nullptr）
upm.robust_control_weight.v1 —— UPM 控制点权重（不同语义）
```

## 测试

```text
V17NonFiniteWeightInvalid / V17NonFiniteSupportInvalid /
V17StatusesExplicit / V17InvalidMethodStatus
```

```text
INTEGRATION_CONTRACT = FROZEN
```
