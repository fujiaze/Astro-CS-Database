
import ctypes, re, sys

SIZES = {
 'int':(4,4),'long':(8,8),'long long':(8,8),'unsigned int':(4,4),'unsigned long':(8,8),
 'int32_t':(4,4),'uint32_t':(4,4),'int64_t':(8,8),'uint64_t':(8,8),
 'int16_t':(2,2),'uint16_t':(2,2),'int8_t':(1,1),'uint8_t':(1,1),
 'char':(1,1),'short':(2,2),'float':(4,4),'double':(8,8),'size_t':(8,8),
 'bool':(1,1),'uintptr_t':(8,8),'intptr_t':(8,8),'void':(1,1),
}
def strip_comments(s):
    s = re.sub(r'/\*.*?\*/',' ',s,flags=re.S)
    s = re.sub(r'//[^\n]*','',s)
    return s

NESTED = {}   # name -> (size, align)

def parse_struct(text, name):
    """返回 (fields, decl_line)。fields=[(field, ctype_str, count, size, align)]"""
    for m in re.finditer(r'typedef\s+struct(?:\s+\w+)?\s*\{(.*?)\}\s*(\w+)\s*;', text, flags=re.S):
        if m.group(2) != name: continue
        body = m.group(1); body_start = m.start(1)
        decl_line = text[:m.start()].count('\n')+1
        fields=[]
        for stmt in body.split(';'):
            if not stmt.strip(): continue
            stmt_line = text[:body_start].count('\n') + body[:body.index(stmt)+len(stmt)].count('\n')  # approx, unused
            s = strip_comments(stmt).strip()
            if not s: continue
            s = re.sub(r'\s+', ' ', s)
            # arrays: "double sip_a[36]"  / "char log_dir[256]"
            am = re.match(r'^(.*?)\b(\w+)\s*\[\s*(\d+)\s*\]$', s)
            star = False
            if am:
                base = am.group(1).strip(); fld = am.group(2); count = int(am.group(3))
            else:
                sm = re.match(r'^(.*?)\b(\w+)\s*$', s)
                if not sm:
                    print('   !! 无法解析语句:', repr(s)); continue
                base = sm.group(1).strip(); fld = sm.group(2); count = 1
                star = base.endswith('*') or '*' in base.split()[-1] if base else False
                star = base.rstrip().endswith('*')
                base = base.replace('*','').strip()
            if star:
                size, align = 8,8; ct='void*'
            else:
                base = re.sub(r'\bconst\b|\bvolatile\b','',base).strip()
                if base in SIZES: size, align = SIZES[base]
                elif base in NESTED: size, align = NESTED[base]
                else:
                    print('   !! 未知类型', repr(base), '在', repr(s)); continue
                ct = base
            fields.append((fld, ct, count, size, align))
        return fields, decl_line
    return None, None

def layout(fields):
    off=0; maxal=1; lay=[]
    for fld, ct, n, sz, al in fields:
        if off % al: off += al - off%al
        lay.append((fld, ct, n, off, n*sz)); off += n*sz; maxal=max(maxal, al)
    if off % maxal: off += maxal - off%maxal
    return lay, off, maxal

def show(path, name):
    txt = open(path, encoding='utf-8', errors='replace').read()
    fs, line = parse_struct(txt, name)
    if fs is None: print('!! 未找到', name); return None
    lay, size, al = layout(fs)
    has_ss = any(f[0] in ('struct_size','abi_version') for f in fs) or any(f[1].startswith('acs_head') for f in fs)
    print(f'--- {name}  @ {path}:{line}  字段数={len(fs)}  sizeof={size}  align={al}  自描述={"有" if has_ss else "无"}')
    print('    ' + ' | '.join(f'{f}({ct}x{n})@{o}+{s}' for f,ct,n,o,s in lay))
    NESTED[name] = (size, al)
    return size, al, has_ss, len(fs)

print('### 校验锚：IpvWcsResult 应为 1560，IpvParams 应为 424（与 V2 ctypes 复算一致）')
IPV='lib/plate_solve/cpp/ipv/include/ipv_api.h'
show(IPV,'IpvWcsResult'); show(IPV,'IpvParams')
show('lib/astro_image_io/include/aio_hips.h','AstroSphereTileView')
show('lib/astro_image_io/include/aio_hips.h','AioHipsSnrPoint')
show('lib/astro_image_io/include/aio_hips.h','AioHipsDiagTileView')
show('lib/gaia_xpsd_client/src/gaia_client.h','GaiaSpectrumStar')
show('lib/gaia_xpsd_client/src/gaia_client.h','GaiaStar')
show('lib/gaia_xpsd_client/src/gaia_client.h','GaiaPhotometryStar')
show('lib/star_detector/include/star_detector.h','SDetParams')
show('lib/dynamic_psf/include/dynamic_psf.h','DPSFFitParams')
show('lib/dynamic_psf/include/dynamic_psf.h','DPSFFitResult')
FS='modules/services/io/include/astrocs/io/fits_stream_v1.h'
show(FS,'acs_fio_keyword_v1')
show(FS,'acs_fio_header_v1')
