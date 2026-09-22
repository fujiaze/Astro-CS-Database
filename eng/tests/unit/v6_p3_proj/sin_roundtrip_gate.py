#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/tests/unit/v6_p3_proj/sin_roundtrip_gate.py — FIX-406 v6 SIN 内核偏差「复现门」。

语义（CLEAN-401 保留登记的一部分；**修好即转红**）:
  - v6 SIN 内核 sin_world2pix 用 ctheta = sqrt(1 - stheta^2)（p3_proj_v6.cpp:195），
    投影中心 theta→pi/2 时灾难性消去 ⇒ pixel→world→pixel 往返误差无上界。
  - 本门**编译真实内核**（p3_proj_v6.cpp，只调公共 API，不复制内部公式当 Oracle），
    在投影中心邻域与两个像素角尺度上实测最大往返误差：
      * 偏差仍复现（max_err >= TOL_REPRODUCE）⇒ rc 0（绿）——登记块与
        lib/algorithms/projection/p3_projection_registry.h 的「已知偏差登记」仍然成立；
      * 偏差消失（max_err < TOL_REPRODUCE）⇒ rc 1（红）——内核已被修好，
        **必须**同步：① p3_proj_v6.h/.cpp 的 RETIRED-CODE-RETAINED 已知缺陷段；
        ② p3_projection_registry.h 的「已知偏差登记（FIX-406）」段与 SIN 行状态；
        ③ run/FIX-406/SIN_ROUNDTRIP_ORACLE.md 的结论。
  - 契约容差单一事实源 = p3_wcs_applicability（TAN: 1e-8 px）。本门不写第二份容差，
    只用 TOL_REPRODUCE = 1e-6 px 作为「偏差是否仍可测」的判定阈值（FIX-406 报告实测
    2.5e-5 px @0.5"/px，比该阈值高 25×；astropy/WCSLIB 参考面 ~2.5e-10 px，低 4 个量级）。

依据: RELEASE-04 GAP_AUDIT G3-6（包已出库）；任务书 CLEAN-401 步骤 7（v6 SIN 内核转 FIX-406）；
      证据 run/FIX-406/SIN_ROUNDTRIP_ORACLE.md、run/FIX-406/evidence/kernel_roundtrip.json。
用法: python3 eng/tests/unit/v6_p3_proj/sin_roundtrip_gate.py [--repo .] [--json-out P] [--quiet]
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

TOL_REPRODUCE_PX = 1e-6  # 偏差仍可测的判定阈值（见文件头；非契约容差）
# 对照打印用（**不是**本门的判定阈值）: p3_wcs_applicability("TAN") 的**紧门**。
# 注意适用域（GATE-WCS-01 裁决 1/4）: 紧门仅在 scale ≥ min_scale_arcsec = 0.9″/px
# 时有保守性证据; 本门扫描尺度 0.5″/px 低于该下限 ⇒ 适用的是**全域保守门**
# 1e-6 px（= TOL_REPRODUCE_PX, G-P1-WCS-BRIDGE-GLOBAL）。SIN 偏差 2.5e-5 px
# 两个门都远超 ⇒ 结论不受影响。
CONTRACT_TOL_PX = 1e-8   # 紧门（对照打印用; 单值来源见 lib/.../p3_wcs.cpp）

