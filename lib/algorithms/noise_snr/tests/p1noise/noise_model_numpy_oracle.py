#!/usr/bin/env python3
"""P1-002 / M3-F-001：docs/science/NOISE_MODEL.md 承诺的 **NumPy 独立 Oracle**。

承诺原文（本轮实测锚）：
  - NOISE_MODEL.md:117  "**Python 参考**：NumPy 对同 data 的 median/MAD/5σ裁剪/
                         平面最小二乘 复算 variance/ivar（rtol 1e-9）。"
  - NOISE_MODEL.md:140  "§11 Oracle 全过：… Python 参考 rtol 1e-9；"

独立性（ENGINEERING_SPEC.md §5.1「不调用生产实现的独立 Oracle」）：
  * 参考值全部由 NumPy 从**同一 data** 第一性原理复算：
    patch 划分 → 有限样本过滤 → median / MAD→σ(1.482602218505602) →
    med±clip_sigma·σ 稳健裁剪 ≤max_rounds 轮 → σ² → 控制点平面最小二乘（SVD）→
    max(var, floor) → ivar = 1/var；
  * 生产面仅作**被测对象**：把 lib/algorithms/noise_snr/cpp/src/noise_model.cpp
    (+snr_science.cpp) 编译成一次性可执行文件（生产源零改动）；Python 侧
    **不 import、不链接、不调用任何生产函数**。
  * 与 tests/backend/test_noise_model_oracle.py 的区别：后者只在 Python 侧写
    不变量/解析式，没有 NumPy 对同 data 的复算（本轮实测该文件 numpy 命中 0）。

容差口径：
  * FP64 model 量（控制点 variance/ivar、全局 variance/ivar/σ）→ rtol 1e-9（SCI §15:140）；
  * fill 平面是 **FLOAT32 产品**（snr_estimator.h:168-174），按 float32 存储精度 rtol 2e-6。

用法: python3 lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py
退出码: 0 = 全过（缺 numpy/g++ 时 skip 亦为 0）；1 = 任一断言失败。
"""
import math
import os
import shutil
import statistics
import subprocess
import sys
import tempfile

try:
    import numpy as np
except ImportError:  # pragma: no cover
    print("SKIP: numpy 不可用，无法执行 NumPy 独立 Oracle")
    sys.exit(0)

HERE = os.path.dirname(os.path.abspath(__file__))


def _repo_root(start):
    """上溯到仓库根（根标志 = ASTROCS_DESIGN.md + 根 CMakeLists.txt）。

    不写死层级：模块自身也有 CMakeLists.txt，故必须用根独有标志判定。
    """
    cur = start
    while True:
        if (os.path.isfile(os.path.join(cur, "ASTROCS_DESIGN.md"))
                and os.path.isfile(os.path.join(cur, "CMakeLists.txt"))):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            raise RuntimeError("repo root not found from %s" % start)
        cur = parent


REPO = _repo_root(HERE)
SNR_INC = os.path.join(REPO, "lib", "algorithms", "noise_snr", "cpp", "include")
SNR_SRC = os.path.join(REPO, "lib", "algorithms", "noise_snr", "cpp", "src")

# MAD→σ 常数由**第一性原理导出**（M3-F-001 独立性补强，R-5 §5.13①）：标准正态
# 的 MAD 分位恒等式 Φ⁻¹(3/4) ⇒ MAD→σ = 1/Φ⁻¹(3/4)。不抄生产字面量。
MAD_TO_SIGMA = 1.0 / statistics.NormalDist().inv_cdf(0.75)
# 自证：与 SCI-NOISE-001 §5:46 冻结字面量逐位相等（不等则本 Oracle 自身失效）
assert MAD_TO_SIGMA == 1.482602218505602, "MAD_TO_SIGMA 与 SCI §5:46 冻结值不逐位相等"
RTOL_MODEL = 1e-9                  # NOISE_MODEL.md §15:140 承诺 rtol 1e-9
RTOL_PLANE = 2e-6                  # fill 面 float32 存储精度

