# -*- coding: utf-8 -*-
"""AUDIT-06 D4 prescan :: candidate classifier (v3) over side_matrix.json.

Deterministic, recomputable criteria.  Reads prescan_extract.py's dump only.

  J1  数值不等            two or more registration sides (侧1/2/3/4/6) carry
                          different normalised literals for the same key
  J1c 登记面 vs 代码兜底   a production read point / cfg member assignment
                          disagrees with every registration side
  J1t 仅 bool/int 表示差异 true≡1 / false≡0 (typed as its own class, not J1)
  J2  值域自相矛盾        one schema object's own `default` falls outside its own
                          enum / const / minimum / exclusiveMinimum / maximum
  J3  哨兵语义不一致      0 / -1 / null carries "unset" on one side while another
                          side carries a positive legal default for the same key
  J4  登记即错            config_registry declared_default disagrees with the
                          value at its own registered_at anchor (defaults.json
                          key or schema pointer), or the anchor carries no value
  J5  该导出却硬编码      key registered somewhere but production never reads it
                          from the config face (only compile-time literals)
  J5b 读键无登记面        production reads "<key>" from a config carrier while no
                          registration side declares it
  J6  同名异物风险        the differing values come from different qualified
                          domains -- needs human reading before grading
"""
from __future__ import annotations

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(sys.argv[1])
SIDES = ["side1_defaults", "side2_template", "side3_cli",
         "side4_schema", "side5_code", "side6_registry"]
REG_SIDES = ["side1_defaults", "side2_template", "side3_cli",
             "side4_schema", "side6_registry"]
SIDE_LABEL = {"side1_defaults": "侧1", "side2_template": "侧2", "side3_cli": "侧3",
              "side4_schema": "侧4", "side5_code": "侧5", "side6_registry": "侧6"}

DOMAIN_ALIAS = {"noise_config": "noise", "drizzle_config": "drizzle",
                "wcs_config": "wcs", "star_detection_config": "star_detection",
                "cosmetic_config": "cosmetic", "photometry_config": "photometry",
                "normalize_block": "", "mosaic_block": "", "export_block": "",
                "mosaic_config": "", "export_config": "", "export_wcs": "",
                "export_crop": "crop", "filter_name": "", "precision": "",
                "platesolve": "platesolve", "coverage_index_ref": "",
                "source_hips": "source", "center": "center",
                "kernel_v1": "cpu_profile", "kernel_v2": "cpu_profile",
                "host": "cpu_profile", "block": "cpu_profile"}
PHASE_SCHEMAS = ["eng/contracts/schemas/phase_config_normalize.schema.json",
                 "eng/contracts/schemas/phase_config_mosaic.schema.json",
                 "eng/contracts/schemas/phase_config_export.schema.json"]


def fmt(n):
    if isinstance(n, float) and n == int(n) and abs(n) < 1e15:
        return str(int(n))
    if isinstance(n, float):
        return repr(n)
    return str(n)


def norm(v):
    """-> (kind, canon, display); kind in num|bool|str|list|na|absent"""
    if v is None:
        return ("na", None, "—")
    if isinstance(v, bool):
        return ("bool", "true" if v else "false", "true" if v else "false")
    if isinstance(v, (int, float)):
        return ("num", float(v), fmt(float(v)))
    s = str(v).strip()
    if s in ("true", "false"):
        return ("bool", s, s)
    if s[:1] in "[{" and s[-1:] in "]}":
        try:
            j = json.loads(s)
        except Exception:
            return ("str", s.lower(), s)
        return ("list", json.dumps(j, sort_keys=True),
                "[" + "×".join(fmt(float(x)) if isinstance(x, (int, float))
                               else str(x) for x in j) + "]")
    t = s.strip('"')
    try:
        return ("num", float(t), fmt(float(t)))
    except ValueError:
        return ("str", t.lower(), t)


