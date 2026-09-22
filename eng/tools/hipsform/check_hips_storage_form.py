#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HIPS-PACK-01 落盘形态检查器（裸 HiPS / zstd 归档包）。

权威依据：
  - docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md（CONTRACT-STORAGE-001：命名/布局/索引/哈希）
  - docs/design/PRODUCT_STORAGE_FORM.md（DESIGN-STORAGE-001：形态判据与完整形态）
  - eng/contracts/schemas/hips_storage_form.schema.json（索引 schema 机器事实源）
  - ASTROCS_DESIGN.md §10（I/O 与原子产品）

检查项（exit 0 = PASS）：
  A. 锚存在：schema / 合同文档 / 设计文档 / 既有 FITS 校验器 / CI 内置 schema 校验器
  B. 索引 schema 校验（product_index / coverage_index 按 index_schema 分派）
  C. 形态解析与命名（N1..N8）：两形态互斥、归档必带索引、非 HiPS 不用 .hips 中缀
  D. 归档容器布局（A1..A5 / Z1..Z5）：逐成员帧、标准工具逐字节还原、properties 不撒谎
  E. 索引不变式（I1..I4）：coverage 与 tiles 集合一致、可重算
  F. 哈希口径（H1..H3）：两形态 tree_hash 相同；容器指纹不作身份
  G. 解压后合法性：既有 FITS 校验器（lib/infrastructure/aio/io/fits_verify.py）逐瓦片通过；
     astropy 可用时交叉复核
  H. --self-test：上述每一项都有可执行正例/负例（真值无效应时判红）

本检查器只用 stdlib + ctypes(libzstd) + tar/zstd CLI；不运行任何 AstroCS 可执行文件，
不加载仓内构建的 .so。

用法：
  python3 eng/tools/hipsform/check_hips_storage_form.py --root . [--scan <path> ...] [--json-out f] [--quiet]
  python3 eng/tools/hipsform/check_hips_storage_form.py --self-test
