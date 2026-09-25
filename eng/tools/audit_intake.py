#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""审查报告接收器：把外部审查节点的报告转成可施工的工单，并逐条核验引用锚。

为什么要它：审查报告的引用锚会失真——实测过的三种形态是
  ① 引用的文件只在 run/*/wsrc/ 这类临时快照里存在，当前树里根本没有；
  ② 行号随并发改动漂移，指向别的代码；
  ③ 文件与行号都在，但该行内容与该条声称的不是一回事。
先做锚核验再动手，可以避免整条工单追逐一个不存在的对象。

本工具**只读**：不跑构建、不跑测试、不写仓库，只读文件并输出工单。

出口：0 全部锚可核；1 存在失效锚（工单里逐条标出）；2 输入缺失或不可解析（fail-closed）。
"""

import argparse
import csv
import os
import re
import subprocess
import sys

# 引用锚形态：路径:行号（路径允许含空格之外的字面量；行号可省）
CITE_RE = re.compile(r"(?P<path>[A-Za-z0-9_./+-]+\.[A-Za-z0-9]+):(?P<line>\d+)(?:-(?P<end>\d+))?")
# 条目形态：Markdown 的 `**N｜标题**` 或 CSV 的一行
MD_ITEM_RE = re.compile(r"^\*\*(?P<id>[0-9]+)\uff5c(?P<title>.+?)\*\*", re.M)


def tracked(root):
    out = subprocess.run(['git', 'ls-files'], cwd=root, capture_output=True, text=True)
    if out.returncode != 0:
        return None
    return set(out.stdout.splitlines())


def temp_only(root, rel):
    """该路径是否只存在于临时快照面（run/ 不入库，审查节点可能引到其中）。"""
    hits = []
    for base, _dirs, files in os.walk(os.path.join(root, 'run')):
        for f in files:
            p = os.path.join(base, f)
            if p.endswith(rel) or rel in p.replace(os.sep, '/'):
                hits.append(os.path.relpath(p, root))
                if len(hits) >= 2:
                    return hits
    return hits


def check_cite(root, tr, path, line, end):
    if path not in tr:
        tmp = temp_only(root, path)
        return ('TEMP_ONLY' if tmp else 'FILE_MISSING', (tmp[0] if tmp else ''))
    full = os.path.join(root, path)
    try:
        n = sum(1 for _ in open(full, 'rb'))
    except OSError as exc:
        return ('UNREADABLE', str(exc)[:60])
    if line < 1 or line > n:
        return ('LINE_OUT_OF_RANGE', '文件共 %d 行，引用第 %d 行' % (n, line))
    if end and end > n:
        return ('RANGE_OUT_OF_RANGE', '文件共 %d 行，引用到第 %d 行' % (n, end))
    return ('OK', '')


def parse_md(text):
    items = []
    marks = list(MD_ITEM_RE.finditer(text))
    for i, m in enumerate(marks):
        stop = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        items.append({'id': m.group('id'), 'title': m.group('title').strip(),
                      'body': text[m.end():stop]})
    return items


def parse_csv(path):
    items = []
    with open(path, newline='', encoding='utf-8') as fh:
        for i, row in enumerate(csv.DictReader(fh), 1):
            items.append({'id': str(row.get('id') or row.get('编号') or i),
                          'title': str(row.get('title') or row.get('问题') or '')[:120],
                          'body': ' '.join(str(v) for v in row.values() if v)})
    return items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('report', help='审查报告（.md 或 .csv）')
    ap.add_argument('--repo', default='.')
    ap.add_argument('--out', default=None, help='工单 CSV 输出路径（默认打印摘要）')
    args = ap.parse_args()

    root = os.path.abspath(args.repo)
    if not os.path.isfile(args.report):
        print('report missing: %s' % args.report, file=sys.stderr)
        return 2
    tr = tracked(root)
    if tr is None:
        print('git ls-files 不可用（fail-closed）', file=sys.stderr)
        return 2

    text = open(args.report, encoding='utf-8').read()
    items = parse_csv(args.report) if args.report.endswith('.csv') else parse_md(text)
    if not items:
        print('未解析出任何条目（fail-closed）：报告格式需含 `**N｜标题**` 或 id 列', file=sys.stderr)
        return 2

    rows, bad = [], 0
    for it in items:
        cites = list(CITE_RE.finditer(it['body'] + ' ' + it['title']))
        if not cites:
            rows.append({'id': it['id'], 'title': it['title'][:70], 'anchor': '',
                         'status': 'NO_ANCHOR', 'detail': '条目没有可核验的 文件:行 锚'})
            bad += 1
            continue
        for c in cites:
            st, det = check_cite(root, tr, c.group('path'), int(c.group('line')),
                                 int(c.group('end')) if c.group('end') else None)
            if st != 'OK':
                bad += 1
            rows.append({'id': it['id'], 'title': it['title'][:70],
                         'anchor': '%s:%s' % (c.group('path'), c.group('line')),
                         'status': st, 'detail': det})

    ok = len(rows) - bad
    print('items=%d anchors=%d ok=%d bad=%d' % (len(items), len(rows), ok, bad))
    for r in rows:
        if r['status'] != 'OK':
            print('  [%s] #%s %s @ %s %s' % (r['status'], r['id'], r['title'][:44],
                                            r['anchor'], r['detail'][:60]))
    if args.out:
        with open(args.out, 'w', newline='', encoding='utf-8') as fh:
            w = csv.DictWriter(fh, fieldnames=['id', 'title', 'anchor', 'status', 'detail'])
            w.writeheader()
            w.writerows(rows)
        print('worklist ->', args.out)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())