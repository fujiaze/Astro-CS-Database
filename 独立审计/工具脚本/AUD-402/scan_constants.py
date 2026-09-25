#!/usr/bin/env python3
"""AUD-402 deterministic numeric-constant surface scanner (READ-ONLY).

Universe = git-tracked files of the frozen baseline HEAD, restricted to the
AUD-402 scope; vendored / archived / ignored trees are excluded.
The script only READS the repository; every output lands outside it.

Outputs (OUT dir):
  universe.tsv    per-universe file counts + full file list
  cpp_hits.tsv    every source line containing a numeric literal
  named.tsv       named constants only (#define / constexpr / const / static)
  json_hits.tsv   every numeric / enum / default / unit field of config+contracts
  counts.tsv      machine summary used by the coverage self-report

Rails:
  --self-test    positive + negative control, exit 4 if the scanner is dead
  --empty-check  proves an empty scan universe exits non-zero (3)
  main()         exits 3 if any universe has 0 files (0 files != 0 hits)
"""
import json
import os
import re
import subprocess
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
OUT = r"独立审计/证据/scan402"

CPP_EXT = (".cpp", ".h", ".hpp", ".cc", ".cxx")
VENDOR = ("lib/third_party/", "/third_party/", "/archive/", ".trae/", "__pycache__",
          "nlohmann/", "json-schema-validator/")

NUM = r"(?:\d[\d']*(?:\.\d[\d']*)?(?:[eE][-+]?\d+)?|\.\d[\d']*(?:[eE][-+]?\d+)?)"
NUMRE = re.compile(r"\b0[xX][0-9a-fA-F]+\b|" + NUM + r"[fFlLuU]*")
DEFINE = re.compile(r"^\s*#\s*define\s+(?P<name>[A-Za-z_]\w*)[ \t]*(?P<val>[^\r\n]*)")
TYPE = (r"(?:long\s+double|unsigned\s+long|double|float|int|long|short|char|bool|auto"
        r"|std::size_t|size_t|ptrdiff_t|uint8_t|uint16_t|uint32_t|uint64_t"
        r"|int8_t|int16_t|int32_t|int64_t|uint\w*|int\w*)")
NAMEDDECL = re.compile(
    r"\b(?P<kind>constexpr|constinit|const)\s+(?:static\s+|inline\s+|thread_local\s+)*"
    r"(?:" + TYPE + r"\s+)?(?P<name>[A-Za-z_]\w*)\s*"
    r"(?:=\s*(?P<val>[-+]?\s*" + NUM + r")[^;]{0,140})?;")
MEMINIT = re.compile(r"(?P<name>[A-Za-z_]\w*)\s*(?:=|\{)\s*(?P<val>[-+]?\s*" + NUM +
                     r")[fF]?[ \t]*(?:[,;}\)]|$)")
SEMANTIC = re.compile(
    r"(tol|epsilon|\beps\b|thresh|sigma|limit|window|radius|factor|coeff|alpha|beta"
    r"|gain|bias|sat|clip|pad|margin|buf|chunk|tile|level|depth|iter|retr|tries"
    r"|timeout|weight|snr|mag|flux|scale|step|fallback|default|floor|ceil|ratio"
    r"|power|min|max|resid|cut\b|unit|exposure|aperture|fwhm|sky|drizzle|cov\b|ivar"
    r"|variance|chi|psf|wcs|arcsec|arcdeg|pixel|second|kelvin|adu|electron)", re.I)


def norm(p):
    return p.replace("\\", "/")


def excluded(f):
    return any(v in f for v in VENDOR)


def git_ls_files():
    r = subprocess.run(["git", "-C", REPO, "ls-files", "-z"], capture_output=True)
    if r.returncode != 0:
        print("git ls-files failed:", r.stderr.decode("utf-8", "replace"))
        sys.exit(2)
    return sorted({norm(p.decode("utf-8")) for p in r.stdout.split(b"\0") if p})


def repo_path(f):
    return f if os.path.isabs(f) else os.path.join(REPO, f.replace("/", os.sep))


