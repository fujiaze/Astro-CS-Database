#!/usr/bin/env python3
"""CAT-GAIA-TEST 独立 oracle（tests/oracle/gaia_oracle.py）。

参考值三层独立来源（绝不调用被测实现）：
  1. manifest.ndjson —— fixture 生成器按记录域精确反演的期望
     （dx/dy/mag_raw → 量化格点 → 投影反演 + dra 修正）。
  2. impl.json / polar_ref.json —— 被测实现与 GAIA_POLAR_PRUNE_DISABLED
     参考二进制（gaia_cat_polar_ref dump）的输出，互相差分（极区剪枝
     必须不影响极冠命中：bitwise 一致）。
  3. 本脚本内独立重实现的量化/投影公式（ALG §2 常量），按叶子
     (x0,y0)/center 重算每颗期望星的 (ra,dec,mag)，与 manifest 数值
     逐位复核（生成器自身 bug 在此拦截）。

用法:
  python3 tests/oracle/gaia_oracle.py <fixture_dir> <impl.json> <polar_ref.json>
退出码 0 = 全部一致；1 = 存在差异。
"""
import json
import math
import os
import sys

INV_SCALE = 1.0 / (3600.0 * 1000.0 * 500.0)   # 1/1.8e9 度/LSB（2 µas）
INV_DRA = 1.0 / (3600.0 * 1000.0 * 100.0)     # 1/3.6e8 度/LSB（10 µas）
POS_TOL = 5e-10
MAG_TOL = 1e-9


def unproject(x, y, proj, center_ra, center_dec):
    """ALG §2 投影反演（Equirect / AzimuthalEquidistant）独立实现。"""
    if proj == "Equirectangular":
        ra = center_ra + x
        ra = math.fmod(ra, 360.0)
        if ra < 0:
            ra += 360.0
        return ra, y
    r = math.hypot(x, y)
    cdec = math.radians(center_dec)
    if r < 1e-15:
        ra = center_ra
        dec = center_dec
    else:
        c = math.asin(min(1.0, max(-1.0, math.cos(math.radians(r)))))
        if cdec < 0:
            c = -c
        ra = center_ra + math.degrees(math.atan2(x * math.sin(r) / r,
                                                 y * math.sin(r) / r))
        dec = math.degrees(c)
    ra = math.fmod(ra, 360.0)
    if ra < 0:
        ra += 360.0
    return ra, dec


def quantized_ref(m):
    """fixture 记录域 → 期望 ra/dec/mag（独立重算，与 manifest 复核）。"""
    proj = m["proj"]
    xq = m["x0"] + m["dx"] * INV_SCALE
    yq = m["y0"] + m["dy"] * INV_SCALE
    ra, dec = unproject(xq, yq, proj, m["cx"], m["cy"])
    if m["dra_raw"] != 0:
        ra += m["dra_raw"] * INV_DRA
        ra = math.fmod(ra, 360.0)
        if ra < 0:
            ra += 360.0
    mag = m["mag_raw"] * 0.001 - 1.5
    return ra, dec, mag


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    fixture_dir, impl_path, ref_path = sys.argv[1:4]

    manifest_path = os.path.join(fixture_dir, "manifest.ndjson")
    manifest = []
    with open(manifest_path, encoding="utf-8") as f:
        for line in f:
            d = json.loads(line)
            if d.get("kind") == "star":
                manifest.append(d)
    print("oracle: manifest stars =", len(manifest))

    failures = 0

    # ---- O1 生成器反演 vs oracle 独立公式 ----
    # 三角函数跨 libm（C glibc vs Python）末位 ulp 差属实现细节：
    # 以冻结容差 |Δpos|≤5e-10 判定（远大于观测 ulp 噪声 ~1e-14）。
    exact = 0
    for m in manifest:
        ra, dec, mag = quantized_ref(m)
        d_ra = abs(ra - m["ra"])
        sep = 360.0 - d_ra if d_ra > 180.0 else d_ra
        if (sep <= POS_TOL and abs(dec - m["dec"]) <= POS_TOL
                and mag == m["mag"]):
            exact += 1
        else:
            print("O1 MISMATCH %s: oracle(%.17g,%.17g,%.17g) vs manifest(%.17g,%.17g,%.17g)"
                  % (m["id"], ra, dec, mag, m["ra"], m["dec"], m["mag"]))
    print("O1 generator-inverse vs oracle-formula: %d/%d within tol"
          % (exact, len(manifest)))
    failures += len(manifest) - exact

    # ---- O2 实现 dump vs manifest（双向 1:1 集合） ----
    with open(impl_path, encoding="utf-8") as f:
        impl = json.load(f)
    impl_stars = [(s["ra"], s["dec"], s["magG"]) for s in impl["stars"]]

    def match(pool, tag_m, used):
        for i, s in enumerate(pool):
            if used[i]:
                continue
            d = abs(s[0] - tag_m[0])
            sep = 360.0 - d if d > 180.0 else d
            if (sep <= POS_TOL and abs(s[1] - tag_m[1]) <= POS_TOL
                    and abs(s[2] - tag_m[2]) <= MAG_TOL):
                used[i] = True
                return True
        return False

    used = [False] * len(impl_stars)
    miss = 0
    for m in manifest:
        if not match(impl_stars, (m["ra"], m["dec"], m["mag"]), used):
            miss += 1
    false_pos = sum(1 for u in used if not u)
    print("O2 impl-vs-manifest: miss=%d false_pos=%d (impl=%d)"
          % (miss, false_pos, len(impl_stars)))
    failures += miss + false_pos
    if impl.get("file_count") != 2 or impl.get("total") != len(manifest):
        failures += 1
        print("O2 header mismatch: file_count=%s total=%s (expect 2/%d)"
              % (impl.get("file_count"), impl.get("total"), len(manifest)))

    # ---- O3 极区剪枝差分：GAIA_POLAR_PRUNE_DISABLED 参考必须 bitwise 一致 ----
    with open(ref_path, encoding="utf-8") as f:
        ref = json.load(f)

    def key(s):
        return (repr(s["ra"]), repr(s["dec"]), repr(s["magG"]))

    impl_polar = sorted(impl["polar"], key=key)
    ref_polar = sorted(ref["polar"], key=key)
    if len(impl_polar) != len(ref_polar):
        failures += 1
        print("O3 polar count %d != %d" % (len(impl_polar), len(ref_polar)))
    else:
        bad = 0
        for a, b in zip(impl_polar, ref_polar):
            if (repr(a["ra"]) != repr(b["ra"]) or repr(a["dec"]) != repr(b["dec"])
                    or repr(a["magG"]) != repr(b["magG"]) or a["spec"] != b["spec"]):
                bad += 1
        print("O3 polar bitwise diff: %d/%d stars" % (bad, len(impl_polar)))
        failures += bad

    # ---- O4 主/极区两路结果一致性（同一客户端内 pos bitwise） ----
    polar_pos = {(repr(s["ra"]), repr(s["dec"])) for s in impl["polar"]}
    sky_pos = {(repr(s["ra"]), repr(s["dec"])) for s in impl["stars"]}
    missing_polar = polar_pos - sky_pos
    print("O4 polar stars present in SKY dump: %d/%d"
          % (len(polar_pos) - len(missing_polar), len(polar_pos)))
    failures += len(missing_polar)

    print("oracle: failures =", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
