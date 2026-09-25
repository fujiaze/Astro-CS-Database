"""AUDIT-06 arch-v W3 helper: derive, from CMake SOURCE TEXT ONLY, which .cpp/.c/.inc
files are compiled by a *production* target.

No cmake invocation, no configure, no build. This parses add_library()/add_executable()
source lists and target_link_libraries() from every tracked CMakeLists.txt / .cmake,
then takes the link closure of the shipped executables plus installed SHARED libs.

Anything it cannot resolve (variables, generator expressions, subprojects) is reported
as `unresolved`, never guessed into the production set.
"""
import io
import os
import re
from collections import defaultdict

ADD_RE = re.compile(r"\b(add_library|add_executable)\s*\(", re.I)
LINK_RE = re.compile(r"\btarget_link_libraries\s*\(", re.I)
INSTALL_RE = re.compile(r"\binstall\s*\(\s*(TARGETS|FILES|PROGRAMS)", re.I)
LIB_PROP_RE = re.compile(r"set_target_properties\s*\(", re.I)


def _balanced(text, open_idx):
    """Return body between the '(' at open_idx and its matching ')'."""
    i = open_idx
    depth = 0
    start = None
    while i < len(text):
        ch = text[i]
        if ch == "(":
            depth += 1
            if depth == 1:
                start = i + 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start:i]
        i += 1
    return text[start:] if start else ""


def strip_comments(text):
    return "\n".join(re.sub(r"^\s*#.*$", "", l) for l in text.splitlines())


def split_args(body):
    return [a for a in body.split() if a]


class Graph:
    def __init__(self):
        self.sources = defaultdict(set)      # target -> {repo-rel files}
        self.deps = defaultdict(set)         # target -> {linked targets}
        self.kind = {}                       # target -> STATIC/SHARED/EXE
        self.installed = set()
        self.files_by_dir = defaultdict(list)
        self.unresolved = defaultdict(set)   # target -> {tokens we could not resolve}


def norm(path, base):
    p = path.replace("\\", "/")
    if p.startswith("${") or p.startswith("$<"):
        return None
    full = p if os.path.isabs(p) else os.path.normpath(os.path.join(base, p)).replace("\\", "/")
    return full


def parse_repo(root):
    g = Graph()
    cmake_files = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "run", "node_modules")]
        for fn in filenames:
            if fn == "CMakeLists.txt" or fn.endswith(".cmake"):
                rel = os.path.relpath(os.path.join(dirpath, fn), root).replace("\\", "/")
                cmake_files.append(rel)
    cfvars = {}
    # pass 0: collect explicit set(... ... CACHE/ FORCE) string/file lists
    for rel in cmake_files:
        txt = strip_comments(io.open(os.path.join(root, rel), encoding="utf-8",
                                     errors="replace").read())
        for m in re.finditer(r"\bset\s*\(\s*(\w+)\s+(.*?)(?:\)|\s+CACH)", txt, re.S | re.I):
            name, body = m.group(1), m.group(2)
            vals = [v.strip("\"") for v in split_args(body)
                    if not v.startswith("#")]
            if vals and all(re.search(r"\.(c|cpp|cc|inc)$", v) for v in vals):
                cfvars[name] = vals
    for rel in cmake_files:
        base = os.path.dirname(rel)
        txt = strip_comments(io.open(os.path.join(root, rel), encoding="utf-8",
                                     errors="replace").read())
        for m in ADD_RE.finditer(txt):
            body = _balanced(txt, m.end() - 1)
            args = split_args(body)
            if not args:
                continue
            tgt = args[0]
            rest = args[1:]
            kw = [r for r in rest if r.upper() in
                  ("STATIC", "SHARED", "MODULE", "OBJECT", "INTERFACE", "EXCLUDE_FROM_ALL",
                   "IMPORTED", "ALIAS", "WIN32", "MACOSX_BUNDLE")]
            if "INTERFACE" in kw:
                g.kind[tgt] = "INTERFACE"
            elif m.group(1).lower() == "add_executable":
                g.kind[tgt] = "EXE"
            elif "SHARED" in kw or "MODULE" in kw:
                g.kind[tgt] = "SHARED"
            elif "STATIC" in kw:
                g.kind[tgt] = "STATIC"
            else:
                g.kind[tgt] = "UNKNOWN"
            for a in rest:
                if a.upper() in ("PRIVATE", "PUBLIC", "INTERFACE", "STATIC", "SHARED",
                                 "MODULE", "OBJECT", "EXCLUDE_FROM_ALL", "IMPORTED",
                                 "ALIAS", "WIN32", "MACOSX_BUNDLE"):
                    continue
                if a.startswith("$<") or a.startswith("\""):
                    g.unresolved[tgt].add(a)
                    continue
                if re.search(r"\.(c|cpp|cc|cxx|inc|h|hpp)$", a):
                    p = norm(a, base)
                    if p:
                        g.sources[tgt].add(p.lstrip("/"))
                    else:
                        g.unresolved[tgt].add(a)
                elif a.startswith("${"):
                    var = re.match(r"\$\{(\w+)\}", a)
                    if var and var.group(1) in cfvars:
                        for v in cfvars[var.group(1)]:
                            p = norm(v, base)
                            if p:
                                g.sources[tgt].add(p.lstrip("/"))
                    else:
                        g.unresolved[tgt].add(a)
                else:
                    g.deps[tgt].add(a)
        for m in LINK_RE.finditer(txt):
            body = _balanced(txt, m.end() - 1)
            args = [a for a in split_args(body)
                    if a.upper() not in ("PRIVATE", "PUBLIC", "INTERFACE")]
            tgt = args[0] if args else None
            if not tgt:
                continue
            for a in args[1:]:
                if a.startswith("$") or a.startswith("-") or a.startswith("debug"):
                    g.unresolved[tgt].add(a)
                else:
                    g.deps[tgt].add(a)
        # install(TARGETS ...) -> installed set
        for m in INSTALL_RE.finditer(txt):
            body = _balanced(txt, m.end() - 1)
            for a in split_args(body):
                if re.match(r"^[A-Za-z_][\w:]*$", a) and a.upper() not in (
                        "DESTINATION", "RUNTIME", "LIBRARY", "ARCHIVE", "PUBLIC_HEADER",
                        "RESOURCE", "FILE_SET", "COMPONENT", "OPTIONAL", "INCLUDES",
                        "EXPORT", "NAMELINK_COMPONENT", "PERMISSIONS", "CONFIGURATIONS"):
                    g.installed.add(a)
    return g


