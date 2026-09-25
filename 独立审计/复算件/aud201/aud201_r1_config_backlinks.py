"""AUD-201 R1: check defaults.json photometry.* authority back-links against
the actual lines of docs/science/PHOTOMETRY.md they cite.

Read-only. Writes a report next to this script. Deterministic, no repo mutation.
Usage:  python -B aud201_r1_config_backlinks.py <repo_root>
"""
import io
import json
import os
import sys

REPO = sys.argv[1] if len(sys.argv) > 1 else r"F:\Astro dev\Astro CS Normalization Database"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "aud201_r1_config_backlinks.out")


def line_of(relpath, ln):
    p = os.path.join(REPO, relpath.replace("/", os.sep))
    if not os.path.isfile(p) or not ln:
        return None
    with io.open(p, encoding="utf-8") as f:
        doc = f.read().split("\n")
    if 0 < ln <= len(doc):
        return doc[ln - 1].strip()
    return "<out-of-range: file has %d lines>" % len(doc)


def main():
    with io.open(os.path.join(REPO, "eng", "packaging", "config", "defaults.json"),
                 encoding="utf-8") as f:
        defaults = json.load(f)

    keys = []

    def walk(o):
        if isinstance(o, dict):
            k = o.get("key")
            if isinstance(k, str) and k.startswith("photometry."):
                keys.append(o)
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)

    walk(defaults)

    line_map = {
        "mag_tolerance": "mag_tolerance",
        "tukey_c": "4.685",
        "irls_max_iterations": "max_iter=50",
        "irls_tolerance_dex": "tol=1e-6",
        "min_reference_stars": "|r_consistent|>=3",
        "min_inlier_stars": "|r_inliers|>=2",
    }

    lines = []
    lines.append("photometry.* entries declared in defaults.json: %d" % len(keys))
    lines.append("")
    n_ok = n_bad = 0
    for e in keys:
        sym = e["key"].split(".")[-1]
        sr = e.get("source_ref") or {}
        rel, ln = sr.get("path"), sr.get("line")
        actual = line_of(rel, ln) if rel else None
        needle = line_map.get(sym, sym)
        hit = bool(actual) and needle in actual
        if hit:
            n_ok += 1
        else:
            n_bad += 1
        lines.append("%-34s value=%-8s unit=%-10s status=%-9s cited=%s:%s" % (
            e["key"], e.get("value"), e.get("unit"),
            e.get("authority_status"), rel, ln))
        lines.append("    cited-line content : %s" % (actual,))
        lines.append("    expected-token     : %r  -> BACKLINK_%s" % (needle, "OK" if hit else "MISMATCH"))
        # locate where the token actually lives
        locs = []
        if rel:
            p = os.path.join(REPO, rel.replace("/", os.sep))
            with io.open(p, encoding="utf-8") as f:
                for i, t in enumerate(f.read().split("\n"), 1):
                    if needle in t and len(locs) < 4:
                        locs.append(i)
        lines.append("    token actually at lines: %s" % locs)
        lines.append("")
    lines.append("SUMMARY: backlink OK = %d, MISMATCH = %d, total = %d" % (n_ok, n_bad, len(keys)))

    # ---- degeneracy / discriminative self-test -------------------------------
    # A predicate that always answers the same way has no evidence value
    # (AGENTS.md §5 "判据必须非退化"). Feed the SAME predicate two different
    # citation inputs and require it to answer differently:
    #   input A = the line the registry actually cites   -> verdict_cited
    #   input B = the line the token really lives on     -> verdict_actual
    # verdict_cited must be True at least once (predicate is satisfiable) and
    # False at least once (predicate is falsifiable). If both are always equal
    # for every key, the check is vacuous and must not be trusted.
    lines.append("")
    lines.append("SELF-TEST (non-degeneracy of the back-link predicate):")
    pairs = []
    for e in keys:
        sym = e["key"].split(".")[-1]
        needle = line_map.get(sym, sym)
        sr = e.get("source_ref") or {}
        p = os.path.join(REPO, sr.get("path", "").replace("/", os.sep)) if sr.get("path") else ""
        actual_lines = []
        if p and os.path.isfile(p):
            with io.open(p, encoding="utf-8") as f:
                for i, t in enumerate(f.read().split("\n"), 1):
                    if needle in t:
                        actual_lines.append(i)
        b_line = actual_lines[0] if actual_lines else None
        verdict_b = bool(line_of(sr.get("path"), b_line)) and needle in line_of(sr.get("path"), b_line)
        verdict_a = needle in (line_of(sr.get("path"), sr.get("line")) or "")
        pairs.append((sym, sr.get("line"), b_line, verdict_a, verdict_b))
        lines.append("  %-26s cited=%-5s actual=%-5s verdict_cited=%-5s verdict_actual=%-5s %s" % (
            sym, sr.get("line"), b_line, verdict_a, verdict_b,
            "DIVERGES" if verdict_a != verdict_b else "same"))
    n_cited_true = sum(1 for p in pairs if p[3])
    n_diverge = sum(1 for p in pairs if p[3] != p[4])
    nd = (n_cited_true > 0) and (n_cited_true < len(pairs)) or n_diverge > 0
    lines.append("  satisfiable(>=1 cited True)=%s  falsifiable(>=1 cited False)=%s  diverging=%d" % (
        n_cited_true > 0, n_cited_true < len(pairs), n_diverge))
    lines.append("  VERDICT: %s" % ("PREDICATE NON-DEGENERATE" if nd else "PREDICATE DEGENERATE"))

    # ---- cross-document negative control ------------------------------------
    # Feed the SAME predicate a deliberately *out-of-source* document group
    # (docs/science/CALIBRATION.md, a different authority doc, same line numbers).
    # A predicate that still says mostly OK here is measuring nothing.
    alt_doc = "docs/science/CALIBRATION.md"
    alt_hits = 0
    for e in keys:
        sym = e["key"].split(".")[-1]
        needle = line_map.get(sym, sym)
        sr = e.get("source_ref") or {}
        a = line_of(alt_doc, sr.get("line"))
        if a and needle in a:
            alt_hits += 1
    lines.append("")
    lines.append("NEGATIVE CONTROL (same predicate, out-of-source doc %s): %d/%d keys match"
                 " -> %s" % (alt_doc, alt_hits, len(keys),
                             "PASS (wrong source correctly reported as not-authoritative)"
                             if alt_hits < n_cited_true else
                             "FAIL (predicate cannot tell sources apart)"))
    text = "\n".join(lines)
    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text.encode("ascii", "replace").decode("ascii"))


if __name__ == "__main__":
    main()
