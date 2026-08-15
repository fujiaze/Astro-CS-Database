# Plate Solve（IPV）

关联：SCI-AST-001；模块：lib/plate_solve/cpp/ipv。

## 输入

星点表 + 参考星表（Gaia）。

## 输出

WCS（CD + SIP）或失败状态。

## Preconditions

≥最小星点数；几何非退化。

## Postconditions

前向投影与星点一致（残差报告）；WCS 头可写。

## Invariants

坐标约定 J2000 + TAN/SIP；CD 与 SIP 一致（A/B 双前向，V18R3 审计）。

## 复杂度

匹配 O(n log n)；退化降级路径显式。

## 并行模型

单线程求解（阶段内串行）。

## 数值风险

极区/大畸变；SIP 高阶振荡 → 阶数上限。

## fast/reference/oracle

合成投影图恢复；与 astrometry.net 语义对照（工程控制 05 spec）。

## ID

ALG-PLATESOLVE-*；TEST-AST-*。
