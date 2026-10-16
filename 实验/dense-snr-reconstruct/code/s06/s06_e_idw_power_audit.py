#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06-E：生产 idw_power 有效默认与 API 一致性审计（只读源码，不改 lib/**）。

核验目标（D-07 / U-04）：
  A) 终裁口径：docs/detail/registry/acsd.phase1.noise-snr.md 规定备选 IDW 幂次默认 1.0
     （适用域=含噪场景、最优带 0.5–1；p=2 属无噪/光滑极限）。
  B) drizzle 逐像素求值器：成员默认与两处非正兜底 = 1.0。
  C) Phase1 估计器：snr_extract_model 三处出口把 out_model->idw_power 硬写 2.0。
  D) 数据面：orchestrator 把该值序列化进 snr_model 块；drizzle 加载器从块里读回并
     **显式传参**给 SnrEvaluator::build ⇒ 兜底 1.0 不生效 ⇒ 生产有效值仍是 2.0。

输出：run/ACSDSDOC-02/S06/results/S06_E_idw_power_audit.json
命令：python3 实验/dense-snr-reconstruct/code/s06/s06_e_idw_power_audit.py
"""
from __future__ import annotations

import re
from pathlib import Path

from s06_common import dump, run_meta, sha256_file

SCRIPT = "s06_e_idw_power_audit.py"
COMMAND = "python3 实验/dense-snr-reconstruct/code/s06/s06_e_idw_power_audit.py"
ROOT = Path(__file__).resolve().parents[4]

FILES = {
    "registry": "docs/detail/registry/acsd.phase1.noise-snr.md",
    "drizzle_h": "lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h",
    "drizzle_cpp": "lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp",
    "estimator_cpp": "lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp",
    "estimator_h": "lib/algorithms/noise_snr/cpp/include/snr_estimator.h",
    "aio_model_h": "lib/infrastructure/aio/include/aio_snr_model.h",
    "aio_io_h": "lib/infrastructure/aio/include/aio_healpix_io.h",
    "orchestrator": "lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp",
    "drizzle_api": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp",
    "aio_io_cpp": "lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp",
}

HITS = {}


def scan(key, pattern, group=0):
    path = ROOT / FILES[key]
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    out = []
    rx = re.compile(pattern)
    for i, ln in enumerate(lines, start=1):
        m = rx.search(ln)
        if m:
            out.append({"file": FILES[key], "line": i,
                        "match": (m.group(group) if group else m.group(0)).strip(),
                        "text": ln.strip()[:200]})
    HITS.setdefault(key, []).extend(out)
    return out


scan("drizzle_h", r"idw_power_\s*=\s*([0-9.]+)")
scan("drizzle_h", r"idw_power\s*-\s*IDW")
scan("drizzle_cpp", r"idw_power_\s*=\s*\(idw_power\s*>\s*0\.0\)\s*\?\s*idw_power\s*:\s*([0-9.]+)")
scan("estimator_cpp", r"out_model->idw_power\s*=\s*([0-9.]+)")
scan("estimator_h", r"idw_power;\s*//\s*IDW")
scan("aio_model_h", r"idw_power;\s*//")
scan("aio_io_h", r"idw_power;\s*//")
scan("orchestrator", r"idw_power")
scan("drizzle_api", r"idw_power")
scan("aio_io_cpp", r"idw_power")
scan("registry", r"幂次默认\s*([0-9.]+)")
scan("registry", r"IDW\s*(幂次|口径)")

# 关键行的断言（位置漂移即报红，避免只统计不定位）
def at(key, line, needle):
    for h in HITS.get(key, []):
        if h["line"] == line and needle in h["text"]:
            return True
    return False


assertions = {
    "drizzle_member_default_is_1p0": {
        "declared": "lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:110",
        "ok": at("drizzle_h", 110, "idw_power_ = 1.0"),
    },
    "drizzle_fallback_build_is_1p0": {
        "declared": "lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:212",
        "ok": at("drizzle_cpp", 212, ": 1.0"),
    },
    "drizzle_fallback_buildF64_is_1p0": {
        "declared": "lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:268",
        "ok": at("drizzle_cpp", 268, ": 1.0"),
    },
    "estimator_593_writes_2p0": {
        "declared": "lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:593",
        "ok": at("estimator_cpp", 593, "2.0"),
    },
    "estimator_730_writes_2p0": {
        "declared": "lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:730",
        "ok": at("estimator_cpp", 730, "2.0"),
    },
    "estimator_833_writes_2p0": {
        "declared": "lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:833",
        "ok": at("estimator_cpp", 833, "2.0"),
    },
    "orchestrator_serializes_model_idw_power": {
        "declared": "lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:3956",
        "ok": at("orchestrator", 3956, "model.idw_power"),
    },
    "drizzle_loader_reads_block_tail": {
        "declared": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp:779 and :860",
        "ok": at("drizzle_api", 779, "idw_power") and at("drizzle_api", 860, "idw_power"),
    },
    "drizzle_passes_explicit_value_to_build": {
        "declared": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp:798,823",
        "ok": at("drizzle_api", 798, "idw_power") and at("drizzle_api", 823, "idw_power"),
    },
}

# 数据面结论：显式传参覆盖兜底 ⇒ 生产有效值 = 块内值 = Phase1 写的 2.0
path_chain = [
    {"step": 1, "where": "lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:593,730,833",
     "what": "snr_extract_model 出口把 out_model->idw_power 硬写 2.0", "value": 2.0},
    {"step": 2, "where": "lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:3943,3956",
     "what": "orchestrator 把 model.idw_power 序列化进 snr_model 块尾部 [snr_phot,median_snr,idw_power]",
     "value": "model.idw_power (=2.0)"},
    {"step": 3, "where": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp:779,860",
     "what": "drizzle 从块尾 memcpy 读回 idw_power", "value": 2.0},
    {"step": 4, "where": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp:798,823",
     "what": "显式作为 build(..., idw_power) 入参传入 ⇒ snr_evaluator.cpp:212/268 的 1.0 兜底不触发",
     "value": 2.0},
    {"step": 5, "where": "lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:309,370",
     "what": "w = 1/gamma^idw_power_ 实际生效", "value": 2.0},
]

registry_text = (ROOT / FILES["registry"]).read_text(encoding="utf-8", errors="replace")
registry_declares_1p0 = "幂次默认 1.0" in registry_text or "默认 1.0" in registry_text
registry_fallback_note = "兜底默认" in registry_text and "1.0" in registry_text

payload = {
    **run_meta(SCRIPT, COMMAND),
    "source_sha256": {k: sha256_file(ROOT / v) for k, v in FILES.items()},
    "hits": HITS,
    "line_assertions": assertions,
    "registry_normative": {
        "file": FILES["registry"],
        "declares_idw_default_1p0": registry_declares_1p0,
        "declares_fallback_default_1p0": registry_fallback_note,
        "note": ("registry 只声明「snr_evaluator 的**兜底**默认与规范一致为 1.0」"
                 "并写明「配置显式传值时不触发兜底」；它未裁决「块内显式值 2.0」这一路径 ⇒ "
                 "规范文字与生产有效值仍分裂。"),
    },
    "production_effective_path": path_chain,
    "production_effective_idw_power": 2.0,
    "normative_idw_power": 1.0,
    "split_live": True,
    "finding_verdict": "RED (effective production default 2.0 != normative 1.0; drizzle compile-time fallback 1.0 never applies when the snr_model block is present)",
    "finding_is_a_defect_not_a_pass": True,
    "recommendation": [
        "单一权威：新增配置键（如 snr.idw_power，默认 1.0），Phase1/Phase2 两条链都从配置解析并写入 manifest/日志；",
        "在 snr_model 块尾字段语义上标注「本字段由 Phase1 估计器写入、消费侧显式优先」或改为「消费侧忽略块内值、一律用配置」；",
        "snr_estimator.h:590 / aio_snr_model.h:57 / aio_healpix_io.h:66 的「默认 2.0」注释与 API 文档必须同步改为 1.0；",
        "lib/** 不在 S06 写域，本项只登记证据与建议，交 Lead 裁决。",
    ],
}

gates = {
    "G_E1_line_assertions_all_ok": {
        "pass": all(a["ok"] for a in assertions.values()),
        "observed": {k: a["ok"] for k, a in assertions.items()},
    },
    "G_E2_split_is_live_not_cosmetic": {
        "pass": bool(assertions["estimator_593_writes_2p0"]["ok"]
                     and assertions["orchestrator_serializes_model_idw_power"]["ok"]
                     and assertions["drizzle_passes_explicit_value_to_build"]["ok"]),
        "observed": {"production_effective_idw_power": 2.0, "normative_idw_power": 1.0},
        "meaning": "Phase1 写 2.0 → 序列化 → 读回 → 显式传参 ⇒ 兜底 1.0 不生效；分裂是活的",
    },
}
payload["gates"] = gates
# 这两条门只判「审计完整性」（断言行未漂移 + 数据链可追），不判科学结论；
# 科学结论是 finding_verdict = RED（分裂仍然活着）。
payload["audit_integrity_pass"] = all(g["pass"] for g in gates.values())
dump("S06_E_idw_power_audit.json", payload)
print(f"[S06-E] audit_integrity_pass={payload['audit_integrity_pass']}  finding_verdict={payload['finding_verdict']}")
for k, a in assertions.items():
    print(f"  {k}: {'OK' if a['ok'] else 'MISSING'}  ({a['declared']})")
