import glob, re, os
n=0
for p in glob.glob('/workspace/Astro CS Database/设计大纲/reports/history/slices/H-S*.md'):
    t=open(p,encoding='utf-8').read()
    o=t
    t=re.sub(r'(?m)^(---)\s*\n\s*\n---\s*\n', '---\n', t)
    t=re.sub(chr(10)+'{3,}', chr(10)*2, t)
    if t!=o:
        open(p,'w',encoding='utf-8').write(t); n+=1
print('清理排版残留', n, '份')
