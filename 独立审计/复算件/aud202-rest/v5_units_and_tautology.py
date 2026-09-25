#!/usr/bin/env python3
"""AUD202 补派 V5：独立取证复算（先于读核验成稿）。

三条独立核算：
 A. SNR 上界式的两侧单位各自推导（ADU 口径 vs 电子口径），给出量纲齐整的正确形式，
    并按"字面代入 ADU 数"重算偏差倍数。
 B. `node_reproduction_max_abs` 换源自测：自实现张量积自然三次样条 / 双线性 / 最近邻，
    喂不同源输入（常场、线性梯度、二次、高频正弦、跨 6 个量级随机、NaN 填充、整体偏置、
    半格错位），看该量能否报出非零；同时报"对解析真值的真实重建误差"作对照。
 C. 检索式记录（判"预测方差未实现"用的命令与命中数）。
纯标准库，不 import 仓库代码。
"""
import math
import re
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


# ---------------------------------------------------------------- A. 单位推导
def part_a():
    print("=" * 74)
    print("A. SNR_rep/SNR_true 上界式的单位（消费点 snr_science.cpp:168-215）")
    print("=" * 74)
    # 消费点事实（本脚本不 import，只按已读的源码式子重算）：
    #   gain>0 : var_i = sig_sky^2 + rn_term + F*P_i/g ; var_f = 1/sum(P_i^2/var_i)
    #   gain<=0: var_f = sig_sky^2 / sum_p2
    # 取均匀近似（逐像素源方差用均值）⇒ sigma_F^2 = (sig_sky^2 + <src var>)/sum_p2
    print("记号：F [ADU]（消费点 out->snr_optimal = F/sigma_f_optimal_adu，sigma_*_adu 全 ADU）")
    print("      sig_sky [ADU rms]（snr_science.cpp 的 sig_sky，字段名 sigma_sky_adu）")
    print("      g [e-/ADU]")
    print()
    print("左端  SNR_rep/SNR_true = sigma_F,true / sigma_F,rep  ⇒ 无量纲（两个 ADU 相除）")
    print("右端字面 1 + F/sig_sky^2 的加数： [ADU]/[ADU^2] = ADU^-1  ⇒ 与 1 相加 = 量纲不齐")
    print("正确：ADU 口径下把源'计数'换成'方差'要除 g  ⇒ 1 + F/(g*sig_sky^2) [ADU/(e-/ADU·ADU^2)]=1/e- ...")
    print("      严格写法回电子：N_e = g*F [e-]，Var_sky,e = g^2*sig_sky^2 [e-^2]")
    print("      ratio = sqrt(1 + N_e/Var_sky,e)  = 方差/方差 ⇒ 无量纲 ✓")
    print()
    # 数值：字面式 vs 正确式，扫描 x = F/(g sig_sky^2)
    print("字面式（丢 g）相对正确式的高估倍数 factor = sqrt((1+g*x)/(1+x))，x=F/(g*sig_sky^2)")
    print(f"{'g[e-/ADU]':>10} {'x=1':>9} {'x=10':>9} {'x=1e2':>9} {'x=1e4':>9} {'x→∞=√g':>10}")
    for g in (0.4, 0.5, 1.0, 1.3, 2.0, 5.0, 9.0, 13.85):
        row = [math.sqrt((1.0 + g * x) / (1.0 + x)) for x in (1.0, 10.0, 1e2, 1e4)]
        print(f"{g:>10.2f} " + " ".join(f"{v:>9.3f}" for v in row) + f" {math.sqrt(g):>10.3f}")
    print()
    print("⇒ 倍数只由 g 决定（x→∞ 时收敛到 √g），与 F/σ 无关；上界 = √g。")
    print()
    # φ 口径：反解文档给出的 41% / 347%
    print("文档同一条给的两个数：φ=0.5 偏高 41%、φ=0.95 偏高 347%")
    print("我反解 φ 的含义 = 源方差占总方差之比 φ = Var_src/(Var_src+Var_sky)：")
    for phi in (0.5, 0.95):
        r = 1.0 / math.sqrt(1.0 - phi)
        print(f"  φ={phi:<5} 1/sqrt(1-φ) = {r:.4f} ⇒ 偏高 {100*(r-1):.1f}%")
    print("⇒ 两数按 1/sqrt(1-φ) 逐位复现（41.4% / 347.2%），说明**数字**取自无量纲方差比，")
    print("  只有**写出来的式子** √(1+F/σ_bg²) 把计数当方差、丢了 1/g ⇒ 文档缺陷，不是数值缺陷。")
    print("  且 φ 口径下该上界**不需要 g**；写成 F/σ_bg² 却需要 g 才能算——")
    print("  而该分支的进入条件正是 gain_e_per_adu<=0（snr_science.cpp:207），自相矛盾。")


