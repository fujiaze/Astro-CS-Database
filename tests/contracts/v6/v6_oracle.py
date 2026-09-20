#!/usr/bin/env python3
"""SCHEMA-INTEGRATE-001 / W6 —— 独立结构 Oracle。

独立性声明：本 Oracle 以 W4 冻结表 docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json
为语义真值，对照**生产 artifact**（contracts/schemas/v6、contracts/data、docs/contracts）逐条核对；
它不复用任何生成器（gen_production_schemas.py / gen_data_dictionary.py）的结论，也不 import 生产实现。
缺项 / 枚举越界 / 单位不一致 / 待签写成已冻结 / 诊断量进权重面 → 判红（rc≠0）。

支持 overrides：{relative_path: replacement_file}，供负向 mutation 在影子文件上重跑同一 Oracle。
"""
import json, os, pathlib

try:
    from . import jsonschema_min as jm
except ImportError:  # 直接以脚本运行
    import jsonschema_min as jm

SCHEMA_FILES = [
    "astrocs.v6.units.v1.schema.json",
    "astrocs.v6.signal.v1.schema.json",
    "astrocs.v6.covariance.v1.schema.json",
    "astrocs.v6.psf.v1.schema.json",
    "astrocs.v6.effective-psf.v1.schema.json",
    "astrocs.v6.point-information.v1.schema.json",
    "astrocs.v6.weight-mode.v1.schema.json",
    "astrocs.v6.psfsw.v1.schema.json",
    "astrocs.v6.provenance.v1.schema.json",
    "astrocs.v6.phase3.v1.schema.json",
]
EXAMPLE_TARGET = {
    "units.example.json": "astrocs.v6.units.v1.schema.json",
    "signal.example.json": "astrocs.v6.signal.v1.schema.json",
    "covariance.example.json": "astrocs.v6.covariance.v1.schema.json",
    "psf.example.json": "astrocs.v6.psf.v1.schema.json",
    "effective-psf.example.json": "astrocs.v6.effective-psf.v1.schema.json",
    "point-information.example.json": "astrocs.v6.point-information.v1.schema.json",
    "weight-mode.example.json": "astrocs.v6.weight-mode.v1.schema.json",
    "psfsw.example.json": "astrocs.v6.psfsw.v1.schema.json",
    "provenance.example.json": "astrocs.v6.provenance.v1.schema.json",
    "phase3.example.json": "astrocs.v6.phase3.v1.schema.json",
}
FREEZE_REL = "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json"
DICT_REL = "contracts/data/v6_data_dictionary_v1.json"
VOCAB_REL = "contracts/data/v6_weight_vocabulary_v1.json"
MIG_REL = "contracts/data/v6_migration_map_v1.json"
DATA_SEM_REL = "docs/contracts/DATA_SEMANTICS.md"
PUB_API_REL = "docs/contracts/PUBLIC_API.md"
INTEG_REL = "docs/contracts/v6/W6_SCHEMA_INTEGRATION.md"

# 被 ac04289d 回退、不得重新引入的段落实体标记
FORBIDDEN_DOC_TOKENS = ["P33-COEF", "snr_coefficient", "SNRCOEF", "SNRCOEFN", "SNRCOEFE",
                        "P27-DEAD-PARAMS", "ipv_dead_params_lock", "### P27：",
                        "DATA-P1-SNR-COEF"]
ALIAS_FIELDS = ["kind_alias_sci_psfw", "units_alias_sci_psfw", "normalization_scope_alias"]
CATALOG_MIGRATION_IDS = ["MIG-WEIGHTMODE-LEGACY", "MIG-UNITS-PXVARIANCE", "MIG-NORM-DRIZZLE",
                         "MIG-PSFSW-VOCAB", "MIG-COVARIANCE-PRODUCT",
                         "MIG-DIAGNOSTIC-NOT-WEIGHT", "MIG-SCHEMA-OWNER"]
VALIDITY_REASONS = {"no_common_star_set", "background_nonpositive_undefined_transform",
                    "insufficient_valid_stars", "selection_bias_gate_failed",
                    "spatial_nonuniformity_gate_failed"}
PRODUCTION_MODES = {"point_information", "surface_gls", "psfsw_robust"}
BASELINE_MODES = {"equal", "pixel_ivar"}


