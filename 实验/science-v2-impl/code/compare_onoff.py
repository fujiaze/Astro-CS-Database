#!/usr/bin/env python3
"""ON/OFF 科学等价逐位对照（science-v2-impl R0D 验收项 3）。

对照对象：同一输入、同一配置、同一机器，仅编译开关不同。
  - OFF = build/linux-sci/acsd   (ACSD_DEV_MODE=OFF)
  - ON  = build/linux-dev/acsd   (ACSD_DEV_MODE=ON)

比较面：
  1. 每帧 HiPS signal/ivar/variance tile 的**逐字节**内容（含文件名集合）；
  2. properties 文件（允许与诊断无关的差异单独列出）；
  3. 工作区科学中间产物（p1_snr.json / p1_phot.json / p1_sources.json）。
输出：JSON 摘要 + 人读报告。
"""
import hashlib
import json
import os
import sys

CHUNK = 1 << 20
ROOT = os.path.join(os.path.dirname(__file__), "..", "..")


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(CHUNK)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def tree(root):
    out = {}
    for dirpath, _dirs, files in os.walk(root):
        for n in sorted(files):
            p = os.path.join(dirpath, n)
            rel = os.path.relpath(p, root)
            out[rel] = p
    return out


def main():
    off_dir = sys.argv[1]
    on_dir = sys.argv[2]
    report = {"schema": "acsd.science-v2.onoff-compare/v1", "off": off_dir, "on": on_dir}
    to = tree(off_dir)
    tn = tree(on_dir)
    only_off = sorted(set(to) - set(tn))
    only_on = sorted(set(tn) - set(to))
    both = sorted(set(to) & set(tn))
    report["only_in_off"] = only_off
    report["only_in_on"] = only_on
    identical, differing = [], []
    for rel in both:
        if sha(to[rel]) == sha(tn[rel]):
            identical.append(rel)
        else:
            differing.append(rel)
    report["n_common"] = len(both)
    report["n_identical"] = len(identical)
    report["n_differing"] = len(differing)
    report["differing_files"] = differing
    # 诊断隔离检查：ON 侧多出来的文件必须全部位于 _dev_diagnostics 下
    stray = [p for p in only_on if not p.startswith("_dev_diagnostics")]
    report["on_only_outside_dev_root"] = stray
    report["dev_root_isolated"] = (len(stray) == 0)
    out = os.path.join(os.path.dirname(__file__), "..", "run", "A",
                       "onoff_compare.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1, ensure_ascii=False)
    print("common=%d identical=%d differing=%d" % (len(both), len(identical), len(differing)))
    print("only_in_off=%d only_in_on=%d" % (len(only_off), len(only_on)))
    print("on_only_outside_dev_root=%s" % stray)
    if differing:
        print("differing samples:")
        for d in differing[:12]:
            print("   ", d)
    print("output:", out)


if __name__ == "__main__":
    sys.exit(main())
