# Geometry / Drizzle 修复报告（task-7）

> **性质**：实现者修复报告，不是独立复核。独立审查见 [audit-geometry.md](audit-geometry.md)（受检 `fa4649d57`）。
> **被修分支/基线**：`rebuild/acsd-v2`，修复起点 `688adeac7`；工作树中他人并行改动（`src/workflows`、`src/model` 未由本任务触碰）。
> **写域**：`src/geometry/*`、`src/coordinates/chart.cpp`；本文件是唯一写入的 `docs/` 文件。
> **未改**：任何测试真值/容差、`src/workflows/`、`src/model/`、`src/numerics/`、其它 docs。未 `git commit`/`push`。
> **复核**：独立复核 [review-geometry-fix.md](review-geometry-fix.md)（受检工作树 `376850340`）。复核后的修复见 **§10**；
> 复核已关闭的 P0-1/P0-2/P0-3(内容)/P0-5 与大部分 P1 不再重复，§1–§8 保留首轮修复的记录。

## 0. 结论摘要

| 审计项 | 状态 | 证据 |
|---|---|---|
| P0-5 极区 `chart_partials` 漏 `L` | **已修** | `chart.cpp:116-204`；探针 A1–A6，C1 复现 13.6× |
| P0-1 Drizzle 跨源 hull Ω/A | **已修** | `drizzle.cpp:169-334` 逐源；探针 C1–C11 |
| P0-2 `R=w/N` 与噪声 `R C Rᵀ` | **已修** | `drizzle.cpp:317-330,400-430,520-540`；探针 C7/C8/C15–C17 |
| P0-3 内容 hash | **已修（8 类对象 + primitives/table/footprint set/support map）**；`Identity::revision` 在 `src/model/units.cpp` 写域外，给出最小补丁 | `kernel.hpp:126-160`、`overlap.cpp:705/785/838/872/920/1026`、`drizzle.cpp:430/482/525/600`、`refine.cpp:172/212`；探针 C18–C20、D10/D11/D15/D16 |
| P0-4 工作流合并携带约束 | **未修（写域外）**：`src/workflows/normalize.cpp` 正由 Lead 在工作树修改；见 §6 | — |
| P0-6 `block>1` 等价性 | **未改代码**；报告给出充分条件 B1–B7 与判据，见 §7 | — |
| P1 face 缝/z=±2/3 拒绝 | **已修** | `kernel.cpp:138-238,689-712`、`containment.cpp:539-548`；探针 B2/B3、D12/D13 |
| P1 `chart_leaf` 越界 clamp | **已修**（哨兵 `UINT64_MAX`，调用者先验范围） | `chart.cpp:244-278`；探针 B1–B3 |
| P1 kernel/containment 冲突取交 | **已修** | `overlap.cpp:583-599`；探针 D6/D8 |
| P1 `Σa+coarse−A=0` 校验 + `a≤Ω_H` | **已修** | `overlap.cpp:628-648,964-983`、约束行 `overlap.cpp:740`；探针 D8/D9 |
| P1 取消保留当前块质量 | **已修** | `kernel.cpp:812` |
| P1 `tolerance_met` 绑定 `area_absolute_sr` | **已修**（并接 `per_leaf_absolute_area`） | `kernel.cpp:851-874`；探针 D12/D13 |
| P1 旋转矩形 footprint 不按 bbox | **已修**（拒绝 unsupported） | `overlap.cpp:121-158`；探针 D4 |
| **复核后修复**（seam 工作预算/apex、`A_L=0` 点值、estimate 状态、端点向外、精确裁剪性能） | **5 项已修**（第 6 项历史驱动只报告） | 见 §10；探针 C2/C3/F，A/B 见 §10.5 |

构建：`cmake --build build/science -j8 --target acsd` 通过（exit 0）；全量 `cmake --build build/science -j16` 通过。
独立探针：`validation/geometry_probe.cpp`（源码，独立程序；**未接入 CMake/ctest**，不是正式 gate，只作证伪证据）→ `PROBES PASS: 0 failure(s)`（75 项检查）。

---

## 1. P0-5：极区 chart 偏导漏 face 经度常数 `L`

### 1.1 数学依据

`SPHERICAL_DRIZZLE` 式 (3)：北冠 `z=1-r²/3, r=√(3(1-z)), φ=(π/4)(L_f+d/r)`，即
`d=r(4φ/π−L_f)`；南冠 `w=√(3(1+z)), r=4−w, d=(4φ/π−L_f)w`。而
`s=(J_f−r+d)/2, t=(J_f−r−d)/2`，所以

```
∂s/∂z = (−∂r/∂z + ∂d/∂z)/2,   ∂d/∂z = (4φ/π − L_f)·∂r/∂z
∂s/∂φ = (∂d/∂φ)/2 = 2r/π（北冠），2w/π（南冠）
```

原实现把 `∂d/∂z` 写成 `(4φ/π)·∂r/∂z`，在 `L_f∈{1,3,5,7}` 的极区 face 上**漏掉 `−L_f·∂r/∂z`**。
后果不是“界更宽”，而是**仿射模型与其残差界同错**：`kernel.cpp` 用同一对
`chart_partials`（点梯度 J0）与 `chart_partials_interval`（区间梯度）构造
`e_s,e_t`，漏项同时进入模型和“模型−真值”的差，式 (16) 的余项不再包含真误差。
在面中经线 `φ=Lπ/4` 上错公式给 `∂s/∂z=0`，真值 `3/(4r)≈0.79–0.97`。

