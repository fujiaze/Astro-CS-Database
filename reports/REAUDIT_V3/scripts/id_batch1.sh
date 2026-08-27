#!/usr/bin/env bash
# AstroCS audit supplement V2 - identity evidence collection batch 1
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
REPO="/home/lighthouse/Astro CS Database"
ID="$ROOT/package/00_identity"
LOG="$ROOT/logs"
mkdir -p "$LOG"
cd "$REPO" || exit 9

run() { # run <outfile> <timeout_s> <cmd...>
  local outfile="$1"; shift
  local tmo="$1"; shift
  {
    echo "=== CMD: $*"
    echo "=== START_UTC: $(date -u +%FT%TZ)"
    echo "=== CWD: $PWD"
  } >> "$LOG/00_identity_commands.log"
  timeout "$tmo" "$@" > "$ID/$outfile" 2> "$LOG/$outfile.stderr.log"
  local ec=$?
  {
    echo "=== EXIT: $ec"
    echo "=== END_UTC: $(date -u +%FT%TZ)"
    echo
  } >> "$LOG/00_identity_commands.log"
  echo "ec=$ec file=$outfile"
}

{
  echo "collect_utc=$(date -u +%FT%TZ)"
  uname -a
  echo "---os-release---"
  cat /etc/os-release
} > "$ID/host_system.txt" 2>&1

timeout 30 lscpu > "$ID/lscpu.txt" 2>&1; echo "lscpu ec=$?"
timeout 30 free -h > "$ID/free_h.txt" 2>&1;  echo "free ec=$?"
timeout 30 df -h   > "$ID/df_h.txt" 2>&1;    echo "df ec=$?"

run git_status_porcelain_v2.txt 60 git status --porcelain=v2 --branch
run head_revparse.txt           60 git rev-parse HEAD main origin/main 'HEAD^{tree}'
run branches.txt                60 git branch -vv -a
run tags_full.txt              120 git for-each-ref refs/tags --format='%(refname) %(objectname) %(objecttype) %(creatordate:iso8601)'
run tags_points_at_head.txt     60 git tag --points-at HEAD --format='%(refname) %(objectname) %(creatordate:iso8601)'
run remotes_raw.txt             60 git remote -v
run submodules_status_raw.txt   60 git submodule status
echo "== batch1 done =="
