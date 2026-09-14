
import os, re
ROOT=os.getcwd()
def strip_cmt(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s
EXCL=re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports|docs|问题扫描|Testing)(/|$)')
PROTO=re.compile(r'\b([A-Za-z_]\w*_?(?:EXPORT|API|CALL)\s+)?([a-z_][a-z0-9_]{3,})\s*\(([^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*;', re.S)
protos={}
for dp,dn,fn in os.walk(ROOT):
    rel=os.path.relpath(dp,ROOT)
    if EXCL.search(rel+'/'): continue
    for f in fn:
        if not f.endswith('.h'): continue
        p=os.path.relpath(os.path.join(dp,f),ROOT)
        raw=open(p,encoding='utf-8',errors='replace').read()
        t=strip_cmt(raw)
        for m in PROTO.finditer(t):
            name=m.group(2); a=[x for x in re.split(r',(?![^()]*\))', m.group(3)) if x.strip()]
            n=0 if (not a or (len(a)==1 and a[0].strip() in ('void','...'))) else len(a)
            if name.startswith(('aio_','ipv_','gaia_','sdet_','dpsf_','pc_','acs_','hp_','astrocs_','xpsd_')) and name not in protos:
                protos[name]=(n,p,raw[:m.start()].count('\n')+1)
FILES=['lib/plate_solve/tools/diag_gaia_psf_projection.py','lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py','lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py','lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py','lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py','lib/astro_image_io/tests/hips_direct_smoke.py','lib/astro_image_io/tests/v5_snr_precision_roundtrip.py','lib/astro_image_io/tests/v5_maptile_oracle.py','lib/astro_image_io/tests/hips_mapping_oracle.py','lib/healpix_db/healpix_browser_qt/tests/gen_hips_browser_test.py','tests/io/test_fits_stream_contract.py','tests/io/make_hips_fixture.py','tests/io/hips_output_fixture.py','tests/io/test_hips_input_contract.py','lib/astro_image_io/tests/test_healpix_io.py','lib/astro_image_io/tests/test_psf_fit_handler.py','lib/snr_estimator/test/test_snr_estimator.py','lib/healpix_db/healpix_drizzle/tests/trace_g4_validate.py']
AT=re.compile(r'([A-Za-z_]\w*)\.argtypes\s*=\s*\[(.*?)\]', re.S)
tot=0; matched=0; bad=[]
for f in FILES:
    p=os.path.join(ROOT,f)
    if not os.path.exists(p): continue
    txt=open(p,encoding='utf-8',errors='replace').read()
    for m in AT.finditer(txt):
        tot+=1
        fn=m.group(1); items=[x for x in re.split(r',(?![^\[]*\])', strip_cmt(m.group(2))) if x.strip()]
        if fn not in protos: continue
        matched+=1
        cn,cp,cl=protos[fn]
        if cn!=len(items): bad.append((f, txt[:m.start()].count('\n')+1, fn, len(items), cn, f'{cp}:{cl}'))
print(f'### arity 核查口径分母：Python argtypes 赋值点 {tot} 处；能配到 C 原型 {matched} 处；未配到 {tot-matched} 处（多为 opaque/宏内或原型跨行未匹配）')
print(f'### 参数个数不一致：{len(bad)} 处')
for b in bad: print('  ★', f'{b[0]}:{b[1]}', b[2], f'PY={b[3]} C={b[4]}  权威={b[5]}')
# gate2 消费者用法
t=open('lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py',encoding='utf-8',errors='replace').read()
print('\n### gate2 如何消费 fit_cpp 返回值（n 行 vs n_valid 行）')
for i,l in enumerate(t.split('\n'),1):
    if 'fit_cpp(' in l or 'isfinite' in l or 'psf_ok' in l or 'n_valid' in l:
        print(f'  gate2:{i}: {l.strip()[:130]}')