# 注意: 本驱动刻意不使用任何反斜杠转义（换行用 nl()），避免多层字符串转义失真。
DRIVER = r'''
#include "snr_estimator.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>

static void nl(void) { std::putchar(10); }

int main(int argc, char** argv) {
    if (argc < 10) { std::fprintf(stderr, "usage: d <data.bin> h w gx gy minsamp clip rounds floor"); nl(); return 2; }
    const char* path = argv[1];
    const int h = std::atoi(argv[2]);
    const int w = std::atoi(argv[3]);
    const int gx = std::atoi(argv[4]);
    const int gy = std::atoi(argv[5]);
    const int minsamp = std::atoi(argv[6]);
    const double clip = std::atof(argv[7]);
    const int rounds = std::atoi(argv[8]);
    const double floor_v = std::atof(argv[9]);
    const std::size_t n = (std::size_t)h * (std::size_t)w;
    std::vector<double> data(n);
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open data file"); nl(); return 2; }
    if (std::fread(data.data(), sizeof(double), n, f) != n) { std::fclose(f); return 2; }
    std::fclose(f);
    SnrNoiseModelConfig cfg;
    snr_noise_model_v1_default_config(&cfg);
    std::printf("DEFCFG %d %d %.17g %.17g %.17g %.17g %d %d %d %.17g"
                " %u %u %.17g %.17g %.17g %u %u",
                cfg.patch_grid_x, cfg.patch_grid_y, cfg.source_mask_radius_px,
                cfg.mask_radius_scale, cfg.gain_e_per_adu, cfg.read_noise_e,
                cfg.min_patch_samples, cfg.max_clip_rounds,
                (int)cfg.enable_spatial_field, cfg.variance_floor,
                (unsigned)cfg.struct_size, (unsigned)cfg.abi_version,
                cfg.mask_k_sigma, cfg.mask_r_min_px, cfg.mask_fwhm_floor_scale,
                (unsigned)cfg.mask_budget_min_patches, (unsigned)cfg.mask_budget_min_sky);
    nl();
    cfg.patch_grid_x = gx; cfg.patch_grid_y = gy;
    cfg.min_patch_samples = minsamp; cfg.cosmic_clip_sigma = clip;
    cfg.max_clip_rounds = rounds; cfg.variance_floor = floor_v;
    cfg.saturation_level = 0.0; cfg.enable_spatial_field = 1;
    // 可选第 11 参数: stars.bin (x,y,flux,fwhm 交错 double) —— MASK-002 源污染 oracle
    std::vector<double> sx, sy, sf, sw;
    if (argc > 10) {
        std::FILE* fs = std::fopen(argv[10], "rb");
        if (!fs) { std::fprintf(stderr, "cannot open stars file"); return 2; }
        std::vector<double> raw;
        double buf[4];
        while (std::fread(buf, sizeof(double), 4, fs) == 4) {
            sx.push_back(buf[0]); sy.push_back(buf[1]);
            sf.push_back(buf[2]); sw.push_back(buf[3]);
        }
        std::fclose(fs);
    }
    NoiseWeightModelV1 m;
    const int rc = snr_noise_model_v1_f64(data.data(), h, w, nullptr,
                                          sx.empty() ? nullptr : sx.data(),
                                          sy.empty() ? nullptr : sy.data(),
                                          sf.empty() ? nullptr : sf.data(),
                                          sw.empty() ? nullptr : sw.data(),
                                          (int)sx.size(), &cfg, &m);
    std::printf("RC %d", rc); nl();
    std::printf("MASK %u %.17g %.17g %u %u", m.mask_degraded, m.mask_radius_p50,
                m.mask_frac, (unsigned)m.struct_size, (unsigned)m.abi_version); nl();
    std::printf("NQ %u %u %u %u", m.n_qualified_patches, m.n_rejected_patches,
                (unsigned)m.has_spatial_field, (unsigned)m.degenerate); nl();
    std::printf("VG %.17g %.17g %.17g", m.variance_bg_global, m.ivar_bg_global, m.sigma_bg_global); nl();
    for (uint32_t i = 0; i < m.n_control_points; ++i) {
        std::printf("CP %u %.17g %.17g %.17g %.17g", i,
                    m.ctrl_x_px[i], m.ctrl_y_px[i], m.ctrl_variance[i], m.ctrl_ivar[i]);
        nl();
    }
    std::vector<float> fv(n), fi(n);
    const int rcf = snr_noise_model_v1_fill(&m, h, w, fv.data(), fi.data());
    std::printf("FILLRC %d", rcf); nl();
    std::printf("FV");
    for (std::size_t i = 0; i < n; ++i) std::printf(" %.9g", (double)fv[i]);
    nl();
    std::printf("FI");
    for (std::size_t i = 0; i < n; ++i) std::printf(" %.9g", (double)fi[i]);
    nl();
    std::printf("GV %.17g %.17g %.17g",
                snr_noise_gain_variance(1000.0, 3.0, 2.0),
                snr_noise_gain_variance(-50.0, 3.0, 2.0),
                snr_noise_gain_variance(0.0, 0.0, 5.0));
    nl();
    double v = 9.0, iv = 1.0 / 9.0;
    snr_noise_scale_law(2.0, &v, &iv);
    std::printf("SL %.17g %.17g", v, iv); nl();
    snr_noise_model_v1_free(&m);
    std::printf("DONE"); nl();
    return 0;
}
'''

