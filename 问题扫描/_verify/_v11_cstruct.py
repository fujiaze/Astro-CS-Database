
import ctypes, re, os, sys, ast

TY = {
 'int':(ctypes.c_int,4,4), 'long':(ctypes.c_long,8,8), 'unsigned int':(ctypes.c_uint,4,4),
 'int32_t':(ctypes.c_int32,4,4), 'uint32_t':(ctypes.c_uint32,4,4),
 'int64_t':(ctypes.c_int64,8,8), 'uint64_t':(ctypes.c_uint64,8,8),
 'short':(ctypes.c_short,2,2), 'unsigned short':(ctypes.c_ushort,2,2),
 'int16_t':(ctypes.c_int16,2,2), 'uint16_t':(ctypes.c_uint16,2,2),
 'int8_t':(ctypes.c_int8,1,1), 'uint8_t':(ctypes.c_uint8,1,1),
 'char':(ctypes.c_char,1,1), 'float':(ctypes.c_float,4,4), 'double':(ctypes.c_double,8,8),
 'size_t':(ctypes.c_size_t,8,8), 'uintptr_t':(ctypes.c_void_p,8,8), 'intptr_t':(ctypes.c_void_p,8,8),
 'bool':(ctypes.c_bool,1,1),
}
PTR_RE = re.compile(r'^(const\s+)?[\w\s\*]+\s*\*')

def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    return s

def parse_typedef(text, name):
    """从 .h 文本里抓 typedef struct {...} NAME; 返回 [(cname, ctype, count, line)]"""
    m = re.search(r'typedef\s+struct[^{]*\{', text)
    # find all struct blocks and pick the one whose typedef name matches
    blocks=[]
    for m in re.finditer(r'typedef\s+struct(?:\s+\w+)?\s*\{(.*?)\}\s*(\w+)\s*;', text, flags=re.S):
        blocks.append((m.group(1), m.group(2), m.start()))
    for body, tname, pos in blocks:
        if tname != name: continue
        line0 = text[:pos].count('\n')+1
        # line offset of each field inside body
        out=[]
        # split body into statements keeping line numbers
        cur_line = line0
        for stmt in re.split(r';', body):
            stmt_clean = stmt
            # advance line counter by newlines consumed so far
            lineno = text[:text.index(body)+pos].count('\n')  # fallback
            s = strip_comments(stmt_clean).strip()
            if not s: continue
            mm = re.match(r'^([\w\s\*]+?)\s*\*?\s*(\w+)\s*\[\s*(\d+)\s*\]$', s)
            arr = None
            if mm:
                base, fld, n = mm.group(1).strip(), mm.group(2), int(mm.group(3))
                star = '*' in s.split(fld)[0]
            else:
                m2 = re.match(r'^([\w\s\*]+?)\s*\*\s*(\w+)$', s)
                m3 = re.match(r'^([\w\s\*]+?)\s+(\w+)$', s)
                if m2: base, fld, star = m2.group(1).strip(), m2.group(2), True; arr=None
                elif m3: base, fld, star = m3.group(1).strip(), m3.group(2), False; arr=None
                else:
                    continue
            base = re.sub(r'\bconst\b|\bvolatile\b|\bstruct\b','',base).strip()
            if star or PTR_RE.match(base+'*'):
                ct,sz,al = ctypes.c_void_p,8,8
            elif base in TY:
                ct,sz,al = TY[base][0], TY[base][1], TY[base][2]
            else:
                # nested struct?
                ct,sz,al = ('NESTED:'+base), None, None
            out.append((fld, ct, arr if arr is not None else 1, sz, al, base+('*' if star else '')))
        return out, line0
    return None, None

def c_layout(fields):
    off=0; maxal=1; lay=[]
    for fld, ct, n, sz, al, raw in fields:
        if sz is None: return None, None, 'NESTED '+raw
        if off % al: off += al - off%al
        lay.append((fld, off, n*sz, raw)); off += n*sz; maxal=max(maxal,al)
    if off % maxal: off += maxal - off%maxal
    return lay, off, maxal

HDRS = {
 'IpvParams':'lib/plate_solve/cpp/ipv/include/ipv_api.h',
 'IpvWcsResult':'lib/plate_solve/cpp/ipv/include/ipv_api.h',
 'AioHipsSnrPoint':'lib/astro_image_io/include/aio_hips.h',
 'AstroSphereTileView':'lib/astro_image_io/include/aio_hips.h',
 'AioHipsDiagTileView':'lib/astro_image_io/include/aio_hips.h',
 'GaiaSpectrumStar':'lib/gaia_xpsd_client/src/gaia_client.h',
 'GaiaStar':'lib/gaia_xpsd_client/src/gaia_client.h',
 'SDetParams':'lib/star_detector/include/star_detector.h',
 'DPSFFitParams':'lib/dynamic_psf/include/dynamic_psf.h',
 'DPSFFitResult':'lib/dynamic_psf/include/dynamic_psf.h',
 'acs_fio_header_v1':'modules/services/io/include/astrocs/io/fits_stream_v1.h',
 'acs_fio_keyword_v1':'modules/services/io/include/astrocs/io/fits_stream_v1.h',
}
cache={}
for name, path in HDRS.items():
    if path not in cache: cache[path]=open(path,encoding='utf-8',errors='replace').read()
    fields, line0 = parse_typedef(cache[path], name)
    if fields is None: print('!! 未解析', name, path); continue
    lay, size, al = c_layout(fields)
    has_ss = any(f[0] in ('struct_size','abi_version','head') for f in fields)
    print(f'{name:24s} {path}:{line0}  fields={len(fields):2d}  sizeof={size}  align={al}  struct_size/abi={"Y" if has_ss else "N"}')
    print('    ' + '  '.join(f'{f}:{o}(+{s})' for f,o,s,r in lay))
