import collections
import re

# Per-family site census for the constant families that carry scientific meaning.
# Output: aud402_families.tsv  (family, value, sites, files, first-3 sites)
SRC = r"独立审计/证据/scan402\cpp_hits.tsv"
OUT = r"独立审计/证据/scan402\aud402_families.tsv"
DROP = ("/tests/", "/test/", "/tools/", "memory.md", "README", "test_", "_test", "oracle",
        "bench", "probe", "fixture", "/examples/")
FAMILIES = {
    "pi": r"\b3\.14159265358979",
    "ln10": r"\b2\.3025850929",
    "mad_to_sigma": r"\b1\.48260?2?2?1?8?5?0?5?6?0?2?\b|\b1\.4826\b",
    "quartile_scale": r"\b0\.67448975",
    "gauss_fwhm_sigma": r"\b2\.3548200450309493\b|\b2\.3548\b",
    "moffat4_fwhm_sigma": r"\b1\.230310\b",
    "trim_mean_to_sigma": r"\b0\.731672",
    "normal_1sigma_mass": r"\b0\.682689\b|\b0\.317311\b",
    "deg_rad_180": r"\b180\.0\b",
    "deg_circle_360": r"\b360\.0\b",
    "deg_arcsec_3600": r"\b3600\.0\b",
    "arcmin_60": r"\b60\.0\b",
    "hour_deg_15": r"\b15\.0\b",
    "tan_pole_cut_85": r"\b85\.0\b",
    "pix_half_offset": r"\b0\.5f?\b",
    "fits_null_sentinel": r"-999\.0\b",
    "uint16_max": r"65535\.0f?\b",
    "uint8_max": r"\b255\.0f?\b",
    "ivar_zero_guard": r"\b1e-12\b|\b1e-12f\b",
    "closure_tol_1e6": r"\b1e-6\b|\b1\.0e-6\b",
    "fp32_eps_like": r"\b1e-7\b|\b1\.19e-7\b|\b1e-9\b|\b1\.0e-9\b",
    "memory_budget_95": r"\b95u?\b(?=[^0-9])",
    "frame_bytes_per_px_116": r"\b116\.0\b",
    "safety_frac_075": r"\b0\.75f?\b",
    "scratch_pool_cap_2": r"kScratchPoolCap",
    "pixfrac_08": r"\b0\.8f?\b",
    "kcorr_14": r"\b1\.4f?\b",
    "tukey_4685": r"\b4\.685\b",
    "huber_1345": r"\b1\.345\b",
    "clip_30": r"\b3\.0f?\b",
    "min_retained_fraction_060": r"\b0\.60f?\b|\b0\.6f\b",
    "sigma_clip_50": r"\b5\.0f?\b",
}
hits = collections.defaultdict(list)
for line in open(SRC, encoding="utf-8").read().split("\n")[1:]:
    p = line.split("\t")
    if len(p) < 9:
        continue
    f, ln, cls, name, assigned, values, ncount, syn, text = p[:9]
    if any(d in f for d in DROP):
        continue
    code = re.split(r"(?<!:)//", text)[0]
    for fam, pat in FAMILIES.items():
        if re.search(pat, code):
            hits[fam].append((f, ln, name, assigned, text[:150]))
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    fh.write("family\tsites\tfiles\tfirst_sites\n")
    for fam in sorted(hits, key=lambda k: -len(hits[k])):
        hs = hits[fam]
        files = {h[0] for h in hs}
        fh.write(f"{fam}\t{len(hs)}\t{len(files)}\t"
                 + " ;; ".join(f"{h[0]}:{h[1]}[{h[2]}={h[3]}]" for h in hs[:4]) + "\n")
print("families with hits:", len(hits), "of", len(FAMILIES))
for fam in sorted(hits, key=lambda k: -len(hits[k])):
    print(f"  {fam:24s} sites={len(hits[fam]):5d} files={len({h[0] for h in hits[fam]}):4d}")
missing = set(FAMILIES) - set(hits)
print("ZERO-HIT families (must re-check the pattern, not report 0):", sorted(missing))
