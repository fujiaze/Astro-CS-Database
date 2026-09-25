"""AUDIT-06 第②层独立复核：核对 p1-spatial-gain 三个"二阶不可用"实测数是否有 results 支撑。

只读仓库内的 JSON 结果文件（不 import 仓库 Python、不跑仓库检查器）。
判据：文档散文里的每个数，必须能在跟踪的 results 里找到同值字段。
"""
import io
import json
import os
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
DATA = os.path.join(REPO, "实验", "photometric-magnitude", "code",
                    "reverse_verify", "p1_spatial_gain", "data")


def load(name):
    with io.open(os.path.join(DATA, name), encoding="utf-8") as f:
        return json.load(f)


def walk(obj, path=""):
    """yield (path, value) for every scalar leaf."""
    if isinstance(obj, dict):
        for k, v in obj.items():
            yield from walk(v, path + "/" + str(k))
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from walk(v, path + "[%d]" % i)
    else:
        yield path, obj


def find_closest(data, target, tol=0.006, limit=12):
    hits = []
    for path, v in walk(data):
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            if abs(float(v) - target) <= tol:
                hits.append((path, v))
    return hits[:limit]


def main():
    out = []
    files = {}
    for n in ("real_ridge.json", "real_gain.json", "real_pixel_check.json",
              "synth_results.json", "aperture_probe.json", "real_analysis.json"):
        files[n] = load(n)
        out.append("loaded %s top-keys=%s" % (n, list(files[n])[:12]))

    out.append("")
    out.append("== 文档散文数 → results 复核（每个数找同值字段）==")
    # 文档 §3.4/§4.4 的数：order2 分半 corr 0.929 / p10 0.41 / min 0.304；
    # order1 corr 0.997；聚集 N=100 order2 形状 RMS 24.4%；
    # 接缝半样本 5.594% -> 7.031%；孔径探针 r=3/4/6/10 = 7.775/4.928/3.807/4.249
    probes = [
        ("real_ridge.json", 0.929, "o2 分半 corr 中位 (doc §3.4/§4.4a)"),
        ("real_ridge.json", 0.997, "o1 分半 corr 中位 (doc §3.4/§4.4a)"),
        ("real_ridge.json", 0.409, "o2 分半 corr p10 (doc 0.41)"),
        ("real_ridge.json", 0.304, "o2 分半 corr min (doc 0.30)"),
        ("synth_results.json", 0.244, "o2 聚集 N=100 形状 RMS (doc 24.4%)"),
        ("synth_results.json", 24.4, "o2 聚集 N=100 形状 RMS 百分数写法"),
        ("synth_results.json", 0.1318, "o2 聚集 N=100 残差场 PTP 13.18%"),
        ("real_gain.json", 0.07031, "接缝半样本 after o1 7.031%"),
        ("real_gain.json", 7.031, "接缝半样本 after o1 (百分数)"),
        ("real_gain.json", 9.781, "接缝半样本 after o2 (百分数)"),
        ("aperture_probe.json", 0.07775, "孔径探针 r=3 before 7.775%"),
        ("aperture_probe.json", 3.807, "孔径探针 r=6 before 3.807%"),
        ("real_pixel_check.json", 6.078, "独立孔径 r=6 after o1 变差 6.078%"),
    ]
    for fn, val, label in probes:
        tol = max(abs(val) * 0.004, 1e-6) if abs(val) > 1 else 0.0006
        hits = find_closest(files[fn], val, tol=tol)
        out.append("%-22s %-34s -> %s" % (fn, label, "HIT" if hits else "MISS"))
        for p, v in hits[:4]:
            out.append("      %s = %r" % (p[:150], v))

    out.append("")
    out.append("== 结构抽样：cross_validation / split_half 键名 ==")
    for fn in ("real_ridge.json", "real_gain.json"):
        d = files[fn]
        keys = [p for p, _ in walk(d) if any(
            t in p.lower() for t in ("split", "half", "corr", "cluster", "shape"))]
        seen = []
        for k in keys:
            base = k.split("[")[0]
            if base not in seen:
                seen.append(base)
        out.append("%s 含相关路径族: %s" % (fn, seen[:14]))

    sys.stdout.reconfigure(encoding="utf-8")
    print("\n".join(out))


if __name__ == "__main__":
    main()