### 1.2 修改

- `src/coordinates/chart.cpp:25` 新增 `polar_longitude_constant(phi)`：极区 face 分支即
  `[(L−1)π/4,(L+1)π/4]` 四个经度象限，故 `L=2·floor(2φ/π)+1`（mod 8）。`chart_partials`
  的公开声明在 `chart.hpp`（**写域外**，未改），因此由已归一到 face 分支的绝对经度恢复 `L`。
- `chart.cpp:127`（北冠）、`chart.cpp:142`（南冠）：`dd_dz = (4φ/π − L)·dr/dz`。
- `chart.cpp:159-204`：`chart_partials_interval` 使用入参 `face` 的 `chart_l(face)`，
  `dd_dz = (4φ/π − L)·dr/dz`（北冠用 `dr/dz`，南冠用 `dw/dz`）。

### 1.3 独立探针（A1–A6，C1）

探针用自写的闭式 chart 公式（不复用 `chart_partials`）核对：

```
A1 北冠 face0(L=1)、φ=π/4、z=0.70/0.75/0.80：实现值 = 3/(4r) = 0.790569/0.866025/0.968246
A2 漏 L 公式在同点给 0，与真值差 > 0.5（可区分）
A3 南冠 face8(L=1)、z=−0.75：实现值 = 3/(4w)
A4 区间版 ds/dz 在块 [0.78,0.80]×[π/4±0.02] 上包含全部采样点真值
A5 修复后声明的均值余项 ≥ 真实仿射模型误差（9×9 网格）
A6 漏 L 模型违反自身残差界
```

C1 复现（块 `z∈[0.78,0.80]`、`φ∈[π/4±0.02]`，16 像素量级）：

```
declared residual 7.235e-04, true error of the missing-L model 9.808e-03 (13.6x larger)
```

与审计 §3 C1 的 12.4× 同量级（差异来自块的 φ 半宽与采样网格，非公式差异）。
另：`validation/geo/geo_validation.cpp` 的独立 chart 反例（若 Lead 复跑）会同样命中。

### 1.4 附带修正（同属证书形式）

- `kernel.cpp:145-150`：`h_k = max(c_k−U_k^L, U_k^U−c_k)` 直接求值，替换
  `0.5*(u1−u0)`（`SPHERICAL_DRIZZLE:234` 点名的 1 ulp 漏包围）。
- `kernel.cpp:339-365`：`chart_half_space_le/ge` 的常数 `offset = bound − value + gu·cu + gv·cv`
  改为区间求值；inner 取收紧端、outer 取放宽端，半空间常数自身的 binary64 舍入不再漏在证书外。

---

## 2. P0-1 / P0-2：Drizzle 的 F/N/R/B 与同一 R

### 2.1 数学依据

`SPHERICAL_DRIZZLE` 式 (1)(2)：`Ω_j=Ω(W(P_j))`、`A_j=Ω(D_j)`、`a_jp=Ω(D_j∩H_p)`、
`w_jp=a_jp/A_j`、`F_p=Σ_j w_jp x_j`、`N_p=Σ_j w_jp Ω_j`、`B_p=F_p/N_p`、`R_pj=w_jp/N_p`。
强制不变量（审计 §1.1 I1–I4/I12）：

- I1 逐源：`A_j`、`Ω_j` 是源的下标对象，不能跨源做 hull；`N_p≠(Σw)·hull(Ω)`。
- I2 同一 R：signal/PSF/noise（`R C Rᵀ`）/response 用同一个 `R=w/N`。
- I3 点值：发布的 B 不能是保守区间中点，必须由同一组代表点算出的比值。
- I4 常量场：`x_j=b0Ω_j ⇒ B_p=b0`（点值精确，且与源顺序无关）。
- I12：`x` 可负，`w≥0`；`N_L>0` 才归一；`N` 跨 0 → `support_unresolved`。

### 2.2 修改（`src/geometry/drizzle.cpp`）

- `drizzle.cpp:169-247`：每个 (leaf, source) 独立算 `w∈[a_L/A_U, min(1,a_U/A_L)]`；
  `A_j` 用 `drop_area`，`Ω_j` 用 `native_area`，`a_jp` 用 `overlap_area`；删除跨源
  `hull(omega/drop/area)`。`A_L=0` 时（measure 只证 A>0）退化为 `w∈[0,1]` 的合法包围并具名记录。
- `drizzle.cpp:250-302`：`F=Σ w x`、`N=Σ w Ω` 逐源累加；代表点
  `w0=mid(w)`、`Ω0=mid(Ω)`、`F0=Σw0x`、`N0=Σw0Ω0`，发布值 `B0=F0/N0`；
  区间 `[B_L,B_U]=[F]/[N]` 仍作证书。常量场下 `F0=b0N0` 精确，B0=b0。
- `drizzle.cpp:308-330`：`R_pj=w0/N0` 写入 CSR（**不再是 w**）；同一叶同一 source 先并再写一列
  （`std::map<U64,double>`，按 source 升序，固定归约）。
