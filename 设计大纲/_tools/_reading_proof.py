import glob, re, os, json
DD='/workspace/Astro CS Database/设计大纲'
sl=sorted(glob.glob(os.path.join(DD,'reports/history/slices/H-S*.md')))
st=sorted(glob.glob(os.path.join(DD,'reports/history/STAGE-*.md'))+glob.glob(os.path.join(DD,'reports/packs/STAGE-*.md')))
ov=[os.path.join(DD,'reports/history/00_OVERVIEW.md'), os.path.join(DD,'reports/packs/00_OVERVIEW.md')]
def rd(p): return open(p,encoding='utf-8').read()
txt_sl=''.join(rd(p) for p in sl); txt_st=''.join(rd(p) for p in st); txt_ov=''.join(rd(p) for p in ov)
def han(t): return len(re.findall(chr(0x4e00)+'-'+chr(0x9fff), t))
print('分片层 %d 份：%d 字（中文），条目组 %d 个' % (len(sl), sum(t.count(chr(0x0A)) for t in [txt_sl]), txt_sl.count(chr(35)+chr(35)+chr(35)+' seq ')))
print('阶段层 %d 份：%d 行' % (len(st), sum(x.count(chr(10)) for x in [txt_st])))
print('两份总览：%d 行' % sum(x.count(chr(10)) for x in [txt_ov]))
print()
for name,t in (('分片层',txt_sl),('阶段层',txt_st),('总览',txt_ov)):
    print('%s: 未证实=%d 自述标记=%d 变更面证据=%d 交叉引用seq=%d 数字指针seq=%d' % (
        name, t.count('未证实'), t.count('自述'), t.count('变更面'), len(re.findall(r'seq \d+', t)), len(set(re.findall(r'seq (\d+)', t)))))
