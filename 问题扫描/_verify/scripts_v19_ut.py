
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
SKIPRX = re.compile(r'SkipTest|skipIf|skipUnless|pytest\.skip|sys\.exit\(77\)|exit\(0\)|return\s*$')
ART = re.compile(r'(["\x27/])(build|run|artifacts|evidence|GaiaDR3SP?|BASS DR3)/')
NET = re.compile(r'urllib|requests\.|socket\.|http://|https://|gaia\.astra|gea\.esa|TAP|astropy\.utils\.data|download')
rows = []
for c in cs:
    cmd = c['command']
    if 'discover' not in cmd: continue
    s = cmd[cmd.index('-s')+1]
    files = sorted(t for t in tracked if t.startswith(s+'/') and t.endswith('.py'))
    art_mods, net_mods, skip_mods = [], [], []
    for f in files:
        src = open(f, encoding='utf-8', errors='replace').read()
        hit_art = [m for m in re.finditer(r'(REPO\s*/\s*["\x27](?:build|run|artifacts|evidence)[^"\x27]*["\x27])|(["\x27](?:build|run|artifacts|evidence|GaiaDR3SP?|BASS DR3)/[^"\x27]{1,60}["\x27])|(os\.path\.join\([^)]*(?:build|run|artifacts)[^)]*\))', src)]
        if hit_art: art_mods.append((f, sorted({re.sub(r'\s+',' ',m.group(0))[:70] for m in hit_art})[:3]))
        if NET.search(src): net_mods.append(f)
        if re.search(r'SkipTest|skipIf|skipUnless', src): skip_mods.append(f)
    print('### %-22s dir=%-26s files=%-3d artifact-dep=%-3d skip-guard=%-3d net=%d' % (c['id'], s, len(files), len(art_mods), len(skip_mods), len(net_mods)))
    for f, ex in art_mods: print('     ART %-58s %s' % (f, ex))
    if net_mods: print('     NET', net_mods[:6])
