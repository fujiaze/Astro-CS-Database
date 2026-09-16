
def probe(fp, subs):
    txt=open(fp,encoding='utf-8',errors='ignore').read().split('\n')
    print('###', fp)
    for tag,s in subs:
        hits=[i+1 for i,l in enumerate(txt) if s in l]
        print('   %-20s %s' % (tag, hits[:6]))
probe('cli/commands.cpp', [('cmd_drizzle','cmd_drizzle'),('voidp','(void)p;'),('quick','"quick"'),('full','"full"'),('group','"group"'),('detail','detail == "timeseries"'),('ofp','m.value("output_fits_path"'),('mbid','module_build_id'),('sleep','ASTROCS_TEST_SLEEP_MS'),('bindir','ASTROCS_TEST_BIN_DIR'),('repo','ASTROCS_REPO'),('crash','ASTROCS_TEST_CRASH'),('pipe','ASTROCS_TEST_PIPELINE_SLEEP_MS'),('writeall','recorder.write_all'),('cpuprof','cpu-profile')])
probe('cli/parser.cpp', [('kBool','kBoolFlags'),('kVal','kValueFlags'),('nside','--nside'),('pixfrac','--pixfrac'),('rd','--resource-detail'),('cpuprof','--cpu-profile'),('drzrule','"drizzle", {"phase3"'),('kGroups','kGroups'),('feat','output_fits_path'),('unknown','unknown key'),('flat','flat_session'),('schema','schema_version'),('kHelp','kHelp')])
probe('lib/phase2/src/upm.cpp', [('raww','p2_upm_raw_weight'),('half','P2UpmBuildConfig cfg;'),('s1','cfg.sigma_floor = 1e-3')])
probe('cli/runtime_client.cpp', [('repo','ASTROCS_REPO'),('feat','output_fits_path')])
probe('lib/snr_estimator/src/module_entry.cpp', [('minw','min_workers'),('acq','acquire(ex->user_data, 1)')])
