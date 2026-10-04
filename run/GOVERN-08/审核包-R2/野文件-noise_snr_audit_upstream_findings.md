# NOISE_SNR.md 事实/一致性审计 — 上游权威链提取结果

> 只读审计。未做任何 git 写操作，未修改任何被审计文件（本文件为新增审计报告）。
> 工作目录 `/workspace/Astro CS Database`；下列命令均在该目录下可直接复跑。

---

## 0 结论速览

| # | 结论 | 严重度 |
|---|---|---|
| A | **NOISE_SNR §3.5 的方差公式与 `D_p²` 等价参数化、`pixfrac⁴` 说法与代码完全一致**，判定 NOISE_SNR 此处正确 | ✅ 无问题 |
| B | **没有任何文档说"守恒在掩膜/部分覆盖/NaN 下被破坏"**；守恒不变量与样本级掩膜重归一规则在同一批文档内并存且互不引用 | 🔴 高（口径空洞） |
| C | `docs/detail/registry/acsd.phase1.drizzle.md:70-71` 明写现行实现"值 NaN **传播、不掩膜**"，与 `DRIZZLE.md:225`、`DISP-DRZ-004` 直接冲突 | 🔴 高（事实错误） |
| D | `acsd.phase1.drizzle.md:87` 用 **"严格通量守恒…全 `pixfrac ∈ (0,1]`"** 作无条件断言，比 NOISE_SNR 强得多，且比其本级上游 `DRIZZLE.md:163` 的条件式表述过冲 | 🔴 高（过度断言） |
| E | `DATA_SEMANTICS.md:62,65` 把 **`w_jp = a_jp/A_pixel,j`**（被 `DRIZZLE_GEOMETRY.md:62` 明令"不满足通量守恒门"的口径）当作 `w_jp` 的定义，并写 `variance = v_num_sum/D_p²` 而**不提 k²** | 🟠 中（符号冲突 + 漏因子） |
| F | `PHASE_PRODUCT_EXCHANGE.md:150-164` 在声明 "`W_p` 在 Phase 1 drizzle drop 分支不成立" 后，规则 1a 仍写 `Var_p = Σ V_j w_jp²/W_p²` | 🟠 中（自相矛盾 + 无 pixfrac⁴） |
| G | `ACSD_DESIGN.md:118-119` 的 "chart + 鞋带、闭合**不依赖数值逼近**" 与 `DRIZZLE.md:169`（实测闭合偏差、超额/亏损分别判红）、`DRIZZLE_GEOMETRY.md:337-345`（极冠 apex 弦亏缺 ≥1−2√2/π ≈ 9.97e-2，比弦长预算大 4 个量级）冲突 | 🟠 中（顶层设计 vs 正本） |
| H | 边缘像元/部分像元覆盖：**not found** — DRIZZLE.md 与 DRIZZLE_GEOMETRY.md 均未讨论源像元位于 CCD 边缘时的足迹截断 | 🟡 中（覆盖缺口） |
| I | `CONFIG.md:57` 指向 `DRIZZLE.md`「校验项清单」一节 —— 该节**不存在**；`FZ-COND-FLUX-CONSERV` 无定义处 | 🟡 低（悬空引用） |

---

## Q1 — CONSERVATION STRENGTH

### Q1.0 全部「通量守恒 / 绝对守恒 / 严格守恒」断言清单

复跑命令：

```bash
grep -rn "通量守恒\|绝对守恒\|严格守恒" docs/
```

| # | file:line | 原文（逐字） | 分类 |
|---|---|---|---|
| 1 | `docs/ACSD_DESIGN.md:117` | 「对连续信号是**通量绝对守恒**的平面到球面 drizzle」 | **(b) 条件式**——限定"对连续信号"，且紧接的 `:118-121` 才给条件；单独读 `:117` 像无条件 |
| 2 | `docs/science/drizzle/DRIZZLE.md:5` | 「对连续信号是**通量绝对守恒**的重采样（drizzle），对稀疏控制点是同一几何框架下的点分配」 | **(b) 条件式**——"对连续信号"是限定词，同句已排除控制点层 |
| 3 | `docs/science/drizzle/DRIZZLE.md:13` | 「1. **总通量守恒**：`Σ_p F_p = Σ_j x_j`，与 `pixfrac` 无关」 | **(b) 条件式**——本行**本身无限定词**，但同文档 `:163` 给出前提「守恒成立的前提是每个 drop 的面积在其覆盖的叶上被精确分完」。**单读第 13 行即为无条件断言** |
| 4 | `docs/science/drizzle/DRIZZLE.md:163` | 「**守恒成立的前提**是每个 drop 的面积在其覆盖的叶上被精确分完」 | **(b) 这就是条件句本身** |
| 5 | `docs/science/drizzle/DRIZZLE.md:179` | 「不参与任何通量守恒的求和——守恒是**对连续信号面成立的性质**，对点样本没有对应物」 | **(b) 条件式**——明确排除稀疏控制点层 |
| 6 | `docs/science/drizzle/DRIZZLE.md:210` | 「\| 通量守恒门 \| `Σ_p Σ_j x_j w_jp = Σ_j x_j`，相对闭合在双精度内 \|」 | **(b) 条件式**（门） |
| 7 | `docs/science/drizzle/README.md:3` | 「守恒映射算子（平面到球面的**通量绝对守恒**重采样）…」 | **(b) 弱条件式**——但同句末尾提到「**守恒前提**」 |
| 8 | `docs/science/algorithms/DRIZZLE_GEOMETRY.md:50-51` | 「**注意：F&H 2002 全文从未陈述 partition of unity**，故 `Σ_o a_io=1` 是**本仓依据 drop 恰覆盖输入像元、邻接输出像元无空隙这一几何事实作出的推断**，不是原文明述的定理」 | **(b) 条件句，且是全仓最诚实的表述** |
| 9 | `docs/science/algorithms/DRIZZLE_GEOMETRY.md:56` | 「`Σ_p w_jp = 1` ⇒ `Σ_p F_p = Σ_j x_j`，**与 pixfrac 无关**」 | **(b) 条件式**——推论前提 `Σ_p w_jp = 1` 正是 `:50-51` 的推断 |
| 10 | `docs/detail/registry/acsd.phase1.drizzle.md:87` | 「这是唯一满足**严格通量守恒**（累加的通量总和等于源端积分通量总和，**全 `pixfrac ∈ (0,1]`**）的口径」 | **(a) 无条件断言**——见 Q5，最高风险条目 |
| 11 | `docs/science/unified/DATA_SEMANTICS.md:236` | 「**总通量守恒**：非恒定场的输出总通量必须等于输入总通量（守恒判据）；两条互补，缺一不可」 | **(b) 弱条件式**——限定"非恒定场"，但未提掩膜/NaN/部分覆盖 |
| 12 | `docs/science/unified/UNIFIED_SCIENCE_MODEL.md:119` | 「核权重必须满足归一 `Σ_p c_p = 1` 与通量守恒，两者共同进入方差传播。」 | **(b)** 归一与守恒并列，无条件词；且 `Σ_p c_p = 1` 与 `DRIZZLE.md:95` 的 `c_jp = w_jp/N_p`（量纲 1/sr）**符号冲突**，见 Q2.5 |

