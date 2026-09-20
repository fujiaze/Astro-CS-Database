#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CFG-001 四条验收门的可复跑断言（rc=0 全过；rc=1 任一失败）。

用法: timeout 120 python3 tests/config/gate_assertions.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cfg_common as C  # noqa: E402

PHASE = {
    "normalize": ("contracts/schemas/phase_config_normalize.schema.json", "config/templates/normalize.phase_config.json"),
    "mosaic": ("contracts/schemas/phase_config_mosaic.schema.json", "config/templates/mosaic.phase_config.json"),
    "export": ("contracts/schemas/phase_config_export.schema.json", "config/templates/export.phase_config.json"),
}
NEGS = [
    ("unknown_filter", "phase_config_normalize", "unknown_filter.phase_config.json"),
    ("missing_output_dir", "phase_config_normalize", "missing_output_dir.phase_config.json"),
    ("precision_out_of_domain", "phase_config_normalize", "precision_out_of_domain.phase_config.json"),
    ("hardware_fields", "phase_config_normalize", "hardware_fields_in_phase_config.json"),
    # CLI-MULTIBLOCK（GAP_AUDIT §9.68，2026-09-20）：多数据块形态的三类新负例
    # （形态互斥 / 块内未知键 / 已退役的逐帧 {phase_name, config, inputs[]} 形态）。
    ("multi_block_mixed_forms", "phase_config_normalize", "normalize_mixed_forms.phase_config.json"),
    ("multi_block_unknown_key", "phase_config_normalize", "normalize_block_unknown_key.phase_config.json"),
    ("retired_perframe_inputs", "phase_config_normalize", "normalize_perframe_inputs.phase_config.json"),
    # FIX-207（§9.71 裁决 2 + §9.73 A44，2026-09-20）：三命令同构块结构的三类新负例
    # （blocks 与平铺键互斥 / 块内 A44 键 / 新分支出现阶段判别键）。
    ("mosaic_blocks_mixed_flat", "phase_config_mosaic", "mosaic_blocks_mixed_flat.phase_config.json"),
    ("export_blocks_mixed_flat", "phase_config_export", "export_blocks_mixed_flat.phase_config.json"),
    ("mosaic_block_weight_mode", "phase_config_mosaic", "mosaic_block_weight_mode.phase_config.json"),
    ("export_block_phase_name", "phase_config_export", "export_block_phase_name.phase_config.json"),
]
CPU = "contracts/schemas/cpu_profile.schema.json"
MANIFEST = "contracts/schemas/run_manifest.schema.json"
FILTERS = "config/filters.json"
# 滤镜库/provenance 定位符由 config/filters.json 声明（目录迁移后不失效）
FIELDS = ["name", "channel", "wavelength_nm", "value", "n_points"]
results = []


def gate(name, ok, detail):
    results.append((name, bool(ok), detail))


# 门 A：三模板通过对应 schema；四个负例必败
ok_tpl, det_tpl = True, []
for phase, (srel, trel) in PHASE.items():
    errs = C.validate(C.load_json(srel), C.load_json(trel))
    ok_tpl = ok_tpl and not errs
    det_tpl.append("%s=%s" % (phase, "PASS" if not errs else "FAIL%d" % len(errs)))
ok_neg, det_neg = True, []
for name, schema_key, fixture in NEGS:
    srel = [v[0] for k, v in PHASE.items() if k == schema_key.split("_")[-1]][0]
    errs = C.validate(C.load_json(srel), C.load_json("tests/config/fixtures/negative/" + fixture))
    ok_neg = ok_neg and bool(errs)
    det_neg.append("%s=%s" % (name, "REJECTED" if errs else "ACCEPTED(BUG)"))
gate("G1 三模板通过对应 schema", ok_tpl, " ".join(det_tpl))
gate("G2 负例必败（原 4 + §9.68 多块 3 + §9.71 同构 4）", ok_neg, " ".join(det_neg))

# 门 B：defaults 计数断言
ddoc = C.load_json("config/defaults.json")
dfields = ddoc["fields"]
n_total = len(dfields)
n_unit = sum(1 for f in dfields if isinstance(f.get("unit"), str) and f["unit"].strip())
n_attr = sum(1 for f in dfields if (isinstance(f.get("source"), str) and f["source"].strip())
             or (f["authority_status"] == "pending_authority" and f.get("pending_task")))
unknown = [f["key"] for f in dfields if f["key"] not in
           {g["key"] for g in dfields
            if (isinstance(g.get("source"), str) and g["source"].strip())
            or (g["authority_status"] == "pending_authority" and g.get("pending_task"))}]
gate("G3 defaults 计数", n_total == n_unit == n_attr and not unknown and n_total == ddoc["field_count"],
     "fields=%d unit=%d attributed=%d unknown=%d declared=%d" % (n_total, n_unit, n_attr, len(unknown), ddoc["field_count"]))

# 门 C：cpu_profile ∩ phase_config == ∅
phase_names = set()
for srel, _t in PHASE.values():
    phase_names |= C.property_names(C.load_json(srel))
cpu = C.load_json(CPU)
inter_v2 = phase_names & C.property_names(cpu["$defs"]["profile_v2"])
inter_all = phase_names & C.property_names(cpu)
inter_manifest = phase_names & C.property_names(C.load_json(MANIFEST))
gate("G4 cpu_profile ∩ phase_config == ∅",
     not inter_v2 and not inter_manifest and inter_all == {"precision"},
     "vs_v2=%d vs_manifest=%d vs_all=%s(登记 legacy 同名)" % (len(inter_v2), len(inter_manifest), sorted(inter_all)))

# 门 D：滤镜转录条数 + provenance 标注 + 无零点列
f = C.load_json(FILTERS)
src = C.load_json(f["transcription"]["source_path"])
prov = C.load_json(f["provenance"]["source_path"])
frozen = (C.sha256_file(f["transcription"]["source_path"]) == f["transcription"]["source_sha256"]
          and C.sha256_file(f["provenance"]["source_path"]) == f["provenance"]["source_sha256"])
verbatim = all(f["filters"][k] == {x: v[x] for x in FIELDS} for k, v in src.items())
unverified = all(f["provenance"]["per_filter"][k]["source"] == prov["filters"][k]["source"]
                 and f["provenance"]["per_filter"][k]["source"].startswith("unverified")
                 for k in src)
zero_keys = [p for p in C.json_key_paths(f) if C.ZERO_RE.search(p)]
zero_text = C.ZERO_RE.search(C.load_text(FILTERS)) is not None
gate("G5 滤镜库转录/provenance/无零点列",
     len(f["filters"]) >= 45 and frozen and verbatim and unverified and not zero_keys and not zero_text,
     "filters=%d verbatim=%s unverified=%d/%d zero_keys=%d zero_text=%s provenance=%s gap=%s"
     % (len(f["filters"]), verbatim, sum(1 for k in src if f["provenance"]["per_filter"][k]["source"].startswith("unverified")),
        len(src), len(zero_keys), zero_text, f["provenance"]["status"], f["provenance"]["gap_id"]))

bad = [r for r in results if not r[1]]
for name, ok, detail in results:
    print("%-38s %s  %s" % (name, "PASS" if ok else "FAIL", detail))
print("GATE_SUMMARY total=%d pass=%d fail=%d" % (len(results), len(results) - len(bad), len(bad)))
sys.exit(1 if bad else 0)