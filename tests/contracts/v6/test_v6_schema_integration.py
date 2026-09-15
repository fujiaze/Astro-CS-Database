#!/usr/bin/env python3
"""SCHEMA-INTEGRATE-001 / W6 —— 生产 schema 集成契约测试（unittest；CI UT-CONTRACTS 可跑）。

覆盖：结构 Oracle（对照 W4 冻结合同）、单一词表、legacy→v6 兼容/迁移、文档守卫、
自带校验器与官方 jsonschema（若可用）差分。
"""
import json, pathlib, sys, unittest

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

try:
    from v6 import v6_oracle as oracle_mod
    from v6 import jsonschema_min as jm
except ImportError:  # 直接运行
    import v6_oracle as oracle_mod
    import jsonschema_min as jm

EXAMPLES = REPO / "contracts/data/examples/v6"
SCHEMAS = REPO / "contracts/schemas/v6"

TARGETS = {
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


def _load(p):
    return json.loads(pathlib.Path(p).read_text(encoding="utf-8"))


class TestV6Oracle(unittest.TestCase):
    def test_oracle_all_checks_pass(self):
        o = oracle_mod.Oracle(REPO)
        _, failures = o.run()
        self.assertEqual([], failures, "Oracle failures: %s" % failures)
        rep = o.report()
        self.assertGreaterEqual(rep["checks_total"], 30)
        self.assertEqual(rep["checks_failed"], 0)

    def test_oracle_is_independent_of_generators(self):
        src = (HERE / "v6_oracle.py").read_text(encoding="utf-8")
        self.assertNotIn("import gen_data_dictionary", src)
        self.assertNotIn("import gen_production_schemas", src)
        self.assertNotIn("subprocess", src)
        self.assertIn("astrocs.v6.contract-freeze.v1.json", src)


class TestV6Examples(unittest.TestCase):
    def test_all_examples_validate_with_bundled_validator(self):
        for ex, fn in TARGETS.items():
            doc = _load(EXAMPLES / ex)
            schema = _load(SCHEMAS / fn)
            self.assertEqual([], jm.validate(doc, schema), "%s should validate" % ex)

    def test_example_count_is_ten(self):
        self.assertEqual(10, len(TARGETS))
        self.assertEqual(sorted(TARGETS), sorted(p.name for p in EXAMPLES.glob("*.example.json")))

    def test_differential_with_official_jsonschema_if_available(self):
        """自带校验器与官方 Draft202012Validator 在正/负语料上判词一致（官方可用时）。"""
        corpus = []
        for ex, fn in TARGETS.items():
            corpus.append((ex, _load(EXAMPLES / ex), _load(SCHEMAS / fn), True))
        ps = _load(SCHEMAS / "astrocs.v6.psfsw.v1.schema.json")
        base = _load(EXAMPLES / "psfsw.example.json")
        m = json.loads(json.dumps(base)); m["weight"]["units"] = "flux^-2"
        corpus.append(("psfsw-bad-units", m, ps, False))
        m = json.loads(json.dumps(base)); m["components"]["concentration"]["units"] = "ADU/px"
        corpus.append(("psfsw-bad-conc", m, ps, False))
        pv = _load(SCHEMAS / "astrocs.v6.provenance.v1.schema.json")
        pb = _load(EXAMPLES / "provenance.example.json")
        m = json.loads(json.dumps(pb)); m["k_corr"]["value"] = 1
        corpus.append(("prov-kcorr-1", m, pv, False))
        wm = _load(SCHEMAS / "astrocs.v6.weight-mode.v1.schema.json")
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


class TestV6FreezeConsistency(unittest.TestCase):
    def setUp(self):
        self.o = oracle_mod.Oracle(REPO)

    def test_units_table_matches_freeze(self):
        fr = self.o.freeze()["units_table"]
        d = self.o.dictionary()
        got = [{k: v for k, v in r.items() if k != "clause_id"} for r in d["frozen_units_table"]]
        self.assertEqual(fr, got)

    def test_weight_mode_sets_match_freeze(self):
        fr = self.o.freeze()["weight_modes"]
        wm = self.o.dictionary()["weight_modes"]
        self.assertEqual(fr["production"], wm["production"])
        self.assertEqual(fr["documented_baseline"], wm["documented_baseline"])
        self.assertEqual(fr["deferred"], wm["deferred"])
        self.assertFalse(wm["legacy_integer_allowed"])
        self.assertNotIn("psf_snr_power", wm["production"])

    def test_forbidden_tokens_match_freeze(self):
        fr = self.o.freeze()["forbidden"]
        f = self.o.dictionary()["forbidden"]
        self.assertEqual(set(fr["weight_source_tokens"]), set(f["weight_source_tokens"]))
        self.assertEqual(set(fr["psfsw_forbidden_keys"]), set(f["psfsw_forbidden_keys"]))
        for tok in ("median_source_snr", "support", "coverage", "fwhm"):
            self.assertIn(tok, f["weight_source_tokens"])

    def test_pending_signoff_not_written_as_frozen(self):
        reg = self.o.dictionary()["clause_registry"]
        self.assertEqual(49, reg["counts_by_status"]["PENDING_OWNER_SIGNOFF"])
        self.assertEqual(39, reg["counts_by_status"]["FROZEN"])
        self.assertEqual(8, reg["counts_by_status"]["OPEN"])
        self.assertFalse(set(reg["ids_by_status"]["PENDING_OWNER_SIGNOFF"])
                         & set(reg["ids_by_status"]["FROZEN"]))
        for it in reg["signoff_items"]:
            self.assertEqual("PENDING_OWNER_SIGNOFF", it["status"])


class TestV6VocabularyAndMigration(unittest.TestCase):
    def setUp(self):
        self.v = _load(REPO / "contracts/data/v6_weight_vocabulary_v1.json")
        self.m = _load(REPO / "contracts/data/v6_migration_map_v1.json")

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

    def test_no_alias_fields_in_production_schemas(self):
        for p in SCHEMAS.glob("*.schema.json"):
            txt = p.read_text(encoding="utf-8")
            for a in ("kind_alias_sci_psfw", "units_alias_sci_psfw", "normalization_scope_alias"):
                self.assertNotIn(a, txt, "%s still has %s" % (p.name, a))

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


class TestV6DocsGuard(unittest.TestCase):
    def test_v6_sections_present(self):
        ds = (REPO / "docs/contracts/DATA_SEMANTICS.md").read_text(encoding="utf-8")
        pa = (REPO / "docs/contracts/PUBLIC_API.md").read_text(encoding="utf-8")
        self.assertIn("## 31. V6 目标态数据合同", ds)
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
        s = _load(REPO / "contracts/data/phase_product_exchange.schema.json")
        planes = set(s["$defs"]["plane"]["properties"]["plane_id"]["enum"])
        self.assertEqual({"signal", "support", "variance", "ivar", "mask"}, planes)


if __name__ == "__main__":
    unittest.main(verbosity=2)
