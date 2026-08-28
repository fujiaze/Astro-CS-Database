#!/usr/bin/env bash
# review capsule: 最新完整文件集快照 (控制包+报告+账本) — 每 Task PASS 后调用
set -eu
TASK="$1"; cd "$(dirname "$0")/../../../.."
OUT="run/reaudit_v4/capsules"; mkdir -p "$OUT"
TS=$(date -u '+%Y%m%dT%H%MZ'); NAME="capsule_${TASK}_${TS}"
python3 - "$OUT/$NAME.zip" "$TASK" <<'PY'
import zipfile, os, sys
out, task = sys.argv[1], sys.argv[2]
roots = ['工程控制/REAUDIT_V4/v4_reaudit', 'reports/REAUDIT_V4']
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for r in roots:
        if not os.path.isdir(r): continue
        for root, _, files in os.walk(r):
            for f in files:
                p = os.path.join(root, f)
                z.write(p, p)
print('capsule:', out)
PY
