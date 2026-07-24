"""
calibration pipeline_adapter 命名块API测试脚本
功能: 验证适配后的校准 handler 是否正确使用命名块容器API
用途: 确认 data 块被替换、cal_stats KV 块有 DARK_K 值
"""

import os
import sys

import numpy as np

# 添加 python 目录到 path
_HERE = os.path.dirname(os.path.abspath(__file__))
_IO_DIR = os.path.normpath(os.path.join(_HERE, "..", "astro_image_io", "python"))
for _d in (_HERE, _IO_DIR):
    if _d not in sys.path:
        sys.path.insert(0, _d)

from astro_image_io import PipelineFramePy, PipelineEngine, STAGE_CALIBRATE
from pipeline_adapter import CalibrateParams, register_calibrate_handler


def main():
    print("calibration pipeline_adapter 命名块API测试")
    print("=" * 60)

    # 1. 构造测试数据: float32 [64,64] 的模拟 Light 帧
    np.random.seed(42)
    data = (np.random.rand(64, 64).astype(np.float32) * 1000 + 1000)

    # 2. 构造 master 帧（简单常数帧）
    master_bias = np.zeros((64, 64), dtype=np.float32) + 500
    master_dark = np.zeros((64, 64), dtype=np.float32) + 100
    master_flat = np.ones((64, 64), dtype=np.float32) * 2.0

    # 3. 构造 frame: data 块 + header KV 块
    frame = PipelineFramePy()
    engine = PipelineEngine()
    try:
        frame.add_block("data", data, description="原始像素")
        frame.kv_set_double("header", "EXPTIME", 60.0)
        frame.kv_set("header", "FILTER", "L")

        # 4. 注册 handler 并执行校准阶段
        params = CalibrateParams(
            master_bias=master_bias,
            master_dark=master_dark,
            master_flat=master_flat,
            dark_exposure=60.0,
            dark_optimization=False,
            enable_cosmetic_correction=False,  # 测试中关闭坏点修复以简化验证
        )
        register_calibrate_handler(engine, params)
        engine.run_single(frame, STAGE_CALIBRATE, STAGE_CALIBRATE)

        # 5. 验证结果
        print("\n--- 验证结果 ---")
        pass_count = 0
        fail_count = 0

        def check(cond, msg):
            nonlocal pass_count, fail_count
            if cond:
                print(f"  [PASS] {msg}")
                pass_count += 1
            else:
                print(f"  [FAIL] {msg}")
                fail_count += 1

        # 验证 data 块存在且被替换
        check(frame.has_block("data"), "data 块存在")

        out = frame.get_block_data("data")
        check(out is not None, "get_block_data('data') 不为 None")
        check(out is not None and out.shape == (64, 64),
              f"data shape == (64,64) (实际={out.shape if out is not None else None})")
        check(out is not None and out.dtype == np.float32,
              f"data dtype == float32 (实际={out.dtype if out is not None else None})")
        check(out is not None and not np.allclose(out, data),
              "data 块值已改变（校准生效）")
        if out is not None:
            print(f"    校准前 mean={data.mean():.2f}, 校准后 mean={out.mean():.2f}")

        # 验证 cal_stats KV 块有 DARK_K 值
        check(frame.has_block("cal_stats"), "cal_stats 块存在")
        dark_k_str = frame.kv_get("cal_stats", "DARK_K")
        check(dark_k_str is not None, "cal_stats/DARK_K 存在")
        if dark_k_str is not None:
            dark_k = float(dark_k_str)
            check(abs(dark_k - 1.0) < 1e-9,
                  f"DARK_K == 1.0 (实际={dark_k:.4f})")

        print(f"\n测试结果: {pass_count} 通过, {fail_count} 失败")
        return 0 if fail_count == 0 else 1

    finally:
        frame.close()
        engine.close()


if __name__ == "__main__":
    sys.exit(main())
