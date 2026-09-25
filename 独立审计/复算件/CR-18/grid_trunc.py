import sys, math
sys.stdout.reconfigure(encoding='utf-8')
GAUSS=2.3548200450309493; MOFF=1.230310
def sum_p2(sigma,H):
    a2=2*sigma*sigma; s=0.0; s2=0.0; cen=0.0
    for j in range(-H,H+1):
        for i in range(-H,H+1):
            t=1.0+(i*i+j*j)/a2; v=1.0/(t*t*t*t)
            s+=v; s2+=v*v
            if i==0 and j==0: cen=v
    return s2/(s*s), cen/s
def autoHalf_no256(fw): return max(30, math.ceil(12*fw))
print("fwhm_det sigma fwhm_eff H_used H_nocap sum_p2(H_used) sum_p2(H_nocap) snr_ratio sqrt(ratio)")
for fwhm_det in [2.0,4.5,8.0,20.0,40.0,60.0,80.0,120.0,260.0]:
    sigma=fwhm_det/GAUSS; fe=sigma*MOFF
    Hu=min(256,autoHalf_no256(fe)); Hn=autoHalf_no256(fe)
    su,_=sum_p2(sigma,Hu); sn,_=sum_p2(sigma,Hn)
    print(f"{fwhm_det:7.1f} {sigma:7.3f} {fe:7.3f} {Hu:5d} {Hn:5d}  {su:.8e} {sn:.8e}  {su/sn:.6f} {math.sqrt(su/sn):.6f}")
