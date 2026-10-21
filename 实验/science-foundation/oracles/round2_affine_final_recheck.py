"""Recheck final C++ integer/candidate fixes; reuse unchanged independent A/B runs.

Inputs and A/B source hashes are verified against preserved full-run records.
Only C++ is re-executed; this is explicitly recorded, never a new A/B run claim.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import round2_performance as driver

HERE=Path(__file__).resolve().parent
EVIDENCE=HERE.parent/'evidence'


def main():
    with tempfile.TemporaryDirectory(prefix='acsd-final-recheck-') as temp:
        exe,env,build=driver.build(temp)
        for stem in ('round2_affine_independent_review','round2_affine_sip_review'):
            old_path=EVIDENCE/(stem+'_pre_candidate_prune.json')
            old=json.loads(old_path.read_text(encoding='utf-8'))
            for source in ('chart_overlap.py','detector_independent.py','round2_sliver.py'):
                assert old['source_sha256'][source]==hashlib.sha256((HERE/source).read_bytes()).hexdigest()
            records=old['cases'] if 'cases' in old else [old]
            for c in records:
                cp=subprocess.run([str(exe)],input=driver.encode(c['request']),env=env,text=True,
                                   capture_output=True,check=True,timeout=60)
                fresh=json.loads(cp.stdout)
                assert fresh['first']['areas']==c['cpp']['first']['areas']
                c['previous_default_areas_bit_identical']=True
                c['cpp']=fresh
                for comp in c['comparisons']:
                    if comp['method']!='cpp': continue
                    pair=fresh['first']['areas'].get('0',[0.,0.]);pair=[pair[0],pair[1]+fresh['first']['spill_sr']]
                    truth=c['truth']['area']
                    comp.update(leaf0_interval_sr=pair,
                                contains_analytic_interval=pair[0]<=truth['lower_sr']<=truth['upper_sr']<=pair[1],
                                proved_positive=pair[0]>0)
                    assert comp['contains_analytic_interval']
            old['build']=build
            old['command']='python 实验/science-foundation/oracles/round2_affine_final_recheck.py'
            old['reused_A_B_evidence']={'path':old_path.name,
                'sha256':hashlib.sha256(old_path.read_bytes()).hexdigest(),
                'note':'Independent A/B full runs reused unchanged; current final C++ executed again. Analytic truth and input header unchanged.'}
            old['source_sha256']['round2_affine.cpp']=build['source_sha256']
            old['source_sha256']['round2_affine_final_recheck.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
            old['source_sha256']['round2_performance.py']=hashlib.sha256((HERE/'round2_performance.py').read_bytes()).hexdigest()
            destination=EVIDENCE/(stem+'_final.json')
            destination.write_text(json.dumps(old,indent=2,allow_nan=False),encoding='utf-8')
            print(stem,build['source_sha256'],len(records),'unchanged true-leaf bounds; analytic truth contained',flush=True)


if __name__=='__main__':main()
