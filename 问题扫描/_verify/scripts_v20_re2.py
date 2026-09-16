
import re, hashlib
t=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read()
ma=t.split('\n')
frags = [
 ('input_lights_read','input_lights'),
 ('master_bias','"master_bias"'),
 ('filter_passband','filter_passband'),
 ('wcs_init_source','init_source'),
 ('wcs_ipv','p1_solve_wcs_ipv|ipv_solve'),
 ('sip','sip_ap_order'),
 ('psf_max_stars','max_stars'),
 ('union_cells_write','{"union_cells"'),
 ('tile_leaf_span','{"tile_leaf_span"'),
 ('smo_read','sample_mask_offset'),
 ('sentinel','~0ull'),
 ('files_accepted','"accepted"'),
 ('weight_mode','"weight_mode"'),
 ('legacy_fb','legacy_allow_weight_fallback'),
 ('persist_upm','persist_upm'),
 ('upm_save','upm_save_path'),
 ('copy_file','copy_file'),
 ('imh','input_manifest_hash'),
 ('node_validate_p1','validate_config(const std::string&'),
 ('node_keys','for (const char* k : {"cosmetic"|"cosmetic", "wcs"'),
 ('hips_paths_key','"hips_paths"'),
 ('nside_clamped','nside_clamped'),
 ('oversample','oversample_factor'),
 ('props_out','obs_filter'),
]
for name, pat in frags:
    rx = re.compile(pat)
    hits=[i+1 for i,l in enumerate(ma) if rx.search(l)]
    print('%-18s %-42s -> %s' % (name, pat[:42], hits[:8]))
print('sha', hashlib.sha256(t.encode()).hexdigest()[:12])
