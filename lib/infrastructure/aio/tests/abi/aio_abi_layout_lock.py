#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""aio_abi_layout_lock.py — AIO HiPS 跨边界结构布局一致性机器锁 (V11-N-01)

ASTROCS_DESIGN §7.3 / ENGINEERING_SPEC §1: 跨 DLL 边界使用版本化 C ABI;
结构体带 struct_size/abi_version。本锁把"公共 C 头定义"与"Python ctypes
镜像"钉死:

  1) 运行 C 探针 aio_abi_layout_probe (只 include aio_hips.h, Linux 可编译),
     读取其 sizeof / alignof / 逐字段 offsetof+sizeof 的 JSON;
  2) 与权威镜像 lib/infrastructure/aio/tools/aio_abi_mirror.py 逐字段比对;
  3) 字段名、字段顺序、字段 offset、字段 size、结构体 sizeof、alignof、
     abi_version 常量、*_STRUCT_SIZE 常量任一不一致 => 退出码 1 (ctest FAIL)。

字段重排/增删/改类型都会必然失败, 因此 V11-N-01 型"头改了、镜像没改"
(C=40B vs 镜像=32B 静默错位) 不可能再悄悄通过。

用法:
  aio_abi_layout_lock.py --probe <探针可执行文件> [--mirror-file <镜像.py>]
  aio_abi_layout_lock.py --probe <...> --selfcheck    # 反向证明锁非恒真

