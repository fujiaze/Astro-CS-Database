import math
FWHM=1.407
for beta in (2.5,4.0):
    alpha=FWHM/(2.0*math.sqrt(2.0**(1.0/beta)-1.0))
    ana=math.pi*alpha*alpha/(beta-1.0)
    for off in [(0.0,0.0),(0.5,0.5)]:
        ox,oy=off; S=0.0
        h=8000 if beta==2.5 else 400
        # radial sum for speed: iterate square grid
        R=int(20000 if beta==2.5 else 400)
        S=0.0
        for j in range(-R,R+1):
            for i in range(-R,R+1):
                r2=(i-ox)**2+(j-oy)**2
                S+=(1.0+r2/(alpha*alpha))**(-beta)
        # tail of analytic beyond grid negligible for R*alpha? add remainder
        rem=math.pi*alpha*alpha/(beta-1.0)*(1.0+(R*R)/(alpha*alpha))**(1.0-beta) if True else 0
        Sn=S+rem
        print("beta=%.1f FWHM=%.3f alpha=%.5f analytic=%.5f discrete(+tail)=%.5f ratio=%.5f err=%.2f%%"%(beta,FWHM,alpha,ana,Sn,Sn/ana,(Sn/ana-1)*100))
