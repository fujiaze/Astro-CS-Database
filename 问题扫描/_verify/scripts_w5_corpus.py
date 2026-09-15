
import subprocess, re, json, os
root = '/workspace/Astro CS Database'
files = subprocess.run(['git','--no-optional-locks','ls-files'], cwd=root, capture_output=True, text=True).stdout.splitlines()
cpp_ext = ('.cpp','.h','.hpp','.c')
prod = [f for f in files if f.endswith(cpp_ext) and (f.startswith('lib/') or f.startswith('cli/') or f.startswith('include/') or f.startswith('runtime/') or f.startswith('providers/') or f.startswith('modules/'))]
py = [f for f in files if f.endswith('.py') and (f.startswith('tools/') or f.startswith('ci/') or f.startswith('scripts/'))]
test = [f for f in files if f.endswith(cpp_ext) and f.startswith('tests/')]
print('PROD_CPP', len(prod))
print('PROD_PY', len(py))
print('TEST_CPP', len(test))
# exclusion census: which prod dirs are excluded from scope
dirs = {}
for f in files:
    if f.endswith(cpp_ext):
        top = f.split('/')[0]
        dirs[top] = dirs.get(top,0)+1
print('ALL_CPP_BY_TOPDIR', json.dumps(dirs, sort_keys=True))

