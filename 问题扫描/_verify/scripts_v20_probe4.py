
import re
def probe(fp, subs):
    txt=open(fp,encoding='utf-8',errors='ignore').read().split('\n')
    print('###', fp)
    for tag,s in subs:
        hits=[i+1 for i,l in enumerate(txt) if s in l]
        print('   %-22s %s' % (tag, hits[:6]))
probe('cli/commands.cpp', [('cmd_drizzle','cmd_drizzle'),('voidp_in_drizzle','(void)p;'),('quick','"quick"'),('full','"full"'),('group','"group"'),('resource_detail_eq','detail == "timeseries"'),('output_fits_read','m.value("output_fits_path"'),('mbid','module_build_id'),('env_sleep','ASTROCS_TEST_SLEEP_MS'),('env_bin','ASTROCS_TEST_BIN_DIR'),('env_repo','ASTROCS_REPO'),('env_crash','ASTROCS_TEST_CRASH'),('env_to','ASTROCS_TEST_TIMEOUT_S'),('pipe_env','ASTROCS_TEST_PIPELINE_SLEEP_MS'),('write_all','recorder.write_all'),('alloc','alloc_rec.write_all'),('cpu_prof','cpu-profile')])
probe('cli/parser.cpp', [('kBoolFlags','kBoolFlags'),('kValueFlags','kValueFlags'),('nside','--nside'),('pixfrac','--pixfrac'),('rd','--resource-detail'),('cpuprof','--cpu-profile'),('drizzle_rule','"drizzle", {''),('kGroups','kGroups'),('feat','output_fits_path'),('unknown','unknown key'),('flat','flat_session'),('schema','schema_version'),('inputs','missing \'inputs\''),('help','kHelp')])
probe('lib/phase2/src/upm.cpp', [('raw_weight','p2_upm_raw_weight(const'),('half_init','P2UpmBuildConfig cfg;'),('sigma1','cfg.sigma_floor = 1e-3')])
probe('cli/runtime_client.cpp', [('repo','ASTROCS_REPO'),('feat','output_fits_path'),('mode','contains("mode")')])
probe('lib/hips/src/module_entry.cpp', [('acquire','acquire(ex->user_data, 1)')])
