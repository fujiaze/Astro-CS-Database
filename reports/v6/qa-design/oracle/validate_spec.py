# -*- coding: utf-8 -*-
"""QA-MATRIX-001 结构/合同校验器（独立，不调用被测实现）。

校验对象：qa_matrix.json（规格）+ mutations.json（mutation 目录）+ baseline_matrix.json +
case_ledger.json（零用例/ skip-only 机制）。
用法：
  python3 validate_spec.py [--matrix qa_matrix.json] [--ledger case_ledger.json]
  python3 validate_spec.py --mutate MUT-SPEC-04        # 注入 spec mutation，期望 rc=1
退出码：0 无违规；1 有违规；2 用法/IO 错误。
"""
from __future__ import annotations
import copy, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, "data")

FAMILIES = {"analytic", "monte_carlo", "injection", "real_data", "baseline", "p0_root_cause"}
ORACLE_KINDS = {"independent_numpy", "independent_numpy_mc", "independent_stdlib", "structural",
                "real_data_checklist", "preregistered_comparison", "independent_driver"}
OPERATORS = {"<=", "<", ">=", ">", "==", "!="}
STATUSES = {"frozen", "pending_freeze"}
REQUIRED = ["gate_id","family","phase","objective","title","claim","anchors","inputs","outputs",
            "units","domain","criterion","tol_source","zero_case_red","fail_closed","oracle",
            "mutations","owner","wave","status"]
ANCHOR_RE = re.compile(r"(FZ-[A-Z0-9\-]+|ADJ-[A-Z0-9\-]+|SCI-[A-Z0-9\-]+|AR-\d+|DESIGN-P[123]|UNIFIED|"
                       r"PSF_SIGNAL_WEIGHT|PROJECT_SPEC|C-004|C-P3-PROP|DRIZZLE|DATA_SEMANTICS|"
                       r"PHASE3|RULINGS|FREEZE_LIST|psf_signal|p1drz|testdata|Oracle|K[0-9]|M[0-9]|"
                       r"宪章|UNIFIED_SCIENCE)")
NEGATION = ("不得", "禁止", "撤销", "legacy", "退休", "REJECT", "illegal", "非法", "取代", "非目标", "不能")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def load_all(matrix=None, ledger=None):
    qm = jload(matrix or os.path.join(ROOT, "qa_matrix.json"))
    muts = jload(os.path.join(DATA, "mutations.json"))["mutations"]
    bm = jload(os.path.join(DATA, "baseline_matrix.json"))
    meta = jload(os.path.join(DATA, "meta.json"))
    led = None
    lp = ledger or os.path.join(ROOT, "case_ledger.json")
    if os.path.exists(lp):
        led = jload(lp)
    return qm, muts, bm, meta, led


def v(rule, gate, msg):
    return {"rule": rule, "gate": gate, "msg": msg}


