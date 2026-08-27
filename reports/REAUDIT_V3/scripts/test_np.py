
import sys, numpy as np
sys.path.insert(0,'/home/lighthouse/astrocs_audit_v2/scripts')
import hpx, hpx_np
for K in [0,1,2,3,4,5,6,7]:
    parent = 7* (4**K) + 3
    xs = np.array([0,128,300,511]); ys=np.array([0,255,200,511])
    ra_np,dec_np = hpx_np.tile_pix2ang(K,parent,xs,ys)
    for j in range(4):
        ra_s,dec_s = hpx.tile_pix2ang(K,parent,int(xs[j]),int(ys[j]))
        e=((ra_np[j]-ra_s+180)%360-180)**2+(dec_np[j]-dec_s)**2
        print('K=%d px=(%d,%d) np=(%.5f,%.5f) sc=(%.5f,%.5f) e=%.2e'%(K,xs[j],ys[j],ra_np[j],dec_np[j],ra_s,dec_s,e))