- `drizzle.cpp:218-224`：source row 越界 → `invalid_model/source_row_out_of_range`，**不再静默填 0**。
- `drizzle.cpp:364-376,490-493`：signal 数组/estimator 单位改为 `observation->unit / sr`
  （`unit_multiply(observation->unit, unit_per_sr())`），不再是 `sr/pixel`。
- `drizzle.cpp:438-447`：无证书区间的叶（NaN）不再写 `achieved` 条目（未知误差不填零），
  并以 `included` 具名；`estimate->assessment.complete=false`。
- `drizzle.cpp:516-540`：`SpatialResponse`/`SignalEstimate` 的 identity 由内容 hash 生成（§4）。
- 噪声：`drizzle.cpp:505-514` 仍用 `control_covariance(native_cov, rows)`，此时 `rows=R`，
  即 `R C Rᵀ`（原为 `w C wᵀ`，差 `N_p²`）。
- 区间四则（`drizzle.cpp:26-70`）改走 `numerics::interval_add/mul/div`（有向舍入），
  替换本地 plain-double 的 min/max 四乘积；发布的 `[B_L,B_U]` 因此是向外的，
  不再是“近似区间”。点值 `B0` 不受影响。

### 2.3 独立探针（C1–C20）

审计 §3 C2 的最小例：一个叶、两个源 `A_1=Ω_1=1`、`A_2=Ω_2=4`、`a=A`，
常量面亮度 `b0=1 ⇒ x=(1,4)`，真值 `N=5,F=5,B=1`。

```
C3/C5   B=1，区间 [1,1]
C4/C6   反转贡献顺序后 B=1，区间 [1,1]
C7/C8   op->rows 作用于 x 得 1（同一 R 产生 signal）
C9/C10  normalizer_sr = 5
C11     点值与贡献顺序无关（差 < 1e-15）
C12     负信号 x=(−1,4) ⇒ B=3/5（保留符号）
C13/C14 零支持叶保留 NaN，不填 0；其余叶仍发布
C15–C17 C=diag(1,4)、R=[1/5,1/5] ⇒ Var(B)=R C Rᵀ=0.2（旧 w C wᵀ=5）
C18–C20 同一 overlap → 同一 operator/response identity；改变 a → identity 改变
```

旧实现在 C3/C4 会给 1.5625/2.2656（且随顺序变），在 C17 会给 5；这些探针均能区分。

---

## 3. P0-3：内容 hash

### 3.1 实现

`src/geometry/kernel.hpp:126-160` 新增 `geometry::ContentDigest`（`model::Sha256` 命名域流，
字段名+值长度前缀，`f64` 用位模式），`kernel.cpp:465-520` 实现；另加
`add_csr`（直接哈希 offsets/indices/values，因为 `operators::make_csr_action` 不给
`LinearAction` 设置 identity，见 §6.3）。

改为内容 hash 的对象（原均为常量字符串）：

| 对象 | 位置 | 内容 |
|---|---|---|
| `GeometryPrimitiveTable` | `overlap.cpp:705` | source index + 每行 (source, order, ipix, kind, lower, upper) |
| `GeometryConstraints` | `overlap.cpp:785` | primitives identity + CSR 载荷 + lower/upper 数组 identity |
| `OverlapTable` | `overlap.cpp:838` | 6 个数组 identity |
| `FootprintSet` | `overlap.cpp:872` | source index、WCS、矩阵/offset/local cell、frame |
| `GeometryRecipe` | `overlap.cpp:920` | footprints、source rows、order、pixfrac、algorithm/version、预算与 arithmetic、per-leaf 限额 |
| `Overlaps` | `overlap.cpp:1026` | 每个 contribution（source/leaf/a/A/Ω/constraints）+ coarse 域 + primitives/constraints/recipe/table identity + status |
| `SupportMap` | `drizzle.cpp:430` | leaf index、geometry、primitives、states/areas 数组 |
| `SpatialResponse` | `drizzle.cpp:482` | R 的 CSR 载荷 + weights/factors + signed |
| `SignalEstimate` | `drizzle.cpp:525` | signal/lower/upper 数组、单位、support、response、noise、condition |
| `DrizzleOperator` | `drizzle.cpp:600` | R 的 CSR 载荷、source index、bounds、normalizer、support、status |
| `GeometrySnapshot` | `refine.cpp:172` | overlaps/operators/recipes/dependents/source_inputs/functionals |
| `GeometryRevision` | `refine.cpp:212` | base/next identity、replacements、affected 列表、status |

另外 `contribution.shared_geometry_constraints` 从常量 `"geometry:constraints"`
改为真实 `constraints->identity.object_id`（`overlap.cpp:791`），使 R 闭包携带同一 primitive/constraint 身份。

### 3.2 探针

```
D10 同一输入 → Overlaps identity 相同（确定性）
D11 改 order 8→7 → identity 改变
D15 改 geometry budget → refine 的 Replacement.old_hash ≠ new_hash
D16 GeometryRevision identity 不再是常量字符串
C18 同一 overlap → operator identity 相同；C19/C20 改 a → operator/response identity 改变
```

### 3.3 未修：`Identity::revision` 恒为 1（写域外）

`src/model/units.cpp:252-263` 的 `make_identity` 写死 `id.revision = 1`。
几何内容变化后 `object_id` 已变（内容寻址），但 `revision` 不递增，`refine_geometry`
的 `GeometryRevision` 仍不能靠 revision 区分新旧。**最小补丁建议**（Lead 执行，1 处）：

