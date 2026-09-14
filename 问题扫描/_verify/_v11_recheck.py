
import ctypes as C, re, os
ROOT=os.getcwd()
def sc(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s
def block_at(t,i):
    d=0
    for k in range(i,len(t)):
        if t[k]=='{': d+=1
        elif t[k]=='}':
            d-=1
            if d==0: return k
    return -1
TY=re.compile(r'typedef\s+struct(?:\s+\w+)?\s*\{')
SZ={'int':(4,4),'int32_t':(4,4),'uint32_t':(4,4),'int64_t':(8,8),'uint64_t':(8,8),'float':(4,4),'double':(8,8),'char':(1,1),'uint8_t':(1,1),'int16_t':(2,2),'uint16_t':(2,2),'size_t':(8,8),'long':(8,8)}
def cstruct(path,name,env=None):
    raw=open(path,encoding='utf-8',errors='replace').read(); t=sc(raw)
    for m in TY.finditer(t):
        oi=m.end()-1; ci=block_at(t,oi)
        if ci<0: continue
        nm=re.match(r'\}\s*(\w+)\s*;', t[ci:ci+90])
        if not nm or nm.group(1)!=name: continue
        fields=[]
        for stmt in t[oi+1:ci].split(';'):
            s=re.sub(r'\s+',' ',stmt).strip()
            if not s: continue
            am=re.match(r'^(.*?)\b(\w+)\s*\[\s*([\w_]+)\s*\]$', s)
            if am:
                base=am.group(1).strip(); fld=am.group(2)
                n=am.group(3)
                n=int(n) if n.isdigit() else (env or {}).get(n)
                if n is None: print('   ??宏', n); continue
                star = '*' in base
            else:
                mm=re.match(r'^(.*?)\b(\w+)$', s)
                if not mm: continue
                base=mm.group(1).strip(); fld=mm.group(2); n=1
                star = base.rstrip().endswith('*')
            base=re.sub(r'\bconst\b|\bvolatile\b','',base).replace('*','').strip()
            if star: sz,al=8,8; ct='ptr'
            elif base in SZ: sz,al=SZ[base]; ct=base
            else: print('   ??类型', base); continue
            fields.append((fld,ct,n,sz,al))
        off=0; ma=1; lay=[]
        for fld,ct,n,sz,al in fields:
            if off%al: off+=al-off%al
            lay.append((fld,ct,off,n*sz)); off+=n*sz; ma=max(ma,al)
        if off%ma: off+=ma-off%ma
        return lay, off, raw[:m.start()].count('\n')+1
    return None,None,None
MAC={'ACS_FIO_KEYWORD_NAME_MAX':9,'ACS_FIO_KEYWORD_VALUE_MAX':72,'ACS_FIO_KEYWORD_COMMENT_MAX':72,'ACS_FIO_NAXIS_MAX':3,'ACS_FIO_HEADER_MAX_CARDS':1024}
class KW(C.Structure):
    _fields_=[("name",C.c_char*9),("value",C.c_char*72),("comment",C.c_char*72)]
class FH(C.Structure):
    _fields_=[("struct_size",C.c_uint32),("abi_version",C.c_uint32),("bitpix",C.c_int32),("naxis",C.c_int32),("naxis_n",C.c_int64*3),("keyword_count",C.c_int32),("keywords",KW*1024)]
PAIRS=[
 ('IpvParams','lib/plate_solve/cpp/ipv/include/ipv_api.h','lib/plate_solve/tools/diag_gaia_psf_projection.py',None),
 ('IpvWcsResult','lib/plate_solve/cpp/ipv/include/ipv_api.h','lib/plate_solve/tools/diag_gaia_psf_projection.py',None),
 ('AioHipsSnrPoint','lib/astro_image_io/include/aio_hips.h','lib/astro_image_io/tests/hips_direct_smoke.py',None),
 ('AioHipsSnrPoint','lib/astro_image_io/include/aio_hips.h','lib/astro_image_io/tests/v5_snr_precision_roundtrip.py',None),
 ('AstroSphereTileView','lib/astro_image_io/include/aio_hips.h','lib/astro_image_io/tests/hips_direct_smoke.py',None),
 ('GaiaSpectrumStar','lib/gaia_xpsd_client/src/gaia_client.h','lib/plate_solve/tools/diag_gaia_psf_projection.py',None),
 ('GaiaSpectrumStar','lib/gaia_xpsd_client/src/gaia_client.h','lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py',None),
 ('SDetParams','lib/star_detector/include/star_detector.h','lib/plate_solve/tools/diag_gaia_psf_projection.py',None),
 ('DPSFFitParams','lib/dynamic_psf/include/dynamic_psf.h','lib/plate_solve/tools/diag_gaia_psf_projection.py',None),
 ('DPSFFitResult','lib/dynamic_psf/include/dynamic_psf.h','lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py',None),
 ('acs_fio_header_v1','modules/services/io/include/astrocs/io/fits_stream_v1.h','tests/io/test_fits_stream_contract.py',FH),
]
# 取 Python 侧 sizeof
import ast
def py_size(path, cls):
    src=open(path,encoding='utf-8').read()
    tree=ast.parse(src)
    for nd in ast.walk(tree):
        if isinstance(nd, ast.ClassDef) and nd.name==cls:
            env=dict(globals()); env['ctypes']=C
            ns={'ctypes':C,'C':C}
            for k,v in MAC.items(): ns[k]=v
            try: exec(compile(ast.Module(body=[nd],type_ignores=[]),'<m>','exec'), ns)
            except Exception as e: return None, f'exec失败:{e}'
            return C.sizeof(ns[cls]), ''
    return None,'类未找到'
print(f'{"C 结构":22s} {"C头:行":52s} {"C sizeof":>8s} | {"镜像文件":58s} {"PY sizeof":>9s} {"Δ":>5s} 判定')
for cname, hdr, pyf, pycls in PAIRS:
    lay,size,line = cstruct(hdr,cname,MAC)
    if size is None: print(f'{cname}: 头未解析'); continue
    ss = any(l[0] in ('struct_size','abi_version') for l in lay)
    psize, err = py_size(pyf, pycls or cname)
    d = (size-psize) if psize else None
    verdict = '一致' if d==0 else f'★漂移 Δ={d}'
    print(f'{cname+("" if ss else " *无自描述"):40s} {hdr+":"+str(line):52s} {size:8d} | {pyf:58s} {str(psize):>9s} {str(d):>5s} {verdict}{err}')
