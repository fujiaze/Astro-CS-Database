
import re, hashlib
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('sha12', hashlib.sha256('\n'.join(ma).encode()).hexdigest()[:12], 'lines', len([x for x in ma if x!='']))
pairs=[(1007,'return p1_session_run(h, c, 0)'),(1121,'p1_has'),(1286,'input_lights'),(1367,'master_bias'),(1398,'k_branch'),(1446,'p1_has(doc, "dark_scale_factor"'),(1448,'std::fabs(cfg_k - k_expo)'),(1457,'ac_calibrate_frame('),
(1523,'doc["cosmetic"]'),(1534,'p1_int(c, "method"'),(1551,'ac_correct_frame('),
(2231,'init_source'),(2347,'ipv'),(1837,'sip'),
(1805,'psf.max_stars'),(1809,'p1_int(psf, "max_stars", 5000)'),
(2662,'int snr_max_sources = 0;'),(2666,'sci_cfg.gain_e_per_adu'),(2675,'sc.value("max_sources"'),(2676,'if (snr_max_sources < 0)'),(2717,'frame_depth_m5_mag"] = nullptr'),(2751,'if (snr_max_sources > 0'),(2787,'frame["snr_max_sources"]'),(2799,'sigma_location_se_status'),
(2917,'nside_mode_given'),(2924,'1x_to_2x_drizzle'),(2925,'nside_mode != "auto"'),(2935,'nv.is_string'),(2947,'must be an integer'),(2967,'p1_num(dj, "pixfrac"'),(2973,'p1_has(dj, "precision_mode"'),(2983,'nested must be 0 or 1'),(2988,'pixfrac must be in (0,1]'),
(3185,'doc.value("filter_passband"'),(3190,'hp_drizzle_run_phase1_hips(frame, nside, nested, pixfrac'),(3211,'{"nside_mode", nside_mode_out}'),(3302,'doc.value("filter_passband"'),(3534,'{"filter_passband"'),
(3549,'{"union_cells"'),(3587,'__workers'),(3729,'P2UpmBuildConfig uc'),(3743,'std::max(1, doc.value("__workers"'),
(3984,'{"tile_leaf_span"'),(4014,'tile_leaf_span'),(4020,'doc.value("reject_profile"'),(4211,'{"tile_leaf_span"'),(4258,'tile_leaf_span'),(4339,'sample_mask_offset'),(4409,'~0ull'),(4419,'frame_slots'),(4573,'weight_mode'),(4576,'{"tile_leaf_span"'),(4595,'weight_mode'),(4635,'tile_leaf_span'),(4786,'ASTROCS_REJECT_PROFILE'),
(4845,'{"input_lights", "output_dir"}'),(4857,'cosmetic must be object'),(4912,'p.min_workers = 1'),(4914,'p.max_workers ='),(4965,'{"module_build_id", desc_.module_id'),(5140,'{"module_build_id", desc_.module_id'),(5160,'cfg2["__workers"]'),
(5246,'struct P3nGeom'),(5264,'bool p3n_check_request_fields'),(5301,'g->sampler = doc.value("sampler"'),(5304,'g->projection = doc.value("projection"'),(5305,'g->frame = doc.value("frame"'),(5306,'g->coverage_output = doc.value("coverage_output"'),(5312,'g->sampler != "nearest"'),(5316,'g->bitpix != -32'),(5452,'p3_wcs_make('),(5454,'g.projection.c_str'),(5584,'g.sampler == "nearest"'),(5623,'g.sampler == "nearest"'),(5788,'{"sampler", g.sampler}'),(5789,'{"bitpix", g.bitpix}'),
(5852,'p3n_input_manifest_hash(g.hips_dir)'),(5876,'prov.sampler_used'),(5882,'const std::string fits_path = g.out_dir + "/output_phase3.fits"'),(5887,'g.bitpix'),(5895,'"/p3_writer.json"'),(5896,'Json wr{'),(5915,'{"provider", "baseline"}'),(5920,'A2 单点键名'),(5922,'(*man)["output_fits_path"]'),(5948,'p3n_read_json(g.out_dir + "/p3_writer.json"'),(5990,'B2-A10'),(5999,'wr.value("module_build_id"'),(6119,'{"module_build_id", desc_.module_id')]
bad=[]
for ln,sub in pairs:
    if ln>len(ma) or sub not in ma[ln-1]:
        bad.append('%5d "%s" -> %s' % (ln,sub,[i+1 for i,l in enumerate(ma) if sub in l][:3]))
print('MISMATCH', len(bad), '/', len(pairs))
print('\n'.join(bad))
