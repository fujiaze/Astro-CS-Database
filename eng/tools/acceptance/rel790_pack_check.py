#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""rel790_pack_check.py —— REL-790 成品帧视觉验收证据包校验器（能红能绿）。

裁决对象 = 负责人收到的**证据包**（一份 JSON 记录 + 它引用的图块/日志）。
校验清单 = eng/tools/acceptance/rel790_checklist.json（帧清单 + 每帧必记字段）。

它回答的是"这一步能不能复核、能不能追溯"，**不代替人看**：
目检结论由 inspector 填写，本门只保证该填的都填了、填的都能定位回原始文件。

用法
----
  python3 eng/tools/acceptance/rel790_pack_check.py --pack <pack.json> \
      [--checklist eng/tools/acceptance/rel790_checklist.json] [--repo-root .]
  python3 eng/tools/acceptance/rel790_pack_check.py --self-test

退出码: 0 判绿 / 1 判红 / 2 fail-closed
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
DEFAULT_CHECKLIST = HERE / "rel790_checklist.json"
EXIT_PASS, EXIT_RED, EXIT_FAILCLOSED = 0, 1, 2
FRAME_VERDICTS = {"ACCEPT", "REJECT", "NOT_EVALUATED"}
REJ_CLASSES = {"product_behavior", "defect", "unknown"}
REJ_EXITS = {"stop_work", "registered"}


def _rule(fid, ok, detail):
    return {"rule": fid, "ok": bool(ok), "detail": detail}


def _sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _inv_gate_ids(repo_root: pathlib.Path):
    p = repo_root / "eng/tools/acceptance/frozen_gate_inventory.json"
    if not p.is_file():
        return None
    try:
        return {g["gate_id"] for g in json.loads(p.read_text(encoding="utf-8"))["gates"]}
    except Exception:
        return None


