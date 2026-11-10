# B3｜球面几何与 Drizzle 的独立审查（task-4）

> **性质**：只读独立审查，不参与实现、未修改 `src/`。本文只新建本文件。
> **被审对象**：`rebuild/acsd-v2@fa4649d57`（含 `e2df158bb` 的 B1 数值原语整改之后）。
> **范围**：`src/geometry/{kernel,overlap,containment,drizzle,refine,exact_polygon,exact_fraction,profile}.cpp`、
> `src/coordinates/{chart,healpix,tan_sip}.cpp`、`src/workflows/normalize.cpp`（几何/Drizzle 调用段）、
> `docs/engineering/rebuild/processes/geometry.overlap.md`、`geometry.drizzle.md`、
> `docs/science/foundation/SPHERICAL_DRIZZLE.md`、`docs/workplans/.../science/07-SPHERICAL-GEOMETRY.md`。
> **方法**：§1 先从 Ω/A/a/w/F/N/R/B 的**定义**正向推出实现必须满足的不变量（不看现有代码），§2 再逐条
> 用 `file:line` 核对真实 C++ 路径，§3 给可手算/可复算反例，§4 判 block-size，§5 逐 operation 裁决。
> 本文的数值例是**纸面算术 + 一个独立 Python 复算脚本**（未复用生产 helper），只用于"证伪"，不当作验收门禁；
> 依用户 2026-11-10 裁决，未使用 CI、未把脚本输出当证明。
>
> **裁决口径**：`P0` = 会让"每片真叶的 a/w/N/B"或"R 的同一身份/失效"在无声中出错（可在常量场/总面积
> 守恒测试下通过）；`P1` = 违反合同、会在具名条件下给出错误科学量或产物；`P2` = 证书的形式漏洞或
> 已定性为保守/可忽略的偏差，但**必须在文档里降级陈述**。
>
> **依赖他人记录、本人未独立重跑的项**：X01 实际运行时间（lead 实测 `ACSD_DRIZZLE_BLOCK=1`
> 每帧 16384 次 `compute_overlap`、单帧 >20 分钟）与 `block=16/1` 的 `measured_stars` 位相同；
> `validation/apps/evidence/x01/attempts/geometry_performance_baseline.md` 的历史计时（几何 2.06 s/64 行、
> 整帧 75.3 s/16384 行）来自实现者档案。我未重跑这些命令；§4 的结论只依赖这些事实的*方向*，
> 不依赖其绝对数值。

---

## 0. 结论摘要

1. **几何主核（affine-strip cut cell + 精确有理裁剪 + dyadic ledger）的骨架是对的**：§6 的内/外多边形
   构造、`a` 的有向上下界、未决质量不归零、OUTSIDE 证明漏选面积为 0，这些在 `src/geometry/kernel.cpp`
   里确实存在，并且用 `ExactFraction`/`Rational` 做了精确裁剪（`exact_polygon.cpp`）。**这条路线可以保留**。
2. 但主核的**残差证书在极区分支上是错的**（P0-5）：`chart_partials`/`chart_partials_interval` 在
   r<1（北极冠）与 r>3（南冠）分支漏掉了 face 经度常数 `L`，于是**仿射模型与其残差界同错**，
   残差界不再包含真误差。实测比值为 12–31 倍（§3 C1）。凡 |Dec|>41.8° 的源，`a` 可以落到错误的叶。
3. **Drizzle 的 F/N/R/B 没有实现式 (2)**（P0-1/P0-2）：`Ω_j`、`A_j` 被跨源做 hull，
   `N_p=(Σw)·hull(Ω)` 而不是 `Σ_j w_jp Ω_j`；点值取保守区间的中点；CSR/response 存的是 `w` 而不是
   `R=w/N`；噪声是 `w C wᵀ` 而不是 `R C Rᵀ`（差 `N_p²`）。常量场 `x_j=b0Ω_j` 下点值 `B≠b0`
   （实算 1.5625 或 2.2656，真值 1.0，且**随源顺序改变**）。这不影响区间包含性，但影响**发布的数值**。
   量级说明：单帧内 `Ω_j/A_j` 差异小时 hull 的点值偏差是二阶小量，但定义上 `N≠Σw_jΩ_j` 已经错；
   跨帧/跨仪器/不同 pixfrac 的 coadd（正是 `w=a/A` 这一形式的用武之地）是一阶错误。
   与 P0-3 叠加后，未决 sliver 叶的发布点值可以偏离真值数十个百分点。
4. **对象身份/修订失效**（P0-3）：`Overlaps/GeometryConstraints/GeometryRecipe/DrizzleOperator/
   SignalEstimate/SpatialResponse/GeometrySnapshot/GeometryRevision` 的 hash 都是**常量字符串**，
   `Identity::revision` 恒为 1（`src/model/units.cpp:252`）。几何改了，`object_id` 不变，
   `refine_geometry` 的 `Replacement.old_hash==new_hash`，依赖闭包无法失效。
5. **工作流合并丢掉了几何的全部约束状态**（P0-4）：`normalize.cpp:1023-1024` 只合并
   `contributions`，`primitives/constraints/coarse_candidates/recipe/status` 全为空；默认构造的
   `status.termination==0==tolerance_met`、`bound_validity==certified`（`include/acsd/model.hpp:50-52`），
   于是 Drizzle 看到一个"已认证、无未决"的 Overlaps，并且 `build_drizzle` 的 order/pixfrac 身份检查
   因 `recipe==nullptr` 被跳过。未决质量与 `Σa+coarse=A` 一起从产品链上消失。
6. **block>1 今天不可能是等价的**（P0-6）：`block_footprint` 把 source row 设成 block 号
   （`normalize.cpp:136-139`），而 `x` 是**每像素**的 `integrated_values`
   （`observation/calibration_kernel.cpp:480`）；`build_drizzle` 用 `x[source_row]` 取值
   （`drizzle.cpp:194-195`），即用第 0 行前 64 个像素的值去当 16×16 块的通量。亮度会差 ~`block_size²` 倍。
   默认 `block=1` **是当前数据模型下的科学必要**；>20 分钟来自 16384 次独立 invocation 的开销，
   不是几何精度要求（§4 给可证明批量化的条件）。
7. 其余 P1：face 缝/z=±2/3 跨界未拒绝（`kernel.cpp:163-165` 的 `pure_*` 算了不用）、
   `chart_leaf` 把越界 `ix/iy` clamp 到 `N-1`（`chart.cpp:220-228`）导致错叶与 containment 的整格重复计数、
   kernel 与 containment 冲突时"kernel 赢"且冲突质量既不归属也不扣减（`overlap.cpp:531-538`）、
   `Σa+coarse−A=0` 只作为声明从不校验（`overlap.cpp:647-655`）、取消时当前块未处理部分质量丢失
   （`kernel.cpp:573-583`）、`tolerance_met` 与预算 `area_absolute_sr` 无关（`kernel.cpp:672-675`）、
   旋转矩形 footprint 被当外接 bbox（`overlap.cpp:114-129`）。

