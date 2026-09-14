import math
K=1.230310
def sig(f): return f/K
def prof(x,y,a2): return 1.0/(1.0+(x*x+y*y)/a2)**4
def sum_p2_point(s, half):
    a2=2*s*s; tot=0.0; q=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            v=prof(i,j,a2); tot+=v; q+=v*v
    return q/(tot*tot)
def sum_p2_pixint(s, half, nsub=15):
    a2=2*s*s; tot=0.0; q=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            acc=0.0
            for b in range(nsub):
                yy=(b+0.5)/nsub-0.5+j
                for aa in range(nsub):
                    acc+=prof((aa+0.5)/nsub-0.5+i, yy, a2)
            acc/=(nsub*nsub)
            tot+=acc; q+=acc*acc
    return q/(tot*tot)
print('FWHM  half  sumP2_point  sumP2_pixint  SNR_point/SNR_true  dm5_mag')
for f in [1.4072,1.5,2.0,2.5,2.9622,3.4166,4.0,5.0]:
    s=sig(f); half=max(30, math.ceil(12*f))
    hp = min(half, 45)
    p2p=sum_p2_point(s,half); p2i=sum_p2_pixint(s,hp,15)
    r=math.sqrt(p2p/p2i)
    print(f'{f:7.3f} {half:4d} {p2p:.8f} {p2i:.8f} {r:8.4f} {2.5*math.log10(r):+8.4f}')
print()
print('--- 连续极限解析校验: sum P^2 -> int P^2 dA = 9/(14 pi s^2) (Moffat beta=4, alpha^2=2 s^2) ---')
for f in [2.5,4.0]:
    s=sig(f); print(f'  FWHM={f}: analytic={9/(14*math.pi*s*s):.8f}  point={sum_p2_point(s,max(30,math.ceil(12*f))):.8f}')