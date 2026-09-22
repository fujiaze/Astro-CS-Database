#!/usr/bin/env python3
"""IMPL-AIO-001 独立 Oracle 库。

真值来源（不调用任何 C++ 被测实现）:
  - eng/contracts/data/v6_clause_registry_v1.json (冻结条款/单位表)
  - eng/contracts/schemas/product_family_field_constraints.schema.json#/$defs/provenance
    (required 键；产品族记录级字段级合同)
  - astropy.io.fits（外部 FITS 校验和/结构真值）
  - hashlib（SHA-256 真值）
  - 本文件内独立转写的 FITS 4.0 §4.4.2.5 1 补码算法（第二真值，交叉 astropy）
"""

from __future__ import annotations

import hashlib
import json
import math
import os
from typing import Dict, List, Tuple

_EXCLUDE = [0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
            0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60]


class _SkipAstropy(Exception):
    """astropy 不可用/文件损坏时跳过该块（失败已登记）。"""


def _fold(hi: int, lo: int) -> Tuple[int, int]:
    hc = hi >> 16
    lc = lo >> 16
    while hc | lc:
        hi = (hi & 0xFFFF) + lc
        lo = (lo & 0xFFFF) + hc
        hc = hi >> 16
        lc = lo >> 16
    return hi, lo


def oc_sum(data: bytes) -> int:
    """FITS 32-bit 1 补码和（even word -> hi, odd -> lo, 每 720 字折叠）。"""
    hi = lo = 0
    words = 0
    for i in range(0, len(data) - 1, 2):
        w = (data[i] << 8) | data[i + 1]
        if words % 2 == 0:
            hi += w
        else:
            lo += w
        words += 1
        if words % 720 == 0:
            hi, lo = _fold(hi, lo)
    hi, lo = _fold(hi, lo)
    return ((hi << 16) | lo) & 0xFFFFFFFF


def oc_add(a: int, b: int) -> int:
    hi = a >> 16
    lo = a & 0xFFFF
    hi += b >> 16
    lo += b & 0xFFFF
    hi, lo = _fold(hi, lo)
    return ((hi << 16) | lo) & 0xFFFFFFFF


def encode_checksum(value: int) -> bytes:
    """Rob Seaman / FITS 4.0 CHECKSUM 16 字符编码（独立转写）。"""
    masks = [0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF]
    asc = [0] * 32
    for ii in range(4):
        byte = (value & masks[ii]) >> (24 - 8 * ii)
        q = byte // 4 + 0x30
        r = byte % 4
        ch = [q, q, q, q]
        ch[0] += r
        check = True
        while check:
            check = False
            for kk in range(13):
                for jj in (0, 2):
                    if ch[jj] == _EXCLUDE[kk] or ch[jj + 1] == _EXCLUDE[kk]:
                        ch[jj] += 1
                        ch[jj + 1] -= 1
                        check = True
        for jj in range(4):
            asc[4 * jj + ii] = ch[jj]
    return bytes(asc[(ii + 15) % 16] for ii in range(16))


def _parse_card_value(card: bytes):
    if card[8:9] != b"=":
        return None
    field = card[10:].decode("latin-1")
    stripped = field.lstrip()
    if stripped.startswith("'"):
        out = []
        i = 1
        while i < len(stripped):
            if stripped[i] == "'":
                if i + 1 < len(stripped) and stripped[i + 1] == "'":
                    out.append("'")
                    i += 2
                    continue
                break
            out.append(stripped[i])
            i += 1
        return "".join(out).rstrip()
    return field[:20].strip()


