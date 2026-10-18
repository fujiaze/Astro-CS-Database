#!/usr/bin/env python3
"""编译并运行 #80 P0-A1 生产罚法方程（deflation）单元测试。

被测 = lib/algorithms/coverage/src/sky_plane.cpp 的 sky_penalized_solve
（经 acsd_sky_plane_penalized_solve_for_test 接缝，同一编译产物；做法与
eng/tests/unit/run_p0_19_mixed_hips_order_test.py 同款：g++ 直编 + 链接已构建静态库）。

用法:
    python3 eng/tests/unit/run_sky_plane_deflation_test.py \
        [--build-dir build/linux-sci] [--dump <生产 matrix dump json>] [--out <readings.json>]

--dump 为可选的**生产真实矩阵 dump**（lib/algorithms/coverage/src/sky_plane.cpp 的
开发态取证快照，由 ACSD_DEV_MODE=ON + ACSD_DEV_SELECTOR 生成）；缺失时该段如实 SKIP。
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = os.path.join(REPO, "eng", "tests", "unit", "sky_plane_deflation_test.cpp")

INCLUDES = [
    "lib/include",
    "lib/third_party",
    "lib/algorithms/coverage/include",
    "lib/algorithms/coverage/src",
    "lib/algorithms/shared",
    "lib/infrastructure/aio/include",
    "lib/infrastructure/aio/src",
    "lib/infrastructure/scheduler/src",
    "lib/infrastructure/aio/io/include",
]

ARCHIVES = [
    "libacsd_module_adapters.a",
    "libacsd_phase2_session.a",
    "libacsd_phase3_session.a",
    "libacsd_phase1_session.a",
    "libacsd_phase2.a",
    "libacsd_cli_runtime.a",
    "libacsd_cpu.a",
    "libacsd_drizzle.a",
    "libacsd_aio.a",
    "libacsd_hips.a",
    "libacsd_hips_properties.a",
    "libacsd_cfitsio.a",
    "libacsd_core.a",
    "libacsd_common.a",
    "libacsd_io_adapter.a",
    "libacsd_calibration.a",
    "libacsd_identifiability.a",
    "libacsd_p1_sdet.a",
    "libacsd_p1_dpsf.a",
    "libacsd_p1_ipv.a",
    "libacsd_phase1_noise.a",
    "libacsd_phase1_phot.a",
    "libacsd_phase1_photcal.a",
    "libacsd_phase1_stars.a",
    "libacsd_phase1_wcs.a",
    "lib/algorithms/integration/normalize_science/libacsd_normalize_science.a",
    "lib/algorithms/integration/phase2_integrate/libacsd_phase2_integrate.a",
    "lib/algorithms/integration/phase1_product/libacsd_phase1_product.a",
    "lib/algorithms/projection/libacsd_p3_projection_wcs.a",
    "lib/algorithms/resample/libacsd_p3_rsmp.a",
    "lib/algorithms/fits_output/libacsd_p3_fits_output.a",
    "lib/infrastructure/aio/product_io/libacsd_product_io.a",
    "lib/infrastructure/pipeline/orchestrator/cpp/libacsd_infra_orchestrator.a",
    "lib/infrastructure/pipeline/orchestrator/cpp/libacsd_orchestrator_jsv.a",
    # DEV-MODE 构建才存在（OFF 时是 INTERFACE，无 .a，缺省跳过）
    "libacsd_dev_diagnostics.a",
    "libacsd_probes.a",
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build/linux-sci")
    ap.add_argument("--dump", default="")
    ap.add_argument("--out", default="/tmp/sky_plane_deflation_readings.json")
    ap.add_argument("--bin", default="/tmp/sky_plane_deflation_test")
    a = ap.parse_args()
    bd = os.path.join(REPO, a.build_dir)
    if not os.path.isdir(bd):
        print("SKIP: build dir not found: %s" % bd)
        return 0
    missing = [x for x in ARCHIVES if not os.path.exists(os.path.join(bd, x))]
    hard_missing = [x for x in missing if x not in ("libacsd_dev_diagnostics.a", "libacsd_probes.a")]
    if hard_missing:
        print("SKIP: missing archives in %s: %s" % (bd, ", ".join(hard_missing[:5])))
        return 0
    cmd = ["g++", "-std=c++17", "-O1", "-fopenmp"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC]
    cmd += ["-Wl,--start-group"]
    cmd += [os.path.join(bd, x) for x in ARCHIVES if os.path.exists(os.path.join(bd, x))]
    cmd += ["-Wl,--end-group", "-lz", "-lpthread", "-o", a.bin]
    print("compile:", " ".join(cmd[:8]), "...")
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print("COMPILE FAILED rc=%d" % rc)
        return rc
    argv = [a.bin, "--out", a.out]
    if a.dump:
        argv += ["--dump", a.dump]
    print("run:", " ".join(argv))
    return subprocess.run(argv).returncode


if __name__ == "__main__":
    sys.exit(main())
