#!/usr/bin/env python3
"""独立 Oracle：V6 Phase2 分类排异（ALG-P2S-REJ.1..7）。

真值来源 = 独立实现（本文件只依赖 Python 标准库），不调用 astrocs 任何实现。
逐条转写冻结合同：
  sigma_eff^2 = sigma_phase1^2 + J C_theta J^T      (DESIGN-P2-001 §5)
  reason: z<=-4.0 -> rejected_low; z>=+3.0 -> rejected_high; else accepted
          继承阈值 4.0/3.0（ALG-REJ-001 / FZ-REJ-INHERITED-THRESH）
  probability: 高斯混合后验，log 空间
  reason_class: 证据门 + 后验最大类（并列取最小类 id）
  小样本 n<=2 -> underdetermined 全接受 recall=0 显式
用法:
  python3 oracle_rej.py --emit-inc tests/unit/v6_p2_rej/oracle_expected.inc
  python3 oracle_rej.py --json run/v6/IMPL-P2-REJ-001/oracle_expected.json
"""
import argparse
import json
import math
import sys

CLASS_NONE = 0
CLASS_COSMIC = 1
CLASS_TRAIL = 2
CLASS_COLUMN = 3
CLASS_MOVING = 4
CLASS_CLOUD = 5
CLASS_DEFOCUS = 6
CLASS_COUNT = 7

REASON_ACCEPTED = 0
REASON_LOW = 1
REASON_HIGH = 2
REASON_UNDET = 3

# 继承阈值（ALG-REJ-001，冻结）
LOWER_SIGMA = 4.0
UPPER_SIGMA = 3.0
# profile v1 常量（版本化，非冻结排异阈值）
MOTION_MIN = 0.5
PSF_MIN = 0.2
PRIOR = 0.05
KAPPA = 4.0
UNDET_N = 2
# 校准门（PENDING_OWNER_SIGNOFF SO-07，fail-closed 按文档值）
BINMIN = 50
CALIB_ABS = 0.10
CALIB_BSS_MIN = 0.10
BIN_COUNT = 10


def posterior(z, prior=PRIOR, kappa=KAPPA):
    zz = z * z
    log_out = -zz / (2.0 * kappa * kappa) - math.log(kappa)
    log_clean = -zz / 2.0
    lo = math.log(prior) + log_out
    lc = math.log(1.0 - prior) + log_clean
    d = lc - lo
    if d > 700.0:
        return 0.0
    if d < -700.0:
        return 1.0
    return 1.0 / (1.0 + math.exp(d))


def classify(case):
    r = case["residual"]
    s1 = case["sigma_phase1"]
    uv = case["upm_variance"]
    flags = case["noise_flags"]
    growth = case.get("large_scale_growth")
    compact = case.get("compact_single_frame")
    column = case.get("column_consistent")
    motion = case.get("cross_frame_motion")
    lowfreq = case.get("low_frequency")
    psf = case.get("psf_shape_anomaly")
    keep_moving = case.get("keep_moving_source", 1)
    n = case["n"]

    sigma_eff = []
    z = []
    for k in range(n):
        se = math.sqrt(s1[k] * s1[k] + uv[k])
        sigma_eff.append(se)
        z.append(r[k] / se)

    def at(arr, k):
        return 0.0 if arr is None else arr[k]

    if n <= UNDET_N:
        return {
            "n": n,
            "status": "UNDERDETERMINED",
            "sigma_eff": sigma_eff,
            "z": z,
            "reason": [REASON_UNDET] * n,
            "reason_class": [CLASS_NONE] * n,
            "deleted": [0] * n,
            "preserved": [0] * n,
            "probability": [0.0] * n,
            "class_probability": [[0.0] * CLASS_COUNT for _ in range(n)],
            "accepted_count": n,
            "rejected_low": 0,
            "rejected_high": 0,
            "recall": 0.0,
        }

    reason = []
    cls = []
    deleted = []
    preserved = []
    prob = []
    clsprob = []
    low = high = 0
    for k in range(n):
        zk = z[k]
        if zk <= -LOWER_SIGMA:
            rk = REASON_LOW
        elif zk >= UPPER_SIGMA:
            rk = REASON_HIGH
        else:
            rk = REASON_ACCEPTED
        p = posterior(zk)
        moving_ev = at(motion, k) >= MOTION_MIN
        is_out = rk != REASON_ACCEPTED
        w = [0.0] * CLASS_COUNT
        if is_out or moving_ev:
            g = at(growth, k) != 0
            c = at(compact, k) != 0
            col = at(column, k) != 0
            lf = at(lowfreq, k) != 0
            pa = at(psf, k)
            w[CLASS_COSMIC] = 1.0 if (c and not g) else 0.0
            w[CLASS_TRAIL] = 1.0 if g else 0.0
            w[CLASS_COLUMN] = 1.0 if col else 0.0
            w[CLASS_MOVING] = 1.0 if moving_ev else 0.0
            w[CLASS_CLOUD] = 1.0 if lf else 0.0
            w[CLASS_DEFOCUS] = 1.0 if pa >= PSF_MIN else 0.0
        wsum = sum(w[1:])
        kcls = CLASS_NONE
        kcp = [0.0] * CLASS_COUNT
        if wsum > 0.0:
            best = -1.0
            for c in range(1, CLASS_COUNT):
                pc = p * (w[c] / wsum)
                kcp[c] = pc
                if pc > best:  # 严格 >: 并列取最小类 id
                    best = pc
                    kcls = c
        pres = (kcls == CLASS_MOVING)
        dele = rk in (REASON_LOW, REASON_HIGH)
        if pres and keep_moving:
            dele = False
            rk = REASON_ACCEPTED
        if rk == REASON_LOW:
            low += 1
        elif rk == REASON_HIGH:
            high += 1
        reason.append(rk)
        cls.append(kcls)
        deleted.append(1 if dele else 0)
        preserved.append(1 if pres else 0)
        prob.append(p)
        clsprob.append(kcp)
    return {
        "n": n,
        "status": "OK" if (n - low - high) > 0 else "ALL_REJECTED",
        "sigma_eff": sigma_eff,
        "z": z,
        "reason": reason,
        "reason_class": cls,
        "deleted": deleted,
        "preserved": preserved,
        "probability": prob,
        "class_probability": clsprob,
        "accepted_count": n - low - high,
        "rejected_low": low,
        "rejected_high": high,
        "recall": (low + high) / float(n),
    }


