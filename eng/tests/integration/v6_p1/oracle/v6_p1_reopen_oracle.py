#!/usr/bin/env python3
"""v6_p1_reopen_oracle.py — P1-INTEGRATE-001 独立重开 Oracle

不链接被测 C++ 实现：只读磁盘产物，用生产 schema（eng/contracts/schemas/v6/）与
W6 最小校验器（eng/tests/contracts/v6/jsonschema_min.py，只读导入）逐条验证：
  1. 每个产品目录的 phase1_product.json 内嵌记录逐条通过生产 schema；
  2. 冻结单位串 / BUNIT 量纲可判 / 二次律 / 权重词表 canonical / 禁第三套词表；
  3. science.fits 为真实 FITS：CHECKSUM/DATASUM 由 C++ 侧独立验证，这里独立
     重算 SHA-256 + 逐 HDU 读取 BUNIT 与层序（不信任 manifest）；
  4. Phase2 消费面：从磁盘重开的两帧四分量独立复算组内归一权重，与 C++ 输出
     逐位比对（独立 Oracle，非同实现自证）；
  5. Oracle 自检：对记录副本注入违反冻结的变异，断言 schema 校验必红（防止
     "零用例/skip-only"式空洞通过）。

用法: python3 v6_p1_reopen_oracle.py --workdir <dir> --repo <repo_root>
"""
import argparse
import copy
import hashlib
import json
import os
import sys


def fail(msg):
    print("ORACLE_FAIL: " + msg)
    sys.exit(1)


def load_json(p):
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)


