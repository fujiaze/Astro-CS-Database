
def probe(fp, subs):
    txt=open(fp,encoding='utf-8',errors='ignore').read().split('\n')
    print('###', fp, len(txt),'lines')
    for tag,s in subs:
        print('   %-16s %s' % (tag, [i+1 for i,l in enumerate(txt) if s in l][:5]))
probe('lib/phase3_session/p3_session.cpp', [('sampler_used_assign','prov.sampler_used'),('sampler_used_result','{"sampler_used"'),('output_result','{"output_fits_path"'),('covcheck','covout != "mask"'),('framecheck','fr != "icrs"')])
probe('tests/cli/test_monitor_events.py', [('downsample','downsample'),('vector','vector<int>')])
probe('cli/parser.cpp', [('group','group'),('run_rule','"run"')])
probe('cli/commands.cpp', [('cmd_ph3','cmd_phase3_run'),('args_return','return astrocs::ARGS')])
