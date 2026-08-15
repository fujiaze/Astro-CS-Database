# UPM Solver

关联：SCI-UPM-001..010、SCI-UPM-PERSIST-001、ALG-UPM-FRAME-BIND-001、
DATA-UPM-MODEL-001；模块：lib/phase2/src/upm.cpp。

## 输入

control 观测（多帧）+ config（robust/weight/anchors/连通分量）。

## 输出

Model：C[frame][control] + frame_index + frame_id_by_index + components。

## Preconditions

观测有限；frame_id 唯一（构建侧去重）。

## Postconditions

- 每帧校准 calibrated = raw − C(frame, leaf)；
- 每分量 gauge = 最小 frame_id；
- persist：frames 列表与 C 行序一致（frame_id_by_index）。

## Invariants（ALG-UPM-FRAME-BIND-001）

```text
len(parameter_rows) == len(frame_id_by_index)
frame_id_by_index 无重复
绑定只由 frame_id 决定；禁止有序 map 遍历推断
```

## 伪代码

```text
collect frames(set) → 建立 cell/control → 连通分量 → 每分量
Huber IRLS（SNR/ivar 权重 + 弱零锚 + 平滑）→ 收敛检查
→ hash → save(sparse json) / materialize dense cache
```

## 复杂度

IRLS O(iter × (obs + K log K))；K=controls。

## 并行模型

块级求值 OpenMP；求解串行（reference）。

## 数值风险

收敛/退化分量；sigma_floor=0；NaN 权重 → INVALID_INPUT。

## fast/reference/oracle

dense cache = sparse 等价（1e-12）；G1 空间真值；PR-UPM-001..010
持久化绑定门。

## ID

ALG-UPM-SOLVE-001..；TEST-PR-UPM-001..010。
