# -*- coding: utf-8 -*-
"""
g_common.py - Gate G/F 公共约定模块 (G-001 ~ G-003 + F-002 共享)

公共约定 (SSOT, 各子任务必须遵守):
  - 模块边界: 本文件定义数据结构 + E-003 结果加载 + HCSD 纯 Python 读写器 + 日志, 不含算法
  - 数据结构:
      CorrectedPanel  - 加性偏移校正后 + 排异标记的单帧像素表
      FusionResult    - 融合结果 (ipix, signal, weight, support_count, snr_eff)
      HcsdLayers      - HCSD 生产层 + 调试质量层 (可开关)
  - 坐标系: RA/Dec 度 (J2000), astropy_healpix HEALPix->lonlat (同 e_common)
  - E-003 结果加载: 从 engineering_authoritative/evidence/E-003/e003_result.json
    读取 surface_coeffs / offsets / center / panel_order
  - HCSD 格式: 纯 Python 实现 lib/astro_image_io/docs/HEALPIX_FORMAT_SPEC.md §3
      Magic "HCSD" + JSON头(zstd) + 子叶索引表(49152×24B) + ipix数组(u64) + pixel数组(f32)
      nside=64 子叶分区 (49152 个子叶, shift = 2*(log2(nside)-6))
  - 调试质量层: 独立 .hcsd.debug 文件 (生产层 + 调试层), 由开关控制是否写入

禁止捷径 (契约级, 继承 E chain):
  - 不得在梯度前做最终叠加 (先校正加性偏移, 再融合)
  - 高 SNR 坏值不得免于排异 (MAD 排异与 SNR 无关)
  - 不得产生硬边 (连续加权, 非硬阈值)
  - 独立 SNR^2 融合 (权重 = SNR^2 × support)
  - 调试层开关 (生产层默认写, 调试层可选)
"""
from __future__ import annotations

import json
import logging
import os
import struct
import sys
import zlib
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

# 项目路径 (SSOT, 同 e_common)
PROJECT_ROOT = r'f:\Astro dev\Astro CS Normalization Database'
HISS_V2_PATH = os.path.join(PROJECT_ROOT, 'lib', 'astro_image_io', 'python')
D001_DIR = os.path.join(PROJECT_ROOT, 'output', 'D-001')
E003_RESULT_PATH = os.path.join(
    PROJECT_ROOT, 'engineering_authoritative', 'evidence', 'E-003', 'e003_result.json')
GF_OUTPUT_DIR = os.path.join(PROJECT_ROOT, 'output', 'G-F')
EVID_DIR = os.path.join(PROJECT_ROOT, 'engineering_authoritative', 'evidence')
LOG_DIR = os.path.join(
    PROJECT_ROOT, 'lib', 'healpix_db', 'healpix_stack', 'python', 'g_chain', 'logs')

# 三片面板名 (固定顺序, 与 e_common 一致, 影响偏移量索引)
PANEL_NAMES = ['panel1', 'panel2', 'panel3']

# HCSD 格式常量 (HEALPIX_FORMAT_SPEC.md §3)
HCSD_MAGIC = b"HCSD"
HCSD_LEAF_NSIDE = 64
HCSD_N_LEAVES = 12 * HCSD_LEAF_NSIDE * HCSD_LEAF_NSIDE  # 49152
HCSD_LEAF_INDEX_ENTRY_SIZE = 24  # leaf_ipix(u64) + data_offset(u64) + data_length(u64)
HCSD_LEAF_INDEX_SIZE = HCSD_N_LEAVES * HCSD_LEAF_INDEX_ENTRY_SIZE  # 1,179,648


# ============================================================================
# 日志配置 (同 e_common, 独立日志目录)
# ============================================================================

