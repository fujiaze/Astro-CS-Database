
import subprocess, re, os, json, collections
root='/workspace/Astro CS Database'
files=subprocess.run(['git','--no-optional-locks','ls-files'],cwd=root,capture_output=True,text=True).stdout.splitlines()
cpp_ext=('.cpp','.h','.hpp','.c')
prod=[f for f in files if f.endswith(cpp_ext) and (f.startswith('lib/') or f.startswith('cli/') or f.startswith('include/') or f.startswith('runtime/') or f.startswith('providers/') or f.startswith('modules/'))]
# exclude vendored/archive
def is_vendored(f):
    return ('/archive/' in f or 'third_party' in f or 'nanoflann' in f or '/legacy/' in f or '/browser_qt/' in f)
prod_core=[f for f in prod if not is_vendored(f)]
prod_acr=[f for f in prod if f.startswith('lib/acr/')]
print('CANDIDATE_SET sizes: prod_all=%d prod_core=%d acr=%d vendored=%d' % (len(prod), len(prod_core), len(prod_acr), len(prod)-len(prod_core)))
excl = sorted(set(p for p in prod if is_vendored(p)))
open('/tmp/w5_excluded.txt','w').write('\n'.join('./'+p for p in excl))
print('EXCLUDED_PREFIX ./lib/healpix_db/archive etc count=', len(excl))

# S1: empty catch (brace-matched) reusing previous logic
catch_re=re.compile(r'\bcatch\s*\(([^)]*)\)')
def strip_comments(text):
    # remove // and /* */ comments (rough but ok for body inspection)
    text=re.sub(r'/\*.*?\*/',' ',text,flags=re.S)
    text=re.sub(r'//[^\n]*',' ',text)
    return text
def load(f):
    try: return open(os.path.join(root,f),encoding='utf-8',errors='replace').read()
    except Exception: return ''
s1=[]; s2=[]
for f in prod_core:
    t=load(f)
    if 'catch' not in t: continue
    for m in catch_re.finditer(t):
        i=m.end()
        while i<len(t) and t[i] in ' \t\r\n': i+=1
        if i>=len(t) or t[i]!='{': continue
        depth=0;j=i
        while j<len(t):
            if t[j]=='{':depth+=1
            elif t[j]=='}':
                depth-=1
                if depth==0:break
            j+=1
        body=t[i+1:j]
        line=t[:m.start()].count('\n')+1
        b=strip_comments(body).strip()
        if b=='':
            s1.append((f,line,m.group(1).strip()[:40]))
        else:
            ctrl=re.search(r'\b(return|throw|break|continue|goto)\b',b)
            log=re.search(r'(aio_log|_log|log_|LOG_|Log|warn|WARN|fprintf|printf|cerr|clog|spdlog|set_error|error_|status|fail)',b)
            if log and not ctrl and 'throw' not in b and 'return' not in b:
                s2.append((f,line,m.group(1).strip()[:40]))
print('S1_empty_catch(prod_core)=',len(s1))
for x in s1: print('   ', x[0]+':'+str(x[1]),'|',x[2])
print('S2_catch_log_or_status_only_no_controlflow=',len(s2))

# S5: (void) explicit discard of calls ending with error-ish names
vs=[]
void_re=re.compile(r'\(void\)\s*([A-Za-z_][\w:]*)\s*\(')
for f in prod_core:
    t=load(f)
    for m in void_re.finditer(t):
        line=t[:m.start()].count('\n')+1
        vs.append((f,line,m.group(1)))
errname=[v for v in vs if re.search(r'(close|free|release|destroy|remove|unlink|rename|flush|sync|write|read|commit|abort|wait|join|cancel|delete)',v[2],re.I)]
print('S5_void_discard_total=',len(vs),' error_like=',len(errname))
for x in errname[:40]: print('   ',x[0]+':'+str(x[1]),'|',x[2])

# std::ignore
si=[]
for f in prod_core:
    t=load(f)
    for m in re.finditer(r'std::ignore\s*=',t):
        si.append((f,t[:m.start()].count('\n')+1))
print('std::ignore sites=',len(si))
for x in si[:20]: print('   ',x)

