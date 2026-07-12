"""
astro_image_io 命名块容器模型测试
功能: 验证 PipelineFrame 的块管理/KV/缓存/导出/引擎功能
用途: 确认 IO 迁移后的命名块模型可正常工作
"""

import os
import sys
import shutil
import tempfile
import traceback

import numpy as np

# 添加 python 目录到 path
_HERE = os.path.dirname(os.path.abspath(__file__))
_PYTHON_DIR = os.path.normpath(os.path.join(_HERE, "..", "python"))
if _PYTHON_DIR not in sys.path:
    sys.path.insert(0, _PYTHON_DIR)

from astro_image_io import (
    PipelineFramePy,
    PipelineEngine,
    PipelineStageHandlerC,
    STAGE_CALIBRATE,
    STAGE_PLATESOLVE,
    STAGE_PHOTOMETRIC,
    STAGE_DRIZZLE,
    STAGE_STACK,
    AIO_BLOCK_FLOAT32,
    AIO_BLOCK_KV,
    AIO_BLOCK_STRING,
)


# ============================================================================
# 测试工具
# ============================================================================

PASS = 0
FAIL = 0
ERRORS = []


def check(condition, msg):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f"  [PASS] {msg}")
    else:
        FAIL += 1
        ERRORS.append(msg)
        print(f"  [FAIL] {msg}")


def section(title):
    print(f"\n=== {title} ===")


# ============================================================================
# 测试用例
# ============================================================================

def test_block_add_get():
    """测试 1: 块添加与读取 (numpy 数组)"""
    section("测试 1: 块添加与读取")
    frame = PipelineFramePy()
    try:
        # 添加 2D float32 数组
        data = np.random.rand(128, 128).astype(np.float32)
        frame.add_block("data", data, description="测试图像")

        check(frame.has_block("data"), "has_block('data') == True")
        check(not frame.has_block("nonexist"), "has_block('nonexist') == False")
        check(frame.n_blocks == 1, f"n_blocks == 1 (实际={frame.n_blocks})")

        # 读取
        out = frame.get_block_data("data")
        check(out is not None, "get_block_data('data') != None")
        check(out.shape == (128, 128), f"shape == (128,128) (实际={out.shape})")
        check(out.dtype == np.float32, f"dtype == float32 (实际={out.dtype})")
        check(np.allclose(out, data), "数据一致 (allclose)")

        # block info
        info = frame.get_block_info("data")
        check(info is not None, "get_block_info != None")
        check(info["name"] == "data", f"info.name == 'data' (实际='{info['name']}')")
        check(info["type"] == AIO_BLOCK_FLOAT32, f"info.type == FLOAT32 (实际={info['type']})")
        check(info["count"] == 128 * 128, f"info.count == 16384 (实际={info['count']})")
        check(info["n_dims"] == 2, f"info.n_dims == 2 (实际={info['n_dims']})")
        check(info["dims"] == [128, 128], f"info.dims == [128,128] (实际={info['dims']})")
        check(info["description"] == "测试图像", f"description == '测试图像' (实际='{info['description']}')")
    finally:
        frame.close()


def test_block_multiple_types():
    """测试 2: 多种块类型"""
    section("测试 2: 多种块类型")
    frame = PipelineFramePy()
    try:
        # float32
        f32 = np.arange(10, dtype=np.float32)
        frame.add_block("f32", f32)
        # float64
        f64 = np.arange(10, dtype=np.float64)
        frame.add_block("f64", f64)
        # int32
        i32 = np.arange(10, dtype=np.int32)
        frame.add_block("i32", i32)
        # int64
        i64 = np.arange(10, dtype=np.int64)
        frame.add_block("i64", i64)
        # string
        frame.add_block_string("note", "Hello 天文")
        # KV
        frame.kv_set("header", "OBJECT", "M31")
        frame.kv_set("header", "EXPTIME", "60.0")

        check(frame.n_blocks == 6, f"n_blocks == 6 (实际={frame.n_blocks})")

        # 验证各类型
        check(np.allclose(frame.get_block_data("f32"), f32), "f32 数据一致")
        check(np.allclose(frame.get_block_data("f64"), f64), "f64 数据一致")
        check(np.array_equal(frame.get_block_data("i32"), i32), "i32 数据一致")
        check(np.array_equal(frame.get_block_data("i64"), i64), "i64 数据一致")

        s = frame.get_block_string("note")
        check(s == "Hello 天文", f"string == 'Hello 天文' (实际='{s}')")

        kv = frame.get_block_kv("header")
        check(kv.get("OBJECT") == "M31", f"KV OBJECT == 'M31' (实际='{kv.get('OBJECT')}')")
        check(kv.get("EXPTIME") == "60.0", f"KV EXPTIME == '60.0' (实际='{kv.get('EXPTIME')}')")
    finally:
        frame.close()