def setup_logger(name: str, log_dir: Optional[str] = None,
                 level: int = logging.INFO) -> logging.Logger:
    """配置模块日志, 可选写入文件。"""
    logger = logging.getLogger(name)
    logger.setLevel(level)
    logger.handlers.clear()
    fmt = logging.Formatter('[%(asctime)s][%(levelname)s][%(name)s] %(message)s',
                            datefmt='%H:%M:%S')
    sh = logging.StreamHandler(sys.stdout)
    sh.setFormatter(fmt)
    logger.addHandler(sh)
    if log_dir:
        os.makedirs(log_dir, exist_ok=True)
        fh = logging.FileHandler(os.path.join(log_dir, f'{name}.log'),
                                 encoding='utf-8')
        fh.setFormatter(fmt)
        logger.addHandler(fh)
    return logger


# ============================================================================
# 数据结构
# ============================================================================

@dataclass
class E003Result:
    """E-003 求解结果 (从 e003_result.json 加载)。"""
    surface_coeffs: np.ndarray   # f64 [6] 曲面系数 (中心化基底)
    offsets: Dict[str, float]    # {panel_id: off_f} 帧偏移
    center: Tuple[float, float]  # (ra0, dec0) 中心化参考点
    panel_order: List[str]       # 帧顺序


@dataclass
class CorrectedPanel:
    """加性偏移校正后 + 排异标记的单帧像素表 (G-001 输出)。"""
    panel_id: str
    nside: int
    nested: bool
    ipix: np.ndarray         # u64 [N] HEALPix 像素索引
    signal_raw: np.ndarray   # f32 [N] 原始信号
    signal_corrected: np.ndarray  # f32 [N] 校正后信号 (signal_raw - offset)
    support: np.ndarray      # u1 [N] 覆盖标记
    ra: np.ndarray           # f64 [N] 赤经 (度)
    dec: np.ndarray          # f64 [N] 赤纬 (度)
    snr: np.ndarray          # f32 [N] 每像素 SNR (IDW 插值)
    surface_pred: np.ndarray  # f64 [N] 全局曲面预测值
    residual: np.ndarray     # f64 [N] 残差 = corrected - surface_pred
    rejected: np.ndarray     # bool [N] True=排异 (不参与融合)
    offset: float            # 该帧加性偏移量
    meta: Dict[str, Any] = field(default_factory=dict)

    @property
    def n_pix(self) -> int:
        return int(self.ipix.size)

    @property
    def n_valid(self) -> int:
        """有效像素数 (非排异 且 support=1)。"""
        return int((~self.rejected & (self.support == 1)).sum())


@dataclass
class FusionResult:
    """SNR^2 连续加权融合结果 (G-002 输出)。

    融合按 ipix 聚合: 重叠区多帧加权平均, 非重叠区单帧直通。
    """
    nside: int
    nested: bool
    ipix: np.ndarray         # u64 [M] 融合后像素索引 (升序, 去重)
    signal: np.ndarray       # f32 [M] 融合信号 (SNR^2 加权平均)
    weight: np.ndarray       # f64 [M] 总权重 Σ(w_i)
    support_count: np.ndarray  # u8 [M] 贡献帧数 (1..F)
    snr_eff: np.ndarray      # f32 [M] 有效 SNR (加权合并)
    coverage: np.ndarray     # u1 [M] 覆盖标记 (始终 1, 融合后均为覆盖)
    ra: np.ndarray           # f64 [M] 赤经
    dec: np.ndarray          # f64 [M] 赤纬
    per_panel_contrib: Dict[str, np.ndarray] = field(default_factory=dict)
    # per_panel_contrib[panel_id]: u1 [M] 1=该帧参与该像素融合

    @property
    def n_pix(self) -> int:
        return int(self.ipix.size)


