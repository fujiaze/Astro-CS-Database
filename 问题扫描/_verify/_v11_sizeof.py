
import ctypes, struct
# ---- C 侧布局推导（SysV AMD64 / MSVC x64 对 POD 规则一致：自然对齐、尾部补齐到最大对齐）----
def csizeof(name, fields):
    # fields: (ctype, count)
    off=0; maxalign=1; layout=[]
    for t,n in fields:
        sz=ctypes.sizeof(t); al=ctypes.alignment(t)
        if off % al: off += al - off%al
        layout.append((off, n*sz)); off += n*sz; maxalign=max(maxalign,al)
    if off % maxalign: off += maxalign - off%maxalign
    print(f'C  {name}: sizeof={off} align={maxalign}')
    for (t,n),(o,s) in zip(fields,layout):
        print(f'     @{o:3d} +{s:3d}  {getattr(t,"__name__",t)} x{n}')
    return off

def pysize(name, cls):
    print(f'PY {name}: sizeof={ctypes.sizeof(cls)} align={cls._alignment_ if hasattr(cls,"_alignment_") else ctypes.alignment(cls)}')
    for f in cls._fields_:
        t=f[1]; n=ctypes.sizeof(t)
        print(f'     @{getattr(cls,f[0]).offset:3d} +{n:3d}  {f[0]}')
    return ctypes.sizeof(cls)

D=ctypes.c_double; F=ctypes.c_float; I=ctypes.c_int; U32=ctypes.c_uint32; I64=ctypes.c_int64; U64=ctypes.c_uint64; P=ctypes.c_void_p; U8=ctypes.c_uint8

print('='*80); print('AioHipsSnrPoint  (lib/astro_image_io/include/aio_hips.h:84-92)')
cA=csizeof('AioHipsSnrPoint',[(D,1),(D,1),(D,1),(I64,1),(U32,1),(U32,1)])
class SnrSmoke(ctypes.Structure):
    _fields_=[("ra_deg",D),("dec_deg",D),("snr",D),("source_id",I64)]
pA=pysize('AioHipsSnrPoint @hips_direct_smoke.py:34 (4 字段)',SnrSmoke)
class SnrFull(ctypes.Structure):
    _fields_=[("ra_deg",D),("dec_deg",D),("snr",D),("star_id",I64),("quality_flags",U32),("photometric_status",U32)]
pB=pysize('AioHipsSnrPoint @v5_snr_precision_roundtrip.py:33 / gen_hips_browser_test.py:41 (6 字段)',SnrFull)
print(f'  Δ smoke 镜像 = {cA-pA} 字节/元素；3 元素数组 C 读 {3*cA} vs PY 分配 {3*pA}')
print(f'  Δ full  镜像 = {cA-pB}')

print('='*80); print('AstroSphereTileView (aio_hips.h:69-81)')
cT=csizeof('AstroSphereTileView',[(U64,1),(U32,1),(U32,1),(I,1),(P,1),(P,1),(P,1),(P,1)])
class Tile7(ctypes.Structure):
    _fields_=[("parent_ipix",U64),("leaf_order",U32),("width",U32),("data_type",I),("flux_sum",P),("covered_area",P),("valid_mask",P)]
pT=pysize('AstroSphereTileView 全部 5 份镜像 (7 字段, 缺 var_num_sum)',Tile7)
print(f'  Δ = {cT-pT} 字节 (C 侧第 8 字段 var_num_sum @48)')

print('='*80); print('GaiaSpectrumStar (lib/gaia_xpsd_client/src/gaia_client.h:57-66)')
cG=csizeof('GaiaSpectrumStar',[(D,1),(D,1),(D,1),(F,1),(F,1)])
class G3(ctypes.Structure):
    _fields_=[("ra",D),("dec",D),("magG",D)]
pG3=pysize('GaiaSpectrumStar @diag_gaia_psf_projection.py:108 (3 字段)',G3)
class G5(ctypes.Structure):
    _fields_=[("ra",D),("dec",D),("magG",D),("flux_min",F),("flux_mul",F)]
