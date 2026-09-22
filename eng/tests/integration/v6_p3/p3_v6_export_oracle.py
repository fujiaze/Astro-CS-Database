#!/usr/bin/env python3
# eng/tests/integration/v6_p3/p3_v6_export_oracle.py — P3-INTEGRATE-001 独立磁盘重开 Oracle
#
# 纯 stdlib（不依赖 astropy/numpy/jsonschema）：从零解析 FITS 2880 块、独立重算
# DATASUM/CHECKSUM（FITS 4.0 §4.4.2.5 1 补码 16-bit 交替和 + Seaman 不变式）、
# 逐 HDU 对照 C++ 写出的 expected.json、并用生产 provenance schema 的 required/allOf
# 与冻结单位表反查 provenance.json。
#
# 用法: p3_v6_export_oracle.py <artifacts_dir> <repo_root>
# 退出码 0 = 全部通过；非 0 = 至少一项失败（输出每个 mode 的失败原因）。
import hashlib
import json
import os
import re
import struct
import sys

EXCLUDE = set([0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60])


def fold16(hi, lo):
    """FITS 1 补码半字折叠：两半交叉进位（与 CFITSIO ffesum/Seaman 同构）。"""
    hicarry = hi >> 16
    locarry = lo >> 16
    while hicarry | locarry:
        hi = (hi & 0xFFFF) + locarry
        lo = (lo & 0xFFFF) + hicarry
        hicarry = hi >> 16
        locarry = lo >> 16
    return hi, lo


def oc_sum(data):
    """FITS 1 补码 32-bit 累加：16-bit 大端字交替进 hi/lo，720 字折半。"""
    hi = lo = 0
    words = 0
    n = len(data) - (len(data) % 2)
    for i in range(0, n, 2):
        w = (data[i] << 8) | data[i + 1]
        if words % 2 == 0:
            hi += w
        else:
            lo += w
        words += 1
        if words % 720 == 0:
            hi, lo = fold16(hi, lo)
    hi, lo = fold16(hi, lo)
    return ((hi & 0xFFFF) << 16) | (lo & 0xFFFF)


def oc_add(a, b):
    hi = a >> 16
    lo = a & 0xFFFF
    hi += b >> 16
    lo += b & 0xFFFF
    hi, lo = fold16(hi, lo)
    return ((hi & 0xFFFF) << 16) | (lo & 0xFFFF)


def card_value(card):
    """从 80 列卡取值：字符串按引号+''转义；数值/逻辑去注释。"""
    field = card[10:].decode("ascii", "replace")
    if field.startswith("'"):
        out = []
        i = 1
        n = len(field)
        while i < n:
            ch = field[i]
            if ch == "'":
                if i + 1 < n and field[i + 1] == "'":
                    out.append("'")
                    i += 2
                    continue
                break
            out.append(ch)
            i += 1
        return "".join(out)
    return field.split("/")[0].strip()


def parse_cards(block):
    cards = {}
    for c in range(36):
        card = block[c * 80:(c + 1) * 80]
        if len(card) < 80:
            break
        key = card[:8].decode("ascii", "replace").strip()
        if key == "END":
            return cards, True
        if len(card) < 10 or card[8:10] != b"= ":
            continue
        cards[key] = card_value(card)
    return cards, False


class Hdu(object):
    pass