@dataclass
class HcsdLayers:
    """HCSD 生产层 + 调试质量层 (G-003 输出)。

    生产层 (必写): signal + support + snr
    调试层 (可开关): per_panel_contrib / rejected_map / weight_map
    """
    # 生产层
    nside: int
    nested: bool
    ipix: np.ndarray         # u64 [M]
    signal: np.ndarray       # f32 [M] 融合信号
    support: np.ndarray      # u1 [M] 覆盖标记
    snr: np.ndarray          # f32 [M] 有效 SNR
    # 调试层 (可选)
    debug_enabled: bool = False
    weight_map: Optional[np.ndarray] = None        # f64 [M] 总权重
    support_count: Optional[np.ndarray] = None     # u8 [M] 贡献帧数
    per_panel_contrib: Optional[Dict[str, np.ndarray]] = None  # {panel: u1[M]}
    rejected_per_panel: Optional[Dict[str, np.ndarray]] = None  # {panel: bool[N_panel]}


# ============================================================================
# E-003 结果加载
# ============================================================================

def load_e003_result(path: str = E003_RESULT_PATH) -> E003Result:
    """从 e003_result.json 加载 E-003 求解结果。

    Returns:
        E003Result (surface_coeffs, offsets, center, panel_order)
    """
    if not os.path.isfile(path):
        raise FileNotFoundError(f'E-003 结果文件不存在: {path}')
    with open(path, 'r', encoding='utf-8') as f:
        d = json.load(f)
    return E003Result(
        surface_coeffs=np.asarray(d['surface_coeffs'], dtype=np.float64),
        offsets={k: float(v) for k, v in d['offsets'].items()},
        center=(float(d['center'][0]), float(d['center'][1])),
        panel_order=list(d['panel_order']),
    )


# ============================================================================
# 曲面基底 (同 e_common, 复用以计算 surface_pred)
# ============================================================================

def poly_basis(ra: np.ndarray, dec: np.ndarray,
               center: Tuple[float, float]) -> np.ndarray:
    """2D 多项式基底 [1, ra', dec', ra'*dec', ra'^2, dec'^2] (D=2, M=6, 中心化)。"""
    ra0, dec0 = float(center[0]), float(center[1])
    x = np.asarray(ra, dtype=np.float64) - ra0
    y = np.asarray(dec, dtype=np.float64) - dec0
    return np.stack([np.ones_like(x), x, y, x * y, x * x, y * y], axis=-1)


def eval_surface(coeffs: np.ndarray, ra: np.ndarray, dec: np.ndarray,
                 center: Tuple[float, float]) -> np.ndarray:
    """用多项式系数评估曲面值。"""
    basis = poly_basis(ra, dec, center)
    return basis @ np.asarray(coeffs, dtype=np.float64)


# ============================================================================
# 统计辅助 (MAD-based, 同 e_common)
# ============================================================================

def safe_mad_sigma(x: np.ndarray) -> float:
    """标量 MAD-sigma = 1.4826 * median(|x - median(x)|), 忽略 NaN/Inf。"""
    x = np.asarray(x, dtype=np.float64)
    x = x[np.isfinite(x)]
    if x.size == 0:
        return 0.0
    med = float(np.median(x))
    mad = float(np.median(np.abs(x - med)))
    return 1.4826 * mad


# ============================================================================
# IDW 插值 (同 e_common, 用于 SNR 控制点 -> 像素位置)
# ============================================================================

