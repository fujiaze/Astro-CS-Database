#!/usr/bin/env python3
"""Issue #92 —— 冻结代码/二进制/真实输入身份 (main-only, 真实输入只读)。

输出 evidence/00-identity.json:
  · git HEAD / 分支 / 工作树状态
  · 生产二进制路径 + sha256
  · 六帧真实 FITS + 三张 master 定标 + 运行配置 + 冻结/本次 p1_wcs.json 的 sha256
  · Gaia XPSD 目录 manifest (name,size) —— 64GB 全量 sha256 记 NOT_RUN
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def sha256(path: str, chunk: int = 1 << 22) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(chunk), b""):
            h.update(b)
    return h.hexdigest()


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True,
                          text=True).stdout.strip()


def main() -> int:
    out_path = os.path.join(os.path.dirname(__file__), "..", "evidence", "00-identity.json")
    td = os.path.join(REPO, "testdata", "M42_T2T3_mosaic_Flying_dutchman", "T3", "M1")
    cal = os.path.join(REPO, "testdata", "T3 calibration files")
    cfg = json.load(open("/workspace/issue92-scratch/cfg6_issue92.json"))
    blk = cfg["blocks"][0]
    masters = [blk["master_bias"], blk["master_dark"], blk["master_flat"]]
    inputs = {}
    for p in list(blk["input_lights"]) + masters:
        inputs[p] = {"sha256": sha256(p), "bytes": os.path.getsize(p)}

    gaia_dir = blk["wcs"]["gaia_data_dir"]
    gaia = sorted(
        ({"name": n, "bytes": os.path.getsize(os.path.join(gaia_dir, n))}
         for n in os.listdir(gaia_dir)), key=lambda x: x["name"])

    evidence_dir = os.path.dirname(out_path)
    extra = {}
    for name in ("50-cfg6.json",):
        p = os.path.join(REPO, "实验/issue12-simpson-domain/evidence", name)
        if os.path.exists(p):
            extra[name] = sha256(p)

    ident = {
        "git": {
            "head": git("rev-parse", "HEAD"),
            "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
            "origin_main": git("rev-parse", "origin/main"),
            "dirty_tracked": [l for l in git("status", "--porcelain").splitlines()
                              if not l.startswith("??")],
        },
        "binary": {
            "path": os.path.join(REPO, "build/linux-sci-p5/acsd"),
            "sha256": sha256(os.path.join(REPO, "build/linux-sci-p5/acsd")),
            "bytes": os.path.getsize(os.path.join(REPO, "build/linux-sci-p5/acsd")),
        },
        "config_run": {"path": "/workspace/issue92-scratch/cfg6_issue92.json",
                       "sha256": sha256("/workspace/issue92-scratch/cfg6_issue92.json"),
                       "source_evidence": "实验/issue12-simpson-domain/evidence/50-cfg6.json",
                       "output_dir": blk["output_dir"]},
        "input_lights_and_masters": inputs,
        "gaia_catalog": {
            "dir": gaia_dir,
            "files": gaia,
            "total_bytes": sum(g["bytes"] for g in gaia),
            "full_sha256": "NOT_RUN (64GB XPSD; 逐文件 sha256 代价过高, 已记录 name+bytes 清单)",
        },
        "evidence_hashes": extra,
        "prior_artifacts_used": {
            "frozen_p1_wcs_frame1": "/workspace/issue12-scratch/norm6_pre.p1-work/"
                                    "M42_M1_T3_flying_dutchman-20251126_041940-300S-Red/p1_wcs.json",
            "frozen_p1_wcs_frame1_sha256": sha256(
                "/workspace/issue12-scratch/norm6_pre.p1-work/"
                "M42_M1_T3_flying_dutchman-20251126_041940-300S-Red/p1_wcs.json"),
        },
    }
    json.dump(ident, open(out_path, "w"), indent=2, ensure_ascii=False)
    print(json.dumps({k: ident[k] for k in ("git", "binary", "config_run")},
                     indent=2, ensure_ascii=False))
    print("inputs:", len(inputs), "| gaia files:", len(gaia),
          "| total GB: %.1f" % (ident["gaia_catalog"]["total_bytes"] / 1e9))
    return 0


if __name__ == "__main__":
    sys.exit(main())