def validate(qm, muts, bm, meta, led=None):
    out = []
    gates = qm.get("gates", [])
    ids = [g.get("gate_id") for g in gates]
    if not gates:
        out.append(v("V-EMPTY", "-", "零门规格：qa_matrix.gates 为空"))
    if len(ids) != len(set(ids)):
        out.append(v("V-ID-DUP", "-", "gate_id 重复"))
    mut_by_id = {m["id"]: m for m in muts}
    for m in muts:
        if not m.get("expect_rc_nonzero"):
            out.append(v("V-MUT-RC", m["id"], "mutation 必须 expect_rc_nonzero=true"))

    for g in gates:
        gid = g.get("gate_id", "?")
        for k in REQUIRED:
            if k not in g or g[k] in ("", None) or g[k] == []:
                # mutations/oracle 等允许 dict；空 list 视为缺失
                if k in ("units",) and g.get(k) == {}:
                    pass
                out.append(v("V-FIELD", gid, "缺字段/空字段: %s" % k))
        if not re.match(r"^(G-[A-Z0-9]+-\d+|P0-\d+)$", str(gid)):
            out.append(v("V-ID", gid, "gate_id 命名不合规"))
        if g.get("family") not in FAMILIES:
            out.append(v("V-FAMILY", gid, "未知 family: %s" % g.get("family")))
        # criterion
        c = g.get("criterion") or {}
        if c.get("kind") not in ("numeric", "structural"):
            out.append(v("V-CRIT-KIND", gid, "criterion.kind 非法"))
        if c.get("kind") == "numeric":
            if c.get("op") not in OPERATORS:
                out.append(v("V-CRIT-OP", gid, "operator 非法: %s" % c.get("op")))
            if not isinstance(c.get("thresh"), (int, float)):
                out.append(v("V-CRIT-TH", gid, "thresh 非数值"))
        else:
            if not c.get("rule"):
                out.append(v("V-CRIT-RULE", gid, "structural criterion 缺 rule"))
        st = c.get("threshold_status")
        if st not in STATUSES:
            out.append(v("V-CRIT-STATUS", gid, "threshold_status 非法: %s" % st))
        if st == "pending_freeze" and not c.get("threshold_owner"):
            out.append(v("V-CRIT-OWNER", gid, "pending_freeze 缺 threshold_owner"))
        if st == "frozen":
            blob = " ".join(str(c.get(k, "")) for k in ("ref",)) + " " + str(g.get("tol_source", ""))
            if not ANCHOR_RE.search(blob):
                out.append(v("V-CRIT-ANCHOR", gid, "frozen 容差无真实锚（ref/tol_source）"))
        # zero case
        z = g.get("zero_case_red") or {}
        if not z.get("mechanism"):
            out.append(v("V-ZC", gid, "缺 zero_case_red.mechanism"))
        if not isinstance(z.get("min_cases"), int) or z.get("min_cases", 0) < 1:
            out.append(v("V-ZC-MIN", gid, "min_cases 必须 >=1（零用例即红）"))
        if "rc" not in str(z.get("runner_rule", "")):
            out.append(v("V-ZC-RULE", gid, "runner_rule 缺 rc 语义"))
        # oracle
        o = g.get("oracle") or {}
        if o.get("kind") not in ORACLE_KINDS:
            out.append(v("V-ORACLE-KIND", gid, "oracle.kind 非白名单: %s" % o.get("kind")))
        if not o.get("must_not"):
            out.append(v("V-ORACLE-MUSTNOT", gid, "oracle.must_not 为空（禁止同源自证）"))
        if not o.get("truth"):
            out.append(v("V-ORACLE-TRUTH", gid, "oracle.truth 为空"))
        # mutations
        gm = g.get("mutations") or []
        if not gm:
            out.append(v("V-GATE-MUT", gid, "门无 mutation（P0-01 R1 违反）"))
        for mid in gm:
            if mid not in mut_by_id:
                out.append(v("V-MUT-REF", gid, "引用不存在的 mutation: %s" % mid))
            elif gid not in mut_by_id[mid].get("targets", []):
                out.append(v("V-MUT-TARGET", gid, "mutation %s 未声明 target 本门" % mid))
        # forbidden weight sources (structured only)
        ws = g.get("weight_sources")
        if ws:
            for t in ws:
                if t in qm.get("forbidden_weight_source_tokens", []):
                    out.append(v("V-TOKEN-WS", gid, "weight_sources 含禁止诊断 token: %s" % t))
        # retired token regression lock (positive assertion)
        blob = " ".join(str(g.get(k, "")) for k in ("claim", "title", "tol_source", "fail_closed"))
        for tok in ("support×snr²", "support_x_snr2", "support*snr"):
            for sent in re.split(r"[。;\n]", blob):
                if tok in sent and not any(n in sent for n in NEGATION):
                    out.append(v("V-TOKEN-RETIRED", gid, "宣称已退休口径 %s 合法（AR-049）" % tok))
                    break
        # P0 ledger layer
        if g.get("family") == "p0_root_cause":
            if not g.get("ledger_layer"):
                out.append(v("V-LEDGER-LAYER", gid, "P0 门缺 ledger_layer（AR-051 合并层口径）"))
            if not g.get("ledger_refs"):
                out.append(v("V-LEDGER-REF", gid, "P0 门缺 ledger_refs"))
        # real-data expected independence
        if g.get("family") == "real_data" and o.get("kind") == "real_data_checklist" and "solely_production_output" not in (o.get("must_not") or []):
            out.append(v("V-RD-EXPECTED", gid, "real_data 门必须禁 solely_production_output"))

    # mutation targets exist
    for m in muts:
        for t in m.get("targets", []):
            if t not in ids:
                out.append(v("V-MUT-TARGET-ID", m["id"], "target 不存在: %s" % t))
        if m.get("mech") == "spec" and not m.get("op"):
            out.append(v("V-MUT-OP", m["id"], "spec mutation 缺 op"))

    # modes
    exp_prod = {"point_information", "surface_gls", "psfsw_robust"}
    prod = qm.get("production_modes", [])
    if set(prod) != exp_prod:
        out.append(v("V-MODES-PROD", "-", "production_modes 必须恰为三模式: %s" % prod))
    for bad in qm.get("forbidden_production_modes", []):
        if bad in prod:
            out.append(v("V-MODES-FORBIDDEN", "-", "生产模式含禁止值: %s" % bad))
    if list(qm.get("deferred_modes", [])) != ["psf_snr_power"]:
        out.append(v("V-MODES-DEFER", "-", "deferred_modes 必须为 [psf_snr_power]"))
    if set(qm.get("baseline_modes", [])) != {"equal", "pixel_ivar"}:
        out.append(v("V-MODES-BASE", "-", "baseline_modes 必须为 equal/pixel_ivar"))

    # freeze ids
    allfz = set(qm.get("all_freeze_ids", []))
    for f in qm.get("required_freeze_ids", meta.get("required_freeze_ids", [])):
        if f not in allfz:
            out.append(v("V-FZ-REQ", "-", "required_freeze_id 不在 all_freeze_ids: %s" % f))
    for g in gates:
        for a in g.get("anchors", []):
            if a.startswith("FZ-") and a not in allfz:
                out.append(v("V-FZ-ANCHOR", g["gate_id"], "FZ 锚不存在: %s" % a))

    # units override
    uo = qm.get("unit_table_override") or {}
    for k, val in uo.items():
        if meta.get("unit_table", {}).get(k) != val:
            out.append(v("V-UNITS", "-", "单位表偏离 FREEZE_LIST §1: %s=%s" % (k, val)))

    # baseline matrix
    modes = {m["mode"]: m for m in bm.get("modes", [])}
    for m in bm.get("modes", []):
        if m.get("class") not in ("production", "baseline", "deferred"):
            out.append(v("V-BM-CLASS", m.get("mode"), "class 非法"))
        if m.get("class") == "production" and m["mode"] not in exp_prod:
            out.append(v("V-BM-PROD", m["mode"], "非三模式却标 production"))
        if m.get("class") == "deferred" and m["mode"] in prod:
            out.append(v("V-BM-DEFER", m["mode"], "延迟模式进入生产"))
        if m["mode"] == "psfsw_robust":
            for t in ("ivar", "fisher"):
                if t not in str(m.get("forbidden_declaration", "")):
                    out.append(v("V-BM-PSFSW", m["mode"], "psfsw 禁止声明缺 %s" % t))
    for c in bm.get("comparisons", []):
        if c.get("baseline") not in list(modes) + ["none"]:
            out.append(v("V-BM-CELL", c.get("cell"), "baseline 未知: %s" % c.get("baseline")))
        if c.get("candidate") not in list(modes) + ["none"]:
            out.append(v("V-BM-CELL", c.get("cell"), "candidate 未知: %s" % c.get("candidate")))
        if not c.get("declaration_limit"):
            out.append(v("V-BM-LIMIT", c.get("cell"), "缺 declaration_limit"))
        if c.get("candidate") == "psf_snr_power" and c.get("cell") != "CMP-DEF":
            out.append(v("V-BM-DEFCELL", c.get("cell"), "psf_snr_power 不得参与生产比较"))
        if c.get("candidate") == "psfsw_robust" and "Fisher 最优" in str(c.get("declaration_limit", "")) \
           and not any(n in str(c.get("declaration_limit")) for n in NEGATION):
            out.append(v("V-BM-FISHER", c.get("cell"), "psfsw 声明 Fisher 最优"))

    # case ledger
    if led is not None:
        entries = {e["gate_id"]: e for e in led.get("entries", [])}
        for g in gates:
            gid = g["gate_id"]
            if gid not in entries:
                out.append(v("V-LEDGER-MISS", gid, "case_ledger 缺该门"))
                continue
            e = entries[gid]
            ex, sk, req = e.get("executed_cases", -1), e.get("skipped_cases", -1), e.get("required_cases", 0)
            if req < 1:
                out.append(v("V-LEDGER-REQ", gid, "required_cases < 1"))
            if ex < 0 or sk < 0:
                out.append(v("V-LEDGER-NEG", gid, "负计数"))
                continue
            if sk >= ex > 0:
                out.append(v("V-LEDGER-SKIP", gid, "skip-only（skipped>=executed）不得 PASS"))
            if ex == 0:
                if not e.get("pends") or e.get("counts_as_pass"):
                    out.append(v("V-LEDGER-ZERO", gid, "零用例却计 PASS / 未标 pending"))
                if e.get("implementation_cases_scheduled", 0) < req:
                    out.append(v("V-LEDGER-SCHED", gid, "pending 门未排期足够用例"))
                if not e.get("owner"):
                    out.append(v("V-LEDGER-OWNER", gid, "pending 门缺 owner"))
            else:
                if ex < req:
                    out.append(v("V-LEDGER-EX", gid, "executed_cases < required_cases"))
    return out


