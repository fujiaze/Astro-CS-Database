"""把独立审计件复制进仓库根 `独立审计/`，并改名（整改 → 独立审计）+ 修内部交叉引用。
只新增文件与两处登记改动，不改动任何既有仓库内容。"""
import sys, os, io, re, shutil, glob
sys.stdout.reconfigure(encoding='utf-8')

SRC_ROOT = r"产出/"
REPO = r"F:\Astro dev\Astro CS Normalization Database"
DST = os.path.join(REPO, "独立审计")
PKG = os.path.join(SRC_ROOT, "独立审计包")

EXCLUDE_DIRS = {"tmp", "dispatch", "scan402", "d1merge"}
# 复算件只带被成稿引用的脚本，不带数据块（merged.json / *.tsv / *.csv 等）
KEEP_COMPUTATION_EXT = {".py", ".md"}


def walk_files(base):
    out = []
    for root, dirs, files in os.walk(base):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
        for fn in files:
            out.append(os.path.join(root, fn))
    return out


copied = 0
bytes_copied = 0


def copy_tree(src, dst, filt=None):
    global copied, bytes_copied
    n = 0
    for f in walk_files(src):
        rel = os.path.relpath(f, src)
        if filt and not filt(f, rel):
            continue
        d = os.path.join(dst, os.path.dirname(rel))
        os.makedirs(d, exist_ok=True)
        t = os.path.join(dst, rel)
        if os.path.exists(t):
            continue
        shutil.copy2(f, t)
        copied += 1
        bytes_copied += os.path.getsize(f)
        n += 1
    return n


def keep_computation(f, rel):
    ext = os.path.splitext(f)[1].lower()
    return ext in KEEP_COMPUTATION_EXT


n1 = copy_tree(PKG, os.path.join(DST))                                   # 交付包
n2 = copy_tree(os.path.join(SRC_ROOT, "raw"), os.path.join(DST, "证据"),
               lambda f, r: f.endswith(".md") and not os.path.basename(f).startswith("scan"))
n3 = copy_tree(os.path.join(SRC_ROOT, "inventory"), os.path.join(DST, "批次清单"),
               lambda f, r: f.endswith(".txt"))
n4 = copy_tree(os.path.join(SRC_ROOT, "复算"), os.path.join(DST, "复算件"), keep_computation)
for extra in ("DISPATCH-CODE-READ-PREAMBLE.md", "DISPATCH-CIT-PREAMBLE.md", "DISPATCH-VERIFY-PREAMBLE.md"):
    s = os.path.join(SRC_ROOT, extra)
    if os.path.exists(s):
        shutil.copy2(s, os.path.join(DST, "派单规程", extra))
        copied += 1

print("copied files:", copied, "MB:", round(bytes_copied / 1048576, 2))
print("  交付包", n1, "| 证据成稿", n2, "| 批次清单", n3, "| 复算件", n4)