逐条复跑：

```bash
grep -n "通量绝对守恒" docs/science/drizzle/DRIZZLE.md
grep -n "守恒成立的前提" docs/science/drizzle/DRIZZLE.md
grep -n "partition of unity" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "严格通量守恒" docs/detail/registry/acsd.phase1.drizzle.md
grep -n "总通量守恒" docs/science/unified/DATA_SEMANTICS.md
grep -n "通量守恒" docs/science/unified/UNIFIED_SCIENCE_MODEL.md
```

**"严格不变量" / "exactly conserved" / "恒成立"用于守恒的：not found。**

```bash
grep -rn "严格不变量\|exactly conserved" docs/     # exit 1, 无匹配
grep -rn "恒成立" docs/ | grep 守恒             # 无匹配
```

（`恒成立` 在 `docs/` 只出现在 `ASTROMETRY.md:13,132`、`PHASE3_PROJ_IMPL.md:406`、`NOISE_SNR.md:252,270`、`GLOSSARY.md:23` —— 与守恒无关。）

---

### Q1(i) 掩膜（masking）

**DRIZZLE.md 说**：不合格样本从分子/分母/方差三项一并剔除**并重新归一**。

> `docs/science/drizzle/DRIZZLE.md:225`
> 「**非有限样本**：源像元值非有限时按**样本级掩膜**处理——不合格样本从分子、分母、方差三项中一并剔除并重新归一；仅当零合格样本时输出 `NaN` 且 `support ≤ 0`。`NaN` 是无效的唯一表示，0 与 `±Inf` 一律读作有效数值。每个输出像元必须暴露被剔除样本的计数，计数为 0 与「字段缺失」必须可区分。」

> `docs/science/drizzle/DRIZZLE.md:226`
> 「**方差可用性是独立通道**：方差面非有限时按不合格样本剔除；方差值有限但不大于 0 表示「有覆盖但无方差信息」，此时信号与几何权重照常计入覆盖，不计入方差项。」

```bash
grep -n "样本级掩膜" docs/science/drizzle/DRIZZLE.md
sed -n '225,226p' docs/science/drizzle/DRIZZLE.md
```

**数学后果（文档未写明，但由 `:225` 直接蕴含）**：剔除后对**剩余样本**重新归一，则
`Σ_p F_p = Σ_{j 合格} x_j ≠ Σ_{全部 j} x_j`。
**即掩膜必然破坏 `DRIZZLE.md:13` 的总通量守恒。**

**没有任何文档把这条后果写出来。** 复跑（守恒行与掩膜行同文件但不互相引用）：

```bash
grep -n "守恒" docs/science/drizzle/DRIZZLE.md
grep -n "掩膜" docs/science/drizzle/DRIZZLE.md
```

两条命令的输出集合**无交集**。

**唯一把掩膜规则写成跨算子统一合同的地方**（同样不提守恒）：

> `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md:158-162`
> 「1. **样本级掩膜（只作用于不合格样本）**：不合格样本（值非有限）**从该输出像素的分子、分母、方差三项中一并剔除**（`F_p = Σ_合格 x_j w_jp`、`W_p = Σ_合格 w_jp`），并**重新归一**。**方差不可用样本不属此列**：它计入 `F_p` 与 `W_p`（保信号、保覆盖），只是不计入 `Var_p`。单个不合格样本**只**作用于该样本自身的项；分母**只**含合格样本的权重（保留被剔除样本的权重会引入系统性偏低）。」

```bash
sed -n '155,170p' docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
```

**🔴 与之直接冲突的下游文档**（这一条最尖锐）：

> `docs/detail/registry/acsd.phase1.drizzle.md:70-71`
> 「**invalid**：现行实现为值 NaN 经面亮度累加**传播、不掩膜**（DRIZZLE.md；drizzle_engine.cpp；回归 finalize 层，covered_area ≤ 0 → variance 记 NaN，`p1drz_tests_core.cpp`）；pixfrac ∈ (0,1] 引擎层严格拒绝；仅 NESTED。」

```bash
grep -n "传播、不掩膜" docs/detail/registry/acsd.phase1.drizzle.md
```

这与三处上位文本同时冲突：
- `docs/science/drizzle/DRIZZLE.md:225`（要求样本级掩膜 + 重归一）
- `docs/science/algorithms/DRIZZLE_GEOMETRY.md:385`（`DISP-DRZ-004`：**禁用**把非有限样本传播进 `F_p`/分母/方差）
- `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md:158`（跨算子统一规则）

即项目历史上的 "masked path breaks conservation" 陷阱，在 `acsd.phase1.drizzle.md:70-71` 里被记成了**相反**的方向（"传播、不掩膜"），且引用了 `DRIZZLE.md` 作为出处。

---

### Q1(ii) 部分像元覆盖（partial pixel coverage）

**not found。** DRIZZLE.md 与 DRIZZLE_GEOMETRY.md 均未讨论源像元位于 CCD 边缘、其足迹被探测器边界截断时的处理。

```bash
grep -n "边缘\|部分覆盖\|边界像元\|边缘像元\|截断\|探测区外\|视场外" docs/science/drizzle/DRIZZLE.md
# → 无匹配
grep -n "边缘\|部分覆盖\|边界像元\|边缘像元\|截断\|探测区外\|视场外" docs/science/algorithms/DRIZZLE_GEOMETRY.md
# → 仅 :318 "FIX-DRZ-F SIP 畸变边缘 patch（15° 宽场）"（测试 fixture，与边缘像元覆盖无关）
grep -rn "边缘像元\|边缘像素\|CCD 边界\|探测器边界\|外框像元\|不完整像元\|部分像元" docs/
# → 无匹配
```

相关但**不同**的两条（不是边缘像元覆盖）：

> `docs/science/drizzle/DRIZZLE.md:131`
> 「`support ∈ [0,1]`，0 表示无覆盖。」

> `docs/science/drizzle/DRIZZLE.md:57`
> 「`pixfrac → 0` 退化为交织（interlacing），`pixfrac = 1` 退化为移位叠加（shift-and-add）[1]。」

**结论**：NOISE_SNR 若要把"源像元不在有效域内 ⇒ 守恒不成立"写成一条判据，**当前无上游依据可引**。

---

### Q1(iii) 连续信号层 vs 稀疏控制点层

**DRIZZLE.md 明确区分，且守恒只对连续信号层成立**：

> `docs/science/drizzle/DRIZZLE.md:179`
> 「- **不做面积加权**：控制点的值原样带到球面对应位置，不进入 drop 交叠加权，也不参与任何通量守恒的求和——守恒是对连续信号面成立的性质，对点样本没有对应物；」

