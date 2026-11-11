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

## 14. 第十轮：C0 其余 P1（2026-11-11，`9ffeb7e37`）

- **export 数值界**：旧实现只把叶值区间通过中点 Q 作用，漏掉 overlap 面积区间。现保留每个 (pixel,leaf) 的系数区间 [cL,cU]，按 `min/max(c_i x_i)` 对 c、x 两个区间求端点点积；`supported_mean` 用分母区间。常场探针验证 `lower ≤ flux ≤ upper`，且 1×1 解析通量仍为 1.76e-11 相对误差。
- **regroup 顺序**：`input_is_mean` 的恢复因子 S 作用在原生样本上，正确算子是 `G∘S`；旧 `compose(S,G)` 对非方 G 是形状错误、对方 G 会算错。已改为 `compose(G,S)`，regroup driver 通过。
- X01 在 `9ffeb7e37` 通过（signal_only；normalize 8/8、mosaic 1、export 1，blind RMS 0.2109，48 残差 max|z|1.9007）。本轮 e2e 从 `ba94d9265` 启动、结束时 `regroup` 提交已存在；regroup 不在 X01 链上，行为等价。

## 15. 第十一轮：C0 身份/单位/域（2026-11-11，`7b470005`）

- `catalogue.lookup`：缺标识符或无界 topocentric 修正时 `complete=false`，后者同时 `noise=unknown/science=conditional`。
- `bind_wcs`：sample 包围盒经 `detector_from_sample`/offset 映射到 detector 后再检查 `wcs.domain`，不再假设 sample 网格等于 detector 网格；bind 身份探针仍给不同像素值不同 hash。
- `information`：`V_F` 单位按 `h` 的 linear/sr/pixel/nm power 与 multiplier 传播；`h` 无量纲时保持协方差单位。参数数组 px/rad 混存仍未修。
- X01 在 `7b470005` 通过（signal_only）：normalize 8/8、mosaic 1、export 1，blind RMS 0.2109，48 残差 max|z|1.9007，旧 5% 失败保留。

## 16. 第十二轮：regroup 界与 B5 旋转/非对称 oracle（2026-11-11，`5e4e1f9de`）

- `regroup` 在父对象没有数值包围时，旧代码把名义值同时写为 lower/upper，制造“精确界”。现在 bounds 保持 null、`complete=false`，只记录 `the_parent_carries_no_numerical_bounds`；regroup driver 通过。
- B5 新增 `validation/projection/export_grid_probe.cpp`：常场单叶、可配置 width/height/CRPIX/CD 旋转。与 Astropy TAN 外角球面多边形面积（Van Oosterom–Strackee）对比：
  - 2×2、CRPIX=(0.5,0.5)、CD 旋转 30°：rel 2.56e-7；
  - 2×2、CRPIX=(0.3,0.7)、CD 旋转 30°：rel 2.45e-7。
- 说明：X01 最新全链证据仍在 `7b470005`（本轮改动不在 X01 链上）；B5 仍缺部分支持、镜像 CD、跨 face、极大像元与重复 pixfrac。

## 17. 第十三轮：information 解析与镜像 CD（2026-11-11，`65a9acb21`）

- `validation/process/information.reference/information_reference.cpp`：\(C=\mathrm{diag}(4,9)\)、\(h=[1,0]\)，`estimate_flux_information` 给 \(V_F=4\) 精确；把 `NoiseModel.state` 改为 unknown 后 `V_F=nullopt/unknown`。补上 information.reference 的独立数值证据（仍非完整 process driver）。
- B5 镜像 CD：`export_grid_probe` 增加 det>0 模式；2×2、CRPIX=(0.3,0.7)、CD 旋转 30° 镜像，export sum 与 Astropy 球面面积相对差 3.57e-8。
- X01 最新全链通过仍在 `7b470005`；本轮改动不在 X01 链上。

## 18. 第十四轮：full profile 协方差路径诊断（2026-11-11）