```cpp
// src/model/units.cpp, make_identity
Identity make_identity(const std::string& schema_id, U32 major, U32 minor,
                       const std::string& content_hash_hex, const std::string& run_id,
                       U64 revision = 1) {   // 新增可选参数，默认 1 保持现有调用
  ...
  id.revision = revision;
  ...
}
```

并在 `refine.cpp` 生成 `GeometryRevision` 时传入 `base->revision + 1`（或由调用方在
`GeometryRevision` 上显式设置 `revision->identity.revision = base->identity.revision + 1`）。
本任务未越界修改。

---

## 4. P1 项

### 4.1 face 缝 / z=±2/3 越界拒绝（`kernel.cpp:138-238, 689-712`、`containment.cpp:539-548`）

- `build_block_model` 现在要求块整体落在**单一 chart 分支**：`pure_band / pure_north /
  pure_south` 三者之一（原先三个变量算了不用，跨 z=±2/3 的块会走 band 公式算冠点）。
- 要求 `s,t∈[0,1]`：赤道 face 的方块条件是 `|d| ≤ min(3−r, r−1)`，比经度分支检查
  `|d|≤1` 严格；越界即 face 缝穿越，返回 false → 调用者细分，depth 用尽则
  `keep_unresolved`（保留质量），不再把 s>1 的窗口 clamp 到最后一列。
- 候选枚举只对闭边界 `floor(1·N)=N` 做 `N−1` 修正；另加 `candidate_range_ok` 防御
  （`kernel.cpp:689-712`）。
- `containment.cpp:539-548`：containment 的候选窗口越出 `[0,N−1]` 时返回 incomplete
  （kernel 界仍权威），不再 clamp 后整格重复计数。

**探针**：B2/B3（`chart_leaf` 越界给哨兵）、D12（跨 z=2/3 的大 footprint 产生 unresolved，
且 `tolerance_met` 被预算拒绝）。

### 4.2 `chart_leaf` 不 clamp（`chart.cpp:244-278`）

`chart.hpp` 声明冻结（写域外），故 `chart_leaf` 对越界 `(ix,iy)` 返回保留哨兵
`ipix=UINT64_MAX`（order≤29 时合法 ipix < 12·2^58 < 2^64−1）；几何调用者先验范围并把
哨兵视为“该块不属于此 face”（细分/未决）。`chart_leaf_for_direction` 只对闭方块的
边界点 `s=1` 做 `N−1` 修正（合法边界，非 clamp 错叶）。
**探针**：B1 合法 cell；B2/B3 越界返回哨兵。

### 4.3 kernel/containment 冲突质量（`overlap.cpp:583-599, 964-983`）

两个独立算法对同一叶都给出包围时，发布 `[max(L1,L2), min(U1,U2)]`（独立有效界取交，
`SPHERICAL_DRIZZLE:259`）；空交集记 `contradictory_leaves`，`bound_validity=invalid`
并具名 issue，kernel 界（主路径）仍发布但不再 certified。
coarse 行改为 `kernel.unresolved_upper` 本身，**删除** `kernel_upper − containment_added_upper`
——containment 的已决格与 kernel 的未决域是不同划分，其差不是可证明的界（审计 §2.4.5）。

### 4.4 `Σa+coarse−A=0` 校验与 `a≤Ω_H`（`overlap.cpp:628-648, 740, 964-983`）

- 发布前把每叶 `a_U` 收紧到 `min(a_U, Ω_H)`，`Ω_H=π/(3N²)` 用 `pi_enclosure()` 的上界
  ceil 到 dyadic 网格。这是**合法收紧**（`a_jp=Ω(D_j∩H_p) ≤ Ω(H_p)`），不是掩盖错误。
- 约束矩阵新增逐叶 `a ≤ Ω_H` 行（`overlap.cpp:740`），行数 = `3·leaf + 2`。
- 用精确 dyadic 校验：`Σa_L ≤ A_U`、`A_L ≤ Σa_U + coarse_U`、`a_L ≤ a_U`；
  违反 → `area_conservation_violated`、`bound_validity=invalid`（不 clamp 成“自洽”值）。
**探针**：D8（每叶 a_U ≤ Ω_H）、D9（约束行数）。

### 4.5 取消保留当前块质量（`kernel.cpp:812`）

候选格循环中取消时，除队列外**当前块**整体 `keep_unresolved`。当前块已处理格的面积已在
ledger，未处理部分用整块质量作上界（上界过计是安全的，质量不丢失；I8）。
已处理格的重复计入使 unresolved 上界偏保守，报告在此明示。

### 4.6 `tolerance_met` 绑定 `area_absolute_sr`（`kernel.cpp:851-874`）

停止判据从“队列无未决块”改为 `ΣU_p ≤ budget.area_absolute_sr`（`SPHERICAL_DRIZZLE:97,101`），
并接 `per_leaf_absolute_area`（逐叶 `a_U−a_L` 限额）。不达 → `termination=depth` +
`reason="area_absolute_sr_not_met"` / `"per_leaf_absolute_area_not_met"`，不再 certified。
`normalizer_min_sr` 仍未接（§8）。
**探针**：D12（area=0 时同一次运行 termination=depth=2）、D13（area=1e9 时 tolerance_met=0）。

