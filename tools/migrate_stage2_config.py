#!/usr/bin/env python3
# tools/migrate_stage2_config.py — V17 stage2 config 迁移（一次性）
#
# V17 schema v2：rejection 段删除 low/high/max_iterations/min_samples，
# profile 改为版本化（wbpp_current → wbpp_2_9_1），normalization 改为
# astrocs_*_v1（median_center → astrocs_median_center_v1）。
#
# 用法：
#   py -3.12 tools/migrate_stage2_config.py <old.json> <new.json>
import json
import sys
from pathlib import Path


def migrate(obj):
    o = json.loads(json.dumps(obj))  # deep copy
    o.setdefault("version", 2)
    rej = o.get("integration", {}).get("rejection", {})
    if rej:
        legacy = {k: rej.pop(k) for k in
                  ("low", "high", "max_iterations", "min_samples")
                  if k in rej}
        if legacy:
            method = rej.get("method", "sigma")
            # 旧 low/high/max_iterations → method-specific typed
            lo = abs(float(legacy.get("low", 4.0)))
            hi = abs(float(legacy.get("high", 3.0)))
            mi = int(legacy.get("max_iterations", 8))
            if "min_samples" in legacy:
                rej["underdetermined_n"] = int(legacy["min_samples"])
            def sig(**kw):
                return {"lower_sigma": kw.get("lower", lo),
                        "upper_sigma": kw.get("upper", hi),
                        "max_iterations": mi}
            if method in ("sigma", "winsorized_sigma", "averaged_sigma",
                          "median_sigma"):
                rej[method if method != "sigma" else "robust_mad_clip"] = sig()
            elif method == "linear_fit":
                rej["linear_fit"] = {"lower": lo, "upper": hi,
                                     "max_iterations": mi}
            elif method == "generalized_esd":
                rej["generalized_esd"] = {"alpha": 0.05,
                                          "max_outliers": mi}
            elif method == "percentile":
                rej["percentile"] = {"low_fraction": lo,
                                     "high_fraction": hi}
        # profile / normalization 规范化
        if rej.get("profile") == "wbpp_current":
            rej["profile"] = "wbpp_2_9_1"
        if rej.get("normalization") == "median_center":
            rej["normalization"] = "astrocs_median_center_v1"
        elif rej.get("normalization") == "median_scale":
            rej["normalization"] = "astrocs_median_scale_v1"
    return o


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    obj = json.loads(src.read_text(encoding="utf-8"))
    new = migrate(obj)
    dst.write_text(json.dumps(new, indent=2, ensure_ascii=False),
                   encoding="utf-8")
    print(f"migrated: {src} -> {dst} (version={new.get('version')})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
