# 几何 / Drizzle 修复的独立复核（task-7 → 复查）

> **性质**：独立审查者复核，不参与实现。本文新建；未改 `src/`、未改他人 docs。
> **受检对象**：工作树（未提交）的 `src/geometry/{kernel,overlap,containment,drizzle,refine}.cpp`、
> `src/geometry/kernel.hpp`、`src/coordinates/chart.cpp`；工作树 HEAD = `376850340`
> （含 Lead 的 `src/workflows`、`src/model` 修改）。修复报告：[geometry-fix-report.md](geometry-fix-report.md)。
> **我的判据**：上一轮 [audit-geometry.md](audit-geometry.md) §1.1 的不变量 I1–I12，
> 从式 (1)(2) 重新推导，不依赖实现者的 probe 与其断言。
> **我自己的证据**：`/tmp/review_probe.cpp`（独立编写，未复用 `/tmp/geometry_probe.cpp` 的用例与断言，
> 只借用了公共 API 的构造方式）与一个独立的 Python 重算脚本；A/B 性能用 `git show HEAD:` 的
> 修复前 geometry 源码单独编译成 `/tmp/prefix/libgeom_prefix*.a` 对照。
> **未由我重跑**：`cmake --build`、`ctest`、X01 全链。我链接的是实现者的 `build/science` 库
> （时间戳晚于全部被改源码，见 §附录 B）。X01 全链结论仍以 Lead 的统一实跑为准。

---

## 0. 裁决总表

| 审计项 / 报告声明 | 我的裁决 | 关键证据（file:line 与我自己的数） |
|---|---|---|
| **P0-5** 极区偏导漏 `L` | **关闭** | `chart.cpp:127,142` 点版、`chart.cpp:158-204` 区间版（用 `chart_l(face)`）；我的 C1 重算：`ds/dz=3/(4r)` 精确符合，残差 `7.2346e-4 ≥ 真误差 3.5880e-4`（§1） |
| **P0-1** 跨源 hull Ω/A | **关闭** | `drizzle.cpp:186-271` 逐 (source,leaf)；我的 3 源常量场 `B=2`（真值 2）、`N=14=ΣwΩ` |
| **P0-2** `R=w/N` 与 `R C Rᵀ` | **关闭** | CSR 系数 `w0/N0`（`drizzle.cpp:316-323`）；`R·x=B`；列值 `1/14`；`Var=1/14`（旧的 `w C wᵀ`=14） |
| **P0-3** 常量身份 | **关闭（内容寻址）**，`Identity::revision` 仍=1 | `kernel.hpp:126-153`、`storage/local_storage.cpp:186-191`（数组内容寻址）、`refine.cpp:139-140`；`model/units.cpp:258` 未改且**无 `src/model` 改动**（未越界，只给建议） |
| **P0-4** 工作流合并 | 写域外：HEAD/工作树已携带 status/recipe/coarse + 内容 hash；primitives 未拼接具名 `conditional` | `normalize.cpp:1039-1092`；我确认 `merged_validity` 取最坏、`merged_termination` 取非 tolerance |
| **P0-6** `block>1` | **仍未修**（报告也如此声明） | 源行/`x[source_row]` 未变 |
| **P1** face 缝 / z=±2/3 | **部分关闭**：错叶已关闭，但代价是**工作预算爆炸**（新 P1） | §3.1 实测：一个缝像素 20000 work / 47–53 s / `term=work` |
| **P1** `chart_leaf` clamp | **关闭** | `chart.cpp:244-277` 哨兵；4 个调用点全部先验范围（`kernel.cpp:689-717,748-755`、`containment.cpp:539-548,629`、`chart.cpp:270-276`） |
| **P1** kernel/containment 冲突 | **关闭** | `overlap.cpp:583-599` 取交 + `contradictory_leaves` + invalid |
| **P1** `Σa+coarse=A` / `a≤Ω_H` | **关闭**（必要条件校验 + 收紧 + 约束行） | `overlap.cpp:639-651,744,968-987` |
| **P1** 取消保留当前块 | **关闭**，但 work 头判仍丢队列（新 P1） | `kernel.cpp:812` vs `kernel.cpp:611-616` |
| **P1** `tolerance_met`↔`area_absolute_sr` | **关闭**（`per_leaf` 也接入）；`normalizer_min_sr` 未接 | `kernel.cpp:855-879` |
| **P1** 旋转矩形 footprint | **关闭** | `overlap.cpp:121-158` |
| **新 P1** seam 激活 apex fallback | **新失败** | `overlap.cpp:486-491`；我的 φ-缝像素：8192 格全未决、contributions=8192、52.7 s（§3.1） |
| **新 P1** `A_L=0` 叶的发布点值 | **新失败** | 我的 R3：`B=1.666667`，真值 2（§3.4） |
| **新 P1** estimate 状态不跟随 overlaps | **新失败** | 我的 R3d：`estimate.complete=1, science=bounded, op.validity=invalid`（§3.5） |
| **新 P2** N/union 端点非向外 | 声明过宽 | `drizzle.cpp:258-259,266`；`leaf_solid_angle` `:60-63` |
| **新 P1(工程)** 精确裁剪成本 | 修复后再慢 ~2.6× | §5.1 A/B：1008.5 → 2621.8 ms（同 case、同 probe、同 -O3） |

