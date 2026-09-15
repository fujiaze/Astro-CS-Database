
import math

out = []
# ============ 1) HEALPix sqrt(pi/3) per-nside arcsec constant ============
true_const = math.sqrt(math.pi/3.0) * (180.0/math.pi) * 3600.0
out.append("1) HEALPIX_SCALE_PER_NSIDE_ARCSEC true value = %.6f" % true_const)
for lit in (210960.0, 211034.6, 58.6*3600):
    out.append("   literal %.1f : delta=%+.2f (%+.4f%%)" % (lit, lit-true_const, 100*(lit-true_const)/true_const))
# decision-flip window for nside selection (prod formula vs 210960 test oracle)
for k in (20,21,22):
    p = float(2**k)
    lo, hi = 210960.0/p, true_const/p   # finest_arcsec window where the two disagree on >= p
    out.append("   k=%d: finest in (%.6f, %.6f] arcsec/px -> 210960-rule picks 2^%d but true formula needs 2^%d" % (k, lo, hi, k-1, k))

# ============ 2) order_needed frozen formula vs code loop ============
def order_needed_doc(W, s_out_arcsec):
    s = s_out_arcsec/3600.0*math.pi/180.0
    return math.ceil(math.log2(math.sqrt(math.pi/3.0)/(W*s)))
def order_select_code(max_order, scale_deg, W=512):
    # replicate p3_resample.cpp:88-94 & plan_estimator:97-100
    for k in range(0, max_order+1):
        nside = 512 << k
        res_arcsec = math.sqrt(4*math.pi/(12.0*nside*nside))*(180*3600/math.pi)
        if res_arcsec/3600.0 <= scale_deg:
            return k
    return max_order
for s_out, hips_order in ((0.108, 12), (0.108, 9), (1.0, 11), (0.201, 11), (3.2, 10)):
    d = order_needed_doc(512, s_out)
    c = order_select_code(hips_order, s_out/3600.0)
    leaf_native = math.sqrt(math.pi/3.0)/(512*(2**hips_order))*206264.806247
    leaf_sel    = math.sqrt(math.pi/3.0)/(512*(2**c))*206264.806247
    out.append("2) s_out=%.3f\"/px hips_order=%d: doc order_needed=%d, code order_sel=%d ; "
               "leaf scale @order_sel=%.4f\" ; leaf scale NATIVE(order=%d)=%.4f\" ; "
               "doc says sample at 2^(sel+9), impl always samples at 2^(%d+9) -> %s" %
               (s_out, hips_order, d, c, leaf_sel, hips_order, leaf_native, hips_order,
                "MATCH" if c==hips_order else "MISMATCH (provenance ORDERSEL=%d but sampler reads native)"%c))
out.append("   exact check for s_out=0.108: order_needed=ceil(log2(sqrt(pi/3)/(512*5.236e-7)))=ceil(%.4f)=%d" %
           (math.log2(math.sqrt(math.pi/3)/(512*(0.108/3600.0*math.pi/180.0))), order_needed_doc(512,0.108)))

# ============ 3) CDELT+CROTA2 synthesis vs module_adapters-written CDELT/CROTA2 (rotated east_left PA=30) ============
s = 1.1e-5
PA = 30.0
cp, sp = math.cos(math.radians(PA)), math.sin(math.radians(PA))
CD = [[-s*cp, s*sp],[s*sp, s*cp]]           # g1_build_cd east_left PA=30 (p3_wcs.cpp:135-138)
cd11,cd12,cd21,cd22 = CD[0][0],CD[0][1],CD[1][0],CD[1][1]
# module_adapters.cpp:3107/3117 writes CDELT1=cd11, CDELT2=cd22, CROTA2=0
cdelt1, cdelt2, crota2 = cd11, cd22, 0.0
# fits_reader.cpp:365-368 reconstruct:
c, sr = math.cos(math.radians(crota2)), math.sin(math.radians(crota2))
CDr = [[cdelt1*c, -cdelt2*sr],[cdelt1*sr, cdelt2*c]]
out.append("3) PA=30 east_left, s=1.1e-5 deg/px: CD=[%.3e %.3e; %.3e %.3e]" % (cd11,cd12,cd21,cd22))
out.append("   written CDELT1=%.4e CDELT2=%.4e CROTA2=0 -> reconstructed CD=[%.3e %.3e; %.3e %.3e]" %
           (cdelt1, cdelt2, CDr[0][0], CDr[0][1], CDr[1][0], CDr[1][1]))
