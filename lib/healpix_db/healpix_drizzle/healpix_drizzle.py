"""
healpix_drizzle.py - HEALPix Drizzle Python 绑定

功能：通过 ctypes 调用 healpix_drizzle.dll，将 FITS 图像 Drizzle 到 HEALPix 网格
用途：FITS 投影图像 → HEALPix 等积网格重投影 (drizzle)，输出 .hiss 格式
      (通过 healpix_io.dll 的 hiss_write 写入; 若 output_path 以 .ahpx 结尾会自动改为 .hiss)

使用示例：
    from healpix_drizzle import hp_drizzle_fits_to_ahpx

    result = hp_drizzle_fits_to_ahpx(
        fits_path="input.fits",
        output_path="output.hiss",
        nside=32768,
        nested=True,
        pixfrac=1.0,
    )
    print(f"源像素: {result.n_source_pixels}, HEALPix 像素: {result.n_healpix_pixels}")
"""

from __future__ import annotations

import os
from ctypes import (
    c_int, c_int64, c_double, c_char, c_char_p, c_void_p,
    POINTER, byref, cdll, Structure,
)
from dataclasses import dataclass
from typing import Optional


# ============================================================================
# C 结构体定义 (对应 hp_drizzle_api.h 的 HpDrizzleResult)
# ============================================================================
class _HpDrizzleResult(Structure):
    _fields_ = [
        ("n_healpix_pixels", c_int64),
        ("n_source_pixels", c_int64),
        ("nside", c_int),
        ("nested", c_int),
        ("pixfrac", c_double),
        ("elapsed_sec", c_double),
        ("error_msg", c_char * 512),
    ]


# ============================================================================
# Python 结果数据类
# ============================================================================
@dataclass
class HpDrizzleResult:
    """Drizzle 执行结果"""
    n_healpix_pixels: int   # 有效 HEALPix 像素数
    n_source_pixels: int    # 源图像像素数
    nside: int              # HEALPix nside
    nested: int             # 1=NESTED, 0=RING
    pixfrac: float          # 像素收缩因子
    elapsed_sec: float      # 耗时 (秒)
    error_msg: str          # 错误信息 (成功时为空)


# ============================================================================
# DLL 加载
# ============================================================================
def _find_dll() -> str:
    """查找 healpix_drizzle.dll: 同目录 → 上级目录"""
    dll_name = "healpix_drizzle.dll"
    base = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base, dll_name),                          # 同目录
        os.path.normpath(os.path.join(base, "..", dll_name)),  # 上级目录
    ]
    for p in candidates:
        if os.path.isfile(p):
            return p
    return os.path.join(base, dll_name)  # 默认同目录


def _load_dll(dll_path: str):
    """加载 healpix_drizzle.dll 并配置 C API 函数签名"""
    mingw_bin = r"C:\msys64\mingw64\bin"
    if os.path.isdir(mingw_bin):
        current_path = os.environ.get("PATH", "")
        if mingw_bin not in current_path:
            os.environ["PATH"] = mingw_bin + ";" + current_path
        try:
            os.add_dll_directory(mingw_bin)
        except OSError:
            pass
    dll_dir = os.path.dirname(os.path.abspath(dll_path))
    try:
        os.add_dll_directory(dll_dir)
    except OSError:
        pass
    # healpix_drizzle.dll 依赖 healpix_io.dll (hiss_write) 和 astro_image_io.dll (PipelineFrame)
    # 添加同级 healpix_io 目录和上级 astro_image_io 目录到 DLL 搜索路径
    base = os.path.dirname(dll_dir)  # lib/healpix_db/
    for sibling in ("healpix_io",):
        sib_dir = os.path.join(base, sibling)
        if os.path.isdir(sib_dir):
            try:
                os.add_dll_directory(sib_dir)
            except OSError:
                pass
    aio_dir = os.path.normpath(os.path.join(base, "..", "astro_image_io"))
    if os.path.isdir(aio_dir):
        try:
            os.add_dll_directory(aio_dir)
        except OSError:
            pass
    dll = cdll.LoadLibrary(dll_path)

    # hp_drizzle_fits_to_ahpx 函数签名
    dll.hp_drizzle_fits_to_ahpx.argtypes = [
        c_char_p,                    # fits_path
        c_char_p,                    # output_path
        c_int,                       # nside
        c_int,                       # nested
        c_double,                    # pixfrac
        c_char_p,                    # snr_path
        c_char_p,                    # weight_path
        POINTER(_HpDrizzleResult),   # result
    ]
    dll.hp_drizzle_fits_to_ahpx.restype = c_int

    # hp_drizzle_run 函数签名 (命名块直通版本)
    # int hp_drizzle_run(PipelineFrame* frame, int nside, int nested,
    #                    double pixfrac, const char* output_path,
    #                    HpDrizzleResult* result)
    dll.hp_drizzle_run.argtypes = [
        c_void_p,                    # frame (PipelineFrame*)
        c_int,                       # nside
        c_int,                       # nested
        c_double,                    # pixfrac
        c_char_p,                    # output_path (可为 nullptr)
        POINTER(_HpDrizzleResult),   # result
    ]
    dll.hp_drizzle_run.restype = c_int

    return dll


# 全局 DLL 实例 (延迟加载)
_dll: Optional[object] = None


