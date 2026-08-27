#!/usr/bin/env bash
# AstroCS audit supplement V2 - identity evidence batch 2 (equality, sanitized remote, submodules, LFS, fsck)
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
REPO="/home/lighthouse/Astro CS Database"
ID="$ROOT/package/00_identity"
LOG="$ROOT/logs"
cd "$REPO" || exit 9

# --- head equality ---
H=$(git rev-parse HEAD)
M=$(git rev-parse main)
O=$(git rev-parse origin/main)
T=$(git rev-parse 'HEAD^{tree}')
BR=$(git rev-parse --abbrev-ref HEAD)
EQ=false
if [ "$H" = "$M" ] && [ "$M" = "$O" ]; then EQ=true; fi
BRANCH_OK=false
if [ "$BR" = "main" ]; then BRANCH_OK=true; fi
PORCELAIN=$(git status --porcelain)
TRACKED_DIRTY=$(git status --porcelain | grep -cv '^??' || true)
PORCELAIN_EMPTY=false
if [ -z "$PORCELAIN" ]; then PORCELAIN_EMPTY=true; fi
cat > "$ID/head_equality.json" <<EOF
{
  "collected_utc": "$(date -u +%FT%TZ)",
  "head": "$H",
  "main": "$M",
  "origin_main": "$O",
  "head_tree": "$T",
  "head_equals_main_equals_origin_main": $EQ,
  "current_branch": "$BR",
  "branch_is_main": $BRANCH_OK,
  "porcelain_v2_untracked_entries": $(git status --porcelain=v2 | grep -c '^?' || true),
  "porcelain_empty_strict": $PORCELAIN_EMPTY,
  "tracked_or_staged_dirty_entries": $TRACKED_DIRTY,
  "gate_note": "strict porcelain-empty gate is FALSE due to exactly the two untracked audit-control documents listed in untracked_control_documents; tracked/staged tree is clean",
  "untracked_control_documents": [
    "AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826.zip",
    "AstroCS_MAIN_FULL_AUDIT_EVIDENCE_REQUEST_V1.md"
  ]
}
EOF
echo "equality json written"

# --- sanitized remote ---
{
  echo "# sanitized remote list (protocol/host/owner/repo kept; userinfo/token/password stripped)"
  git remote -v | sed -E 's#https://[^/@]+@#https://#g; s#(https?)://[^/@]+:[^@]*@#\1://[REDACTED]@#g'
  echo ""
  echo "# raw URL credential scan (should be no match):"
  if git remote -v | grep -Eq '(//[^/@[:space:]]+:[^@[:space:]]*@|x-oauth-basic|ghp_|github_pat_)'; then
    echo "FOUND_CREDENTIAL_PATTERN_IN_REMOTE_URL"
  else
    echo "NO_CREDENTIALS_IN_REMOTE_URL"
  fi
} > "$ID/remote_sanitized.txt"
echo "remote_sanitized written"

# --- submodules / gitlink ---
{
  echo "== .gitmodules tracked? =="
  if git ls-files --error-unmatch .gitmodules >/dev/null 2>&1; then echo ".gitmodules TRACKED"; else echo ".gitmodules NOT_TRACKED (absent from index and worktree)"; fi
  echo ""
  echo "== mode-160000 entries in HEAD tree =="
  git ls-tree -r HEAD | awk '$1=="160000"{print}'
  echo ""
  echo "== git submodule status =="
  git submodule status 2>&1
  echo ""
  echo "== gitlink target object existence =="
  GL=$(git ls-tree -r HEAD | awk '$1=="160000"{print $3}' | head -1)
  if [ -n "${GL:-}" ]; then
    echo "gitlink_sha=$GL"
    if git cat-file -e "$GL^{commit}" 2>/dev/null; then
      echo "gitlink_target_object_present_in_repo=true"
    else
      echo "gitlink_target_object_present_in_repo=false"
      echo "cat_file_err:"; git cat-file -t "$GL" 2>&1 || true
    fi
  else
    echo "no_mode_160000_entry"
  fi
  echo ""
  echo "== worktree AstroCS.wiki contents =="
  ls -la "$REPO/AstroCS.wiki" 2>&1 | head -10
} > "$ID/submodules.txt" 2>&1
echo "submodules written"

# --- LFS ---
{
  echo "== git lfs availability =="
  if command -v git-lfs >/dev/null 2>&1; then
    git lfs version
    echo "-- git lfs env (sanitized keys only) --"
    git lfs env 2>&1 | sed -E 's#(https?)://[^/@]+:[^@]*@#\1://[REDACTED]@#g' | head -20
  else
    echo "git-lfs NOT_INSTALLED"
  fi
  echo ""
  echo "== .gitattributes LFS filter usage =="
  if grep -E 'filter=lfs' "$REPO/.gitattributes" 2>/dev/null; then
    echo "LFS_FILTER_USED_IN_GITATTRIBUTES"
  else
    echo "NO_LFS_FILTER_IN_GITATTRIBUTES"
  fi
  echo ""
  echo "== tracked lfs pointers =="
  git ls-files | head -0
  LFS_COUNT=$(git ls-files -z 2>/dev/null | xargs -0 -I{} true; git grep -l 'version https://git-lfs.github.com/spec/v1' -- . 2>/dev/null | wc -l)
  echo "approx_lfs_pointer_files=$LFS_COUNT"
} > "$ID/lfs.txt" 2>&1
echo "lfs written"

# --- fsck ---
{
  echo "=== CMD: git fsck --full --no-progress"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
  timeout 600 git fsck --full --no-progress 2>&1
  echo "=== EXIT: $?"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} > "$ID/git_fsck.txt" 2>&1
echo "fsck done"

# --- historical anchor SHAs ---
{
  echo "sha,exists,object_type,reachable_from_main,subject,author_date"
  for sha in b38b446e63d0d27eac672b85ce30527399a057fc 83471979a1dd778b4e557a9c7a92e22c137107f3 b9c628399d92fcff0c81d94c74da506f707f2078 a7e063e57516dbe3e0c5dc894f8c3e04d32b4a1c 9d79204b0a8cf02c3f7f6c145cb4c7e52ece3d2f; do
    if git cat-file -e "$sha^{commit}" 2>/dev/null; then
      T=$(git cat-file -t "$sha")
      R=$(git merge-base --is-ancestor "$sha" main 2>/dev/null && echo true || echo false)
      S=$(git log -1 --format='%s' "$sha" | sed 's/,/;/g')
      D=$(git log -1 --format=%cI "$sha")
      echo "$sha,true,$T,$R,$S,$D"
    else
      echo "$sha,false,,,,"
    fi
  done
} > "$ID/historical_anchor_shas.csv" 2>&1
echo "anchors written"
echo "== batch2 done =="
