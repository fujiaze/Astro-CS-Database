
import subprocess, re, json, os, sys
root = '/workspace/Astro CS Database'
files = subprocess.run(['git','--no-optional-locks','ls-files'], cwd=root, capture_output=True, text=True).stdout.splitlines()
cpp_ext = ('.cpp','.h','.hpp','.c')
prod = [f for f in files if f.endswith(cpp_ext) and (f.startswith('lib/') or f.startswith('cli/') or f.startswith('include/') or f.startswith('runtime/') or f.startswith('providers/') or f.startswith('modules/'))]
py   = [f for f in files if f.endswith('.py') and (f.startswith('tools/') or f.startswith('ci/') or f.startswith('scripts/'))]

def load(f):
    try:
        return open(os.path.join(root,f), encoding='utf-8', errors='replace').read()
    except Exception:
        return ''

# --- C++ catch block extraction: brace matching ---
catch_re = re.compile(r'\bcatch\s*\(([^)]*)\)')
def cpp_catch_sites(text):
    """return list of (line_no, catch_param, body) where body is the {} content (may be empty)."""
    out=[]
    # find each 'catch(' occurrence, then locate the following block
    for m in catch_re.finditer(text):
        # ensure not in comment/string - rough check: preceded by try or whitespace only on line
        i = m.end()
        # skip whitespace
        while i < len(text) and text[i] in ' \t\r\n': i+=1
        if i>=len(text) or text[i]!='{':
            # catch without block? unlikely; record as no-brace
            body=None
        else:
            depth=0; j=i
            while j < len(text):
                if text[j]=='{': depth+=1
                elif text[j]=='}':
                    depth-=1
                    if depth==0: break
                j+=1
            body = text[i+1:j]
        line = text[:m.start()].count('\n')+1
        out.append((line, m.group(1).strip(), body))
    return out

empty_catch=[]; catch_log_only=[]; catch_total=0
for f in prod:
    t=load(f)
    if 'catch' not in t: continue
    for (ln, param, body) in cpp_catch_sites(t):
        catch_total+=1
        if body is None: continue
        b = body.strip()
        # strip comments crudely
        b2 = re.sub(r'//[^\n]*','',b)
        b2 = re.sub(r'/\*.*?\*/','',b2, flags=re.S).strip()
        if b2=='':
            empty_catch.append((f,ln,param))
        else:
            # log-only: body contains only logging/ignore, no return/throw/status set
            has_control = re.search(r'\b(return|throw|continue|break|goto)\b', b2)
            has_log = re.search(r'(log_|Log|WARN|warn|fprintf|printf|cerr|clog|spdlog|LOG)', b2)
            if has_log and not has_control:
                catch_log_only.append((f,ln,param,b2[:120]))

print('CPP_CATCH_TOTAL', catch_total)
print('S1_EMPTY_CATCH', len(empty_catch))
for x in empty_catch[:40]: print('  EMPTY', x[0]+':'+str(x[1]), '|', x[2][:60])
print('S2_LOG_ONLY_CATCH', len(catch_log_only))
for x in catch_log_only[:40]: print('  LOGONLY', x[0]+':'+str(x[1]), '|', x[2][:40], '|', x[3][:80])

# --- Python ---
import ast
py_bare=[]; py_except_pass=[]; py_except_log=[]; py_except_total=0
for f in py:
    t=load(f)
    try: tree=ast.parse(t)
    except Exception as e:
        py_bare.append((f,0,'PARSE_FAIL:'+str(e)[:40])); continue
    for node in ast.walk(tree):
        if isinstance(node, ast.ExceptHandler):
            py_except_total+=1
            body=node.body
            # all pass / continue / Ellipsis
            if all(isinstance(s,(ast.Pass,)) for s in body):
                py_except_pass.append((f,node.lineno, ast.unparse(node.type) if node.type else 'BARE'))
            elif all(isinstance(s,ast.Continue) for s in body):
                py_except_pass.append((f,node.lineno,'CONTINUE_ONLY:'+(ast.unparse(node.type) if node.type else 'BARE')))
            if node.type is None:
                py_bare.append((f,node.lineno,'BARE_EXCEPT'))
print('PY_EXCEPT_TOTAL', py_except_total)
print('PY_EXCEPT_PASS/CONTINUE', len(py_except_pass))
for x in py_except_pass[:40]: print('  PYPASS', x[0]+':'+str(x[1]), '|', x[2])
print('PY_BARE_EXCEPT', len(py_bare))
for x in py_bare[:40]: print('  BARE', x[0]+':'+str(x[1]))