PLACEHOLDER = re.compile(r"path/to/|\.\./|\.fits$|\.xisf$|/out/|^——$|^无$|^中心$"
                         r"|^由 |^全部$|^全$|^安装目录|^stdout|<output_dir>"
                         r"|^逐帧|^键缺省|^随 `|派生|标定|实测|^~|非空|必填")


def is_placeholder(disp):
    return bool(PLACEHOLDER.search(str(disp).strip().strip('"')))


def canon_of(rec, side):
    if side == "side4_schema":
        return norm(rec["value"]) if rec.get("kw") == "default" else None
    if side == "side1_defaults":
        if rec.get("value") is None:
            return ("null", "null", "null")
        return norm(rec.get("value"))
    if side == "side3_cli":
        v = rec.get("template_value")
        if v is None:
            v = rec.get("value")
        if v is None:
            return ("absent", "absent", "骨架不写值(json=nullptr)")
        return norm(v)
    if side == "side6_registry":
        v = rec.get("declared_default")
        return norm(v) if v is not None else None
    if side == "side2_template":
        return norm(rec.get("value"))
    return norm(rec.get("value"))


def domain_of(rec, side):
    if side == "side1_defaults":
        p = rec["key"].split(".")[:-1]
    elif side == "side2_template":
        p = rec["key"].replace("blocks[].", "").split(".")[:-1]
    elif side == "side3_cli":
        base = (rec.get("nested_of") or rec["key"]).replace(" (block#2)", "")
        p = base.split(".")[:-1]
    elif side == "side4_schema":
        ch = [c for c in rec.get("chain", "").split("/")[:-1]
              if c not in ("properties", "$defs")]
        p = [DOMAIN_ALIAS.get(c, c) for c in ch[-1:]]
    elif side == "side6_registry":
        p = [x for x in (rec.get("registered_key") or "").split(".")[:-1]
             if x not in ("blocks[]", "config", "mosaic", "export", "normalize")]
        if not p:
            p = (rec.get("key") or "").split(".")[:-1]
    else:
        p = rec["key"].split(".")[:-1]
    return ".".join(x for x in p if x)


def resolve_pointer(doc, pointer):
    if not pointer.startswith("#/"):
        return False, None
    cur = doc
    for seg in pointer[2:].split("/"):
        seg = seg.replace("~1", "/").replace("~0", "~")
        if isinstance(cur, dict) and seg in cur:
            cur = cur[seg]
        elif isinstance(cur, list) and seg.isdigit() and int(seg) < len(cur):
            cur = cur[int(seg)]
        else:
            return False, None
    return True, cur


SCHEMA = {}


def get_doc(rel):
    if rel not in SCHEMA:
        try:
            SCHEMA[rel] = json.load(io.open(os.path.join(REPO, rel),
                                            encoding="utf-8-sig"))
        except Exception:
            SCHEMA[rel] = None
    return SCHEMA[rel]


