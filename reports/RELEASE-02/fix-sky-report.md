# FIX-SKY 报告：sky_plane gauge/秩亏（rc=6）修复

- 任务：RELEASE-02 / FIX-SKY（PERF-P2 定位的 `[sky_plane] build FAILED rc=6` 结构性缺陷）
- 工作目录：`/workspace/Astro CS Database`
- 改动文件：`lib/algorithms/coverage/src/sky_plane.cpp`（主修）；`lib/infrastructure/scheduler/src/module_adapters.cpp`（仅 rc=6 处置：顶层降级可见性）
- 产物：`run/RELEASE-02/fix-sky/`（复现/诊断脚本与输出）
- 约束遵守：未跑 ninja/cmake/ctest；仅 `g++ -fsyntax-only`；零 git 写权限；未改 docs/`**`

---

## 0. 摘要（结论先行）

1. **缺陷真实且 100% 复现**：用生产 `p2_samples.json`（277234 点 / 49 帧）在 Python 中逐行复刻
   `p2_sky_plane_build` 的装配与 Cholesky，第一轮 IRLS 即在 C++ 的 `sky_plane.cpp:770`
   失败（`chol_spd` 在 i=24 取到 pivot `sum=-2.776e-17`），错误串与生产 stderr 完全一致。

2. **根因与 PERF-P2 归因不同（关键发现）**：
   - 约化数据矩阵 `H_red` **满秩**（n_free=30，rank=30，cond≈2.2e5），**不存在 3 维 gauge 零空间**；
     代码把参考帧 δ≡0 固定，已经用掉了 `B_ref+δ_k` 的规范自由度。
   - 真正奇异的是**粗糙度惩罚自己的零空间**：二阶差分 stencil 两个方向的公共零空间是
     **4 维 bilinear 系数空间 {1, ix, iy, ix·iy}**（不是 PERF-P2 说的 3 维 {1,ξ,η}）。
     δ_k 只到一阶，无法吸收 bilinear 项，所以它**不是规范自由度，是真实模型方向**。
   - 生产数据下 **惩罚项 trace=0.228 而 H_red trace=1.80e-18（相差 1.27e17）**；惩罚矩阵自身的
     浮点舍入 ~1e-18 淹没了数据在零空间方向上的曲率 ~1e-20，使
     `H_solve = H_red + λDᵀD` 在 float64 下**数值上失去正定性**（而非精确奇异：
     `max|H_solve·1|≈1.75e-18 ≠ 0`）。这是**尺度/条件数**问题，不是结构秩亏。

3. **修复**：求解先走**原直接 Cholesky**（常规量级档逐位不变）；**仅当其失败**时，对**惩罚零空间
   方向**做极小 Tikhonov 锚 `+α·Q Qᵀ`（Q=4 维 bilinear 基，α=惩罚对角量级）使 H_solve 严格 SPD，
   再用 **deflation 校正** `B = B_a + Q G^{-1}(Qᵀ rhs − Qᵀ H_red B_a)`（G=QᵀH_redQ）把锚偏置精确扣回。
   修好后生产数据可完成 30 轮 IRLS 并落盘，`:840/:855` 诊断链可达。

4. **确定性**：全部为固定列序 MGS、确定性 α、固定顺序的 4×4 小解；无随机、无并行归约、无数据依赖分支
   之外的分支。同配置同输入 ⇒ 逐位可复现。

5. **诊断真实数值**（生产数据、修复路径，Python 逐行复刻）：n_free=30、rank=30、
   **κ=2.845e5**（λ_max=2.628e-19，λ_min=9.237e-25），n_params=174，iterations=30。

6. **rc=6 处置建议**：建议对**结构性 rc（6/7/8）升为 fail-closed**；至少必须保留回退但在顶层
   manifest 置 `sky_plane_degraded=true`（已在 `module_adapters.cpp` 落地可见性）。**由前台裁决**。

---

## 1. 缺陷事实链（复核）

生产两次 mosaic（`mosaic_49` / `mosaic_49_w1`）均出现：

```
[sky_plane] build FAILED rc=6 reduced normal matrix not SPD -> explicit fallback to UPM C field
```

