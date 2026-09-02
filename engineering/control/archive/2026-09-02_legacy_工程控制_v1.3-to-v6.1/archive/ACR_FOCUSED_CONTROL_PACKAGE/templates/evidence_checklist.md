# Focused ACR Evidence Checklist

- [ ] implementation HEAD
- [ ] clean `git status --porcelain`
- [ ] control package SHA and `08_CURRENT_EXECUTION_PLAN` load proof
- [ ] path guard PASS and exit code
- [ ] CPU-only build/test command and raw log
- [ ] CUDA build/test command and raw log
- [ ] standard focused benchmark raw records
- [ ] qualified OperationProfile and schema validation log
- [ ] CPU-only/GPU-only/forced-Mixed/AutoMixed comparison
- [ ] execution reports and coverage
- [ ] resident-chain transfer report
- [ ] RAM/pinned/VRAM budget and OOM injection report
- [ ] ASan/UBSan raw log
- [ ] compute-sanitizer memcheck/racecheck raw log
- [ ] all commands include cwd, environment, timeout, exit code
- [ ] source snapshot, patch/diff, manifest and UTF-8 SHA256
- [ ] no build cache, duplicate source tree or old-HEAD evidence