def adjudicate(pack, checklist, repo_root: pathlib.Path):
    findings = []
    frames = checklist.get("frames") or []
    required = {f["field"] for f in checklist.get("required_fields_input_frame") or []}
    vis_required = {f["field"] for f in checklist.get("required_fields_visual_panel") or []}
    vis_items = [i["key"] for i in checklist.get("visual_items") or []]
    pack_required = {f["field"] for f in checklist.get("pack_level_required") or []}
    gate_ids = _inv_gate_ids(repo_root)

    # RP-01 包级必填
    miss = [k for k in sorted(pack_required) if k not in pack]
    findings.append(_rule("RP-01", not miss,
                          "包级缺字段: " + ",".join(miss) if miss
                          else "包级 %d 项必填齐备" % len(pack_required)))

    # RP-02 清单里的每一帧都必须有记录（帧清单是分母，不得静默缩）
    recs = {r.get("frame_id"): r for r in (pack.get("frame_records") or [])
            if isinstance(r, dict)}
    absent = [f["frame_id"] for f in frames if f["frame_id"] not in recs]
    extra = [k for k in recs if k not in {f["frame_id"] for f in frames}]
    findings.append(_rule("RP-02", not absent and not extra,
                          ("清单 %d 帧中缺记录 %d 帧，例：%s"
                           % (len(frames), len(absent), absent[:2])) if absent else
                          ("记录里有清单外的帧：%s" % extra[:3]) if extra else
                          "清单 %d 帧逐帧记录齐备（分母=清单，不得以抽样替代）" % len(frames)))

    # RP-03 逐帧必填字段
    bad = []
    for fid, r in recs.items():
        for f in sorted(required):
            # 字段必须在位；取值为 null 是合法语义（ACCEPT 帧的 rejection=null），
            # 但"键缺失"一律判红（缺字段 = 不可复核）。
            if f not in r:
                bad.append("%s: 缺字段 %s" % (fid, f))
        if r.get("verdict") not in FRAME_VERDICTS:
            bad.append("%s: verdict 取值域外 %r" % (fid, r.get("verdict")))
    findings.append(_rule("RP-03", not bad,
                          "%d 处缺项，例：%s" % (len(bad), bad[0]) if bad
                          else "%d 帧必填字段齐备" % len(recs)))

    # RP-04 拒绝块：原因逐字、分类非 unknown、有出口、门 ID 在盘点表内
    bad = []
    n_rej = 0
    for fid, r in recs.items():
        if r.get("verdict") != "REJECT":
            continue
        n_rej += 1
        rj = r.get("rejection")
        if not isinstance(rj, dict):
            bad.append("%s: verdict=REJECT 但缺 rejection 块" % fid)
            continue
        if not (rj.get("reason") or "").strip():
            bad.append("%s: 缺拒绝原因（须逐字引产品日志）" % fid)
        if rj.get("classification") not in REJ_CLASSES:
            bad.append("%s: classification 取值域外 %r" % (fid, rj.get("classification")))
        elif rj.get("classification") == "unknown":
            bad.append("%s: 拒绝分不清是产品行为还是缺陷（unknown 不接受）" % fid)
        if rj.get("exit") not in REJ_EXITS:
            bad.append("%s: 缺 rejection.exit（停工还是登记）" % fid)
        if gate_ids is not None and rj.get("gate_id") and rj["gate_id"] not in gate_ids:
            bad.append("%s: gate_id %s 不在冻结门盘点表内（门须先被盘点）"
                       % (fid, rj["gate_id"]))
    findings.append(_rule("RP-04", not bad,
                          "%d 处不合格，例：%s" % (len(bad), bad[0]) if bad
                          else "%d 条拒绝记录齐备（原因逐字 + 分类 + 出口）" % n_rej))

    # RP-05 目检图块：六项逐项判词 + PNG 指纹
    panels = [p for p in (pack.get("visual_panels") or []) if isinstance(p, dict)]
    bad = []
    for p in panels:
        for f in sorted(vis_required):
            if f not in p:
                bad.append("%s: 缺字段 %s" % (p.get("panel_id"), f))
        items = p.get("items")
        if not isinstance(items, dict):
            bad.append("%s: items 缺" % p.get("panel_id"))
        else:
            for k in vis_items:
                if k not in items:
                    bad.append("%s: 漏判目检项 %s" % (p.get("panel_id"), k))
        if p.get("kind") not in ("full", "tile"):
            bad.append("%s: kind 取值域外 %r" % (p.get("panel_id"), p.get("kind")))
    needs_full = [d for d in (checklist.get("scope") or {}).get("datasets") or []
                  if d.get("key") == pack.get("dataset")]
    if not panels:
        bad.append("visual_panels 为空（至少要交整幅图块）")
    findings.append(_rule("RP-05", not bad,
                          "%d 处不合格，例：%s" % (len(bad), bad[0]) if bad
                          else "%d 个图块六项逐项齐备" % len(panels)))

    # RP-06 证据指针：PNG/日志必须存在且指纹相符
    bad, checked = [], 0
    refs = []
    for p in panels:
        img = p.get("image")
        if isinstance(img, dict) and img.get("path"):
            refs.append((p.get("panel_id"), img))
    for fid, r in recs.items():
        for e in (r.get("evidence") or []):
            refs.append((fid, e))
    for who, ev in refs:
        checked += 1
        if isinstance(ev, str):
            ev = {"path": ev}
        if not isinstance(ev, dict) or not ev.get("path"):
            bad.append("%s: 证据对象缺 path" % who)
            continue
        fp = repo_root / ev["path"]
        if not fp.is_file():
            bad.append("%s: 证据文件不存在 %s" % (who, ev["path"]))
            continue
        if fp.stat().st_size == 0:
            bad.append("%s: 证据文件为空 %s" % (who, ev["path"]))
            continue
        if ev["path"].startswith("run/") and not ev.get("sha256"):
            bad.append("%s: run/ 下证据必须带 sha256 %s" % (who, ev["path"]))
            continue
        if ev.get("sha256") and _sha256_file(fp) != ev["sha256"]:
            bad.append("%s: 证据指纹不符 %s" % (who, ev["path"]))
    findings.append(_rule("RP-06", not bad,
                          "%d/%d 处不合格，例：%s" % (len(bad), checked, bad[0]) if bad
                          else "%d 个证据指针可解析且指纹相符" % checked))

    # RP-07 接缝机器度量：目检项 no_seam 必须配机器逐边度量（不可只靠眼睛）
    sm = pack.get("seam_machine_metrics")
    ok = isinstance(sm, dict) and sm.get("path")
    if ok:
        fp = repo_root / sm["path"]
        ok = fp.is_file() and fp.stat().st_size > 0
    findings.append(_rule("RP-07", bool(ok),
                          "seam_machine_metrics 缺失或文件不可读（无接缝项不得只靠目检背书）"
                          if not ok else "接缝逐边度量已附：%s" % sm.get("path")))

    # RP-08 同二进制性：二进制 + HEAD + CPU 画像三者必须同时登记
    b = pack.get("binary_and_profile") or {}
    ok = all(isinstance(b.get(k), str) and b.get(k) for k in ("binary", "head_sha", "cpu_profile_commit"))
    findings.append(_rule("RP-08", bool(ok),
                          "binary_and_profile 缺 binary/head_sha/cpu_profile_commit（三者缺一则"
                          "同二进制性不可复核）" if not ok
                          else "同二进制性三要素齐备（%s @ %s）"
                          % (b.get("head_sha", "")[:8], (b.get("cpu_profile_commit") or "")[:8])))

    red = [f for f in findings if not f["ok"]]
    return {"verdict": "pass" if not red else "red", "findings": findings,
            "n_red": len(red), "n_frames": len(frames), "n_records": len(recs),
            "n_panels": len(panels), "n_rejected": n_rej}