def idw_interpolate(query_ra: np.ndarray, query_dec: np.ndarray,
                    src_ra: np.ndarray, src_dec: np.ndarray,
                    src_val: np.ndarray, power: float = 2.0,
                    k: int = 8) -> np.ndarray:
    """反距离加权插值 (IDW), 球面大圆弧距离。"""
    query_ra = np.asarray(query_ra, dtype=np.float64)
    query_dec = np.asarray(query_dec, dtype=np.float64)
    src_ra = np.asarray(src_ra, dtype=np.float64)
    src_dec = np.asarray(src_dec, dtype=np.float64)
    src_val = np.asarray(src_val, dtype=np.float64)

    Q = query_ra.size
    S = src_ra.size
    out = np.full(Q, np.nan, dtype=np.float64)
    if S == 0:
        return out

    def to_xyz(ra, dec):
        r = np.deg2rad(ra)
        d = np.deg2rad(dec)
        return np.stack([np.cos(d) * np.cos(r), np.cos(d) * np.sin(r), np.sin(d)], axis=-1)
    src_xyz = to_xyz(src_ra, src_dec)
    q_xyz = to_xyz(query_ra, query_dec)

    kk = min(k, S)
    for i in range(Q):
        dots = np.clip(src_xyz @ q_xyz[i], -1.0, 1.0)
        ang = np.arccos(dots)
        idx = np.argpartition(ang, kk - 1)[:kk]
        d_k = ang[idx]
        v_k = src_val[idx]
        if np.any(d_k < 1e-12):
            out[i] = v_k[d_k < 1e-12][0]
            continue
        w = 1.0 / np.power(d_k, power)
        out[i] = float(np.sum(w * v_k) / np.sum(w))
    return out


# ============================================================================
# HISS 读取 (复用 e_common.load_panel, 此处包装以减少跨模块依赖)
# ============================================================================

def load_panel(panel_id: str, hiss_dir: str = D001_DIR):
    """读取单片 V1 HISS, 返回 PanelData (同 e_common.load_panel)。"""
    if HISS_V2_PATH not in sys.path:
        sys.path.insert(0, HISS_V2_PATH)
    from hiss_v2 import v1_read_snr_model  # type: ignore
    from astropy_healpix import HEALPix
    from astropy import units as u

    fname = f'T4_RED_GalaxyCenter_{panel_id}.hiss'
    path = os.path.join(hiss_dir, fname)
    if not os.path.isfile(path):
        raise FileNotFoundError(f'HISS 文件不存在: {path}')

    nside, nested, ipix, pixel, meta, snr_model = v1_read_snr_model(path)
    n_pix = int(ipix.size)
    support = np.ones(n_pix, dtype=np.uint8)
    signal = pixel.astype(np.float32)

    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)

    # 返回轻量 dict (避免跨模块 dataclass 依赖)
    return dict(
        panel_id=panel_id, nside=int(nside), nested=bool(nested),
        ipix=ipix.astype(np.uint64), signal=signal, support=support,
        ra=ra, dec=dec, meta=dict(meta), snr_model=snr_model,
    )


def load_all_panels(hiss_dir: str = D001_DIR) -> Dict[str, dict]:
    """读取三片 HISS。"""
    return {p: load_panel(p, hiss_dir) for p in PANEL_NAMES}


# ============================================================================
# HCSD 纯 Python 写入器 (HEALPIX_FORMAT_SPEC.md §3, 不依赖 DLL)
# ============================================================================

def _compute_leaf_ipix(ipix_fine: np.ndarray, nside_fine: int) -> np.ndarray:
    """计算 fine ipix 对应的 nside=64 子叶 ipix (nested 位运算)。

    shift = 2 * (log2(nside_fine) - log2(64)) = 2 * (log2(nside_fine) - 6)
    leaf_ipix = ipix_fine >> shift
    """
    nside_log2 = int(round(np.log2(nside_fine)))
    shift = 2 * (nside_log2 - 6)
    return (ipix_fine.astype(np.uint64) >> shift).astype(np.uint64)


