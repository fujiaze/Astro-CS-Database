"""Known positive but interval-sign-unresolved CD: explicit reject required.

Run old snapshot to expose underbound, then current source to confirm rejection.
Only a research microkernel is compiled; no production C++ is touched.
"""
import argparse
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile
from mpmath import mp
import round2_performance as driver

HERE=Path(__file__).resolve().parent
EVIDENCE=HERE.parent/'evidence'


def main(source,output):
    mp.dps=100
    a,b,c,d=1.740755848013471,.819144672764259,1.9137791654017695,.9005639762607065
    determinant=Fraction(a)*Fraction(d)-Fraction(b)*Fraction(c)
    corners=[]
    for u,v in [(-.5,-.5),(.5,-.5),(.5,.5),(-.5,.5)]:
        corners.append(mp.matrix([(mp.mpf(a)*u+mp.mpf(b)*v)*mp.pi/180,
                                  (mp.mpf(c)*u+mp.mpf(d)*v)*mp.pi/180,1]))
    def dot(p,q):return sum(p[i]*q[i] for i in range(3))
    def tri(p,q,r):
        lp,lq,lr=[mp.sqrt(dot(t,t)) for t in (p,q,r)]
        numerator=abs(mp.det(mp.matrix([[p[i],q[i],r[i]] for i in range(3)])))
        denominator=lp*lq*lr+dot(p,q)*lr+dot(q,r)*lp+dot(r,p)*lq
        return 2*mp.atan2(numerator,denominator)
    truth=tri(*corners[:3])+tri(corners[0],corners[2],corners[3])
    def product(x,y):return math.nextafter(x*y,-math.inf),math.nextafter(x*y,math.inf)
    p,q=product(a,d),product(b,c)
    detlo=math.nextafter(p[0]-q[1],-math.inf)
    dethi=math.nextafter(p[1]-q[0],math.inf)
    h=driver.fixture(45.,0.,3600.)
    h.update(CRPIX1=1.,CRPIX2=1.,CD1_1=a,CD1_2=b,CD2_1=c,CD2_2=d)
    request={'name':'positive_cd_uncertain_binary64_sign','header':h,'nside':1,'pixfrac':1.,
             'tolerance_sr':1e-25,'max_nodes':1,'side':1,'cache':0,'threads':1}
    result={'command':f'python 实验/science-foundation/oracles/round2_cd_domain_review.py --source {source} --output {output}',
            'request':request,'exact_binary64_determinant_deg2':str(determinant),
            'exact_determinant_decimal_deg2':mp.nstr(mp.mpf(determinant.numerator)/determinant.denominator,90),
            'raw_binary64_interval_determinant_deg2':[detlo,dethi],
            'positive_spherical_area_sr':mp.nstr(truth,90),
            'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'driver_sha256':hashlib.sha256(Path(driver.__file__).read_bytes()).hexdigest(),
            'review_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix='acsd-cd-domain-') as temp:
        driver.SOURCE=source
        exe,env,build=driver.build(temp);result['build']=build
        cp=subprocess.run([str(exe)],input=driver.encode(request),env=env,text=True,capture_output=True,timeout=30)
        result.update(returncode=cp.returncode,stderr=cp.stderr,stdout=cp.stdout)
        if cp.returncode==0:
            measurement=json.loads(cp.stdout);result['measurement']=measurement
            upper=sum(mp.mpf(v[1]) for v in measurement['first']['areas'].values())+mp.mpf(measurement['first']['spill_sr'])
            result['contains_true_positive_total_area']=upper>=truth
            result['judgment']='UNSAFE_ABSOLUTE_DETERMINANT_UNDERBOUND' if upper<truth else 'no_underbound_on_this_input'
        else:
            result['judgment']='explicit_input_domain_rejection' if cp.returncode==2 else 'unexpected_failure'
    output.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
    print(result['judgment'],result['returncode'],flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=HERE/'round2_affine.cpp')
    parser.add_argument('--output',type=Path,default=EVIDENCE/'round2_cd_domain_review.json')
    args=parser.parse_args();main(args.source.resolve(),args.output)
