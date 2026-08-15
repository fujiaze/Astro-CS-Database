# Module: plate_solve (ipv)

## 职责

星表匹配 + TAN/SIP plate solve → WCS。

## 非职责

不重采样图像。

## Public API

ipv_solver DLL。

## Data contract

星点表 + 参考表 → WCS 结构（CD/SIP/残差）。

## Ownership

求解器状态 RAII。

## Thread safety

实例级独立；静态常量只读。

## Errors

几何退化/星不足 → PLATESOLVE 失败。

## Science IDs

SCI-AST-001；ALG-PLATESOLVE-*。

## Tests

合成投影恢复（AST-001..008）；SIP 前向 A/B 一致性。

## Source files

lib/plate_solve/cpp/ipv/。
