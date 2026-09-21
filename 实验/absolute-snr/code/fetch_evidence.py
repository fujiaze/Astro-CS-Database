#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-B 三重佐证（外部一手证据）抓取与核验：论文 DOI/arXiv、开源项目 文件:行。

网络失败时保留上一次缓存并显式标注 stale=true（不伪造）。
输出：results/evidence_web.json
"""
from __future__ import annotations

import json
import os
import re
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sci_b_common as C  # noqa: E402

UA = {"User-Agent": "AstroCS-SCI-B/1.0 (research evidence check)"}


def fetch(url, timeout=25):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.status, r.read().decode("utf-8", "replace")


def grep_lines(text, pattern, flags=0):
    out = []
    for i, line in enumerate(text.splitlines(), 1):
        if re.search(pattern, line, flags):
            out.append(dict(line=i, text=line.strip()[:220]))
    return out


TARGETS = {
    "pixinsight_doc": dict(
        url="https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html",
        greps={"psfsnr": r"PSFSNR|PSF SNR", "psfsw": r"PSF Signal Weight|PSFSW",
               "eq18": r"\[18\]", "eq16": r"\[16\]", "standard_snr": r"\[20\]"}),
    "crossref_horne1986": dict(url="https://api.crossref.org/works/10.1086/131801", greps={}),
    "crossref_zackay2017": dict(url="https://api.crossref.org/works/10.3847/1538-4357/836/2/187", greps={}),
    "arxiv_1512.06872": dict(url="https://arxiv.org/abs/1512.06872", greps={"title": r"<title>"}),
    "arxiv_1512.06879": dict(url="https://arxiv.org/abs/1512.06879", greps={"title": r"<title>"}),
    "siril_median_and_mean_1_2_4": dict(
        url="https://gitlab.com/free-astro/siril/-/raw/1.2.4/src/stacking/median_and_mean.c",
        greps={"frame_weight": r"pweights\[layer\]\[i\]\s*=|bgnoise", "pscale": r"pscale"}),
    "swarp_coadd": dict(url="https://raw.githubusercontent.com/astromatic/swarp/master/src/coadd.c",
                        greps={"weighted_mean": r"weight", "variance": r"var"}),
    "photutils_errors": dict(url="https://raw.githubusercontent.com/astropy/photutils/main/photutils/utils/errors.py",
                             greps={"calc_total_error": r"def calc_total_error|effective_gain|bkg_error"}),
}


def main():
    out = {"generated_at": C.now(), "entries": {}}
    cache_path = os.path.join(C.RESULTS, "evidence_web.json")
    old = {}
    if os.path.exists(cache_path):
        try:
            old = json.load(open(cache_path, encoding="utf-8")).get("entries", {})
        except Exception:
            old = {}
    for key, t in TARGETS.items():
        entry = dict(url=t["url"], ok=False, stale=False)
        try:
            status, text = fetch(t["url"])
            entry.update(status=status, bytes=len(text), greps={})
            for gk, pat in t.get("greps", {}).items():
                entry["greps"][gk] = grep_lines(text, pat)[:6]
            if key.startswith("crossref"):
                msg = json.loads(text)["message"]
                entry["meta"] = dict(title=msg.get("title"), container=msg.get("container-title"),
                                     volume=msg.get("volume"), page=msg.get("page"),
                                     year=(msg.get("published-print") or msg.get("published") or {}).get(
                                         "date-parts", [[None]])[0][0],
                                     DOI=msg.get("DOI"))
            entry["ok"] = (status == 200)
        except Exception as e:  # 网络不可用时保留缓存并标注 stale
            entry["error"] = str(e)[:300]
            if key in old:
                entry.update(old[key]); entry["stale"] = True
        out["entries"][key] = entry
        print("%-28s ok=%s stale=%s" % (key, entry.get("ok"), entry.get("stale")), flush=True)
    # 汇总核验判据
    e = out["entries"]
    out["verdict"] = dict(
        pixinsight_resolvable=bool(e["pixinsight_doc"].get("ok")),
        pixinsight_psfsnr_found=bool(e["pixinsight_doc"].get("greps", {}).get("psfsnr")),
        pixinsight_psfsw_found=bool(e["pixinsight_doc"].get("greps", {}).get("psfsw")),
        horne_doi_resolvable=bool(e["crossref_horne1986"].get("ok")),
        zackay_doi_resolvable=bool(e["crossref_zackay2017"].get("ok")),
        arxiv_1512_06872_resolvable=bool(e["arxiv_1512.06872"].get("ok")),
        arxiv_1512_06879_resolvable=bool(e["arxiv_1512.06879"].get("ok")),
        siril_source_resolvable=bool(e["siril_median_and_mean_1_2_4"].get("ok")),
        swarp_source_resolvable=bool(e["swarp_coadd"].get("ok")),
        photutils_source_resolvable=bool(e["photutils_errors"].get("ok")),
    )
    C.save_json(cache_path, out)
    print("wrote", cache_path)
    print(json.dumps(out["verdict"], indent=1))


if __name__ == "__main__":
    main()
