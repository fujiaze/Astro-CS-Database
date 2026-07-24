#!/usr/bin/env python3
"""
Gaia XPSD 全天区覆盖验证脚本
功能：使用 gaia_client.dll 在关键天区做 cone search，
验证数据库全天球覆盖完整性（赤道、南北天极、南天备份区）
用途：DLL编译后验证、问题诊断

修复记录（2026-06-07）：
- 原 bbox 计算仅用 AzimuthalEquidistant 树的4个角点，
  遗漏投影中心(0,0)→极点，导致两极查询返回0结果
- 修复后全天区查询正常：北天极 8927颗, 南天极 13904颗
"""
import os
import sys
import ctypes

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DLL_DIR = os.path.dirname(SCRIPT_DIR)
PROJECT_ROOT = os.path.dirname(os.path.dirname(DLL_DIR))

DLL_NAME = "gaia_client.dll" if os.name == "nt" else "libgaia_client.so"
DLL_PATH = os.path.join(DLL_DIR, DLL_NAME)

if not os.path.exists(DLL_PATH):
    sys.exit(f"DLL不存在: {DLL_PATH}  请先执行 make dll 编译")

dll = ctypes.CDLL(DLL_PATH)

dll.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
dll.gaia_client_create_ex.restype = ctypes.c_void_p

dll.gaia_client_destroy.argtypes = [ctypes.c_void_p]
dll.gaia_client_destroy.restype = None

dll.gaia_client_cone_search_for_solver.argtypes = [
    ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double,
    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ctypes.POINTER(ctypes.c_int),
]
dll.gaia_client_cone_search_for_solver.restype = ctypes.c_int

dll.gaia_client_get_db_type.argtypes = [ctypes.c_void_p]
dll.gaia_client_get_db_type.restype = ctypes.c_int

dll.gaia_client_get_file_count.argtypes = [ctypes.c_void_p]
dll.gaia_client_get_file_count.restype = ctypes.c_int

dll.gaia_client_get_total_sources.argtypes = [ctypes.c_void_p]
dll.gaia_client_get_total_sources.restype = ctypes.c_int


def cone_search(client, ra, dec, radius, mag_high, label):
    ra_ptr = ctypes.POINTER(ctypes.c_double)()
    dec_ptr = ctypes.POINTER(ctypes.c_double)()
    mag_ptr = ctypes.POINTER(ctypes.c_float)()
    n = ctypes.c_int()

    dll.gaia_client_cone_search_for_solver(
        client,
        ctypes.c_double(ra), ctypes.c_double(dec),
        ctypes.c_double(radius), ctypes.c_double(mag_high),
        ctypes.byref(ra_ptr), ctypes.byref(dec_ptr),
        ctypes.byref(mag_ptr), ctypes.byref(n)
    )

    count = n.value
    if count > 0:
        mags = [f"{mag_ptr[i]:.2f}" for i in range(min(count, 5))]
        print(f"  {label:30s} RA={ra:7.2f} Dec={dec:+7.2f} r={radius:.1f}deg mag<{mag_high} "
              f"-> OK {count:>6d} stars  mag_sample={mags}")
    else:
        print(f"  {label:30s} RA={ra:7.2f} Dec={dec:+7.2f} r={radius:.1f}deg mag<{mag_high} "
              f"-> FAIL 0 stars (no data!)")

    if count > 0:
        ctypes.cdll.msvcrt.free(ra_ptr)
        ctypes.cdll.msvcrt.free(dec_ptr)
        ctypes.cdll.msvcrt.free(mag_ptr)
    return count


def verify_database(db_type, db_dir, label):
    db_path = os.path.join(PROJECT_ROOT, db_dir).encode("utf-8")
    if not os.path.exists(os.path.join(PROJECT_ROOT, db_dir)):
        print(f"  SKIP: 数据目录不存在 {db_dir}")
        return

    print(f"\n{'='*80}")
    print(f"  {label}")
    print(f"{'='*80}")

    client = dll.gaia_client_create_ex(db_path, ctypes.c_int(db_type))
    if not client:
        print("  创建客户端失败!")
        return

    files = dll.gaia_client_get_file_count(client)
    sources = dll.gaia_client_get_total_sources(client)
    print(f"  文件数: {files}, 总星数: {sources:,}")

    test_points = [
        (266.4167, -28.9867, "GalacticCenter"),
        (0.0, 0.0, "Equator(RA=0,Dec=0)"),
        (180.0, 0.0, "Equator(RA=180,Dec=0)"),
        (0.0, 90.0, "NorthPole"),
        (0.0, 45.0, "NorthMid(Dec=+45)"),
        (0.0, -45.0, "SouthBoundary(Dec=-45)"),
        (0.0, -50.0, "SouthMid(Dec=-50)"),
        (0.0, -60.0, "SouthMid(Dec=-60)"),
        (0.0, -75.0, "SouthHigh(Dec=-75)"),
        (0.0, -89.0, "SouthPoleNear(Dec=-89)"),
        (100.0, -50.0, "South(RA=100,Dec=-50)"),
        (200.0, -60.0, "South(RA=200,Dec=-60)"),
        (300.0, -70.0, "South(RA=300,Dec=-70)"),
    ]

    passed = 0
    failed = 0
    for ra, dec, desc in test_points:
        count = cone_search(client, ra, dec, 2.0, 16.0, desc)
        if count > 0:
            passed += 1
        else:
            failed += 1

    dll.gaia_client_destroy(client)
    print(f"\n  结果: {passed} passed, {failed} failed / {len(test_points)} total")
    return failed == 0


def main():
    print("=" * 80)
    print("  Gaia XPSD 全天区覆盖验证")
    print("=" * 80)

    all_ok = True
    for db_type, db_dir, label in [
        (1, "GaiaDR3", "GaiaDR3 (Full, 1.8B stars)"),
        (2, "GaiaDR3SP", "GaiaDR3SP (Spectrum, 220M stars)"),
    ]:
        if not verify_database(db_type, db_dir, label):
            all_ok = False

    print(f"\n{'='*80}")
    print(f"  {'ALL PASSED' if all_ok else 'SOME FAILED - check logs above'}")
    print(f"{'='*80}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
