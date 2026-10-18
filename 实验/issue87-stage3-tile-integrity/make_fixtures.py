#!/usr/bin/env python3
"""Issue #87 / E2：Stage3 HiPS tile 读取完整性 —— 受控输入夹具生成器。

生成**只改变指定 tile 字节/存在/访问状态**的 HiPS 产品集，并冻结输入哈希：
  * 同一合法产品（order-0 12 tile）作为基准；每个变体只改一个/几个 tile；
  * MOC 覆盖声明（MOC_LEAF / MOC_UNDETERMINED / MANIFEST_ONLY / NONE）分级；
  * FITS CHECKSUM/DATASUM 正例、bitflip 反例、缺校验字段；
  * 文件截断、非法 NAXIS/尺寸、非法 BITPIX、目录占位（受控 I/O 故障）。

输出:
  work/fixtures/<case>/           受控 HiPS 产品集（properties/manifest/Moc/tile）
  work/real/hips/                 真实 writer 产品的**只读**单位归一化视图（symlink）
  fixtures.json                   输入哈希 + 场景矩阵（run_matrix.py 消费）

用法:
  python3 实验/issue87-stage3-tile-integrity/make_fixtures.py
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
FIX = os.path.join(WORK, "fixtures")
REAL_SRC = os.path.join(REPO, "run", "HIPS-PACK-01", "corpus_big")
REAL_DST = os.path.join(WORK, "real", "hips")

import healpy as hp
from astropy.io import fits
from astropy.table import Table
from astropy.wcs import WCS

TILE_PX = 512
K_ORDER0 = 0


# ── 基础写入 ────────────────────────────────────────────────────────────────
def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def props_text(order: int, *, bunit: str = "ADU/sr",
               semantics: str = "surface_brightness",
               power: str = "-2") -> str:
    return "\n".join([
        "creator_did = ivo://acsd/issue87/controlled",
        "obs_title = Issue87 controlled tile-integrity fixture",
        "hips_version = 1.4",
        f"hips_order = {order}",
        f"hips_tile_width = {TILE_PX}",
        "hips_frame = equatorial",
        "hips_ordering = NESTED",
        "dataproduct_type = image",
        "hips_tile_format = fits",
        f"BUNIT={bunit}",
        f"ACSD_SIGNAL_UNIT={bunit}",
        f"ACSD_PIXEL_SEMANTICS={semantics}",
        f"ACSD_PIXEL_AREA_POWER={power}",
        "",
    ])


def write_props(root: str, order: int, **kw) -> str:
    p = os.path.join(root, "signal", "properties")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(props_text(order, **kw))
    return p


def write_manifest(root: str, n_leaf_tiles: int | None = None) -> str:
    m = {"schema": "acsd-hips-manifest/1", "products": ["signal"]}
    if n_leaf_tiles is not None:
        m["n_leaf_tiles"] = int(n_leaf_tiles)
    p = os.path.join(root, "manifest.json")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        json.dump(m, f, indent=2)
        f.write("\n")
    return p


def uniq_value(order: int, ipix: int) -> int:
    return 4 * (4 ** order) + int(ipix)


def write_moc(root: str, order: int, ipixs, *, entry_order: int | None = None) -> str:
    """写 signal/Moc.fits（PRIMARY 空 + BINTABLE UNIQ，与 writer 同结构）。
    entry_order < order 时故意写低阶(父级)条目，用于 FOOTPRINT_UNDETERMINED。"""
    eo = order if entry_order is None else int(entry_order)
    p = os.path.join(root, "signal", "Moc.fits")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    uniq = np.array([uniq_value(eo, ip) for ip in ipixs], dtype=np.int64)
    table = Table({"UNIQ": uniq})
    hdu = fits.HDUList([fits.PrimaryHDU(),
                        fits.BinTableHDU(table, name="MOC")])
    hdu.writeto(p, overwrite=True)
    return p


def tile_path(root: str, order: int, ipix: int) -> str:
    d = (int(ipix) // 10000) * 10000
    p = os.path.join(root, "signal", f"Norder{order}", f"Dir{d}",
                     f"Npix{int(ipix)}.fits")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


def write_tile(root: str, order: int, ipix: int, value: float, *,
               checksum: bool = True, bitpix: int = -32,
               shape: tuple[int, int] = (TILE_PX, TILE_PX)) -> str:
    p = tile_path(root, order, ipix)
    if bitpix == -64:
        data = np.full(shape, value, dtype="f8")
    elif bitpix == 16:
        data = np.full(shape, int(value), dtype="i2")
    elif bitpix == 32:
        data = np.full(shape, int(value), dtype="i4")
    else:
        data = np.full(shape, value, dtype="f4")
    fits.HDUList([fits.PrimaryHDU(data)]).writeto(p, overwrite=True,
                                                  checksum=checksum)
    return p


def flip_data_byte(path: str, offset: int = 160) -> None:
    """在头之后的数据区翻转 1 bit（头保持可解析；DATASUM 失配）。
    offset 相对数据区起点；默认 160 落在首行，center_bit 用于让采样区看到坏值。"""
    raw = bytearray(open(path, "rb").read())
    # 找 END 卡后的 2880 对齐位置
    pos = 0
    while pos + 80 <= len(raw):
        card = bytes(raw[pos:pos + 80])
        if card[:3] == b"END" and card[3:].strip() == b"":
            pos += 80
            break
        pos += 80
    hdr_len = ((pos + 2879) // 2880) * 2880
    idx = hdr_len + offset
    raw[idx] ^= 0x01
    open(path, "wb").write(raw)


def center_bit_offset() -> int:
    """tile 几何中心像元（row=256,col=256）的 float32 最低字节偏移。"""
    return (256 * TILE_PX + 256) * 4 + 3


def truncate_file(path: str, keep: int = 1500) -> None:
    raw = open(path, "rb").read()
    open(path, "wb").write(raw[:keep])


def replace_with_dir(path: str) -> None:
    if os.path.isdir(path):
        shutil.rmtree(path)
    elif os.path.exists(path):
        os.remove(path)
    os.makedirs(path)


# ── 几何 ────────────────────────────────────────────────────────────────────
def tile_center(order: int, ipix: int) -> tuple[float, float]:
    nside = 2 ** order
    theta, phi = hp.pix2ang(nside, int(ipix), nest=True)
    return float(np.degrees(phi)), float(90.0 - np.degrees(theta))


def tiles_in_field(ra: float, dec: float, w: int, h: int, scale: float,
                   order: int) -> list[int]:
    """与生产 p3_wcs_make(east_left, PA=0) 同构的 TAN WCS：CD=diag(-s,+s)。"""
    wcs = WCS(naxis=2)
    wcs.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    wcs.wcs.crval = [ra, dec]
    wcs.wcs.crpix = [(w + 1) / 2.0, (h + 1) / 2.0]
    wcs.wcs.cd = [[-scale, 0.0], [0.0, scale]]
    x, y = np.meshgrid(np.arange(w, dtype=float), np.arange(h, dtype=float))
    ra_p, dec_p = wcs.all_pix2world(x, y, 0)
    ok = np.isfinite(ra_p) & np.isfinite(dec_p)
    leaf_nside = TILE_PX << order
    ipix = hp.ang2pix(leaf_nside, np.radians(90.0 - dec_p[ok]),
                      np.radians(ra_p[ok]), nest=True)
    return sorted({int(v) >> 18 for v in ipix})


# ── 场景 ────────────────────────────────────────────────────────────────────
SCENARIOS: list[dict] = []


def input_hashes(root: str) -> dict:
    out = {}
    for rel in ("signal/properties", "manifest.json", "signal/Moc.fits"):
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            out[rel] = sha256_file(p)
    sig = os.path.join(root, "signal")
    if os.path.isdir(sig):
        for dirpath, _, files in os.walk(sig):
            for fn in sorted(files):
                if fn.startswith("Npix") and fn.endswith(".fits"):
                    p = os.path.join(dirpath, fn)
                    rel = os.path.relpath(p, root)
                    out[rel] = sha256_file(p)
    return out


def add_case(name: str, root: str, *, order: int, expect: str,
             config: dict, oracle: dict, expect_code: str | None = None,
             expect_tiles: list[int] | None = None,
             expect_codes: list[str] | None = None,
             expect_coverage_verified: bool | None = None,
             repeat: int = 1, note: str = "") -> None:
    SCENARIOS.append({
        "case": name,
        "hips_dir": root,
        "order": order,
        "expect": expect,
        "expect_code": expect_code,
        "expect_tiles": expect_tiles,
        "expect_codes": expect_codes,
        "expect_coverage_verified": expect_coverage_verified,
        "repeat": repeat,
        "config": config,
        "oracle": oracle,
        "note": note,
        "input_hashes": input_hashes(root),
    })


def cfg(ra: float, dec: float, w: int, h: int, scale: float,
        sampler: str = "nearest", sb: int = 64, max_tiles: int | None = None) -> dict:
    c = {"center": {"ra_deg": ra, "dec_deg": dec},
         "width_px": w, "height_px": h, "scale_deg_per_px": scale,
         "sampler": sampler, "longitude_parity": "east_left",
         "output_mode": "surface_brightness", "sub_block_px": sb}
    if max_tiles is not None:
        c["max_tiles"] = max_tiles
    return c


def fresh(name: str) -> str:
    root = os.path.join(FIX, name)
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    return root


# 1) 完整合法：12 tile 全部有值 + 完整 MOC + CHECKSUM
def build_legal_full() -> None:
    root = fresh("legal_full")
    write_props(root, 0)
    write_manifest(root, n_leaf_tiles=12)
    write_moc(root, 0, list(range(12)))
    for ip in range(12):
        write_tile(root, 0, ip, 1000.0 + ip, checksum=True)
    ra, dec = tile_center(0, 4)
    add_case("legal_full_nearest", root, order=0, expect="ok",
             config=cfg(ra, dec, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "constant", "tile": 4, "value": 1004.0},
             expect_coverage_verified=True,
             note="完整合法 HiPS；tile 4 常量 1004；nearest 应逐像素精确等于 1004")
    add_case("legal_full_bilinear", root, order=0, expect="ok",
             config=cfg(ra, dec, 128, 128, 0.01, "bilinear", 32),
             oracle={"kind": "bitwise_vs_prefix", "tile": 4},
             expect_coverage_verified=True,
             note="完整合法 HiPS；bilinear 与修复前逐位一致")


# 2) 合法稀疏 MOC：MOC 只声明 {4,5,6}
def build_legal_sparse_moc() -> None:
    root = fresh("legal_sparse_moc")
    write_props(root, 0)
    write_manifest(root, n_leaf_tiles=3)
    write_moc(root, 0, [4, 5, 6])
    for ip in (4, 5, 6):
        write_tile(root, 0, ip, 2000.0 + ip, checksum=True)
    ra4, dec4 = tile_center(0, 4)
    add_case("sparse_moc_covered", root, order=0, expect="ok",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "constant", "tile": 4, "value": 2004.0},
             expect_coverage_verified=True,
             note="合法稀疏 MOC：tile 4 在声明内且存在 → 正常有限值")
    ra7, dec7 = tile_center(0, 7)
    add_case("sparse_moc_outside", root, order=0, expect="ok",
             config=cfg(ra7, dec7, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "nan_all"}, expect_coverage_verified=True,
             note="真实无覆盖：tile 7 不在 MOC 声明内 → 合法 NaN/coverage=0，不误判损坏")


# 3) 无 MOC / 无覆盖计数的合法稀疏（HiPS MOC 可选提示；身份等级 NONE）
def build_legal_unhinted() -> None:
    root = fresh("legal_unhinted_sparse")
    write_props(root, 0)
    write_manifest(root, n_leaf_tiles=None)
    write_tile(root, 0, 4, 2104.0, checksum=True)
    ra4, dec4 = tile_center(0, 4)
    add_case("unhinted_covered", root, order=0, expect="ok",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "constant", "tile": 4, "value": 2104.0},
             expect_coverage_verified=False,
             note="无 MOC 且无覆盖计数：已有 tile 正常；身份等级 NONE 随成功产物落盘")
    ra7, dec7 = tile_center(0, 7)
    add_case("unhinted_outside", root, order=0, expect="ok",
             config=cfg(ra7, dec7, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "nan_all"}, expect_coverage_verified=False,
             note="#91 legal_partial_coverage 同构：无 MOC 稀疏产品缺失 ≠ 损坏")


# 4) 声明内缺失
def build_declared_missing() -> None:
    root = fresh("declared_missing")
    write_props(root, 0)
    write_manifest(root, n_leaf_tiles=2)
    write_moc(root, 0, [4, 5])
    write_tile(root, 0, 4, 3004.0, checksum=True)
    # tile 5 声明存在但文件缺失
    ra5, dec5 = tile_center(0, 5)
    add_case("declared_missing", root, order=0, expect="fail",
             config=cfg(ra5, dec5, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"},
             expect_code="P3-TILE-DECLARED-MISSING",
             expect_tiles=[5],
             note="MOC 声明 tile 5 但文件缺失 → 输入完整性错误，不得发布")


# 5) FITS 损坏族
def build_invalid_family() -> None:
    # 非法尺寸/NAXIS
    root = fresh("invalid_dims")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    write_tile(root, 0, 4, 4004.0, shape=(256, 256), checksum=True)
    ra4, dec4 = tile_center(0, 4)
    add_case("invalid_dims", root, order=0, expect="fail",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"}, expect_code="P3-TILE-INVALID",
             expect_tiles=[4], note="tile 尺寸 256² ≠ 512² → INVALID_TILE")

    # 非法 BITPIX（int16）
    root = fresh("invalid_bitpix")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    write_tile(root, 0, 4, 4004, bitpix=16, checksum=True)
    add_case("invalid_bitpix", root, order=0, expect="fail",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"}, expect_code="P3-TILE-INVALID",
             expect_tiles=[4], note="tile BITPIX=16 → INVALID_TILE（信号面只接受 -32/-64）")

    # 截断（头完整、数据截断）
    root = fresh("truncated")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    p = write_tile(root, 0, 4, 4004.0, checksum=True)
    truncate_file(p)
    add_case("truncated", root, order=0, expect="fail",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"}, expect_code="P3-TILE-INVALID",
             expect_tiles=[4], note="FITS 数据区截断 → INVALID_TILE（不是 I/O 故障）")


# 6) CHECKSUM 正例 / bitflip 反例 / 缺校验字段
def build_checksum_family() -> None:
    root = fresh("checksum_ok")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    write_tile(root, 0, 4, 5004.0, checksum=True)
    ra4, dec4 = tile_center(0, 4)
    add_case("checksum_ok", root, order=0, expect="ok",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "constant", "tile": 4, "value": 5004.0},
             note="CHECKSUM/DATASUM 正例：验证通过并正常出图")

    root = fresh("checksum_bad")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    p = write_tile(root, 0, 4, 5004.0, checksum=True)
    flip_data_byte(p, center_bit_offset())
    add_case("checksum_bad", root, order=0, expect="fail",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"}, expect_code="P3-TILE-CHECKSUM-FAILED",
             expect_tiles=[4], note="数据区 bitflip → CHECKSUM_FAILED")

    root = fresh("checksum_absent")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    write_tile(root, 0, 4, 5004.0, checksum=False)
    add_case("checksum_absent", root, order=0, expect="ok",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "constant", "tile": 4, "value": 5004.0},
             note="缺少 DATASUM/CHECKSUM：明确规则 = 不失败，但计数 checksum_absent_reads")


# 7) 受控 I/O 故障（文件位置被目录占位；root 下 chmod 不可控）
def build_io_error() -> None:
    root = fresh("io_error")
    write_props(root, 0)
    write_manifest(root, 2)
    write_moc(root, 0, [4, 5])
    # 必须保留一个**正常** tile 让 hips_product_validate 的目录枚举非空
    # （properties 节点先于 tile 读取；否则会变成"无 tile 产品"而不是本实验要测的
    #  tile 级受控 I/O 故障）。
    write_tile(root, 0, 5, 5005.0, checksum=True)
    p = tile_path(root, 0, 4)
    replace_with_dir(p)
    ra4, dec4 = tile_center(0, 4)
    add_case("io_error", root, order=0, expect="fail",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"}, expect_code="P3-TILE-IO-ERROR",
             expect_tiles=[4], note="tile 路径被目录占位 → 受控 open I/O 失败（IO_ERROR）")


# 8) 足迹不可判
def build_undetermined() -> None:
    # MOC 只有低阶(父级)条目，order-1 叶级身份不可归约
    root = fresh("undetermined_coarse_moc")
    write_props(root, 1)
    write_manifest(root, n_leaf_tiles=48)
    write_moc(root, 1, list(range(12)), entry_order=0)   # 12 个 order-0 父单元
    write_tile(root, 1, 0, 6000.0, checksum=True)        # 只有一个叶级 tile 存在
    ra5, dec5 = tile_center(1, 5)
    add_case("undetermined_coarse_moc", root, order=1, expect="fail",
             config=cfg(ra5, dec5, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"},
             expect_code="P3-TILE-FOOTPRINT-UNDETERMINED",
             expect_tiles=[5],
             note="MOC 仅低阶父级条目：不归约(#95 独立)，缺失判 UNDETERMINED，不冒充合法空洞")

    # 无 MOC，但 manifest 声称 12 个叶级 tile
    root = fresh("undetermined_manifest_only")
    write_props(root, 0)
    write_manifest(root, n_leaf_tiles=12)
    write_tile(root, 0, 4, 6104.0, checksum=True)
    ra5, dec5 = tile_center(0, 5)
    add_case("undetermined_manifest_only", root, order=0, expect="fail",
             config=cfg(ra5, dec5, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "fail"},
             expect_code="P3-TILE-FOOTPRINT-UNDETERMINED",
             expect_tiles=[5],
             note="无 MOC 但 manifest.n_leaf_tiles=12：无 tile 身份 → UNDETERMINED")


# 9) 多坏 tile + 多 worker 聚合
def build_multi_bad() -> None:
    root = fresh("multi_bad")
    write_props(root, 0)
    write_manifest(root, 12)
    write_moc(root, 0, list(range(12)))
    # 先按目标字段算出会被采样到的 tile（nearest 精确一对一）
    # 18.1° 视场（64 × 0.2°）满足 p3_wcs_make 的 FOV=scale·√(w²+h²)≤20° 适用域；
    # 中心 (45°,0°) 跨 order-0 的 4 个 tile（0/4/5/8），足以覆盖"不同坏 tile 并发"。
    center_ra, center_dec = 45.0, 0.0
    w = h = 64
    scale = 0.2
    touched = tiles_in_field(center_ra, center_dec, w, h, scale, 0)
    assert len(touched) >= 3, touched
    code_cycle = ["DECLARED_MISSING", "INVALID", "CHECKSUM_FAILED", "IO_ERROR"]
    expected_codes = {}
    for i, ip in enumerate(touched):
        kind = code_cycle[i % len(code_cycle)]
        if kind == "DECLARED_MISSING":
            pass  # 不写文件
        elif kind == "INVALID":
            write_tile(root, 0, ip, 7000.0 + ip, shape=(256, 256), checksum=True)
        elif kind == "CHECKSUM_FAILED":
            p = write_tile(root, 0, ip, 7000.0 + ip, checksum=True)
            flip_data_byte(p, center_bit_offset())
        else:
            replace_with_dir(tile_path(root, 0, ip))
        expected_codes[ip] = "P3-TILE-" + kind.replace("_", "-")
    first = min(touched)
    add_case("multi_bad", root, order=0, expect="fail",
             config=cfg(center_ra, center_dec, w, h, scale, "nearest", 16),
             oracle={"kind": "fail"}, expect_code=expected_codes[first],
             expect_tiles=sorted(touched),
             expect_codes=sorted(set(expected_codes.values())),
             note=f"不同坏 tile 并发：期望聚合 {len(touched)} 个 tile，按 tile_ipix 升序")


# 10) 同一坏 tile 被多 worker 访问
def build_same_bad_multi_worker() -> None:
    root = fresh("same_bad_multi_worker")
    write_props(root, 0)
    write_manifest(root, 1)
    write_moc(root, 0, [4])
    p = write_tile(root, 0, 4, 8004.0, checksum=True)
    flip_data_byte(p, center_bit_offset())
    ra4, dec4 = tile_center(0, 4)
    add_case("same_bad_multi_worker", root, order=0, expect="fail",
             config=cfg(ra4, dec4, 256, 256, 0.01, "nearest", 32),
             oracle={"kind": "fail"}, expect_code="P3-TILE-CHECKSUM-FAILED",
             expect_tiles=[4], repeat=3,
             note="同一坏 tile 被 4 worker 访问：聚合为 1 条，worker_min/max/count 稳定")


# 11) 真实 writer 产品（只读单位归一化视图；真实未覆盖仍为 NaN）
def build_real_product() -> None:
    if not os.path.isdir(REAL_SRC):
        print(f"WARN: real product missing: {REAL_SRC}", file=sys.stderr)
        return
    if os.path.isdir(REAL_DST):
        shutil.rmtree(REAL_DST)
    os.makedirs(os.path.join(REAL_DST, "signal"), exist_ok=True)
    # manifest 原样复制（n_leaf_tiles=523; 完成清单）
    shutil.copy2(os.path.join(REAL_SRC, "manifest.json"),
                 os.path.join(REAL_DST, "manifest.json"))
    # properties：只把单位声明归一化为 E1 已验收的 canonical 面亮度身份；
    # 像元字节/MOC/几何全部保持原样（原产品仍只读）。
    src_props = os.path.join(REAL_SRC, "signal", "properties")
    lines = []
    for line in open(src_props, encoding="utf-8").read().splitlines():
        s = line.strip()
        if s.startswith("BUNIT="):
            lines.append("BUNIT=ADU/sr")
        elif s.startswith("ASTROCS_SIGNAL_UNIT="):
            lines.append("ACSD_SIGNAL_UNIT=ADU/sr")
        elif s.startswith("ASTROCS_PIXEL_SEMANTICS="):
            lines.append("ACSD_PIXEL_SEMANTICS=surface_brightness")
        elif s.startswith("ASTROCS_PIXEL_AREA_POWER="):
            lines.append("ACSD_PIXEL_AREA_POWER=-2")
        else:
            lines.append(line)
    open(os.path.join(REAL_DST, "signal", "properties"), "w",
         encoding="utf-8").write("\n".join(lines) + "\n")
    # MOC 与 tile 树用 symlink（保持原产品只读；本实验不修改真实 tile）
    for name in os.listdir(os.path.join(REAL_SRC, "signal")):
        src = os.path.join(REAL_SRC, "signal", name)
        dst = os.path.join(REAL_DST, "signal", name)
        if name == "properties":
            continue
        os.symlink(os.path.abspath(src), dst)
    ra, dec = 84.7265625, -6.911928859889699
    add_case("real_covered", REAL_DST, order=9, expect="ok",
             config=cfg(ra, dec, 32, 32, 0.002, "nearest", 32),
             oracle={"kind": "real"}, expect_coverage_verified=True,
             note="真实 writer 产品（M42 管线 run/HIPS-PACK-01/corpus_big 的只读单位归一化视图）")
    add_case("real_partial", REAL_DST, order=9, expect="ok",
             config=cfg(ra, dec, 64, 64, 0.01, "nearest", 32),
             oracle={"kind": "real_partial"}, expect_coverage_verified=True,
             note="真实产品未覆盖区域仍为 NaN（覆盖 1778 / 未覆盖 2318 像元），不因严格化把合法空洞变成失败")


# 3b) Issue #87/E2-R：同一无 MOC 产品的两个"不可辨识世界"
#   世界 A 合法稀疏（tile 4 从未存在）；世界 B 曾有 tile 4 但被删除。
#   读侧仅凭文件集合无法区分 ⇒ 两者都必须发布 UNVERIFIED_LEGACY 身份，
#   不得把缺失当"已证实无覆盖"（旧行为即审计所指缺陷）。
def build_legacy_unverified_pair() -> None:
    ra4, dec4 = tile_center(0, 4)
    # 世界 A：合法稀疏 —— 只有 tile 7
    root_a = fresh("unhinted_legacy_sparse")
    write_props(root_a, 0)
    write_manifest(root_a, n_leaf_tiles=None)
    write_tile(root_a, 0, 7, 2207.0, checksum=True)
    add_case("unhinted_legacy_sparse", root_a, order=0, expect="ok",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "nan_all"}, expect_coverage_verified=False,
             note="世界A：合法稀疏无 MOC（tile4 从未存在）→ best-effort NaN + UNVERIFIED_LEGACY")
    # 世界 B：先写 tile 4 再删除（磁盘状态与世界 A 同构）
    root_b = fresh("unhinted_deleted")
    write_props(root_b, 0)
    write_manifest(root_b, n_leaf_tiles=None)
    write_tile(root_b, 0, 7, 2207.0, checksum=True)
    write_tile(root_b, 0, 4, 2204.0, checksum=True)
    os.remove(tile_path(root_b, 0, 4))   # 删除"本应存在"的 tile
    add_case("unhinted_deleted", root_b, order=0, expect="ok",
             config=cfg(ra4, dec4, 64, 64, 0.01, "nearest", 64),
             oracle={"kind": "nan_all"}, expect_coverage_verified=False,
             note="世界B：曾有 tile4 被删除；与 A 不可辨识 ⇒ 不得声称完整性已验证")


def main() -> int:
    os.makedirs(FIX, exist_ok=True)
    build_legal_full()
    build_legal_sparse_moc()
    build_legal_unhinted()
    build_legacy_unverified_pair()
    build_declared_missing()
    build_invalid_family()
    build_checksum_family()
    build_io_error()
    build_undetermined()
    build_multi_bad()
    build_same_bad_multi_worker()
    build_real_product()

    doc = {
        "schema": "acsd.issue87.fixtures/v1",
        "created_utc": __import__("datetime").datetime.utcnow().isoformat() + "Z",
        "repo": REPO,
        "tile_px": TILE_PX,
        "real_source": REAL_SRC,
        "real_view": REAL_DST,
        "scenarios": SCENARIOS,
    }
    out = os.path.join(WORK, "fixtures.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    print(f"wrote {out}: {len(SCENARIOS)} scenarios")
    for s in SCENARIOS:
        print(f"  {s['case']:32s} expect={s['expect']:4s} "
              f"code={s['expect_code']} tiles={s['expect_tiles']} "
              f"files={len(s['input_hashes'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
