#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FIX-205 端到端负例: 请求未实现投影 ⇒ rc≠0 + 明确原因（不得静默回落 TAN）。

规范依据:
  * ASTROCS_DESIGN.md §5.3（8 投影冻结; 未实现的必须显式报「不支持」,
    禁止声称支持; 当前登记仅 TAN 已实现）;
  * CONTROL_PACK_SPEC.md §5（验收门必须机器可复跑）。

判据（全部 rc + 文本双断言, 能红能绿）:
  N1 对 8 冻结码中 7 个未实现者 + 1 个未知码:  export --json <cfg> -y
     ⇒ rc != 0, 且输出含请求码 / "unsupported" / 已支持清单 "supported projections: TAN";
  N2 对照（TAN）: 投影门必须放行 —— 输出不得出现上述「不支持」文本
     （证明 N1 的拒绝由投影码引起, 而非请求整体恒失败）。

用法: python3 p3_projection_unsupported_cli.py <astrocs 可执行文件>
exit 0 = 判据全过; 1 = 判据失败; 2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile

UNIMPLEMENTED = ["SIN", "CAR", "AIT", "STG", "MOL", "CEA", "ZEA", "ZZZ"]
SUPPORTED_MARK = "supported projections: TAN"


def run_export(exe: str, projection: str, tmp: str):
    """在临时目录内以最小配置跑 export; 返回 (rc, 输出文本)。"""
    hips = os.path.join(tmp, "hips")
    os.makedirs(os.path.join(hips, "signal"), exist_ok=True)
    with open(os.path.join(hips, "signal", "properties"), "w", encoding="utf-8") as fh:
        fh.write("hips_order = 3\n")
    cfg = {
        "source": {"hips_dir": hips},
        "center": {"ra_deg": 150.0, "dec_deg": 2.0},
        "output_dir": os.path.join(tmp, "out_" + projection),
        "scale_deg_per_px": 0.0001389,
        "width_px": 64,
        "height_px": 64,
        "projection": projection,
    }
    cfg_path = os.path.join(tmp, "cfg_%s.json" % projection)
    with open(cfg_path, "w", encoding="utf-8") as fh:
        json.dump(cfg, fh)
    r = subprocess.run([exe, "export", "--json", cfg_path, "-y"],
                       capture_output=True, text=True, timeout=300)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


def main() -> int:
    if len(sys.argv) < 2 or not os.path.isfile(sys.argv[1]):
        print("FAIL: astrocs 可执行文件不可用（fail-closed）", file=sys.stderr)
        return 2
    exe = os.path.abspath(sys.argv[1])
    bad = 0
    tmp = tempfile.mkdtemp(prefix="fix205_proj_cli_")
    try:
        for code in UNIMPLEMENTED:
            rc, out = run_export(exe, code, tmp)
            if rc == 0:
                print("FAIL: projection=%s 静默通过（rc=0）" % code, file=sys.stderr)
                bad += 1
                continue
            for needle, what in ((code, "请求码"), ("unsupported", "「不支持」"),
                                 (SUPPORTED_MARK, "已支持清单")):
                if needle not in out:
                    print("FAIL: projection=%s 输出缺 %s（%r）" % (code, what, needle),
                          file=sys.stderr)
                    bad += 1
            print("ok: projection=%-3s rc=%d 明确报不支持" % (code, rc))
        # 对照: TAN 必须过投影门（拒绝文本不得出现）
        rc_tan, out_tan = run_export(exe, "TAN", tmp)
        if "unsupported" in out_tan or SUPPORTED_MARK in out_tan:
            print("FAIL: 对照 TAN 被投影门拒绝（判据退化）", file=sys.stderr)
            bad += 1
        else:
            print("ok: 对照 TAN 通过投影门（rc=%d, 后续失败与投影无关）" % rc_tan)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if bad:
        print("FIX-205 CLI NEGATIVE FAIL (%d)" % bad, file=sys.stderr)
        return 1
    print("FIX-205 CLI NEGATIVE PASS: 未实现投影 rc!=0 + 明确原因 + 已支持清单")
    return 0


if __name__ == "__main__":
    sys.exit(main())