在 `sky::to_csr` 增加 compose-diagonal/通用 sparse 组合、并在 `dense_covariance_factor` 增加 diagonal factor 快路径后，2 帧全噪声链仍只在数秒内完成但**没有协方差对象**。逐层打印显示帧影响是：

```
factor_kind=diagonal (16384), influence_kind=compose
  child0=csr, child1=compose
```

即影响不是简单的 `CSR ∘ diagonal`，而是 `CSR ∘ (…∘…)`；快路径未命中，4096 列物化上界仍在 `to_csr` 处返回 unavailable，joint covariance 继续降级。尝试的通用 sparse 组合未经充分反例验证，本轮未保留，`sky.cpp` 已退回 HEAD。

结论：full profile 的下一最小步骤是**扁平化 selection/compose 链并直接得到每一列的稀疏模式**（或在 build_joint_noise 里用 action 组合 + 对控制向量做二次型，不做列物化）；在此之前 full 仍应保持不可用，不能靠提高 4096 上界来“通过”。

## 19. 第十五轮：full profile 打通（2026-11-11，`2a85553dc`）

两项实现消除了列物化：

- `sky.controls`：控制协方差
  \[
  C_c=\sum_k (L_kF_k)(L_kF_k)^\top
  \]
  由 diagonal root factor 与 influence 的稀疏列直接累加，再分解为 \(U\sqrt\Lambda\)；原逐 latent 列的 factor 仅作 fallback。
- `coadd`：joint model 用 embedding \(E_f\) 与 influence 组合，不再 `to_csr` 逐列；shared root 用 sum action 合并。`runtime::ingest_action` 把 factor/influence 的 payload arrays/children 搬过 Host 边界。

结果：X01 以 `product_profile=full` 全链通过（normalize 8/8、mosaic 1、export 1），mosaic/export 均发布 `profile=full` 且带 `CovarianceAction`；`covariance_apply(C,1)` 在 mosaic（domain 1）与 export（domain 16384）上均有限，mosaic max|C·1|=2.62e22、export 236906。状态仍是 `noise=unknown`，因为帧噪声本身 conditional（P1-5 等），不是协方差对象缺失。

数值证据不变：blind RMS 0.2109，48 残差 max|z|1.9007，旧 5% 失败保留。

## 20. 第十六轮：full 协方差的单位与 PSD（2026-11-11，`6c3f8d7af`）

- **单位矛盾修复**：`C=Var(y)`，故 `unit(C)=unit(y)^2`。X01 的 target unit 显式写成 `"1"`，旧 coadd 直接把它当输出单位，而协方差仍是 native ADU²。现在未指定单位（`"1"` 且各 power 为 0）继承帧的公共 variance unit（signal 取平方根）；显式给出不同单位则 `invalid_request` 拒绝。
- **full 证据**：X01 全链 `product_profile=full` 通过（normalize 8/8、mosaic 1、export 1），blind RMS 0.2109、48 残差 max|z|1.9007 不变。`validation/product/covariance_semantics_probe.cpp` 对 mosaic/export 的 C 检查：
  - `signal_unit == var_unit == cov_unit`（`acsd.native:x01-03`）；
  - `x·Cy = y·Cx`（相对差 2.2e-16 / 1.9e-14）；
  - `x·Cx > 0`（PSD）；
  - `C·1` 有限（mosaic 2.62e22、export 2.37e5）。
- 状态仍 `noise=unknown`（帧噪声 conditional），但协方差对象、单位、对称性与 PSD 现在一致。

## 21. 第十七轮：C_out = Q C_leaf Qᵀ 的端到端验证（2026-11-11）

- export 产品现在把投影算子 `Q` 放进 `ProductGraph.closure`；产品 reader 重新打开 closure 中的 action，而不是静默丢弃。这样 `C_out` 在重开后仍可审计。
- `validation/product/projection_covariance_probe.cpp` 在 2 帧链上打开 mosaic/export，取 `Q`、`C_leaf`、`C_out`，对随机像素向量 x 比较
  \[
  C_{\rm out}x \quad\text{vs}\quad Q\,C_{\rm leaf}(Q^\top x):
  \]
  `max_abs_diff=2.84e-14`，`max_abs=91.13`，相对差 **3.12e-16**。