### 4.7 旋转矩形 footprint（`overlap.cpp:121-158`）

`rings_are_axis_aligned_rectangle` 现在要求：单环 4 顶点、每条边恰为水平或垂直、
非退化、`x/y` 跨度为正。旋转/剪切四边形 → `unsupported`（`compute_overlap:source_footprint`），
**不再把外接 bbox 当 drop**（否则 `Σa≈2A` 且 `w≤1` 截断掩盖）。
**探针**：D3/D4（45° 旋转 → `unsupported`）。

### 4.8 containment 单元面积上界（`containment.cpp:44, 368, 533`）

`cell_area_upper` 由 floor 除法改为 ceil（审计 §5 的 P2“少 <1 格”）。

---

## 5. 复核审计报告时发现的偏差（不盲信报告）

1. **审计 §0.5 / §2.7 关于 `normalize.cpp:1023-1040` 的描述**在 `fa4649d57` 与 HEAD 上
   均成立（当时确实只合并 `contributions`），但**工作树**已有 Lead 的并行修改
   （`git diff` 显示 `src/workflows/normalize.cpp` +74/−2，合并 status/recipe/coarse 并把
   primitives 未合并具名为 conditional）。该文件不在本任务写域，未触碰；P0-4 以 Lead 的
   工作树为准。
2. **审计 §2.2 “`pure_*` 变量从未使用”**属实；但审计未指出 `footprint_area` 也复用
   `build_block_model`。若直接在 `build_block_model` 拒绝跨界块，会连带让“跨 chart 分支的
   大 footprint 的面积 A_j”变成 unsupported（本报告 §5 实测：探针 D12 首轮即
   `code=unsupported`）。修复时因此拆出 `build_density_model`（`kernel.cpp:115-136`），
   `footprint_area` 以 `require_chart=false` 只算 g 模型；chart 相关检查仍只作用于 cut-cell。
   这是审计未列、但实现必须处理的耦合。
3. **审计 §2.6 “CSR 行值 = nominal_w（不是 w/N）”** 属实，已改为 `R`。注意
   `validation/geo/geo_validation.cpp` 的 `two_source_row_sums_normalised`（非 CMake 目标）
   检查 `op->rows` 的**行和≈1**，这是旧的 `w` 语义；改成 `R` 后行和是
   `Σ_p w_jp/N_p ≠ 1`，该历史检查必须按 `R` 重新推导，不能直接复跑当 PASS。
4. **审计 §2.4.3 支持面积**：原实现 `area=hull(a)` 的 upper 是 `max a_U`。本次改为
   `S_L=max a_L`、`S_U=min(Ω_H, Σa_U)` 的**粗界**（`SPHERICAL_DRIZZLE:156` 明示这是粗界，
   不是真 union）。真正的独立 Boolean OR 未实现（§8），因此 `SupportMap` 仍是粗包含，
   不能称“覆盖率”。
5. **`chart_leaf` 的写域限制**：审计要求“删除 clamp”，但 `chart.hpp` 声明在写域外，
   故用哨兵 + 调用者校验实现“不 clamp”。最小补丁（Lead 可做）：把声明改为
   `bool chart_leaf(U32 order, U32 face, U64 ix, U64 iy, HealpixLeaf& out)`。

---

## 6. 写域外的未修项与最小补丁建议

### 6.1 P0-4：工作流合并（`src/workflows/normalize.cpp`，Lead 负责）

工作树已修：合并 `status/termination/bound_validity/support/coarse/recipe`，primitives
未拼接时 `Code::unsupported` + `conditional`。本报告不重复实现，只提示两点残余：
- 合并后的 `Overlaps` identity 仍是常量 `sha256("acsd.normalize.overlaps")`（`normalize.cpp`
  当前工作树），应改为对 contributions+coarse+recipe+status 的内容 hash；
- `merged->primitives/constraints` 为空（只具名 conditional），Drizzle 的
  `shared_geometry_constraints` 因此无法跨块统一；`build_drizzle` 现在对
  `recipe==nullptr` 具名 `identity_conflict` 并把 `assessment.complete=false`
  （不再静默跳过身份检查）。

### 6.2 P0-6：`block>1` 等价性充分条件（不改 workflows）

当前数据模型（`src/observation/calibration_kernel.cpp:480` 每像素一条 `integrated_values`；
`normalize.cpp:136-139` 把 `source.sample` 设成 block 号；`drizzle.cpp` 用 `x[source_row]`）
下，`block>1` **不是更快地算同一件事**：块 0 取像素 0 的值当 256 像素的通量，
`B_code/B_true≈1/256`。要证明 `block>1` 与 `block=1` **逐位等价**，至少需同时满足：

- **B1 源行不变**：每个子矩形的 `source.sample` 仍是**像素号**，`x` 仍按像素取值；
  不得把 block 当源行（这是当前 P0-6 的根因）。
- **B2 逐子矩形建模**：`chart_enclosure`/`build_block_model`/残差/内外裁剪逐子矩形做，
  不能用 batch 的外接矩形（否则 `e_s` 随 batch 尺寸增长）。
- **B3 逐 (source,leaf) 记账**：ledger 键仍是 `(source_id, leaf)`，候选枚举逐子矩形；
  `Σ_batch a` 可加（真叶分割球面）。
