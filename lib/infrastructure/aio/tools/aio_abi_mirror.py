# -*- coding: utf-8 -*-
"""AIO HiPS 跨边界结构 (aio_hips.h) 的权威 ctypes 镜像 —— 单一事实源。

对齐头文件: lib/infrastructure/aio/include/aio_hips.h
本文件是仓内 **唯一** 的 AIO HiPS ABI Python 侧镜像 —— 诊断脚本、冒烟测试与
一致性锁都必须 import 本模块, 不得各自复制一份 _fields_ (V11-N-01 的根因正是
"C=40B 而镜像=32B" 的镜像分叉: 按镜像语义传 3 个 SNR 点会静默错位写数据,
第 3 条 snr 落进 ra_deg 槽并越界读 24 字节, rc 仍为 0)。

机器锁
    ctest 目标 aio_abi_layout_lock (lib/infrastructure/aio/tests/abi/
    aio_abi_layout_lock.py) 编译 C 探针 (aio_abi_layout_probe.cpp, 只 include
    aio_hips.h), 读取其 sizeof/alignof/逐字段 offsetof JSON, 与本模块逐字段
    比对: 字段名/顺序/offset/size/结构体 sizeof/alignof 任一不一致 => FAIL;
    aio_abi_layout_lock_selfcheck 用被篡改的镜像 (字段重排 + 常量污染) 反向
    证明该门非恒真。

ABI 自描述
    四个跨边界结构首部两个 uint32_t: struct_size @0, abi_version @4。
    自行构造结构时**必须**调用 C 头里的初始化器 (aio_hips_tile_view_abi_init /
    aio_hips_snr_point_abi_init / aio_hips_diag_tile_view_abi_init /
    aio_hips_tile_abi_init), 或使用本模块的工厂函数 —— 否则公共入口按
    struct_size/abi_version 不匹配 fail-closed (返回 -9)。
"""

import ctypes

# 必须与 aio_hips.h 的 #define AIO_HIPS_*_ABI_VERSION 完全一致。
AIO_HIPS_TILE_VIEW_ABI_VERSION = 1
AIO_HIPS_SNR_POINT_ABI_VERSION = 1
AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION = 1
AIO_HIPS_TILE_ABI_VERSION = 1
AIO_HIPS_VERIFY_REPORT_ABI_VERSION = 1
# aio_hips.h: #define AIO_HIPS_ABI_MISMATCH (-9)
AIO_HIPS_ABI_MISMATCH = -9
# aio_hips.h: #define ACS_HIPS_MAX_FRAME_SCALE_ARCSEC 824.5167388361774
ACS_HIPS_MAX_FRAME_SCALE_ARCSEC = 824.5167388361774
# aio_hips.h: enum AioHipsDataType { AIO_HIPS_FLOAT32 = 0, AIO_HIPS_FLOAT64 = 1 }
AIO_HIPS_FLOAT32 = 0
AIO_HIPS_FLOAT64 = 1


class AstroSphereTileView(ctypes.Structure):
    """aio_hips.h::AstroSphereTileView 的逐字段镜像 (顺序 = C 声明顺序)。"""

    _fields_ = [
        # --- ABI 自描述 (ASTROCS_DESIGN 7.3; 偏移 0/4) ---
        ("struct_size", ctypes.c_uint32),      # = sizeof(AstroSphereTileView)
        ("abi_version", ctypes.c_uint32),      # = AIO_HIPS_TILE_VIEW_ABI_VERSION
        # --- 视图 ---
        ("parent_ipix", ctypes.c_uint64),
        ("leaf_order", ctypes.c_uint32),
        ("width", ctypes.c_uint32),
        ("data_type", ctypes.c_int32),
        ("flux_sum", ctypes.c_void_p),
        ("covered_area", ctypes.c_void_p),
        ("valid_mask", ctypes.c_void_p),
        ("var_num_sum", ctypes.c_void_p),
    ]


class AioHipsSnrPoint(ctypes.Structure):
    """aio_hips.h::AioHipsSnrPoint 的逐字段镜像。"""

    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("ra_deg", ctypes.c_double),
        ("dec_deg", ctypes.c_double),
        ("snr", ctypes.c_double),
        ("star_id", ctypes.c_int64),
        ("quality_flags", ctypes.c_uint32),
        ("photometric_status", ctypes.c_uint32),
    ]


class AioHipsDiagTileView(ctypes.Structure):
    """aio_hips.h::AioHipsDiagTileView 的逐字段镜像。"""

    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("parent_ipix", ctypes.c_uint64),
        ("leaf_order", ctypes.c_uint32),
        ("width", ctypes.c_uint32),
        ("nused", ctypes.c_void_p),   # const int32_t*
        ("nrej", ctypes.c_void_p),    # const int32_t*
    ]


class AioHipsTile(ctypes.Structure):
    """aio_hips.h::AioHipsTile (legacy, 仅 aio_hips_write 使用) 的逐字段镜像。"""

    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("parent_ipix", ctypes.c_uint64),
        ("depth", ctypes.c_uint32),
        ("signal", ctypes.c_void_p),
        ("support", ctypes.c_void_p),  # const uint8_t*
    ]


