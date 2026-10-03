#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 R2 · 判定式 GEN-2「生成物 / 人工产物」分类器
=====================================================
只读脚本。不编译、不跑测试、不跑实验、不写 git。

用法:
    python3 gen2_classify.py <主体行数.tsv> <逐份判定.tsv> <摘要.txt>

------------------------------------------------------------------
判定式 GEN-2
------------------------------------------------------------------
受审主体 S = git ls-files docs 实验 lib eng。

对 S 中每一份受跟踪文件 f：

      f ∈ 生成物   ⟺   G1(f) ∨ G2(f) ∨ G3(f)
      f ∈ 第三方   ⟺   G5(f)          （单列，不并入生成物）
      f ∈ 人工产物 ⟺   ¬G1 ∧ ¬G2 ∧ ¬G3 ∧ ¬G5

G1 运行事实自述
    f 的正文含墙钟键  "elapsed_s" | "runtime_s" | "wall_s" |
                    "generated_at" | "written_utc"   （JSON 字符串键形式）
    仓内依据：实验/README.md:8 ——「墙钟类字段（elapsed_s / runtime_s / wall_s /
    generated_at / written_utc）是运行时事实，不参与可复现判定」。
    自述运行时事实的文件，按定义不是人手写下的正本。

G2 派生自述
    f 的正文含派生溯源键  "transcription" | "derived_from" | "generated_by" |
                        "generated_from" | "produced_by"   （后接 ":"）
    或含文本生成横幅  @generated / DO NOT EDIT / 自动生成 / 本文件由…脚本生成。
    自述「我由别的东西派生」的文件不是正本。

