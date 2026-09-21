#!/usr/bin/env python3
"""p3_proj_wcs_oracle.py — IMPL-P3-PROJ-001 独立 WCS Oracle

真值来源（不 import / 不链接被测实现）:
  * astropy.wcs（WCSLIB，FITS WCS Paper II 独立实现）做逐点绝对对拍；
  * 自实现球面四边形盈余（Van Oosterom & Strackee）做逐像素 Ω；
  * 自实现 CAR 解析纬度带 Ω = Δα_rad (sin δ_hi − sin δ_lo)（仅 |CRVAL2|≤1e-9）；
  * CRPIX↔CRVAL 定义性不变量（Paper I §2.1.1）与 AIT 椭圆域 A≤1（Paper II）。

被测面: eng/tests/unit/v6_p3_proj/v6_p3_proj_probe.cpp 打印的 CSV（生产实现输出）。

用法:
  python3 p3_proj_wcs_oracle.py run        --probe <probe_bin> [--json out.json]
  python3 p3_proj_wcs_oracle.py mutate NAME --probe <probe_bin>
  python3 p3_proj_wcs_oracle.py mutate-all  --probe <probe_bin>
退出码: run 全过 0 否则 1; mutate 检出缺陷(红) 1，未检出 0; mutate-all 全检出 0 否则 1。
"""
import argparse
import json
import math
import subprocess
import sys

import numpy as np

try:
    from astropy.wcs import WCS
    HAVE_ASTROPY = True
except Exception:  # pragma: no cover
    HAVE_ASTROPY = False

DEG = math.pi / 180.0

# case = (proj, ra0, dec0, scale, w, h, grid_n)
# dec0≠0 用例是 CRVAL2 进入映射（Paper II §2.2 三 Euler 角）的唯一区分面：
# dec0≡0 时恒等旋转与标准解重合，对 CRVAL2 缺陷零区分力（R-1 §4-A）。
CASES = [
    ("TAN", 350.0, 30.0, 0.002, 97, 89, 5),
    ("SIN", 350.0, 30.0, 0.02, 97, 89, 5),
    ("CAR", 0.0, 0.0, 0.2, 16, 601, 3),
    ("CAR", 0.0, 0.0, 1.0, 97, 97, 3),
    ("CAR", 180.0, 0.0, 1.0, 97, 97, 3),
    ("AIT", 0.0, 0.0, 0.2, 61, 61, 3),
    ("TAN", 0.0, 60.0, 0.2, 61, 61, 3),
    ("TAN", 0.0, 0.0, 0.2, 11, 11, 3),
    # --- dec0≠0（CRVAL2 进映射 + LONPOLE 标准默认 0/180）---
    ("CAR", 10.0, 30.0, 0.2, 17, 17, 3),
    ("CAR", 10.0, -30.0, 0.2, 17, 17, 3),
    ("CAR", 10.0, 60.0, 0.2, 17, 17, 3),
    ("AIT", 10.0, 30.0, 0.2, 17, 17, 3),
    ("AIT", 10.0, -30.0, 0.2, 17, 17, 3),
    ("AIT", 200.0, -45.0, 0.2, 17, 17, 3),
]

MUTATIONS = ["const_omega", "legacy_ait", "legacy_car", "swap_norm", "naive_wrap"]


def run_probe(probe, case):
    proj, ra0, dec0, scale, w, h, gn = case
    out = subprocess.run(
        [probe, proj, repr(ra0), repr(dec0), repr(scale), str(w), str(h), str(gn)],
        check=True, capture_output=True, text=True, timeout=600)
    head = omega_stat = plan = None
    crpixw = None
    rows, rts, norms = [], [], []
    for line in out.stdout.splitlines():
        p = line.split()
        if not p:
            continue
        kv = lambda: {t.split("=")[0]: t.split("=")[1] for t in p[1:]}
        if p[0] == "V6PROBE":
            head = kv()
        elif p[0] == "OMEGA_STAT":
            omega_stat = kv()
        elif p[0] == "ROW":
            rows.append(kv())
        elif p[0] == "RT":
            rts.append(kv())
        elif p[0] == "PLAN":
            plan = kv()
        elif p[0] == "CRPIXW":
            crpixw = kv()
        elif p[0] == "NORM":
            norms.append({"kind": "NORM", **kv()})
        elif p[0] == "NORM_ROWSUM":
            norms.append({"kind": "ROWSUM", **kv()})
        elif p[0] == "NORM_COLSUM":
            norms.append({"kind": "COLSUM", **kv()})
        elif p[0] == "NORM_S":
            norms.append({"kind": "NORM_S", **kv()})
    return head, omega_stat, rows, rts, plan, norms, crpixw


