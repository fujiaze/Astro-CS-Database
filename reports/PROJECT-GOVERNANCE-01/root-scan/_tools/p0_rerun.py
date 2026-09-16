#!/usr/bin/env python3
"""ROOT-004 controller P0 recheck: extract 93 P0 rows from shards, re-run first evidence command per row, emit JSON for manual verdicts."""
import os, re, csv, json, subprocess, collections
ROOT = "/workspace/Astro CS Database"
SHD = os.path.join(ROOT,'reports/PROJECT-GOVERNANCE-01/root-scan/shards')
GEN = os.path.join(ROOT,'reports/PROJECT-GOVERNANCE-01/root-scan/_gen')
led = list(csv.DictReader(open(os.path.join(ROOT,'问题扫描','账本','FIX_LEDGER.csv'),encoding='utf-8-sig')))
P0 = [r['id'] for r in led if r['priority']=='P0']
assert len(P0)==93
HEADER="ID|原类别|原优先级|旧判据(文档+节号/路径)|最新权威条款|当前证据(命令+输出)|结论|归属|GAP关系|备注"
rows={}
for fn in sorted(os.listdir(SHD)):
    if not fn.endswith('.psv'): continue
    lines=[l.rstrip('\n') for l in open(os.path.join(SHD,fn),encoding='utf-8') if l.strip()]
    assert lines[0]==HEADER, (fn,"bad header")
    for l in lines[1:]:
        c=l.split('|')
        if len(c)!=10 or c[2]!='P0': continue
        rows[c[0].strip()]={'shard':fn[:-4],'old':c[3],'auth':c[4],'ev':c[5],'state':c[6],'attr':c[7],'gap':c[8],'note':c[9]}
missing=[i for i in P0 if i not in rows]
print("P0 rows found:", len(rows), "missing:", missing)
def extract_cmds(ev):
    m=re.search(r'命令：(.*)$', ev, re.S)
    body=m.group(1) if m else ev
    body=body.split('；输出：')[0].split('输出：')[0]
    # split on ';' but keep 'timeout' segments
    parts=re.split(r'(?=timeout d+)', body)
    cmds=[p.strip().strip('；;').strip() for p in parts if p.strip().startswith('timeout')]
    if not cmds:
        cmds=[body.strip()]
    return cmds
out=[]
for i in P0:
    r=rows.get(i)
    if not r: out.append({'id':i,'skip':'missing'}); continue
    cmds=extract_cmds(r['ev'])[:3]
    results=[]
    for c in cmds:
        try:
            p=subprocess.run(['bash','-c','cd "%s" && %s'%(ROOT,c)],capture_output=True,text=True,timeout=70)
            o=(p.stdout+p.stderr)
            results.append({'cmd':c[:200],'rc':p.returncode,'out':o[:400]})
        except Exception as e:
            results.append({'cmd':c[:200],'rc':'TIMEOUT/ERR','out':str(e)[:150]})
    # claimed output
    mo=re.search(r'输出：(.*)$', r['ev'], re.S)
    claim=mo.group(1) if mo else ''
    out.append({'id':i,'shard':r['shard'],'state':r['state'],'attr':r['attr'],'gap':r['gap'],'note':r['note'][:120],'claim':claim[:400],'rerun':results})
json.dump(out,open(os.path.join(GEN,'p0_rerun.json'),'w',encoding='utf-8'),ensure_ascii=False,indent=1)
st=collections.Counter(r['state'] for r in rows.values())
print('state dist:',dict(st))
print('wrote p0_rerun.json', len(out))

