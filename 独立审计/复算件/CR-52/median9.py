import sys, itertools
sys.stdout.reconfigure(encoding='utf-8')
# 逐字转录 sdet_image.cpp:302-310 的比较-交换序列
PAIRS = [(1,2),(4,5),(7,8), (0,1),(3,4),(6,7), (1,2),(4,5),(7,8),
         (0,3),(5,8),(4,7), (3,6),(1,4),(2,5), (4,7),(4,2),(6,4), (4,2)]
def run(p):
    p = list(p)
    for a,b in PAIRS:
        if p[a] > p[b]:
            p[a], p[b] = p[b], p[a]
    return p
bad = 0; first = None
for perm in itertools.permutations(range(9)):
    out = run(perm)
    if out[4] != 4:
        bad += 1
        if first is None: first = (perm, out[4])
print("SW 序列数:", len(PAIRS))
print("全 9! = 362880 种互异输入中 p[4] != 真中位 的个数:", bad)
if first: print("首个反例 输入/实际p[4]:", first)
# 附加：与经典 Paulos/Smith median-of-9 序列对照
CLASSIC = [(1,2),(4,5),(7,8),(0,1),(3,4),(6,7),(1,2),(4,5),(7,8),
           (3,6),(4,7),(5,8),(4,7),(2,5),(4,2),(6,4),(4,2)]
def run2(p):
    p=list(p)
    for a,b in CLASSIC:
        if p[a]>p[b]: p[a],p[b]=p[b],p[a]
    return p
bad2=sum(1 for perm in itertools.permutations(range(9)) if run2(perm)[4]!=4)
print("经典 17-SW 序列反例数:", bad2)
