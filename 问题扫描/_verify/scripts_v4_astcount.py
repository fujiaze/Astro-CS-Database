import ast, pathlib, re, collections
REPO=pathlib.Path("/workspace/Astro CS Database")
def cases(root, pattern="test*.py"):
    files=sorted(p for p in root.glob(pattern) if p.is_file())
    for sub in sorted(root.iterdir()):
        if sub.is_dir() and (sub/"__init__.py").is_file():
            files+=cases(sub, pattern)
    tot=0; per={}
    for f in files:
        try: tree=ast.parse(f.read_text(encoding='utf-8', errors='ignore'))
        except SyntaxError as e:
            per[f.name]="SYNTAX-ERR"; continue
        n=0
        for node in ast.walk(tree):
            if isinstance(node, ast.ClassDef):
                bases=[ast.unparse(b) for b in node.bases]
                if any("TestCase" in b for b in bases):
                    n+=sum(1 for x in node.body if isinstance(x,(ast.FunctionDef,ast.AsyncFunctionDef)) and x.name.startswith("test"))
        has_main=any(isinstance(x,(ast.FunctionDef,ast.AsyncFunctionDef)) and x.name=="main" for x in tree.body)
        per[f.name]=(n, "main" if has_main else "-")
        tot+=n
    return tot, per
for d in ["tests/abi","ci/tests","tests/cli","tests/quality"]:
    t,per=cases(REPO/d)
    print(f"### {d}: total TestCase methods = {t}")
    for k,v in per.items(): print("   ", k, v)
