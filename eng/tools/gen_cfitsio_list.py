#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BLD-001: 生成显式 cfitsio 源清单(替代 file(GLOB))。

证据源（唯一，禁止回退）：
  lib/infrastructure/aio/third_party/cfitsio/*.c
  —— ARCH-001 迁移后路径。迁移前路径 lib/astro_image_io/third_party/cfitsio 已消失，
  本生成器**不**把它当回退：回退会让「证据源被搬走」伪装成「生成成功」。
产物：eng/cmake/cfitsio_sources.cmake（被 CMakeLists.txt:176 include，消费面唯一来源）。

判据（fail-closed）：
  G1 证据源目录缺失 / 不可列目录 → exit 2，**不写盘**（在册清单保持原样）；
  G2 排除表外命中数为 0 → exit 2（「零命中」≠「清单本来就该是空的」）；
  G3 只有 G1/G2 通过才写盘；--check 只比对不写。

用法：
  python3 eng/tools/gen_cfitsio_list.py [--root <repo>] [--check] [--out <path>]
exit 0 = 生成成功 / 与在册清单逐字节一致；exit 1 = 与在册清单不一致；exit 2 = 输入不可用。
"""
from __future__ import annotations

import argparse
import pathlib
import sys

EXCL = ("f77_wrap", "drvrgsiftp", "drvrsmem", "smem", "vms", "windumpexts",
        "iter_a", "iter_b", "iter_c", "cookbook", "speed_test", "fpack",
        "funpack", "fitscopy", "listhead", "liststruc", "imcopy", "imarith",
        "tabcompile", "sortcol", "tabselect")
SRC_REL = "lib/infrastructure/aio/third_party/cfitsio"
LEGACY_SRC_REL = "lib/astro_image_io/third_party/cfitsio"   # 已消失；仅用于报错提示
OUT_REL = "eng/cmake/cfitsio_sources.cmake"
GENERATOR_REL = "eng/tools/gen_cfitsio_list.py"


def render(root: pathlib.Path) -> tuple[int, str, str]:
    """返回 (rc, 文本或空, 说明)。rc=2 表示输入不可用（调用方必须不写盘）。"""
    src = root / SRC_REL
    if not src.is_dir():
        hint = ""
        if (root / LEGACY_SRC_REL).exists():
            hint = "（旧路径 %s 存在 ⇒ 证据源又搬回去了，请核对 ARCH-001 迁移事实）" % LEGACY_SRC_REL
        return (2, "", "CFITSIO_LIST_INPUT_MISSING: 证据源目录不存在: %s%s" % (SRC_REL, hint))
    try:
        files = sorted(p.name for p in src.glob("*.c"))
    except OSError as exc:
        return (2, "", "CFITSIO_LIST_INPUT_UNREADABLE: %s: %s" % (SRC_REL, exc))
    srcs = [n for n in files if not any(e in n for e in EXCL)]
    if not srcs:
        return (2, "", "CFITSIO_LIST_ZERO_SOURCES: 目录存在但排除表外命中 0 个 .c"
                       "（扫到 %d 个 .c，全部命中排除表）—— 零命中不是空清单的理由"
                       % len(files))
    out = ["# 生成: %s (BLD-001 禁止 production GLOB)" % GENERATOR_REL,
           "# 证据源: %s/*.c（命中 %d）" % (SRC_REL, len(srcs)),
           "set(ASTROCS_CFITSIO_SOURCES"]
    out += ["  %s/%s" % (SRC_REL, s) for s in srcs]
    out.append(")")
    return (0, "\n".join(out) + "\n", "sources=%d" % len(srcs))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[2]))
    ap.add_argument("--check", action="store_true", help="只比对在册清单，不写盘")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    root = pathlib.Path(args.root).resolve()
    out_path = pathlib.Path(args.out) if args.out else (root / OUT_REL)

    rc, text, note = render(root)
    if rc != 0:
        print(note, file=sys.stderr)
        print("CFITSIO_LIST_FAIL: 未写盘（%s 保持原样）" % OUT_REL, file=sys.stderr)
        return rc

    if args.check:
        if not out_path.is_file():
            print("CFITSIO_LIST_INPUT_MISSING: 在册清单不存在: %s" % out_path, file=sys.stderr)
            return 2
        on_disk = out_path.read_text(encoding="utf-8")
        if on_disk != text:
            print("CFITSIO_LIST_DRIFT: %s 与证据源不一致（重跑本生成器）" % OUT_REL,
                  file=sys.stderr)
            return 1
        print("CFITSIO_LIST_CHECK_OK %s" % note)
        return 0

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(text, encoding="utf-8")
    print("CFITSIO_LIST_WRITTEN %s -> %s" % (note, OUT_REL))
    return 0


if __name__ == "__main__":
    sys.exit(main())
