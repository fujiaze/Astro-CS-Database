# SCI-005 Rejection/Integration 规则与图层语义 —— 核对 PASS

> G3 任务：固定每个 rejection method 的样本数边界、中心/尺度估计、阈值、winsorization、
> 原因码；fixed rejected sample 是否进入 support/variance、fallback 的含义和输出图层。
> 判定：**PASS**（`docs/science/REJECTION.md`(SCI-REJ-001..008) 与
> `docs/science/INTEGRATION.md`(SCI-INT) 已冻结，且与代码逐点一致，无需修改）。
> 复核：2026-08-27。

## 1 方法边界 / 阈值（auto 路由，按 nominal n）

| n | method | lower / upper | iter | 代码 |
|---|---|---|---|---|
| n < 6 | percentile | 0.2 / 0.1 | — | `rejection.cpp:1083` |
| 6 ≤ n ≤ 15 | winsorized_sigma | 4.0 / 3.0 | 8 | `rejection.cpp:1076-1077` |
| n > 15 | linear_fit | 5.0 / 3.5 | 8 | `rejection.cpp:1080-1081` |
| sigma / averaged | — | 4.0 / 3.0 | 8 | `rejection.cpp:1075,1078-1079` |
| ESD | alpha | 0.05 | max 10 | `rejection.cpp:9` |

## 2 原因码 / 状态

- `P2_REASON_*`：`ACCEPTED / REJECTED_LOW / REJECTED_HIGH / UNDERDETERMINED`（`rejection.h:76`）。
- `P2_STATUS_*`：`OK / MIN_SAMPLES / ALL_REJECTED / INVALID_INPUT / UNDERDETERMINED / ...`（`rejection.h:83-86`）。
- `UNDERDETERMINED`：n ≤ `underdetermined_n(=2)` 或 n < `minimum_n`（`rejection.h:153-154`）。

## 3 rejected → support/variance / fallback / 输出图层

- **rejected 不进入 support/variance**：`INTEGRATION.md` `valid(i) ⇔ accepted(i) ∧ finite(values[i]) ∧ (support 空 ∨ (finite∧>0))`；仅 `accepted[i]==1` 进有效集；`support=max(accepted support)`（canonical reducer，`integrate.h:17-18`）。rejected 样本不贡献 signal/support/variance。
- **fallback 含义**：`UNDERDETERMINED (n≤2) → 不做猜测，全接受`（`REJECTION.md:62,79,86`；`P2_REASON_UNDERDETERMINED`，recall=0 显式，不宣称可剔）。
- **输出图层**：`P2PixelResult{ signal; support(=max accepted support); n_used; n_finite; status(P2IntegrateStatus) }`（`integrate.h:53-62`）。输出支持信号/support/n_used/n_finite/status 字段。

## 4 结论

REJECTION/INTEGRATION 规则与图层语义已冻结且与代码一致（阈值逐点核对 `rejection.cpp:1075-1083`），
无修改项。SCI-005 判定 PASS。
