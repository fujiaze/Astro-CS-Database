"""第②层判定回收器：从 复核-*.md 抽"条目 → 判定"，落台账并算缺口。

用法：
  python verify_collect.py                # 回收＋出台账＋缺口
  python verify_collect.py --self-test    # 正负例自检（含"必须判为缺口"的负例）

判定行识别（两种形态都吃）：
  ## AUD401-002 · 主张文字 —— 判定：确认
  **判定：降级（S2）**            ← 取该文件里最近的 ## 标题作条目
档位词表固定；表外词一律落 `未匹配` 不猜。
"""
import re
import sys
import io
import csv
import json
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
RAW = BASE / 'raw'
OUT = BASE / 'inventory'
INVENTORY_DIR = BASE / 'inventory'

VERDICTS = ['确认', '降级', '推翻', '待证', '重复']
HEAD = re.compile(r'^##+\s*([A-Za-z]?[A-Za-z0-9_.\-]*(?:-\d+|[0-9]{2})|[^\n]{0,80}?)\s*·\s*(.*)$')
VD = re.compile(r'判定[:：]\s*(' + '|'.join(VERDICTS) + ')')
IDIN = re.compile(r'(AUD\d{3}-\d{2,3}|AUD403-\d{2}|A-\d{2}|[A-Z]+-\d{2}|B\d+|W\d+|SEC|[^ ]+::[^ ]+)')

FILES_FOR_BATCH = {'V401-A': 'V401-A.txt', 'V401-B': 'V401-B.txt', 'V402': 'V402.txt',
                   'V403': 'V403.txt', 'V404-S': 'V404-S.txt', 'V404-C': 'V404-C.txt',
                   'V501-A': 'V501-A.txt', 'V501-B': 'V501-B.txt'}
# V404.txt 已由 V404-S/V404-C 两批接替（原批吞 125 行 CSV 未产出），计入会双计，故不入台账


def expected_ids(batch_file: Path):
    ids = []
    for line in batch_file.read_text(encoding='utf-8').splitlines():
        if line.startswith('#') or not line.strip():
            continue
        ids.append(line.split('\t')[0].strip())
    return ids


def parse(path: Path):
    rows = []
    cur_head = None
    for ln, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        m = re.match(r'^##\s+(.{0,140})$', line)
        if m:
            cur_head = m.group(1).strip()
        v = VD.search(line)
        if v and (cur_head or '判定' in line):
            key = None
            if cur_head:
                mi = IDIN.match(cur_head)
                key = mi.group(1) if mi else cur_head.split('·')[0].strip()[:40]
            rows.append((key or '(无标题)', v.group(1), ln, (cur_head or '')[:90]))
            cur_head_ = cur_head
    merged = []
    seen = {}
    for key, vd, ln, head in rows:
        if key in seen:
            continue
        seen[key] = True
        merged.append((key, vd, ln, head))
    return merged


def collect():
    ledger = []
    per_batch = {}
    for tag, bf in FILES_FOR_BATCH.items():
        rev = RAW / ('复核-%s.md' % tag)
        bfp = INVENTORY_DIR / bf
        if not bfp.exists():
            continue
        exp = expected_ids(bfp)
        got = parse(rev) if rev.exists() else []
        gmap = {k: (v, ln) for k, v, ln, h in got}
        done = miss = unmatched = 0
        for e in exp:
            hit = None
            for k, val in gmap.items():
                if e == k or e.split('::')[0] in k or k.startswith(e[:12]):
                    hit = (k, val)
                    break
            if hit:
                vd = hit[1][0]
                ledger.append([bf, e, hit[0], vd, hit[1][1], ''])
                if vd in ('确认', '降级'):
                    done += 1
                elif vd in ('推翻', '待证', '重复'):
                    done += 1
                else:
                    unmatched += 1
            else:
                miss += 1
                ledger.append([bf, e, '', '未判', '', '缺口'])
        extra = [k for k in gmap if not any(k.startswith(e[:12]) or e.split('::')[0] in k for e in exp)]
        per_batch[bf] = dict(expected=len(exp), judged=sum(1 for e in exp
                            if any((k == e or e.split('::')[0] in k or k.startswith(e[:12]))
                                   for k in gmap)), extra=extra)
    return ledger, per_batch


SELF = [
    ('## AUD401-002 · 阶段名出现在代码目录 —— 判定：确认', 'AUD401-002', '确认'),
    ('## A-08 两条 runner 判定分叉\n**判定：降级（S2）**', 'A-08', '降级'),
    ('## 无关小节标题\n**判定：待证**', None, '待证'),
    ('判定：未知档', None, None),
]


def selftest():
    bad = 0
    tmp = Path(r'独立审计/复算件/verify_collect_selftest.md')
    for i, (body, want_id, want_vd) in enumerate(SELF):
        tmp.write_text(body + '\n', encoding='utf-8')
        got = parse(tmp)
        if want_vd is None:
            if got and want_id is None and got[0][1] not in VERDICTS:
                bad += 1
                print('  SELF FAIL(不该收的行被收):', got)
            continue
        if not got:
            bad += 1
            print('  SELF FAIL(漏收):', body[:40])
            continue
        k, vd = got[0][0], got[0][1]
        if vd != want_vd:
            bad += 1
            print('  SELF FAIL(档位): %s != %s' % (vd, want_vd))
        if want_id and want_id not in k:
            bad += 1
            print('  SELF FAIL(键): %r 不含 %r' % (k, want_id))
    tmp.unlink(missing_ok=True)
    print('selftest: %d 例，失败 %d' % (len(SELF), bad))
    return 1 if bad else 0


if '--self-test' in sys.argv:
    sys.exit(selftest())

OUT.mkdir(exist_ok=True)
led, per = collect()
with io.open(OUT / '第②层判定台账.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.writer(f)
    w.writerow(['批次清单', '条目', '复核件里的键', '判定', '行号', '备注'])
    for r in led:
        w.writerow(r)
tot = len(led)
jd = sum(1 for r in led if r[3] != '未判')
print('条目总数 %d，已判 %d，未判 %d' % (tot, jd, tot - jd))
for bf, s in sorted(per.items()):
    print('   %-14s 应判 %3d  已判 %3d  额外键 %s' % (bf, s['expected'], s['judged'],
                                                    (len(s['extra']) and s['extra'][:3]) or '无'))
dist = {}
for r in led:
    dist[r[3]] = dist.get(r[3], 0) + 1
print('判定分布：', dist)
print('可进 D6／负责人清单（确认＋降级）= %d；作废留痕（推翻）= %d；待证 = %d；未判 = %d'
      % (dist.get('确认', 0) + dist.get('降级', 0), dist.get('推翻', 0), dist.get('待证', 0), dist.get('未判', 0)))
(BASE / '复算' / 'xref' / 'verify_counts.json').write_text(
    json.dumps({'total': tot, 'judged': jd, 'dist': dist, 'per': {k: [v['expected'], v['judged']]
                for k, v in per.items()}}, ensure_ascii=False), encoding='utf-8')