--selfcheck: 用探针数据构造一个被故意破坏 (字段重排 + 漏字段 + 常量污染)
的镜像, 断言锁必然报错。若锁对被破坏镜像仍然"通过", 说明门失效 (非零退出)。
"""

import argparse
import ctypes
import importlib.util
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# HERE = lib/infrastructure/aio/tests/abi -> ../../tools = lib/infrastructure/aio/tools
DEFAULT_MIRROR = os.path.normpath(
    os.path.join(HERE, "..", "..", "tools", "aio_abi_mirror.py"))

ABI_CONST = {
    "AstroSphereTileView": "AIO_HIPS_TILE_VIEW_ABI_VERSION",
    "AioHipsSnrPoint": "AIO_HIPS_SNR_POINT_ABI_VERSION",
    "AioHipsDiagTileView": "AIO_HIPS_DIAG_TILE_VIEW_ABI_VERSION",
    "AioHipsTile": "AIO_HIPS_TILE_ABI_VERSION",
}
SIZE_CONST = {
    "AstroSphereTileView": "AIO_HIPS_TILE_VIEW_STRUCT_SIZE",
    "AioHipsSnrPoint": "AIO_HIPS_SNR_POINT_STRUCT_SIZE",
    "AioHipsDiagTileView": "AIO_HIPS_DIAG_TILE_VIEW_STRUCT_SIZE",
    "AioHipsTile": "AIO_HIPS_TILE_STRUCT_SIZE",
}


def load_module(path, name="aio_abi_mirror_under_test"):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("无法加载镜像模块: %s" % path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run_probe(probe_path):
    proc = subprocess.run([probe_path], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=120)
    if proc.returncode != 0:
        raise RuntimeError("C 探针退出码 %d; stderr:\n%s"
                           % (proc.returncode, proc.stderr.decode("utf-8", "replace")))
    return json.loads(proc.stdout.decode("utf-8"))


def _ctype_for_size(size):
    return {1: ctypes.c_char, 2: ctypes.c_uint16, 4: ctypes.c_uint32,
            8: ctypes.c_double}.get(size)


def make_mutated_mirror(probe_data):
    """用探针真实布局构造一个"漏同步"的假镜像:
    AioHipsSnrPoint 的 ra_deg/dec_deg 顺序对调 + 删掉 snr 字段,
    并污染 ABI/尺寸常量。用于证明锁的报警不是恒真。"""
    import types
    c_fields = probe_data["AioHipsSnrPoint"]["fields"]
    cls = type("MutatedAioHipsSnrPoint", (ctypes.Structure,), {})
    fields = []
    for f in c_fields:
        if f["name"] == "snr":
            continue  # 故意漏字段
        if f["name"] == "ra_deg":
            fields.append(("dec_deg", ctypes.c_double))   # 故意重排
            continue
        if f["name"] == "dec_deg":
            fields.append(("ra_deg", ctypes.c_double))
            continue
        fields.append((f["name"], _ctype_for_size(f["size"])))
    cls._fields_ = fields
    ns = types.SimpleNamespace()
    ns.MIRRORED_STRUCTS = {"AioHipsSnrPoint": cls}
    for name, const in ABI_CONST.items():
        setattr(ns, const, 999)          # 版本也篡改
    for name, const in SIZE_CONST.items():
        setattr(ns, const, -1)           # 尺寸也篡改
    return ns


def compare_struct(name, probe_struct, mirror_cls):
    """返回 (errors:list[str], rows:list[tuple])。"""
    errors = []
    rows = []
    probe_fields = probe_struct["fields"]
    mirror_fields = list(mirror_cls._fields_)

    n = max(len(probe_fields), len(mirror_fields))
    for i in range(n):
        c = probe_fields[i] if i < len(probe_fields) else None
        p = mirror_fields[i] if i < len(mirror_fields) else None
        c_name = c["name"] if c else "<缺失>"
        p_name = p[0] if p else "<缺失>"
        c_off = c["offset"] if c else None
        p_off = getattr(mirror_cls, p[0]).offset if p else None
        c_size = c["size"] if c else None
        p_size = getattr(mirror_cls, p[0]).size if p else None
        ok = (c is not None and p is not None and c_name == p_name
              and c_off == p_off and c_size == p_size)
        rows.append((i, c_name, c_off, c_size, p_name, p_off, p_size, ok))
        if not ok:
            errors.append("[%s#%d] C(name=%s,offset=%s,size=%s) != "
                          "Python(name=%s,offset=%s,size=%s)"
                          % (name, i, c_name, c_off, c_size, p_name, p_off, p_size))

    m_size = ctypes.sizeof(mirror_cls)
    m_align = ctypes.alignment(mirror_cls)
    if probe_struct["sizeof"] != m_size:
        errors.append("[%s] sizeof: C=%d Python=%d" % (name, probe_struct["sizeof"], m_size))
    if probe_struct.get("alignof") != m_align:
        errors.append("[%s] alignof: C=%s Python=%s"
                      % (name, probe_struct.get("alignof"), m_align))
    return errors, rows


def check_all(probe_data, mirror_ns):
    errors = []
    tables = []
    mirrored = getattr(mirror_ns, "MIRRORED_STRUCTS", {})

    for name, probe_struct in probe_data.items():
        if name not in mirrored:
            errors.append("镜像缺少结构体 %s (MIRRORED_STRUCTS)" % name)
            continue
        errs, rows = compare_struct(name, probe_struct, mirrored[name])
        errors.extend(errs)
        tables.append((name, probe_struct, rows))

    for name in mirrored:
        if name not in probe_data:
            errors.append("C 探针缺少镜像声明的结构体 %s" % name)

    # ABI 头部契约 (ASTROCS_DESIGN 7.3) + 版本/尺寸常量一致性
    for name, probe_struct in probe_data.items():
        pf = probe_struct["fields"]
        if not pf or pf[0]["name"] != "struct_size" or pf[0]["offset"] != 0:
            errors.append("ASTROCS_DESIGN 7.3: %s 首字段必须是 struct_size@0" % name)
        if len(pf) < 2 or pf[1]["name"] != "abi_version" or pf[1]["offset"] != 4:
            errors.append("ASTROCS_DESIGN 7.3: %s 第二字段必须是 abi_version@4" % name)
        if pf and pf[0]["size"] != 4:
            errors.append("%s.struct_size 必须为 4 字节 (uint32_t)" % name)
        if len(pf) > 1 and pf[1]["size"] != 4:
            errors.append("%s.abi_version 必须为 4 字节 (uint32_t)" % name)
        abi_c = probe_struct.get("abi_version_value")
        abi_py = getattr(mirror_ns, ABI_CONST[name], None)
        if abi_c != abi_py:
            errors.append("%s: C=%s Python=%s" % (ABI_CONST[name], abi_c, abi_py))
        size_py = getattr(mirror_ns, SIZE_CONST[name], None)
        if size_py != probe_struct["sizeof"]:
            errors.append("%s: C=%d Python=%s"
                          % (SIZE_CONST[name], probe_struct["sizeof"], size_py))
    return errors, tables


def print_tables(tables):
    for name, probe_struct, rows in tables:
        print("== %s (C sizeof=%d alignof=%s) ==" % (name, probe_struct["sizeof"],
                                                     probe_struct.get("alignof")))
        print("  %-4s %-30s %-8s %-6s | %-30s %-8s %-6s %s"
              % ("#", "C field", "off", "size", "Python field", "off", "size", "OK"))
        for i, cn, co, cs, pn, po, ps, ok in rows:
            print("  %-4d %-30s %-8s %-6s | %-30s %-8s %-6s %s"
                  % (i, cn, co, cs, pn, po, ps, "OK" if ok else "MISMATCH"))


def main(argv=None):
    ap = argparse.ArgumentParser(description="AIO HiPS 跨边界结构布局一致性锁")
    ap.add_argument("--probe", default=os.environ.get("AIO_LAYOUT_PROBE"),
                    help="C 探针可执行文件路径 (或设 AIO_LAYOUT_PROBE)")
    ap.add_argument("--mirror-file", default=DEFAULT_MIRROR,
                    help="ctypes 镜像模块路径 (默认仓内权威镜像)")
    ap.add_argument("--selfcheck", action="store_true",
                    help="反向自检: 对被破坏镜像必须报错")
    args = ap.parse_args(argv)

    if not args.probe:
        sys.stderr.write("缺少 --probe (或环境变量 AIO_LAYOUT_PROBE)\n")
        return 2

    probe_data = run_probe(args.probe)

    if args.selfcheck:
        mirror_ns = make_mutated_mirror(probe_data)
        errors, _ = check_all(probe_data, mirror_ns)
        print("[selfcheck] 对被破坏镜像 (字段重排+漏字段+常量污染) 的报错数: %d"
              % len(errors))
        for e in errors[:8]:
            print("[selfcheck]   " + e)
        if errors:
            print("[selfcheck] PASS: 锁对被破坏镜像确实报警 (非恒真)")
            return 0
        print("[selfcheck] FAIL: 锁对被破坏镜像仍然通过 => 门失效")
        return 1

    mirror_ns = load_module(args.mirror_file)
    errors, tables = check_all(probe_data, mirror_ns)
    print_tables(tables)
    print("镜像: %s" % args.mirror_file)
    for name in sorted(probe_data):
        print("C %s: sizeof=%d abi_version=%s | Python %s=%s"
              % (name, probe_data[name]["sizeof"],
                 probe_data[name].get("abi_version_value"),
                 SIZE_CONST[name], getattr(mirror_ns, SIZE_CONST[name], None)))
    if errors:
        print("\nAIO_ABI_LAYOUT_LOCK FAIL (%d 处不一致):" % len(errors))
        for e in errors:
            print("  - " + e)
        return 1
    print("\nAIO_ABI_LAYOUT_LOCK PASS: C 与 ctypes 镜像逐字段一致 (含 ABI 头部契约)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
