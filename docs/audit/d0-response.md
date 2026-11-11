# D0 独立复核响应（2026-11-11）

依据 `docs/audit/d0-independent-review-2026-11-11.md`（HEAD `63c21e6a5`）。以下按报告条目给出处理状态与本轮证据。

## P0-1 缺失 support states 仍当全支持

**接受**。`read_states` 在 `!array` 时返回 `ok(empty)`，`build_export` 的 `states.empty()` 使所有几何重叠叶全权重进 Q。已改为返回 `unavailable`（`the support map carries no states array`），与 coadd/sky 的拒绝一致。正常常场/旋转/镜像探针仍通过；X01 full 带 states，未受影响。

## P0-2 单位链把 sr⁻¹ 发布成 sr⁰

**接受**。根因在 `control_covariance`：`C'=T C Tᵀ` 但未把 `unit(T)` 作用到方差单位。现 `variance_unit = unit_multiply(source.variance_unit, unit_square(T->spaces.output))`。X01 复测：

```
signal_unit=…*1/sr (sr=-1, L=1)
var_unit   =…*1/sr (sr=-2, L=2)
cov_unit   =…*1/sr (sr=-2, L=2)
```

## P1-5 identity 不依赖内容

**部分接受并修复**。export 的 SignalEstimate/NoiseModel 与 coadd 的 estimate identity 现在包含实际 signal array id、noise id、单位幂与 state/unknown 数，不再是 `(count,state,quantity)` 的裸哈希。其余对象（Index/Array 等）仍以结构哈希为主，下一轮继续。

## P1-1 state==2（undecided）语义

**接受**。coadd 与 export 现在把 `state != 1` 视为“非确定支持”，并记录 `at_least_one_…_undecided_support_state`、置 conditional/incomplete；不再一边排除、一边全权重包含。

## P1-2 mean 归一分母下界为 0

**接受**。`total_lower == 0` 时系数上界不再置 0，而是 `+inf`；export 记录 `the_normalisation_denominator_is_not_bounded_away_from_zero` 并置 conditional/incomplete。

## 证据工具

**接受批评**。`assessment_invariants_probe.py` 的类型过滤确实漏掉 `SupportMap` 等；`projection_covariance_probe` 的 Q 两侧同源。下一轮把扫描改为“所有带 assessment 的对象”，并增加 shared-root/多叶的 Q 对照；当前报告不把这两项当作“全称证明”。

## 仍开

- P1-3 bind 的 complete=true 与 omitted 并存（本轮只传播参数协方差未知，未全面重审）。
- P1-16 `Ω(E_q)`/native_area 区间仍取中点。
- identity 覆盖面、证据工具独立性、真实 shared-root/多叶投影反例。

## 补充：shared-root / 多叶投影反例（本轮新增）

`validation/product/shared_root_projection_probe.cpp` 独立构造 2 叶、1 个 shared root、影响 \(L=[1,2]^\top\)，以及 \(Q=[[1,0],[1,1]]\)。解析真值
\[
C=LL^\top=\begin{pmatrix}1&2\\2&4\end{pmatrix},\quad
QCQ^\top=\begin{pmatrix}1&3\\3&9\end{pmatrix}.
\]
生产路径 `make_covariance → control_covariance → covariance_apply` 给出完全相同矩阵，`max_abs_err=0`。这触发了 shared root 的跨叶交叉项与多叶投影，补上了 X01 `nleaf=1/shared_roots=0` 未覆盖的证据面。

**仍开（新发现）**：把同一 `SharedId` 拆成两个 root（各自影响不同叶）时，`make_covariance` 在非 ASan 构建下段错误、ASan 下报 `an influence is missing`。该合并路径在真实数据中出现 shared master（bias/flat/dark）时会触发，应作为下一轮 P1 优先修复；当前不声称 shared-root 合并已验证。