- 这说明 export 的协方差语义确实是 `Q C Qᵀ`（含 shared/cross 项），不是逐叶对角化或丢弃投影相关。

### X01 全链上的同一验证（`177fde19c`）

X01 `product_profile=full` 通过（normalize 8/8、mosaic 1、export 1），blind RMS 0.2109、48 残差 max|z|1.9007 不变。对 X01 mosaic/export 产品运行同一探针：

```
npix=16384 nleaf=1
max_abs_diff=2.84217e-14  max_abs=59.2686  rel=4.8e-16
```

即 X01 的导出协方差确实等于 `Q C_leaf Qᵀ`，而不是逐叶独立或漏掉 cross 项。

## 22. 第十八轮：X01 dark 夹具（2026-11-11，`f3b72c3ea`）

`calibration.json` 的 dark 从 4.8 ADU 改为 truth 对应的 480 ADU（`12 e⁻/s × 100 s / 2.5`），曝光时间同为 100 s。此前 475.2 ADU 被 β 吸收，N0 的 dark 路径未被真实覆盖。X01 full 全链在 `f3b72c3ea` 仍通过，blind RMS/48 残差不变；dark 路径现在有数据覆盖。

## 23. 第十九轮：support states 重开 P0（2026-11-11，`6889aa307`）

- `decode_support_map` 漏解码 support 数组引用，重开产品时 `states=null`，export 把所有几何重叠叶当作 supported；`states=0` 的叶仍导出非零。
- `read_states` 读失败还静默返回空 vector，同样等于“全支持”。
- 修复后：`states present=1`、`states=0` 叶导出 NaN；读失败具名拒绝。X01 full 在 `6889aa307` 通过，blind RMS 0.2109、48 残差 max|z|1.9007 不变。

## 24. 第二十轮：coadd/sky 的 support 缺失不再默认全支持（2026-11-11，`7775f6a14`）

export 的 P0（`6889aa307`）修复后，继续检查同一类“重开即放宽”：

- `coadd::read_frame` 与 `sky::read_frame` 在 `query_support` 失败、support 为空或 `states` 为空时都 `assign(count,1)`，即把所有叶当 supported。权重和控制统计会静默使用生产者排除的行。
- 两处现在都返回具名失败（`the frame publishes no support states`），不再默认 1。2 帧 full 链仍发布；X01 full 在 `7775f6a14` 通过，blind RMS 0.2109、48 残差 max|z|1.9007 不变。
- 仍未处理的同类点：DQ 数组缺失时 coadd 的 `view.dq` 为空，选择是否把它当“全部质量良好”需单独审查（X01 有 DQ，不在本轮覆盖）。

## 25. 第二十一轮：DQ 缺失与条件传播（2026-11-11，`aa1c7931e`）

- **DQ 缺失语义**：`coadd` 旧逻辑对 `view.dq.empty()` 直接放行，等于“全部质量良好”。现在缺 DQ 的行仍可选（没有 bit 可拒），但 selection 记录 `the_quality_mask_is_absent_for_at_least_one_frame`、`science=conditional`、`complete=false`；DQ 数组长度与 index 不一致则具名拒绝（避免越界）。
- **条件传播**：coadd estimate 曾用全新 `complete=true/bounded` assessment，selection 的 conditional/omitted 丢失；现在合并 selection 的 included/omitted/issues，并继承 conditional/incomplete。
- **export 传播**：export estimate 同样曾重置为 bounded/complete；现在继承源 estimate 的 omitted/issues/conditional/incomplete，线性映射不能把条件结果升级为 bounded。
- X01 full 在 `aa1c7931e` 通过；mosaic 与 export 均为 `full`、`science=conditional`、携带 `the_quality_mask_is_absent_for_at_least_one_frame`；blind RMS 0.2109、48 残差 max|z|1.9007 不变。