**一句话**：P0-1/P0-2/P0-3(内容)/P0-5 与绝大多数 P1 我复核为**真的关闭了**；但 seam/z 分支的
"拒绝"把**静默错叶**换成了**每个缝像素吃光工作预算**，并使 estimate 的状态链与点值在 `A_L=0`
边界上仍不成立。报告对 seam 项用了"已修"（探针 `max_depth=1`），这在我的裁决里是**夸大**。

---

## 1. P0-5 复核（不依赖实现者 probe）

### 1.1 数学重推
北冠：`r=√(3(1−z))`、`d=r(4φ/π−L)`、`s=(J−r+d)/2`（`chart.cpp:62-65`），故
`∂d/∂z=(4φ/π−L)·dr/dz`、`∂s/∂z=(−dr/dz+(4φ/π−L)dr/dz)/2`、`∂s/∂φ=2r/π`。
修复后 `chart.cpp:127` 正是此式；南冠 `chart.cpp:142` 用 `dw/dz` 同构。
区间版 `chart_partials_interval` 现在接收 `face` 并用 `chart_l(face)` 减 `L`
（`l_const` 在 `chart.cpp:165`；北 `:182`、南 `:197`）。

### 1.2 我自己的 C1 重算（闭式真值，未调用 `chart_partials`）

面中经线 `φ=Lπ/4=π/4` 上真值 `∂s/∂z=3/(4r)`：

| z | 代码值（修复后） | 闭式真值 `3/(4r)` |
|---|---|---|
| 0.70 | +0.790569415 | +0.790569415 |
| 0.75 | +0.866025404 | +0.866025404 |
| 0.80 | +0.968245837 | +0.968245837 |

按 `kernel.cpp:277` 的均值形式复算残差（导数区间取块内真实极值，即区间版一定能包含的集合；
`e_s ≥` 由代码的区间给出，故下面是对代码残差的**保守下界**比较）：

| 块 | `e_s`(声明) | 真实 `max|s_true−s_model|` | 是否覆盖 | 旧（缺 L）模型误差 |
|---|---|---|---|---|
| `z∈[0.78,0.80]`、`φ∈[π/4±0.02]`（16px 量级） | 7.2346e-4 | 3.5880e-4 | **是** | 9.8079e-3（13.6×） |
| `z∈[0.78995,0.79005]`、`φ∈[π/4±1e-4]`（1px 量级） | 1.7658e-8 | 8.8284e-9 | **是** | 4.7254e-5（2675×） |

结论：**P0-5 关闭**。修复前的"声明小 12–13×"确认为真（与审计 §3 C1 的 12.4×、
实现者自报 13.6× 一致）。

### 1.3 仍需补的边界（P2，不是失败）
`chart_partials(z, φ)` 的公开声明没有 `face` 参数（`chart.hpp:43`），实现者用
`polar_longitude_constant(φ)=2·floor(2φ/π)+1 (mod 8)` 从**经度反推 `L`**（`chart.cpp:18-31`（函数在 `:25`））。
它只在"入参经度已被归一到该 face 分支"时成立——唯一调用点 `kernel.cpp:227` 传的
`phi_centre` 满足（`kernel.cpp:182-191` 已拒绝分支外的块）。但这是**隐式契约**：
将来任何调用者传绝对 RA 而不经 `chart_face_longitude`，会得到一个**静默错误的导数**。
建议改为 face 参数（或加断言/返回 false），并在 `chart.hpp` 注释里写死该前置条件。