G_FAIL = 0
G_TOTAL = 0


def close(got, exp, rtol, what):
    global G_FAIL, G_TOTAL
    G_TOTAL += 1
    got_a = np.asarray(got, dtype=np.float64)
    exp_a = np.asarray(exp, dtype=np.float64)
    if got_a.shape != exp_a.shape:
        G_FAIL += 1
        print("FAIL %s: shape %s vs %s" % (what, got_a.shape, exp_a.shape))
        return
    denom = np.maximum(np.abs(exp_a), 1e-300)
    rel = np.abs(got_a - exp_a) / denom
    bad = int(np.sum(rel > rtol))
    if bad:
        G_FAIL += 1
        idx = int(np.argmax(rel))
        print("FAIL %s: %d/%d 点超差 max_rel=%.3g (rtol=%.3g) got=%.17g exp=%.17g"
              % (what, bad, got_a.size, float(rel.max()), rtol,
                 float(got_a.reshape(-1)[idx]), float(exp_a.reshape(-1)[idx])))


def check(cond, what):
    global G_FAIL, G_TOTAL
    G_TOTAL += 1
    if not cond:
        G_FAIL += 1
        print("FAIL %s" % what)


# ---------------------------------------------------------------------------
# MASK-002 (claim SC-009) 源污染 oracle 的**独立**半径实现 (不抄生产字面量):
#   SCI-NOISE-001 §5a: Moffat β 的 r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1),
#   α = FWHM/(2·sqrt(2^(1/β)−1)); β=2.5 = 掩膜专用保守翼指数; k=0.1;
#   r_i = clip(r_local, max(1.5, 0.75·FWHM), 60); 圆盘光栅化 = 像素中心在半径内。
# ---------------------------------------------------------------------------
MASK_BETA = 2.5


def moffat_r_local(flux, fwhm, k_sigma_bg, beta=MASK_BETA):
    alpha = fwhm / (2.0 * math.sqrt(2.0 ** (1.0 / beta) - 1.0))
    q = (flux * (beta - 1.0) / (math.pi * alpha * alpha * k_sigma_bg)) ** (1.0 / beta)
    if q <= 1.0:
        return 0.0
    return alpha * math.sqrt(q - 1.0)


def disk_mask_frac(w, h, cx, cy, radii):
    """独立圆盘光栅化 (与生产同判据: 像素中心 (lround(cx),lround(cy)) 起 dx²+dy² ≤ r²)。"""
    mask = np.zeros((h, w), dtype=bool)
    yy, xx = np.mgrid[0:h, 0:w]
    for x, y, r in zip(cx, cy, radii):
        if r <= 0.0:
            continue
        icx, icy = int(round(float(x))), int(round(float(y)))
        sel = (xx - icx) ** 2 + (yy - icy) ** 2 <= r * r
        mask |= sel
    return float(mask.mean())