pG5=pysize('GaiaSpectrumStar @xpsd_*.py (5 字段)',G5)
print(f'  Δ diag 镜像 = {cG-pG3}')

print('='*80); print('SDetParams (star_detector.h:17-27)')
cS=csizeof('SDetParams',[(I,1),(I,1),(F,1),(I,1),(I,1),(I,1),(I,1),(F,1),(F,1)])
class SD(ctypes.Structure):
    _fields_=[("structureLayers",I),("hotPixelFilterRadius",I),("iterativeClipSigma",F),("iterativeMaxRounds",I),("medianFilterDetail",I),("maxStars",I),("fitRadius",I),("fwhmClipSigma",F),("maxAxisRatio",F)]
pS=pysize('SDetParams 3 份镜像',SD)
print(f'  Δ = {cS-pS}')

print('='*80); print('DPSFFitParams / DPSFFitResult (dynamic_psf.h:38-42 / 17-31)')
cP=csizeof('DPSFFitParams',[(I,1),(I,1),(D,1)])
class DP(ctypes.Structure):
    _fields_=[("fitRadius",I),("maxIter",I),("tolerance",D)]
print('  PY DPSFFitParams =',pysize('DPSFFitParams 3 份镜像',DP), 'Δ=',cP-ctypes.sizeof(DP))
cR=csizeof('DPSFFitResult',[(I,1)]+[(D,1)]*12)
class DR(ctypes.Structure):
    _fields_=[("status",I)]+[(n,D) for n in ["B","A","cx","cy","sx","sy","theta","fwhm_x","fwhm_y","mad","flux","eccentricity"]]
print('  PY DPSFFitResult =',pysize('DPSFFitResult @gate1:51',DR),'Δ=',cR-ctypes.sizeof(DR))

print('='*80); print('IpvWcsResult (ipv_api.h:39-61)')
cW=csizeof('IpvWcsResult',[(D,4),(D,2),(D,2),(I,1),(D,36),(D,36),(I,1),(D,36),(D,36),(D,1),(D,1),(I,1),(I,1),(I,1),(I,1),(I,1),(I,1),(ctypes.c_char,16),(ctypes.c_char,16),(ctypes.c_char,256)])
class WR(ctypes.Structure):
    _fields_=[("cd",D*4),("crval",D*2),("crpix",D*2),("sip_order",I),("sip_a",D*36),("sip_b",D*36),("sip_ap_order",I),("sip_ap",D*36),("sip_bp",D*36),("rms_px",D),("rms_arcsec",D),("n_pairs",I),("success",I),("n_detected",I),("n_catalog",I),("trans_order",I),("best_inliers",I),("ctype1",ctypes.c_char*16),("ctype2",ctypes.c_char*16),("error_msg",ctypes.c_char*256)]
print('  PY IpvWcsResult =',pysize('IpvWcsResult @diag:79',WR),'Δ=',cW-ctypes.sizeof(WR))

print('='*80); print('IpvParams (ipv_api.h:64-91) —— 已知实例 V2-D8 现状复测')
cI=csizeof('IpvParams(C 现行)',[(I,1),(I,1),(D,1),(I,1),(I,1),(D,1),(D,1),(D,1),(I,1),(D,1),(D,1),(D,1),(D,1),(D,1),(D,1),(D,1),(D,1),(D,1),(D,1),(D,1),(I,1),(D,1),(ctypes.c_char,256)])
class IP(ctypes.Structure):
    _fields_=[("polygon_sides",I),("n_pivot",I),("sigma_d_arcsec",D),("vote_threshold",I),("ransac_max_iter",I),("ransac_inlier_threshold_arcsec",D),("s_min",D),("s_max",D),("img_n_target",I),("gaia_density_ratio",D),("gaia_query_radius_factor",D),("m_lim_step",D),("m_lim_max_iter",I),("density_tolerance",D),("log_dir",ctypes.c_char*256)]
print('  PY IpvParams =',pysize('IpvParams @diag:61',IP),'Δ=',cI-ctypes.sizeof(IP))