def calibration(prob, truth, bin_count=BIN_COUNT):
    n = len(prob)
    bs = sum((prob[k] - truth[k]) ** 2 for k in range(n)) / n
    base = sum(truth) / n
    bsref = sum((base - truth[k]) ** 2 for k in range(n)) / n
    bss = 1.0 - bs / bsref if bsref > 0 else 0.0
    order = sorted(range(n), key=lambda k: (prob[k], k))
    maxdev = 0.0
    used = skipped = used_samples = 0
    for b in range(bin_count):
        lo = b * n // bin_count
        hi = (b + 1) * n // bin_count
        if hi <= lo:
            continue
        nb = hi - lo
        mp = sum(prob[order[i]] for i in range(lo, hi)) / nb
        obs = sum(truth[order[i]] for i in range(lo, hi)) / nb
        dev = abs(obs - mp)
        if nb >= BINMIN:
            used += 1
            used_samples += nb
            maxdev = max(maxdev, dev)
        else:
            skipped += 1
    if used == 0:
        st = "INSUFFICIENT_SAMPLES"
    elif maxdev > CALIB_ABS:
        st = "RELIABILITY_FAIL"
    elif not (bsref > 0) or not (bss > CALIB_BSS_MIN):
        st = "BSS_FAIL"
    else:
        st = "OK"
    return {
        "status": st,
        "brier": bs,
        "brier_ref": bsref,
        "bss": bss,
        "max_abs_reliability_dev": maxdev,
        "bins_total": bin_count,
        "bins_used": used,
        "bins_skipped_small": skipped,
        "used_samples": used_samples,
        "gate": 1 if st == "OK" else 0,
    }


