#!/usr/bin/env python3
"""IMPL-AIO-001 Oracle 负向门：注入违反冻结的产物，独立检查必须判红。

每个 mutation 施加于产物副本，断言 run_checks 至少产生一条含指定关键词的失败。
"""
import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from v6_aio_oracle_lib import (  # noqa: E402
    encode_checksum, oc_add, oc_sum, parse_hdus, run_checks)


def _patch_bunit_and_fix_checksum(raw: bytearray, hdu_index: int, key: str,
                                  new_value: str) -> None:
    hdus = parse_hdus(bytes(raw))
    hd = hdus[hdu_index]
    start = hd["data_off"] - hd["header_bytes"]
    header = bytearray(raw[start:start + hd["header_bytes"]])
    for c in range(0, len(header), 80):
        card = header[c:c + 80]
        if card[:8].decode("latin-1").strip() == key:
            field = ("'" + new_value + "'").encode("latin-1")
            card[10:10 + len(field)] = field
            for i in range(10 + len(field), 30):
                card[i] = 0x20
            header[c:c + 80] = card
            break
    # 重算 CHECKSUM（DATASUM 不变：数据未改）。
    datasum = hd["datasum"]
    off = header.find(b"CHECKSUM")
    if off < 0:
        raise RuntimeError("CHECKSUM card not found")
    header[off + 11:off + 27] = b"0" * 16
    hsum0 = oc_sum(bytes(header))
    enc = encode_checksum(0xFFFFFFFF - oc_add(datasum, hsum0))
    header[off + 11:off + 27] = enc
    raw[start:start + len(header)] = header


def _mut_missing_flux_factor(d):
    p = os.path.join(d, "provenance.json")
    j = json.load(open(p))
    j.pop("flux_conservation_factor", None)
    json.dump(j, open(p, "w"), indent=2)
    return "missing required key: flux_conservation_factor"


def _mut_bare_adu(d):
    p = os.path.join(d, "provenance.json")
    j = json.load(open(p))
    j["units"]["bunit"] = "ADU"
    j["units"]["pixel_semantics"] = "integrated_flux"
    j["units"]["pixel_area_power"] = 0
    j["pixel_semantics"] = "integrated_flux"
    json.dump(j, open(p, "w"), indent=2)
    return "BUNIT not dimensionally decidable"


def _mut_kcorr_one(d):
    p = os.path.join(d, "provenance.json")
    j = json.load(open(p))
    j["k_corr"]["value"] = 1.0
    json.dump(j, open(p, "w"), indent=2)
    return "k_corr=1"


def _mut_unavailable_placeholder(d):
    p = os.path.join(d, "provenance.json")
    j = json.load(open(p))
    j["unavailable"] = {"flag": True, "reason": "TBD", "scope": "variance"}
    json.dump(j, open(p, "w"), indent=2)
    return "unavailable"


def _mut_fits_data_flip(d):
    p = os.path.join(d, "product.fits")
    raw = bytearray(open(p, "rb").read())
    hdus = parse_hdus(bytes(raw))
    raw[hdus[0]["data_off"] + 5] ^= 0xFF
    open(p, "wb").write(raw)
    return "DATASUM"


def _mut_fits_variance_bunit(d):
    p = os.path.join(d, "product.fits")
    raw = bytearray(open(p, "rb").read())
    _patch_bunit_and_fix_checksum(raw, 1, "BUNIT", "ADU/px^2")
    open(p, "wb").write(raw)
    return "variance BUNIT"


def _mut_fits_checksum_card(d):
    p = os.path.join(d, "product.fits")
    raw = bytearray(open(p, "rb").read())
    off = bytes(raw).find(b"CHECKSUM")
    raw[off + 11:off + 27] = b"AAAAAAAAAAAAAAAA"
    open(p, "wb").write(raw)
    return "checksum"


def _mut_manifest_sha(d):
    p = os.path.join(d, "manifest.json")
    j = json.load(open(p))
    j["files"][0]["sha256"] = "0" * 64
    json.dump(j, open(p, "w"), indent=2)
    return "sha256 mismatch"


def _mut_manifest_role(d):
    p = os.path.join(d, "manifest.json")
    j = json.load(open(p))
    j["files"][0]["role"] = "weight"
    json.dump(j, open(p, "w"), indent=2)
    return "invalid role"


def _mut_properties_missing_creator(d):
    p = os.path.join(d, "properties")
    lines = [ln for ln in open(p).read().splitlines()
             if not ln.startswith("creator_did")]
    open(p, "w").write("\n".join(lines) + "\n")
    return "properties missing key creator_did"


MUTATIONS = [
    ("M1-missing-flux-factor", _mut_missing_flux_factor),
    ("M2-bare-adu-bunit", _mut_bare_adu),
    ("M3-kcorr-one", _mut_kcorr_one),
    ("M4-unavailable-placeholder", _mut_unavailable_placeholder),
    ("M5-fits-data-flip", _mut_fits_data_flip),
    ("M6-fits-variance-bunit", _mut_fits_variance_bunit),
    ("M7-fits-checksum-card", _mut_fits_checksum_card),
    ("M8-manifest-sha", _mut_manifest_sha),
    ("M9-manifest-role", _mut_manifest_role),
    ("M10-properties-missing-creator", _mut_properties_missing_creator),
]


def main(argv):
    if len(argv) != 3:
        print("usage: v6_aio_oracle_negative.py <artifacts_dir> <repo_root>")
        return 2
    art_dir, repo_root = argv[1], argv[2]
    failures = run_checks(art_dir, repo_root)
    if failures:
        print("PRECONDITION FAIL: pristine artifacts not oracle-clean:")
        for f in failures:
            print("  - " + f)
        return 1
    tmp = tempfile.mkdtemp(prefix="v6_aio_oracle_neg_")
    not_caught = []
    try:
        for name, fn in MUTATIONS:
            mut = os.path.join(tmp, name)
            shutil.copytree(art_dir, mut)
            keyword = fn(mut)
            fails = run_checks(mut, repo_root)
            caught = any(keyword.lower() in f.lower() for f in fails)
            if not caught:
                not_caught.append(name)
                print("MISSED %-32s expected keyword %r; fails=%r" %
                      (name, keyword, fails[:3]))
            else:
                print("CAUGHT %-32s (%d failures, keyword=%r)" %
                      (name, len(fails), keyword))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not_caught:
        print("ORACLE NEGATIVE FAIL: %d/%d not caught: %s" %
              (len(not_caught), len(MUTATIONS), not_caught))
        return 1
    print("ORACLE NEGATIVE PASS: %d/%d caught" % (len(MUTATIONS), len(MUTATIONS)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
