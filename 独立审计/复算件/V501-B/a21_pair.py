# A-21 复算：CTest 注册面配对（静态枚举 add_test vs 登记侧 glob / 命令侧正则）
# 只读被审仓库；不 import 仓库内任何 Python 模块。
import fnmatch
import json
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = "F:/Astro dev/Astro CS Normalization Database"


def git_lsfiles(prefix):
    out = subprocess.run(["git", "-C", REPO, "ls-files", prefix],
                         capture_output=True, text=True, encoding="utf-8").stdout
    return [l for l in out.splitlines() if l]


def read(p):
    with open(REPO + "/" + p, encoding="utf-8") as f:
        return f.read()


# 1) 静态枚举面：add_test(NAME ...)
names = []          # (file, line_no, name)
bare = []           # add_test 出现但我的正则没抓到 -> 需要人工看
for f in git_lsfiles("eng/tests"):
    if not f.endswith("CMakeLists.txt"):
        continue
    txt = read(f)
    for i, line in enumerate(txt.splitlines(), 1):
        for m in re.finditer(r"add_test\s*\(\s*NAME\s+(\S+)", line):
            names.append((f, i, m.group(1)))
        if re.search(r"\badd_test\s*\(", line) and not re.search(r"add_test\s*\(\s*NAME\s+\S+", line):
            bare.append((f, i, line.strip()[:120]))
genex = [(f, i, n) for (f, i, n) in names if "$" in n]
print("add_test NAME 捕获数 =", len(names))
print("distinct 名 =", len({n for _, _, n in names}))
print("含生成器表达式($)的名 =", len(genex), genex[:5])
print("add_test( 出现但正则未捕获 =", len(bare), bare[:6])

# 2) 登记面 A：checks.json 的 ctest_targets
reg = json.loads(read("eng/ci/checks.json"))
patterns = []


def walk(o):
    if isinstance(o, dict):
        for k, v in o.items():
            if k == "ctest_targets" and isinstance(v, list):
                for x in v:
                    if isinstance(x, str):
                        patterns.append(x)
            else:
                walk(v)
    elif isinstance(o, list):
        for x in o:
            walk(x)


walk(reg)
print("checks.json ctest_targets 模式总条数 =", len(patterns), "distinct =", len(set(patterns)))

# 3) 登记面 B：ctest_baseline.json targets
base = json.loads(read("eng/ci/ctest_baseline.json"))
bt = base.get("targets") if isinstance(base, dict) else None
print("ctest_baseline targets 条数 =", (len(bt) if isinstance(bt, list) else "N/A"),
      "顶层键 =", list(base.keys())[:8] if isinstance(base, dict) else type(base))
union = set(patterns) | set(bt if isinstance(bt, list) else [])

# 4) fnmatch 并集覆盖
matched = [x for x in names if any(fnmatch.fnmatchcase(x[2], p) for p in union)]
unmatched = [x for x in names if not any(fnmatch.fnmatchcase(x[2], p) for p in union)]
print("fnmatch 并集覆盖 =", len(matched), "/", len(names))
for u in unmatched:
    print("  未覆盖 ->", u[0] + ":" + str(u[1]), u[2])

# 5) 点名 drizzle 一例的两种方言
probe = "drizzle_pf_sb_v6_area_invalid"
print("drizzle_pf_sb_* 对该名的 fnmatch =", any(fnmatch.fnmatchcase(probe, p) for p in patterns))
cmdpats = sorted({p for p in patterns if p.startswith("drizzle_pf_sb")})
print("登记侧 drizzle_pf_sb* 模式 =", cmdpats)
# 命令侧：checks.json 里 --target 后面的表达式
txt = read("eng/ci/checks.json")
tt = re.findall(r'"--target",\s*"([^"]+)"', txt)
print("命令侧 --target 表达式（distinct）=", sorted(set(tt))[:20], "总条数 =", len(tt))
for e in sorted(set(tt)):
    if "drizzle" in e:
        rx = re.compile("^%s$" % e)
        hit = [n for _, _, n in names if rx.match(n)]
        print("  正则 %r 展开命中 add_test 名 = %d 条" % (e, len(hit)), hit[:6])
        gl = [n for _, _, n in names if fnmatch.fnmatchcase(n, e)]
        print("  同一串按 glob 解释命中 = %d 条" % len(gl))

# 6) 陈旧登记（C4 侧）：登记模式匹配不到任何现存目标
all_targets = [n for _, _, n in names]
base_set = set(bt if isinstance(bt, list) else [])
dead_vs_addtest = sorted({p for p in patterns if any(c in p for c in "*?[")
                          and not any(fnmatch.fnmatchcase(n, p) for n in all_targets)})
print("含 glob 字符且匹配不到任何 add_test 的登记模式 =", len(dead_vs_addtest), dead_vs_addtest)
for p in dead_vs_addtest:
    print("   ", repr(p), "是否被 baseline 命中 =",
          any(fnmatch.fnmatchcase(b, p) for b in base_set))
# 6b) baseline 目标与 add_test 名的差（C5 侧静态近似）
only_base = sorted({b for b in base_set if b not in set(all_targets)})
print("baseline 里在 add_test 面找不到的名 =", len(only_base), only_base[:12])
# 6c) 多行 add_test（NAME 在行尾、名在下一行）
multi = []
for f in git_lsfiles("eng/tests"):
    if not f.endswith("CMakeLists.txt"):
        continue
    ls = read(f).splitlines()
    for i, line in enumerate(ls):
        if re.search(r"add_test\s*\(\s*NAME\s*$", line):
            multi.append((f, i + 1, ls[i + 1].strip()[:60] if i + 1 < len(ls) else ""))
print("NAME 后换行的 add_test =", len(multi), multi[:5])
# 6d) 全仓 add_test 面（不止 eng/tests）
for f in git_lsfiles("."):
    if (f.endswith("CMakeLists.txt") or f.endswith(".cmake")) and not f.startswith("eng/tests"):
        c = len(re.findall(r"add_test\s*\(\s*NAME\s+\S+", read(f)))
        if c:
            print("eng/tests 之外的 add_test:", f, c)
