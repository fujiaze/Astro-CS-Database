"""残余A 的机判细分：登记面点名的常数，其登记条目自己有没有出处、出处锚是否真含该值。"""
import sys
import io
import csv
import json
import re
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')
ATT = BASE / '独立审计包' / '06_实施' / '附件'

DOSSIER = ATT / 'A4_生产具名常数机械卷宗.csv'
rows = list(csv.DictReader(io.open(DOSSIER, encoding='utf-8-sig')))
resid_a = [r for r in rows if r['预分档'] == '残余A·登记面点名但无支撑']
print('残余A 符号 %d 个' % len(resid_a))

# 载入登记面，抽每个键的自述出处字段
reg = {}
for f in ['eng/packaging/config/defaults.json', 'eng/packaging/config/config_registry.json',
          'eng/contracts/resource_gate_v1.json']:
    p = REPO / f
    if not p.exists():
        continue
    try:
        txt = p.read_text(encoding='utf-8', errors='replace')
        data = json.loads(txt)
    except Exception as e:
        print('  %s 解析失败（%s）→ 该面按"不可机读"处理' % (f, type(e).__name__))
        data = None
    if data is None:
        continue

    def walk(node, path):
        if isinstance(node, dict):
            name = node.get('key') or node.get('name') or node.get('id') or path[-1] if path else ''
            if isinstance(name, str) and name:
                srcs = {k: str(node[k]) for k in node
                        if any(t in k.lower() for t in ('source', 'authority', 'note', 'ref', 'basis', 'origin'))}
                if srcs:
                    reg.setdefault(name, []).append((f, srcs))
            for k, v in node.items():
                if isinstance(v, (dict, list)):
                    walk(v, path + [str(k)])
        elif isinstance(node, list):
            for it in node:
                walk(it, path)
    walk(data, [f])
print('登记面抽出带出处字段的键 %d 个' % len(reg))

ANCH = re.compile(r'([\w./\-]+\.(?:md|json|csv|py|cpp|h|yaml|txt)):(\d+)')
out = []
for r in resid_a:
    s = r['符号']
    hits = []
    for f, srcs in reg.items():
        pass
    entries = reg.get(s, []) or reg.get(s.split('.')[-1], [])
    cls, detail = 'A-1 登记面点名但无出处字段', ''
    anchors = []
    for fname, srcs in entries:
        for k, v in srcs.items():
            anchors += [(fname, k) + m for m in ANCH.findall(v)]
    if entries and not anchors:
        cls = 'A-2 登记面有出处文字但无可解析锚'
        detail = ' | '.join('%s.%s' % (k, v[:40]) for _, ss in entries for k, v in ss.items())[:160]
    elif anchors:
        ok = bad = 0
        bads = []
        for fname, field, path, ln in anchors[:6]:
            target = (REPO / path) if not path.startswith('/') else Path(path)
            if not target.exists():
                for cand in REPO.rglob(Path(path).name):
                    target = cand
                    break
            try:
                lines = target.read_text(encoding='utf-8', errors='replace').splitlines()
                seg = lines[int(ln) - 1] if 0 < int(ln) <= len(lines) else ''
            except Exception:
                seg = ''
            val = (r['值样例'] or '').split('|')[0].strip()
            if val and val in seg:
                ok += 1
            else:
                bad += 1
                bads.append('%s:%s→%s:%s' % (path, ln, field, seg[:40]))
        cls = 'A-3 锚可解析且真含该值' if bad == 0 and ok else ('A-4 锚失效或不含该值' if bad else 'A-5 锚指向不可解析路径')
        detail = ' ;; '.join(bads[:3])
    out.append({'符号': s, '细分档': cls, '位点数': r['位点数'], '值样例': r['值样例'][:40],
                '登记面': ' | '.join(sorted({f for f, _ in entries}))[:120], '证据/失效详情': detail[:200]})

with io.open(ATT / 'A5_登记面点名常数的出处细分.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(out[0].keys()))
    w.writeheader()
    for r in out:
        w.writerow(r)
from collections import Counter
print('细分结果：')
for k, v in Counter(r['细分档'] for r in out).most_common():
    print('   %-30s %d' % (k, v))
need = [r for r in out if r['细分档'].startswith(('A-1', 'A-2', 'A-4', 'A-5'))]
print('需人工/派工判定的 = %d 个符号（占残余A 的 %.0f%%）' % (len(need), 100 * len(need) / max(1, len(out))))
