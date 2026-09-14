
import os, re, ast, json, sys
ROOT=os.getcwd()
EXCL = re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports)(/|$)')

# 1) 收集所有公共头里的 extern "C" 导出函数名（按头文件）
hdr_syms = {}
for dirpath, dirnames, filenames in os.walk(ROOT):
    rel = os.path.relpath(dirpath, ROOT)
    if EXCL.search(rel+'/'): continue
    for fn in filenames:
        if not fn.endswith('.h'): continue
        p = os.path.join(rel, fn)
        try: txt = open(p, encoding='utf-8', errors='replace').read()
        except Exception: continue
        if 'EXPORT' not in txt: continue
        names = set()
        for m in re.finditer(r'\b(\w*_EXPORT|\w*_API|\w*_DECL)\b', txt): pass
        # 抓 "XXX_EXPORT 返回类型 函数名(" 形式
        for m in re.finditer(r'\b[A-Z][A-Z0-9_]*_(EXPORT|API|CALL)\b[\s\w\*]*?\b(\w+)\s*\(', txt):
            names.add(m.group(2))
        # 也抓 extern "C" 块内普通声明
        if names: hdr_syms[p] = names
all_exported = set()
for p, ns in hdr_syms.items(): all_exported |= ns
print(f'### 带 *_EXPORT/*_API 宏声明的头 {len(hdr_syms)} 个，导出符号 {len(all_exported)} 个')

# 2) 抓所有 .def 文件
defs = {}
for dirpath, dirnames, filenames in os.walk(ROOT):
    rel = os.path.relpath(dirpath, ROOT)
    if EXCL.search(rel+'/'): continue
    for fn in filenames:
        if fn.endswith('.def'):
            p=os.path.join(rel,fn); t=open(p,encoding='utf-8',errors='replace').read()
            defs[p]=set(re.findall(r'^\s*(\w+)\s*(?:@\d+)?\s*$', t, flags=re.M))
print('### .def 文件:', {k:len(v) for k,v in defs.items()})

# 3) 收集 Python ctypes 引用的函数属性名
PY_CT = ['lib/plate_solve/tools/diag_gaia_psf_projection.py',
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
 'lib/snr_estimator/test/test_snr_estimator.py','tests/io/test_hips_input_contract.py',
 'lib/astro_image_io/tests/test_healpix_io.py','lib/astro_image_io/tests/test_psf_fit_handler.py']
REF = re.compile(r'\b([a-z_][a-z0-9_]*)\.([a-z_][a-z0-9_]{3,})\s*(?:\.restype|\.argtypes|\()')
print('\n### Python 引用的 C 符号 ↔ 头文件导出表 比对')
missing = {}
for p in PY_CT:
    fp=os.path.join(ROOT,p)
    if not os.path.exists(fp): print('  (文件不存在)', p); continue
    txt=open(fp,encoding='utf-8',errors='replace').read()
    refs=set()
    for m in REF.finditer(txt):
        obj,fn = m.group(1), m.group(2)
        if obj in ('os','sys','self','np','pt','args','path','re','json','math','aio','ipv','gaia','sdet','dpsf','pc','lib','dll','so','mod','h','t','f'):
            pass
        if fn.startswith(('aio_','ipv_','gaia_','sdet_','dpsf_','pc_','acs_','hp_','xpsd_')):
            refs.add(fn)
    unknown = sorted(r for r in refs if r not in all_exported)
    print(f'  {p}: 引用 {len(refs)} 个 C 符号; 头文件导出表无声明: {unknown}')
    for u in unknown: missing.setdefault(u, []).append(p)
print('\n### 汇总：被 ctypes 引用但任何 *_EXPORT 头里都无声明的符号')
for s, ps in sorted(missing.items()):
    print(f'  {s}  <- {ps}')