---

## 2. P0-1 / P0-2 复核（我自己的 3 源常量场探针）

### 2.1 代码路径
- 逐 (source,leaf)：`a=[overlap_area]`、`A=[drop_area]`、`Ω=[native_area]`（`drizzle.cpp:194-206`）；
  `w=[a_L/A_U, min(1,a_U/A_L)]`（`:223-231`）；`A_L=0` 退 `[0,1]` 并具名（`:232-238`）。
- `F=Σ_j w_j x_j`（`multiply` 走 `numerics::interval_mul`，`:257`）；
  `N=Σ_j [w_LΩ_L, w_UΩ_U]`（`:258-259`，端点见 §2.3/§4 第2条）；代表点
  `F0=Σw0x`、`N0=Σw0Ω0`、发布 `B0=F0/N0`（`:252-261,304-307`）。
- `R_pj=w0_j/N0` 写 CSR，按 source 升序合并（`:316-323`）；response/噪声用同一 `rows`
  （`:457`、`:505-507`）。
- 无 `recipe` 时不再静默跳过：`recipe_identity_proven` 进 `estimate` 的具名 issue
  （`:549-555`），避免"没查就是通过"。

### 2.2 我的实测（`/tmp/review_probe.cpp`，输入完全由我构造）
三个源 `A_j=Ω_j=(1,4,9)`、`a_j=A_j`、常量场 `b0=2 ⇒ x=(2,8,18)`，真值 `N=14, F=28, B=2`：

```
R1  B=b0=2（精确）            N=14（=ΣwΩ）
R1  R 的三列都=1/14=w/N       R·x=2（同一 R 产生 signal）
R1  反转贡献顺序后 B 逐位相同，区间 [2,2]
R2  x=(-2,8,18) ⇒ B=24/14（负号保留）
R5  C=diag(1,4,9) ⇒ Var(B)=1/14=0.071429（旧 w C wᵀ 会是 14）
R4  source_row 越界 ⇒ invalid_model（不再静默填 0）
```

**P0-1/P0-2 关闭**：`N_p=Σ_j w_jpΩ_j` 逐源、无跨源 hull；`R=w/N` 是唯一算子，signal、
response、noise 都由它产生；常量场点值与源顺序无关（逐位）。

### 2.3 仍未覆盖的两处（见 §3.4 与 §4 第 2 条）
`A_L=0` 时的**点值**与 `N` 端点的**向外性**不在上面的探针覆盖范围内，分别单列。

---

## 3. 我新构造的反例

### 3.1 （P1）face 缝 / z=±2/3 的"拒绝"会吃光工作预算
源码：`build_block_model` 现在要求单一 chart 分支与 `s,t∈[0,1]`（`kernel.cpp:199-216`），
不满足即返回 false；调用者只按 `depth+1 < max_depth` 细分（`kernel.cpp:660-676`），
**没有"整块已在 face 之外 ⇒ 立即未决"的终止规则**。包含缝的块在**任意深度**都被拒
（真 `s` 在块内跨过 1/0），于是细分链永不收敛，只能撞 `max_depth` 或 `max_work`。

我的实测（`cut_cell_overlap` 直调，绕开 containment；order 6、`max_depth=64`、
`max_work=20000`、`area_absolute_sr=1e-12`，1 像素 drop）：

| 情形 | termination | work | unresolved | leaves | 墙钟 |
|---|---|---|---|---|---|
| face 4 内部（对照） | `tolerance_met`(0) | 5 | 0 | 4 | 2.6 s |
| **φ=45° 面缝上的像素** | **work(1)**，`work_budget_exhausted` | **20000** | **0** | 4 | **47.5 s** |
| **z=+2/3 线上的像素** | **work(1)**，`shared_work_budget_exhausted` | **20001** | 13524 | 3 | **52.7 s** |
| **跨面缝的 16×16 块** | **work(1)**，`work_budget_exhausted` | **20000** | 0 | 4 | **46.9 s** |

（同一次运行的计数：13353 次 chart 模型、6647 块、53176 次有理裁剪。）

- 成本与 drop 尺寸**无关**（1 像素与 16×16 块同价），因为它是"每块递归"，不是面积问题。
- X01/生产预算 `max_work=2e6`（`normalize` 的请求）下还要再乘 ~100 → **小时级/像素**。
- 触发条件：drop 跨 `φ=(2k+1)π/4` 的任一面缝（随机 RA 场约 1/8），或跨 `|z|=2/3`
  小圆（`|Dec|≈41.81°` 的场）。这类帧会以 `resource_exhausted` 失败（`normalize` 对
  `termination!=tolerance_met` 直接判 frame failed）。
