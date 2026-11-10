# 阶段报告 02｜独立审查、修复与 X01 全链裁决（2026-11-10）

> 分支 `rebuild/acsd-v2`，远端 `2bc4f9147`。本报告只记录实际运行/独立复算得到的结论；设计存在、代码登记、单测通过都不作为科学正确性证据。

## 1. 提交序列（本节新增）

| SHA | 内容 |
|---|---|
| `77a13c89c` | 四份独立审计的 P0/P1 第一批修复（geometry/statistics/normalize/runtime + 跨模块） |
| `b600b9e2d` | CLI 用用户 `Limits` 建唯一 Services（runtime P0-3 的缺失半边） |
| `1f48c58ad` | X01 改为 signal_only，omitted noise 具名 `no_noise` |
| `0f8e362f5` | BUNIT 保持标准 FITS 单位，模型 scale 进 ACSUNIT |
| `79346a8ee` | 第二轮修复：R1/R1b/R2/R3、apply_sky 帧身份、coadd 外层 influence、export 单位/domain/issue、geometry seam/A_L=0/状态/端点/性能、CSR domain 列 |
| `748c431d1` | R4 共享根时 covariance state=unknown |
| `54e57e846` | 按帧 apply_sky 协方差不是发布估计量，改发 unknown + conditional |
| `033073ab3` | compose 中段维度校验 |
| `2bc4f9147` | 最终 X01 证据归档 |

## 2. 独立审查结论（审查者不参与实现）

- Normalize（`review-normalize-fix.md` §13）：R1 数据残差移出半径（独立合成例 1313.08 ADU → 2.449e-9 ADU；X01 新半径相对通量 1.6e-10）；R1b 谱范数收缩、R2 角色不猜、R3 fallback 重置、P1-2 state、P1-6 BUNIT 均关闭。R4 的“只记录 unknown”被裁决为不足，已补 state=unknown。X01 通过被独立判定为**不依赖 R1**（比较只读 measured_stars 与设计矩阵的 OLS sandwich；56 个通量与修前逐位相同）。
- Geometry（`review-geometry-fix.md` §11）：seam work 20000→1525、47.5s→0.36–0.43s，`Σa_U+unresolved_U=A` 精确闭合、order-12 抽样 wrong_leaves=0；A_L=0 B=2.000000；estimate 状态、union 上界、裁剪 33× 且 dyadic 逐位不变，均关闭。审查者指出报告对“1″ 缝像素仍 tolerance_met”偏乐观：实测 unresolved=1.58e-12>1e-12，是 depth；已记录。
- Statistics（`review-statistics-fix.md`）：coadd 外层 influence V=0.5（旧 2）关闭；export 内存路径值/单位/domain/issue 关闭；**按帧 apply_sky 协方差仍错**（Var 0.75 vs 1、Var(Σ) 0 vs 4；coadd V 0.4375 vs 0.625）——已改为发布 unknown+conditional，联合入口保留精确路径。产品层无法重开协方差，故这条链仍不能端到端验证。
- Runtime（`review-runtime-fix.md`）：P0-4/5/6 关闭；P0-3 主路径关闭，但 command spill 取到 0 的残留已由 workflow 请求 64 MiB 修复；contract 驱动仍不建 Services；product 假 complete 已由 signal_only + profile 拒绝修复。

## 3. X01 最终裁决（`033073ab3`，默认 block=1）

```
normalize exit 0, published 8/8
mosaic    exit 0, published 1
export    exit 0, published 1
blind solve RMS 0.21085254580257834 px (tolerance 0.5)
48 ratio residuals: max |z| 1.9006702794601922, mean 0.155974, rms 0.905425, 48/48 ≤3σ
historical 5% criterion: max relative error 0.10333067715982192, FAIL, kept as superseded
```

`comparison.json` 标 pass；旧 5% 失败保留，不隐藏。产品为 signal_only：`manifest.complete=true` 表示发布闭包完整，`assessment.complete=false/science=conditional/noise=unknown`，estimate 带 `no covariance action can be reopened` issue；无悬空 `$ref`。这证明链能跑通、信号校正/测光/天测残余在统计上一致；**不证明**产品协方差闭包、N4 PSF/noise 产品链或暗/绝对标度路径。