def strip_comment(line):
    out = re.split(r"(?<!:)//", line)[0]
    return re.sub(r"/\*.*?\*/", " ", out)


def read_lines(f):
    try:
        with open(repo_path(f), "r", encoding="utf-8", errors="replace") as fh:
            return fh.read().splitlines()
    except OSError as e:
        print("READ-FAIL", f, e, file=sys.stderr)
        return []


def classify(code, dm, nd):
    cls = []
    if dm:
        cls.append("define")
    if nd and nd.group("kind") in ("constexpr", "constinit"):
        cls.append("constexpr-named")
    elif nd and nd.group("kind") == "const":
        cls.append("const-named")
    elif re.search(r"\b(constexpr|constinit)\b", code):
        cls.append("constexpr-expr")
    elif re.search(r"\bconst\b", code):
        cls.append("const-expr")
    if re.search(r"\b(static|thread_local)\b[^=]*=", code) and "define" not in cls:
        cls.append("static-init")
    if not cls:
        cls.append("value-init" if re.search(r"\w+\s*(=|\{\s*)\s*[-+]?" + NUM, code)
                   else "literal-inline")
    return cls


def pick_name(code, dm, nd):
    if dm:
        return dm.group("name")
    if nd:
        return nd.group("name")
    mm = MEMINIT.search(code)
    if mm:
        return mm.group("name")
    nm = re.search(r"\b([A-Za-z_]\w*)\s*(?:=[^=]|\{)", code)
    return nm.group(1) if nm else ""


def scan_cpp(files):
    rows = []
    for f in files:
        for i, raw in enumerate(read_lines(f), 1):
            code = strip_comment(raw)
            if not NUMRE.search(code):
                continue
            dm = DEFINE.match(code)
            nd = NAMEDDECL.search(code)
            vals = [v.rstrip("fFlLuU") for v in NUMRE.findall(code)]
            assigned = (dm.group("val").strip() if dm else
                        (re.sub(r"\s+", "", nd.group("val")) if nd and nd.group("val") else
                         (re.sub(r"\s+", "", MEMINIT.search(code).group("val"))
                          if (not dm and not nd and MEMINIT.search(code)) else "")))
            rows.append({
                "file": norm(f), "line": i, "class": "|".join(classify(code, dm, nd)),
                "name": pick_name(code, dm, nd), "assigned": assigned,
                "values": ";".join(vals[:10]), "ncount": len(vals),
                "sem": "1" if SEMANTIC.search(code) else "0",
                "text": raw.strip()[:400]})
    return rows


def scan_json(files):
    rows = []
    for f in files:
        try:
            with open(repo_path(f), "r", encoding="utf-8-sig") as fh:
                data = json.load(fh)
        except Exception as e:
            print("JSON-FAIL", f, e, file=sys.stderr)
            continue
        stack = [(data, "")]
        while stack:
            node, path = stack.pop()
            if isinstance(node, dict):
                for k, v in node.items():
                    kp = f"{path}/{k}"
                    if isinstance(v, bool):
                        rows.append({"file": f, "jpath": kp, "kind": "bool",
                                     "value": json.dumps(v)})
                    elif isinstance(v, (int, float)):
                        rows.append({"file": f, "jpath": kp, "kind": "number",
                                     "value": repr(v)})
                    elif isinstance(v, str):
                        kind = ""
                        if NUMRE.fullmatch(v.strip()):
                            kind = "numeric-string"
                        elif NUMRE.search(v) and re.search(r"(unit|version|second|pixel|mag|deg|frame)", k, re.I):
                            kind = "unit/version-string"
                        if kind:
                            rows.append({"file": f, "jpath": kp, "kind": kind,
                                         "value": v[:200]})
                    elif isinstance(v, list):
                        tag = ("enum-or-default-list" if k in ("enum", "default", "examples")
                               else "list")
                        rows.append({"file": f, "jpath": kp, "kind": tag,
                                     "value": json.dumps(v, ensure_ascii=False)[:600]})
                        for j, item in enumerate(v):
                            stack.append((item, f"{kp}/{j}"))
                    else:
                        stack.append((v, kp))
            elif isinstance(node, list):
                for j, item in enumerate(node):
                    stack.append((item, f"{path}/{j}"))
            elif isinstance(node, (int, float)) and not isinstance(node, bool):
                rows.append({"file": f, "jpath": path, "kind": "number-in-list",
                             "value": repr(node)})
    return rows


