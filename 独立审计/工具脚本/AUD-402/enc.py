import sys
# usage: enc.py FILE...  -> prints utf8-ok / gbk / unknown, and CJK-comment count
for f in sys.argv[1:]:
    b = open(f, "rb").read()
    u = g = None
    try:
        b.decode("utf-8"); u = True
    except Exception:
        u = False
    try:
        b.decode("gbk"); g = True
    except Exception:
        g = False
    head = ""
    try:
        head = b.decode("utf-8")[:0]
    except Exception:
        head = ""
    n_nonascii = sum(1 for x in b if x > 127)
    print(f, "utf8_ok=", u, "gbk_ok=", g, "nonascii_bytes=", n_nonascii)
