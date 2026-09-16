
import re, hashlib
fp='lib/core/src/module_adapters.cpp'
ma=open(fp,encoding='utf-8',errors='ignore').read().split('\n')
print('sha12', hashlib.sha256('\n'.join(ma).encode()).hexdigest()[:12], 'lines', len(ma))
# (cited_line, distinctive substring)  —— 检查行锚是否仍成立
pairs = [
 (1007,'return p1_session_run(h, c, 0)'),(1391,'k_branch'),(1446,'p1_has(doc, "dark_scale_factor"'),(1457,'ac_calibrate_frame('),
 (1523,'doc["cosmetic"]'),(1571,'ac_correct_frame('),(1534,'p1_int(c, "method"'),
 (2242,'init_source'),(2363,'ipv'),(1858,'sip'),
 (1703,'max_stars'),(2662,'int snr_max_sources = 0;'),(2675,'sc.value("max_sources"'),(2676,'if (snr_max_sources < 0)'),
 (2723,'sci_cfg'),(2787,'frame["snr_max_sources"]'),(2799,'sigma_location_se_status'),(2717,'nullptr'),
 (2917,'nside_mode_given'),(2924,'1x_to_2x_drizzle'),(2934,'nv.is_string'),(2962,'auto_nside'),(2968,'precision_mode'),(2974,'p1_has(dj, "precision_mode"'),(2983,'nested must be 0 or 1'),(2988,'pixfrac must be'),
 (3149,'hp_drizzle'),(3185,'filter_passband'),(3205,'nside_mode'),(3293,'filter_passband'),(3313,'filter_passband'),(3336,'filter_passband'),
 (3549,'{"union_cells"'),(3587,'__workers'),(3730,'P2UpmBuildConfig uc'),(3743,'std::max(1, doc.value("__workers"'),
 (3984,'{"tile_leaf_span"'),(4014,'tile_leaf_span'),(4020,'doc.value("reject_profile"'),(4211,'{"tile_leaf_span"'),
 (4258,'tile_leaf_span'),(4339,'sample_mask_offset'),(4409,'~0ull'),(4419,'frame_slots'),(4576,'{"tile_leaf_span"'),
 (4573,'weight_mode'),(4595,'weight_mode'),(4635,'tile_leaf_span'),(4676,'input_lights'),(4786,'ASTROCS_REJECT_PROFILE'),(4845,'validate_config'),(4848,'filter_passband'),(4912,'p.min_workers = 1'),(4914,'p.max_workers ='),(4965,'desc_.module_id'),(5140,'desc_.module_id'),(5160,'cfg2["__workers"]'),(6089,'desc_.module_id'),
 (5246,'struct P3nGeom'),(5264,'p3n_check_request_fields'),(5301,'g->sampler'),(5304,'g->projection'),(5305,'g->frame'),(5306,'g->coverage_output'),(5312,'g->sampler != "nearest"'),(5316,'g->bitpix != -32'),
 (5452,'p3_wcs_make('),(5454,'g.projection.c_str'),(5579,'g.sampler == "nearest"'),(5608,'g.sampler == "nearest"'),
 (5758,'{"sampler", g.sampler}'),(5759,'{"bitpix", g.bitpix}'),(5846,'prov.sampler_used'),(5852,'input_manifest_hash'),(5857,'g.bitpix'),(5866,'Json wr{'),(5885,'provider'),(5892,'(*man)["output_fits_path"]'),(5918,'p3n_read_json(g.out_dir + "/p3_writer.json"'),(5960,'透传'),(5969,'wr.value("module_build_id"'),
]
bad=[]
for ln,sub in pairs:
    if ln<1 or ln>len(ma) or sub not in ma[ln-1]:
        # find nearest
        true=[i+1 for i,l in enumerate(ma) if sub in l][:3]
        bad.append('%5d "%s"  -> 实际 %s' % (ln, sub, true))
print('MISMATCH %d / %d' % (len(bad), len(pairs)))
print('\n'.join(bad))
