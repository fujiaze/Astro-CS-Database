p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/raww_probe3.c"
s = open(p).read()
# Add the probe2 field combo to find which field breaks it
extra = '''
    /* Now toggle probe2 fields one by one on top of 'a' (ivar=4) */
    {
        P2ControlObservation t = a; t.control_ivar = 4.0;
        double w;
        t.uncertainty = 0.25; p2_upm_raw_weight(&t, &cfg, &w);
        printf("a+unc=0.25: %.6f\\n", w);
        t.control_variance = 0.0625; p2_upm_raw_weight(&t, &cfg, &w);
        printf("+cvar=0.0625: %.6f\\n", w);
        t.snr_available = 1; p2_upm_raw_weight(&t, &cfg, &w);
        printf("+snr_available=1: %.6f\\n", w);
        t.snr = 10.0; p2_upm_raw_weight(&t, &cfg, &w);
        printf("+snr=10: %.6f\\n", w);
        t.support = 1.0; p2_upm_raw_weight(&t, &cfg, &w);
        printf("+support=1: %.6f\\n", w);
        t.value = 100.0; p2_upm_raw_weight(&t, &cfg, &w);
        printf("+value=100: %.6f\\n", w);
    }
'''
s = s.replace('    /* my earlier probe set uncertainty=0.25 and snr_available unset(0) - try toggling those */\n    P2ControlObservation c = a; c.control_ivar = 4.0; c.uncertainty = 0.25;\n    double wc=0; p2_upm_raw_weight(&c, &cfg, &wc);\n    printf("with unc=0.25: w=%.6f\\\\n", wc);\n', extra)
open(p, "w").write(s)
print("patched")
