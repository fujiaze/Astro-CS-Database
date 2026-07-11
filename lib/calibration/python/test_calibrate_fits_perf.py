# -*- coding: utf-8 -*-
"""单帧校准性能测试
测试 calibrate_fits() 单帧校准耗时，目标 < 1秒/帧。
"""
import os
import sys
import time
import json

# 强制 UTF-8
try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

# 添加模块路径
_LIB_BASE = os.path.dirname(os.path.abspath(__file__))
if _LIB_BASE not in sys.path:
    sys.path.insert(0, _LIB_BASE)

from calibrate_fits import calibrate_fits

# 测试数据（_LIB_BASE = lib/calibration/python，需上溯3层到项目根）
LIGHT_PATH = os.path.normpath(os.path.join(
    _LIB_BASE, "..", "..", "..", "testdata", "Galaxy_Center_T4", "lights", "panel1",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
))
CALIBRATION_DIR = os.path.normpath(os.path.join(
    _LIB_BASE, "..", "..", "..", "testdata", "T4 calibration files",
))
OUTPUT_DIR = os.path.normpath(os.path.join(
    _LIB_BASE, "..", "logs", "perf_test",
))
OUTPUT_PATH = os.path.join(OUTPUT_DIR, "01_calibrated.fits")


def main():
    print("=" * 60)
    print("单帧校准性能测试")
    print(f"  Light: {LIGHT_PATH}")
    print(f"  校准帧目录: {CALIBRATION_DIR}")
    print(f"  输出: {OUTPUT_PATH}")
    print("=" * 60)

    # 预检查
    assert os.path.isfile(LIGHT_PATH), f"Light 帧不存在: {LIGHT_PATH}"
    assert os.path.isdir(CALIBRATION_DIR), f"校准帧目录不存在: {CALIBRATION_DIR}"
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 计时
    t0 = time.perf_counter()
    result = calibrate_fits(
        light_path=LIGHT_PATH,
        output_path=OUTPUT_PATH,
        calibration_dir=CALIBRATION_DIR,
        mode="production",
        cc_method="median",
        max_workers=16,
    )
    t1 = time.perf_counter()
    elapsed = t1 - t0

    print("=" * 60)
    print(f"耗时: {elapsed:.3f}s ({elapsed * 1000:.1f}ms)")
    print(f"成功: {result.get('success')}")
    if result.get("success"):
        print(f"输出: {result.get('output_path')}")
        stats = result.get("stats", {})
        cc = stats.get("cc_stats", {})
        print(f"  热像素: {cc.get('hot_pixels', 'N/A')}")
        print(f"  冷像素: {cc.get('cold_pixels', 'N/A')}")
        print(f"  总修复: {cc.get('total_bad', 'N/A')}")
        print(f"  修复后mean: {cc.get('corrected_mean', 'N/A')}")
        # 验证输出文件存在
        out = result.get("output_path")
        if out and os.path.isfile(out):
            size_mb = os.path.getsize(out) / (1024 * 1024)
            print(f"  输出文件大小: {size_mb:.2f} MB")
        if elapsed < 1.0:
            print(f"  性能测试: 通过 (< 1s/帧)")
        else:
            print(f"  性能测试: 未达标 (>= 1s/帧)")
    else:
        print(f"错误: {result.get('error')}")
    print("=" * 60)

    # 输出 JSON
    summary = {
        "success": result.get("success"),
        "elapsed_s": round(elapsed, 3),
        "output_path": result.get("output_path"),
        "error": result.get("error", ""),
    }
    print(json.dumps(summary, ensure_ascii=True, default=str))

    return 0 if result.get("success") and elapsed < 1.0 else 1


if __name__ == "__main__":
    sys.exit(main())
