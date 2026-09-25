#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D4 第二路的**筛**：从 records.tsv（抽取原始记录，未经表格化）机械筛出分叉行。

判据：
  T1 数值不等        同一规范键，**各侧给出的取值集合互不相同**（同侧多值不算跨侧分叉）
  T2 值域自相矛盾    合同 default 落在自身 exclusiveMinimum / minimum / maximum / enum 之外
  T3 哨兵语义不一致  一侧 0 / -1 表"未设"、另一侧同键 0 是合法值；或合同允许 null/空串而别侧把它当实值
  T4 登记即错        登记册 declared_default 或 note 断言与 defaults.json 实测不符
  T5 该导出却硬编码  defaults.json 内某数值 = 另两数之积/和（可导出 ⇒ 硬编码冗余），只出候选表
"""
import collections
import csv
import io
import json
import os
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

RAW = r"独立审计/证据"
HERE = os.path.dirname(os.path.abspath(__file__))
FULL = os.path.join(RAW, "D4-多侧默认值全表.csv")
RECS = os.path.join(HERE, "records.tsv")
OUT = os.path.join(HERE, "D4-分叉行筛选.csv")
OUTT5 = os.path.join(HERE, "T5_可导出却硬编码_候选.csv")

SIDES = ["S1_defaults.json", "S2_出厂模板", "S3_CLI骨架",
         "S4_合同schema", "S5_代码兜底", "S6_登记册"]
NOITEM = "该侧无此项"


def load_records():
    g = {}
    with io.open(RECS, encoding="utf-8") as f:
        head = next(f).rstrip("\n").split("\t")
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 6:
                continue
            d = dict(zip(head, parts))
            g.setdefault(d["group"], []).append(d)
    return g


def norm(v):
    v = str(v).strip().strip('"').strip("'")
    if v.lower() in ("true", "false"):
        return v.lower()
    if v.lower() in ("null", "none", "nullptr", ""):
        return None
    try:
        return repr(float(v))
    except Exception:
        return v.lower() if len(v) < 46 else None


def as_num(v):
    try:
        return float(str(v).strip().strip('"'))
    except Exception:
        return None


def side_values(recs_for_group, side):
    """该侧在该键上给出的标量默认集合（S4 只取 default=，S6 只取 declared_default=）。"""
    out = []
    for r in recs_for_group:
        if r["side"] != side:
            continue
        v = r["value"]
        if v.startswith(NOITEM):
            continue
        if side == "S4_合同schema":
            m = re.search(r"(?:^|;)default=([^;]+)", v)
            if m:
                out.append(m.group(1).strip())
        elif side == "S6_登记册":
            m = re.match(r'declared_default=(.*?)(?:\s｜|$)', v)
            if m:
                x = m.group(1).strip()
                if x not in ("null", '""'):
                    out.append(x)
        elif side == "S5_代码兜底":
            for p in v.split(" ## "):
                m = re.match(r"^(\S+)\s+×\d+", p)
                if m:
                    out.append(m.group(1))
        else:
            out.append(v)
    return [x for x in (norm(y) for y in out) if x is not None]


def main():
    groups = load_records()
    full = list(csv.reader(io.open(FULL, encoding="utf-8-sig")))
    rowidx = {r[0]: r for r in full[1:]}

    hits = []
    crit_counter = collections.Counter()
    for g in sorted(groups):
        recs = groups[g]
        byside = collections.defaultdict(list)
        for r in recs:
            byside[r["side"]].append(r)
        sets = {s: set(side_values(recs, s)) for s in SIDES}
        present = {s: v for s, v in sets.items() if v}
        crit, detail = [], []

        # T1
        if len(present) >= 2 and len({frozenset(v) for v in present.values()}) > 1:
            detail.append("T1: " + " ｜".join(
                "%s={%s}" % (s.split("_")[0], ",".join(sorted(v)))
                for s, v in present.items()))
            s1 = sets["S1_defaults.json"]
            s5 = sets["S5_代码兜底"]
            if s1 and s5 and not (s5 & s1):
                crit.append("T1a生产兜底在权威对面")
            elif s1 and s5 and len(s5 - s1) > 0:
                crit.append("T1b生产兜底含权威外值")
            if s1 and (sets["S2_出厂模板"] or sets["S3_CLI骨架"]):
                other = sets["S2_出厂模板"] | sets["S3_CLI骨架"]
                if other and not (other & s1):
                    crit.append("T1c模板/骨架与权威异值")
            crit.append("T1数值不等")

        # T2 值域自相矛盾
        for r in byside.get("S4_合同schema", []):
            v = r["value"]
            d = re.search(r"(?:^|;)default=([^;]+)", v)
            if not d:
                continue
            dv = d.group(1).strip().strip('"')
            dn = as_num(dv)
            ex = re.search(r"exclusiveMinimum=([^;]+)", v)
            mi = re.search(r"(?:^|;)minimum=([^;]+)", v)
            ma = re.search(r"(?:^|;)maximum=([^;]+)", v)
            en = re.search(r"(?:^|;)enum=\[(.*?)\](?:;|$)", v)
            bad = []
            if dn is not None and ex is not None and as_num(ex.group(1)) is not None \
                    and dn <= as_num(ex.group(1)):
                bad.append("default=%s <= exclusiveMinimum=%s" % (dv, ex.group(1)))
            if dn is not None and mi is not None and as_num(mi.group(1)) is not None \
                    and dn < as_num(mi.group(1)):
                bad.append("default=%s < minimum=%s" % (dv, mi.group(1)))
            if dn is not None and ma is not None and as_num(ma.group(1)) is not None \
                    and dn > as_num(ma.group(1)):
                bad.append("default=%s > maximum=%s" % (dv, ma.group(1)))
            if en:
                items = [x.strip().strip('"') for x in en.group(1).split(",")]
                if dv not in items:
                    bad.append("default=%s 不在 enum=[%s]" % (dv, en.group(1)))
            if bad:
                crit.append("T2值域自相矛盾")
                detail.append("T2: %s @%s" % ("; ".join(bad), r["site"][:130]))

        # T3 哨兵
        sent = []
        s1, s5, s4 = sets["S1_defaults.json"], sets["S5_代码兜底"], sets["S4_合同schema"]
        alls = set().union(*[v for v in sets.values() if v]) if sets else set()
        zeroish = {"0", "0.0", "-1", "-1.0"}
        if s1 and s5 and (s1 & zeroish) and (s5 - zeroish):
            sent.append("defaults.json 取 %s 而代码兜底另取 %s：0/-1 在一侧是哨兵、另一侧是实值"
                        % (",".join(sorted(s1 & zeroish)), ",".join(sorted(s5 - zeroish))))
        if (s1 & zeroish or s5 & zeroish) and any(
                re.search(r"exclusiveMinimum=0", r["value"])
                for r in byside.get("S4_合同schema", [])):
            sent.append("合同 exclusiveMinimum=0 把 0 判为非法，而某侧把 0 当默认/兜底 ⇒ 0 两义")
        if "-1" in s5 or "-1.0" in s5:
            others = {s: v for s, v in sets.items() if v and s != "S5_代码兜底"}
            if any(not (v & zeroish) for v in others.values()):
                sent.append("代码侧用 -1 作哨兵，其余侧无 -1 约定（兄弟字段哨兵不同形）")
        null_ok = [r for r in byside.get("S4_合同schema", [])
                   if re.search(r"enum=\[[^\]]*null", r["value"]) or
                   re.search(r'enum=\[[^\]]*""', r["value"])]
        if null_ok and len(present) >= 2:
            sent.append("合同 enum 允许 null/空串（未设即合法值），而其余侧对它给了实值")
        for r in null_ok:
            m = re.search(r"(?:^|;)default=([^;]+)", r["value"])
            if m:
                sent.append("同一合同节点既声明 default=%s 又把 null/空串列进 enum ⇒ "
                            "「未设」与该默认值在值域内同时合法（哨兵与实值两义同节点）@%s"
                            % (m.group(1), r["site"].split("/")[-1][:60]))
                break
        if sent:
            crit.append("T3哨兵不一致")
            detail.append("T3: " + "；".join(sent))

        # T4 登记即错
        for r in byside.get("S6_登记册", []):
            v = r["value"]
            m = re.search(r'declared_default=(\S+)', v)
            n = re.search(r'note默认值断言[:：]?\s*(.*)', v)
            for a in sets["S1_defaults.json"]:
                if m:
                    dv = norm(m.group(1).strip('"'))
                    if dv is not None and dv != a:
                        crit.append("T4登记即错")
                        detail.append("T4: 登记册 declared_default=%s 但 defaults.json 实测=%s @%s"
                                      % (m.group(1), a, r["site"][:120]))
                if n:
                    nums = set(re.findall(r"\d+\.\d+|\d+", n.group(1)))
                    if nums and a not in nums:
                        try:
                            ok = any(abs(float(x) - float(a)) < 1e-12 for x in nums
                                     if as_num(x) is not None)
                        except Exception:
                            ok = False
                        if not ok:
                            crit.append("T4登记即错")
                            detail.append("T4: note 断言「%s」与 defaults.json 实测 %s 不符 @%s"
                                          % (n.group(1)[:70], a, r["site"][:110]))

        if crit:
            row = rowidx.get(g, [""] * 9)
            hits.append([g, row[1], "|".join(sorted(set(crit))),
                         " ;; ".join(dict.fromkeys(detail))[:1600],
                         row[3], row[4], row[5], row[6], row[7], row[8],
                         str(len(present))])
            for c in set(crit):
                crit_counter[c] += 1

    # ── T5 候选：defaults.json 数值可由同表另两数导出 ──
    dvals = collections.defaultdict(set)
    for g, recs in groups.items():
        for r in recs:
            if r["side"] == "S1_defaults.json":
                n = as_num(r["value"])
                if n is not None and n != 0:
                    dvals[g].add(n)
    flat = [(g, v) for g, vs in dvals.items() for v in vs]
    cand = collections.defaultdict(set)
    for i, (gi, a) in enumerate(flat):
        for j, (gj, b) in enumerate(flat):
            if i >= j or a == b:
                continue
            prod, sm = a * b, a + b
            for k, (gk, t) in enumerate(flat):
                if k in (i, j) or t in (0, 1):
                    continue
                if abs(prod - t) < 1e-9:
                    cand[gk].add("%s(%g) × %s(%g) = %g" % (gi, a, gj, b, t))
                elif abs(sm - t) < 1e-9 and t > max(a, b):
                    cand[gk].add("%s(%g) + %s(%g) = %g" % (gi, a, gj, b, t))

    PRIO = {"T1a生产兜底在权威对面": 0, "T1b生产兜底含权威外值": 0,
            "T1c模板/骨架与权威异值": 1, "T2值域自相矛盾": 1,
            "T3哨兵不一致": 2, "T4登记即错": 3, "T1数值不等": 4}
    PRIO_NAME = {0: "①生产兜底在对面", 1: "②值域自相矛盾", 2: "③哨兵不一致",
                 3: "④登记面/文档面分歧", 4: "⑤仅跨侧异值（含同名异义嫌疑）",
                 9: "⑤仅跨侧异值（含同名异义嫌疑）"}

    def prio(h):
        return min(PRIO.get(c, 9) for c in h[2].split("|"))

    hits.sort(key=lambda h: (prio(h), h[0]))
    with io.open(OUT, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["优先档", "键", "键族", "机械命中判据", "机械细节"] + SIDES + ["有值侧数"])
        for h in hits:
            w.writerow([PRIO_NAME[prio(h)]] + h)
    with io.open(OUTT5, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["键", "defaults.json 内部可导出关系（候选，需人工判定是否真为导出量）"])
        for k in sorted(cand):
            w.writerow([k, " ;; ".join(sorted(cand[k])[:8])])

    print("全量表数据行: %d ; 分组数: %d" % (len(full) - 1, len(groups)))
    print("机械筛出分叉行: %d (%.1f%%)" % (len(hits), 100.0 * len(hits) / max(1, len(full) - 1)))
    for k, v in crit_counter.most_common():
        print("  %-16s %d" % (k, v))
    print("T5 候选键数: %d" % len(cand))
    print("输出: %s ; %s" % (OUT, OUTT5))


if __name__ == "__main__":
    main()
