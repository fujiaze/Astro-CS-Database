
import ctypes, re, sys, json

SIZES = {'int':(4,4),'long':(8,8),'long long':(8,8),'unsigned int':(4,4),'unsigned long':(8,8),
 'int32_t':(4,4),'uint32_t':(4,4),'int64_t':(8,8),'uint64_t':(8,8),
 'int16_t':(2,2),'uint16_t':(2,2),'int8_t':(1,1),'uint8_t':(1,1),
 'char':(1,1),'short':(2,2),'float':(4,4),'double':(8,8),'size_t':(8,8),
 'bool':(1,1),'uintptr_t':(8,8),'intptr_t':(8,8),'void':(1,1)}
NESTED={}

def strip_comments(s):
    s = re.sub(r'/\*.*?\*/',' ',s,flags=re.S)
    s = re.sub(r'//[^\n]*','',s)
    return s

MACRO_DEFAULTS = {'ACS_FIO_KEYWORD_NAME_MAX':9,'ACS_FIO_KEYWORD_VALUE_MAX':72,'ACS_FIO_KEYWORD_COMMENT_MAX':72,
                  'ACS_FIO_NAXIS_MAX':3,'ACS_FIO_HEADER_MAX_CARDS':1024}

def resolve_count(txt):
    txt=txt.strip()
    if txt.isdigit(): return int(txt)
    if txt in MACRO_DEFAULTS: return MACRO_DEFAULTS[txt]
    raise KeyError(txt)

def parse_struct(text, name):
    clean = strip_comments(text)
    off_map = None
    for m in re.finditer(r'typedef\s+struct(?:\s+\w+)?\s*\{(.*?)\}\s*(\w+)\s*;', clean, flags=re.S):
        if m.group(2)!=name: continue
        body=m.group(1)
        decl_line = text[:m.start()].count('\n')+1
        fields=[]
        for stmt in body.split(';'):
            s=re.sub(r'\s+',' ',stmt).strip()
            if not s: continue
            star = '*' in s.split()[-1] if False else bool(re.search(r'\*', s.split(s.split()[-1])[-1] if False else s))
            am = re.match(r'^(.*?)\b(\w+)\s*\[\s*([\w_]+)\s*\]$', s)
            if am:
                base=am.group(1).strip(); fld=am.group(2); cnt=resolve_count(am.group(3)); star = '*' in base
            else:
                sm = re.match(r'^(.*?)\b(\w+)$', s)
                if not sm: print('  !!skip', repr(s)); continue
                base=sm.group(1).strip(); fld=sm.group(2); cnt=1
                star = base.rstrip().endswith('*') or '*' in base
            base = re.sub(r'\bconst\b|\bvolatile\b','',base).replace('*','').strip()
            if star: sz,al,ct = 8,8,'ptr'
            elif base in SIZES: sz,al,ct = SIZES[base][0],SIZES[base][1],base
            elif base in NESTED: sz,al,ct = NESTED[base][0],NESTED[base][1],base
            else: print('  !! 未知类型', repr(base), repr(s)); continue
            fields.append((fld,ct,cnt,sz,al))
        lay=[]; off=0; maxal=1
        for f,c,n,sz,al in fields:
            if off%al: off+=al-off%al
            lay.append((f,c,n,off,n*sz)); off+=n*sz; maxal=max(maxal,al)
        if off%maxal: off+=maxal-off%maxal
        has_ss = any(f in ('struct_size','abi_version') for f,_,_,_,_ in lay)
        NESTED[name]=(off,maxal)
        return lay, off, maxal, has_ss, decl_line
    return None

def show(path,name,note=''):
    txt=open(path,encoding='utf-8',errors='replace').read()
    r=parse_struct(txt,name)
    if not r: print('!! 未找到',name); return
    lay,size,al,has_ss,line=r
    print(f'{name:22s} {path}:{line}  n={len(lay):2d} sizeof={size:5d} align={al} struct_size/abi={"Y" if has_ss else "N"} {note}')
    print('      '+' | '.join(f'{f}:{o}+{s}' for f,c,n,o,s in lay))
    return size,al,has_ss,lay

print('### 锚校验（期望 IpvWcsResult=1560, IpvParams=424, AioHipsSnrPoint=40, AstroSphereTileView=56）')
IPV='lib/plate_solve/cpp/ipv/include/ipv_api.h'
show(IPV,'IpvWcsResult'); show(IPV,'IpvParams')
AIO='lib/astro_image_io/include/aio_hips.h'
show(AIO,'AstroSphereTileView'); show(AIO,'AioHipsSnrPoint'); show(AIO,'AioHipsDiagTileView')
FS='modules/services/io/include/astrocs/io/fits_stream_v1.h'
kw=show(FS,'acs_fio_keyword_v1'); hd=show(FS,'acs_fio_header_v1')
print('### Python 镜像侧 sizeof')
import ctypes as C
class FioKeyword(C.Structure):
    _fields_=[("name",C.c_char*9),("value",C.c_char*72),("comment",C.c_char*72)]
class FioHeader(C.Structure):
    _fields_=[("struct_size",C.c_uint32),("abi_version",C.c_uint32),("bitpix",C.c_int32),("naxis",C.c_int32),("naxis_n",C.c_int64*3),("keyword_count",C.c_int32),("keywords",FioKeyword*1024)]
print('  FioKeyword PY =',C.sizeof(FioKeyword),' vs C =',kw[0])
print('  FioHeader  PY =',C.sizeof(FioHeader),' vs C =',hd[0])
print('    PY FioHeader 布局:', [(f,C.sizeof(t)) for f,t in FioHeader._fields_])
print('    C  FioHeader 布局:', [(f,s) for f,c,n,o,s in hd[3]])
class TH(C.Structure):
    _fields_=[("struct_size",C.c_uint32),("abi_version",C.c_uint32),("on_read_bytes",C.c_void_p),("on_write_bytes",C.c_void_p),("is_cancelled",C.c_void_p),("user_data",C.c_void_p)]
print('  TraceHooks PY =',C.sizeof(TH))