def test_block_remove():
    """测试 3: 块删除"""
    section("测试 3: 块删除")
    frame = PipelineFramePy()
    try:
        frame.add_block("a", np.zeros(5, dtype=np.float32))
        frame.add_block("b", np.zeros(5, dtype=np.float32))
        frame.add_block("c", np.zeros(5, dtype=np.float32))
        check(frame.n_blocks == 3, f"初始 n_blocks == 3 (实际={frame.n_blocks})")

        ok = frame.remove_block("b")
        check(ok, "remove_block('b') 返回 True")
        check(not frame.has_block("b"), "删除后 has_block('b') == False")
        check(frame.has_block("a"), "a 仍存在")
        check(frame.has_block("c"), "c 仍存在")
        check(frame.n_blocks == 2, f"删除后 n_blocks == 2 (实际={frame.n_blocks})")

        # 删除不存在的块
        ok = frame.remove_block("nonexist")
        check(not ok, "remove_block('nonexist') 返回 False")
    finally:
        frame.close()


def test_list_blocks():
    """测试 4: 列出所有块"""
    section("测试 4: 列出所有块")
    frame = PipelineFramePy()
    try:
        names = ["data", "header", "psf", "weight", "snr"]
        for n in names:
            if n == "header":
                frame.kv_set("header", "KEY", "VAL")
            else:
                frame.add_block(n, np.zeros(1, dtype=np.float32))

        blocks = frame.list_blocks()
        check(len(blocks) == 5, f"list_blocks 长度 == 5 (实际={len(blocks)})")
        for n in names:
            check(n in blocks, f"'{n}' 在列表中")
    finally:
        frame.close()


def test_kv_operations():
    """测试 5: KV 操作 (含 double)"""
    section("测试 5: KV 操作")
    frame = PipelineFramePy()
    try:
        # 字符串 set/get
        frame.kv_set("header", "OBJECT", "NGC7000")
        frame.kv_set("header", "FILTER", "Ha")
        frame.kv_set("header", "DATE-OBS", "2026-07-12T20:00:00")

        check(frame.kv_get("header", "OBJECT") == "NGC7000", "KV OBJECT")
        check(frame.kv_get("header", "FILTER") == "Ha", "KV FILTER")
        check(frame.kv_get("header", "DATE-OBS") == "2026-07-12T20:00:00", "KV DATE-OBS")
        check(frame.kv_get("header", "NONEXIST") is None, "KV NONEXIST == None")

        # double set/get
        frame.kv_set_double("header", "EXPTIME", 120.0)
        frame.kv_set_double("header", "GAIN", 1.5)
        frame.kv_set_double("header", "CD1_1", -5.6789e-05)

        check(abs(frame.kv_get_double("header", "EXPTIME") - 120.0) < 1e-9, "KV EXPTIME double")
        check(abs(frame.kv_get_double("header", "GAIN") - 1.5) < 1e-9, "KV GAIN double")
        check(abs(frame.kv_get_double("header", "CD1_1") - (-5.6789e-05)) < 1e-15, "KV CD1_1 double")

        # 覆盖已存在的 key
        frame.kv_set("header", "OBJECT", "M42")
        check(frame.kv_get("header", "OBJECT") == "M42", "KV OBJECT 覆盖后 == 'M42'")

        # 多个 KV 块
        frame.kv_set("calib", "DARK", "dark.fits")
        frame.kv_set("calib", "FLAT", "flat.fits")
        check(frame.kv_get("calib", "DARK") == "dark.fits", "calib DARK")
        check(frame.kv_get("calib", "FLAT") == "flat.fits", "calib FLAT")
        check(frame.kv_get("header", "OBJECT") == "M42", "header 不受 calib 影响")
    finally:
        frame.close()