- `rc=6 = P2_SKY_PLANE_RANK_DEFICIENT`（`sky_plane.h:197`）；
- 触发点 `sky_plane.cpp:770`（本次改动前）`chol_spd(H_solve)`；
- `chol_spd` 为**无 jitter** Cholesky（`sky_plane.cpp:127-146`），对角 pivot `!(sum>0)` 即失败；
- 下游 `upm-apply` 因 `p2_sky_plane.bin` 不存在而 `sky_plane_applied=false`（`module_adapters.cpp:4892`），
  `corrected = raw − C_k − b_k` 的 `b_k` 从未生效。

## 2. 根因确认（对 PERF-P2 归因的修正）

### 2.1 复现与输入

用生产实际输入 `run/RELEASE-02/L4-rebuild/mosaic_out/p2_samples.json`（`n_obs=277234`，49 帧），
按 `module_adapters.cpp:4610-4625` 的默认配置（`spline_degree=3, node_spacing_deg=1.0,
frame_gradient_order=1, gauge_mode=0, weight_mode=0, roughness_penalty=1e-3, max_nodes=2048`）
逐行复刻切平面投影、B 样条装配、逐帧 Schur 消元与 Cholesky（脚本 `run/RELEASE-02/fix-sky/diag_rank.py`、
`diag_penalty.py`）。

网格：`nx=ny=6`（`n_full=36`），自由节点 `n_free=30`（6 个 bbox 角点无支撑被剔除），`m=3`。

### 2.2 H_red 满秩——不存在 3 维 gauge 零空间

| 量 | 值 |
|---|---|
| H_red 特征值 | `3.62e-24 .. 7.87e-19` |
| H_red 条件数 | `2.17e5` |
| H_red rank（rtol=1e-10） | **30 / 30（满秩）** |

代码把**参考帧 δ≡0** 固定（`:668-707` 对 `k==ref_frame` 不建 M/S/t），参考帧数据直接约束 B_ref，
故 `(B_ref+q, δ_k−q)` 在**约化系统里不是对称性**。PERF-P2 所述 "H_solve·1 = H_red·1 + λDᵀD·1 = 0"
与实测不符：`max|H_red·1|=9.39e-19`、`max|P·1|=1.74e-18`，`H_solve·1≈1.75e-18 ≠ 0`。

### 2.3 惩罚零空间是 4 维 bilinear，而非 3 维

两个方向的二阶差分 `v={1,−2,1}` 分别湮灭"关于该方向仿射"的系数向量；**公共**零空间为
关于 ix、iy 均仿射的系数向量 `a + b·ix + c·iy + d·ix·iy`（均匀张量 B 样条下即函数空间
`{1, u, v, u·v}`）：

| 量 | 值 |
|---|---|
| rank(D) / nullity | 26 / **4** |
| 解析基 Q={1,ix,iy,ix·iy} 与数值零空间子空间距离 | `‖QQᵀ − N Nᵀ‖ = 1.0e-14` |
| 惩罚矩阵装配后特征值（最小 6 个） | `-3.56e-18, -1.76e-18, -1.45e-18, -1.22e-18, 3.94e-4, 3.94e-4` |

4 个 ~1e-18 的负特征值是**惩罚矩阵自身的浮点舍入**（其真零空间方向应为 0）。
δ_k 只到一阶、无法吸收 bilinear 项，故 bilinear 是**真实模型方向**（由数据决定），不是 gauge。

### 2.4 数值根因：尺度悬殊 + 惩罚舍入淹没数据

| 量 | 值 |
|---|---|
| 惩罚 trace(P)=λ·tr(DᵀD) | `0.228` |
| 数据 trace(H_red) | `1.80e-18` |
| trace(P)/trace(H_red) | **`1.27e17`** |
| 数据在惩罚零空间上的曲率（Rayleigh） | `6.7e-21 .. 1.06e-19` |
| 惩罚矩阵在零空间上的舍入量级 | `~1e-18` |