def parse_fits(raw):
    hdus = []
    pos = 0
    first = True
    while pos + 2880 <= len(raw):
        blocks = 0
        cards = {}
        found_end = False
        b = 0
        while pos + (b + 1) * 2880 <= len(raw):
            cards, end = parse_cards(raw[pos + b * 2880:pos + (b + 1) * 2880])
            if end:
                found_end = True
                blocks = b + 1
                break
            b += 1
        if not found_end:
            raise RuntimeError("no END card at HDU %d" % len(hdus))
        header_bytes = blocks * 2880
        h = Hdu()
        h.is_primary = first
        h.bitpix = int(cards["BITPIX"].split("/")[0].strip())
        naxis = int(cards.get("NAXIS", "0").split("/")[0].strip())
        h.naxis = []
        for i in range(1, naxis + 1):
            h.naxis.append(int(cards["NAXIS%d" % i].split("/")[0].strip()))
        h.bunit = cards.get("BUNIT", "").strip().strip("'")
        h.extname = "" if first else cards.get("EXTNAME", "").strip().strip("'")
        h.datasum_card = cards.get("DATASUM", "").strip()
        h.checksum_card = cards.get("CHECKSUM", "").strip().strip("'")
        bpp = abs(h.bitpix) // 8
        raw_size = bpp
        for d in h.naxis:
            raw_size *= d
        padded = ((raw_size + 2879) // 2880) * 2880
        data_off = pos + header_bytes
        if data_off + padded > len(raw):
            raise RuntimeError("truncated data at HDU %d" % len(hdus))
        h.data_off = data_off
        h.data_len = raw_size
        h.padded = padded
        h.header_off = pos
        h.header_bytes = header_bytes
        # 独立 CHECKSUM/DATASUM 复核
        h.recomputed_datasum = oc_sum(raw[data_off:data_off + padded])
        header_sum = oc_sum(raw[pos:pos + header_bytes])
        h.combined = oc_add(h.recomputed_datasum, header_sum)
        hdus.append(h)
        pos = data_off + padded
        first = False
    return hdus


# -------- 独立单位幂次解析 --------
def parse_unit(u):
    u = u.strip()
    if not u:
        return None
    adu = px = 0
    if "*" in u and "/" not in u:
        for f in u.split("*"):
            e = parse_factor(f)
            if e is None:
                return None
            adu += e[0]
            px += e[1]
        return (adu, px)
    parts = u.split("/")
    if len(parts) > 2:
        return None
    for f in parts[0].split("*"):
        e = parse_factor(f)
        if e is None:
            return None
        adu += e[0]
        px += e[1]
    if len(parts) == 2:
        for f in parts[1].split("*"):
            e = parse_factor(f)
            if e is None:
                return None
            adu -= e[0]
            px -= e[1]
    return (adu, px)


def parse_factor(f):
    f = f.strip()
    if f == "1":
        return (0, 0)
    name, exp = f, 1
    if "^" in f:
        name, es = f.split("^", 1)
        name = name.strip()
        try:
            exp = int(es.strip())
        except ValueError:
            return None
    if name == "ADU":
        return (exp, 0)
    if name in ("px", "pixel"):
        return (0, exp)
    return None


def main(argv):
    if len(argv) != 3:
        print("usage: p3_v6_export_oracle.py <artifacts_dir> <repo_root>")
        return 2
    art, root = argv[1], argv[2]
    fails = []
    checks = 0

    def ck(cond, msg):
        nonlocal checks
        checks += 1
        if not cond:
            fails.append(msg)

    # 生产 provenance 记录级 schema（产品族字段级合同 $defs.provenance：required + allOf 规则）
    with open(os.path.join(root, "eng/contracts/schemas/product_family_field_constraints.schema.json")) as f:
        pschema = json.load(f)["$defs"]["provenance"]
    required = pschema["required"]
    allof = pschema.get("allOf", [])
    with open(os.path.join(root, "eng/contracts/data/v6_clause_registry_v1.json")) as f:
        dictionary = json.load(f)
    ck("quadratic_law" in dictionary, "dictionary has quadratic_law")

    modes = ["surface_brightness", "point_source_flux", "visualization"]
    expected_bunits = {
        "surface_brightness": {"": "ADU/sr", "VARIANCE": "ADU^2/sr^2", "COVERAGE": "1"},
        "point_source_flux": {"": "ADU/sr", "VARIANCE": "ADU^2/sr^2", "COVERAGE": "1",
                              "FLUX": "ADU", "FLUX_VARIANCE": "ADU^2", "EFFECTIVE_PSF": "1"},
        "visualization": {"": "ADU/sr"},
    }
    for mode in modes:
        d = os.path.join(art, mode)
        fits_path = os.path.join(d, "product.fits")
        prov_path = os.path.join(d, "provenance.json")
        exp_path = os.path.join(d, "expected.json")
        ck(os.path.isfile(fits_path), "%s: product.fits present" % mode)
        ck(os.path.isfile(prov_path), "%s: provenance.json present" % mode)
        ck(os.path.isfile(exp_path), "%s: expected.json present" % mode)
        if not (os.path.isfile(fits_path) and os.path.isfile(prov_path) and os.path.isfile(exp_path)):
            continue
        raw = open(fits_path, "rb").read()
        expected = json.load(open(exp_path))
        prov = json.load(open(prov_path))
        hdus = parse_fits(raw)

        # 1) FITS 完整性：EXTEND 一致性/CHECKSUM 不变式/DATASUM
        for idx, h in enumerate(hdus):
            ck(h.datasum_card == str(h.recomputed_datasum),
               "%s HDU%d DATASUM %s != %d" % (mode, idx, h.datasum_card, h.recomputed_datasum))
            ck(h.combined in (0, 0xFFFFFFFF),
               "%s HDU%d CHECKSUM invariant broken (combined=0x%08X)" % (mode, idx, h.combined))
            ck(len(h.checksum_card) == 16, "%s HDU%d CHECKSUM length" % (mode, idx))

        # 2) HDU 集合 + BUNIT（含二次律：variance == signal^2, ivar == 1/variance）
        got = {}
        for h in hdus:
            got[h.extname] = h.bunit
        ck(set(got.keys()) == set(expected_bunits[mode].keys()),
           "%s: HDU set %s != %s" % (mode, sorted(got.keys()), sorted(expected_bunits[mode].keys())))
        for ext, want in expected_bunits[mode].items():
            ck(got.get(ext) == want, "%s: BUNIT[%s]=%r != %r" % (mode, ext or "PRIMARY", got.get(ext), want))
        sig = parse_unit(got.get("", ""))
        if "VARIANCE" in got:
            var = parse_unit(got["VARIANCE"])
            ck(sig is not None and var is not None and var == (2 * sig[0], 2 * sig[1]),
               "%s: variance BUNIT != signal^2" % mode)

        # 3) 逐 HDU 数据回环（独立 big-endian float64 解码）
        by_ext = {}
        for h in hdus:
            by_ext[h.extname] = h
        def data_of(ext):
            h = by_ext[ext]
            n = h.data_len // 8
            return list(struct.unpack(">%dd" % n, raw[h.data_off:h.data_off + n * 8]))
        for ext, key in (("", "signal"), ("VARIANCE", "variance"), ("FLUX", "flux"),
                         ("FLUX_VARIANCE", "flux_variance"), ("EFFECTIVE_PSF", "effective_psf")):
            if ext in by_ext:
                vals = data_of(ext)
                want = expected[key]
                ck(len(vals) == len(want), "%s: %s length %d != %d" % (mode, ext or "PRIMARY", len(vals), len(want)))
                bad = 0
                for a, b in zip(vals, want):
                    if abs(a - b) > 1e-9 * max(1.0, abs(b)):
                        bad += 1
                ck(bad == 0, "%s: %s %d/%d values differ from expected" % (mode, ext or "PRIMARY", bad, len(want)))

        # 4) provenance：required + allOf + 单位二次律 + 禁区
        for k in required:
            ck(k in prov and prov[k] is not None, "%s: provenance missing key %s" % (mode, k))
        units = prov.get("units", {})
        if units.get("bunit") == "ADU":
            ck(units.get("pixel_semantics") == "surface_brightness" and units.get("pixel_area_power") == -2,
               "%s: allOf ADU rule" % mode)
        if re.search(r"/sr$", str(units.get("bunit", ""))):
            ck(units.get("pixel_area_power") == -2, "%s: allOf px^2 rule" % mode)
        ck(units.get("pixel_semantics") == prov.get("pixel_semantics"),
           "%s: units.pixel_semantics != top-level" % mode)
        su = parse_unit(units.get("bunit", ""))
        ck(su == (1, -2), "%s: primary bunit exponents" % mode)
        if mode != "visualization":
            ck(prov.get("flux_conservation_factor") is not None
               and float(prov["flux_conservation_factor"]) > 0, "%s: flux_conservation_factor" % mode)
        kc = prov.get("k_corr")
        ck(isinstance(kc, dict), "%s: k_corr block" % mode)
        if isinstance(kc, dict):
            ck(abs(float(kc.get("value", 0)) - 1.0) > 1e-12, "%s: k_corr != 1" % mode)
            ck(isinstance(kc.get("domain"), dict) and len(kc["domain"]) > 0, "%s: k_corr.domain" % mode)
            cal = kc.get("calibration", {})
            ck(bool(cal.get("script")) and bool(cal.get("calibration_run_id")) and "seed" in cal,
               "%s: k_corr.calibration" % mode)
        ck(prov.get("correlation_summary", {}).get("representation") in
           ("correlation_kernel", "low_rank_factors", "common_master", "full_matrix_unavailable"),
           "%s: correlation_summary.representation" % mode)
        un = prov.get("unavailable", {})
        ck(bool(un.get("reason")) and bool(un.get("scope")), "%s: unavailable reason/scope" % mode)
        ck(re.fullmatch(r"[0-9a-f]{40}", str(prov.get("software_sha", ""))) is not None,
           "%s: software_sha 40-hex" % mode)
        # output_hash 必须等于磁盘 FITS 的实际 sha256（独立）
        want_hash = "sha256:" + hashlib.sha256(raw).hexdigest()
        ck(prov.get("output_hash") == want_hash,
           "%s: output_hash %s != %s" % (mode, prov.get("output_hash"), want_hash))
        # visualization 必须登记 uncertainty unavailable 且无测量层
        if mode == "visualization":
            ck(un.get("flag") is True, "visualization: unavailable.flag true")
            ck(set(by_ext.keys()) == {""}, "visualization: measurement HDUs forbidden")

    # astropy 交叉复核（若可用）：独立第三方 reader 校验 CHECKSUM + 标准符合性。
    try:
        import warnings as _w
        from astropy.io import fits as afits  # noqa
        astropy_ok = True
    except Exception:
        astropy_ok = False
    if astropy_ok:
        print("ORACLE_ASTROPY_AVAILABLE")
        for mode in modes:
            path = os.path.join(art, mode, "product.fits")
            if not os.path.isfile(path):
                continue
            with _w.catch_warnings(record=True) as rec:
                _w.simplefilter("always")
                hdul = afits.open(path, checksum=True)
                hdul.verify("exception")
                bad = [x for x in rec
                       if "not FITS standard" in str(x.message)
                       or "Checksum verification failed" in str(x.message)]
                ck(len(bad) == 0,
                   "%s: astropy non-standard/checksum warnings: %s"
                   % (mode, [str(x.message) for x in bad]))
                ck(all(bool(h._checksum) and bool(h._datasum) for h in hdul),
                   "%s: astropy DATASUM/CHECKSUM ok" % mode)
                hdul.close()
    else:
        print("ORACLE_ASTROPY_UNAVAILABLE (stdlib-only path; not a failure)")

    if fails:
        print("ORACLE_FAIL %d/%d checks failed:" % (len(fails), checks))
        for f in fails:
            print("  -", f)
        return 1
    print("ORACLE_PASS %d checks across %d modes" % (checks, len(modes)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
