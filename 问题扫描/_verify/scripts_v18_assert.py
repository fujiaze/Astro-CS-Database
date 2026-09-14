import json,re
ROOT="/workspace/Astro CS Database"
rows=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_str.json",encoding="utf-8"))
vend=re.compile(r"/third_party/|/cfitsio/|/sqlite3|/nlohmann/")
def isassert(n): return bool(re.search(r"CHECK|ASSERT|EXPECT|VERIFY|REQUIRE|FAIL",n))
a=[r for r in rows if isassert(r["macro"])]
print("assertion-like macros with a NEVER-EVALUATED param:",len(a))
for r in a:
    v=" [VENDORED]" if vend.search(r["file"]) else ""
    print("  %-22s :%-5d %s%s" % (r["macro"], r["line"], r["file"], v))
    print("      params=%s never=%s" % (r["params"], r["never_used"]))
    print("      body=" + r["body"].replace(chr(10)," | ")[:230])
print()
nv=[r for r in rows if not isassert(r["macro"]) and not vend.search(r["file"])]
print("non-assertion, non-vendored (for completeness):",len(nv))
for r in nv[:20]: print("  ",r["macro"],r["file"]+":"+str(r["line"]),"never=",r["never_used"])