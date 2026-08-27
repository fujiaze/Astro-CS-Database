
import struct, sys
p = sys.argv[1]
d = open(p, 'rb').read(2880*40)
cards = []
for i in range(0, min(len(d), 2880*40), 80):
    rec = d[i:i+80]
    if rec[:8] == b'END     ': break
    try: cards.append(rec.decode('ascii', 'replace').rstrip())
    except: pass
for c in cards:
    if any(k in c for k in ['NAXIS', 'NAXIS1', 'NAXIS2', 'BITPIX', 'CRVAL', 'CRPIX', 'CD1_', 'CD2_', 'CTYPE', 'HPX', 'ORDER', 'IPIX', 'NSIDE', 'EXTNAME']):
        print(c)
print('--- all cards with values ---')
for c in cards:
    if '=' in c: pass