> `docs/science/drizzle/DRIZZLE.md:183`
> 「面亮度的稠密层与控制点的稀疏层是两个独立对象，共用同一套天球网格与同一套几何核，但语义与归一规则不同，**不得互相代入**。」

```bash
sed -n '173,183p' docs/science/drizzle/DRIZZLE.md
```

**几何合同同一条**（两层共用同一套 kernel）：

> `docs/ACSD_DESIGN.md:117`
> 「P1 的信号平面与 P2 的稀疏控制点都要从帧域搬到球面域，本创新点提供这一算子：对连续信号是通量绝对守恒的平面到球面 drizzle；对稀疏控制点是同一几何框架下的点分配，绝对信噪比值原样带到球面对应位置。」

`ACSD_DESIGN.md:117` 与 `DRIZZLE.md:5` 措辞一致，**均以"对连续信号 / 对稀疏控制点"作显式二分**。这一点上 NOISE_SNR 没有冲突。

---

### Q1(iv) HEALPix face / edge 奇异性

**DRIZZLE.md 的处理口径**（只保证"不改变面积口径"，不保证闭合精度）：

> `docs/science/drizzle/DRIZZLE.md:224`
> 「- **退化与边界**：drop 跨叶缝、跨赤道、跨面界时逐边裁剪的分支奇点由保守查询圆盘覆盖并单独计数，**不改变面积口径**。」

```bash
grep -n "跨面界\|跨叶缝\|跨赤道" docs/science/drizzle/DRIZZLE.md
```

**几何闭合的条件与误差（关键）**：

> `docs/science/drizzle/DRIZZLE.md:166`
> 「`Σ_p a_jp = A_drop,j = pixfrac² · A_pixel,j`」

> `docs/science/drizzle/DRIZZLE.md:169`
> 「这是构造性的，不依赖数值逼近：drop 多边形被逐边裁剪到目标叶边界内，叶边界曲线由自适应细分逼近，分割后的面积由球面立体角解析式累加。实现对每个源像元实测闭合相对偏差 `rel = (Σ_p a_jp − pixfrac²·A_pixel,j) / (pixfrac²·A_pixel,j)`，**超额与亏损分开具名判红**——只判超额会让面积亏损静默进入产品。」

```bash
grep -n "超额与亏损分开具名判红" docs/science/drizzle/DRIZZLE.md
```

**⚠️ 这里有一处同文档内的措辞冲突**：`DRIZZLE.md:169` 说闭合"**不依赖数值逼近**"，但同一行又说叶边界"由自适应细分**逼近**"且必须实测闭合偏差并判红。而 `DRIZZLE_GEOMETRY.md:337-345` 给出的极冠数值与"不依赖数值逼近"直接冲突：

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:337-345`
> 「- 极冠边（β_0 < 2/3 两侧的等纬度边与经线边）：**不适用**上述相对 hp_res 预算。叶边界在 nside≥256 由 4 角弦表示（生产路径），经过极点 apex 的叶其"弦/真"相对亏缺 ≥(1−2√2/π)（与 N 无关，N≳64 后已收敛），**比 1e-6·hp_res 的弦长预算大 4 个数量级**；…- **4 角弦四边形的亏缺闭式与系数（本节为唯一数值正本）**：相对亏缺极限 `2√2/π − 1 = −9.968368384e-02`（**解析闭式，不随 nside 收缩**）；绝对亏缺 `π/3·(1−2√2/π)·N⁻² = 0.1043885·N⁻²`。」

**接缝域的相对闭合不可达**：

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:374-376`
> 「**相对项在接缝不可达**——u+v=1 接缝的相对闭合 ∝1/A_drop，0.04″/px 实测 3.9e-3、0.005″/px 实测 9.9e-2，故接缝与任何 `A_drop ≲1e-9 sr` 的域一律以绝对项判，相对门对该域不适用。」

```bash
grep -n "相对项在接缝不可达\|2√2/π − 1\|大 4 个数量级" docs/science/algorithms/DRIZZLE_GEOMETRY.md
```

**"构造闭合由裁剪与鞋带的代数结构直接成立…不依赖数值逼近"** 出自顶层设计：

> `docs/ACSD_DESIGN.md:119`
> 「构造闭合由裁剪与鞋带的代数结构直接成立：每个 drop 的面积在其覆盖的 leaf 上恰好分完，不依赖数值逼近。」

> `docs/ACSD_DESIGN.md:118`
> 「以 12 个 HEALPix 基面的等面积 chart 为坐标域，每个面是单位正方形、Jacobian 恒为 π/3，leaf 在 chart 内轴对齐；源像素 drop 的球面足迹映射进 chart 后，交叠退化为二维轴对齐裁剪与**鞋带**公式，逐 leaf 面积以绝对立体角口径累加。」

```bash
grep -n "构造闭合由裁剪与鞋带" docs/ACSD_DESIGN.md
```

**🔴 `ACSD_DESIGN.md:118-119` 与实现/正本双重冲突**：
1. 实现不是"chart + 鞋带"，是**球面 Sutherland–Hodgman 逐边裁剪 + Van Oosterom & Strackee 立体角**，且明令「**禁用** "Girard 定理" 命名」「**禁用** PolyClip（平面 S-H/Shoelace）」（`DRIZZLE_GEOMETRY.md:145-149, 389`）。
2. "恰好分完，不依赖数值逼近" 与 `:337-345` 的 ~10% 极冠亏缺、`:374-376` 的接缝相对门不可达冲突。

`docs/ACSD_DESIGN.md:121` 同时声明「face 角点、极面折线、赤道带对角线与面缝是 chart 的分支奇点，被精确定位、检测与回退处理；**面积口径不受分支影响**」——这句话本身是对的（保守查询圆盘确实保证零漏选，见 `DRIZZLE_GEOMETRY.md:136`），但它说的是**候选枚举完备性**，不是**面积闭合精度**，与 `:119` 的"恰好分完"不是同一件事。

---

## Q2 — VARIANCE PROPAGATION FORMULA

### Q2.1 DRIZZLE.md 自己的公式

> `docs/science/drizzle/DRIZZLE.md:144`
> 「`variance_p        = Σ_j c_jp² · v_j  = Σ_j v_j w_jp² / N_p²`」

> `docs/science/drizzle/DRIZZLE.md:145-146`
> 「`ivar_p            = 1 / variance_p`
> `Cov(S_p, S_q)     = Σ_j c_jp c_jq v_j = Σ_j v_j w_jp w_jq / (N_p N_q)     （p ≠ q 时一般非零）`」

> `docs/science/drizzle/DRIZZLE.md:149`
> 「**信号与方差必须用同一个 `w_jp`**。若方差项少一次平方或漏掉 `N_p²`，量纲与标度都不成立。」

```bash
grep -n "方差\|variance\|N_p\|D_p\|pixfrac\|归一" "docs/science/drizzle/DRIZZLE.md"
```

**分母 = `N_p²`，不是 `D_p²`。** `N_p` 的定义：