def v_sky(ra_deg, dec_deg):
    a = np.asarray(ra_deg, float) * DEG
    d = np.asarray(dec_deg, float) * DEG
    return np.stack([np.cos(d) * np.cos(a), np.cos(d) * np.sin(a), np.sin(d)], axis=-1)


def tri_area(a, b, c):
    num = np.abs(np.einsum("...i,...i->...", a, np.cross(b, c)))
    den = (1.0 + np.einsum("...i,...i->...", a, b) + np.einsum("...i,...i->...", b, c)
           + np.einsum("...i,...i->...", c, a))
    return 2.0 * np.arctan2(num, den)


def quad_area(v00, v10, v11, v01):
    return tri_area(v00, v10, v11) + tri_area(v00, v11, v01)


def astropy_wcs(head):
    w = WCS(naxis=2)
    proj = head["proj"]
    w.wcs.ctype = ["RA---" + proj, "DEC--" + proj]
    w.wcs.crval = [float(head["crval1"]), float(head["crval2"])]
    w.wcs.crpix = [float(head["crpix1"]), float(head["crpix2"])]
    w.wcs.cd = [[float(head["cd11"]), float(head["cd12"])],
                [float(head["cd21"]), float(head["cd22"])]]
    w.wcs.set()
    return w


def grid_omega_astropy(w, wd, ht):
    X, Y = np.meshgrid(np.arange(wd, dtype=float), np.arange(ht, dtype=float))
    vs = []
    for dx, dy in [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]:
        ra, dec = w.all_pix2world(X + dx, Y + dy, 0)
        vs.append(v_sky(ra, dec))
    return quad_area(vs[0], vs[1], vs[2], vs[3])


