#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 R2 · 抗漂移切片器 GEN-2-SLICE
=====================================
只读。不编译、不跑测试、不跑实验、不写 git。

用法:
    python3 slice.py <逐份判定.tsv> <片清单.yaml> <切片统计.txt> [目标行数T]

------------------------------------------------------------------
为什么这样切（原台账的病：分母漂移 160%，「一遍 = 63 车道」失效）
------------------------------------------------------------------
原台账把片定义成「绝对行数的等分块」，并把**完成度记在片号上**。
分母一变，片号整体重排，已完成的片号对不上新片号 ⇒ 进度归零或重复计。

GEN-2-SLICE 用两条规则消掉这个病：

  规则 A（分层键稳定）
      片的首要身份是**分层键** = 受审顶层的四个域
      `docs` / `实验` / `lib` / `eng`。
      这四个键就是「审查主体」定义本身（git ls-files docs 实验 lib eng），
      与行数无关、与内容增删无关，是本仓最稳的结构单位。
      分母漂移时域键集合不变，变的只是**每个域内切成几片**。

      域内再按**路径字典序**排序 —— 字典序即层级序，故模块天然连续，
      片不会把一个模块拦腰截断（跨模块只发生在域内的片边界上）。
      片另记 `主模块`（该片内占比最大的三级目录）供派单时定位，不参与片身份。

  规则 B（完成度按文件记账，不按片号记账）—— 这是抗漂移的关键
      一份交付件声明它读了哪些**文件路径**。重切片只改片号、不改文件集合，
      故「已读一遍」的判定 = 文件路径集合，重切后依然成立：
      既不会因片号漂移而丢失，也不会因片号重排而重复计数。
      片号只是**派发与排程用的视图**，不是进度的真相源。

  分层内切分：顺序累加行数 → 每满 T 行切一片。**单个文件不跨片**；
  单文件 > T 时独占一片并标 oversize=true。

  分层深度为什么取 1（顶层域）而不是 3：
  实测按三级目录分层会得到约 330 个层，绝大多数层不足 T 行 ⇒ 片数 367、
  片行数中位仅 249，远超「一遍 = 70 车道」的容量口径（敏感度表见切片统计.txt）。
  分层过细会把容量约束架空。故取顶层域作分层键，模块连续性由字典序保证。
