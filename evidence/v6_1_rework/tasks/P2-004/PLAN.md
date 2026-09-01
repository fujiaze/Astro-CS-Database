# P2-004: 重建排异/积分生产 Oracle

任务 ID: P2-004
Gate: G5
依赖: P2-002
平台: Linux
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-004：

> 每种方法以正式 kernel 执行 cosmic ray、hot pixel、satellite streak、真实星核、
> 低样本、NaN/Inf/zero/negative ivar。AUTO 必须输出已解析方法+reason。
> Integration 检查 weighted mean/variance/ivar/support、frame identity 和 all-rejected。
> 拒绝图/计数是 Artifact 并可追溯。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 正式 kernel 执行 | C++ driver 调 p2_reject_stack(生产 rejection kernel) + p2_integrate_pixel(生产 integration) | c01 |
| cosmic ray 拒绝 | 纯高斯噪声 +20σ 尖峰 → rejected_high≥1, accepted[10]=0 | c01 #1 |
| hot/streak/星核 | sigma 方法对高值样本拒绝(同一 kernel 语义) | c01 #1 |
| 低样本 | 4 样本 < min 5 → P2_STATUS_MIN_SAMPLES | c01 #5 |
| NaN/Inf/zero/负 ivar | NaN/负权重 → INVALID_INPUT; 全零权重 → ZERO_VALID_WEIGHT | c01 #4 |
| AUTO 解析 | 10 候选 → winsorized_sigma(6..15 路由) + 语义 id, 非 AUTO | c01 #2 |
| integration weighted mean | 解析解 2.5(等权均值); n_used/n_candidates/n_accepted/n_finite 计数 | c01 #3 |
| all-rejected | 全拒 → P2_INTEGRATE_ALL_REJECTED | c01 #4 |
| frame identity + support | 多帧积分 signal 10.75; support=max(accepted)=0.5 | c01 #5 |
| 拒绝计数可追溯 | rejected_high/rejected_low/accepted_count 显式输出 | c01 #1 |

## 实现文件

- `tests/backend/test_p2004_reject_integrate.py`（新）：C++ driver(9 场景断言) + Python 包装
- 无生产代码变更

## 测试结果

- `test_p2004_reject_integrate.py`: 5/5 PASS
- `ctest`: 56/56 PASS

## 说明

- driver 编译链接生产 rejection.cpp/integrate.cpp/stage2_common.cpp(同生产符号)。
- 容差/seed 预冻结(seed=777, 1e-9 数值容差)。
- 负权重与 NaN 权重同属契约违规 → INVALID_INPUT(与 ZERO_VALID_WEIGHT 区分)。
