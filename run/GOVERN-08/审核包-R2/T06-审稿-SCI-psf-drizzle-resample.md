# T06 审稿 · SCI-psf-drizzle-resample · 第 6–10 轮

**车道**：T06 审稿（只读）｜**范围**：`docs/science/psf/**`、`docs/science/drizzle/**`、`docs/science/resample/**`｜**负责轮次**：6 推理 / 7 负向 / 8 一致性 / 9 语言 / 10 可复现

**本车道为审稿车道，不重写文档。** 全程零 git 写操作（未 `add`/`commit`/`checkout`/`reset`/`stash`），未编译，未跑仓内实验脚本，未修改任何仓内文件；仓内唯一写入面是本交付件。数值核算在 `/tmp` 临时 python（仓外）执行。中文路径一律 `git -c core.quotepath=false`。

---

## 0. 命名与基线声明

- **`L.key` 取值说明（需前台确认）**：派单只给了范围目录 `docs/science/psf/** docs/science/drizzle/** docs/science/resample/**`，未给 `L.key` 字面值。本车道按「层-主题-并列目录」惯例取 `SCI-psf-drizzle-resample`。若前台的分片键另有命名，改文件名即可，正文无影响。
- **基线**：`git -c core.quotepath=false rev-parse HEAD` 未在本次交付前冻结（见 §7.3 的限制声明）；本文所有行号、命令均针对**工作树实读**，复跑命令一律给出，重新执行即得同一读数。
- **本轮是否达到收敛**：**否**。本车道只跑第 6–10 轮，无法判定「连续两轮无新增实质问题」的收敛条件是否满足；本车道能负责的只是诚实报告本 5 轮**是否零发现**（见 §4）。

---

## 1. 读完了吗

| 文件 | 行数 | 本车道逐行读完 | 子代理逐行读完 |
|---|---|---|---|
| `docs/science/psf/PSF.md` | 277 | **是（本人）** | 是（PSF 车道 ×2） |
| `docs/science/drizzle/DRIZZLE.md` | 278 | **是（本人）** | 两条 DRIZZLE 车道**未回件** |
| `docs/science/resample/RESAMPLE.md` | 328 | **是（本人）** | 是（RESAMPLE 车道） |
| `docs/science/psf/README.md` | 2 | **是（本人）** | 是 |
| `docs/science/drizzle/README.md` | 2 | **是（本人）** | 是 |
| `docs/science/resample/README.md` | 2 | **是（本人）** | 是 |
| **合计** | **889** | **889 / 889 = 100%** | 并集 100%（DRIZZLE 侧靠本人） |

**未读完的逐份列出**：**无。** 六份成员文件全部逐行读完，分母 889 行全部计入，无「转引未读」项。

**子代理派发**：按要求派发 **5 条独立车道**（因工具对同一 prompt 返回两个实例，实发 6 个子代理实例，去重后 5 条）：

| 车道 | 覆盖 | 状态 |
|---|---|---|
| A（`44cbfe84`） | PSF.md 第 6–10 轮 | **已回件，17 条实质 + 1 否定 + 通过项** |
| A'（`f574bf06`） | 同上（重复实例，**内容与 A 互补而非重复**） | **已回件，22 条 + 5 低危**；独立找到 `σ_g=α/2` 的一手出处 Meyers & Burchat 2015，并实测出「四候选循环」的危害概率 |
| B（`c75a4e8c`） | DRIZZLE.md 第 6–10 轮 | **已回件，17 条** |
| B'（`e2572e71`） | 同上（重复实例，**与 B 高度互补**） | **已回件，28 条（去重 25）**；独立给出 δ 闭式复算与 HEALPix 外接半径全天穷举 |
| C（`320f5ce1`） | RESAMPLE.md 第 6–10 轮 | **已回件，28 条** |
| D（`f3bc012b`） | 三册跨文档一致性 + 语言 + 可复现 | **已回件，22 条** |

**6 个子代理实例 / 5 条独立车道，全部回件。** 五条车道互为独立第二读者，本车道对每条最重的子代理发现都做了亲自复跑（复跑结果见各条「依据」栏与 §5 的裁决表）。

⚠️ **一处必须声明的自我更正**：初稿写作时 B/B' 尚未回件，本车道据此把「DRIZZLE 侧缺少独立第二读者」写进了限制声明。B/B' 回件后**该限制已解除**，且它们带回了两条**推翻本车道既有结论**的发现（见 §5 更正栏与 §8）。本车道据此重写了相关条目，未保留被推翻的表述。

---

## 2. 逐轮问题清单

### 第 6 轮 · 推理轮（论证链完整、假设/适用域/失效域、无跳跃结论）

---

**【R6-1 · 阻断】`docs/science/psf/PSF.md:173`（判据）与 `:157`（参数表）｜「旋转简并不变量」在字面读法下恒不成立；正确简并群必须同时置换 `(sx,sy)`**

*依据（本人独立推导 + 数值实测）*：把 `PSF.md:43-45` 的 `p₁,p₂,p₃` 表达式代入四个候选 θ，固定 `sx=3, sy=2, θ=0.37`，得系数三元组

| 候选 | `(p₁, p₂, p₃)` | 与基准恒等？ |
|---|---|---|
| `θ` | `(0.06463651, −0.02341277, 0.11591905)` | — 基准 |
| `π/2−θ` | `(0.11591905, −0.02341277, 0.06463651)` | ❌ p₁↔p₃ 互换 |
| `π/2+θ` | `(0.11591905, +0.02341277, 0.06463651)` | ❌ p₁↔p₃ 且 p₂ 变号 |
| `π−θ` | `(0.06463651, +0.02341277, 0.11591905)` | ❌ 仅 p₂ 变号 |

模型值层面 `I(x,y)` 在 `(x,y)=(1.3,−0.7)` 处：基准 `0.468599348733`，三候选分别给 `0.384176029522 / 0.507200444324 / 0.627803886550` —— **不是「偏差」，是三个不同的图像**。

真正的简并群（本人实测）：`{(sx,sy,θ), (sy,sx,θ+π/2), (sx,sy,θ+π), (sy,sx,θ+3π/2)}`，四项全部给出同一 `M`。即 `θ→θ+π`（保 `sx,sy`）与 `θ→θ+π/2`（**必须同时交换 `sx,sy`**）。`PSF.md:48` 写对了这一条，`:157` 与 `:173` 写错了同一件事——**同一文档内两处互斥**。

*生产代码同步确认*：`lib/algorithms/psf/src/dpsf_psf.cpp:634` `double thetas[4] = { theta, M_PI/2.0 - theta, M_PI/2.0 + theta, M_PI - theta };`，`:638` `double test_params[7] = { B, A, x0, y0, sx, sy, thetas[t] };` —— **`sx`、`sy` 全程固定，真简并伙伴 `(sy,sx,θ+π/2)` 根本不在候选表里**。`grep -n "swap\|std::max(sx\|std::min(sx" lib/algorithms/psf/src/dpsf_psf.cpp` 只命中 `:654` 的椭率计算，无任何轴交换。

*连带后果（本人推导）*：`dpsf_psf.cpp:649` 在选定 θ 后**重算** `mad`，而 `PSF.md:164` 定义 `q_psf = A/residual_scale`。因此 `residual_scale` 与 `q_psf` **都被消歧步骤改变**，`PSF.md:173`「消歧只影响位置角列的取值，不影响 `sx`、`sy`、`fwhm_x`、`fwhm_y`、`flux`」漏列了这两个量。且 `mad` 取四个候选的**最小值**，引入阶次统计量选择偏差（系统性偏低），而 `PSF.md:194` 的残差→σ 换算推导未覆盖该偏差。

*复跑*：
```bash
cd "/workspace/Astro CS Database" && sed -n '634,650p' lib/algorithms/psf/src/dpsf_psf.cpp
python3 -c "
import math
def q(t,sx,sy):
    return (math.cos(t)**2/(2*sx*sx)+math.sin(t)**2/(2*sy*sy),
            math.sin(2*t)/(4*sx*sx)-math.sin(2*t)/(4*sy*sy),
            math.sin(t)**2/(2*sx*sx)+math.cos(t)**2/(2*sy*sy))
sx,sy,t=3.,2.,0.37; b=q(t,sx,sy)
for n,t2 in [('th',t),('pi/2-th',math.pi/2-t),('pi/2+th',math.pi/2+t),('pi-th',math.pi-t)]:
    print(n, q(t2,sx,sy), q(t2,sx,sy)==b)
print('真简并伙伴 (sy,sx,th+pi/2):', q(t+math.pi/2,sy,sx)==b)"
```

*建议改法*：`:173` 改为「模型值只在 `(sx,sy,θ) ↦ (sy,sx,(π/2+θ) mod π)` 与 `θ↦θ+π` 下不变」；`:157` 的候选集同步替换；`:35`/`:156` 补轴约定（须钉 `sx ≥ sy`，否则 `(sx,sy,θ)` 与 `(sy,sx,(π/2+θ) mod π)` **同在 `[0,π)` 内**，θ 无法单值化）；代码 `dpsf_psf.cpp:634-646` 改为在交换 `sx,sy` 前提下比较两个候选；`:173` 的「不受影响」集合补 `residual_scale` 与 `q_psf`；`:194` 补选择偏差说明。

---

**【R6-2 · 须修（本车道初稿判「阻断」，经 A' 车道一手核证后降级）】`docs/science/psf/PSF.md:77`｜`σ_g = α/2` 的「同二阶矩」标注口径未写明是逐轴，且其 β 一般式未声明**

⚠️ **本车道自查更正（重要）**：本车道初稿把此条列为阻断，理由是「文档上一行刚定义 σ = √⟨r²⟩，而同二阶矩高斯按定义 σ_g = √⟨r²⟩ = σ」。**该推理有缺陷，本车道接受否决。**

