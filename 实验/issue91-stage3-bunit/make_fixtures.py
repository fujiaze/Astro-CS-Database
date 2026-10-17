#!/usr/bin/env python3
"""Issue #91：从同一个合法 HiPS 测试输入制作受控 BUNIT 变体。

唯一合法基础输入（base_hips）:
  * hips_order = 0，512×512 float32 FITS tiles（Norder0/Dir0/Npix0..11.fits），
    即 12 个 HEALPix face；properties 只声明合法面亮度身份。
  * metadata 与 tile 字节由 numpy 定种子确定性生成 → 任何一轮可逐字节复算。

受控变体（variants/<case>/）: 每个都是 base_hips 的**完整拷贝**，只重写
  signal/properties 里的单位声明块；与 BUNIT 无关的字节（tiles / manifest /
  其余 properties 行）逐字节不变（脚本内断言）。

用法:
    python3 实验/issue91-stage3-bunit/make_fixtures.py
输出:
    <本目录>/work/... 与 <本目录>/work/fixtures.json（含各输入 SHA-256）
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys

import numpy as np
from astropy.io import fits

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
BASE = os.path.join(WORK, "base_hips")
PARTIAL = os.path.join(WORK, "partial_hips")
VARIANTS = os.path.join(WORK, "variants")

NSIDE_ORDER = 0
TILE = 512
NFACES = 12

# 与 BUNIT 无关的 properties 行（冻结；所有变体逐字节共用）
PROPS_HEAD = [
    "creator_did = ivo://acsd/issue91/controlled",
    "obs_title = Issue91 controlled BUNIT fixture",
    "hips_version = 1.4",
    "hips_order = 0",
    "hips_tile_width = 512",
    "hips_frame = equatorial",
    "hips_ordering = NESTED",
    "dataproduct_type = image",
    "hips_tile_format = fits",
]

# 合法（生产形态）单位声明块：BUNIT + 辅助声明面 + 像素语义
UNIT_BLOCK_LEGAL = [
    "BUNIT=ADU/sr",
    "ACSD_SIGNAL_UNIT=ADU/sr",
    "ACSD_PIXEL_SEMANTICS=surface_brightness",
    "ACSD_PIXEL_AREA_POWER=-2",
]


def tile_data(face: int) -> np.ndarray:
    """确定性 tile 数据：位置相关但完全可复算（float32）。"""
    idx = np.arange(TILE * TILE, dtype=np.int64).reshape(TILE, TILE)
    base = (idx % 977).astype(np.float32) / np.float32(1000.0)
    return (base + np.float32(face)).astype(np.float32)


def write_tile(path: str, data: np.ndarray) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fits.PrimaryHDU(data=data.astype(np.float32)).writeto(path, overwrite=True)


def build_hips(root: str, faces) -> None:
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(os.path.join(root, "signal", "Norder0", "Dir0"))
    with open(os.path.join(root, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump({"schema": "acsd-hips-manifest/1",
                   "products": ["signal"],
                   "hips_root": root}, f, indent=2)
        f.write("\n")
    for face in faces:
        write_tile(os.path.join(root, "signal", "Norder0", "Dir0", f"Npix{face}.fits"),
                   tile_data(face))
    write_properties(root, UNIT_BLOCK_LEGAL)


def write_properties(root: str, unit_lines) -> bytes:
    text = "\n".join(PROPS_HEAD + list(unit_lines)) + "\n"
    blob = text.encode("utf-8")
    with open(os.path.join(root, "signal", "properties"), "wb") as f:
        f.write(blob)
    return blob


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ── 受控 BUNIT 矩阵 ────────────────────────────────────────────────────────────
# expect: ok  → exit 0 且发布 FITS
#         r3  → 明确错误（输入缺失/格式错/声明冲突；CLI exit 3）
#         r4  → 明确错误（科学语义/身份矛盾；CLI exit 4）
CASES = [
    # 合法明确 BUNIT（生产形态四键）
    dict(case="legal_explicit", unit=UNIT_BLOCK_LEGAL, expect="ok"),
    # 合法：裸 ADU + 像素语义声明（FZ-BUNIT-SEMANTICS (b) → canonical 归一）
    dict(case="legal_bare_adu_sb", expect="ok", unit=[
        "BUNIT=ADU",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 合法：显式立体角幂次等价写法（非 legacy 像元面积别名）
    dict(case="legal_sr_power_written", expect="ok", unit=[
        "BUNIT=ADU^1/sr^1",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # legacy 像元面积写法：冻结口径明确否定（"§31.1a 同时否定 'ADU/px^2'"）→ 科学语义拒
    dict(case="legacy_px2_alias", expect="r4", unit=[
        "BUNIT=ADU/px^2",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    dict(case="legacy_pixel2_alias", expect="r4", unit=[
        "BUNIT=ADU/pixel^2",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # BUNIT 缺失（辅助声明面仍在 → 仍缺 FITS canonical 键）
    dict(case="missing_bunit", expect="r3", unit=[
        "ACSD_SIGNAL_UNIT=ADU/sr",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 两个声明面都缺
    dict(case="missing_both", expect="r3", unit=[
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # BUNIT 存在但空串
    dict(case="empty_bunit", expect="r3", unit=[
        "BUNIT=",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 同值重复逻辑键（唯一声明合同 → 拒）
    dict(case="dup_same", expect="r3", unit=[
        "BUNIT=ADU/sr",
        "BUNIT=ADU/sr",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 不同值重复（冲突）
    dict(case="dup_diff", expect="r3", unit=[
        "BUNIT=ADU/sr",
        "BUNIT=ADU",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 大小写别名重复（同一 FITS 逻辑键）
    dict(case="case_alias_dup", expect="r3", unit=[
        "BUNIT=ADU/sr",
        "bunit=ADU/sr",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 两个声明面值不同 → 冲突
    dict(case="alias_conflict", expect="r3", unit=[
        "BUNIT=ADU/sr",
        "ACSD_SIGNAL_UNIT=ADU",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 单位格式非法
    dict(case="illegal_format", expect="r3", unit=[
        "BUNIT=banana",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 合法 FITS 量纲但消费者无法解释（非 ADU 承载族；#9 未闭合 → 不自行映射）
    dict(case="legal_but_unknown_vocab", expect="r3", unit=[
        "BUNIT=Jy/sr",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 标度已变却写成量的种类已变（上游明确禁写串）→ 不强行统一成 ADU/sr
    dict(case="applied_relative_unit", expect="r3", unit=[
        "BUNIT=ACSD_RELATIVE_FLUX",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=-2",
    ]),
    # 显式面亮度单位 ↔ integrated_flux 像元语义矛盾
    dict(case="semantics_conflict", expect="r4", unit=[
        "BUNIT=ADU/sr",
        "ACSD_PIXEL_SEMANTICS=integrated_flux",
        "ACSD_PIXEL_AREA_POWER=0",
    ]),
    # 裸 ADU + surface_brightness ↔ 面积幂次 0 矛盾
    dict(case="power_conflict", expect="r4", unit=[
        "BUNIT=ADU",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
        "ACSD_PIXEL_AREA_POWER=0",
    ]),
    # 可解析但非面亮度（signal 给方差面单位）
    dict(case="not_surface_brightness", expect="r4", unit=[
        "BUNIT=ADU^2/sr^2",
        "ACSD_PIXEL_SEMANTICS=surface_brightness",
    ]),
    # 裸 ADU 无像素语义 → 不可判
    dict(case="bare_adu_no_semantics", expect="r3", unit=[
        "BUNIT=ADU",
    ]),
    # 未知像元语义词
    dict(case="bad_pixel_semantics", expect="r4", unit=[
        "BUNIT=ADU/sr",
        "ACSD_PIXEL_SEMANTICS=per_pixel_counts",
    ]),
]


def main() -> int:
    os.makedirs(WORK, exist_ok=True)
    build_hips(BASE, range(NFACES))
    # 合法物理无覆盖：只发布 face 4 的 tile（其余 face 无 tile ⇒ 采样足迹外/内
    # 混合 —— 输出中心落在 face 4/5 边界处时，一部分像元有覆盖、一部分为 NaN）
    build_hips(PARTIAL, [4])

    if os.path.isdir(VARIANTS):
        shutil.rmtree(VARIANTS)
    os.makedirs(VARIANTS)

    manifest = {
        "base_hips": BASE,
        "partial_hips": PARTIAL,
        "base_properties_sha256": sha256_file(os.path.join(BASE, "signal", "properties")),
        "base_signal_tile_sha256": {
            f"Npix{i}.fits": sha256_file(
                os.path.join(BASE, "signal", "Norder0", "Dir0", f"Npix{i}.fits"))
            for i in range(NFACES)
        },
        "partial_signal_tile_sha256": {
            "Npix4.fits": sha256_file(
                os.path.join(PARTIAL, "signal", "Norder0", "Dir0", "Npix4.fits"))
        },
        "variants": [],
    }

    for spec in CASES:
        case = spec["case"]
        dst = os.path.join(VARIANTS, case)
        shutil.copytree(BASE, dst)
        blob = write_properties(dst, spec["unit"])
        # 受控性断言：除 properties 外，变体与 base 逐字节一致
        for rel in ["manifest.json"] + [
                f"signal/Norder0/Dir0/Npix{i}.fits" for i in range(NFACES)]:
            a = sha256_file(os.path.join(BASE, rel))
            b = sha256_file(os.path.join(dst, rel))
            assert a == b, f"variant {case} mutated non-BUNIT byte: {rel}"
        manifest["variants"].append({
            "case": case,
            "hips_dir": dst,
            "expect": spec["expect"],
            "unit_lines": spec["unit"],
            "properties_sha256": hashlib.sha256(blob).hexdigest(),
        })
        print(f"variant {case:26s} expect={spec['expect']:2s} props_sha={manifest['variants'][-1]['properties_sha256'][:12]}")

    with open(os.path.join(WORK, "fixtures.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"wrote {os.path.join(WORK, 'fixtures.json')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