- **B4 共同约束只算一次**：`Ω_j,A_j` 与 `a` 用同一 ledger/同一 revision；不可变的
  `WcsModel`/chart 常数可跨子矩形共享。
- **B5 固定归约**：输出按 `(leaf, source)` 排序后归并，累加次序固定。
- **B6 取消/预算按子矩形边界**：已算子矩形保留，未算进 unresolved（本任务已在 kernel
  内实现单块边界语义，batch 需沿用）。
- **B7 Drizzle 侧**：`by_leaf` 的源集合、`N` 的求和键与逐像素调用一致；containment
  fallback 仍以子矩形为 drop。

缺任一条即不等价（尤其 B1/B2/B3/B7）。**在 B1 未满足前，`block>1` 必须 fail-fast**；
本任务未改 workflows，故未实现该 fail-fast（`build_drizzle` 无法从身份上区分 block 行与
像素行，见 §5 探针外的说明）。

### 6.3 `LinearAction` identity 未设置（`src/operators/linear_action.cpp:470-550`，写域外）

`make_csr_action` 不给 `LinearAction` 的 `Model::identity` 赋值（默认空串），因此
`op->rows->identity.object_id == ""`。本任务在几何侧改为直接哈希 CSR 载荷
（`ContentDigest::add_csr`），但其它模块（sky/coadd/export）若用 action identity 做
依赖闭包仍会拿到空串。最小补丁：`make_csr_action` 结束时用
`ContentDigest`（或 `model::hash_payload`）对 offsets/indices/values + domain/codomain
生成 `identity`。

---

## 7. 构建与探针命令

```bash
# 构建（exit 0）
cmake --build build/science -j16

# 独立探针（不在 CMake，不新建正式 gate；源码在 validation/geometry_probe.cpp）
c++ -std=c++20 -O2 -I include -I src validation/geometry_probe.cpp -o /tmp/geometry_probe \
  -Wl,--start-group \
  build/science/libgeometry.a build/science/libcoordinates.a build/science/liboperators.a \
  build/science/libuncertainty.a build/science/libnumerics.a build/science/libmodel.a \
  build/science/libinformation.a build/science/libstorage.a build/science/libresources.a \
  build/science/libruntime.a \
  -Wl,--end-group -lzstd -lpthread
/tmp/geometry_probe
# 输出：PROBES PASS: 0 failure(s)（75 项 ok，0 FAIL；exit 0）
```

探针分组：A（P0-5 chart 偏导/残差，含 C1 13.6× 复现）、B（chart_leaf 越界）、
C（Drizzle 常量场/顺序无关/负信号/零支持/`R C Rᵀ`/identity 敏感性）、
D（旋转矩形、Ω_H 约束、identity 确定性、area 预算停止、refine replacement）、
E（X01 实际 WCS：3 个像素 order 6 均 `tolerance_met`+`certified`，确认严格化未回归生产配置）、
C2（复核 R3：`A_L=0` 点值 = 2）、C3（复核 R3d：estimate 状态跟随 overlaps）、
F（复核 §3.1：φ 缝与 z=2/3 像素的工作有界、contributions 小、无 apex 全格填充）。

`ctest`：19/20 通过。唯一与几何无关的失败是工作树中他人并行修改的
`process_photometry.fit_x01_minimal_repro`（`src/photometry/joint_fit.cpp` 已由他人改动）
与 `process_calibration-faults_calibration_faults`（`src/runtime`/`src/observation`
他人改动，失败信息为 spool/cancel writer 计数）。几何相关
`process_geometry.overlap_m5_contracts`、`typed_smoke`、`numerics/operators/covariance_selftest`
全部通过。

---

## 8. 仍未完成 / 未决条件

1. **`Identity::revision`**：`src/model/units.cpp:258` 恒为 1（写域外，§3.3 最小补丁）。
2. **`normalizer_min_sr`** 仍未接：Drizzle 的 `weak_support` 分类与 `support_unresolved`
   阈值没有使用该字段；`SupportMap` 的 union 仍是粗界（§5.4），不是独立 Boolean OR。
3. **`max_bytes`** 在 `src/geometry` 仍零引用；`too_many_contributions=4096` 是硬失败，
   没有外存/多遍路径（审计 §2.4.7）。
4. **`allow_target_fallback`** 仍未接：`target_containment` 由 `overlap.cpp` 事后无条件调用，
   kernel 不看该字段（审计 §2.2）。
5. **P0-6 `block>1`**：未改 workflows；B1 未满足，当前只能靠默认 `block=1`。建议 Lead
   在 `normalize` 对 `block_size>1` fail-fast，直到 B1–B7 实现。
6. **`geo_validation.cpp` 的 `two_source_row_sums_normalised`** 需按 `R` 重写（§5.3）。
7. **apex fallback 的候选填充**：**复核后已修**——只在 `kernel.apex_singular` 时调用
   `target_containment_apex`，且合并后的 contributions 也做 4096 检查（见 §10.1）；
   极区真实 apex 帧仍可能因 4·N² 全格填充触发 `resource_exhausted`（诚实失败）。
8. **半空间/`e_g` 的 P2**：`solid_angle_density_gradient` 的 `ddet_du/ddet_dv` 仍各漏一项
   （审计 §2.1 末尾），因 g 只用“代表值 + 交叉区间界”，只让 `e_g` 变大、不破坏包含性；
   本次未改 `src/coordinates/tan_sip.cpp`（写域外），文档须保持“这不是 §6 的导数余项”的降级陈述。

