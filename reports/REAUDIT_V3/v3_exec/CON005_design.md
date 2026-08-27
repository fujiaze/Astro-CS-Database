# CON-005 UPM build/solve 并行化 — 实现设计
溯源: lib/phase2/src/upm.cpp（1336 行），P2UpmModel build+solve。

## 已确认结构（read/grep 核验）
- 观察构建（L293-325）：逐 obs 映射到 control cell；`if(!nodes) add_control(...)`；否则只读 cell_index，`m->controls[k].obs_idx.push_back(i)` + `frame_ids.insert(o.frame_id)`。
- 邻接图（L356-410）+ 求解前连通分量（L412-471）`component_count_solve`，**每分量独立 gauge**（参考帧最小 content-stable frame_id C=0），输入顺序无关、断开分量不虚构跨组件约束。
- 求解/权重：`huber_rho/huber_w`（L194-200，Huber delta=1.345），`zero_anchor_weight=1e-3`，`max_iterations=100`，`use_ivar_weight=1` 默认。

## 并行设计（满足 CON-005 约束）
1. **observation 构建必须并行**：只读 cell_index 的 map compute（每个 obs 独立算 tile/gx/gy/control_idx）并行；但由于 `obs_idx` 需保持升序 obs 索引（确定性），采用 **parallel-compute + deterministic merge**：每线程算 (obs_idx, control_idx) 对，按 obs_idx 排序后按序 append（不得并行 append 到共享 vector）。`frame_ids` 用 per-thread set 并集（set 顺序确定）。`if(!nodes)` 的 add_control 变异共享状态 → **该路径保持串行**（生产 nodes 非空，不走此路径）。
2. **按 frame/control 聚合必须并行**：`m->C`/`m->obs_w`（F×K）按 (frame,control) 填充，逐观测独立（每 obs 隶属唯一 frame+control）→ 逐行并行写入，无冲突。
3. **SpMV/残差必须并行**（真实热循环）：IRLS 每迭代，每 observation 一行的设计矩阵作用 => per-row（per-obs）并行 SpMV + 残差；约简到 C 的更新用固定行号顺序（per-thread 累加再定序合并，避免浮点非确定性）。预分配临时区（`A_rhs`、`A_diag` 等），禁止 per-obs/per-pixel heap allocation（复用 scratch）。
4. **保持固定顺序（不得并行/不得改变）**：gauge（零锚/参考帧）、连通分量、收敛判据、最终归并入 C——串行且固定序（规范明示）。
5. **OpenMP 门控**：仅 `#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)` 且 worker>1；默认 OFF 恒串行（行为不变）。worker 来自 ExecutionOptions（参考 CON-004 在 stage2.cpp 的 `cfg.exec.cpu_workers` 灌引；若 P2UpmConfig 无 cpu_workers 字段，加一个并灌引）。

## 测试（Linux 可跑，UPM synthetic 无 HiPS fixture）
- 新增 `upm_parallel_consistency_test`：同一 P2UpmModel build（常量场/已知加性场/Huber 污染/断开分量/空/退化）分别 1T/2T 运行，断言 C/m 收敛值 exact（结构）+ 浮点按 SCI 容差；1T/2T 逐项一致。
- 复用 synthetic_gate 的 Phase2Upm S0/S1/S2/G1SpatialFieldTruth 等（**Linux 可跑**）确认并行不改变结果。

## 风险与必要条件
- obs_idx 升序 + set 并集：保证确定性（禁止并行 append 共享 vector）。
- 每分量独立 gauge：并行只能作用于分量**内部**的 SpMV/残差；分量级循环（连通分量、gauge、归并）串行固定序。
- 浮点约简顺序固定：per-thread 累加后按行号定序合并，避免 1T/2T 浮点漂移。
- 完整编译 OFF(默认)+ON(openmp-on)；跑 Phase2Upm* 全组确认无回归。
