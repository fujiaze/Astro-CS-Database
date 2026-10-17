#!/usr/bin/env python3
"""#80 P5 E0 — 冻结真实 2xH-alpha + 1xRed 三帧 Mosaic 输入身份。

真本（pristine）来源：
  A = run/science-v2-impl/e2e/out/ab_off/M42_M1_T2_flying_dutchman-20251212_014241-600S-H-alpha
  B = run/science-v2-impl/e2e/out/ab_off/M42_M1_T2_flying_dutchman-20251216_031003-600S-H-alpha
  C = run/PHOT-FIX/p1_wcs_full/M42_M1_T2_flying_dutchman-20251212_012404-300S-Red   (300 s, Baader R)

本脚本**只读**真本，在 scratch 下建立硬链接树（大 tile 不复制），仅对
`signal/properties` 的 `obs_filter=` 行做受控标签改写（#11 元数据对照臂）。
产出 evidence/input_index.json：逐文件 sha256 + 帧级身份 + 软件/二进制 SHA。

真实观测 FITS 不入 Git；本脚本不写任何真本路径。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = {
    "A_Halpha_600s": "run/science-v2-impl/e2e/out/ab_off/"
                     "M42_M1_T2_flying_dutchman-20251212_014241-600S-H-alpha",
    "B_Halpha_600s": "run/science-v2-impl/e2e/out/ab_off/"
                     "M42_M1_T2_flying_dutchman-20251216_031003-600S-H-alpha",
    "C_Red_300s": "run/PHOT-FIX/p1_wcs_full/"
                  "M42_M1_T2_flying_dutchman-20251212_012404-300S-Red",
    "D_M42M2_Red_300s": "run/PHOT-FIX/p1_wcs_full/"
                        "M42_M2_T2_flying_dutchman-20251212_020002-300S-Red",
    "E_M42M2_Red_300s": "run/PHOT-FIX/p1_wcs_full/"
                        "M42_M2_T2_flying_dutchman-20251216_033550-300S-Red",
    "F_M42M2_Red_300s": "run/PHOT-FIX/p1_wcs_full/"
                        "M42_M2_T2_flying_dutchman-20251224_033141-300S-Red",
}


def sha256_file(path: str, chunk: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for c in iter(lambda: fh.read(chunk), b""):
            h.update(c)
    return h.hexdigest()


def tree_hash(root: str) -> dict:
    """per-file sha256 keyed by path relative to root, plus aggregate."""
    out = {}
    agg = hashlib.sha256()
    for dp, _dd, fs in os.walk(root):
        for f in sorted(fs):
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, root)
            out[rel] = {"sha256": sha256_file(p), "size": os.path.getsize(p)}
            agg.update(rel.encode())
            agg.update(out[rel]["sha256"].encode())
    return {"files": out, "aggregate_sha256": agg.hexdigest(), "n_files": len(out)}


def read_properties(path: str) -> dict:
    kv = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if "=" in line:
                k, v = line.split("=", 1)
                kv[k] = v
    return kv


def read_json(path: str):
    if not os.path.isfile(path):
        return None
    with open(path) as fh:
        return json.load(fh)


def frame_identity(path: str) -> dict:
    props = read_properties(os.path.join(path, "signal", "properties"))
    man = read_json(os.path.join(path, "manifest.json")) or {}
    fin = read_json(os.path.join(path, "p1_final.json"))
    stack = read_json(os.path.join(path, "p1_stack.json")) or {}
    geom = man.get("geometry", {})
    ident = {
        "dir": path,
        "dirname": os.path.basename(path.rstrip("/")),
        "obs_filter_declared": props.get("obs_filter"),
        "hips_order": props.get("hips_order"),
        "bunit": props.get("BUNIT"),
        "pixel_semantics": props.get("ACSD_PIXEL_SEMANTICS"),
        "manifest_units": man.get("units"),
        "nside": man.get("nside"),
        "n_leaf_tiles": man.get("n_leaf_tiles"),
        "nused_tiles": man.get("nused_tiles"),
        "acsd_covered_sky_fraction": man.get("acsd_covered_sky_fraction"),
        "geometry": {k: geom.get(k) for k in
                     ("crval1", "crval2", "crpix1", "crpix2", "cd11", "cd12",
                      "cd21", "cd22", "ctype1", "pixel_origin")},
        "has_p1_final": fin is not None,
        "has_p1_wcs": os.path.isfile(os.path.join(path, "p1_wcs.json")),
        "filter_passband": (fin or {}).get("filter_passband"),
        "frame_id": (fin or {}).get("frame_id"),
        "input_light": (fin or {}).get("input_light"),
        "uncertainty_available": (fin or {}).get("uncertainty_available"),
        "p1_stack_schema": stack.get("schema"),
        "p1_stack_entry": stack.get("entry"),
        "p1_stack_photscal": stack.get("photscal"),
        "p1_stack_photappl": stack.get("photappl"),
        "p1_stack_pixfrac": stack.get("pixfrac"),
        "p1_stack_nside": stack.get("nside"),
        "variance_audit": stack.get("variance_audit"),
    }
    return ident


def build_overlay(src: str, dst: str, label: str) -> None:
    """硬链接树 + 受控改写 signal/properties 的 obs_filter 行。"""
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    subprocess.run(["cp", "-al", src, dst], check=True)
    p = os.path.join(dst, "signal", "properties")
    if os.path.isfile(p):
        with open(p, encoding="utf-8", errors="replace") as fh:
            lines = fh.readlines()
        lines = [ln for ln in lines if not ln.startswith("obs_filter=")]
        lines.append(f"obs_filter={label}\n")
        os.remove(p)  # 断开硬链接，绝不回写真本
        with open(p, "w", encoding="utf-8") as fh:
            fh.writelines(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scratch", default="/workspace/issue80-scratch")
    ap.add_argument("--out", default=os.path.join(REPO, "实验/issue80-p5-nonspd-e0/evidence"))
    ap.add_argument("--skip-tree-hash", action="store_true",
                    help="只做帧级身份（不再逐文件哈希）")
    a = ap.parse_args()

    frozen = os.path.join(a.scratch, "frozen")
    os.makedirs(frozen, exist_ok=True)
    os.makedirs(a.out, exist_ok=True)

    head = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"],
                          capture_output=True, text=True).stdout.strip()
    acsd = os.path.join(REPO, "build/linux-sci-p5/acsd")
    index = {
        "schema": "acsd.issue80.input-index/v1",
        "git_head": head,
        "acsd_binary": acsd,
        "acsd_binary_sha256": sha256_file(acsd) if os.path.isfile(acsd) else None,
        "src_roots": {k: os.path.join(REPO, v) for k, v in SRC.items()},
        "frames": {},
        "arms": {},
    }
    # 近近似 A/B/C 的真实真本；D/E/F 为同滤镜三帧正对照（M42_M2）
    for key, rel in SRC.items():
        root = os.path.join(REPO, rel)
        if not os.path.isdir(root):
            index["frames"][key] = {"MISSING": root}
            continue
        rec = frame_identity(root)
        if not a.skip_tree_hash:
            rec["tree"] = tree_hash(root)
        else:
            rec["tree"] = {"skipped": True}
        index["frames"][key] = rec

    # 臂：real abc / 2xHalpha / label swap / 全同滤镜三帧正对照
    arms = {
        "abc_real": [("A_Halpha_600s", "Baader 7nm H-alpha"),
                     ("B_Halpha_600s", "Baader 7nm H-alpha"),
                     ("C_Red_300s", "Baader R")],
        "ab_2halpha": [("A_Halpha_600s", "Baader 7nm H-alpha"),
                       ("B_Halpha_600s", "Baader 7nm H-alpha")],
        "abc_label_swap_all_halpha": [("A_Halpha_600s", "Baader 7nm H-alpha"),
                                      ("B_Halpha_600s", "Baader 7nm H-alpha"),
                                      ("C_Red_300s", "Baader 7nm H-alpha")],
        "abc_label_swap_all_red": [("A_Halpha_600s", "Baader R"),
                                   ("B_Halpha_600s", "Baader R"),
                                   ("C_Red_300s", "Baader R")],
        "def_samefilter_red_3frame": [("D_M42M2_Red_300s", "Baader R"),
                                      ("E_M42M2_Red_300s", "Baader R"),
                                      ("F_M42M2_Red_300s", "Baader R")],
    }
    for arm, specs in arms.items():
        paths = []
        for key, label in specs:
            root = os.path.join(REPO, SRC[key])
            if not os.path.isdir(root):
                paths.append({"key": key, "error": "missing"})
                continue
            dst = os.path.join(frozen, arm, os.path.basename(root))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            build_overlay(root, dst, label)
            paths.append({
                "key": key,
                "label": label,
                "path": dst,
                "properties_sha256": sha256_file(os.path.join(dst, "signal", "properties")),
                "obs_filter_written": label,
            })
        index["arms"][arm] = {"frames": paths}
        print(f"[freeze] arm {arm}: {len(paths)} frames")

    outp = os.path.join(a.out, "input_index.json")
    with open(outp, "w") as fh:
        json.dump(index, fh, indent=1, ensure_ascii=False)
    print(f"[freeze] wrote {outp}")
    for key, rec in index["frames"].items():
        if "MISSING" in rec:
            print(f"  {key}: MISSING")
            continue
        print(f"  {key}: filter={rec['obs_filter_declared']!r} "
              f"passband={rec['filter_passband']!r} files={rec['tree'].get('n_files')} "
              f"agg={rec['tree'].get('aggregate_sha256', '')[:16]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
