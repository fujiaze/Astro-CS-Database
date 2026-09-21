#!/usr/bin/env python3
"""SCHEMA-INTEGRATE-001 / W6 —— 负向 mutation：注入已知冻结违例，Oracle 必须判红。

每条 mutation 把单个生产 artifact 的篡改副本放入临时影子文件，通过 Oracle.overrides
重跑**同一** Oracle；断言 Oracle 失败且指定的检查项变红。含正向控制：未篡改时 Oracle 全过。
"""
import copy, json, os, pathlib, shutil, sys, tempfile, unittest

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[3]

sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

try:
    from v6 import v6_oracle as oracle_mod
except ImportError:
    import v6_oracle as oracle_mod

PSFSW = "eng/contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json"
WEIGHTMODE = "eng/contracts/schemas/v6/astrocs.v6.weight-mode.v1.schema.json"
PROVENANCE = "eng/contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json"
COVARIANCE = "eng/contracts/schemas/v6/astrocs.v6.covariance.v1.schema.json"
SIGNAL = "eng/contracts/schemas/v6/astrocs.v6.signal.v1.schema.json"
EPSF = "eng/contracts/schemas/v6/astrocs.v6.effective-psf.v1.schema.json"
DICT = "eng/contracts/data/v6_data_dictionary_v1.json"
VOCAB = "eng/contracts/data/v6_weight_vocabulary_v1.json"
MIG = "eng/contracts/data/v6_migration_map_v1.json"
EX_PSFSW = "eng/contracts/data/examples/v6/psfsw.example.json"
EX_PROV = "eng/contracts/data/examples/v6/provenance.example.json"
DOC_DS = "docs/contracts/DATA_SEMANTICS.md"
DOC_PA = "docs/contracts/PUBLIC_API.md"


def _json_mut(path_fn):
    def f(d):
        d = copy.deepcopy(d)
        path_fn(d)
        return d
    return f


def _text_mut(fn):
    def f(t):
        return fn(t)
    return f


