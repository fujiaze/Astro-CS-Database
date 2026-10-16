#!/usr/bin/env python3
"""编译并运行 P0-19 跨 hips_order fail-closed 生产入口测试。

被测 = lib/infrastructure/scheduler/src/module_adapters.cpp（生产函数本体）。
与仓内既有 C++ 单测同款做法（eng/tests/unit/run_photo_identity_test.py）:
g++ 直编测试源 + 链接已构建静态库, 不新增 CMake 目标。

用法:
    python3 eng/tests/unit/run_p0_19_mixed_hips_order_test.py \
        [--build-dir build/linux-sci-p5] [--fixtures /tmp/p019/fx]

--fixtures 为真实可读 HiPS 夹具根（含 ord0_halpha/ord1_halpha/ord0_red）;
缺失时 A 段如实 SKIP（不谎报 PASS）。夹具由
实验/p0-19-mixed-hips-order/code/make_hips_fixture.cpp 用生产写侧生成。
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = os.path.join(REPO, "eng", "tests", "unit", "p0_19_mixed_hips_order_test.cpp")

INCLUDES = [
    "lib/include",
    "lib/third_party",
    "lib/algorithms/coverage/include",
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
]

SRCS = []  # crypto/healpix 已由 libacsd_common.a 提供（避免重复定义）


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build/linux-sci-p5")
    ap.add_argument("--fixtures", default="/tmp/p019/fx")
    ap.add_argument("--out", default="/tmp/p0_19_mixed_hips_order_test")
    a = ap.parse_args()
    bd = os.path.join(REPO, a.build_dir)
    if not os.path.isdir(bd):
        print("SKIP: build dir not found: %s" % bd)
        return 0
    missing = [x for x in ARCHIVES if not os.path.exists(os.path.join(bd, x))]
    if missing:
        print("SKIP: missing archives in %s: %s" % (bd, ", ".join(missing[:5])))
        return 0
    cmd = ["g++", "-std=c++17", "-O1", "-fopenmp"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [SRC]
    cmd += ["-Wl,--start-group"]
    cmd += [os.path.join(bd, x) for x in ARCHIVES]
    cmd += [os.path.join(REPO, s) for s in SRCS]
    cmd += ["-Wl,--end-group", "-lz", "-lpthread", "-o", a.out]
    print("compile:", " ".join(cmd))
    rc = subprocess.run(cmd).returncode
    if rc != 0:
        print("COMPILE FAILED rc=%d" % rc)
        return rc
    print("run:", a.out)
    return subprocess.run([a.out, a.fixtures, "/tmp/p019_cxx_test"]).returncode


if __name__ == "__main__":
    sys.exit(main())
