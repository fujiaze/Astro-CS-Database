
import json
res=json.load(open('问题扫描/_cache/v20_unused_params.json'))
for path,lineno,name,unused in res:
    if any(k in path for k in ('sdet_api','gaia_client','hp_drizzle_api','aio_pipeline','healpix_core','stack_engine','drizzle_engine','spherical_overlap','ipv_select','executor','profile_reader','cost_estimator','aio_publish')):
        print(f'{path}:{lineno}  {name}  {unused}')