> `docs/science/drizzle/DRIZZLE.md:83`
> 「`N_p = Σ_j w_jp · A_pixel,j`」

> `docs/science/drizzle/DRIZZLE.md:89`
> 「`N_p = Σ_j a_jp / pixfrac² = D_p / pixfrac² ,     D_p = Σ_j a_jp`」

> `docs/science/drizzle/DRIZZLE.md:30`
> 「\| 输出 \| `N_p` \| 面亮度归一分母 \| sr \|」

### Q2.2 关于 "归一分母" 到底是什么（关键区分）

`N_p` **不是**权重和（不是 `Σ_j w_jp = 1` 的那个和），而是**带面积量纲的 sr**：

> `docs/science/drizzle/DRIZZLE.md:35`
> 「面积量 `a_jp`、`A_drop,j`、`A_pixel,j`、`D_p`、`N_p` 一律是**立体角（sr）**。」

而 `Σ_p w_jp = 1` 是**核权重行归一**，两者是不同的东西：

> `docs/science/drizzle/DRIZZLE.md:75`
> 「`w_jp = a_jp / A_drop,j          （无量纲，Σ_p w_jp = 1）`」

`docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md:150-153` 把这一命名冲突显式登记了：

> 「**`W_p` 在 Phase 1 drizzle drop 分支不成立** —— 该分支的分母是**球面面积** `D_p`（量纲 `sr`，= 本文件与 `DATA_SEMANTICS` 的 `covered_area`），**不是**无量纲权重和。」

```bash
grep -n "W_p\` 在 Phase 1 drizzle drop 分支不成立" docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
```

### Q2.3 pixfrac 的幂次：DRIZZLE.md 自己怎么说

DRIZZLE.md **没有** `pixfrac⁴` 这个词，但有等价的两条：

> `docs/science/drizzle/DRIZZLE.md:116`
> 「- 若改取 `w_jp = a_jp / A_pixel,j`，则 `Σ_p w_jp = 1/pixfrac²`，总通量被压低 `pixfrac²` 倍，`pixfrac = 0.8` 时压到 `0.64`，等价于整帧偏暗 `−2.5·log10(0.64) = 0.485` mag；」

> `docs/science/drizzle/DRIZZLE.md:149`
> 「参数化换成等价的 `a_jp / A_pixel,j` 后 `pixfrac²` 在分子分母相消，逐位给出同一个 `variance_p`，因此该参数化变更不改变信噪比标度。」

以及发布因子：

> `docs/science/drizzle/DRIZZLE.md:276`
> 「- 累加量到产品的归一与写出：`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`，发布因子 `k = D_p/N_p`，**signal 乘 `k`、variance 乘 `k²`**。」

```bash
grep -n "signal 乘 \`k\`、variance 乘 \`k²\`" docs/science/drizzle/DRIZZLE.md
```