def main():
    mat = json.load(io.open(os.path.join(HERE, "side_matrix.json"),
                            encoding="utf-8"))["matrix"]
    led = json.load(io.open(os.path.join(
        REPO, "eng/ci/ledgers/config_default_divergences.json"), encoding="utf-8"))
    waivered = {e["id"].split(":", 1)[1] for e in led["entries"]
                if e["id"].startswith("config_default_divergence:")}
    watch = sorted({k for e in led["entries"] for k in (e.get("watch_keys") or [])})
    dead = json.load(io.open(os.path.join(
        REPO, "eng/ci/ledgers/dead_config_keys.json"), encoding="utf-8"))
    dead_keys = {e["id"].split(":", 1)[1] for e in dead["entries"]}
    s5all = json.load(io.open(os.path.join(HERE, "side5_reads_all.json"),
                              encoding="utf-8"))
    claims_path = os.path.join(HERE, "claims.json")
    claims = json.load(io.open(claims_path, encoding="utf-8")) \
        if os.path.isfile(claims_path) else []
    claims_by_leaf = {}
    for c in claims:
        claims_by_leaf.setdefault(c["leaf"], []).append(c)

    # ---- faithful re-implementation of CHK-CONFIG-DEFAULTS' D1 key set -------
    gate_d1 = set()
    tpl_leaf_contributed = 0
    tdir = os.path.join(REPO, "eng/packaging/config/templates")
    for fn in sorted(os.listdir(tdir)):
        if not fn.endswith(".json"):
            continue
        doc = get_doc("eng/packaging/config/templates/" + fn)
        stack = [doc.get("config") or {}]        # <-- the gate's own assumption
        while stack:
            node = stack.pop()
            if not isinstance(node, dict):
                continue
            for key, value in node.items():
                if isinstance(value, dict):
                    stack.append(value)
                else:
                    gate_d1.add(key)
                    tpl_leaf_contributed += 1
    for f in get_doc("eng/packaging/config/defaults.json")["fields"]:
        gate_d1.add(f["key"].split(".")[-1])
    for rel in PHASE_SCHEMAS:
        for name, definition in (get_doc(rel).get("$defs") or {}).items():
            if name.endswith("_config") and isinstance(definition, dict):
                gate_d1 |= set(definition.get("properties") or {})
    gate_d1 |= set(watch)
    json.dump({"template_leaf_keys_contributed": tpl_leaf_contributed,
               "gate_d1_size": len(gate_d1), "keys": sorted(gate_d1)},
              io.open(os.path.join(HERE, "gate_d1_keys.json"), "w",
                      encoding="utf-8"), ensure_ascii=False, indent=1)

    # ---- J2: schema objects whose own default violates their own bounds ------
    objs = {}
    for leaf, cell in mat.items():
        for r in cell["side4_schema"]:
            src, _, chain = r["loc"].partition("#")
            objs.setdefault((src, chain), {})[r["kw"]] = r["value"]
    j2 = {}
    for (src, chain), kw in sorted(objs.items()):
        if "default" not in kw or not chain:
            continue
        try:
            d = json.loads(kw["default"])
        except Exception:
            continue
        probs = []
        for b, hi in (("minimum", False), ("exclusiveMinimum", False),
                      ("maximum", True), ("exclusiveMaximum", True)):
            if b in kw and isinstance(d, (int, float)) and not isinstance(d, bool):
                lim = json.loads(kw[b])
                if (d > lim if hi else d < lim) or \
                        (b.startswith("exclusive") and d == lim):
                    probs.append("%s=%s" % (b, kw[b]))
        if "enum" in kw:
            try:
                en = json.loads(kw["enum"])
            except Exception:
                en = None
            if isinstance(en, list) and d not in en:
                flat = [x for sub in en for x in (sub if isinstance(sub, list) else [sub])]
                if not (isinstance(d, list) and any(x in flat for x in d)):
                    probs.append("enum=%s" % json.dumps(en, ensure_ascii=False)[:70])
        if "const" in kw and d != json.loads(kw["const"]):
            probs.append("const=%s" % kw["const"])
        if probs:
            lf = chain.split("/")[-1]
            j2.setdefault(lf, []).append("%s#%s：default=%s 越自身值域 %s"
                                         % (src, chain, kw["default"], "; ".join(probs)))

    # ---------- derived aliases: defaults.json registers a scientific default
    # under a dotted key whose *carrier* key on the config face is spelled
    # differently (sparse_snr.spacing_px -> sparse_snr_spacing_px, snr.path ->
    # snr_path, detection.threshold_sigma -> detection_threshold, ...).
    # The leaf-name join -- which is what CHK-CONFIG-DEFAULTS uses -- cannot see
    # these pairs, so we derive them from the registration text itself.
    face_keys = set()
    for leaf, cell in mat.items():
        for side in ("side2_template", "side3_cli", "side4_schema", "side6_registry"):
            for r in cell[side]:
                face_keys.add(r["key"].split(".")[-1].replace(" (block#2)", ""))
                face_keys.add((r.get("registered_key") or "").split(".")[-1])
    face_keys.discard("")
    alias = {}
    for leaf, cell in mat.items():
        for f in cell["side1_defaults"]:
            txt = str(f.get("note") or "") + " " + str(f.get("constraint") or "")
            # (a) explicit "承载字段 = ... / 键面 = ..." statements only -- a free
            #     text scan over-merges (one note may mention ten unrelated keys)
            for seg in re.findall(r"承载字段[ =]*([^；。]{0,80})", txt):
                for cand in face_keys:
                    if cand != leaf and re.search(r"(?<![\w.])%s(?![\w.])"
                                                  % re.escape(cand), seg):
                        alias.setdefault(leaf, set()).add(cand)
                        alias.setdefault(cand, set()).add(leaf)
            # (b) enum_target pointer -> the schema property it points at
            tgt = f.get("enum_target") or {}
            if isinstance(tgt, dict) and tgt.get("pointer"):
                pl = str(tgt["pointer"]).rstrip("/").split("/")[-1]
                if pl in face_keys and pl != leaf:
                    alias.setdefault(leaf, set()).add(pl)
                    alias.setdefault(pl, set()).add(leaf)
            # (c) registry-declared identity: registered_key leaf == the face key
            #     spelling under which the same default is carried
    # (b) registration-declared identity: registry field "detection_threshold"
    # is registered against defaults key "detection.threshold_sigma" -> same field.
    # inputs_block is excluded: there registered_key names the *container*
    # (blocks[].input_lights carries gain/read_noise), not an identity.
    for cell_leaf, cell in mat.items():
        for r in cell["side6_registry"]:
            if r.get("registration") == "inputs_block":
                continue
            rk = (r.get("registered_key") or "").split(".")[-1]
            if rk and rk != cell_leaf and rk in mat:
                alias.setdefault(cell_leaf, set()).add(rk)
                alias.setdefault(rk, set()).add(cell_leaf)
    json.dump({k: sorted(v) for k, v in alias.items()},
              io.open(os.path.join(HERE, "derived_aliases.json"), "w",
                      encoding="utf-8"), ensure_ascii=False, indent=1)
    # merge aliased cells into one row (canonical = the most qualified spelling)
    parent = {k: k for k in mat}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra == rb:
            return
        # canonical = the longer key name (more qualified spelling)
        keep, drop = (ra, rb) if len(ra) >= len(rb) else (rb, ra)
        parent[drop] = keep
    for src, tgts in alias.items():
        for t in tgts:
            if t in parent and src in parent:
                union(src, t)
    groups = {}
    for k in mat:
        groups.setdefault(find(k), []).append(k)
    merged = {}
    for canon, members in groups.items():
        cell = {s: [] for s in SIDES}
        for m in sorted(members):
            for s in SIDES:
                cell[s].extend(mat[m][s])
        merged[canon] = cell
        if len(members) > 1:
            merged[canon]["_aliases"] = sorted(x for x in members if x != canon)
    mat = merged

    rows = []
    defaults_by_key = {f["key"]: f for f in get_doc(
        "eng/packaging/config/defaults.json")["fields"]}
    for leaf, cell in sorted(mat.items()):
        side_vals = {}
        doms = {}
        for side in SIDES:
            for r in cell[side]:
                if side == "side1_defaults":
                    r["loc"] = "%s [%s]" % (r["loc"], r.get("authority_status"))
                    if r.get("value") is None:
                        r["_null"] = True
                c = canon_of(r, side)
                if c is None:
                    continue
                kind, can, disp = c
                if kind == "na":
                    continue
                flag = ""
                if kind == "absent" or is_placeholder(disp):
                    flag = "|示例值" if kind != "absent" else "|骨架不写值"
                tag = disp + flag
                d = side_vals.setdefault(side, {})
                e = d.setdefault(tag, {"locs": [], "kinds": set(), "doms": set(),
                                       "kind": kind, "can": can})
                e["locs"].append(r["loc"])
                e["kinds"].add(r.get("kind") or kind)
                dom = domain_of(r, side)
                e["doms"].add(dom)
                doms.setdefault(dom, set()).add(side)
        hits, notes = [], []

        def usable(x):
            return "|示例值" not in x and "|骨架不写值" not in x

        def clean(x):
            return x.split("|")[0]

        strict_kinds = {"read-arg", "read-arg[]", "ternary-fallback"}
        code = side_vals.get("side5_code", {})
        strict_code = {k: v for k, v in code.items()
                       if usable(k) and (v["kinds"] & strict_kinds
                                         or any(x.startswith("member-assign")
                                                for x in v["kinds"]))}
        reg_seen = {}
        for s in REG_SIDES:
            for disp in side_vals.get(s, {}):
                if usable(disp):
                    reg_seen.setdefault(clean(disp), set()).add(s)
        # ---------------- J1 / J1t / J1c
        if len(reg_seen) > 1:
            cans = {side_vals[s][d]["can"] for s in REG_SIDES
                    for d in side_vals.get(s, {}) if usable(d)}
            nums = {c for c in cans if isinstance(c, float)}
            bools = {c for c in cans if c in ("true", "false")}
            only_bool_int = bool(bools) and nums <= {0.0, 1.0} and \
                all(isinstance(c, str) or c in (0.0, 1.0) for c in cans)
            if only_bool_int and len(cans) <= 3:
                hits.append("J1t")
            else:
                hits.append("J1")
            notes.append("登记侧取值：%s" % "; ".join(
                "%s∈{%s}" % (SIDE_LABEL[s], ",".join(sorted(
                    clean(x) for x in side_vals[s] if usable(x))))
                for s in REG_SIDES
                if s in side_vals and any(usable(x) for x in side_vals[s])))
        if strict_code:
            cvals = {strict_code[k]["can"] for k in strict_code}
            regvals = {v["can"] for s in REG_SIDES for k, v in side_vals.get(s, {}).items()
                       if usable(k)}
            if regvals and not (cvals & regvals):
                hits.append("J1c")
                notes.append("登记侧 %s 与代码兜底 %s 无交集"
                             % (sorted(reg_seen), sorted(clean(k) for k in strict_code)))
            elif len(cvals | regvals) > max(1, len(regvals)):
                hits.append("J1c")
                notes.append("登记侧 %s vs 代码兜底 %s"
                             % (sorted(reg_seen), sorted(clean(k) for k in strict_code)))
        # ---------------- J2
        if leaf in j2:
            hits.append("J2")
            notes += j2[leaf]
        # ---------------- J3 sentinel
        s1txt = " ".join((r.get("constraint") or "") + " " + (r.get("note") or "")
                         for r in cell["side1_defaults"])
        allv = {s: {clean(k) for k in side_vals[s] if usable(k)} for s in side_vals}
        zeros = {s for s, d in allv.items() if d & {"0", "-1", "0.0", "-1.0"}}
        positives = {s for s, d in allv.items()
                     if any(re.match(r"^-?\d+(\.\d+)?([eE][-+]?\d+)?$", x)
                            and float(x) > 0 for x in d)}
        if zeros and positives and zeros != positives and re.search(
                r"未提供|unset|未设|未知|哨兵|0/负|不得为 ?0|必须 ?> ?0|恒正|缺失|= ?0",
                s1txt):
            hits.append("J3")
            m = re.search(r"[^；;]{0,58}(未提供|unset|未设|哨兵|0/负)[^；;]{0,36}", s1txt)
            notes.append("0/-1 承载侧=%s；正值侧=%s；侧1 文义：%s" % (
                sorted(zeros), sorted(positives), m.group(0).strip() if m else "—"))
        # ---------------- J4 registry anchor
        for r in cell["side6_registry"]:
            if r.get("registration") in (None, "none") or r.get("declared_default") is None:
                continue
            dd = r["declared_default"]
            if r.get("composite"):
                notes.append("登记册本行是复合字段名 %r（多键共用一个默认列），"
                             "值列按整串取 ⇒ 不可机器判等，需人工拆行"
                             % r.get("field_raw"))
            anchor = r.get("registered_at") or ""
            rel, _, pointer = anchor.partition("#")
            if rel.endswith("defaults.json"):
                full = r.get("registered_key") or ""
                entry = defaults_by_key.get(full)
                if entry is None:
                    cands = [k for k in defaults_by_key if k.endswith("." + full)
                             or k == full]
                    entry = defaults_by_key[cands[0]] if len(cands) == 1 else None
                if entry is None:
                    hits.append("J4")
                    notes.append("登记册 registration=defaults_json registered_key=%s，"
                                 "但 defaults.json 无该键（declared_default=%r）"
                                 % (full, dd))
                    continue
                val = entry.get("value")
                if norm(val)[1] != norm(dd)[1]:
                    hits.append("J4")
                    notes.append("登记册 declared_default=%r 声称已登记，defaults.json 实测="
                                 "%r（%s，authority_status=%s）"
                                 % (dd, val, "eng/packaging/config/defaults.json#" +
                                    entry["key"], entry.get("authority_status")))
                elif entry.get("authority_status") == "pending_authority" and val is None:
                    hits.append("J4")
                    notes.append("登记册 declared_default=%r，defaults.json 该键 "
                                 "value=null + pending_authority（%s）"
                                 % (dd, entry["key"]))
                continue
            if rel.endswith(".schema.json") and pointer.startswith("/"):
                doc = get_doc(rel)
                found, node = (resolve_pointer(doc, "#" + pointer)
                               if doc else (False, None))
                if found and isinstance(node, dict) and "default" not in node:
                    sub = (r.get("registered_key") or "").split(".")[-1]
                    props = node.get("properties")
                    if isinstance(props, dict) and sub in props:
                        node = props[sub]
                if not isinstance(node, dict) or "default" not in node:
                    hits.append("J4?")
                    notes.append("登记点 %s 不承载 default（登记册 declared_default=%r）"
                                 % (anchor.replace(rel + "#", ""), dd))
                elif norm(node["default"])[1] != norm(dd)[1]:
                    hits.append("J4")
                    notes.append("登记册 declared_default=%r ≠ schema 锚点 default=%r（%s）"
                                 % (dd, node["default"], anchor.replace(rel + "#", "")))
        # ---------------- J5 registered but never read from the config face
        registered = any(side_vals.get(s) for s in REG_SIDES)
        if registered and not strict_code and side_vals.get("side5_code"):
            hits.append("J5")
            notes.append("登记面有值、生产无配置读取点（代码侧仅 %s 字面量）"
                         % sorted({k for v in side_vals["side5_code"].values()
                                   for k in v["kinds"]}))
        # ---------------- J6 homonym risk
        real_doms = {d for d in doms if d}
        if len(real_doms) > 1 and ({"J1", "J1c"} & set(hits)):
            hits.append("J6")
            notes.append("限定域并列：%s" % "; ".join(
                "%s←%s" % (d, ",".join(SIDE_LABEL[s] for s in sorted(doms[d])))
                for d in sorted(real_doms)))
        # ---------------- gate coverage verdict
        code_multi = len({clean(k) for k in strict_code}) > 1
        if code_multi:
            gate = "部分：门只比侧5跨单元字面量（侧2恒空），登记面数值不参与"
        elif leaf in gate_d1:
            gate = "否：键在门 D1 集合内，但门不比较侧1/3/4/6 的数值"
        else:
            gate = "否：键不在门 D1 集合内（模板面因 blocks[] 形态恒空）"
        if leaf in dead_keys:
            gate += "；死键台账已登记"
        if leaf in waivered:
            gate += "；差异台账已豁免"
        al = cell.get("_aliases") or []
        if al:
            notes.append("同名异写合并：%s（预筛按登记文本/注册键并入本行，"
                         "键面本身不同名 ⇒ 门按末段名比较时看不见）"
                         % ",".join(al))
        rows.append({"key": leaf, "aliases": al,
                     "aliases": cell.get("_aliases", []),
                     "sides": {s: {v: {"locs": sorted(set(i["locs"])),
                                       "kinds": sorted(i["kinds"]),
                                       "doms": sorted(i["doms"]),
                                       "can": i["can"], "kind": i["kind"]}
                                   for v, i in side_vals[s].items()}
                               for s in side_vals},
                     "n_reg_sides": len([s for s in REG_SIDES
                                         if any(usable(x) for x in side_vals.get(s, {}))]),
                     "n_sides": len(side_vals), "hits": hits, "notes": notes,
                     "gate": gate, "domains": sorted(real_doms),
                     "waivered": leaf in waivered, "dead": leaf in dead_keys})

    # ---- J5b: config reads with no registration face -------------------------
    covered_keys = {r["key"] for r in rows}
    extra = {}
    for r in s5all:
        if r["leaf"] in covered_keys or not r.get("carrier_hint"):
            continue
        extra.setdefault(r["leaf"], []).append(r)
    for leaf, recs in sorted(extra.items()):
        vals = {}
        for r in recs:
            vals.setdefault(norm(r["value"])[2], []).append(r["loc"])
        rows.append({"key": leaf,
                     "sides": {"side5_code": {v: {"locs": sorted(set(l)),
                                                  "kinds": ["read-arg/undeclared"],
                                                  "doms": [], "can": norm(v)[1],
                                                  "kind": norm(v)[0]}
                                              for v, l in vals.items()}},
                     "n_reg_sides": 0, "n_sides": 1,
                     "hits": ["J5b"],
                     "notes": ["生产按配置载体读取 \"%s\" 并带缺省字面量，"
                               "六侧登记面无任何一项（该侧无此项）" % leaf,
                               "读取点 %d 处：%s" % (len(recs),
                                                    "; ".join(sorted({r["loc"] for r in recs})[:4]))],
                     "gate": ("是（若键名已在门 D1/ watch_keys）" if leaf in gate_d1
                              else "否：键不在门 D1 集合内"),
                     "domains": [], "waivered": leaf in waivered,
                     "dead": leaf in dead_keys})

    order = {"J2": 0, "J4": 1, "J3": 2, "J1": 3, "J1c": 4, "J5": 5, "J5b": 6,
             "J1t": 7, "J4?": 8, "J6": 9}
    rows.sort(key=lambda r: (min([order.get(h, 9) for h in r["hits"]] or [9]),
                             -len([h for h in r["hits"] if h != "J6"]), r["key"]))
    json.dump(rows, io.open(os.path.join(HERE, "candidates.json"), "w",
                            encoding="utf-8"), ensure_ascii=False, indent=1)
    with io.open(os.path.join(HERE, "candidates.txt"), "w", encoding="utf-8") as f:
        for r in rows:
            if not r["hits"]:
                continue
            f.write("%-30s %-20s 登记侧=%d %s%s\n" % (
                r["key"], ",".join(r["hits"]), r["n_reg_sides"], r["gate"],
                ""))
            for s in SIDES:
                for v, info in sorted(r["sides"].get(s, {}).items()):
                    f.write("      %-4s %-18s %s%s\n" % (
                        SIDE_LABEL[s], v, "; ".join(info["locs"][:3]),
                        "; +%d" % (len(info["locs"]) - 3) if len(info["locs"]) > 3 else ""))
            for n in r["notes"]:
                f.write("      · %s\n" % n[:230])
        f.write("\n== keys=%d hit=%d clean=%d J2objs=%d undeclared_reads=%d\n" % (
            len(rows), sum(1 for r in rows if r["hits"]),
            sum(1 for r in rows if not r["hits"]), len(j2), len(extra)))
    sys.stdout.write("keys=%d hit=%d clean=%d\n" % (
        len(rows), sum(1 for r in rows if r["hits"]),
        sum(1 for r in rows if not r["hits"])))


if __name__ == "__main__":
    main()
