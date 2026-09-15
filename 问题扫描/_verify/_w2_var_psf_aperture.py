import math

FWHM = 1.407
sig = FWHM / 1.230310

def prof(i,j,ox,oy,s2,beta):
    r2=(i-ox)**2+(j-oy)**2
    return (1.0+r2/(2.0*s2))**(-beta)

def ds_sum(ox,oy,beta,half=400):
    S=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            S+=prof(i,j,ox,oy,sig*sig,beta)
    return S

ana4=2.0*math.pi*sig*sig/3.0
for off in [(0.0,0.0),(0.5,0.0),(0.5,0.5)]:
    d=ds_sum(off[0],off[1],4.0)
    print("S1 Moffat4 off=%s discrete=%.6f analytic=%.6f ratio=%.6f err=%.3f%%"%(off,d,ana4,d/ana4,(d/ana4-1)*100))

beta=2.5
ana25=2.0*math.pi*sig*sig/(2.0*(beta-1.0))
for off in [(0.0,0.0),(0.5,0.5)]:
    d=ds_sum(off[0],off[1],beta)
    print("S1b Moffat2.5 off=%s discrete=%.4f analytic=%.4f ratio=%.6f err=%.2f%%"%(off,d,ana25,d/ana25,(d/ana25-1)*100))

s=ds_sum(0.0,0.0,4.0)
print("S1c P_center discrete=%.5f analytic=%.5f ratio=%.4f"%(1.0/s, 1.0/ana4, ana4/s))

def ds_sumsq(ox,oy,beta,half=400):
    S=0.0;S2=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            v=prof(i,j,ox,oy,sig*sig,beta); S+=v; S2+=v*v
    return S,S2
S,S2=ds_sumsq(0,0,4.0)
sum_p2_disc=S2/(S*S)
# analytic sum_p2 analogue: int I^2 / (int I)^2
# int (1+r^2/2s2)^-8 dA = 2 pi s2 * int (1+Q)^-8 dQ = 2 pi s2 /7
ana_sp2=(2.0*math.pi*sig*sig/7.0)/ana4**2
print("S1d sum_p2 discrete=%.6f analytic=%.6f ratio(disc/ana)=%.4f -> Var(F) ratio=%.4f (%.1f%%)"%(sum_p2_disc,ana_sp2,sum_p2_disc/ana_sp2,ana_sp2/sum_p2_disc,(ana_sp2/sum_p2_disc-1)*100))

# ---------- S2: aperture area / enclosed fraction ----------
r_ap=1.5*FWHM
n_cont=math.pi*r_ap*r_ap
def npix_and_ffrac(ox,oy):
    # whole-pixel aperture count around center (ox,oy) on integer grid
    cnt=0; s_in=0.0
    S=ds_sum(ox,oy,4.0,half=50)
    for j in range(-int(r_ap)-2, int(r_ap)+3):
        for i in range(-int(r_ap)-2, int(r_ap)+3):
            d2=(i-ox)**2+(j-oy)**2
            if d2<=r_ap*r_ap:
                cnt+=1; s_in+=prof(i,j,ox,oy,sig*sig,4.0)
    return cnt, s_in/S
f_cont=1.0-(1.0+(r_ap*r_ap)/(2.0*sig*sig))**-3
print("S2 r=%.4f px: pi r^2=%.3f"%(r_ap,n_cont))
for off in [(0.0,0.0),(0.5,0.0),(0.5,0.5)]:
    cnt,fd=npix_and_ffrac(off[0],off[1])
    print("S2 off=%s whole-pixel npix=%d (vs %.3f, %.1f%%) ; f_in discrete=%.5f vs continuous=%.5f  bias=%.2f%%"%(off,cnt,n_cont,(cnt/n_cont-1)*100,fd,f_cont,(f_cont/fd-1)*100))

# ---------- S3: photometer CCD equation ----------
def phot_err(sum_adu, n_in, sky_sigma):
    return math.sqrt(max(sum_adu,0.0)+n_in*sky_sigma*sky_sigma)
F=1000.0; gain=2.0; sky=10.0; n_in=13; n_sky=50.0
code=phot_err(F,n_in,sky)
correct=math.sqrt(F/gain + n_in*sky*sky*(1.0+n_in/n_sky))
print("S3 photometer: code sigma_F=%.2f vs gain-aware+sky-mean sigma_F=%.2f  (var ratio %.3f)"%(code,correct,(code/correct)**2))
# pure Poisson part only
print("S3b Poisson part: code adds %g ADU^2 vs correct %g ADU^2 (gain=2): over by %.0f%%"%(F,F/gain,(F/(F/gain)-1)*100))