def fits_hdus(data):
    """最小 FITS HDU 走查：返回每 HDU 的 (bitpix, naxis, bunit)。"""
    out = []
    pos = 0
    n = len(data)
    while pos + 2880 <= n:
        header = data[pos:pos + 2880]
        bitpix = None
        naxis = 0
        axes = []
        bunit = None
        for i in range(0, 2880, 80):
            card = header[i:i + 80].decode("ascii", "replace")
            key = card[:8].strip()
            if key == "END":
                break
            if key in ("SIMPLE", "XTENSION"):
                continue
            raw = card[10:].strip()
            if raw.startswith("'"):
                end = raw.find("'", 1)
                val = raw[1:end] if end > 0 else raw[1:]
            else:
                val = raw.split("/")[0].strip()
            if key == "NAXIS":
                try:
                    naxis = int(val)
                except ValueError:
                    naxis = 0
            elif key.startswith("NAXIS") and key != "NAXIS":
                try:
                    axes.append(int(val))
                except ValueError:
                    pass
            elif key == "BITPIX":
                try:
                    bitpix = int(val)
                except ValueError:
                    pass
            elif key == "BUNIT":
                bunit = val.strip()
        raw = 1
        for d in axes:
            raw *= d
        bpp = {8: 1, 16: 2, 32: 4, -32: 4, -64: 8}.get(bitpix, 1)
        data_bytes = ((raw * bpp + 2879) // 2880) * 2880
        out.append({"bitpix": bitpix, "naxis": axes, "bunit": bunit})
        if naxis == 0:
            break
        pos += 2880 + data_bytes
    return out


def group_weights_from_components(components):
    """独立按冻结复合式复算组内归一权重（不调用被测实现）。"""
    floor = 1e-12
    alpha, beta, gamma, delta = 2.0, 1.0, 2.0, 1.0
    wt = []
    for c in components:
        s = max(c["signal"], floor)
        conc = max(c["concentration"], floor)
        nn = max(c["noise"], floor)
        bb = max(c["background"], floor)
        wt.append((s ** alpha) * (conc ** beta) / ((nn ** gamma) * (bb ** delta)))
    srt = sorted(wt)
    m = len(srt)
    med = srt[m // 2] if m % 2 == 1 else 0.5 * (srt[m // 2 - 1] + srt[m // 2])
    if not (med > 0.0):
        fail("independent group median <= 0")
    return wt, [w / med for w in wt], med


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", required=True)
    ap.add_argument("--repo", required=True)
    args = ap.parse_args()

    # BLD-401: 路径随 2026-09-21 根目录整合订正（tests/ → eng/tests/）
    sys.path.insert(0, os.path.join(args.repo, "eng", "tests", "contracts", "v6"))
    try:
        import jsonschema_min  # noqa: E402
    except Exception as exc:  # pragma: no cover
        fail("cannot import jsonschema_min from eng/tests/contracts/v6: %s" % exc)

    schemas = {}
    for name, key in [
        ("point-information", "point_information"),
        ("psfsw", "psfsw"),
        ("covariance", "calibration_covariance"),
        ("covariance", "drizzle_covariance"),
        ("effective-psf", "effective_psf"),
        ("provenance", "provenance"),
    ]:
        # BLD-401: 路径随 2026-09-21 根目录整合订正（contracts/ → eng/contracts/）
        path = os.path.join(args.repo, "eng", "contracts", "schemas", "v6",
                            "astrocs.v6.%s.v1.schema.json" % name)
        schemas[key] = load_json(path)

    checks = 0
    products = [os.path.join(args.workdir, "positive", "frame_a.p1"),
                os.path.join(args.workdir, "positive", "frame_b.p1")]
    for pdir in products:
        if not os.path.isdir(pdir):
            fail("missing product dir: " + pdir)
        rec = load_json(os.path.join(pdir, "phase1_product.json"))
        sci = os.path.join(pdir, "science.fits")

        # (1) 内嵌记录逐条过生产 schema。
        for key, sch in schemas.items():
            checks += 1
            if not jsonschema_min.is_valid(rec[key], sch):
                errs = jsonschema_min.validate(rec[key], sch)
                fail("schema %s failed for %s: %s" % (key, pdir, errs[:3]))

        # (2) 冻结单位串 / 权重词表 canonical。
        u = rec["units"]
        checks += 1
        if (u["signal_sb"], u["sb_variance_out"], u["sb_ivar_out"]) != (
                "ADU/px^2", "ADU^2/px^4", "px^4/ADU^2"):
            fail("frozen unit strings violated in " + pdir)
        w = rec["psfsw"]["weight"]
        checks += 1
        if not (w["kind"] == "psfsw_robust_weight" and w["units"] == "1"
                and w["group_normalized"] is True
                and w["normalization"]["scope"] == "group"
                and abs(w["normalization"]["median_target"] - 1.0) <= 1e-12
                and w["normalization"]["constants_version"]):
            fail("canonical psfsw vocabulary violated in " + pdir)
        checks += 1
        if w["weight_value"] is not None:
            fail("phase1 single frame must not emit group-normalized weight_value")
        for bad in ("weight_normalized", "normalization_scope", "weight_type",
                    "norm_scope", "is_group_normalized", "weight_dimensionless"):
            checks += 1
            if bad in w or bad in rec["psfsw"]:
                fail("third vocabulary token present: " + bad)
        for tok in ("ivar", "fisher", "inverse_variance", "w_info", "w_psf"):
            checks += 1
            if tok in json.dumps(rec["psfsw"]["weight"]):
                fail("psfsw weight carries forbidden token: " + tok)
        checks += 1
        if rec["psfsw"]["components"]["concentration"]["units"] != "ADU/px^2":
            fail("concentration unit authority (W6) violated")
        checks += 1
        if rec["phase1_extensions"]["group_normalization_deferred_to"] != "phase2":
            fail("OI-01 group normalization must be deferred to phase2")

        # (3) FITS：独立 SHA-256 + BUNIT/层序。
        with open(sci, "rb") as f:
            data = f.read()
        sha = hashlib.sha256(data).hexdigest()
        checks += 1
        if sha != rec["manifest"]["output_hash"]:
            fail("manifest.output_hash != recomputed sha256 in " + pdir)
        checks += 1
        if sha != rec["provenance"]["output_hash"].replace("sha256:", ""):
            fail("provenance.output_hash != recomputed sha256 in " + pdir)
        hdus = fits_hdus(data)
        checks += 1
        if [h["bunit"] for h in hdus] != ["ADU/px^2", "px^2", "ADU^2/px^4", "px^4/ADU^2"]:
            fail("FITS BUNIT layer order violated: %s" % [h["bunit"] for h in hdus])
        checks += 1
        if [h["bitpix"] for h in hdus] != [-64, -64, -64, -64]:
            fail("FITS BITPIX must be -64 on every layer")
        # 二次律：variance BUNIT == signal BUNIT 的平方（显式 px 幂次语义）。
        sig, var, ivar = hdus[0]["bunit"], hdus[2]["bunit"], hdus[3]["bunit"]
        checks += 1
        if not (sig == "ADU/px^2" and var == "ADU^2/px^4" and ivar == "px^4/ADU^2"):
            fail("BUNIT quadratic law strings violated")

        # (4) 禁诊断量进方差/权重来源。
        fv = rec["drizzle_covariance"]["variance_from"]
        checks += 1
        if fv in rec["drizzle_covariance"]["forbidden_variance_sources"]:
            fail("variance_from is a forbidden diagnostic/weight source")

    # (5) Phase2 消费面：独立复算组内归一并与 C++ 输出比对。
    grp = load_json(os.path.join(args.workdir, "group", "group_result.json"))
    comps = []
    for pdir in products:
        rec = load_json(os.path.join(pdir, "phase1_product.json"))
        c = rec["psfsw"]["components"]
        comps.append({
            "signal": c["signal"]["value"],
            "concentration": c["concentration"]["value"],
            "noise": c["noise"]["value"],
            "background": c["background"]["value"],
        })
    wt, wnorm, med = group_weights_from_components(comps)
    checks += 1
    if grp["n_frames"] != len(products):
        fail("group_result n_frames mismatch")
    for i, (a, b) in enumerate(zip(wt, grp["wt_unnormalized"])):
        checks += 1
        if abs(a - b) > 1e-12 * max(1.0, abs(a)):
            fail("independent wt[%d] %.17g != disk %.17g" % (i, a, b))
    for i, (a, b) in enumerate(zip(wnorm, grp["w_psfsw"])):
        checks += 1
        if abs(a - b) > 1e-12 * max(1.0, abs(a)):
            fail("independent w_psfsw[%d] %.17g != disk %.17g" % (i, a, b))
    checks += 1
    srt = sorted(grp["w_psfsw"])
    mmed = srt[0] if len(srt) == 1 else 0.5 * (srt[0] + srt[-1])
    if abs(mmed - 1.0) > 1e-9:
        fail("group median(w_psfsw) != 1")

    # (6) Oracle 自检：注入变异必须被 schema 判红（防空洞通过）。
    base = load_json(os.path.join(products[0], "phase1_product.json"))
    muts = []
    m = copy.deepcopy(base)
    m["psfsw"]["weight"]["kind"] = "ivar"
    muts.append(("psfsw", m["psfsw"], "weight.kind=ivar"))
    m = copy.deepcopy(base)
    m["psfsw"]["weight"]["group_normalized"] = False
    muts.append(("psfsw", m["psfsw"], "group_normalized=false"))
    m = copy.deepcopy(base)
    m["psfsw"]["components"]["concentration"]["units"] = "ADU/px"
    muts.append(("psfsw", m["psfsw"], "concentration ADU/px"))
    m = copy.deepcopy(base)
    del m["provenance"]["output_hash"]
    muts.append(("provenance", m["provenance"], "provenance.output_hash missing"))
    m = copy.deepcopy(base)
    m["point_information"]["authoritative_formula"] = "W = median(SNR)"
    muts.append(("point_information", m["point_information"],
                 "point_information authoritative_formula altered"))
    m = copy.deepcopy(base)
    m["drizzle_covariance"]["variance_from"] = "weight"
    muts.append(("drizzle_covariance", m["drizzle_covariance"],
                 "covariance.variance_from=weight"))
    caught = 0
    for key, inst, what in muts:
        checks += 1
        if not jsonschema_min.is_valid(inst, schemas[key]):
            caught += 1
        else:
            fail("oracle self-check: mutation NOT caught: " + what)
    print("ORACLE_PASS checks=%d mutations_caught=%d/%d" % (checks, caught, len(muts)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
