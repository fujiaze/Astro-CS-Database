import os, re, subprocess, json, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
structs = json.load(open(os.path.join(HERE, "_w6_structs.json"), encoding="utf-8"))
byname = collections.defaultdict(list)
for s in structs: byname[s["name"]].append(s)
names = ["IpvParams","IpvWcsResult","SDetParams","DPSFFitParams","DPSFFitResult","GaiaSpectrumStar","AstroSphereTileView","AioHipsSnrPoint","FioKeyword","FioHeader"]
for n in names:
    hits = byname.get(n, [])
    if not hits:
        print("C-TYPENAME-NOT-FOUND", n, "(可能为匿名 struct 或命名 typedef 在别处)")
        continue
    for h in hits:
        print("%-20s %-58s:%-5d struct_size=%-5s acs_head=%-5s abi_version=%s" % (n, h["file"], h["line"], h["struct_size"], h["acs_head"], h["abi_version"]))
print()
print("=== 名称匹配但位于 src/ 的头（分母外） ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","-e","GaiaSpectrumStar","-e","AioHipsSnrPoint","--","*.h","*.hpp"], capture_output=True, text=True)
print(out.stdout[:2500])