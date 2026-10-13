#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""阶段一端到端重跑（本批次总验收）：逐项核验，只读。

口径修正（实测确认）：
  · 产品面 = `<out>/{t2,t3}/`：逐帧 HiPS 目录（signal/support/variance/ivar +
    manifest.json + p1_stack.json）+ `p1_products.json` + 运行级记账；
  · 工作区 = `<out>/{t2,t3}.p1-work/`：13 类中间产物（含 p1_snr/p1_phot/p1_sources/p1_psf/
    p1_flux.json 与 <FRAME>/{p1_final.json,p1_wcs.json}）；
  · `sigma_location_se_dex` 在 **`p1_snr.json`**（不在 `p1_phot.json`）；
  · 逐帧几何在**帧内清单 `manifest.json` 的 `geometry`**。
"""
from __future__ import annotations

import json
import re
import statistics as st
from pathlib import Path

ROOT = Path("/workspace/Astro CS Database")
UNIT = ROOT / "实验/photometric-magnitude"
OUT = ROOT / "run/PHOT-FIX/p1_rerun_final"
CFG = ROOT / "run/PHOT-FIX/p1_rerun_final_config.json"
RES = UNIT / "results/p1_rerun_final"
LOG = OUT / "logs/normalize.log"
SAMPLE = 6


def main() -> int:
    RES.mkdir(parents=True, exist_ok=True)
    res = {}
    cfg = json.loads(CFG.read_text(encoding="utf-8"))

    # ---------- 1 帧集与逐帧计数
    frames = {}
    for b in cfg["blocks"]:
        sub = "t2" if "t2" in b["name"] else "t3"
        for p in b["input_lights"]:
            m = re.search(r"(M\d)_(T\d)_flying_dutchman-(\d{8})@(\d{6})", Path(p).name)
            frames["M42_%s_%s_flying_dutchman-%s_%s-300S-Red" % m.groups()] = sub
    res["n_expected"] = len(frames)
    per = {}
    for sub in ("t2", "t3"):
        d = OUT / sub
        dirs = sorted(p.name for p in d.iterdir() if p.is_dir() and p.name.startswith("M42_")) if d.exists() else []
        per[sub] = dict(n=len(dirs), dirs=dirs,
                        missing=sorted(k for k, s in frames.items() if s == sub and k not in dirs),
                        extra=sorted(k for k in dirs if k not in frames))
    res["per_block"] = per
    res["n_frame_dirs"] = sum(v["n"] for v in per.values())

    # ---------- 2 产品面 vs 工作区
    def kinds(d):
        if not d.exists():
            return []
        out = set()
        for p in d.iterdir():
            n = p.name
            if n.startswith("M42_"):
                out.add("<FRAME>/" if p.is_dir() else "<FRAME>")
            else:
                out.add(re.sub(r"M42_M\d_T\d_flying_dutchman-\d{8}@\d{6}-\d+S-Red", "<FRAME>", n))
        return sorted(out)

    surf, work = {}, {}
    for sub in ("t2", "t3"):
        surf[sub] = dict(entries=sorted(p.name for p in (OUT / sub).iterdir())
                         if (OUT / sub).exists() else [])
        wk = OUT / (sub + ".p1-work")
        work[sub] = dict(entries=kinds(wk),
                         n_entries=len(list(wk.iterdir())) if wk.exists() else 0)
        fd = [p for p in wk.iterdir() if p.is_dir() and p.name.startswith("M42_")] if wk.exists() else []
        work[sub]["frame_dir_entries"] = sorted(p.name for p in fd[0].iterdir()) if fd else []
    res["product_surface"] = surf
    res["workspace"] = work
    mid = {"p1_snr.json", "p1_phot.json", "p1_sources.json", "p1_psf.json", "p1_flux.json",
           "badcol_report.json"}
    res["product_surface_has_intermediates"] = sorted(
        {e for sub in surf for e in surf[sub]["entries"]} & mid)
    res["workspace_kind_count"] = {sub: len(work[sub]["entries"]) for sub in work}

    # ---------- 3 帧目录内容 + 几何
    geo_ok, geo_bad = [], []
    for sub in ("t2", "t3"):
        for fk in per[sub]["dirs"]:
            fd = OUT / sub / fk
            ent = sorted(p.name for p in fd.iterdir())
            need = {"signal", "support", "variance", "ivar", "manifest.json", "p1_stack.json"}
            if not need.issubset(set(ent)):
                geo_bad.append(dict(frame=fk, missing=sorted(need - set(ent))))
                continue
            man = json.loads((fd / "manifest.json").read_text(encoding="utf-8"))
            g = man.get("geometry") or {}
            w = OUT / (sub + ".p1-work") / fk / "p1_wcs.json"
            ws = json.loads(w.read_text(encoding="utf-8")).get("wcs", {}) if w.exists() else {}
            same = all(g.get(k) is not None and abs(float(g[k]) - float(ws[k])) <= 1e-12
                       for k in ("crval1", "crval2", "crpix1", "crpix2", "cd11", "cd12", "cd21", "cd22")
                       if ws.get(k) is not None)
            sip = bool((g.get("sip") or {}).get("a"))
            geo_ok.append(dict(frame=fk, geometry_present=True, matches_solved=same, sip=sip))
    res["frame_dir_audit"] = dict(n_ok=len(geo_ok), n_bad=len(geo_bad), bad=geo_bad,
                                  all_have_geometry=all(x["geometry_present"] for x in geo_ok),
                                  all_match_solved=all(x["matches_solved"] for x in geo_ok),
                                  all_have_sip=all(x["sip"] for x in geo_ok))

    # ---------- 3b 抽样 provenance
    prov = []
    for sub in ("t2", "t3"):
        for fk in per[sub]["dirs"][:SAMPLE // 2]:
            man = json.loads((OUT / sub / fk / "manifest.json").read_text(encoding="utf-8"))
            prov.append(dict(block=sub, frame=fk,
                             geometry_crval=[man["geometry"]["crval1"], man["geometry"]["crval2"]],
                             geometry_ctype=[man["geometry"]["ctype1"], man["geometry"]["ctype2"]]))
    res["provenance_samples"] = prov

    # ---------- 通带身份
    pb = {}
    for sub in ("t2", "t3"):
        p = json.loads((OUT / (sub + ".p1-work") / "p1_phot.json").read_text(encoding="utf-8"))
        pi = p.get("passband_identity", {})
        q = pi.get("qe") or {}
        pb[sub] = dict(declared_filter=pi.get("declared_filter_passband"),
                       resolved_filter=pi.get("resolved_library_key"),
                       filter_source=pi.get("curve_source"), filter_n=pi.get("n_points"),
                       gate=pi.get("gate"),
                       declared_qe=q.get("declared_qe_name"), resolved_qe=q.get("resolved_library_key"),
                       qe_source=q.get("curve_source"), qe_n=q.get("n_points"))
    res["passband_identity"] = pb

    # ---------- 4 m5 + 5 se
    for sub in ("t2", "t3"):
        s = json.loads((OUT / (sub + ".p1-work") / "p1_snr.json").read_text(encoding="utf-8"))
        m5 = [f.get("frame_depth_m5_mag") for f in s["frames"]]
        se = [f.get("sigma_location_se_dex") for f in s["frames"]]
        stt = sorted({f.get("sigma_location_se_status") for f in s["frames"]})
        mv = [v for v in m5 if v is not None]
        res.setdefault("m5", {})[sub] = dict(
            n=len(m5), n_nonnull=len(mv),
            reference_flux_source=s.get("reference_flux_source"),
            snr_reference_scope=s.get("snr_reference_scope"),
            reference_zero_point_syn_mag=s.get("reference_zero_point_syn_mag"),
            snr_caliber=s.get("snr_caliber"),
            median=st.median(mv) if mv else None,
            min=min(mv) if mv else None, max=max(mv) if mv else None)
        sv = [v for v in se if v is not None]
        res.setdefault("sigma_location_se_dex", {})[sub] = dict(
            n=len(se), n_nonnull=len(sv), n_nonzero=sum(1 for v in sv if v != 0.0),
            median=st.median(sv) if sv else None, min=min(sv) if sv else None,
            max=max(sv) if sv else None, statuses=stt)

    # ---------- 7 帧级失败
    fails = {}
    for sub in ("t2", "t3"):
        p = json.loads((OUT / (sub + ".p1-work") / "p1_phot.json").read_text(encoding="utf-8"))
        s = json.loads((OUT / (sub + ".p1-work") / "p1_snr.json").read_text(encoding="utf-8"))
        fails[sub] = dict(phot_failed_frames=p.get("failed_frames"),
                          phot_n_failed=p.get("n_frames_failed"),
                          phot_n_ok=p.get("n_frames_ok"),
                          snr_degraded=[dict(file=f.get("file"), reason=f.get("snr_degraded_reason"),
                                             status=f.get("snr_catalogue_status"))
                                        for f in s["frames"]
                                        if f.get("snr_degraded_reason") or f.get("snr_catalogue_status") != "ok"])
    res["frame_level_failures"] = fails

    # ---------- 6 时长 / RSS / 8 smoke
    if LOG.exists():
        t = LOG.read_text(encoding="utf-8", errors="replace")
        res["log"] = dict(
            guard_cabs_pair=len(re.findall(r"C_abs 二元组守卫", t)),
            fail_closed=len(re.findall(r"fail-closed", t)),
            peak_rss_gb=(re.findall(r"peak_rss_gb=([0-9.]+)", t) or [None])[-1],
            exit_codes=re.findall(r'"exit_code":(\d+)', t))
    res["self_describing_smoke"] = dict(
        frame_set_from_products=sorted(per["t2"]["dirs"] + per["t3"]["dirs"])[:3],
        n_from_products=res["n_frame_dirs"],
        geometry_from_products={x["frame"]: x["geometry_crval"] for x in prov[:2]},
        p1_products_frame_count={
            sub: len(json.loads((OUT / sub / "p1_products.json").read_text(encoding="utf-8")).get("frames", []))
            for sub in ("t2", "t3")})

    (RES / "acceptance.json").write_text(json.dumps(res, indent=2, ensure_ascii=False, default=float),
                                         encoding="utf-8")
    print("① 帧数: 期望 %d  实际 %d  (t2 %d + t3 %d)" % (
        res["n_expected"], res["n_frame_dirs"], per["t2"]["n"], per["t3"]["n"]))
    print("   缺失:", per["t2"]["missing"] + per["t3"]["missing"],
          " 多出:", per["t2"]["extra"] + per["t3"]["extra"])
    print("② 产品面含中间产物:", res["product_surface_has_intermediates"] or "无 ✓")
    print("   工作区种类数:", res["workspace_kind_count"])
    for sub in ("t2", "t3"):
        print("   %s 工作区种类: %s" % (sub, res["workspace"][sub]["entries"]))
    print("③ 帧目录审计:", {k: v for k, v in res["frame_dir_audit"].items() if k != "bad"})
    print("   通带:", json.dumps(pb, ensure_ascii=False)[:320])
    print("④ m5:", {s: (v["n"], v["n_nonnull"], v["reference_flux_source"]) for s, v in res["m5"].items()})
    print("⑤ se:", {s: (v["n_nonzero"], v["statuses"], v["median"]) for s, v in res["sigma_location_se_dex"].items()})
    print("⑥ 日志:", res.get("log"))
    print("⑦ 帧级失败:", {s: (v["phot_n_failed"], len(v["snr_degraded"])) for s, v in fails.items()})
    print("⑧ smoke:", res["self_describing_smoke"]["p1_products_frame_count"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
