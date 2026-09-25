# One-off CALIBRATION probe (not part of the delivered stdlib computation).
# Reads RING-scheme pixel centers + 4 corners from an independent community
# implementation (astropy_healpix 2.0.0 / astropy 6.1.7) to pin down and later
# check my own stdlib transcription of the HEALPix ring corner geometry.
import numpy as np
from astropy_healpix import HEALPix

n = 2
hp = HEALPix(nside=n, order='ring')
idx = np.arange(hp.npix)
lon, lat = hp.healpix_to_lonlat(idx)
vlon, vlat = hp.boundaries_lonlat(idx, step=1)
print('vlon.shape', vlon.shape, 'center shape', lon.shape)
for j in range(hp.npix):
    cz = np.sin(np.deg2rad(float(lat.value[j])))
    cs = []
    for k in range(vlon.shape[1]):
        cs.append('(%.4f,%+.8f)' % (float(vlon[j][k].deg),
                                    np.sin(np.deg2rad(float(vlat[j][k].deg)))))
    print('pix %3d  phi_c=%8.3f z_c=%+.8f  |  %s' % (j, float(lon.value[j]), cz, ' '.join(cs)))
