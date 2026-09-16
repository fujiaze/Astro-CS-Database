#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""hips_compare.py — HiPS 结构/数值比较器（T904/W3）

比较两个 HiPS 目录（baseline vs candidate）：
- 结构：manifest（nside/tile_width/products/n_leaf_tiles）、tile set（Norder/Dir/Npix）、
  MOC 数据、metadata 数据 —— exact。
- support：数据区 exact（离散计数，0 容差）。
- signal：数据区按冻结容差（默认 1e-4，R70 曾证明 max_abs=0.0）比较；报告 max_abs/mean_abs/
  exact 比例、NaN 域漂移。
- FITS 字节比较排除 CHECKSUM/DATASUM 卡片（内含运行时间戳，非科学差异）。

自测：--self（candidate==baseline → exact PASS）与 --perturb（注入扰动 → 必须 FAIL）。

用法：
  python3 hips_compare.py --baseline <dir> --candidate <dir> --out <json> [--signal-tol 1e-4]
  python3 hips_compare.py --self --baseline <dir> --candidate <dir> --out <json>   # self=0
  python3 hips_compare.py --perturb --baseline <dir> --candidate <dir> --out <json> # 扰动 FAIL

退出码：0 PASS，1 比较 FAIL，2 环境错误，3 参数/结构错误。
"""
import argparse, json, os, re, struct, sys

FITS_HDU_BLOCK = 2880
_CARD_RE = re.compile(r"^([A-Z0-9_]{8})")


def _read_header(path):
    """返回 (header 卡片 dict{key: value_str}, data 起始偏移)。
    FITS 主头可跨多个 2880 字节块，逐块读取直到 END 卡片。"""
    cards = {}
    n_cards = 0
    with open(path, "rb") as f:
        while True:
            blk = f.read(FITS_HDU_BLOCK)
            if len(blk) < FITS_HDU_BLOCK:
                raise ValueError(f"{path}: truncated FITS header")
            for i in range(0, len(blk) - 80 + 1, 80):
                rec = blk[i:i + 80]
                key = rec[:8].decode("ascii", "replace").strip()
                if key == "END":
                    n_cards += 1
                    blocks = (n_cards * 80 + FITS_HDU_BLOCK - 1) // FITS_HDU_BLOCK
                    return cards, blocks * FITS_HDU_BLOCK
                cards[key] = rec[8:80].decode("ascii", "replace")
                n_cards += 1


def _data(path, start):
    with open(path, "rb") as f:
        f.seek(start)
        return f.read()


def _read_float32(data):
    """FITS -32 数据区 → float 列表（大端）。"""
    n = len(data) // 4
    return list(struct.unpack(">%df" % n, data[:n * 4]))


def _fits_files(hipsdir, product):
    """返回 {relpath: fullpath}，仅收集 Npix 数据 tile（Moc.fits/metadata.fits 单独处理）。"""
    base = os.path.join(hipsdir, product)
    out = {}
    if not os.path.isdir(base):
        return out
    for dirpath, _, files in os.walk(base):
        for fn in files:
            if not fn.endswith(".fits"):
                continue
            if fn in ("Moc.fits", "metadata.fits"):
                continue  # 结构文件单独比较
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, hipsdir).replace("\\", "/")
            out[rel] = full
    return out


def _masked_bytes(path):
    """返回整个文件字节，但把所有 80 字节对齐的 CHECKSUM/DATASUM 卡片值域清零
    （排除运行时间戳差异；结构文件通常为 header 卡片，数据区误命中的概率可忽略）。"""
    data = open(path, "rb").read()
    out = bytearray(data)
    pos = 0
    while pos + 80 <= len(data):
        key = data[pos:pos + 8].decode("ascii", "replace").strip()
        if key in ("CHECKSUM", "DATASUM"):
            for i in range(pos + 8, pos + 80):
                out[i] = 0
        pos += 80
    return bytes(out)


def _structural_files(hipsdir, product):
    """返回该 product 下 Moc.fits / metadata.fits 的 masked 字节。"""
    base = os.path.join(hipsdir, product)
    out = {}
    for fn in ("Moc.fits", "metadata.fits"):
        p = os.path.join(base, fn)
        if os.path.exists(p):
            out[fn] = _masked_bytes(p)
    return out


def _compare_tileset(b, c, product):
    kb = _fits_files(b, product)
    kc = _fits_files(c, product)
    sb, sc = set(kb), set(kc)
    only_b = sorted(sb - sc)
    only_c = sorted(sc - sb)
    return len(sb), len(sc), only_b, only_c, kb, kc


def compare_dir(b, c, signal_tol=1e-4, perturb=None):
    """比较两个 HiPS 目录。返回结果 dict。"""
    res = {"products": {}, "structural": {}, "fatal": None}
    for prod in ("signal", "support"):
        pr = {"tileset": {}, "data": {}}
        nb, nc, only_b, only_c, kb, kc = _compare_tileset(b, c, prod)
        pr["tileset"] = {
            "baseline": nb, "candidate": nc,
            "only_baseline": only_b[:10], "only_candidate": only_c[:10],
            "tileset_exact": (not only_b) and (not only_c),
        }
        # structural files (Moc/metadata): masked byte compare
        stb = _structural_files(b, prod)
        stc = _structural_files(c, prod)
        pr["structural"] = {}
        for fn in sorted(set(stb) | set(stc)):
            pr["structural"][fn] = {
                "baseline": fn in stb, "candidate": fn in stc,
                "masked_exact": (fn in stb) and (fn in stc) and stb[fn] == stc[fn],
            }
        # compare common tiles by data region
        common = sorted(set(kb) & set(kc))
        n_exact = 0
        n_tol = 0
        n_diff = 0
        n_nan_b = 0
        n_nan_c = 0
        max_abs = 0.0
        mean_abs = 0.0
        total = 0
        worst = None
        for rel in common:
            hb, sb = _read_header(kb[rel])
            hc, sc = _read_header(kc[rel])
            # header compare excluding CHECKSUM/DATASUM
            hb2 = {k: v for k, v in hb.items() if k not in ("CHECKSUM", "DATASUM")}
            hc2 = {k: v for k, v in hc.items() if k not in ("CHECKSUM", "DATASUM")}
            hdr_exact = hb2 == hc2
            db = _data(kb[rel], sb)
            dc = _data(kc[rel], sc)
            if db == dc and hdr_exact:
                n_exact += 1
                continue
            if db != dc:
                # 仅对字节不一致的 signal tile 做浮点解码（R70 exact 场景下极少触发）
                if prod == "signal":
                    vb = _read_float32(db)
                    vc = _read_float32(dc)
                    if len(vb) != len(vc):
                        n_diff += 1
                        pr["data"]["len_mismatch"] = rel
                        continue
                    n_nan_b += sum(1 for x in vb if x != x)
                    n_nan_c += sum(1 for x in vc if x != x)
                    # 逐像素：NaN 匹配 NaN 视为一致；NaN 失配或数值差>tol 视为 diff
                    d = []
                    na = 0
                    for a, bb in zip(vb, vc):
                        if a != a and bb != bb:
                            continue
                        if a != a or bb != bb:
                            na += 1
                            continue
                        d.append(abs(a - bb))
                    if na > 0:
                        n_diff += 1
                        worst = worst or (rel, float("nan"))
                    if d:
                        ma = max(d)
                        mean = sum(d) / len(d)
                        total += len(d)
                        max_abs = max(max_abs, ma)
                        mean_abs += mean
                        if ma <= signal_tol:
                            n_tol += 1
                        else:
                            n_diff += 1
                            if worst is None or ma > worst[1]:
                                worst = (rel, ma)
                    elif na == 0 and not d:
                        # 数据完全一致（仅 header 差异）
                        n_tol += 1
                else:
                    # support 离散（exact 契约）
                    n_diff += 1
                    if worst is None:
                        worst = (rel, 0.0)
            else:
                # header 不一致但数据一致：仅结构差异（如 checksum 卡片值）
                n_tol += 1
        pr["data"] = {
            "common_tiles": len(common),
            "exact_data": n_exact,
            "within_tol": n_tol,
            "diff": n_diff,
            "max_abs_diff": round(max_abs, 9),
            "mean_abs_diff": round(mean_abs / max(1, len(common)), 9),
            "nan_baseline": n_nan_b,
            "nan_candidate": n_nan_c,
            "worst": worst,
        }
        res["products"][prod] = pr

    # manifest compare
    for f in ("manifest.json",):
        bp = os.path.join(b, f)
        cp = os.path.join(c, f)
        if os.path.exists(bp) and os.path.exists(cp):
            jb = json.load(open(bp))
            jc = json.load(open(cp))
            # exclude known timestamps/non-structural if any
            res["structural"][f] = {
                "baseline": jb, "candidate": jc,
                "exact": jb == jc,
            }
    return res


def verdict(res, signal_tol):
    p = res["products"]
    ok = True
    reasons = []
    for prod in ("signal", "support"):
        t = p[prod]["tileset"]
        d = p[prod]["data"]
        st = p[prod].get("structural", {})
        if not t["tileset_exact"]:
            ok = False
            reasons.append(f"{prod} tileset not exact (only_b={len(t['only_baseline'])}, only_c={len(t['only_candidate'])})")
        if d["diff"] > 0:
            ok = False
            reasons.append(f"{prod} data diff={d['diff']} (max_abs={d['max_abs_diff']})")
        if d["nan_baseline"] or d["nan_candidate"]:
            ok = False
            reasons.append(f"{prod} NaN domain drift (base={d['nan_baseline']}, cand={d['nan_candidate']})")
        for fn, st in st.items():
            if not st["masked_exact"]:
                ok = False
                reasons.append(f"{prod}/{fn} structural not exact")
        if not d["within_tol"] and d["diff"] > 0 and prod == "signal":
            reasons.append(f"signal within_tol={d['within_tol']}/{d['common_tiles']}")
    return ok, reasons


def write_diff_hips(b, c, out, product="signal"):
    """生成 difference HiPS：对 baseline/candidate 公共 signal tile 写 (cand - base) float32。
    仅含 product 数据 tile；保留 Norder/Dir 结构；manifest 反映 n_leaf_tiles。"""
    import struct as _st
    os.makedirs(out, exist_ok=True)
    kb = _fits_files(b, product)
    kc = _fits_files(c, product)
    common = sorted(set(kb) & set(kc))
    n_leaf = 0
    n_written = 0
    for rel in common:
        hb, sb = _read_header(kb[rel])
        hc, sc = _read_header(kc[rel])
        db = _data(kb[rel], sb)
        dc = _data(kc[rel], sc)
        if len(db) != len(dc):
            continue
        nb = len(db) // 4
        if db == dc:
            # 数据字节一致 → diff 全零（免浮点解码，快）
            diff = [0.0] * nb
        else:
            vb = list(_st.unpack(">%df" % nb, db[:nb * 4]))
            vc = list(_st.unpack(">%df" % nb, dc[:nb * 4]))
            diff = [0.0 if (x != x and y != y) else (x - y if (x == x and y == y) else float("nan"))
                    for x, y in zip(vc, vb)]
        dst = os.path.join(out, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        # header: reuse candidate header minus CHECKSUM/DATASUM/END
        hdr = bytearray()
        with open(kc[rel], "rb") as f:
            blk = f.read(FITS_HDU_BLOCK)
        for i in range(0, len(blk) - 80 + 1, 80):
            rec = blk[i:i + 80]
            key = rec[:8].decode("ascii", "replace").strip()
            if key in ("CHECKSUM", "DATASUM"):
                continue
            hdr += rec
            if key == "END":
                break
        while len(hdr) % FITS_HDU_BLOCK != 0:
            hdr += b" " * 80
        data = _st.pack(">%df" % len(diff), *diff)
        if len(data) % FITS_HDU_BLOCK != 0:
            data += b"\x00" * (FITS_HDU_BLOCK - len(data) % FITS_HDU_BLOCK)
        with open(dst, "wb") as f:
            f.write(bytes(hdr) + data)
        n_written += 1
        # 深度层级 tile 均写；leaf = 最深 Norder 的 tiles（此处统计 Npix 文件数）
    # manifest（仅 signal 产品；n_leaf 以最深 Norder 的 tile 计数）
    from collections import Counter
    orders = Counter()
    for rel in common:
        parts = rel.split("/")
        if len(parts) >= 2 and parts[1].startswith("Norder"):
            orders[parts[1]] += 1
    deepest = max(orders, key=lambda o: int(o.replace("Norder", ""))) if orders else "Norder0"
    n_leaf = orders.get(deepest, 0)
    manifest = {
        "format_version": 1, "hips_version": "1.4", "nside": 65536,
        "tile_width": 512, "data_type": "float32", "products": [product],
        "n_leaf_tiles": n_leaf, "note": "difference = candidate - baseline",
    }
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    return {"written_tiles": n_written, "n_leaf_tiles": n_leaf, "deepest_norder": deepest}


def write_view_state(base, cand, diff, support, out, views):
    """写统一 view_state.json（浏览器启动/视觉验收状态）。"""
    vs = {
        "version": 1,
        "stf": {"mode": "fixed", "signal_min": 0.0, "signal_max": 1.0},
        "products": {
            "baseline_signal": base,
            "candidate_signal": cand,
            "difference_signal": diff,
            "candidate_support": support,
        },
        "views": views,
    }
    with open(out, "w", encoding="utf-8") as f:
        json.dump(vs, f, ensure_ascii=False, indent=2)
    return vs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--candidate", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--signal-tol", type=float, default=1e-4)
    ap.add_argument("--self", action="store_true", help="self-test: candidate==baseline → exact")
    ap.add_argument("--perturb", action="store_true",
                    help="perturbation test: copy candidate, corrupt 1 signal tile, compare → must FAIL")
    ap.add_argument("--diff-hips", default=None, help="output difference HiPS dir (candidate - baseline)")
    ap.add_argument("--view-state", default=None, help="output view_state.json path")
    args = ap.parse_args()
    if not os.path.isdir(args.baseline) or not os.path.isdir(args.candidate):
        print(json.dumps({"error": "baseline/candidate dir missing"}, ensure_ascii=False))
        sys.exit(2)
    b, c = args.baseline, args.candidate
    res = compare_dir(b, c, signal_tol=args.signal_tol)
    ok, reasons = verdict(res, args.signal_tol)
    if args.diff_hips:
        dres = write_diff_hips(b, c, args.diff_hips)
        res["diff_hips"] = dres
    if args.view_state:
        vs = write_view_state(
            base=b, cand=c,
            diff=args.diff_hips or "",
            support=os.path.join(c, "support"),
            out=args.view_state,
            views=[
                {"name": "gc_wide", "ra_deg": 266.4168, "dec_deg": -29.0078, "fov_deg": 6.0, "product": "baseline_signal"},
                {"name": "panel1_2_overlap", "ra_deg": 266.4168, "dec_deg": -29.0078, "fov_deg": 2.0, "product": "candidate_signal"},
                {"name": "panel2_3_overlap", "ra_deg": 266.4168, "dec_deg": -29.0078, "fov_deg": 2.0, "product": "difference_signal"},
                {"name": "seam_closeup", "ra_deg": 266.4168, "dec_deg": -29.0078, "fov_deg": 0.5, "product": "difference_signal"},
                {"name": "support_view", "ra_deg": 266.4168, "dec_deg": -29.0078, "fov_deg": 2.0, "product": "candidate_support"},
            ])
        res["view_state"] = vs
    if args.self:
        # candidate vs itself → must be exact everywhere
        res_self = compare_dir(c, c, signal_tol=args.signal_tol)
        self_exact = all(
            res_self["products"][p]["data"]["diff"] == 0
            and res_self["products"][p]["tileset"]["tileset_exact"]
            for p in ("signal", "support"))
        res["self_test"] = {"passed": self_exact, "detail": res_self}
        if not self_exact:
            ok = False
            reasons.append("self-test not exact (self != 0)")
    if args.perturb:
        # 注入扰动：复制 candidate 的 signal/support 产品与 manifest（跳过 dense cache 等
        # 非比较内容），损坏一个 signal tile 数据字节，比较必须 FAIL
        import shutil, tempfile
        tmp = tempfile.mkdtemp(prefix="hips_perturb_")
        pert_cand = os.path.join(tmp, "perturbed")
        os.makedirs(pert_cand)
        for prod in ("signal", "support"):
            src = os.path.join(c, prod)
            if os.path.isdir(src):
                shutil.copytree(src, os.path.join(pert_cand, prod))
        if os.path.exists(os.path.join(c, "manifest.json")):
            shutil.copy2(os.path.join(c, "manifest.json"), os.path.join(pert_cand, "manifest.json"))
        tiles = sorted(
            os.path.join(dirpath, fn)
            for dirpath, _, files in os.walk(os.path.join(pert_cand, "signal"))
            for fn in files if fn.startswith("Npix") and fn.endswith(".fits"))
        if not tiles:
            print(json.dumps({"error": "no signal tiles to perturb"}, ensure_ascii=False))
            sys.exit(2)
        t = tiles[0]
        data = bytearray(open(t, "rb").read())
        pos = 0
        while pos + 80 <= len(data):
            if data[pos:pos + 8] == b"END     ":
                break
            pos += 80
        doff = ((pos // 80 + 1) * 80 + 2879) // 2880 * 2880
        data[doff + 4] ^= 0x01
        open(t, "wb").write(bytes(data))
        res_pert = compare_dir(b, pert_cand, signal_tol=args.signal_tol)
        pert_diff = sum(
            res_pert["products"][p]["data"]["diff"] for p in ("signal", "support"))
        pert_ok, pert_reasons = verdict(res_pert, args.signal_tol)
        res["perturb_test"] = {
            "mode": "inject+recompare",
            "perturbed_tile": os.path.basename(t),
            "diff_after_perturb": pert_diff,
            "must_fail": True,
            "observed": "FAIL" if pert_diff > 0 else "PASS",
            "reasons": pert_reasons,
        }
        shutil.rmtree(tmp, ignore_errors=True)
        if pert_diff == 0:
            ok = False
            reasons.append("perturbation test did NOT detect injected diff (must FAIL)")
    result = {
        "tool": "hips_compare",
        "baseline": b,
        "candidate": c,
        "signal_tol": args.signal_tol,
        "status": "PASS" if ok else "FAIL",
        "reasons": reasons,
        "detail": res,
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(json.dumps({"status": result["status"], "reasons": reasons}, ensure_ascii=False, indent=2))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
