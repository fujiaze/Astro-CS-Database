import struct, os, math
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "runs", "synthetic_calibrated_2048.fts")
W = H = 2048
def val(x, y):
    r = (x - 1024)**2 + (y - 1024)**2
    g = 3000.0 * math.exp(-r / (2 * 600.0**2))
    base = 700.0 + 0.2 * x
    return int(base + g)
def card(key, val, comment=""):
    s = "%-8s= %s" % (key, val)
    if comment:
        s = (s + " " * (30 - len(s)) + "/ " + comment) if len(s) < 30 else s + " / " + comment
    return s[:80].ljust(80)
cards = [card("SIMPLE","T"), card("BITPIX","16"), card("NAXIS","2"),
         card("NAXIS1",str(W)), card("NAXIS2",str(H)), card("BZERO","0"), card("BSCALE","1"),
         card("CTYPE1",chr(39)+"RA---TAN"+chr(39)), card("CTYPE2",chr(39)+"DEC--TAN"+chr(39)),
         card("CRVAL1","272.825665"), card("CRVAL2","-13.131811"),
         card("CRPIX1","1024.5"), card("CRPIX2","1024.5"),
         card("CD1_1","0.001752"), card("CD1_2","0.0000004"),
         card("CD2_1","-0.0000007"), card("CD2_2","0.001752"),
         card("PHOTSCAL","1.0"), card("PHOTAPPL","1"), card("END","")]
header = "".join(cards).encode("latin1")
header += b" " * (2880 - len(header) % 2880) if len(header) % 2880 else b""
data = bytearray()
for y in range(H):
    for x in range(W):
        data += struct.pack(">h", val(x, y))
data += b" " * ((2880 - len(data) % 2880) % 2880)
open(OUT, "wb").write(header + bytes(data))
print("wrote", OUT, "size", os.path.getsize(OUT))
