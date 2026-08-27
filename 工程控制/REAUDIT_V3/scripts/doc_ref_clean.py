import re, os, json
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
SRC = os.path.join(ROOT, "current")
# 1) genuine markdown links [text](path) where path looks like a file
link_re = re.compile(r'\[[^\]]*\]\(([^)]+)\)')
bad_links = []; good_links = 0; checked = 0
for dp, dn, fn in os.walk(SRC):
    if "/.git" in dp: continue
    for f in fn:
        if f.endswith(".md"):
            p = os.path.join(dp, f)
            for m in link_re.finditer(open(p, encoding="utf-8", errors="ignore").read()):
                tgt = m.group(1).strip()
                # skip urls, anchors-only, code
                if tgt.startswith("http") or tgt.startswith("#") or tgt.startswith("mailto"):
                    continue
                if re.search(r'\s', tgt):  # not a real path
                    continue
                checked += 1
                # resolve relative to doc dir
                full = os.path.normpath(os.path.join(dp, tgt))
                if os.path.exists(full):
                    good_links += 1
                else:
                    bad_links.append((os.path.relpath(p, SRC), tgt))
# 2) genuine commit SHAs (40-hex) in docs that are claimed as refs
sha_re = re.compile(r'\b[0-9a-f]{40}\b')
bad_shas = []; good_shas = 0
for dp, dn, fn in os.walk(SRC):
    if "/.git" in dp: continue
    for f in fn:
        if f.endswith((".md", ".json")):
            txt = open(os.path.join(dp, f), encoding="utf-8", errors="ignore").read()
            for sha in sha_re.findall(txt):
                r = os.popen(f"git -C {SRC} cat-file -e {sha} 2>/dev/null && echo yes || echo no").read().strip()
                if r == "yes": good_shas += 1
                else: bad_shas.append((os.path.relpath(os.path.join(dp,f), SRC), sha))
out = {
  "method": "genuine-only re-validation (round 133): [text](path) markdown links to local files + 40-hex commit SHAs resolvable in the repo; API signatures in backticks NOT counted as links (the earlier tool's false positives excluded)",
  "markdown_file_links_checked": checked,
  "markdown_file_links_good": good_links,
  "markdown_file_links_broken": len(bad_links),
  "broken_links_sample": bad_links[:8],
  "commit_shas_checked": good_shas + len(bad_shas),
  "commit_shas_good": good_shas,
  "commit_shas_bad": len(bad_shas),
  "bad_shas_sample": bad_shas[:5]
}
print(json.dumps(out, indent=1, ensure_ascii=False)[:2000])
open("/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/doc_ref_clean.json","w").write(json.dumps(out, indent=1, ensure_ascii=False))