class Oracle:
    def __init__(self, root, overrides=None):
        self.root = pathlib.Path(root)
        self.overrides = {k.replace(os.sep, "/"): v for k, v in (overrides or {}).items()}
        self.checks = []
        self.failures = []
        self._freeze = None
        self._dict = None

    # ─ IO ──
    def _read(self, rel):
        rel = rel.replace(os.sep, "/")
        if rel in self.overrides:
            return pathlib.Path(self.overrides[rel]).read_text(encoding="utf-8")
        return (self.root / rel).read_text(encoding="utf-8")

    def j(self, rel):
        return json.loads(self._read(rel))

    def freeze(self):
        if self._freeze is None:
            self._freeze = self.j(FREEZE_REL)
        return self._freeze

    def dictionary(self):
        if self._dict is None:
            self._dict = self.j(DICT_REL)
        return self._dict

    def schema(self, fn):
        return self.j("contracts/schemas/v6/" + fn)

    def _ck(self, cid, ok, detail=""):
        self.checks.append({"id": cid, "ok": bool(ok), "detail": detail})
        if not ok:
            self.failures.append("%s: %s" % (cid, detail))
        return ok

    def _prop(self, doc, *names):
        cur = doc
        for n in names:
            cur = cur["properties"][n]
        return cur

    def _const_at(self, doc, *names):
        node = self._prop(doc, *names[:-1]) if len(names) > 1 else doc
        return node["properties"][names[-1]].get("const")

    # ── 语义门（JSON Schema 无法表达的跨字段规则，Oracle 独立实现）──
    def semantic_gate_errors(self, kind, doc):
        errs = []
        fb = set(self.dictionary()["forbidden"]["weight_source_tokens"])
        psks = set(self.dictionary()["forbidden"]["psfsw_forbidden_keys"])
        tops = {p["path"].split(".")[0] for p in self.dictionary()["provenance_minimal_set"]}
        if kind == "psfsw":
            comps = doc.get("components") or {}
            for n in ("signal", "concentration", "noise", "background"):
                if not isinstance(comps.get(n), dict):
                    errs.append("missing/collapsed component: " + n)
            present = [n for n in ("signal", "concentration", "noise", "background") if isinstance(comps.get(n), dict)]
            ids = [comps[n].get("measurement_id") for n in present]
            if len(ids) != len(set(ids)):
                errs.append("measurement_id not mutually distinct")
            cfu = doc.get("component_flux_unit")
            if not cfu:
                errs.append("component_flux_unit not declared")
            if cfu and isinstance(comps.get("concentration"), dict):
                if comps["concentration"].get("units") != cfu + "/px^2":
                    errs.append("concentration units != component_flux_unit/px^2")
            for n in ("signal", "noise", "background"):
                if cfu and isinstance(comps.get(n), dict) and comps[n].get("units") != cfu:
                    errs.append(n + " units != component_flux_unit")
            for n in present:
                ss = comps[n].get("spatial_summary") or {}
                if not (ss.get("p05", 0) <= ss.get("p50", 0) <= ss.get("p95", 0)):
                    errs.append("p05<=p50<=p95 violated: " + n)
                v = ss.get("valid_area_fraction")
                if not isinstance(v, (int, float)) or not (0 <= v <= 1):
                    errs.append("valid_area_fraction out of [0,1]: " + n)
            w = doc.get("weight") or {}
            if w.get("kind") != "psfsw_robust_weight":
                errs.append("weight.kind != psfsw_robust_weight")
            if w.get("units") != "1":
                errs.append("weight.units != 1 (psfsw must be dimensionless)")
            if w.get("group_normalized") is not True:
                errs.append("group_normalized != true")
            norm = w.get("normalization") or {}
            if norm.get("scope") != "group" or norm.get("median_target") != 1.0:
                errs.append("normalization.scope/median_target")
            val = doc.get("validity") or {}
            if val.get("valid") is False and w.get("weight_value") is not None:
                errs.append("valid=false but weight_value not null")
            for key in psks:
                if key in doc or key in w:
                    errs.append("psfsw forbidden key injected: " + key)
        elif kind == "weight_mode":
            m = doc.get("weight_mode")
            mc = doc.get("mode_class")
            if isinstance(m, bool) or m in ("auto", "support_x_snr2", "psf_snr_power", 0) \
                    or (m not in PRODUCTION_MODES and m not in BASELINE_MODES):
                errs.append("illegal weight_mode: %r" % (m,))
            if m in PRODUCTION_MODES and mc != "production":
                errs.append("mode_class != production")
            if m in BASELINE_MODES and mc != "documented_baseline":
                errs.append("mode_class != documented_baseline")
            for s in (doc.get("weight") or {}).get("sources", []) or []:
                if s in fb:
                    errs.append("diagnostic token in weight.sources: " + str(s))
            if m == "psfsw_robust":
                w = doc.get("weight") or {}
                if w.get("kind") != "psfsw_robust_weight" or w.get("units") != "1" \
                        or w.get("group_normalized") is not True:
                    errs.append("psfsw canonical weight fields")
        elif kind == "provenance":
            u = doc.get("units") or {}
            bunit = u.get("bunit")
            judgeable = (isinstance(bunit, str) and "px^2" in bunit) or \
                        (bunit == "ADU" and u.get("pixel_semantics") == "surface_brightness"
                         and u.get("pixel_area_power") == -2)
            if not judgeable:
                errs.append("BUNIT not dimensionally judgeable")
            kc = doc.get("k_corr")
            if not isinstance(kc, dict):
                errs.append("k_corr missing")
            else:
                if kc.get("value") == 1:
                    errs.append("k_corr=1 ignores correlation")
                if "domain" not in kc or "calibration" not in kc:
                    errs.append("k_corr missing domain/calibration")
            for f in sorted(tops):
                if f not in doc:
                    errs.append("provenance missing minimal-set key: " + f)
        elif kind == "covariance":
            if doc.get("variance_from") in fb:
                errs.append("variance_from is a forbidden weight source")
            if doc.get("representation") == "diagonal_variance" and \
                    "correlation_kernel" not in doc and "operator_descriptor" not in doc:
                errs.append("diagonal variance without correlation kernel/operator summary")
            b = doc.get("psfsw_boundary")
            if isinstance(b, dict) and (b.get("variance_from_weight") is not False
                                        or b.get("uses_relative_weight_as_ivar") is not False):
                errs.append("psfsw_boundary violates FZ-GATE-PSFSW-COV")
        elif kind == "signal":
            ps = doc.get("pixel_semantics")
            pap = doc.get("pixel_area_power")
            if ps == "surface_brightness" and pap != -2:
                errs.append("surface_brightness requires pixel_area_power=-2")
            if ps == "integrated_flux" and pap != 0:
                errs.append("integrated_flux requires pixel_area_power=0")
        elif kind == "effective_psf":
            if not doc.get("effective_psf_id"):
                errs.append("effective_psf_id empty")
            if doc.get("definition") != "impulse_response_of_combination":
                errs.append("effective PSF must be operational impulse response")
            if "normalization" not in doc:
                errs.append("effective PSF normalization not declared")
            vom = doc.get("values_or_model") or {}
            if vom.get("kind") == "values" and not vom.get("values"):
                errs.append("effective PSF values empty")
            if vom.get("kind") == "model" and not vom.get("model"):
                errs.append("effective PSF model empty")
        return errs


    # ── 主入口 ─
    def run(self):
        self.checks = []
        self.failures = []
        self._freeze = None
        self._dict = None
        self._o01_schema_meta()
        self._o02_family()
        self._o03_units()
        self._o05_modes()
        self._o07_forbidden()
        self._o11_provenance()
        self._o14_covariance()
        self._o16_psfsw()
        self._o17_point_information()
        self._o18_weight_mode()
        self._o19_freeze_id_coverage()
        self._o20_clause_registry()
        self._o21_open_registry()
        self._o22_vocabulary()
        self._o23_migration()
        self._o24_concentration()
        self._o25_docs_present()
        self._o26_docs_guard()
        self._o27_examples()
        self._o28_clause_ids_known()
        self._o29_no_alias_or_third_vocab()
        self._o30_structural_guards()
        self._o31_guard_liveness()
        self._o32_pending_structural_refs()
        return self.checks, self.failures

    # ── checks ──
    def _o01_schema_meta(self):
        bad = []
        for fn in SCHEMA_FILES:
            try:
                d = self.schema(fn)
            except Exception as e:
                bad.append("%s unreadable: %s" % (fn, e)); continue
            if d.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
                bad.append(fn + " $schema")
            if not d.get("$id") or not d.get("title"):
                bad.append(fn + " $id/title")
            if d.get("type") != "object" or d.get("additionalProperties") is not False:
                bad.append(fn + " root object/additionalProperties")
            prod = d.get("x-astrocs-production")
            if not prod or prod.get("task") != "SCHEMA-INTEGRATE-001" or prod.get("wave") != 6:
                bad.append(fn + " x-astrocs-production")
        self._ck("O01-schema-meta", not bad, "; ".join(bad))

    def _o02_family(self):
        have = {p.name for p in (self.root / "contracts/schemas/v6").glob("*.schema.json")}
        have |= {k.split("/")[-1] for k in self.overrides if k.startswith("contracts/schemas/v6/")}
        d = self.dictionary()
        idx = {e["path"].split("/")[-1] for e in d["schema_index"]}
        self._ck("O02-family", have == set(SCHEMA_FILES) and idx == set(SCHEMA_FILES),
                 "have=%s idx=%s" % (sorted(have), sorted(idx)))

    def _o03_units(self):
        fr = self.freeze()
        d = self.dictionary()
        got = [{k: v for k, v in r.items() if k != "clause_id"} for r in d["frozen_units_table"]]
        self._ck("O03-frozen-units-table", got == fr["units_table"], "dict.frozen_units_table != freeze.units_table")
        design = {r["symbol"]: r for r in d["design_units_table"]}
        self._ck("O04-design-units-extra",
                 design.get("sb_variance_out", {}).get("unit") == "ADU^2/px^4"
                 and design.get("sb_ivar_out", {}).get("unit") == "px^4/ADU^2"
                 and design.get("sb_variance_out", {}).get("clause_id") == "FZ-UNIT-VAR-SB"
                 and design.get("sb_ivar_out", {}).get("clause_id") == "FZ-UNIT-IVAR-SB",
                 "missing separated sb_variance_out/sb_ivar_out naming")
        q = d["quadratic_law"]
        self._ck("O04b-quadratic", q["freeze_id"] == "FZ-P3-BUNIT-QUADRATIC"
                 and "variance = signal^2" in q["statement"], "quadratic law")
        b = d["bunit_semantics"]
        self._ck("O04c-bunit-rule", b["freeze_id"] == "FZ-BUNIT-SEMANTICS"
                 and "pixel_area_power=-2" in b["rule"]
                 and b["pixel_area_power_defaults"]["signal_sb"] == -2
                 and b["pixel_area_power_defaults"]["sb_variance_out"] == -4, "bunit rule")

    def _o05_modes(self):
        fr = self.freeze()["weight_modes"]
        wm = self.dictionary()["weight_modes"]
        ok = (wm["production"] == fr["production"] and wm["documented_baseline"] == fr["documented_baseline"]
              and wm["deferred"] == fr["deferred"] and wm["legacy_superseded"] == fr["legacy_superseded"]
              and wm["legacy_integer_allowed"] == fr["legacy_integer_allowed"])
        self._ck("O05-mode-sets", ok, "mode sets != freeze")
        frd = {m["mode"]: m for m in fr["mode_details"]}
        bad = []
        for m in wm["mode_details"]:
            f = frd.get(m["mode"])
            if not f:
                bad.append(m["mode"] + " unknown"); continue
            for k in ("units", "group_normalized", "covariance_source", "effective_psf_required",
                      "authoritative_formula", "status"):
                if m.get(k) != f.get(k):
                    bad.append("%s.%s %r != %r" % (m["mode"], k, m.get(k), f.get(k)))
        self._ck("O06-mode-details", not bad, "; ".join(bad))

    def _o07_forbidden(self):
        fr = self.freeze()["forbidden"]
        f = self.dictionary()["forbidden"]
        self._ck("O07-weight-source-tokens", set(f["weight_source_tokens"]) == set(fr["weight_source_tokens"]),
                 "weight_source_tokens != freeze")
        self._ck("O08-psfsw-forbidden-keys", set(f["psfsw_forbidden_keys"]) == set(fr["psfsw_forbidden_keys"]),
                 "psfsw_forbidden_keys != freeze")
        self._ck("O08b-guard-keys", set(f["psfsw_extended_guard_keys"]) == set(fr["psfsw_extended_guard_keys"]),
                 "extended guard keys != freeze")
        self._ck("O09-prod-forbidden-values",
                 f["production_weight_mode_forbidden_values"] == fr["production_weight_mode_forbidden_values"],
                 "production forbidden values != freeze")
        self._ck("O10-declared-sources", self.dictionary()["declared_weight_sources"] == self.freeze()["declared_weight_sources"],
                 "declared_weight_sources != freeze")

    def _o11_provenance(self):
        s = self.schema("astrocs.v6.provenance.v1.schema.json")
        req = set(s["required"])
        tops = {p["path"].split(".")[0] for p in self.dictionary()["provenance_minimal_set"]}
        missing = sorted(tops - req)
        self._ck("O11-prov-required", not missing, "provenance required missing: %s" % missing)
        kc = self._prop(s, "k_corr")
        self._ck("O12-kcorr", {"definition", "domain", "value", "calibration"} <= set(kc["required"]),
                 "k_corr required subset")
        val = kc["properties"]["value"]
        self._ck("O12b-kcorr-neq1", jm.is_valid(0.9, val) and not jm.is_valid(1.0, val),
                 "k_corr.value must accept !=1 and reject 1")
        ep = self.schema("astrocs.v6.effective-psf.v1.schema.json")
        self._ck("O13-epsf-required",
                 {"effective_psf_id", "definition", "normalization", "kernel_transfer",
                  "values_or_model", "combination_coefficients_ref"} <= set(ep["required"]),
                 "effective-psf required subset")

    def _o14_covariance(self):
        s = self.schema("astrocs.v6.covariance.v1.schema.json")
        self._ck("O14-propagation", self._const_at(s, "propagation") == "C_out = R C_in R^T",
                 "propagation const")
        vf = set(self._prop(s, "variance_from")["enum"])
        bad = vf & set(self.dictionary()["forbidden"]["weight_source_tokens"])
        self._ck("O14b-variance-from", not bad, "variance_from contains forbidden: %s" % bad)
        self._ck("O15-diagonal", self._const_at(s, "diagonal_approximation", "is_lower_bound") is True
                 and self._const_at(s, "diagonal_approximation", "use_for_aperture") is False,
                 "diagonal_approximation consts")

    def _o16_psfsw(self):
        s = self.schema("astrocs.v6.psfsw.v1.schema.json")
        bad = []
        if self._const_at(s, "weight", "kind") != "psfsw_robust_weight":
            bad.append("weight.kind")
        if self._const_at(s, "weight", "units") != "1":
            bad.append("weight.units")
        if self._const_at(s, "weight", "group_normalized") is not True:
            bad.append("weight.group_normalized")
        if self._const_at(s, "weight", "normalization", "scope") != "group":
            bad.append("normalization.scope")
        if self._const_at(s, "weight", "normalization", "median_target") != 1.0:
            bad.append("median_target")
        if "component_flux_unit" not in s["required"]:
            bad.append("component_flux_unit not required")
        conc = s["properties"]["components"]["properties"]["concentration"]
        pat = conc["allOf"][1]["properties"]["units"]["pattern"]
        if not jm.is_valid("ADU/px^2", {"pattern": pat}) or jm.is_valid("ADU/px", {"pattern": pat}):
            bad.append("concentration pattern")
        reason = set(self._prop(s, "validity", "reason")["enum"]) - {None}
        if reason != VALIDITY_REASONS:
            bad.append("validity reason enum %s" % sorted(reason))
        pn = set(s.get("propertyNames", {}).get("not", {}).get("enum", []))
        if pn != set(self.freeze()["forbidden"]["psfsw_forbidden_keys"]):
            bad.append("propertyNames guard")
        self._ck("O16-psfsw", not bad, "; ".join(bad))

    def _o17_point_information(self):
        s = self.schema("astrocs.v6.point-information.v1.schema.json")
        bad = []
        for name, unit in (("W_info", "ADU^-2"), ("Q", "ADU^-1"), ("flux", "ADU")):
            anch = s["properties"][name].get("x-astrocs", {}).get("units")
            if anch != unit:
                bad.append("%s anchor units %r" % (name, anch))
        self._ck("O17-winfo-units", not bad, "; ".join(bad))

    def _o18_weight_mode(self):
        s = self.schema("astrocs.v6.weight-mode.v1.schema.json")
        enum = set(self._prop(s, "weight_mode")["enum"])
        fr = self.freeze()["weight_modes"]
        self._ck("O18-mode-enum", enum == set(fr["production"]) | set(fr["documented_baseline"]),
                 "weight_mode enum %s" % sorted(enum))
        self._ck("O18b-deferred", self._const_at(s, "deferred_modes_documented") == ["psf_snr_power"],
                 "deferred_modes_documented")
        self._ck("O18c-legacy", self._const_at(s, "legacy_integer_allowed") is False
                 and self._const_at(s, "legacy_superseded") == fr["legacy_superseded"],
                 "legacy_integer_allowed/legacy_superseded")
        self._ck("O18d-no-deferred", "psf_snr_power" not in enum, "psf_snr_power in production enum")

    def _o19_freeze_id_coverage(self):
        fr = self.freeze()
        need = fr["required_freeze_ids"]
        seen = set()
        for fn in SCHEMA_FILES:
            seen |= set(self.schema(fn)["x-astrocs-production"]["clause_ids"])
        d = self.dictionary()
        seen |= {c["clause_id"] for c in d["fail_closed"]}
        seen |= {p["clause_id"] for p in d["provenance_minimal_set"]}
        seen |= set(d["clause_registry"]["ids_by_status"].get("FROZEN", []))
        missing = [x for x in need if x not in seen]
        self._ck("O19-required-freeze-ids", not missing, "uncovered required_freeze_ids: %s" % missing)

    def _o20_clause_registry(self):
        fr = self.freeze()
        reg = self.dictionary()["clause_registry"]
        bad = []
        if reg["total"] != len(fr["clauses"]):
            bad.append("total")
        if reg["counts_by_status"] != {"FROZEN": 39, "PENDING_OWNER_SIGNOFF": 49, "OPEN": 8}:
            bad.append("counts %s" % reg["counts_by_status"])
        for st, key in (("FROZEN", "frozen"), ("PENDING_OWNER_SIGNOFF", "pending_owner_signoff"),
                        ("OPEN", "open")):
            if len(reg["ids_by_status"].get(st, [])) != fr["counts"][key]:
                bad.append(st)
        for it in reg["signoff_items"]:
            if it["status"] != "PENDING_OWNER_SIGNOFF":
                bad.append("signoff %s written as %s" % (it["id"], it["status"]))
        if set(reg["ids_by_status"]["PENDING_OWNER_SIGNOFF"]) & set(reg["ids_by_status"]["FROZEN"]):
            bad.append("pending∩frozen")
        if "fail-closed" not in reg["policy"]:
            bad.append("policy")
        self._ck("O20-clause-registry", not bad, "; ".join(bad))

    def _o21_open_registry(self):
        reg = self.dictionary()["open_registry"]
        have = set(reg["design_open_items"])
        missing = [x for x in ["DI-01", "DI-06", "DI-07", "OI-01", "PF-01", "PF-07",
                               "AR-032-GAP", "AR-036-SIGNOFF", "P3-OPEN-EPSILON-CORR"] if x not in have]
        clause_open = set(self.dictionary()["clause_registry"]["ids_by_status"]["OPEN"])
        self._ck("O21-open-registry", not missing and len(clause_open) == self.freeze()["counts"]["open"]
                 and "DI-01" in reg["w6_closed"] and "DI-07" in reg["w6_closed"],
                 "missing=%s" % missing)

    def _o22_vocabulary(self):
        v = self.j(VOCAB_REL)
        bad = []
        fields = {f["field"] for f in v["canonical_fields"]}
        for want in ("weight.kind", "weight.units", "weight.group_normalized",
                     "weight.normalization.scope", "weight.normalization.median_target"):
            if want not in fields:
                bad.append("missing " + want)
        if len(v["dual_mapping"]) != 4:
            bad.append("dual_mapping len")
        for row in v["dual_mapping"]:
            if row["canonical"] not in fields:
                bad.append("mapping target " + row["canonical"])
        kg = next(f for f in v["canonical_fields"] if f["field"] == "weight.kind")
        ug = next(f for f in v["canonical_fields"] if f["field"] == "weight.units")
        if kg["canonical_psfsw_value"] != "psfsw_robust_weight" or kg["legacy_aliases"][0]["token"] != "relative_dimensionless":
            bad.append("kind mapping")
        if ug["canonical_psfsw_value"] != "1" or ug["legacy_aliases"][0]["token"] != "dimensionless_relative":
            bad.append("units mapping DI-07")
        dis = v["legacy_integer"]["disposition"]
        if dis["0"]["action"] != "REJECT" or dis["1"]["target"] != "equal" or dis["2"]["target"] != "pixel_ivar":
            bad.append("legacy integer disposition")
        if v["legacy_integer"]["legacy_integer_allowed"] is not False:
            bad.append("legacy_integer_allowed")
        self._ck("O22-vocabulary", not bad, "; ".join(bad))

    def _o23_migration(self):
        m = self.j(MIG_REL)
        bad = []
        n_schema_mig = len([e for e in m["file_migrations"] if e["kind"] == "schema"])
        if n_schema_mig != 10:
            bad.append("file_migrations schema count %d" % n_schema_mig)
        covered = set(m.get("catalog_migrations_covered", []))
        for r in m["record_migrations"]:
            covered |= set(r.get("catalog_refs", []))
            covered.add(r["id"])
        missing = [x for x in CATALOG_MIGRATION_IDS if x not in covered]
        if missing:
            bad.append("catalog migrations uncovered: %s" % missing)
        lw = m["legacy_weight_mode_disposition"]
        if lw["0"]["action"] != "REJECT" or lw["1"]["target"] != "equal" or lw["2"]["target"] != "pixel_ivar":
            bad.append("legacy disposition")
        if lw["psf_snr_power"]["action"] != "REJECT":
            bad.append("psf_snr_power must REJECT in migration")
        self._ck("O23-migration-map", not bad, "; ".join(bad))

    def _o24_concentration(self):
        d = self.dictionary()["concentration_unit_authority"]
        s = self.schema("astrocs.v6.psfsw.v1.schema.json")
        pat = s["properties"]["components"]["properties"]["concentration"]["allOf"][1]["properties"]["units"]["pattern"]
        ex = self.j("contracts/data/examples/v6/psfsw.example.json")
        ok = (d["authoritative_unit"] == "component_flux_unit/px^2"
              and d["a_nea_unit"] == "px^2"
              and d["registered_text_error"]["as_written"] == "ADU/px"
              and d["registered_text_error"]["not_silently_adopted"] is True
              and not jm.is_valid("ADU/px", {"pattern": pat})
              and jm.is_valid(ex["components"]["concentration"]["units"], {"pattern": pat})
              and ex["components"]["concentration"]["units"] == "ADU/px^2"
              and ex.get("component_flux_unit") == "ADU")
        self._ck("O24-concentration-authority", ok, "concentration unit authority drifted")

    def _o25_docs_present(self):
        ds = self._read(DATA_SEM_REL)
        pa = self._read(PUB_API_REL)
        ig = self._read(INTEG_REL)
        # 2026-09-20：§31 标题按 GAP_AUDIT §4.1 Q2 裁决去「生产目标态」措辞（DOC-203 落地）
        need_ds = ["## 31. V6 合同层数据合同", "DATA-V6-SCHEMA", "FZ-BUNIT-SEMANTICS",
                   "ADU/px^2", "ADU^-2", "psfsw_robust_weight", "component_flux_unit/px^2",
                   "FZ-PROV-MINIMAL-SET", "FZ-MODE-DEFERRED", "FZ-FIELD-WEIGHTMODE",
                   "0=support×snr²", "fail-closed", "PENDING_OWNER_SIGNOFF"]
        need_pa = ["API-V6-WEIGHTMODE-001", "point_information", "surface_gls", "psfsw_robust",
                   "support×snr²", "legacy 整数", "canonical"]
        need_ig = ["DATA-V6-SCHEMA-INTEGRATION", "MIG-WEIGHTMODE-LEGACY", "DI-06", "SO-01"]
        miss = [("DATA_SEMANTICS", t) for t in need_ds if t not in ds]
        miss += [("PUBLIC_API", t) for t in need_pa if t not in pa]
        miss += [("INTEGRATION", t) for t in need_ig if t not in ig]
        self._ck("O25-docs-present", not miss, "missing doc markers: %s" % miss)

    def _o26_docs_guard(self):
        bad = []
        for rel in (DATA_SEM_REL, PUB_API_REL):
            txt = self._read(rel)
            for tok in FORBIDDEN_DOC_TOKENS:
                if tok in txt:
                    bad.append("%s contains reverted token %r" % (rel, tok))
        self._ck("O26-no-p33-p27-reintroduction", not bad, "; ".join(bad))

    def _o27_examples(self):
        bad = []
        kinds = {"psfsw.example.json": "psfsw", "weight-mode.example.json": "weight_mode",
                 "provenance.example.json": "provenance", "covariance.example.json": "covariance",
                 "signal.example.json": "signal", "effective-psf.example.json": "effective_psf"}
        for ex, fn in EXAMPLE_TARGET.items():
            doc = self.j("contracts/data/examples/v6/" + ex)
            errs = jm.validate(doc, self.schema(fn))
            if errs:
                bad.append("%s schema: %s" % (ex, errs[:2]))
            if ex in kinds:
                sem = self.semantic_gate_errors(kinds[ex], doc)
                if sem:
                    bad.append("%s semantic: %s" % (ex, sem[:2]))
        self._ck("O27-examples-positive", not bad, "; ".join(bad[:6]))

    def _o28_clause_ids_known(self):
        known = {c["id"] for c in self.freeze()["clauses"]}
        extra = set()
        for fn in SCHEMA_FILES:
            extra |= set(self.schema(fn)["x-astrocs-production"]["clause_ids"])
        unknown = sorted(extra - known)
        self._ck("O28-clause-ids-known", not unknown, "unknown clause ids: %s" % unknown)

    def _o29_no_alias_or_third_vocab(self):
        bad = []
        for fn in SCHEMA_FILES:
            txt = self._read("contracts/schemas/v6/" + fn)
            for a in ALIAS_FIELDS:
                if a in txt:
                    bad.append("%s still has %s" % (fn, a))
        third = self.j(VOCAB_REL)["forbidden_third_vocabulary_tokens"]
        for fn in SCHEMA_FILES:
            txt = self._read("contracts/schemas/v6/" + fn)
            for t in third:
                if '"' + t + '"' in txt:
                    bad.append("%s has third-vocab token %s" % (fn, t))
        self._ck("O29-single-vocabulary", not bad, "; ".join(bad))

    def _o31_guard_liveness(self):
        """门必须能红：已知冻结违例必须被 schema 或语义门拒绝（正向控制 = 合法记录被接受）。"""
        ps = self.schema("astrocs.v6.psfsw.v1.schema.json")
        wms = self.schema("astrocs.v6.weight-mode.v1.schema.json")
        pv = self.schema("astrocs.v6.provenance.v1.schema.json")
        cov = self.schema("astrocs.v6.covariance.v1.schema.json")
        sig = self.schema("astrocs.v6.signal.v1.schema.json")
        base_ps = self.j("contracts/data/examples/v6/psfsw.example.json")
        base_wm = self.j("contracts/data/examples/v6/weight-mode.example.json")
        base_pv = self.j("contracts/data/examples/v6/provenance.example.json")
        base_cov = self.j("contracts/data/examples/v6/covariance.example.json")
        base_sig = self.j("contracts/data/examples/v6/signal.example.json")

        def clone(d):
            return json.loads(json.dumps(d))

        fixtures = []  # (name, kind, schema, doc)
        m = clone(base_ps); m["weight"]["units"] = "flux^-2"
        fixtures.append(("psfsw-units-ivar", "psfsw", ps, m))
        m = clone(base_ps); m["weight"]["group_normalized"] = False
        fixtures.append(("psfsw-group-false", "psfsw", ps, m))
        m = clone(base_ps); m["components"]["concentration"]["units"] = "ADU/px"
        fixtures.append(("concentration-ADU-px", "psfsw", ps, m))
        m = clone(base_ps); m["components"]["concentration"]["measurement_id"] = "psfsw.signal"
        fixtures.append(("measurement-collapse", "psfsw", ps, m))
        m = clone(base_ps); m["weight"]["variance"] = 0.1
        fixtures.append(("psfsw-inject-variance", "psfsw", ps, m))
        m = clone(base_ps); m["validity"] = {"valid": False, "reason": "median_snr_fallback"}
        fixtures.append(("validity-reason-out-of-whitelist", "psfsw", ps, m))
        m = clone(base_ps); m["validity"] = {"valid": False, "reason": "no_common_star_set"}; m["weight"]["weight_value"] = 1.0
        fixtures.append(("invalid-but-weight-not-null", "psfsw", ps, m))
        m = clone(base_ps); m["required_extra_check"] = True
        fixtures.append(("psfsw-additional-prop", "psfsw", ps, m))
        m = clone(base_wm); m["weight_mode"] = "psf_snr_power"
        fixtures.append(("psf-snr-power-in-production", "weight_mode", wms, m))
        m = clone(base_wm); m["weight_mode"] = 0
        fixtures.append(("legacy-0", "weight_mode", wms, m))
        m = clone(base_wm); m["weight"]["sources"] = ["median_source_snr"]
        fixtures.append(("diagnostic-source", "weight_mode", wms, m))
        m = clone(base_wm); m["mode_class"] = "production"; m["weight_mode"] = "equal"
        fixtures.append(("baseline-as-production", "weight_mode", wms, m))
        m = clone(base_pv); m["k_corr"]["value"] = 1
        fixtures.append(("kcorr-eq-1", "provenance", pv, m))
        m = clone(base_pv); m["units"] = {"bunit": "ADU", "pixel_semantics": "integrated_flux",
                                          "pixel_area_power": 0, "target_pixel_area": 1.0}
        fixtures.append(("bunit-unjudgeable", "provenance", pv, m))
        m = clone(base_pv); del m["flux_conservation_factor"]
        fixtures.append(("provenance-missing-key", "provenance", pv, m))
        m = clone(base_pv); m["units"]["bunit"] = "ADU/px^2"; m["units"]["pixel_area_power"] = 0
        fixtures.append(("bunit-power-mismatch", "provenance", pv, m))
        m = clone(base_cov); m["representation"] = "diagonal_variance"; del m["correlation_kernel"]
        fixtures.append(("diagonal-without-kernel", "covariance", cov, m))
        m = clone(base_cov); m["variance_from"] = "psfsw_robust_weight"
        fixtures.append(("covariance-from-weight", "covariance", cov, m))
        m = clone(base_sig); m["pixel_semantics"] = "surface_brightness"; m["pixel_area_power"] = -4
        fixtures.append(("signal-pixel-area-power", "signal", sig, m))
        m = clone(base_ps); m["components"]["concentration"]["spatial_summary"]["p05"] = 99.0
        fixtures.append(("p05-gt-p50", "psfsw", ps, m))

        not_rejected = []
        for name, kind, sch, doc in fixtures:
            if jm.is_valid(doc, sch) and not self.semantic_gate_errors(kind, doc):
                not_rejected.append(name)
        # 正向控制：合法生产正例必须全部 ACCEPT（否则门恒真自证）
        pos_bad = []
        for doc, sch, kind in ((base_ps, ps, "psfsw"), (base_wm, wms, "weight_mode"),
                               (base_pv, pv, "provenance"), (base_cov, cov, "covariance"),
                               (base_sig, sig, "signal")):
            if not jm.is_valid(doc, sch) or self.semantic_gate_errors(kind, doc):
                pos_bad.append(kind)
        self._ck("O31-guard-liveness", not not_rejected and not pos_bad,
                 "not_rejected=%s positive_control_broken=%s" % (not_rejected, pos_bad))

    def _o30_structural_guards(self):
        """结构门单独可用：必须被 JSON Schema 本体（不经语义层）拒绝的已知违例。"""
        specs = [
            ("psfsw", "astrocs.v6.psfsw.v1.schema.json", "psfsw.example.json",
             [("weight.units", "flux^-2"), ("weight.group_normalized", False),
              ("weight.normalization.scope", "global"), ("weight.normalization.median_target", 2.0),
              ("components.concentration.units", "ADU/px")]),
            ("weight-mode", "astrocs.v6.weight-mode.v1.schema.json", "weight-mode.example.json",
             [("weight_mode", "psf_snr_power"), ("weight_mode", 0),
              ("legacy_integer_allowed", True), ("weight_mode", "equal")]),
            ("provenance", "astrocs.v6.provenance.v1.schema.json", "provenance.example.json",
             [("k_corr.value", 1)]),
            ("covariance", "astrocs.v6.covariance.v1.schema.json", "covariance.example.json",
             [("variance_from", "psfsw_robust_weight"), ("propagation", "Var = 1/W")]),
            ("signal", "astrocs.v6.signal.v1.schema.json", "signal.example.json", []),
        ]

        def set_path(doc, dotted, value):
            cur = doc
            parts = dotted.split(".")
            for p in parts[:-1]:
                cur = cur[p]
            cur[parts[-1]] = value

        def clone(d):
            return json.loads(json.dumps(d))

        not_rejected = []
        for tag, fn, ex, muts in specs:
            sch = self.schema(fn)
            base = self.j("contracts/data/examples/v6/" + ex)
            for dotted, val in muts:
                m = clone(base)
                set_path(m, dotted, val)
                if jm.is_valid(m, sch):
                    not_rejected.append("%s:%s=%r" % (tag, dotted, val))
        # weight.units=ADU 需同时改 pixel_area_power 才触发 signal 结构门（单独改 units 合法）
        m = clone(self.j("contracts/data/examples/v6/signal.example.json"))
        m["pixel_semantics"] = "surface_brightness"; m["pixel_area_power"] = -4
        if jm.is_valid(m, self.schema("astrocs.v6.signal.v1.schema.json")):
            not_rejected.append("signal:pixel_area_power mismatch")
        # 缺键同样必须结构拒绝
        m = clone(self.j("contracts/data/examples/v6/provenance.example.json"))
        del m["k_corr"]
        if jm.is_valid(m, self.schema("astrocs.v6.provenance.v1.schema.json")):
            not_rejected.append("provenance:missing k_corr")
        m = clone(self.j("contracts/data/examples/v6/effective-psf.example.json"))
        del m["effective_psf_id"]
        if jm.is_valid(m, self.schema("astrocs.v6.effective-psf.v1.schema.json")):
            not_rejected.append("effective-psf:missing id")
        m = clone(self.j("contracts/data/examples/v6/provenance.example.json"))
        m["units"] = {"bunit": "ADU", "pixel_semantics": "integrated_flux",
                      "pixel_area_power": 0, "target_pixel_area": 1.0}
        if jm.is_valid(m, self.schema("astrocs.v6.provenance.v1.schema.json")):
            not_rejected.append("provenance:bunit unjudgeable")
        m = clone(self.j("contracts/data/examples/v6/covariance.example.json"))
        m["representation"] = "diagonal_variance"; del m["correlation_kernel"]
        if jm.is_valid(m, self.schema("astrocs.v6.covariance.v1.schema.json")):
            not_rejected.append("covariance:diagonal without kernel")
        self._ck("O30-structural-guards", not not_rejected,
                 "schema failed to reject: %s" % not_rejected)

    def _o32_pending_structural_refs(self):
        """结构门里编码的 PENDING 数值必须显式标注待签，不得冒充已冻结。"""
        status = {c["id"]: c["status"] for c in self.freeze()["clauses"]}
        bad = []
        for fn in SCHEMA_FILES:
            d = self.schema(fn)
            for cid in d["x-astrocs-production"].get("pending_owner_signoff_structural_refs", []):
                if status.get(cid) != "PENDING_OWNER_SIGNOFF":
                    bad.append("%s: %s not PENDING in freeze" % (fn, cid))
        n_common = self.schema("astrocs.v6.psfsw.v1.schema.json")["properties"]["common_star_set"]["properties"]["n_common"]
        if "PENDING_OWNER_SIGNOFF" not in (n_common.get("x-astrocs", {}).get("signoff") or ""):
            bad.append("psfsw n_common not annotated pending")
        kcv = self.schema("astrocs.v6.provenance.v1.schema.json")["properties"]["k_corr"]["properties"]["value"]
        if "PENDING_OWNER_SIGNOFF" not in (kcv.get("x-astrocs", {}).get("signoff") or ""):
            bad.append("provenance k_corr.value not annotated pending")
        self._ck("O32-pending-structural-refs", not bad, "; ".join(bad))

    def report(self):
        return {
            "checks_total": len(self.checks),
            "checks_passed": sum(1 for c in self.checks if c["ok"]),
            "checks_failed": sum(1 for c in self.checks if not c["ok"]),
            "failures": self.failures,
            "checks": self.checks,
        }


def main():
    root = pathlib.Path(__file__).resolve().parents[3]
    o = Oracle(root)
    _, failures = o.run()
    rep = o.report()
    print(json.dumps(rep, ensure_ascii=False, indent=2))
    if failures:
        print("ORACLE_FAIL (%d)" % len(failures))
        return 1
    print("ORACLE_PASS %d/%d" % (rep["checks_passed"], rep["checks_total"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