**最低限度结论**：X01 的几何配置（order 6、|z|<0.16、半径 2.86°、单像素 drop）**恰好绕开**了
P0-5/P1-1 的全部触发条件（无极点、无 face 缝、无跨 z=±2/3）。而且按仓库内冻结的事件流，
记录的 X01 运行在 drizzle 之前就以 `code=failed, "no catalogue star supplied"`
（`validation/apps/evidence/x01/x01_evidence.json` 的 `frame_finished`）终止，**从未执行过
`build_drizzle`**。因此：X01 **不能**作为"球面几何已认证"的证据；P0-1..P0-4 是"一旦走到
drizzle/合并路径就生效"，不是"已在 X01 中观测到错值"。要观测到它们，需要一次至少跑到
`build_drizzle` 的实跑（见 §附录 A 的最短复现）。

---

## 1. 从定义正向推导：实现必须满足什么（不看现有代码）

### 1.1 定义与"必须逐源、不能逐叶 hull"的代数结构

原生像元（或显式 footprint 分量）`P_j`，先在 detector 内收缩 `Q_j(f)`，连续 `W` 映到球面
`D_j=W(Q_j)`，真叶 `H_p` 是 ICRS NESTED 叶——在等面积 face chart 里就是
`[i/N,(i+1)/N]×[k/N,(k+1)/N]`（`N=2^order`），`Ω_H=π/(3N²)`：

```
Ω_j   = Ω(W(P_j))          —— 源 j 的属性，与叶无关
A_j   = Ω(D_j)             —— 源 j 的属性，与叶无关
a_jp  = Ω(D_j ∩ H_p)       —— 唯一依赖 (j,p) 的量
w_jp  = a_jp / A_j
F_p   = Σ_j w_jp x_j
N_p   = Σ_j w_jp Ω_j
R_pj  = w_jp / N_p
B_p   = F_p / N_p = Σ_j w_jp x_j / Σ_j w_jp Ω_j
```

由此**强制**得到（这些就是审查判据，逐条编号 I1…I12）：

- **I1（逐源）**：`A_j`、`Ω_j` 是源的下标对象。任何"把这个叶上所有源的 A 或 Ω 合成一个数"的写法
  都会把 `w_jp=a_jp/A_j` 变成 `a_jp/A_hull`，把 `N_p=Σw_jΩ_j` 变成 `(Σw_j)·Ω_hull`。式 (2) 不允许。
  只有在**所有 `A_j`、`Ω_j` 严格相等**时才碰巧相等；一般情形不成立。
- **I2（同一 A、同一 Ω、同一 R）**：`w` 的 `A`、`N` 的 `Ω` 必须与 `A_j`、`Ω_j` 是同一 primitive
  同一 revision；`R_pj` 必须是 signal、PSF、noise（`R C Rᵀ`）、background basis、Export 的**同一个**
  线性算子（`SPHERICAL_DRIZZLE.md:22, 205-212`；`geometry.drizzle.md:18, 86`）。
- **I3（点值不许用保守区间中点冒名）**：区间 `[w_L,w_U]` 只用于证书；**发布的信号值**必须是对
  `w` 的最佳可用代表（在未决质量内就是未决，不是中点）。文档原话："不能独立归一 R 的中点来伪造精度"
  （`SPHERICAL_DRIZZLE.md:166`）。
- **I4（常量场判别）**：`x_j=b0Ω_j ⇒ B_p=b0`（`SPHERICAL_DRIZZLE.md:24`），要求**点值精确**，
  而不是"区间包含 b0"。区间包含是弱条件：把 `w` 缩成 `[0,1]` 也能包含。
- **I5（面积守恒）**：真叶分割球面且不重不漏 ⇒ `Σ_p a_jp = A_j`（`SPHERICAL_DRIZZLE.md:24`）。
  实现必须输出 `[aL,aU]` 且能**校验** `Σ_p a_p^L ≤ A_j^L ≤ A_j^U ≤ Σ_p a_p^U + coarse_U`；
  这条校验在现有代码里不存在（§2.4）。
- **I6（a 的上界）**：`a_jp ≤ min(A_j, Ω_H)`。`a ≤ Ω_H` 这条约束在 `overlap.cpp:616-655` 的约束集里
  **缺失**（只有 `a≥0`、`a≤A`、`A≤Ω`、`Σa+coarse=A`）。
- **I7（候选完备）**：候选 `ix` 必须覆盖真 `s` 的**包络**：`ix_lo=floor(s_L·N)`、`ix_hi=floor(s_U·N)`，
  且 `[s_L,s_U] ⊇ {s(u): u∈U}`。若包络本身不成立（分支/极点/缝），必须**拒绝并细分**，
  不能把越界 `ix` clamp 到 `[0,N-1]`——clamp 会把面积送进错误的叶，并在整格 fallback 里**重复计数**。
- **I8（未决不归零）**：任何不可判定块保留全质量到 `unresolved`；取消/预算耗尽时**包括当前正在处理的
  块**（不是只有队列里的）。
- **I9（支持≠面积和）**：`S=∪(D_j∩H_p)` 不是 `Σa`，也不是 `max_j a_jp`；SupportMap 的面积必须真正
  是 union 的包含（`geometry.drizzle.md:86`）。用"该叶上单源最大 a"当 upper 会**低估** union。
- **I10（修订失效）**：`W/order/pixfrac/budget/arithmetic` 或任一 `a/A/Ω` primitive 变化 ⇒ 新
  `GeometryRevision`，旧 R 供旧引用、新 R 与 signal/noise/PSF/response/sky/weights 事务替换
  （`geometry.drizzle.md:88`）。身份必须内容相关；`revision` 必须递增。
- **I11（外存 CSR 与固定键）**：`(target_leaf, source_id, primitive_revision)` 为键；`(source,leaf)`
  先并再算；两遍（先 N，再 R）；行 spool 不许全 nnz 常驻；R 的系数必须是**最终 R**。
- **I12（负信号/负支持）**：`x` 可负（天空减除），`w≥0`，所以 `F` 的界要乘 `w` 的全部端点；
  `N_L>0` 才归一；`N` 跨 0 只能 `support_unresolved`（`SPHERICAL_DRIZZLE.md:156`）。

### 1.2 由 I1–I7 推出的"候选 + 包含"算法（应当的形状）

1. **每个源 j 单独**：`[Ω_j]`、`[A_j]` 用同一个 ledger 算（`A_j` 的 g 积分与 `a` 的 g 积分共用同一
   primitive）；`f` 只收缩一次。
2. 源域候选：把 `Q_j`（或 rect）在 detector 上 dyadic 细分；对每块给 `s,t` 的**有效包络**
   `[s_L,s_U]×[t_L,t_U]`（含 face 分支、z=±2/3 分界、apex、face 缝各自的并包），
   候选 `ix∈[floor(s_L N),floor(s_U N)]`，并要求 `0≤ix,iy≤N-1` 否则**拒绝该块**（细分或未决）。
3. 对每个候选在 `(u,v)` 里做**仿射模型 + 均值余项**：
   `s(u)=s(c)+J0·(u−c)+e`，`|e|≤Σ_k mag(∂_k s(U)−J0_k)·h_k`，`h_k≥max(c_k−U_k^L,U_k^U−c_k)`，
   全部**向外**；`∂_k s(U)` 必须是**真实的**偏导（极区分支要含 `−L`），否则内外多边形与残差
   同错，证书失效。用精确有理裁剪得 `P_in⊆D∩H⊆P_out`；`a∈[∫_{P_in}g_L,∫_{P_out}g_U]`。