### Q2.4 DRIZZLE_GEOMETRY.md 自己的公式（两处，表述不同）

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:82-84`
> 「- 方差分子: `sumVarNum_p = Σ_j v_j · w_jp²`，**单位 = ADU²**（…）(double)v · (double)w² 中转再转 Scalar，仅当 v>0 累加，…）——"仅 v>0"即"方差不可用样本不进入方差项"」

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:106-110`
> 「- **S_p = F_p/N_p 归一不在本模块**: sumFlux/sumArea/sumVarNum 原始和逐 tile 传出…，归一在 aio_hips_writer finalize_tile（**variance = var_num_sum/area²**，`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`）」

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:404`
> 「`sumArea`（D_p=Σa_jp，support 语义）与 `sumVarNum`/`variance=sumVarNum/sumArea²` 语义不变。」

```bash
grep -n "sumVarNum\|var_num_sum\|sumArea²\|variance = var_num_sum" docs/science/algorithms/DRIZZLE_GEOMETRY.md
```

**判读**：`DRIZZLE_GEOMETRY.md` 只写 `variance = sumVarNum/sumArea²`，**不提 k²**。这是**文档缺口，不是数值错误**——见 Q2.6 代码核验：`sumVarNum` 传给 writer 之前已经乘过 k²。

### Q2.5 判据文档

> `docs/science/drizzle/DRIZZLE.md:211`
> 「| 方差恒等判据 | `variance_p = Σ_j c_jp² v_j`，由 raw `(a_jp, A_pixel,j, D_p)` 重算而非读取已计算量 | 红：漏平方、漏 `N²`、或改用核权重的平方做分母 |」

**符号冲突**：`docs/science/unified/UNIFIED_SCIENCE_MODEL.md:119` 写「核权重必须满足归一 `Σ_p c_p = 1`」。而 `DRIZZLE.md:95` 定义 `c_jp = w_jp / N_p`（量纲 `1/sr`）。两者不能同时成立：`Σ_p c_jp = 1/N_p ≠ 1`。

```bash
grep -n "c_jp = w_jp / N_p" docs/science/drizzle/DRIZZLE.md
grep -n "Σ_p c_p = 1" docs/science/unified/UNIFIED_SCIENCE_MODEL.md
```

### Q2.6 代码核验（只读）— NOISE_SNR §3.5 判定为正确

> `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h:38-48`
> 「//   k := D_p / N_p = sumArea / sumNorm
> //      = (Σ_j a_jp) / (Σ_j w_jp·A_pixel,j) = pixfrac²
> // …
> // 于是发布量 = sumFlux·k（writer 再除 covered_area ⇐ S_p = F_p/N_p），
> // 方差分子同步乘 k²（signal 与 variance 共用同一归一分母 ⇒ 幂次 k²，
> // 由 var_pub = (sumVarNum·k²)/D_p² = sumVarNum/N_p² 反解）。
> //
> // 幂次守恒是硬约束：signal 的 k 幂次 = +1、variance 的 k 幂次 = +2；
> // 任一末端漏乘 ⇒ signal 偏 1/pixfrac²、**variance 偏 1/pixfrac⁴**（pixfrac<1）。」

> `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:190-193`
> 「`dense_flux[local] = Scalar((double)acc.sumFlux * k);`
> … `dense_var[local] = Scalar((double)acc.sumVarNum * k * k);`」

```bash
grep -n "k := D_p / N_p\|variance 偏 1/pixfrac⁴" lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h
grep -n "acc.sumFlux \* k\|acc.sumVarNum \* k \* k" lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp
```

**结论（Q2 判定）**：
- `N_p²` 是权威分母 —— `DRIZZLE.md:144` 与 `NOISE_SNR.md:297` **完全一致**。
- `k = D_p/N_p = pixfrac²`，`variance` 乘 `k²` —— `NOISE_SNR.md:300` 的等价参数化 `Σ_j v_j w_jp² k²/D_p²` 与代码逐字一致。
- 「漏掉 `k²` 会把方差压低 `pixfrac⁴` 倍」——代码 `astro_sphere_sink.h:48` 逐字复述同一句。**NOISE_SNR §3.5 在此项上无可挑剔。**

### Q2.7 但下游有一份文档用了被明令禁用的核参数化

> `docs/science/unified/DATA_SEMANTICS.md:62`
> 「`variance = v_num_sum / D_p² ,      ivar = 1 / variance`」

> `docs/science/unified/DATA_SEMANTICS.md:65`
> 「其中 `v_num_sum = Σ_j v_j w_jp²` 是方差分子的累加，`D_p = Σ_j a_jp` 是覆盖面积（单位 sr）。**`w_jp = a_jp / A_pixel,j`** 是无量纲的重叠面积比：分子分母同为立体角。」

```bash
grep -n "v_num_sum\|w_jp = a_jp" docs/science/unified/DATA_SEMANTICS.md
```

**问题**：`a_jp/A_pixel,j` 正是被明令禁止作为核权重的口径：

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:60-63`
> 「**A_pixel 分母为实现细节（代数等价参数化，不构成第二套口径）**：`w′_jp = a_jp/A_pixel,j = pixfrac²·w_jp`，…该构造 `Σ_p w′_jp = pixfrac²`（pf=0.8 ⇒ 0.64），**不满足通量守恒门**，故核权重口径一律取 drop 面积归一；」

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:396-397`
> 「**面亮度保持权重的实现口径**：`drizzle_engine.cpp` 的 `processPixelSharedTiled` 内**核权重恒为** `weight = overlap_area / drop_area`（即 canonical `w_jp = a_jp/A_drop,j`，drop 面积归一…）」

```bash
grep -n "不满足通量守恒门\|核权重恒为" docs/science/algorithms/DRIZZLE_GEOMETRY.md
```

**数值后果**：两套参数化给出**同一个 variance 数值**（`DATA_SEMANTICS` 的 `D_p²` 分母恰好吸收了 `pixfrac⁴`），所以 `DATA_SEMANTICS` 的 variance 公式本身不产生数值错误。但：
1. `DATA_SEMANTICS.md:65` 把被禁口径写成 `w_jp` 的**定义**，而生产代码的 `w_jp` 是 `a_jp/A_drop,j` —— **符号事实错误**。
2. 若有人照 `DATA_SEMANTICS.md:65` 的 `w_jp` 定义去替换生产核，`signal` 会偏 `pixfrac²`（这正是 `DISP-DRZ-009` 的负例，`DRIZZLE_GEOMETRY.md:390`）。
3. `DATA_SEMANTICS.md:62` 的 `v_num_sum/D_p²` **未声明 k² 已乘**；同一公式也出现在 `docs/engineering/data/ARTIFACTS.md:22`（「面亮度方差 = `var_num_sum/covered_area²`」）。

```bash
grep -n "var_num_sum/covered_area²" docs/engineering/data/ARTIFACTS.md
```

**NOISE_SNR 相对这两份文档是更准确的那一份**（它把 k² 显式写出来了）。若 NOISE_SNR 需要补一句，应当是补「本式与 `DATA_SEMANTICS` §3.3 / `ARTIFACTS` DATA-HIPS-VAR-001 的 `v_num_sum/D_p²` 是同一数，差别只在该处的 `v_num_sum` 已被 k² 缩放过」。

### Q2.8 PHASE_PRODUCT_EXCHANGE 的方差分母自相矛盾

> `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md:164`
> 「1a. **方差项的乘积表达**：`Var_p = Σ_{方差可用} V_j w_jp² / W_p²`，其中 `W_p` **含方差不可用样本的 `w_jp`**（**分母不缩小**，避免方差系统性偏高）。」

同一文档 `:150-153` 已声明 `W_p` 在 drizzle drop 分支**不成立**、分母是 `D_p`。规则 1a 仍用 `W_p²`，且**全篇不提 `pixfrac⁴` / k²**。对 Phase-1 drizzle 分支，规则 1a 与 `DRIZZLE.md:144` 的 `N_p²` 不等价。

```bash
sed -n '150,153p' docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
sed -n '164,167p' docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
```

---

## Q3 — CORRELATION

### Q3.1 DRIZZLE.md 明确说相邻输出像元相关

> `docs/science/drizzle/DRIZZLE.md:153`
> 「**相邻目标像元的噪声是相关的**，这正是 Drizzle 文献强调的效应 [1]：一个输入像元的功率被分给多个输出像元，逐像元方差求和会漏掉全部交叉项。Drizzle 文献给出的噪声相关比 `R = σ_c/σ_p` 由 `pixfrac` 与 `scale` 的比值决定 [1]；在 ACSD 的口径下，相关系数的闭式就是上面的 `Cov(S_p, S_q)`…」

> `docs/science/drizzle/DRIZZLE.md:146`
> 「`Cov(S_p, S_q)     = Σ_j c_jp c_jq v_j = Σ_j v_j w_jp w_jq / (N_p N_q)     （p ≠ q 时一般非零）`」

> `docs/science/drizzle/DRIZZLE.md:159`
> 「而只用对角元给出的 `Σ_p a_p² variance_p` 是这个量的**严格下界**。粗化到父级网格时同理：父级方差的对角归约只能声明为下界，并必须同时给出可重建的算子摘要与亏损量 `deficit = (exact − diag) / exact`，声明为精确值被拒。」

> `docs/science/drizzle/DRIZZLE.md:213`
> 「| 协方差门 | 对角元等于 `variance_p`；孔径精确方差不小于对角归约，且存在非对角贡献时严格大于 | 红：声明对角归约为精确值 |」

> `docs/science/drizzle/DRIZZLE.md:16`
> 「本分册…**不产出完整协方差矩阵产品**…」

```bash
grep -n "相邻目标像元的噪声是相关的\|严格下界\|不产出完整协方差矩阵产品" docs/science/drizzle/DRIZZLE.md
```

### Q3.2 RESAMPLE.md

> `docs/science/resample/RESAMPLE.md:133`
> 「对角元即输出方差。…相关系数矩阵由 `C_y` 的对角归一得到，**输出像元之间的非对角相关性一般非零**。」

> `docs/science/resample/RESAMPLE.md:135`
> 「允许的协方差表示有四种：完整、**精确相关核**、带近似误差的相关核、仅对角。**由权重标量反推的表示恒被拒绝**；把相对权重当作逆方差也恒被拒绝。仅对角表示必须另有相关核或已声明的近似误差，否则拒绝。**近似相关核表示…受一个尚未取值的阈值门管辖**：在该阈值被批准之前，凡依赖近似相关核的输出面一律登记为不可用并给出原因。」

> `docs/science/resample/RESAMPLE.md:274`
> 「- **协方差近似**：以对角元近似完整协方差会**低估**块平均方差（漏掉全部交叉项）。采用该近似必须声明误差量并通过审计；否则输出面不可用。」

```bash
grep -n "非对角相关性一般非零\|精确相关核\|低估\*\*块平均方差" docs/science/resample/RESAMPLE.md
```

### Q3.3 相关核是否落盘？

**要求落盘的地方**（不是 drizzle 分册）：

> `docs/detail/registry/acsd.phase1.drizzle.md`（「只存对角 variance 时**必须**另存 correlation kernel / scale 或可重建算子摘要」）

> `docs/science/unified/UNIFIED_SCIENCE_MODEL.md:117`
> 「- 输出若只存对角方差，就**必须另存相关核或可重建的算子摘要**；否则 `R C_in Rᵀ` 的非对角部分被静默丢弃，后续任何依赖空间结构的估计都会偏乐观。」

> `docs/science/unified/DATA_SEMANTICS.md:248`
> 「| 相邻像素相关且未存相关核 | 依赖空间结构的推断偏乐观，须判为不可用 |」

```bash
grep -n "另存相关核" docs/science/unified/UNIFIED_SCIENCE_MODEL.md
grep -n "相邻像素相关且未存相关核" docs/science/unified/DATA_SEMANTICS.md
grep -n "correlation kernel" docs/detail/registry/acsd.phase1.drizzle.md
```

**现状是"未存"**：`DRIZZLE.md:159` 只要求「必须同时给出可重建的算子摘要与亏损量」，而产品面（`DATA-HIPS-VAR-001` / `DATA-HIPS-IVAR-001`，`docs/engineering/data/ARTIFACTS.md:22-23`）只登记 `variance` / `ivar` 两个子产品，**没有相关核或算子摘要的登记项**。这是一个**已登记但未闭合**的缺口。

```bash
sed -n '22,23p' docs/engineering/data/ARTIFACTS.md
grep -n "correlation\|相关核\|covariance\|协方差" docs/engineering/data/ARTIFACTS.md   # 仅控制点 k_corr，无 drizzle 相关核
```

### Q3.4 Fruchter & Hook 2002 / [9]

**有的，两处都有。**

> `docs/science/drizzle/DRIZZLE.md:249`
> 「[1] Fruchter A. S., Hook R. N. Drizzle: a method for the linear reconstruction of undersampled images. Publications of the Astronomical Society of the Pacific, 2002, 114: 144–152. https://doi.org/10.1086/338393 （预印本 arXiv:astro-ph/9808087v2）。」

> `docs/science/drizzle/DRIZZLE.md:251`
> 「…第 7.1 节「Drizzle frequently divides the power from a given input pixel between several output pixels. As a result, the noise in adjacent pixels will be correlated.」与第 7.2 节式 (6)(7) 的单输出像元方差、式 (8)–(10) 的噪声相关比（「方差与协方差传播」的相邻像元相关出处）。」

> `docs/science/drizzle/DRIZZLE.md:253`
> 「需要明确的边界：…该文献**不含协方差矩阵产品**，其噪声相关比 `R` 是一个标量统计量而非逐对像元的相关系数；ACSD 的 `Cov(S_p, S_q)` 是本仓在「方差与协方差传播」中于统一线性算子框架下的推导。」

> `docs/science/algorithms/DRIZZLE_GEOMETRY.md:457`
> 「- Drizzle 线性重建/drop/pixfrac：Fruchter & Hook 2002, PASP 114, 144（DOI 10.1086/338393；arXiv:astro-ph/9808087**v2**…）」

```bash
grep -n "Fruchter" docs/science/drizzle/DRIZZLE.md docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "the noise in adjacent pixels will be correlated" docs/science/drizzle/DRIZZLE.md
```

**Q3 判定**：NOISE_SNR:302「守恒映射自身的噪声与像素相关讨论见 [9]」—— [9] 就是 Fruchter & Hook 2002（`docs/science/noise_snr/NOISE_SNR.md:437`）。**引用正确**。

**但引用位置有小错**：DRIZZLE.md:153 的相关讨论**就在 DRIZZLE.md 自己这一册里**（`:146` 的 `Cov` 闭式、`:153` 的相关比、`:156-159` 的孔径精确二次型），不需要跳到文献。NOISE_SNR 把 [9]（外部文献）而不是 [19]（DRIZZLE.md）当作"守恒映射自身的相关讨论"出处，而 [19] 在 `NOISE_SNR.md:450` 已被定义为 DRIZZLE.md。**建议 NOISE_SNR 同时引 [19][9]**。

---

## Q4 — NaN / 哨兵语义

### Q4.1 NOISE_SNR §3.5 的两句话

> `docs/science/noise_snr/NOISE_SNR.md:323`
> 「**无覆盖与缺数**。无覆盖、无有效样本、输入方差缺项时输出取 NaN，不以 0 或无穷冒充；输入方差非有限属产品损坏，显式失败。零权重的哨兵值在阶段间的语义不同：阶段一产品面把零逆方差定义为不可用并强制成对方差为零，投影导出读入时把同一值读作零权重，输出取 NaN 传播态。」

```bash
grep -n "无覆盖与缺数" docs/science/noise_snr/NOISE_SNR.md
```

### Q4.2 下游 I/O 文档逐条比对

| 文档 | file:line | 原文要点 | 与 NOISE_SNR:323 |
|---|---|---|---|
| `docs/science/unified/DATA_SEMANTICS.md` | `:74` | 「\| 有覆盖但方差不可用 \| `D_p > 0` 且有限，方差分子为 0 \| 有限 \| `0` \| `0` \|」 | ✅ **一致** |
| 同上 | `:75` | 「\| 无覆盖 \| `D_p <= 0` 或非有限 \| `NaN` \| `NaN` \| `NaN` \|」 | ✅ **一致** |
| 同上 | `:78` | 「不可用的方差写 `0`，而 `ivar = 0` 本身就是显式不可用标记，与"夹紧的结果"可区分；NaN 只留给"无覆盖"这一与信号同态的情形。」 | ✅ **一致** |
| 同上 | `:80` | 「**读侧把 `ivar = 0` 视为合法零权重**，但必须先通过 `support > 0 ∧ finite(signal)` 的资格门。」 | ✅ **一致**（"投影导出读入时把同一值读作零权重"） |
| `docs/engineering/data/ARTIFACTS.md` | `:13` | DATA-IMG-VAR-001：「**无方差信息 = 0（显式不可用，a）**；NaN/负 = 损坏（写侧 rc=−6 硬失败）」 | ✅ 一致 |
| 同上 | `:14` | DATA-IMG-IVAR-001：「**ivar=0 = 显式不可用**（禁 `1/0→Inf`）；NaN/负 = 损坏」 | ✅ 一致 |
| 同上 | `:22` | DATA-HIPS-VAR-001：「**无覆盖（covered_area≤0）= NaN（与 signal 同态）**；有覆盖但无方差信息（covered_area>0 ∧ var_num_sum≤0）= **0**（显式不可用）；负 = 损坏。编码权威 = `DATA_SEMANTICS` a」 | ✅ 一致 |
| 同上 | `:23` | DATA-HIPS-IVAR-001：「**无覆盖 = NaN（同 signal）**；有覆盖但无方差信息 = **0**（禁 `1/0→Inf`）；负 = 损坏」 | ✅ 一致 |
| `docs/engineering/UNIFIED_OBJECTS.md` | `:33` | variance 行：「无覆盖 = `null`（对象级）/ `NaN`（Phase1 产品面，与 signal 同态）；**有覆盖但无方差信息 = `0`（显式不可用）**；负值 = 损坏」 | ✅ 一致 |
| 同上 | `:34` | ivar 行：「`0` = 显式不可用（禁 `1/0→Inf`）；`null` = 缺失；无覆盖 = `NaN`（同 signal）」 | ✅ 一致 |
| `docs/science/resample/RESAMPLE.md` | `:28` | 「`ivar = 0` 的像元使 `u = 1/ivar` 无效，按 `NaN` 传播态处理。」 | ✅ 一致 |
| 同上 | `:103` | 「零合格样本（或权重和为 0）时输出 `NaN`，覆盖标志**不变**…用 0 填充替代 `NaN` 是错误的。」 | ✅ 一致 |
| 同上 | `:149` | 「逐输出像元的传播状态有三态，正常传播、`NaN` 传播态（足迹内 `u` 为 `NaN` 或 `ivar = 0`）、覆盖不一致」 | ✅ 一致 |
| `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md` | `:166-169` | 「…表达为 **`variance=0 ∧ ivar=0`（显式不可用）**；该情形只写 0 —— NaN 保留给「无覆盖」…**NaN 是无效的唯一表示**；`0`、`±Inf` 或任意哨兵值一律不表示无效。」 | ✅ 一致 |
| `docs/detail/PRODUCT_STORAGE_FORM.md` | — | **not found** — 只在压缩/打洞语境出现 NaN（`:187, 190, 200, 203, 211`），**不定义 variance/ivar 哨兵语义** | — |
| `docs/science/IO_002_HIPS_INPUT_INTERFACE.md` | — | **not found** — 只定义 `ACS_HIPS_STATUS_COUNT` 哨兵（`:162`）与 `ACS_FIO_ERR_NANINF` 透传（`:220`），**不定义 variance/ivar 的零值语义**；`:276` 明确把「科学 dtype/unit/invalid 语义权威」让给 `DATA-HIPS-*` | — |

复跑：

```bash
grep -n "读侧把 \`ivar = 0\` 视为合法零权重" docs/science/unified/DATA_SEMANTICS.md
sed -n '71,80p' docs/science/unified/DATA_SEMANTICS.md
sed -n '13,14p;22,23p' docs/engineering/data/ARTIFACTS.md
sed -n '33,34p' docs/engineering/UNIFIED_OBJECTS.md
grep -n "ivar = 0\` 的像元使" docs/science/resample/RESAMPLE.md
grep -n "ivar\|variance" docs/science/IO_002_HIPS_INPUT_INTERFACE.md   # 无零值语义
grep -n "ivar\|variance" docs/detail/PRODUCT_STORAGE_FORM.md            # 无零值语义
```

### Q4.3 唯一的一处分歧

**`DATA_SEMANTICS.md:62/65` 的 variance 公式不带 k²**（见 Q2.7）——这是 Q4 之外的独立问题，但它落在同一段「三态编码」里，容易与哨兵语义混读。**哨兵三态本身无分歧。**

另有一处**表述**上的分歧（不影响数值）：

> `docs/science/resample/RESAMPLE.md:28`
> 「`ivar = 0` 的像元使 `u = 1/ivar` 无效，按 `NaN` 传播态处理。」

vs

> `docs/science/unified/DATA_SEMANTICS.md:80`
> 「读侧把 `ivar = 0` 视为**合法零权重**，但必须先通过 `support > 0 ∧ finite(signal)` 的资格门。」

两者在**资格门**上不一致：`DATA_SEMANTICS` 要求 `support > 0 ∧ finite(signal)` 才把 `ivar=0` 当合法零权重；`RESAMPLE.md:28` 直接把它归入 `NaN` 传播态，未提资格门（`RESAMPLE.md:262`「覆盖与值解耦 | 覆盖只由 tile 存在性决定；值非有限不改覆盖」也只说覆盖由 tile 存在性决定）。**保守侧一致（都不当权重用），但一个是"零权重"一个是"NaN 传播态"，产品面表现不同**（零权重 → 参与重归一；NaN 传播态 → 整像元 NaN）。这是一处**待裁决的口径分叉**。

---

## Q5 — 是否存在 NOISE_SNR 与之矛盾的过度守恒断言

**是，两处，都在 NOISE_SNR 的下游（detail/engineering 层）：**

### Q5.1 `acsd.phase1.drizzle.md:87` —— "严格通量守恒…全 pixfrac ∈ (0,1]"

```bash
grep -n "严格通量守恒" docs/detail/registry/acsd.phase1.drizzle.md
sed -n '84,95p' docs/detail/registry/acsd.phase1.drizzle.md
```

> 「- **核按 drop 面积归一（canonical）**：交叠面积除以该 drop 的面积。…这是唯一满足**严格通量守恒**（累加的通量总和等于源端积分通量总和，**全 `pixfrac ∈ (0,1]`**）的口径；」

比 `DRIZZLE.md:13`（无限定词）更强，因为：
1. 用了"**严格**通量守恒"；
2. 给了"**全 `pixfrac ∈ (0,1]`**"这样的全域断言；
3. **完全没有提到掩膜/NaN/边缘像元/几何闭合**——而 `DRIZZLE.md:163` 明写「守恒成立的前提是每个 drop 的面积在其覆盖的叶上被精确分完」，`DRIZZLE_GEOMETRY.md:374-376` 明写接缝域相对闭合门不可达（实测 9.9e-2）。

### Q5.2 `DATA_SEMANTICS.md:235-236` —— 守恒作为"必须"级判据

```bash
sed -n '233,241p' docs/science/unified/DATA_SEMANTICS.md
```

> 「- **常量面亮度守恒**：把常量面亮度的解析场送入映射算子，输出必须逐点等于该常量（归一判据）；
> - **总通量守恒**：非恒定场的输出总通量必须等于输入总通量（守恒判据）；两条互补，缺一不可；」

`UNIFIED_SCIENCE_MODEL.md:119` 同构（「核权重必须满足归一 … 与通量守恒」）。这两条与 `PHASE_PRODUCT_EXCHANGE.md:158-162` 的掩膜重归一规则并存：**掩膜一旦发生，"输出总通量必须等于输入总通量"就字面为假**。

### Q5.3 `acsd.phase1.drizzle.md:70-71` —— 反向过度断言

不是守恒的过度断言，而是**相反方向**的事实错误（见 Q1(i)）：它说 NaN **传播、不掩膜**，与 `DRIZZLE.md:225`、`DISP-DRZ-004`、`PHASE_PRODUCT_EXCHANGE.md:158` 冲突。

### Q5.4 `ACSD_DESIGN.md:118-119` —— chart/鞋带/"不依赖数值逼近"

不是措辞过度，而是**描述的几何算法与实现不符**，且"恰好分完、不依赖数值逼近"与 `DRIZZLE_GEOMETRY.md:337-345` 的 ~10% 极冠亏缺冲突（见 Q1(iv)）。

---

## Q6 复现命令索引（全部已在 `/workspace/Astro CS Database` 下实跑验证）

```bash
# Q1 守恒断言全集
grep -rn "通量守恒\|绝对守恒\|严格守恒" docs/
grep -n "守恒" docs/science/drizzle/DRIZZLE.md
grep -n "守恒" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "通量绝对守恒" docs/science/drizzle/DRIZZLE.md
grep -n "守恒成立的前提" docs/science/drizzle/DRIZZLE.md
grep -n "超额与亏损分开具名判红" docs/science/drizzle/DRIZZLE.md
grep -n "partition of unity" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "不满足通量守恒门" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "核权重恒为" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "严格通量守恒" docs/detail/registry/acsd.phase1.drizzle.md
grep -n "总通量守恒" docs/science/unified/DATA_SEMANTICS.md
grep -n "Σ_p c_p = 1" docs/science/unified/UNIFIED_SCIENCE_MODEL.md