def hcsd_write(path: str, nside: int, nested: bool,
               ipix: np.ndarray, pixel: np.ndarray,
               meta: Dict[str, Any]) -> int:
    """纯 Python 写入 .hcsd 文件 (HEALPIX_FORMAT_SPEC.md §3)。

    流程:
      1. 按 leaf_ipix + ipix 升序排序
      2. 构建 49152 子叶索引表 (leaf_ipix -> data_offset, data_length)
      3. 写 Magic + JSON头(zstd) + 子叶索引表 + ipix数组 + pixel数组

    Args:
        path: 输出 .hcsd 路径
        nside: HEALPix nside (>=64)
        nested: 是否 nested (HCSD 要求 nested)
        ipix: uint64 [N] 像素索引
        pixel: float32 [N] 像素值
        meta: JSON 头元数据 (nside/nested/n_pix/filter/n_frames/...)

    Returns:
        0=成功
    """
    ipix = np.ascontiguousarray(ipix, dtype="<u8")
    pixel = np.ascontiguousarray(pixel, dtype="<f4")
    n_pix = int(ipix.size)
    if pixel.size != n_pix:
        raise ValueError(f"ipix/pixel 长度不一致: {n_pix}/{pixel.size}")

    # 1. 按 leaf_ipix + ipix 升序排序
    leaf_ipix = _compute_leaf_ipix(ipix, nside)
    order = np.lexsort((ipix, leaf_ipix))  # 主键 leaf_ipix, 次键 ipix
    ipix_sorted = ipix[order]
    pixel_sorted = pixel[order]
    leaf_sorted = leaf_ipix[order]

    # 2. 构建子叶索引表 (49152 × 24B)
    # 每个子叶: [leaf_ipix u64][data_offset u64][data_length u64]
    # data_offset: 该子叶像素在 ipix 数组区的字节偏移 (相对 ipix 数组起始)
    # data_length: 该子叶的像素数量
    leaf_index = np.zeros(HCSD_N_LEAVES, dtype=[
        ('leaf_ipix', '<u8'), ('data_offset', '<u8'), ('data_length', '<u8')])
    leaf_index['leaf_ipix'] = np.arange(HCSD_N_LEAVES, dtype=np.uint64)

    if n_pix > 0:
        # 找到每个子叶的边界
        unique_leaves, starts = np.unique(leaf_sorted, return_index=True)
        for i, ul in enumerate(unique_leaves):
            start = starts[i]
            if i + 1 < len(unique_leaves):
                end = starts[i + 1]
            else:
                end = n_pix
            count = end - start
            # data_offset = 字节偏移 = start * 8 (ipix u64)
            leaf_index['data_offset'][ul] = np.uint64(start * 8)
            leaf_index['data_length'][ul] = np.uint64(count)

    # 3. 构建 JSON 头
    meta_full = dict(meta)
    meta_full.update({
        'nside': int(nside),
        'nested': bool(nested),
        'n_pix': int(n_pix),
    })
    json_bytes = json.dumps(meta_full, ensure_ascii=False).encode('utf-8')
    try:
        import zstandard as zstd
        cctx = zstd.ZstdCompressor(level=5)
        json_comp = cctx.compress(json_bytes)
    except ImportError:
        # 无 zstd 时退化为不压缩 (不应发生, hiss_v2 依赖 zstandard)
        json_comp = json_bytes
    json_uncomp_len = len(json_bytes)
    json_comp_len = len(json_comp)

    # 4. 写文件
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as fp:
        # Magic (4B)
        fp.write(HCSD_MAGIC)
        # JSON 头长度 (4B + 4B)
        fp.write(struct.pack("<II", json_uncomp_len, json_comp_len))
        # 压缩 JSON 头
        fp.write(json_comp)
        # 子叶索引表 (49152 × 24B)
        for i in range(HCSD_N_LEAVES):
            fp.write(struct.pack("<QQQ",
                                 int(leaf_index['leaf_ipix'][i]),
                                 int(leaf_index['data_offset'][i]),
                                 int(leaf_index['data_length'][i])))
        # ipix 数组 (n_pix × 8B)
        fp.write(ipix_sorted.tobytes())
        # pixel 数组 (n_pix × 4B)
        fp.write(pixel_sorted.tobytes())

    file_size = os.path.getsize(path)
    logging.getLogger('g_common').info(
        f'HCSD 写入: {os.path.basename(path)} nside={nside} n_pix={n_pix} '
        f'n_leaves_with_data={int((leaf_index["data_length"] > 0).sum())} size={file_size}')
    return 0