def fixtures():
    f = []
    f.append({
        "name": "mixed_12",
        "n": 12,
        "residual": [8.0, -6.0, 5.0, 1.5, 8.0, -5.0, 0.5, -1.0, 6.0, 7.0, 10.0, 0.2],
        "sigma_phase1": [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 0.05],
        "upm_variance": [0, 1, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0],
        "noise_flags": [3] * 12,
        "large_scale_growth": [0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
        "compact_single_frame": [1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1],
        "column_consistent": [0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0],
        "cross_frame_motion": [0, 0, 0, 0.8, 0, 0, 0, 0, 0, 0.6, 0, 0],
        "low_frequency": [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0],
        "psf_shape_anomaly": [0, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, 0],
        "keep_moving_source": 1,
    })
    f.append({
        "name": "normal_3",
        "n": 3,
        "residual": [0.1, 4.5, -2.0],
        "sigma_phase1": [1.0, 1.0, 1.0],
        "upm_variance": [0.0, 0.25, 3.0],
        "noise_flags": [3, 3, 3],
        "large_scale_growth": [0, 0, 0],
        "compact_single_frame": [0, 1, 0],
        "column_consistent": [0, 0, 0],
        "cross_frame_motion": [0.0, 0.0, 0.0],
        "low_frequency": [0, 0, 1],
        "psf_shape_anomaly": [0.0, 0.0, 0.0],
        "keep_moving_source": 1,
    })
    f.append({
        "name": "small_2",
        "n": 2,
        "residual": [50.0, -30.0],
        "sigma_phase1": [1.0, 1.0],
        "upm_variance": [0.0, 0.0],
        "noise_flags": [3, 3],
        "large_scale_growth": [1, 0],
        "compact_single_frame": [0, 1],
        "column_consistent": [0, 0],
        "cross_frame_motion": [0.0, 0.0],
        "low_frequency": [0, 0],
        "psf_shape_anomaly": [0.0, 0.0],
        "keep_moving_source": 1,
    })
    return f


def lcg_labels(p_list, seed=20260915):
    """确定性 Bernoulli 标签（LCG），使训练/验收样本可复跑。"""
    state = seed
    y = []
    for p in p_list:
        state = (1103515245 * state + 12345) % (2 ** 31)
        u = state / float(2 ** 31)
        y.append(1 if u < p else 0)
    return y


def calib_fixtures():
    out = []
    n = 2000
    p1 = [0.02 + 0.96 * ((i + 0.5) / n) for i in range(n)]
    out.append({"name": "calib_good", "n": n, "p": p1, "y": lcg_labels(p1)})
    n2 = 600
    p2 = [0.3] * n2
    # p 常数、各箱 obs=p 精确 → 可靠性过；BSS==0 → BSS_FAIL（隔离 BSS 门）
    y2 = [(1 if (i % 60) < 18 else 0) for i in range(n2)]
    out.append({"name": "calib_bss_fail", "n": n2, "p": p2, "y": y2})
    n3 = 600
    p3 = [0.05 + 0.9 * ((i + 0.5) / n3) for i in range(n3)]
    out.append({"name": "calib_rel_fail", "n": n3, "p": p3,
                "y": lcg_labels([0.2] * n3, seed=99)})
    n4 = 60
    p4 = [0.05 + 0.9 * ((i + 0.5) / n4) for i in range(n4)]
    out.append({"name": "calib_insufficient", "n": n4, "p": p4,
                "y": lcg_labels(p4, seed=1234)})
    # 可靠性中度偏差（dev~0.2）：用于检出 CALIB-ABS 被放宽到 0.50
    n5 = 600
    p5 = [0.5] * n5
    y5 = [(1 if (i % 60) < 42 else 0) for i in range(n5)]  # obs=0.7, dev=0.2
    out.append({"name": "calib_rel_mid", "n": n5, "p": p5, "y": y5})
    # BSS 区间 (0,0.10)：用于检出 BSS_MIN 被放宽（如 0）
    n6 = 2000
    p6 = [0.4 + 0.2 * ((i + 0.5) / n6) for i in range(n6)]
    out.append({"name": "calib_bss_mid", "n": n6, "p": p6,
                "y": lcg_labels(p6, seed=555)})
    return out


def cxx_arr(ctype, name, vals, fmt):
    body = ", ".join(fmt(v) for v in vals)
    return "static const %s %s[%d] = {%s};\n" % (ctype, name, len(vals), body)


def emit_inc(path):
    cases = fixtures()
    outs = [classify(c) for c in cases]
    lines = []
    lines.append("// 由 tests/unit/v6_p2_rej/oracle_rej.py 机械生成；请勿手改。\n")
    lines.append("// 独立 Oracle（纯标准库）期望值，不调用被测实现。\n")
    lines.append("#define ORACLE_CASE_COUNT %d\n" % len(cases))
    nlist = [c["n"] for c in cases]
    lines.append(cxx_arr("int", "oracle_n", nlist, lambda v: str(v)))
    for ci, c in enumerate(cases):
        n = c["n"]
        lines.append(cxx_arr("double", "oracle_%d_residual" % ci, c["residual"], lambda v: repr(float(v))))
        lines.append(cxx_arr("double", "oracle_%d_sigma_phase1" % ci, c["sigma_phase1"], lambda v: repr(float(v))))
        lines.append(cxx_arr("double", "oracle_%d_upm" % ci, c["upm_variance"], lambda v: repr(float(v))))
        lines.append(cxx_arr("unsigned char", "oracle_%d_flags" % ci, c["noise_flags"], lambda v: str(int(v))))
        def opt(name, key):
            vals = c.get(key)
            if vals is None:
                return "static const unsigned char* oracle_%d_%s = nullptr;\n" % (ci, name)
            if any(isinstance(v, float) for v in vals):
                return cxx_arr("double", "oracle_%d_%s_data" % (ci, name), vals, lambda v: repr(float(v))) + \
                    "static const double* oracle_%d_%s = oracle_%d_%s_data;\n" % (ci, name, ci, name)
            return cxx_arr("unsigned char", "oracle_%d_%s_data" % (ci, name), vals, lambda v: str(int(v))) + \
                "static const unsigned char* oracle_%d_%s = oracle_%d_%s_data;\n" % (ci, name, ci, name)
        for key, nm in [("large_scale_growth", "growth"), ("compact_single_frame", "compact"),
                        ("column_consistent", "column"), ("cross_frame_motion", "motion"),
                        ("low_frequency", "lowfreq"), ("psf_shape_anomaly", "psf")]:
            lines.append(opt(nm, key))
        o = outs[ci]
        lines.append(cxx_arr("double", "oracle_%d_sigma_eff" % ci, o["sigma_eff"], lambda v: repr(float(v))))
        lines.append(cxx_arr("double", "oracle_%d_z" % ci, o["z"], lambda v: repr(float(v))))
        lines.append(cxx_arr("double", "oracle_%d_p" % ci, o["probability"], lambda v: repr(float(v))))
        lines.append(cxx_arr("int", "oracle_%d_reason" % ci, o["reason"], lambda v: str(int(v))))
        lines.append(cxx_arr("int", "oracle_%d_class" % ci, o["reason_class"], lambda v: str(int(v))))
        lines.append(cxx_arr("int", "oracle_%d_deleted" % ci, o["deleted"], lambda v: str(int(v))))
        lines.append(cxx_arr("int", "oracle_%d_preserved" % ci, o["preserved"], lambda v: str(int(v))))
        flat = [x for row in o["class_probability"] for x in row]
        lines.append(cxx_arr("double", "oracle_%d_clsprob" % ci, flat, lambda v: repr(float(v))))
        lines.append("static const int oracle_%d_accepted = %d;\n" % (ci, o["accepted_count"]))
        lines.append("static const int oracle_%d_low = %d;\n" % (ci, o["rejected_low"]))
        lines.append("static const int oracle_%d_high = %d;\n" % (ci, o["rejected_high"]))
        lines.append("static const double oracle_%d_recall = %s;\n" % (ci, repr(float(o["recall"]))))
    lines.append("static const char* oracle_status[ORACLE_CASE_COUNT] = {%s};\n" %
                 ", ".join('"%s"' % o["status"] for o in outs))
    # ---- 校准门 fixture（独立 Oracle 期望） ----
    cf = calib_fixtures()
    lines.append("#define ORACLE_CALIB_COUNT %d\n" % len(cf))
    lines.append(cxx_arr("int", "oracle_calib_n", [c["n"] for c in cf], lambda v: str(v)))
    lines.append("static const char* const oracle_calib_name[%d] = {%s};\n" %
                 (len(cf), ", ".join('"%s"' % c["name"] for c in cf)))
    for ci, c in enumerate(cf):
        lines.append(cxx_arr("double", "oracle_calib_%d_p" % ci, c["p"], lambda v: repr(float(v))))
        lines.append(cxx_arr("unsigned char", "oracle_calib_%d_y" % ci, c["y"], lambda v: str(int(v))))
        o = calibration(c["p"], c["y"])
        lines.append("static const char* oracle_calib_%d_status = \"%s\";\n" % (ci, o["status"]))
        for key in ("brier", "brier_ref", "bss", "max_abs_reliability_dev"):
            lines.append("static const double oracle_calib_%d_%s = %s;\n" % (ci, key, repr(float(o[key]))))
        for key in ("bins_total", "bins_used", "bins_skipped_small", "used_samples", "gate"):
            lines.append("static const int oracle_calib_%d_%s = %d;\n" % (ci, key, int(o[key])))
    with open(path, "w") as fh:
        fh.write("".join(lines))
    return {"cases": [c["name"] for c in cases],
            "outputs": outs}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emit-inc")
    ap.add_argument("--json")
    args = ap.parse_args()
    res = {"fixtures": fixtures(), "outputs": [classify(c) for c in fixtures()]}
    res["calibration"] = {c["name"]: calibration(c["p"], c["y"])
                          for c in calib_fixtures()}
    if args.emit_inc:
        emit_inc(args.emit_inc)
        print("wrote", args.emit_inc)
    if args.json:
        with open(args.json, "w") as fh:
            json.dump(res, fh, indent=1)
        print("wrote", args.json)
    if not args.emit_inc and not args.json:
        print(json.dumps(res, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
