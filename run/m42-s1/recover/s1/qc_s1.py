#!/usr/bin/env python3
"""Independent S1 QC: the three Normalize products, the Mosaic and the Export.

Read only.  The product encodes every payload with its semantic identity
(`acsd.Array:...`) in `identity.object_id`, while the manifest lists the content
hash; references use the semantic identity.
"""
import glob
import json
import os
import sys

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

ROOT = "/workspace/Astro CS Database"
S1 = os.path.join(ROOT, "run/m42-s1/recover/s1")

DTYPE = {"f64": "<f8", "u64": "<u8", "u8": "u1", "u16": "<u2", "f32": "<f4", "i64": "<i8"}


class Product:
    def __init__(self, directory):
        self.root = directory
        self.manifest = json.load(open(os.path.join(directory, "manifest.json")))
        self.blocks = {b["id"]: os.path.join(directory, b["path"]) for b in self.manifest["blocks"]}
        self.by_id = {}
        for entry in self.manifest["objects"]:
            payload = json.load(open(os.path.join(directory, entry["path"])))["data"]
            self.by_id[payload["identity"]["object_id"]] = payload
        self.sidecar = json.load(open(os.path.join(
            directory, self.manifest.get("sidecar", "frame_science.json"))))["data"] \
            if os.path.exists(os.path.join(directory, "frame_science.json")) else None
        if self.sidecar is None:
            for name in ("mosaic_science.json", "export_science.json"):
                path = os.path.join(directory, name)
                if os.path.exists(path):
                    self.sidecar = json.load(open(path))["data"]
                    break

    def array(self, ref):
        payload = self.by_id[ref] if isinstance(ref, str) else ref
        data = b"".join(open(self.blocks[b], "rb").read() for b in payload["block_ids"])
        return np.frombuffer(data, dtype=DTYPE[payload["dtype"]])

    def signal_parts(self):
        estimate = self.by_id[self.sidecar["signal"]["$ref"]]
        support = self.by_id[estimate["support"]["$ref"]]
        index = self.by_id[support["index"]["$ref"]]
        ids = self.array(index["ids"]["$ref"]).reshape(-1, 2)
        return {
            "estimate": estimate,
            "support": support,
            "ids": ids,
            "signal": self.array(estimate["signal"]["$ref"]),
            "lower": self.array(estimate["numerical_lower"]["$ref"])
            if estimate.get("numerical_lower") else None,
            "upper": self.array(estimate["numerical_upper"]["$ref"])
            if estimate.get("numerical_upper") else None,
            "states": self.array(support["states"]["$ref"]),
            "area_lower": self.array(support["area_lower_sr"]["$ref"]),
            "area_upper": self.array(support["area_upper_sr"]["$ref"]),
        }

    def check_closure(self):
        declared_ids = set(self.by_id)
        refs = set()

        def walk(node):
            if isinstance(node, dict):
                if "$ref" in node and isinstance(node["$ref"], str):
                    refs.add(node["$ref"])
                for value in node.values():
                    walk(value)
            elif isinstance(node, list):
                for value in node:
                    walk(value)

        for payload in self.by_id.values():
            walk(payload)
        return sorted(refs - declared_ids)

    def tiles(self):
        files = sorted(glob.glob(os.path.join(self.root, "signal", "*", "*", "*.fits")))
        report = {"files": len(files), "bad_cards": [], "finite": 0, "rows": []}
        for path in files:
            header = fits.getheader(path)
            data = fits.getdata(path)
            report["finite"] += int(np.isfinite(data).sum())
            for card in header.cards:
                try:
                    _ = card.value
                except Exception as exc:  # noqa: BLE001
                    report["bad_cards"].append(f"{os.path.basename(path)}:{card.keyword}:{exc}")
            if len(report["rows"]) < 2:
                report["rows"].append({"path": os.path.relpath(path, self.root) + ".fits",
                                       "shape": list(data.shape),
                                       "bunit": header.get("BUNIT"),
                                       "cts": header.get("CTYPE1")})
        return report


