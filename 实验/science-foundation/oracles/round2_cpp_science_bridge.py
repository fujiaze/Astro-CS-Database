"""Apply independent science propagation to the actual round-2 C++ area bounds.

The scalar oracle does not import the C++ geometry. Native/drop TAN areas are
computed independently as high-precision interval solid angles of two exact
gnomonic spherical triangles. This is valid for TAN without SIP only.
Analytic HEALPix quarter/full-face overlaps provide independent science truth.
Other leaves have conditional enclosures only, with no invented actual error.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import platform
import time
import mpmath as mp
from round2_science_budget import (Q, Interval, rat, evaluate_integrated,
                                  covariance_entry_error, dot, matvec)

mp.iv.dps = 60


def iv_exact(value):
    value = rat(value)
    return mp.iv.mpf(value.numerator)/mp.iv.mpf(value.denominator)


def endpoint_q(t):
    sign, man, exponent, _ = t
    out = Q(-man if sign else man)
    return out*2**exponent if exponent >= 0 else out/Q(2**(-exponent))


def enclosed(value):
    return Interval(endpoint_q(value._mpi_[0]), endpoint_q(value._mpi_[1]))


def vdot(a, b):
    return sum((aa*bb for aa, bb in zip(a, b)), mp.iv.mpf(0))


def det3(a, b, c):
    return (a[0]*(b[1]*c[2]-b[2]*c[1])
            -a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0]))


def triangle_area(a, b, c):
    na, nb, nc = [mp.iv.sqrt(vdot(v, v)) for v in (a, b, c)]
    numerator = abs(det3(a, b, c))
    denominator = na*nb*nc+vdot(a, b)*nc+vdot(b, c)*na+vdot(c, a)*nb
    return 2*mp.iv.atan2(numerator, denominator)


def tan_area(header, center_xy0=(0, 0), pixfrac=1):
    """No SIP: detector edges map to exact great-circle arcs under TAN."""
    if "SIP" in header["CTYPE1"]:
        raise ValueError("independent analytic area supports no SIP")
    cd = [[iv_exact(header[f"CD{i+1}_{j+1}"])*mp.iv.pi/180
           for j in range(2)] for i in range(2)]
    vertices = []
    half = rat(pixfrac)/2
    for dx, dy in ((-half, -half), (half, -half), (half, half), (-half, half)):
        u = iv_exact(rat(center_xy0[0])+dx+1-rat(header["CRPIX1"]))
        v = iv_exact(rat(center_xy0[1])+dy+1-rat(header["CRPIX2"]))
        vertices.append([cd[0][0]*u+cd[0][1]*v,
                         cd[1][0]*u+cd[1][1]*v, mp.iv.mpf(1)])
    return enclosed(triangle_area(*vertices[:3])+triangle_area(
        vertices[0], vertices[2], vertices[3]))


def summarize_case(case, selected_leaves, analytic_truth=None):
    req = case["request"]
    first = case["measurement"]["first"]
    offset = Q(1-int(req["side"]), 2)
    omega = tan_area(req["header"], (offset, offset), 1)
    drop = tan_area(req["header"], (offset, offset), req["pixfrac"])
    leafarea = enclosed(mp.iv.pi/(3*int(req["nside"])**2))
    # Native uncertainty corresponds to .01/.02 L/sr and shared rho=.1.
    c = [[(omega.mid/100)**2, omega.mid*leafarea.mid/50000],
         [omega.mid*leafarea.mid/50000, (leafarea.mid/50)**2]]
    rows = []
    for leaf in selected_leaves:
        aa = Interval(*first["areas"][str(leaf)])
        # An ordinary full-leaf contributor supplies nonzero normalization.
        # It is a deliberate synthetic reference region, not a second detector
        # TAN footprint. Exact specified native integrals yield surface values
        # equal to 1e9 and -2 up to the independently bounded area evaluation.
        signal = [Q(10**9)*omega.mid, Q(-2)*leafarea.mid]
        got = evaluate_integrated([aa, leafarea], [drop, leafarea],
                                  [omega, leafarea], signal,
                                  absolute_tolerance="1e-3")
        r0 = got["r_estimate"]
        ee = [max(abs(ri.lo-v), abs(ri.hi-v)) for ri, v in zip(got["r"], r0)]
        cv0 = dot(r0, matvec(c, r0))
        cvbound = covariance_entry_error(ee, ee, r0, r0, c)
        row = {"leaf": int(leaf), "cpp_area_bound_sr": aa.dump(),
               "normalization_sr": got["normalization"].dump(),
               "surface_L_per_sr": got["surface"].dump(),
               "surface_estimate": float(got["estimate"]),
               "native_integrals_L": list(map(float, signal)),
               "surface_error_bound": float(got["exact_box_error_bound"]),
               "surface_target": .001, "science_status": got["science_status"],
               "R_bounds_per_sr": [v.dump() for v in got["r"]],
               "variance_center_L2_per_sr2": float(cv0),
               "variance_geometry_error_bound": float(cvbound),
               "true_surface": None, "actual_surface_error": None,
               "actual_variance_error": None}
        if analytic_truth is not None:
            atrue = analytic_truth
            assert aa.lo <= atrue.lo <= atrue.hi <= aa.hi
            wt = atrue/drop
            nt = wt*omega+leafarea
            rt = [wt/nt, Interval(1, 1)/nt]
            bt = rt[0]*signal[0]+rt[1]*signal[1]
            vt = sum_intervals(rt[i]*rt[j]*c[i][j] for i in range(2) for j in range(2))
            assert got["surface"].lo <= bt.lo <= bt.hi <= got["surface"].hi
            berr = Interval(max(Q(0), abs(got["estimate"]-bt.mid)-bt.radius),
                            abs(got["estimate"]-bt.mid)+bt.radius)
            verr = Interval(max(Q(0), abs(cv0-vt.mid)-vt.radius),
                            abs(cv0-vt.mid)+vt.radius)
            assert berr.hi <= got["exact_box_error_bound"]
            assert verr.hi <= cvbound
            row.update(analytic_area_sr=atrue.dump(), true_surface=bt.dump(),
                       actual_surface_error=berr.dump(),
                       true_variance=vt.dump(), actual_variance_error=verr.dump())
        rows.append(row)
    return {"name": req["name"], "native_area_sr": omega.dump(),
            "drop_area_sr": drop.dump(), "reference_leaf_area_sr": leafarea.dump(),
            "geometry_status": first["status"], "spill_sr": first["spill_sr"],
            "truth_status": "analytic known area with conditional mp.iv primitives"
                if analytic_truth is not None else "conditional; per-leaf truth unavailable",
            "rows": rows}


def sum_intervals(values):
    out = Interval(0, 0)
    for value in values:
        out = out+value
    return out


def main():
    started = time.perf_counter()
    base = Path(__file__).resolve().parents[1]
    inputs = [base/"evidence"/"round2_performance_certified_cells.json"]
    raw = [path.read_bytes() for path in inputs]
    perf = json.loads(raw[0])
    analytic = {"cases": [c for c in perf['cases'] if c['request']['name'] in
                           ('analytic_face_quarter','analytic_complete_face')]}
    cases = []
    for case in analytic["cases"]:
        # Both mature analytic constructions target NESTED face 4 at N=1.
        denom = 12 if "quarter" in case["request"]["name"] else 3
        cases.append(summarize_case(case, [4], enclosed(mp.iv.pi/denom)))
    for case in perf["cases"]:
        req = case["request"]
        if req['name'] in ('analytic_face_quarter','analytic_complete_face'):
            continue
        if "SIP" in req["header"]["CTYPE1"]:
            continue
        areas = case["measurement"]["first"]["areas"]
        leaves = [int(k) for k, v in areas.items() if int(k) != 2**64-1 and v[1] > 0]
        # Most and least positive area probe different conditioning. Tiny entries
        # containing zero are retained; the guaranteed reference supplies N>0.
        leaves.sort(key=lambda k: sum(areas[str(k)]))
        leaves = list(dict.fromkeys(leaves[:1]+leaves[-1:]))
        cases.append(summarize_case(case, leaves))
    result = {"source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "propagation_source_sha256": hashlib.sha256((base/"oracles"/"round2_science_budget.py").read_bytes()).hexdigest(),
              "input_sha256": {path.name: hashlib.sha256(data).hexdigest()
                               for path, data in zip(inputs, raw)},
              "environment": {"python": platform.python_version(), "mpmath": mp.__version__,
                              "interval_decimal_precision": mp.iv.dps},
              "run_command": "python 实验/science-foundation/oracles/round2_cpp_science_bridge.py",
              "random_seed": None,
              "scope": "actual C++ area bounds; independent TAN native/drop solid angles; exact native integrals near surface 1e9 and -2 L/sr; known noise surface scales .01/.02 L/sr, shared rho=.1",
              "machine_bound_caveat": "exact Fraction propagation; TAN/analytic truth depends on mp.iv enclosure primitives, not a formal proof of the machine library",
              "cases": cases, "elapsed_seconds": time.perf_counter()-started}
    out = base/"evidence"/"round2_cpp_science_bridge_final.json"
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"cases": len(cases), "seconds": result["elapsed_seconds"],
                      "evidence": str(out)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
