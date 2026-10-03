#!/usr/bin/env python3
# GOVERN-08 R2 分母/片划分实测器
# 只读脚本：统计 git ls-files 主体文件的份数与行数，输出 TSV。
# 用法: python3 measure.py <out.tsv>
import subprocess, sys, os

REPO = "/workspace/Astro CS Database"
DIRS = ["docs", "实验", "lib", "eng"]


def sh(args, cwd=REPO):
    return subprocess.run(args, cwd=cwd, capture_output=True, check=True).stdout


def tracked_files():
    out = sh(["git", "-c", "core.quotepath=false", "ls-files", "-z", *DIRS])
    return [p for p in out.split(b"\0") if p]


def count_lines(path):
    """行数 = 换行符个数 + (末字节非换行则 +1)。二进制按 \n 计数并单独标注。"""
    n = 0
    with open(path, "rb") as f:
        last = b""
        while True:
            chunk = f.read(1 << 20)
            if not chunk:
                break
            n += chunk.count(b"\n")
            last = chunk[-1:]
    if last and last != b"\n":
        n += 1
    return n


def is_binary(path):
    with open(path, "rb") as f:
        head = f.read(8192)
    return b"\0" in head


def main():
    out_path = sys.argv[1]
    files = tracked_files()
    rows = []
    for rel in files:
        ap = os.path.join(REPO, rel.decode("utf-8"))
        if not os.path.exists(ap):
            rows.append((rel.decode("utf-8"), -1, 1, "MISSING"))
            continue
        b = 1 if is_binary(ap) else 0
        rows.append((rel.decode("utf-8"), count_lines(ap), b, ""))
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("path\tlines\tbinary\tnote\n")
        for r in rows:
            f.write("%s\t%d\t%d\t%s\n" % r)
    print("files=%d" % len(rows))
    print("lines=%d" % sum(r[1] for r in rows if r[1] > 0))
    print("binary=%d" % sum(r[2] for r in rows))


if __name__ == "__main__":
    main()
