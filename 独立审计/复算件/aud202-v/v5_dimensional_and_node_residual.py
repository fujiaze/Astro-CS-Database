#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD202-V / V5: 两处独立复算
(a) 量纲不齐的噪声式  docs/plugins/algorithms_phase1/07_noise_snr.md:121
    「解析式 SNR_rep/SNR_true = sqrt(1+F/sigma_bg^2)；phi=0.5 偏高 41%、phi=0.95 偏高 347%」
(b) 节点复现最大绝对误差对插值算子是否恒为 0（复刻 weight_chain.cpp 的算术路径）
"""
import math

K_GAUSS = 2.3548200450309493
K_M4 = 1.230310


# ---------------------------------------------------------------- (a) 上界偏差
def moffat_profile(sigma_px, half):
    a2 = 2.0 * sigma_px * sigma_px
    v = []
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            v.append((1.0 + (i * i + j * j) / a2) ** -4)
    s = sum(v)
    return [x / s for x in v]


def exact_ratio(P, sigma_sky_adu, gain, F_adu):
    """本仓 snr_science.cpp 的真实两条臂：
       false (gain<=0 路径): var = sigma_sky^2 / sum P^2
       true  (gain>0 路径) : 1/var = sum P^2/(sigma_sky^2 + F*P/g)
       返回 SNR_false/SNR_true = sigma_true/sigma_false >= 1"""
    s2 = sigma_sky_adu ** 2
    sum_p2 = sum(p * p for p in P)
    var_false = s2 / sum_p2
    inv_true = sum(p * p / (s2 + F_adu * p / gain) for p in P)
    var_true = 1.0 / inv_true
    return math.sqrt(var_true / var_false)


def part_a():
    print("=" * 78)
    print("(a) 07_noise_snr.md:121 的解析式核对")
    print("  本仓单位约定（snr_science.cpp 头注 / NOISE_MODEL §5c）：")
    print("    flux F [ADU]、sigma_sky [ADU/像素]、gain g [e-/ADU]")
    print("    源散粒方差 = F*P_i/g [ADU^2]、背景方差 = sigma_bg^2 [ADU^2]")
    print("  => F/sigma_bg^2 的量纲 = ADU/ADU^2 = ADU^-1  —— 与 1 不可加（量纲不齐）")
    print("  正确无量纲组合：a = F/(g*sigma_bg^2)（= 全通量集中在一个像素时源方差/背景方差）")
    print()
    print("  恒等式核对：文档另一处口径 1/sqrt(1-phi) 与 sqrt(1+a) 何时一致？")
    for phi in (0.5, 0.95):
        a = phi / (1.0 - phi)
        print("    phi=%.2f -> 1/sqrt(1-phi)=%.6f (偏高 %.1f%%)   同 a=phi/(1-phi)=%.4f"
              " 时 sqrt(1+a)=%.6f" % (phi, 1 / math.sqrt(1 - phi),
                                      100 * (1 / math.sqrt(1 - phi) - 1), a, math.sqrt(1 + a)))
    print("    ⇒ 文档给出的 41%/347% 只能由 1/sqrt(1-phi)（phi=源方差占比）给出；")
    print("      而它写的式子是 sqrt(1+F/sigma_bg^2)：两式互为倒数变形的前提是")
    print("      F/sigma_bg^2 ≡ a = phi/(1-phi)，即 F 必须是**电子标度**且**含 1/g**，")
    print("      且 phi 从未定义 ⇒ 同一括号里两个符号不同源。")
    print()
    print("  真实算子（Horne 逐像素）相对 naive sqrt(1+a) 的偏差（本仓 Moffat4 轮廓）：")
    print("    sigma_bg=5 ADU, g=1.3, 检测块 FWHM=3.5 px")
    P = moffat_profile(3.5 / K_GAUSS, 40)
    p0 = max(P)
    print("    峰值像素占比 P_0 = %.6f" % p0)
    print("    %-12s %-14s %-14s %-14s %s" % ("F [ADU]", "a=F/(g sg^2)",
                                              "sqrt(1+a)", "本仓真实比值", "naive/真实"))
    for F in (10.0, 100.0, 1e3, 1e4, 1e5, 1e6):
        a = F / (1.3 * 25.0)
        ex = exact_ratio(P, 5.0, 1.3, F)
        print("    %-12.4g %-14.6g %-14.6g %-14.6g %.4f"
              % (F, a, math.sqrt(1 + a), ex, math.sqrt(1 + a) / ex))
    print()
    print("  把文档式按字面（漏 g）在 ADU 标度求值，与真实比值的倍数：")
    for F in (10.0, 100.0, 1e3, 1e4, 1e5):
        naive_nog = math.sqrt(1 + F / 25.0)
        ex = exact_ratio(P, 5.0, 1.3, F)
        print("    F=%-10.4g 字面 sqrt(1+F/sigma^2)=%-12.6g 真实=%-12.6g 偏 %-8.4f 倍"
              % (F, naive_nog, ex, naive_nog / ex))
    print()
    print("  正确形式（可直接写回文档）：")
    print("    SNR_rep/SNR_true = [ ΣP_i² / Σ_i P_i²/(1+a·P_i) ]^(1/2),"
          "  a = F/(g·σ_bg²)  （1 ≤ 比值 ≤ sqrt(1+a·P_0)）")
    print("    单像素/全通量集中时退化为 sqrt(1+a)；孔径求和时为 sqrt(1+a_eff)，")
    print("    a_eff = (F/g)/(N_pix σ_bg²)（Howell CCD 方程口径）。")


# ------------------------------------------------- (b) 节点复现残差是否恒为 0
def nat_second_deriv(y, n, M):
    for i in range(n):
        M[i] = 0.0
    if n < 3:
        return
    m = n - 2
    cp = [0.0] * m
    dp = [0.0] * m
    b0 = 4.0
    cp[0] = 1.0 / b0
    dp[0] = 6.0 * (y[2] - 2 * y[1] + y[0]) / b0
    for i in range(1, m):
        den = 4.0 - cp[i - 1]
        cp[i] = 1.0 / den
        rhs = 6.0 * (y[i + 2] - 2 * y[i + 1] + y[i])
        dp[i] = (rhs - dp[i - 1]) / den
    M[n - 2] = dp[m - 1]
    for i in range(m - 2, -1, -1):
        M[i + 1] = dp[i] - cp[i] * M[i + 2]


def nat_eval(y, M, n, pos):
    if n == 1:
        return y[0]
    i = int(math.floor(pos))
    i = 0 if i < 0 else (n - 2 if i > n - 2 else i)
    h = pos - i
    h = 0.0 if h < 0 else (1.0 if h > 1 else h)
    y0, y1 = y[i], y[i + 1]
    m0, m1 = M[i], M[i + 1]
    b = (y1 - y0) - (2 * m0 + m1) / 6.0
    return y0 + b * h + m0 * h * h / 2.0 + (m1 - m0) * h * h * h / 6.0


def bilinear(grid, nx, ny, gx, gy):
    cx = min(max(gx, 0.0), nx - 1.0)
    cy = min(max(gy, 0.0), ny - 1.0)
    i0 = min(max(int(math.floor(cx)), 0), nx - 2)
    j0 = min(max(int(math.floor(cy)), 0), ny - 2)
    fx, fy = cx - i0, cy - j0
    a = grid[j0 * nx + i0]; b = grid[j0 * nx + i0 + 1]
    c = grid[(j0 + 1) * nx + i0]; d = grid[(j0 + 1) * nx + i0 + 1]
    return (1 - fx) * (1 - fy) * a + fx * (1 - fy) * b + (1 - fx) * fy * c + fx * fy * d


def spline2d_node(grid, nx, ny, i, j):
    """可分离：y 向预计算二阶导 -> 在 gy=j 求值成 tmp[]，再 x 向在 gx=i 求值。"""
    my = [0.0] * (nx * ny)
    for ci in range(nx):
        col = [grid[r * nx + ci] for r in range(ny)]
        mcol = [0.0] * ny
        nat_second_deriv(col, ny, mcol)
        for r in range(ny):
            my[r * nx + ci] = mcol[r]
    tmp = [0.0] * nx
    mx = [0.0] * nx
    for ci in range(nx):
        col = [grid[r * nx + ci] for r in range(ny)]
        mcol = [my[r * nx + ci] for r in range(ny)]
        tmp[ci] = nat_eval(col, mcol, ny, float(j))
    nat_second_deriv(tmp, nx, mx)
    return nat_eval(tmp, mx, nx, float(i))


def node_residual_bilinear(grid, nx, ny, x0, dx):
    r = 0.0
    for j in range(ny):
        for i in range(nx):
            v = bilinear(grid, nx, ny, (x0 + i * dx - x0) / dx, (x0 + j * dx - x0) / dx)
            r = max(r, abs(v - grid[j * nx + i]))
    return r


def node_residual_spline(grid, nx, ny):
    r = 0.0
    for j in range(ny):
        for i in range(nx):
            v = spline2d_node(grid, nx, ny, i, j)
            r = max(r, abs(v - grid[j * nx + i]))
    return r


def part_b():
    print("=" * 78)
    print("(b) node_reproduction_max_abs 的判别力自测（复刻 weight_chain.cpp 算术）")
    tol = 1e-9
    cases = {
        "正常 4x4 场": [1.0, 1.1, 0.9, 1.05, 0.8, 1.4, 1.2, 0.7,
                       1.3, 0.6, 1.5, 1.0, 0.95, 1.25, 0.85, 1.35],
        "整体乘 1e6（水平错 6 个数量级）": [v * 1e6 for v in
                                          [1.0, 1.1, 0.9, 1.05, 0.8, 1.4, 1.2, 0.7,
                                           1.3, 0.6, 1.5, 1.0, 0.95, 1.25, 0.85, 1.35]],
        "值被完全打乱（错配到别的像素）": [13.7, 0.12, 88.4, 2.2, 0.31, 45.9, 7.7, 3.3,
                                         0.04, 21.0, 66.6, 1.1, 9.9, 0.55, 120.0, 4.4],
        "整场取同一常数（无空间信息）": [3.3] * 16,
        "上/下交替棋盘（欠采样混叠）": [1.0, 9.0, 1.0, 9.0, 9.0, 1.0, 9.0, 1.0,
                                       1.0, 9.0, 1.0, 9.0, 9.0, 1.0, 9.0, 1.0],
        "整体乘 1e7（合法但量级大的绝对 SNR）": [v * 1e7 for v in
                                            [1.0, 1.1, 0.9, 1.05, 0.8, 1.4, 1.2, 0.7,
                                             1.3, 0.6, 1.5, 1.0, 0.95, 1.25, 0.85, 1.35]],
    }
    print("    %-34s %-14s %-14s %s" % ("输入（同源/异源）", "双线性残差", "自然样条残差", "过 1e-9 门？"))
    for name, g in cases.items():
        rb = node_residual_bilinear(g, 4, 4, 3.5, 8.0)
        rs = node_residual_spline(g, 4, 4)
        ok = "绿" if (rb <= tol and rs <= tol) else "红"
        print("    %-34s %-14.6g %-14.6g %s" % (name, rb, rs, ok))
    print("    ⇒ 五组输入里包含**完全不同源**的场（值被打乱、整体错 1e6、整场平坦、")
    print("      棋盘混叠），残差都在 1e-13 以下 ⇒ 该量对『重建出来的值对不对』零判别力。")
    # 序级错位是否判红（检验 weight_chain.cpp:527-528 的自我声明）
    wrong_order = [0.0] * 16
    src = cases["正常 4x4 场"]
    for j in range(4):
        for i in range(4):
            wrong_order[i * 4 + j] = src[j * 4 + i]      # 行列互换后当作 grid_
    rt = node_residual_spline(wrong_order, 4, 4)
    print("    对照 `weight_chain.cpp:527-528` 的自述（「索引/几何错位会在此放大为红灯」）：")
    print("      把 grid_ 换成行列互错的版本，按同一路径自检的残差 = %.6g ⇒ 仍为 0。" % rt)
    print("      原因：比较的两侧（eval 的输出与 grid_[idx]）**同源于那张内部数组**，")
    print("      任何值级/序级破坏同时改变两侧 ⇒ 恒等。")
    print("      ⇒ 该门唯一真实作用域 = 重建器**自身实现**是否自洽（样条求解器写错、")
    print("        NaN 泄漏），属单元级自测；作为逐帧产品/manifest 里的『重建误差』无证据资格。")
    # mesh_median 档：比的是滤波后的 grid_，声明值从未参与
    print("    mesh_median 档：prepare() 先 median3_mesh 覆写 grid_，随后")
    print("      max_resid 比的是 vv 与 **滤波后** 的 grid_[idx]（weight_chain.cpp:541），")
    print("      层的**声明控制点值**从未进入比较 ⇒ 名义上的「控制点自身复现」对这一档不成立。")


if __name__ == "__main__":
    part_a()
    print()
    part_b()
