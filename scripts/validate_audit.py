#!/usr/bin/env python3
"""REL-003: 审核包验证器。解包后重验 SHA; 无 .git/build/testdata。"""
import hashlib, json, pathlib, sys, tarfile, tempfile

def main():
    if len(sys.argv) < 2:
        print("usage: validate_audit.py <tarball>"); return 2
    tb = pathlib.Path(sys.argv[1])
    if not tb.is_file():
        print(f"FAIL: no tarball {tb}"); return 1
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        with tarfile.open(tb) as tf:
            tf.extractall(td)
        mf = json.loads((td / "audit_manifest.json").read_text(encoding="utf-8"))
        bad = 0
        for rel, sha in mf["files"].items():
            p = td / rel
            if not p.is_file():
                print(f"FAIL: missing {rel}"); bad += 1; continue
            if hashlib.sha256(p.read_bytes()).hexdigest() != sha:
                print(f"FAIL: sha mismatch {rel}"); bad += 1
        # 禁止项
        for fd in (".git", "build", "testdata"):
            if (td / fd).exists():
                print(f"FAIL: forbidden {fd}"); bad += 1
        if bad:
            print(f"VALIDATE_FAIL: {bad} issues"); return 1
        print(f"VALIDATE_OK: {len(mf['files'])} files sha-verified, no .git/build/testdata")
        return 0

if __name__ == "__main__":
    raise SystemExit(main())
