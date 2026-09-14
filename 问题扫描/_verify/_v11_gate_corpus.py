
import re, pathlib, sys
# 逐字复刻 tools/check_abi_boundary.py 的两个正则
REPO=pathlib.Path('.')
abi=(REPO/'include/astrocs/common_abi_v1.h').read_text(encoding='utf-8')
structs = re.findall(r"typedef\s+struct\s+(\w+)", abi)
structs = [s for s in structs if not s.startswith("acs_handle")]
print('### 正则1 typedef\\s+struct\\s+(\\w+) 命中（要求有 tag 名）:', len(structs), structs)
blocks = re.findall(r"typedef\s+struct\s+(\w+)\s*\{(.*?)\}\s*\w+\s*;", abi, re.S)
print('### 正则2 blocks 命中:', len(blocks), [b[0] for b in blocks])
# 实际该头里有多少个 typedef struct（匿名也算）
all_named = re.findall(r'typedef\s+struct\s+(?:\w+\s*)?\{', abi)
print('### 该头里 typedef struct 实际总数（含匿名）:', len(all_named))
names = re.findall(r'typedef\s+struct[^{]*\{(.*?)\}\s*(\w+)\s*;', abi, re.S)
print('### 按 typedef 名提取的实际结构:', len(names), [n for _,n in names])
anon = [n for _,n in names]
missing_from_gate = [n for n in anon if n not in [b[0] for b in blocks]]
print('### 门扫不到（因正则要求 tag 名）的结构:', missing_from_gate)
print()
print('### 门语料清单（HEADERS）:')
print('  include/astrocs/common_abi_v1.h')
for h in sorted((REPO/'lib/backend_host').glob('*.h')):
    if "impl" not in h.name and "inc" not in h.name: print('  ', h.as_posix())
print()
# 跨语言消费的 legacy 头在不在语料里
legacy = {'lib/plate_solve/cpp/ipv/include/ipv_api.h':'IpvParams/IpvWcsResult',
 'lib/astro_image_io/include/aio_hips.h':'AstroSphereTileView/AioHipsSnrPoint/AioHipsDiagTileView/AioHipsVerifyReport/AioHipsTile',
 'lib/astro_image_io/include/aio_pipeline.h':'AstroAbiInfo/AioKVEntry/AioBlock/PipelineFrame',
 'lib/star_detector/include/star_detector.h':'SDetParams',
 'lib/dynamic_psf/include/dynamic_psf.h':'DPSFFitResult/DPSFFitParams',
 'lib/gaia_xpsd_client/src/gaia_client.h':'GaiaStar/GaiaSpectrumStar/GaiaPhotometryStar/GaiaPlanStats',
 'modules/services/io/include/astrocs/io/fits_stream_v1.h':'acs_fio_*',
 'include/astrocs/io/aio_abi_v1.h':'aio_abi_info_v1'}
inhdr=set()
for h in [REPO/'include/astrocs/common_abi_v1.h']+[p for p in sorted((REPO/'lib/backend_host').glob('*.h')) if "impl" not in p.name and "inc" not in p.name]:
    inhdr.add(pathlib.Path(h).as_posix())
print('### 被 ctypes/跨 DLL 消费的结构所在头 是否在 ABI-BOUNDARY 语料内:')
for p,s in legacy.items():
    print(f'  {p:56s} {"在" if p in inhdr else "不在 <<<"}   结构: {s}')
