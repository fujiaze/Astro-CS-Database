#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CONTRACT-FREEZE-001 独立一致性 Oracle：机器检查 冻结表 ↔ W3 规格 ↔ SCI-ADJ 冻结 三方一致。
缺项/单位不一致/枚举越界/数值含糊/禁止项命中/pending 冒充 frozen 即红（rc=1）。
独立性：直接消费 SCI-ADJ 机器裁决、W3 tracked 规格文本与 catalog；不调用 gen_freeze.py 的任何函数。"""
import json, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
DEFAULT_MACHINE = os.path.join(ROOT, "docs", "contracts", "v6", "frozen", "astrocs.v6.contract-freeze.v1.json")

def jload(p):
    with open(p if os.path.isabs(p) else os.path.join(ROOT, p), encoding="utf-8") as f:
        return json.load(f)

def readtext(p):
    with open(os.path.join(ROOT, p), encoding="utf-8") as f:
        return f.read()

FREEZE_LIST = "docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md"
PSFSW_FREEZE_KEYS = {"ivar", "variance", "var", "sigma", "sigma2", "inverse_variance",
                     "fisher", "information", "w_info", "w_psf"}
VOCAB = {"FROZEN", "PENDING_OWNER_SIGNOFF", "OPEN"}
FUZZY = ["tbd", "tba", "待定", "待决", "???", "xx", "n/a", "待填", "约等于"]

def run_oracle(machine_path=DEFAULT_MACHINE):
    fails, checks = [], []
    def C(cid, cond, msg):
        checks.append(cid)
        if not cond:
            fails.append(cid + ": " + msg)
    try:
        M = jload(machine_path)
    except Exception as e:
        return ["C1: machine table unreadable: %s" % e], ["C1"]
    ADJ = jload("reports/v6/science-adjudication/adjudications.json")
    CAT = jload("contracts/proposals/v6/data/astrocs.v6.data-design-catalog.v1.json")
    P1 = jload("docs/algorithms/v6/phase1/alg_p1_001_spec.json")
    FL = readtext(FREEZE_LIST)

    C("C1", M.get("freeze_schema") == "astrocs.v6.contract-freeze/v1", "freeze_schema mismatch")
    C("C1b", bool(re.fullmatch(r"[0-9a-f]{40}", str(M.get("baseline_head", "")))), "baseline_head not a 40-hex sha")

    # C2 units three-way
    C("C2", M.get("units_table") == ADJ.get("units_table"), "units_table != adjudications.units_table")
    C("C2b", M.get("units_table") == CAT.get("frozen_units_table"), "units_table != catalog.frozen_units_table")
    wt = M.get("weight_modes", {})
    C("C2c", wt.get("production") == ADJ.get("production_weight_modes"), "production modes != adjudications")
    C("C2d", wt.get("documented_baseline") == ADJ.get("documented_baseline_modes"), "baseline modes != adjudications")
    C("C2e", wt.get("deferred") == ADJ.get("deferred_modes"), "deferred modes != adjudications")
    C("C2f", wt.get("legacy_integer_allowed") is False, "legacy_integer_allowed must be false")
    C("C2g", wt.get("production") == CAT["weight_mode"]["production"], "production != catalog")
    C("C2h", wt.get("documented_baseline") == CAT["weight_mode"]["documented_baseline"], "baseline != catalog")

    # C3 enum boundary
    bad = [m for m in wt.get("production", []) if m in ("psf_snr_power", "auto", "support_x_snr2", 0)]
    C("C3", not bad, "production modes contain forbidden values: %r" % bad)
    C("C3b", wt.get("psf_snr_power" if False else "deferred") == ["psf_snr_power"], "psf_snr_power must remain deferred")
    C("C3c", all(m in ("point_information", "surface_gls", "psfsw_robust") for m in wt.get("production", [])),
      "production mode outside frozen triplet")

    # C4 forbidden tokens
    fb = M.get("forbidden", {})
    C("C4", set(fb.get("weight_source_tokens", [])) == set(ADJ.get("forbidden_weight_source_tokens", [])),
      "forbidden weight_source_tokens != adjudications")
    C("C4b", set(fb.get("weight_source_tokens", [])) == set(CAT["forbidden"]["weight_source_tokens"]),
      "forbidden tokens != catalog")
    C("C4c", set(fb.get("psfsw_forbidden_keys", [])) == set(CAT["forbidden"]["psfsw_forbidden_keys"]),
      "psfsw_forbidden_keys != catalog")
    C("C4d", PSFSW_FREEZE_KEYS.issubset(set(fb.get("psfsw_forbidden_keys", []))),
      "psfsw_forbidden_keys missing FREEZE_LIST set")
    for tok in ("median_source_snr", "psfsw_robust_weight", "psfsw"):
        C("C4e:" + tok, tok in fb.get("weight_source_tokens", []), "token missing from forbidden set: " + tok)

    # C5 clause inventory vs SCI-ADJ (42) + required (19)
    clauses = M.get("clauses", [])
    ids = [c.get("id") for c in clauses]
    C("C5", len(ids) == len(set(ids)), "duplicate clause ids")
    adj_ids = set(c["id"] for c in ADJ["freeze_table"])
    C("C5b", set(M.get("all_sci_adj_freeze_ids", [])) == adj_ids, "all_sci_adj_freeze_ids != adjudications")
    C("C5c", adj_ids.issubset(set(ids)), "missing SCI-ADJ freeze clause(s): %r" % sorted(adj_ids - set(ids)))
    req = set(ADJ["required_freeze_ids"])
    C("C5d", set(M.get("required_freeze_ids", [])) == req, "required_freeze_ids != adjudications")
    C("C5e", req.issubset(set(ids)), "required freeze id missing: %r" % sorted(req - set(ids)))
    C("C5f", all(c.get("required_freeze_id") for c in clauses if c["id"] in req), "required flag not set")

    # C6 semantic value/scope/gate/anchor identical to adjudications
    ft = {c["id"]: c for c in ADJ["freeze_table"]}
    for c in clauses:
        if c["id"] in ft:
            e = ft[c["id"]]
            for k_src, k_dst in (("value", "value"), ("scope", "scope"), ("gate", "gate"), ("anchor", "anchor")):
                if c.get(k_dst) != e.get(k_src):
                    C("C6:" + c["id"], False, "%s mismatch vs SCI-ADJ" % k_dst)
            C("C6:" + c["id"], True, "")

    # C7 status vocabulary + signoff consistency
    for c in clauses:
        C("C7:" + str(c.get("id")), c.get("status") in VOCAB, "bad status vocab")
        so = c.get("owner_signoff")
        if c["id"] in ft:
            if so is None:
                C("C7a:" + c["id"], c.get("status") == "FROZEN", "unsigned semantic clause must be FROZEN")
            else:
                C("C7b:" + c["id"], c.get("status") == "PENDING_OWNER_SIGNOFF", "signed clause must be PENDING_OWNER_SIGNOFF")
                C("C7c:" + c["id"], bool(c.get("signoff_reason")), "PENDING clause missing signoff_reason")

    # C8 numeric thresholds: unique value or explicit pending+owner; source binding resolvable
    bind_count = 0
    for c in clauses:
        if c.get("kind") != "threshold":
            continue
        cid = c["id"]
        val = c.get("value")
        status = c.get("status")
        if val is None:
            C("C8-null:" + cid, status in ("PENDING_OWNER_SIGNOFF", "OPEN") and bool(c.get("owner_signoff")),
              "null numeric value without pending+owner")
            continue
        for fz in FUZZY:
            C("C8-fuzzy:" + cid, fz not in str(val).lower(), "fuzzy numeric value: %r" % val)
        if status == "FROZEN":
            C("C8-own:" + cid, c.get("owner_signoff") is None, "FROZEN numeric clause must not carry owner_signoff")
        elif status == "PENDING_OWNER_SIGNOFF":
            C("C8-own:" + cid, bool(c.get("owner_signoff")), "PENDING numeric clause missing owner_signoff")
        b = c.get("source_binding")
        C("C8-bind:" + cid, bool(b), "numeric clause missing source_binding")
        if not b:
            continue
        bind_count += 1
        f = b.get("file")
        C("C8-file:" + cid, bool(f) and os.path.isfile(os.path.join(ROOT, f)), "binding file missing: %r" % f)
        if not (f and os.path.isfile(os.path.join(ROOT, f))):
            continue
        txt = readtext(f)
        loc, cont = str(b.get("locator")), str(b.get("contains"))
        if b.get("kind") == "md_table_row":
            rows = [ln for ln in txt.splitlines() if ln.lstrip().startswith("|") and loc in ln]
            C("C8-row:" + cid, any(cont in ln for ln in rows),
              "W3 source row %r does not contain %r" % (loc, cont))
        else:
            i = txt.find(loc)
            C("C8-loc:" + cid, i >= 0, "locator %r not found in %s" % (loc, f))
            if i >= 0:
                win = txt[max(0, i - 50): i + 600]
                C("C8-cont:" + cid, cont in win, "contains %r not near locator %r in %s" % (cont, loc, f))
        if isinstance(val, (int, float)) and not isinstance(val, bool):
            cd = str(val)
            okv = (cont in cd) or (cd in cont)
            if not okv:
                try:
                    okv = abs(float(cont) - float(val)) <= 1e-12 * max(1.0, abs(float(val)))
                except Exception:
                    pass
            if not okv and cont.endswith("%"):
                try:
                    okv = abs(float(cont[:-1]) / 100.0 - float(val)) <= 1e-12
                except Exception:
                    pass
            C("C8-value:" + cid, okv, "frozen value %r inconsistent with W3 binding %r" % (val, cont))

    # C9 declared weight sources disjoint from forbidden tokens & no psfsw/median as ivar
    dsw = M.get("declared_weight_sources", {})
    forb = set(fb.get("weight_source_tokens", []))
    for mode, srcs in dsw.items():
        overlap = set(srcs) & forb
        C("C9:" + mode, not overlap, "declared weight sources hit forbidden tokens: %r" % sorted(overlap))
        C("C9b:" + mode, "median_source_snr" not in srcs and "median_snr" not in srcs, "median SNR used as weight source")
    # psfsw must remain dimensionless
    detail = {m["mode"]: m for m in M["weight_modes"]["mode_details"]}
    C("C9c", detail["psfsw_robust"]["units"] == "1", "psfsw_robust units != 1")
    C("C9d", detail["psfsw_robust"]["group_normalized"] is True, "psfsw_robust group_normalized != true")
    C("C9e", detail["point_information"]["units"] == "ADU^-2", "point_information units != ADU^-2")
    C("C9f", detail["surface_gls"]["units"] != "1", "surface_gls units degenerate")
    # no clause declares psfsw as ivar
    for c in clauses:
        v = str(c.get("value", ""))
        C("C9g:" + c["id"], not re.search(r"psfsw[_a-z]*\s*=\s*(ivar|1/variance|ADU\^-2)", v),
          "clause equates psfsw weight with ivar/variance")

    # C10 signoff items SO-01..07 all pending with owner
    so = {s["id"]: s for s in M.get("signoff_items", [])}
    for i in range(1, 8):
        sid = "SO-0%d" % i
        C("C10:" + sid, sid in so, "missing signoff item " + sid)
        if sid in so:
            C("C10b:" + sid, so[sid].get("status") == "PENDING_OWNER_SIGNOFF", "signoff item not pending")
            C("C10c:" + sid, bool(so[sid].get("owner")), "signoff item missing owner")

    # C11 open items coverage
    oids = set(o["id"] for o in M.get("open_items", []))
    need = set(["DI-0%d" % i for i in range(1, 8)]) | set(["OI-0%d" % i for i in range(1, 6)]) | \
           set(["PF-0%d" % i for i in range(1, 8)])
    C("C11", need.issubset(oids), "missing open items: %r" % sorted(need - oids))
    for o in M.get("open_items", []):
        C("C11b:" + o["id"], bool(o.get("owner")), "open item missing owner")

    # C12 superseded SCI coverage
    sup = M.get("superseded_sections", [])
    files = " ".join(s.get("file", "") for s in sup)
    for tok in ("DRIZZLE.md", "CONTROL_WEIGHT_SNR.md", "ACR_EQUIVALENCE.md", "INTEGRATION.md", "CALIBRATION.md", "PHASE3_HIPS_TO_FITS.md"):
        C("C12:" + tok, tok in files, "superseded list missing " + tok)
    allids = set(ids)
    for s in sup:
        for sid in s.get("superseded_by", []):
            C("C12b:" + s["id"] + ":" + sid, (sid in allids) or sid.startswith("ADJ-"), "superseded_by id unresolvable: " + sid)
        C("C12c:" + s["id"], bool(s.get("signoff")), "superseded section missing signoff")

    # C13 W3 spec consistency (phase1 machine spec + QA pending count)
    C("C13", P1.get("production_weight_modes") == ADJ["production_weight_modes"], "phase1 spec production modes mismatch")
    C("C13b", P1.get("deferred_modes") == ADJ["deferred_modes"], "phase1 spec deferred modes mismatch")
    oi = set(o["id"] for o in P1.get("open_items", []))
    C("C13c", set(["OI-0%d" % i for i in range(1, 6)]).issubset(oi), "phase1 spec OI-01..05 incomplete")
    qam = readtext("reports/v6/qa-design/qa_matrix.json")
    C("C13d", qam.count('"pending_freeze"') == 7, "qa_matrix pending_freeze count != 7 (%d)" % qam.count('"pending_freeze"'))
    ifs = M.get("counts", {})
    C("C13e", ifs.get("open_items_registered") == len(M.get("open_items", [])), "counts.open_items mismatch")
    C("C13f", ifs.get("superseded_sections") == len(sup), "counts.superseded mismatch")
    C("C13g", ifs.get("frozen") == sum(1 for c in clauses if c["status"] == "FROZEN"), "counts.frozen mismatch")
    C("C13h", ifs.get("pending_owner_signoff") == sum(1 for c in clauses if c["status"] == "PENDING_OWNER_SIGNOFF"), "counts.pending mismatch")
    C("C13i", ifs.get("open") == sum(1 for c in clauses if c["status"] == "OPEN"), "counts.open mismatch")

    # C14 no clause promotes psf_snr_power to production / forbidden tokens absent from declared sources
    for c in clauses:
        if c["id"] in ("FZ-MODE-PRODUCTION", "FZ-MODE-DEFERRED"):
            continue
        v = str(c.get("value", "")) + " " + str(c.get("subject", ""))
        C("C14:" + c["id"], "psf_snr_power" not in v or "DEFERRED" in v or "deferred" in v or "禁止" in v or "不进" in v,
          "clause may promote psf_snr_power")
    C("C14b", "psf_snr_power" not in wt.get("production", []), "psf_snr_power in production")
    C("C14c", "psf_snr_power" not in wt.get("documented_baseline", []), "psf_snr_power in documented baseline")

    # C15 required fields
    for c in clauses:
        for k in ("id", "kind", "layer", "status", "fail_closed", "negative_mutation", "scope", "gate"):
            C("C15:%s:%s" % (c.get("id"), k), c.get(k) not in (None, ""), "clause missing field " + k)

    return fails, checks

def main(argv):
    path = DEFAULT_MACHINE
    if "--json" in argv:
        path = argv[argv.index("--json") + 1]
    fails, checks = run_oracle(path)
    print("Oracle checks run:", len(checks))
    if fails:
        print("FAILURES (%d):" % len(fails))
        for f in fails:
            print("  - " + f)
        return 1
    print("ALL PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