def test_cache_roundtrip():
    """测试 6: 缓存文件保存/加载往返"""
    section("测试 6: 缓存文件往返")
    tmpdir = tempfile.mkdtemp(prefix="aio_test_")
    cache_path = os.path.join(tmpdir, "test_cache.aio")
    try:
        # 构造源帧
        src = PipelineFramePy()
        try:
            data = np.random.rand(64, 64).astype(np.float32)
            src.add_block("data", data, description="图像像素")
            src.kv_set("header", "OBJECT", "LDN43")
            src.kv_set("header", "FILTER", "L")
            src.kv_set_double("header", "EXPTIME", 300.0)
            src.kv_set_double("header", "CD1_1", 0.000123)
            src.add_block_string("note", "缓存测试 2026")

            # 保存
            src.save_cache(cache_path)
            check(os.path.isfile(cache_path), f"缓存文件已创建: {os.path.basename(cache_path)}")
            check(os.path.getsize(cache_path) > 0, "缓存文件非空")
        finally:
            src.close()

        # 加载到新帧
        dst = PipelineFramePy()
        try:
            dst.load_cache(cache_path)

            check(dst.has_block("data"), "加载后 has_block('data')")
            check(dst.has_block("header"), "加载后 has_block('header')")
            check(dst.has_block("note"), "加载后 has_block('note')")

            # 数据一致性
            out = dst.get_block_data("data")
            check(out is not None, "data 不为 None")
            check(out.shape == (64, 64), f"data shape == (64,64) (实际={out.shape})")
            check(np.allclose(out, data), "data 数据一致")

            # KV 一致性
            check(dst.kv_get("header", "OBJECT") == "LDN43", "KV OBJECT 一致")
            check(dst.kv_get("header", "FILTER") == "L", "KV FILTER 一致")
            check(abs(dst.kv_get_double("header", "EXPTIME") - 300.0) < 1e-9, "KV EXPTIME double 一致")
            check(abs(dst.kv_get_double("header", "CD1_1") - 0.000123) < 1e-12, "KV CD1_1 double 一致")

            # 字符串块
            note = dst.get_block_string("note")
            check(note == "缓存测试 2026", f"note 一致 (实际='{note}')")
        finally:
            dst.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_export_xml():
    """测试 7: XML 调试导出"""
    section("测试 7: XML 调试导出")
    tmpdir = tempfile.mkdtemp(prefix="aio_test_xml_")
    try:
        frame = PipelineFramePy()
        try:
            frame.add_block("data", np.arange(16, dtype=np.float32).reshape(4, 4))
            frame.kv_set("header", "OBJECT", "M31")
            frame.kv_set("header", "EXPTIME", "60")
            frame.add_block_string("note", "XML 测试")

            # 导出所有块
            xml_all = os.path.join(tmpdir, "all.xml")
            frame.export_all_xml(xml_all)
            check(os.path.isfile(xml_all), "export_all_xml 生成文件")
            with open(xml_all, "r", encoding="utf-8") as f:
                content = f.read()
            check("data" in content, "XML 包含 'data' 块")
            check("header" in content, "XML 包含 'header' 块")
            check("note" in content, "XML 包含 'note' 块")
            check("M31" in content, "XML 包含 M31")

            # 导出单个块
            xml_block = os.path.join(tmpdir, "header.xml")
            frame.export_block_xml("header", xml_block)
            check(os.path.isfile(xml_block), "export_block_xml 生成文件")
            with open(xml_block, "r", encoding="utf-8") as f:
                content = f.read()
            check("header" in content, "单块 XML 包含 'header'")
            check("M31" in content, "单块 XML 包含 M31")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_export_fits():
    """测试 8: FITS 块导出"""
    section("测试 8: FITS 块导出")
    tmpdir = tempfile.mkdtemp(prefix="aio_test_fits_")
    try:
        frame = PipelineFramePy()
        try:
            data = np.random.rand(32, 32).astype(np.float32)
            frame.add_block("data", data)

            fits_path = os.path.join(tmpdir, "block.fits")
            frame.export_block_fits("data", fits_path)
            check(os.path.isfile(fits_path), "export_block_fits 生成文件")
            check(os.path.getsize(fits_path) >= 2880, "FITS 文件至少 2880 字节 (一个块)")
        finally:
            frame.close()
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def test_engine_block_drop():
    """测试 9: 引擎块丢弃策略"""
    section("测试 9: 引擎块丢弃策略")

    # 简单的阶段处理函数: 给帧添加一个块
    def make_handler(add_block_name):
        def handler(frame_ptr, params, err_buf, err_cap):
            try:
                f = PipelineFramePy.from_c_ptr(frame_ptr)
                f.add_block(add_block_name, np.zeros(1, dtype=np.float32))
                return 0
            except Exception as e:
                msg = str(e).encode("utf-8")[:err_cap - 1]
                err_buf[:len(msg)] = msg
                return -1
        return handler

    calibrate_handler = make_handler("calib_result")
    platesolve_handler = make_handler("wcs")

    c_calibrate = PipelineStageHandlerC(calibrate_handler)
    c_platesolve = PipelineStageHandlerC(platesolve_handler)

    frame = PipelineFramePy()
    engine = PipelineEngine()
    try:
        engine.register(STAGE_CALIBRATE, c_calibrate)
        engine.register(STAGE_PLATESOLVE, c_platesolve)

        # 自定义丢弃: PLATESOLVE 后丢弃 calib_result
        engine.set_block_drop(STAGE_PLATESOLVE, "calib_result")

        # 初始添加 data 块
        frame.add_block("data", np.zeros(1, dtype=np.float32))

        # 执行
        engine.run_single(frame, STAGE_CALIBRATE, STAGE_PLATESOLVE)

        check(frame.has_block("wcs"), "platesolve 后有 wcs 块")
        check(not frame.has_block("calib_result"), "platesolve 后 calib_result 被丢弃")
        check(frame.has_block("data"), "data 块保留")
    finally:
        frame.close()
        engine.close()