4. `a` 的 ledger **按 (j,p)** 累加；`Σ_p a_jp=A_j` 是**可校验**的后果（I5），必须真的校验。
5. 跨 face / apex / 缝的块：要么给出**多 face 并包**并按各 face 的 cell 分别记账（同一块在不同
   face 的 cell 集互不重叠），要么未决；**不能** clamp。
6. 支持并集用独立 Boolean OR，不以 `Σa` 或 `max a` 代替（I9）。
7. Drizzle：**逐 (j,p)** 累 `F,N`，`R=w/N` 只生产一次，signal/PSF/noise/response 全用它；
   点值必须是 `w` 的最佳代表（未决就未决）。

对照这份推导，现有实现**在骨架层面是对的**（第 2、3 步的 cut cell 与精确裁剪都存在，
`exact_polygon.cpp:22-47`、`kernel.cpp:592-619`），但第 3 步"真实偏导"、第 5 步"不 clamp"、
第 4 步"校验"、以及 Drizzle 的第 7 步都**没有做到**。下面逐条给证据。

---

## 2. 代码符合性审查（file:line）

### 2.1 主核的仿射 chart 模型与残差证书

`build_block_model`（`src/geometry/kernel.cpp:112-248`）做的是正确的事：取块中心方向
（`forward_point`，:120）、面（`chart_inverse`，:122）、`s0,t0`（:125-126）、块上 `n` 的区间
（`forward_interval_iv`，:132）、`J_W` 区间（:137）、由 `|J_W|·h` 得角半径（:138-144）、
把 φ 限制在该 face 分支（:149-159）、`chart_interval` 给 `s,t` 包络（:169-174）、
`chart_partials` 给点导数（:177-193）、`chart_partials_interval` 给导数区间（:198-209）、
最后按 §6 式 (16) 的**均值形式**给残差
`e_s = mag(∂_u s(U)−J0_u)·h_u + mag(∂_v s(U)−J0_v)·h_v`（:231-232）。

**问题 P0-5：极区分支的偏导漏掉 face 常数 `L`。**
`chart_from_face`（`src/coordinates/chart.cpp:59-78`）在北冠用
`d = r·(4φ/π − L)`（:65），故真实
`∂d/∂z = (4φ/π − L)·dr/dz`，`∂s/∂z = (−dr/dz + (4φ/π−L)dr/dz)/2`。
但 `chart_partials`（`chart.cpp:106-115`）写的是
`dd_dz = (4φ/π)·dr/dz`，**少了 `−L·dr/dz`**；南冠同样（`chart.cpp:118-127`）；
区间版本 `chart_partials_interval`（`chart.cpp:152-165`、`:167-180`）用的是同一个漏项公式。
而 `build_block_model` 把 `phi_centre = chart_face_longitude(...)`（含 `L·π/4` 的**绝对**经度，
`kernel.cpp:149-153`）直接传给这两个函数（:181、:202），所以漏项真实生效。

后果不是"界更宽"，而是**界与模型同错、残差不再包含真误差**：真实差 `l·dr/dz/2` 在块内随
`dr/dz(z)` 变化，而 `e_s` 只包含"错公式自身的变化"。数值见 §3 C1：在 f=0 的**面中经线**上
（φ=π/4），错公式给出 `ds/dz=0`，真值 0.79–0.97；1 像素块的真实模型误差比声明的 `e_s` 大 31 倍，
16 像素块大 12 倍。|Dec|>41.8° 的源全部命中。

**次要 P2**：`h_u=0.5*(u1-u0)`（`kernel.cpp:117-118`）是"舍入半宽"，不是文档要求的
`h_k ≥ max(c_k−U_k^L, U_k^U−c_k)`（`SPHERICAL_DRIZZLE.md:234` 明确点名此 1 ulp 漏包围并说
初版 bounds 已撤资格）；`chart_half_space_*` 的半空间常数 `offset` 在 double 里算完再
`from_double_exact`（:288-289），也没有把该舍入并入内外约束（`SPHERICAL_DRIZZLE.md:236`）。
两项都远小于 1e-16 量级，但"certified"的形式陈述必须降级或修正。

**`g` 的积分（`kernel.cpp:234-247`、`250-270`）**：`e_g=max(|g_L−A_H|,|g_U−A_L|)` 是
"区间 vs 仿射值域"的交叉配对，**这是有效的保守界**（对任意点，
`g−affine∈[g_L−A_H,g_U−A_L]`），只是比式 (16) 的导数均值余项粗；这一点我确认实现是对的，
不在缺陷列表。`solid_angle_density_gradient`（`tan_sip.cpp:229-230`）的 `ddet_du/ddet_dv`
各漏一项（`(1+au)·buv`、`(1+au)·bvv`），但因为 `g` 只用"代表值+交叉区间界"，
错梯度只让 `e_g` 变大、不破坏包含性——**P2，且必须注明"这不是 §6 的导数余项"**。

### 2.2 候选完备性（I7）：face 缝、z=±2/3、apex、clamp

- `build_block_model` 只检查 **φ 是否在该 face 的分支内**（`kernel.cpp:157-159`），
  **没有检查 `s,t∈[0,1]`**。而等面积 chart 里，赤道 face（J=3）的方块条件是真
  `|d| ≤ min(3−r, r−1)`，比 `|d|≤1` 严格得多（`chart_from_face` 的 band 分支，
  `chart.cpp:71-74`）；北/南 face 的方块也不同。所以"φ 在分支内"**不保证** `s,t` 在方块内。
- `chart_interval`（`chart.cpp:188-218`）按 `z.lower/z.upper` 二选一分支：
  `z.lower>2/3` 走冠、`z.upper<−2/3` 走南冠、否则走 band。**跨 z=±2/3 的块走 band 公式去算冠点**。
  `build_block_model` 明明算了 `pure_band/pure_north/pure_south`（`kernel.cpp:163-165`），
  **三个变量从未使用**——分支歧义没有被拒绝。文档要求"跨 r=1/3 的区间分别求值取并包"
  （`SPHERICAL_DRIZZLE.md:45`）。
- 候选索引用 `floor(s*N)` 直接转 `long long`（`kernel.cpp:550-553`），越界（负数或 ≥N）不报错，
  交给 `chart_leaf` **clamp**：`cx=min(N−1,ix)`、`cy=min(N−1,iy)`（`chart.cpp:220-228`）。
  负 `s_L` 会被 cast 成巨大 `U64` 再 clamp 到 `N−1`（多一个空候选，只浪费）；
  而 `s_U>1` 时 `ix_hi>N−1` 会把**多个互不相交的 s 窗口全部映射到同一个叶**（§3 C3），
  kernel 侧是"错叶"，containment 侧还会**重复累加整格面积**（`containment.cpp:610-611` 同样 clamp，
  且每格加一次 `cell_area`，`:613-620`）→ `Σ_p a_p > A_j`，I5 直接被破坏。