# 否定项（确认 not found）
grep -rn "严格不变量\|exactly conserved" docs/            # exit 1
grep -rn "边缘像元\|边缘像素\|CCD 边界\|探测器边界\|不完整像元\|部分像元" docs/   # exit 1
grep -n "边缘\|部分覆盖\|截断\|探测区外\|视场外" docs/science/drizzle/DRIZZLE.md # exit 1
grep -n "校验项清单" docs/science/drizzle/DRIZZLE.md      # exit 1

# Q1 掩膜
grep -n "样本级掩膜" docs/science/drizzle/DRIZZLE.md
sed -n '225,226p' docs/science/drizzle/DRIZZLE.md
sed -n '155,170p' docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
grep -n "传播、不掩膜" docs/detail/registry/acsd.phase1.drizzle.md
sed -n '173,183p' docs/science/drizzle/DRIZZLE.md

# Q1(iv) HEALPix 面/极/接缝
grep -n "跨面界\|跨叶缝\|跨赤道" docs/science/drizzle/DRIZZLE.md
grep -n "相对项在接缝不可达\|2√2/π − 1\|大 4 个数量级" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "构造闭合由裁剪与鞋带" docs/ACSD_DESIGN.md
sed -n '115,122p' docs/ACSD_DESIGN.md

