import sys, json, os
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
d=json.load(open(os.path.join(root,"eng/packaging/config/filters.json"),encoding="utf-8"))
F=d["filters"]; P=d["provenance"]["per_filter"]
print("== channel 分配（按品牌）==")
for n in sorted(F): print(f"  {n:40s} ch={F[n]['channel']!r:8s} n={F[n]['n_points']:4d} wl=[{min(F[n]['wavelength_nm'])},{max(F[n]['wavelength_nm'])}]")
print("\n== 声明波长域未被 330-1050 nm 完全包含的条目（PHOTOMETRY.md:230）==")
bad=[(n,F[n]['channel'],min(F[n]['wavelength_nm']),max(F[n]['wavelength_nm'])) for n in F if min(F[n]['wavelength_nm'])<330.0 or max(F[n]['wavelength_nm'])>1050.0]
for r in sorted(bad,key=lambda z:z[2]): print("  ",r)
print("  计数:",len(bad),"/45")
print("\n== 未被消费网格 336-1020/2nm 覆盖（:230 本仓消费口径）==")
bad2=[(n,min(F[n]['wavelength_nm']),max(F[n]['wavelength_nm'])) for n in F if min(F[n]['wavelength_nm'])<336.0 or max(F[n]['wavelength_nm'])>1020.0]
for r in sorted(bad2,key=lambda z:z[1]): print("  ",r)
print("  计数:",len(bad2))
print("\n== T>0 的有效带边 vs 采样端点（前 6 条越界者）==")
for n,_,lo,hi in sorted(bad,key=lambda z:z[2])[:6]:
    wl=F[n]['wavelength_nm']; v=F[n]['value']
    on=[w for w,x in zip(wl,v) if x>0]
    print(f"  {n:40s} 采样[{lo},{hi}] T>0 区[{min(on)},{max(on)}]")
