import glob, re, os, json
DD='/workspace/Astro CS Database/设计大纲'
sl=glob.glob(os.path.join(DD,'reports/history/slices/H-S*.md'))
cards=glob.glob(os.path.join(DD,'_evidence/commits/cards/*.md'))+glob.glob(os.path.join(DD,'_evidence/commits/compact/*.md'))
packs=glob.glob(os.path.join(DD,'reports/packs/P-G*.md'))
pd=glob.glob(os.path.join(DD,'_evidence/packs/digest/*.md'))
sz=lambda fs: sum(os.path.getsize(f) for f in fs)
print('提交证据底座(全卡+紧凑卡) %.1f MB → 历史分片报告 %.2f MB  （压缩 %d:1）' % (sz(cards)/1048576, sz(sl)/1048576, sz(cards)/max(1,sz(sl))))
print('包证据底座(digest+inventory+instances) %.1f MB → 包组报告 %.2f MB （压缩 %d:1）' % ((sz(pd)+os.path.getsize(os.path.join(DD,'_evidence/packs/pack_instances.json')))/1048576, sz(packs)/1048576, (sz(pd))/max(1,sz(packs))))
t=''.join(open(p,encoding='utf-8').read() for p in sl)
pat={'消息未提及':0,'自述与变更面交集':0,'与自述一致':0,'变更面证据':0,'矛盾':0,'不成立':0,'无产物可核':0,'未证实':0}
for k in pat: pat[k]=t.count(k)
print('分片层判断性用语频次:', pat)
import statistics
ls=[len(open(p,encoding='utf-8').read()) for p in sl]
print('单片报告长度：中位 %d 字，最短 %d，最长 %d；条目组共 %d → 平均每条 %d 字' % (statistics.median(ls), min(ls), max(ls), t.count(chr(35)*3+' seq '), sum(ls)/max(1,t.count(chr(35)+chr(35)+chr(35)+' seq '))))
for p in sorted(glob.glob(os.path.join(DD,'reports/history/STAGE-*.md')))+sorted(glob.glob(os.path.join(DD,'reports/packs/STAGE-*.md')))+[os.path.join(DD,'reports/history/00_OVERVIEW.md'),os.path.join(DD,'reports/packs/00_OVERVIEW.md')]:
    s=open(p,encoding='utf-8').read(); print('  %-46s %5d 字  %3d 行  指针 %d' % (p.split('reports/')[-1], len(s), s.count(chr(10)), len(re.findall(r'seq \d+', s))+s.count('sha')))