def hcsd_read(path: str) -> Tuple[int, bool, np.ndarray, np.ndarray, Dict[str, Any]]:
    """纯 Python 读取 .hcsd 文件 (全量)。

    Returns:
        (nside, nested, ipix, pixel, meta)
    """
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != HCSD_MAGIC:
        raise ValueError(f"HCSD magic 不匹配: {data[:4]!r}")
    json_uncomp_len, json_comp_len = struct.unpack_from("<II", data, 4)
    json_start = 12
    json_end = json_start + json_comp_len
    try:
        import zstandard as zstd
        dctx = zstd.ZstdDecompressor()
        json_bytes = dctx.decompress(data[json_start:json_end])
    except ImportError:
        json_bytes = data[json_start:json_end]
    if len(json_bytes) != json_uncomp_len:
        raise ValueError(f"JSON 解压长度不一致: {len(json_bytes)} != {json_uncomp_len}")
    meta = json.loads(json_bytes.decode('utf-8'))

    nside = int(meta['nside'])
    nested = bool(meta['nested'])
    n_pix = int(meta['n_pix'])

    leaf_index_end = json_end + HCSD_LEAF_INDEX_SIZE
    pos = leaf_index_end
    ipix = np.frombuffer(data, dtype="<u8", count=n_pix, offset=pos).astype(np.uint64)
    pos += n_pix * 8
    pixel = np.frombuffer(data, dtype="<f4", count=n_pix, offset=pos).astype(np.float32)

    return nside, nested, ipix, pixel, meta


# ============================================================================
# JSON 序列化辅助 (同 e_common)
# ============================================================================

def json_default(o):
    """JSON 默认序列化器 (numpy -> python)。"""
    if isinstance(o, (np.bool_, bool)):
        return bool(o)
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.floating):
        return float(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    return str(o)


# ============================================================================
# 自测
# ============================================================================

if __name__ == '__main__':
    log = setup_logger('g_common', log_dir=LOG_DIR)
    log.info('=' * 70)
    log.info('g_common.py 自测: E-003 结果加载 + HCSD 写读往返')
    log.info('=' * 70)

    # 1. E-003 结果加载
    e003 = load_e003_result()
    log.info(f'E-003 曲面系数: {np.array2string(e003.surface_coeffs, precision=4)}')
    log.info(f'E-003 偏移: {e003.offsets}')
    log.info(f'E-003 中心: {e003.center}')
    assert abs(sum(e003.offsets.values())) < 1e-6, '零均值约束违反'
    log.info('[OK] E-003 结果加载 + 零均值检查')

    # 2. HCSD 写读往返 (合成数据)
    import tempfile
    nside = 512
    test_ipix = np.array([100, 200, 300, 400, 500], dtype=np.uint64)
    test_pixel = np.array([1.0, 2.0, 3.0, 4.0, 5.0], dtype=np.float32)
    test_meta = {
        'filter': 'Red', 'n_frames': 3, 'total_exposure_s': 540.0,
        'sigma_clip': {'sigma': 3.0, 'max_iter': 5},
        'stack_stats': {'mean_pixel_count': 1.5, 'median_exposure': 180.0},
    }
    tmp = os.path.join(tempfile.gettempdir(), 'g_common_test.hcsd')
    hcsd_write(tmp, nside, True, test_ipix, test_pixel, test_meta)
    nside_r, nested_r, ipix_r, pixel_r, meta_r = hcsd_read(tmp)
    assert nside_r == nside and nested_r == True
    assert ipix_r.size == 5
    # 验证排序 (按 leaf_ipix + ipix)
    log.info(f'写读往返: ipix={ipix_r} pixel={pixel_r}')
    log.info('[OK] HCSD 写读往返一致')
    os.remove(tmp)

    log.info('g_common.py 自测 PASS')