G3 产出面
    f 的路径落在下列**声明式产出面**之一，且 f 的扩展名不在「人工件扩展名集」内
    （产出面里的 .md 报告、.py/.cpp 脚本是人写的，留作人工产物）：
        O1 = 实验/*/results/**            ← 实验/README.md:3,8
        O2 = 实验/engineering-evidence/** ← 实验/README.md:7
        O3 = eng/tests/validation/release02/** ← eng/tests/validation/release02/README.md:1,5,6

G5 第三方内联
    f 的路径含 /third_party/。这是「上游代码内联」，既非本仓生成、也非本仓撰写，
    性质与生成物不同，故单列；分母给「含/不含」双口径。

------------------------------------------------------------------
已尝试并**否决**的候选维度（留档，防止后人重犯）
------------------------------------------------------------------
D1「由仓内生成器脚本产生」→ 用「basename 被仓内任一脚本逐字提及」近似实现，
   实测把 1,506 份 / 532,245 行单独判成生成物，误报率不可接受：
   lib/infrastructure/scheduler/src/module_adapters.cpp（16,260 行手写 C++）
   仅因被 eng/tests/backend/*.py 引用即被判生成物。
   根因：**消费 ≠ 生产**。仓内代码引用一个文件名只说明它被读，不说明它被写。
   故 D1 不能单独作判定式；本版不采用。
D2「登记面自述的规则」→ docs/DOCUMENT_INDEX.yaml:24 与
   docs/engineering/DOCUMENT_GOVERNANCE.md:24 写有「机器源与台账不进 docs，落在
   eng/contracts/ 与 artifacts/evidence/」。这是**声明**，且其覆盖面只到 docs/**，
   不覆盖本主体的 实验/ lib/ eng/，本身不构成判定式。G3 的 O1/O2/O3 只借用其中
   可定位到具体行的具名表述，不把该声明本身当判定式。
D3「按大小/按扩展名」→ 纯阈值。实测 top-119 JSON 在 HEAD 只合计 636,629 行，
   与派单给的 925,309 行对不上；且纯阈值无法区分手写大文件与生成大文件。
   故不采用。
"""
import re
import sys
import fnmatch
import collections

# ---------------- G1 ----------------
G1_RE = re.compile(r'"(?:elapsed_s|runtime_s|wall_s|generated_at|written_utc)"')

# ---------------- G2 ----------------
G2_KEY_RE = re.compile(
    r'"(?:transcription|derived_from|generated_by|generated_from|produced_by)"\s*:')
G2_TXT_RE = re.compile(
    r'@generated|DO NOT EDIT|自动生成|自动產生|本文件由\s*\S+\s*(?:脚本|生成器|程序)\s*生成',
    re.IGNORECASE)

# ---------------- G3 ----------------
SURFACES = [
    ("O1", "实验/*/results/*", "实验/README.md:3,8 —— results/ 具名为「结果与判据表」面"),
    ("O2", "实验/engineering-evidence/*",
     "实验/README.md:7 —— 「工程实测类证据留档……时点证据快照，历史记录不改写」"),
    ("O3", "eng/tests/validation/release02/*",
     "eng/tests/validation/release02/README.md:1,5,6 —— 「由 run/RELEASE-02/<name>/ "
     "收集而来（run/ 为 gitignore，故迁入此处长期保留）」"),
]

# 产出面内属「人工件」的扩展名：产出面里的这些是人写的报告/脚本，不算生成物
HUMAN_EXT = {
    ".md", ".py", ".sh", ".cpp", ".c", ".h", ".hpp", ".cmake", ".m4", ".y",
    ".tex", ".rst", ".txt.md",
}

# ---------------- G1 / G2 的适用面（收紧，见下）----------------
# G1/G2 是「内容自述」信号。代码里出现这些键，不等于该件是生成物：
#   · lib/infrastructure/acr/profile/profile_reader.cpp —— 解析 "generated_at" 的读取侧
#   · eng/contracts/schemas/evidence_manifest.schema.json —— 在 required 列表里**定义**该键
#   · lib/algorithms/.../e18_workpool.cpp —— raw string 里的**测试夹具**
# 三者都是人写的正本/合同。故 G1/G2 只在「数据格式件」上生效，且只在**文件头 40 行**内
# 命中（真实结果 dump 的自述键在顶部），并排除 schema / example 这两类**合同定义件**
# （仓内命名约定：eng/contracts/schemas/*.schema.json、eng/contracts/data/examples/*.example.json）。
DATA_EXT = {".json", ".jsonl", ".csv", ".out", ".log", ".txt"}
CONTRACT_EXT = {".schema.json", ".example.json"}

# ---------------- G5 ----------------
G5_TOKEN = "/third_party/"


def ext_of(p):
    base = p.rsplit("/", 1)[-1]
    return base[base.rfind("."):].lower() if "." in base else "(noext)"


def content_signal_eligible(p):
    e = ext_of(p)
    if e not in DATA_EXT:
        return False, "扩展名非数据格式"
    for ce in CONTRACT_EXT:
        if p.lower().endswith(ce):
            return False, "合同定义件（schema/example）"
    return True, ""


def header_of(p):
    """全文。G1/G2 不设行窗口 —— 行窗口是人为参数，会切掉
    「墙钟键出现在结果文件中部」的真生成物（实测 audit_exp3_physical.json 在第 1076 行）。
    误报改由「数据格式件 + 非合同定义件」这条面上规则挡住，判据可见、可审。"""
    try:
        with open(p, "rb") as f:
            return f.read().decode("utf-8", errors="replace")
    except OSError:
        return ""


def match_surface(path):
    for tag, glob, why in SURFACES:
        if fnmatch.fnmatch(path, glob):
            return tag, why
    return None, None


def read_text(path):
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8", errors="replace")
    except OSError:
        return ""


def main():
    tsv_in, tsv_out, sum_out = sys.argv[1], sys.argv[2], sys.argv[3]

    rows = []
    with open(tsv_in, encoding="utf-8") as f:
        next(f)
        for line in f:
            line = line.rstrip("\n")
            if line:
                p, n, b, note = line.split("\t")
                rows.append((p, int(n), int(b)))

    recs = []
    sig_count = collections.Counter()
    suppressed = collections.Counter()
    for p, n, b in rows:
        eligible, why_not = content_signal_eligible(p)
        if eligible:
            head = header_of(p)
            g1 = bool(G1_RE.search(head))
            g2 = bool(G2_KEY_RE.search(head)) or bool(G2_TXT_RE.search(head))
        else:
            g1 = g2 = False
            suppressed[why_not] += 1
        stag, swhy = match_surface(p)
        g3 = bool(stag) and ext_of(p) not in HUMAN_EXT
        sigs_human = (stag + "(人工件扩展名)") if (stag and not g3) else ""
        g5 = G5_TOKEN in p

        sigs = []
        if g1:
            sigs.append("G1")
        if g2:
            sigs.append("G2")
        if g3:
            sigs.append("G3:" + stag)
        if g5:
            sigs.append("G5")
        for s in sigs:
            sig_count[s] += 1

        is_gen = 1 if (g1 or g2 or g3) else 0
        is_tp = 1 if g5 else 0
        recs.append((p, n, b, is_gen, is_tp, "+".join(sigs) or "-",
                     swhy or "", sigs_human))

    with open(tsv_out, "w", encoding="utf-8") as f:
        f.write("path\tlines\tbinary\tgenerated\tthird_party\tsignals"
                "\tsurface_basis\tsurface_human_exception\n")
        for r in recs:
            f.write("%s\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n" % r)

    gen = [r for r in recs if r[3]]
    tp = [r for r in recs if r[4]]
    gen_no_tp = [r for r in gen if not r[4]]
    art = [r for r in recs if not r[3] and not r[4]]

    L = []
    A = L.append
    tot_f, tot_l = len(recs), sum(r[1] for r in recs)

    def block(title, rs):
        A(f"{title:24s} {len(rs):5d} 份 {sum(r[1] for r in rs):9d} 行"
          f"  占主体 {sum(r[1] for r in rs) / tot_l * 100:5.1f}%")

    A("=== GEN-2 判定汇总（HEAD f9650dd0）===")
    block("受审主体", recs)
    block("生成物 G1∨G2∨G3", gen)
    block("  其中不含第三方", gen_no_tp)
    block("第三方内联 G5", tp)
    block("人工产物", art)
    A("")
    A("--- 信号命中计数（可重叠）---")
    for s, c in sorted(sig_count.items(), key=lambda x: -x[1]):
        A(f"  {s:10s} {c:5d} 份")
    A("")
    A("--- G1/G2 收紧：被排除在内容信号外的件（防止误报）---")
    for w, c in suppressed.most_common():
        A(f"  {w:24s} {c:5d} 份")
    A("")
    A("--- 生成物唯一信号归因（不含第三方）---")
    only = collections.Counter()
    onlyl = collections.Counter()
    for r in gen_no_tp:
        only[r[5]] += 1
        onlyl[r[5]] += r[1]
    for s, c in only.most_common():
        A(f"  {s:24s} {c:5d} 份 {onlyl[s]:9d} 行")
    A("")
    A("--- G3 逐产出面命中（仅计数据件；人工件扩展名除外）---")
    for tag, glob, why in SURFACES:
        hit = [r for r in recs if r[5].find("G3:" + tag) >= 0]
        exc = [r for r in recs if r[7].startswith(tag)]
        A(f"  {tag} {glob}")
        A(f"      生成物 {len(hit):4d} 份 {sum(x[1] for x in hit):8d} 行")
        A(f"      人工件例外（留在主体）{len(exc):4d} 份 {sum(x[1] for x in exc):8d} 行")
        A(f"      依据: {why}")
    A("")
    for label, rs in (("人工产物", art), ("生成物", gen), ("第三方内联", tp)):
        A(f"--- {label} 按顶层目录 ---")
        d = collections.Counter()
        dl = collections.Counter()
        for p, n, b, g, t, s, w, e in rs:
            top = p.split("/")[0]
            d[top] += 1
            dl[top] += n
        for k in sorted(dl, key=lambda x: -dl[x]):
            A(f"  {k:8s} {d[k]:5d} 份 {dl[k]:9d} 行")
        A("")
    A("--- 人工产物 Top 40 行数（人工抽查面）---")
    for p, n in sorted(((r[0], r[1]) for r in art), key=lambda x: -x[1])[:40]:
        A(f"  {n:7d}  {p}")
    A("")
    A("--- 人工产物：实验/ 域 Top 15（人工语义密度面）---")
    for p, n in sorted(((r[0], r[1]) for r in art if r[0].startswith("实验/")),
                       key=lambda x: -x[1])[:15]:
        A(f"  {n:7d}  {p}")
    A("")
    A("--- 第三方内联 目录构成 ---")
    d = collections.Counter()
    dl = collections.Counter()
    for p, n, b, g, t, s, w, e in tp:
        key = "/".join(p.split("/")[:3])
        d[key] += 1
        dl[key] += n
    for k in sorted(dl, key=lambda x: -dl[x]):
        A(f"  {k:60s} {d[k]:4d} 份 {dl[k]:8d} 行")

    out = "\n".join(L)
    with open(sum_out, "w", encoding="utf-8") as f:
        f.write(out + "\n")
    print(out)


if __name__ == "__main__":
    main()