def check_case(probe, case, injections):
    proj, ra0, dec0, scale, wd, ht, gn = case
    head, omega_stat, rows, rts, plan, norms, crpixw = run_probe(probe, case)
    checks = []
    if head is None or head["status_make"] != "0":
        return False, ["make_status FAIL"], {}
    if not HAVE_ASTROPY:
        return False, ["astropy unavailable"], {}
    w = astropy_wcs(head)

    # 1) 采样点 pix2world + 逐像素 Ω vs astropy
    worst_sep = 0.0
    prod_om, ref_om = [], []
    for r in rows:
        if r["status"] != "0":
            continue
        x, y = float(r["x"]), float(r["y"])
        ra_a, dec_a = w.all_pix2world([[x, y]], 0)[0]
        ddec = abs(dec_a - float(r["dec"]))
        dra = abs(ra_a - float(r["ra"]))
        dra = min(dra, 360.0 - dra)
        if "legacy_car" in injections and proj == "CAR":
            ddec = abs((-dec_a) - float(r["dec"]))   # 缺陷: dec 反号
        worst_sep = max(worst_sep, dra, ddec)
        vs = []
        for dx, dy in [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]:
            ra, dec = w.all_pix2world([[x + dx, y + dy]], 0)[0]
            vs.append(v_sky(ra, dec))
        prod_om.append(float(r["omega_excess"]))
        ref_om.append(float(quad_area(*vs)))
    checks.append(("pix2world_vs_astropy", worst_sep < 1e-8, worst_sep))
    if "const_omega" in injections and prod_om:
        prod_om = [prod_om[0]] * len(prod_om)   # 缺陷: 常数 Ω
    if "legacy_ait" in injections and proj == "AIT":
        prod_om = [v * 0.5 for v in prod_om]    # 缺陷: legacy 缺 √2
    om_rel = max((abs(a - b) / b for a, b in zip(prod_om, ref_om)), default=0.0) \
        if prod_om else 1.0
    checks.append(("omega_vs_astropy", om_rel < 1e-6, om_rel))

    # 2) 全网格 Ω ratio vs astropy
    om_grid = grid_omega_astropy(w, wd, ht)
    ref_ratio = float(np.nanmax(om_grid) / np.nanmin(om_grid))
    prod_ratio = float(omega_stat["ratio"])
    checks.append(("omega_ratio_vs_astropy",
                   abs(prod_ratio - ref_ratio) / ref_ratio < 1e-4,
                   (prod_ratio, ref_ratio)))

    # 3) CAR 解析纬度带交叉（小像素适用；球面盈余 vs 纬度带小圆边的曲率差
    #    随像素角尺度增大：0.2° 场残差 ~1e-6，1° 场 ~2.5e-5）。
    #    ⚠ 量测域: 仅 |CRVAL2| ≤ 1e-9（未倾斜 CAR）成立——CRVAL2≠0 时行是倾斜
    #    等纬线，Ω ≠ s_ra(sinδ_hi−sinδ_lo)，该判据对任何正确实现都误判
    #    （R-1 §4-B：δ0=30° 偏差 17.36%；δ0=60° 偏差 110.2%）。
    if (proj == "CAR" and abs(float(head["crval2"])) <= 1e-9
            and abs(float(head["cd11"])) <= 0.5):
        decs = float(head["crval2"]) + (np.arange(ht, dtype=float) -
                                        (float(head["crpix2"]) - 1.0)) * float(head["cd22"])
        s_ra = abs(float(head["cd11"])) * DEG
        lo = (decs - 0.5 * abs(float(head["cd22"]))) * DEG
        hi = (decs + 0.5 * abs(float(head["cd22"]))) * DEG
        analytic = s_ra * (np.sin(hi) - np.sin(lo))
        ref_col = om_grid[:, wd // 2]
        rel = float(np.max(np.abs(ref_col - analytic) / analytic))
        checks.append(("car_analytic_latitude_band_untilte", rel < 1e-5, rel))

    # 3b) CRPIX↔CRVAL 定义性不变量（Paper I §2.1.1；比球面距离更硬）：
    #     world2pix 中间坐标为 0 的像素（CRPIX）其天球坐标必须 == CRVAL。
    if crpixw is None:
        checks.append(("crpix_world_eq_crval", False, "no CRPIXW line"))
    else:
        st_c = int(crpixw["status"])
        if st_c != 0:
            checks.append(("crpix_world_eq_crval", False, "status=%d" % st_c))
        else:
            d_ra = abs(float(crpixw["ra"]) - float(head["crval1"]))
            d_ra = min(d_ra, 360.0 - d_ra)
            d_dec = abs(float(crpixw["dec"]) - float(head["crval2"]))
            checks.append(("crpix_world_eq_crval", max(d_ra, d_dec) < 1e-9,
                           (d_ra, d_dec)))

    # 3c) AIT 椭圆域（Paper II）：A = xp²/4 + yp²，A ≤ 1 域内 / A > 1 必须 HEMISPHERE。
    #     判据只用标准椭圆 + 探针平面坐标，不引用实现内部状态。
    if proj == "AIT":
        cd11, cd12 = float(head["cd11"]), float(head["cd12"])
        cd21, cd22 = float(head["cd21"]), float(head["cd22"])
        cx, cy = float(head["crpix1"]), float(head["crpix2"])
        worst_dom = 0.0
        bad_dom = 0
        for r in rows:
            x, y = float(r["x"]), float(r["y"])
            xd = cd11 * ((x + 1) - cx) + cd12 * ((y + 1) - cy)
            yd = cd21 * ((x + 1) - cx) + cd22 * ((y + 1) - cy)
            xp = xd * DEG / math.sqrt(2.0)
            yp = yd * DEG / math.sqrt(2.0)
            a_ell = xp * xp / 4.0 + yp * yp
            inside = a_ell <= 1.0
            if inside != (r["status"] == "0"):
                bad_dom += 1
                worst_dom = max(worst_dom, a_ell)
        checks.append(("ait_ellipse_domain_A_le_1", bad_dom == 0,
                       (bad_dom, worst_dom)))

    # 4) 往返
    worst_rt = max((float(r["err"]) for r in rts if r["status"] == "0"), default=0.0)
    checks.append(("roundtrip_lt_1e-6px", worst_rt < 1e-6, worst_rt))

    # 5) plan wrap 独立复算
    X, Y = np.meshgrid(np.arange(wd, dtype=float), np.arange(ht, dtype=float))
    ra_g, _ = w.all_pix2world(X, Y, 0)
    ra_flat = np.sort(np.ravel(ra_g) % 360.0)
    raw_span = float(ra_flat[-1] - ra_flat[0])
    max_gap = max(float(ra_flat[0] + 360.0 - ra_flat[-1]), float(np.diff(ra_flat).max()))
    circ = 360.0 - max_gap
    wrap_ref = (raw_span > 180.0) and (circ < raw_span - 1e-9)
    wrap_prod = plan["wrap"] == "1"
    if "naive_wrap" in injections:
        wrap_prod = False
    checks.append(("ra_wrap_vs_astropy", wrap_prod == wrap_ref, (wrap_prod, wrap_ref)))

    # 6) NORM R/S 独立复算
    rs = {int(n["i"]): float(n["v"]) for n in norms if n["kind"] == "ROWSUM"}
    cs = {int(n["j"]): float(n["v"]) for n in norms if n["kind"] == "COLSUM"}
    s_rows = {(int(n["i"]), int(n["j"])): float(n["v"]) for n in norms if n["kind"] == "NORM_S"}
    s2_rows = {(int(n["i"]), int(n["j"])): float(n["s2"]) for n in norms if n["kind"] == "NORM_S"}
    if "swap_norm" in injections:
        rs = {k: 0.5 for k in rs}
    worst_row = max((abs(v - 1.0) for v in rs.values()), default=1.0)
    worst_col = max((abs(v - 1.0) for v in cs.values()), default=1.0)
    worst_rtc = max((abs(s_rows[k] - s2_rows[k]) for k in s_rows), default=1.0)
    checks.append(("norm_row_sums", worst_row < 1e-12, worst_row))
    checks.append(("norm_col_sums", worst_col < 1e-12, worst_col))
    checks.append(("norm_row_to_col", worst_rtc < 1e-14, worst_rtc))

    ok = all(c[1] for c in checks)
    return ok, ["%s=%s(%s)" % (n, "PASS" if v else "FAIL", val) for n, v, val in checks], {}


def all_cases(probe, injections=frozenset()):
    results = []
    all_ok = True
    for c in CASES:
        ok, checks, _ = check_case(probe, c, injections)
        all_ok = all_ok and ok
        name = "%s_%g_%g_%g_%dx%d" % (c[0], c[1], c[2], c[3], c[4], c[5])
        results.append({"case": name, "ok": ok, "checks": checks})
        print("CASE %-26s %s" % (name, "PASS" if ok else "FAIL"))
        if not ok:
            for s in checks:
                if "FAIL" in s:
                    print("    " + s)
    return all_ok, results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["run", "mutate", "mutate-all"])
    ap.add_argument("name", nargs="?")
    ap.add_argument("--probe", required=True)
    ap.add_argument("--json")
    args = ap.parse_args()

    if args.mode == "run":
        ok, results = all_cases(args.probe)
        if args.json:
            json.dump({"ok": ok, "astropy": HAVE_ASTROPY, "results": results},
                      open(args.json, "w"), indent=1)
        print("ORACLE %s" % ("PASS" if ok else "FAIL"))
        return 0 if ok else 1

    if args.mode == "mutate":
        name = args.name
        if name not in MUTATIONS:
            print("unknown mutation %s" % name)
            return 2
        ok, _ = all_cases(args.probe, frozenset([name]))
        detected = not ok
        print("MUTATION %s %s" % (name, "CAUGHT(red)" if detected else "NOT-DETECTED"))
        return 1 if detected else 0

    all_detected = True
    for name in MUTATIONS:
        ok, _ = all_cases(args.probe, frozenset([name]))
        detected = not ok
        all_detected = all_detected and detected
        print("MUTATION %-12s %s" % (name, "CAUGHT" if detected else "NOT-DETECTED"))
    print("MUTATE-ALL %s" % ("PASS" if all_detected else "FAIL"))
    return 0 if all_detected else 1


if __name__ == "__main__":
    sys.exit(main())