MUTATIONS = [
    {"id": "M01_psfsw_units_ivar", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "psfsw weight.units = flux^-2 (ivar dimension)",
     "mutate": _json_mut(lambda d: d["properties"]["weight"]["properties"]["units"].update(const="flux^-2"))},
    {"id": "M02_psfsw_group_normalized_false", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "group_normalized const false",
     "mutate": _json_mut(lambda d: d["properties"]["weight"]["properties"]["group_normalized"].update(const=False))},
    {"id": "M03_psfsw_scope_global", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "normalization.scope = global",
     "mutate": _json_mut(lambda d: d["properties"]["weight"]["properties"]["normalization"]["properties"]["scope"].update(const="global"))},
    {"id": "M04_psfsw_median_target_2", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "normalization.median_target = 2.0",
     "mutate": _json_mut(lambda d: d["properties"]["weight"]["properties"]["normalization"]["properties"]["median_target"].update(const=2.0))},
    {"id": "M05_psfsw_drop_component_flux_unit", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "drop component_flux_unit from required",
     "mutate": _json_mut(lambda d: d.__setitem__("required", [r for r in d["required"] if r != "component_flux_unit"]))},
    {"id": "M06_psfsw_relax_concentration_pattern", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "concentration.units pattern relaxed to .* (allows ADU/px)",
     "mutate": _json_mut(lambda d: d["properties"]["components"]["properties"]["concentration"]["allOf"][1]["properties"]["units"].update(pattern="^.*$"))},
    {"id": "M07_psfsw_validity_reason_extra", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "validity.reason whitelist + median_snr_fallback",
     "mutate": _json_mut(lambda d: d["properties"]["validity"]["properties"]["reason"]["enum"].append("median_snr_fallback"))},
    {"id": "M08_psfsw_clear_propertynames", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "clear psfsw forbidden-key propertyNames guard",
     "mutate": _json_mut(lambda d: d["propertyNames"]["not"].update(enum=[]))},
    {"id": "M18_psfsw_kind_ivar", "target": PSFSW, "kind": "json", "expect": "O16-psfsw",
     "inject": "weight.kind const = ivar",
     "mutate": _json_mut(lambda d: d["properties"]["weight"]["properties"]["kind"].update(const="ivar"))},
    {"id": "M09_weightmode_add_deferred", "target": WEIGHTMODE, "kind": "json", "expect": "O18-mode-enum",
     "inject": "weight_mode enum += psf_snr_power",
     "mutate": _json_mut(lambda d: d["properties"]["weight_mode"]["enum"].append("psf_snr_power"))},
    {"id": "M10_weightmode_legacy_allowed", "target": WEIGHTMODE, "kind": "json", "expect": "O18c-legacy",
     "inject": "legacy_integer_allowed const true",
     "mutate": _json_mut(lambda d: d["properties"]["legacy_integer_allowed"].update(const=True))},
    {"id": "M11_weightmode_clear_deferred", "target": WEIGHTMODE, "kind": "json", "expect": "O18b-deferred",
     "inject": "deferred_modes_documented = []",
     "mutate": _json_mut(lambda d: d["properties"]["deferred_modes_documented"].update(const=[]))},
    {"id": "M12_provenance_kcorr_allow_1", "target": PROVENANCE, "kind": "json", "expect": "O12b-kcorr-neq1",
     "inject": "k_corr.value != 1 structural gate removed",
     "mutate": _json_mut(lambda d: d["properties"]["k_corr"]["properties"].update(value={"type": "number"}))},
    {"id": "M13_provenance_drop_kcorr", "target": PROVENANCE, "kind": "json", "expect": "O11-prov-required",
     "inject": "provenance required -= k_corr",
     "mutate": _json_mut(lambda d: d.__setitem__("required", [r for r in d["required"] if r != "k_corr"]))},
    {"id": "M14_covariance_variance_from_weight", "target": COVARIANCE, "kind": "json", "expect": "O14b-variance-from",
     "inject": "variance_from enum += psfsw_robust_weight",
     "mutate": _json_mut(lambda d: d["properties"]["variance_from"]["enum"].append("psfsw_robust_weight"))},
    {"id": "M15_covariance_propagation_changed", "target": COVARIANCE, "kind": "json", "expect": "O14-propagation",
     "inject": "propagation const -> Var = 1/W",
     "mutate": _json_mut(lambda d: d["properties"]["propagation"].update(const="Var = 1/W"))},
    {"id": "M16_signal_remove_power_gate", "target": SIGNAL, "kind": "json", "expect": "O30-structural-guards",
     "inject": "remove signal pixel_semantics <-> pixel_area_power structural gate",
     "mutate": _json_mut(lambda d: d.pop("allOf"))},
    {"id": "M17_epsf_drop_id_required", "target": EPSF, "kind": "json", "expect": "O13-epsf-required",
     "inject": "effective-psf required -= effective_psf_id",
     "mutate": _json_mut(lambda d: d.__setitem__("required", [r for r in d["required"] if r != "effective_psf_id"]))},
    {"id": "M19_dict_winfo_unit_wrong", "target": DICT, "kind": "json", "expect": "O03-frozen-units-table",
     "inject": "dictionary W_info unit -> ADU^-1",
     "mutate": _json_mut(lambda d: next(r for r in d["frozen_units_table"] if r["symbol"] == "W_info").update(unit="ADU^-1"))},
    {"id": "M20_dict_forbidden_drop_fwhm", "target": DICT, "kind": "json", "expect": "O07-weight-source-tokens",
     "inject": "dictionary forbidden sources -= fwhm",
     "mutate": _json_mut(lambda d: d["forbidden"].__setitem__("weight_source_tokens",
             [t for t in d["forbidden"]["weight_source_tokens"] if t != "fwhm"]))},
    {"id": "M21_dict_pending_as_frozen", "target": DICT, "kind": "json", "expect": "O20-clause-registry",
     "inject": "signoff item status -> FROZEN (pending written as frozen)",
     "mutate": _json_mut(lambda d: d["clause_registry"]["signoff_items"][0].update(status="FROZEN"))},
    {"id": "M22_dict_counts_wrong", "target": DICT, "kind": "json", "expect": "O20-clause-registry",
     "inject": "pending count -> 48",
     "mutate": _json_mut(lambda d: d["clause_registry"]["counts_by_status"].update({"PENDING_OWNER_SIGNOFF": 48}))},
    {"id": "M23_dict_concentration_wrong", "target": DICT, "kind": "json", "expect": "O24-concentration-authority",
     "inject": "concentration authoritative unit -> ADU/px",
     "mutate": _json_mut(lambda d: d["concentration_unit_authority"].update(authoritative_unit="ADU/px"))},
    {"id": "M24_dict_mode_units_wrong", "target": DICT, "kind": "json", "expect": "O06-mode-details",
     "inject": "psfsw mode_details units -> 1/flux^2",
     "mutate": _json_mut(lambda d: next(m for m in d["weight_modes"]["mode_details"] if m["mode"] == "psfsw_robust").update(units="1/flux^2"))},
    {"id": "M25_vocab_units_canonical_wrong", "target": VOCAB, "kind": "json", "expect": "O22-vocabulary",
     "inject": "canonical weight.units -> dimensionless_relative",
     "mutate": _json_mut(lambda d: next(f for f in d["canonical_fields"] if f["field"] == "weight.units").update(canonical_psfsw_value="dimensionless_relative"))},
    {"id": "M26_vocab_legacy0_allowed", "target": VOCAB, "kind": "json", "expect": "O22-vocabulary",
     "inject": "legacy 0 disposition -> MIGRATE_TO",
     "mutate": _json_mut(lambda d: d["legacy_integer"]["disposition"]["0"].update(action="MIGRATE_TO", target="equal"))},
    {"id": "M27_mig_legacy0_allowed", "target": MIG, "kind": "json", "expect": "O23-migration-map",
     "inject": "migration map legacy 0 action -> MIGRATE_TO",
     "mutate": _json_mut(lambda d: d["legacy_weight_mode_disposition"]["0"].update(action="MIGRATE_TO"))},
    {"id": "M28_mig_drop_schema_file", "target": MIG, "kind": "json", "expect": "O23-migration-map",
     "inject": "drop one schema file migration",
     "mutate": _json_mut(lambda d: d.__setitem__("file_migrations", [e for e in d["file_migrations"] if "signal" not in e["from"]]))},
    {"id": "M29_example_concentration_adu_px", "target": EX_PSFSW, "kind": "json", "expect": "O27-examples-positive",
     "inject": "production example concentration.units = ADU/px",
     "mutate": _json_mut(lambda d: d["components"]["concentration"].update(units="ADU/px"))},
    {"id": "M30_example_kcorr_1", "target": EX_PROV, "kind": "json", "expect": "O27-examples-positive",
     "inject": "production example k_corr.value = 1",
     "mutate": _json_mut(lambda d: d["k_corr"].update(value=1))},
    {"id": "M34_pending_numeric_written_as_frozen", "target": PSFSW, "kind": "json", "expect": "O32-pending-structural-refs",
     "inject": "PENDING n_common numeric annotated as FROZEN",
     "mutate": _json_mut(lambda d: d["properties"]["common_star_set"]["properties"]["n_common"]["x-astrocs"].update(signoff="FROZEN (signed)"))},
    {"id": "M31_doc_remove_v6_section", "target": DOC_DS, "kind": "text", "expect": "O25-docs-present",
     "inject": "remove DATA_SEMANTICS section-31 heading",
     "mutate": _text_mut(lambda t: t.replace("## 31. V6 合同层数据合同", "## 31. (removed)"))},
    {"id": "M32_doc_readd_p33_snr_coefficient", "target": DOC_DS, "kind": "text", "expect": "O26-no-p33-p27-reintroduction",
     "inject": "reintroduce P33-COEF SNR coefficient section",
     "mutate": _text_mut(lambda t: t + "\n\n**P33-COEF：帧级 SNR 系数在 HiPS 产品中的落位**\n\n- snr_coefficient 重新落位\n")},
    {"id": "M33_doc_readd_p27_dead_params", "target": DOC_PA, "kind": "text", "expect": "O26-no-p33-p27-reintroduction",
     "inject": "reintroduce P27 dead-params section",
     "mutate": _text_mut(lambda t: t + "\n\n### P27：生产路径不消费的 IpvParams 字段\n\nP27-DEAD-PARAMS\n")},
]


