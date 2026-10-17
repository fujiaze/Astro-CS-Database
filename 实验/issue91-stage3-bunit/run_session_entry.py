#!/usr/bin/env python3
"""Issue #91：正式 session 入口（p3_session_run）前后对比证据。

p3_session_run 不在 CLI 的 phase3 IR 节点链上（见 REPORT「入口可达性」），
因此只能经其**生产 C API** 驱动：编译 session_entry_probe.cpp 并链接
acsd_phase3_session 及其生产链接闭包（本脚本对同一份源码分别链接修复前/后构建）。

用法:
  python3 实验/issue91-stage3-bunit/run_session_entry.py \
      --src /tmp/acsd-issue91-base --build /tmp/acsd-issue91-base-build --tag base
  python3 实验/issue91-stage3-bunit/run_session_entry.py \
      --src "$PWD" --build build/linux-dev --tag fixed
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.join(HERE, "evidence")
WORK = os.path.join(HERE, "work")
PROBE = os.path.join(HERE, "session_entry_probe.cpp")

LIBS = [
    "libacsd_phase3_session.a",
    "lib/algorithms/fits_output/libacsd_p3_fits_output.a",
    "lib/algorithms/resample/libacsd_p3_rsmp.a",
    "lib/algorithms/projection/libacsd_p3_projection_wcs.a",
    "libacsd_hips_properties.a",
    "libacsd_hips.a",
    "libacsd_aio.a",
    "libacsd_common.a",
    "libacsd_cfitsio.a",
]
INCLUDES = ["lib/phase3_session", "lib/include", "lib/include/acsd",
            "lib/algorithms/projection", "lib/algorithms/resample",
            "lib/algorithms/fits_output", "lib/algorithms/shared",
            "lib/algorithms/shared/crypto", "lib/algorithms/shared/healpix",
            "lib/infrastructure/aio/include"]

# 覆盖：合法（canonical / 裸 ADU+语义 / 等价幂次写法） / 缺失 / 空 / 重复 / 声明面冲突 /
#       语义矛盾 / legacy 像元面积别名
CASES = ["legal_explicit", "legal_bare_adu_sb", "legal_sr_power_written",
         "missing_bunit", "empty_bunit", "case_alias_dup", "dup_diff",
         "alias_conflict", "semantics_conflict", "legacy_px2_alias"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--build", required=True)
    ap.add_argument("--tag", required=True)
    args = ap.parse_args()
    src = os.path.abspath(args.src)
    build = os.path.abspath(args.build)
    out_dir = os.path.join(EVID, args.tag)
    os.makedirs(out_dir, exist_ok=True)

    exe = os.path.join(out_dir, "session_entry_probe")
    cmd = ["g++", "-std=c++17", "-O2", "-o", exe, PROBE]
    cmd += ["-I" + os.path.join(src, i) for i in INCLUDES]
    cmd += ["-I" + build]
    cmd += [os.path.join(build, l) for l in LIBS]
    cmd += ["-lz", "-lpthread", "-lm"]
    print("+ " + " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print(f"compile failed rc={rc}", file=sys.stderr)
        return rc

    results = []
    for case in CASES:
        hips = os.path.join(WORK, "variants", case)
        od = os.path.join(WORK, "out_session", args.tag, case)
        if os.path.isdir(od):
            shutil.rmtree(od)
        os.makedirs(od)
        proc = subprocess.run([exe, hips, od], capture_output=True, text=True)
        rec = {"case": case, "hips_dir": hips, "out_dir": od,
               "probe_rc": proc.returncode}
        try:
            rec.update(json.loads(proc.stdout.strip().splitlines()[-1]))
        except Exception:
            rec["raw_stdout"] = proc.stdout
            rec["raw_stderr"] = proc.stderr
        rec["published_fits"] = os.path.isfile(os.path.join(od, "output_phase3.fits"))
        results.append(rec)
        print(f"[{args.tag}] {case:22s} status={rec.get('status')} "
              f"({rec.get('status_name')}) fits={rec['published_fits']} "
              f"err={str(rec.get('last_error',''))[:80]}")

    out = {
        "tag": args.tag,
        "src": src,
        "build": build,
        "probe_sha256": hashlib.sha256(open(PROBE, "rb").read()).hexdigest(),
        "results": results,
    }
    path = os.path.join(out_dir, "session_entry.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
