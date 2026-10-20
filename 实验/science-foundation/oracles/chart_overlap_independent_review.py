"""对 chart_overlap 的独立外库抽查及严格界算术反例；不修改被审核文件。"""
import hashlib
import json
import math
from fractions import Fraction
from pathlib import Path

from astropy_healpix import HEALPix
import mpmath as mp
import numpy as np

import chart_overlap as target


def main():
    mp.mp.dps=70
    rng=np.random.default_rng(1117001)
    hp=HEALPix(nside=1,order='nested')
    tested=0
    worst=0.
    # 在完整根、随机矩形、双极点邻域和赤道/极区分支相交矩形上比较。
    for face in range(12):
        boxes=[(0.,1.,0.,1.),(1-2**-20,1.,1-2**-20,1.),(0.,2**-20,0.,2**-20),(.45,.55,.45,.55)]
        for _ in range(15):
            a,b=np.sort(rng.uniform(0,1,2));c,d=np.sort(rng.uniform(0,1,2))
            boxes.append((a,b,c,d))
        for a,b,c,d in boxes:
            bound=target.face_vector(face,target.iv.mpf([a,b]),target.iv.mpf([c,d]))
            lo=np.array([target.low(v) for v in bound]);hi=np.array([target.high(v) for v in bound])
            s=rng.uniform(a,b,80);t=rng.uniform(c,d,80)
            lon,lat=hp.healpix_to_lonlat(np.repeat(face,len(s)),dx=s,dy=t)
            vec=np.column_stack([np.cos(lat.rad)*np.cos(lon.rad),np.cos(lat.rad)*np.sin(lon.rad),np.sin(lat.rad)])
            outside=np.maximum(np.maximum(lo-vec,vec-hi),0)
            worst=max(worst,float(outside.max()));tested+=len(s)
    assert worst<1e-14 # 外库为 float64；此检查不宣称外库坐标精确到 interval dps
    # 模拟同一叶内粗块+100个深层已内含块，完全符合四叉分块dyadic质量类型。
    accepted=.25
    for _ in range(100):accepted+=4.**-28
    exact=Fraction(1,4)+100*Fraction(1,4**28)
    unit=target.iv.pi/3
    reported=target.high(unit*accepted)
    truth=mp.pi/3*mp.mpf(exact.numerator)/exact.denominator
    arithmetic={'dyadic_start':.25,'dyadic_added':4.**-28,'n_additions':100,'float_accumulator':accepted,
                'exact_fraction':str(exact),'reported_upper_sr':reported,'high_precision_exact_sr':str(truth),
                'exact_exceeds_claimed_upper_sr':float(truth-mp.mpf(reported)),
                'is_counterexample':bool(truth>reported),
                'scope':'对参考代码的普通float质量累计和末端单ulp扩展给算术见证；不是声称已构造某个实际WCS产生此相同队列。'}
    assert arithmetic['is_counterexample']
    # 直接执行待审求解器的累加过程；这里替换几何分类器为精确dyadic集合。
    # 该检验只审核面积账本，不将不连通测试集合声称为真实TAN像元。
    original_source=target.Source
    class ExactDyadicClassifier:
        contraction_upper=0
        def __init__(self,*args):pass
        def classify(self,face,depth,ix,iy):
            if face!=0:return False
            den=1<<depth
            if 2*(ix+1)<=den and 2*(iy+1)<=den:return True
            if depth==0:return None
            if depth>28:raise AssertionError('known exact set needs no deeper subdivision')
            step=1<<(28-depth)
            xlo,xhi=ix*step,(ix+1)*step
            ylo,yhi=iy*step,(iy+1)*step
            start=1<<27
            intersects=xlo<start+100 and xhi>start and ylo<1 and yhi>0
            if not intersects:return False
            return True if depth==28 else None
    try:
        target.Source=ExactDyadicClassifier
        ledger=target.solve_overlap({},nside=1,absolute_tolerance_sr=1e-25,max_nodes=10000)
        stalled=target.solve_overlap({},nside=1,absolute_tolerance_sr=1e-25,max_nodes=10000,max_depth=2)
    finally:
        target.Source=original_source
    upper=ledger['total_area']['upper_sr']
    ledger_check={'exact_sr':str(truth),'reported_upper_sr':upper,'exact_exceeds_upper_sr':float(truth-mp.mpf(upper)),
                  'is_counterexample':bool(truth>upper),'processed_nodes':ledger['processed_nodes'],
                  'scope':'实际solve_overlap质量账本，几何classifier被精确已知dyadic集合替换；只验证累加，不表示真实WCS。'}
    assert not ledger_check['is_counterexample']
    assert mp.mpf(ledger['total_area']['lower_sr'])<=truth<=mp.mpf(ledger['total_area']['upper_sr'])
    assert mp.mpf(stalled['total_area']['lower_sr'])<=truth<=mp.mpf(stalled['total_area']['upper_sr'])
    assert stalled['status']=='precision_exhausted'
    budget=target.solve_overlap(target.fixture(),nside=4,absolute_tolerance_sr=1e-3,max_nodes=1)
    assert budget['processed_nodes']==1
    assert len(budget['coarse_unresolved_candidates'])>0
    rejected={}
    for name,change in [('radian_units',{'CUNIT1':'rad'}),('unknown_reference_frame',{'RADESYS':'FK5'}),('additional_pv',{'PV1_1':.1})]:
        h=target.fixture();h.update(change)
        try:
            target.Source(h,(0.,0.),1.)
            rejected[name]=False
        except ValueError:
            rejected[name]=True
    assert all(rejected.values())
    result={'reviewed_sha256':hashlib.sha256(Path(target.__file__).read_bytes()).hexdigest(),
            'historical_findings':{'reviewed_sha256':'ce2a27d1c4d52017a883e7ba689ab6a8e5517f3211fe2263aa06211d7e5f6506','initialization_budget':{'max_nodes':1,'actual_processed_nodes':64},'status':'Original run observed before root agent correction; retained for provenance, not recomputed against corrected code.'},
            'face_box_astropy_healpix_check':{'seed':1117001,'points':tested,'max_coordinate_outside_interval':worst,'status':'INDEPENDENTLY_TESTED finite sample only'},
            'float_mass_counterexample':arithmetic,
            'live_mass_ledger_check':ledger_check,
            'initialization_budget_recheck':{'max_nodes':1,'actual_processed_nodes':budget['processed_nodes'],'status':budget['status'],'coarse_candidates_retained':len(budget['coarse_unresolved_candidates'])},
            'precision_budget_recheck':{'max_depth':2,'status':stalled['status'],'truth_within_bounds':True},
            'header_domain_rejections':rejected,
            'static_review':{'face_chart':'12 faces; r north/equatorial/south branches; nr=0 conservative wedge; sampled checks passed',
                            'homotopy':'If each Bernstein interval-control hull bbox misses the connected target projected bbox, straight-line homotopy stays disjoint; parity membership valid for certified Jordan source.',
                            'candidate_completeness':'Every face root traversed; same-direction dyadic subdivision conserves exact area in real arithmetic; shared chart edges are zero measure.',
                            'corrections_verified':'Exact Fraction mass accumulation passes controlled-dyadic truth; all work uses unified bounded queue; precision exhaustion preserves truth; CUNIT/frame/PV unsupported header rejection exercised.',
                            'remaining_scope':'max_depth<=48 ensures exact binary dyadic endpoints; interval library directed rounding assumed. Finite external point containment is not a global machine proof; real HST/M42 physical WCS adequacy is untested.'}}
    out=Path(__file__).resolve().parents[1]/'evidence/chart_overlap_independent_review.json'
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(json.dumps({'output':str(out),'face_points':tested,'outside':worst,'mass_counterexample':arithmetic['exact_exceeds_claimed_upper_sr'],'processed_nodes':budget['processed_nodes']},ensure_ascii=False))


if __name__=='__main__':main()