# ---------------------------------------------------------------- spec mutations
def set_path(obj, path, value):
    cur = obj
    for k in path[:-1]:
        cur = cur[k]
    cur[path[-1]] = value


def clear_path(obj, path):
    cur = obj
    for k in path[:-1]:
        cur = cur[k]
    cur[path[-1]] = []


def apply_spec_mutation(qm, led, op):
    qm = copy.deepcopy(qm); led = copy.deepcopy(led) if led else None
    kind = op.get("kind")
    if kind == "set_top":
        qm[op["key"]] = op["value"]
    elif kind == "set_gate":
        g = [x for x in qm["gates"] if x["gate_id"] == op["gate_id"]][0]
        set_path(g, op["path"], op["value"])
    elif kind == "set_gate_multi":
        g = [x for x in qm["gates"] if x["gate_id"] == op["gate_id"]][0]
        for s in op["sets"]:
            set_path(g, s["path"], s["value"])
    elif kind == "clear_gate":
        g = [x for x in qm["gates"] if x["gate_id"] == op["gate_id"]][0]
        clear_path(g, op["path"])
    elif kind == "add_gate_field":
        g = [x for x in qm["gates"] if x["gate_id"] == op["gate_id"]][0]
        set_path(g, op["path"], op["value"])
    elif kind == "ledger_set":
        if led is None:
            raise RuntimeError("ledger required for ledger_set")
        e = [x for x in led["entries"] if x["gate_id"] == op["gate_id"]][0]
        set_path(e, op["path"], op["value"])
    else:
        raise RuntimeError("unknown op kind %s" % kind)
    return qm, led


def main(argv):
    matrix = None; ledger = None; mutate = None
    i = 1
    while i < len(argv):
        if argv[i] == "--matrix": matrix = argv[i+1]; i += 2
        elif argv[i] == "--ledger": ledger = argv[i+1]; i += 2
        elif argv[i] == "--mutate": mutate = argv[i+1]; i += 2
        else:
            print("unknown arg", argv[i]); return 2
    qm, muts, bm, meta, led = load_all(matrix, ledger)
    if mutate:
        m = [x for x in muts if x["id"] == mutate][0]
        qm, led = apply_spec_mutation(qm, led, m["op"])
    viol = validate(qm, muts, bm, meta, led)
    doc = {"mutate": mutate, "n_violations": len(viol), "violations": viol[:60]}
    print(json.dumps(doc, ensure_ascii=False)[:4000])
    return 1 if viol else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