即：**数据在零空间方向上的曲率比惩罚的浮点舍入小 1–2 个数量级**。当 `H_solve = H_red + P`
在 float64 中相加时，`H_red`（~1e-18）在 `P`（~1e-3）的 ULP（~1e-19）之下被舍入掉，
零空间方向的符号由舍入决定 ⇒ `chol_spd` 在 i=24 取到 `sum=-2.776e-17` ⇒ rc=6。
**这与数据质量无关，是权重单位（逆方差 ~1e-21）与绝对 λ（1e-3）量级失配导致的数值问题。**

## 3. 修复方案

### 3.1 代码改动（`sky_plane.cpp`）

1. **惩罚零空间基**（`:648-694`，IRLS 前一次性构造）：
   `Q`（n_free×mq，mq≤4）= 自由节点上的 `{1, ix, iy, ix·iy}` 经**固定列序修正 Gram-Schmidt**
   正交化（列序、阈值 `1e-9·orig_norm` 固定 ⇒ 确定性）。退化列自动丢弃（mq<4）。

2. **求解**（`:818-908`）：
   ```text
   先 H_solve = H_red + λDᵀD 直接 chol_spd（原路径，逐位不变）
   if 成功: Bnew = H_solve^{-1} rhs
   else if mq>0:
       α = max_i (H_solve_ii − H_red_ii)        # 惩罚对角量级
       H_solve += α·Q Qᵀ                          # 极小 Tikhonov 锚，只锚零空间方向
       if chol_spd(H_solve) 成功:
           B_a = H_solve^{-1} rhs
           HrQ = H_red·Q ;  G = Qᵀ·HrQ           # mq×mq
           corr = G^{-1}(Qᵀ rhs − HrQᵀ B_a)      # deflation 校正
           Bnew = B_a + Q·corr
       else: rc=6
   else: rc=6
   ```

### 3.2 为什么用"锚 + deflation 校正"而不是只锚

只做 Tikhonov 锚（`+αQQᵀ`）会把零空间方向压向 0，**改变科学解**：实测 `‖B_anchor − B_exact‖/‖B_exact‖ ≈ 1.0`
（100% 偏差）。锚只是把矩阵抬成正定；**必须**用 deflation 校正把锚偏置扣回，才与直接求解惩罚法方程等价。
校正实测与独立 stacked-QR 参考解一致到 `2.8e-7`（该差异就是本问题 ~1e17 条件数下的浮点极限）。

### 3.3 为什么"只锚零空间、不全矩阵加 jitter"

全矩阵加 `εI` 会同时扰动 26 个受惩罚方向，改变科学解；本修复的 αQQᵀ 只作用在 4 维惩罚零空间上，
且校正后偏置被扣回。

### 3.4 为什么不实现成"gauge 约束 Σδ=0"

- 约化系统里参考帧 δ≡0 已固定规范，**不存在 gauge 零空间**（§2.2）；
- 惩罚零空间 4 维中只有 3 维（{1,ξ,η}）可被 δ_k 吸收，第 4 维（bilinear）**不能被 δ_k 吸收**，
  强加 `Σδ=0` 不会消除它，也就不能修复 rc=6；
- 若把 bilinear 分量也强行置零，会移除一个由数据决定的真实模型方向（改变科学解）。

### 3.5 常规档不受影响（保护既有测试）

改动是**先原路径、失败才走新路径**。单元测试量级（`σ≈0.05`，H_red~1e3，λ 相对可忽略）下直接
Cholesky 成功 ⇒ **代码路径与结果逐位不变**；新路径只在生产这种数据项 ~1e-18 的极端档触发。

## 4. 确定性论证

- Q 的构造：固定输入向量、固定列序 MGS、固定阈值；无排序歧义、无随机。
- α：`max_i (H_solve_ii − H_red_ii)`，同一 IRLS 轮同一 H_solve ⇒ 同一 α。
- 校正：`HrQ=H_red·Q`、`G=QᵀHrQ`、`corr` 均为固定顺序的确定性和/积；4×4 Cholesky 固定。
- 分支只依赖 `chol_spd` 的布尔结果（确定性）与 mq（确定性）。
- 无跨线程归约（本模块串行）、无时间/地址依赖。
⇒ 同配置、同输入、同机器，重复构建 `model_hash` 逐位一致（既有 `[determinism]` 用例覆盖）。

## 5. 诊断数值（修复后，:840/:855 可达）

