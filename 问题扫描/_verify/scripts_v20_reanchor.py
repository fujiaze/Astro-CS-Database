
import re, hashlib
t=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read()
ma=t.split('\n')
frags = [
 ('dark_branch','bool k_branch'),
 ('k_fixed_read','p1_num(doc, "dark_scale_factor"'),
 ('k_compare','std::fabs(cfg_k - k_expo)'),
 ('ac_calibrate_call','ac_calibrate_frame('),
 ('cos_enabled','p1_flag(doc["cosmetic"], "enabled"'),
 ('cos_correct','ac_correct_frame('),
 ('snr_sci_cfg','sci_cfg.gain_e_per_adu = sc.value'),
 ('snr_neg_fold','if (snr_max_sources < 0)'),
 ('snr_partial','if (snr_max_sources > 0 &&'),
 ('snr_echo','frame["snr_max_sources"]'),
 ('snr_se_status','frame["sigma_location_se_status"]'),
 ('snr_m5_null','frame["frame_depth_m5_mag"] = nullptr'),
 ('nside_mode_given','const bool nside_mode_given'),
 ('nside_alias','1x_to_2x_drizzle") nside_mode = "auto"'),
 ('precision_missing','drizzle requires'),
 ('nested_range','nested must be 0 or 1'),
 ('pixfrac_range','pixfrac must be in (0,1]'),
 ('nside_pow2','must be a power of two'),
 ('hp_drizzle_run','rc = hp_drizzle_run_phase1_hips('),
 ('echo_nside_mode','{"nside_mode", nside_mode_out}'),
 ('upm_cfg','uc.sigma_floor'),
 ('uc_workers','std::max(1, doc.value("__workers"'),
 ('workers_inject','cfg2["__workers"]'),
 ('reject_profile_read','doc.value("reject_profile"'),
 ('p3n_geom_struct','struct P3nGeom'),
 ('p3n_check','bool p3n_check_request_fields'),
 ('p3n_sampler','g->sampler = doc.value("sampler"'),
 ('p3n_ranges','if (g->sampler != "nearest"'),
 ('p3n_wcs_make','const P3WcsStatus wst = p3_wcs_make('),
 ('p3n_bitpix_echo','{"bitpix", g.bitpix}'),
 ('p3n_fits_path','const std::string fits_path ='),
 ('p3n_wr_path','const std::string json_path = g.out_dir + "/p3_writer.json"'),
 ('p3n_wr_read','if (!p3n_read_json(g.out_dir + "/w'),
 ('p3n_mbid_read','wr.value("module_build_id"'),
 ('p3n_echo_comment','verify 侧同源'),
 ('p1api_run0','return p1_session_run(h, c, 0)'),
 ('node_validate','std::vector<std::string> missing;'),
 ('mbid_manifest','desc_.module_id + "@" + ASTROCS_VERSION_STRING'),
]
for name, pat in frags:
    hits=[i+1 for i,l in enumerate(ma) if pat in l]
    print('%-20s %-46s -> %s' % (name, pat[:46], hits[:6]))
print('sha', hashlib.sha256(t.encode()).hexdigest()[:12], 'lines', len(ma))
# writer Json block
i=[k for k,l in enumerate(ma) if 'Json wr{' in l or re.match(r'\s*Json wr\s*[{(]', l)]
print('Json wr at', [x+1 for x in i])
for x in i[:1]:
    print('\n'.join('%d: %s' % (j+1, ma[j].strip()[:95]) for j in range(x, x+30)))