## 4. 仍未闭合（不得写成 PASS）

- Normalize：R4 跨帧共享根交叉块本体（需按根影响 API）、P1-5 θ 随机影响、P1-7 位置身份、X01 dark 4.8 vs 480 ADU、XP 系数全 [1,0,0]。
- Geometry：`Identity::revision` 恒 1、`make_csr_action` identity、SupportMap 真 Boolean union、`normalizer_min_sr`/`max_bytes`、`block_size>1`。
- Statistics：产品层重开 covariance（signal_only 的根因）、R6 单位链（帧 ADU/SR → mosaic 1 → export ADU）、R7 γ 常数、R8 residual_enclosure 证书串、R9 offset 别名、`CoaddRequest` POD 未初始化、exact-constraint 绝对容差、降级白名单吞执行失败。
- Runtime：contract 驱动 Limits、`close_command` 无生产调用者、未知 recipe→identity、product 侧 fsync/整数组单块/root path 约束、ingest 计费、Unit 默认初始化、事件流写失败。
- 全链只覆盖 X01 的 8+1+1；其余 21 个 process 仍是 operation 级 IMPLEMENTED_NOT_VERIFIED/PASS_SCOPED。
- 未跑 ASan/UBSan/TSan、掉电/满盘、Windows；第三方库线程安全/fenv 未认证。

## 5. 结论

本轮完成了 A0–B4 与 C1 的“审计—修复—独立复核”闭环中的主要 P0，并首次在默认算法（block=1）下跑通 X01 三命令全链且保留旧失败。它不是“ACSD V2 全部完成”：协方差产品闭包、暗/绝对标度、剩余 process 与平台可靠性仍开。后续应按 `docs/workplans/v2-science-engineering/README.md` 的 B5/C0/C2/D1 继续，且不得把本报告的 X01 pass 扩大为全系统认证。

## 6. 第二轮补充（2026-11-11，`677e1a808`）

在阶段 02 的首次 X01 通过之后，继续闭合产品协方差闭包：

| 项 | 内容 |
|---|---|
| 产品 reader | 新增 `LinearAction`/`NoiseRoot`/`NoiseModel`/`CovarianceAction` 的重开；未知或 `solve` recipe 显式失败，不再静默 identity |
| 动作身份 | 所有 action 由 kind/children/payload/size 内容寻址；CSR 的 offsets/indices/values 作为产品数组序列化并列入 recipe；diagonal/CSR 的 domain/codomain Index 与 error record 进入闭包 |
| 发布 | 只有整个协方差闭包可写时才存 noise；否则 unknown + 具名 `no_noise`，estimate 自身带 issue；profile=full 有遗漏时拒绝发布 |
| 独立 round-trip | `validation/product/roundtrip_probe.cpp`：发布带 covariance 的 full 产品，重开，`C·1 = 4·1`，0 失败 |
| 真实链 | 2 帧 mosaic 产品重开后 export 成功（旧 `action cannot be rebuilt` 消失） |
| DTO/Identity | `Unit`、`SolvePolicy`、`RejectionPolicy`、`CoaddRequest` 默认初始化；`make_identity` 接受显式 revision，refine 递增 |
| 信息 | `control_covariance` 的 unknown term influence 随 T 变换 |

X01 在 `677e1a808` 再次全链通过：normalize 8/8、mosaic 1、export 1，blind RMS 0.2109，48 残差 max|z|1.9007、全部 ≤3σ；旧 5% 失败保留。注意 X01 仍是 signal_only，所以这次通过**不**等于协方差链端到端认证；协方差链的证据是 roundtrip probe 与 2 帧 mosaic→export 实测。

## 7. 第三轮补充（2026-11-11，`5cdb27c15`）

