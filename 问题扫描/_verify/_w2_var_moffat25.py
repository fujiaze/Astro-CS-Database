import math
import numpy as np
FWHM=1.407
def dsquare(ox,oy,beta,alpha,R):
    i=np.arange(-R,R+1,dtype=np.float64)
    S=0.0
    ii,J=np.meshgrid(i,i,indexing='ij')
    r2=(ii-ox)**2+(J-oy)**2
    return np.sum((1.0+r2/(alpha*alpha))**(-beta))
for beta,R in ((2.5,3000),(4.0,400)):
    alpha=FWHM/(2.0*math.sqrt(2.0**(1.0/beta)-1.0))
    ana=math.pi*alpha*alpha/(beta-1.0)
    for off in [(0.0,0.0),(0.5,0.5)]:
        S=dsquare(off[0],off[1],beta,alpha,R)
        rem=math.pi*alpha*alpha/(beta-1.0)*(1.0+(R*R)/(alpha*alpha))**(1.0-beta)
        Sn=S+rem
        print("beta=%.1f alpha=%.5f analytic=%.5f discrete(R=%d+tail)=%.5f ratio=%.5f err=%.3f%%"%(beta,alpha,ana,R,Sn,Sn/ana,(Sn/ana-1)*100))
