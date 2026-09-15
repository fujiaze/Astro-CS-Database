import os, re, subprocess, json, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","*.h","*.hpp"], capture_output=True, text=True).stdout.split(chr(0)) if p]
EXCL = ("问题扫描/", "工程控制/", "设计大纲/", "third_party/", "lib/astro_image_io/third_party/", "lib/orchestrator/cpp/third_party/", "lib/acr/", "healpix_browser_qt", "GaiaDR3")
hdrs = [f for f in files if not f.startswith(EXCL)]
print("口径 W6-G：受跟踪 .h/.hpp 总数", len(files), "排除", list(EXCL), "后", len(hdrs))
ST = re.compile(r"typedef\s+struct\s*(\w+)?\s*\{", re.S)
rows = []
for f in hdrs:
    txt = open(f, encoding="utf-8", errors="replace").read()
    for m in ST.finditer(txt):
        # 取平衡花括号体
        i = txt.find("{", m.start()); d = 0; j = i
        while j < len(txt):
            if txt[j] == "{": d += 1
            elif txt[j] == "}":
                d -= 1
                if d == 0: break
            j += 1
        body = txt[i:j+1]
        tail = txt[j+1:j+80]
        tm = re.match(r"\s*(\w+)\s*;", tail)
        name = m.group(1) or (tm.group(1) if tm else "?")
        ln = txt[:m.start()].count(chr(10)) + 1
        rows.append({"file": f, "line": ln, "name": name,
                     "struct_size": bool(re.search(r"\bstruct_size\b", body)),
                     "abi_version": bool(re.search(r"\babi_version\b", body)),
                     "acs_head": bool(re.search(r"\bacs_head\b", body)),
                     "body_len": len(body)})
print("typedef struct 总数(分母口径 W6-H):", len(rows))
print("带 struct_size:", sum(1 for r in rows if r["struct_size"]),
      "| 带 abi_version:", sum(1 for r in rows if r["abi_version"]),
      "| 带 acs_head:", sum(1 for r in rows if r["acs_head"]))
noSelf = [r for r in rows if not (r["struct_size"] or r["acs_head"])]
print("既无 struct_size 也无 acs_head:", len(noSelf))
# 边界头口径：出现在 ABI/合同/导出面的头
EDGE = ("include/astrocs/", "/include/", "abi", "contract")
edge = [r for r in noSelf if ("include/" in r["file"]) and ("third_party" not in r["file"])]
print()
print("=== 位于任何 */include/ 公共目录且无自描述头的 struct（口径 W6-I） ===", len(edge))
byfile = collections.Counter(r["file"] for r in edge)
for f, c in byfile.most_common(60): print("   ", f, c)
json.dump(rows, open(os.path.join(HERE, "_w6_structs.json"), "w"), indent=1, ensure_ascii=False)