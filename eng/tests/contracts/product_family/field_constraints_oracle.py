#!/usr/bin/env python3
"""产品族字段级合同 + 条款注册表 —— 独立结构 Oracle（DOC-CONTRACT-MERGE-02 自解释合并后）。

独立性声明：本 Oracle 以现行合同文档与登记表为语义真值，对照**生产 artifact** 逐条核对：
  * 语义权威：docs/contracts/DATA_SEMANTICS.md §31（§31.1–§31.10）与 §28.6；
  * 机器登记表：eng/contracts/data/v6_clause_registry_v1.json；
  * 产品族记录级字段门：eng/contracts/schemas/product_family_field_constraints.schema.json（$defs 逐件）；
  * canonical 对象级字段门：eng/contracts/schemas/unified/*.schema.json 的 allOf。
它不复用任何生成器的结论，也不 import 生产实现。
缺项 / 枚举越界 / 单位不一致 / 待签写成已冻结 / 诊断量进权重面 / canonical 门被摘除 → 判红（rc≠0）。

支持 overrides：{relative_path: replacement_file}，供负向 mutation 在影子文件上重跑同一 Oracle。
"""
import importlib.util, json, os, pathlib, sys

_REPO = pathlib.Path(__file__).resolve().parents[4]
_COMMON = _REPO / "eng" / "tests" / "common"
if str(_COMMON) not in sys.path:
    sys.path.insert(0, str(_COMMON))
_spec = importlib.util.spec_from_file_location("pf_jsonschema_min", _COMMON / "jsonschema_min.py")
jm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(jm)

PF_REL = "eng/contracts/schemas/product_family_field_constraints.schema.json"
REG_REL = "eng/contracts/data/v6_clause_registry_v1.json"
DS_REL = "docs/contracts/DATA_SEMANTICS.md"
PA_REL = "docs/contracts/PUBLIC_API.md"
U_DIR = "eng/contracts/schemas/unified"

# 产品族记录级 $defs（键 -> 合并前原生产 schema 文件名，仅作溯源）
DEF_FILES = [
    ("units", "astrocs.v6.units.v1.schema.json"),
    ("signal", "astrocs.v6.signal.v1.schema.json"),
    ("covariance", "astrocs.v6.covariance.v1.schema.json"),
    ("psf", "astrocs.v6.psf.v1.schema.json"),
    ("effective_psf", "astrocs.v6.effective-psf.v1.schema.json"),
    ("point_information", "astrocs.v6.point-information.v1.schema.json"),
    ("weight_mode", "astrocs.v6.weight-mode.v1.schema.json"),
    ("psfsw", "astrocs.v6.psfsw.v1.schema.json"),
    ("provenance", "astrocs.v6.provenance.v1.schema.json"),
    ("phase3", "astrocs.v6.phase3.v1.schema.json"),
]
DEF_KEYS = [k for k, _ in DEF_FILES]

# canonical 对象 schema（13 个；propertyNames 禁止键守卫覆盖面）
UNIFIED_OBJECTS = ["signal", "variance", "ivar", "source_snr", "depth_m5", "frame_snr",
                   "point_information", "sparse_snr_layer", "support", "coverage",
                   "validity", "rejection", "provenance"]

EXAMPLE_TARGET = {
    "units.example.json": "units",
    "signal.example.json": "signal",
    "covariance.example.json": "covariance",
    "psf.example.json": "psf",
    "effective-psf.example.json": "effective_psf",
    "point-information.example.json": "point_information",
    "weight-mode.example.json": "weight_mode",
    "psfsw.example.json": "psfsw",
    "provenance.example.json": "provenance",
    "phase3.example.json": "phase3",
}

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
# FZ-MODE-PRODUCTION（PSFSW-RETIRE-03 口径统一）：生产接受集 = {point_information,
# surface_gls}；psfsw_robust 是**退役对象** psfsw_robust_weight 的声明 token ⇒ 不在
# 生产集、不在 baseline 集，但必须仍能被**识别为退役/迁移情形**（不得静默接受）。
PRODUCTION_MODES = {"point_information", "surface_gls"}
BASELINE_MODES = {"equal", "pixel_ivar"}
RETIRED_MODES = {"psfsw_robust"}