- **产品协方差链**：新增 `LinearAction/NoiseModel/CovarianceAction` 重开、action 内容身份、CSR payload 序列化、subscription arrays/indices/children 闭包；`validation/product/roundtrip_probe.cpp` 0 失败；2 帧 full 链 normalize→mosaic→export 发布 full 产品并导出约 2279 ADU。
- **量化修正**：`apply_scale` 的绝对 \(1/a\) 使 X01 signal 达 \(5\times10^{29}\) ADU/sr（α≈2.3e16）；改为帧内中位数相对 gauge 后回到 \(8.8\times10^{13}\)，导出约 2679 ADU。X01 `xp_coefficients` 改为 `[weight,0,0]`。
- **Export**：Q 点系数改取认证区间中点（旧取下界会系统压低通量）；BUNIT 标准单位 + ACSUNIT/ACSSTD；Astropy 7 读回：WCS 往返 6.75e-11 px、数据 128×128 全 finite。
- **空协方差模型**：coadd 路径发布无根模型，export 的 `make_covariance` 拒绝发明根；已降级为 noise=null + no_noise，material issue 使 full 发布被拒绝。X01 因此使用显式 signal_only，在 `5cdb27c15` 再次通过。
- **B0**：新增乘积可辨识、同一 R/非交换、P=I−EKT 负交叉项、Gaia gauge/秩四处正向推导；B0 仍因 B5/D0 未闭合而不能 PASS。

## 8. 第四轮补充（2026-11-11，`5c320706b`）

- 尝试把 X01 三个 config 改回 `full`：normalize 8/8、mosaic 1、export 1 均可发布 full 产品，导出量级约 2679 ADU；但最终 export 的 covariance 仍无法从 coadd 模型重建。
- coadd 的 `to_csr` 在把非 CSR 的帧影响（diagonal/compose）物化时失败：`the diagonal cannot be read`。这是 full 协方差的直接阻塞点；coadd 的未知模型现在保留 root 结构，但该物化路径仍需修。
- 空协方差模型（无 root/influence）在 `product.cpp` 已降级为 `noise=null` + `no_noise`；material unavailable/unsupported issue 使 full 发布被拒绝。X01 因此保持显式 `signal_only`，在 `5cdb27c15` 通过；`5c320706b` 仅增加 coadd 根保留与诊断，不改变 X01 结果。
- 下一步最小修复：在 `to_csr` 用 impact 的声明 domain 大小（`action->spaces.domain` 或 root coordinates）而不是 `domain_size()`，并对 diagonal 读取按实际输入长度校验；然后重跑 full X01。

## 9. 第五轮补充（2026-11-11，`61531d170`）

- 修正 `compose(outer, inner)` 的 spaces：组合的 domain 必须是 inner 的 domain、codomain 是 outer 的 codomain；旧实现整套用 outer spaces，使 latent→leaf 的影响声称 leaf domain，coadd 物化时读错尺寸。
- 进一步定位 full coadd 协方差的阻塞：`to_csr` 读取帧影响数组时报 `missing_dependency`，根因是跨 Host 边界后这些数组没有 ingest 到 runtime storage。`ingest_noise` 只搬 coordinate index，不搬 factor/influence 的 payload arrays/children；临时加入的 `ingest_action` 能修复读取，但随后 `to_csr` 按 latent 列逐列 apply，2 帧即 >22 min，8 帧不可接受。因此未提交该路径。
- 正确修法是直接在 leaf/latent 稀疏结构上组合（embedding ∘ influence），避免逐列物化；这仍是 full profile 的下一前置。
- X01 在 `61531d170` 再次全链通过（signal_only）：normalize 8/8、mosaic 1、export 1，blind RMS 0.2109，48 残差 max|z|1.9007，旧 5% 失败保留。

## 10. 第六轮诊断（2026-11-11）

尝试用稀疏 `embedding ∘ influence` 替代 `to_csr` 逐列物化，并给 `ingest_action` 补齐 factor/influence 的 payload arrays/children。两项改动本身可编译，但真实 2 帧全噪声链在 **`sky.controls` 入口之后挂起**（>40 s，CPU 100%，尚未进入 coadd），说明 full 噪声路径的阻塞点比 coadd 的 `to_csr` 更早：`build_sky_controls` 处理带协方差/大 action 的帧时进入了不受控的展开。为避免把 hang 留在分支上，本轮撤回 `coadd.cpp`、`normalize_runtime.hpp`、`mosaic.cpp` 的实验改动；分支退回到 `61531d170` 的可运行状态（X01 signal_only 通过）。