def test_engine_auto_free_default():
    """测试 10: 引擎默认自动丢弃策略 (PLATESOLVE 后丢弃 weight)"""
    section("测试 10: 引擎默认自动丢弃策略")

    def calibrate_handler(frame_ptr, params, err_buf, err_cap):
        try:
            f = PipelineFramePy.from_c_ptr(frame_ptr)
            # 模拟校准阶段生成 weight 块
            if not f.has_block("weight"):
                f.add_block("weight", np.zeros(1, dtype=np.float32))
            return 0
        except Exception as e:
            return -1

    def platesolve_handler(frame_ptr, params, err_buf, err_cap):
        try:
            f = PipelineFramePy.from_c_ptr(frame_ptr)
            # 模拟解算阶段生成 wcs 块
            if not f.has_block("wcs"):
                f.add_block("wcs", np.zeros(1, dtype=np.float32))
            return 0
        except Exception as e:
            return -1

    c_cal = PipelineStageHandlerC(calibrate_handler)
    c_ps = PipelineStageHandlerC(platesolve_handler)

    frame = PipelineFramePy()
    engine = PipelineEngine()
    try:
        engine.register(STAGE_CALIBRATE, c_cal)
        engine.register(STAGE_PLATESOLVE, c_ps)

        frame.add_block("data", np.zeros(1, dtype=np.float32))
        engine.run_single(frame, STAGE_CALIBRATE, STAGE_PLATESOLVE)

        # 默认策略: PLATESOLVE 后丢弃 weight, 保留 data 和 wcs
        check(not frame.has_block("weight"), "默认策略: weight 被丢弃")
        check(frame.has_block("wcs"), "默认策略: wcs 保留")
        check(frame.has_block("data"), "data 保留")
    finally:
        frame.close()
        engine.close()


