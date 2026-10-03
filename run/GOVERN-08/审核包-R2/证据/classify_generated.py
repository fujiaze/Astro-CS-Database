#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 R2 · 判定式 GEN-1「生成物」分类器
==========================================
只读。不编译、不跑测试、不跑实验、不写 git。

用法:
    python3 classify_generated.py <主体行数.tsv> <输出判定表.tsv> <输出摘要.txt>

判定式 GEN-1（对受审主体 S = docs ∪ 实验 ∪ lib ∪ eng 的每一份受跟踪文件 f）：

    f ∈ 生成物   ⟺   G1(f) ∨ G2(f) ∨ G3(f)

  G1 运行事实自述：f 全文含墙钟键 elapsed_s|runtime_s|wall_s|generated_at|written_utc
      仓内依据 = 实验/README.md:8 ——「墙钟类字段（elapsed_s / runtime_s / wall_s /
      generated_at / written_utc）是运行时事实，不参与可复现判定」。自述运行时事实的
      文件按定义不是人工撰写件。
  G2 派生自述：f 全文含派生标记键 transcription|derived_from|generated_by|
      generated_from|produced_by，或含文本生成标记 @generated / DO NOT EDIT /
      自动生成 / 本文件由…生成。
  G3 产出面：f 路径命中下列声明式产出面模式（每条附仓内依据，见摘要输出）。
        -f1  实验/**/results/**          实验/README.md:3,8 具名 results/ 为结果与判据表面
        -f2  实验/engineering-evidence/**  实验/README.md:7 具名为「时点证据快照，历史记录不改写」
        -f3  eng/tests/validation/**/data/**  验证输入数据集面

  G4 生成器指名：f 的 basename 在主体内**另一份**脚本/构建件（.py/.sh/.cpp/.c/.cmake/
      CMakeLists.txt/.m4/.y）中被逐字提及。仓内程序能指名该文件名 ⇒ 它是某个仓内程序
      的文件级产物或消费件，不是人手写下的正本。命中时记录指名者路径以便人工审计。

  G5 第三方内联（单列，不并入生成物）：路径含 /third_party/。
      这是「上游内联」而非「本仓生成」，性质不同，故单列并给双口径分母。

每份文件都输出命中信号与指名者，便于逐份审计。
"""
import re
import sys
import fnmatch
import collections

# ---- G1 墙钟键（依据：实验/README.md:8）----
G1_RE = re.compile(r'"(?:elapsed_s|runtime_s|wall_s|generated_at|written_utc)"')

# ---- G2 派生标记键 / 文本生成标记 ----
G2_KEY_RE = re.compile(
    r'"(?:transcription|derived_from|generated_by|generated_from|produced_by)"\s*:')
G2_TXT_RE = re.compile(
    r'@generated|DO NOT EDIT|自动生成|自动產生|本文件由\s*\S+\s*(?:脚本|生成器|程序)\s*生成',
    re.IGNORECASE)

# ---- G3 产出面模式（声明式，逐条附依据）----
G3_GLOBS = [
    ("f1", "实验/*/results/*",                       "实验/README.md:3,8 —— results/ 具名为「结果与判据表」面"),
    ("f2", "实验/*/*/results/*",                     "实验/README.md:3,8（嵌套层）"),
    ("f3", "实验/engineering-evidence/*",            "实验/README.md:7 —— 「工程实测类证据留档……时点证据快照，历史记录不改写」"),
    ("f4", "eng/tests/validation/*/*/data/*",        "验证输入数据集面（eng/tests/validation 下的 data/）"),
    ("f5", "eng/tests/validation/*/data/*",          "验证输入数据集面（二级）"),
]

# ---- G4 生成器指名 ----
SCRIPT_EXT = (".py", ".sh", ".cpp", ".c", ".h", ".hpp", ".cmake", ".m4", ".y", ".bash")

# ---- G5 第三方内联 ----
G5_TOKEN = "/third_party/"


def build_name_index(paths):
    """basename -> 提及它的脚本路径列表（排除同名自身）。"""
    idx = collections.defaultdict(list)
    for p in paths:
        if p.endswith(SCRIPT_EXT) or p.endswith("CMakeLists.txt"):
            try:
                with open(p, "rb") as f:
                    txt = f.read().decode("utf-8", errors="replace")
            except OSError:
                continue
            for b in set(re.findall(r'[\w./-]+\.[A-Za-z][A-Za-z0-9]{0,5}', txt)):
                idx[b].append(p)
    return idx


def match_g3(path):
    for tag, glob, why in G3_GLOBS:
        if fnmatch.fnmatch(path, glob):
            return tag, why
    return None, None


def read_text(path, limit=None):
    try:
        with open(path, "rb") as f:
            data = f.read() if limit is None else f.read(limit)
        return data.decode("utf-8", errors="replace")
    except OSError:
        return ""


def main():
    tsv_in, tsv_out, sum_out = sys.argv[1], sys.argv[2], sys.argv[3]

    rows = []
    with open(tsv_in, encoding="utf-8") as f:
        next(f)
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            p, n, b, note = line.split("\t")
            rows.append((p, int(n), int(b)))

    recs = []
    sig_count = collections.Counter()
    nameidx = build_name_index([r[0] for r in rows])
    for p, n, b in rows:
        txt = read_text(p)
        g1 = bool(G1_RE.search(txt))
        g2 = bool(G2_KEY_RE.search(txt)) or bool(G2_TXT_RE.search(txt))
        g3tag, g3why = match_g3(p)
        base = p.rsplit("/", 1)[-1]
        namers = [q for q in nameidx.get(base, []) if q != p]
        g4 = bool(namers)
        g5 = G5_TOKEN in p
        sigs = []
        if g1:
            sigs.append("G1")
        if g2:
            sigs.append("G2")
        if g3tag:
            sigs.append("G3:" + g3tag)
        if g4:
            sigs.append("G4")
        if g5:
            sigs.append("G5")
        is_gen = g1 or g2 or bool(g3tag) or g4
        for s in sigs:
            sig_count[s] += 1
        recs.append((p, n, b, int(is_gen), int(g5), "+".join(sigs) or "-",
                     g3why or "", ";".join(namers[:3])))

    with open(tsv_out, "w", encoding="utf-8") as f:
        f.write("path\tlines\tbinary\tgenerated\tthird_party\tsignals\tg3_basis\tnamers\n")
        for r in recs:
            f.write("%s\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n" % r)

    gen = [r for r in recs if r[3]]
    tp = [r for r in recs if r[4]]
    gen_no_tp = [r for r in gen if not r[4]]
    art = [r for r in recs if not r[3] and not r[4]]

    L = []
    A = L.append
    A("=== GEN-1 判定汇总 ===")
    A(f"受审主体           : {len(recs)} 份 / {sum(r[1] for r in recs)} 行")
    A(f"生成物 (G1∨G2∨G3∨G4): {len(gen)} 份 / {sum(r[1] for r in gen)} 行")
    A(f"  其中不含第三方     : {len(gen_no_tp)} 份 / {sum(r[1] for r in gen_no_tp)} 行")
    A(f"第三方内联 (G5)    : {len(tp)} 份 / {sum(r[1] for r in tp)} 行")
    A(f"人工产物           : {len(art)} 份 / {sum(r[1] for r in art)} 行")
    A("")
    A("--- 信号命中计数（可重叠）---")
    for s, c in sorted(sig_count.items(), key=lambda x: -x[1]):
        A(f"  {s:10s} {c:5d} 份")
    A("")
    A("--- 唯一信号归因（生成物，排除第三方）---")
    only = collections.Counter()
    onlyl = collections.Counter()
    for r in gen_no_tp:
        only[r[5]] += 1
        onlyl[r[5]] += r[1]
    for s, c in only.most_common():
        A(f"  {s:28s} {c:5d} 份 {onlyl[s]:9d} 行")
    A("")
    A("--- G3 逐模式命中 ---")
    for tag, glob, why in G3_GLOBS:
        hit = [r for r in recs if r[5].find("G3:" + tag) >= 0]
        A(f"  {tag} {glob:42s} n={len(hit):4d} lines={sum(x[1] for x in hit):8d}  ← {why}")
    A("")
    A("--- 仅由 G4（生成器指名）单独判定的文件 —— 误报审计清单 ---")
    g4only = [r for r in gen_no_tp if r[5] == "G4"]
    A(f"  合计 {len(g4only)} 份 / {sum(r[1] for r in g4only)} 行")
    for r in sorted(g4only, key=lambda x: -x[1])[:40]:
        A(f"    {r[1]:7d}  {r[0]}")
        A(f"            指名者: {r[7][:150]}")
    A("")
    A("--- 按顶层目录：人工产物 ---")
    d = collections.Counter()
    dl = collections.Counter()
    for p, n, b, g, t, s, w, nm in art:
        top = p.split("/")[0]
        d[top] += 1
        dl[top] += n
    for k in sorted(dl, key=lambda x: -dl[x]):
        A(f"  {k:8s} {d[k]:5d} 份 {dl[k]:9d} 行")
    A("")
    A("--- 按顶层目录：生成物 ---")
    d2 = collections.Counter()
    dl2 = collections.Counter()
    for p, n, b, g, t, s, w, nm in gen:
        top = p.split("/")[0]
        d2[top] += 1
        dl2[top] += n
    for k in sorted(dl2, key=lambda x: -dl2[x]):
        A(f"  {k:8s} {d2[k]:5d} 份 {dl2[k]:9d} 行")
    A("")
    A("--- 人工产物 Top 40 行数（人工审计抽查面）---")
    for p, n in sorted(((r[0], r[1]) for r in art), key=lambda x: -x[1])[:40]:
        A(f"  {n:7d}  {p}")

    out = "\n".join(L)
    with open(sum_out, "w", encoding="utf-8") as f:
        f.write(out + "\n")
    print(out)


if __name__ == "__main__":
    main()
