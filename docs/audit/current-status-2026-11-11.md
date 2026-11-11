# ACSD V2 当前证据快照（2026-11-11）

> 本页记录 `rebuild/acsd-v2` 在 2026-11-11 的实际可复核证据与未闭合项；不替代 `IMPLEMENTATION-STATUS.md` 的历史冻结结论，也不把设计/旧 PASS 当当前全链认证。当时 HEAD 见文末；X01 证据 SHA 单独列出。

## 1. 已复跑的全链

`product_profile=full` 的 X01 8 帧链（`validation/x01/e2e_x01.py`）：

```
normalize 8/8, mosaic 1, export 1, exit 0
mosaic/export: profile=full, science=conditional, noise=unknown
blind RMS = 0.21085254580257834 px
48 ratio residuals: max |z| = 1.9006702794601922, 48/48 <= 3s
historical 5% = 0.10333067715982192 FAIL (retained, superseded)
```

证据 SHA：`2e91dc801`（含 UBSan 修复后的最后一次全链）。`validation/x01/comparison.json` 记录该 SHA。

## 2. 已闭合的链上缺陷（本轮系列）

| 主题 | 证据/提交 |
|---|---|
| support states 重开不再当全支持 | `6889aa307`、`read_states` 具名失败 |
| coadd/sky 缺 support 拒绝 | `7775f6a14` |
| DQ 缺失 = conditional，不丢到 export | `aa1c7931e` |
| full 协方差：sky 控制协方差 + coadd sparse joint | `14eb2f815`，2 帧 full 秒级发布 |
| 单位链 `sr^-1` 信号 / `sr^-2` 方差 | `1b39d0316`；X01 复测一致 |
| `C_out = Q C_leaf Q^T` | `177fde19c`，X01 rel 4.8e-16 |
| assessment/state 一致 | `f6e49552d`，语义对象 0 violation |
| 合成 CSR Index 身份 | `4ebdd3136`，revision/schema 不再垃圾 |
| UBSan 未初始化 bool | `2e91dc801` |

## 3. 仍开（不能宣称 D0/D1 PASS）

1. **帧噪声仍 unknown**：P1-5 θ random influence 缺 `H` 因子；无 DQ 证据；控制协方差未测。full 产品是 conditional，不是已知协方差。
2. **C0 无 driver**：information.reference 已补 typed driver；coadd.select/weights、information.scene、photometry.apply、projection.export、sky.controls/fit、uncertainty、operators、drizzle 仍缺 process-level driver。
3. **B0/D0**：Export 正本已写，但系统性偏差的有限界实例、D0 对证据工具的独立性批评（类型过滤、Q 两侧同源）未完全闭合。
4. **B5 边界**：部分支持、跨 face、极大目标像元、重复 pixfrac 未覆盖。
5. **C2**：Windows/双平台、GUI 协议、真实性能未验收。
6. **C1**：完整 `acsd` 主程序未在 ASan 下运行；TSan、掉电、满盘未跑。
7. **真实 shared-master 多帧链**：现有多叶/shared-root 反例是合成探针，未在真实 normalise 链触发。

## 4. 证据工具

- `validation/product/assessment_invariants_probe.py`：语义对象 assessment 不变量，X01 0 violation；
- `validation/product/projection_covariance_probe.cpp`：`C_out=QCQ^T`，X01 rel 4.8e-16；
- `validation/product/shared_root_projection_probe.cpp`：单/双 shared root，多叶投影误差 0；
- `validation/projection/export_constant_probe.cpp`：常场解析、无支持 NaN、负/零信号、单位界；
- 上述探针在 UBSan 与 `-static-libasan` 的 ASan 下通过（生产静态库未整体 ASan 编译）。

## 5. 2026-11-11 追加：未知协方差不可 apply

`covariance_apply` 现在在 `NoiseModel.state != known` 时返回 `unavailable`（以前只拒绝带 material unknown term 的模型）。typed driver `uncertainty.covariance` 正/负例通过。

因此 X01 full 产品（`noise=unknown`）的 `C·1` / `C_out=QCQᵀ` **不再有数值证据**：这些探针现在正确报告“apply refused”。`C_out=QCQᵀ` 的代数关系只在**已知协方差**的合成反例（`shared_root_projection_probe`，`max_abs_err=0`）上验证。此前在 unknown C 上给出的 4.8e-16 数值不再作为当前证据。
