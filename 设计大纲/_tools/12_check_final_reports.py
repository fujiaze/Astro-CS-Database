import json, re, sys, os, glob
DD='/workspace/Astro CS Database/设计大纲'
BT=chr(96)
idx={}
for l in open(os.path.join(DD,'_evidence/commits/index.jsonl'),encoding='utf-8'):
    d=json.loads(l); idx[d['seq']]=d['sha'][:8]
def audit(p):
    t=open(p,encoding='utf-8').read()
    cn=len(re.findall(chr(0x4e00)+'-'+chr(0x9fff), t))
    cn=len([c for c in t if chr(0x4e00)<=c<=chr(0x9fff)])
    fences=t.count(BT*3)
    pairs=re.findall(r'(?:C|seq\s*)(\d{1,4})\s*(?:\||｜)\s*([0-9a-f]{7,8})', t)
    bad=[]; seen=set()
    for s,sha in pairs:
        si=int(s)
        if si in idx and (idx[si].startswith(sha) or sha.startswith(idx[si])): seen.add(si)
        else: bad.append((si,sha,idx.get(si,'?')))
    bare=set(int(m) for m in re.findall(r'C(\d{1,4})(?![0-9])', t) if int(m) in idx)
    sec=re.split(r'(?m)^#{1,2} ', t)
    heads=re.findall(r'(?m)^#{1,2} (.{0,40})', t)
    lens=[len([c for c in b if chr(0x4e00)<=c<=chr(0x9fff)]) for b in sec[1:]]
    bullets=len(re.findall(r'(?m)^\s*[-*] ', t))
    tbl=len(re.findall(r'(?m)^\|', t))
    print('%s' % os.path.basename(p))
    print('  中文字 %d | 代码围栏 %d | 列表行 %d | 表格行 %d' % (cn, fences, bullets, tbl))
    print('  指针 seq|sha 共 %d 处（其中异常 %d）；独立 seq %d 个 / 全集 %d' % (len(pairs), len(bad), len(seen|bare), len(idx)))
    if bad: print('  异常样本:', bad[:6])
    print('  节数 %d；各节中文字: %s' % (len(lens), lens))
    if lens and max(lens)>0:
        print('  最长/最短节: %d / %d；<300 字的节数: %d' % (max(lens), min(lens), sum(1 for x in lens if x<300)))
tgts=sys.argv[1:] or sorted(glob.glob(os.path.join(DD,'大报告_*.md')))
if not tgts: print('未找到大报告文件')
for p in tgts: audit(p)
