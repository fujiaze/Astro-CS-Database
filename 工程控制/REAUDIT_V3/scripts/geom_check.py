
import csv
rows = list(csv.DictReader(open('/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/package/03_testdata/testdata_manifest.csv')))
frames = [r for r in rows if r.get('panel') and r['panel'] != '']
print("total rows:", len(rows), "frames:", len(frames))
print("cols:", list(rows[0].keys()))
for r in frames[:40]:
    print(r.get('index'), r.get('panel'), 'WCS=', r.get('wcs_present'), 'CR=', r.get('wcs_crval'), 'CD=', r.get('wcs_cd_or_pc'), 'bit=', r.get('bitpix_or_dtype'), 'exp=', r.get('exposure'), 'w=', r.get('width'), 'h=', r.get('height'))