- apex：`chart_interval` 在 `r.lower≤0` 时返回 false（`chart.cpp:197,206`），`build_block_model`
  返回 false 并置 `apex`（`kernel.cpp:519-523`）；`cut_cell_overlap` 在 `depth+1≥max_depth` 时
  `termination=unsupported, reason="chart_apex_singular"`（:525-528），否则细分，最终
  `keep_unresolved`（:538）。这条路径是**正确的诚实停止**（不归零），但与 `geometry.overlap.md:97`
  要求的"到 max_depth 或极点 chart 无定义触发 target A"不同：主核**自己不做** fallback，
  是否做由 `overlap.cpp:453-468` 事后决定，且**完全不看 `budget.arithmetic.allow_target_fallback`**
  （该字段在 `src/geometry` 里零引用，只在 `wcs_process.cpp:107` 被打印）。
- 细 sliver / 切触：`integrate_affine` 对 `<3` 顶点返回 0（`kernel.cpp:255`），
  `clip_half_space` 只在 `denominator` 非零时插值（`exact_polygon.cpp:38-46`），
  接触得到零面积多边形；正 sliver 会得到正 `inner` 或留在 `outer`。**这部分是符合"采样未见≠0"的**。
- `f≠1`：`compute_overlap` 对 native rings 与 `contract_rings` 后的 drop 各算一次面积
  （`overlap.cpp:413-414, 430-431`），kernel 在 `drop_rectangle` 上做（:437-438）——
  **收缩只做一次**，这一点符合 `geometry.overlap.md:95`。但注意 `A_j` 来自 `footprint_area`
  的"仿射 g 积分"，`a` 来自"bbox 上的 cut cell"；两者对**非轴对齐矩形**不一致（见 2.3）。
- 负信号：几何层不接触 `x`，没问题；Drizzle 层的符号处理见 2.4。

### 2.3 源域形状：旋转矩形被静默当成外接 bbox（P1）

`rings_are_axis_aligned_rectangle`（`overlap.cpp:114-129`）只要求 **1 个 ring、4 个顶点**，
然后把顶点 min/max 当矩形返回——**不检查边是否轴对齐、是否闭合、顺序是否 CCW**。
`detector_footprint` 用 `set.detector_from_sample`（2×2，可为旋转/剪切）映射 local cell 的 4 个角
（`overlap.cpp:260-280`），`block_footprint` 直接用轴对齐的块（`normalize.cpp:150-160`）。
于是：只要 `detector_from_sample` 带旋转，`A_j` 是**真实旋转后四边形**的面积
（`footprint_area_rational` 用 ring 的 shoelace，`kernel.cpp:716-725`），而 `a` 是**外接 bbox** 的
切片积分 → `Σ_p a_jp` 对每个叶都系统性偏大、`a` 可能超过 `A_j`，`w=a/A` 被
`w.upper=min(1,…)`（`drizzle.cpp:190`）**掩盖**。这违反 I1/I5/I6。
最小反例：`detector_from_sample` 取 45° 旋转（见 §3 C6）。

### 2.4 约束矩阵、支持与面积守恒（I5/I6/I9）

`overlap.cpp:616-655` 声明四类行：`a≥0`、`a−A≤0`、`A−Ω≤0`、
`Σa + coarse − A = [0,0]`（:647-655）。问题：

1. **`Σa+coarse−A=0` 是"精确等式"的声明，但代码从不校验它**。`kernel.total_lower/total_upper`
   算出来了（`kernel.cpp:668-669`），除了 `overlap.cpp:773` 用 `total_upper` 估位数外**没有任何
   与 `A` 的比较**。如果主核残差失效（P0-5）或缝处 clamp（P1），这条约束事实上不可行，但没有
   任何 `Validity` 被降级，下游拿到的仍然是一个 `certified` 的 Overlaps。
2. **缺 `a ≤ Ω_H`**（I6）。一个叶的 `a` 可以大于自己的面积（缝 clamp 的重复计数、
   或极区错模型），`w≤1` 的截断又把它藏起来。
3. **支持面积用 hull 而不是 union**：`drizzle.cpp:177-186` 里 `area = hull(area, a)`，
   即"该叶上单源 a 的 min/max"，随后直接作为 `SupportMap.area_lower_sr/area_upper_sr`（:299-306、329-330）。
   对"两个源各覆盖叶面积一半"的叶，真 union=Ω_H，而 `area_upper=max a ≤ Ω_H/2` →
   **upper 低估 union**（I9）。文档明确 `max a_L ≤ S ≤ min(Ω_H, Σa_U)` 只是粗界，
   但在这里连 `min(Ω_H, Σa_U)` 都不是。
4. **kernel 与 containment 冲突时"kernel 赢"**（`overlap.cpp:521-538`）：containment 只对
   `published` 里**不存在**的叶生效。可主核给每个访问过的候选格都建了 ledger 条目
   （`kernel.cpp:627-631`），哪怕 `inner=outer=0`。于是"主核证明 0、独立算法证明 x>0"时，
   发布的是 0，`x` 既不进叶、也不从 coarse 扣（`containment_added_upper` 只累加新增叶，
   :534-537）→ **总面积仍守恒（coarse 里还留着），叶归属错**（§3 C4）。正确做法是
   取两者**交**（`max(L1,L2),min(U1,U2)`）并在空交集时报矛盾；`SPHERICAL_DRIZZLE.md:259`
   也是"可合并独立有效界取交集"。
5. `coarse_row` 的 upper 用 `kernel_upper − containment_added_upper`（:561-572），
   但 `containment_added_upper` 只统计"新增叶"的 upper，没统计"冲突叶"的 lower——
   两边都不算它，所以这个"剩余未决质量"并不是一个可证明的界（只是两者之差，无单调性保证）。
6. `bound_validity=certified` 仅由 `termination==tolerance_met` 决定（:766-768），
   而 `tolerance_met` 的判据是"队列空且没有 unresolved 块"（`kernel.cpp:672-675`），
   **与 `budget.area_absolute_sr`、`per_leaf_absolute_area`、`normalizer_min_sr` 无关**——
   这三个科学停止字段在 `src/geometry` 里零引用；`overlap_process.cpp:228-238` 只校验前两个的
   有限性，`normalizer_min_sr` 连校验都没有。
   也就是说 `tolerance_met` = "没有未决块"，**不是**"达到请求的绝对面积/末值精度"。
   在 `area_absolute_sr=1e-12` 的 X01 里，只要块没留未决，就被标成 certified，
   没有任何 `Σ(a_U−a_L)` 与预算的比较。
7. `max_bytes`（内存账本）在 `src/geometry` 零引用；`overlaps->contributions` 超过 4096 直接
   `resource_exhausted`（`overlap.cpp:439-441`），没有文档要求的"单行过大时多遍/外存"路径
   （`geometry.drizzle.md:82`）。

### 2.5 取消路径（I8）

`kernel.cpp:573-583`：在候选格循环里发现取消时，只 `break` 出循环、置
`termination=unsupported, reason="cancelled"`，**没有把当前块未处理的其余候选格**加入
`unresolved`；随后 `while` 下一轮才把**队列里**的块 `keep_unresolved`（:484-490）。
当前块被处理过的那部分已进 ledger，未处理的部分**静默丢失** → `total_upper < A`。
取消是合法工程路径，但这里给出的是"面积少了"而不是"未决质量保留"。

### 2.6 Drizzle：F/N/R/B 与同一 R（I1/I2/I3/I4）

`build_drizzle`（`src/geometry/drizzle.cpp`）的实际公式：