def reference_model(data, h, w, gx, gy, minsamp, clip, rounds, floor_v):
    """NumPy 第一性原理复算（不调用生产实现）。"""
    gx = max(2, gx)
    gy = max(2, gy)
    clip = max(1.0, clip)
    minsamp = max(1, minsamp)
    rounds = max(0, rounds)
    xs, ys, var = [], [], []
    n_rejected = 0
    for py in range(gy):
        y0 = (py * h) // gy
        y1 = ((py + 1) * h) // gy
        for px in range(gx):
            x0 = (px * w) // gx
            x1 = ((px + 1) * w) // gx
            smp = data[y0:y1, x0:x1].reshape(-1).astype(np.float64)
            smp = smp[np.isfinite(smp)]          # saturation_level=0 → 仅有限性过滤
            if smp.size < minsamp:
                n_rejected += 1
                continue
            cur = smp
            rejected = False
            for _ in range(rounds):
                med = float(np.median(cur))
                sig = MAD_TO_SIGMA * float(np.median(np.abs(cur - med)))
                if not (sig > 0.0):
                    break
                lo = med - clip * sig
                hi = med + clip * sig
                kept = cur[(cur >= lo) & (cur <= hi)]
                if kept.size < minsamp:
                    rejected = True
                    break
                if kept.size == cur.size:
                    break
                cur = kept
            if rejected:
                n_rejected += 1
                continue
            med = float(np.median(cur))
            sig = MAD_TO_SIGMA * float(np.median(np.abs(cur - med)))
            if not math.isfinite(sig) or sig <= 0.0:
                n_rejected += 1
                continue
            xs.append((x0 + x1) * 0.5)
            ys.append((y0 + y1) * 0.5)
            var.append(sig * sig)
    return (np.asarray(xs), np.asarray(ys), np.asarray(var), n_rejected)


def run_case(exe, name, data, gx, gy, minsamp, clip, rounds, floor_v, tmp,
             stars=None):
    h, w = data.shape
    binp = os.path.join(tmp, name + ".bin")
    data.astype("<f8").tofile(binp)
    argv = [exe, binp, str(h), str(w), str(gx), str(gy),
            str(minsamp), repr(clip), str(rounds), repr(floor_v)]
    if stars is not None:
        sp = os.path.join(tmp, name + "_stars.bin")
        np.asarray(stars, dtype="<f8").reshape(-1).tofile(sp)
        argv.append(sp)
    r = subprocess.run(argv, capture_output=True, text=True, timeout=300)
    check(r.returncode == 0, "%s driver rc=0" % name)
    out = {"CP": []}
    for line in r.stdout.splitlines():
        tok = line.split()
        if not tok:
            continue
        if tok[0] == "CP":
            out["CP"].append([float(x) for x in tok[2:]])
        elif tok[0] in ("RC", "NQ", "VG", "FILLRC", "GV", "SL", "DEFCFG", "MASK"):
            out[tok[0]] = tok[1:]
        elif tok[0] == "FV":
            out["FV"] = np.asarray([float(x) for x in tok[1:]], dtype=np.float64)
        elif tok[0] == "FI":
            out["FI"] = np.asarray([float(x) for x in tok[1:]], dtype=np.float64)
    check(out.get("RC", ["?"])[0] == "0", "%s rc=0 (成功/含全局兜底)" % name)
    if stars is not None:
        # 含星帧: 生产侧掩膜由 §5a 逐星半径决定, 未掩膜参考复算在调用点单独给出
        # (main() 的 D_starfield 段: 独立 σ_seed → 独立 r_local/光栅化/预算断言)。
        return out
    if G_FAIL:
        return out

    xs, ys, var, n_rej = reference_model(data, h, w, gx, gy, minsamp, clip, rounds, floor_v)
    cp = np.asarray(out["CP"], dtype=np.float64) if out["CP"] else np.zeros((0, 4))
    check(cp.shape[0] == var.size, "%s 控制点数一致 (prod=%d ref=%d)"
          % (name, cp.shape[0], var.size))
    check(int(out["NQ"][0]) == var.size and int(out["NQ"][1]) == n_rej,
          "%s n_qualified/n_rejected 一致 (prod=%s ref=%d/%d)"
          % (name, out["NQ"][:2], var.size, n_rej))
    check(int(out["NQ"][2]) == 1, "%s has_spatial_field=1（>=4 控制点）" % name)
    if cp.shape[0] and cp.shape[0] == var.size:
        close(cp[:, 0], xs, RTOL_MODEL, "%s 控制点 x [px]" % name)
        close(cp[:, 1], ys, RTOL_MODEL, "%s 控制点 y [px]" % name)
        close(cp[:, 2], np.maximum(var, floor_v), RTOL_MODEL, "%s 控制点 variance [ADU^2]" % name)
        close(cp[:, 3], 1.0 / np.maximum(var, floor_v), RTOL_MODEL, "%s 控制点 ivar [ADU^-2]" % name)
        vg = max(float(np.median(var)), floor_v)
        close(float(out["VG"][0]), vg, RTOL_MODEL, "%s variance_bg_global" % name)
        close(float(out["VG"][1]), 1.0 / vg, RTOL_MODEL, "%s ivar_bg_global" % name)
        close(float(out["VG"][2]), math.sqrt(vg), RTOL_MODEL, "%s sigma_bg_global" % name)
        A = np.column_stack([np.ones_like(xs), xs, ys])
        coef, *_ = np.linalg.lstsq(A, np.maximum(var, floor_v), rcond=None)
        yy, xx = np.mgrid[0:h, 0:w]
        ref_var = np.maximum(coef[0] + coef[1] * xx + coef[2] * yy, floor_v)
        check(int(out["FILLRC"][0]) == 0, "%s fill rc=0" % name)
        close(out["FV"], ref_var.reshape(-1), RTOL_PLANE,
              "%s fill variance 平面 [ADU^2, f32]" % name)
        close(out["FI"], (1.0 / ref_var).reshape(-1), RTOL_PLANE,
              "%s fill ivar 平面 [ADU^-2, f32]" % name)
    return out


