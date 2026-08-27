
import sys, math
sys.path.insert(0, '/home/lighthouse/astrocs_audit_v2/scripts')
import hpx
# known field centers
pts = [(272.8257,-13.13),(272.8,-18.2),(272.8,-23.3),(272.8,-15.67),(272.8,-20.75)]
for (ra,dec) in pts:
    for K in [5,6,7]:
        nside=512*(1<<K)
        leaf=hpx.ang2pix_nest(ra,dec,nside)
        parent=leaf>>18
        local=leaf & ((1<<18)-1)
        x,y=hpx._nest_to_xy(local,9)
        ra2,dec2=hpx.tile_pix2ang(K,parent,x,y)
        err=math.sqrt(((ra2-ra+180)%360-180)**2+(dec2-dec)**2) if 'math' in dir() else 0
        import math
        err=math.sqrt(((ra2-ra+180)%360-180)**2+(dec2-dec)**2)
        ok='OK' if err<0.001 else 'BAD'
        print('ra=%.3f dec=%.3f K=%d parent=%d x=%d y=%d -> (%.4f,%.4f) err=%.5f %s' % (ra,dec,K,parent,x,y,ra2,dec2,err,ok))