# ------------------------------------------------------- B. 算子与自检量复算
def natural_1d_second_derivs(y, h=1.0):
    """自然边界三次样条二阶导 M（三对角追赶法），与 weight_chain.cpp 同名做法。"""
    n = len(y)
    if n < 3:
        return [0.0] * n
    m = [0.0] * n
    a = [0.0] * n  # sub
    b = [1.0] * n  # diag
    c = [0.0] * n  # super
    d = [0.0] * n
    for i in range(1, n - 1):
        a[i], b[i], c[i] = 1.0, 4.0, 1.0
        d[i] = 6.0 * (y[i + 1] - 2 * y[i] + y[i - 1]) / (h * h)
    a[0], b[0], c[0], d[0] = 0.0, 1.0, 0.0, 0.0
    a[n - 1], b[n - 1], c[n - 1], d[n - 1] = 0.0, 1.0, 0.0, 0.0
    for i in range(1, n):
        w = a[i] / b[i - 1] if b[i - 1] else 0.0
        b[i] -= w * c[i - 1]
        d[i] -= w * d[i - 1]
    m[n - 1] = d[n - 1] / b[n - 1]
    for i in range(n - 2, -1, -1):
        m[i] = (d[i] - c[i] * m[i + 1]) / b[i]
    return m


def spline_eval_1d(xs, ys, ms, h, x):
    i = min(max(int(math.floor((x - xs[0]) / h)), 0), len(xs) - 2)
    if len(xs) == 1:
        return ys[0]
    A = (xs[i + 1] - x) / h
    B = (x - xs[i]) / h
    return (A * ys[i] + B * ys[i + 1]
            + ((A ** 3 - A) * ms[i] + (B ** 3 - B) * ms[i + 1]) * h * h / 6.0)


class Bilinear:
    name = "bilinear_regular_grid"

    def __init__(self, grid, x0, y0, dx, dy, clip=None):
        self.g, self.x0, self.y0, self.dx, self.dy = grid, x0, y0, dx, dy
        self.nx, self.ny = len(grid[0]), len(grid)
        self.clip = clip

    def __call__(self, x, y):
        fx = (x - self.x0) / self.dx
        fy = (y - self.y0) / self.dy
        i = min(max(int(math.floor(fx)), 0), self.nx - 2)
        j = min(max(int(math.floor(fy)), 0), self.ny - 2)
        tx, ty = fx - i, fy - j
        v = ((1 - tx) * (1 - ty) * self.g[j][i] + tx * (1 - ty) * self.g[j][i + 1]
             + (1 - tx) * ty * self.g[j + 1][i] + tx * ty * self.g[j + 1][i + 1])
        if self.clip:
            v = min(max(v, self.clip[0]), self.clip[1])
        return v


class Nearest:
    name = "nearest_control_point"

    def __init__(self, grid, x0, y0, dx, dy, clip=None):
        self.g, self.x0, self.y0, self.dx, self.dy = grid, x0, y0, dx, dy
        self.nx, self.ny = len(grid[0]), len(grid)

    def __call__(self, x, y):
        i = min(max(int(round((x - self.x0) / self.dx)), 0), self.nx - 1)
        j = min(max(int(round((y - self.y0) / self.dy)), 0), self.ny - 1)
        return self.g[j][i]


def clean_bicubic(grid, x0, y0, dx, dy, clip=None):
    """干净的张量积自然样条（分别对列、行做样条），避免上面占位实现。"""
    ny, nx = len(grid), len(grid[0])
    colM = [natural_1d_second_derivs([grid[j][i] for j in range(ny)], dy) for i in range(nx)]

    def ev(x, y):
        fy = (y - y0) / dy
        j = min(max(int(math.floor(fy)), 0), ny - 2)
        t = fy - j
        colvals = []
        for i in range(nx):
            ys = [grid[k][i] for k in range(ny)]
            ms = colM[i]
            A = 1.0 - t
            B = t
            colvals.append(A * ys[j] + B * ys[j + 1]
                           + ((A ** 3 - A) * ms[j] + (B ** 3 - B) * ms[j + 1]) * dy * dy / 6.0)
        rowM = natural_1d_second_derivs(colvals, dx)
        fx = (x - x0) / dx
        i = min(max(int(math.floor(fx)), 0), nx - 2)
        s = fx - i
        A = 1.0 - s
        B = s
        v = (A * colvals[i] + B * colvals[i + 1]
             + ((A ** 3 - A) * rowM[i] + (B ** 3 - B) * rowM[i + 1]) * dx * dx / 6.0)
        if clip:
            v = min(max(v, clip[0]), clip[1])
        return v
    return ev


def node_residual(ev, grid, x0, y0, dx, dy):
    """复刻 weight_chain.cpp:530-544：在节点坐标处经同一求值路径比 grid_[idx]。"""
    m = 0.0
    for j in range(len(grid)):
        for i in range(len(grid[0])):
            m = max(m, abs(ev(x0 + i * dx, y0 + j * dy) - grid[j][i]))
    return m