- **正确的修法**（不是"继续细分"）：①块完全在 face 方格外 ⇒ 立即 `keep_unresolved`（终止）；
  ②更好：跨缝块按两个 face **分别求值并并包**（`SPHERICAL_DRIZZLE.md:45` 明确要求
  "跨 r=1/3 的区间分别求值取并包"），各自的 cell 分别记账；③apex 另有终止规则（已有）。

**连带效应（新 P1）**：kernel 一旦因缝变成 incomplete，`compute_overlap` 会**无条件**尝试
`target_containment_apex`（`overlap.cpp:486-491`）——即使这个 drop 离极点十万八千里。我的
φ-缝像素实测：

```
containment: segs=128 decided=0 undecided=8192 inverse=0 work=0
compute_overlap 总墙钟 52731 ms，返回 contributions=8192（每个都带该像素的 A、a∈[0,Ω_H]）
```

即 2 个候选 face × N²（order 6 时 8192）**全部未决**，每格还按 `1+samples` 计费
（`:384-386`，N=64 时 8192×129 ≈ 1.06e6 work 单位）。order 18 时 N²=6.9e10，会先撞
`max_work` 再撞共享账本。这条路径还会把 8192 个"未决叶"塞进 contributions，绕过
`kMaxInlineContributions=4096`（该检查只看 `kernel.leaves`，`overlap.cpp` 的 4096 检查在
containment 之前）。建议：只在确有 apex 迹象（`chart_interval` 因 `r≤0` 返回 false）时才
调用 apex 版本，并对最终 contributions 总数也做 4096 检查。

### 3.2 （P1）work 预算的**头判**丢弃未决质量
`kernel.cpp:611-616`：
```cpp
while (!queue.empty()) {
  if (outcome.work >= max_work) { outcome.termination = work; reason="work_budget_exhausted"; break; }
```
这里 **`break` 前没有 `keep_unresolved(queue)`**；另外两条预算路径（`:644-651`、`:826-834`）
都有。实测 φ-缝像素：`unresolved=0`（上面表格），即队列里所有未处理块的质量既不在叶上、
也不在 coarse 里 → 违反 I8。通常会被 §3.3 的面积校验抓成 `area_conservation_violated`，
但当丢失的是残余 frontier（小质量）时**不会**触发，且 `tolerance_met` 已不可能。

### 3.3 （P1）面积守恒校验本身是"必要条件"，不是完整性证明
`overlap.cpp:639-651` 的 `sum(a_L) ≤ A_U`、`A_L ≤ sum(a_U)+coarse_U`、`a_L≤a_U` 都是从
`Σ_p a_p = A` 推出的**必要条件**：任何包含真值的包围都自动满足，所以它们只能抓"明显坏掉"，
抓不到"漏掉一小块未决质量"（§3.2）。这符合我上轮建议的"运行时不变量"，但**不能**在文档里
说成"Σa+coarse=A 已验证"。

### 3.4 （P1，新）`A_L=0` 的叶发布"伪造的"点值
代码：`A_L=0` 时 `w=[0,1]`（`drizzle.cpp:232-238`），点值取 `representative(w)=0.5`（`:239`），
于是发布 `B0=F0/N0` 用的是**保守区间中点**——正是 I3/`SPHERICAL_DRIZZLE.md:166` 禁止的
"用中点伪造精度"（其余情形因为 `A_L>0` 时 `w` 区间紧，中点近似真值，所以问题被藏住）。

我的反例（R3）：源0 全盖该叶且 `a=A=4` 但只证到 `A∈[0,4]`（`x=10`），源1 正常 `A=a=1`（`x=0`）。
真值 `w=(1,1)`、`F=10`、`N=5`、**B=2**。代码发布：

```
B0 = 1.666667（低 16.7%），区间 [-0.0, 10.0] 仍包含真值
```

⇒ 区间包含测试通过、常量场测试也通过（常数因子 0.5 会约掉），**点值错**。建议：该叶标
`support_unresolved` 不发布 B；或点值用 `min(1, a0/A0)`，`A0=max(A_L,a0)`（保证 w≤1 且
不引入 0.5）。