---

## 9. 结论边界

- 本报告只声称：上述代码路径的**数学命题已按定义修正**，且独立探针能区分修复前/后的实现；
  不声称 X01 或任何生产产品已通过科学验收（X01 证据链仍以 Lead 的统一实跑为准）。
- 未决质量、负信号、零支持、未决区间均保留；未改任何测试真值/容差；未把未决质量清零。
- 未 `git commit`/`push`；所有改动留在工作树，由 Lead 统一提交。

---

## 10. 复核后修复（review-geometry-fix.md 的 6 项）

> 复核报告：[review-geometry-fix.md](review-geometry-fix.md)（独立复核，受检工作树 `376850340`）。
> 本节只记录复核发现后的修复与前后数值；复核已关闭的 P0-1/P0-2/P0-3(内容)/P0-5 与大部分
> P1 不再重复。所有改动仍在原写域内（`src/geometry/*`、`src/coordinates/chart.cpp`）。

### 10.1 新 P1：seam / z=±2/3 的“拒绝”不再吃光工作预算（含 apex fallback）

- **数学**：跨 chart 缝的块在任意深度都跨缝（细分只把未决前沿缩到 `2^-k`，不改变“跨缝”
  这一事实），因此对它的无限细分没有收敛意义；正确做法是把它的质量作为**有界 coarse**
  保留，而不是用工作预算买精度。`SPHERICAL_DRIZZLE:45` 的正解是“跨 r=1/3 分别求值取
  并包”；本实现选择“未决 + 有界”，因为多 face 并包需要在同一 detector 子矩形上按 face
  分割质量，而当前 chart 逆映射不提供该分割（属后续工作，见 §8）。
- **修改**：
  - `kernel.cpp` `kMaxSeamRefineDepth = 6`；`build_block_model` 增加
    `chart_domain_failure` 分类（φ 分支/分支跨界/`s,t` 出方格 → chart domain；
    `r≤0`/极点 → apex flag）；`cut_cell_overlap` 对 chart-domain 失败最多再细分 6 层，
    之后 `keep_unresolved`（质量保留、工作有界）。
  - `kernel.cpp` work 头判 `break` 前 drain 队列（I8；复核 §3.2）。
  - `KernelOutcome::apex_singular`（`kernel.hpp`）+ `overlap.cpp:492`：只有 kernel 真的
    遇到 apex 时才调用 `target_containment_apex`；seam 不再触发 2·N² 全格填充。
  - 合并后的 contributions 也做 4096 检查（`overlap.cpp`，复核 §3.1）。
- **前后**（同一 0.5°/px、order 6、max_depth 64、max_work 20000 的缝像素）：

| 情形 | 复核前（review §3.1） | 复核后（探针 F） |
|---|---|---|
| φ 面缝像素 | work=**20000**、**47.5 s**、unresolved=**0**（头判丢质量）、compute_overlap **8192** contributions（apex 全格填充） | work=**1525**、blocks=496、**0.6–0.8 s**、contributions=**4**、unresolved=276（有界 coarse）、termination=depth(`area_absolute_sr_not_met`) |
| z=+2/3 像素 | work=**20001**、**52.7 s**、unresolved=13524 | work=**487**、blocks=154、**1.3–1.6 s**、contributions=**4**、unresolved=66、termination=depth |

  说明：0.5° 像素（面积 ~7.6e-5 sr）在 1e-12 绝对预算下不可能靠 6 层细分达标，所以
  `termination=depth` 是诚实结果（工作有界、质量保留）；1″ 像素（X01）在 6 层后未决
  前沿 ≈ `2^-6 px` ≈ 4e-13 sr < 1e-12，仍可 `tolerance_met`（探针 E 已证）。z=2/3 的
  1.3 s 来自边界格的精确裁剪（§10.5），不是预算爆炸。

### 10.2 新 P1：`A_L=0` 的叶不再发布错误点值

- **数学**：`w=a/A`。`A_L=0` 时 `a_U/A_L` 无定义，但 `a≥a_L`、`A≤A_U ⇒ w≥a_L/A_U`，
  且 `a≤A ⇒ w≤1`。合法且更紧的包围是 `w∈[a_L/A_U, 1]`，不是 `[0,1]`；原实现的
  `[0,1]` 把代表点压到 0.5，`B0=F0/N0` 因此低 16.7%。
- **修改**：`drizzle.cpp` 的 A_L=0 分支（`[a_L/A_U, 1]`，仅当 `A_U>0`；`A_U≤0` 才退
  `[0,1]` 并保留具名）。
- **数值**（复核 R3：源0 Ω=4、A∈[0,4]、a=[4,4]、x=10；源1 Ω=A=a=1、x=0；真值 B=2）：
  **B 1.666667 → 2.000000**，区间仍包含 2（探针 C2.2/C2.3）。

### 10.3 新 P1：estimate 状态跟随 overlaps

- **修改**：`drizzle.cpp`；`estimate->assessment.complete/science/validity` 由
  `overlaps->status.termination==tolerance_met && bound_validity==certified` 决定。
- **数值**（复核 R3d：termination=depth、bound_validity=invalid、带 recipe）：
  complete **1→0**、science **bounded(0)→unconverged(1)**、validity
  **certified(0)→invalid(2)**（探针 C3.2–C3.4）。