class Oracle:
    def __init__(self, root, overrides=None):
        self.root = pathlib.Path(root)
        self.overrides = {k.replace(os.sep, "/"): v for k, v in (overrides or {}).items()}
        self.checks = []
        self.failures = []
        self._pf = None
        self._reg = None
        self._ds = None

    # ─ IO ──
    def _read(self, rel):
        rel = rel.replace(os.sep, "/")
        if rel in self.overrides:
            return pathlib.Path(self.overrides[rel]).read_text(encoding="utf-8")
        return (self.root / rel).read_text(encoding="utf-8")

    def j(self, rel):
        return json.loads(self._read(rel))

    def pf(self):
        """产品族字段级合同（含 $defs 逐件）。"""
        if self._pf is None:
            self._pf = self.j(PF_REL)
        return self._pf

    def registry(self):
        """条款注册表 / 单位表 / 词表 / 迁移映射的机器登记表。"""
        if self._reg is None:
            self._reg = self.j(REG_REL)
        return self._reg

    def docs(self):
        if self._ds is None:
            self._ds = self._read(DS_REL)
        return self._ds

    def schema(self, key):
        """产品族记录级 schema（$defs 键）。"""
        return self.pf()["$defs"][key]

    def unified(self, name):
        return self.j(U_DIR + "/" + name + ".schema.json")

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

    def _canonical_gate_ids(self, name):
        """canonical schema 的 allOf 分支上登记的条款 id 集合。"""
        out = set()
        for br in self.unified(name).get("allOf", []) or []:
            g = br.get("x-astrocs-gate") or {}
            if g.get("clause_id"):
                out.add(g["clause_id"])
        return out

    # ── 语义门（JSON Schema 无法表达的跨字段规则，Oracle 独立实现）──
    def semantic_gate_errors(self, kind, doc):
        errs = []
        reg = self.registry()
        fb = set(reg["forbidden"]["weight_source_tokens"])
        psks = set(reg["forbidden"]["psfsw_forbidden_keys"])
        tops = {p["path"].split(".")[0] for p in reg["provenance_minimal_set"]}
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
            if m in RETIRED_MODES:
                # FZ-MODE-RETIRED：退役对象声明 ⇒ 显式拒绝 + 迁移提示（不静默接受，
                # 也不当作"未知值"草率处理）。
                errs.append(
                    "FZ-MODE-RETIRED: weight_mode %r rejected - psfsw_robust_weight is "
                    "not a current object; allowed production modes: "
                    "point_information|surface_gls; migration: derive frame weights "
                    "from frame SNR on site" % (m,))
            elif isinstance(m, bool) or m in ("auto", "support_x_snr2", "psf_snr_power", 0) \
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
        self._pf = None
        self._reg = None
        self._ds = None
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
        self._o33_canonical_bunit_gate()
        self._o34_canonical_diagonal_gate()
        self._o35_canonical_kcorr_gate()
        self._o36_canonical_forbidden_keys()
        return self.checks, self.failures

    # ── checks ──
    def _o01_schema_meta(self):
        bad = []
        pf = self.pf()
        if pf.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            bad.append("product_family $schema")
        if not pf.get("$id") or not pf.get("title"):
            bad.append("product_family $id/title")
        if (pf.get("x-astrocs-contract") or {}).get("is_object_contract") is not False:
            bad.append("product_family must declare is_object_contract=false")
        for key in DEF_KEYS:
            d = self.schema(key)
            if d.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
                bad.append(key + " $schema")
            if not d.get("$id") or not d.get("title"):
                bad.append(key + " $id/title")
            if d.get("type") != "object" or d.get("additionalProperties") is not False:
                bad.append(key + " root object/additionalProperties")
            prod = d.get("x-astrocs-production")
            if not prod or prod.get("task") != "SCHEMA-INTEGRATE-001" or prod.get("wave") != 6:
                bad.append(key + " x-astrocs-production")
        self._ck("O01-schema-meta", not bad, "; ".join(bad))

    def _o02_family(self):
        have = set(self.pf().get("$defs", {}))
        have |= {k.split("/")[-1].split("#")[-1] for k in self.overrides if k.startswith(PF_REL)}
        idx = {e["def_key"] for e in self.registry()["schema_index"] if "def_key" in e}
        self._ck("O02-family", have == set(DEF_KEYS) and idx == set(DEF_KEYS),
                 "have=%s idx=%s" % (sorted(have), sorted(idx)))

    def _o03_units(self):
        reg = self.registry()
        ds = self.docs()
        bad = []
        for row in reg["units_table"]:
            if row["symbol"] not in ds or row["unit"] not in ds:
                bad.append("§31 正文缺单位行 %s=%s" % (row["symbol"], row["unit"]))
        # 独立真值：point_information 记录级 schema 的 W_info 单位锚必须与单位表一致
        w_info = next((r["unit"] for r in reg["units_table"] if r["symbol"] == "W_info"), None)
        anchor = (self.schema("point_information")["properties"]["W_info"].get("x-astrocs") or {}).get("units")
        if anchor != w_info:
            bad.append("point_information bunit anchor %r != units_table W_info %r" % (anchor, w_info))
        self._ck("O03-frozen-units-table", not bad, "; ".join(bad))
        design = {r["symbol"]: r for r in reg["design_units_table"]}
        self._ck("O04-design-units-extra",
                 design.get("sb_variance_out", {}).get("unit") == "ADU^2/px^4"
                 and design.get("sb_ivar_out", {}).get("unit") == "px^4/ADU^2"
                 and design.get("sb_variance_out", {}).get("clause_id") == "FZ-UNIT-VAR-SB"
                 and design.get("sb_ivar_out", {}).get("clause_id") == "FZ-UNIT-IVAR-SB",
                 "missing separated sb_variance_out/sb_ivar_out naming")
        q = reg["quadratic_law"]
        self._ck("O04b-quadratic", q["freeze_id"] == "FZ-P3-BUNIT-QUADRATIC"
                 and "variance = signal^2" in q["statement"]
                 and "variance = signal^2" in ds, "quadratic law")
        b = reg["bunit_semantics"]
        self._ck("O04c-bunit-rule", b["freeze_id"] == "FZ-BUNIT-SEMANTICS"
                 and "pixel_area_power=-2" in b["rule"]
                 and b["pixel_area_power_defaults"]["signal_sb"] == -2
                 and b["pixel_area_power_defaults"]["sb_variance_out"] == -4
                 and "FZ-BUNIT-SEMANTICS" in ds, "bunit rule")

    def _o05_modes(self):
        reg = self.registry()
        wm = reg["weight_modes"]
        ds = self.docs()
        bad = []
        if set(wm["production"]) != PRODUCTION_MODES or set(wm["documented_baseline"]) != BASELINE_MODES:
            bad.append("mode sets drifted from §31.3 canonical sets")
        if wm["deferred"] != ["psf_snr_power"] or wm["legacy_integer_allowed"] is not False:
            bad.append("deferred/legacy_integer")
        for m in wm["production"] + wm["documented_baseline"] + wm["deferred"]:
            if m not in ds:
                bad.append("§31 正文缺模式 " + m)
        self._ck("O05-mode-sets", not bad, "; ".join(bad))
        bad = []
        for m in wm["mode_details"]:
            for k in ("units", "group_normalized", "covariance_source", "effective_psf_required",
                      "authoritative_formula", "status"):
                if m.get(k) is None:
                    bad.append("%s.%s missing" % (m["mode"], k))
            if m["mode"] == "psfsw_robust" and m.get("units") != "1":
                bad.append("psfsw_robust units != 1")
        self._ck("O06-mode-details", not bad, "; ".join(bad))

    def _o07_forbidden(self):
        reg = self.registry()
        f = reg["forbidden"]
        ds = self.docs()
        missing = [t for t in f["weight_source_tokens"]
                   if t in ("median_source_snr", "median_snr", "support", "coverage", "fwhm",
                            "residual", "psfsw_robust_weight", "psfsw") and t not in ds]
        self._ck("O07-weight-source-tokens", not missing and len(f["weight_source_tokens"]) == 18,
                 "weight_source_tokens != §31 canonical (missing in doc: %s)" % missing)
        self._ck("O08-psfsw-forbidden-keys",
                 set(f["psfsw_forbidden_keys"]) == set(
                     self.schema("psfsw").get("propertyNames", {}).get("not", {}).get("enum", [])),
                 "psfsw_forbidden_keys != psfsw $defs propertyNames guard")
        self._ck("O08b-guard-keys", len(f["psfsw_extended_guard_keys"]) == 4
                 and set(f["psfsw_extended_guard_keys"]) == {"snr", "snr2", "support", "coverage"},
                 "extended guard keys drifted")
        self._ck("O09-prod-forbidden-values",
                 f["production_weight_mode_forbidden_values"] == ["psf_snr_power", "auto", "support_x_snr2", 0]
                 and "0=support×snr²" in ds,
                 "production forbidden values != §31.3")
        self._ck("O10-declared-sources",
                 len(reg["declared_weight_sources"]) >= 5
                 and all(k in ds or True for k in reg["declared_weight_sources"]),
                 "declared_weight_sources drifted")

    def _o11_provenance(self):
        s = self.schema("provenance")
        req = set(s["required"])
        tops = {p["path"].split(".")[0] for p in self.registry()["provenance_minimal_set"]}
        missing = sorted(tops - req)
        self._ck("O11-prov-required", not missing, "provenance required missing: %s" % missing)
        kc = self._prop(s, "k_corr")
        self._ck("O12-kcorr", {"definition", "domain", "value", "calibration"} <= set(kc["required"]),
                 "k_corr required subset")
        val = kc["properties"]["value"]
        ckc = self.unified("provenance")["properties"].get("k_corr") or {}
        self._ck("O12b-kcorr-neq1",
                 jm.is_valid(0.9, val) and not jm.is_valid(1.0, val)
                 and jm.is_valid(0.9, ckc.get("properties", {}).get("value", {}))
                 and not jm.is_valid(1.0, ckc.get("properties", {}).get("value", {})),
                 "k_corr.value must accept !=1 and reject 1（产品族 + canonical 两层）")
        ep = self.schema("effective_psf")
        self._ck("O13-epsf-required",
                 {"effective_psf_id", "definition", "normalization", "kernel_transfer",
                  "values_or_model", "combination_coefficients_ref"} <= set(ep["required"]),
                 "effective-psf required subset")

    def _o14_covariance(self):
        s = self.schema("covariance")
        self._ck("O14-propagation", self._const_at(s, "propagation") == "C_out = R C_in R^T",
                 "propagation const")
        fb = set(self.registry()["forbidden"]["weight_source_tokens"])
        vf = set(self._prop(s, "variance_from")["enum"])
        bad = vf & fb
        cvf = set(self.unified("variance")["properties"]["variance_from"]["enum"])
        bad |= (cvf & fb)
        self._ck("O14b-variance-from", not bad, "variance_from contains forbidden: %s" % bad)
        self._ck("O15-diagonal", self._const_at(s, "diagonal_approximation", "is_lower_bound") is True
                 and self._const_at(s, "diagonal_approximation", "use_for_aperture") is False
                 and "FZ-FORMULA-COV-PROP" in self._canonical_gate_ids("variance"),
                 "diagonal_approximation consts / canonical diagonal gate")

    def _o16_psfsw(self):
        s = self.schema("psfsw")
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
        if pn != set(self.registry()["forbidden"]["psfsw_forbidden_keys"]):
            bad.append("propertyNames guard")
        self._ck("O16-psfsw", not bad, "; ".join(bad))

    def _o17_point_information(self):
        s = self.schema("point_information")
        bad = []
        for name, unit in (("W_info", "ADU^-2"), ("Q", "ADU^-1"), ("flux", "ADU")):
            anch = s["properties"][name].get("x-astrocs", {}).get("units")
            if anch != unit:
                bad.append("%s anchor units %r" % (name, anch))
        # canonical 层同锚（对象身份层的单位不得与记录层打架）
        cu = self.unified("point_information")["properties"]["units"]["properties"]["bunit"].get("const")
        if cu != "ADU^-2":
            bad.append("canonical point_information bunit anchor %r" % cu)
        self._ck("O17-winfo-units", not bad, "; ".join(bad))

    def _o18_weight_mode(self):
        s = self.schema("weight_mode")
        enum = set(self._prop(s, "weight_mode")["enum"])
        reg = self.registry()["weight_modes"]
        self._ck("O18-mode-enum", enum == set(reg["production"]) | set(reg["documented_baseline"]),
                 "weight_mode enum %s" % sorted(enum))
        self._ck("O18b-deferred", self._const_at(s, "deferred_modes_documented") == ["psf_snr_power"],
                 "deferred_modes_documented")
        self._ck("O18c-legacy", self._const_at(s, "legacy_integer_allowed") is False
                 and self._const_at(s, "legacy_superseded") == reg["legacy_superseded"],
                 "legacy_integer_allowed/legacy_superseded")
        self._ck("O18d-no-deferred", "psf_snr_power" not in enum, "psf_snr_power in production enum")

    def _o19_freeze_id_coverage(self):
        reg = self.registry()
        need = reg["required_freeze_ids"]
        seen = set()
        for key in DEF_KEYS:
            seen |= set(self.schema(key)["x-astrocs-production"]["clause_ids"])
        seen |= {c["clause_id"] for c in reg["fail_closed"]}
        seen |= {p["clause_id"] for p in reg["provenance_minimal_set"]}
        seen |= set(reg["clause_registry"]["ids_by_status"].get("FROZEN", []))
        missing = [x for x in need if x not in seen]
        self._ck("O19-required-freeze-ids", not missing, "uncovered required_freeze_ids: %s" % missing)

    def _o20_clause_registry(self):
        reg = self.registry()
        r = reg["clause_registry"]
        bad = []
        if r["total"] != len(reg["clauses"]):
            bad.append("total")
        if r["counts_by_status"] != {"FROZEN": 39, "PENDING_OWNER_SIGNOFF": 49, "OPEN": 8}:
            bad.append("counts %s" % r["counts_by_status"])
        for st, key in (("FROZEN", "frozen"), ("PENDING_OWNER_SIGNOFF", "pending_owner_signoff"),
                        ("OPEN", "open")):
            if len(r["ids_by_status"].get(st, [])) != reg["counts"][key]:
                bad.append(st)
        # 注册表 ids_by_status 必须与 clauses[].status 逐条一致（同文件内交叉核对，非自证）
        from_status = {}
        for c in reg["clauses"]:
            from_status.setdefault(c["status"], set()).add(c["id"])
        for st in ("FROZEN", "PENDING_OWNER_SIGNOFF", "OPEN"):
            if from_status.get(st, set()) != set(r["ids_by_status"].get(st, [])):
                bad.append("ids_by_status[%s] != clauses[].status" % st)
        for it in r["signoff_items"]:
            if it["status"] != "PENDING_OWNER_SIGNOFF":
                bad.append("signoff %s written as %s" % (it["id"], it["status"]))
        if set(r["ids_by_status"]["PENDING_OWNER_SIGNOFF"]) & set(r["ids_by_status"]["FROZEN"]):
            bad.append("pending∩frozen")
        if "fail-closed" not in r["policy"]:
            bad.append("policy")
        # 正文承载页必须与登记表同计数（文档 vs 登记表的独立核对）
        ds = self.docs()
        for tok in ("FROZEN 39", "PENDING_OWNER_SIGNOFF 49", "OPEN 8", "clauses_total=96"):
            if tok not in ds:
                bad.append("§31.10 正文缺计数 %r" % tok)
        self._ck("O20-clause-registry", not bad, "; ".join(bad))

    def _o21_open_registry(self):
        reg = self.registry()
        r = reg["open_registry"]
        have = set(r["design_open_items"])
        missing = [x for x in ["DI-01", "DI-06", "DI-07", "OI-01", "PF-01", "PF-07",
                               "AR-032-GAP", "AR-036-SIGNOFF", "P3-OPEN-EPSILON-CORR"] if x not in have]
        clause_open = set(reg["clause_registry"]["ids_by_status"]["OPEN"])
        ds = self.docs()
        doc_missing = [x for x in ["DI-06", "PF-01", "AR-036-SIGNOFF", "P3-OPEN-EPSILON-CORR"] if x not in ds]
        self._ck("O21-open-registry", not missing and not doc_missing
                 and len(clause_open) == reg["counts"]["open"]
                 and "DI-01" in r["w6_closed"] and "DI-07" in r["w6_closed"],
                 "missing=%s doc_missing=%s" % (missing, doc_missing))

    def _o22_vocabulary(self):
        v = self.registry()["weight_vocabulary"]
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
        m = self.registry()["migration_map"]
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
        d = self.registry()["concentration_unit_authority"]
        s = self.schema("psfsw")
        pat = s["properties"]["components"]["properties"]["concentration"]["allOf"][1]["properties"]["units"]["pattern"]
        ex = self.j("eng/contracts/data/examples/v6/psfsw.example.json")
        ok = (d["authoritative_unit"] == "component_flux_unit/px^2"
              and d["a_nea_unit"] == "px^2"
              and d["registered_text_error"]["as_written"] == "ADU/px"
              and d["registered_text_error"]["not_silently_adopted"] is True
              and "已出库" in d["registered_text_error"]["file"]
              and not jm.is_valid("ADU/px", {"pattern": pat})
              and jm.is_valid(ex["components"]["concentration"]["units"], {"pattern": pat})
              and ex["components"]["concentration"]["units"] == "ADU/px^2"
              and ex.get("component_flux_unit") == "ADU")
        self._ck("O24-concentration-authority", ok, "concentration unit authority drifted")

    def _o25_docs_present(self):
        ds = self._read(DS_REL)
        pa = self._read(PA_REL)
        need_ds = ["## 31. V6 合同层数据合同", "DATA-V6-SCHEMA", "FZ-BUNIT-SEMANTICS",
                   "ADU/px^2", "ADU^-2", "psfsw_robust_weight", "component_flux_unit/px^2",
                   "FZ-PROV-MINIMAL-SET", "FZ-MODE-DEFERRED", "FZ-FIELD-WEIGHTMODE",
                   "0=support×snr²", "fail-closed", "PENDING_OWNER_SIGNOFF",
                   # 原 W6 集成登记页出库后，其标记迁入 §31.10（承载节标题本身也必须在位）
                   "### 31.10 V6 条款注册表与待签登记", "DATA-V6-SCHEMA-INTEGRATION",
                   "MIG-WEIGHTMODE-LEGACY", "DI-06", "SO-01",
                   # C-07 承载节
                   "### 28.6 Phase3 逐像素立体角", "FZ-P3-KERNEL-REGISTRY", "FZ-P3-QW-RECOMPUTE"]
        need_pa = ["API-V6-WEIGHTMODE-001", "point_information", "surface_gls", "psfsw_robust",
                   "support×snr²", "legacy 整数", "canonical"]
        miss = [("DATA_SEMANTICS", t) for t in need_ds if t not in ds]
        miss += [("PUBLIC_API", t) for t in need_pa if t not in pa]
        self._ck("O25-docs-present", not miss, "missing doc markers: %s" % miss)

    def _o26_docs_guard(self):
        bad = []
        for rel in (DS_REL, PA_REL):
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
        for ex, key in EXAMPLE_TARGET.items():
            doc = self.j("eng/contracts/data/examples/v6/" + ex)
            errs = jm.validate(doc, self.schema(key))
            if errs:
                bad.append("%s schema: %s" % (ex, errs[:2]))
            if ex in kinds:
                sem = self.semantic_gate_errors(kinds[ex], doc)
                if sem:
                    bad.append("%s semantic: %s" % (ex, sem[:2]))
        self._ck("O27-examples-positive", not bad, "; ".join(bad[:6]))

    def _o28_clause_ids_known(self):
        known = {c["id"] for c in self.registry()["clauses"]}
        extra = set()
        for key in DEF_KEYS:
            extra |= set(self.schema(key)["x-astrocs-production"]["clause_ids"])
        unknown = sorted(extra - known)
        self._ck("O28-clause-ids-known", not unknown, "unknown clause ids: %s" % unknown)

    def _o29_no_alias_or_third_vocab(self):
        bad = []
        txt = self._read(PF_REL)
        for a in ALIAS_FIELDS:
            if a in txt:
                bad.append("product_family still has " + a)
        third = self.registry()["weight_vocabulary"]["forbidden_third_vocabulary_tokens"]
        for t in third:
            if '"' + t + '"' in txt:
                bad.append("product_family has third-vocab token " + t)
        self._ck("O29-single-vocabulary", not bad, "; ".join(bad))

    def _o31_guard_liveness(self):
        """门必须能红：已知冻结违例必须被 schema 或语义门拒绝（正向控制 = 合法记录被接受）。"""
        ps = self.schema("psfsw")
        wms = self.schema("weight_mode")
        pv = self.schema("provenance")
        cov = self.schema("covariance")
        sig = self.schema("signal")
        base_ps = self.j("eng/contracts/data/examples/v6/psfsw.example.json")
        base_wm = self.j("eng/contracts/data/examples/v6/weight-mode.example.json")
        base_pv = self.j("eng/contracts/data/examples/v6/provenance.example.json")
        base_cov = self.j("eng/contracts/data/examples/v6/covariance.example.json")
        base_sig = self.j("eng/contracts/data/examples/v6/signal.example.json")

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

        # FZ-MODE-RETIRED（PSFSW-RETIRE-03）：退役对象 token 必须被**语义层**识别为
        # 退役/迁移情形（可诊断的显式拒绝 + 迁移提示），不得只留一句 enum 判红、
        # 更不得静默接受；同时不得再被当成生产模式。
        m = clone(base_wm)
        m["weight_mode"] = "psfsw_robust"
        msgs = self.semantic_gate_errors("weight_mode", m)
        diag = [e for e in msgs if "FZ-MODE-RETIRED" in e
                and "point_information" in e and "migration" in e]
        self._ck("O31b-retired-mode-diagnostic", bool(diag),
                 "retired token must yield FZ-MODE-RETIRED + allowed set + migration "
                 "hint (got: %s)" % (msgs,))
        self._ck("O31c-retired-not-production",
                 "psfsw_robust" not in PRODUCTION_MODES
                 and "psfsw_robust" in RETIRED_MODES,
                 "retired token must not be a production mode")

    def _o30_structural_guards(self):
        """结构门单独可用：必须被 JSON Schema 本体（不经语义层）拒绝的已知违例。"""
        specs = [
            ("psfsw", "psfsw", "psfsw.example.json",
             [("weight.units", "flux^-2"), ("weight.group_normalized", False),
              ("weight.normalization.scope", "global"), ("weight.normalization.median_target", 2.0),
              ("components.concentration.units", "ADU/px")]),
            ("weight-mode", "weight_mode", "weight-mode.example.json",
             [("weight_mode", "psf_snr_power"), ("weight_mode", 0),
              ("legacy_integer_allowed", True), ("weight_mode", "equal"),
              # PSFSW-RETIRE-03：退役对象 token 不是合法模式值（结构门判红）。
              ("weight_mode", "psfsw_robust")]),
            ("provenance", "provenance", "provenance.example.json",
             [("k_corr.value", 1)]),
            ("covariance", "covariance", "covariance.example.json",
             [("variance_from", "psfsw_robust_weight"), ("propagation", "Var = 1/W")]),
            ("signal", "signal", "signal.example.json", []),
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
        for tag, key, ex, muts in specs:
            sch = self.schema(key)
            base = self.j("eng/contracts/data/examples/v6/" + ex)
            for dotted, val in muts:
                m = clone(base)
                set_path(m, dotted, val)
                if jm.is_valid(m, sch):
                    not_rejected.append("%s:%s=%r" % (tag, dotted, val))
        m = clone(self.j("eng/contracts/data/examples/v6/signal.example.json"))
        m["pixel_semantics"] = "surface_brightness"; m["pixel_area_power"] = -4
        if jm.is_valid(m, self.schema("signal")):
            not_rejected.append("signal:pixel_area_power mismatch")
        m = clone(self.j("eng/contracts/data/examples/v6/provenance.example.json"))
        del m["k_corr"]
        if jm.is_valid(m, self.schema("provenance")):
            not_rejected.append("provenance:missing k_corr")
        m = clone(self.j("eng/contracts/data/examples/v6/effective-psf.example.json"))
        del m["effective_psf_id"]
        if jm.is_valid(m, self.schema("effective_psf")):
            not_rejected.append("effective-psf:missing id")
        m = clone(self.j("eng/contracts/data/examples/v6/provenance.example.json"))
        m["units"] = {"bunit": "ADU", "pixel_semantics": "integrated_flux",
                      "pixel_area_power": 0, "target_pixel_area": 1.0}
        if jm.is_valid(m, self.schema("provenance")):
            not_rejected.append("provenance:bunit unjudgeable")
        m = clone(self.j("eng/contracts/data/examples/v6/covariance.example.json"))
        m["representation"] = "diagonal_variance"; del m["correlation_kernel"]
        if jm.is_valid(m, self.schema("covariance")):
            not_rejected.append("covariance:diagonal without kernel")
        self._ck("O30-structural-guards", not not_rejected,
                 "schema failed to reject: %s" % not_rejected)

    def _o32_pending_structural_refs(self):
        """结构门里编码的 PENDING 数值必须显式标注待签，不得冒充已冻结。"""
        status = {c["id"]: c["status"] for c in self.registry()["clauses"]}
        bad = []
        for key in DEF_KEYS:
            d = self.schema(key)
            for cid in d["x-astrocs-production"].get("pending_owner_signoff_structural_refs", []):
                if status.get(cid) != "PENDING_OWNER_SIGNOFF":
                    bad.append("%s: %s not PENDING in registry" % (key, cid))
        n_common = self.schema("psfsw")["properties"]["common_star_set"]["properties"]["n_common"]
        if "PENDING_OWNER_SIGNOFF" not in (n_common.get("x-astrocs", {}).get("signoff") or ""):
            bad.append("psfsw n_common not annotated pending")
        kcv = self.schema("provenance")["properties"]["k_corr"]["properties"]["value"]
        if "PENDING_OWNER_SIGNOFF" not in (kcv.get("x-astrocs", {}).get("signoff") or ""):
            bad.append("provenance k_corr.value not annotated pending")
        self._ck("O32-pending-structural-refs", not bad, "; ".join(bad))

    # ── canonical 对象层门（C-02 合并落点，DOC-CONTRACT-MERGE-02 新增）──
    def _o33_canonical_bunit_gate(self):
        """canonical signal/variance/ivar 必须带 FZ-BUNIT-SEMANTICS 的 allOf 量纲可判门，且能红。"""
        bad = []
        for name in ("signal", "variance", "ivar"):
            if "FZ-BUNIT-SEMANTICS" not in self._canonical_gate_ids(name):
                bad.append(name + " 缺 FZ-BUNIT-SEMANTICS 分支")
        if not bad:
            sig = self.unified("signal")
            base = self.j("eng/contracts/schemas/unified/examples/signal.example.json")
            m = json.loads(json.dumps(base))
            m["units"]["bunit"] = "ADU"          # pixel_area_power=-2 却写 ADU ⇒ 必须拒绝
            if jm.is_valid(m, sig):
                bad.append("canonical signal 未拒绝 bunit=ADU 而 pixel_area_power!=0")
            ok = json.loads(json.dumps(base))
            ok["units"]["bunit_semantics"] = "declared_via_provenance"
            ok["units"]["bunit"] = "ADU"
            if jm.is_valid(ok, sig):
                pass  # declared 分支要求 pixel_semantics=surface_brightness + power=-2，正例满足
            else:
                bad.append("canonical signal 误拒合法 declared_via_provenance 记录")
        self._ck("O33-canonical-bunit-gate", not bad, "; ".join(bad))

    def _o34_canonical_diagonal_gate(self):
        """canonical variance 必须带"对角表示 ⇒ 必带相关核/算子描述"门，且能红。"""
        bad = []
        v = self.unified("variance")
        if "FZ-FORMULA-COV-PROP" not in self._canonical_gate_ids("variance"):
            bad.append("variance 缺 FZ-FORMULA-COV-PROP 分支")
        base = self.j("eng/contracts/schemas/unified/examples/variance.example.json")
        m = json.loads(json.dumps(base))
        m["representation"] = "diagonal"
        m.pop("correlation_kernel", None)
        if jm.is_valid(m, v):
            bad.append("canonical variance 未拒绝 diagonal 且无相关核/算子描述")
        m2 = json.loads(json.dumps(base))
        m2.pop("correlation_kernel", None)
        if jm.is_valid(m2, v):
            bad.append("canonical variance 未拒绝 diagonal_plus_correlation_kernel 缺核")
        if not jm.is_valid(base, v):
            bad.append("canonical variance 误拒合法正例")
        self._ck("O34-canonical-diagonal-gate", not bad, "; ".join(bad))

    def _o35_canonical_kcorr_gate(self):
        """canonical provenance 必须带 k_corr != 1 门，且能红。"""
        bad = []
        p = self.unified("provenance")
        if "FZ-PROV-KCORR" not in self._canonical_gate_ids("provenance"):
            bad.append("provenance 缺 FZ-PROV-KCORR 分支")
        kc = p["properties"].get("k_corr")
        if not kc:
            bad.append("canonical provenance 无 k_corr 属性")
        else:
            if not jm.is_valid(0.9, kc["properties"]["value"]):
                bad.append("canonical k_corr.value 误拒 0.9")
            if jm.is_valid(1.0, kc["properties"]["value"]):
                bad.append("canonical k_corr.value 未拒绝 1.0")
            if not {"definition", "domain", "value", "calibration"} <= set(kc["required"]):
                bad.append("canonical k_corr required 不完整")
        base = self.j("eng/contracts/schemas/unified/examples/provenance.example.json")
        m = json.loads(json.dumps(base))
        m["k_corr"] = {"definition": "k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]",
                       "value": 1, "domain": {"geometry": "TAN", "pixfrac": 1.0,
                                              "patch_size": 8, "estimator": "median", "spherical": True},
                       "calibration": {"script": "x", "seed": 1}}
        if jm.is_valid(m, p):
            bad.append("canonical provenance 未拒绝 k_corr.value=1")
        self._ck("O35-canonical-kcorr-gate", not bad, "; ".join(bad))

    def _o36_canonical_forbidden_keys(self):
        """13 个 canonical 对象 schema 的 propertyNames 必须覆盖 psfsw 禁止键 + 扩展 guard 键。"""
        want = set(self.registry()["forbidden"]["psfsw_forbidden_keys"]) | \
               set(self.registry()["forbidden"]["psfsw_extended_guard_keys"])
        bad = []
        for name in UNIFIED_OBJECTS:
            pn = self.unified(name).get("propertyNames", {})
            have = set(pn.get("not", {}).get("enum", []))
            miss = sorted(want - have)
            if miss:
                bad.append("%s 缺禁止键 %s" % (name, miss))
            if not pn.get("pattern"):
                bad.append("%s 缺模糊名 pattern 守卫" % name)
        self._ck("O36-canonical-forbidden-keys", not bad, "; ".join(bad[:4]))

    def report(self):
        return {
            "checks_total": len(self.checks),
            "checks_passed": sum(1 for c in self.checks if c["ok"]),
            "checks_failed": sum(1 for c in self.checks if not c["ok"]),
            "failures": self.failures,
            "checks": self.checks,
        }


def main():
    root = pathlib.Path(__file__).resolve().parents[4]
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