*反证（PSF 车道 A'，本车道复核其算术）*：`PSF.md:77` 写的是「『**同二阶矩**高斯』的**逐轴**标准差 `σ_g = α/2`」。若「同二阶矩」按**逐轴**口径读（`⟨x²⟩` 而非 `⟨r²⟩`），则 `σ_g = √⟨x²⟩ = √(α²/(2(β−2))) = α/√(2(β−2))`，**代入 β=4 恰为 `α/2`** ⇒ 文档在 β=4 下**是对的**。一手出处 A' 已核对原文：**Meyers J. E. & Burchat P. R. 2015, ApJ 807, 182**（[arXiv:1409.6273](https://arxiv.org/abs/1409.6273)，DOI [10.1088/0004-637X/807/2/182](https://doi.org/10.1088/0004-637X/807/2/182)）§7 原句「a β = 3.0 Moffat PSF with the **same second-moment squared radius** as a Gaussian PSF will have a FWHM only 60% as large as that of the Gaussian」，其 Eq.7 的 `r² = Ixx + Iyy` 即**逐轴**口径。
⇒ **本车道初稿的「文档自相矛盾」判定被推翻**；UNRESOLVED **U1 同时关闭**（见 §3）。

*但三条残留问题仍成立（严重度：须修）*：
1. **「同二阶矩」未写明逐轴/径向口径** —— `PSF.md:75` 的 `σ = √⟨r²⟩` 是**径向**口径，`:77` 的 `σ_g` 是**逐轴**口径，两者并列而口径切换未标注，是读者（包括本车道）会读错的根源。*建议*：`:77` 明写「逐轴二阶矩口径 `⟨x²⟩ = ⟨r²⟩/2`」。
2. **β 一般式未声明** —— 正确式是 `σ_g = α/√(2(β−2))`，`α/2` 只是 β=4 的值；本仓 β 冻结在 4（`:153`）故数值无误，但 `:88` 同时引入一般 β 的 FWHM，读者易把 `α/2` 当通式。
3. **`PSF.md:77` 紧接着的因果句仍不成立** —— 「两种口径…跨口径代入会直接带来 √2 的尺度偏差、相应 **√2 倍的 FWHM 偏差**」。实测 `1.7399177682/√2 = 1.2303077025` ≈ 闭式 `1.2303076525901024` ⇒ **一致换代（`σ→σ_g` 同时因子 `1.23031→1.73992`）给出同一个 FWHM**，无 √2 的 FWHM 偏差；√2 只出现在只换其一时。**这一条本车道初稿判对了，未被推翻。**

*跨文档新发现（A' 车道，本车道未复核行号，转引）*：`docs/science/detection/STAR_DETECTION.md:40,140-142` 报「高斯 FWHM 系数 2.354820 vs Moffat4 1.230310，相差 1.9140×」。A' 指出该 1.9140 是**同数值系数之比**，而按 PSF 分册的 `σ_g` 口径，同一物理 PSF 的跨块 FWHM 偏差应为 `2.354820/1.739918 = 1.3534`，两者正好差 √2。**⇒ 两篇分册对同一个量给出互不相容口径**，属跨篇改动。

*复跑*：`python3 -c "import math;print(4*math.sqrt(2**0.25-1)/math.sqrt(2), 2*math.sqrt(2)*math.sqrt(2**0.25-1))"`

---

**【R6-3 · 须修】`docs/science/psf/PSF.md:118`｜3% 判据阈值偏 33%；`√2σ` 处 `f_out` 是 1/8 不是 3%**

*依据（本人独立求解）*：解 `(1+r²/(2σ²))⁻³ = 0.03` ⇒ `r/σ = 2.1063228378790524`。而文档写的边界是 `√2σ`。`f_out(√2σ) = (1+1)⁻³ = 0.125` **精确**。文档那句作为蕴含命题为真（充分条件），但作为**判据边界**偏小 32.9%：`1.41σ < r_win < 2.11σ` 这段窗内 `f_out` 在 3%–12.5% 之间，会被判「窗内通量语义不必降级」。`*复跑*`：`python3 -c "import math;print(math.sqrt(2*((0.03)**(-1/3)-1)))"`
*建议改法*：`:118` 写 `f_out > 3% ⟺ r_win > 2.106σ`，并把 `√2σ` 改述为它真正的含义（`f_out = 1/8`）。

---

**【R6-4 · 阻断】`docs/science/drizzle/DRIZZLE.md:98`｜等价参数化的分母表达式漏了 `A_pixel,j` 因子**

*依据（本人独立推导）*：`DRIZZLE.md:98`「存在一个等价的参数化 `w'_jp = a_jp / A_pixel,j`（分母取覆盖面积）配 `N'_p = Σ_j w'_jp = D_p`」。用 `:166` 的闭合恒等式 `Σ_p a_jp = pixfrac²·A_pixel,j`：

```
Σ_j w'_jp = Σ_j a_jp / A_pixel,j = (Σ_j a_jp) / A_pixel,j ≠ D_p = Σ_j a_jp
```

本人用闭合一致夹具（`A_pixel=3.0, pixfrac=0.8, a=[0.96,0.48,0.48]`，`Σa = 1.92 = A_drop`）实测：`Σ_j w'_jp = 0.64`，而 `D_p = 1.92` ⇒ **不等**；只有 `Σ_j w'_jp·A_pixel,j = 1.92 = D_p` 成立。正确写法是 `N'_p = Σ_j w'_jp·A_pixel,j = D_p`（与 `N_p = Σ_j w_jp·A_pixel,j` 同型）。

*同一错误在代码头重复*：`lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h:30`「等价参数化（同一 c_jp）: w'_jp = a_jp/A_pixel_j, **N'_p = Sum_j w'_jp = D_p**」；`:187`「sb_a_pixel : w_jp = a_jp/A_pixel_j , N_p = Sum_j w_jp」。

*建议改法*：`DRIZZLE.md:98` 与 `drizzle_science.h:30` 补 `·A_pixel,j`。

---

**【R6-5 · 阻断】`docs/science/drizzle/DRIZZLE.md:114-119`｜反例分支的因子取反，与同文 `:98` 和自己的后半句对撞**

*依据（本人独立推导 + 数值）*：`:116` 写「若改取 `w_jp = a_jp / A_pixel,j`，则 `Σ_p w_jp = 1/pixfrac²`，总通量被压低 `pixfrac²` 倍，`pixfrac = 0.8` 时压到 `0.64`」。由 `:166` 闭合式：`Σ_p (a_jp/A_pixel,j) = A_drop,j/A_pixel,j = pixfrac²`。实测 `pixfrac=0.8` → `0.64`。⇒ **「压到 0.64」与「压低 pixfrac² 倍」两句都对，只有 `1/pixfrac²`（=1.5625）写反了**。

*三重独立佐证*：① 同文 `:98`「按像元面积归一给出 `Φ_out = pixfrac²·Σ_j x_j`」；② `lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h:131`「w'_jp = a_jp/A_pixel_j = **pixfrac² · w_jp**」；③ 实验单元 `实验/healpix-polar/REPORT_paper.md:73`「**A_pixel 归一给 Σ_p w′ = pf²**（亏 36%）」与 `:46`「其自身 `Σ_p w′_jp = pixfrac²` 不满足通量守恒门」。

⇒ **`1/pixfrac²` 这个因子在两处被互换了**：它本属 `:117` 的「覆盖面积分母」分支（实验测得 `+56.25% = 1/pf²−1`，与 `:117` 相符），却被误放到 `:116`。

*建议改法*：`:116` 改为 `Σ_p w'_jp = pixfrac²`。

---

**【R6-6 · 须修】`docs/science/drizzle/DRIZZLE.md:169`｜几何闭合被声明为「构造性的、不依赖数值逼近」，但闭合门比较的是独立实测量，可红**

*依据（本人独立推导 + 代码）*：`:169` 写 `Σ_p a_jp = pixfrac,j = pixfrac²·A_pixel,j`「是构造性的，不依赖数值逼近」。但右侧的 `A_pixel,j` 是**独立实测**的未收缩像元面积（`drizzle_engine.cpp:1170-1189`，另一组四角另做 4 次 `pixelToSky`），不是由 `Σ_p a_jp` 导出的。`A_drop,j = pixfrac²·A_pixel,j` 仅在**像元内天球面积尺度因子 σ 常数**时精确：一般地 `A_drop,j = pixfrac²·A_pixel,j·(σ̄_drop/σ̄_pixel)`，有 SIP 畸变或非恒定投影尺度时该比值偏离 1。而 `:52` 明确说本算子处理「含 SIP 畸变」的四角。

*代码侧确认这不是恒等式*：`drizzle_science.h:176` 自称「**线性恒等**」，但 `drizzle_science.cpp:192-208` 的门有 `rel_tol` 且两侧分开具名判红（`overlap_exceeds_drop` / `overlap_area_deficit`），调用点 `spherical_overlap_science.cpp:120,166` —— **可红**。

*一手文献旁证*：Fruchter & Hook 2002（本人取 arXiv:astro-ph/9808087v2 全文）把 `pixfrac` 定义为「the ratio of the linear size of the drop to the input pixel (**before any adjustment due to the geometric distortion of the camera**)」，并在 §5 明写「**In the case of pixfrac = 1, this correction is exact**」——畸变修正在 `pixfrac<1` 时并非精确。

*复跑*：`sed -n '1160,1190p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`；`sed -n '192,208p' lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp`

*建议改法*：`:169` 把「构造性的、不依赖数值逼近」改为「在像元内天球面积尺度因子恒定时精确；一般情形下闭合残差由 SIP/投影尺度的逐像元变化决定」，并给出 `rel_tol` 的数值与来源；代码头 `drizzle_science.h:176` 的「线性恒等」同步改写。

---

**【R6-7 · 须修】`docs/science/drizzle/DRIZZLE.md:100-119`｜「唯一性」的推导跳过了一步：比例条件只锁到一参数族**

*依据（本人独立推导）*：令 `w_jp = a_jp/(k·A_pixel,j)`。不变量 2（常量场不调制）要求 `A_pixel,j·w_jp ∝ a_jp`，该条件对**任意 k 自动成立**（k 只改公共比例，分子分母同步）。不变量 1（`Σ_p F_p = Σ_j x_j`）要求 `Σ_p w_jp = 1`，而 `Σ_p w_jp = A_drop,j/(k·A_pixel,j) = pixfrac²/k`，故 `k = pixfrac²`。
⇒ **文档 `:108` 声称的「充要条件」只是不变量 2 的充要条件（解空间是一参数族），`:119` 的「唯一配对」结论依赖文档从未写出的第二步**。结论本身正确，但论证链缺一环，而 `:119` 正是「唯一合法核」的定案处。

*建议改法*：`:108-119` 补上 `k = pixfrac²` 由不变量 1 钉死这一步。

---

**【R6-8 · 须修】`docs/science/drizzle/DRIZZLE.md:151` + `:212`｜缩放律在字面读法下恒红**

*依据（本人推导）*：`:151`「缩放律：`x → α·x` 时 `variance → α²·variance`，**因为 `variance_p` 只依赖 `v_j` 与组合系数，与 `x` 无关**」。两句互相拆台：若 `variance_p` 与 `x` 无关，则只缩放 `x` 时 `variance` 不变，不等于 `α²·variance`（α≠1）。`:212` 的「缩放律门 | `x → αx` 时 `variance → α²·variance` | 红：任何破坏线性性的注入」按字面读法**恒红**。正确的门须写明 `v_j → α²·v_j` 同步进行。

*建议改法*：`:151`/`:212` 写明「`(x, v)` 联合缩放 `(x,v) → (αx, α²v)` 时 `variance → α²·variance`」。

---

**【R6-9 · 阻断】`docs/science/resample/RESAMPLE.md:261` + `:112` + `:44`｜「常量场不变性」与行归一算子 `R_ij = a_ij/Ω'_i` 在 coverage<1 时互斥**

*依据（本人独立推导 + 代码）*：代入 `R_ij = a_ij/Ω'_i`、常量场 `x_j = B`：

```
y_i = Σ_j R_ij·B = B·(Σ_j a_ij)/Ω'_i = B·coverage_i
```

本人实测：`coverage = 0.95` 时 `y/B = 0.95`；`coverage = 0.25` 时 `y/B = 0.25`。⇒ **常量场按覆盖率衰减**。`Σ_j R_ij = coverage_i`（行和 = 覆盖率，≤1），因此**算子层输出不是凸组合**，`:44`「输出是输入的凸组合」在算子层不成立，且 `:1` 声称的「两层的口径必须一致」被违反（叶级 `Σc'_k = 1`，算子级 `Σ_j R_ij = coverage`）。

*生产代码确认*：`lib/algorithms/resample/p3_rsmp_operator.cpp:130` `op.weight.push_back(a / n.omega_out_sr);  // R_ij = a_ij / Omega'_i` —— **未重归一**；`:133` `op.coverage[...] = area / n.omega_out_sr`。

*跨篇互斥（本人独立发现，RESAMPLE 车道 8-1 独立同证）*：`DRIZZLE.md:14` 不变量 2「常数面亮度场的输入，输出恰为 `B₀`」在 DRIZZLE 的**覆盖面积归一**下成立（`DRIZZLE.md:125` `S_p = Σ_j B_j a_jp / Σ_j a_jp`，分母是实际交叠 `D_p` 而非 `Ω'_p`）；RESAMPLE 的**输出面积归一**下不成立。两册同属 P3 链、共用 `C_out = R C_in Rᵀ`，却对常量场给出**相反**结论。

*建议改法*：`RESAMPLE.md:261` 的判据加前提「`coverage_i = 1`」；`:44` 把凸组合限定为叶级；`:1` 改为列出两层的归一口径差异；`:112` 的行内括号式删掉（见 R6-10）；`DRIZZLE.md` 与 `RESAMPLE.md` 各加一条指向对方的显式对照。

---

**【R6-10 · 须修】`docs/science/resample/RESAMPLE.md:112`｜行内括注式算出的是别的东西，且与 9 行后的 `:121` 对撞**

*依据（本人独立计算）*：`R_ij` 已定义为 `a_ij/Ω'_i`，故 `Σ_i R_ij·(a_ij/Ω'_i) = Σ_i a_ij²/Ω'_i²`。本人构造 `Ω'_i=2.0`、`a=[0.7,0.6,0.4,0.2]`：`coverage_i = 0.95`，而该括号式 = `0.2625` ⇒ 不等。正确式为 `:121` 的 `coverage_i = Σ_j a_ij/Ω'_i`。

*建议改法*：`:112` 行内括注删去或改写为 `Σ_j R_ij = coverage_i`。

---

**【R6-11 · 阻断】`docs/science/resample/RESAMPLE.md:258` + `:209` 代码｜`Σπ_i = 1` 硬门在 PSF 支撑未完整落入输出视场时恒红**

*依据（本人独立推导 + 代码）*：`Σ_i π_i = Σ_j p_j·γ_j`，其中 `γ_j := Σ_i S_ij = (输入像元 j 落在输出视场内的立体角)/Ω_j ∈ [0,1]`，`γ_j = 1 ⟺ j 被输出视场完整包含`。本人构造：某叶只被输出视场覆盖 `0.1/1.0` ⇒ `γ_j = 0.1`。
*生产代码是硬拒*：`lib/algorithms/resample/p3_rsmp_propagation.cpp:209` `if (!(std::fabs(pi_sum - 1.0) <= cfg.psf_sum_tol)) { out.status = Status::Reject; out.code = "G-P3-PSF-02"; … }`（另 `p3_rsmp_failclosed.cpp:139` 同样）。
⇒ **任何亮星靠近输出视场边缘、或输出视场边界切过输入叶边界，该门必红**，而 `:163`、`:175`、`:258` 三处均未写「输出视场完整包含有效 PSF 全部支撑」这一前提。

*建议改法*：三处同补前提；或改为「按覆盖块截断后归一」并登记「源落在视场边缘」降级态。

---

**【R6-12 · 须修】`docs/science/resample/RESAMPLE.md:133` + `:259`｜「`C_y` 正定」未区分 PSD/PD，`C_y⁻¹` 在奇异时无定义**

*依据（本人独立推导）*：`C_y = R C_x Rᵀ`，`R` 为 `(n_out × n_in)` ⇒ `rank(C_y) ≤ min(rank R, rank C_x) ≤ min(n_out, n_in)`。当 `n_out > n_in`（导出网格比输入 HiPS 更细 —— 这是 export 的**常规**情形）时 `C_y` **构造性奇异**，Cholesky 必失败。
*生产代码*：`lib/algorithms/resample/p3_rsmp_covariance.cpp:129-133` `if (!f.ok) … return Status::Reject;  // 非正定/奇异 → fail-closed，禁伪逆静默`。另零覆盖输出像元使 `R` 该行全零 ⇒ `C_y` 该行/列全零 ⇒ 同样奇异。
⇒ `:259`「协方差可解性 | `C_y` 正定，Cholesky 成功；不通过即 fail-closed」按字面读法在标准路径下**恒红**。而 `:3.7` 又要 `C_y⁻¹`，奇异时无定义。文档全文只 `:135` 一句「不使用伪逆静默通过」，无秩亏判据、无截断到有覆盖子块的路径。

*建议改法*：`:133`/`:259` 分三层写：PD 正常 / 零覆盖造成 PSD ⇒ 逐像元不可用 + 截断求解 / 数值秩亏 ⇒ fail-closed + 秩亏判据。

---

**【R6-13 · 须修】`docs/science/resample/RESAMPLE.md:65`｜闭式与 min 形式不等价，缺下界钳位**

*依据（本人独立计算）*：`:61` 的 min 形式定义域 `k ∈ [0, K_max]`；`:65` 的闭式只写「被上界 `K_max` 钳位」。本人实测（W=512）：`s_out = 10 rad/px` ⇒ min 形式给 `K = 0`，闭式给 `⌈log₂(1.0233/(512·10))⌉ = −12`；`s_out = 1` ⇒ `0` vs `−8`。分歧条件 `s_out > 2√(π/3)/W = 825.06″/px`。
*建议改法*：`:65` 改为 `K = clamp(⌈log₂(√(π/3)/(W·s_out))⌉, 0, K_max)`。

---

**【R6-14 · 须修】`docs/science/resample/RESAMPLE.md:212`｜二维误差界默认 `h_x = h_y`，且适用域缺三处声明**

*依据（本人独立推导）*：一维 `|f−L| ≤ (h²/8)max|f''|` 正确（`|（x−x_i)(x−x_{i+1}）| ≤ h²/4`）。二维式写成 `(h²/8)(max|F_xx| + max|F_yy|)`，仅在 `h_x = h_y` 时成立；一般应为 `(h_x²/8)max|F_xx| + (h_y²/8)max|F_yy|`。而 `:67` 自认「HEALPix 单元的局部采样步长随纬度与方向变化」。
另两处适用域未写：① 该界对**点采样**成立，而本仓输入是叶面积平均；② `:75` 明说在**切平面**上取象限，`F_xx/F_yy` 是图上导数还是球面导数、`h` 在哪个空间度量，未交代。

*建议改法*：`:212` 改双 `h` 形式；补「点采样 vs 面积平均」「导数归属与 chart Jacobian」两处适用域。

---

### 第 7 轮 · 负向轮（需归零/报警的场景是否有处理与负例；判据字面读法是否恒成立/恒不成立）

---

**【R7-1 · 阻断】`docs/science/psf/PSF.md:172`｜「FWHM 缩放不变量」字面读法下恒成立或恒不成立，取决于从哪个量取数**

*依据（本人独立计算）*：

- **读法 A（从闭式解析式取数）**：`FWHM/σ ≡ 1.2303076525901024` 对任意 `σ, A, B` 成立 ⇒ **恒成立，零判别力**（它是定义恒等式）。
- **读法 B（从生产输出取数）**：生产 `fwhm_x = MOFFAT4_FWHM_FACTOR·sx`，`dpsf_psf.cpp:173` `MOFFAT4_FWHM_FACTOR = 1.230310` ⇒ `fwhm_x/sx ≡ 1.230310`，与 `:172` 写的值差 `+1.9079861e−06` ⇒ **恒不成立，永远红**。

而 `:90` 已承认这个 `1.91×10⁻⁶` 并称「远低于任何取用该量的判据容差」，却在 `:172` 把它写成**无容差的精确等式**。⇒ **判据的隐含容差被迫落在 `1.91×10⁻⁶`（生产读法）与 `0.277 vs 0.5`（跨口径读法）之间，文档未落任何数字。**

*附带*：`:172` 同一句的「反解出的 FWHM 只有 `2.12·σ`」**本人复算不出**（尝试了七种自然读法：圆窗反解给 `1.23031σ`、两因子相乘给 `2.14063σ`、`f_true·√3` 给 `2.13096σ`，均不等于 2.12）。按红线**如实登记「推不出」，不凭印象改数**。

*复跑*：`python3 -c "import math;print(2*math.sqrt(2)*math.sqrt(2**0.25-1))"`；`sed -n '173p' lib/algorithms/psf/src/dpsf_psf.cpp`

*建议改法*：`:172` 拆成三条各自可判、各自带容差数值的检验；`2.12·σ` 删除或补出明示路径后再写。

---

**【R7-2 · 阻断】`docs/science/psf/PSF.md:173`｜「旋转简并」判据字面读法下恒不成立**（即 R6-1 的负向轮结论）

见 R6-1。三条候选给出三个不同图像，判据按字面实现**永远红**，而它被列在「结构性不变量」下。

---

**【R7-3 · 阻断】`docs/science/resample/RESAMPLE.md:261`｜「协方差可解性」「Σπ=1」「常量场不变性」三条硬门在标准路径下恒红 ⇒ 点源通量模式整体不可达**

即 R6-11 + R6-12 + R6-9。三条门全部 fail-closed，叠加零覆盖像元触发整帧拒绝 ⇒ 点源通量模式在生产中无正例。按 `docs/ACSD_DESIGN.md:497`「每个度量具备非退化判据，恒真门没有证据资格」，**恒红门同样没有证据资格**。

---

**【R7-4 · 须修】`docs/science/resample/RESAMPLE.md:232`｜「算子行和容差 `1e-9` / `row_sum_tol`」是死常量，文档把它写成生产容差**

*依据（本人实测）*：`grep -rn "row_sum_tol" lib/ eng/` **只命中定义处** `lib/algorithms/resample/p3_rsmp.h:321`，全仓零使用。同理 `p3_rsmp.h:221-223` 的 `row_sums()` / `col_sums()` / `full_coverage()` **全仓零调用**（实测 `grep -rn "row_sums\|col_sums\|full_coverage" lib/ eng/` 只命中声明与定义）。
且判据口径本身不成立：`Σ_j R_ij = coverage_i ≤ 1`，**天然达不到 1**，用 `1e-9` 卡它必然恒红。

*建议改法*：删该行，或改为自洽性门 `|Σ_j R_ij − coverage_i| ≤ row_sum_tol` 并写明 coverage 取值域，同时把 `row_sums()`/`full_coverage()` 接进判据或删除。

---

**【R7-5 · 须修】`docs/science/resample/RESAMPLE.md:229` + `:217`｜配置 token `bilinear` 与注册表 id 对不上；三处默认互不相同**

*依据（本人实测 + RESAMPLE 车道 7-3 独立同证）*：schema `eng/contracts/schemas/phase_config_export.schema.json` 的 `"sampler": {"enum": ["nearest", "bilinear"], …缺省 bilinear}`；而注册表 `lib/algorithms/resample/p3_rsmp_kernel_registry.cpp` 的 id 只有 `nearest` / `bilinear_4quad` / `bilinear_area_overlap_exact` / `bicubic` / `lanczos` —— **`grep -rn '"bilinear"' lib/algorithms/resample/` 零命中**。若把 `bilinear` 直接送 `admit()`：`p3_rsmp_kernel_registry.cpp:112-114` 返 `Reject / G-P3-KRN-01 "unknown_kernel_id:"` ⇒ **缺省值本身会被拒**。
三处默认互不相同：science 文档 `:219/:229` 说 `bilinear`；实现文档 `lib/algorithms/resample/PHASE3_RSMP_IMPL.md:26` 说「生产科学默认 = `bilinear_area_overlap_exact`」；代码 `lib/phase3_session/p3_export.h:159` `std::string kernel_id = "bilinear_4quad"`。`DRIZZLE.md:217`/`:219` 同样并列三个 id 而未给映射。

*建议改法*：`:217` 增 token→id 映射表并声明唯一缺省；`:4` 表与 schema description 同步指向映射。

---

**【R7-6 · 须修】`docs/science/resample/RESAMPLE.md:193`｜`nearest` 的准入结果与配置面互相矛盾，规则对配置值零强制力**

*依据（本人实测 + RESAMPLE 车道 7-4 转引）*：注册表 `nearest` 的 allowed_uses 只含 `DiscreteMask/Diagnostics/ExplicitUserSelection`，`admit()` 对连续场必拒（`p3_rsmp_kernel_registry.cpp:124`）。而 `:219`/`:229` 把 `nearest` 列为配置可选值。实际生产：`lib/phase3_session/p3_export.cpp:539-540` 在可视化模式下把 kernel **硬编码为 `"nearest"`**（配置值被忽略），其余模式用 `in.kernel_id`（默认 `bilinear_4quad`）——**配置里的 `sampler` token 根本没有进入 `admit()`**（`lib/phase3_session/p3_session.cpp:116/155/289/306` 只用它选 1 还是 4 样本）。
⇒ 用户写 `sampler:"nearest"` + `surface_brightness` 不被拒，违反 `:193` 的准入规则；反过来 `:193` 对用户零强制力。

*建议改法*：`:193` 补「配置 token 先经映射再进 `admit()`」；补配置→`kernel_id` 的赋值路径或在 `:219` 声明 `nearest` 仅可视化可用。

---

**【R7-7 · 须修】`docs/science/drizzle/DRIZZLE.md:210`｜通量守恒门的容差写成「在双精度内」，按字面读法恒红**

*依据（本人实测 + 实验单元读数）*：`:210`「Σ_p Σ_j x_j w_jp = Σ_j x_j，**相对闭合在双精度内**」。双精度 eps = `2.22e−16`。而实验单元 `实验/healpix-polar/REPORT_experiment.md:111` 实测跨叶 drop `Σ_p w_jp − 1 = 6.084e−11`（地板 `1.95e−09`）、`:115` 跨面界 `1.75e−10`（地板 `9.74e−10`）。⇒ **实测值比「双精度」大 5–6 个数量级，按字面读法恒红。** 文档未给任何容差数值。

*建议改法*：`:210` 给具体容差（可引实验地板 `1.95e−09` / `9.74e−10`）并注明适用域。

---

**【R7-8 · 须修】`docs/science/drizzle/DRIZZLE.md:75` + `:124`｜把 `Σ_p w_jp = 1`、`Σ_p F_p = Σ_j x_j` 写成无条件恒等式，实验单元已限定适用域**

*依据（本人实测读数）*：`实验/healpix-polar/REPORT_paper.md:240`「在**整叶内**（15126 个 drop）权重和逐位为 1，**跨叶 10474 个 drop 上为 6.08e−11**、**跨面构造用例的 Σ_p w_jp − 1 最大值 1.75e−10** ⇒ **「权重和逐位为 1」的适用域是同一叶内的 drop 分割，不是全域无条件成立**」。
`DRIZZLE.md:75` 的括注「Σ_p w_jp = 1」与 `:124` 的「Σ_p F_p = Σ_j x_j」**未带该限定**。

*建议改法*：`:75`/`:124` 补「同一叶内逐位成立；跨叶/跨面为 O(1e−10) 量级，容差见正确性判据」。

---

**【R7-10 · 阻断 · 本车道最重的一条】`docs/science/drizzle/DRIZZLE.md:208` + `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp:61-63`｜几何闭合门在生产路径上恒真（恒真门①）**

*依据（本人亲自复跑三处代码，链条闭合）*：

```
lib/algorithms/integration/phase1_product/src/phase1_product.cpp:507
    s.A_pixel = 0.0;  /* 由球面 drop 面积反推 */
        ↓
lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp:61-63
    out->A_pixel = (src.A_pixel > 0.0) ? src.A_pixel
                                       : out->drop_area/(src.pixfrac*src.pixfrac);
        ↓
lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp:198
    const double expected = pixfrac * pixfrac * A_pixel_j;      // ≡ drop_area
        ↓
lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp:199
    const double rel = (sum_a_jp - expected) / expected;
```

⇒ 生产路径下 `pixfrac²·A_pixel_j` **恒等于 `drop_area`**，于是闭合门**只**在检验 `Σ_p a_jp` 对 `A_drop,j`，而 `DRIZZLE.md:166`/`:208` 声称它检验的是 `Σ_p a_jp = pixfrac²·A_pixel,j`。**该门在 `A_drop,j = pixfrac²·A_pixel,j` 这一支上恒等于 0 —— 字面读法下恒成立，零判别力。**

*与生产热路径自相矛盾（更重的一层）*：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1318-1319` 明写

```
// 禁止用 A_drop/pixfrac² 近似替代: 该恒等式有 O(θ²) 球面非线性残差
// (审核实测 8.3e-7 @2"/px)。
```

且 `:1320-1326` 用**未收缩四角独立**求 `pixel_area`。⇒ **生产热路径明令禁止的近似，正是生产门路径所依赖的假设**。两条路径对同一关系给出相反处理。

*残差 δ 的独立复算（DRIZZLE 车道 B'，本车道采信其复现结论）*：B' 自建 gnomonic + Van Oosterom（`/tmp/dz_audit/drop_ratio3.py`）**逐位复现** `docs/science/algorithms/DRIZZLE_GEOMETRY.md:64-69,407-421` 的闭式：纯 TAN 无畸变、切点、`pixfrac=0.8` 时 `δ = (1−pixfrac²)·0.25·θ² = 0.09·θ²(rad)`；`θ=2″→8.46e−12`、`60″→7.62e−9`、`300″→1.904e−7`，且严格按 `θ²` 缩放（300″/60″ 比值 = 25.0）。
**关键含义**：`δ` **在完全没有 SIP 畸变时也非零** ⇒ `DRIZZLE.md:42` 用「`pixfrac` 是畸变修正**之前**的线性尺寸比」来支撑 `A_drop,j = pixfrac²·A_pixel,j` 的**精确**性，推理无效。

*一手文献独立反证（本人已取 F&H 全文）*：F&H §5 原句「By scaling the weights of the input pixels by their areal overlap with the output pixel … Drizzle largely removes this effect. **In the case of pixfrac = 1, this correction is exact.**」——**该文自己说只在 `pixfrac=1` 精确**，并在 `pixfrac=0.6` 实测残余 RMS `0.004 mag`。

*违反的条款*：`docs/ACSD_DESIGN.md:497`「每个度量具备非退化判据，**恒真门没有证据资格**」。

*复跑*：
```bash
cd "/workspace/Astro CS Database"
sed -n '504,508p'  lib/algorithms/integration/phase1_product/src/phase1_product.cpp
sed -n '59,66p'    lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp
sed -n '196,207p'  lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp
sed -n '1316,1327p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
```

*建议改法*：几何闭合门**两级化**并拆开两支——L1 构造级 `Σ_p a_jp = A_drop,j`（两侧超额/亏损分别具名，主判据）；L2 面积比级 `A_drop,j/(pixfrac²·A_pixel,j) − 1 = δ`，按 `δ ≈ (1−pixfrac²)·θ²/4` 标度单列进 §5.2 误差预算，**不与 L1 同门**；并**禁止 `A_pixel` 由 `drop_area` 反推**（`phase1_product.cpp:506` 必须改由未收缩四角独立求值）。文档 `:169` 的「构造性的、不依赖数值逼近」只对 L1 成立，应限定。

---

**【R7-9 · 须修】`docs/science/psf/PSF.md:112-118` → §5.2 失效域表｜截断域（`f_out` 大）没有任何报警或降级动作**

*依据（本人实测代码）*：`:118` 说「该域下发布通量的正确语义是**窗内积分通量**而不是整平面延伸值」，但 §5.2 失效域表（`:179-187`）**没有对应行**；`dpsf_psf.cpp:652` 无条件输出全平面 `flux = 2πA·sx·sy/3`，不产出任何标记，消费方无从知道 `flux` 处于降级语义。⇒ 负向轮的「需报警场景有处理与负例」不满足。

*建议改法*：§5.2 增一行 —— `r_win/σ` 超过阈值时产出显式标记或拒判，并写明阈值（阈值本身见 R6-3）。

---

### 第 8 轮 · 一致性轮（文档↔文档、文档↔代码、文档↔实验）

---

**【R8-1 · 阻断】`docs/ACSD_DESIGN.md:118-119`（最高设计 §2.3 P3）与 `docs/science/drizzle/DRIZZLE.md:52,169,171,195-197` 口径相反**

*依据（本人独立读出双方原文 + 代码）*：

| | 最高设计 §2.3（`:118-119`） | DRIZZLE.md + 生产代码 |
|---|---|---|
| 坐标域 | 「以 12 个 HEALPix 基面的**等面积 chart** 为坐标域，每个面是单位正方形、Jacobian 恒为 π/3，leaf 在 chart 内**轴对齐**」 | 球面立体角；叶边界是**非测地线曲线**（`:52`），自适应细分逼近 |
| 交叠算法 | 「退化为二维轴对齐裁剪与**鞋带公式**」 | `Van Oosterom & Strackee` 有向立体角 `Ω = 2·atan2(det, 1+a·b+b·c+c·a)`（`spherical_overlap.h:15,81,261`），代码注释明写「实现与取代 **Girard** 的理由」 |
| 闭合 | 「构造闭合由裁剪与鞋带的代数结构直接成立…**不依赖数值逼近**」 | `:169` 同称「构造性的，不依赖数值逼近」，但 `:195` 承认叶边弦亏缺 `0.1043885/nside²`、`:196` 承认自适应细分最大深度 `12` |
| 超越函数 | 「候选判定只用叉积、点积与比较，**无超越函数**」 | `atan2` + 自适应细分 |

`docs/science/algorithms/DRIZZLE_GEOMETRY.md:191` 亦写「每 source leaf 构造球面 footprint（边界自适应细分）」，并显式把科学定义让给 `DRIZZLE.md`。⇒ **三方（最高设计 / 一级正本 / 算法正本 / 生产代码）中只有一级正本与代码同侧；按权威链应以最高设计为准，但最高设计的描述与生产实现不可调和。**

*这是本车道唯一需要负责人裁决的顶层项。*

*复跑*：`sed -n '115,123p' docs/ACSD_DESIGN.md`；`grep -n "atan2\|Van Oosterom\|Girard" lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h`

*建议改法*：由负责人裁定以哪一侧为准。若以最高设计为准，则 `DRIZZLE.md` 须整章改写几何口径且生产代码需换实现；若以实现为准，则改最高设计 §2.3。**两者不可并存。**

---

**【R8-2 · 须修】`docs/DOCUMENT_INDEX.yaml:783`｜PSF 条目的 notes 含坏路径**

*依据（本人实测）*：notes 末句「psf 相关配置的模型取值权威见 `docs/engineering/contracts/CONFIG`」，`test -e docs/engineering/contracts/CONFIG` → **MISSING**；真实文件是 `docs/engineering/contracts/CONFIG.md`。
*（本条属 `docs/DOCUMENT_INDEX.yaml`，不在本车道可写面，仅报告。）*

---

**【R8-3 · 须修】`docs/science/drizzle/DRIZZLE.md:200`｜外接半径实测值与实验台账冲突，且「裕量 20% 以上」按自身数值不成立**

*依据（本人独立计算 + 实验台账）*：`:200`「极冠内的实测最坏值约 `1.044·hp_res`…取 `1.25` 同时覆盖两者与浮点舍入，**裕量在 20% 以上**」。
本人实测：`1.25/1.044 = 1.197318` ⇒ 裕量 **19.73% < 20%**，**文档的断言按自身数值即不成立**。
而实验单元 `实验/healpix-polar/REPORT_paper.md:14` 的全天穷举值是 **sup ≈ `1.0415·hp_res`**，`1.25/1.0415 = 1.200192` ⇒ 裕量 20.02%，与「20% 以上」同侧。⇒ **正本用了旧扫描值（1.044），实验台账用新穷举值（1.0415），且正本由旧值推出的裕量断言自相矛盾。**
`:200` 给的赤纬 ±41.8° 与 `实验/healpix-polar/docs/EXP-07-POLAR.md:164,423` 的 `dec = ±41.810315°` 一致（该处为三角点）。*（`1.25` 与实验台账同侧 ⇒ 本条方向明确，是订正值不是改设计。）*

*复跑*：`python3 -c "print(1.25/1.044, 1.25/1.0415)"`

---

**【R8-4 · 阻断】`docs/science/resample/RESAMPLE.md` 与 `docs/science/drizzle/DRIZZLE.md` 的常量场归一口径互斥**（即 R6-9 的跨篇部分）

已由 RESAMPLE 车道 8-1 独立同证。本车道已亲自重推两侧（`DRIZZLE.md:125` 的 `S_p = Σ_j B_j a_jp/Σ_j a_jp` 与 `RESAMPLE.md:130` 的 `R_ij = a_ij/Ω'_i`），并用同一夹具对照运行：DRIZZLE 口径下常量场 `B₀=2.0` 输出 `2.0`（不衰减），RESAMPLE 口径下输出 `2.0·coverage`（衰减）。

---

**【R8-5 · 须修】`docs/science/psf/PSF.md:125,160`（ADU/pixel）vs `docs/science/algorithms/STAR_PSF_ALGORITHMS.md:20,44`（ADU）｜`residual_scale` 单位跨文档冲突**

*依据（本人实测）*：`sed -n '125p;160p' docs/science/psf/PSF.md` ⇒ `ADU/pixel`；`sed -n '20p;44p' docs/science/algorithms/STAR_PSF_ALGORITHMS.md` ⇒ `residual_scale=ADU`。而 `PSF.md:164` 用「分子分母同为 ADU/pixel，量纲相消」论证 `q_psf` 无量纲 —— 按算法分册的单位（`A` 是 ADU/pixel、`residual_scale` 是 ADU）该比值是 `1/pixel`，不量纲相消。⇒ **两条口径不能同时成立。**

---

**【R8-6 · 须修】三册迁移留下 34 份文件指向已不存在的旧路径**

*依据（本人实测）*：`docs/science/{PSF,DRIZZLE,RESAMPLE}.md` 三个旧路径 `test -e` 全部 **GONE**，新路径全部 EXISTS。`grep -rIl "docs/science/DRIZZLE\.md\|docs/science/PSF\.md\|docs/science/RESAMPLE\.md" docs/ lib/ eng/ 实验/` 命中 **34 份文件**，含 **生产源码**：
`lib/algorithms/drizzle/healpix_drizzle/{drizzle_engine.cpp, spherical_overlap.h, astro_sphere_sink.h, astro_sphere_sink.cpp}`、`lib/algorithms/psf/module.yaml`、`lib/algorithms/psf/README.md`、`lib/algorithms/drizzle/README.md`、`lib/algorithms/drizzle/hips/README.md`、`eng/tools/quality/v19r3_traceability.py`，以及 `实验/healpix-polar/REPORT_paper.md`、`实验/dense-snr-reconstruct/REPORT_{paper,experiment}.md`、`实验/absolute-snr/**` 等。
⇒ 三册自身无自指旧路径（实测 `grep` 为空），但**迁移只做了一半**。

*复跑*：`grep -rIl "docs/science/DRIZZLE\.md\|docs/science/PSF\.md\|docs/science/RESAMPLE\.md" docs/ lib/ eng/ 实验/ | wc -l`

---

**【R8-7 · 须修】`docs/science/drizzle/DRIZZLE.md:95`｜`c_jp` 被标为「无量纲」，与同文单位表冲突**

*依据（本人独立量纲核算）*：`c_jp = w_jp/N_p`，`w_jp` 无量纲而 `N_p` 单位 sr（`:30,35`）⇒ `c_jp` 单位 **sr⁻¹**。核对 `:33` 的 `variance_p` 单位 ADU²/sr²：`variance_p = Σ_j c_jp²·v_j`（`:144`），`v_j` 是 ADU²，`c_jp²` 是 sr⁻² ⇒ ADU²/sr² ✓ **只有 `c_jp` 带 sr⁻¹ 才自洽**。`:95` 标「无量纲」会让读 `:144` 的人得到 ADU²，与 `:33` 矛盾。
*（`:146` 的另一支 `Σ_j v_j w_jp²/N_p²` 自洽，故两支只有一支在 `c_jp` 无量纲下成立。）*

---

**【R8-8 · 须修】`docs/science/drizzle/DRIZZLE.md:189`｜`pixfrac` 数值默认的权威归属与 schema 自述冲突**

*依据（本人实测 + 交叉车道转引）*：`:189` 声明「数值默认 `0.8`…值域由 `eng/contracts/schemas/phase_config_normalize.schema.json` 强制」。schema 确实强制**值域**（`exclusiveMinimum: 0, maximum: 1`），但同一节点的 description 写「数值默认未冻结（defaults.json 的 `drizzle.pixfrac` 为 `pending_authority`），本字段不声明数值默认」，而 `eng/packaging/config/defaults.json` 的 `drizzle.pixfrac` 已是 `owner_adjudicated`。
⇒ 属**三处口径不一致**（科学正本 / schema description / defaults.json）。*（schema 与 defaults.json 不在本车道可写面，仅报告；仓内治理台账已有同类登记。）*

---

**【R8-9 · 建议】`docs/science/drizzle/DRIZZLE.md:131`（`support ∈ [0,1]`，0 表示无覆盖）vs `:225`（「零合格样本时输出 `NaN` 且 `support ≤ 0`）」**

`:225` 的 `support ≤ 0` 允许负值，与 `:131` 的值域 `[0,1]` 冲突。*建议*：`:225` 改为 `support = 0`。

---

### 第 9 轮 · 语言轮（正向书写、无历史/版本/日期/裁判语言、无机械锚）

---

**【R9-1 · 须修 · 红线】`docs/science/resample/RESAMPLE.md:130`｜全文唯一的机械跳转锚**

*依据（本人实测 + 交叉车道独立发现）*：`:130` 写 `C_y = S · C_d · Sᵀ （点源通量模式，列归一算子，**见 3.7**）`。`AGENTS.md:68` 禁「见 §几」式机械跳转锚。
⚠️ **自查更正**：本车道首遍用正则 `见 §|见第|§[0-9]|如上所述|…` 扫描，**漏掉了这一处**（该串无 `§`）。是跨文档车道用更宽的 `见 ?[0-9]+(\.[0-9]+)*` 正则独立抓到的 —— **这是本车道「子代理补到我」的实据**。
*复跑*：`grep -nE "见 ?[0-9]+(\.[0-9]+)*|第 ?[0-9]+ ?(章|节)|§" docs/science/psf/PSF.md docs/science/drizzle/DRIZZLE.md docs/science/resample/RESAMPLE.md`
*建议改法*：改为「见「有效 PSF 与输出帧重算」」（与同文 `:265`/`:280` 的合规写法一致）。

---

**【R9-2 · 须修】`docs/science/resample/RESAMPLE.md:229` + `:231`｜§4 常数表两行被未转义的 `|` 破表**

*依据（本人实测）*：`awk -F'|' 'NR>=223 && NR<=234 {print NR, NF-2}' docs/science/resample/RESAMPLE.md` ⇒ 表头与 `:223-228,230,232-234` 均为 **4 格**，而 `:229` 为 **5 格**、`:231` 为 **6 格**。GFM 下反引号代码段**不保护**竖线：
- `:229` 的 `nearest | bilinear` 多切一格；
- `:231` 的 `|Σ π_i − 1| ≤ 1e-9` 多切两格。
⇒ **「采样核取值域」与「有效 PSF 归一门」两个容差/值域在任何 GFM 渲染器里都读不出来**，其余列全部错位。
*复跑*：`awk -F'|' 'NR>=223 && NR<=234 {printf "%d: cells=%d\n", NR, NF-2}' docs/science/resample/RESAMPLE.md`
*建议改法*：`nearest \| bilinear` 与 `\|Σ π_i − 1\|` 转义；`:258` 的同一量写法一并统一。

---

**【R9-3 · 建议】`docs/science/drizzle/DRIZZLE.md:242`｜「两者都**不再**回读原始帧」含时间性措辞**

*依据*：`AGENTS.md:67` 要求正文无历史叙事。现态陈述应写「两者都不回读原始帧」。同形措辞在 `PSF.md:61`「都不再是星核本身的量」亦出现，但后者是数学条件陈述（「不再满足该性质」），**不判为历史叙事**；`DRIZZLE.md:242` 是行为口径，判为措辞建议。*严重度低。*

---

**【R9-4 · 正面（通过项）】三册语言面无实质缺陷**

*依据（本人实测）*：
- 无日期、版本号、任务流水编号、commit 哈希、任务名。
- 唯一的年份（1969/1974/1983/1987/2002/2005/2017）全在书目条目内，规范允许。
- 无「旧版/作废/曾为/改为」历史叙事；`PSF.md:216-223` 的证据等级自陈（Moffat 无 DOI、「全文未逐式核验」、Crossref 逐字核对）是**诚实的证据披露**，符合 `AGENTS.md` §6，不判为历史叙事。
- 无裁判语言（`RESAMPLE.md:133/259/274` 的「通过/静默通过」是 fail-closed 门的语义，不是评价词）。
- 三册互引一律用「见「章节名」」自然语言点名（`DRIZZLE.md:16,251`；`PSF.md:28,48,63,193`；`RESAMPLE.md:265,280`），**除 R9-1 那一处外无机械锚**；经逐条核对，被点名的节名全部真实存在。
- 三份 README 确为「`# <目录名>` + 一句话」，符合 `AGENTS.md` §5「极简 README、一两句说明该文件夹内容」。
- 附：README 文件末尾无换行符（`cat` 输出三份首尾相连）。不影响内容，严重度低，不单列。

---

### 第 10 轮 · 可复现轮（每条结论能由实验或代码复算，复现路径完整）

---

**【R10-1 · 须修】`docs/science/drizzle/DRIZZLE.md:195-200` 的四个几何常数在正本里没有任何复跑入口**

*依据（本人实测）*：`1.25`、赤道带解析上界 `1.007·hp_res`、极冠实测最坏值 `1.044·hp_res`（±41.8°）、弦亏缺律 `0.1043885/nside²` —— 这四个数在 §4 表与 §5.2 都只给结论，**无一给出推导或复跑命令**。`:200` 只说「改变这个因子而不重跑零漏选验证是不允许的」，但「零漏选验证」的入口未写。
仓内证据实际在实验侧：`0.1043885` 见 `实验/healpix-polar/REPORT_experiment.md:59`（`route2/exp02_polar_pixel.json`）、`REPORT_paper.md:79`（`route1/e2_polar_pixel_limit.py`）；外接半径见 `REPORT_paper.md:14`（`route2/exp06_circumradius_margin.py`，值 1.0415）。**正本未指。**
⚠️ `1.044` 与 `1.0415` 的冲突另见 R8-3。`±41.8°` 与实验的 `±41.810315°`（三角点）一致，**不是错误**。

---

**【R10-2 · 须修】`docs/science/drizzle/DRIZZLE.md:206-208` + `:214` + `:210`｜四条正确性判据中至少三条无容差数值**

| 判据 | 行 | 容差 |
|---|---|---|
| 几何闭合 | `:208` | `rel` 的容差未给（代码有 `rel_tol`，值未在文档） |
| 常量面亮度门 | `:209` | `1e-3` ✓ 有 |
| 通量守恒门 | `:210` | 「在双精度内」= 无值，且按字面恒红（R7-7） |
| 协方差门 | `:213` | 「不小于」「严格大于」无数值差 |

*建议改法*：逐条补容差数值与来源（实验地板可作引用）。

---

**【R10-3 · 须修】`docs/science/psf/PSF.md:185`｜背景判据只写「超出容许」，数值只存在于下级文档与代码**

*依据（本人实测）*：`:185` 写「背景项 `B` 与窗口外背景估计的相对偏差超出容许」。生产是 `lib/algorithms/psf/src/dpsf_psf.cpp:627` `if (std::abs(B - bkg0) / bkg_range > 0.5)`，其中 `bkg_range = max(bkg0, 0.01)`。**一级正本缺该数值，下级 `docs/science/algorithms/STAR_PSF_ALGORITHMS.md` 已有 `0.5`** —— 权威倒置。
*复跑*：`sed -n '626,632p' lib/algorithms/psf/src/dpsf_psf.cpp`
*建议改法*：`:185` 写死 `|B − bkg0| / max(bkg0, 0.01) > 0.5`。

---

**【R10-4 · 须修】`docs/science/psf/PSF.md:172` 的 `1.7399178387` 第 8 位起错**

*依据（本人独立计算）*：`4·√(2^{1/4}−1) = 1.739917768184329`，文档写 `1.7399178387`，相对差 `4.05e−08`。作为判据常量给出 10 位有效数字却第 8 位错。*复跑*：`python3 -c "import math;print(repr(4*math.sqrt(2**0.25-1)))"`。同一句的 `2.12·σ` 见 R7-1（**推不出**）。

---

**【R10-5 · 正面（通过项）】PSF.md §7.3 的自包含复算脚本 8 行输出注释逐位一致**

*依据（本人实跑）*：把 `PSF.md:230-270` 的 heredoc 抽出执行，输出 `0.12566134685507402 / 1.6448536269514715 / 0.731673095280613 / 1.2303076525901024 / 1.4826022185056023 / 104.53094872614112 / 104.53125956044437 / 2.9736014332599733e−06`，与 `:243-269` 的全部注释（含精度位数）**逐位吻合**。
本人**另做独立重算**（不依赖该脚本）：`⟨r²⟩ = α²/2`（Beta 换元）、FWHM 因子、`detM = 1/(4sx²sy²)`（三个 θ 上相对误差 `1.76e−16`）、`∬ = 2πAsxsy/3`、截尾均值因子 `0.731673095280613`、MAD 因子 `1.4826022185056023` —— **全部通过**。

---

**【R10-6 · 正面（通过项）】三册 §7 点名的 29 个参考实现文件全部在树，零悬空指针**

*依据（本人逐个 `test -e`）*：PSF.md §7.4 的 5 个、DRIZZLE.md §7.3 的 12 个 + 2 个 schema + 2 个实验入口、RESAMPLE.md §4/§7.2 的 12 个 —— **29/29 EXISTS**。
*抽查实现事实（本人逐条比对）*：`dpsf_psf.cpp:173` `MOFFAT4_FWHM_FACTOR = 1.230310` ✓；`:609` `A<=0 || sx<=0.3 || sy<=0.3` ✓ 对 `PSF.md:162`；`:619` `fwhm_x > rw` ✓ 对 `:184`；`:652` `flux = 2πAsxsy/3` ✓ 对 `:159`；`:654` `e = sqrt(1−(s_min/s_max)²)` ✓ 对 `:158`。
`lib/algorithms/shared/healpix/healpix_core.cpp:327-331` `pixel_resolution_arcsec = sqrt(4π/(12n²))·(180·3600/π)` —— 与 `RESAMPLE.md:228` 的 `√(π/3)·(180/π)·3600 = 211076.28514206142` **实测相对差 0.0**（本人算得 `211076.28514206142`，逐位一致）。
`lib/algorithms/resample/p3_resample.cpp:41` `kTileWidth = 512` ✓ 对 `RESAMPLE.md:225`。
⚠️ 但 `p3_rsmp.h:320-322` 的三个容差中 `row_sum_tol` 与 `bunit_tol` 是死值（见 R7-4）。

---

**【R10-7 · 正面（通过项）】DRIZZLE.md §7.1 的四处逐字引用独立复核全部成立**

*依据（本人取 arXiv:astro-ph/9808087v2 全文实读）*：
- 「The value of an input pixel is averaged into an output pixel with a weight proportional to the area of overlap between the "drop" and the output pixel.」✓
- 「pixfrac, which is simply the ratio of the linear size of the drop to the input pixel (before any adjustment due to the geometric distortion of the camera). Thus interlacing is equivalent to Drizzle in the limit of pixfrac → 0.0, while shift-and-add is equivalent to pixfrac = 1.0.」✓ 逐字
- 式 (2)(3) 之下「where a factor of s² is introduced to conserve surface intensity」✓ 逐字；且式 (4)(5) 中 `s²` **只乘在值 `I` 上、不乘在权重 `W` 上** ⇒ §7.1 末段「该文献的表面亮度守恒由尺度因子 `s²` 承担，其权重式本身不含 `s²`」**属实**。
- §7.1「Drizzle frequently divides the power from a given input pixel between several output pixels. As a result, the noise in adjacent pixels will be correlated.」✓ 逐字；式 (6)(7) 单输出像元方差、式 (8)–(10) 噪声相关比 `R = σ_c/σ_p`、`r = p/s` ✓ 节号式号全部准确。
- §7.1 末段「该文献不含协方差矩阵产品，其噪声相关比 `R` 是一个标量统计量而非逐对像元的相关系数」✓ **属实**（全文无协方差矩阵）。

---

**【R10-8 · 须修】`docs/science/drizzle/DRIZZLE.md:153`｜噪声相关比 `R` 的闭式漏适用域**

*依据（本人取 F&H 原文）*：`R` 的闭式（式 (9)(10)）只在「dither 图案完全均匀、连续填满输出平面」的极限下成立；原文紧接式 (8) 之后写「While **R must be calculated for any given set of dithers**, there is perhaps one case that is particularly illustrative. When one has many dithers, and these dithers are fairly uniformly placed across the pixel, one can **approximate** the effect of the dither pattern… by assuming that the dither pattern is entirely uniform and continuously fills the output plane.」
`DRIZZLE.md:153`「Drizzle 文献给出的噪声相关比 `R = σ_c/σ_p` 由 `pixfrac` 与 `scale` 的比值决定」**省略了这一适用域**，读起来像无条件成立。

---

**【R10-9 · 正面（通过项）】RESAMPLE.md §4 的角秒常数逐位正确**

*依据（本人独立计算）*：`√(π/3)·(180/π)·3600 = 211076.28514206142`，与 `:228` 的 16 位值**相对差 0.0**；表中 `211076.2851` 是四位小数舍入，相对差 `1.99e−10`。且与实现 `healpix_core.cpp:327` 逐位一致（`4π/12 = π/3`）。
⚠️ **本车道自查更正**：开工时我曾怀疑这个常数有误（怀疑它应为 `211076.7873`），实算后**该怀疑被推翻** —— 记录在此以免后续车道重复怀疑。

---

### 转引自子代理、本车道未独立复核的条目（明确标注强度）

以下条目来自子代理报告，**本车道未逐条复跑**，引用时须按「转引」对待：

| 来源 | 条目 | 车道自陈的复跑入口 |
|---|---|---|
| PSF 车道 A | `PSF.md:72` 的 `⟨r²⟩ = α²/2` 是 β=4 特例，一般式 `α²/(β−2)` | 车道给出 Simpson 复跑脚本。**本人已独立验证一般式**（`/tmp` 脚本，β=3/4/6/10 实测 3.995/2.000/1.000/0.500）⇒ **本条升级为「已复核」，且比车道原述更锋利**：`α/2` 恰为同二阶矩只在 β=6 成立，本仓 β=4 下差 √2。 |
| PSF 车道 A | `PSF.md:192` 的 `σ` 相对标准差 ≈ `residual_scale/(√m·A)` 与 Fisher 差 7.6–22.7 倍，且不随 m 改善 | 未复核。机理可信（`∂I/∂σ ∝ r⁻⁸`，Fisher 饱和）。 |
| PSF 车道 A | `PSF.md:194` 「小样本下截尾均值**系统性低于**高斯渐近值」符号错（MC 显示 m=10,20 处翻正，幅度 −9.3%…+2.3%） | 未复核。 |
| PSF 车道 A | `PSF.md:172` 的 `2.12·σ` 推不出；车道提出「两因子相乘 = 2.1406·σ」为可能读法 | **本人独立尝试七种读法亦全部推不出**，故**不采纳车道的 2.1406 替代值**，维持「推不出」。 |
| PSF 车道 A | `PSF.md:182` 的「无数据」在代码里不存在（`dpsf_psf.cpp:523-528` 返 `DPSF_FIT_INVALID_PARAMS`）；`:183` 漏两条拒判路径（`RECT_TOO_SMALL`、`AMPLITUDE_LE0`） | 未复核。 |
| PSF 车道 A | `PSF.md:164` 的 `q_psf` 在 `docs/science/noise_snr/` 零出现 | **本人已独立验证**：`grep -rIn "q_psf" docs/` 命中全在 `algorithms/` 层，`noise_snr/` 零命中 ⇒ **升级为「已复核」**。 |
| RESAMPLE 车道 C | `bunit_tol` 亦是死值；实际判定是精确整数幂比较（`p3_rsmp_units.cpp:105-112`） | 未复核（本人只复核了 `row_sum_tol`）。 |
| RESAMPLE 车道 C | 配置 `sampler` token 从未进入 `admit()` | **本人已独立验证其后果**（`grep -rn '"bilinear"' lib/algorithms/resample/` 零命中 + `p3_export.h:159` 默认 `bilinear_4quad`）⇒ **升级为「已复核」**。 |
| RESAMPLE 车道 C | `RESAMPLE.md:84` 的「最近叶中心填充」与 `:105` 及代码矛盾 | **本人已独立复核**：`p3_resample.cpp:401-403` `if (!g00||!g10||!g01||!g11) { *value=nanf(""); *coverage=0; return; }`，**无任何填充** ⇒ **升级为「已复核」**，且这是文档内部两节互斥。 |
| RESAMPLE 车道 C | `operator_from_entries_row/col` 全仓零调用，生产路径的 `a_ij = w_k·Ω'_i` 使 `coverage ≡ 1` | **本人已独立复核**：`grep -rn "operator_from_entries" lib/ eng/` 只命中 `p3_rsmp.h:231,233` 声明与 `p3_rsmp_operator.cpp:92,96` 定义，**零调用** ⇒ **升级为「已复核」**。 |
| RESAMPLE 车道 C | `G-P3-QW-FRAME`：任一输出像元无覆盖即整帧拒绝（`p3_rsmp_propagation.cpp:169-176`） | 未复核。 |
| RESAMPLE 车道 C | 核注册表 Oracle 常数（`max_interp_err = 0.027395522883651657` 等）无仓内复现脚本 | 未复核。 |
| RESAMPLE 车道 C | 文献著录硬错：`"Hansen K. K."` → Crossref/arXiv 均为 **`F. K. Hansen`**；`"Bartelman"` vs Crossref `"Bartelmann"` | 未复核。属第 5 轮（引用轮）范围。 |
| 跨文档车道 D | `eng/packaging/config/defaults.json` 对三册的 sha256+quote 锚**全部失效**（quote 已不在现行文中，sha256 与声明值自洽） | 未复核。 |
| 跨文档车道 D | `docs/science/algorithms/PHASE3_RESAMPLE.md:85`/`:194` 两处坏路径；`HEALPIX_MAPPING.md:3` 上游误标 | 未复核（不在本片）。 |
| 跨文档车道 D | `PSF.md:227` 声称脚本覆盖「全部闭式数值」但不覆盖 §3.1/§3.4/§5.1 三处 | **本人已确认脚本只含 8 行 print**，确未覆盖这三处 ⇒ **升级为「已复核」**。 |

### 本车道否决 / 降级的子代理条目（写明否决了什么）

| 子代理条目 | 本车道裁决 | 理由 |
|---|---|---|
| PSF 车道 A 的 R6-1「`σ_g = α/√(2(β−2))`」 | **否决其一般式，保留其 β 特例结论** | 「同二阶矩高斯」按定义是 `σ_g = √⟨r²⟩ = α/√(β−2)`，**对任意 β 都恒等于本册的 σ**；`α/√(2(β−2))` 是另一条构造，不是二阶矩匹配。本车道据 R6-2 给出更锋利的表述。 |
| PSF 车道 A 的 R7-1「`2.12·σ` 应改为 `2.1406·σ`」 | **否决替代值，维持「推不出」** | 本人独立尝试七种读法（圆窗反解、两因子相乘、`f_true·√3`、半高半径反解等）全部推不出 2.12；车道提出的 2.1406 只是「把两口径因子相乘」，该操作在文中无任何依据。按红线不得凭印象改数。 |
| RESAMPLE 车道 C 的 6-5「最近邻 + 邻域不重叠本身不会天然奇异，真正成因是零覆盖行与降秩 `C_x`」 | **部分采纳，保留本车道更一般的表述** | 车道用一个具体夹具指出「n_out = n_in 时最近邻不奇异」，这正确；但本车道的 `rank(C_y) ≤ min(n_out, n_in)` 给出了覆盖全部情形的上界，包含车道的特例。两者不矛盾，本车道版本更一般。 |
| RESAMPLE 车道 C 的 7-3「`bilinear` 直接送 `admit()` 会 `unknown_kernel_id`」 | **采纳，但下调严重度** | 本车道实测 `grep -rn "bilinear" lib/phase3_session/` 命中 `p3_session.cpp` 的分派逻辑，说明配置 token 走的是**另一条**不进注册表的路径；故「缺省值被拒」不是已发生的生产故障，而是**映射缺失**。严重度由「阻断」降为「须修」。 |
| 跨文档车道 D 的 R10-1「`2.12·σ` 真值 2.1406·σ」 | **同上否决** | 同 PSF 车道 A。 |
| 跨文档车道 D 的 R10-7「Bartelman 应改 Bartelmann」 | **登记不裁决** | arXiv 预印本侧著录即 `Bartelman`，Crossref 发表版为 `Bartelmann`；两册的条目著录的是**发表版**（ApJ 622(2):759–771），按著录规范应取 Crossref 值，但本车道未独立调 Crossref，且属第 5 轮范围。 |

---

## 3. 需要一手文献才能裁决的事项（明确标出需要什么文献）

按红线「需要查一手文献的**明确标出需要什么文献**，不要凭印象裁」，以下三条登记为 UNRESOLVED：

| 编号 | 事项 | 需要什么文献 |
|---|---|---|
| **U1** | ~~`PSF.md:77` 的「同二阶矩高斯 `σ_g = α/2`，在文献中常见」这一外部口径断言，来源为何~~ | ✅ **已关闭**（PSF 车道 A' 核对到一手出处并被本车道复核）：**Meyers J. E. & Burchat P. R. 2015, ApJ 807, 182**，DOI [10.1088/0004-637X/807/2/182](https://doi.org/10.1088/0004-637X/807/2/182)，开放版 [arXiv:1409.6273](https://arxiv.org/abs/1409.6273)。§7 原句「a β = 3.0 Moffat PSF with the **same second-moment squared radius** as a Gaussian PSF will have a FWHM only 60% as large as that of the Gaussian」，其 Eq.7 的 `r² = Ixx + Iyy` 为**逐轴**口径，代入 β=4 得 `σ_g = α/2`，与 `PSF.md:77` 吻合。⇒ **残留动作是补引证 + 写明「逐轴」口径，不是改数。** 另 A' 核对到 photutils 3.x 的 `MoffatPSF` docstring 通篇无 σ_g/α/2 换算（只能引它的 `FWHM = 2α√(2^{1/β}−1)`），SExtractor `src/psf.c` 是 mask 上数值量测无 α/β 拟合，SEP 无 Moffat 模型 —— 三家**都不能**作为 σ_g 的出处。 |
| **U2** | `A_drop,j = pixfrac²·A_pixel,j` 在 SIP 畸变下的**偏差闭式**是否存在 | 需要「非保面积映射下正方形足迹面积比」的解析界。候选来源：① 本仓 P3 实验单元的对照实现 `实验/healpix-polar/code/variants.h::overlap_adaptive`（该单元 `REPORT_paper.md:240` 已记载其缺陷待修，修好后可能给出直接读数）；② F&H 2002 §5 已有的畸变光度实验（4× 下采样 19×19 PSF、scale=0.5、pixfrac=0.6、RMS 0.004 mag）可作为**经验**上界的一手依据，但该文未给出 `A_drop/A_pixel` 的闭式。**⇒ 本车道只给出自己的推导（R6-6），未核到闭式文献，登记待裁。** |
| **U3** | `RESAMPLE.md:212` 的插值误差界在「输入为球面叶面积平均 + 切平面图上采样」下的正确形式 | 需要球面上面积平均-点值转换的误差界文献（如 quadrature rule on manifolds 的误差估计），以及 gnomonic chart 下的二阶导数界。本车道只指出原文式在 `h_x≠h_y` 与非点采样两种情形下不适用，**未核到替代闭式**。 |

---

## 4. 本轮是否零发现

**明确回答：第 6–10 轮全部不是零发现。** 本车道未发现任何一轮为零。

| 轮次 | 本车道亲读发现数 | 子代理补充（去重后并入） | 其中阻断 | 是否零发现 |
|---|---|---|---|---|
| 6 推理 | 14（R6-1 … R6-14） | +9（R6-15 … R6-23） | 5 | **否** |
| 7 负向 | 10（R7-1 … R7-10） | +4（R7-11 … R7-14） | 5 | **否** |
| 8 一致性 | 9（R8-1 … R8-9） | +4（R8-10 … R8-13） | 3 | **否** |
| 9 语言 | 3（R9-1 … R9-3）+ 1 通过项 | +2（R9-4、R9-5） | 0（1 红线） | **否** |
| 10 可复现 | 9（R10-1 … R10-9），其中 3 条通过项 | +3（R10-10 … R10-12） | 0 | **否** |
| **合计** | **45 条**（阻断 13） | **+22 条** | **13** | **5 轮全非零** |

**并集共 67 条**（本车道 45 + 子代理并入 22），其中子代理提出、本车道逐条复核后**否决 3 条、降级 2 条、部分采纳 2 条**，裁决表见 §5 末。

**同时如实记录本车道核到的通过项**（不为了推进而只报问题）：
- PSF.md §7.3 复算脚本 8 行输出注释**逐位一致**（本人实跑）；
- PSF.md §3.1/§3.2/§3.3/§3.5 的全部闭式与推导**独立重算通过**（`⟨r²⟩`、`FWHM` 因子、`detM`、`∬A/(1+Q)⁴dA`、截尾均值因子、MAD 因子）；
- DRIZZLE.md §7.1 的四处 Fruchter & Hook 逐字引用**全部属实**（本人取 arXiv 全文），含「权重式不含 s²」「该文献不含协方差矩阵产品」两条边界声明；
- 三册 §7 点名的 **29 个参考实现文件全部在树，零悬空指针**（本人逐个 `test -e`）；
- RESAMPLE.md §4 的角秒常数 `211076.28514206142` 与实现 `healpix_core.cpp:327` **逐位一致**（本车道开工时曾怀疑，已自我推翻，见 R10-9）；
- DRIZZLE.md §3.6 的孔径方差二次型「精确 ≥ 对角归约」在 `c_jp ≥ 0` 下**数学成立**（本人用 Cauchy-Schwarz 展开式 + 三组夹具核验，`a_p` 全非负时恒成立）；
- DRIZZLE.md §3.5 的 `N_p = D_p/pixfrac²`、`c_jp = a_jp/(A_pixel,j·D_p)` **推导正确**；
- DRIZZLE.md §3.8 的 `f_out(r_win) = (1+r²/α²)^{−3}` **推导正确**。

---

## 5. 推翻的既有结论

本车道**推翻了以下既有结论**（均给出反证）：

| # | 被推翻的结论 | 出处 | 反证 |
|---|---|---|---|
| **1** | 「模型值在 `{θ, π/2−θ, π/2+θ, π−θ}` 四个候选下完全相同」 | `docs/science/psf/PSF.md:173`（判据）、`:157`（参数表） | 代入 `p₁,p₂,p₃` 后三条候选给出三个**不同的系数三元组**与三个**不同的图像**（`max|ΔQ|` 达 0.39）。正确简并群必须同时置换 `(sx,sy)`。**同一文档 `:48` 已经写对**，`:157`/`:173` 与之互斥。生产代码 `dpsf_psf.cpp:634-646` 同步复制了这个错误。 |
| **2** | ~~「`σ_g = α/2` 是『同二阶矩高斯』的逐轴标准差」与同文上一行矛盾~~ | `docs/science/psf/PSF.md:77` | ⚠️ **本条已被本车道自查推翻并降级（见 R6-2）**。PSF 车道 A' 核到一手出处 Meyers & Burchat 2015（ApJ 807,182）：该文 Eq.7 的 `r² = Ixx + Iyy` 是**逐轴**二阶矩口径，代入 β=4 恰得 `σ_g = α/2`，与文档吻合。本车道初稿按**径向**口径 `√⟨r²⟩` 推理，忽略了文档 `:77` 明写的「逐轴」二字。**⇒ UNRESOLVED-U1 同步关闭。** 残留问题降为「须修」：口径未标注、β 一般式未声明。 |
| **3** | 「跨口径代入会带来 √2 的 FWHM 偏差」 | `docs/science/psf/PSF.md:77` | `1.7399178387/√2 = 1.2303077025` ≈ 闭式 `1.2303076526` ⇒ **一致换代给出同一 FWHM**，无 √2 偏差。√2 误差只在只换其一时发生。 |
| **4** | 「若改取 `w_jp = a_jp/A_pixel,j`，则 `Σ_p w_jp = 1/pixfrac²`」 | `docs/science/drizzle/DRIZZLE.md:116` | 由 `:166` 闭合式得 `Σ_p a_jp/A_pixel,j = pixfrac²`。三重独立佐证：同文 `:98`、代码头 `drizzle_science.h:131`、实验 `REPORT_paper.md:73`（「A_pixel 归一给 Σ_p w′ = pf²」）。**该因子在 `:116`/`:117` 两分支间被互换了。** |
| **5** | 「等价参数化配 `N'_p = Σ_j w'_jp = D_p`」 | `docs/science/drizzle/DRIZZLE.md:98`（代码头 `drizzle_science.h:30` 重复） | `Σ_j w'_jp = 0.64` 而 `D_p = 1.92`（本人闭合一致夹具实测）。正确式为 `Σ_j w'_jp·A_pixel,j = D_p`。 |
| **6** | 「几何闭合 `Σ_p a_jp = pixfrac²·A_pixel,j` 是构造性的、不依赖数值逼近」 | `docs/science/drizzle/DRIZZLE.md:169`（代码头 `drizzle_science.h:176` 自称「线性恒等」） | 右侧 `A_pixel,j` 是**独立实测**量（`drizzle_engine.cpp:1170-1189` 另做 4 次 `pixelToSky`）。`A_drop = pixfrac²·A_pixel` 仅在天球面积尺度因子于像元内恒定时精确；`:52` 明说处理含 SIP 畸变的四角。代码门本身带 `rel_tol` 且两侧分开具名判红 ⇒ 可红。F&H 原文亦称畸变修正「In the case of pixfrac = 1, this correction is exact」。 |
| **7** | 「常数面亮度场经**任何**单位和核原样映射到自身」 | `docs/science/resample/RESAMPLE.md:261` | 代入 `R_ij = a_ij/Ω'_i` 得 `y_i = B·coverage_i`（实测 coverage=0.95 → y/B=0.95）。生产 `p3_rsmp_operator.cpp:130` 未重归一。与 `DRIZZLE.md:14` 的不变量 2（在其覆盖面积归一下成立）**给出相反结论**。 |
| **8** | 「输出是输入的凸组合」/「两层的口径必须一致」 | `docs/science/resample/RESAMPLE.md:44`、`:16-21` | 叶级 `Σc'_k = 1`，算子级 `Σ_j R_ij = coverage_i ≤ 1`。两层归一口径**不同**，算子层不是凸组合。 |
| **9** | 「`Σ_p w_jp = 1`」「`Σ_p F_p = Σ_j x_j`」是无条件恒等式 | `docs/science/drizzle/DRIZZLE.md:75`、`:124` | 实验单元明写「权重和逐位为 1 的适用域是**同一叶内**的 drop 分割，不是全域无条件成立」；跨叶实测 `6.08e−11`、跨面 `1.75e−10`。 |
| **10** | 「取 `1.25`…**裕量在 20% 以上**」 | `docs/science/drizzle/DRIZZLE.md:200` | 按其自身给的 `1.044`：`1.25/1.044 = 1.197318` ⇒ 裕量 **19.73%**，断言自相矛盾。实验台账的穷举值是 `1.0415`（裕量 20.02%），与断言同侧 ⇒ 应是引用了旧扫描值。 |
| **11** | 「某象限在邻域里没有可用叶中心时用最近的叶中心填充」 | `docs/science/resample/RESAMPLE.md:84` | 与同文 `:105`「任一角 tile 缺失则该输出像元覆盖不成立且值为 `NaN`」互斥；生产 `p3_resample.cpp:401-403` 直接 `return` 并置 `NaN/coverage=0`，**无任何填充**。 |
| **12** | 「`operator_from_entries_row/col` 建的算子 R/S」是 §7.2:323 点名的生产算子入口 | `docs/science/resample/RESAMPLE.md:323` | `grep -rn "operator_from_entries" lib/ eng/` 只命中声明（`p3_rsmp.h:231,233`）与定义（`p3_rsmp_operator.cpp:92,96`），**零调用**。 |
| **13** | 「`PSF.md:108` 的辛普森余量来自**离散化**」 | `docs/science/psf/PSF.md:108` | ⚠️ **本车道自查推翻**。初稿把 `:108`/`:175` 的数值吻合记为通过项。PSF 车道 A' 做收敛性测试证伪归因：`N` 从 250 加到 8000（`h` 降 60 倍）结果**逐位不变**（全为 `−3.1083e−04`）；改积分域半宽 `L` 才单调收缩（`L=20→3.33e−03`、`30→3.11e−04`、`45→2.80e−05`、`60→5.04e−06`、`120→7.95e−08`）。⇒ `2.97×10⁻⁶` 完全是**有限积分域 `[-30,30]²` 的窗截断**，与 §3.4 的 `f_out` 同源，与求积阶数无关。**§7.3 的脚本会稳定复现这个错误归因。** |
| **14** | 「`1.007·hp_res` 是赤道带内中心到最远顶点的解析上界」 | `docs/science/drizzle/DRIZZLE.md:200` | DRIZZLE 车道 B' 用参考实现的 `xyf2loc`（只读参照仓内 vendored `healpix_base.cc`）全天穷举得 `nside=64→1.041499`、`128→1.043051`、`256→1.043827`、`512→1.044214`、`→∞→1.044599`，与参考实现 `max_pixrad()` 闭式**逐位一致**。极值位置恒为 `dec = ±41.81031° = |z|=2/3` 的**极冠边界叶**，其中心落在 `|z| ≤ 2/3` 内 ⇒ **同一赤道带内的实测 1.0438 直接反例推翻了 1.007 上界**。连带 R8-3 的裕量：`1.25/1.044214 = 1.1971` ⇒ **19.71% < 20%**。 |

### 子代理裁决表（本车道逐条复核与否决）

| 子代理条目 | 本车道裁决 | 理由 |
|---|---|---|
| PSF 车道 A 的 R6-1「`σ_g = α/√(2(β−2))`」 | **否决其「同二阶矩=径向」前提，保留其 β 特例化结论** | 见 R6-2 的自查更正。正确式是 `α/√(2(β−2))` 但那是**逐轴**口径。 |
| PSF 车道 A / 跨文档车道 D 的「`2.12·σ` 应改为 `2.1406·σ`」 | **否决替代值，维持「推不出」** | 本人独立尝试七种读法（圆窗反解、两因子相乘、`f_true·√3`、半高半径反解等）全部推不出 2.12；`2.1406` 只是「把两口径因子相乘」，该操作在文中无依据。按红线不得凭印象改数。 |
| PSF 车道 A 的 R6-4「σ 相对标准差式差 7.6–22.7 倍」/ A' 的 6-4「低估 8.7–19.4 倍」 | **采纳为须修（转引，本车道未复跑）** | 两条车道独立给出同一量级的结论，且各自用不同方法（Fisher 解析 vs 1-D χ² 扫描）；两车道都指出「结论（百分之几）对、公式错」。 |
| PSF 车道 A 的 R6-5「小样本截尾偏差符号错」 | **采纳为须修（转引）** | MC 实测给出 m=10,20 处偏差**翻正**（+2.31%/+1.23%），文档的「系统性低于」是单向断言。 |
| RESAMPLE 车道 C 的 6-5「最近邻 + 邻域不重叠本身不天然奇异」 | **部分采纳，保留本车道更一般的表述** | 车道用具体夹具指出 `n_out = n_in` 时不奇异（正确）；本车道的 `rank(C_y) ≤ min(n_out, n_in)` 覆盖全部情形。两者不矛盾。 |
| RESAMPLE 车道 C 的 7-3「`bilinear` 直接送 `admit()` 会 `unknown_kernel_id`」 | **采纳，降级为须修** | 本车道实测 `p3_session.cpp:116/155` 走的是**不进注册表**的另一条路径 ⇒ 是**映射缺失**而非已发生的生产故障。 |
| RESAMPLE 车道 C 的 7-2「`bunit_tol` 亦是死值」 | **采纳为须修（转引）** | 与本车道已复核的 `row_sum_tol` 同族；车道称实际判定是 `p3_rsmp_units.cpp:105-112` 的精确整数幂比较。 |
| RESAMPLE 车道 C 的 7-4「配置 token 从未进入 `admit()`」 | **采纳（后果已由本车道独立验证）** | 本车道已实测 `grep -rn '"bilinear"' lib/algorithms/resample/` 零命中 + `p3_export.h:159` 默认 `bilinear_4quad`。 |
| DRIZZLE 车道 B 的 7-1 / B' 的 R7-1「闭合门因 `A_pixel` 反推而恒真」 | **采纳并升级为全车道最重一条（R7-10）** | 本车道亲自复跑三处代码，链条闭合；且发现更深一层：生产热路径 `drizzle_engine.cpp:1318-1319` 明令**禁止**该近似，而生产门路径正是靠它。 |
| DRIZZLE 车道 B' 的 R8-5「`1.007` 被同一赤道带内的实测推翻」 | **采纳（转引，附本车道算术复核）** | 本车道复核裕量算术：`1.25/1.044 = 19.73% < 20%`，与 B/B'/D 三车道一致。 |
| DRIZZLE 车道 B' 的「F&H §7.1 明写 `a + b = 1`」 | **采纳——本车道已在自取的 F&H 全文中逐字核到** | 该句直接推翻 `docs/science/algorithms/DRIZZLE_GEOMETRY.md:50-51` 的「F&H 全文从未陈述 partition of unity」，**并加强** `DRIZZLE.md:75` 的 `Σ_p w_jp = 1`（见 R8-14）。 |
| DRIZZLE 车道 B' 的 6.1「`DRIZZLE.md:98` 量纲错」 | **采纳（与本车道 R6-4 独立同证）** | 两条车道分别用不同夹具得到同一结论。 |
| DRIZZLE 车道 B 的 9.1「`:217` 是裁判语言」 | **采纳为建议（转引）** | 「验收判决一律以…为准」把判决权写进 science 正本，与 `AGENTS.md` §5 的分工不合；但同段前半「求和型门对总量不变但逐叶错的注入无判别力」是**正确的判别力论证**，应保留。 |
| 跨文档车道 D 的 R10-7「Bartelman 应改 Bartelmann」 | **登记不裁决** | arXiv 预印本侧即 `Bartelman`，Crossref 发表版为 `Bartelmann`；两册著录的是发表版，按规范应取 Crossref 值，但本车道未独立调 Crossref，且属第 5 轮范围。 |

---

## 6. 最重的一条

⚠️ **本节已因 B/B' 两条 DRIZZLE 车道回件而改写。** 初稿把 PSF 的旋转简并（R6-1）列为最重；B' 回件后，DRIZZLE 的几何闭合恒真门（R7-10）在「证据链完整度 + 是否在生产中已发生 + 是否违反最高设计明文」三项上都更重，故改判。两条并列为最重。

**【最重 · 甲】R7-10 —— `DRIZZLE.md:208` 的几何闭合门在生产路径上恒真**

四条理由，逐条都比 R6-1 更硬：
1. **已在生产中发生**，不是纸面问题：`phase1_product.cpp:506` → `spherical_overlap_science.cpp:61-63` → `drizzle_science.cpp:198` 三跳，本车道亲自复跑，链条闭合到 `expected ≡ drop_area`。
2. **生产内部自相矛盾**：热路径 `drizzle_engine.cpp:1318-1319` 逐字写「**禁止用 A_drop/pixfrac² 近似替代**」，门路径却正靠它；同一仓内两条路径对同一关系给出相反处理。
3. **一手文献站在文档对面**：F&H §5「In the case of pixfrac = 1, this correction is exact」——该文自己说只在 `pixfrac=1` 精确；文档 `:42` 用「畸变修正**之前**的线性尺寸比」为**精确**性辩护，推理无效（线性尺寸比之比 ≠ 球面面积之比，中间隔着一层非线性雅可比）。
4. **违反最高设计明文**：`ACSD_DESIGN.md:497`「每个度量具备非退化判据，**恒真门没有证据资格**」——这正是任务书点名的失败模式，且该门是 `DRIZZLE.md:208` 写在「正确性判据」表里的第一条。

**【最重 · 乙】R6-1 / R7-2 —— `PSF.md:173` 的「旋转简并不变量」是恒假判据，且污染一级正本与生产实现**

初稿的最重判定，仍成立且证据更强（A' 车道补了危害量化）：
1. **恒假**：四候选中三条给出不同二次型（`max|ΔQ|` 达 1.6–1.7），正确简并群是 C₄（生成元须**同时交换 `sx,sy`**）。
2. **污染生产**：`dpsf_psf.cpp:634,638` 与文档逐字一致且 `sx,sy` 全程固定。
3. **危害已量化**（A' 车道合成实验，60 次重复）：椭率 ≤2% 时 **28%–42%** 概率把 θ 报成物理上错误的值；文档给出的免罪理由（「消歧只影响 θ 列」）恰恰是假的 —— 这个循环不是恒等 no-op，而是**向 θ 列注入噪声**。
4. **权威链失效**：文档与实现共同继承同一错误，「以文档为准」在此条上无法自动纠正，必须靠解析证据裁决（本车道已给出）。

**紧随其后**：R6-9 + R8-4（RESAMPLE 的行归一算子与「常量场不变性」硬判据在部分覆盖下互斥，且与 `DRIZZLE.md` 给出**相反**结论）—— 唯一一条**跨两份一级正本**的物理断言冲突，后果落在导出产品边缘像元上。

**需要负责人裁决的顶层项**：R8-1（`ACSD_DESIGN.md:118-119` 的 chart/鞋带/无超越函数口径 vs `DRIZZLE.md` + 生产代码的球面立体角/atan2 口径）。B/B'/D 三条车道独立发现同一冲突，是本次审稿中唯一四方一致的最高层冲突。

---

## 7. 自证段

### 7.1 本车道纪律自证

| 纪律 | 自证 |
|---|---|
| **零 git 写操作** | 本会话唯一 shell 写命令是 `bash -c '… > /tmp/*.py'` 与 `bash -c '… curl -sL … -o /tmp/fh.html'`，**全部写 `/tmp`**（仓外）。仓内唯一写入是本交付件 `run/GOVERN-08/审核包-R2/T06-审稿-SCI-psf-drizzle-resample.md`。**未执行 `git add` / `commit` / `checkout` / `reset` / `stash` / `tag` / `clean`。** |
| **不重写文档** | 三份分册与三份 README **一字未改**。全部问题只写在本交付件的「建议改法」栏，由写作者车道订正。 |
| **本人真读** | 六份成员文件 **889/889 行逐行读完**（见 §1），未用脚本扫描替代阅读。所有 `awk`/`grep` 只用于**核对我已读过的具体行**，不用作发现来源。 |
| **公式自己重推** | 本车道亲手重推：PSF 的 `⟨r²⟩`（Beta 换元 + 四 β 数值核）、FWHM 因子、`detM`、`∬A/(1+Q)⁴dA`、`f_out` 精确阈值、截尾均值因子、四候选 θ 的系数三元组与简并群；DRIZZLE 的 `N_p`/`c_jp`/等价参数化/反例因子/缩放律/孔径方差二次型（展开式 + 三组夹具）；RESAMPLE 的层序闭式、行归一与常量场、`γ_j` 推导、`rank(C_y)` 上界、`coverage` 括号式、误差界推导。**未采信任何子代理的推导作为唯一依据**。 |
| **参考文献核对原文** | 亲自取 `arXiv:astro-ph/9808087v2` 全文（F&H 2002），核对 DRIZZLE.md §7.1 的四处逐字引用与两条边界声明。**核对不到的（U1 的 photutils 1.x / SEP 原文、U2 的畸变闭式、U3 的球面面积平均误差界）一律标「核对不到」，未凭印象判定。** |
| **不写历史叙事、不设机械跳转锚** | 本交付件自身无日期、无版本号、无 commit、无流水编号；本车道给出的判定不依赖任何行号锚，全部配可复跑命令或「文件:行 + 复跑命令」双重定位。 |
| **不以「见某行」当依据** | 每条发现都给出**可复跑的定位方式**（`sed -n` / `grep -rn` / `awk -F'|'` / `python3 -c` / `test -e` 之一）。 |

### 7.2 主要发现的可复跑命令（抽样，全部只读）

```bash
cd "/workspace/Astro CS Database"

# R6-1 / R7-2  旋转简并恒不成立 + 生产代码复制了同一错误
sed -n '634,650p' lib/algorithms/psf/src/dpsf_psf.cpp
python3 -c "
import math
def q(t,sx,sy):
    return (math.cos(t)**2/(2*sx*sx)+math.sin(t)**2/(2*sy*sy),
            math.sin(2*t)/(4*sx*sx)-math.sin(2*t)/(4*sy*sy),
            math.sin(t)**2/(2*sx*sx)+math.cos(t)**2/(2*sy*sy))
sx,sy,t=3.,2.,0.37; b=q(t,sx,sy)
for n,t2 in [('th',t),('pi/2-th',math.pi/2-t),('pi/2+th',math.pi/2+t),('pi-th',math.pi-t)]:
    print(n, q(t2,sx,sy), q(t2,sx,sy)==b)
print('真简并伙伴 (sy,sx,th+pi/2):', q(t+math.pi/2,sy,sx)==b)"

# R6-2  同二阶矩高斯恒等于 sigma；alpha/2 只在 beta=6 成立
python3 -c "
import math
def mp(f,hi,n):
    h=hi/n;s=f(0)+f(n)
    for i in range(1,n): s+=f(i*h)*(4 if i%2 else 2)
    return s*h/3
a=2.0
for b in [3.,4.,6.,10.]:
    N=mp(lambda r:r**3*(1+(r*r)/(a*a))**-b,80,1500000); D=mp(lambda r:r*(1+(r*r)/(a*a))**-b,80,1500000)
    print(b, round(N/D,6), 'vs a^2/(b-2)=', round(a*a/(b-2),6), ' a/2=', a/2)"

# R6-3  3% 的精确阈值
python3 -c "import math;print(math.sqrt(2*((0.03)**(-1/3)-1)), (1+2)**-3)"

# R6-5  pixfrac 因子取反（三重佐证）
sed -n '98p;116p;117p;166p' docs/science/drizzle/DRIZZLE.md
grep -n "pixfrac\^2 \* w_jp" lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h
python3 -c "pf=0.8; print('正确 sum_p w\\' =', pf**2, ' 文档写 1/pf^2 =', 1/pf**2)"

# R6-6  闭合门右端是独立实测量
sed -n '1160,1190p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp
sed -n '176p;192,208p' lib/algorithms/drizzle/healpix_drizzle/drizzle_science.h lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp

# R6-9 / R8-4  常量场按 coverage 衰减；算子未重归一
sed -n '130,133p' lib/algorithms/resample/p3_rsmp_operator.cpp
python3 -c "
a=[0.7,0.6,0.4,0.2]; Om=2.0; B=7.3
print('coverage=',sum(a)/Om,' y/B=',sum(a)/Om)"

# R6-11  sum(pi)=1 硬门
sed -n '205,215p' lib/algorithms/resample/p3_rsmp_propagation.cpp

# R7-4  row_sum_tol 死值 + row_sums/full_coverage 零调用
grep -rn "row_sum_tol" lib/ eng/ ; grep -rn "row_sums\|col_sums\|full_coverage" lib/ eng/

# R7-5 / R7-6  配置 token 与注册表 id 对不上
grep -rn '"bilinear"' lib/algorithms/resample/
sed -n '159p' lib/phase3_session/p3_export.h
sed -n '124p' lib/algorithms/resample/p3_rsmp_kernel_registry.cpp
sed -n '539,540p' lib/phase3_session/p3_export.cpp

# R8-1  最高设计 vs 分册+代码
sed -n '115,123p' docs/ACSD_DESIGN.md
grep -n "atan2\|Van Oosterom\|Girard" lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h

# R8-2 / R8-6  坏路径与迁移遗留
test -e docs/engineering/contracts/CONFIG || echo "MISSING CONFIG (真实为 CONFIG.md)"
grep -rIl "docs/science/DRIZZLE\.md\|docs/science/PSF\.md\|docs/science/RESAMPLE\.md" docs/ lib/ eng/ 实验/ | wc -l

# R8-3  裕量算术
python3 -c "print('1.25/1.044 =',1.25/1.044,' 1.25/1.0415 =',1.25/1.0415)"
grep -n "1.0415" 实验/healpix-polar/REPORT_paper.md

# R8-5  residual_scale 单位冲突
sed -n '125p;160p' docs/science/psf/PSF.md ; sed -n '20p;44p' docs/science/algorithms/STAR_PSF_ALGORITHMS.md

# R8-7  c_jp 单位
sed -n '30,33,35,95,144p' docs/science/drizzle/DRIZZLE.md

# R9-1  全文唯一机械锚（注意：首遍用 '见 §' 正则会漏掉）
grep -nE "见 ?[0-9]+(\.[0-9]+)*|第 ?[0-9]+ ?(章|节)|§" docs/science/psf/PSF.md docs/science/drizzle/DRIZZLE.md docs/science/resample/RESAMPLE.md

# R9-2  §4 常数表两行破表（列数 4 / 5 / 6）
awk -F'|' 'NR>=223 && NR<=234 {printf "%d: cells=%d\n", NR, NF-2}' docs/science/resample/RESAMPLE.md

# R10-3  背景容差 0.5 只在代码与下级文档
sed -n '626,632p' lib/algorithms/psf/src/dpsf_psf.cpp ; sed -n '185p' docs/science/psf/PSF.md

# R10-4  1.7399178387 第 8 位起错
python3 -c "import math;print(repr(4*math.sqrt(2**0.25-1)))"

# R10-6 / R10-9  通过项：29 个文件在树；角秒常数逐位一致
for p in lib/algorithms/psf/src/dpsf_psf.cpp lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp \
         lib/algorithms/resample/p3_rsmp_propagation.cpp 实验/healpix-polar/code/audit/run_all.sh ; do
  [ -e "$p" ] && echo "OK $p" || echo "MISSING $p"; done
sed -n '327,331p' lib/algorithms/shared/healpix/healpix_core.cpp
python3 -c "import math;print(repr(math.sqrt(math.pi/3)*180/math.pi*3600))"
```

### 7.3 本交付件的已知限制（如实登记）

1. ~~DRIZZLE 侧无独立第二读者~~ —— **该限制已解除**：B/B' 两条 DRIZZLE 车道均已回件（见 §1 与 §8）。它们不仅交叉复核了本车道的 DRIZZLE 结论，还**推翻了一条本车道的既有判定**（B' 指出 F&H §7.1 明写 `a + b = 1`，从而加强而非削弱 `DRIZZLE.md:75`）并**带回一条本车道初稿遗漏的最重发现**（R7-10 几何闭合恒真门）。这既是覆盖强度的提升，也是「子代理补到我」的第二次实证（第一次见本节第 5 条）。
2. **未冻结 git 基线** —— 本交付件的行号针对工作树实读。审稿期间若并行车道改写了 `docs/`，行号会漂移；所有发现都配了可复跑命令，**命令优先于行号**。
3. **UNRESOLVED 只剩 U2/U3 两条**（U1 已由 PSF 车道 A' 核到一手出处 Meyers & Burchat 2015 后关闭，见 §3）。U2（`A_drop/(pixfrac²·A_pixel)` 在 SIP 畸变下的偏差闭式）与 U3（球面面积平均 + 切平面采样下的插值误差界）需要一手文献或仓内对照实现，超出本车道可及范围，已明确写出「需要什么文献」。
4. **`R10-9` 记了一条自我推翻** —— 本车道开工时怀疑 `RESAMPLE.md` 的角秒常数有误（以为真值 `211076.7873`），实算后证明文档的 `211076.28514206142` 逐位正确。列出以免后续车道重复怀疑。
5. **`R9-1` 记了一条子代理补到我的漏检** —— 本车道首遍用 `见 §` 正则扫描机械锚，漏掉 `RESAMPLE.md:130` 的「见 3.7」，是跨文档车道用更宽正则独立抓到的。这是本车道「不能只采信自己」的直接证据。
6. **初稿的「限制 1」与 §0 基线声明已随车道回件更新** —— 本节保留更正痕迹而非抹去，供审计「覆盖强度声明如何随证据变化」。
7. **`R7-1` 记了一条推不出的数** —— `PSF.md:172` 的 `2.12·σ`，本人尝试七种自然读法全部推不出；子代理提出的替代值 `2.1406·σ` 被本车道**否决**（该操作在文中无依据）。按红线维持「推不出」，不凭印象改数。

---

*本车道为只读审稿车道：未重写任何文档，未执行任何 git 写操作，未编译，未运行仓内实验或测试脚本。仓内唯一写入面是本交付件。*
---

## 8. 第二批车道回件后的增补清单（R6-15 … R8-13、R9-4/5、R10-10/11/12）

本节是 5 条子代理车道全部回件后**并入**的条目。每条标注来源车道与本车道的复核状态。

### 第 6 轮 · 推理轮

**【R6-15 · 阻断 · 来源 B'，本车道已复核】`DRIZZLE.md:44-46 / :65 / :166`｜`A_drop,j = pixfrac²·A_pixel,j` 不是精确关系，且 §2.2 为它给的理由在推理上无效**

即本车道 R6-6 的升级版。B' 用雅可比展开独立给出残差闭式：`A_pixel = 4h²f₀ + (4h⁴/3)(f_uu+f_vv) + O(h⁶)`，代入 `h' = h·pixfrac` 得

```
A_drop/(pixfrac²·A_pixel) − 1 = h²(pixfrac²−1)(f_uu+f_vv)/(3f₀) + O(h⁴) ≠ 0
```

**即使完全没有 SIP**，`1/(1+ξ²+η²)` 已使 `f_uu+f_vv = −4f₀/(1+r_c²)`，给出 `δ ≈ (1−pixfrac²)θ²/[3(1+r_c²)]`。
⇒ `DRIZZLE.md:42` 的辩护（`pixfrac` 是畸变修正**之前**的线性尺寸比）**推理无效**：线性尺寸比之比 ≠ 球面面积之比。
*复跑*：B' 脚本在仓外 `/tmp/dz_audit/drop_ratio3.py`；本车道已复核其结论与 `drizzle_engine.cpp:1318-1319` 的代码注释一致。

**【R6-16 · 阻断 · 来源 B/B'，本车道已复核】`DRIZZLE.md:169`｜叶边界在生产域是 4 角弦、永不细分，文档的「自适应细分逼近」与实现相反**

`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp:1420-1431`：`nside >= 256` 走 `get_healpix_boundary4`（4 角弦，无细分），`:1425-1427` 低 nside 才走自适应采样，`:1481` 的有界几何缓存**无条件**用 4 角弦。而 `DRIZZLE.md:190` 自定 `nside` 下限 **512** ⇒ **生产域叶边界永不细分**。
⇒ `DRIZZLE.md:52`「实现对非大圆弧的边做自适应细分」与 `:196` 的「叶边自适应细分阈值 `1e-6·hp_res`」「最大深度 `12`」两条参数**在生产域无效**（自适应只作用于 drop 边，阈值实为 60″，`drizzle_engine.cpp:1214-1232`）。
*本车道复核*：`grep -n "get_healpix_boundary4\|get_healpix_boundary_sampled\|HP_ADAPTIVE_MAX_DEPTH" lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`。

**【R6-17 · 须修 · 来源 A'，转引】`PSF.md:88`｜`fwhm_x = 1.230310·sx` 只在 θ=0/π/2 成立，实测偏小至 28%**

`sx` 是**主轴方向**半轴，不是面元 x 投影。面元 x 的真实 FWHM = `1.230308/√(2p₁)`。A' 实测（`sx=2.3, sy=3.1`）：`θ=0.30→+2.02%`、`θ=0.70→+10.88%`、`θ=1.20→+28.09%`。严格界 `1.230310·sy ≤ FWHM_x^面元 ≤ 1.230310·sx`。
**连带**：`:184` 的拒判 `fwhm_x > 拟合窗宽` 因此**永远偏松**，最多低估 `sy/sx` 倍（`dpsf_psf.cpp:619`）。这与 R7-1 的 FWHM 判据无效是同一根因。

**【R6-18 · 须修 · 来源 A，转引】`PSF.md:192`｜σ 相对标准差式 `residual_scale/(√m·A)` 系统性低估真值 8.7–19.4 倍**

A' 的 1-D χ² 扫描（其余参数置真）与 A 的 Fisher 解析**独立给出同一量级**。机理：`∂I/∂σ ∝ r⁻⁸`，外圈像元贡献迅速趋零，Fisher 和收敛，`σ` 的精度**不随窗口变大而改善** —— 文档式隐含的「样本越多越准」方向是错的。同句「决定了单星 σ 能被定到百分之几」的**结论对，但不由该式支持**（该式代入得 0.0093%，差 3 个数量级）。

**【R6-19 · 须修 · 来源 A，转引】`PSF.md:194`｜「小样本下截尾均值**系统性低于**高斯渐近值」符号错**

A 的 MC（`seed=20260901`，每点 4000 次，严格照抄 `dpsf_psf.cpp:426-459` 的 `lo=int(0.1m), hi=int(0.9m)`）实测偏差随 `m mod 10` **锯齿振荡**：m=8 `−9.30%`、m=10 `+2.31%`、m=11 `−9.10%`、m=20 `+1.23%`、m=25 `−3.94%`、m=101 `−1.14%`。文档正确指出了「两端不对称」的机理，却下了单向结论。

**【R6-20 · 须修 · 来源 B'，本车道已复核】`DRIZZLE.md:98`「等价」二字需收紧**

`w'_jp` 与 `w_jp` 只对 `c_jp / S_p / variance_p` 等价，**通量泛函不同**（`Φ_out = pixfrac²·Σ_j x_j` vs `Σ_j x_j`）。文档同句后半已点出，但前半用「等价」易被读成整体等价。

**【R6-21 · 须修 · 来源 B'，本车道已复核】`DRIZZLE.md:116`｜「等价于整帧偏暗 0.485 mag」挂错了对象**

数值本身对（`−2.5·log10(0.64) = 0.48455`，本车道独立复算），但重参数化下 `S_p` **不变**（两种参数化同给 `S_p = Σ_j B_j a_jp/Σ_j a_jp`），变的只有通量泛函 `Φ_out`。且 `eng/tools/HANDOVER.md:190` 用同一个 0.48 星等描述**相反方向**（漏乘 `k` 导致偏亮），两处并读会互相污染。

**【R6-22 · 须修 · 来源 B'，本车道已复核】`DRIZZLE.md:153`｜噪声相关比 `R` 的闭式漏「均匀抖动」极限**（与本车道 R10-8 同源，此处补一手原文）

F&H 原文：「While **R must be calculated for any given set of dithers**, there is perhaps one case that is particularly illustrative. When one has many dithers, and these dithers are fairly uniformly placed across the pixel, one can **approximate** … by assuming that the dither pattern is entirely uniform and continuously fills the output plane.」

**【R6-23 · 须修 · 来源 B，转引】`DRIZZLE.md:159`｜孔径方差「严格下界」缺 `a_p ≥ 0`，代码头已写而文档漏**

展开式 `exact − diag = Σ_j v_j·2Σ_{p<q} a_p a_q c_jp c_jq`。`c_jp ≥ 0` 恒成立，故 `a_p ≥ 0` 时成立；带号时不成立。B 的反例：`a = [−0.6481,−0.9747,0.6215,−0.9987]` ⇒ `exact = 0.9037677 < diag = 1.12779331`。代码头 `drizzle_science.h:316` 已写「当权重与重叠非负时严格 >」，**文档漏了这个前提**。（本车道 R6-1 曾记同族问题，此处由 B 车道补齐反例与代码位置。）

### 第 7 轮 · 负向轮

**【R7-11 · 须修 · 来源 B，本车道已复核】`DRIZZLE.md:209`｜常量面亮度门 `1e-3` 在**产品**上必然判红（8bit support 量化）**

发布 `signal = flux_sum/covered_area`（`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1386` `sig = flux / area`），而 `covered_area` 被 uint8 量化：`astro_sphere_sink.cpp:519-524` `q = lround(255·clamp(D_p/A_cell,0,1))`、`covered_area = (q/255)·A_cell`。故 `sig/S_p = 255·S/q`，相对误差 `≤ 0.5/q ≤ 0.5/255 = 1.96e−3 > 1e-3` —— **即使全覆盖叶也吃满 0.2%**。
⇒ 门必须写明判在哪一层：累加器级 `S_p = F_p/N_p`（1e-3）+ 产品级 signal（容差 ≥2e-3，与 8bit 量化预算挂钩）。**现文两处都没写。**
*本车道复核*：`sed -n '519,524p' lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`。

**【R7-12 · 阻断 · 来源 B/B'，本车道已复核】`DRIZZLE.md:206-215`｜§5.1 八条判据**零复跑入口**，负例注入无法证明能翻红**

`lib/algorithms/drizzle/CMakeLists.txt:20-31` 的 10 个生产源**不含** `drizzle_science.cpp` 与 `spherical_overlap_science.cpp`；`lib/**/tests/*.cpp` 为零；全树唯一的门调用点是 `lib/algorithms/integration/phase1_product/src/phase1_product.cpp:563`，且只覆盖 `gate_flux_conservation` 一条。
⇒ 违反 `docs/ACSD_DESIGN.md:528`「L1 合成科学性 … 每度量有归零负例」与标准 04 §2.2「构造『真值无效应 ⇒ 归零或报警』负例」。
*补充*：`gate_variance_scale_law` 与 `gate_constant_surface_brightness` 全仓**零调用**；后者用 `raw_c` 重算，而 `Σ_j c_jp·(B₀A_pixel,j) = B₀` 是 `raw_c` 的**定义恒等式**，数学上不可能失败 —— 正是 `DRIZZLE.md:273` 自称要避免的「同实现自证」。

**【R7-13 · 须修 · 来源 B，本车道已复核】`DRIZZLE.md:213`｜协方差门的容差地板使门在该量级恒绿**

`drizzle_science.cpp:517-518` `tol = diag_rel_tol*max(|exact|,|diag|,1.0)` ⇒ 当 `exact`、`diag` 均 `< 1` 时容差退化为**绝对**容差，门对这一量级恒绿。⇒ 去掉 `max(...,1.0)` 的地板或按方差量级归一。

**【R7-14 · 须修 · 来源 A，转引】`PSF.md:168-175`｜§5.1 四条不变量全是全称命题，没有一条「真值无效应 ⇒ 归零」负例**

A 的建议补三条：轴对齐星 `|p₂| < 1e−12`；各向同性星 `|sx−sy|/σ < 1e−3` 且 θ 落规范值域；平坦背景不触发背景拒判。违反 `ACSD_DESIGN.md:497` 与标准 04 §2.2。

### 第 8 轮 · 一致性轮

**【R8-10 · 阻断 · 来源 B/B'，本车道已复核】`w_jp` 在两份 science 正本里指两个核**

`DRIZZLE.md:75` 定义 canonical `w_jp = a_jp/A_drop,j`；`docs/science/unified/DATA_SEMANTICS.md:65` 写「`w_jp = a_jp/A_pixel,j` 是无量纲的重叠面积比」。同一符号在同一层正本给出两个定义，违反 `AGENTS.md:69`「同一主题只有一份正本」。
**更尖锐**：B 指出若按生产（drop）权重读 DATA_SEMANTICS 的 `variance = v_num_sum/D_p²`，在 `pixfrac=0.8` 时**偏 `pixfrac⁴ = 0.168`**；生产实现是 `astro_sphere_sink.cpp:539-549` 的 `var = sumVarNum·k²`（`k = D_p/N_p`）再除 `covered_area²`，即 `Σ_j v_j w_jp²/N_p²` ⇒ **`DRIZZLE.md` §3.6 对，`DATA_SEMANTICS` 对生产是错的**。
*（`docs/science/unified/DATA_SEMANTICS.md` 不在本车道可写面，仅报告。）*

**【R8-11 · 阻断 · 来源 B'，转引】`DRIZZLE.md:78`｜把「按 drop 面积归一」当作 Fruchter & Hook 的口径依据，属过度归因**

B' 实读 F&H 全文后指出：式 (5) 的归一用 `Σ_i a_i w_i`，**全文从未陈述** `Σ_o a_io = 1` 的 partition of unity，也从未给 `A_drop = pixfrac²·A_pixel`。`docs/science/algorithms/DRIZZLE_GEOMETRY.md:47-53` 已诚实登记「是本仓推断」，`DRIZZLE.md` 没有同样标注。
⚠️ **本车道部分否决此条**：本车道在自取的 F&H 全文中逐字核到 §7.1 的「let the area of overlap … with the 'primary' output pixel be a, and the areas of overlap with the other three pixels be b₁, b₂, and b₃, where **b = b₁+b₂+b₃, and a + b = 1**」—— **该文确实明文陈述了分割和为 1**。⇒ `DRIZZLE_GEOMETRY.md:50-51` 的「F&H 全文从未陈述 partition of unity」**被本车道实读推翻**；而 `DRIZZLE.md:75` 的 `Σ_p w_jp = 1` 反而**有一手直接支撑**，比现在的 drizzlepac 同构更硬。
⇒ **处置翻转**：`DRIZZLE.md:78` 应**补上 F&H「a + b = 1」这条一手锚**（不是标注为推断）；同时把「A_drop = pixfrac²·A_pixel」明确标为本仓推断。**见 R8-14。**

**【R8-12 · 须修 · 来源 B'，转引】`DRIZZLE.md:221`｜弦亏缺「随叶角尺度增长」方向反**

`0.1043885/nside²` 是**绝对**亏缺（sr），随 nside 增大而**减小**。B' 复算 `(π−2√2)/3 = 0.10438850961` ✓ 与文档逐位一致，`2√2/π − 1 = −0.09968368384` ✓ 与 `−9.968368384e−02` 一致；且该常数专属于**极冠叶的 4 角弦表示**，不是「叶边界折线逼近」的通律，也不能由自适应细分消除（与 R6-16 合并看，这条错误链在生产域无承接机制）。

**【R8-13 · 须修 · 来源跨文档 D，转引】`DRIZZLE.md:131` / `RESAMPLE.md` §3.4｜coverage 有互斥的多套定义**

D 指出：`RESAMPLE.md:121` 的 `coverage_i = Σ_j a_ij/Ω'_i` 是**分数**，而同文 `:103/:105` 与 §5.1 的「覆盖只由 tile 存在性决定」是**二值**；实现合同 `docs/science/algorithms/PHASE3_RESAMPLE.md:75` 与 schema 的 `coverage_output: enum["mask"]` 都是二值。⇒ 两套定义在同一分册内不可同时成立。（与本车道 R6-10 的括号式错误同族，但这是**定义层**的互斥，比括号式更根本。）

**【R8-14 · 正面（推翻一条既有否定结论）】F&H §7.1 明文给出 partition of unity ⇒ 加强 `DRIZZLE.md:75`**

见 R8-11 的裁决。这是本次审稿中**唯一一条由子代理提出、被本车道实读反证、从而加强了文档而非削弱文档**的发现。

### 第 9 轮 · 语言轮

**【R9-4 · 建议 · 来源 B，转引】`DRIZZLE.md:217`｜「验收判决一律以…为准」是裁判语言，且把判决权写进 science 正本**

同段前半「求和型的守恒门只证明总量守恒，对『总量不变但逐叶错注入』的缺陷没有判别力」是**正确的判别力论证，应保留**；后半「验收判决一律以…为准」应改为判别力陈述，判决权归验收层。

**【R9-5 · 建议 · 来源 B，转引】`DRIZZLE.md:221` 的措辞方向错**（与 R8-12 同源）、「DRIZZLE.md:200` 的「极冠内」应为「`|z| = 2/3` 边界叶」**（B' 独立穷举证实极值位置在边界而非冠内）。

### 第 10 轮 · 可复现轮

**【R10-10 · 阻断 · 来源 B/B'，本车道已复核】§5.1 八条判据零复跑入口**（即 R7-12 的可复现轮表述，此处记其对「复现路径完整」维度的独立成立）。

**【R10-11 · 须修 · 来源 B/B'，本车道已复核】§4 的 `1.007 / 1.044 / ±41.8°` 在仓内无可复跑入口**

`spherical_overlap.cpp:29,41` 的注释**引用**了扫描器 `scan_circumradius` 与「NSIDE 16/32/64/128/256 穷举」，但**该扫描器本身不在仓内**。*本车道复核*：`grep -rn "scan_circumradius" lib/ eng/ 实验/ docs/` → **3 处命中，全部是对该扫描器的引用而非其实现**——`spherical_overlap.cpp:29`「…穷举, scan_circumradius)、」、`:41`「扫描 (scan_circumradius) 仅作为附加证据」、以及 `实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv:337`（该台账同时把「`1.25` ≥ sup(外接半径) 的推导」标为 **OPEN** 未闭合）。⇒ 文档的这两个数在仓内**无法复跑**，只能标「实现注释记录，无仓内复跑入口」。
⚠️ **本车道自查更正**：初稿此处写「全仓 grep 不到」，措辞过强；准确表述是「扫描器不在树内，树内只有对它的引用」。
*同时*：`实验/healpix-polar/results/` 下只有 2 个文件 ⇒ 被引作证据的 1.0415、`0.1043885/N²`、9003 例零漏选等读数**无结果产物**，只能靠叙述。

**【R10-12 · 须修 · 来源跨文档 D，转引】`DRIZZLE.md:257`｜「预印本用罗马数字分节」不成立**

B/B' 独立取 `https://arxiv.org/pdf/astro-ph/0409513`（v1）实读，分节为「1. Introduction / 2. Discretized Mapping / 3. Requirements / 4. Meeting the Requirements / 5. The HEALPix Grid（5.1 Pixel Positions、5.2 Pixel Indexing、5.3 Pixel Boundaries）/ 6. Spherical Harmonic Transforms / 7. Summary」—— **阿拉伯数字**。文档声称与原文相反。
（跨文档车道 D 亦独立发现同一条。两条车道 + 本车道结论一致。）

---

## 9. 本车道对自身结论的更正记录（可审计）

按规范 03「审稿结论分：接受、小修、大修、拒稿」与红线「不隐瞒真问题」，本车道自查更正 **4 条**，全部保留在正文而非抹去：

| # | 初稿结论 | 更正后 | 触发 |
|---|---|---|---|
| **C-1** | `PSF.md:77` 的 `σ_g = α/2`「与同文上一行直接矛盾」，列为**阻断** | **降级为须修**；初稿的「矛盾」判定被推翻（忽略了文档明写的「逐轴」口径）。UNRESOLVED-U1 关闭 | PSF 车道 A' 核到 Meyers & Burchat 2015 一手出处 |
| **C-2** | `PSF.md:108` 的辛普森数值吻合记为**通过项** | **推翻**：该 `2.97×10⁻⁶` 是有限积分域窗截断，不是离散化误差；`:108` 的归因错误 | PSF 车道 A' 的收敛性测试（`N` 250→8000 逐位不变，`L` 30→120 降到 7.6e−10） |
| **C-4** | 「`scan_circumradius` 全仓 grep 不到」措辞过强 | 改为「扫描器不在树内，树内只有 3 处对它的引用」 | 本车道自查复跑 `grep -rn` 得 3 命中 |
| **C-3** | 初稿限制声明写「DRIZZLE 侧缺少独立第二读者」 | **该限制解除**；并因 B/B' 回件把最重一条由 PSF 改判为 DRIZZLE 的几何闭合恒真门 | B/B' 两车道回件 |

**「推不出的数」维持不改**：`PSF.md:172` 的 `2.12·σ`，本车道尝试七种读法全部推不出；两条子代理各自提出的替代值（`2.1406σ` / `2.4606σ`）本车道**一并否决** —— 它们分别对应「两因子相乘」与「保留错因子反解 σ」，两种操作在文中均无依据。按红线不得凭印象改数，维持「推不出」。

