#!/usr/bin/env bash
# AstroCS audit supplement V2 - section 3: freeze audit object into package/01_repository
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
REPO="/home/lighthouse/Astro CS Database"
P="$ROOT/package/01_repository"
LOG="$ROOT/logs"
mkdir -p "$LOG"
cd "$REPO" || exit 9

H=$(git rev-parse HEAD)
T=$(git rev-parse 'HEAD^{tree}')

# --- current_identity.json ---
cat > "$P/current_identity.json" <<EOF
{
  "collected_utc": "$(date -u +%FT%TZ)",
  "head": "$H",
  "head_tree": "$T",
  "main": "$(git rev-parse main)",
  "origin_main": "$(git rev-parse origin/main)",
  "author_time": "$(git log -1 --format=%aI)",
  "committer_time": "$(git log -1 --format=%cI)",
  "subject": $(git log -1 --format=%s | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'),
  "body_present": $( [ -n "$(git log -1 --format=%b)" ] && echo true || echo false )
}
EOF
echo "current_identity.json ok"

# --- main-source.tar.gz ---
{
  echo "=== CMD: git archive --format=tar.gz -o <pkg>/main-source.tar.gz main"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
} > "$LOG/01_archive.log"
S=$(date +%s)
timeout 300 git archive --format=tar.gz -o "$P/main-source.tar.gz" main >> "$LOG/01_archive.log" 2>&1
EC=$?
E=$(date +%s)
{ echo "=== EXIT: $EC"; echo "=== DURATION_S: $((E-S))"; echo "=== END_UTC: $(date -u +%FT%TZ)"; } >> "$LOG/01_archive.log"
echo "archive ec=$EC"

# --- main-history.bundle ---
{
  echo "=== CMD: git bundle create main-history.bundle main --tags"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
} > "$LOG/01_bundle.log"
S=$(date +%s)
timeout 300 git bundle create "$P/main-history.bundle" main --tags >> "$LOG/01_bundle.log" 2>&1
BEC=$?
E=$(date +%s)
{
  echo "=== CREATE_EXIT: $BEC";
  echo "=== CMD: git bundle verify main-history.bundle"
  timeout 120 git bundle verify "$P/main-history.bundle" 2>&1
  echo "=== VERIFY_EXIT: $?"
  echo "=== DURATION_S: $((E-S))"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} >> "$LOG/01_bundle.log"
echo "bundle create ec=$BEC"

# --- tracked_files.txt ---
git ls-files -s > "$P/tracked_files.txt" 2>&1
echo "tracked_files lines=$(wc -l < "$P/tracked_files.txt")"

# --- histories ---
git log --first-parent --format='commit %H%nauthor_date %aI%ncommitter_date %cI%nsubject %s' --shortstat main > "$P/commit_history_first_parent.txt" 2>&1
echo "first_parent lines=$(wc -l < "$P/commit_history_first_parent.txt")"

{
  git log 83471979a1dd778b4e557a9c7a92e22c137107f3..HEAD --format='commit %H%nauthor %aN <%aE>%nauthor_date %aI%nsubject %s%nbody %b'
  echo "==== NAME-STATUS ===="
  git log 83471979a1dd778b4e557a9c7a92e22c137107f3..HEAD --name-status --format='--- commit %h %s'
  echo "==== STAT ===="
  git log 83471979a1dd778b4e557a9c7a92e22c137107f3..HEAD --stat --format='--- commit %h %s' | tail -n +1
} > "$P/commit_history_since_8347197.txt" 2>&1
echo "since_8347197 lines=$(wc -l < "$P/commit_history_since_8347197.txt")"

# --- diffs ---
timeout 120 git diff 83471979a1dd778b4e557a9c7a92e22c137107f3 HEAD > "$P/diff_8347197_current.patch" 2>&1
echo "diff_8347197_current bytes=$(wc -c < "$P/diff_8347197_current.patch")"
timeout 120 git diff b38b446e63d0d27eac672b85ce30527399a057fc 83471979a1dd778b4e557a9c7a92e22c137107f3 > "$P/diff_b38b446_8347197.patch" 2>&1
echo "diff_b38b446_8347197 bytes=$(wc -c < "$P/diff_b38b446_8347197.patch")"

# --- source inventory CSV ---
python3 - <<'PYEOF' > "$P/source_inventory.csv"
import subprocess, csv, sys, os, re

repo = "/home/lighthouse/Astro CS Database"

def git(*args):
    return subprocess.run(["git", "-C", repo] + list(args), capture_output=True, text=True).stdout

ls = git("ls-files", "-s").splitlines()
w = csv.writer(sys.stdout)
w.writerow(["path","language","module","size_bytes","blob_sha","is_generated","is_third_party","is_test","is_doc"])

def classify(path):
    p = path.lower()
    ext = os.path.splitext(p)[1]
    lang_map = {".cpp":"cpp",".cc":"cpp",".cxx":"cpp",".h":"cpp_header",".hpp":"cpp_header",".hxx":"cpp_header",
                ".py":"python",".md":"markdown",".json":"json",".yaml":"yaml",".yml":"yaml",".csv":"csv",
                ".txt":"text",".cmake":"cmake",".sh":"shell",".ps1":"powershell",".ui":"qt_ui",".qrc":"qt_qrc",
                ".frag":"glsl",".vert":"glsl",".glsl":"glsl",".xml":"xml",".html":"html",".css":"css",".js":"javascript"}
    language = lang_map.get(ext, "other")
    if ext == ".txt" and "CMakeLists" in path:
        language = "cmake"
    is_test = any(s in p for s in ["test", "/tests/", "_test", "test_", "self_review"]) or p.startswith("testdata/")
    is_doc = ext in (".md", ".rst", ".adoc") or "/docs/" in p or p.startswith("docs/")
    is_generated = any(s in p for s in ["generated", "/gen/", ".pb.h", ".pb.cc"])
    is_third_party = any(s in p for s in ["third_party", "vendor/", "external/", "/extlib"])
    if re.search(r"/(build|out)/", p):
        is_generated = True
    # module inference
    parts = path.split("/")
    if len(parts) >= 2:
        module = parts[0]
        if parts[0] in ("lib", "docs", "tools", "evidence", "reports"):
            module = "/".join(parts[:2]) if parts[0] != "lib" else "lib/" + parts[1]
        elif parts[0] == "工程控制":
            module = "工程控制/" + (parts[1] if len(parts) > 1 else "")
    else:
        module = "(root)"
    return language, module, is_test, is_doc, is_generated, is_third_party

for line in ls:
    meta = line.split()
    if len(meta) < 4:
        continue
    mode, blob, _stage, path = meta[0], meta[1], meta[2], meta[3]
    size_out = subprocess.run(["git", "-C", repo, "cat-file", "-s", blob], capture_output=True, text=True)
    try:
        size = int(size_out.stdout.strip())
    except ValueError:
        size = -1
    language, module, is_test, is_doc, is_gen, is_3p = classify(path)
    w.writerow([path, language, module, size, blob,
                "true" if is_gen else "false",
                "true" if is_3p else "false",
                "true" if is_test else "false",
                "true" if is_doc else "false"])
PYEOF
echo "inventory rows=$(( $(wc -l < "$P/source_inventory.csv") - 1 ))"

# --- tar integrity spot check ---
timeout 60 tar -tzf "$P/main-source.tar.gz" | head -5 > "$LOG/01_tar_spotcheck.txt" 2>&1
echo "tar spotcheck ec=$?"
timeout 60 bash -c "tar -tzf '$P/main-source.tar.gz' | wc -l" > "$LOG/01_tar_entrycount.txt" 2>&1
echo "tar entries: $(cat "$LOG/01_tar_entrycount.txt")"
echo "== freeze done =="