PROBE_SRC = r"""
#include "p3_proj_v6.h"
#include <cmath>
#include <cstdio>
using astrocs::phase3proj::v6::Descriptor;
using astrocs::phase3proj::v6::ProjectionId;
using astrocs::phase3proj::v6::ProjStatus;

static double rt(const Descriptor& d, double x, double y) {
    double ra, dec, x2, y2;
    if (astrocs::phase3proj::v6::pix2world(&d, x, y, &ra, &dec) != ProjStatus::kOk) return -1.0;
    if (astrocs::phase3proj::v6::world2pix(&d, ra, dec, &x2, &y2) != ProjStatus::kOk) return -1.0;
    return std::hypot(x2 - x, y2 - y);
}

static double scan(double scale_deg_per_px, double* at_x, double* at_y) {
    Descriptor d{};
    if (astrocs::phase3proj::v6::make(ProjectionId::kSIN, 150.0, 2.0, scale_deg_per_px,
                                      64, 64, "east_left", 0.0, &d) != ProjStatus::kOk)
        return -1.0;
    const double cx = 32.5, cy = 32.5;   /* FITS 1-based 中心 ⇒ 0-based 31.5；两侧各扫 */
    double worst = -1.0;
    for (int iy = -40; iy <= 40; ++iy) {
        for (int ix = -40; ix <= 40; ++ix) {
            const double x = cx + ix * 0.25, y = cy + iy * 0.25;
            if (x < 0.5 || y < 0.5 || x > 63.5 || y > 63.5) continue;
            const double e = rt(d, x, y);
            if (e > worst) { worst = e; *at_x = x; *at_y = y; }
        }
    }
    return worst;
}

int main() {
    const double scales[2] = {0.0001388888888888889 /*0.5"/px*/, 0.00001388888888888889 /*0.05"/px*/};
    std::printf("{\"cases\":[");
    for (int i = 0; i < 2; ++i) {
        double ax = 0, ay = 0;
        const double e = scan(scales[i], &ax, &ay);
        std::printf("%s{\"scale_deg_per_px\":%.17g,\"max_err_px\":%.17g,\"at_x\":%.17g,\"at_y\":%.17g}",
                    i ? "," : "", scales[i], e, ax, ay);
    }
    std::printf("]}\n");
    return 0;
}
"""


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="FIX-406 v6 SIN 内核偏差复现门（修好即转红）")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    repo = pathlib.Path(args.repo).resolve()
    src = repo / "lib" / "algorithms" / "projection" / "p3_proj_v6.cpp"
    hdr = repo / "lib" / "algorithms" / "projection" / "p3_proj_v6.h"
    if not src.is_file() or not hdr.is_file():
        print("SIN-GATE_ANCHOR_STALE: 内核源/头不存在 %s" % src)
        return 2

    with tempfile.TemporaryDirectory(prefix="sin_gate_") as td:
        td = pathlib.Path(td)
        probe = td / "probe.cpp"
        probe.write_text(PROBE_SRC, encoding="utf-8")
        exe = td / "probe"
        cmd = ["g++", "-std=c++17", "-O2", "-I", str(repo / "lib" / "algorithms" / "projection"),
               str(probe), str(src), "-o", str(exe)]
        cp = subprocess.run(cmd, capture_output=True, text=True)
        if cp.returncode != 0:
            print("SIN-GATE_BUILD_FAIL: g++ rc=%d\n%s" % (cp.returncode, cp.stderr[-2000:]))
            return 2
        rp = subprocess.run([str(exe)], capture_output=True, text=True)
        if rp.returncode != 0:
            print("SIN-GATE_PROBE_FAIL: rc=%d\n%s" % (rp.returncode, rp.stderr[-2000:]))
            return 2
        data = json.loads(rp.stdout.strip().splitlines()[-1])

    worst = max(c["max_err_px"] for c in data["cases"])
    reproduced = worst >= TOL_REPRODUCE_PX
    data["tol_reproduce_px"] = TOL_REPRODUCE_PX
    data["contract_tol_px"] = CONTRACT_TOL_PX
    data["worst_max_err_px"] = worst
    data["defect_reproduced"] = reproduced
    data["verdict"] = "REPRODUCED" if reproduced else "FIXED"

    if args.json_out:
        p = pathlib.Path(args.json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")

    if not args.quiet:
        print("=== FIX-406 v6 SIN roundtrip gate（修好即转红）===")
        for c in data["cases"]:
            print("  scale=%.6g deg/px  max_err=%.6g px  @(%.3f, %.3f)"
                  % (c["scale_deg_per_px"], c["max_err_px"], c["at_x"], c["at_y"]))
        print("  worst=%.6g px  tol_reproduce=%.6g px  contract_tol=%.6g px"
              % (worst, TOL_REPRODUCE_PX, CONTRACT_TOL_PX))

    if reproduced:
        print("SIN-GATE_PASS(REPRODUCED): 偏差仍复现 ⇒ 保留登记成立（p3_proj_v6.h/.cpp + "
              "p3_projection_registry.h）")
        return 0
    print("SIN-GATE_FAIL(FIXED): 偏差已消失 ⇒ 内核已被修好，必须同步退役登记："
          "① p3_proj_v6.h/.cpp 的 RETIRED-CODE-RETAINED 已知缺陷段；"
          "② p3_projection_registry.h 的「已知偏差登记（FIX-406）」与 SIN 行状态；"
          "③ run/FIX-406/SIN_ROUNDTRIP_ORACLE.md 结论。")
    return 1


if __name__ == "__main__":
    sys.exit(main())