### 10.4 新 P2：N / union / Ω_H 端点向外

- **修改**（`drizzle.cpp`）：N 的逐源项改 `multiply(weight, omega)`（`numerics::interval_mul`
  有向）；union 上界用有向 `add` 累加；`leaf_solid_angle_upper()` 对 π 与商各
  `nextafter(+inf)` 一次。下界 `max a_L` 是精确输入的最大值，无需舍入。
- **探针**：常量场仍 B=2 精确、区间 [1,1]（C2/C.5）；`SupportMap.area_upper` 仍是
  `min(Ω_H, Σa_U)` 的粗上界（I9 的独立 Boolean OR 仍未实现，见 §8）。

### 10.5 新 P1(工程)：精确裁剪“慢 2.6×”的归因与优化

- **归因**（gprof，0.5°/px、order 6、32 clips 的边界像素）：**97% 在 `model::BigInt::gcd`
  的朴素 Euclid**（未约分分母达 ~935 bit，单次 ~4 ms，997 次 = 3.9 s）；其次
  `integrate_affine` 里 `model::Rational` 每步 normalize（2.07 ms/叶）。半空间常数区间化
  只是让分母位长变大，不是根因。
- **优化（全部精确、不改值）**：
  1. 单元格级 inside/outside 快速路径（`kernel.cpp` 单元循环）：块矩形被四个内半空间
     包含 → 内外多边形就是矩形；被某个外半空间排除 → 重叠为 0；两者都跳过 8 次裁剪。
  2. 逐半空间“包含整个块矩形”时跳过该次裁剪。
  3. `integrate_affine` 全程 `ExactFraction`，结果直接量化到 2^-120 dyadic
     （`DyadicLedger::add(ExactFraction)`、`dyadic_from_fraction`），不再走
     `model::Rational` 的每步 gcd；`footprint_area` 同样。
  4. `ExactFraction` 用**二进制（Stein）gcd** + 尾部零位二分查找，替换朴素 Euclid
     （精确值不变，阈值 384 bit）。
  - **等价性证据**：用 exact dyadic dump 把“快速路径”与“关闭优化的全裁剪路径”逐位对拍，
    X01 内部/边缘与 0.5° 边界共 5 个构型的 `lower/upper/total` **逐位相同**。
- **前后**（同一 bench、同一 `-O2`、同一输入；prefix = `git show HEAD:` 的修复前源码）：

| 构型 | 修复前 HEAD | 复核后 | 加速 |
|---|---|---|---|
| X01 内部像素（1″、order 6、1 格） | 10.5 ms | **0.8 ms** | 13× |
| 0.5°/px 边界像素 row 0（4 格、16 clips） | 2990 ms | **90–95 ms** | 32× |
| 0.5°/px 边界像素 row 1（4 格、32 clips） | 11163 ms | **305–378 ms** | 30–37× |
| 0.5°/px 边界像素 row 64（2 格、16 clips） | 2193 ms | **42–64 ms** | 34–52× |

  ⇒ 复核的“修复后慢 2.6×”在加入上述优化后反转为 **13–52× 更快**。
- **仍存成本**：边界格的精确裁剪仍走 ExactFraction + gcd，约 2 ms/clip（阈值 384）。
  不改变科学量的进一步优化建议（未实施）：用“8 个原始半平面两两求交 + 可行点按极角
  排序 + 鞋带公式”直接算面积/矩，顶点分母只含一个 2×2 行列式，避免顺序裁剪的分母累积。

### 10.6 复核其他条目

- **§1.3（P2）`chart_partials` 的隐式 face 契约**：`chart.cpp` 两个极区分支增加
  `|4φ/π − L| ≤ 1` 的运行期拒绝（不再可能返回“错 L 的导数”）；`chart.hpp` 注释里的
  前置条件仍在写域外，建议 Lead 补一行。
- **§3.6（P2）`achieved` 无索引列**：`drizzle.cpp` 在 assessment 里记录
  `achieved_error_leaf_indices=...`，partial 帧的对应关系可读；模型类型缺 unknown 标记
  （`include/acsd/model.hpp` 写域外）。
- **§5.2 P0-6、`Identity::revision`、`make_csr_action` identity、`normalizer_min_sr`**：
  仍按 §8 未修（写域外/后续 task）。
- **§6（历史 `validation/geo/geo_validation.cpp` 的行和检查）**：该文件不在写域，
  **只报告**：`two_source_row_sums_normalised` 读 `op->rows` 期望行和≈1，是旧 `w` 语义；
  现在 `op->rows = R = w/N`，行和 = `Σ_p w_jp/N_p ≠ 1`。应改为验证
  `Σ_p R_pj·N_p = Σ_p w_jp = 1` 或 `R·x = B`。

### 10.7 复核后构建与探针

- `cmake --build build/science -j8 --target acsd` → **exit 0**。
- `validation/geometry_probe.cpp`：**75 ok / 0 fail**（新增 C2/C3/F 三组）。
- `process_geometry.overlap_m5_contracts`：104 项中 103 通过；唯一失败是
  `export positive: build_export [invalid_request]`（`src/projection/export_projection.cpp:395`
  “the export row cannot be applied”），来自工作树中他人未提交的 export 改动，与几何无关；
  几何/drizzle 全部通过。
