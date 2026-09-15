
import subprocess, re, os, collections
root='/workspace/Astro CS Database'
files=subprocess.run(['git','--no-optional-locks','ls-files'],cwd=root,capture_output=True,text=True).stdout.splitlines()
cpp_ext=('.cpp','.h','.hpp','.c')
def sel(prefixes, excl=None):
    r=[f for f in files if f.endswith(cpp_ext) and any(f.startswith(p) for p in prefixes)]
    if excl: r=[f for f in r if not any(e in f for e in excl)]
    return r
prod=sel(['lib/','cli/','include/','runtime/','providers/','modules/'])
vend=('/archive/','/legacy/','nanoflann','/browser_qt/','/tests/','third_party')
core=[f for f in prod if not any(e in f for e in vend)]
def load(f):
    try: return open(os.path.join(root,f),encoding='utf-8',errors='replace').read()
    except Exception: return ''
def strip(text):
    text=re.sub(r'/\*.*?\*/',' ',text,flags=re.S)
    text=re.sub(r'//[^\n]*',' ',text)
    return text

# ---- S4: JSON default fallback in production code
s4=[]
val_re=re.compile(r'\.value\(\s\"([^\"]+)\"\s*,')
vo_re=re.compile(r'\.value_or\(')
jget=re.compile(r'\bjget(?:_ref)?\s*\(')
for f in core:
    t=load(f); ts=strip(t)
    n1=len(val_re.findall(ts)); n2=len(vo_re.findall(ts)); n3=len(jget.findall(ts))
    if n1+n2+n3: s4.append((f,n1,n2,n3))
tot=[sum(x[i] for x in s4) for i in (1,2,3)]
print('S4 json .value(k,default) total=%d ; value_or=%d ; jget-with-default=%d ; files=%d' % (tot[0],tot[1],tot[2],len(s4)))
s4.sort(key=lambda x:-(x[1]+x[2]+x[3]))
for x in s4[:25]: print('   ',x)

# ---- out-param error codes never read (proxy for S3)
op=[]
pat=re.compile(r'\b(?:bool|error_code|Status|int)&\s+(\w*(?:ok|err|status|rc|error)\w*)',re.I)
for f in core:
    t=strip(load(f))
    for m in re.finditer(r'\b(\w+)\s*\(\s*([^;()]*?(?:&\s*)?\b(ok|sok|err|ec|rc|status|error|s)\b[^;()]*?)\)\s*;',t):
        pass
# simpler: find declared bool ok=false; then count uses of 'ok' after
s3_files=[]
for f in core:
    t=strip(load(f))
    for m in re.finditer(r'\bbool\s+(\w*(?:ok|success|good|valid)\w*)\s*=\s*false\s*;',t,re.I):
        name=m.group(1); line=t[:m.start()].count('\n')+1
        after=t[m.end():]
        if not re.search(r'\b'+re.escape(name)+r'\b', after):
            s3_files.append((f,line,name))
print('S3 out-param-declared-but-never-read =',len(s3_files))
for x in s3_files[:30]: print('   ',x)

# ---- assert() in production code (removed under NDEBUG)
asn=[]
for f in core:
    t=load(f)
    ts=strip(t)
    for m in re.finditer(r'(?<![\w.:])assert\s*\(',ts):
        line=ts[:m.start()].count('\n')+1
        asn.append((f,line))
byfile=collections.Counter(f for f,_ in asn)
print('ASSERT sites in prod core =',len(asn),' files=',len(byfile))
for k,v in byfile.most_common(25): print('   ',k,v)

# ---- retry loops
rt=[]
for f in core:
    t=load(f)
    for m in re.finditer(r'(?i)\b(retry|retries|attempt|max_tries|backoff|重试)\b',t):
        line=t[:m.start()].count('\n')+1
        rt.append((f,line,t.splitlines()[line-1].strip()[:100]))
print('RETRY mentions prod core =',len(rt))
for x in rt[:40]: print('   ',x[0]+':'+str(x[1]),'|',x[2])