### 3.5 （P1，新）estimate 的评估不跟随 overlaps 的未认证状态
`drizzle.cpp:525-528`：
```cpp
estimate->assessment.complete = !any_unresolved && !any_missing_error && recipe_identity_proven;
estimate->assessment.science  = (...) ? unconverged : bounded;
```
**不看 `overlaps->status`**。我的 R3d：把一个 `termination=depth`、`bound_validity=invalid`、
带 recipe 的 Overlaps 喂进去，所有叶的 `N_L>0`：

```
estimate.complete=1, estimate.science=bounded(0), op.validity=invalid(2)
```
即 operator 说"界无效"、estimate 说"已收敛"。当前 `normalize` 恰好用 `op->assessment`
判失败（`normalize.cpp` 的 `stage_terminated`），所以生产链暂时挡得住；但 export/coadd/refine
若读 `estimate->assessment`，会把未认证几何当 bounded。修复只需一行：
`complete &= (overlaps->status.termination==tolerance_met && overlaps->status.bound_validity==certified)`，
并把 science 相应降级。

### 3.6 （P2）`achieved` 数组丢失下标
`drizzle.cpp:438-448` 对无证书区间的叶 `continue`（不再写 NaN —— 这一点是对的），
但 `ErrorRecord::achieved` 没有索引列（`include/acsd/model.hpp:218-221`），序列化后
读者无法把 achieved[i] 对回第 i 个叶。干净帧不受影响；partial 帧会错位。

---

## 4. 与修复报告 / 探针的不一致（直说）

1. **报告把"seam/z 拒绝"记为"已修"并引用 D12/D13**：但 D12/D13 用的是
   `shallow.max_depth = 1`（`/tmp/geometry_probe.cpp:611`）。在 `max_depth=1` 下，
   被拒的块立刻落到 `keep_unresolved`，所以探针**看不到**生产 `max_depth=64` 下的
   指数细分。我的 §3.1 用 `max_depth=64` 复现出 47–53 s / 全预算。报告没有测这个配置。
2. **报告 §2.2 "区间四则改走 numerics、发布 [B_L,B_U] 因此是向外的" 过宽**：
   `f_interval`/`brightness` 确实向外（`drizzle.cpp:37-54,257,304`），但
   `n_interval` 的两个端点是**裸 double 乘**（`:258-259`），`union_upper +=`（`:266`）与
   `leaf_solid_angle`（`:60-63`）也是裸 double。严格说 B 的包围在 ulp 级可能不包含。
3. **报告 §2.2 "A_L=0 时退化为 w∈[0,1] 的合法包围"**：包围合法，但被写成"合法"就带过了
   点值问题；§3.4 是反例。
4. **报告"P0-5 修复 chart.cpp:116-204"**：点版在 `:120-155`、区间版 `:158-204`、`L` 反推在
   `:18-29`。实质正确，行号略偏。
5. **报告 §5.2 的 `D12 首轮 code=unsupported`** 是拆分 `build_density_model` 之前的现象；
   现在 `footprint_area` 走 `require_chart=false`（`kernel.cpp:915-917`），我确认这条耦合处理
   是对的（它也顺带回答了审计没提的"`footprint_area` 复用 `build_block_model`"）。
6. **`ctest 19/20` 与"唯一失败与几何无关"**：我未重跑，依赖他人记录，不在此确认。
7. **性能**：报告没有给出修复前后同一构型的对照；我给出 §5.1 的 A/B。

---

## 5. 性能与 block-size（补充我上一轮的 §4）

### 5.1 同一构型的 A/B（我的 probe，同一像素、order 6、4 个候选格）
| 版本（同为 `-O3 -DNDEBUG`） | 总 | 其中精确裁剪 | clips |
|---|---|---|---|
| 修复前（`git show HEAD:` 源码编译） | 1008.5 ms | — | 32 |
| 修复后（工作树） | 2621.8 ms | 1922.9 ms | 32 |

⇒ **修复使精确裁剪慢 ~2.6×**（另用 `-O2` 对 `-O2` 的同一对照得 706.7→1962.2 ms，2.8×，
方向一致；两次都在同一 probe、同一输入上）。机制与"半空间常数改为区间求值"
（`kernel.cpp:321-364`）一致：
`from_double_exact(offset)` 的分子/分母位长上升，而 8 次裁剪的分母是乘性增长的，主成本仍在
裁剪（这里 98%）。