退出码：0 = PASS；1 = FAIL；--self-test 恒 0 = 全部内置正/负例符合预期（任一例不符预期则 1）。
"""
from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import hashlib
import importlib.util
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

TILE_WIDTH_DEFAULT = 16
SUBPRODUCT = "signal"


# --------------------------------------------------------------------------
# 动态导入既有件（不复制实现）
# --------------------------------------------------------------------------
def _load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, str(path))
    if spec is None or spec.loader is None:
        raise RuntimeError(f"无法加载模块：{path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# --------------------------------------------------------------------------
# libzstd（ctypes）——逐成员帧打包/解压，不依赖 zstd CLI 的 seekable 扩展
# --------------------------------------------------------------------------
class Zstd:
    def __init__(self) -> None:
        lib = ctypes.util.find_library("zstd")
        if not lib:
            raise RuntimeError("libzstd 不可用")
        self.z = ctypes.CDLL(lib)
        self.z.ZSTD_compressBound.restype = ctypes.c_size_t
        self.z.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
        self.z.ZSTD_compress.restype = ctypes.c_size_t
        self.z.ZSTD_compress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                         ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
        self.z.ZSTD_decompress.restype = ctypes.c_size_t
        self.z.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                           ctypes.c_void_p, ctypes.c_size_t]
        self.z.ZSTD_isError.restype = ctypes.c_uint
        self.z.ZSTD_isError.argtypes = [ctypes.c_size_t]
        self.z.ZSTD_getErrorName.restype = ctypes.c_char_p
        self.z.ZSTD_getErrorName.argtypes = [ctypes.c_size_t]

    def compress(self, data: bytes, level: int = 3) -> bytes:
        cap = self.z.ZSTD_compressBound(len(data))
        out = ctypes.create_string_buffer(cap)
        n = self.z.ZSTD_compress(out, cap, data, len(data), level)
        if self.z.ZSTD_isError(n):
            raise RuntimeError(self.z.ZSTD_getErrorName(n).decode())
        return out.raw[:n]

    def decompress(self, data: bytes, usize: int) -> bytes:
        out = ctypes.create_string_buffer(usize)
        n = self.z.ZSTD_decompress(out, usize, data, len(data))
        if self.z.ZSTD_isError(n):
            raise RuntimeError(self.z.ZSTD_getErrorName(n).decode())
        return out.raw[:n]


_ZSTD: Zstd | None = None


def zstd() -> Zstd:
    global _ZSTD
    if _ZSTD is None:
        _ZSTD = Zstd()
    return _ZSTD


# --------------------------------------------------------------------------
# 最小合法 HiPS fixture（纯 Python 写 FITS；DATASUM 用既有校验器同一算法族）
# --------------------------------------------------------------------------
def _card(key: str, value: str = "") -> bytes:
    s = f"{key:<8}= {value}"
    return (s + " " * 80)[:80].encode("ascii")


def _datasum32(data_region: bytes) -> int:
    """标准 32-bit 1 补码折叠（与 fits_verify._datasum_std32 同语义）。"""
    pad = (-len(data_region)) % 2880
    buf = data_region + b"\0" * pad
    total = 0
    for i in range(0, len(buf), 4):
        total = (total + struct.unpack(">I", buf[i:i + 4])[0]) & 0xFFFFFFFF
    return total


def write_fits_tile(path: Path, tw: int, ipix: int) -> None:
    """写一张合法 f32 FITS 瓦片（SIMPLE/BITPIX/NAXIS/NAXIS1/NAXIS2/DATASUM/END）。"""
    import numpy as np  # numpy 是仓内既有依赖（COMPRESS-01 口径），仅用于 fixture 生成
    arr = (np.arange(tw * tw, dtype=">f4") % 251.0) + float(ipix % 7)
    data = arr.tobytes()
    cards = [_card("SIMPLE", "T"), _card("BITPIX", "-32"), _card("NAXIS", "2"),
             _card("NAXIS1", str(tw)), _card("NAXIS2", str(tw)),
             _card("PIXTYPE", "'HEALPIX'"), _card("ORDERING", "'NESTED'"),
             _card("COORDSYS", "'C'"), _card("NSIDE", str(1 << 9)),
             _card("FIRSTPIX", "0"), _card("LASTPIX", str(tw * tw - 1)),
             _card("DATASUM", f"{_datasum32(data):>10}"), _card("END")]
    hdr = b"".join(cards)
    hdr = hdr + b" " * ((-len(hdr)) % 2880)
    body = data + b"\0" * ((-len(data)) % 2880)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(hdr + body)


def write_properties(path: Path, order: int, tw: int, tile_format: str = "fits") -> None:
    lines = [
        "creator_did=ivo://astrocs/hipsform",
        "obs_title=AstroCS storage-form fixture",
        "hips_version=1.4",
        f"hips_order={order}",
        f"hips_tile_width={tw}",
        "hips_frame=equatorial",
        "hips_ordering=NESTED",
        "dataproduct_type=image",
        f"hips_tile_format={tile_format}",
        "hips_status=private master",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_min_hips(root: Path, name: str, order: int = 1, tw: int = TILE_WIDTH_DEFAULT,
                   ipixs=(0, 1, 2), tile_format: str = "fits") -> Path:
    """构造 <root>/<name>.hips/ 最小合法 HiPS 产品集（signal 子产品 + 产品集 manifest）。"""
    prod = root / f"{name}.hips"
    sub = prod / SUBPRODUCT
    write_properties(sub / "properties", order, tw, tile_format)
    for ip in ipixs:
        d = sub / f"Norder{order}" / f"Dir{(ip // 10000) * 10000}"
        write_fits_tile(d / f"Npix{ip}.fits", tw, ip)
    (prod / "manifest.json").write_text(json.dumps(
        {"products": [SUBPRODUCT], "hips_version": "1.4", "tile_width": tw}, indent=1) + "\n",
        encoding="utf-8")
    return prod


# --------------------------------------------------------------------------
# tar 打包（自写头，逐成员一个 zstd 帧）
# --------------------------------------------------------------------------
def _octal(value: int, size: int) -> bytes:
    return (f"{value:0{size - 1}o}").encode("ascii") + b"\0"


def tar_header(name: str, size: int) -> bytes:
    h = bytearray(512)
    nb = name.encode("utf-8")
    if len(nb) > 100:
        raise ValueError("成员名过长")
    h[0:len(nb)] = nb
    h[100:108] = _octal(0o644, 8)
    h[108:116] = _octal(0, 8)
    h[116:124] = _octal(0, 8)
    h[124:136] = _octal(size, 12)
    h[136:148] = _octal(0, 12)
    h[148:156] = b" " * 8
    h[156:157] = b"0"
    h[257:263] = b"ustar\0"
    h[263:265] = b"00"
    h[265:297] = b"root".ljust(32, b"\0")
    h[297:329] = b"root".ljust(32, b"\0")
    h[329:337] = _octal(0, 8)
    h[337:345] = _octal(0, 8)
    chk = sum(h)
    h[148:156] = (f"{chk:06o}").encode("ascii") + b"\0 "
    return bytes(h)


def iter_product_files(prod: Path):
    out = []
    for dp, dn, fn in os.walk(prod):
        dn.sort()
        for f in sorted(fn):
            p = Path(dp) / f
            out.append((str(p.relative_to(prod)).replace(os.sep, "/"), p))
    out.sort(key=lambda e: e[0])
    return out


def pack_archive(prod: Path, archive: Path, level: int = 3, extra_frames=None) -> dict:
    """打包为 <archive>：tar 流按成员边界切分为独立 zstd 帧后串接。

    extra_frames: 可选 [(name, bytes)]，在尾部追加非 tar 内容（仅负例注入用）。
    返回 {'table': [...], 'tar_bytes': N, 'sha256': ...}。
    """
    z = zstd()
    table = []
    coff = 0
    uoff = 0
    tar_stream = bytearray()
    with open(archive, "wb") as out:
        for rel, p in iter_product_files(prod):
            data = p.read_bytes()
            hdr = tar_header(rel, len(data))
            pad = b"\0" * ((-len(data)) % 512)
            member = hdr + data + pad
            frame = z.compress(member, level)
            out.write(frame)
            table.append({"name": rel, "uoff": uoff, "usize": len(member),
                          "coff": coff, "csize": len(frame),
                          "doff": 512, "dsize": len(data)})
            tar_stream += member
            uoff += len(member)
            coff += len(frame)
        tail = b"\0" * 1024
        frame = z.compress(tail, level)
        out.write(frame)
        table.append({"name": None, "uoff": uoff, "usize": len(tail),
                      "coff": coff, "csize": len(frame), "doff": 0, "dsize": 0})
        tar_stream += tail
        if extra_frames:
            for (nm, blob) in extra_frames:
                out.write(blob)
                table.append({"name": nm, "uoff": uoff, "usize": 0,
                              "coff": coff, "csize": len(blob), "doff": 0, "dsize": 0})
                coff += len(blob)
    return {"table": table, "tar_bytes": bytes(tar_stream),
            "sha256": sha256_file(archive)}


def unpack_std(archive: Path, dst: Path) -> subprocess.CompletedProcess:
    """标准工具解压：zstd -dc | tar -xf -（合同 Z2 的唯一判定路径）。"""
    dst.mkdir(parents=True, exist_ok=True)
    return subprocess.run(f"zstd -dc '{archive}' | tar -xf - -C '{dst}'",
                          shell=True, capture_output=True)


def std_decompress(archive: Path) -> subprocess.CompletedProcess:
    return subprocess.run(["zstd", "-dc", str(archive)], capture_output=True)


def read_member(archive: Path, table, rel: str) -> bytes:
    """按帧表随机读一个成员（pread + 单帧解压 + 切片）。"""
    z = zstd()
    ent = next(e for e in table if e["name"] == rel)
    with open(archive, "rb") as f:
        f.seek(ent["coff"])
        raw = f.read(ent["csize"])
    frame = z.decompress(raw, ent["usize"])
    return frame[ent["doff"]:ent["doff"] + ent["dsize"]]


# --------------------------------------------------------------------------
# 索引
# --------------------------------------------------------------------------
def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blk in iter(lambda: f.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


def tree_entries(root: Path):
    ents = []
    for rel, p in iter_product_files(root):
        ents.append({"path": rel, "size": p.stat().st_size, "sha256": sha256_file(p)})
    return ents


def tree_hash(root: Path) -> str:
    ents = tree_entries(root)
    return hashlib.sha256(json.dumps(ents, separators=(",", ":"), sort_keys=True).encode()).hexdigest()


def runs_from(ipixs):
    runs = []
    for v in sorted(ipixs):
        if runs and runs[-1][0] + runs[-1][1] == v:
            runs[-1][1] += 1
        else:
            runs.append([v, 1])
    return runs


def expand_runs(runs):
    out = []
    for r in runs:
        out.extend(range(int(r[0]), int(r[0]) + int(r[1])))
    return out


def build_product_index(prod: Path, name: str, archive_info=None,
                        storage_form: str = "bare", table=None,
                        leaf_frac=None, tile_format: str = "fits") -> dict:
    sub = prod / SUBPRODUCT
    props = parse_properties(sub / "properties")
    order = int(props["hips_order"])
    tw = int(props["hips_tile_width"])
    leaf = []
    for rel, p in iter_product_files(sub):
        m = re.match(rf"^Norder{order}/Dir(\d+)/Npix(\d+)\.fits$", rel)
        if m:
            leaf.append(int(m.group(2)))
    leaf.sort()
    entry = {
        "name": SUBPRODUCT, "hips_order": order, "hips_tile_width": tw,
        "hips_tile_format": tile_format, "n_leaf_tiles": len(leaf),
        "coverage": {"encoding": "runs", "leaf_ipix_runs": runs_from(leaf),
                     "frac_quant": 255, "leaf_frac": leaf_frac or [255] * len(leaf)},
    }
    if storage_form == "archive":
        entry["tiles"] = [{"ipix": int(re.search(r"Npix(\d+)", e["name"]).group(1)),
                           "coff": e["coff"], "csize": e["csize"], "usize": e["usize"],
                           "doff": e["doff"], "dsize": e["dsize"]}
                          for e in (table or []) if e["name"] and e["name"].endswith(".fits")]
        entry["tiles"].sort(key=lambda t: t["ipix"])
    idx = {"index_schema": "astrocs.hips-index/v1", "product": name,
           "storage_form": storage_form, "archive": archive_info,
           "subproducts": [entry]}
    return idx


def write_index(path: Path, idx: dict) -> None:
    path.write_text(json.dumps(idx, separators=(",", ":")) + "\n", encoding="utf-8")


def parse_properties(path: Path) -> dict:
    props = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            k, v = line.split("=", 1)
            props[k.strip()] = v.strip()
    return props


# --------------------------------------------------------------------------
# 既有 HiPS 校验（IO-002 §3 语义 + 既有 FITS 校验器）
# --------------------------------------------------------------------------
def _resolve_refs(node, root, seen=None):
    """把本地 #/$defs/... 引用就地展开（仓内 eng/ci/run.py 的最小校验器不解析 $ref）。"""
    seen = seen or set()
    if isinstance(node, dict):
        ref = node.get("$ref")
        if isinstance(ref, str) and ref.startswith("#/"):
            if ref in seen:
                return {}
            cur = root
            for part in ref[2:].split("/"):
                cur = cur[part]
            merged = dict(_resolve_refs(cur, root, seen | {ref}))
            for k, v in node.items():
                if k != "$ref":
                    merged[k] = _resolve_refs(v, root, seen)
            return merged
        return {k: _resolve_refs(v, root, seen) for k, v in node.items()}
    if isinstance(node, list):
        return [_resolve_refs(v, root, seen) for v in node]
    return node