def true_error(ev, f, x0, y0, dx, dy, nx, ny, nstep=7):
    """对照量：域内（含非节点）对解析真值的最大绝对误差。"""
    m = 0.0
    for a in range(nstep * nx):
        for b in range(nstep * ny):
            x = x0 + (a / (nstep * nx - 1)) * (nx - 1) * dx
            y = y0 + (b / (nstep * ny - 1)) * (ny - 1) * dy
            m = max(m, abs(ev(x, y) - f(x, y)))
    return m


def make_field(kind, nx, ny, x0, y0, dx, dy):
    import random
    rng = random.Random(20260925)

    def at(i, j):
        x, y = x0 + i * dx, y0 + j * dy
        if kind == "const":
            return 30.0
        if kind == "grad":
            return 20.0 + 0.7 * x + 0.4 * y
        if kind == "quad":
            return 5.0 + 1e-4 * x * x + 2e-4 * y * y
        if kind == "sine":
            return 40.0 + 12.0 * math.sin(x / 13.0) * math.cos(y / 9.0)
        if kind == "resolved":      # 波长 = 4*cell，充分分辨的受控域
            return 28.0 + 6.0 * math.sin(2 * math.pi * x / 128.0) * math.cos(2 * math.pi * y / 128.0)
        if kind == "random6":
            return 10.0 ** rng.uniform(0.0, 6.0)
        raise ValueError(kind)
    return [[at(i, j) for i in range(nx)] for j in range(ny)], None


def truth_of(kind, x0, y0):
    import random
    rng = random.Random(20260925)

    def f(x, y):
        if kind == "const":
            return 30.0
        if kind == "grad":
            return 20.0 + 0.7 * x + 0.4 * y
        if kind == "quad":
            return 5.0 + 1e-4 * x * x + 2e-4 * y * y
        if kind == "sine":
            return 40.0 + 12.0 * math.sin(x / 13.0) * math.cos(y / 9.0)
        if kind == "resolved":
            return 28.0 + 6.0 * math.sin(2 * math.pi * x / 128.0) * math.cos(2 * math.pi * y / 128.0)
        return None
    return f


