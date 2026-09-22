#!/usr/bin/env python3
"""产品族字段级合同 + 条款注册表 —— 集成契约测试（unittest；CI UT-CONTRACTS 可跑）。

覆盖：结构 Oracle（对照条款注册表与 §31 正文）、单一词表、legacy→canonical 兼容/迁移、
文档守卫、自带校验器与官方 jsonschema（若可用）差分、canonical 对象层 allOf 门。
"""
import json, pathlib, sys, unittest

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[3]

sys.path.insert(0, str(REPO / "eng" / "tests" / "common"))
sys.path.insert(0, str(HERE))

import field_constraints_oracle as oracle_mod  # noqa: E402
import jsonschema_min as jm  # noqa: E402

EXAMPLES = REPO / "eng/contracts/data/examples/v6"
PF = REPO / "eng/contracts/schemas/product_family_field_constraints.schema.json"
REG = REPO / "eng/contracts/data/v6_clause_registry_v1.json"
UNIFIED = REPO / "eng/contracts/schemas/unified"

TARGETS = {
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


def _load(p):
    return json.loads(pathlib.Path(p).read_text(encoding="utf-8"))


def _defs():
    return _load(PF)["$defs"]


class TestFieldConstraintsOracle(unittest.TestCase):
    def test_oracle_all_checks_pass(self):
        o = oracle_mod.Oracle(REPO)
        _, failures = o.run()
        self.assertEqual([], failures, "Oracle failures: %s" % failures)
        rep = o.report()
        self.assertGreaterEqual(rep["checks_total"], 40)
        self.assertEqual(rep["checks_failed"], 0)

    def test_oracle_is_independent_of_generators(self):
        src = (HERE / "field_constraints_oracle.py").read_text(encoding="utf-8")
        self.assertNotIn("import gen_data_dictionary", src)
        self.assertNotIn("import gen_production_schemas", src)
        self.assertNotIn("subprocess", src)
        self.assertIn("v6_clause_registry_v1.json", src)
        self.assertIn("product_family_field_constraints.schema.json", src)

    def test_product_family_schema_is_not_an_object_contract(self):
        """产品族记录级合同不是对象合同：$id 不得与 canonical 对象 schema 撞车，也不得自称为对象类。"""
        pf = _load(PF)
        self.assertIs(False, pf["x-astrocs-contract"]["is_object_contract"])
        canonical_ids = {_load(p)["$id"] for p in UNIFIED.glob("*.schema.json")}
        def_ids = [pf["$defs"][k]["$id"] for k in TARGETS.values()]
        self.assertEqual(len(def_ids), len(set(def_ids)), "产品族 $defs 的 $id 必须互不相同")
        self.assertEqual(set(), set(def_ids) & canonical_ids,
                         "产品族 $defs 复用了 canonical 对象 schema 的 $id")
        self.assertEqual(set(TARGETS.values()), set(pf["$defs"]), "产品族 $defs 键集漂移")


class TestFieldConstraintsExamples(unittest.TestCase):
    def test_all_examples_validate_with_bundled_validator(self):
        defs = _defs()
        for ex, key in TARGETS.items():
            doc = _load(EXAMPLES / ex)
            self.assertEqual([], jm.validate(doc, defs[key]), "%s should validate" % ex)

    def test_example_count_is_ten(self):
        self.assertEqual(10, len(TARGETS))
        self.assertEqual(sorted(TARGETS), sorted(p.name for p in EXAMPLES.glob("*.example.json")))

    def test_differential_with_official_jsonschema_if_available(self):
        """自带校验器与官方 Draft202012Validator 在正/负语料上判词一致（官方可用时）。"""
        defs = _defs()
        corpus = []
        for ex, key in TARGETS.items():
            corpus.append((ex, _load(EXAMPLES / ex), defs[key], True))
        ps = defs["psfsw"]
        base = _load(EXAMPLES / "psfsw.example.json")
        m = json.loads(json.dumps(base)); m["weight"]["units"] = "flux^-2"
        corpus.append(("psfsw-bad-units", m, ps, False))
        m = json.loads(json.dumps(base)); m["components"]["concentration"]["units"] = "ADU/px"
        corpus.append(("psfsw-bad-conc", m, ps, False))
        pv = defs["provenance"]
        pb = _load(EXAMPLES / "provenance.example.json")
        m = json.loads(json.dumps(pb)); m["k_corr"]["value"] = 1
        corpus.append(("prov-kcorr-1", m, pv, False))
        wm = defs["weight_mode"]
        wb = _load(EXAMPLES / "weight-mode.example.json")
        m = json.loads(json.dumps(wb)); m["weight_mode"] = "psf_snr_power"
        corpus.append(("wm-psf-snr-power", m, wm, False))

        for name, doc, schema, expect in corpus:
            self.assertEqual(expect, jm.is_valid(doc, schema), "bundled verdict mismatch: " + name)
        try:
            from jsonschema import Draft202012Validator
        except ImportError:
            # 官方库不可用：至少确认语料覆盖正/负两类（不得 skip-only）
            self.assertTrue(any(c[3] for c in corpus) and any(not c[3] for c in corpus))
            return
        for name, doc, schema, expect in corpus:
            self.assertEqual(expect, Draft202012Validator(schema).is_valid(doc),
                             "official verdict mismatch: " + name)


class TestRegistryConsistency(unittest.TestCase):
    def setUp(self):
        self.o = oracle_mod.Oracle(REPO)
        self.reg = _load(REG)

    def test_units_table_matches_document(self):
        ds = (REPO / "docs/contracts/DATA_SEMANTICS.md").read_text(encoding="utf-8")
        for row in self.reg["units_table"]:
            self.assertIn(row["symbol"], ds)
            self.assertIn(row["unit"], ds)

    def test_weight_mode_sets(self):
        wm = self.reg["weight_modes"]
        # FZ-MODE-PRODUCTION（PSFSW-RETIRE-03 口径统一）：生产接受集 = 2 项；
        # psfsw_robust 是退役对象 token ⇒ 不在生产集，但必须仍被登记为退役（可判）。
        self.assertEqual(["point_information", "surface_gls"], list(wm["production"]))
        self.assertEqual(["equal", "pixel_ivar"], list(wm["documented_baseline"]))
        self.assertEqual(["psf_snr_power"], list(wm["deferred"]))
        self.assertFalse(wm["legacy_integer_allowed"])
        self.assertNotIn("psf_snr_power", wm["production"])
        self.assertNotIn("psfsw_robust", wm["production"])
        self.assertEqual(["psfsw_robust"], list(wm["retired"]))
        retired_detail = [m for m in wm["mode_details"] if m["mode"] == "psfsw_robust"]
        self.assertEqual(1, len(retired_detail))
        self.assertEqual("RETIRED_NOT_PRODUCTION", retired_detail[0]["status"])
        self.assertIn("FZ-MODE-RETIRED", retired_detail[0]["retirement"])

    def test_forbidden_tokens(self):
        f = self.reg["forbidden"]
        for tok in ("median_source_snr", "support", "coverage", "fwhm"):
            self.assertIn(tok, f["weight_source_tokens"])
        self.assertEqual(11, len(f["psfsw_forbidden_keys"]))
        self.assertEqual({"snr", "snr2", "support", "coverage"},
                         set(f["psfsw_extended_guard_keys"]))

    def test_pending_signoff_not_written_as_frozen(self):
        reg = self.reg["clause_registry"]
        self.assertEqual(49, reg["counts_by_status"]["PENDING_OWNER_SIGNOFF"])
        self.assertEqual(39, reg["counts_by_status"]["FROZEN"])
        self.assertEqual(8, reg["counts_by_status"]["OPEN"])
        self.assertFalse(set(reg["ids_by_status"]["PENDING_OWNER_SIGNOFF"])
                         & set(reg["ids_by_status"]["FROZEN"]))
        for it in reg["signoff_items"]:
            self.assertEqual("PENDING_OWNER_SIGNOFF", it["status"])

    def test_clause_anchors_reanchored_to_living_docs(self):
        """49 条待签条款的锚不得悬空：source_binding.file 必须存在。"""
        missing = []
        for c in self.reg["clauses"]:
            sb = c.get("source_binding")
            if not isinstance(sb, dict):
                continue
            f = sb["file"]
            if f.startswith("docs/") and not (REPO / f).exists():
                missing.append((c["id"], f))
        self.assertEqual([], missing, "clause anchors dangling: %s" % missing[:5])


class TestVocabularyAndMigration(unittest.TestCase):
    def setUp(self):
        reg = _load(REG)
        self.v = reg["weight_vocabulary"]
        self.m = reg["migration_map"]

    def test_single_canonical_vocabulary(self):
        fields = {f["field"] for f in self.v["canonical_fields"]}
        for want in ("weight.kind", "weight.units", "weight.group_normalized",
                     "weight.normalization.scope", "weight.normalization.median_target"):
            self.assertIn(want, fields)
        self.assertEqual(4, len(self.v["dual_mapping"]))

    def test_legacy_integer_disposition(self):
        dis = self.v["legacy_integer"]["disposition"]
        self.assertEqual("REJECT", dis["0"]["action"])
        self.assertEqual("equal", dis["1"]["target"])
        self.assertEqual("pixel_ivar", dis["2"]["target"])
        self.assertFalse(self.v["legacy_integer"]["legacy_integer_allowed"])
        self.assertEqual("REJECT", dis["psf_snr_power"]["action"])

    def test_no_alias_fields_in_product_family_schema(self):
        txt = PF.read_text(encoding="utf-8")
        for a in ("kind_alias_sci_psfw", "units_alias_sci_psfw", "normalization_scope_alias"):
            self.assertNotIn(a, txt, "product_family schema still has %s" % a)

    def test_migration_map_covers_catalog_and_legacy(self):
        modeled = set(self.m["catalog_migrations_covered"])
        for mid in ("MIG-WEIGHTMODE-LEGACY", "MIG-UNITS-PXVARIANCE", "MIG-NORM-DRIZZLE",
                    "MIG-PSFSW-VOCAB", "MIG-COVARIANCE-PRODUCT",
                    "MIG-DIAGNOSTIC-NOT-WEIGHT", "MIG-SCHEMA-OWNER"):
            self.assertIn(mid, modeled)
        lw = self.m["legacy_weight_mode_disposition"]
        self.assertEqual("REJECT", lw["0"]["action"])
        self.assertEqual("equal", lw["1"]["target"])
        self.assertEqual("pixel_ivar", lw["2"]["target"])
        # schema/plane 迁移必须明确登记为 OPEN（DI-06）而不是偷偷改 exchange schema
        self.assertEqual("DEFERRED_COUPLED",
                         next(r for r in self.m["record_migrations"]
                              if r["id"] == "MIG-SCHEMA-PLANE-OWNER")["legacy_status"])


class TestCanonicalObjectGates(unittest.TestCase):
    """C-02：产品族字段级判据在 canonical 对象层 allOf 的落点必须真实在册且能红。"""

    def _gate_ids(self, name):
        out = set()
        for br in _load(UNIFIED / ("%s.schema.json" % name)).get("allOf", []) or []:
            g = br.get("x-astrocs-gate") or {}
            if g.get("clause_id"):
                out.add(g["clause_id"])
        return out

    def test_bunit_gate_present(self):
        for name in ("signal", "variance", "ivar"):
            self.assertIn("FZ-BUNIT-SEMANTICS", self._gate_ids(name))

    def test_diagonal_kernel_gate_present(self):
        self.assertIn("FZ-FORMULA-COV-PROP", self._gate_ids("variance"))

    def test_kcorr_gate_present(self):
        self.assertIn("FZ-PROV-KCORR", self._gate_ids("provenance"))

    def test_forbidden_key_guard_present(self):
        want = set(_load(REG)["forbidden"]["psfsw_forbidden_keys"]) | \
               set(_load(REG)["forbidden"]["psfsw_extended_guard_keys"])
        for name in oracle_mod.UNIFIED_OBJECTS:
            have = set(_load(UNIFIED / ("%s.schema.json" % name))
                       .get("propertyNames", {}).get("not", {}).get("enum", []))
            self.assertTrue(want <= have, "%s 缺禁止键守卫: %s" % (name, sorted(want - have)))


class TestDocsGuard(unittest.TestCase):
    def test_v6_sections_present(self):
        ds = (REPO / "docs/contracts/DATA_SEMANTICS.md").read_text(encoding="utf-8")
        pa = (REPO / "docs/contracts/PUBLIC_API.md").read_text(encoding="utf-8")
        self.assertIn("## 31. V6 合同层数据合同", ds)
        self.assertIn("### 31.10 V6 条款注册表与待签登记", ds)
        self.assertIn("### 28.6 Phase3 逐像素立体角", ds)
        self.assertIn("FZ-P3-KERNEL-REGISTRY", ds)
        self.assertIn("component_flux_unit/px^2", ds)
        self.assertIn("API-V6-WEIGHTMODE-001", pa)
        self.assertIn("support×snr²", pa)

    def test_reverted_sections_not_reintroduced(self):
        guard = oracle_mod.FORBIDDEN_DOC_TOKENS
        for rel in ("docs/contracts/DATA_SEMANTICS.md", "docs/contracts/PUBLIC_API.md"):
            txt = (REPO / rel).read_text(encoding="utf-8")
            for tok in guard:
                self.assertNotIn(tok, txt, "%s reintroduced %r" % (rel, tok))

    def test_exchange_plane_enum_untouched(self):
        """DI-06：runtime validator 不在本任务写域，故 exchange schema 的 plane 枚举不得单方面扩展。"""
        s = _load(REPO / "eng/contracts/data/phase_product_exchange.schema.json")
        planes = set(s["$defs"]["plane"]["properties"]["plane_id"]["enum"])
        self.assertEqual({"signal", "support", "variance", "ivar", "mask"}, planes)


if __name__ == "__main__":
    unittest.main(verbosity=2)
