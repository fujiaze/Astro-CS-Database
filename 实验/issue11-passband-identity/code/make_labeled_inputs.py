#!/usr/bin/env python3
"""Issue #11 —— 构造"受控 P2 正向输入"：真实 Phase1 HiPS 产品的硬链接 overlay，
只把 `signal/properties` 的 `obs_filter` 名义标签改成指定值。

为什么是"受控"而不是"伪造上游"：
  · 科学数据（signal/support/variance/ivar tile、Moc、p1_stack.json 几何）**逐字节
    来自真实 Phase1 产品**（硬链接，不复制不修改）；
  · 唯一改动是 `signal/properties` 里**被测量本身**——名义通带标签 `obs_filter`
    （coverage 的 `inspect_frame` 只读这一处决定 `filter_passband`）；
  · 这正是 #11 的对象（名义标签≠响应曲线），不是为让下游变绿而编造 integrated 产物。

用法：
  python3 make_labeled_inputs.py --src <real_frame_dir> --dst <overlay_dir> \
      [--label ""] [--drop-key]
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--dst", required=True)
    ap.add_argument("--label", default=None, help="obs_filter value (empty string allowed)")
    ap.add_argument("--drop-key", action="store_true",
                    help="删除 obs_filter 行（负例：缺少 key，应 fail-closed）")
    a = ap.parse_args()
    src = os.path.abspath(a.src)
    dst = os.path.abspath(a.dst)
    if not os.path.isdir(os.path.join(src, "signal")):
        print(f"FATAL: {src} has no signal/ (not a HiPS product)")
        return 2
    if os.path.exists(dst):
        shutil.rmtree(dst)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    # 硬链接整树（同设备；不复制数据、不消耗额外空间）
    r = subprocess.run(["cp", "-al", src, dst], capture_output=True, text=True)
    if r.returncode != 0:
        print("FATAL: cp -al failed:", r.stderr)
        return 1
    prop = os.path.join(dst, "signal", "properties")
    # 断开硬链接后写新文件（绝不原地改写共享 inode）
    lines = open(prop, encoding="utf-8").read().splitlines()
    have = any(l.startswith("obs_filter=") for l in lines)
    out = []
    for l in lines:
        if l.startswith("obs_filter="):
            if a.drop_key:
                continue  # 整行删除 ⇒ 缺少 key
            out.append("obs_filter=" + (a.label if a.label is not None else ""))
        else:
            out.append(l)
    if not have and not a.drop_key:
        out.append("obs_filter=" + (a.label if a.label is not None else ""))
    os.remove(prop)
    with open(prop, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print(f"ok dst={dst} label={a.label!r} drop_key={a.drop_key}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