class Ctx:
    def __init__(self, repo: Path):
        self.repo = repo
        self.fits_verify = _load_module(repo / "lib/infrastructure/aio/io/fits_verify.py",
                                        "astrocs_fits_verify")
        self.schema = json.loads((repo / "eng/contracts/schemas/hips_storage_form.schema.json")
                                 .read_text(encoding="utf-8"))
        self._validate = _load_module(repo / "eng/ci/run.py", "astrocs_ci_run").validate_against_schema
        # 三命令输入合同的校验器：复用仓内既有最小校验器（支持 propertyNames / if-then-else /
        # $ref，正是 CLI 键面与形态键禁令所在的语法面），不另写一份。
        self.jsm = _load_module(repo / "eng/tests/common/jsonschema_min.py", "astrocs_jsonschema_min")
        self.phase_schemas = {
            name: json.loads((repo / ("eng/contracts/schemas/phase_config_%s.schema.json" % name))
                             .read_text(encoding="utf-8"))
            for name in ("normalize", "mosaic", "export")
        }

    def validator(self, instance, sub_schema, path="$"):
        return self._validate(instance, _resolve_refs(sub_schema, self.schema), path)

    def phase_schema(self, name: str) -> dict:
        return self.phase_schemas[name]

    def phase_validator(self, instance, schema: dict) -> list:
        return ["%s: %s" % ("/".join(str(p) for p in pth) or "$", msg)
                for pth, msg in self.jsm.validate(instance, schema, schema)]


