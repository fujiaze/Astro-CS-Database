# -*- coding: utf-8 -*-
import os, re, json, collections
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
SCOPE=("lib/","tests/","providers/","cli/","runtime/","modules/","include/","tools/","scripts/","ci/","cmake/","packaging/","docs/","contracts/")
CODEEXT={".c",".h",".cpp",".hpp",".cc",".cu",".py",".inc",".sh",".ps1",".js"}
DOCEXT={".md",".yaml",".yml",".json",".csv",".txt"}
FAM = {
 "MAD15": (r"1\.482602218505602", "MAD→σ 15位冻结值"),
 "MAD4":  (r"1\.4826(?!\d)", "MAD→σ 4位截断"),
 "MAD11": (r"1\.4826022185(?!05602)", "MAD→σ 11位"),
 "INV06745": (r"0\.6745(?!\d)", "0.6745 (=1/1.4826 倒数式)"),
 "FWHM": (r"1\.23031(\d*)", "Moffat4 FWHM/sigma 因子"),
 "CAPPED200000": (r"\b200000\b|MAX_STARS_RESULT|m_lim_gaia_cap_per_file", "Gaia 每文件触顶上限"),
 "GRID8": (r"\bgrid\b\s*(=|==|!=|:|\b)\s*8|8\s*[x\u00d7]\s*8|control_grid_per_tile", "UPM 控制网格 8"),
 "WL343": (r"\b343\b|WL_COUNT", "Gaia DR3SP 波长数 343"),
 "RECLAIM05": (r"reclaim|0\.5\s*\*|kReclaim|32\s*\*\s*1024\s*\*\s*1024|33554432|UnexplainedResidual|AllocReclaim", "F-14 内存回收门 0.5/32MiB"),
 "CPU8560": (r"kCpuMeanMinPercent|kCpuLowDip|0\.85\b|0\.60\b|85\.0\b|60\.0\b", "§10.5/§17.6 CPU 85%/60% 门"),
 "MARGIN025": (r"0\.25\b", "0.25 星等剪枝裕量"),
 "MARGIN12": (r"1\.2\b", "1.2 bbox 裕量"),
 "M60": (r"\bm_cut\b|6\.0\s*\+|1\.5\s*\*\s*std::log10|m_lim_m0_offset|m_lim_clamp|m_lim_safety", "IPV 初值公式 6.0/1.5/2.0 与 ±13.0 钳位"),
}
res = {k: {"code":[], "doc":[], "files":collections.Counter()} for k in FAM}
for f in sorted(tracked):
    if not f.startswith(SCOPE): continue
    ext = os.path.splitext(f)[1]
    if ext not in CODEEXT and ext not in DOCEXT: continue
    iscode = ext in CODEEXT
    p = os.path.join(root,f)
    try: lines = open(p,encoding="utf-8",errors="ignore").read().splitlines()
    except Exception: continue
    for i,l in enumerate(lines,1):
        for k,pat in ((k,v[0]) for k,v in FAM.items()):
            if re.search(pat,l):
                rec=(f,i,l.strip()[:200])
                res[k]["code" if iscode else "doc"].append(rec)
                res[k]["files"][f]+=1
summary={k:{"code":len(v["code"]),"doc":len(v["doc"]),"nfiles":len(v["files"])} for k,v in res.items()}
print(json.dumps(summary,ensure_ascii=False,indent=1))
json.dump({k:{"code":v["code"],"doc":v["doc"],"files":dict(v["files"])} for k,v in res.items()}, open(os.path.join(root,"问题扫描/_cache/v12_fam.json"),"w"),ensure_ascii=False)