def parse_hdus(raw: bytes) -> List[dict]:
    hdus: List[dict] = []
    pos = 0
    while pos < len(raw):
        cards: Dict[str, str] = {}
        b = 0
        found = False
        while pos + (b + 1) * 2880 <= len(raw):
            blk = raw[pos + b * 2880: pos + (b + 1) * 2880]
            for c in range(36):
                card = blk[c * 80:(c + 1) * 80]
                kw = card[:8].decode("latin-1").strip()
                if kw == "END":
                    found = True
                    break
                v = _parse_card_value(card)
                if v is not None:
                    cards[kw] = v
            if found:
                break
            b += 1
        if not found:
            raise ValueError("no END card at offset %d" % pos)
        header_bytes = (b + 1) * 2880
        bitpix = int(cards["BITPIX"])
        naxis = int(cards["NAXIS"])
        dims = [int(cards.get("NAXIS%d" % (i + 1), 0)) for i in range(naxis)]
        bpp = abs(bitpix) // 8
        size = 1
        for d in dims:
            size *= d
        raw_bytes = size * bpp
        padded = (raw_bytes + 2879) // 2880 * 2880
        dstart = pos + header_bytes
        if dstart + padded > len(raw):
            raise ValueError("truncated data at %d" % pos)
        header = raw[pos:pos + header_bytes]
        dsum = oc_sum(raw[dstart:dstart + padded])
        hsum = oc_sum(header)
        # CHECKSUM 字段清零后的头校验和 -> 期望编码。
        # 卡片值: 下标 10=''', 11..26=16 字符, 27='''。
        zero_header = bytearray(header)
        off = header.find(b"CHECKSUM")
        if off >= 0 and (off % 80) == 0:
            zero_header[off + 11:off + 27] = b"0" * 16
        hsum0 = oc_sum(bytes(zero_header))
        expected = encode_checksum(0xFFFFFFFF - oc_add(dsum, hsum0))
        hdus.append({
            "cards": cards,
            "dims": dims,
            "bitpix": bitpix,
            "header_bytes": header_bytes,
            "data_off": dstart,
            "padded": padded,
            "datasum": dsum,
            "stored_datasum": cards.get("DATASUM"),
            "stored_checksum": cards.get("CHECKSUM"),
            "combined": oc_add(dsum, hsum),
            "expected_checksum": expected.decode("latin-1"),
            "raw": raw,
        })
        pos = dstart + padded
    return hdus


def parse_unit(u: str) -> Tuple[int, int]:
    u = u.strip()
    if u == "1":
        return (0, 0)

    def factor(f: str) -> Tuple[int, int]:
        f = f.strip()
        if "^" in f:
            name, _, e = f.partition("^")
            exp = int(e)
        else:
            name, exp = f, 1
        name = name.strip()
        if name == "ADU":
            return (exp, 0)
        if name == "px":
            return (0, exp)
        raise ValueError("unknown unit factor: " + f)

    parts = u.split("/")
    adu = px = 0
    for f in parts[0].split("*"):
        a, b = factor(f)
        adu += a
        px += b
    if len(parts) == 2:
        for f in parts[1].split("*"):
            a, b = factor(f)
            adu -= a
            px -= b
    return (adu, px)


def _sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def _bunit_decidable(bunit: str, pixel_semantics: str, pixel_area_power: int,
                     target_pixel_area) -> bool:
    try:
        adu, px = parse_unit(bunit)
    except ValueError:
        return False
    if px != 0:
        return True
    if (adu == 1 and px == 0 and pixel_semantics == "surface_brightness"
            and pixel_area_power == -2 and target_pixel_area is not None
            and target_pixel_area > 0):
        return True
    return False


