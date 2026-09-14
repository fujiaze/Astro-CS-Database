#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
层1-C 分片脚本：把 index.jsonl 压成紧凑卡，再按固定条数窗口切片，
相邻两片共享最后 10 条（重叠），供子子代理交叉验证。
输出:
  _evidence/commits/compact/NNNN-sha8.md      紧凑卡（约 1.2KB，覆盖全部提交）
  _evidence/commits/slices/Sxx/manifest.csv   分片清单（seq/角色/紧凑卡/全卡）
  _evidence/commits/slices/Sxx/BRIEF.md       分片说明书与输出模板
  _evidence/commits/slices/index.csv          全部分片总表
用法: python3 03_build_slices.py [--size 55] [--overlap 10]
"""
import os, json, re, sys, csv
from collections import defaultdict

REPO = "/workspace/Astro CS Database"
EV = os.path.join(REPO, "设计大纲/_evidence/commits")
COM = os.path.join(EV, "compact"); SL = os.path.join(EV, "slices")
os.makedirs(COM, exist_ok=True); os.makedirs(SL, exist_ok=True)
argv = sys.argv[1:]
def flag(n, d): return argv[argv.index(n) + 1] if n in argv else d
SIZE = int(flag("--size", "55")); OVER = int(flag("--overlap", "10"))

CODE_DIRS = ("lib/", "cli/", "include/", "tests/", "providers/", "runtime/", "modules/",
             "contracts/", "cmake/", "CMakeLists.txt", "tools/", "ci/", "scripts/", "packaging/")
PACK_RE = re.compile(r"(工程控制|engineering/control|_control_packs|/tasks/|00_READ_FIRST|TASK_LEDGER|control-pack\.json|MANIFEST\.json|SHA256SUMS|START_PROMPT|^evidence/|^artifacts/|^reports/)")

recs = [json.loads(l) for l in open(os.path.join(EV, "index.jsonl"), encoding="utf-8")]
recs.sort(key=lambda r: r["seq"])

def compact(r):
    o = []
    o.append("C%04d %s %s | %s | %s | 文件%d +%d -%d | 分级%s%s" % (
        r["seq"], r["sha"][:8], r["adate"][:10], r["aname"], "merge" if r["merge"] else "单亲",
        r["nfile"], r["add"], r["dele"], r["cls"], " | 结构模式" if r.get("struct_only") else ""))
    o.append("编号: " + (", ".join(r["ids"][:10]) if r["ids"] else "无"))
    o.append("消息首行: " + r["subject"][:260].replace("\n", " ") + ("…" if len(r["subject"]) > 260 else ""))
    body = (r.get("body") or "").strip()
    o.append("消息正文长度 %d 字符%s" % (len(body), "" if body else "（无正文）"))
    dh = sorted((r["dirhist"] or {}).items(), key=lambda x: -x[1][0])[:8]
    o.append("变更面: " + ("; ".join("%s(%d文件,+%d/-%d)" % (k, v[0], v[1], v[2]) for k, v in dh)
                          if dh else "全部在排除区（影子树/构建产物/数据）"))
    st = defaultdict(int)
    for f in r["files"]:
        s = f[0]
        st["A" if s == "A" else "D" if s in ("D",) else "R" if s in ("R", "C", "T") else "M"] += 1
    o.append("操作构成: 新增%d 修改%d 删除%d 改名%d | 二进制%d 噪声区%d" % (
        st["A"], st["M"], st["D"], st["R"], r["nbin"], r["nnoise"]))
    code = [f for f in r["files"] if (f[2] or f[1]).startswith(CODE_DIRS)]
    pack = [f for f in r["files"] if PACK_RE.search(f[2] or f[1])]
    o.append("代码面文件 %d 个；包与证据面文件 %d 个" % (len(code), len(pack)))
    if code[:12]: o.append("  代码样本: " + ", ".join((f[2] or f[1]).split("/")[-1] for f in code[:12]))
    if pack[:12]: o.append("  包与证据样本: " + ", ".join((f[2] or f[1]).split("/")[-1] for f in pack[:12]))
    if r.get("root_hint"): o.append("谱系: " + r["root_hint"])
    full = os.path.join(EV, r["card"])
    o.append("全卡: " + r["card"] + ("（%d 字节）" % os.path.getsize(full) if os.path.exists(full) else "（缺失）"))
    return "\n".join(o)

series = []
for r in recs:
    cp = os.path.join(COM, "%04d-%s.md" % (r["seq"], r["sha"][:8]))
    txt = compact(r)
    if len(txt.encode("utf-8")) > 1800: txt = txt[:1700] + "\n（紧凑卡截断，细节看全卡）"
    open(cp, "w", encoding="utf-8").write(txt)
    r["_compact"] = os.path.relpath(cp, EV)
    r["_weight"] = {"S": 1, "M": 2, "L": 4}.get(r["cls"], 1)
    series.append(r)

total = len(series)
sids = []; i = 0; k = 0
while i < total:
    k += 1
    prim = series[i:i + SIZE]
    ov = series[max(0, i - OVER):i] if i > 0 else []
    sid = "S%02d" % k
    d = os.path.join(SL, sid); os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "manifest.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["seq", "sha8", "日期", "作者", "分级", "权重", "角色", "紧凑卡", "全卡", "消息首行"])
        for r in ov:
            w.writerow([r["seq"], r["sha"][:8], r["adate"][:10], r["aname"], r["cls"], r["_weight"],
                        "重叠复核", r["_compact"], r["card"], r["subject"][:90]])
        for r in prim:
            w.writerow([r["seq"], r["sha"][:8], r["adate"][:10], r["aname"], r["cls"], r["_weight"],
                        "主责", r["_compact"], r["card"], r["subject"][:90]])
    wsum = sum(r["_weight"] for r in prim) + sum(r["_weight"] for r in ov)
    def has(r, pred): return any(pred(x[2] or x[1]) for x in r["files"])
    code_ct = sum(1 for r in prim if has(r, lambda p: p.startswith(CODE_DIRS)))
    pack_ct = sum(1 for r in prim if has(r, lambda p: bool(PACK_RE.search(p))))
    doc_ct = len(prim) - code_ct - pack_ct
    fams = sorted({re.split(r"[^A-Za-z0-9]", t)[0] for r in prim for t in r["ids"]})
    b = []
    b.append("# 分片 %s 说明书" % sid)
    b.append("")
    b.append("- 主责提交 %d 条（seq %d..%d，%s 至 %s）；重叠复核 %d 条（seq %s）" % (
        len(prim), prim[0]["seq"], prim[-1]["seq"], prim[0]["adate"][:10], prim[-1]["adate"][:10],
        len(ov), ("%d..%d" % (ov[0]["seq"], ov[-1]["seq"])) if ov else "无"))
    b.append("- 权重合计 %d；触及代码或测试 %d 条；触及控制包与证据面 %d 条；纯文档或其它 %d 条" % (wsum, code_ct, pack_ct, doc_ct))
    b.append("- 本区编号族: " + (", ".join(fams[:25]) or "无"))
    b.append("")
    b.append("## 读法（必须遵守）")
    b.append("1. 先完整读 manifest.csv 每一行（含消息首行、变更面、代码与包计数）。")
    b.append("2. 主责条目必须逐条出正文；重叠复核条目只写与上一片结论是否一致。")
    b.append("3. 需要更多细节时读紧凑卡，仍不足再读全卡（全卡含内容摘录、函数签名、台账行变化、文档标题增删）。")
    b.append("4. 只有卡片明显不足时才允许自行执行 git show，且报告中不得粘贴源码或 diff；引用标识符即可。")
    b.append("5. 分级 L 或标了结构模式的提交（批量导入、搬家、整理）：按文件集合与目录直方图归纳，不逐文件展开。")
    b.append("6. 改动全在排除区的提交：登记为运行产物或影子树副本类，注明不构成工作内容证据。")
    b.append("")
    b.append("## 输出模板（每条一组，同编号连续提交可合并成组但须列全 seq 与 sha8）")
    b.append("### seq 段 | sha8 | 日期 | 分级")
    b.append("- 做了什么：自然语言，指向具体模块、文件、任务号")
    b.append("- 落点：目录或文件（可聚类列举）")
    b.append("- 内容性质：生产代码 / 测试 / CI 或机器门 / 文档或规格 / 控制包或台账 / 数据或产物 / 结构整理")
    b.append("- 意图证据：区分" + chr(8220) + "消息自述" + chr(8221) + "与" + chr(8220) + "变更面证据" + chr(8221))
    b.append("- 关联：与同编号或同模块的相邻提交关系")
    b.append("- 不确定点：证据不足处显式写" + chr(8220) + "未证实" + chr(8221))
    b.append("")
    b.append("## 片尾必写")
    b.append("- 本片主题归纳（标注为归纳，并列出依据的 seq 清单）")
    b.append("- 重叠区核对（逐条：一致 或 不一致 + 原因）")
    b.append("- 本片遗留问题与证据缺口")
    open(os.path.join(d, "BRIEF.md"), "w", encoding="utf-8").write("\n".join(b) + "\n")
    sids.append({"slice": sid, "n_primary": len(prim), "n_overlap": len(ov), "weight": wsum,
                 "seq_from": prim[0]["seq"], "seq_to": prim[-1]["seq"],
                 "date_from": prim[0]["adate"][:10], "date_to": prim[-1]["adate"][:10],
                 "code_ct": code_ct, "pack_ct": pack_ct, "doc_ct": doc_ct,
                 "id_families": ";".join(sorted({t for r in prim for t in r["ids"]})[:20])})
    i += SIZE
    json.dump(sids, open(os.path.join(SL, "index.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    with open(os.path.join(SL, "index.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(sids[0].keys())); w.writeheader(); w.writerows(sids)
print("slices=%d commits=%d size=%d overlap=%d" % (len(sids), total, SIZE, OVER))
