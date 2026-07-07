#!/usr/bin/env python3
"""
V4.17 星检测回归验证: NGC6302 H-alpha 窄带
功能: 验证 reject_star 阈值修复后 NGC6302 检测星数 ≥ 20
用途: Phase 1 Task 2 验证 checkpoint
"""
import sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from star_detector import StarDetector, SDetParamsPy

TEST_FRAMES = [
    r"testdata\lights\NGC6302_T1-20260326@081129-300S-H-alpha.fts",
    r"testdata\lights\NGC6302_T1_flying_dutchman-20260328@065249-300S-H-alpha.fts",
    r"testdata\lights\NGC6302_T1_flying_dutchman-20260411@060632-300S-H-alpha.fts",
]

def main():
    root = Path(__file__).resolve().parents[3]
    print(f"项目根目录: {root}")
    print(f"测试帧数: {len(TEST_FRAMES)}")
    print()

    params = SDetParamsPy(
        iterativeClipSigma=9.0,
        fwhmClipSigma=3.0,
        maxAxisRatio=2.0,
        fitRadius=8,
    )
    detector = StarDetector(params=params)

    all_pass = True
    for rel in TEST_FRAMES:
        p = root / rel
        if not p.exists():
            print(f"[MISS] {p.name} (文件不存在)")
            all_pass = False
            continue
        try:
            from astropy.io import fits
            with fits.open(p) as hdul:
                data = hdul[0].data
                if data.dtype != np.uint16:
                    if data.dtype == np.float32 or data.dtype == np.float64:
                        data = np.clip(data, 0, 65535).astype(np.uint16)
                    else:
                        data = data.astype(np.uint16)
        except Exception as e:
            print(f"[ERR ] {p.name} 读取失败: {e}")
            all_pass = False
            continue

        coords = detector.detect(data)
        n = len(coords)
        status = "PASS" if n >= 20 else "FAIL"
        if n < 20:
            all_pass = False
        print(f"[{status}] {p.name}: {n} 颗星")

    detector.close()
    print()
    if all_pass:
        print("=== Phase 1 Task 2 验证通过: 所有 NGC6302 帧 ≥ 20 颗星 ===")
    else:
        print("=== Phase 1 Task 2 验证失败: 部分帧 < 20 颗星 ===")
        sys.exit(1)

if __name__ == "__main__":
    main()
