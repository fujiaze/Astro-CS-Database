
import os, re, sys
ROOT=os.getcwd()
EXCL=re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports|docs|问题扫描|Testing)(/|$)')
def strip_cmt(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s

# ---- 1) 从所有 .h 里抽 extern "C" 函数原型的参数个数 ----
PROTO=re.compile(r'\b([A-Za-z_]\w*_?(?:EXPORT|API|CALL)?\s+)?([a-z_][a-z0-9_]{3,})\s*\(([^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*;', re.S)
protos={}
for dp,dn,fn in os.walk(ROOT):
    rel=os.path.relpath(dp,ROOT)
    if EXCL.search(rel+'/'): continue
    for f in fn:
        if not f.endswith('.h'): continue
        p=os.path.join(rel,f)
        try: raw=open(p,encoding='utf-8',errors='replace').read()
        except Exception: continue
        t=strip_cmt(raw)
        for m in PROTO.finditer(t):
            name=m.group(2); argstr=m.group(3)
            line=raw[:m.start()].count('\n')+1
            args=[a.strip() for a in re.split(r',(?![^()]*\))', argstr) if a.strip()]
            if args==['void'] or args==[]: n=0
            elif len(args)==1 and re.match(r'^(void|\.\.\.)$', args[0]): n=0
            else: n=len(args)
            if name.startswith(('aio_','ipv_','gaia_','sdet_','dpsf_','pc_','acs_','hp_','astrocs_','xpsd_')):
                if name not in protos: protos[name]=(n,p,line,argstr)
print(f'### 从公共/模块头抽到 {len(protos)} 个 C 原型')

# ---- 2) 从 Python 抽 argtypes 声明的个数 ----
FILES=['lib/plate_solve/tools/diag_gaia_psf_projection.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py',
 'lib/astro_image_io/tests/hips_direct_smoke.py',
 'lib/astro_image_io/tests/v5_snr_precision_roundtrip.py',
 'lib/astro_image_io/tests/v5_maptile_oracle.py',
 'lib/astro_image_io/tests/hips_mapping_oracle.py',
 'lib/healpix_db/healpix_browser_qt/tests/gen_hips_browser_test.py',
 'tests/io/test_fits_stream_contract.py','tests/io/make_hips_fixture.py','tests/io/hips_output_fixture.py',
 'tests/io/test_hips_input_contract.py','lib/astro_image_io/tests/test_healpix_io.py','lib/astro_image_io/tests/test_psf_fit_handler.py',
 'lib/snr_estimator/test/test_snr_estimator.py','lib/healpix_db/healpix_drizzle/tests/trace_g4_validate.py']
AT=re.compile(r'([A-Za-z_]\w*)\.argtypes\s*=\s*\[(.*?)\]', re.S)
print('\n### arity 比对（Python 声明的参数个数 vs C 原型参数个数）')
bad=[]
for f in FILES:
    p=os.path.join(ROOT,f)
    if not os.path.exists(p): continue
    txt=open(p,encoding='utf-8',errors='replace').read()
    for m in AT.finditer(txt):
        fn=m.group(1); body=strip_cmt(m.group(2))
        items=[x.strip() for x in re.split(r',(?![^\[]*\])', body) if x.strip()]
        line=txt[:m.start()].count('\n')+1
        if fn not in protos:
            continue
        cn, cp, cl, argstr = protos[fn]
        if cn != len(items):
            bad.append((f,line,fn,len(items),cn,cp,cl,argstr))
            print(f'  ★ {f}:{line}  {fn}  PY argtypes={len(items)}  vs  C 原型={cn}  ({cp}:{cl})')
            print(f'      C 参数: {re.sub(chr(10)," ",argstr)[:200]}')
print(f'\n### 合计 arity 不一致 {len(bad)} 处')
for b in bad: print('   ', b[0]+':'+str(b[1]), b[2], f'PY={b[3]} C={b[4]}')
