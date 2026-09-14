import csv, json, sys
p='问题扫描/账本/FIX_LEDGER.csv'
rows=list(csv.DictReader(open(p,encoding='utf-8')))
print('TOTAL', len(rows))
print('COLS', list(rows[0].keys()))
key_disp=[k for k in rows[0].keys() if 'disposition' in k.lower() or '处置' in k]
print('DISPKEY', key_disp)
for r in rows:
    t=json.dumps(r, ensure_ascii=False)
    if any(k in t for k in ['SNR','snr','NOISE','噪声']):
        print('='*8)
        for k,v in r.items():
            if v and v.strip(): print(k,':',v[:300].replace(chr(10),' / '))