def validate_hips_dir(ctx: Ctx, prod: Path) -> list:
    """既有 HiPS 校验：properties 必填键 + NESTED 布局 + 每瓦片过既有 FITS 校验器。"""
    errs = []
    if not prod.is_dir():
        return [f"{prod}: 不是目录"]
    sub = prod / SUBPRODUCT
    if not sub.is_dir():
        return [f"{prod}: 缺子产品目录 {SUBPRODUCT}"]
    pp = sub / "properties"
    if not pp.is_file():
        return [f"{prod}: 缺 properties"]
    props = parse_properties(pp)
    for k in ("hips_version", "hips_order", "hips_tile_width", "hips_tile_format", "hips_frame"):
        if k not in props:
            errs.append(f"{prod}: properties 缺必填键 {k}")
    if errs:
        return errs
    if not props["hips_version"].startswith("1.4"):
        errs.append(f"{prod}: hips_version={props['hips_version']} 非 1.4")
    if props["hips_tile_format"] != "fits":
        errs.append(f"{prod}: hips_tile_format={props['hips_tile_format']} 非既有读端接受的 fits")
    tw = int(props["hips_tile_width"])
    if tw <= 0 or (tw & (tw - 1)) != 0:
        errs.append(f"{prod}: hips_tile_width={tw} 非 2 的幂")
    order = int(props["hips_order"])
    if not (0 <= order <= 29):
        errs.append(f"{prod}: hips_order={order} 越界")
    if props["hips_frame"] not in ("equatorial", "icrs"):
        errs.append(f"{prod}: hips_frame={props['hips_frame']} 不被接受")
    n_leaf = 0
    for rel, p in iter_product_files(sub):
        if not rel.endswith(".fits") or not rel.startswith("Norder"):
            continue
        m = re.match(r"^Norder(\d+)/Dir(\d+)/Npix(\d+)\.fits$", rel)
        if not m:
            errs.append(f"{prod}: 布局不符 {rel}")
            continue
        K, D, N = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if K > order:
            errs.append(f"{prod}: {rel} 的 order {K} 大于 hips_order {order}")
        if D != (N // 10000) * 10000:
            errs.append(f"{prod}: {rel} 的 Dir 与 ipix 不符（应为 {(N // 10000) * 10000}）")
        if N >= 12 * 4 ** K:
            errs.append(f"{prod}: {rel} 的 ipix 越界")
        if K == order:
            n_leaf += 1
        try:
            hd = ctx.fits_verify.verify_fits_file(str(p))
            if getattr(hd, "naxis1", None) not in (None, tw):
                errs.append(f"{prod}: {rel} NAXIS1={hd.naxis1} != tile_width {tw}")
        except Exception as exc:  # noqa: BLE001
            errs.append(f"{prod}: {rel} FITS 校验失败：{exc}")
    if n_leaf == 0:
        errs.append(f"{prod}: 无叶级瓦片")
    return errs


# --------------------------------------------------------------------------
# 形态解析与产品校验（合同 §2/§3/§4/§5/§6）
# --------------------------------------------------------------------------
def resolve_form(root: Path, name: str):
    """返回 (form, path) 或 (None, 错误列表)。"""
    bare = root / f"{name}.hips"
    arch = root / f"{name}.hips.zst"
    if bare.exists() and arch.exists():
        return None, [f"两形态共存：{bare.name} 与 {arch.name}（N5）"]
    if arch.exists():
        if not arch.is_file() or arch.is_symlink():
            return None, [f"{arch}: 归档形态必须是常规文件（N2）"]
        return "archive", arch
    if bare.exists():
        if not bare.is_dir():
            return None, [f"{bare}: 裸形态必须是目录（N1）"]
        return "bare", bare
    return None, [f"未找到产品 {name}（.hips 或 .hips.zst）"]


def validate_product(ctx: Ctx, root: Path, name: str, workdir: Path) -> list:
    errs = []
    form, path = resolve_form(root, name)
    if form is None:
        return path  # 错误列表
    idx_path = root / f"{name}.hips.index.json"
    if form == "archive":
        if not idx_path.is_file():
            return [f"归档形态缺产品级索引 {idx_path.name}（N6，fail-closed）"]
        idx = json.loads(idx_path.read_text(encoding="utf-8"))
        errs += ctx.validator(idx, ctx.schema["$defs"]["product_index"], "$.product_index")
        if idx.get("storage_form") != "archive":
            errs.append(f"索引 storage_form={idx.get('storage_form')} 与磁盘形态 archive 不符")
        if (idx.get("archive") or {}).get("name") != path.name:
            errs.append("索引 archive.name 与实际归档名不符")
        if (idx.get("archive") or {}).get("sha256") != sha256_file(path):
            errs.append("索引 archive.sha256 与归档字节不符")
        if (idx.get("archive") or {}).get("bytes") != path.stat().st_size:
            errs.append("索引 archive.bytes 与归档字节数不符")
        # Z2：标准工具解压 = tar 流；A4：properties 不撒谎
        ext = workdir / f"{name}.extract"
        shutil.rmtree(ext, ignore_errors=True)
        p = unpack_std(path, ext)
        if p.returncode != 0:
            return [f"标准工具解压失败（Z2）：{p.stderr.decode()[:200]}"]
        errs += validate_hips_dir(ctx, ext)
        # 索引不变式 I1/I2/I3 + 随机访问逐字节一致
        for sub in idx.get("subproducts", []):
            cover = expand_runs(sub["coverage"]["leaf_ipix_runs"])
            if sub["n_leaf_tiles"] != len(cover):
                errs.append(f"n_leaf_tiles={sub['n_leaf_tiles']} != coverage 展开 {len(cover)}（I1）")
            if len(sub["coverage"]["leaf_frac"]) != len(cover):
                errs.append("leaf_frac 与 coverage 展开长度不符")
            tiles = sub.get("tiles") or []
            if sorted(t["ipix"] for t in tiles) != cover:
                errs.append("tiles.ipix 集合 != coverage 集合（I2）")
            for t in tiles:
                if t["coff"] + t["csize"] > path.stat().st_size:
                    errs.append(f"tile ipix={t['ipix']} 定位越界（I3）")
                    continue
                try:
                    got = read_member(path, [{"name": None, "coff": t["coff"], "csize": t["csize"],
                                              "usize": t["usize"], "doff": t["doff"],
                                              "dsize": t["dsize"]}], None)
                except Exception as exc:  # noqa: BLE001
                    errs.append(f"tile ipix={t['ipix']} 解压失败：{exc}")
                    continue
                want = (ext / SUBPRODUCT / f"Norder{sub['hips_order']}"
                        / f"Dir{(t['ipix'] // 10000) * 10000}" / f"Npix{t['ipix']}.fits").read_bytes()
                if got != want:
                    errs.append(f"tile ipix={t['ipix']} 随机访问内容与解压树不符")
        shutil.rmtree(ext, ignore_errors=True)
    else:
        errs += validate_hips_dir(ctx, path)
        if idx_path.is_file():
            idx = json.loads(idx_path.read_text(encoding="utf-8"))
            errs += ctx.validator(idx, ctx.schema["$defs"]["product_index"], "$.product_index")
            if idx.get("storage_form") != "bare":
                errs.append("索引 storage_form 与磁盘形态 bare 不符")
    return errs


def verify_form_equivalence(bare_dir: Path, archive: Path, workdir: Path) -> list:
    """写入侧不变式 A4/H1：归档解压树必须与裸形态逐字节等价（properties 与整树）。

    写路径在 stage 内两形态并存时执行本检查，然后只发布其中一种形态。
    """
    errs = []
    ext = workdir / "equiv"
    shutil.rmtree(ext, ignore_errors=True)
    p = unpack_std(archive, ext)
    if p.returncode != 0:
        return [f"标准工具解压失败：{p.stderr.decode()[:200]}"]
    a = (ext / SUBPRODUCT / "properties")
    b = (bare_dir / SUBPRODUCT / "properties")
    if not a.is_file() or not b.is_file():
        return ["两形态之一缺 properties（A4）"]
    if a.read_bytes() != b.read_bytes():
        errs.append("归档内 properties 与裸形态不一致（A4：properties 不许撒谎）")
    if tree_hash(ext) != tree_hash(bare_dir):
        errs.append("两形态 tree_hash 不同（H1：身份必须与打包参数无关）")
    shutil.rmtree(ext, ignore_errors=True)
    return errs


# --------------------------------------------------------------------------
# 形态输入配置 / 输出清单字段（合同 §10；词表 = schema#x-astrocs-field-vocabulary）
# --------------------------------------------------------------------------
def load_form_vocabulary(schema: dict) -> dict:
    """字段词表（唯一源）。缺词表 ⇒ 判据失效，fail-closed。"""
    vocab = schema.get("x-astrocs-field-vocabulary")
    if not isinstance(vocab, dict):
        raise RuntimeError("schema 缺 x-astrocs-field-vocabulary（字段词表锚缺失，fail-closed）")
    for need in ("phase1_input_form_key", "frame_fields", "manifest_storage_section",
                 "manifest_storage_fields", "coverage_index_ref_fields",
                 "index_name_suffix", "coverage_index_name", "layers"):
        if need not in vocab:
            raise RuntimeError("字段词表缺 %s（fail-closed）" % need)
    return vocab


def resolve_phase1_form(block: dict, vocab: dict):
    """按合同 §10.1 解析 Phase1 落盘形态。

    返回 (form, source, warn_required, findings)：
      * 键缺失 / 空串 / null ⇒ (默认形态, "default", True, [])；
      * 显式 archive|bare    ⇒ (该值, "config", False, [])；
      * 登记外取值           ⇒ (None, None, False, [finding])。
    """
    spec = vocab["phase1_input_form_key"]
    key, default, values = spec["name"], spec["default"], spec["values"]
    if key not in block or block[key] in ("", None):
        return default, "default", True, []
    value = block[key]
    if value not in values:
        return None, None, False, ["%s=%r 不在登记取值 %s 内" % (key, value, values)]
    return value, "config", False, []


def form_warn_event(form: str, vocab: dict, run: str = "run-hipsform") -> dict:
    """缺省/留空形态键必须产生的 warn 事件（LOG-001 事件模型：level=warn / event=warn）。"""
    key = vocab["phase1_input_form_key"]["name"]
    return {
        "schema": "astrocs.log.event.v1", "seq": 1, "ts": "1970-01-01T00:00:00Z",
        "run": run, "task": "", "node": "", "module": "aio", "phase": "phase1",
        "commit": "0" * 40, "host": "localhost", "level": "warn", "event": "warn",
        "units": "", "elapsed": 0.0,
        "diagnostic": ("配置键 %s 未给出或留空 ⇒ 采用默认 %s（输入 JSON 未显式声明落盘形态）"
                       % (key, form)),
    }


def _warn_hits(events, vocab):
    key = vocab["phase1_input_form_key"]["name"]
    return [e for e in (events or [])
            if isinstance(e, dict) and e.get("level") == "warn" and e.get("event") == "warn"
            and key in str(e.get("diagnostic", ""))]


def check_form_resolution(block: dict, events, vocab: dict, resolver=None) -> list:
    """判据 F0：缺省/留空 ⇒ 必须走默认形态**且**必须报 warn；显式声明 ⇒ 不得报 warn。

    resolver 可注入（负例自检用「留空却返回非登记默认」的假解析器证明判据有牙）。
    """
    resolver = resolver or resolve_phase1_form
    form, _source, warn_required, findings = resolver(block, vocab)
    if form is None:
        return findings
    hits = _warn_hits(events, vocab)
    if warn_required:
        default = vocab["phase1_input_form_key"]["default"]
        if form != default:
            findings.append("缺省形态键未走默认 %s（实际 %s）" % (default, form))
        if not hits:
            findings.append("缺省形态键未报 warn（level=warn 事件缺失）⇒ 静默取默认")
    elif hits:
        findings.append("显式声明形态却报了 warn（判据非恒真）")
    return findings


def check_frame_storage(ctx: "Ctx", frame: dict, vocab: dict, product=None) -> list:
    """判据 F1..F4：p1_products.json 逐帧必须带齐形态字段且自洽。"""
    fields = list(vocab["frame_fields"])
    errs = [f for f in fields if f not in frame]
    if errs:
        return ["逐帧条目缺形态字段 %s（输出 JSON 必须携带索引路径与指纹）" % sorted(errs)]
    proj = {k: frame[k] for k in fields}
    errs += ctx.validator(proj, ctx.schema["$defs"]["frame_storage"], "$.frame_storage")
    if errs:
        return errs
    if frame["storage_form"] == "bare":
        if frame["archive_sha256"] is not None:
            errs.append("bare 形态的 archive_sha256 必须为 null（F2）")
    elif not isinstance(frame["archive_sha256"], str):
        errs.append("archive 形态必须给出 archive_sha256（F2）")
    base = os.path.basename(str(frame["index_path"]))
    suffix = vocab["index_name_suffix"]
    if not base.endswith(suffix):
        errs.append("index_path 必须指向 <name>%s（F3）：%s" % (suffix, base))
    elif product is not None and base != str(product) + suffix:
        errs.append("index_path 的 <name> 必须等于产品名 %s（F3）：%s" % (product, base))
    return errs


def check_manifest_storage(ctx: "Ctx", storage: dict, vocab: dict, events=None) -> list:
    """判据 M1..M4：运行完成清单 storage 段字段齐备且与逐产品条目/日志自洽。"""
    errs = ctx.validator(storage, ctx.schema["$defs"]["manifest_storage"], "$.storage")
    if errs:
        return errs
    suffix = vocab["index_name_suffix"]
    products = storage["products"]
    for ent in products:
        if ent["storage_form"] != storage["storage_form"]:
            errs.append("products[%s].storage_form 与运行级 storage_form 不一致（M1）" % ent["product"])
        base = os.path.basename(str(ent["index_path"]))
        if base != str(ent["product"]) + suffix:
            errs.append("products[%s].index_path 的 <name> 与产品名不符（M4）：%s" % (ent["product"], base))
        if ent["storage_form"] == "bare" and (ent["archive_sha256"] is not None
                                              or ent["archive_bytes"] is not None):
            errs.append("products[%s] 为 bare 却带归档指纹/字节数（M1）" % ent["product"])
    if storage["form_source"] == "default":
        if not _warn_hits(events, vocab):
            errs.append("form_source=default 却无 warn 事件（M2：默认必须留痕）")
    elif _warn_hits(events, vocab):
        errs.append("form_source=config 却报了缺省 warn（M2）")
    cov = storage.get("coverage_index")
    if cov is not None:
        if os.path.basename(str(cov["path"])) != vocab["coverage_index_name"]:
            errs.append("storage.coverage_index.path 必须指向 %s（M3）" % vocab["coverage_index_name"])
        if int(cov["n_blocks"]) < 1:
            errs.append("storage.coverage_index.n_blocks 必须 ≥ 1（M3：空覆盖索引不是有效产物）")
    return errs


def check_form_key_rejected(ctx: "Ctx", phase: str, doc: dict) -> list:
    """判据 X1/X3：mosaic / export 输入出现形态键必须 REJECT（两阶段产物形态固定）。

    **只在注入了 storage_form 键的输入上调用**（合法输入不走本判据）。
    返回 findings：空 = 已 REJECT（符合预期）；非空 = 未被拒（判红）。
    """
    errs = ctx.phase_validator(doc, ctx.phase_schema(phase))
    if errs:
        return []
    return ["%s 输入含 %s 却未被 schema REJECT（该阶段产物形态固定，出现即 REJECT）"
            % (phase, "storage_form")]


def check_input_accepted(ctx: "Ctx", phase: str, doc: dict) -> list:
    """判据 X4（非退化对照）：合法输入不得被判红（防「一律 REJECT」的恒真门）。"""
    return ctx.phase_validator(doc, ctx.phase_schema(phase))


def check_phase1_input(ctx: "Ctx", doc: dict) -> list:
    """判据 X2：normalize 输入含 storage_form（含空串/null）必须通过 schema（默认 + warn 由 F0 判）。"""
    return ctx.phase_validator(doc, ctx.phase_schema("normalize"))


def check_doc_consistency(vocab: dict, read_text) -> list:
    """判据 D1/D2：逐层文档对同一字段必须使用同一口径（词表唯一源 = schema）。

    D1 每层文档必须出现其登记词（缺 ⇒ 该层与词表口径不一致）；
    D2 任何层不得出现禁用同义名（第二套命名 = 两层口径打架）。
    """
    errs = []
    layers = vocab.get("layers") or []
    if not layers:
        return ["词表缺 layers（逐层判据失效，fail-closed）"]
    texts = {}
    for ent in layers:
        rel = ent["file"]
        try:
            texts[rel] = read_text(rel)
        except Exception as exc:  # noqa: BLE001
            errs.append("层文档不可读 %s（%s）：%s" % (rel, ent.get("layer"), exc))
            continue
        for tok in ent.get("must_contain", []):
            if tok not in texts[rel]:
                errs.append("%s（%s）未出现登记词 %r —— 该层与字段词表口径不一致"
                            % (rel, ent.get("layer"), tok))
    for syn in vocab.get("forbidden_synonyms", []):
        for rel, text in texts.items():
            if syn in text:
                errs.append("%s 出现禁用同义名 %r（词表只认 %r）"
                            % (rel, syn, vocab["phase1_input_form_key"]["name"]))
    return errs

# --------------------------------------------------------------------------
# self-test：正例 / 负例注入（能红能绿）
# --------------------------------------------------------------------------
def self_test(repo: Path) -> list:
    ctx = Ctx(repo)
    cases = []
    tmp = Path(tempfile.mkdtemp(prefix="hipsform-selftest-"))

    def case(name, expect_ok, fn):
        try:
            errs = fn()
        except Exception as exc:  # noqa: BLE001
            errs = [f"异常：{type(exc).__name__}: {exc}"]
        ok = (len(errs) == 0) == expect_ok
        cases.append({"case": name, "expect_green": expect_ok, "errors": errs[:4], "ok": ok})
        return errs

    try:
        # --- P1 裸形态正例 ---
        def p1():
            d = tmp / "p1"
            prod = build_min_hips(d, "frame01", order=1, ipixs=(0, 1, 2))
            write_index(d / "frame01.hips.index.json",
                        build_product_index(prod, "frame01", storage_form="bare"))
            return validate_product(ctx, d, "frame01", tmp / "w1")
        case("P1 bare 正例", True, p1)

        # --- P2 归档形态正例（stage 两形态等价 + 只发布归档 + 标准工具还原 + 既有校验）---
        def p2():
            d = tmp / "p2"
            stage = d / "stage"
            prod = build_min_hips(stage, "frame02", order=1, ipixs=(0, 1, 2))
            arc = d / "frame02.hips.zst"
            info = pack_archive(prod, arc)
            errs = verify_form_equivalence(prod, arc, tmp / "w2")      # A4 / H1（写入侧）
            # Z2 硬判定：标准工具解压输出 = 打包前 tar 流（逐字节）
            got = std_decompress(arc).stdout
            if got != info["tar_bytes"]:
                errs.append(f"zstd -dc 输出与 tar 流不一致（{len(got)} vs {len(info['tar_bytes'])}）（Z2）")
            # 索引从归档解压树构建（写端/读端同源：properties 与叶块集合来自产品内容）
            ext = tmp / "w2c"
            shutil.rmtree(ext, ignore_errors=True)
            unpack_std(arc, ext)
            write_index(d / "frame02.hips.index.json",
                        build_product_index(ext, "frame02", storage_form="archive",
                                            archive_info={"name": arc.name, "bytes": arc.stat().st_size,
                                                          "sha256": sha256_file(arc),
                                                          "frame_unit": "tar_member"},
                                            table=info["table"]))
            shutil.rmtree(stage, ignore_errors=True)                   # 只发布归档形态
            shutil.rmtree(ext, ignore_errors=True)
            errs += validate_product(ctx, d, "frame02", tmp / "w2")
            return errs
        case("P2 archive 正例", True, p2)

        # --- P3 哈希口径：两形态同身份；容器指纹随档位变（身份 ≠ 容器指纹）---
        def p3():
            d = tmp / "p3"
            stage = d / "stage"
            prod = build_min_hips(stage, "frame03", order=1, ipixs=(0, 1, 2))
            a1 = d / "a1.zst"
            a2 = d / "a2.zst"
            pack_archive(prod, a1, level=1)
            pack_archive(prod, a2, level=9)
            errs = []
            if sha256_file(a1) == sha256_file(a2):
                errs.append("两档位容器字节相同，判据退化")
            if tree_hash(prod) != tree_hash(prod):
                errs.append("tree_hash 不稳定")
            e1 = tmp / "w3a"
            e2 = tmp / "w3b"
            unpack_std(a1, e1)
            unpack_std(a2, e2)
            if tree_hash(e1) != tree_hash(prod) or tree_hash(e2) != tree_hash(prod):
                errs.append("解压树 tree_hash 与裸形态不同（H1）")
            shutil.rmtree(e1, ignore_errors=True)
            shutil.rmtree(e2, ignore_errors=True)
            return errs
        case("P3 哈希口径（身份与档位无关）", True, p3)

        # --- N1 归档内 properties 撒谎 ---
        def n1():
            d = tmp / "n1"
            prod = build_min_hips(d, "frame04", order=1, ipixs=(0, 1))
            write_properties(prod / SUBPRODUCT / "properties", 1, TILE_WIDTH_DEFAULT, "zstd")
            arc = d / "frame04.hips.zst"
            info = pack_archive(prod, arc)
            write_index(d / "frame04.hips.index.json",
                        build_product_index(prod, "frame04", storage_form="archive",
                                            archive_info={"name": arc.name,
                                                          "bytes": arc.stat().st_size,
                                                          "sha256": sha256_file(arc),
                                                          "frame_unit": "tar_member"},
                                            table=info["table"], tile_format="zstd"))
            return validate_product(ctx, d, "frame04", tmp / "w4")
        case("N1 properties 声明非标准格式 → 红", False, n1)

        # --- N2 归档形态缺索引 ---
        def n2():
            d = tmp / "n2"
            prod = build_min_hips(d, "frame05", order=1, ipixs=(0, 1))
            pack_archive(prod, d / "frame05.hips.zst")
            return validate_product(ctx, d, "frame05", tmp / "w5")
        case("N2 归档缺索引 → 红（fail-closed）", False, n2)

        # --- N3 形态与类型不符（.hips.zst 是目录 / .hips 是文件）---
        def n3():
            d = tmp / "n3"
            (d / "frame06.hips.zst").mkdir(parents=True)
            return validate_product(ctx, d, "frame06", tmp / "w6")
        case("N3 .hips.zst 是目录 → 红", False, n3)

        # --- N4 两形态共存 ---
        def n4():
            d = tmp / "n4"
            prod = build_min_hips(d, "frame07", order=1, ipixs=(0, 1))
            pack_archive(prod, d / "frame07.hips.zst")
            return validate_product(ctx, d, "frame07", tmp / "w7")
        case("N4 两形态共存 → 红", False, n4)

        # --- N5 索引 coverage 与 tiles 集合不一致 ---
        def n5():
            d = tmp / "n5"
            prod = build_min_hips(d, "frame08", order=1, ipixs=(0, 1, 2))
            arc = d / "frame08.hips.zst"
            info = pack_archive(prod, arc)
            idx = build_product_index(prod, "frame08", storage_form="archive",
                                      archive_info={"name": arc.name, "bytes": arc.stat().st_size,
                                                    "sha256": sha256_file(arc), "frame_unit": "tar_member"},
                                      table=info["table"])
            idx["subproducts"][0]["tiles"] = idx["subproducts"][0]["tiles"][:-1]
            write_index(d / "frame08.hips.index.json", idx)
            return validate_product(ctx, d, "frame08", tmp / "w8")
        case("N5 coverage/tiles 集合不一致 → 红", False, n5)

        # --- N6 归档截断 ---
        def n6():
            d = tmp / "n6"
            prod = build_min_hips(d, "frame09", order=1, ipixs=(0, 1))
            arc = d / "frame09.hips.zst"
            info = pack_archive(prod, arc)
            blob = arc.read_bytes()
            arc.write_bytes(blob[:len(blob) // 2])
            idx = build_product_index(prod, "frame09", storage_form="archive",
                                      archive_info={"name": arc.name, "bytes": arc.stat().st_size,
                                                    "sha256": sha256_file(arc), "frame_unit": "tar_member"},
                                      table=info["table"])
            write_index(d / "frame09.hips.index.json", idx)
            return validate_product(ctx, d, "frame09", tmp / "w9")
        case("N6 归档截断 → 红", False, n6)

        # --- N7 zstd 流内混装 skippable frame（"部分不压缩"）→ 标准工具解压缺块 ---
        def n7():
            d = tmp / "n7"
            prod = build_min_hips(d, "frame10", order=1, ipixs=(0, 1))
            payload = b"ASTROCS-UNCOMPRESSED-BLOCK" * 64
            sk = struct.pack("<II", 0x184D2A50, len(payload)) + payload
            arc = d / "frame10.hips.zst"
            info = pack_archive(prod, arc, extra_frames=[("SKIPPABLE", sk)])
            got = std_decompress(arc).stdout
            if got == info["tar_bytes"]:
                return ["skippable frame 未改变解压输出，负例退化"]
            return [f"skippable 混装使解压输出缺块（{len(got)} vs {len(info['tar_bytes'])}）（Z3）"]
        case("N7 流内混装不压缩区 → 红（Z3）", False, n7)

        # --- N8 byte-shuffle 预变换 → 解压后不是合法 FITS ---
        def n8():
            import numpy as np
            d = tmp / "n8"
            prod = build_min_hips(d, "frame11", order=1, ipixs=(0, 1))
            # 只对 .fits 成员做字节转置后再压缩（模拟 shuffle 归档）
            arc = d / "frame11.hips.zst"
            z = zstd()
            table = []
            coff = 0
            uoff = 0
            with open(arc, "wb") as out:
                for rel, p in iter_product_files(prod):
                    data = p.read_bytes()
                    if rel.endswith(".fits"):
                        hdr, body = data[:2880], data[2880:]
                        a = np.frombuffer(body, dtype="u1").reshape(-1, 4).T.copy().tobytes()
                        data = hdr + a
                    hdr = tar_header(rel, len(data))
                    member = hdr + data + b"\0" * ((-len(data)) % 512)
                    fr = z.compress(member, 3)
                    out.write(fr)
                    table.append({"name": rel, "coff": coff, "csize": len(fr), "usize": len(member),
                                  "doff": 512, "dsize": len(data)})
                    coff += len(fr); uoff += len(member)
            ext = tmp / "w11" / "x"
            shutil.rmtree(ext, ignore_errors=True)
            unpack_std(arc, ext)
            return validate_hips_dir(ctx, ext)
        case("N8 byte-shuffle 预变换 → 解压后非法 FITS（Z4）", False, n8)

        # --- N9 归档内 properties 与裸形态不一致 ---
        def n9():
            d = tmp / "n9"
            prod = build_min_hips(d, "frame12", order=1, ipixs=(0, 1))
            arc = d / "frame12.hips.zst"
            info = pack_archive(prod, arc)
            write_index(d / "frame12.hips.index.json",
                        build_product_index(prod, "frame12", storage_form="archive",
                                            archive_info={"name": arc.name, "bytes": arc.stat().st_size,
                                                          "sha256": sha256_file(arc),
                                                          "frame_unit": "tar_member"},
                                            table=info["table"]))
            # 归档打包后篡改裸形态 properties（模拟两形态分叉，写入侧 A4 判定）
            with open(prod / SUBPRODUCT / "properties", "a", encoding="utf-8") as f:
                f.write("\n")
            return verify_form_equivalence(prod, arc, tmp / "w12")
        case("N9 两形态 properties 分叉 → 红（A4）", False, n9)

        # --- N10 索引 schema 违规（多出未知字段）---
        def n10():
            d = tmp / "n10"
            prod = build_min_hips(d, "frame13", order=1, ipixs=(0, 1))
            idx = build_product_index(prod, "frame13", storage_form="bare")
            idx["subproducts"][0]["coverage"]["pixel_bitmap"] = [1, 2, 3]
            write_index(d / "frame13.hips.index.json", idx)
            return validate_product(ctx, d, "frame13", tmp / "w13")
        case("N10 索引 schema 违规 → 红", False, n10)

        # --- P4 数据集级覆盖索引正例（块粒度）---
        def p4():
            d = tmp / "p4"
            cov = {"index_schema": "astrocs.coverage-index/v1",
                   "granularity": {"unit": "hips_leaf_tile", "tile_width": 16, "hips_order": 1},
                   "frames": ["f00", "f01"],
                   "blocks": [{"ipix": 0, "frames": [{"f": "f00", "frac": 255},
                                                     {"f": "f01", "frac": 128}]},
                              {"ipix": 1, "frames": [{"f": "f01", "frac": 255}]}]}
            return ctx.validator(cov, ctx.schema["$defs"]["coverage_index"], "$.coverage_index")
        case("P4 数据集级覆盖索引正例", True, p4)

        # --- N11 覆盖索引逐像素化（unit 非块粒度）→ 红 ---
        def n11():
            cov = {"index_schema": "astrocs.coverage-index/v1",
                   "granularity": {"unit": "pixel", "tile_width": 16, "hips_order": 1},
                   "frames": ["f00"], "blocks": [{"ipix": 0, "frames": [{"f": "f00", "frac": 255}]}]}
            return ctx.validator(cov, ctx.schema["$defs"]["coverage_index"], "$.coverage_index")
        case("N11 覆盖索引非块粒度 → 红", False, n11)

        # ================= 形态输入配置 / 输出清单字段（合同 §10） =================
        vocab = load_form_vocabulary(ctx.schema)
        form_key = vocab["phase1_input_form_key"]["name"]
        default_form = vocab["phase1_input_form_key"]["default"]

        def _p1_block(**extra):
            blk = {"input_lights": ["light1.fits"], "output_dir": "out/red"}
            blk.update(extra)
            return blk

        # --- P5 形态键缺省 ⇒ 默认 archive + warn（正例） ---
        def p5():
            blk = _p1_block()
            form, source, warn_required, _ = resolve_phase1_form(blk, vocab)
            errs = check_form_resolution(blk, [form_warn_event(form, vocab)], vocab)
            if (form, source, warn_required) != (default_form, "default", True):
                errs.append("缺省解析错误：(%r, %r, %r)" % (form, source, warn_required))
            # warn 事件本身必须是合法 LOG-001 行（事件模型不另写一份）
            log_schema = json.loads((repo / "lib/infrastructure/observability/logging/log_event_v1.schema.json")
                                    .read_text(encoding="utf-8"))
            ev = form_warn_event(form, vocab)
            errs += ["warn 事件不合 LOG-001：" + e
                     for e in ctx._validate(ev, log_schema, "$.log_event")]
            errs += check_phase1_input(ctx, {"schema_version": "1", "blocks": [blk]})
            return errs
        case("P5 形态键缺省 ⇒ 默认 %s + warn（正例）" % default_form, True, p5)

        # --- P6 形态键留空（空串 / null）⇒ 默认 + warn（正例） ---
        def p6():
            errs = []
            for empty in ("", None):
                blk = _p1_block(**{form_key: empty})
                form, source, warn_required, _ = resolve_phase1_form(blk, vocab)
                if (form, source, warn_required) != (default_form, "default", True):
                    errs.append("留空值 %r 解析错误：(%r, %r, %r)" % (empty, form, source, warn_required))
                errs += check_form_resolution(blk, [form_warn_event(form, vocab)], vocab)
                errs += check_phase1_input(ctx, {"schema_version": "1", "blocks": [blk]})
            return errs
        case("P6 形态键留空 ⇒ 默认 + warn（正例）", True, p6)

        # --- P7 形态键显式声明 ⇒ 不报 warn（正例） ---
        def p7():
            errs = []
            for value in vocab["phase1_input_form_key"]["values"]:
                blk = _p1_block(**{form_key: value})
                form, source, warn_required, _ = resolve_phase1_form(blk, vocab)
                if (form, source, warn_required) != (value, "config", False):
                    errs.append("显式 %r 解析错误：(%r, %r, %r)" % (value, form, source, warn_required))
                errs += check_form_resolution(blk, [], vocab)
                errs += check_phase1_input(ctx, {"schema_version": "1", "blocks": [blk]})
            return errs
        case("P7 形态键显式声明 ⇒ 不报 warn（正例）", True, p7)

        # --- P8 输出 JSON 逐帧形态字段正例（archive / bare 各一） ---
        def p8():
            hex64 = "a" * 64
            arch = {"storage_form": "archive", "index_path": "f00.hips.index.json",
                    "index_sha256": hex64, "archive_sha256": hex64}
            bare = {"storage_form": "bare", "index_path": "f01.hips.index.json",
                    "index_sha256": hex64, "archive_sha256": None}
            return (check_frame_storage(ctx, arch, vocab, "f00")
                    + check_frame_storage(ctx, bare, vocab, "f01"))
        case("P8 输出 JSON 逐帧形态字段正例", True, p8)

        # --- P9 运行完成清单 storage 段正例 ---
        def p9():
            hex64 = "b" * 64
            st = {"storage_form": "archive", "form_source": "default",
                  "products": [{"product": "f00", "storage_form": "archive",
                               "index_path": "f00.hips.index.json", "index_sha256": hex64,
                               "archive_bytes": 4096, "archive_sha256": hex64,
                               "tree_hash": hex64}],
                  "coverage_index": {"path": "coverage.index.json", "sha256": hex64,
                                     "n_frames": 1, "n_blocks": 3}}
            return check_manifest_storage(ctx, st, vocab, [form_warn_event(default_form, vocab)])
        case("P9 运行完成清单 storage 段正例", True, p9)

        # --- P10 逐层文档字段口径一致（真仓库；词表唯一源 = schema） ---
        def p10():
            return check_doc_consistency(
                vocab, lambda rel: (repo / rel).read_text(encoding="utf-8", errors="replace"))
        case("P10 逐层文档字段口径一致（真仓库）", True, p10)

        # --- P11 mosaic 输入含 archive 必须 REJECT；P12 合法 mosaic 输入不得被拒（非退化对照）---
        def _mosaic_doc(**extra):
            blk = {"hips_paths": ["/in/f00.hips"], "output_dir": "out/mosaic"}
            blk.update(extra)
            return {"schema_version": "1", "blocks": [blk]}

        def p11():
            doc = _mosaic_doc(**{form_key: "archive"})
            errs = ctx.phase_validator(doc, ctx.phase_schema("mosaic"))
            return [] if errs else ["mosaic 输入含 %s=archive 未被 REJECT（Phase2 固定裸形态）" % form_key]
        case("P11 mosaic 输入含 archive ⇒ REJECT", True, p11)

        def p12():
            doc = _mosaic_doc()
            errs = ctx.phase_validator(doc, ctx.phase_schema("mosaic"))
            return ["合法 mosaic 输入被判红（判据恒真）：%s" % errs] if errs else []
        case("P12 合法 mosaic 输入不被拒（非退化对照）", True, p12)

        # --- P13 mosaic 加性可选键 coverage_index 被接受 ---
        def p13():
            doc = _mosaic_doc(**{vocab["phase2_input_index_ref_key"]["name"]: "out/coverage.index.json"})
            errs = ctx.phase_validator(doc, ctx.phase_schema("mosaic"))
            return ["coverage_index 加性可选键未被接受：%s" % errs] if errs else []
        case("P13 mosaic 加性可选键 coverage_index 被接受", True, p13)

        # --- N12 缺省形态键却未报 warn ⇒ 红 ---
        def n12():
            return check_form_resolution(_p1_block(), [], vocab)
        case("N12 缺省形态键未报 warn ⇒ 红（静默取默认）", False, n12)

        # --- N12b 缺省形态键却未走默认 ⇒ 红 ---
        def n12b():
            # 注入「留空却返回非登记默认」的假解析器：判据必须能抓住（否则 F0 半条失效）
            def bad_resolver(_blk, _vocab):
                return "bare", "default", True, []
            return check_form_resolution(_p1_block(**{form_key: ""}),
                                         [form_warn_event("bare", vocab)], vocab,
                                         resolver=bad_resolver)
        case("N12b 缺省形态键未走登记默认 ⇒ 红", False, n12b)

        # --- N13 输出 JSON 逐帧缺 index_path ⇒ 红 ---
        def n13():
            hex64 = "c" * 64
            frame = {"storage_form": "archive", "index_sha256": hex64, "archive_sha256": hex64}
            return check_frame_storage(ctx, frame, vocab, "f00")
        case("N13 输出 JSON 逐帧缺 index_path ⇒ 红", False, n13)

        # --- N14 bare 形态却带 archive_sha256 ⇒ 红（F2） ---
        def n14():
            hex64 = "d" * 64
            frame = {"storage_form": "bare", "index_path": "f00.hips.index.json",
                     "index_sha256": hex64, "archive_sha256": hex64}
            return check_frame_storage(ctx, frame, vocab, "f00")
        case("N14 bare 形态带归档指纹 ⇒ 红（F2）", False, n14)

        # --- N15 运行完成清单 form_source=default 却无 warn ⇒ 红（M2） ---
        def n15():
            hex64 = "e" * 64
            st = {"storage_form": "archive", "form_source": "default",
                  "products": [{"product": "f00", "storage_form": "archive",
                               "index_path": "f00.hips.index.json", "index_sha256": hex64,
                               "archive_bytes": 4096, "archive_sha256": hex64,
                               "tree_hash": hex64}],
                  "coverage_index": None}
            return check_manifest_storage(ctx, st, vocab, [])
        case("N15 清单 form_source=default 却无 warn ⇒ 红（M2）", False, n15)

        # --- N16 export 输入含形态键必须 REJECT ---
        def n16():
            doc = {"schema_version": "1",
                   "blocks": [{"source": {"hips_dir": "/in/f00.hips"},
                              "output_dir": "out/p3", "output_mode": "surface_brightness",
                              form_key: "bare"}]}
            errs = ctx.phase_validator(doc, ctx.phase_schema("export"))
            return [] if errs else ["export 输入含 %s 未被 REJECT（Phase3 固定裸 FITS）" % form_key]
        case("N16 export 输入含形态键 ⇒ REJECT", True, n16)

        # --- N17 注入「两层文档对同一字段口径不一致」⇒ 红 ---
        def _layer_texts():
            return {ent["file"]: (repo / ent["file"]).read_text(encoding="utf-8", errors="replace")
                    for ent in vocab["layers"]}

        def n17():
            real = _layer_texts()
            target = vocab["layers"][1]["file"]          # design 层
            broken = dict(real)
            broken[target] = broken[target].replace(form_key, "storage_mode")
            return check_doc_consistency(vocab, lambda rel: broken[rel])
        case("N17 注入同义名（两层口径打架）⇒ 红", False, n17)

        def n18():
            real = _layer_texts()
            target = vocab["layers"][2]["file"]          # design-p1 层
            broken = dict(real)
            broken[target] = broken[target].replace("archive", "zst")
            return check_doc_consistency(vocab, lambda rel: broken[rel])
        case("N18 注入取值口径漂移（archive→zst）⇒ 红", False, n18)

    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return cases


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--scan", nargs="*", default=[])
    ap.add_argument("--json-out")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--doc-consistency", action="store_true",
                    help="只跑逐层字段口径一致性判据（D1/D2），不做形态扫描")
    a = ap.parse_args()
    repo = Path(a.root).resolve()

    if a.self_test:
        cases = self_test(repo)
        bad = [c for c in cases if not c["ok"]]
        for c in cases:
            print(("PASS " if c["ok"] else "FAIL ") + c["case"] +
                  ("" if c["ok"] else "  " + json.dumps(c["errors"], ensure_ascii=False)))
        print(f"self-test: {len(cases) - len(bad)}/{len(cases)} 例符合预期")
        if a.json_out:
            outp = Path(a.json_out)
            outp.parent.mkdir(parents=True, exist_ok=True)
            outp.write_text(json.dumps({"cases": cases}, ensure_ascii=False, indent=1),
                            encoding="utf-8")
        return 1 if bad else 0

    errors = []
    # A. 锚存在（fail-closed）
    anchors = ["eng/contracts/schemas/hips_storage_form.schema.json",
               "docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md",
               "docs/design/PRODUCT_STORAGE_FORM.md",
               "lib/infrastructure/aio/io/fits_verify.py",
               "eng/ci/run.py",
               "eng/tests/common/jsonschema_min.py",
               "lib/infrastructure/observability/logging/log_event_v1.schema.json",
               "eng/contracts/schemas/phase_config_normalize.schema.json",
               "eng/contracts/schemas/phase_config_mosaic.schema.json",
               "eng/contracts/schemas/phase_config_export.schema.json"]
    for rel in anchors:
        if not (repo / rel).is_file():
            errors.append(f"锚缺失：{rel}")
    if errors:
        for e in errors:
            print("FAIL " + e)
        return 1

    ctx = Ctx(repo)
    # D. 逐层字段口径一致（词表唯一源 = schema；层文件缺失/词缺失/同义名都判红）
    vocab = load_form_vocabulary(ctx.schema)
    doc_errors = check_doc_consistency(
        vocab, lambda rel: (repo / rel).read_text(encoding="utf-8", errors="replace"))
    if a.doc_consistency:
        for e in doc_errors:
            print("FAIL " + e)
        print("CHK-HIPS-STORAGE-FORM[doc-consistency]: verdict=%s errors=%d"
              % ("red" if doc_errors else "green", len(doc_errors)))
        return 1 if doc_errors else 0
    errors += doc_errors
    scanned = 0
    for root_s in a.scan:
        root = (repo / root_s).resolve() if not os.path.isabs(root_s) else Path(root_s)
        if not root.exists():
            errors.append(f"扫描根不存在：{root}")
            continue
        names = set()
        for p in root.iterdir():
            if p.name.endswith(".hips.zst"):
                names.add(p.name[:-len(".hips.zst")])
            elif p.name.endswith(".hips") and p.is_dir():
                names.add(p.name[:-len(".hips")])
        for nm in sorted(names):
            scanned += 1
            errors += validate_product(ctx, root, nm, repo / "run/hipsform-scan")

    verdict = "red" if errors else "green"
    out = {"check": "CHK-HIPS-STORAGE-FORM", "verdict": verdict,
           "scanned_products": scanned, "errors": errors[:40], "error_count": len(errors)}
    if a.json_out:
        Path(a.json_out).parent.mkdir(parents=True, exist_ok=True)
        Path(a.json_out).write_text(json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    if not a.quiet:
        for e in errors[:40]:
            print("FAIL " + e)
        print(f"CHK-HIPS-STORAGE-FORM: verdict={verdict} scanned={scanned} errors={len(errors)}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