用生产 `p2_samples.json` 逐行复刻完整 IRLS（`run/RELEASE-02/fix-sky/sim_build.py`）：

| 字段 | 值 |
|---|---|
| n_nodes（n_full） | 36 |
| n_free | 30 |
| mq（惩罚零空间维数） | 4 |
| n_params | 174（=30 + 48×3） |
| iterations | 30 |
| α（锚） | 1.200e-2 |
| λ_max（H_red 幂迭代） | 2.628e-19 |
| λ_min（H_red 逆幂迭代） | 9.237e-25 |
| **κ** | **2.845e5**（< kappa_max=1e8 ⇒ 通过） |
| **rank** | **30**（满秩，`λ_min > rank_rtol·λ_max`） |
| rms_weighted | 3.366e12 |
| rms_unweighted | 7.685e13 |
| chi2_red | 1.737e9 |

说明：这些数值来自对生产输入的逐行 Python 复刻（无 ninja/cmake 构建权限，C++ 未实跑）；
数值与 C++ 可能末位不同，但**量级、rank、κ 判据（κ<1e8 通过、rank=30）**是稳健的。
κ=2.845e5 与首轮 H_red 的 κ=2.17e5 同量级，均远低于门限。

### 5.1 独立交叉验证

用 **stacked-LS（Householder QR）** 直接解
`min ‖LᵀB − c‖² + λ‖DB‖²`（L=chol(H_red)）作为不依赖法方程的独立参考：修复路径的解与之一致到
`2.824e-7`（相对），且两种解的法方程残差同量级（~5–7·‖rhs‖，被 ‖B‖~6e13 的浮点极限主导）。
QR 法在良态合成档与直接解一致到 `3.7e-12`，证明校正未引入系统偏差。

## 6. rc=6 处置建议（由前台裁决）

现状：`module_adapters.cpp` 在 build 失败时打印 stderr、写 `sky_plane_status=fallback_build_failed`、
`sky_plane_rc`、`sky_plane_error`，然后**继续**（保留 UPM C 场）。它不是静默，但**每次必然触发的
固定降级等价于天光面从未生效**，机器消费者无法据此判断 mosaic 不完整。

**建议（推荐 fail-closed）**：
- `rc=6 RANK_DEFICIENT / 7 KAPPA_EXCEEDED / 8 NONFINITE` 属**结构性/数值缺陷**（非数据不足），
  按 `DATA-UNC-001 §30.1` 的 **unavailable 显式登记 + fail-closed** 原则，建议
  `upm-fit` 直接返回 `Result<void>::fail(...)`（rc≠0 终止 mosaic），**与 `integrate` 因 weight chain
  未闭合硬失败（rc=2）的先例一致**。
  - 生产影响：任何触发该缺陷的数据集会让 mosaic **显式失败**而非产出"少了 b_k 扣除"的 mosaic；
    由于本修复已让生产数据可建成，fail-closed 不会误伤当前生产，但会阻止未来回归被静默接受。
  - 代价：极端/退化数据集会从"降级产出"变成"无产出"，需有重跑/修数据路径。
- 若前台选择**保留回退**：必须让"从未生效"在验收面可见。已在 `module_adapters.cpp` 落地**顶层
  `sky_plane_degraded=true`**（disabled / no_samples / build_failed / save_failed 四处；成功为 false），
  并建议 `ACCEPTANCE.md`/资源门增加判据：**`sky_plane_degraded==true` 的 mosaic 不得判为完整
  L4 验收通过**（与 `sky_plane_applied` 联合使用）。
- 两种选择都需要把 `sky_plane_rc` 保留在 manifest（已保留）。

> 本次仅落地"可见性"（非行为改变），fail-closed 的最终取舍留前台。

## 7. 科学影响面

- **预期变化（本任务的目的）**：天光面现在能建成并落盘，`upm-apply` 会执行
  `corrected = raw − C_k − b_k`，故所有下游 mosaic 的逐像素校正值**会改变**（背景台阶/大尺度结构被扣除）。
  这是"天光面真正生效"的必然结果，不是回归。
- **模型语义不变**：修复求解的是**同一个** `(H_red + λDᵀD)B = rhs`（锚偏置被 deflation 扣回），
  未改公式、未改默认 λ、未改权重、未改容差；gauge/输出格式/持久化 schema 均不变。