sc_true, sc_rec = math.sqrt(abs(cd11*cd22-cd12*cd21))*3600, math.sqrt(abs(CDr[0][0]*CDr[1][1]-CDr[0][1]*CDr[1][0]))*3600
out.append("   pixel scale true=%.4f\" vs reconstructed=%.4f\" (err %+.2f%%); rotation dropped (%.0f deg)" % (sc_true, sc_rec, 100*(sc_rec-sc_true)/sc_true, PA))
# corner displacement for R=2048 px from CRPIX:
R=2048.5
import math as m
p_true=(R*m.radians(cd11*-R+cd12*R), R*m.radians(cd21*-R+cd22*R))  # not sky; use px-domain diff instead:
d_px = max(abs(cd12-CDr[0][1])/ (math.radians(s)) , 0)  # simpler: displacement of far corner in px:
# far corner (dx,dy)=(-R,R): delta_pos = (CD-CDr) . (dx,dy) in deg then arcsec
dv = [ (CD[0][0]-CDr[0][0])*(-R)+(CD[0][1]-CDr[0][1])*(R), (CD[1][0]-CDr[1][0])*(-R)+(CD[1][1]-CDr[1][1])*(R) ]
out.append("   far-corner (r=2048px) position error from mis-decomposition = %.2f arcsec" % (math.hypot(*dv)*3600))
# correct decomposition (cfitsio wcssub.c:253-280): theta=atan2(cd21,cd11); cd1=cd11/cos; cd2=cd22/cos
th = math.atan2(cd21, cd11); 
out.append("   cfitsio-conform CROTA2 for this CD = %.2f deg (atan2(CD2_1,CD1_1)); writer emits 0" % math.degrees(th))

# ============ 4) fits_reader vs correct legacy mapping sign check ============
# CD1_2 must be -CDELT2*sin, CD2_1 = +CDELT1*sin (cfitsio inversion). check with anisotropic CDELT:
c1v, c2v, rot = -0.2/3600.0, 0.25/3600.0, 20.0
cr, sr2 = math.cos(math.radians(rot)), math.sin(math.radians(rot))
CDstd = [[c1v*cr, -c2v*sr2],[c1v*sr2, c2v*cr]]
# fits_reader formula: cd[0]=c1*cos, cd[1]=-c2*sin, cd[2]=c1*sin, cd[3]=c2*cos  -> identical
CDfr  = [[c1v*cr, -c2v*sr2],[c1v*sr2, c2v*cr]]
out.append("4) fits_reader/hp_drizzle CROTA2 branch equals cfitsio-consistent synthesis (incl. anisotropic pairing): %s" % (CDstd==CDfr))

# ============ 5) aio_wcs_rotation_deg vs PA semantics ============
def arotd(CD): return math.degrees(math.atan2(CD[1][0], CD[0][0]))
for PA_t in (0.0, 30.0, -25.0):
    c_, s_ = math.cos(math.radians(PA_t)), math.sin(math.radians(PA_t))
    CDel = [[-s*c_, s*s_],[s*s_, s*c_]]
    out.append("5) true east_left PA=%.0f -> aio_wcs_rotation_deg returns %.1f (relation: %.0f - PA => sign flipped + 180 offset)" % (PA_t, arotd(CDel), 180))