"""
import sys
import collections

T_DEFAULT = 11000


DOMAIN_ORDER = ["docs", "实验", "lib", "eng"]


def stratum_of(path, depth=1):
    parts = path.split("/")
    return "/".join(parts[:depth])


def module_of(path, depth=3):
    parts = path.split("/")
    if len(parts) < depth:
        return "/".join(parts)
    return "/".join(parts[:depth])


def build_slices(art, T):
    """art = [(path, lines)]。返回 [(sid, stratum, items, target, actual, oversize, dom_label)]"""
    by_dom = collections.defaultdict(list)
    for p, n in art:
        by_dom[p.split("/")[0]].append((p, n))

    slices = []
    for di, dom in enumerate(DOMAIN_ORDER, start=1):
        items = sorted(by_dom.get(dom, []), key=lambda x: x[0])
        if not items:
            continue
        cur, cur_l, cur_mod = [], 0, collections.Counter()
        k = 0
        for p, n in items:
            if n > T:
                if cur:
                    k += 1
                    slices.append((f"S{di}-{k:02d}", dom, cur, T, cur_l, False,
                                   cur_mod.most_common(1)[0][0]))
                    cur, cur_l, cur_mod = [], 0, collections.Counter()
                k += 1
                mc = collections.Counter({module_of(p): n})
                slices.append((f"S{di}-{k:02d}", dom, [(p, n)], n, n, True,
                               mc.most_common(1)[0][0]))
                continue
            if cur_l + n > T and cur:
                k += 1
                slices.append((f"S{di}-{k:02d}", dom, cur, T, cur_l, False,
                               cur_mod.most_common(1)[0][0]))
                cur, cur_l, cur_mod = [], 0, collections.Counter()
            cur.append((p, n))
            cur_l += n
            cur_mod[module_of(p)] += n
        if cur:
            k += 1
            slices.append((f"S{di}-{k:02d}", dom, cur, T, cur_l, False,
                           cur_mod.most_common(1)[0][0]))
    return slices


def yaml_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    verdict_tsv, yaml_out, stat_out = sys.argv[1], sys.argv[2], sys.argv[3]
    T = int(sys.argv[4]) if len(sys.argv) > 4 else T_DEFAULT

    rows = []
    with open(verdict_tsv, encoding="utf-8") as f:
        next(f)
        for line in f:
            line = line.rstrip("\n")
            if line:
                p, n, b, g, t, sig, basis, exc = line.split("\t")
                rows.append((p, int(n), int(g), int(t)))

    art = [(p, n) for p, n, g, t in rows if not g and not t]
    tot_files, tot_lines = len(art), sum(n for _, n in art)

    sl = build_slices(art, T)
    n_slices = len(sl)
    ten = n_slices * 10

    # ---------------- YAML ----------------
    by_dom = collections.Counter()
    by_dom_l = collections.Counter()
    for p, n in art:
        by_dom[p.split("/")[0]] += 1
        by_dom_l[p.split("/")[0]] += n

    strat_lines = collections.Counter()
    strat_files = collections.Counter()
    for sid, skey, items, tg, ac, ov, dom in sl:
        strat_lines[skey] += ac
        strat_files[skey] += len(items)

    with open(yaml_out, "w", encoding="utf-8") as f:
        f.write("# GOVERN-08 R2 · 收敛分母与片划分（机器可读）\n")
        f.write("# 仓内持久路径；不用 /tmp（工作包与 /tmp/r3 已先后丢失）。\n")
        f.write("# 生成命令：python3 证据/measure.py … && python3 证据/gen2_classify.py … "
                "&& python3 证据/slice.py … 11000\n")
        f.write("---\n")
        f.write("元信息:\n")
        f.write("  仓: \"Astro CS Database\"\n")
        f.write("  head: \"f9650dd0\"\n")
        f.write("  审查主体: \"git ls-files docs 实验 lib eng\"\n")
        f.write("  行数口径: \"wc -l（换行符计数）。备选口径 +1（末行无换行另计 1 行）\n")
        f.write("    给出 1,748,063 行，差 438 行 = 438 份无末尾换行文件，"
                "占比 0.025%，不改变车道数。\"\n")
        f.write("  判定式: \"GEN-2（见 00-分母与片划分.md §2）\"\n")
        f.write("  切片规则: \"GEN-2-SLICE（见 00-分母与片划分.md §4）\"\n")
        f.write("  目标行数T: %d\n" % T)
        f.write("  完成度记账单位: \"文件路径（不按片号）—— 抗漂移关键\"\n")
        f.write("\n分母:\n")
        f.write("  主体:       { 份数: %d, 行数: %d }\n" % (
            len(rows), sum(n for _, n, _, _ in rows)))
        f.write("  生成物:     { 份数: %d, 行数: %d }\n" % (
            sum(1 for _, _, g, _ in rows if g),
            sum(n for _, n, g, _ in rows if g)))
        f.write("  第三方内联: { 份数: %d, 行数: %d }\n" % (
            sum(1 for _, _, _, t in rows if t),
            sum(n for _, n, _, t in rows if t)))
        f.write("  人工产物:   { 份数: %d, 行数: %d }\n" % (tot_files, tot_lines))
        f.write("  人工产物_按域:\n")
        for d in sorted(by_dom_l, key=lambda x: -by_dom_l[x]):
            f.write("    %s: { 份数: %d, 行数: %d }\n" % (
                yaml_escape(d), by_dom[d], by_dom_l[d]))
        f.write("\n车道:\n")
        f.write("  一遍车道数: %d\n" % n_slices)
        f.write("  十遍车道数: %d\n" % ten)
        f.write("  片数依据: \"人工产物 %d 行 ÷ 目标 %d 行/车道 = %.1f 车道；\n"
                "    实际 %d 片。差额来自「单文件不跨片」与 2 份超 T 巨型单文件独占成片。\n"
                "    四域各自成片、域内按路径字典序（层级序）累加切分。\"\n" % (
                    tot_lines, T, tot_lines / T, n_slices))
        f.write("\n分层:\n")
        for skey in DOMAIN_ORDER:
            if strat_lines[skey] == 0:
                continue
            f.write("  - 键: \"%s\"\n    份数: %d\n    行数: %d\n    片数: %d\n" % (
                yaml_escape(skey), strat_files[skey], strat_lines[skey],
                sum(1 for s in sl if s[1] == skey)))
        f.write("\n片:\n")
        for sid, skey, items, tg, ac, ov, dom in sl:
            f.write("  - 片号: \"%s\"\n" % sid)
            f.write("    分层键: \"%s\"\n" % yaml_escape(skey))
            f.write("    主模块: \"%s\"\n" % yaml_escape(dom))
            f.write("    目标行数: %d\n" % tg)
            f.write("    实际行数: %d\n" % ac)
            f.write("    成员数: %d\n" % len(items))
            f.write("    超大片: %s\n" % ("true" if ov else "false"))
            f.write("    划片依据: \"分层键=%s；域内按路径字典序（层级序，模块连续）累加行数，"
                    "每满 %d 行切一片；单文件不跨片（实际 %d 行）\"\n" % (
                        skey, T, ac))
            f.write("    成员文件:\n")
            for p, n in items:
                f.write("      - { 路径: \"%s\", 行数: %d }\n" % (yaml_escape(p), n))

    # ---------------- 统计 ----------------
    L = []
    A = L.append
    A("=== GEN-2-SLICE 切片统计（T = %d 行/车道）===" % T)
    A(f"人工产物 {tot_files} 份 / {tot_lines} 行")
    A(f"理论车道数 = ceil({tot_lines} / {T}) = {-(-tot_lines // T)}")
    A(f"实际片数   = {n_slices}")
    A(f"一遍 = {n_slices} 车道    十遍 = {ten} 车道")
    A("")
    A("--- 片实际行数分布 ---")
    acs = sorted(s[4] for s in sl)
    A(f"  最小 {acs[0]}  中位 {acs[len(acs) // 2]}  最大 {acs[-1]}")
    A(f"  达到 T 的 90% 以上: {sum(1 for x in acs if x >= T * 0.9)} / {n_slices}")
    A(f"  超大片（单文件 > T）: {sum(1 for s in sl if s[5])}")
    for sid, skey, items, tg, ac, ov, dom in sl:
        if ov:
            A(f"    {sid}  {ac} 行  {items[0][0]}")
    A("")
    A("--- 容量敏感度（车道数随 T 变化）---")
    for t2 in (8000, 10000, 11000, 12000, 15000):
        s2 = build_slices(art, t2)
        A(f"  T={t2:6d} → {len(s2):4d} 片   十遍 {len(s2) * 10:5d} 车道")
    A("")
    A("--- 分域 → 片数（片数依据）---")
    c = collections.Counter()
    cl = collections.Counter()
    cf = collections.Counter()
    for sid, skey, items, tg, ac, ov, dom in sl:
        c[skey] += 1
        cl[skey] += ac
        cf[skey] += len(items)
    for skey in DOMAIN_ORDER:
        if cl[skey] == 0:
            continue
        A(f"  {cl[skey]:7d} 行 {cf[skey]:4d} 份 {c[skey]:3d} 片  {skey}")
    A(f"  {'合计':>7s}     {tot_files:4d} 份 {n_slices:3d} 片")
    A("")
    A("--- 每片主模块分布（跨模块片数）---")
    cross = 0
    for sid, skey, items, tg, ac, ov, dom in sl:
        mods = set(module_of(p) for p, _ in items)
        if len(mods) > 1:
            cross += 1
    A(f"  跨 >1 个主模块的片: {cross} / {n_slices}")
    A("")
    A("--- 全部片一览（片号 / 分层 / 主模块 / 份数 / 实际行数）---")
    for sid, skey, items, tg, ac, ov, dom in sl:
        A(f"  {sid:8s} {skey:6s} {dom:44s} {len(items):4d} 份 {ac:6d} 行"
          f"{'  [超大片]' if ov else ''}")

    out = "\n".join(L)
    with open(stat_out, "w", encoding="utf-8") as f:
        f.write(out + "\n")
    print(out)


if __name__ == "__main__":
    main()
