
import re, os
targets = ['tests/backend/fixture_common.py','tests/cpu/dispatch/run_cpu_capability_checks.py','tests/io/hips_output_fixture.py','tests/cli/test_cli004_process_protocol.py','tests/runtime/test_rt003_budget_wiring.py','tools/check_duplication.py','tools/check_legacy_exit.py']
for f in targets:
    src=open(f,encoding='utf-8',errors='replace').read().splitlines()
    print('==============', f, '(%d lines)'%len(src))
    for i,l in enumerate(src,1):
        if re.search(r'''g\+\+|["']gcc["']|["']nm["']|["']objdump["']|shutil\.which|which\(|SkipTest|skipUnless|skipIf|sys\.exit\(77\)|return\s+0\s*#|FileNotFoundError|HAS_''', l):
            print('  %4d: %s' % (i, l.strip()[:150]))
