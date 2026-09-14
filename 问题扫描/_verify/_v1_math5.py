import math
K=1.230310
def sum_p2_ellip(sx,sy,half=60,nsub=7):
    tot=0.0;q=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            acc=0.0
            for b in range(nsub):
                yy=(b+0.5)/nsub-0.5+j
                for aa in range(nsub):
                    xx=(aa+0.5)/nsub-0.5+i
                    acc+=1.0/(1.0+(xx*xx/(sx*sx)+yy*yy/(sy*sy))/2.0)**4
            acc/=(nsub*nsub); tot+=acc; q+=acc*acc
    return q/(tot*tot)
def sum_p2_circ(s,fwhm,half):
    a2=2*s*s; tot=0.0;q=0.0
    for j in range(-half,half+1):
        for i in range(-half,half+1):
            v=1.0/(1.0+(i*i+j*j)/a2)**4; tot+=v; q+=v*v
    return q/(tot*tot)
print('各向同性假设的偏差 (生产用单一 fwhm_px = 长短轴均值, 见 s.fwhm_px=2.3548*0.5*(a+b))')
print(' axis_ratio  sigma_m   snr_iso/snr_ellipse')
for ratio in [1.0,0.9,0.8,0.7,0.5]:
    sy=2.0; sx=sy/ratio   # sx>=sy, 椭率 e=1-sy/sx
    fwhm_a=K*sx; fwhm_b=K*sy; fwhm_mean=0.5*(fwhm_a+fwhm_b); s_mean=fwhm_mean/K
    p2e=sum_p2_ellip(sx,sy,45,5)
    p2c=sum_p2_circ(s_mean, fwhm_mean, 45)
    print(f'  {1/ratio:5.2f}      {s_mean:6.3f}   {math.sqrt(p2c/p2e):8.4f}   (SNR bias {(math.sqrt(p2c/p2e)-1)*100:+6.2f}%)')