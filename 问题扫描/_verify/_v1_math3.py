import math
K=1.230310
def sp2(s,half):
    a2=2*s*s; tot=0.0; q=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            v=1.0/(1.0+(i*i+j*j)/a2)**4; tot+=v; q+=v*v
    return q/(tot*tot)
def sigmaF(F,s_sky,fwhm,gain=0.0,rn=0.0):
    s=fwhm/K; half=max(30,math.ceil(12*fwhm))
    a2=2*s*s; vals=[]; tot=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            v=1.0/(1.0+(i*i+j*j)/a2)**4; vals.append(v); tot+=v
    acc=0.0
    for v in vals:
        P=v/tot; var=s_sky*s_sky
        if gain>0:
            var+=(rn/gain)**2
            if F*P>0: var+=F*P/gain
        acc+=P*P/var
    return 1.0/math.sqrt(acc)
print('=== (3) 5-sigma depth: code F5=5*sigma_F(F_ref) vs self-consistent F=5*sigma_F(F) ===')
sky=260.8590110604006; fwhm=3.0; zp=22.0
for gain,Fref in [(0.0,1e4),(1.5,1e4),(1.5,1e5),(1.5,2984.19)]:
    sFref=sigmaF(Fref,sky,fwhm,gain)
    F5=5*sFref
    # self-consistent solve
    lo,hi=1.0,1e9
    for _ in range(200):
        mid=math.sqrt(lo*hi)
        if mid < 5*sigmaF(mid,sky,fwhm,gain): lo=mid
        else: hi=mid
    Fstar=math.sqrt(lo*hi)
    snr_at_F5=F5/sigmaF(F5,sky,fwhm,gain) if gain>0 else 5.0
    print(f'  gain={gain} F_ref={Fref:9.1f}: code F5={F5:11.2f} (m5={zp-2.5*math.log10(F5):6.3f}) | self-consistent F*={Fstar:11.2f} (m5*={zp-2.5*math.log10(Fstar):6.3f}) | bias={2.5*math.log10(F5/Fstar):+6.3f} mag | true SNR at code F5={snr_at_F5:6.2f}')
print()
print('=== (4) read-noise double counting: sigma_i^2 = sigma_sky(empirical)^2 + (RN/g)^2 + ... ===')
for g,rn,sk in [(2.0,5.0,10.0),(2.0,5.0,3.0),(1.5,10.0,260.859),(1.5,10.0,20.0)]:
    add=(rn/g)**2; print(f'  g={g} RN={rn}e- sigma_sky={sk}: extra var={add:.4f} ADU^2 vs empirical {sk*sk:.1f} -> inflation {add/(sk*sk)*100:6.3f}% -> SNR low by {(1/math.sqrt(1+add/(sk*sk))-1)*100:+6.3f}%')
print()
print('=== (5) zero-point SE constant: median 1.2533 vs Tukey-bisquare IRLS (c=4.685) ===')
def gauss_moments(c, n=400001, R=10.0):
    e2=0.0; ep=0.0; dx=2*R/(n-1)
    for k in range(n):
        u=-R+k*dx; pdf=math.exp(-0.5*u*u)/math.sqrt(2*math.pi)
        if abs(u)<c:
            t=(u/c)**2; psi=u*(1-t)**2; dpsi=(1-t)*(1-3*t)
        else:
            psi=0.0; dpsi=0.0
        e2+=psi*psi*pdf*dx; ep+=dpsi*pdf*dx
    return math.sqrt(e2)/abs(ep)
for c in [4.685, 4.0, 6.0]:
    m=gauss_moments(c, 200001, 8.0)
    print(f'  Tukey c={c}: SE multiplier = {m:.4f} (median={math.sqrt(math.pi/2):.4f}, mean=1.0000) -> code 1.253 overstates by {(1.253/m-1)*100:5.1f}%')