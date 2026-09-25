"""AUDIT-06 第②层：逐数定位——二阶"不可用"的三个实测数各自的 results 出处。"""
import io
import json
import os
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
DATA = os.path.join(REPO, "实验", "photometric-magnitude", "code",
                    "reverse_verify", "p1_spatial_gain", "data")
sys.stdout.reconfigure(encoding="utf-8")


def load(n):
    with io.open(os.path.join(DATA, n), encoding="utf-8") as f:
        return json.load(f)


# 1) 聚集星分布 + N=100 的合成配置身份
s = load("synth_results.json")
print("== synth_results.nstar_sensitivity.configs 身份表 ==")
for i, c in enumerate(s["nstar_sensitivity"]["configs"]):
    lbl = {k: v for k, v in c.items() if not isinstance(v, (dict, list))}
    o2 = c.get("summary", {}).get("orders", {}).get("2", {})
    shape = o2.get("m_shape_err_A_rms_pct", {})
    print("cfg[%d] %s | o2 shapeRMS mean=%s median=%s" % (
        i, lbl, ("%.3f" % shape["mean"]) if "mean" in shape else "-",
        ("%.3f" % shape["median"]) if "median" in shape else "-"))

print("\n== doc §3.5 表定位：N=100 聚集 (o2 shape) ==")
for i, c in enumerate(s["nstar_sensitivity"]["configs"]):
    lbl = {k: v for k, v in c.items() if not isinstance(v, (dict, list))}
    if str(lbl.get("nstars")) == "100" or str(lbl.get("n")) == "100":
        print("  cfg[%d] %s" % (i, lbl))
        print("  orders keys:", list(c["summary"]["orders"].keys()))
        for o in ("1", "2"):
            oo = c["summary"]["orders"].get(o, {})
            print("   order%s m_shape_err_A_rms_pct=%s field_ptp_after=%s" % (
                o, oo.get("m_shape_err_A_rms_pct"),
                {k: v for k, v in oo.items() if "ptp" in k}))

# 2) 真实数据：接缝（相邻板块）半样本交叉验证
g = load("real_gain.json")
print("\n== real_gain.json cross_validation ==")
cv = g["cross_validation"]
print(json.dumps(cv, ensure_ascii=False, indent=1)[:2500])

# 3) 孔径探针
ap = load("aperture_probe.json")
print("\n== aperture_probe.json pairs ==")
for p in ap["pairs"]:
    ident = {k: v for k, v in p.items() if not isinstance(v, (dict, list))}
    print(" pair:", ident)
    for r, d in sorted(p.get("radii", {}).items()):
        print("   r=%s -> %s" % (r, {k: v for k, v in d.items()
                                     if not isinstance(v, (dict, list))}))

# 4) 像素级独立孔径复核
pc = load("real_pixel_check.json")
print("\n== real_pixel_check.json pairs (前 4) ==")
for p in pc["pairs"][:4]:
    ident = {k: v for k, v in p.items() if not isinstance(v, (dict, list))}
    print(" pair:", ident)
    for kk, vv in sorted(p.get("orders", {}).items()):
        if "o1" in kk or "o2" in kk:
            print("   %s -> %s" % (kk, {k: v for k, v in vv.items()
                                        if not isinstance(v, (dict, list))}))