class AioHipsVerifyReport(ctypes.Structure):
    """aio_hips.h::AioHipsVerifyReport (输出结构: 调用方分配/库写入) 的逐字段镜像。"""

    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("signal_present", ctypes.c_int),
        ("n_signal_tiles", ctypes.c_int),
        ("variance_present", ctypes.c_int),
        ("ivar_present", ctypes.c_int),
        ("n_variance_tiles", ctypes.c_int),
        ("n_ivar_tiles", ctypes.c_int),
        ("nrej_present", ctypes.c_int),
        ("nused_present", ctypes.c_int),
        ("nrej_declared", ctypes.c_int),
        ("nused_declared", ctypes.c_int),
        ("n_nrej_tiles", ctypes.c_int),
        ("n_nused_tiles", ctypes.c_int),
        ("prov_keys_present", ctypes.c_int),
        ("uncertainty_available", ctypes.c_int),
        ("manifest_keys_present", ctypes.c_int),
        ("diag_negative_pixels", ctypes.c_int),
        ("unreadable_tiles", ctypes.c_int),
        ("value_mismatch", ctypes.c_int),
    ]


# 机器锁覆盖的结构体集合 (名称 -> ctypes 类型); 名称与 C 头 typedef 名逐字一致。
MIRRORED_STRUCTS = {
    "AstroSphereTileView": AstroSphereTileView,
    "AioHipsSnrPoint": AioHipsSnrPoint,
    "AioHipsDiagTileView": AioHipsDiagTileView,
    "AioHipsTile": AioHipsTile,
    "AioHipsVerifyReport": AioHipsVerifyReport,
}

# 尺寸常量 (供调用方分配/自检; 由机器锁与 C 侧 sizeof 对齐)
AIO_HIPS_TILE_VIEW_STRUCT_SIZE = ctypes.sizeof(AstroSphereTileView)
AIO_HIPS_SNR_POINT_STRUCT_SIZE = ctypes.sizeof(AioHipsSnrPoint)
AIO_HIPS_DIAG_TILE_VIEW_STRUCT_SIZE = ctypes.sizeof(AioHipsDiagTileView)
AIO_HIPS_TILE_STRUCT_SIZE = ctypes.sizeof(AioHipsTile)
AIO_HIPS_VERIFY_REPORT_STRUCT_SIZE = ctypes.sizeof(AioHipsVerifyReport)


def c_layout(struct_cls):
    """返回 ctypes 侧布局描述 (供锁与诊断打印)。"""
    return {
        "sizeof": ctypes.sizeof(struct_cls),
        "alignment": ctypes.alignment(struct_cls),
        "fields": [
            {"name": name, "offset": getattr(struct_cls, name).offset,
             "size": getattr(struct_cls, name).size}
            for name, _ in struct_cls._fields_
        ],
    }


# ---------------------------------------------------------------------------
# 工厂函数: 唯一构造入口 (自动填 ABI 自描述; 禁止手工裸构造)
# ---------------------------------------------------------------------------
def tile_view(parent_ipix, leaf_order, width, data_type, flux_sum,
              covered_area, valid_mask=None, var_num_sum=None,
              abi_version=AIO_HIPS_TILE_VIEW_ABI_VERSION):
    v = AstroSphereTileView()
    v.struct_size = ctypes.sizeof(AstroSphereTileView)
    v.abi_version = abi_version
    v.parent_ipix = parent_ipix
    v.leaf_order = leaf_order
    v.width = width
    v.data_type = data_type
    v.flux_sum = flux_sum if flux_sum is None else ctypes.c_void_p(flux_sum)
    v.covered_area = (covered_area if covered_area is None
                      else ctypes.c_void_p(covered_area))
    v.valid_mask = valid_mask if valid_mask is None else ctypes.c_void_p(valid_mask)
    v.var_num_sum = var_num_sum if var_num_sum is None else ctypes.c_void_p(var_num_sum)
    return v


def snr_point(ra_deg, dec_deg, snr, star_id, quality_flags=0,
              photometric_status=0, abi_version=AIO_HIPS_SNR_POINT_ABI_VERSION):
    p = AioHipsSnrPoint()
    p.struct_size = ctypes.sizeof(AioHipsSnrPoint)
    p.abi_version = abi_version
    p.ra_deg = ra_deg
    p.dec_deg = dec_deg
    p.snr = snr
    p.star_id = star_id
    p.quality_flags = quality_flags
    p.photometric_status = photometric_status
    return p


def snr_points(rows, abi_version=AIO_HIPS_SNR_POINT_ABI_VERSION):
    """rows: (ra, dec, snr, star_id[, qf[, ps]]) 序列 -> ctypes 数组 (连续步长)。"""
    arr = (AioHipsSnrPoint * len(rows))()
    for i, r in enumerate(rows):
        p = snr_point(r[0], r[1], r[2], r[3],
                      r[4] if len(r) > 4 else 0,
                      r[5] if len(r) > 5 else 0, abi_version=abi_version)
        arr[i] = p
    return arr


def diag_tile_view(parent_ipix, leaf_order, width, nused, nrej,
                   abi_version=AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION):
    v = AioHipsDiagTileView()
    v.struct_size = ctypes.sizeof(AioHipsDiagTileView)
    v.abi_version = abi_version
    v.parent_ipix = parent_ipix
    v.leaf_order = leaf_order
    v.width = width
    v.nused = nused if nused is None else ctypes.c_void_p(nused)
    v.nrej = nrej if nrej is None else ctypes.c_void_p(nrej)
    return v


def legacy_tile(parent_ipix, depth, signal, support,
                abi_version=AIO_HIPS_TILE_ABI_VERSION):
    t = AioHipsTile()
    t.struct_size = ctypes.sizeof(AioHipsTile)
    t.abi_version = abi_version
    t.parent_ipix = parent_ipix
    t.depth = depth
    t.signal = signal if signal is None else ctypes.c_void_p(signal)
    t.support = support if support is None else ctypes.c_void_p(support)
    return t