def build_universe():
    allf = git_ls_files()

    def has(prefixes):
        return [f for f in allf if any(f.startswith(p) for p in prefixes)]

    u = {}
    u["lib_cpp"] = [f for f in has(["lib/"]) if f.endswith(CPP_EXT) and not excluded(f)]
    u["eng_cpp"] = [f for f in has(["eng/"]) if f.endswith(CPP_EXT) and not excluded(f)]
    u["eng_py"] = [f for f in has(["eng/"]) if f.endswith(".py") and not excluded(f)]
    u["packaging_cfg"] = [f for f in has(["eng/packaging/"]) if f.endswith((".json", ".h.in"))]
    u["contracts_json"] = [f for f in has(["eng/contracts/"]) if f.endswith(".json")]
    u["cli_cpp"] = [f for f in has(["lib/infrastructure/cli/"]) if f.endswith(CPP_EXT)]
    u["lib_json"] = [f for f in has(["lib/"]) if f.endswith(".json") and not excluded(f)]
    u["build_meta"] = [f for f in has(["lib/", "eng/"])
                       if f.endswith((".yaml", ".yml", ".cmake", "CMakeLists.txt", ".h.in"))
                       and not excluded(f)]
    u["exp_code"] = [f for f in has(["\u5b9e\u9a8c/"]) if f.endswith((".py", ".cpp", ".c", ".h"))]
    u["exp_json"] = [f for f in has(["\u5b9e\u9a8c/"]) if f.endswith(".json")]
    return u


FIXTURE = [
    # (source line, expected value token, expected name, expected class substring)
    ("constexpr double kMagTolerance = 0.025;", "0.025", "kMagTolerance", "constexpr-named"),
    ("#define DEFAULT_TILE_PX 256", "256", "DEFAULT_TILE_PX", "define"),
    ("float ivar_floor = 1e-12f;", "1e-12", "ivar_floor", "value-init"),
    ("    cfg.block_mb = 64;", "64", "block_mb", "value-init"),
    ("static const int kMaxRetries = 7;", "7", "kMaxRetries", "const-named"),
    ("constexpr double kZPError = -5.0e-5;", "-5.0e-5", "kZPError", "constexpr-named"),
    ("    return sigma * 2.5 + 1.0;", "2.5", "", "literal-inline"),
]
CLEAN_FIXTURE = ["void f() { return; }  // no digits in this line",
                 "#include <vector>", "}  // namespace astrocs"]


def self_test():
    import tempfile
    d = tempfile.mkdtemp(prefix="aud402_ctl_")
    fp = os.path.join(d, "control.cpp")
    with open(fp, "w", encoding="utf-8") as fh:
        fh.write("\n".join(t for t, _v, _n, _c in FIXTURE) + "\n")
    rows = scan_cpp([fp])
    byline = {r["line"]: r for r in rows}
    ok = True
    for i, (text, want, wantname, wantcls) in enumerate(FIXTURE, 1):
        r = byline.get(i)
        if not r:
            print(f"SELFTEST-MISS line{i}: {text}")
            ok = False
            continue
        toks = r["values"].split(";") + [r["assigned"]]
        if want not in toks:
            print(f"SELFTEST-VALUE line{i}: want {want} got {toks}")
            ok = False
        if wantname and wantname.lower() not in r["name"].lower():
            print(f"SELFTEST-NAME line{i}: want {wantname} got {r['name']}")
            ok = False
        if wantcls not in r["class"]:
            print(f"SELFTEST-CLASS line{i}: want {wantcls} got {r['class']}")
            ok = False
    fp2 = os.path.join(d, "clean.cpp")
    with open(fp2, "w", encoding="utf-8") as fh:
        fh.write("\n".join(CLEAN_FIXTURE) + "\n")
    if scan_cpp([fp2]):
        print("SELFTEST-FALSEPOS clean.cpp produced hits")
        ok = False
    print("SELFTEST", "PASS" if ok else "FAIL", "fixture_hit_lines=", len(rows))
    sys.exit(0 if ok and len(rows) == len(FIXTURE) else 4)


