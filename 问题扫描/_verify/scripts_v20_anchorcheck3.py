
import re, hashlib
def sha(fp):
    t=open(fp,encoding='utf-8',errors='ignore').read()
    return hashlib.sha256(t.encode()).hexdigest()[:12], t
pairs = {
 'cli/commands.cpp': [(635,'emit_resource_summary'),(652,'detail == "timeseries"'),(657,'payload["curve_points"] = nlohmann::json::array()'),(658,'payload["downsample_max"] = astrocs::kDownsampleMax'),(735,'resource_detail_arg'),(737,'summary'),(889,'recorder.write_all(res_out_dir'),(898,'alloc_rec.write_all'),(1162,'resource_detail_arg(p)'),(1361,'resource_detail_arg(p)'),(1746,'resource_detail_arg(p)'),(373,'module_build_id'),(546,'output_fits_path'),(1325,'output_fits_path'),(1898,'cmd_drizzle'),(1902,'(void)p'),(163,'cpu-profile'),(2345,'quick'),(2516,'group')],
 'cli/parser.cpp': [(22,'kBoolFlags'),(25,'nside'),(61,'drizzle'),(52,'resource-detail'),(200,'kGroups'),(336,'output_fits_path'),(338,'doc.begin()'),(343,'unknown key'),(347,'flat_session'),(350,'schema_version'),(360,'inputs')],
 'cli/resource_events.h': [(29,'kDownsampleMax = 121'),(33,'downsample_curve'),(96,'build_curve'),(115,'return downsample_curve')],
 'cli/runtime_client.cpp': [(42,'output_fits_path'),(43,'doc.contains("mode")'),(308,'ASTROCS_REPO')],
 'tests/cli/test_monitor_events.py': [(118,'def test_04_tier_downsample_present_only_when_timeseries'),(126,'raw_dir'),(168,'downsample')],
 'lib/hips/src/module_entry.cpp': [(307,'仅作回显'),(387,'c->max_workers'),(617,'max_workers=1'),(666,'min_workers'),(843,'ex->acquire'),(844,'acquire(ex->user_data, 1)')],
 'lib/snr_estimator/src/module_entry.cpp': [(369,'仅作回显'),(473,'c->max_workers'),(768,'snr_plan_emit'),(940,'acquire(ex->user_data, 1)')],
 'lib/calibration/src/module_entry.cpp': [(790,'build_out_manifest'),(828,'return ACS_OK'),(942,'c.max_workers'),(1076,'cal_describe'),(761,'decode_plane'),(778,'(void)what')],
 'lib/cosmetic/src/module_entry.cpp': [(623,'build_out_manifest'),(760,'c.max_workers'),(852,'cos_describe')],
 'lib/drizzle/src/module_entry.cpp': [(313,'rev_scale'),(456,'c->rev_scale'),(481,'DRZ_CFG_KEY_REV_PROJ'),(593,'max_workers'),(1049,'crval')],
 'lib/gaia_xpsd_client/src/module_entry.c': [(443,'cancel_active'),(650,'cancel_active = 1'),(661,'c.max_workers')],
 'lib/phase2/src/upm.cpp': [(1296,'p2_upm_raw_weight'),(1302,'cfg.sigma_floor = 1e-3')],
 'lib/phase3_session/p3_session.cpp': [(99,'doc.contains("frame")'),(128,'coverage_output'),(376,'out_path'),(379,'output_dir')],
 'lib/phase1_session/p1_session.cpp': [(216,'async_io_depth < 0')],
}
for fp, lst in pairs.items():
    s,_ = sha(fp)
    txt = open(fp,encoding='utf-8',errors='ignore').read().split('\n')
    bad=[]
    for ln,sub in lst:
        if ln>len(txt) or sub not in txt[ln-1]:
            bad.append('%5d "%s" -> %s' % (ln, sub, [i+1 for i,l in enumerate(txt) if sub in l][:3]))
    print('%-46s sha %s  %s' % (fp, s, 'ALL OK' if not bad else 'DRIFT:\n   '+'\n   '.join(bad)))
