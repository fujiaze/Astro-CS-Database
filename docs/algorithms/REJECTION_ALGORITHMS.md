# Rejection Algorithms

关联：ALG-REJ-001..008（RJ-001..008）；模块：lib/phase2/src/rejection.cpp。

## 输入

candidate stack（values/support/weights/frame_ids）+ 计划（method/profile）。

## 输出

decision（accept/reject + reason + status）。

## Preconditions

方法合法（AUTO 不进 kernel）；参数 typed。

## Postconditions

- INVALID_* → hard fail；UNDERDETERMINED → 不做猜测；
- large_scale：结构生长（trail）不生长 compact cosmic。

## Invariants

status/reason 分离；low/high 阈值独立；支持度掩码有效。

## 复杂度

O(n log n)（排序/ESD/RCR）。

## 数值风险

ESD 双 sqrt（已修 RJ-004）；NONE 方法 NaN（已修 RJ-002）。

## fast/reference/oracle

NIST ESD / Rosner 对照；卫星线注入门（recall=1.0）。

## ID

ALG-REJ-001..008；TEST-REJ-*。