def evaluate_mutations(verbose=False):
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="v6_mut_"))
    results = []
    try:
        for spec in MUTATIONS:
            src = REPO / spec["target"]
            if spec["kind"] == "json":
                doc = json.loads(src.read_text(encoding="utf-8"))
                mutated = spec["mutate"](doc)
                dst = tmp / (spec["id"] + ".json")
                dst.write_text(json.dumps(mutated, ensure_ascii=False, indent=2), encoding="utf-8")
            else:
                txt = src.read_text(encoding="utf-8")
                dst = tmp / (spec["id"] + ".txt")
                dst.write_text(spec["mutate"](txt), encoding="utf-8")
            o = oracle_mod.Oracle(REPO, overrides={spec["target"]: str(dst)})
            o.run()
            failed_ids = {c["id"] for c in o.checks if not c["ok"]}
            results.append({
                "id": spec["id"], "target": spec["target"], "inject": spec["inject"],
                "expected_check": spec["expect"], "detected": bool(o.failures),
                "expected_check_red": spec["expect"] in failed_ids,
                "failed_checks": sorted(failed_ids),
            })
            if verbose:
                print("%-40s detected=%s expect_red=%s" %
                      (spec["id"], bool(o.failures), spec["expect"] in failed_ids))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return results


class TestV6NegativeMutations(unittest.TestCase):
    def test_at_least_12_mutations(self):
        self.assertGreaterEqual(len(MUTATIONS), 12)

    def test_all_mutations_are_red(self):
        results = evaluate_mutations()
        missed = [r["id"] for r in results if not r["detected"]]
        self.assertEqual([], missed, "mutations NOT detected (gate not live): %s" % missed)
        wrong = [(r["id"], r["expected_check"], r["failed_checks"]) for r in results
                 if not r["expected_check_red"]]
        self.assertEqual([], wrong, "mutations detected by wrong gate: %s" % wrong)

    def test_positive_control_oracle_green_without_mutation(self):
        o = oracle_mod.Oracle(REPO)
        _, failures = o.run()
        self.assertEqual([], failures)


if __name__ == "__main__":
    unittest.main(verbosity=2)
