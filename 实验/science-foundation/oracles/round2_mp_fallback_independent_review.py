"""Independent analytic/normalization review and small TAN fallback rerun.

No geometry truth comes from the fallback. The true sliver has independent
phi,z integral; normalization is also integrated by a different 1D primitive.
The expensive SIP solve is read with verified source hashes, not claimed rerun.
"""
from fractions import Fraction as Q
import hashlib
import json
from pathlib import Path
import random
from mpmath import mp, iv
import round2_mp_affine_fallback as candidate
from round2_science_budget import Interval
import round2_sliver as truth_model

HERE=Path(__file__).resolve().parent
EVIDENCE=HERE.parent/'evidence'
INPUT=EVIDENCE/'round2_mp_affine_fallback.json'
OUT=EVIDENCE/'round2_mp_fallback_independent_review.json'


def decode(d):return Interval(*(Q(v) for v in d['exact']))


def mpq(q):return mp.mpf(q.numerator)/q.denominator


def independent_native_area(h,dps):
    # Integrate detector u analytically first. Only a smooth one-dimensional
    # high-precision quadrature remains; no affine strips or candidate clipping.
    mp.dps=dps
    k=mp.mpf(h['CD1_1'])*mp.pi/180
    shift=1-mp.mpf(h['CRPIX2'])
    q=mp.mpf(h.get('A_0_2',0))
    def primitive(u,v):
        t=u+q*v*v; aa=1+k*k*v*v
        return k*k*t/(aa*mp.sqrt(aa+k*k*t*t))
    return mp.quad(lambda v:primitive(mp.mpf('.5'),v)-primitive(mp.mpf('-.5'),v),
                   [shift-mp.mpf('.5'),shift+mp.mpf('.5')])


def main():
    prior=json.loads(INPUT.read_text(encoding='utf-8'))
    for name,sha in prior['source_sha256'].items():
        assert hashlib.sha256((HERE/name).read_bytes()).hexdigest()==sha,name
    rng=random.Random(11120261021)
    scale=1<<candidate.AREA_LEDGER_BITS
    for _ in range(100):
        p=Q(rng.randrange(-10**18,10**18),rng.randrange(1,10**12))
        width=Q(rng.randrange(1,100),rng.randrange(1,10**12))
        original=Interval(p,p+width);enclosed=candidate.dyadic_enclosure(original)
        assert enclosed.lo<=original.lo<=original.hi<=enclosed.hi
        assert 0<=original.lo-enclosed.lo<Q(1,scale)
        assert 0<=enclosed.hi-original.hi<Q(1,scale)
    result={'command':'python 实验/science-foundation/oracles/round2_mp_fallback_independent_review.py',
            'source_sha256':{n:hashlib.sha256((HERE/n).read_bytes()).hexdigest() for n in
                ('round2_mp_fallback_independent_review.py','round2_mp_affine_fallback.py',
                 'round2_science_budget.py','round2_sliver.py')},
            'reviewed_evidence':{'path':INPUT.name,'sha256':hashlib.sha256(INPUT.read_bytes()).hexdigest(),
                                 'all_recorded_source_hashes_match_current':True},
            'dyadic_ledger_independent_signed_rational_cases':100,'seed':11120261021,
            'qualification':'Source/analytic review; TAN solve independently rerun. SIP expensive solve only hash-verified and independently checked, not rerun. MP quadrature convergence is a numerical check, not an interval certificate.',
            'cases':[]}
    for i,c in enumerate(prior['cases']):
        h=c['header'];truth=truth_model.analytic_new(h)
        true=Interval(Q(truth['area']['lower_sr']),Q(truth['area']['upper_sr']))
        area=decode(c['attempts'][-1]['leaf0_area_sr'])
        native=decode(c['native_drop_area_sr'])
        assert area.lo<=true.lo<=true.hi<=area.hi
        a70=independent_native_area(h,70)
        a110=independent_native_area(h,110)
        assert mpq(native.lo)<=a110<=mpq(native.hi)
        assert abs(a70-a110)<mp.mpf('1e-75')
        own_record={'header':h,'native_area_independent_mp_quad_sr':mp.nstr(a110,100),
                    'quad_70_to_110_abs_difference_sr':mp.nstr(abs(a70-a110),30),
                    'independent_normalization_within_reported_interval':True,
                    'reported_overlap_contains_independent_analytic_interval':True,
                    'reported_geometry_nodes':c['attempts'][-1]['geometry']['evaluated_nodes'],
                    'reported_geometry_status':c['attempts'][-1]['geometry']['status']}
        actual=[]
        true_area=(mp.mpf(truth['area']['lower_sr'])+mp.mpf(truth['area']['upper_sr']))/2
        for sign in (1,-1):
            flux=area/native*Q(sign*10**12)
            true_flux=sign*mp.mpf(10**12)*true_area/a110
            error=abs(mpq(flux.mid)-true_flux)
            assert error<=mpq(flux.radius)
            actual.append({'sign':sign,'true_flux_L':mp.nstr(true_flux,50),
                           'actual_midpoint_error_L':mp.nstr(error,40),
                           'predicted_error_bound_L':float(flux.radius),
                           'science_target_L':.01,'science_tolerance_met_exact_rational':flux.radius<=Q(1,100)})
        own_record['science']=actual
        if i==0:
            iv.dps=60
            model=candidate.Model(h);candidate.admission(model)
            requested=Q(1,100)*native.lo/(2*Q(10**12))
            rerun=candidate.solve(model,requested,max_nodes=20001)
            got=rerun.pop('area')
            assert got.lo<=true.lo<=true.hi<=got.hi
            own_record['independently_rerun_TAN']=dict(rerun,area=got.dump(),contains_analytic_truth=True)
        result['cases'].append(own_record)
    OUT.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
    print([(c['reported_geometry_nodes'],[(s['actual_midpoint_error_L'],s['predicted_error_bound_L']) for s in c['science']]) for c in result['cases']],flush=True)


if __name__=='__main__':main()