下一步必须先定位 `build_sky_controls` 里对 `FrameDescription.noise` 的使用：如果它只需要 signal/DQ/index，应显式拒绝/跳过协方差，而不是展开 action；或者把 full 协方差的产品保留到 coadd 阶段，不在 sky 控制里处理。该点未解决前，full profile 不应重新启用。

## 11. 第七轮修复（2026-11-11，`2840f1ac7`）

- 定位 full 噪声链挂起点：`sky.controls` 的 `dense_action` 与 coadd 的 `to_csr` 都按 latent 列逐列 apply（X01 每帧 16384 列 × 多 root），没有上界。
- 两处均加入能力上界：latent 或行数 > 4096 时返回 `unavailable` 并具名原因，调用方保留协方差 unknown，不阻塞工作流。2 帧全噪声链从 >40 s 挂起变为秒级完成，发布 `signal_only` + `omitted:no_noise`，export 继续发布但 noise unknown。
- 这是**有界失败**而不是科学修复：full 协方差仍需 sparse 组合（不做逐列物化）；但它消除了一个会让三命令永久挂起的工程缺陷。
- X01 在 `2840f1ac7` 再次全链通过（signal_only）：normalize 8/8、mosaic 1、export 1，blind RMS 0.2109，48 残差 max|z|1.9007，旧 5% 失败保留。

## 12. 第八轮补充（2026-11-11，`c858bb130`）

- **B5 常场解析 Oracle（新证据）**：`validation/projection/export_constant_probe.cpp` 发布单叶常面亮度产品，1×1 TAN 像元 `supported_flux = b0·|det CD|`（相对误差 1.76e-11）、`supported_mean = b0`；远离叶的像元返回 **NaN**（非 0）。这验证 Q 定义、pixfrac=1、mean/flux 区分与无支持语义。
- **D0 四语义映射**：`docs/audit/d0-error-semantics.md` 把确定数值包围/随机统计/物理系统偏差/未知映射到 `Interval`、`CovarianceAction`、`ErrorRecord`、`UnknownTerm/noise=null`，并列出数据残差进半径、unknown 填零、逐叶对角化等反例；D0 仍因 full 协方差端到端未闭合而不能 PASS。
- **C0 22 Process 审计**：已启动独立审查（报告 `docs/audit/c0-process-audit.md` 由 subagent 产出），本轮结束时尚未返回；下一轮收集其 P0/P1 并按序修复。

## 13. 第九轮：C0 P0 修复（2026-11-11，`3fa331a90`）

C0 审计报告 `docs/audit/c0-process-audit.md` 返回 2 个 P0、若干 P1；本轮关闭：

- **P0-1 bind 身份**：`revise` 纳入父 content_hash/revision 并更新 object_id；`bind_wcs`/`bind_observation_model` payload 纳入 integrated_values/dq 数组哈希。探针 `validation/observation/bind_identity_probe.cpp`：同 frame/index/parent、不同像素值 → 不同 hash（修复前相同）。
- **P0-2 全量构建**：`MetaAction` 补 `dependency_arrays/children`；schema 生成器解析默认初值与 nested enum 默认。`python3 tools/generate_contract_schema.py` 输出 302 defs/79 variants；`cmake --build build/science -j8` 全量通过；typed_smoke 1255、host_lifecycle 66。
- **P1**：apply_scale 记录 frame-median gauge 与绝对零点未测；astrometry 检查 `query_quads` 后再解引用；information 把非 known 协方差视为 unknown。

X01 在 `3fa331a90` 再次全链通过（signal_only）：normalize 8/8、mosaic 1、export 1，blind RMS 0.2109，48 残差 max|z|1.9007，旧 5% 失败保留。C0 其余 P1（bind 域检查、regroup 维度/区间、export 数值界、information 单位、11 个无 driver process）仍开。
