#!/usr/bin/env bash
# V8-CI-009 可重放注入脚本：N07/N08(GAP-G2)/N09/N10（registry / lock / bootstrap 层，离线）
# 用法：bash inject_n07_n09_n10_registry.sh
set -u
PY=python3
T=$(mktemp -d)

echo "== N07 短 SHA lock → 期望 LockInvalid（exit 3） =="
cat > "$T/lock_short.json" <<'EOF'
{"schema_version":1,"entries":[{"action":"actions/checkout","tag":"v7.0.1",
 "commit_sha":"shortsha","verified_utc":"2026-01-01T00:00:00Z"}]}
EOF
timeout 60 $PY ci/verify_actions_lock.py --lock "$T/lock_short.json"; echo "exit=$?"

echo "== N08a registry 携带 -j4（真实脚本）→ 现状 exit 0（GAP-G2） =="
cat > "$T/r_j4.json" <<'EOF'
{"schema_version":1,"checks":[{"id":"N-J4","profiles":["fast"],"platform":"any",
 "command":["python3","ci/select_profile.py","-j4"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY ci/validate_registry.py --registry "$T/r_j4.json" --strict | python3 -c "import json,sys;d=json.load(sys.stdin);print('verdict',d['verdict'],'errors',d['errors'])"; echo "exit=$?（0 = GAP-G2 现状放行）"

echo "== N08b registry 携带 --threads=64 → 现状 exit 0（GAP-G2） =="
cat > "$T/r_thr.json" <<'EOF'
{"schema_version":1,"checks":[{"id":"N-THR","profiles":["fast"],"platform":"any",
 "command":["python3","ci/select_profile.py","--threads=64"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY ci/validate_registry.py --registry "$T/r_thr.json" --strict > /dev/null; echo "exit=$?（0 = GAP-G2 现状放行）"

echo "== N09a heavy 无 monitor → 期望 exit 1（R7） =="
cat > "$T/r_r7.json" <<'EOF'
{"schema_version":1,"checks":[{"id":"N-HEAVY","profiles":["linux-deep"],"platform":"any",
 "command":["python3","ci/select_profile.py"],"timeout_seconds":30,
 "heavy":true,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY ci/validate_registry.py --registry "$T/r_r7.json" --strict | grep -o 'R7[^"]*'; echo "exit=${PIPESTATUS[0]}"

echo "== N09b mutates_workspace 混入 fast → 期望 exit 1（R8） =="
cat > "$T/r_r8.json" <<'EOF'
{"schema_version":1,"checks":[{"id":"N-FASTHEAVY","profiles":["fast"],"platform":"any",
 "command":["python3","ci/select_profile.py"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":true,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY ci/validate_registry.py --registry "$T/r_r8.json" --strict | grep -o 'R8[^"]*'; echo "exit=${PIPESTATUS[0]}"

echo "== N10 版本漂移（strict policy）→ 期望 exit 2 =="
cat > "$T/strict_policy.json" <<'EOF'
{"linux_hosted":{"runner":"ubuntu-24.04","architecture":"x86_64",
 "primary_compiler":"gcc-99","secondary_compiler":"clang-99","cmake":"99.99.99","generator":"Ninja"},
 "windows_hosted":{"runner":"windows-2022","architecture":"x64","visual_studio_major":17,
 "platform_toolset":"v143","cmake":"99.99.99"}}
EOF
timeout 60 $PY ci/bootstrap.py --platform linux --policy "$T/strict_policy.json" --json > "$T/n10.json"; echo "exit=$?"
python3 -c "import json;d=json.load(open('$T/n10.json'));print('ok=',d['ok'],'failures=',d['failures'])"

rm -rf "$T"
echo "replay done."
