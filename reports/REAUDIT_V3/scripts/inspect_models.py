import json, math
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m2 = json.load(open(B + "fatduck_2f_upm.json"))
m26 = json.load(open(B + "fatduck_26f_upm.json"))
print("2f keys:", list(m2.keys())[:12])
print("C type:", type(m2.get("C")).__name__, "len:", len(m2.get("C", [])) if isinstance(m2.get("C"), list) else "?")
# understand C layout
c = m2.get("C")
if isinstance(c, list) and c:
    print("C[0] type:", type(c[0]).__name__, "len:", len(c[0]) if isinstance(c[0], (list, dict)) else "scalar")
    print("C[0] sample:", c[0][:5] if isinstance(c[0], list) else c[0])
print("frames 2f:", m2.get("frames"))
print("frames 26f count:", len(m26.get("frames", [])))
print("control_count 2f/26f:", m2.get("control_count"), m26.get("control_count"))
# controls leaf layout
ctrl2 = m2.get("controls", [])
print("controls[0] (2f):", ctrl2[0] if ctrl2 else None)
