p1 = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/package/08_science_oracles/UPM_RAW_WEIGHT_DEVIATION.md"
s = open(p1).read()
banner = "# [SUPERSEDED - RETRACTED round 68; see UPM_RAW_WEIGHT_RETRACTION.md] The 'finding' below was a probe artifact (zero-cfg ablation path + probe bugs). Kept for the record.\n\n"
if not s.startswith("# [SUPERSEDED"):
    open(p1, "w").write(banner + s)
print("deviation note superseded")