def _get_dll():
    """获取全局 DLL 实例 (首次调用时加载)"""
    global _dll
    if _dll is None:
        _dll = _load_dll(_find_dll())
    return _dll


# ============================================================================
# 主接口函数
# ============================================================================
def hp_drizzle_fits_to_ahpx(
    fits_path: str,
    output_path: str,
    nside: int = 32768,
    nested: bool = True,
    pixfrac: float = 1.0,
    snr_path: Optional[str] = None,
    weight_path: Optional[str] = None,
) -> HpDrizzleResult:
    """
    执行 Drizzle: FITS → .hiss

    参数:
        fits_path:    输入 FITS 文件路径 (UTF-8)
        output_path:  输出 .hiss 文件路径 (UTF-8, 若以 .ahpx 结尾会自动改为 .hiss)
        nside:        HEALPix nside (默认 32768, 必须是 2 的幂)
        nested:       True=NESTED, False=RING (默认 True)
        pixfrac:      像素收缩因子 0.0~1.0 (默认 1.0, 0=点采样, <1.0 会产生固有缝隙)
        snr_path:     可选 SNR FITS 文件路径 (None 则不用)
        weight_path:  可选权重 FITS 文件路径 (None 则不用)

    返回:
        HpDrizzleResult 对象

    异常:
        RuntimeError: Drizzle 失败时抛出 (含错误信息)
    """
    dll = _get_dll()
    result = _HpDrizzleResult()
    ret = dll.hp_drizzle_fits_to_ahpx(
        fits_path.encode("utf-8"),
        output_path.encode("utf-8"),
        nside,
        1 if nested else 0,
        pixfrac,
        (snr_path or "").encode("utf-8"),
        (weight_path or "").encode("utf-8"),
        byref(result),
    )
    # c_char*512 返回 bytes, 截取第一个 null 之前的内容 (C 字符串)
    raw = bytes(result.error_msg)
    error_msg = raw.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
    if ret != 0:
        raise RuntimeError(f"Drizzle 失败 (code={ret}): {error_msg}")
    return HpDrizzleResult(
        n_healpix_pixels=result.n_healpix_pixels,
        n_source_pixels=result.n_source_pixels,
        nside=result.nside,
        nested=result.nested,
        pixfrac=result.pixfrac,
        elapsed_sec=result.elapsed_sec,
        error_msg=error_msg,
    )


# ============================================================================
# 命名块直通接口 (PipelineFrame → .hiss, 不经临时 FITS 文件)
# ============================================================================
def hp_drizzle_run(
    frame,
    nside: int = 32768,
    nested: bool = True,
    pixfrac: float = 1.0,
    output_path: Optional[str] = None,
) -> HpDrizzleResult:
    """执行 Drizzle: PipelineFrame 命名块直通 → .hiss (不经临时 FITS)

    从 PipelineFrame 的 "data" 块 (float32[H,W]) 和 "header" KV 块
    (含 WCS/SIP 字段) 直接构造 FitsImage 调用 DrizzleEngine。
    输出 .hiss 文件通过 healpix_io.dll 的 hiss_write 写入。

    参数:
        frame:       PipelineFramePy 对象, 或 C 端 PipelineFrame 指针 (int/c_void_p)
        nside:       HEALPix nside (默认 32768, 必须是 2 的幂)
        nested:      True=NESTED, False=RING (默认 True)
        pixfrac:     像素收缩因子 0.0~1.0 (默认 1.0, 0=点采样, <1.0 会产生固有缝隙)
        output_path: 输出 .hiss 文件路径 (None 则不写文件, 仅返回统计;
                      若以 .ahpx 结尾会自动改为 .hiss)

    返回:
        HpDrizzleResult 对象

    异常:
        RuntimeError: Drizzle 失败时抛出 (含错误信息)

    使用示例:
        from astro_image_io import PipelineFramePy
        from healpix_drizzle import hp_drizzle_run

        frame = PipelineFramePy()
        frame.add_block("data", pixels)  # float32[H,W]
        frame.kv_set("header", "CRVAL1", "180.0")
        # ... 其他 WCS 字段
        result = hp_drizzle_run(frame, nside=4096, output_path="out.hiss")
    """
    dll = _get_dll()

    # 提取 C 端 PipelineFrame 指针
    if hasattr(frame, "handle"):
        # PipelineFramePy 对象
        frame_ptr = frame.handle
    else:
        # 已经是 int / c_void_p / POINTER
        frame_ptr = frame

    result = _HpDrizzleResult()
    ret = dll.hp_drizzle_run(
        frame_ptr,
        nside,
        1 if nested else 0,
        pixfrac,
        (output_path or "").encode("utf-8") if output_path else None,
        byref(result),
    )

    raw = bytes(result.error_msg)
    error_msg = raw.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
    if ret != 0:
        raise RuntimeError(f"hp_drizzle_run 失败 (code={ret}): {error_msg}")
    return HpDrizzleResult(
        n_healpix_pixels=result.n_healpix_pixels,
        n_source_pixels=result.n_source_pixels,
        nside=result.nside,
        nested=result.nested,
        pixfrac=result.pixfrac,
        elapsed_sec=result.elapsed_sec,
        error_msg=error_msg,
    )