绝对量级：**一个内部像素 2.6 s**（1 px、order 6、4 个候选格，-O3）；而冻结档案
（`validation/apps/evidence/x01/attempts/geometry_performance_baseline.md`）称整帧 16384 行
只要 75.3 s（4.6 ms/行）。两者差 ~2 个数量级，说明 **那份档案不能作为当前实现的性能基线**；
lead 实测的 ">20 分钟/帧" 与我的量级方向一致（我这里是小时级，机器/配置不同）。
⇒ 上一轮"批量 invocation、保留 block=1"的结论不变，并且现在**必须**同时处理精确裁剪成本：
建议按 `SPHERICAL_DRIZZLE.md:261` 的"跨块累计先向外量化到固定 dyadic 格 `2^-b`"，
把**半空间常数**也向外量化到 `2^-b`（b≈90；inner 取 floor、outer 取 ceil，仍是有效包含），
分母位长被钉住；再叠加 interior-run 精确合并（上一轮审计 §4.2 R1）。

### 5.2 P0-6 仍未修
报告声明不改 workflows；我确认 `x[source_row]` / `source.sample=block` 路径未变，
`block>1` 仍不是同一估计量。建议 Lead 在 `normalize` 对 `block_size>1` fail-fast，
或实现上一轮 B1–B7。

---

## 6. 建议的最小修复顺序（不参与实现）

1. **seam/z 终止规则**（P1，最高优先）：完全在 face 方格外 ⇒ 立即 `keep_unresolved`；
   跨缝 ⇒ 两 face 并包（正解）或至少用预算外的"seam 终止"规则截断。
2. **`kernel.cpp:611-616` 的 work 头判 drain queue**（P1，一行）。
3. **apex fallback 触发条件收紧**（P1）：只在确有 apex 迹象（`chart_interval` 因 `r≤0` 失败）
   时才走 `target_containment_apex`；否则 seam 像素会额外吃 `2·N²` 格（我实测 8192 格全
   undecided）。
4. **estimate 状态跟随 overlaps**（P1，一行 + science 降级）。
5. **`A_L=0` 不发布点值**（P1）：标 unresolved 或改用 `min(1,a0/A0)`。
6. **`n_interval`/`union_upper`/`leaf_solid_angle` 走 interval 原语**（P2，一致性）。
7. **半空间常数 dyadic 量化 + interior-run 批量化**（工程，§5.1）。
8. （Lead）`Identity::revision` 递增；`make_csr_action` 设 action identity；`block>1` fail-fast。

---

## 附录 A：我的探针（打印原样）

```
[R1] constant field with three sources (A=Omega=1,4,9; b0=2)
ok   R1b B = b0 = 2 exactly                2.000000
ok   R1d N = sum_j w_j Omega_j = 14        14.000000
ok   R1g R applied to x reproduces B       2.000000
ok   R1h R column 0/1/2 = w/N = 1/14       0.071429
ok   R1i the two orderings give the identical published value   2.000000 vs 2.000000
[R2] x=(-2,8,18)  ok R2a B = 24/14          1.714286
[R3] A_L=0  FAIL R3b published point 1.666667 (truth 2)  ok R3c interval contains [-0.0,10.0]
[R3d] FAIL estimate.complete=1 science=0  op.validity=2（应 unconverged）
[R5] Var(B)=R C R^T  0.071429  (w C w^T = 14)
kernel interior:        term=0 work=5     unresolved=0    leaves=4   2621.8 ms
kernel phi-seam pixel:  term=1 work=20000 unresolved=0    leaves=4  47467.8 ms
kernel z=2/3 pixel:     term=1 work=20001 unresolved=13524 leaves=3 52671.5 ms
kernel phi-seam 16x16:  term=1 work=20000 unresolved=0    leaves=4  46854.0 ms
```

## 附录 B：复现方式（我实际执行的）

```bash
# 我的探针（不复用实现者 probe 的用例/断言）
c++ -std=c++20 -O2 -I include -I src /tmp/review_probe.cpp -o /tmp/review_probe \
  -Wl,--start-group build/science/lib{geometry,coordinates,operators,uncertainty,numerics,model,information,storage,resources,runtime}.a -Wl,--end-group -lzstd -lpthread
/tmp/review_probe drizzle      # R1–R5
/tmp/review_probe kernel0..3   # 内部 / φ-缝 / z=2/3 / 16x16 缝
# A/B：把 git HEAD 的 geometry+chart 源码单独编译（同一 -O2，另有 -O3 对照）
git show HEAD:src/geometry/kernel.cpp > /tmp/prefix/src/geometry/kernel.cpp  (其余同理)
ar rcs /tmp/prefix/libgeom_prefix.a /tmp/prefix/obj/*.o
```
构建库的时间戳（`build/science/*.a`）晚于全部被改源码，故我链接的是修复后的实现。

