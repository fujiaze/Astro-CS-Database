
import math
out=[]
# C3: PC+CDELT dropped by input readers (synthesis = diag(CDELT))
c1, c2 = -3.0e-7, 3.0e-7   # deg/px
PA = -30.0
pc = [[math.cos(math.radians(PA)), math.sin(math.radians(PA))], [-math.sin(math.radians(PA)), math.cos(math.radians(PA))]]
CDtrue = [[c1*pc[0][0], c1*pc[0][1]], [c2*pc[1][0], c2*pc[1][1]]]
CDread = [[c1,0.0],[0.0,c2]]
for (dx,dy,label) in ((-1024,1024,"corner dx=-1024,dy=+1024"), (1024,0,"mid dx=+1024")):
    e = [ (CDtrue[0][0]-CDread[0][0])*dx + (CDtrue[0][1]-CDread[0][1])*dy,
          (CDtrue[1][0]-CDread[1][0])*dx + (CDtrue[1][1]-CDread[1][1])*dy ]
    out.append("C3 %s: displacement = %.2f arcsec" % (label, math.hypot(*e)*3600))
# C7: WcsTan RA domain
s=1.1e-5
for ra0 in (210.0, 10.0):
    for dxpx in (30,-30):
        xi_deg = -s*dxpx
        ra_true = (ra0 + xi_deg/math.cos(math.radians(2.0))) % 360.0
        ra_wstan = ra0 + xi_deg/math.cos(math.radians(2.0))
        while ra_wstan > 180.0: ra_wstan -= 360.0
        while ra_wstan < -180.0: ra_wstan += 360.0
        out.append("C7 ra0=%.0f dx=%+dpx: [0,360) truth=%.5f ; WcsTan=%.5f ; jump=%.3f deg" % (ra0, dxpx, ra_true, ra_wstan, abs(ra_true-ra_wstan)))
print("\n".join(out))
