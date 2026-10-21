"""Finite input/domain and one-node cached-work budget negative controls."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import round2_performance as driver

HERE=Path(__file__).resolve().parent
OUT=HERE.parent/'evidence'/'round2_numeric_admission_review.json'


def main():
    base={'name':'cache_one_work_unit','header':driver.fixture(),'nside':65536,'pixfrac':.7,
          'tolerance_sr':1e-30,'max_nodes':1,'side':1,'cache':1,'threads':1}
    cases=[base]
    def add(name,**change):
        c=dict(base);c['header']=dict(base['header']);c.update(name=name,cache=0)
        c.update(change);cases.append(c);return c
    add('nan_cd')['header']['CD1_1']='nan'
    add('nan_pixfrac',pixfrac='nan')
    add('unsupported_tangent')['header'].update(CD1_1=400.,CD2_2=400.)
    add('injectivity_unresolved')['header'].update(CTYPE1='RA---TAN-SIP',CTYPE2='DEC--TAN-SIP',A_ORDER=2,B_ORDER=2,A_0_2=3.)
    add('declination_out_of_range')['header']['CRVAL2']=91.
    result={'command':'python 实验/science-foundation/oracles/round2_numeric_admission_review.py',
            'source_sha256':{n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in
                             ('round2_affine.cpp','round2_numeric_admission_review.py','round2_performance.py')},
            'qualification':'Finite negative parser/domain probes; not a general C++ parser robustness or formal machine proof',
            'cases':[]}
    with tempfile.TemporaryDirectory(prefix='acsd-domain-admission-') as temp:
        exe,env,build=driver.build(temp);result['build']=build
        for c in cases:
            encoded=driver.encode(c)
            cp=subprocess.run([str(exe)],input=encoded,env=env,text=True,capture_output=True,timeout=15)
            rec={'request':c,'encoded_input':encoded,'returncode':cp.returncode,'stdout':cp.stdout,'stderr':cp.stderr}
            if c['name']=='cache_one_work_unit':
                measured=json.loads(cp.stdout);rec['measurement']=measured
                assert measured['nodes']<=1 and measured['cache_local_retries']==0
                rec['budget_respected']=True
            else:
                assert cp.returncode==2,(c['name'],cp.returncode,cp.stdout,cp.stderr)
                rec['explicit_rejection']=True
            result['cases'].append(rec)
    OUT.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
    print([(x['request']['name'],x['returncode'],x['stderr'].strip()) for x in result['cases']],flush=True)


if __name__=='__main__': main()