def main():
    report = {"products": {}, "pairwise": {}, "export": None}
    products = []
    for i in range(3):
        directory = glob.glob(os.path.join(S1, f"f{i}", "out", "*"))[0]
        products.append(Product(directory))

    for i, product in enumerate(products):
        parts = product.signal_parts()
        manifest = product.manifest
        estimate = parts["estimate"]
        report["products"][f"f{i}"] = {
            "directory": product.root,
            "manifest_complete": manifest["complete"],
            "manifest_assessment": manifest["assessment"],
            "sidecar_assessment": product.sidecar["assessment"],
            "estimate_assessment": estimate["assessment"],
            "objects": len(manifest["objects"]),
            "dangling_refs": product.check_closure(),
            "leaves": int(len(parts["ids"])),
            "supported": int((parts["states"] == 1).sum()),
            "unresolved": int((parts["states"] == 2).sum()),
            "absent": int((parts["states"] == 0).sum()),
            "signal_finite": int(np.isfinite(parts["signal"]).sum()),
            "area_upper_sr": float(np.nansum(parts["area_upper"])),
            "area_lower_sr": float(np.nansum(parts["area_lower"])),
            "unit": estimate["unit"],
            "tiles": product.tiles(),
        }
        products[i]._parts = parts

    maps = [{(int(o), int(p)): float(s)
             for (o, p), s in zip(p._parts["ids"], p._parts["signal"])}
            for p in products]
    common = set(maps[0]) & set(maps[1]) & set(maps[2])
    report["common_leaves"] = len(common)
    for a, b in ((0, 1), (0, 2), (1, 2)):
        ratios, diffs = [], []
        for leaf in common:
            x, y = maps[a][leaf], maps[b][leaf]
            if np.isfinite(x) and np.isfinite(y) and abs(x) > 0 and abs(y) > 0:
                ratios.append(y / x)
                diffs.append(y - x)
        ratios, diffs = np.array(ratios), np.array(diffs)
        report["pairwise"][f"f{a}_over_f{b}"] = {
            "n": int(len(ratios)),
            "ratio_median": float(np.median(ratios)),
            "ratio_p16": float(np.percentile(ratios, 16)),
            "ratio_p84": float(np.percentile(ratios, 84)),
            "diff_median": float(np.median(diffs)),
            "diff_mad": float(np.median(np.abs(diffs - np.median(diffs)))),
        }

    mosaic_root = os.path.join(S1, "mosaic", "mosaic")
    if os.path.exists(os.path.join(mosaic_root, "manifest.json")):
        mosaic = Product(mosaic_root)
        parts = mosaic.signal_parts()
        report["mosaic"] = {
            "manifest_complete": mosaic.manifest["complete"],
            "assessment": mosaic.sidecar["assessment"],
            "leaves": int(len(parts["ids"])),
            "supported": int((parts["states"] == 1).sum()),
            "unresolved": int((parts["states"] == 2).sum()),
            "absent": int((parts["states"] == 0).sum()),
            "dangling_refs": mosaic.check_closure(),
            "tiles": mosaic.tiles(),
        }
        fit = mosaic.sidecar.get("sky")
        if fit:
            report["mosaic"]["sky_beta"] = fit.get("beta")
        else:
            # The sidecar carries no SkyFit object; the closure stores the
            # numeric beta as a graph array.  Record the absence explicitly
            # instead of silently skipping the check.
            report["mosaic"]["sky_beta"] = None
            report["mosaic"]["sky_beta_note"] = (
                "not in the sidecar; the numeric beta is the graph's sky_beta array")

    export_root = os.path.join(S1, "mosaic", "export")
    paths = glob.glob(os.path.join(export_root, "signal.fits"))
    if paths:
        path = paths[0]
        with fits.open(path) as hdul:
            header = hdul[0].header
            data = np.asarray(hdul[0].data, dtype=float)
        wcs = WCS(header)
        finite = np.isfinite(data)
        corners = np.array([[0, 0], [data.shape[1] - 1, 0],
                            [0, data.shape[0] - 1], [data.shape[1] - 1, data.shape[0] - 1]])
        back = np.array(wcs.all_world2pix(wcs.all_pix2world(corners, 0), 0))
        bad = []
        for card in header.cards:
            try:
                _ = card.value
            except Exception as exc:  # noqa: BLE001
                bad.append(f"{card.keyword}: {exc}")
        cd = np.array([[header.get("CD1_1", 0), header.get("CD1_2", 0)],
                       [header.get("CD2_1", 0), header.get("CD2_2", 0)]], dtype=float)
        report["export"] = {
            "path": path,
            "shape": list(data.shape),
            "finite": int(finite.sum()),
            "nan_fraction": float(1 - finite.sum() / data.size),
            "median": float(np.nanmedian(data)),
            "mad": float(np.nanmedian(np.abs(data - np.nanmedian(data)))),
            "bunit": header.get("BUNIT"),
            "acsunit": header.get("ACSUNIT"),
            "ctype1": header.get("CTYPE1"),
            "crpix": [header.get("CRPIX1"), header.get("CRPIX2")],
            "crval": [header.get("CRVAL1"), header.get("CRVAL2")],
            "cd": cd.tolist(),
            "pixel_scale_arcsec": float(np.sqrt(abs(np.linalg.det(cd))) * 3600.0),
            "sip_order": header.get("A_ORDER"),
            "wcs_roundtrip_max_px": float(np.max(np.abs(back - corners))),
            "bad_cards": bad,
        }
    print(json.dumps(report, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