TESTISH = re.compile(r"(^|[_\-.])(test|tests|gtest|selftest|selfcheck|probe|example|bench|"
                     r"oracle|fixture|conformance|mock|spy|echo|noop)([_\-.]|$)|"
                     r"_test$|^test_|^ut_|^rt[0-9]", re.I)


def is_testish(t):
    return bool(TESTISH.search(t))


def production_target_names(root, g):
    """Production targets = the shipped CLI + everything it links transitively +
    installed SHARED/EXE units that are not test-shaped."""
    seeds = set()
    for t, k in g.kind.items():
        if k == "EXE" and not is_testish(t) and t in g.installed:
            seeds.add(t)
    seeds |= {t for t in g.installed if g.kind.get(t) == "SHARED" and not is_testish(t)}
    for exe in ("acsd", "astrocs"):
        if g.kind.get(exe) == "EXE":
            seeds.add(exe)
    out = set()
    stack = set(seeds)
    while stack:
        t = stack.pop()
        if t in out:
            continue
        out.add(t)
        stack |= set(g.deps.get(t, ())) - out
    return seeds, out


def production_source_set(root):
    g = parse_repo(root)
    _seeds, prod = production_target_names(root, g)
    srcs = set()
    for t in prod:
        srcs |= g.sources.get(t, set())
    # text-included .cpp/.inc: a compiled host TU drags its quoted .cpp/.inc bodies in
    hosts = {s for s in srcs if re.search(r"\.(cpp|cc|cxx)$", s)}
    dirmap = defaultdict(set)
    for h in hosts:
        dirmap[os.path.dirname(h)].add(h)
    for h in hosts:
        txt = _read(root, h)
        for m in re.finditer(r'#\s*include\s+"([^"]+\.(?:cpp|cc|inc))"', txt):
            inc = m.group(1)
            cand = os.path.normpath(os.path.join(os.path.dirname(h), inc)).replace("\\", "/")
            if os.path.exists(os.path.join(root, cand)):
                srcs.add(cand)
            else:
                for d in {os.path.dirname(x) for x in _all_c_source_files(root)}:
                    c2 = os.path.normpath(os.path.join(d, inc)).replace("\\", "/")
                    if os.path.exists(os.path.join(root, c2)):
                        srcs.add(c2)
    return srcs


_CACHE = {}


def _read(root, rel):
    try:
        return io.open(os.path.join(root, rel), encoding="utf-8",
                       errors="replace").read()
    except OSError:
        return ""


def _all_c_source_files(root):
    if "src" not in _CACHE:
        s = set()
        for dp, dn, fn in os.walk(root):
            dn[:] = [d for d in dn if d not in (".git", "build", "run", "node_modules")]
            for f in fn:
                if re.search(r"\.(c|cpp|cc|cxx)$", f):
                    s.add(os.path.relpath(os.path.join(dp, f), root).replace("\\", "/"))
        _CACHE["src"] = s
    return _CACHE["src"]


if __name__ == "__main__":
    import sys
    R = sys.argv[1] if len(sys.argv) > 1 else r"F:\Astro dev\Astro CS Normalization Database"
    g = parse_repo(R)
    seeds, prod = production_target_names(R, g)
    print("production targets (%d):" % len(prod))
    for t in sorted(prod):
        print("   %-40s kind=%-8s sources=%d %s" % (
            t, g.kind.get(t, "?"), len(g.sources.get(t, ())),
            "INSTALLED" if t in g.installed else ""))
    print("\nnon-production (test/other) targets seen:", len([t for t in g.kind if t not in prod]))
    ps = production_source_set(R)
    print("\nproduction-compiled source files (%d)" % len(ps))
    for probe in ("lib/infrastructure/scheduler/src/executor.cpp",
                  "lib/infrastructure/scheduler/src/module_adapters.cpp",
                  "lib/infrastructure/scheduler/src/scheduler.cpp",
                  "lib/infrastructure/scheduler/src/normalize_workflow.cpp",
                  "lib/infrastructure/scheduler/src/mosaic_window.cpp",
                  "lib/infrastructure/scheduler/src/export_stream.cpp",
                  "lib/infrastructure/pipeline/module_loader/secure_loader.c",
                  "lib/infrastructure/pipeline/module_loader/module_registry.c"):
        print("   %-62s %s" % (probe, "PROD" if probe in ps else "not-production"))
