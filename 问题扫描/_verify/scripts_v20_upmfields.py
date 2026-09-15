
import re, os, collections
fields=['robust_loss','snr_weight_mode','huber_delta','smoothing_lambda','zero_anchor_weight','max_iterations','tolerance','target_order','sigma_floor','support_power','quality_mode','use_ivar_weight','control_reliability','input_manifest_hash','cpu_workers','grid']
SRC=[]
for root in ['lib','cli','runtime','providers','modules','tests','include']:
    for dp,dn,fn in os.walk(root):
        if 'third_party' in dp: continue
        for f in fn:
            if f.endswith(('.c','.cpp','.cc','.h','.hpp')): SRC.append(os.path.join(dp,f))
def strip(t):
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//[^\n]*','',t); return t
blobs={p:strip(open(p,encoding='utf-8',errors='replace').read()) for p in SRC}
hdr='lib/phase2/include/astro/phase2/upm.h'
for f in fields:
    wr=[];rd=[]
    pw=re.compile(r'(?:\b(?:uc|mcfg|cfg|c)\.)'+re.escape(f)+r'\s*(?:=[^=]|\+\+|\|=)')
    pr=re.compile(r'(?:\b(?:uc|mcfg|cfg|c)\.)'+re.escape(f)+r'\b')
    for p,b in blobs.items():
        if p==hdr: continue
        if len(pw.findall(b)): wr.append(p)
        if len(pr.findall(b)): rd.append(p)
    tag='WRITE-ONLY' if wr and not rd else ('READ-ONLY' if rd and not wr else 'both')
    print(f'{f:24s} {tag:11s} W={len(wr)} R={len(rd)}')
    if tag=='WRITE-ONLY': print('     writers:', sorted(set(wr))[:6])
    if tag=='READ-ONLY': print('     readers:', sorted(set(rd))[:6])
