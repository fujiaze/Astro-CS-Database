import os, json
root="/workspace/Astro CS Database"
j=json.load(open(os.path.join(root,"问题扫描/_cache/v12_fam.json")))
for fam in ["CAPPED200000","WL343","M60"]:
    print("#### "+fam+" :: CODE ####")
    for f,i,l in j[fam]["code"]:
        print(f"  {f}:{i}: {l[:150]}")
    print("#### "+fam+" :: DOC ####")
    for f,i,l in j[fam]["doc"]:
        print(f"  {f}:{i}: {l[:150]}")
    print()
