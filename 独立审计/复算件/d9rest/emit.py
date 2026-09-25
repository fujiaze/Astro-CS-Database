# -*- coding: utf-8 -*-
"""D9 补派成稿写出器：追加 ROW 行至 CSV + MD，并维护 PROGRESS 计数。

列序与 `D9-工单对账.csv` 一致：
行号/原条目键/对象/条目/原判级·复核后·判定/问题/原证据位点/现在位点/四态判定/取证/置信/是否进实施任务书
"""
import csv, io, os, sys

RAW = r"独立审计/证据"
CSV = os.path.join(RAW, "D9-工单对账-补.csv")
MD = os.path.join(RAW, "D9-工单对账-补.md")
TOTAL = 30
HEADER = ["行号", "原条目键", "对象", "条目", "原判级/复核后/判定", "问题", "原证据位点",
          "现在位点", "四态判定", "取证命令或读到的实际内容", "置信",
          "是否仍需进 RELEASE-06 实施任务书"]

MD_HEAD = u"""# D9 — 开发节点工单对账【补派：源行 61–90】

**批次**：AUDIT-06 D9 缺口补派。队列 = `独立审计/批次清单/D9-rest.csv`（30 行，源行号 61–90），条目键一律从该件原样截取，不另立清单。
**与成稿 `D9-工单对账.md` 的关系**：沿用其开头**判定口径**（四态定义、字段列序、「已修须见被点名行本身且规模不回退」），不沿用其任何判定结论；两份可直接按行号合并。
**复核基线**：仓库 `F:\\Astro dev\\Astro CS Normalization Database`，HEAD=`c8f64e9a`。
**开工基线状态**：`git status --porcelain` ⇒ 恰两行 `?? ACSD整治工作包_AUDIT-06.zip`、`?? site/`（与工包要求一致，无额外脏项）。
**手段限制**：严格只读、零 git 写；不跑构建 / ctest / `run_checks.py` / 任何 `eng/**` 脚本 / 端到端；不 `import` 仓库 Python；本机 `rg` 不可用，计数一律 `git grep`；判「不存在」以跟踪集 + 删除历史为准。
**时间轴**：工单 CSV 落盘于 2026-09-24 22:26。其后的提交才算本批整改证据：`e9e4b603`(09-24 22:43) 起至 `c8f64e9a`(09-25 13:57)，本批涉及 `c4af4136`(09-25 01:16 文档正向化／无来源常数订正)、`139a2bc4`(09-25 10:11 排异路由／接线面收敛)、`5f8c237b`(09-25 13:30 检查器可信化)、`a6f602fe`(09-24 23:49 项目标识贯穿，行号普遍漂移)。此前提交（`25c8227d` 09-22、`9a2b5d11` 09-21）不作整改证据。

---

## 逐条判定（源行 61–90）

"""


def _count_csv_rows():
    if not os.path.exists(CSV):
        return 0
    with io.open(CSV, "r", encoding="utf-8-sig", newline="") as f:
        return max(0, sum(1 for _ in csv.reader(f)) - 1)


def flush(rows):
    # --- CSV ---
    new = not os.path.exists(CSV)
    with io.open(CSV, "a", encoding="utf-8-sig" if new else "utf-8", newline="") as f:
        w = csv.writer(f, quoting=csv.QUOTE_ALL)
        if new:
            w.writerow(HEADER)
        for r in rows:
            w.writerow(list(r))
    n = _count_csv_rows()

    # --- MD ---
    if not os.path.exists(MD):
        with io.open(MD, "w", encoding="utf-8", newline="\n") as f:
            f.write(MD_HEAD)
    buf = []
    for r in rows:
        buf.append(u"### ROW %s — `%s` :: `%s`\n" % (r[0], r[2], r[3]))
        buf.append(u"- **原判级/复核后/判定**：%s\n" % r[4])
        buf.append(u"- **问题**：%s\n" % r[5])
        buf.append(u"- **原证据位点**：%s\n" % r[6])
        buf.append(u"- **现在位点**：%s\n" % r[7])
        buf.append(u"- **四态判定**：**%s**\n" % r[8])
        buf.append(u"- **取证**：%s\n" % r[9])
        buf.append(u"- **置信**：%s\n" % r[10])
        buf.append(u"- **是否仍需进 RELEASE-06 实施任务书**：%s\n" % r[11])
        buf.append(u"\n")
    text = u"".join(buf)

    with io.open(MD, "r", encoding="utf-8") as f:
        old = f.read()
    marker = u"<!-- PROGRESS: "
    idx = old.find(marker)
    head = old[:idx].rstrip() if idx >= 0 else old.rstrip()
    if head.endswith(u"---"):
        head = head[:-3].rstrip()
    with io.open(MD, "w", encoding="utf-8", newline="\n") as f:
        f.write(head + u"\n\n" + text + u"---\n\n<!-- PROGRESS: %d/%d -->\n" % (n, TOTAL))
    sys.stdout.write("wrote rows %s ; progress %d/%d\n" % ([r[0] for r in rows], n, TOTAL))
