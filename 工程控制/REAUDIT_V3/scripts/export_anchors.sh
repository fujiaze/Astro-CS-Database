#!/usr/bin/env bash
# AstroCS audit supplement V2 - section 4: export A/B/C trees and seam-path raw diffs
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
REPO="/home/lighthouse/Astro CS Database"
LOG="$ROOT/logs"
HS="$ROOT/package/02_historical_seam"
mkdir -p "$LOG" "$HS"
cd "$REPO" || exit 9

A=b38b446e63d0d27eac672b85ce30527399a057fc
B=83471979a1dd778b4e557a9c7a92e22c137107f3

# --- full-tree exports (out-of-repo, no branch creation) ---
export_tree() { # export_tree <sha> <destdir>
  local sha="$1" dest="$2"
  {
    echo "=== CMD: git archive $sha | tar -x -C $dest"
    echo "=== START_UTC: $(date -u +%FT%TZ)"
    timeout 300 git archive "$sha" | tar -x -C "$dest"
    echo "=== PIPE_EXIT: $?"
    echo "=== END_UTC: $(date -u +%FT%TZ)"
  } >> "$LOG/04_anchor_exports.log"
}
export_tree HEAD   "$ROOT/current"
export_tree "$A"   "$ROOT/historical_b38b446"
export_tree "$B"   "$ROOT/historical_8347197"
echo "trees exported"

# --- seam-relevant path list (used for restricted diffs) ---
SEAM_PATHS="lib/phase2 lib/healpix_db/healpix_drizzle lib/astro_image_io lib/orchestrator/configs docs/science/PHASE2_UPM.md docs/science/DRIZZLE.md docs/algorithms/UPM_SOLVER.md"

# --- restricted diffs A->B and B->C ---
{
  echo "=== CMD: git diff $A $B -- $SEAM_PATHS"
  timeout 120 git diff "$A" "$B" -- $SEAM_PATHS > "$HS/seam_paths_diff_b38b446_to_8347197.patch" 2>&1
  echo "=== patch_exit=$?"
  timeout 120 git diff --name-status "$A" "$B" -- $SEAM_PATHS > "$HS/seam_paths_name-status_b38b446_to_8347197.txt" 2>&1
} > /dev/null 2>&1
{
  echo "=== CMD: git diff $B HEAD -- $SEAM_PATHS"
  timeout 180 git diff "$B" HEAD -- $SEAM_PATHS > "$HS/seam_paths_diff_8347197_to_current.patch" 2>&1
  echo "=== patch_exit=$?"
  timeout 120 git diff --name-status "$B" HEAD -- $SEAM_PATHS > "$HS/seam_paths_name-status_8347197_to_current.txt" 2>&1
} > /dev/null 2>&1
echo "restricted diffs done"

# --- seam-related file inventory per tree (GC 3-panel/T4/seam/UPM/mosaic keywords) ---
inv() { # inv <treedir> <outfile>
  local d="$1" o="$2"
  {
    echo "# tree=$d"
    find "$d" -type f \( -path "*lib/phase2/*" -o -path "*healpix_drizzle/*" -o -path "*lib/orchestrator/configs*" \) | sort
    echo ""
    echo "# files whose name/content hints GC-3panel/T4/seam/UPM/mosaic:"
    grep -rIlE 'galaxy.?center|gc_panel|panel[123]|\bT4\b|seam|upm|mosaic' "$d/lib/orchestrator" "$d/lib/phase2" "$d/docs" 2>/dev/null | sed "s|^$d/||" | sort | head -200
  } > "$o" 2>&1
}
inv "$ROOT/current"                "$HS/seam_related_files_current.txt"
inv "$ROOT/historical_b38b446"     "$HS/seam_related_files_b38b446.txt"
inv "$ROOT/historical_8347197"     "$HS/seam_related_files_8347197.txt"
echo "inventories done"

# --- hunk-header extraction for semantic CSV groundwork ---
python3 - <<'PYEOF' > "$HS/hunk_symbol_index.csv"
import subprocess, csv, sys, re

repo = "/home/lighthouse/Astro CS Database"
pairs = [
    ("b38b446e63d0d27eac672b85ce30527399a057fc", "83471979a1dd778b4e557a9c7a92e22c137107f3", "A_b38b446", "B_8347197"),
    ("83471979a1dd778b4e557a9c7a92e22c137107f3", "HEAD", "B_8347197", "C_current_main"),
]
paths = ["lib/phase2", "lib/healpix_db/healpix_drizzle", "lib/astro_image_io",
         "lib/orchestrator/configs", "docs/science/PHASE2_UPM.md", "docs/science/DRIZZLE.md",
         "docs/algorithms/UPM_SOLVER.md"]

w = csv.writer(sys.stdout)
w.writerow(["commit_from","commit_to","module","file","hunk_header_or_config_key","change_kind_raw"])
for a, b, fa, fb in pairs:
    outp = subprocess.run(["git", "-C", repo, "diff", "--unified=0", a, b, "--"] + paths,
                          capture_output=True, text=True).stdout
    cur_file = None
    module = ""
    for line in outp.splitlines():
        if line.startswith("diff --git"):
            m = re.search(r" b/(.+)$", line)
            cur_file = m.group(1) if m else None
            if cur_file:
                parts = cur_file.split("/")
                module = "/".join(parts[:2]) if parts[0] in ("lib","docs") else parts[0]
        elif line.startswith("@@") and cur_file:
            w.writerow([fa, fb, module, cur_file, line.strip(), "code_hunk"])
        elif line.startswith("diff --git"):
            pass
        elif cur_file and (line.startswith("+") or line.startswith("-")) and "=" in line and not line.startswith("+++") and not line.startswith("---"):
            m = re.match(r"^([+-])\s*([A-Za-z0-9_.]+)\s*=", line)
            if m:
                kind = "config_key_added" if m.group(1) == "+" else "config_key_removed"
                w.writerow([fa, fb, module, cur_file, m.group(2), kind])
PYEOF
echo "hunk index rows=$(( $(wc -l < "$HS/hunk_symbol_index.csv") - 1 ))"
echo "== anchors done =="
