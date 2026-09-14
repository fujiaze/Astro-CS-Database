import math
K=1.230310
def sig(f): return f/K
def prof(x,y,a2): return 1.0/(1.0+(x*x+y*y)/a2)**4
def sum_p2_point(s, half):
    a2=2*s*s; tot=0.0; q=0.0; cen=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            v=prof(i,j,a2); tot+=v; q+=v*v
            if i==0 and j==0: cen=v
    return q/(tot*tot), cen/tot
def pixel_integrated(s, half, nsub=9):
    # P_k = integral over pixel of I / total integral (subsample nsub^2)
    a2=2*s*s; off=(nsub+1)/2.0
    tot=0.0; q=0.0; weights=[]
    for j in range(-half,half+1):
        row=[]
        for i in range(-half,half+1):
            acc=0.0
            for b in range(1,nsub+1):
                for aa in range(1,nsub+1):
                    acc+=prof(aa-off+i, b-off+j, a2)
            acc/= (nsub*nsub)
            row.append(acc); tot+=acc; q+=acc*acc
        weights.append(row)
    return q/(tot*tot)
print('=== (1) Moffat4 beta=4: FWHM factor / analytic identities ===')
import sys
r_half=math.sqrt(2*(2**0.25-1)); print(' FWHM/sigma from half-max =', round(2*r_half,6), ' code const =', K)
s=2.0; a2=2*s*s
num=0.0
N=200000
for k in range(N):
    r=(k+0.5)*10.0/N
    num+= 2*math.pi*r*prof(r,0,a2)*(10.0/N)
print(' int I dA (sigma=2) =', round(num,8), ' 2*pi*s^2/3 =', round(2*math.pi*s*s/3,8))
for rf in [1.0,2.0,3.0]:
    fin=1-(1+rf*rf/(2*s*s))**-3
    num=0.0
    for k in range(200000):
        r=(k+0.5)*rf/200000
        num+=2*math.pi*r*prof(r,0,a2)*(rf/200000)
    print(f'  enclosed(r={rf}s) numeric={num/(2*math.pi*s*s/3):.9f} formula={fin:.9f}')
print()
print('=== (2) point-sampled vs pixel-integrated sum(P^2) -> sigma_F / SNR bias ===')
for f in [1.4072320071088888, 1.5, 2.5, 2.9622233810299274, 3.0339454080028325, 3.4166259417890283, 4.0, 5.0]:
    s=sig(f); half=max(30, math.ceil(12*f))
    p2p,_=sum_p2_point(s,half)
    p2i=pixel_integrated(s, min(half,60), 9)
    ratio=math.sqrt(p2i/p2p)   # sigma_F_int/sigma_F_point = sqrt(p2p/p2i)... SNR_int/SNR_point = sqrt(p2i/p2p)
    print(f'  FWHM={f:6.3f} half={half} sumP2_point={p2p:.8f} sumP2_pixint={p2i:.8f} SNR_point/SNR_pixint={math.sqrt(p2p/p2i):.6f}  dm5={2.5*math.log10(math.sqrt(p2p/p2i)):+.5f} mag')