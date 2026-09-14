# -*- coding: utf-8 -*-
"""IpvParams / IpvWcsResult 的权威 ctypes 镜像 (宪章 §8.6 版本化 C ABI)。

单一事实源 (single source of truth):
    与 lib/plate_solve/cpp/ipv/include/ipv_api.h 逐字段对齐。
    本文件是仓内 **唯一** 的 Python 侧镜像 —— 诊断工具与一致性锁都必须
    import 本模块, 不得各自复制一份 _fields_ (V2-N-01 的根因正是镜像分叉)。

机器锁:
    ctest 目标 ipv_abi_layout_lock (lib/plate_solve/cpp/ipv/test/
    ipv_abi_layout_lock.py) 会编译 C 探针 (ipv_abi_layout_probe.cpp),
    读取其 sizeof/offsetof JSON, 与本模块的 ctypes 布局逐字段比对:
    任一字段名/顺序/offset/size 或结构体总大小不一致即 FAIL。
    因此任何字段增删/重排/改类型都必须同时改 ipv_api.h 与本文件, 否则门变红。

ABI 自描述:
    IpvParams 首部两个 uint32_t (struct_size 偏移 0, abi_version 偏移 4)。
    C 侧 ipv_get_default_params() 会写入这两个字段; 公共入口据此 fail-closed。
    自行构造 IpvParams 时 (不经 ipv_get_default_params) 必须显式设置二者。
"""

import ctypes

# 必须与 ipv_api.h 的 #define IPV_PARAMS_ABI_VERSION 完全一致。
IPV_PARAMS_ABI_VERSION = 2


class IpvParams(ctypes.Structure):
    """ipv_api.h::IpvParams 的逐字段镜像 (字段顺序 = C 声明顺序, 不可随意重排)。"""

    _fields_ = [
        # --- ABI 自描述 (宪章 §8.6; 首部, 偏移 0/4) ---
        ("struct_size", ctypes.c_uint32),   # = ctypes.sizeof(IpvParams)
        ("abi_version", ctypes.c_uint32),   # = IPV_PARAMS_ABI_VERSION
        # --- 求解参数 ---
        ("polygon_sides", ctypes.c_int),
        ("n_pivot", ctypes.c_int),
        ("sigma_d_arcsec", ctypes.c_double),
        ("vote_threshold", ctypes.c_int),
        ("ransac_max_iter", ctypes.c_int),
        ("ransac_inlier_threshold_arcsec", ctypes.c_double),
        ("s_min", ctypes.c_double),
        ("s_max", ctypes.c_double),
        ("img_n_target", ctypes.c_int),
        ("gaia_density_ratio", ctypes.c_double),
        ("gaia_query_radius_factor", ctypes.c_double),
        # --- 极限星等割线迭代 (P4-magiter; 与 ipv::IPVSolverParams 逐字段对应)。
        #     m_lim_step 已删除, 由 m_lim_alpha_prior 替换。 ---
        ("m_lim_alpha_prior", ctypes.c_double),
        ("m_lim_alpha_min", ctypes.c_double),
        ("m_lim_alpha_max", ctypes.c_double),
        ("m_lim_safety", ctypes.c_double),
        ("m_lim_m0_exposure_s", ctypes.c_double),
        ("m_lim_m0_offset", ctypes.c_double),
        ("m_lim_clamp_lo", ctypes.c_double),
        ("m_lim_clamp_hi", ctypes.c_double),
        ("m_lim_zero_step", ctypes.c_double),
        ("m_lim_gaia_cap_per_file", ctypes.c_double),
        ("m_lim_max_iter", ctypes.c_int),
        ("density_tolerance", ctypes.c_double),
        # --- 日志 ---
        ("log_dir", ctypes.c_char * 256),   # 空字符串 = 不写日志
    ]


# sizeof(IpvParams): 供调用方分配/自检使用; 由机器锁与 C 侧 sizeof 对齐。
IPV_PARAMS_STRUCT_SIZE = ctypes.sizeof(IpvParams)


class IpvWcsResult(ctypes.Structure):
    """ipv_api.h::IpvWcsResult 的逐字段镜像。"""

    _fields_ = [
        ("cd", ctypes.c_double * 4),
        ("crval", ctypes.c_double * 2),
        ("crpix", ctypes.c_double * 2),
        ("sip_order", ctypes.c_int),
        ("sip_a", ctypes.c_double * 36),
        ("sip_b", ctypes.c_double * 36),
        ("sip_ap_order", ctypes.c_int),
        ("sip_ap", ctypes.c_double * 36),
        ("sip_bp", ctypes.c_double * 36),
        ("rms_px", ctypes.c_double),
        ("rms_arcsec", ctypes.c_double),
        ("n_pairs", ctypes.c_int),
        ("success", ctypes.c_int),
        # 诊断信息
        ("n_detected", ctypes.c_int),
        ("n_catalog", ctypes.c_int),
        ("trans_order", ctypes.c_int),
        ("best_inliers", ctypes.c_int),
        ("ctype1", ctypes.c_char * 16),
        ("ctype2", ctypes.c_char * 16),
        ("error_msg", ctypes.c_char * 256),
    ]


# 机器锁覆盖的结构体集合 (名称 -> ctypes 类型)。
MIRRORED_STRUCTS = {
    "IpvParams": IpvParams,
    "IpvWcsResult": IpvWcsResult,
}


def c_layout(struct_cls=IpvParams):
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
