
import json, os, re, subprocess
MIRRORS = {
 'lib/plate_solve/tools/diag_gaia_psf_projection.py':['IpvParams','IpvWcsResult','SDetParams','DPSFFitParams','GaiaSpectrumStar'],
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py':['SDetParams','DPSFFitParams','DPSFFitResult'],
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py':['SDetParams','DPSFFitParams'],
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py':['GaiaSpectrumStar'],
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py':['GaiaSpectrumStar'],
 'lib/astro_image_io/tests/hips_direct_smoke.py':['AstroSphereTileView','AioHipsSnrPoint'],
 'lib/astro_image_io/tests/v5_snr_precision_roundtrip.py':['AstroSphereTileView','AioHipsSnrPoint'],
 'lib/astro_image_io/tests/v5_maptile_oracle.py':['AstroSphereTileView'],
 'lib/astro_image_io/tests/hips_mapping_oracle.py':['AstroSphereTileView'],
 'lib/healpix_db/healpix_browser_qt/tests/gen_hips_browser_test.py':['AstroSphereTileView','AioHipsSnrPoint'],
 'tests/io/test_fits_stream_contract.py':['acs_fio_header_v1','acs_fio_keyword_v1','acs_fio_trace_hooks_v1'],
 'tests/io/make_hips_fixture.py':['acs_fio_header_v1','acs_fio_keyword_v1'],
 'tests/io/hips_output_fixture.py':['acs_fio_header_v1','acs_fio_keyword_v1'],
}
cfg=json.load(open('ci/checks.json',encoding='utf-8'))
checks=cfg['checks']
cmds=[(' '.join(c.get('command',[]) if isinstance(c.get('command'),list) else str(c.get('command')),)) for c in checks]
allci=' || '.join(x[0] for x in cmds).replace('\\','/')
# 也看 .github workflows
wf=''
for dp,dn,fn in os.walk('.github'):
    for f in fn:
        if f.endswith(('.yml','.yaml')): wf+=open(os.path.join(dp,f),encoding='utf-8',errors='replace').read()
wf=wf.replace('\\','/')
# ctest 注册面（CMake add_test/add_executable 里的 py 脚本名）
cm=''
for root in ['tests','lib','CMakeLists.txt','cmake']:
    for dp,dn,fn in os.walk(root):
        if re.search(r'/(build|run|worktrees)(/|$)', dp+'/'): continue
        for f in fn:
            if f=='CMakeLists.txt' or f.endswith('.cmake'):
                cm+=open(os.path.join(dp,f),encoding='utf-8',errors='replace').read()
print('镜像文件'.ljust(66),'| 在 ci/checks.json | 在 .github | 在 CMake/ctest 注册')
print('-'*118)
for p, structs in MIRRORS.items():
    stem=os.path.basename(p)
    in_ci = stem in allci or p in allci
    in_wf = stem in wf
    in_cm = stem in cm
    print(f'{p:66s} | {"是" if in_ci else "否":^14s} | {"是" if in_wf else "否":^9s} | {"是" if in_cm else "否"}')
print()
print('### UT-IO 是否真能采到 tests/io 的这三个脚本（discover 规则）')
for c in checks:
    if c.get('id')=='UT-IO':
        print('  UT-IO 命令:', ' '.join(c['command']), ' profiles=', c['profiles'], ' waivable=', c.get('waivable'))
print()
print('### 各镜像脚本是否含 unittest.TestCase（决定 discover 能否采到）')
for p in MIRRORS:
    if not os.path.exists(p): print('  (缺)',p); continue
    t=open(p,encoding='utf-8',errors='replace').read()
    ntc=len(re.findall(r'class\s+\w+\s*\(\s*unittest\.TestCase', t))
    ntest=len(re.findall(r'\ndef test_\w+', t))
    main='有 __main__ 直跑' if '__main__' in t else ''
    print(f'  {os.path.basename(p):44s} TestCase类={ntc}  test_函数={ntest}  {main}')
