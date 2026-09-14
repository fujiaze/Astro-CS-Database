
import os, re, ast, json, sys
ROOT=os.getcwd()
targets = [
 'lib/plate_solve/tools/diag_gaia_psf_projection.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py',
 'lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_production_validation.py',
 'lib/astro_image_io/tests/hips_direct_smoke.py',
 'lib/astro_image_io/tests/v5_snr_precision_roundtrip.py',
 'lib/astro_image_io/tests/v5_maptile_oracle.py',
 'lib/astro_image_io/tests/hips_mapping_oracle.py',
 'lib/healpix_db/healpix_browser_qt/tests/gen_hips_browser_test.py',
 'tests/io/test_fits_stream_contract.py',
 'tests/io/make_hips_fixture.py',
 'tests/io/hips_output_fixture.py',
]
for t in targets:
    p=os.path.join(ROOT,t)
    if not os.path.exists(p): print('MISSING', t); continue
    src=open(p,encoding='utf-8',errors='replace').read()
    tree=ast.parse(src)
    print('='*100); print('FILE:', t)
    for node in ast.walk(tree):
        if isinstance(node, ast.ClassDef):
            bases=[ast.unparse(b) for b in node.bases]
            if not any('Structure' in b or 'Union' in b for b in bases): continue
            packed=None; fields=None
            for st in node.body:
                if isinstance(st, ast.AnnAssign): continue
                if isinstance(st, ast.Assign):
                    for tg in st.targets:
                        if isinstance(tg, ast.Name):
                            if tg.id=='_pack_': packed=ast.unparse(st.value)
                            if tg.id=='_fields_': fields=st.value
            print(f'-- class {node.name}(line {node.lineno}) bases={bases} _pack_={packed}')
            if fields is not None:
                try:
                    fl=ast.literal_eval(ast.unparse(fields).replace('ctypes.','').replace('c_double','0').replace('c_int','0').replace('c_char','0').replace('c_void_p','0').replace('c_uint64','0').replace('c_size_t','0').replace('c_char_p','0').replace('c_uint32','0').replace('c_int32','0').replace('c_uint8','0'))
                except Exception as e:
                    fl=None
                if fl is None:
                    print('   RAW:', ast.unparse(fields).replace('\n',' ')[:1400])
                else:
                    for name,ty in fl:
                        if isinstance(ty, list):
                            print(f'   {name}: ARRAY {ty[0]} x {ty[1]}')
                        else:
                            print(f'   {name}: {ty}')
        # POINTER fields show as ast.Subscript -> print raw anyway
