
import json
out=json.load(open('问题扫描/_cache/v20_params3.json'))
for o in out:
    p=o['path']
    if '/third_party/' in p or 'cfitsio' in p: continue
    if 'module_entry' not in p: continue
    print(f"{p}:{o['line']}  {o['fn']}  UNUSED={o['unused']}")
    print(f"    params={o['params'][:170]}")
