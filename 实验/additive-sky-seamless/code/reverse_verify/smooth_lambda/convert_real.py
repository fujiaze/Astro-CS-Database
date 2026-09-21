#!/usr/bin/env python3
"""真实 L4 p2_samples.json -> UPMB v1（喂给 upm_sweep，链接真实 upm.cpp）。只读转换。"""
import json, os, sys
import numpy as np
_HERE = os.path.dirname(os.path.abspath(__file__))   # .../实验/<unit>/code/reverse_verify/smooth_lambda
ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))   # 仓库根（从脚本自身位置推导）
SRC = os.path.join(ROOT, "run/RELEASE-02/L4-rebuild/mosaic_out/p2_samples.json")
OUT = os.path.join(ROOT, "run/reverse_verify/smooth_lambda/real49.upmb")

d = json.load(open(SRC))
frames = np.array(d["frame_ids"], np.uint64)
ctrls = d["controls"]; obs = d["observations"]
K = len(ctrls); n = len(obs); F = len(frames)
with open(OUT, "wb") as f:
    f.write(np.uint32(0x55504D31).tobytes()); f.write(np.uint32(1).tobytes())
    f.write(np.uint64(n).tobytes()); f.write(np.uint64(K).tobytes()); f.write(np.uint64(F).tobytes())
    f.write(frames.tobytes())
    dt = np.dtype([("frame_id","<u8"),("control_id","<u8"),("leaf_ipix","<u8"),
                   ("ra","<f8"),("dec","<f8"),("value","<f8"),("unc","<f8"),
                   ("snr","<f8"),("ivar","<f8"),("cvar","<f8"),("civar","<f8"),
                   ("snrav","<i4"),("support","<f8"),("qflags","<u4")])
    a = np.zeros(n, dt)
    a["frame_id"] = np.array([o["frame_id"] for o in obs], np.uint64)
    a["control_id"] = np.array([o["control_id"] for o in obs], np.uint64)
    a["leaf_ipix"] = np.array([o["leaf_ipix"] for o in obs], np.uint64)
    a["ra"] = [o["ra_deg"] for o in obs]; a["dec"] = [o["dec_deg"] for o in obs]
    a["value"] = [o["value"] for o in obs]; a["unc"] = [o["uncertainty"] for o in obs]
    a["snr"] = [o["snr"] for o in obs]; a["ivar"] = [o["ivar"] for o in obs]
    a["cvar"] = [o["control_variance"] for o in obs]
    a["civar"] = [o["control_ivar"] for o in obs]
    a["snrav"] = [o["snr_available"] for o in obs]
    a["support"] = [o["support"] for o in obs]
    a["qflags"] = [o["quality_flags"] for o in obs]
    f.write(a.tobytes())
    nd = np.dtype([("control_id","<u8"),("tile_ipix","<u8"),("gx","<i4"),("gy","<i4"),
                   ("ra","<f8"),("dec","<f8"),("leaf_ipix","<u8")])
    na = np.zeros(K, nd)
    na["control_id"] = np.array([c["control_id"] for c in ctrls], np.uint64)
    na["tile_ipix"] = np.array([c["tile_ipix"] for c in ctrls], np.uint64)
    na["gx"] = [c["gx"] for c in ctrls]; na["gy"] = [c["gy"] for c in ctrls]
    na["ra"] = [c["ra_deg"] for c in ctrls]; na["dec"] = [c["dec_deg"] for c in ctrls]
    na["leaf_ipix"] = np.array([c["leaf_ipix"] for c in ctrls], np.uint64)
    f.write(na.tobytes())
print("wrote", OUT, os.path.getsize(OUT), "n_obs", n, "n_nodes", K, "n_frames", F)
