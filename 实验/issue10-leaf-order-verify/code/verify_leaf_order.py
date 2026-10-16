#!/usr/bin/env python3
"""Issue #10 —— 独立核对 `frame_science.signal.leaf_order` 与真实 HEALPix 叶阶。

合同（唯一 oracle，不读 sidecar 自己声明同自己）：
  · 叶阶 K = log2(frame_nside)；叶像元数 12·4^K；单叶实球面面积 Ω = 4π/(12·4^K) sr。
  · 512×512 HiPS tile 目录阶 Norder = K − 9（仅因 512 = 2^9）。
  · aio_hips writer 口径：hips_order(properties) = tile_order = K − 9；
    A_cell = 4π/(12·nside²)，nside = 2^K；
    hips_pixel_scale[deg] = (180/π)·sqrt(π/3)/nside ⇒ Ω == (pixel_scale_rad)²。

本脚本对**已发布产品目录**做以下可判定核对：
  1. sidecar.signal.leaf_order == K；
  2. sidecar.precision.controls[*].leaf_order == K；
  3. sidecar.precision.controls[*].leaf_ipix < 12·4^K；
  4. signal/properties hips_order == K − 9；
  5. signal 叶 tile 目录 Norder == K − 9（磁盘实际路径）；
  6. hips_tile_width == 512；
  7. Ω(K) == (hips_pixel_scale deg→rad)²（独立公式对照，双向）；
  8. support_area_sr 是 Ω 的正整数倍（覆盖=N 个叶像元）；
  9. signal.leaf_order 不得等于 K − 9（即修复前 bug 的签名；K−9==K 不可能）。

用法：
    python3 verify_leaf_order.py <product_dir> [--expect-K K] [--json out.json]
<product_dir> 为 `<...>/<frame_key>`，其下含 frame_science.json 与 signal/。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys

PI = math.pi


def omegas(K: int) -> float:
    return 4.0 * PI / (12.0 * (4.0 ** K))


def pixel_scale_deg(K: int) -> float:
    return (180.0 / PI) * math.sqrt(PI / 3.0) / (2.0 ** K)


def norder_dirs(signal_dir: str) -> list[int]:
    if not os.path.isdir(signal_dir):
        return []
    out = []
    for name in os.listdir(signal_dir):
        m = re.fullmatch(r"Norder(\d+)", name)
        if m:
            out.append(int(m.group(1)))
    return sorted(out)


def read_props(path: str) -> dict:
    kv = {}
    if not os.path.isfile(path):
        return kv
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if "=" in line:
                k, v = line.split("=", 1)
                kv[k.strip()] = v.strip()
    return kv


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("product_dir")
    ap.add_argument("--expect-K", type=int, default=None)
    ap.add_argument("--json", default=None)
    ap.add_argument("--label", default=None)
    a = ap.parse_args()
    pr = a.product_dir.rstrip("/")
    sidecar = os.path.join(pr, "frame_science.json")
    signal = os.path.join(pr, "signal")
    props = read_props(os.path.join(signal, "properties"))

    checks: list[dict] = []

    def chk(name: str, ok: bool, detail: str) -> None:
        checks.append({"check": name, "pass": bool(ok), "detail": detail})

    if not os.path.isfile(sidecar):
        chk("sidecar_present", False, f"missing {sidecar}")
        _emit(a, pr, checks, None)
        return 1
    d = json.load(open(sidecar, encoding="utf-8"))
    sig = d.get("signal", {})
    sig_leaf = sig.get("leaf_order")
    controls = (d.get("precision", {}) or {}).get("controls", []) or []
    ctrl_orders = sorted({c.get("leaf_order") for c in controls})
    props_order = int(props["hips_order"]) if "hips_order" in props else None
    ndirs = norder_dirs(signal)
    tile_px = int(props.get("hips_tile_width", 0)) if props else 0
    px_scale = float(props["hips_pixel_scale"]) if "hips_pixel_scale" in props else None

    # 真叶阶 K 的来源（独立面，优先级：显式 > properties hips_order+9 > controls 一致值）
    cand: list[tuple[str, int]] = []
    if a.expect_K is not None:
        cand.append(("cli_expect_K", a.expect_K))
    if props_order is not None:
        cand.append(("properties.hips_order+9", props_order + 9))
    if len(ctrl_orders) == 1 and ctrl_orders[0] is not None:
        cand.append(("controls[*].leaf_order", int(ctrl_orders[0])))
    K = a.expect_K if a.expect_K is not None else (cand[0][1] if cand else None)

    chk("sidecar_present", True, sidecar)
    chk("K_determined", K is not None, f"K={K} candidates={cand}")
    if K is None:
        _emit(a, pr, checks, None)
        return 1

    # 独立来源必须一致（不允许"两个错字段互相相等"当 oracle）
    for src, v in cand:
        chk(f"K_source_consistent[{src}]", v == K, f"{src}={v} vs K={K}")

    chk("signal.leaf_order==K", sig_leaf == K,
        f"signal.leaf_order={sig_leaf} expected K={K}")
    chk("signal.leaf_order!=K-9(bug signature)", sig_leaf != (K - 9),
        f"signal.leaf_order={sig_leaf} K-9={K-9}")
    chk("controls.leaf_order==K", ctrl_orders == [K],
        f"controls leaf_order set={ctrl_orders} expected [{K}] (n_controls={len(controls)})")
    bad_ipix = [c.get("control_id") for c in controls
                if isinstance(c.get("leaf_ipix"), str)
                and c["leaf_ipix"].isdigit()
                and int(c["leaf_ipix"]) >= 12 * (4 ** K)]
    chk("controls.leaf_ipix<12*4^K", not bad_ipix,
        f"out-of-range controls={bad_ipix[:3]} n_controls={len(controls)}")
    chk("properties.hips_order==K-9", props_order == K - 9,
        f"properties hips_order={props_order} expected {K-9}")
    # 磁盘叶 tile 目录：最大 Norder 应等于 K-9（叶层；另有低阶 hierarchy 目录）
    chk("signal leaf Norder dir==K-9", (max(ndirs) if ndirs else None) == K - 9,
        f"Norder dirs={ndirs} expected leaf {K-9}")
    chk("hips_tile_width==512", tile_px == 512, f"tile_width={tile_px}")
    om = omegas(K)
    if px_scale is not None:
        om_ps = (math.radians(px_scale)) ** 2
        rel = abs(om_ps - om) / om
        chk("Omega(K)==(pixel_scale_rad)^2", rel < 5e-3,
            f"Omega={om:.6e} from_pixel_scale={om_ps:.6e} rel={rel:.3e}")
    else:
        chk("Omega(K)==(pixel_scale_rad)^2", False, "hips_pixel_scale absent")
    # support_area_sr = N * Omega（正整数倍）
    areas = [c.get("support_area_sr") for c in controls
             if isinstance(c.get("support_area_sr"), (int, float))]
    if areas:
        ratios = [x / om for x in areas]
        worst = max(abs(r - round(r)) for r in ratios)
        chk("support_area_sr==N*Omega", worst < 5e-3 and all(round(r) >= 1 for r in ratios),
            f"min/max N={min(round(r) for r in ratios)}/{max(round(r) for r in ratios)} worst_frac={worst:.3e}")
    else:
        chk("support_area_sr==N*Omega", False, "no control support_area_sr")

    _emit(a, pr, checks, K)
    return 0 if all(c["pass"] for c in checks) else 1


def _emit(a, pr, checks, K):
    label = a.label or pr
    res = {"product": pr, "label": label, "K": K,
           "n_checks": len(checks), "n_fail": sum(1 for c in checks if not c["pass"]),
           "checks": checks}
    for c in checks:
        print(f"  [{'PASS' if c['pass'] else 'FAIL'}] {c['check']} — {c['detail']}")
    print(f"  => {label}: K={K} FAIL={res['n_fail']}/{res['n_checks']}")
    if a.json:
        json.dump(res, open(a.json, "w", encoding="utf-8"), indent=2, ensure_ascii=False)


if __name__ == "__main__":
    sys.exit(main())
