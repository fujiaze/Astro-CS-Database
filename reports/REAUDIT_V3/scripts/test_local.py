
import sys, os
sys.path.insert(0, '/home/lighthouse/astrocs_audit_v2/scripts')
import extract_seam as es, hpx
t = '/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/mosaic_samples/Npix112.fits'
if not os.path.exists(t):
    print('NO FILE'); sys.exit(0)
n1,n2,bp,data = es.fits_f32_data(t)
print('Npix112 naxis=',n1,n2,'bitpix=',bp,'bytes=',len(data))
if data:
    sig = es.parse_f32(data)
    # parent=112 order=2
    K=2; parent=112
    for (row,col) in [(0,0),(256,256),(511,511)]:
        fi=row*512+col; v=sig[fi]
        y=col; x=511-row
        ra,dec=hpx.tile_pix2ang(K,parent,x,y)
        print('  pix fi=%d row=%d col=%d sig=%.5f -> (ra=%.3f dec=%.3f)' % (fi,row,col,v,ra,dec))