---

## 11. 复核后修复的独立复核（`79346a8ee`，2026-11-10 第三轮）

> 本节针对本文件 §3.1/§3.2/§3.4/§3.5 与 §4 第 2 条的新 P1/P2 做复验；其余结论仍以 §0–§6 为准。
> 受检：`79346a8ee`（HEAD 为 `48fa046ac`），工作树干净；`build/science` 库时间戳晚于全部被改源码。
> 我仍然只用自己的 probe（`/tmp/review_probe.cpp` 扩展）与独立的 prefix 库对照，未复用实现者
> `validation/geometry_probe.cpp` 的断言。

### 11.1 裁决

| 上一轮项 | 本轮裁决 | 我的证据 |
|---|---|---|
| §3.1 seam/z=±2/3 吃光预算 | **关闭**（工作有界、质量守恒、无错叶） | 1″ 像素、order 6、`max_work=20000`、`max_depth=64`：φ-缝 work **20000→1525**、**47.5 s→0.36–0.43 s**、unresolved **0→276**；z=2/3 work **20001→1259**、**52.7 s→0.28–0.32 s**、unresolved 234；16×16 缝块 work 1525、0.31–0.59 s |
| 同上：未决质量是否丢弃 | **关闭**（精确守恒） | φ-缝：`Σa=2.192064e-11`、`unresolved_U=1.583795e-12`、`A=2.350443e-11` ⇒ 和 = A；z=2/3：`2.216165e-11+1.342782e-12=A`；16×16 块：`5.611683e-09+4.054514e-10=A`。我独立用 `footprint_area` 复算 A |
| 同上：是否静默错叶 | **关闭**（order 12 抽样，0 错叶） | 对 1″ drop 做 25×25 独立 chart 抽样（`chart_inverse`→cell→`chart_leaf`），published 的 4/4/3/4 个叶全部命中抽样集，`wrong_leaves=0`、`sampled_not_published=0`；面缝两侧由子块中心各自选 face，缝带留在 unresolved |
| §3.1 连带：seam 激活 apex fallback | **关闭** | `KernelOutcome::apex_singular` + `overlap.cpp` 只在 apex 时调用；缝像素 contributions **8192→4**（无 2·N² 全格填充） |
| §3.2 work 头判丢队列 | **关闭** | `kernel.cpp` 头判 `break` 前 drain；实测 `unresolved` 非 0 且与 A 精确闭合 |
| §3.4 `A_L=0` 点值 | **关闭** | 我的 R3：B **1.666667→2.000000**，区间 **[2,2]**（w=[a_L/A_U,1] 而非 [0,1]） |
| §3.5 estimate 状态 | **关闭** | 我的 R3d：`complete 1→0`、`science bounded→unconverged`、`validity certified→invalid`，与 `op.validity=invalid` 一致 |
| §4 第 2 条 N/union 端点向外 | **union 关闭并给出可判别反例；N 由构造向外但未构造出可判别反例** | union：源 a_U=(1, 2⁻⁵³) ⇒ 旧 `area_upper=1.0` **< 精确 1+2⁻⁵³**（上界失效），新 `1.0000000000000007` ≥；N：源码已改 `interval_mul`，但我的单源判别搜索里旧实现的 N 1 ulp 误差被 interval 原语自带的数 ulp 松弛吸收，B 区间仍包含真值（详见 11.3） |
| §5.1 精确裁剪性能/精度 | **关闭（更快且逐位不变）** | 内部像素：prefix(79346a8ee^) **1319.0 ms → 39.97 ms（33×）**，且**叶 dyadic 上下界逐位相同**；缝像素 25.2/32.0/28.0 s → 0.51/0.39/0.34 s（49–82×，界按设计变化）；另把新源码的"快速路径"三处短路强制关闭后与开启版对拍 4 个构型 **BIT_IDENTICAL** |

**仍失败的**：无新增失败（本轮 5 项均通过）。**仍存的两处表述问题**见 11.4。