def main():
    if not shutil.which("g++"):
        print("SKIP: 缺 g++，无法编译被测生产源")
        return 0
    for p in (SNR_INC, SNR_SRC):
        if not os.path.isdir(p):
            print("FAIL 生产源路径不存在: %s" % p)
            return 1
    tmp = tempfile.mkdtemp(prefix="p1noise_numpy_oracle_")
    try:
        drv = os.path.join(tmp, "d.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER)
        exe = os.path.join(tmp, "d")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-I" + SNR_INC, drv,
                            os.path.join(SNR_SRC, "noise_model.cpp"),
                            os.path.join(SNR_SRC, "snr_science.cpp"),
                            "-pthread", "-o", exe],
                           capture_output=True, text=True, timeout=600)
        if r.returncode != 0:
            print("FAIL g++ 编译失败: %s" % r.stderr[-800:])
            return 1

        rng = np.random.default_rng(20260916)
        H = W = 64
        yy, xx = np.mgrid[0:H, 0:W]

        plane = 1000.0 + 0.5 * xx + 0.25 * yy
        a = plane + rng.normal(0.0, 4.0, size=(H, W))
        flat = a.reshape(-1)
        idx = np.arange(0, flat.size, 97)
        flat[idx] += np.where(np.arange(idx.size) % 2 == 0, 200.0, -200.0)
        data_a = flat.reshape(H, W)

        data_b = 1200.0 + rng.normal(0.0, 7.3, size=(H, W))

        out_a = run_case(exe, "A_plane_outlier", data_a, 8, 8, 32, 5.0, 2, 1e-12, tmp)
        out_b = run_case(exe, "B_blank_gauss", data_b, 8, 8, 32, 5.0, 2, 1e-12, tmp)

        # (2) 冻结默认值契约（R-5 §5.13②）：default_config 逐字段必须等于
        # SCI/ALG/registry 冻结值。"min_samples 5 vs 64" 这类 SCI↔实现冲突
        # 在本断言下必红（旧版用例逐字段覆盖默认值 ⇒ 零区分力）。
        if "DEFCFG" in out_a:
            d = out_a["DEFCFG"]
            check(int(d[0]) == 8 and int(d[1]) == 8, "default_config patch_grid 8x8")
            check(abs(float(d[2]) - 10.0) < 1e-12 and abs(float(d[3]) - 6.0) < 1e-12,
                  "default_config source_mask_radius_px=10 / mask_radius_scale=6 (rmax=60px)")
            check(abs(float(d[4])) < 1e-300 and abs(float(d[5])) < 1e-300,
                  "default_config gain_e_per_adu=0 / read_noise_e=0 (DISP-NOISE-003 现状)")
            check(int(d[6]) == 64, "default_config min_patch_samples==64 (SCI-NOISE-001 §4 冻结)")
            # MASK-002 (claim SC-009): ABI 头部 + 逐星掩膜默认值必须与 SCI §5a 一致。
            # 旧断言把 10/6 当"rmax=60px 即掩膜半径"的合同冻结 —— 那是被证伪的
            # 「统一半径」语义; 新合同 = 60 px **硬上界** + 5 个 §5a 参数。
            check(int(d[10]) > 0 and int(d[11]) == 1,
                  "default_config ABI 头部在场 (struct_size=%s abi_version=%s)"
                  % (d[10], d[11]))
            check(abs(float(d[12]) - 0.1) < 1e-12, "default_config mask_k_sigma==0.1 (§5a)")
            check(abs(float(d[13]) - 1.5) < 1e-12, "default_config mask_r_min_px==1.5")
            check(abs(float(d[14]) - 0.75) < 1e-12, "default_config mask_fwhm_floor_scale==0.75")
            check(int(d[15]) == 8, "default_config mask_budget_min_patches==8")
            check(int(d[16]) == 9216, "default_config mask_budget_min_sky==9216")
            check(int(d[7]) == 2, "default_config max_clip_rounds==2 (SCI §5 5sigma<=2 轮)")
            check(int(d[8]) == 1, "default_config enable_spatial_field==1")
            check(abs(float(d[9]) - 1e-12) < 1e-24, "default_config variance_floor==1e-12")

        # (3) 公式级偏差用例（R-5 §5.13③）：纯高斯帧、8x8 网格、每 patch 恰 64
        # 样本 ⇒ sigma_bg_global/sigma 必须落在 R-5 EXP-2 的 MC 区间
        # (E=0.983170, SE=0.022736, T=976, N=64/patch) 的 ±3sigma 带内。
        # 该用例能判"公式/阈值错"（min_samples=5 时 E=0.7488 必红），而非仅"抄写错"。
        for seed_c in (11, 12, 13):
            rng_c = np.random.default_rng(seed_c)
            data_c = 1000.0 + rng_c.normal(0.0, 5.0, size=(64, 64))
            out_c = run_case(exe, "C_bias_n64_s%d" % seed_c, data_c, 8, 8, 64, 5.0, 2, 1e-12, tmp)
            if "VG" in out_c:
                g_rel = math.sqrt(float(out_c["VG"][0])) / 5.0
                check(0.91496 <= g_rel <= 1.05138,
                      "C_bias_n64 sigma_bg/sigma in MC band (seed=%d got=%.5f band=[0.91496,1.05138])"
                      % (seed_c, g_rel))


        # (4) MASK-002 源污染 oracle (含星帧; SCI-NOISE-001 §11, claim SC-009)
        # 结构性缺口: 旧 §11 只用无星纯高斯帧 ⇒ 掩膜半径改动在原理上不影响
        # 结果 (实测 r=0/8/60 px 输出逐位相同)。本用例注入 Moffat(β=2.5) 星场,
        # 由 NumPy **独立**复算 §5a 半径/掩膜/σ_bg:
        rng_s = np.random.default_rng(20260940)
        H4 = W4 = 512
        sigma4 = 5.0
        n_stars4 = 40
        fwhm4 = 3.0
        beta4 = MASK_BETA
        data4 = sigma4 * rng_s.normal(0.0, 1.0, size=(H4, W4))
        u4 = rng_s.uniform(0.0, 1.0, size=n_stars4)
        flux4 = 2.0e2 * (1.0e5 / 2.0e2) ** u4
        cx4 = rng_s.uniform(0.0, W4, size=n_stars4)
        cy4 = rng_s.uniform(0.0, H4, size=n_stars4)
        alpha4 = fwhm4 / (2.0 * math.sqrt(2.0 ** (1.0 / beta4) - 1.0))
        yy4, xx4 = np.mgrid[0:H4, 0:W4]
        for k in range(n_stars4):
            amp4 = flux4[k] * (beta4 - 1.0) / (math.pi * alpha4 * alpha4)
            data4 = data4 + amp4 * (1.0 + ((xx4 - cx4[k]) ** 2 + (yy4 - cy4[k]) ** 2)
                                    / (alpha4 * alpha4)) ** (-beta4)
        stars4 = np.column_stack([cx4, cy4, flux4, np.full(n_stars4, fwhm4)])
        out_d = run_case(exe, "D_starfield", data4, 8, 8, 64, 5.0, 2, 1e-12, tmp,
                         stars=stars4.reshape(-1))
        if "MASK" in out_d:
            # 第一遍 σ_seed 独立复算: 4·FWHM 掩膜 → 被掩膜像素置 NaN → 同 patch 管线
            seed_r = np.minimum(np.maximum(4.0 * fwhm4, 1.5), 60.0)
            mask_seed = np.zeros((H4, W4), dtype=bool)
            for k in range(n_stars4):
                mask_seed |= ((xx4 - int(round(float(cx4[k])))) ** 2
                              + (yy4 - int(round(float(cy4[k])))) ** 2) <= seed_r * seed_r
            data_seed = data4.copy()
            data_seed[mask_seed] = np.nan
            _xs, _ys, var_seed, _nr = reference_model(data_seed, H4, W4, 8, 8, 64, 5.0, 2, 1e-12)
            check(var_seed.size > 0, "D_starfield 第一遍 σ_seed 有合格 patch")
            sigma_seed = float(np.median(np.sqrt(var_seed))) if var_seed.size else 0.0
            r_local = np.array([moffat_r_local(float(flux4[k]), fwhm4, 0.1 * sigma_seed)
                                for k in range(n_stars4)])
            radii = np.clip(r_local, np.maximum(1.5, 0.75 * fwhm4), 60.0)
            close(float(out_d["MASK"][1]), float(np.median(radii)), 1e-9,
                  "D_starfield mask_radius_p50 (NumPy 独立 §5a 复算)")
            close(float(out_d["MASK"][2]), disk_mask_frac(W4, H4, cx4, cy4, radii), 1e-6,
                  "D_starfield mask_frac (NumPy 独立光栅化)")
            check(int(out_d["MASK"][0]) == 0, "D_starfield mask_degraded==0 (F/FWHM 齐备)")
            check(int(out_d["MASK"][3]) > 0 and int(out_d["MASK"][4]) == 1,
                  "D_starfield 模型 ABI 头部在场")
            sig_prod = math.sqrt(float(out_d["VG"][0]))
            bias4 = abs(sig_prod / sigma4 - 1.0)
            check(bias4 <= 0.02,
                  "D_starfield 源污染 |σ/σ_true−1| = %.4f ≤ 2%% (SCI §11)" % bias4)
            check(int(out_d["NQ"][0]) >= 8, "D_starfield n_qualified ≥ 8 (天空预算)")
            check((1.0 - float(out_d["MASK"][2])) * H4 * W4 >= 9216.0,
                  "D_starfield N_sky ≥ 9216 (天空预算)")

        if "VG" in out_a:
            sg_a = math.sqrt(float(out_a["VG"][0]))
            check(abs(sg_a - 4.0) <= 0.1 * 4.0, "A 稳健 σ≈4（离群被裁剪）got=%.4f" % sg_a)
        if "VG" in out_b:
            sg_b = math.sqrt(float(out_b["VG"][0]))
            check(abs(sg_b - 7.3) <= 0.05 * 7.3, "B σ≈7.3 got=%.4f" % sg_b)

        if "GV" in out_a:
            gv = [float(x) for x in out_a["GV"]]
            close(gv[0], 1000.0 / 3.0 + (2.0 / 3.0) ** 2, RTOL_MODEL, "GV Poisson+read (s>0)")
            close(gv[1], (2.0 / 3.0) ** 2, RTOL_MODEL, "GV Poisson+read (s<0 截断)")
            close(gv[2], 0.0, RTOL_MODEL, "GV gain<=0 → 0")
            sl = [float(x) for x in out_a["SL"]]
            close(sl[0], 9.0 * 4.0, RTOL_MODEL, "SL var'=alpha^2 var")
            close(sl[1], (1.0 / 9.0) / 4.0, RTOL_MODEL, "SL ivar'=ivar/alpha^2")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if G_FAIL:
        print("NUMPY-ORACLE FAIL: %d/%d 项不通过" % (G_FAIL, G_TOTAL))
        return 1
    print("NUMPY-ORACLE PASS: %d/%d 项通过（rtol model=%.0e / plane f32=%.0e）"
          % (G_TOTAL, G_TOTAL, RTOL_MODEL, RTOL_PLANE))
    return 0


if __name__ == "__main__":
    sys.exit(main())