def _tiny_checklist_and_pack(repo: pathlib.Path):
    png = repo / "run/_rel790_selftest/panel.png"
    png.parent.mkdir(parents=True, exist_ok=True)
    png.write_bytes(b"\x89PNG\r\n\x1a\n" + b"fake")
    log = repo / "run/_rel790_selftest/frame.log"
    log.write_text("WCS 拒绝: 拟合残差 rms_px=0.6554 > 0.5 (DISP-WCS-001 冻结语义, RESCUE F-9)\n",
                   encoding="utf-8")
    ref_png = {"path": str(png.relative_to(repo)), "sha256": _sha256_file(png)}
    ref_log = {"path": str(log.relative_to(repo)), "sha256": _sha256_file(log)}
    cl = {
        "schema": "acsd.rel790_checklist.v1",
        "required_fields_input_frame": [{"field": f} for f in
                                        ("frame_id", "verdict", "n_pairs", "rms_px",
                                         "rejection", "evidence")],
        "required_fields_visual_panel": [{"field": f} for f in
                                         ("panel_id", "kind", "image", "inspector",
                                          "items", "zoom_note", "reject_reason")],
        "pack_level_required": [{"field": f} for f in
                                ("pack_id", "dataset", "binary_and_profile",
                                 "seam_machine_metrics", "coverage_metrics",
                                 "stage_logs", "tier_verdict_record")],
        "visual_items": [{"key": k} for k in ("no_black_hole", "no_bright_spot", "no_seam",
                                              "star_quality", "background_geometry",
                                              "global_impression")],
        "scope": {"datasets": [{"key": "m42"}]},
        "frames": [{"frame_id": "F1", "dataset": "m42"},
                   {"frame_id": "F2", "dataset": "m42"}],
    }
    pack = {
        "pack_id": "REL790-SELFTEST", "dataset": "m42",
        "binary_and_profile": {"binary": "build/acsd", "head_sha": "0" * 40,
                               "cpu_profile_commit": "0" * 40},
        "seam_machine_metrics": {"path": ref_log["path"]},
        "coverage_metrics": {"covered_px": 1, "total_px": 2, "finite_fraction": 0.5},
        "stage_logs": [ref_log],
        "tier_verdict_record": {"path": ref_log["path"]},
        "frame_records": [
            {"frame_id": "F1", "verdict": "REJECT", "n_pairs": 28, "rms_px": 0.6554,
             "rejection": {"gate_id": "F9-WCS-ABS",
                           "reason": "WCS 拒绝: 拟合残差 rms_px=0.6554 > 0.5 "
                                     "(DISP-WCS-001 冻结语义, RESCUE F-9)",
                           "classification": "defect", "exit": "registered",
                           "deterministic_replay": True},
             "evidence": [ref_log]},
            {"frame_id": "F2", "verdict": "ACCEPT", "n_pairs": 38, "rms_px": 0.102,
             "rejection": None, "evidence": [ref_log]},
        ],
        "visual_panels": [{
            "panel_id": "m42-full", "kind": "full", "image": ref_png,
            "inspector": "agent", "zoom_note": None, "reject_reason": None,
            "items": {k: "PASS" for k in ("no_black_hole", "no_bright_spot", "no_seam",
                                          "star_quality", "background_geometry",
                                          "global_impression")},
        }],
    }
    return cl, pack


