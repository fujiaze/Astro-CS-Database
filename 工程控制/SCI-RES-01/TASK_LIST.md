# 任务列表（SCI-RES-01）

| ID | 课题 | 依赖 | 状态 | 交付 |
|---|---|---|---|---|
| R-001 | 稀疏帧内 SNR 层控制点密度 | — | NOT_STARTED | 结论 + 反例 + 文档修订提案 |
| R-002 | 标量门判据与阈值 | — | NOT_STARTED | 同上 |
| R-003 | 暗场-亮场曝光容差定义（值为已裁决 5 s） | — | NOT_STARTED | 同上 |
| R-004 | 已裁决值落文档（5 s + R-003 定义） | R-003 | NOT_STARTED | 批准后的文档修订 |
| R-005 | drizzle pixfrac 默认值的权威出处 | — | NOT_STARTED | 结论 + 出处 + 提案 |

## 依赖图

```text
R-001 ┐
R-002 ├─（互不依赖，可并行）
R-003 ┴── R-004
R-005 （独立）
```

## 状态词

NOT_STARTED / IN_PROGRESS / CONCLUSION_READY / BLOCKED / LANDED（批准并落文档后）