# Q2 方差公式
grep -n "方差\|variance\|N_p\|D_p\|pixfrac\|归一" "docs/science/drizzle/DRIZZLE.md"
grep -n "signal 乘 \`k\`、variance 乘 \`k²\`" docs/science/drizzle/DRIZZLE.md
grep -n "sumVarNum\|var_num_sum\|sumArea²\|variance = var_num_sum" docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "v_num_sum\|w_jp = a_jp" docs/science/unified/DATA_SEMANTICS.md
sed -n '62,65p' docs/science/unified/DATA_SEMANTICS.md
grep -n "var_num_sum/covered_area²" docs/engineering/data/ARTIFACTS.md
grep -n "\`W_p\` 在 Phase 1 drizzle drop 分支不成立" docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
sed -n '164,167p' docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md
grep -n "c_jp = w_jp / N_p" docs/science/drizzle/DRIZZLE.md
grep -n "k := D_p / N_p\|variance 偏 1/pixfrac⁴" lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h
grep -n "acc.sumFlux \* k\|acc.sumVarNum \* k \* k" lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp

# Q3 相关
grep -n "相邻目标像元的噪声是相关的\|严格下界\|不产出完整协方差矩阵产品" docs/science/drizzle/DRIZZLE.md
grep -n "非对角相关性一般非零\|精确相关核" docs/science/resample/RESAMPLE.md
grep -n "另存相关核" docs/science/unified/UNIFIED_SCIENCE_MODEL.md
grep -n "相邻像素相关且未存相关核" docs/science/unified/DATA_SEMANTICS.md
grep -n "Fruchter" docs/science/drizzle/DRIZZLE.md docs/science/algorithms/DRIZZLE_GEOMETRY.md
grep -n "the noise in adjacent pixels will be correlated" docs/science/drizzle/DRIZZLE.md