def run_checks(art_dir: str, repo_root: str) -> List[str]:
    """返回失败消息列表；空列表 = 全部独立检查通过。"""
    fail: List[str] = []

    def check(cond, msg):
        if not cond:
            fail.append(msg)

    def load_json(name):
        with open(os.path.join(art_dir, name), "r", encoding="utf-8") as fh:
            return json.load(fh)

    contract_path = os.path.join(
        repo_root, "eng/contracts/data/v6_clause_registry_v1.json")
    schema_path = os.path.join(
        repo_root, "eng/contracts/schemas/product_family_field_constraints.schema.json")
    with open(contract_path, "r", encoding="utf-8") as fh:
        contract = json.load(fh)
    with open(schema_path, "r", encoding="utf-8") as fh:
        prov_schema = json.load(fh)["$defs"]["provenance"]

    # ── C1 冻结条款在合同 JSON 中存在且状态可判 ──────────────────────────
    clause_ids = {c["id"] for c in contract["clauses"]}
    for cid in ["FZ-BUNIT-SEMANTICS", "FZ-PROV-MINIMAL-SET", "FZ-P3-BUNIT-QUADRATIC",
                "FZ-COND-FLUX-CONSERV", "FZ-PROV-KCORR", "FZ-UNIT-SIGNAL-SB",
                "FZ-UNIT-PSFSW", "FZ-UNIT-WINFO"]:
        check(cid in clause_ids, "contract missing clause " + cid)

    # ── C2 单位表真值 ────────────────────────────────────────────────────
    units_table = {u["symbol"]: u for u in contract["units_table"]}
    check(units_table["signal_sb"]["unit"] == "ADU/px^2", "contract signal_sb unit")
    check(units_table["signal_sb"]["variance_unit"] == "ADU^2/px^4",
          "contract signal_sb variance unit")
    check(units_table["signal_sb"]["ivar_unit"] == "px^4/ADU^2",
          "contract signal_sb ivar unit")
    check(units_table["W_info"]["unit"] == "ADU^-2", "contract W_info unit")
    # PSFSW-RETIRE-03：退役对象 psfsw_robust_weight 已从现行单位表**移出**
    # （与 §31.1 的 OBSOLETE/对象真删一致；物理删除对齐，见 UnitId 侧）。
    # 识别面只剩"退役登记"：它必须仍被登记为退役对象与退役模式，但**不得**再作为
    # 现行单位行出现（否则同一对象在权威链上出现第二个身份）。
    check("psfsw_robust_weight" not in units_table,
          "retired unit must not be a live units_table row (PSFSW-RETIRE-03)")
    check(contract["x-astrocs-canonical-object-retirement"]["retired_canonical_object"]
          == "psfsw_robust_weight",
          "retired unit must stay registered as a retired canonical object")
    check("psfsw_robust" in contract["weight_modes"].get("retired", []),
          "retired weight mode token must stay registered as retired")

    # ── C3 provenance 最小集（独立 required 来源 = schema） ──────────────
    # W6 SCHEMA-INTEGRATE-001 可能迁移 schema 文件：缺失时回退到语义同源的
    # 冻结清单（仍为独立于 C++ 实现的第二个来源），不因此破坏门。
    prov = load_json("provenance.json")
    if os.path.exists(schema_path):
        required = prov_schema.get("required", [])
    else:
        required = [
            "provenance_schema", "schema_version", "product", "software_sha",
            "run_id", "input_product_hashes", "config_hash", "units",
            "coordinate", "pixel_semantics", "sampling", "algorithm_ids",
            "module", "provider", "approximations", "degradations",
            "normalization_version", "weight_mode_version",
            "correlation_summary", "flux_conservation_factor", "k_corr",
            "generated_utc", "output_hash"]
    check(len(required) >= 20, "schema required list too short")
    for key in required:
        check(key in prov and prov[key] is not None,
              "provenance missing required key: " + key)
    check(isinstance(prov.get("input_product_hashes"), list) and prov["input_product_hashes"],
          "input_product_hashes must be non-empty")
    check(isinstance(prov.get("algorithm_ids"), list) and prov["algorithm_ids"],
          "algorithm_ids must be non-empty")
    check(isinstance(prov.get("software_sha"), str) and len(prov["software_sha"]) == 40
          and all(c in "0123456789abcdef" for c in prov["software_sha"]),
          "software_sha must be 40 lowercase hex")

    # ── C4 BUNIT 量纲可判 (FZ-BUNIT-SEMANTICS) ──────────────────────────
    u = prov["units"]
    check(u["bunit"] == units_table["signal_sb"]["unit"],
          "provenance units.bunit != contract signal_sb unit")
    check(_bunit_decidable(u["bunit"], u["pixel_semantics"], u["pixel_area_power"],
                           u.get("target_pixel_area")),
          "BUNIT not dimensionally decidable: %r" % (u,))

    # ── C5 flux_conservation_factor (FZ-COND-FLUX-CONSERV) ──────────────
    pixfrac = None
    if isinstance(prov.get("sampling"), dict) and "pixfrac" in prov["sampling"]:
        pixfrac = float(prov["sampling"]["pixfrac"])
    if pixfrac is not None and 0 < pixfrac < 1:
        fcf = prov.get("flux_conservation_factor")
        check(isinstance(fcf, (int, float)) and fcf > 0,
              "pixfrac<1 requires positive flux_conservation_factor")

    # ── C6 k_corr (FZ-PROV-KCORR) ───────────────────────────────────────
    kc = prov.get("k_corr")
    check(isinstance(kc, dict), "k_corr block missing")
    if isinstance(kc, dict):
        check(abs(float(kc.get("value", 0)) - 1.0) > 1e-12,
              "k_corr=1 ignores correlation")
        check(float(kc.get("value", 0)) > 0, "k_corr value must be positive")
        dom = kc.get("domain", {})
        check(all(k in dom for k in ["geometry", "pixfrac", "patch_size",
                                     "estimator", "spherical"]),
              "k_corr.domain incomplete")
        cal = kc.get("calibration", {})
        check(all(k in cal for k in ["script", "seed", "calibration_run_id"]),
              "k_corr.calibration incomplete")

    # ── C7 unavailable 显式登记 ─────────────────────────────────────────
    un = prov.get("unavailable")
    check(isinstance(un, dict) and un.get("reason") and un.get("scope"),
          "unavailable.{flag,reason,scope} required")
    if isinstance(un, dict) and un.get("flag"):
        placeholders = {"", "tbd", "todo", "unknown", "n/a", "na", "none",
                        "placeholder", "?", "-", "null"}
        check(str(un.get("reason", "")).strip().lower() not in placeholders,
              "unavailable reason is a placeholder: %r" % (un.get("reason"),))

    # ── C8 FITS 外部真值 (astropy) + 结构 ───────────────────────────────
    fits_path = os.path.join(art_dir, "product.fits")
    import astropy.io.fits as pyfits  # 外部独立真值

    try:
        hdul = pyfits.open(fits_path)
    except Exception as exc:  # 结构损坏必须判红，不得崩溃。
        fail.append("FITS open failed: %s" % exc)
        hdul = None
    try:
        if hdul is None:
            raise _SkipAstropy()
        check(len(hdul) == 3, "expected 3 HDUs")
        names = [h.name for h in hdul]
        check(names == ["PRIMARY", "VARIANCE", "IVAR"], "HDU names: %r" % (names,))
        for h in hdul:
            check(h.verify_datasum() == 1, "astropy datasum failed for " + h.name)
            check(h.verify_checksum() == 1, "astropy checksum failed for " + h.name)
        check(tuple(hdul[0].data.shape) == (4, 8), "PRIMARY shape")
        check(hdul[0].header.get("BUNIT") == "ADU/px^2", "PRIMARY BUNIT")
        check(hdul[1].header.get("BUNIT") == "ADU^2/px^4", "VARIANCE BUNIT")
        check(hdul[2].header.get("BUNIT") == "px^4/ADU^2", "IVAR BUNIT")
        sig = parse_unit(hdul[0].header.get("BUNIT"))
        var = parse_unit(hdul[1].header.get("BUNIT"))
        ivar = parse_unit(hdul[2].header.get("BUNIT"))
        check(var == (2 * sig[0], 2 * sig[1]), "variance BUNIT != signal^2")
        check(ivar == (-var[0], -var[1]), "ivar BUNIT != 1/variance")
    except _SkipAstropy:
        pass
    finally:
        if hdul is not None:
            hdul.close()

    # ── C9 独立 1 补码算法（第二真值） ──────────────────────────────────
    with open(fits_path, "rb") as fh:
        raw = fh.read()
    try:
        hdus = parse_hdus(raw)
        check(len(hdus) == 3, "pure-python parser HDU count")
        for i, hd in enumerate(hdus):
            check(str(hd["datasum"]) == str(hd["stored_datasum"]),
                  "HDU %d DATASUM mismatch: %s vs %s" %
                  (i, hd["datasum"], hd["stored_datasum"]))
            check(hd["combined"] in (0, 0xFFFFFFFF),
                  "HDU %d combined checksum wrong: %x" % (i, hd["combined"]))
            check(hd["expected_checksum"] == hd["stored_checksum"],
                  "HDU %d CHECKSUM encoding mismatch: %r vs %r" %
                  (i, hd["expected_checksum"], hd["stored_checksum"]))
    except ValueError as exc:
        fail.append("pure-python FITS parse failed: %s" % exc)

    # ── C10 manifest / SHA 交叉校验 ─────────────────────────────────────
    man = load_json("manifest.json")
    files = {f["relative_path"]: f for f in man.get("files", [])}
    check(man.get("tile_count") == len(man.get("files", [])),
          "manifest tile_count != len(files)")
    check("product.fits" in files, "manifest missing product.fits")
    for name, rec in files.items():
        p = os.path.join(art_dir, name)
        check(os.path.isfile(p), "manifest lists missing file " + name)
        if os.path.isfile(p):
            check(_sha256_file(p) == rec["sha256"],
                  "sha256 mismatch for " + name)
    allowed_roles = {"signal", "variance", "ivar", "coverage", "validity",
                     "support", "rejection", "point_information", "psf",
                     "manifest", "properties", "tile"}
    for rec in man.get("files", []):
        check(rec.get("role") in allowed_roles, "invalid role " + str(rec.get("role")))
        rp = rec.get("relative_path", "")
        check(rp and not rp.startswith("/") and ".." not in rp,
              "unsafe relative_path " + rp)
    fits_sha = _sha256_file(fits_path)
    if "product.fits" in files:
        check(files["product.fits"]["sha256"] == fits_sha, "manifest fits sha != hashlib")
    prov_sha = _sha256_file(os.path.join(art_dir, "provenance.json"))
    check(man.get("provenance_sha256") == prov_sha, "manifest provenance sha mismatch")
    check(prov.get("output_hash", "").replace("sha256:", "") == fits_sha,
          "provenance.output_hash != sha256(product.fits)")

    # ── C11 HiPS properties ─────────────────────────────────────────────
    props = open(os.path.join(art_dir, "properties"), "r", encoding="utf-8").read()
    kv = {}
    for line in props.splitlines():
        if "=" in line:
            k, _, v = line.partition("=")
            kv[k.strip()] = v.strip()
    for k in ["creator_did", "obs_collection", "dataproduct_type", "hips_version",
              "hips_order", "hips_tile_width", "hips_tile_format", "hips_frame"]:
        check(k in kv and kv[k], "properties missing key " + k)
    check(kv.get("hips_tile_format") in ("fits", "png", "jpeg", "jpg"),
          "properties tile format invalid")

    return fail


def main(argv: List[str]) -> int:
    if len(argv) != 3:
        print("usage: v6_aio_oracle.py <artifacts_dir> <repo_root>")
        return 2
    failures = run_checks(argv[1], argv[2])
    if failures:
        print("ORACLE FAIL (%d)" % len(failures))
        for f in failures:
            print("  - " + f)
        return 1
    print("ORACLE PASS")
    return 0


if __name__ == "__main__":
    import sys
    raise SystemExit(main(sys.argv))
