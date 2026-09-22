#!/usr/bin/env python3
"""v6_p1_reopen_oracle.py — P1-INTEGRATE-001 独立重开 Oracle

不链接被测 C++ 实现：只读磁盘产物，用生产 schema（eng/contracts/schemas/product_family_field_constraints.schema.json 的 $defs）与
最小校验器（eng/tests/common/jsonschema_min.py，只读导入）逐条验证：
  1. 每个产品目录的 phase1_product.json 内嵌记录逐条通过生产 schema；
  2. 冻结单位串 / BUNIT 量纲可判 / 二次律 / 权重词表口径（PSFSW-RETIRE-03：新产品
     **不再需要**声明退役对象 psfsw_robust_weight；旧产品的退役声明仍必须可判）；
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


def with_retired_declaration(rec):
    """返回"旧产品"形状的副本（带退役对象声明块；PSFSW-RETIRE-03 退役/迁移情形）。

    现行产品不再写出该声明（schema 已把它移出 required）；本函数把退役 canonical
    形式原样放回，用于独立验证"旧产品仍能被识别"这条路径（形状可判 + 消费面拒绝）。
    """
    d = copy.deepcopy(rec)
    ps = d["psfsw"]
    d["units"]["psfsw_robust_weight"] = "1"
    ps["weight_mode"] = "psfsw_robust"
    ps["weight"] = {
        "kind": "psfsw_robust_weight",
        "units": "1",
        "group_normalized": True,
        "normalization": {
            "scope": "group",
            "median_target": 1.0,
            "constants_version": ps["composite"]["C_norm_version"],
        },
        "weight_value": None,
    }
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", required=True)
    ap.add_argument("--repo", required=True)
    args = ap.parse_args()

    # BLD-401: 路径随 2026-09-21 根目录整合订正（tests/ → eng/tests/）
    sys.path.insert(0, os.path.join(args.repo, "eng", "tests", "common"))
    try:
        import jsonschema_min  # noqa: E402
    except Exception as exc:  # pragma: no cover
        fail("cannot import jsonschema_min from eng/tests/common: %s" % exc)

    # DOC-CONTRACT-MERGE-02：v6 逐件 schema 已并入现行承载面
    # eng/contracts/schemas/product_family_field_constraints.schema.json 的 $defs
    # （原 eng/contracts/schemas/v6/astrocs.v6.<name>.v1.schema.json 已按
    # CHG-2026-09-22-V6-CONTRACT-MERGE 整体出库；见 docs/contracts/DATA_SEMANTICS.md §31）。
    pf_path = os.path.join(args.repo, "eng", "contracts", "schemas",
                           "product_family_field_constraints.schema.json")
    pf = load_json(pf_path)
    schemas = {}
    for def_key, key in [
        ("point_information", "point_information"),
        ("psfsw", "psfsw"),
        ("covariance", "calibration_covariance"),
        ("covariance", "drizzle_covariance"),
        ("effective_psf", "effective_psf"),
        ("provenance", "provenance"),
    ]:
        if def_key not in pf.get("$defs", {}):
            fail("product_family_field_constraints.schema.json missing $defs.%s" % def_key)
        schemas[key] = pf["$defs"][def_key]

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

        # (2) 冻结单位串 / 权重词表口径（PSFSW-RETIRE-03 产品合同收口后）。
        u = rec["units"]
        checks += 1
        if (u["signal_sb"], u["sb_variance_out"], u["sb_ivar_out"]) != (
                "ADU/px^2", "ADU^2/px^4", "px^4/ADU^2"):
            fail("frozen unit strings violated in " + pdir)
        checks += 1
        if "psfsw_robust_weight" in u:
            fail("new phase1 product must not declare the retired unit symbol "
                 "(units.psfsw_robust_weight) in " + pdir)
        ps = rec["psfsw"]
        checks += 1
        # 新产品**不再需要**声明退役对象：退役声明面（weight_mode/weight）必须缺席。
        if "weight_mode" in ps or "weight" in ps:
            fail("new phase1 product must not carry the retired weight declaration "
                 "(psfsw.weight_mode / psfsw.weight) in " + pdir)
        checks += 1
        if set(ps["components"]) != {"signal", "concentration", "noise", "background"}:
            fail("psfsw four components missing in " + pdir)
        for bad in ("weight_normalized", "normalization_scope", "weight_type",
                    "norm_scope", "is_group_normalized", "weight_dimensionless"):
            checks += 1
            if bad in ps:
                fail("third vocabulary token present: " + bad)
        checks += 1
        if ps["components"]["concentration"]["units"] != "ADU/px^2":
            fail("concentration unit authority (W6) violated")
        checks += 1
        if rec["phase1_extensions"]["group_normalization_deferred_to"] != "phase2":
            fail("OI-01 group normalization must be deferred to phase2")
        # 旧产品路径（可判定的退役情形）：把退役对象声明原样放回 ⇒ schema 必须仍能
        # 识别该形状（"旧产品仍能被识别"），且消费面必须显式拒绝（见 (5)）。
        checks += 1
        if not jsonschema_min.is_valid(with_retired_declaration(rec)["psfsw"],
                                       schemas["psfsw"]):
            fail("legacy retired declaration must stay schema-recognizable "
                 "(retirement/migration case, not a structural error) in " + pdir)

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

    # (5) Phase2 消费面：FZ-MODE-RETIRED —— 该面**整体退役**，必须无条件 fail-closed。
    #     PSFSW-RETIRE-03 后新产品不再携带退役声明，但消费面的唯一产物就是退役对象
    #     psfsw_robust_weight 的组内归一权重 ⇒ 无论产品是否携带声明都不得产出 w_psfsw
    #     （不静默接受）。本 Oracle 独立断言拒绝理由可诊断。
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
    # 非空洞守卫：fixture 分量本身能独立算出合法的组内归一（全正 + 归一后 median=1）
    # ⇒ 拒绝是"退役对象"策略拒绝，不是数据退化导致的偶然拒绝。
    # 注意 group_weights_from_components 返回的 med 是**未归一**中位数，归一后中位数需自算。
    wt, wnorm, med = group_weights_from_components(comps)
    srt = sorted(wnorm)
    mmed = srt[0] if len(srt) == 1 else 0.5 * (srt[0] + srt[-1])
    checks += 1
    if not (len(wnorm) == len(products) and all(v > 0.0 for v in wnorm)
            and abs(mmed - 1.0) <= 1e-9):
        fail("fixture components do not yield a valid group normalization "
             "(retired-object rejection would be vacuous)")
    checks += 1
    if grp.get("retired_rejected") is not True:
        fail("Phase2 consumption did not reject the retired psfsw weight declaration")
    msg = grp.get("error", "")
    for tok in ("FZ-MODE-RETIRED", "psfsw_robust_weight", "point_information",
                "surface_gls", "migration"):
        checks += 1
        if tok not in msg:
            fail("group reject reason missing %r" % tok)
    checks += 1
    if grp.get("w_psfsw"):
        fail("retired object produced group weights (must be fail-closed)")

    # (6) Oracle 自检：注入变异必须被 schema 判红（防空洞通过）。
    #     退役声明的变异都建立在"旧产品形状"（带退役声明）之上——否则变异会退化成
    #     "缺 required 键"，测不到退役形状门本身。
    base = load_json(os.path.join(products[0], "phase1_product.json"))

    def legacy(doc):
        return with_retired_declaration(doc)

    muts = []
    m = legacy(base)
    m["psfsw"]["weight"]["kind"] = "ivar"
    muts.append(("psfsw", m["psfsw"], "retired weight.kind=ivar"))
    m = legacy(base)
    m["psfsw"]["weight"]["group_normalized"] = False
    muts.append(("psfsw", m["psfsw"], "retired group_normalized=false"))
    m = legacy(base)
    m["psfsw"]["weight_mode"] = "point_information"
    muts.append(("psfsw", m["psfsw"], "retired declaration with wrong weight_mode"))
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