# Q4 哨兵
sed -n '71,80p' docs/science/unified/DATA_SEMANTICS.md
sed -n '13,14p;22,23p' docs/engineering/data/ARTIFACTS.md
sed -n '33,34p' docs/engineering/UNIFIED_OBJECTS.md
grep -n "ivar = 0\` 的像元使" docs/science/resample/RESAMPLE.md
grep -n "ivar\|variance" docs/science/IO_002_HIPS_INPUT_INTERFACE.md   # 无零值语义
grep -n "ivar\|variance" docs/detail/PRODUCT_STORAGE_FORM.md            # 无零值语义

# 悬空引用
grep -rn "FZ-COND-FLUX-CONSERV" docs/
```

---

## Q7 建议 NOISE_SNR.md 采取的动作（按优先级）

1. **§3.5 保持不动。** 方差公式、k² 等价参数化、`pixfrac⁴` 说法三项均与 `DRIZZLE.md:144`、代码 `astro_sphere_sink.h:38-48` 逐字一致，无需订正。
2. **§3.5 补一句交叉引用**：说明本式与 `DATA_SEMANTICS` §3.3（`:62`）和 `ARTIFACTS` DATA-HIPS-VAR-001（`:22`）的 `v_num_sum/D_p²` 是同一数，差别只在该处的 `v_num_sum` 已被 `k²` 缩放。否则读者按 `DATA_SEMANTICS:65` 的 `w_jp = a_jp/A_pixel,j` 理解会得到不同结论。
3. **§3.5 相关引用改为 [19][9]**：守恒映射自身的 `Cov` 闭式在 `DRIZZLE.md:146/153/156-159`（即 [19]），[9] 只是"文献强调了该效应"的出处。当前只引 [9] 会让读者以为本仓没有自己的推导。
4. **新增一条"守恒的适用域"边界**（当前 NOISE_SNR 通篇未提）：守恒只在「未掩膜、几何闭合、源像元完整落在有效域内」时成立；掩膜后 `Σ_p F_p = Σ_{合格} x_j`。可引 `DRIZZLE.md:163`（闭合前提）、`DRIZZLE.md:225`（掩膜重归一）、`DRIZZLE.md:179`（控制点层不参与）、`DRIZZLE_GEOMETRY.md:50-51`（partition of unity 是本仓推断）。
5. **§3.5 哨兵句可加一句 `support > 0 ∧ finite(signal)` 资格门**，以消解 `RESAMPLE.md:28` 与 `DATA_SEMANTICS.md:80` 的分叉（见 Q4.3）。
6. **不要**把 `DATA_SEMANTICS.md:65` 的 `w_jp` 定义当作上游依据引用——它是 detail 层对被禁参数化的误记（Q2.7），正确引用是 `DRIZZLE.md:75` + `DRIZZLE_GEOMETRY.md:396-397`。