def _mut(o, fn):
    r = copy.deepcopy(o)
    fn(r)
    return r


NEG_CASES = [
    ("N1 清单里少一帧的记录（静默缩分母）",
     lambda p: p["frame_records"].pop(), "RP-02"),
    ("N2 拒绝没有原因",
     lambda p: p["frame_records"][0]["rejection"].update(reason=""), "RP-04"),
    ("N3 拒绝分不清产品行为还是缺陷",
     lambda p: p["frame_records"][0]["rejection"].update(classification="unknown"), "RP-04"),
    ("N4 目检漏判一项",
     lambda p: p["visual_panels"][0]["items"].pop("no_seam"), "RP-05"),
    ("N5 目检图块文件不存在",
     lambda p: p["visual_panels"][0]["image"].update(path="run/_rel790_selftest/none.png"),
     "RP-06"),
    ("N6 无接缝只靠目检、没有机器逐边度量",
     lambda p: p.pop("seam_machine_metrics"), "RP-07"),
    ("N7 同二进制性缺 CPU 画像指纹",
     lambda p: p["binary_and_profile"].pop("cpu_profile_commit"), "RP-08"),
    ("N8 逐帧缺 rms_px（拒绝判据读数缺失）",
     lambda p: p["frame_records"][0].pop("rms_px"), "RP-03"),
]


def self_test(repo_root: pathlib.Path):
    cl, pack = _tiny_checklist_and_pack(repo_root)
    lines = []
    r = adjudicate(pack, cl, repo_root)
    ok = r["verdict"] == "pass"
    lines.append("[%s] S0 正例 => %s" % ("PASS" if ok else "FAIL", r["verdict"]))
    if not ok:
        for f in r["findings"]:
            if not f["ok"]:
                lines.append("        %s: %s" % (f["rule"], f["detail"]))
    n_ok = 0
    for name, mut, expect in NEG_CASES:
        r2 = adjudicate(_mut(pack, mut), cl, repo_root)
        hit = [f["rule"] for f in r2["findings"] if not f["ok"]]
        good = r2["verdict"] == "red" and expect in hit
        n_ok += int(good)
        lines.append("[%s] %s => %s rules=%s 期望含 %s"
                     % ("PASS" if good else "FAIL", name, r2["verdict"], hit, expect))
    passed = ok and n_ok == len(NEG_CASES)
    lines.append("RP_SELFTEST_%s: 正例=%s 注入负例=%d/%d"
                 % ("PASS" if passed else "FAIL", "1/1" if ok else "0/1", n_ok, len(NEG_CASES)))
    print("\n".join(lines))
    return EXIT_PASS if passed else EXIT_RED


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="REL-790 证据包校验器（RP-01..RP-08）")
    ap.add_argument("--pack")
    ap.add_argument("--checklist", default=str(DEFAULT_CHECKLIST))
    ap.add_argument("--repo-root", default=str(REPO))
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    repo_root = pathlib.Path(a.repo_root).resolve()
    if a.self_test:
        return self_test(repo_root)
    if not a.pack:
        print("用法: --pack <pack.json> | --self-test", file=sys.stderr)
        return EXIT_FAILCLOSED
    try:
        pack = json.loads(pathlib.Path(a.pack).read_text(encoding="utf-8"))
        checklist = json.loads(pathlib.Path(a.checklist).read_text(encoding="utf-8"))
    except Exception as exc:
        print("RP_FAILCLOSED: 证据包/清单不可读: %s" % exc, file=sys.stderr)
        return EXIT_FAILCLOSED
    if not isinstance(pack, dict) or not isinstance(checklist, dict):
        print("RP_FAILCLOSED: 顶层不是对象（门不可判）", file=sys.stderr)
        return EXIT_FAILCLOSED
    res = adjudicate(pack, checklist, repo_root)
    for f in res["findings"]:
        print("  [%s] %s: %s" % ("ok " if f["ok"] else "RED", f["rule"], f["detail"]))
    print("REL790_PACK_%s: 清单帧=%d 记录=%d 图块=%d 拒绝=%d red_rules=%d"
          % (res["verdict"].upper(), res["n_frames"], res["n_records"],
             res["n_panels"], res["n_rejected"], res["n_red"]))
    return EXIT_PASS if res["verdict"] == "pass" else EXIT_RED


if __name__ == "__main__":
    raise SystemExit(main())