```
by_leaf: (order,ipix) -> 贡献下标列表                 (drizzle.cpp:123-127)
每个叶内对贡献循环：
  omega = hull(omega, a_prim)   drop = hull(drop, drop_prim)   area = hull(area, a)   (:177-186)
  w = divide(a, {drop.upper, drop.lower}); w.upper = min(1,w.upper)                     (:189-190)
  weight_sum += w;  numerator += w*x[source_row]                                        (:191-198)
  nominal_w = 0.5*(w.lower+w.upper)                                                     (:199)
N = weight_sum * omega                                                                   (:202)
B = numerator / N; published = 0.5*(B.lower+B.upper)                                    (:232-236)
CSR 行值 = nominal_w（不是 w/N）                                                        (:242-245)
noise = control_covariance(rows) = w C wᵀ                                               (:393-399)
response.spatial_actions = rows = w                                                     (:355-369)
```

对照 I1–I4：

- **P0-1**：`omega`、`drop` 是**跨源** hull。`N_p` 应为 `Σ_j w_jpΩ_j`，实为 `(Σ_j w_jp)·[minΩ,maxΩ]`。
  单源叶正确；多源叶（生产里绝大多数，因为叶宽 ≫ 像素）在 `Ω_j` 不等时错。§3 C2 的常量场例子：
  真 `B=1`，发布点值 `1.5625`（源顺序 A=1 先）或 `2.2656`（A=4 先）——**且点值依赖贡献顺序**。
- **P0-2**：`R_pj=w_jp/N_p`。代码里 `rows=w`、`op->normalizer_sr` 存了 `N` 但**全仓库无消费者**
  （只有 `drizzle.cpp:426` 赋值）；噪声 `w C wᵀ` 与 `R C Rᵀ` 差 `diag(1/N_p²)`；response 也是 `w`。
  所以 signal 的 `/N` 是**另一条独立路径**（`divide(numerator,n)`），PSF/noise/background
  拿到的是 `w`——**不是同一 R**（I2 被违反）。下游 `sky.cpp:1950` 把 `signal->noise` 当信号的
  噪声用，于是方差被放大 `N_p²` 倍。
- **P0-3（点值）**：`B` 发布为保守区间的中点（:234）。`w` 的区间在有未决 sliver 时是
  `[0, a_U/A_L]`，中点可以是真值的两倍；`N` 的区间同理。这违反 I3，而且**区间包含性照旧成立**，
  所以"B∈[B_L,B_U]"的测试会通过。
- `drizzle.cpp:195`：`value = source_row < source_count ? x[source_row] : 0.0` ——
  源号越界**静默填 0**，违反"未知误差不填零"（AGENTS.md:13）。例如 block 路径的源号语义错位
  （§4），这里不会报错，只会悄悄用错值。
- `drizzle.cpp:249-253`：所有叶都无支持时才失败；单叶无支持保留 NaN。
  `error->achieved` 对 NaN 叶调用 `dyadic_from_double(NaN)`（:343-349）→ 无定义的 dyadic。
- 单位：`estimate->signal` 存的是 `unit_sr_per_pixel()`（:280-281），而
  `estimate->unit`/`estimator.unit` 是 `observation->unit`（:379-382）——**单位不一致**。
- `overlaps->recipe` 为空时跳过 order/pixfrac 身份检查（:100-110）；而
  `normalize.cpp:1023-1040` 合并后的 Overlaps 正是 `recipe=nullptr`（见 2.7）→ **检查被绕过**。
- 外存 CSR（I11）：`build_drizzle` 用 `std::map` 全量常驻 + 一次性 CSR（:123-146, 316-318），
  没有 `geometry.drizzle.md:70-82` 的两遍/行 spool/外存归并；`normalize` 把**所有块**的
  contributions 攒进一个 `std::vector`（`normalize.cpp:1012-1013`）。对 16384 像素、每像素 ~4 叶
  就是 65536 条，尚可；但这不是合同要求的结构，且 `(source,leaf)` 没有先并（同一 source 重复列）。
- 同一叶的多源 `row` 允许重复 `source_row` 列（:198），apply 会相加；规范要求先并（:73-74）。
  对当前"每块源只出现一次"的调用没问题，但对多块/多帧拼接会重复计面积。

### 2.7 修订与身份（I10）

- `overlap.cpp:792-793` `Overlaps` 的 content hash 是常量 `"acsd.overlaps"`；
  `GeometryConstraints`（:679-680）是 `"acsd.geometry.constraints"`；
  `GeometryRecipe`（:753-754）`"acsd.geometry.recipe"`；`drizzle.cpp:371-372` 的
  `SpatialResponse`、:403-404 的 `SignalEstimate`、:449-450 的 `DrizzleOperator`、
  `refine.cpp:137-138` 的 `GeometrySnapshot`、:155-156 的 `GeometryRevision` 也都是常量。
- `make_identity` 令 `object_id = schema_id + ":" + content_hash`、`revision = 1`
  **恒定**（`src/model/units.cpp:252-265`）。所以任何几何变更后 `object_id` 与 `revision` 都不变；
  `refine_geometry` 的 `Replacement{old_hash=new_hash}`（`refine.cpp:111-114`）是恒等映射，
  `affected_*` 闭包与下游缓存无从失效。这直接违反 G3 "几何修订必须使依赖它的 R/PSF/noise/sky/选择/权/Export
  旧对象失效后重新推导"（`07-SPHERICAL-GEOMETRY.md:26`）与 `geometry.drizzle.md:88`。
- `refine_geometry` 另外三点：`affected_domains` 永远是空（`refine.cpp:50` 定义，从不 push）；
  若某个 recipe 的 `source_rows` 为空则 `continue` 不 push（:79），而后面按**下标**配
  `base->source_inputs`（:122-131）→ 之后的 Drizzle 会用**错的观测**重建；没有任何事务性
  替换/一致性校验（`status` 只是局部变量）。
- `normalize.cpp:1023-1040`：`merged` Overlaps 只有 `contributions` 与常量身份；
  `primitives/constraints/coarse_candidates/recipe` 皆空，`status` 是**默认构造**
  （`Termination{0}=tolerance_met`、`Validity{0}=certified`、`SupportState{0}=absent`，
  `include/acsd/model.hpp:50-52`）。于是：`op->status` 声称 certified（`drizzle.cpp:428,433-448`），
  `build_drizzle` 的身份检查被跳过，`SupportMap.primitives` 变成 `""`（:327），
  未决质量与 `Σa+coarse=A` 从产品链上**消失**。这是 P0。

---

## 3. 反例（可手算/可复算，附预期数值）

> 全部用纸面公式或独立 Python 复算（脚本见附录 A 的算式；未调用生产 helper）。
> 每个反例都说明"什么错误实现会在普通面积/常量场测试下通过"。

### C1（P0-5）极区 face 的仿射模型与残差界同错：面中经线上 `ds/dz` 应为 ~0.9，代码给 0

取北冠 face `f=0`（`chart.cpp:kJ[0]=2, kL[0]=1` → J=2、L=1），方向满足 `z=sin δ`、`φ=Lπ/4=π/4`。
用真实 chart 公式（`chart.cpp:62-65`）：
`r=√(3(1−z))`，`d=r(4φ/π−1)`，`s=(2−r+d)/2`。在 φ=π/4 时 `4φ/π−1=0`，故 `d=0`，
`∂s/∂z = (−dr/dz + 0)/2 = 3/(4r)`：

