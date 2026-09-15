import os, re, subprocess, json, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","*.py"], capture_output=True, text=True).stdout.split(chr(0)) if p]
EXCL = ("问题扫描/", "工程控制/", "设计大纲/", "engineering/control/")
py = [f for f in files if not f.startswith(EXCL)]
mirrors = []
for f in py:
    txt = open(f, encoding="utf-8", errors="replace").read()
    if "Structure" not in txt: continue
    for m in re.finditer(r"class\s+(\w+)\s*\(\s*(?:ctypes\.)?Structure\s*\)\s*:", txt):
        i = txt.find("{", m.end()-1)
        if i < 0: continue
        d=0; j=i
        while j < len(txt):
            if txt[j] == "{": d+=1
            elif txt[j] == "}":
                d-=1
                if d==0: break
            j+=1
        body = txt[i:j]
        flds = re.findall(r"\"(\w+)\",\s*(\w+)", body)
        ln = txt[:m.start()].count(chr(10)) + 1
        mirrors.append({"file": f, "line": ln, "cls": m.group(1),
                        "n_fields": len(flds),
                        "has_struct_size": any(n == "struct_size" for n, t in flds),
                        "has_abi_version": any(n == "abi_version" for n, t in flds)})
print("口径 W6-J：Python ctypes 镜像结构类总数:", len(mirrors))
print("其中带 struct_size 字段:", sum(1 for m in mirrors if m["has_struct_size"]))
print("带 abi_version:", sum(1 for m in mirrors if m["has_abi_version"]))
byfile = collections.Counter(m["file"] for m in mirrors)
print("-- 镜像文件分布 --")
for f, c in byfile.most_common(60): print("   ", f, c)
json.dump(mirrors, open(os.path.join(HERE, "_w6_mirrors.json"),"w"), indent=1, ensure_ascii=False)
print()
print("-- 无 struct_size 的镜像类清单 --")
for m in mirrors:
    if not m["has_struct_size"]:
        print("   %-52s %-30s fields=%d abi_version=%s" % (m["file"], m["cls"], m["n_fields"], m["has_abi_version"]))