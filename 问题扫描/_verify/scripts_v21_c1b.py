
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
# pattern: a variable named rc/status/err_code/st/ret assigned from a call, then never compared before being reassigned or scope end
rcdecl = re.compile(r'^\s*(?:int|acs_status|long|status_t|std::uint32_t|uint32_t|auto)\s+(rc|ret|st|status|err_code|error_code|ec)\s*=\s*([^;]+);\s*$')
cmpwords = re.compile(r'\b(rc|ret|st|status|err_code|error_code|ec)\b\s*(?:!=|==|>=|<=|>|<)|(?:!=|==|>=|<=)\s*\b\1\b|\bif\s*\(\s*!?(?:\1)\s*\)|\breturn\s+\1\b|\b(\1)\s*\)', re.M)
out=[]
for f in prod:
    try: lines = open(f, encoding='utf-8', errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines):
        m = rcdecl.match(L)
        if not m: continue
        var, rhs = m.group(1), m.group(2)
        # look ahead up to 60 lines for any use of var
        used=None; reassigned=None
        for j in range(i+1, min(i+60, len(lines))):
            seg = lines[j]
            if re.search(r'\b'+var+r'\b', seg):
                # is it a compare/use?
                if re.search(r'\b'+var+r'\b\s*(?:==|!=|>=|<=|>|<|\|=|&&|\?)', seg) or re.search(r'(?:==|!=|>=|<=|>|<)\s*\b'+var+r'\b', seg) or re.search(r'\breturn\b[^;]*\b'+var+r'\b', seg) or re.search(r'\bif\s*\([^)]*\b'+var+r'\b', seg) or re.search(r'\b'+var+r'\b\s*[,)]', seg) or seg.strip().startswith('//'):
                    used=j+1; break
                if rcdecl.match(seg): reassigned=j+1; break
            if re.match(r'^\s*\}', seg) and len(seg)-len(seg.lstrip())<=len(L)-len(L.lstrip()): break
        if used is None:
            out.append((f,i+1,L.strip()[:150], 'REASSIGNED@'+str(reassigned) if reassigned else 'NO-USE-60L'))
print('rc-like decl with no visible use within 60 lines:', len(out))
for f,i,s,t in out:
    print(f+':'+str(i)+'  ('+t+')  '+s)