def test_memory_usage():
    """测试 11: 内存占用统计"""
    section("测试 11: 内存占用统计")
    frame = PipelineFramePy()
    try:
        mem0 = frame.memory_usage
        check(mem0 >= 0, f"初始 memory_usage >= 0 (实际={mem0})")

        # 添加 1MB float32 数据 (512x512)
        data = np.zeros((512, 512), dtype=np.float32)  # 1MB
        frame.add_block("big", data)
        mem1 = frame.memory_usage
        check(mem1 > mem0, f"添加数据后 memory_usage 增加 ({mem0} -> {mem1})")
        check(mem1 - mem0 >= 512 * 512 * 4, f"内存增量 >= 1MB (实际增量={mem1 - mem0})")

        # 删除后内存减少
        frame.remove_block("big")
        mem2 = frame.memory_usage
        check(mem2 < mem1, f"删除后 memory_usage 减少 ({mem1} -> {mem2})")
    finally:
        frame.close()


def test_large_kv():
    """测试 12: 大量 KV 条目 (动态扩容)"""
    section("测试 12: 大量 KV 条目 (动态扩容)")
    frame = PipelineFramePy()
    try:
        # 添加 100 个 KV 条目 (超过初始容量 8)
        for i in range(100):
            frame.kv_set("header", f"KEY_{i:03d}", f"VALUE_{i:03d}")

        # 验证
        kv = frame.get_block_kv("header")
        check(len(kv) == 100, f"KV 条目数 == 100 (实际={len(kv)})")
        check(kv.get("KEY_000") == "VALUE_000", "KEY_000 一致")
        check(kv.get("KEY_050") == "VALUE_050", "KEY_050 一致")
        check(kv.get("KEY_099") == "VALUE_099", "KEY_099 一致")

        # 修改中间一个
        frame.kv_set("header", "KEY_050", "MODIFIED")
        check(frame.kv_get("header", "KEY_050") == "MODIFIED", "修改后 KEY_050 一致")
    finally:
        frame.close()


def test_dict_to_kv_block():
    """测试 13: dict 自动转 KV 块"""
    section("测试 13: dict 自动转 KV 块")
    frame = PipelineFramePy()
    try:
        meta = {
            "OBJECT": "IC1396",
            "FILTER": "OIII",
            "EXPTIME": "180.0",
        }
        frame.add_block("header", meta)

        check(frame.has_block("header"), "header 块存在")
        check(frame.kv_get("header", "OBJECT") == "IC1396", "dict OBJECT")
        check(frame.kv_get("header", "FILTER") == "OIII", "dict FILTER")
        check(frame.kv_get("header", "EXPTIME") == "180.0", "dict EXPTIME")
    finally:
        frame.close()


# ============================================================================
# 主入口
# ============================================================================

def main():
    print("astro_image_io 命名块容器模型测试")
    print("=" * 60)

    tests = [
        test_block_add_get,
        test_block_multiple_types,
        test_block_remove,
        test_list_blocks,
        test_kv_operations,
        test_cache_roundtrip,
        test_export_xml,
        test_export_fits,
        test_engine_block_drop,
        test_engine_auto_free_default,
        test_memory_usage,
        test_large_kv,
        test_dict_to_kv_block,
    ]

    for t in tests:
        try:
            t()
        except Exception as e:
            global FAIL
            FAIL += 1
            ERRORS.append(f"{t.__name__} 异常: {e}")
            print(f"  [ERROR] {t.__name__} 抛出异常:")
            traceback.print_exc()

    print("\n" + "=" * 60)
    print(f"测试结果: {PASS} 通过, {FAIL} 失败")
    if ERRORS:
        print("\n失败项:")
        for e in ERRORS:
            print(f"  - {e}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