| z | r | s_true | ds/dz 真 | ds/dz 代码（`chart.cpp:109`） |
|---|---|---|---|---|
| 0.70 | 0.948683 | 0.52565835 | **+0.790569** | **0** |
| 0.75 | 0.866025 | 0.56698730 | **+0.866025** | **0** |
| 0.80 | 0.774597 | 0.61270167 | **+0.968246** | **0** |

`z: 0.70→0.80` 时真 `Δs=+0.08704331`，仿射模型预言 0。按 `kernel.cpp:231-232` 复算残差：
块 `z∈[0.78,0.80]`、`φ∈[π/4±0.02]` 时 `e_s=7.90e-4`，而真实 `max|s_true−s_model|=9.81e-3`
→ **声明值小 12.4 倍**；一像素大小的块（`Δz=1e-4`）比值 **31 倍**。
按叶宽换算：块的真实 s 误差在 `N=2^18` 上相当于 2571 个叶（声明只有 207）；一像素块相当于
12.4 个叶（声明 0.4）。

**为什么"面积总和守恒/常量场"测试抓不到**：候选枚举用的是 `chart_interval`（**正确**的包络，
`chart.cpp:188-218`），所以所有候选叶都在枚举里、总面积仍然闭合；错的只是内/外多边形与残差。
把 `a` 从叶 X 挪到相邻叶 Y，`Σa` 与常量场 `B=b0` 都不变，只有逐叶成员错。

### C2（P0-1/P0-3）常量场 + 多源同叶：点值 `B` 应为 1，实得 1.5625/2.2656，且随源顺序变

一个叶，两个源：`A_1=Ω_1=1, A_2=Ω_2=4`，都完全覆盖该叶（`a_1=1, a_2=4`），
常量面亮度 `b0=1` 的积分输入 `x_j=b0·Ω_j` → `x=(1,4)`。真值：
`N=1·1+1·4=5`，`F=1+4=5`，`B=1`。
按 `drizzle.cpp:160-236` 逐行复算：

| 贡献顺序 | `w_sum` | `omega`(hull) | `N` 区间 | 发布 `N` | `B` 区间 | **发布 `B`** |
|---|---|---|---|---|---|---|
| A=1 先 | [2,2] | [1,4] | [2,8] | 5.0 | [0.625,2.5] | **1.5625** |
| A=4 先 | [1.25,2] | [1,4] | [1.25,8] | 4.625 | [0.53125,4] | **2.2656** |

真值都是 1。**区间都包含 1**（所以"B 落在区间内"通过），但发布的点值错 56% / 127%，
并且**依赖贡献顺序**（`by_leaf` 内部按 push 顺序，`overlap.cpp:539` 的 `std::map` 顺序又依赖叶号）。

### C3（P1）face 缝处 `chart_leaf` 的 clamp：同一叶重复/错叶，containment 还会整格重复

等面积 chart 里赤道 face（J=3, L=0）的方块条件是 `s=(3−r+d)/2∈[0,1]`。
取 `N=64`、`z=0`（r=2）、`d=1.02`（φ=π/4·1.02，刚出方块）：
`s=(3−2+1.02)/2=1.01`、`t=(3−2−1.02)/2=−0.01`。一个跨缝的 drop（box `s∈[0.99,1.02]`）
在 `containment.cpp:526-529` 得 `ix_lo=floor(0.99·64)=63`、`ix_hi=floor(1.02·64)=65`；
循环 `ix=63,64,65` 全部经 `chart_leaf` clamp 到 `ix=63`（`chart.cpp:222`）：
若三格的 `inside` 判定都成立，**同一个叶**累加 3 个 `Ω_H` → `a=3Ω_H > Ω_H`，
`Σ_p a_p > A_j`（I5/I6 同时破坏）。
主核侧不走这条"整格"路径，但同样把 `ix=64,65` 的 s 窗口记到 `ix=63` 的叶上（`kernel.cpp:583-588`）
→ 区块面积归属到错误叶。真叶应在**另一个 face**（北冠/赤道邻面）。

**为什么普通测试抓不到**：总面积 `Σa+coarse` 仍可能恰好等于 `A`（窗口在 s 上互不重叠，
union 覆盖 block），常量场 `B=b0` 也不变；只有"逐叶 a 的上界 `a≤min(A,Ω_H)`"或
"该叶的 HEALPix 中心是否在 drop 内"这类独立性谓词能抓。

### C4（P1）kernel 与独立算法矛盾时"kernel 赢"：总面积守恒，叶归属错

沿用 `SPHERICAL_DRIZZLE.md:308` 的 tiny sliver：真 `a_X=1.79434e-14 sr`。
设主核因 C1 的残差失效把叶 X 证成 outside（`inner=outer=0`），独立 containment
（算法 A）给出 `[1.79207e-14, 1.79661e-14]`。`overlap.cpp:531-538` 因 X 已在 `published`
（值为 `[0,0]`）而**跳过** containment；`coarse_upper` 不扣 X 的质量。
结果：`a_X=0`（叶 X 的 `w=0`、该叶无信号），`Σ_p a_p + coarse = A` 仍然成立，
总通量 `Σ_p F_p=Σ_j x_j`（把 coarse 计入时）也成立。

**为什么普通测试通过**：面积/通量守恒把未决质量算在 coarse 里，`B` 在"有支持"的叶上仍是 b0。
只有"逐叶 a 与独立算法的交集"或"叶 X 的 a 下界"能抓。正确做法是取交并在空交集时报矛盾。

### C5（P0-6）block>1 的源号语义：`x[block]` 是单个像素，亮度差 ~`block_size²`

X01：`128×128`、`ACSD_DRIZZLE_BLOCK=16` → 64 个块（`normalize.cpp:130-134`），
`source.sample=block`（:139），`compute_overlap` 把 block 号写进 primitive source row
（`overlap.cpp:492-506`），`build_drizzle` 取 `x[source_row]`（`drizzle.cpp:194-195`）。
`x` 是**每像素**的 `integrated_values`（`observation/calibration_kernel.cpp:480`，长度 16384）。
于是 block 0 拿到的是像素 0 的值，`N` 却是该块 16×16 像素的 `Ω_block`、`w≈1`：
`B_code ≈ x_0/Ω_block`，而正确值是 `(Σ_{256 像素}x)/Ω_block`。
均匀场下 **`B_code/B_true ≈ 1/256`**（一般场是同数量级的错）。同时 `w≤1` 的截断、
`Δ` 的区间都在"看似正常"的范围，不会触发任何失败码。

**为什么 `measured_stars` 位相同不能证明 drizzle 等价**：`measured_stars` 在
`normalize.cpp:689` 记录，处于星测量阶段；几何/Drizzle 在其后（:936-1060）。
星测量消费的是观测/源测光产物，不是本 process 的 `DrizzleOperator`。
因此两者相同**在结构上就与 drizzle 产品无关**。工程档案也已经承认 block 路径
"not claimed correct"（`validation/apps/evidence/x01/attempts/geometry_performance_baseline.md`：
块内归属误差最大 1.0、RMS 0.1055，权重矩阵 64 列）。

### C6（P1）旋转 footprint 的 `a` 取外接 bbox

