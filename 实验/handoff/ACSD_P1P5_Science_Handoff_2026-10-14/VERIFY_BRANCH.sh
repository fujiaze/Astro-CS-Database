#!/usr/bin/env bash
set -euo pipefail
EXPECTED="e31b2be66ffe37d547e67a4ba007e7257e49789d"
BRANCH="audit/acsd-science-docs-p5"
echo "Repository: $(git remote get-url origin 2>/dev/null || echo UNKNOWN)"
echo "Branch: $(git branch --show-current)"
echo "Current: $(git rev-parse HEAD)"
if [ "$(git branch --show-current)" != "$BRANCH" ]; then
  echo "ERROR: not on the specified branch" >&2; exit 2
fi
if ! git merge-base --is-ancestor "$EXPECTED" HEAD; then
  echo "ERROR: the handoff baseline SHA is not in current HEAD ancestry" >&2; exit 3
fi
if [ -n "$(git status --porcelain)" ]; then
  echo "WARNING: worktree contains uncommitted changes. Protect them before execution." >&2
  git status --short
fi
for p in  '实验/README.md'  '实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md'  '实验/shared/references/CORE_REFERENCES_VERIFIED.bib'  '实验/shared/references/PRIOR_ART_CLAIM_MATRIX.md'  '实验/engineering-evidence/acsd-science-docs-p5/DEEPSEEK_SCIENCE_EXECUTION.md'  '实验/photometric-magnitude/REPORT_paper.md'  '实验/absolute-snr/REPORT_paper.md'  '实验/healpix-polar/REPORT_paper.md'  '实验/dense-snr-reconstruct/REPORT_paper.md'  '实验/additive-sky-seamless/REPORT_paper.md'; do
  [ -f "$p" ] || { echo "ERROR: missing $p" >&2; exit 4; }
done
echo "PASS: exact scientific revision baseline is included; required paper files exist."
