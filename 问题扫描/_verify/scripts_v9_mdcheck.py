import re
lines=open('问题扫描/_verify/V9.md',encoding='utf-8').read().split(chr(10))
def cols(l): return len(re.split(r'(?<!\\)\|', l))-1
cur=None
for i,l in enumerate(lines,1):
    if l.startswith('|'):
        c=cols(l)
        if cur is None: cur=c; print('line %d table start, cols=%d'%(i,c))
        elif c!=cur: print('  !! line %d cols=%d (expected %d): %s' % (i,c,cur,l[:70]))
    else:
        cur=None
print('checked')
print('total lines', len(lines))