`detector_from_sample = [[cos45,−sin45],[sin45,cos45]]`（45° 旋转），1 个 local cell。
`detector_footprint` 先旋转 4 个角（`overlap.cpp:270-278`），
`rings_are_axis_aligned_rectangle` 只取 min/max（:117-127）→ `drop_rectangle` 是边长为
`√2·w` 的轴对齐方框；`A_j` 却用旋转四边形的 shoelace（`kernel.cpp:716-725`）。
于是 `Σ_p a_jp ≈ 2·A_j`（bbbox 面积 = 2× 正方形面积），`w=a/A` 最高到 2，
被 `drizzle.cpp:190` 截成 1。`Σa+coarse=A` 不可行，但没人检查（§2.4）。

---

## 4. 默认 `ACSD_DRIZZLE_BLOCK=1` 是否科学必要？以及可证明等价的批量化

### 4.1 事实与判断

- 几何**本身**的目标量对"源的划分"没有要求：`a_jp` 对互不相交的源分区是可加的，
  `Σ_{j∈分区} a_jp = a_{分区,p}`。所以"把 256 个像素合成一个源行"在**几何上**完全合法，
  只要该源行的 `x`、`Ω`、`A` 也按同一分区聚合。
- 但**当前数据模型**把源行 = 像素写死：`integrated_values` 每像素一条
  （`observation/calibration_kernel.cpp:480`），`block_footprint` 却把源号设成 block
  （`normalize.cpp:136-139`），`build_drizzle` 用 `x[source_row]`（`drizzle.cpp:194-195`）。
  所以 **block>1 今天不是"更快地算同一件事"，而是算别的事**（C5）。
- 即便把 `x` 正确聚合，block>1 仍是**另一个估计量**：式 (2) 的 `w_jp` 是逐像素的，
  块版本把 `w` 在块内取了常值（块作为一个源行），差别是块尺度的二阶项；块跨叶时更大。
  因此"block=16 与 block=1 的 drizzle 产品等价"**在数学上不成立**，只能证明"在容差内接近"，
  而块宽/叶宽 ≈ 1（X01 的 16 px ≈ 0.8″ vs 叶 0.82″）时二阶项并不小。
- **结论**：默认 `block=1` **是科学上必要的**（在现有式 (1)(2) 与现有 observation 数据模型下，
  它是唯一实现 `F=Σ_j w_jp x_j` 的源划分）。>20 分钟的原因**不是**几何精度要求，
  而是 16384 次独立 invocation 的开销；历史档案（几何 2.06 s / 64 个 16×16 行 = 32 ms/行，
  整帧 75.3 s / 16384 行 = 4.6 ms/行）说明每行几何约 4.6 ms，而 lead 实测 73 ms/行，
  其中 ~15–16 倍需要另行归因（invocation 计划/存储/codec、或某些块走 `build_block_model`
  失败→细分到 `max_depth=64` 的最坏路径，`kernel.cpp:524-540`）。**在归因清楚前，不得用
  "提高 block"掩盖这 16 倍。**

### 4.2 可证明等价的批量化条件（不改代码，只给条件）

记 batch = 一次 invocation 内处理的一组源像素 `{j_1..j_m}`（互不相交的 detector 矩形）。
**若且仅若**下列条件全部成立，batch 的输出 = 逐像素输出拼接（逐位相同），只是省掉 m−1 次
invocation 开销：

- **B1（源行不变）**：每个子矩形的 `source.sample` 仍是像素号；`x` 仍按像素取；
  不得把 block 当源行。
- **B2（逐子矩形建模）**：`chart_enclosure`/`build_block_model`/残差/内-外裁剪**逐子矩形**做。
  不能用 batch 的外接矩形（否则 `e_s` 随 batch 尺寸增长，界与结果都变）。
- **B3（逐 (source,leaf) 记账）**：ledger 键仍是 `(source_id, leaf)`；候选枚举逐子矩形；
  `Σ_{batch} a` 可加（真叶分割球面）——这正是式 (1) 的直接后果。
- **B4（共同约束只算一次）**：`Ω_j,A_j` 与 `a` 用同一 ledger/同一 revision；
  provider 的 `model()`、`WcsIdentity`、chart 常数缓存可跨子矩形共享（不可变）。
- **B5（固定归约）**：输出按 `(leaf, source)` 排序后归并，累加次序固定，才能逐位复现。
- **B6（取消/预算按子矩形边界）**：取消只在子矩形之间检查；已算的子矩形结果保留，
  未算的进 unresolved（不能像 `kernel.cpp:573-583` 那样丢当前块）。
- **B7（Drizzle 侧）**：`by_leaf` 的源集合、`N` 的求和键与逐像素调用完全一致；
  containment fallback 仍以**子矩形**为 drop 计算，不能以 batch 为 drop。

在这 7 条下，batch 是"同一个数学算子的行块"，可以证明等价；缺任何一条都不行
（尤其是 B2/B3/B7）。

**另外一条更强的、可证明的加速（推荐优先做，因为它改变的是"内部归属"而非源划分）**：
- **R1（interior-run 合并）**：若一个连续像素 run 的全部 4 个角与块的 s/t 包络都严格落在
  **同一叶** cell 内（用 `s,t∈[ix/N+e_s,(ix+1)/N−e_s]` 严格内证明），则该 run 的每个像素
  都有 `a=Ω_px`、`w≈1`；把 run 合成一条源行、`x_run=Σx_px`、`Ω_run=ΣΩ_px`、
  `A_run=Ω_run` 是**精确**的（`a` 可加、`w=1`、`N` 贡献 `Ω_run`），且噪声按同一 0/1 聚合矩阵
  求和（对角噪声时方差相加，同根交叉项保留）。边界 run 仍逐像素。
  在 `N=2^18`、叶宽 ≈16 px 时，内部像素占比 ~90%，这能把 invocation 数与有理裁剪数都降一个量级，
  **且不改变 block=1 的科学量**（它是 block=1 的精确重排，前提是 R1 的严格内证明）。
- **R2（资源账本）**：一次 batch 的 `consume_work` 必须等于其子矩形 work 之和（不得重置），
  `max_bytes` 按 batch 的活跃多边形/ledger 计；`plan` 的 `work_upper` 用 `Σ` 而不是
  `max_work`（现状是 `plan_build_drizzle` 直接把 `budget.max_work` 当 work 上界，
  `drizzle_process.cpp:207-212`，这会让账本失去"实际工作量"的含义）。

**不要做的**：用"block 大一点"来省时间（C5/C6 + 档案已证伪）；用"batch 的外接 bbox"做候选
（会与 C6 同类地把 bbox 当 drop）。

---

## 5. 逐 operation 裁决