def part_b():
    print()
    print("=" * 74)
    print("B. node_reproduction_max_abs 换源自测（不同源输入 ⇒ 能否报非零？）")
    print("=" * 74)
    nx = ny = 6
    x0 = y0 = 15.5           # cell 中心约定：x0 = (dx-1)/2
    dx = dy = 32.0
    ops = {
        "natural_bicubic": lambda g: clean_bicubic(g, x0, y0, dx, dy,
                                                   clip=(min(min(r) for r in g),
                                                         max(max(r) for r in g))),
        "bilinear": lambda g: Bilinear(g, x0, y0, dx, dy),
        "nearest": lambda g: Nearest(g, x0, y0, dx, dy),
    }
    print(f"{'输入场':<26}{'算子':<16}{'节点复现残差':>16}{'对真值最大误差':>18}")
    worst_resid = 0.0
    for kind in ("const", "grad", "quad", "sine", "resolved", "random6"):
        grid, _ = make_field(kind, nx, ny, x0, y0, dx, dy)
        for oname, mk in ops.items():
            ev = mk(grid)
            r = node_residual(ev, grid, x0, y0, dx, dy)
            worst_resid = max(worst_resid, r)
            te = "-" if kind == "random6" else f"{true_error(ev, truth_of(kind, x0, y0), x0, y0, dx, dy, nx, ny):.4e}"
            print(f"{kind:<26}{oname:<16}{r:>16.3e}{te:>18}")
    print(f"\n六场 × 三算子 = 18 组，节点复现残差最大值 = {worst_resid:.3e}"
          f"  （生产门限 kNodeReproductionTol = 1e-9，weight_chain.cpp:17）")
    print("⇒ 含非零梯度、跨 6 个量级的随机场都报 0（浮点舍入级），恒真性立。")

    print("\n--- 换真值源：控制值与真值不同源（整体 +0.3 dex 标定偏置）---")
    grid, _ = make_field("sine", nx, ny, x0, y0, dx, dy)
    biased = [[v * 10 ** 0.3 for v in row] for row in grid]
    f_true = truth_of("sine", x0, y0)
    for oname, mk in ops.items():
        ev = mk(biased)
        print(f"  [{oname:<16}] node_reproduction_max_abs = "
              f"{node_residual(ev, biased, x0, y0, dx, dy):.3e}   "
              f"对真值场最大绝对误差 = "
              f"{true_error(ev, f_true, x0, y0, dx, dy, nx, ny):.4e}")
    print("  ⇒ 三个冻结词表算子一律报 0；bilinear/nearest 的'对真值误差'无实现歧义（可手工复核），")
    print("    量级即偏置本身（2 倍场 ≈ 数十 ADU），判据看不见。")
    print("    （natural_bicubic 在此欠采样栅格上还叠加样条振铃，数字只作定性，不入结论。）")

    print("\n--- 反向：什么样的输入能让它报非零（能红面 = 只剩索引/几何一致性）---")
    g_grad, _ = make_field("grad", nx, ny, x0, y0, dx, dy)
    bl = Bilinear(g_grad, x0, y0, dx, dy)
    bl_shift = Bilinear(g_grad, x0 + 0.5 * dx, y0, dx, dy)   # 求值栅格错半格
    print(f"  bilinear + 梯度场，正确栅格     ⇒ 残差 = {node_residual(bl, g_grad, x0, y0, dx, dy):.4e}")
    print(f"  bilinear + 梯度场，错半 cell     ⇒ 残差 = "
          f"{node_residual(bl_shift, g_grad, x0, y0, dx, dy):.4e}（非零 = 能红）")
    g_const = [[30.0] * nx for _ in range(ny)]
    bl_cshift = Bilinear(g_const, x0 + 0.5 * dx, y0, dx, dy)
    print(f"  同一错位但常场                   ⇒ 残差 = "
          f"{node_residual(bl_cshift, g_const, x0, y0, dx, dy):.4e}（平移不变 ⇒ 仍 0）")
    ev_shift = clean_bicubic(grid, x0 + 0.5 * dx, y0, dx, dy)
    print(f"  （bicubic + sine 场错半格参考值  ⇒ 残差 = "
          f"{node_residual(ev_shift, grid, x0, y0, dx, dy):.4e}）")
    print("  ⇒ 它只对'算子索引/几何与存储栅格不一致'敏感，且常场下连这个都看不见；")
    print("    对'重建出来的 SNR 与真 SNR 差多少'完全无分辨力。")

    print("\n--- 13_integration.md:39 把它登记为 manifest 里的『重建误差』；"
          "weight_chain.h:167 自己注明『应 ~0』 ⇒ 名称与内容不符 ---")
    print("--- 合同要求的『重建算子返回预测方差』在 SparseReconstruction 结构体里无字段 ---")
    src = open(r"F:\Astro dev\Astro CS Normalization Database\lib\algorithms\integration\v6"
               r"\include\astrocs\v6\weight_chain.h", encoding="utf-8").read()
    blk = src[src.index("struct SparseReconstruction"):src.index("class SparseSnrReconstructor")]
    fields = re.findall(r"^\s*(?:std::\w+|double|bool|int)\s+(\w+)", blk, re.M)
    print("  SparseReconstruction 字段：", ", ".join(fields))
    print("  含 var/variance/uncert 的字段：",
          [f for f in fields if re.search(r"var|uncert|sigma", f, re.I)] or "无")
    print("  对象数为 0 的守卫：weight_chain.cpp:372 `layer.points.empty()` ⇒ fail-closed 存在；"
          "但该判据本身把 0 当绿（无 'n==0 ⇒ 红' 的判据式）。")


def part_c():
    print()
    print("=" * 74)
    print("C. 判『预测方差未实现』用的检索式（跟踪集为准，命中数自报）")
    print("=" * 74)
    print(r"""  git -c core.quotePath=false grep -n "predicted_variance_adu2" -- lib/            => 0
  git -c core.quotePath=false grep -n "预测方差|predicted_var|prediction_var|pred_var" => 见下分类
  SparseReconstruction 出参：无逐像素方差字段（本文 B 节列字段）""")
    print("  命中分三类（只有第③类是合同要求的对象）：")
    print("    ① noise_model.predicted_variance_adu2 —— 只出现在 实验/absolute-snr/code/ 的")
    print("       Python 侧（p7_noise/exp3_sky_scan.py、exp01/hst_sim.py 注释），lib/ 跟踪集 0 命中；")
    print("       对象 = 逐像素物理噪声方差，不是重建方差。")
    print("    ② 实验侧 kriging/GPR 预测方差（snr-propagation-design.md:537、exp4_kriging_scaling.py，")
    print("       b4_integration.py 的 pred_var）——分析代码，不在生产算子里。")
    print("    ③ 合同要求（ACCEPTANCE_SPEC.md:60、ASTROCS_DESIGN.md:327、")
    print("       docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:99：『重建算子…返回预测方差』，")
    print("       且 snr-propagation-design.md:541 规定该方差要进权重分母）——生产算子无实现、无字段、")
    print("       eng/contracts 的 schema 侧亦无该字段的登记（grep variance∧(sparse|recon) 只命中")
    print("       位标志表与 REJECT 规则，无 prediction 字段）。")


if __name__ == "__main__":
    part_a()
    part_b()
    part_c()
