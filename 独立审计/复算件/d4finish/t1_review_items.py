# -*- coding: utf-8 -*-
"""Task 1.1 ② refined —— curated layer-2 (复核) entry list, taken from the item
headings of 复核-AUD201/202/202-补/203/204, mapped to the constant / tolerance /
threshold / power each item adjudicates, then matched against master rows.

The candidate patterns are copied verbatim from the review documents' own item
titles and 判定 lines (see t1_review_items.tsv for the source line of each);
this script only performs membership.
"""
import io, re, sys, csv, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
RAW = ROOT + r'\raw'
OUT = ROOT + r'\复算\d4finish'

# item -> (needle regexes tested against master row text)
ITEMS = collections.OrderedDict([
 ('AUD201-V1 通量式星等因子',    [r'flux_magnitude|mag0|星等因子|通量式']),
 ('AUD201-V2 λ 色项',           [r'lambda|色项|color']),
 ('AUD201-V3 锚分箱轴',          [r'分箱|bin']),
 ('AUD201-V4 出处登记为死键',    [r'出处|source_ref|dead.?key|死键']),
 ('AUD201-V5 sigma_residual',   [r'sigma_residual']),
 ('AUD201-V6 质心 p95 容差',     [r'p95|centroid|质心']),
 ('AUD202-V1 权重信号维退化',    [r'weight_mode|权重模式']),
 ('AUD202-V2 稀疏层对象',        [r'sparse|稀疏']),
 ('AUD202-V3 两篇 FROZEN 互斥',  [r'snr|FROZEN']),
 ('AUD202-V4 snr_path 死键',     [r'snr_path']),
 ('AUD202-V5 量纲/插值判据',     [r'bilinear_upsample|interpolat|量纲']),
 ('AUD202-V6 头条单次实现值',    [r'headline|头条|41%|347']),
 ('AUD203-V1 控制点权重不归零',  [r'control_ivar|civar|控制点']),
 ('AUD203-V2 接缝门统计量',      [r'seam|接缝']),
 ('AUD203-V3 additive_mode',     [r'additive_mode']),
 ('AUD203-V4 tolerance_relative', [r'tolerance_relative']),
 ('AUD203-V5 stalled=2',         [r'stalled']),
 ('AUD204-W1 1e-15 面积闭合',    [r'1e-15|闭合|closure']),
 ('AUD204-W2 折线缝精度平台',    [r'1e-6|折线|polyline|缝平台']),
 ('AUD204-W3 hips_profile',      [r'hips_profile']),
 ('AUD204-W4 pixfrac parity 门', [r'pixfrac']),
])

rows = [r for r in csv.reader(io.open(MASTER, encoding='utf-8-sig', newline=''))][1:]
rows = [r for r in rows if len(r) >= 16]
blobs = [(' | '.join(r)).lower() for r in rows]

hit = collections.Counter()
report = []
for item, pats in ITEMS.items():
    lines = []
    for pat in pats:
        rx = re.compile(pat, re.I)
        idxs = [i for i, b in enumerate(blobs) if rx.search(b)]
        lines.append((pat, len(idxs), [rows[i][0][:44] for i in idxs[:3]]))
        hit[item] += len(idxs)
    report.append((item, lines))

for item, lines in report:
    tot = sum(n for _, n, _ in lines)
    flag = 'MISS ' if tot == 0 else 'ok   '
    print('%s %-28s hits=%-4d %s' % (flag, item, tot,
          ' / '.join('%s=%d' % (p[:22], n) for p, n, _ in lines)))
    for p, n, ex in lines:
        if n == 0:
            print('        ^ no master row matches /%s/' % p)

print('\nitems with zero master hits =',
      sum(1 for item, lines in report if sum(n for _, n, _ in lines) == 0))