### 11.2 我自己的数（原样）

```
kernel interior:        term=0 work=5      unresolved=0   A=2.350443e-11  sum(a)=2.350443e-11  un=0           39.8 ms
kernel phi-seam pixel:  term=2 work=1525   unresolved=276 A=2.350443e-11  sum(a)=2.192064e-11  un=1.583795e-12 428.7 ms
kernel z=2/3 pixel:     term=2 work=1259   unresolved=234 A=2.350443e-11  sum(a)=2.216165e-11  un=1.342782e-12 321.3 ms
kernel phi-seam 16x16:  term=2 work=1525   unresolved=276 A=6.017134e-09  sum(a)=5.611683e-09  un=4.054514e-10 592.0 ms
leak order-12: interior 4/4 wrong=0; phi-seam 4/4 wrong=0; z=2/3 3/3 wrong=0; 16x16 4/4 wrong=0
R3  A_L=0: B=2.000000 interval [2,2]
R3d status: complete=0 science=1(unconverged) validity=2(invalid)  op.validity=2
outward-a (N, single source): B=[19.999999999999982,20.000000000000018]  (exact 2/0.1 略小于 20 → 包含)
outward-b (union):      old area_upper=1.0   new area_upper=1.0000000000000007   (exact 1+2^-53)
A/B case0: prefix2 1318.99 ms vs new 39.97 ms，LEAF_BOUNDS_IDENTICAL（33×）
快速路径开/关: case0..3 全部 BIT_IDENTICAL（同一源码、同一库，只把三处短路置 false）
```

`term=2` = `Termination::depth`，reason=`area_absolute_sr_not_met`：seam 像素的未决前沿
（1.58e-12 sr）大于 1e-12 的请求预算，所以是**诚实未收敛**；这正是"工作有界 + 质量保留 +
不冒充 certified"的语义。

### 11.3 关于 N 端点的说明（我未能构造可判别反例）
- 源码层面：`n_interval = add(n_interval, multiply(weight, omega))`、`union_upper` 走 `add`、
  `leaf_solid_angle_upper` 两次 `nextafter(+inf)` —— 与"向外"一致（我逐行读过）。
- 数值层面：我按"旧实现用最近舍入的 `w·Ω` 端点"构造单源判别式（B=x/Ω 与 w 无关），
  精确有理数下旧 B 的端点本应漏掉真值；但**实测**旧库也给出
  `[19.999999999999986, 20.000000000000014]`，比精确值宽 ~8 ulp —— 即 `numerics::interval_*`
  的既有松弛吸收了这 1 ulp 的 N 误差。所以这一项我只能判"**由构造成立**"，
  不能像 union 那样给出"旧错新对"的可判别反例；**union 的反例是可判别的**（见上表）。
- 这不影响裁决：union/N 的改动方向正确，且 union 的旧失效已被实测钉死。

### 11.4 报告里仍略偏乐观的两处（不影响裁决，需在文档里写准）
1. §10.1 "1″ 像素在 6 层后未决前沿 ≈ 2⁻⁶ px ≈ 4e-13 sr < 1e-12，仍可 `tolerance_met`"：
   我的实测是 **276 个前沿块 × 2⁻¹² 像素 = 6.7% 像素 = 1.583795e-12 sr > 1e-12**
   ⇒ 1″ 缝像素是 **depth/unconverged**，不是 tolerance_met。该句只对"不跨缝的像素"成立
   （不跨缝时 unresolved=0，本就不需要这 6 层）。前沿块数随缝的链长 ~2^d 增长，
   所以质量比例 ≈ 2^(d)/(4^d) 的链长因子，比 2^-d 的估算大 4×。
2. §10.1 的 "unresolved 有界（276/66）"：276 我在 1″/order-6 上复现；66 来自 0.5°/px 构型，
   我的同构型是 234（z=2/3）。数量级与"有界"结论一致，但不同构型不可直接比较，
   文档应写明构型。

### 11.5 我这一轮没有覆盖的
- apex（极点真的在 drop 内）路径未重跑；只确认了"seam 不再误触发 apex"（contributions 4 而非 8192）。
- 0.5°/px 生产构型未重跑（我用 1″/px，order 6/12）；报告 §10.5 的 13–52× 我在自己的构型上得到 33×（内部）
  与 49–82×（缝），方向一致。
- `ctest`、X01 全链依赖他人记录。
