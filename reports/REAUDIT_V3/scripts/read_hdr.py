
import tarfile, struct
T = '/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/testdata/t4_red_plus_masters.tar'
name = 'testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts'
with tarfile.open(T, 'r') as tf:
    f = tf.extractfile(f'./{name}' if './'+name in tf.getnames() else name)
    hdr = f.read(2880*20)
# parse header cards
cards = []
for i in range(0, len(hdr), 80):
    rec = hdr[i:i+80]
    if rec[:8] == b'END     ': break
    cards.append(rec.decode('ascii', 'replace').rstrip())
for c in cards:
    if any(k in c for k in ['NAXIS', 'CRVAL', 'CRPIX', 'CD1_', 'CD2_', 'CTYPE', 'CUNIT', 'PC1_', 'PC2_', 'RADESYS', 'EQUINOX', 'PHOTSCAL', 'FILTER', 'OBJCTRA', 'OBJCTDEC', 'FOV', 'PIXSCALE', 'CDELT', 'INSTNAME']):
        print(c)
