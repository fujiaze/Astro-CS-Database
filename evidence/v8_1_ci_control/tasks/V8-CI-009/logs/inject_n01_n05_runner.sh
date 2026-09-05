#!/usr/bin/env bash
# V8-CI-009 可重放注入脚本：N01–N05（run.py 执行链，fixture 仓库，离线）
# 用法：bash inject_n01_n05_runner.sh   （无需参数；在仓库根或任意目录可执行）
set -u
PY=python3
REPO_ROOT="$(cd "$(dirname "$0")/../../../../.." && pwd)"   # logs→任务→tasks→v8_1_ci_control→evidence→仓库根
RUNNER="$REPO_ROOT/ci/run.py"

mk_repo() {  # $1 = 目标目录
  git init -q -b main "$1" && cd "$1" || exit 1
  git config user.email ci-replay@astrocs.invalid
  git config user.name "V8-CI-009 Replay"
  printf 'A\n' > A.txt; printf 'B\n' > B.txt
  git add -A && git commit -qm init
  mkdir -p ci
}

# 期望基线：带主仓库结果 schema（执行模式需要 ci/ci_result.schema.json）
with_schema() {
  if [ -n "${REPO_CI:-}" ] && [ -f "$REPO_CI/ci_result.schema.json" ]; then
    cp "$REPO_CI/ci_result.schema.json" ci/
  fi
}

T=$(mktemp -d)

echo "== N01a 非法 JSON registry → 期望 exit 2 =="
mk_repo "$T/n01a"; with_schema
printf '{oops' > ci/checks.json
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast; echo "exit=$?"

echo "== N01b 伪造 PASS 遇真实失败 → 期望 exit 1，per-check FAIL =="
mk_repo "$T/n01b"; with_schema
cat > ci/checks.json <<'EOF'
{"schema_version":1,"checks":[{"id":"N-T","profiles":["fast"],"platform":"any",
 "command":["python3","-c","import sys;sys.exit(3)"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
mkdir -p artifacts/ci/forged/checks
printf '{"id":"N-T","verdict":"PASS","exit_code":0}' > artifacts/ci/forged/checks/N-T.json
printf '{"schema_version":1,"verdict":"PASS","checks":[]}' > artifacts/ci/forged/CI_RESULT.json
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast --output-root "$PWD/artifacts/ci/forged"; echo "exit=$?"
cat artifacts/ci/forged/checks/N-T.json; echo

echo "== N02 非零退出 → 期望 exit 1, verdict FAIL =="
mk_repo "$T/n02"; with_schema
sed 's/print(.ok.)/import sys;sys.exit(3)/' > /dev/null # 占位
cat > ci/checks.json <<'EOF'
{"schema_version":1,"checks":[{"id":"N-EXIT","profiles":["fast"],"platform":"any",
 "command":["python3","-c","import sys;sys.exit(3)"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast | grep -o '"verdict": *"[A-Z()a-z]*"' | head -3; echo "exit=${PIPESTATUS[0]}"

echo "== N03 超时 → 期望 exit 1, verdict TIMEOUT =="
mk_repo "$T/n03"; with_schema
cat > ci/checks.json <<'EOF'
{"schema_version":1,"checks":[{"id":"N-SLP","profiles":["fast"],"platform":"any",
 "command":["python3","-c","import time;time.sleep(4)"],"timeout_seconds":1,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast > /tmp/n03_out.json; echo "exit=$?"
grep -o '"verdict": *"TIMEOUT"' /tmp/n03_out.json | head -1

echo "== N04 dirty workspace → 期望 exit 1, verdict FAIL(dirty) =="
mk_repo "$T/n04"; with_schema
cat > ci/checks.json <<'EOF'
{"schema_version":1,"checks":[{"id":"N-DIRTY","profiles":["fast"],"platform":"any",
 "command":["python3","-c","open('dirty_probe.txt','w').write('x')"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast > /tmp/n04_out.json; echo "exit=$?"
grep -o '"verdict": *"FAIL(dirty)"' /tmp/n04_out.json | head -1

echo "== N05 未登记检查 ID → 期望 exit 2 =="
mk_repo "$T/n05"; with_schema
cat > ci/checks.json <<'EOF'
{"schema_version":1,"checks":[{"id":"N-OK","profiles":["fast"],"platform":"any",
 "command":["python3","-c","print('ok')"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast --check N-GHOST; echo "exit=$?"

echo "== N04-GAP-G1 过期 WRITE_LEASE 放行 → 现状 exit 0（GAP） =="
mk_repo "$T/n04g"; with_schema
cat > ci/WRITE_LEASE.json <<EOF
{"lease_id":"LL-FAKE","task_id":"V8-CI-009","base_sha":"$(printf '0%.0s' {1..40})","expires_utc":"2020-01-01T00:00:00Z"}
EOF
cat > ci/checks.json <<'EOF'
{"schema_version":1,"checks":[{"id":"N-OK","profiles":["fast"],"platform":"any",
 "command":["python3","-c","print('ok')"],"timeout_seconds":30,
 "heavy":false,"mutates_workspace":false,"outputs":[],"waivable":false,
 "changed_paths":[],"requires_monitor":false}]}
EOF
timeout 60 $PY "$RUNNER" --repo-root "$PWD" --profile fast > /dev/null 2>&1; echo "exit=$?（0 = GAP-G1 现状放行）"

cd /; rm -rf "$T"
echo "replay done."
