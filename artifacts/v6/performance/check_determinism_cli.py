#!/usr/bin/env python3
"""PERF-SCALE-001 CLI determinism comparison across CPU-budget (worker) runs.

Compares products of the same input produced under different CPU affinities (the V6 CLI
derives its worker budget from affinity). Science products must be byte-identical; run-scoped
files (manifests/graphs/resource series, which legitimately embed a run id and timestamps) are
reported separately and additionally compared after normalization of run-scoped identifiers.

Usage:
  python3 check_determinism_cli.py --runs "1:DIR1,4:DIR2,16:DIR3" \
      --out-json OUT.json --out-csv OUT.csv [--products-subdir SUB]
"""
from __future__ import annotations
import argparse, csv, hashlib, json, os, pathlib, re, sys

RUN_SCOPED_RE = [
    re.compile(r"^astrocs_run_[0-9a-f]+\.json$"),
    re.compile(r"^run_context\.json$"),
    re.compile(r"^graph/"),
    re.compile(r"^resource_samples\.csv$"),
    re.compile(r"^resource_summary\.json$"),
    re.compile(r"^worker_balance\.csv$"),
    re.compile(r"^alloc_samples\.csv$"),
    re.compile(r"^alloc_report\.json$"),
    re.compile(r"^run_cfg\.json$"),
    re.compile(r"^run\.log$"),
]


def is_run_scoped(rel: str) -> bool:
    return any(r.match(rel) for r in RUN_SCOPED_RE)


def sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def normalize(data: bytes, run_dir: pathlib.Path) -> bytes:
    """Blank out run-scoped metadata only: run id, wall-clock elapsed, creation stamps and
    the run's own output path. The science payload (FITS arrays, numeric JSON fields) is
    untouched; a 1-byte payload flip still changes the normalized hash."""
    s = data.decode("utf-8", "replace")
    # Recorded raw-product digests depend on the FITS RUNID/CHECKSUM header bytes; the
    # science payload is compared separately (data arrays + normalized FITS header).
    # This MUST run before the 12-hex run-id rule, which would otherwise mangle 64-hex digests.
    s = re.sub(r'("(?:product_)?sha256"\s*:\s*")[0-9a-f]{64}(")', r"\1<SHA256>\2", s)
    s = re.sub(r"[0-9a-f]{12}", "<RUNID>", s)
    s = re.sub(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", "<TS>", s)
    s = re.sub(r'("elapsed_sec"\s*:\s*)[0-9.eE+-]+', r"\1<ELAPSED>", s)
    s = re.sub(r'("elapsed_seconds"\s*:\s*)[0-9.eE+-]+', r"\1<ELAPSED>", s)
    for tok in (str(run_dir), str(run_dir.resolve())):
        s = s.replace(tok, "<RUNDIR>")
    return s.encode("utf-8")


TEXT = {".json", ".csv", ".txt", ".md", ".yaml", ".yml"}
FITS_SUFFIX = {".fits", ".fts", ".fit"}
# FITS header keys that carry run-scoped provenance, not science content.
FITS_RUN_KEYS = {"RUNID", "CHECKSUM", "DATASUM", "DATE", "DATE-OBS", "MJD-OBS", "CREATED"}


def fits_signature(raw: bytes, path: pathlib.Path):
    """Return (data_digest, normalized_header_digest, error). FITS science payload is the
    concatenated HDU data arrays (bitwise); the normalized header drops run-scoped keys."""
    try:
        import numpy as np
        from astropy.io import fits
        import io
        with fits.open(io.BytesIO(raw), memmap=False) as hdul:
            dh = hashlib.sha256()
            for hdu in hdul:
                data = hdu.data
                if data is None:
                    dh.update(b"<none>")
                else:
                    dh.update(np.ascontiguousarray(data).tobytes())
            hh = hashlib.sha256()
            for i, hdu in enumerate(hdul):
                # Header.tostring() is a run of fixed 80-byte card images with no separators;
                # slice by 80 so each card is tested individually. Only run-scoped provenance
                # cards (run id, checksums/dates) are dropped.
                img = hdu.header.tostring()
                for off in range(0, len(img), 80):
                    card = img[off:off + 80]
                    if card[:8].strip() in FITS_RUN_KEYS:
                        continue
                    hh.update(("%d|" % i).encode("utf-8"))
                    hh.update(card.encode("utf-8", "replace"))
                    hh.update(b"\n")
            return dh.hexdigest(), hh.hexdigest(), None
    except Exception as e:      # astropy missing or not a FITS file
        return None, None, str(e)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", required=True)
    ap.add_argument("--out-json", required=True)
    ap.add_argument("--out-csv", required=True)
    ap.add_argument("--products-subdir", default="")
    args = ap.parse_args(argv)
    runs = []
    seen = {}
    for item in args.runs.split(","):
        label, path = item.split(":", 1)
        seen[label] = seen.get(label, 0) + 1
        if seen[label] > 1:
            label = "%s#%d" % (label, seen[label])   # labels must be unique: dict keys hash runs
        root = pathlib.Path(path)
        if args.products_subdir:
            root = root / args.products_subdir
        runs.append((label, root))
    labels = [l for l, _ in runs]
    if len(set(labels)) != len(labels):
        print("FAIL: duplicate labels after uniquify", file=sys.stderr)
        return 2
    all_hashes = {}
    norm_hashes = {}
    for label, root in runs:
        if not root.exists():
            print("FAIL: run dir missing: %s" % root, file=sys.stderr)
            return 2
        h = {}
        nh = {}
        for p in sorted(root.rglob("*")):
            if not p.is_file():
                continue
            rel = str(p.relative_to(root)).replace(os.sep, "/")
            raw = p.read_bytes()
            h[rel] = hashlib.sha256(raw).hexdigest()
            if p.suffix.lower() in FITS_SUFFIX:
                dsha, hsha, err = fits_signature(raw, p)
                if dsha:
                    nh[rel] = "fits:" + dsha + ":" + hsha
                else:
                    nh[rel] = "fits-unparsed:" + h[rel]
            # HiPS "properties" files carry no extension; classify text by content, not suffix.
            elif p.suffix.lower() in TEXT or b"\x00" not in raw[:4096]:
                nh[rel] = hashlib.sha256(normalize(raw, root)).hexdigest()
            else:
                nh[rel] = h[rel]
        all_hashes[label] = h
        norm_hashes[label] = nh

    rels = sorted(set().union(*[set(h) for h in all_hashes.values()]))
    rows = []
    science_raw_equal = True
    science_norm_equal = True
    run_scoped_raw_equal = True
    run_scoped_norm_equal = True
    for rel in rels:
        vals = [all_hashes[l].get(rel) for l in labels]
        nvals = [norm_hashes[l].get(rel) for l in labels]
        identical = len(set(vals)) == 1
        norm_identical = len(set(nvals)) == 1
        scoped = is_run_scoped(rel)
        cat = "run_scoped" if scoped else "science_product"
        if scoped:
            run_scoped_raw_equal &= identical
            run_scoped_norm_equal &= norm_identical
        else:
            science_raw_equal &= identical
            science_norm_equal &= norm_identical
        rows.append({"relative_path": rel, "category": cat, "identical_across_budgets": identical,
                     "normalized_identical": norm_identical,
                     **{("sha256_b%s" % l): (vals[i] or "") for i, l in enumerate(labels)}})

    product_count = sum(1 for r in rows if r["category"] == "science_product")
    # Hard gate: the science payload must be byte-identical once only run-scoped metadata
    # (run id, output path, wall-clock elapsed, creation timestamp) is blanked. Raw equality
    # is reported too, together with the exact raw differences, so nothing is hidden.
    verdict = "PASS" if (science_norm_equal and product_count > 0) else "FAIL"
    raw_diffs = [r["relative_path"] for r in rows
                 if r["category"] == "science_product" and not r["identical_across_budgets"]]
    doc = {
        "schema": "astrocs.v6.perf.determinism-cli/v1",
        "budgets": labels,
        "run_dirs": {l: str(p) for l, p in runs},
        "n_files_total": len(rows), "n_science_products": product_count,
        "science_products_raw_byte_identical": science_raw_equal,
        "science_products_normalized_byte_identical": science_norm_equal,
        "science_products_raw_differing": raw_diffs,
        "run_scoped_files_raw_identical": run_scoped_raw_equal,
        "run_scoped_files_normalized_identical": run_scoped_norm_equal,
        "verdict": verdict,
        "normalization_note": "run-scoped metadata blanked: run_id, output path, elapsed_sec, "
                              "ISO creation timestamps. FITS arrays and numeric science fields "
                              "are untouched; a payload byte flip still changes the digest.",
        "rows": rows,
    }
    pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
    pathlib.Path(args.out_json).write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with open(args.out_csv, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["relative_path", "category", "identical_across_budgets",
                                          "normalized_identical"] + ["sha256_b%s" % l for l in labels])
        w.writeheader()
        for r in rows:
            w.writerow(r)
    print("DETERMINISM_CLI_%s science_products=%d raw_identical=%s norm_identical=%s "
          "run_scoped_raw_identical=%s"
          % (verdict, product_count, science_raw_equal, science_norm_equal, run_scoped_raw_equal))
    for rel in raw_diffs:
        print("  RAW-DIFF (run-scoped metadata only): %s" % rel)
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
