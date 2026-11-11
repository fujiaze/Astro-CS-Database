#!/usr/bin/env python3
"""Independent audit script (read-only): Normalize product contents.

Run from the repo root: python3 run/m42-s1/review/independent/verify_product.py
"""
import glob
import json
import os

import numpy as np
from astropy.io import fits

ROOT = "/workspace/Astro CS Database"
STG = f"{ROOT}/run/m42-s1/staging/smoke256sig"
PROD = f"{STG}/out/M42_M5_T2_flying_dutchman-20251212_041842-600S-H-alpha"


def manifest():
    print("== manifest / frame_science ==")
    man = json.load(open(f"{PROD}/manifest.staging.json"))
    fs = json.load(open(f"{PROD}/frame_science.json"))
    print(" manifest assessment:", man["assessment"])
    print(" frame_science assessment:", fs["data"]["assessment"])
    print(" capabilities:", [c for c in man["capabilities"] if "omitted" in c or c.startswith("acsd.SupportMap")])


def objects():
    print("\n== signal / support / action ==")
    sig = sig = json.load(open(glob.glob(f"{PROD}/objects/acsd.SignalEstimate*.json")[0]))
    sup = json.load(open(glob.glob(f"{PROD}/objects/acsd.SupportMap*.json")[0]))
    idx = json.load(open(glob.glob(f"{PROD}/objects/acsd.Index*.json")[0]))
    act = json.load(open(glob.glob(f"{PROD}/objects/acsd.LinearAction*.json")[0]))
    blocks = {b["id"]: b for b in man_blocks()} if False else {}
    print(" signal shape:", [b["shape"] for b in json.load(open(f"{PROD}/manifest.staging.json"))["blocks"]
                             if b["id"] == sig["data"]["signal"]["$ref"]])
    print(" support index size:", idx["data"]["size"], "kind:", sup["data"]["kind"],
          "primitives:", sup["data"]["primitives"])
    arrays = act["data"]["arrays"]
    print(" LinearAction arrays:", arrays)


def man_blocks():
    return []


def tiles():
    print("\n== published tiles (sample) ==")
    files = sorted(glob.glob(f"{PROD}/signal/Norder13/*/*.fits"))
    print(" n tile files:", len(files))
    for f in files[:1]:
        with fits.open(f, memmap=True, ignore_missing_end=True) as h:
            d = h[0].data
            print(" sample", d.shape, d.dtype, os.path.getsize(f), "B")
            print(" finite first 5 planes:", [int(np.isfinite(d[i]).sum()) for i in range(5)])
    # the three dirs named in the executor report
    for name in ("Dir0", "Dir130000", "Dir20000"):
        f = glob.glob(f"{PROD}/signal/Norder13/{name}/*.fits")[0]
        with fits.open(f, memmap=True, ignore_missing_end=True) as h:
            d = h[0].data
            idx = [0, 2500, 5000, 7500, 9999]
            print(f" {name}: finite@ {idx} = {[int(np.isfinite(d[i]).sum()) for i in idx]}")
    raw = open(glob.glob(f"{PROD}/signal/Norder13/Dir0/*.fits")[0], "rb").read(2880 * 2)
    cards = [raw[i:i + 80].decode("latin1").rstrip() for i in range(0, len(raw), 80)]
    bad = [c for c in cards if c.startswith("ACSUNIT")]
    print(" ACSUNIT card:", bad)
    try:
        from astropy.io import fits as f2
        h = f2.getheader(glob.glob(f"{PROD}/signal/Norder13/Dir0/*.fits")[0])
        _ = h["ACSUNIT"]  # malformed card: parsing this keyword raises
        print(" astropy access ACSUNIT: OK")
    except Exception as exc:  # noqa: BLE001
        print(" astropy access ACSUNIT FAILED:", type(exc).__name__, exc)


def trace():
    print("\n== stdout / trace ==")
    for line in open(f"{STG}/stdout.jsonl"):
        e = json.loads(line)
        if e["event"] in ("frame_finished", "run_finished"):
            print(" ", e["event"], "exit_code", e.get("exit_code"), "status",
                  e.get("summary", {}).get("frames"))
    agg = {}
    for line in open(f"{STG}/trace.jsonl"):
        e = json.loads(line)
        k = (e.get("step"), e.get("process"), e.get("operation"))
        agg[k] = agg.get(k, 0) + 1
        if e.get("step") == "measured_stars":
            print(" measured_stars:", e["stars"])
    for k, n in sorted(agg.items(), key=lambda kv: -kv[1])[:6]:
        print(" ", n, k)


if __name__ == "__main__":
    manifest()
    objects()
    tiles()
    trace()
