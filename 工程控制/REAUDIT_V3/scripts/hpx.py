
import math
TWO_PI = 2*math.pi
HALF_PI = 0.5*math.pi
K2O3 = 2.0/3.0
KR3 = math.sqrt(3.0)

def _xy_to_nest(ix,iy,bits):
    ip=0
    for i in range(bits):
        ip |= ((ix>>i)&1) << (2*i)
        ip |= ((iy>>i)&1) << (2*i+1)
    return ip

def _nest_to_xy(v,bits):
    ix=iy=0
    for i in range(bits):
        ix |= ((v>>(2*i))&1) << i
        iy |= ((v>>(2*i+1))&1) << i
    return ix,iy

def _xyz_to_hp(vx,vy,vz,nside):
    phi=math.atan2(vy,vx)
    if phi<0: phi+=TWO_PI
    phi_t=phi % HALF_PI
    ns=int(nside)
    if vz>=K2O3 or vz<=-K2O3:
        north=(vz>=K2O3)
        zz=vz if north else -vz
        coz=math.sqrt(vx*vx+vy*vy)
        kx=(coz/math.sqrt(1.0+zz))*KR3*abs(ns*(2*phi_t-math.pi)/math.pi)
        ky=(coz/math.sqrt(1.0+zz))*KR3*ns*2*phi_t/math.pi
        if north:
            xx=ns-kx; yy=ns-ky
        else:
            xx=ky; yy=kx
        ix=int(min(ns-1,math.floor(xx))); iy=int(min(ns-1,math.floor(yy)))
        if ix<0: ix=0
        if iy<0: iy=0
        sector=(phi-phi_t)/HALF_PI
        off=int(round(sector))%4
        base=off if north else 8+off
    else:
        zunits=(vz+K2O3)/(4.0/3.0)
        phiu=phi_t/HALF_PI
        u1=zunits+phiu; u2=zunits-phiu+1.0
        xx=u1*ns; yy=u2*ns
        sector=(phi-phi_t)/HALF_PI
        off=int(round(sector))%4
        if xx>=ns:
            xx-=ns
            if yy>=ns:
                yy-=ns; base=off
            else:
                base=((off+1)%4)+4
        else:
            if yy>=ns:
                yy-=ns; base=off+4
            else:
                base=8+off
        ix=int(math.floor(xx)); iy=int(math.floor(yy))
        if ix<0: ix=0
        if iy<0: iy=0
        if ix>=ns: ix=ns-1
        if iy>=ns: iy=ns-1
    return base,ix,iy

def _hp_to_xyz(base,px,py,dx,dy,nside):
    ns=int(nside)
    x=float(px)+dx; y=float(py)+dy
    chp=base
    eq=True; zf=1.0
    if (chp<=3) and (x+y)>ns: eq=False; zf=1.0
    if (chp>=8) and (x+y)<ns: eq=False; zf=-1.0
    if eq:
        zoff=0.0; phioff=0.0
        x/=ns; y/=ns
        if chp<=3: phioff=1.0
        elif chp<=7: zoff=-1.0; chp-=4
        else: phioff=1.0; zoff=-2.0; chp-=8
        z=K2O3*(x+y+zoff)
        phi=(math.pi/4.0)*(x-y+phioff+2.0*chp)
    else:
        xx=x; yy=y
        if zf==-1.0:
            xx,yy=yy,xx; xx=ns-xx; yy=ns-yy
        if yy>=ns and xx>=ns: phi_t=0.0
        else: phi_t=math.pi*(ns-yy)/(2.0*((ns-xx)+(ns-yy)))
        if phi_t<math.pi/4.0:
            z=1.0-(math.pi*(ns-xx)/((2.0*phi_t-math.pi)*ns))**2/3.0
        else:
            z=1.0-(math.pi*(ns-yy)/(2.0*phi_t*ns))**2/3.0
        z*=zf
        if base>=8: phi=HALF_PI*(base-8)+phi_t
        else: phi=HALF_PI*base+phi_t
    if phi<0: phi+=TWO_PI
    if phi>=TWO_PI: phi-=TWO_PI
    if z>1.0: z=1.0
    if z<-1.0: z=-1.0
    theta=math.acos(z)
    return (math.sin(theta)*math.cos(phi), math.sin(theta)*math.sin(phi), math.cos(theta))

def pix2ang_nest(nside, ipix):
    L=int(round(math.log2(nside)))
    npf=1<<(2*L)  # nside^2
    face=int(ipix>>(2*L))
    inf=ipix & (npf-1)
    ix,iy=_nest_to_xy(inf,L)
    x,y,z=_hp_to_xyz(face,ix,iy,0.5,0.5,nside)
    ra=math.atan2(y,x)
    if ra<0: ra+=TWO_PI
    dec=math.asin(max(-1,min(1,z)))
    return ra*180.0/math.pi, dec*180.0/math.pi

def ang2pix_nest(ra_deg,dec_deg,nside):
    ra=ra_deg*math.pi/180.0; dec=dec_deg*math.pi/180.0
    cd=math.cos(dec)
    vx=cd*math.cos(ra); vy=cd*math.sin(ra); vz=math.sin(dec)
    base,ix,iy=_xyz_to_hp(vx,vy,vz,nside)
    L=int(round(math.log2(nside)))
    return (base<<(2*L)) | _xy_to_nest(ix,iy,L)

def tile_fits_to_parent_xy(fi, K):
    # fi = row*512+col (0-based, col fastest). decode to healpix local (x,y)
    row=fi//512; col=fi%512
    y=col
    x=511-row
    return x,y

def tile_pix2ang(K, parent, x, y):
    # raster pixel (x,y) of tile (K,parent) -> sky
    nside=512*(1<<K)
    local=_xy_to_nest(x,y,9)
    leaf=(parent<<18)|local
    return pix2ang_nest(nside, leaf)

if __name__=='__main__':
    # validation 1: ang2pix_nest roundtrip: pick sky -> pix -> sky approx equal (center)
    for ra,dec,n in [(272.83,-13.13,512),(272.8,-15.7,1024),(272.8,-20.9,4096),(300.0,30.0,512)]:
        p=ang2pix_nest(ra,dec,n)
        ra2,dec2=pix2ang_nest(n,p)
        err=((ra2-ra+180)%360-180)**2+(dec2-dec)**2
        print('rt n=%d ra=%.3f dec=%.3f -> pix=%d -> ra=%.4f dec=%.4f err=%.2e' % (n,ra,dec,p,ra2,dec2,math.sqrt(err)))
    # validation 2: cell 7 at order0 (Npix7) dec range
    lo=+90; hi=-90
    for y in [0,255,511]:
        for x in [0,255,511]:
            ra,dec=tile_pix2ang(0,7,x,y)
            lo=min(lo,dec); hi=max(hi,dec)
    print('Norder0 Npix7 dec range: [%.2f, %.2f]' % (lo,hi))
    # validation 3: Norder1 cells 28..31 (children of cell7) dec ranges should subdivide it
    for p in [28,29,30,31]:
        lo=+90; hi=-90
        for j in [0,255,511]:
            for i in [0,255,511]:
                ra,dec=tile_pix2ang(1,p,i,j)
                lo=min(lo,dec); hi=max(hi,dec)
        print('Norder1 Npix%d dec [%.3f,%.3f]' % (p,lo,hi))