| operation / 函数 | 裁决 | 依据（file:line） | 最小输入与预期数学值 | 错误实现会怎样通过常量场/面积测试 |
|---|---|---|---|---|
| `make_footprint_provider` / `forward_interval` / `boundary_interval` / `derivative_interval` | **可保留**（WCS 层不在本审查范围；B1 后四则/sqrt 已修） | `coordinates/*` | — | — |
| `footprint`（收缩+曲边） | **可保留**，需补：4 顶点 ring 的轴对齐/方向/自交校验 | `overlap.cpp:114-129, 289-348` | 旋转 45° 的 4 角 ring → 必须 `unsupported` 或保留真实四边形 | bbox 当 drop 时 `A` 与 `a` 口径不同，`Σa=2A` 但没人查 |
| `measure`（Ω_j, A_j） | **可保留，需修**：非矩形 ring 的 g 积分用 bbox 的 `e_g`（保守），但要求 ring 校验；`A_j` 必须同时给出 `A_j>0` 证书 | `overlap.cpp:354-385`、`kernel.cpp:683-743` | 单位正方形 TAN：`Ω=4 atan(ab/√(1+a²+b²))` | 中心采样 g 也能过常量场 |
| `compute_overlap` / `cut_cell_overlap` | **需修（主核可保留，证书需修）**：P0-5 偏导、P1 缝/clamp/跨界、取消丢质量、`area_absolute_sr` 未用、`Σa=A` 未校验、`a≤Ω_H` 缺失 | `kernel.cpp:112-248,430-677`、`chart.cpp:102-181,220-228` | 北冠 face 0、φ=π/4、`z:0.70→0.80`：真 `Δs=0.08704331` | 候选用正确包络 → 总面积守恒；错的只是逐叶成员（C1） |
| `target_containment` / `_apex` | **需修**：clamp 导致整格重复（`a>Ω_H`）、`cell_area_upper` 用 floor 除法（`containment.cpp:359,524`，少 <1 格），与主核冲突时取 union 而非交 | `containment.cpp:355-362,470-639` | C3 的 `s∈[0.99,1.02]` → 同叶 3×Ω_H | 总量仍可能等于 A |
| `refine_geometry` / `refinable.*` | **替换/需修**：身份常量 + 恒等 Replacement + `affected_domains` 空 + 下标错位无事务 | `refine.cpp:50,57-135,145-156`、`units.cpp:252-265` | 改 order：新 revision 必须产生新 `object_id`/`revision=2`，且旧 R 仍可读 | 同一 hash → 缓存命中旧 R，测试仍"一致" |
| `build_drizzle` | **需替换核心公式**：P0-1（hull）、P0-2（R）、P0-3（中点）、越界填 0、单位、NaN dyadic、CSR 无常驻限制 | `drizzle.cpp:122-247,299-318,355-399,426` | C2 的两源一叶 → `N=5,B=1`，发布必须 `1`（不是 1.5625） | 区间包含 b0；总量 `ΣF=Σx` 仍成立 |
| `normalize` 的几何合并段（工作流，非本 process 但消费本 process） | **需修（P0-4）**：合并必须携带 `primitives/constraints/coarse/recipe/status` 并做 `Σ` 校验 | `normalize.cpp:1014-1040` | 两帧/多块合并：未决质量必须进入 merged，`status` 不得默认 certified | 默认 `Termination{0}=tolerance_met` 直接冒充通过 |
| `healpix_candidate_leaves`（算法 B 的整数规则） | **规格缺口**：实现了，但不在生产路径；没有对拍记录 | `healpix.cpp:114-180` | 与 `chart_leaf` 对同一方向必须给出同一叶 | 只用 `chart_leaf` 时两者都错也一致 |
| 预算/账本 | **规格缺口**：`area_absolute_sr`/`per_leaf_absolute_area`/`normalizer_min_sr`/`max_bytes`/`allow_target_fallback` 在 `src/geometry` 零引用 | `overlap_process.cpp:228-238`、`wcs_process.cpp:107` | 请求 `area_absolute_sr=1e-12`：必须按 `Σ(a_U−a_L)` 判定 tolerance；不达 → unconverged | 只要没有 unresolved 块标 certified |

---

## 6. 建议修复顺序（不参与实现，仅排序）

1. **先修证书（P0-5 + P1 缝）**：`chart.cpp` 的极区分支加 `−L·dr/dz`；`chart_interval`/`build_block_model`
   对 `z=±2/3` 做分支并包；对 `s,t∉[0,1]` 的块**拒绝**（细分/未决），删除 `chart_leaf` 的静默 clamp
   （改为返回 `invalid` 或让调用者处理）。理由：这是唯一会让"每片真叶的 a"在无声中错的东西，
   且修复后 C1/C3 可由现有 `Σa≤…` 检查自动暴露。
2. **加运行时守恒校验（I5/I6）**：kernel 结束时比较 `total_lower ≤ A_L`、`A_U ≤ total_upper+unresolved_upper`、
   每叶 `a_U ≤ min(A_U, Ω_H)`；不满足 → `bound_validity=invalid`（具名 issue），不得 certified。
   把 `Σa+coarse−A=[0,0]` 从"声明"改成"校验+区间"。
3. **重写 `build_drizzle` 的 F/N/R/B（P0-1/2/3）**：逐 (j,p) 累加；`R=w/N` 唯一算子；
   噪声 `R C Rᵀ`；点值用 `w` 的最佳代表（未决则未决，不用中点）；删除跨源 hull；
   `x` 越界 fail 而不是 0。
4. **身份内容化（P0-3）**：所有 geometry/drizzle 对象 hash 改为内容哈希（依赖 WCS/order/pixfrac/
   budget/arithmetic/primitives）；`revision` 递增；`refine_geometry` 做事务校验。
5. **工作流合并携带约束（P0-4）**：`normalize` 必须合并 primitives/constraints/coarse/status/recipe，
   否则 drizzle 必须拒绝（`recipe==nullptr` 不能跳过身份检查）。
6. **block 路径二选一**：要么删除（默认 block=1 已是唯一正确路径），要么实现 C5 的 x/Ω/A/噪声聚合
   并把"block 版本"登记成**另一个**估计量（明确 product 语义）。在 3/5 修好前，block>1 必须
   fail-fast。
7. **性能（§4）**：R1 interior-run 精确合并 + B1–B7 多矩形单次调用；先归因 73 ms/行 vs 4.6 ms/行
   的差，再谈吞吐。`plan` 的 `work_upper` 改为 `Σ`。
8. **P2 清理**：`h_k` 用 `max(c−U_L,U_U−c)`；半空间常数并入区间；`cell_area_upper` 用 ceil；
   `g` 梯度补 `buv/bvv`；`pure_*` 变量要么用要么删；`allow_target_fallback` 接上；
   外存 CSR/spool 按合同实现。

---

## 附录 A：独立复算（仅证伪，不作门禁）

- C1/C2 的数值由两个 30 行 Python 脚本按本文公式逐项打字复算（`sqrt`、`hull`、`divide`、
  `0.5*(l+u)` 与 `src/geometry/drizzle.cpp:160-236`、`src/coordinates/chart.cpp:59-181` 逐行对应），
  输出即正文表格；脚本目录不入库。C1 的表由 `r=√(3(1−z))`、`s=(2−r+d)/2`、
  `∂s/∂z=(−dr/dz+(4φ/π−1)dr/dz)/2` 直接算出；C2 的表由 `hull/divide/min(1,·)/中点` 直接算出。
- 未运行 `cmake`、未跑任何生产命令、未改 `src/`。§2 的关键判据都是"从
  `file:line` 读出公式，与 §1 的定义对照"，不是测试结论。
- 可以复核 C2 的最短路径：把两个 `OverlapContribution`（`A=Ω=1`、`A=Ω=4`、`a=A`）按两种顺序
  喂给 `build_drizzle`，`x=(1,4)`，看 `estimate.signal` 是否为 1.0——这个实验不需要几何，
  但它**不能**替代 §1 的代数论证（只能证伪）。