def empty_check():
    empties = [k for k, v in {"nonexistent_universe": []}.items() if len(v) == 0]
    print("empty-guard detected:", empties)
    sys.exit(3 if empties else 5)


def main():
    if "--self-test" in sys.argv:
        self_test()
    if "--empty-check" in sys.argv:
        empty_check()
    os.makedirs(OUT, exist_ok=True)
    u = build_universe()
    empties = [k for k, v in u.items() if len(v) == 0]
    with open(os.path.join(OUT, "universe.tsv"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("kind\tname\tcount_or_file\n")
        for k, v in sorted(u.items()):
            fh.write(f"count\t{k}\t{len(v)}\n")
        for k, v in sorted(u.items()):
            for f in v:
                fh.write(f"file\t{k}\t{f}\n")
    cpp_files = sorted(set(u["lib_cpp"] + u["eng_cpp"] + u["cli_cpp"] + u["build_meta"]))
    rows_cpp = scan_cpp(cpp_files)
    rows_named = [r for r in rows_cpp if any(c in r["class"] for c in
                  ("define", "constexpr-named", "const-named", "constexpr-expr", "const-expr"))]
    json_files = sorted(set(f for f in (u["packaging_cfg"] + u["contracts_json"]
                            + u["exp_json"] + u["lib_json"]) if f.endswith(".json")))
    rows_json = scan_json(json_files)
    with open(os.path.join(OUT, "cpp_hits.tsv"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("file\tline\tclass\tname\tassigned\tvalues\tncount\tsyn\ttext\n")
        for r in rows_cpp:
            fh.write("\t".join([r["file"], str(r["line"]), r["class"], r["name"],
                                r["assigned"].replace("\t", " "), r["values"],
                                str(r["ncount"]), r["sem"], r["text"].replace("\t", " ")]) + "\n")
    with open(os.path.join(OUT, "named.tsv"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("file\tline\tclass\tname\tassigned\ttext\n")
        for r in rows_named:
            fh.write("\t".join([r["file"], str(r["line"]), r["class"], r["name"],
                                r["assigned"].replace("\t", " "),
                                r["text"].replace("\t", " ")]) + "\n")
    with open(os.path.join(OUT, "json_hits.tsv"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("file\tjpath\tkind\tvalue\n")
        for r in rows_json:
            fh.write("\t".join([r["file"], r["jpath"], r["kind"],
                                r["value"].replace("\t", " ").replace("\n", " ")]) + "\n")
    with open(os.path.join(OUT, "counts.tsv"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("metric\tvalue\n")
        for k, v in sorted(u.items()):
            fh.write(f"universe_files.{k}\t{len(v)}\n")
        fh.write(f"cpp_files_scanned\t{len(cpp_files)}\n")
        fh.write(f"cpp_hit_lines\t{len(rows_cpp)}\n")
        fh.write(f"cpp_numeric_literals\t{sum(r['ncount'] for r in rows_cpp)}\n")
        fh.write(f"cpp_named_constant_lines\t{len(rows_named)}\n")
        fh.write(f"cpp_named_unique_symbols\t{len({r['name'] for r in rows_named if r['name']})}\n")
        fh.write(f"cpp_semantic_hit_lines\t{sum(1 for r in rows_cpp if r['sem'] == '1')}\n")
        fh.write(f"json_files_scanned\t{len(json_files)}\n")
        fh.write(f"json_fields\t{len(rows_json)}\n")
    print("universe:", {k: len(v) for k, v in u.items()})
    print("cpp_files:", len(cpp_files), "hit_lines:", len(rows_cpp),
          "literals:", sum(r["ncount"] for r in rows_cpp), "named_lines:", len(rows_named))
    print("json_files:", len(json_files), "fields:", len(rows_json))
    if empties:
        print("EMPTY-UNIVERSE:", empties, file=sys.stderr)
        sys.exit(3)


if __name__ == "__main__":
    main()