## 26. 第二十二轮：sky/covariance 条件进入发布对象（2026-11-11，`7223b34ad`）

- `SkyFit` 之前只把控制协方差未知/不可辨识条件放在返回的 `Result.assessment` 上，发布对象自身仍是 `complete=true, noise=known`；现在 `fit->assessment` 同步 unknown/conditional/incomplete 并记录 `known_control_covariance`/`not_estimable_sky_directions`。
- `make_covariance` 现在在 `model->state != known` 或存在 unknown term 时把协方差 assessment 置 `conditional`、`complete=false`、`omitted=known_complete_variance`，不再发布“complete covariance”。
- coadd estimate 同时合并 `SkyFit` 的条件/omitted/issues。
- X01 full 在 `7223b34ad` 通过；mosaic/export 均 `full, science=conditional, noise=unknown`，omitted 含 `the_quality_mask_is_absent...` 与 `known_control_covariance`；blind RMS/48 残差不变。

## 27. 第二十三轮：assessment 与 state 的一致性（2026-11-11，`f6e49552d`）

用产品 JSON 扫描 `SignalEstimate/CovarianceAction/NoiseModel/SkyFit/Selection/WeightSolution/ProductGraph` 的不变量：

- `noise=unknown` 不得 `science=bounded`；
- `complete=true` 不得同时含非 ok issue；
- `science=conditional` 不得 `complete=true`。

初始 mosaic/export 各有 1 个违规 `NoiseModel`（id `06a825…` = `hash_index(latent,1)`）。逐个根因修复：

1. calibration/regroup/coadd-fallback/sky-parameter/apply_sky/export 的 NoiseModel 在 state=unknown 时仍 `complete=true`；
2. 它们的 identity 未覆盖 state，导致旧 bounded 对象被复用（同一 id 内容不变）；
3. 逐一改为 `complete = state==known && unknown_terms.empty()`，unknown 时 `science=conditional`、`omitted=known_complete_variance`，identity 纳入 state。

最终扫描：mosaic/export 各 3 个模型对象，**0 violations**。X01 full 在 `f6e49552d` 通过，blind RMS 0.2109、48 残差 max|z|1.9007 不变；mosaic/export 仍 `full, science=conditional, noise=unknown`。

### 证据工具：assessment 不变量扫描

`validation/product/assessment_invariants_probe.py` 对产品 `objects/*.json` 中带 noise 语义的模型检查三条不变量（unknown→conditional、complete→无 issue、conditional→非 complete），只作人工证据、不设 CI 门禁。X01 mosaic/export 当前输出：

```
mosaic: 3 model assessments checked
export: 3 model assessments checked
assessment invariants: 0 violation(s)
```

## 28. 第二十四轮：astrometry 参数块单位（2026-11-11）

- `AstrometricSolution.parameter_units` 按块声明 crpix(px)、ra/dec(rad)、cd(rad/px)、SIP(px^-n)，solve_astrometry 发布时填充。
- astrometry driver 新增 `parameter_units_cover_the_vector`（8 块）与 `parameter_units_are_block_specific`（px,px,rad,rad,rad/px×4）检查，均通过。
- 这是声明层修复：`parameters` 仍是单一 Unit，`covariance_apply` 尚未按块解释 parameter_noise；跨参数 functional 的单位推导仍可能错。
- 本改动不改变数值路径；最新 X01 全链证据仍在 `c351c2fd1`（本轮未重跑 X01）。

## 29. 第二十五轮：X01 证据推进到当前 HEAD（2026-11-11，`66f034456`）

- 在 `66f034456`（含 astrometry 参数块单位声明）重跑 X01 full：normalize 8/8、mosaic 1、export 1，exit 0；blind RMS 0.2109、48 残差 max|z|1.9007 不变。
- 同一产品上运行 `assessment_invariants_probe.py`：mosaic/export 各 3 个模型，`0 violation(s)`。
- 说明：参数块单位是声明层改动，数值路径未变；本次重跑使 evidence SHA 与 HEAD 一致。
