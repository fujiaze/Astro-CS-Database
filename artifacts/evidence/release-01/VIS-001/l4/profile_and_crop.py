import sys, numpy as np
sys.path.insert(0,'run/RELEASE-01/e2e')
from fits_probe import read_fits
from PIL import Image
import os
os.makedirs('run/RELEASE-01/vis/l4/crops',exist_ok=True)
def profile(p,label):
    h,d,a=read_fits(p)
    a=a.reshape(d[1],d[0])
    med=np.median(a,axis=1)
    dif=np.diff(med)
    scale=np.median(np.abs(dif))+1e-30
    jumps=np.abs(dif)/scale
    idx=np.argsort(jumps)[-8:]
    print('%s: row-median p99-p1=%.4g ; top jumps (row,ratio): %s'%(label,np.percentile(med,99)-np.percentile(med,1),
          sorted((int(i),round(float(jumps[i]),1)) for i in idx)))
    return d
for g in ('m42','gc'):
    p='run/RELEASE-01/e2e/l4/p3_%s_vis/output_phase3.fits'%g
    profile(p,g)
# crops around seams (1:1) from the stretched PNG
def crop(png,box,out):
    im=Image.open(png); im.crop(box).save(out); print('crop',out,box)
crop('run/RELEASE-01/vis/l4/m42_full.png',(1200,1150,2200,1350),'run/RELEASE-01/vis/l4/crops/m42_seam1.png')
crop('run/RELEASE-01/vis/l4/m42_full.png',(1500,200,2300,700),'run/RELEASE-01/vis/l4/crops/m42_upper.png')
crop('run/RELEASE-01/vis/l4/gc_full.png',(1400,500,2400,800),'run/RELEASE-01/vis/l4/crops/gc_seam1.png')
crop('run/RELEASE-01/vis/l4/gc_full.png',(1400,2800,2400,3100),'run/RELEASE-01/vis/l4/crops/gc_seam2.png')
crop('run/RELEASE-01/vis/l4/gc_full.png',(900,1600,1500,2200),'run/RELEASE-01/vis/l4/crops/gc_core.png')