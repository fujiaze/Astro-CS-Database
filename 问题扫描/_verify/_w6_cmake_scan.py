#!/usr/bin/env python3
# W6 axis read-only scan: static extraction of add_test / add_executable / add_library / add_subdirectory / install()
import re, os, json, subprocess, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
files = subprocess.run(["git","--no-optional-locks","ls-files","*CMakeLists.txt"], capture_output=True, text=True).stdout.split()

def strip_comments(text):
    # 逐字符剥离注释：# 在双引号内不算注释（根 CMakeLists:62 "#define" 即为一例）
    out = []
    for line in text.split("\n"):
        res = []
        q = False
        for ch in line:
            if ch == '"':
                q = not q
                res.append(ch)
            elif ch == '#' and not q:
                break
            else:
                res.append(ch)
        out.append("".join(res))
    return out

def commands(lines):
    buf = ""
    depth = 0
    start = 0
    for n, line in enumerate(lines, 1):
        if depth == 0:
            start = n
        buf += line + " "
        depth += line.count("(") - line.count(")")
        while True:
            m = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(", buf)
            if not m:
                sp = re.search(r"[A-Za-z_][A-Za-z0-9_]*\s*\(", buf)
                if not sp:
                    buf = ""
                    break
                buf = buf[sp.start():]
                m = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(", buf)
            d = 0
            end = None
            for i in range(m.end()-1, len(buf)):
                if buf[i] == "(": d += 1
                elif buf[i] == ")":
                    d -= 1
                    if d == 0:
                        end = i
                        break
            if end is None:
                break
            yield start, m.group(1).lower(), buf[m.end():end]
            buf = buf[end+1:]
            if not buf.strip():
                buf = ""
                break

addtest = collections.defaultdict(list)
addexe = collections.defaultdict(list)
addlib = collections.defaultdict(list)
addsub = collections.defaultdict(list)
installs = []
for f in files:
    text = open(f, encoding="utf-8", errors="replace").read()
    for n, cmd, body in commands(strip_comments(text)):
        b = body.strip()
        if cmd == "add_test":
            m = re.search(r"NAME\s+\"?([^\"\s]+)\"?", b)
            toks = b.split()
            name = m.group(1) if m else (toks[0].strip('"') if toks else "?")
            addtest[name].append(f + ":" + str(n))
        elif cmd == "add_executable":
            toks = b.split()
            if toks: addexe[toks[0]].append(f + ":" + str(n))
        elif cmd == "add_library":
            toks = b.split()
            if toks: addlib[toks[0]].append(f + ":" + str(n))
        elif cmd == "add_subdirectory":
            toks = b.split()
            if toks: addsub[toks[0]].append(f + ":" + str(n))
        elif cmd == "install":
            m = re.match(r"\s*(\w+)", b)
            installs.append([f + ":" + str(n), m.group(1) if m else "?", re.sub(r"\s+", " ", b)[:180]])

out = {"cmakelists_total": len(files), "add_test": dict(addtest), "add_executable": dict(addexe),
       "add_library": dict(addlib), "add_subdirectory": dict(addsub), "install": installs}
dest = os.path.join(HERE, "_w6_cmake_facts.json")
json.dump(out, open(dest, "w"), indent=1, ensure_ascii=False)
print("cmakelists:", len(files), "add_test names:", len(addtest), "add_executable:", len(addexe),
      "add_library:", len(addlib), "add_subdirectory:", len(addsub), "install sites:", len(installs))
for s in installs:
    print("  INSTALL", s[0], s[1], s[2][:110])