# ============ 6) TAN pole-cross field passes guards (p3_wcs emulation) ============
def pix2world(crp, crval, CD, x, y):
    dx = (x+1)-crp[0]; dy=(y+1)-crp[1]
    xi = (CD[0][0]*dx+CD[0][1]*dy)*math.pi/180
    eta= (CD[1][0]*dx+CD[1][1]*dy)*math.pi/180
    r = math.hypot(xi,eta)
    hemi = r >= math.pi/2
    th = math.atan2(1.0, r); ph = math.atan2(-xi, eta)
    a0=crval[0]*math.pi/180; d0=crval[1]*math.pi/180
    dec = math.asin(math.sin(th)*math.sin(d0)+math.cos(th)*math.cos(d0)*math.cos(ph))
    dra = math.atan2(-math.cos(th)*math.sin(ph), math.cos(d0)*math.sin(th)-math.sin(d0)*math.cos(th)*math.cos(ph))
    return ((a0+dra)*180/math.pi)%360, dec*180/math.pi, hemi
W=H=15000; scale=1e-3; crval=(40.0, 85.0); CD=[[-scale,0],[0,scale]]
ra,dec,hemi = pix2world(((W+1)/2,(H+1)/2), crval, CD, 0, H-1)
out.append("6) center dec=85, 15000x15000 @1e-3 deg/px (7.5 deg half-side, field spans the pole):")
out.append("   corner (0,14999) -> RA=%.3f dec=%.3f hemisphere_ok=%s ; north-most point should be beyond 90deg;" % (ra,dec,not hemi))
out.append("   RA flipped by %.1f deg vs center (pole fold) => guard passes a pole-wrapping image" % (abs(ra-40.0)))
ra2,dec2,_ = pix2world(((W+1)/2,(H+1)/2), crval, CD, 0, 7500)
out.append("   mid-top edge (0,7500): dec=%.3f, distance from pole=%.3f deg (crosses pole by %.3f)" % (dec2, 90-dec2, (7.5-(90-85.0))))

# ============ 7) WcsTan vs wcs_sip 1px & SIP-origin gap, and ipv 0.5px origin ============
crpix=2048.5
s11 = 1.1e-5
def wstan(x): return (x - crpix)                # module_adapters feeds 0-based x
def wsip(x):  return (x - (crpix-1.0))          # wcs_sip.cpp:247
out.append("7) same header CRPIX=2048.5, array pixel x=1000: WcsTan path dx=%.1f vs wcs_sip path dx=%.1f -> 1 px = %.4f arcsec (s=1.1e-5)" % (wstan(1000), wsip(1000), abs(wsip(1000)-wstan(1000))*s11*3600))
A20 = 8e-5
u = 1000-crpix
out.append("   SIP A_20=8e-5: p1 self-check dx=%.1f -> A=%.5f px ; drizzle dx=%.1f -> A=%.5f px ; diff=%.5f px = %.3f arcsec" %
          (u, A20*u*u, u+1, A20*(u+1)**2, A20*((u+1)**2-u**2), A20*((u+1)**2-u**2)*s11*3600))
# ipv 0.5px: solver U origin at array (W/2,H/2); FITS CRPIX=W/2+0.5 <-> array W/2-0.5
out.append("   ipv origin: U=0 at array x=W/2=%.1f ; header CRVAL sits at array %.1f -> %.1f px per axis = %.4f arcsec diag=%.4f (BASS 1.109e-4 deg/px)" %
          (2048, 2047.5, 0.5, 0.5*1.109723e-4*3600, math.hypot(.5,.5)*1.109723e-4*3600))

# ============ 8) RA range conventions ============
out.append("8) WcsTan wraps RA to [-180,180) (wcs_tan.cpp:35-36); wcs_sip/p3_wcs/snr_estimator to [0,360); contract ASTROMETRY.md:29 says [0,360): ra0=350 deg, x offset -30 px -> WcsTan returns %.1f" % (350-30*1.1e-5*180/math.pi*0 - 30*1.1e-5/ (math.pi/180)*0 - (30*1.1e-5)* (180/math.pi)/1 if False else 350-30*1.1e-5))
print("\n".join(out))
