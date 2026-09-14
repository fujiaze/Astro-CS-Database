
import ctypes as C
D=C.c_double; F=C.c_float; I=C.c_int; CH=C.c_char
# C 现行 IpvParams（ipv_api.h:64-91 逐字）
class Cpar(C.Structure):
    _fields_=[("polygon_sides",I),("n_pivot",I),("sigma_d_arcsec",D),("vote_threshold",I),("ransac_max_iter",I),
      ("ransac_inlier_threshold_arcsec",D),("s_min",D),("s_max",D),("img_n_target",I),("gaia_density_ratio",D),
      ("gaia_query_radius_factor",D),("m_lim_alpha_prior",D),("m_lim_alpha_min",D),("m_lim_alpha_max",D),
      ("m_lim_safety",D),("m_lim_m0_exposure_s",D),("m_lim_m0_offset",D),("m_lim_clamp_lo",D),("m_lim_clamp_hi",D),
      ("m_lim_zero_step",D),("m_lim_gaia_cap_per_file",D),("m_lim_max_iter",I),("density_tolerance",D),("log_dir",CH*256)]
class Ppar(C.Structure):
    _fields_=[("polygon_sides",I),("n_pivot",I),("sigma_d_arcsec",D),("vote_threshold",I),("ransac_max_iter",I),
      ("ransac_inlier_threshold_arcsec",D),("s_min",D),("s_max",D),("img_n_target",I),("gaia_density_ratio",D),
      ("gaia_query_radius_factor",D),("m_lim_step",D),("m_lim_max_iter",I),("density_tolerance",D),("log_dir",CH*256)]
print('C  IpvParams sizeof =', C.sizeof(Cpar), ' 字段数', len(Cpar._fields_))
print('PY IpvParams sizeof =', C.sizeof(Ppar), ' 字段数', len(Ppar._fields_))
print('Δ =', C.sizeof(Cpar)-C.sizeof(Ppar))
print('log_dir 偏移: C=%d  PY=%d  差=%d' % (C.IpvParams if 0 else getattr(Cpar,'log_dir').offset, getattr(Ppar,'log_dir').offset, getattr(Cpar,'log_dir').offset-getattr(Ppar,'log_dir').offset))
print('density_tolerance 偏移: C=%d PY=%d' % (getattr(Cpar,'density_tolerance').offset, getattr(Ppar,'density_tolerance').offset))
print('m_lim_max_iter  偏移: C=%d PY=%d' % (getattr(Cpar,'m_lim_max_iter').offset, getattr(Ppar,'m_lim_max_iter').offset))
print('\n字段集合差:')
cset=[f for f,_ in Cpar._fields_]; pset=[f for f,_ in Ppar._fields_]
print('  C 有 PY 无:', [x for x in cset if x not in pset])
print('  PY 有 C 无:', [x for x in pset if x not in cset])