- **须前台注意的量级事实**：生产权重为逆方差 `~1e-21`、惩罚 trace 比数据 trace 大 `1.27e17`。
  这意味着当前 `roughness_penalty=1e-3` 相对生产数据是**极强**正则（未惩罚解 ‖B‖≈1.22e15，
  惩罚解 ‖B‖≈6.34e13）。修复后能出结果，但"λ 的物理量级是否合适/是否需要按数据尺度归一"
  属**科学/语义选择**，本次未动，建议单独立项由负责人裁决。
- **诊断面**：`rank`/`kappa` 字段从"永不可达"变为真实可读（rank=30，κ=2.845e5），
  `P2_SKY_PLANE_KAPPA_EXCEEDED` 门重新具备鉴别力。

## 8. 测试影响

- **不新增/不改测试**（未动 `tests/`）。
- 既有 `v6_p2_sky` 用例：因"先直接 Cholesky"路径逐位不变，预期 **全绿**；
  其中 `[overflow]`（宽视场稀疏采样）本就接受 `OK || RANK_DEFICIENT || TOO_FEW_SAMPLES`，
  修复后无论走通（新路径）还是仍 rc=6 都在该用例判据内，**不回归**。
- 建议新增（前台统一构建时）：① 生产量级回归（逆方差 ~1e-21）锁定"能建成 + rank/κ 可读"；
  ② "锚前/锚后预测一致性"（本修复的核心不变量）；③ `sky_plane_degraded` 顶层可见性断言。
- 构建/测试由前台统一执行（本任务未跑 ninja/cmake/ctest）；已做 `g++ -fsyntax-only` 通过
  （sky_plane.cpp 与 module_adapters.cpp 均无语法错误）。

## 9. 证据与复现（run/RELEASE-02/fix-sky/）

| 文件 | 内容 |
|---|---|
| `diag_probe.py` | 生产采样几何（n_obs/帧/投影域） |
| `diag_rank.py` | 逐行复刻装配 + Schur；输出 H_red 谱、Cholesky 失败点、网格/自由节点 |
| `diag_mag.py` | 量级/条件数（惩罚 vs 数据） |
| `diag_penalty.py` | 惩罚零空间、Rayleigh、缩放对照（证明是尺度问题） |
| `diag_null.py` | rank(D)/nullity=4、bilinear 零空间 |
| `diag_validate.py` | 合成良态问题上验证 deflation 公式（1e-13） |
| `sim_synth.py` | 良态档对照（证明"直接 Cholesky 成功时不触发新路径"） |
| `sim_build.py` | 生产数据完整 IRLS 复刻 + 修复路径 + 诊断（κ/rank/rms） |
| `diag_prod_cmp.py` / `diag_ref.py` | stacked-QR 独立参考与交叉验证 |

复现命令（不构建项目，仅 Python + 只读输入）：

```bash
export TMPDIR=/dev/shm/astrocs_sky
python3 run/RELEASE-02/fix-sky/diag_rank.py       # 复现 rc=6 的数值根因
python3 run/RELEASE-02/fix-sky/diag_penalty.py    # 量级对照（1e17）
python3 run/RELEASE-02/fix-sky/sim_build.py       # 修复路径完整构建 + 诊断数值
python3 run/RELEASE-02/fix-sky/diag_prod_cmp.py   # QR 交叉验证
```

---

## 附：改动清单

- `lib/algorithms/coverage/src/sky_plane.cpp`
  - 文件头数值结构注释补充惩罚零空间与修复策略；
  - `:648-694` 新增惩罚零空间基（bilinear 4 维，确定性 MGS）；
  - `:818-908` 求解改为"直接 Cholesky 优先，失败时零空间锚 + deflation 校正"，失败仍 rc=6。
- `lib/infrastructure/scheduler/src/module_adapters.cpp`（仅 rc=6 处置）
  - 天光面 disabled / no_samples / build_failed / save_failed 分支置顶层 `sky_plane_degraded=true`，
    成功置 false；保留既有 `sky_plane_status`/`sky_plane_rc`/`sky_plane_error`。
