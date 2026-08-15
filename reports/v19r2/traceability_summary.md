# Traceability Summary (V19R2)

- docs/TRACEABILITY.csv：30 行（SCI-NOISE-001..015、SCI-DRZ-001/014、
  SCI-UPM-001..010/PERSIST、ALG-REJ-001..008、SCI-INT-*、SCI-CAL/AST/
  PHOT/PSF、DATA-HIPS-*、ENG-*、TEST-PR-UPM-001..010）。
- 字段：requirement_id/type/title/authority_doc/algorithm_id/module/
  public_api/implementation_files/symbols/test_ids/test_files/
  diagnostics/error_codes/release_gate/status/notes。
- 检查器 tools/quality/check_traceability.py：
  - rows=30 ok=30 broken=0；
  - 抽样 science/public 符号 13/13 code→contract→test；
  - contract→code→test 反向一致。
- status 仅 VERIFIED（无 UNKNOWN/TBD）；权威文档不依赖 memory.md。

```text
CODE_TO_DOC_TRACEABILITY=PASS
DOC_TO_CODE_TRACEABILITY=PASS
TRACEABILITY_BROKEN=0
```
