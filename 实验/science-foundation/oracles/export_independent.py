"""新Export科学reference的独立闭环；不是旧生产Export端到端执行。

被测面积接口为chart_overlap；真值另用Astropy前向、外库HEALPix成员、
独立TAN面积Jacobian的iid积分。输入为真实叶内分段常数，可正/负/零。
"""
import json
import hashlib
import math
from pathlib import Path
import numpy as np
from mpmath import iv
import astropy.units as u
from astropy.io.fits import Header
from astropy.wcs import WCS
from astropy_healpix import HEALPix
from chart_overlap import solve_overlap


def quotient_bounds(nlo,nhi,dlo,dhi):
    if dlo<=0:return None
    vals=[nlo/dlo,nlo/dhi,nhi/dlo,nhi/dhi]
    return [math.nextafter(min(vals),-math.inf),math.nextafter(max(vals),math.inf)]


def export_reference(areas,signal,supported):
    """面积接口消费者。均值单位L/sr，积分单位L，面积单位sr。

    几何区间按信号符号传播；未知/未覆盖不是零值。几何取中点的R
    仅用于固定算子条件下的线性误差传播，不吞掉几何区间误差。
    """
    numerator=iv.mpf(0);denominator=iv.mpf(0);coeff={}
    all_area=math.fsum(a['mid_sr'] for a in areas.values())
    for key,a in areas.items():
        leaf=int(key)
        if leaf not in supported:continue
        value=signal[leaf];al,ah=a['lower_sr'],a['upper_sr']
        ai=iv.mpf([al,ah]);numerator+=value*ai;denominator+=ai;coeff[leaf]=a['mid_sr']
    lo=math.nextafter(float(numerator.a),-math.inf) if coeff else 0.
    hi=math.nextafter(float(numerator.b),math.inf) if coeff else 0.
    dlo=max(0.,math.nextafter(float(denominator.a),-math.inf)) if coeff else 0.
    dhi=math.nextafter(float(denominator.b),math.inf) if coeff else 0.
    midarea=math.fsum(coeff.values())
    integral=math.fsum(coeff[k]*signal[k] for k in coeff)
    return {'supported_area_sr':[dlo,dhi],'support_integral_L':[lo,hi],
            'support_mean_L_per_sr':quotient_bounds(lo,hi,dlo,dhi),
            'midpoint_integral_L':integral,'midpoint_supported_mean_L_per_sr':integral/midarea if midarea else None,
            'supported_area_fraction_midpoint':midarea/all_area if all_area else None,
            'wrong_full_pixel_denominator_result':integral/all_area if all_area else None,
            'coefficients_midpoint_sr':coeff}


def main():
    base=Path(__file__).resolve().parents[1]
    header={'NAXIS':2,'NAXIS1':2,'NAXIS2':2,'CTYPE1':'RA---TAN','CTYPE2':'DEC--TAN',
            'CUNIT1':'deg','CUNIT2':'deg','RADESYS':'ICRS','CRVAL1':359.8,'CRVAL2':5.2,
            'CRPIX1':1.37,'CRPIX2':1.12,'CD1_1':-5.4,'CD1_2':1.56,'CD2_1':.78,'CD2_2':6.6}
    centers=[(0,0),(1,0),(0,1),(1,1)];nside=8
    w=WCS(Header(header));hp=HEALPix(nside=nside,order='nested')
    # 各像元结果立即存档，重跑统计验证复用同一面积表。
    cache=base/'evidence/export_overlap_cache.json'
    config={'header':header,'centers':centers,'nside':nside,'absolute_tolerance_sr':3e-4,'max_nodes':20000}
    stored=json.loads(cache.read_text(encoding='utf8')) if cache.exists() else {}
    overlaps=stored.get('overlaps',[]) if stored.get('config')==json.loads(json.dumps(config)) else []
    chart_source=Path(__file__).with_name('chart_overlap.py')
    chart_hash=hashlib.sha256(chart_source.read_bytes()).hexdigest()
    provenance=stored.get('provenance')
    if provenance is None:
        review=json.loads((base/'evidence/chart_overlap_independent_review.json').read_text(encoding='utf8'))
        provenance={'chart_overlap_source_sha256':chart_hash,
                    'identity_recorded_retrospectively':bool(overlaps),
                    'prior_independent_review_same_source_hash':review['reviewed_sha256']==chart_hash,
                    'note':'The original cache had no source hash. The producing baseline is the current chart_overlap source: its hash matches the earlier independent review, and its modification time predates this cache. This hash annotation was added retrospectively; numerical overlap results are unchanged.'}
    for c in centers[len(overlaps):]:
        overlaps.append(solve_overlap(header,c,1.,nside,absolute_tolerance_sr=config['absolute_tolerance_sr'],max_nodes=config['max_nodes']))
        cache.write_text(json.dumps({'config':config,'overlaps':overlaps},indent=2)+'\n',encoding='utf8')
    cache.write_text(json.dumps({'config':config,'provenance':provenance,'overlaps':overlaps},indent=2)+'\n',encoding='utf8')
    assert all(o['status']=='bounded' for o in overlaps)
    leaves=sorted({int(k) for o in overlaps for k in o['areas']})
    signal={k:float([-2,0,3][i%3]) for i,k in enumerate(leaves)}
    supports={'fully_covered':set(leaves),'partial_support':{k for i,k in enumerate(leaves) if i%3!=1}}
    rng=np.random.default_rng(1117003);samples=1000000
    cd=np.deg2rad(np.array([[header['CD1_1'],header['CD1_2']],[header['CD2_1'],header['CD2_2']]]))
    det_density=abs(np.linalg.det(cd))
    # 独立包含算术算出binary64头CD的|det|上界，不把RN结果直接当严格上界。
    dd=abs(iv.mpf(header['CD1_1'])*iv.mpf(header['CD2_2'])-iv.mpf(header['CD1_2'])*iv.mpf(header['CD2_1']))*(iv.pi/180)**2
    g_bound=math.nextafter(float(dd.b),math.inf) # 无SIP：TAN分母>=1
    # 联合控制全部四输出、两支持模式、分子/分母：Bonferroni+Hoeffding。
    delta=1e-6;family=4*2*2;tail=math.sqrt(math.log(2*family/delta)/(2*samples))
    cases={name:[] for name in supports}
    for c,overlap in zip(centers,overlaps):
        p=rng.uniform(np.array(c)-.5,np.array(c)+.5,(samples,2))
        uv=p+1-np.array([header['CRPIX1'],header['CRPIX2']]);plane=uv@cd.T
        g=det_density/(1+np.sum(plane*plane,axis=1))**1.5
        sky=w.all_pix2world(p,0);leaf=hp.lonlat_to_healpix(sky[:,0]*u.deg,sky[:,1]*u.deg)
        assert set(map(int,np.unique(leaf)))<=set(leaves)
        val=np.array([signal[int(k)] for k in leaf])
        for name,supported in supports.items():
            mask=np.isin(leaf,list(supported));num=float(np.mean(g*val*mask));den=float(np.mean(g*mask))
            derror=g_bound*tail;nerror=5*g_bound*tail # Y in [-2g_bound,+3g_bound]
            dci=[max(0.,den-derror),den+derror];nci=[num-nerror,num+nerror]
            mean_ci=quotient_bounds(*nci,*dci)
            reference=export_reference(overlap['areas'],signal,supported)
            # 参考几何界必须与独立统计真值界相交；不把统计一致称数学证明。
            def intersects(a,b):return a[0]<=b[1] and b[0]<=a[1]
            tests={'area_intersects':intersects(reference['supported_area_sr'],dci),
                   'integral_intersects':intersects(reference['support_integral_L'],nci),
                   'mean_intersects':intersects(reference['support_mean_L_per_sr'],mean_ci)}
            assert all(tests.values())
            cases[name].append({'pixel_xy0':list(c),'new_reference':reference,
                               'independent_iid':{'supported_area_sr':den,'supported_area_confidence_interval_sr':dci,
                               'support_integral_L':num,'support_integral_confidence_interval_L':nci,
                               'supported_mean_L_per_sr':num/den if den else None,'mean_confidence_interval_L_per_sr':mean_ci},
                               'checks':tests})
    # 重复同leaf的几何贡献必须先合并，否则误将同一个随机变量当独立样本。
    R=np.zeros((4,len(leaves)));raw=[]
    for j,case in enumerate(cases['fully_covered']):
        a=case['new_reference']['coefficients_midpoint_sr'];den=math.fsum(a.values())
        pieces=[]
        for k,v in a.items():
            for fraction in (.35,.65):pieces.append((int(k),fraction*v/den))
        merged={}
        for k,value in pieces:merged[k]=merged.get(k,0.)+value
        for k,value in merged.items():R[j,leaves.index(k)]=value
        raw.append(pieces)
    C=np.diag(np.linspace(.2,.8,len(leaves)))+.06*np.ones((len(leaves),len(leaves)))
    predicted=R@C@R.T
    solid_angles=np.array([math.fsum(case['new_reference']['coefficients_midpoint_sr'].values()) for case in cases['fully_covered']])
    R_integral=R*solid_angles[:,None]
    # 第二种逐项定义式评估，不调用矩阵乘法作参考。
    explicit=np.array([[math.fsum(vi*wj*C[leaves.index(ki),leaves.index(kj)] for ki,vi in raw[i] for kj,wj in raw[j]) for j in range(4)] for i in range(4)])
    assert np.max(np.abs(explicit-predicted))<1e-14
    draws=200000;z=rng.multivariate_normal(np.zeros(len(leaves)),C,size=draws)
    outputs=z@R.T;empirical=np.cov(outputs,rowvar=False)
    se=np.sqrt((predicted**2+np.outer(np.diag(predicted),np.diag(predicted)))/(draws-1))
    max_z=float(np.max(abs(empirical-predicted)/se));assert max_z<6
    correct_white=np.sum(R**2,axis=1)
    wrong_white=np.array([math.fsum(v*v for k,v in parts) for parts in raw])
    covariance={'input_leaf_order':leaves,'R_support_mean':R.tolist(),'C_input':C.tolist(),'R_C_Rt':predicted.tolist(),
                'R_supported_integral_sr':R_integral.tolist(),'integral_covariance_L2':(R_integral@C@R_integral.T).tolist(),
                'units':{'input_C':'L^2/sr^2','R_support_mean':'dimensionless','mean_output_C':'L^2/sr^2','R_supported_integral':'sr','integral_output_C':'L^2'},
                'explicit_double_sum_max_error':float(np.max(abs(explicit-predicted))),
                'gaussian_simulations':draws,'empirical_covariance':empirical.tolist(),'maximum_standardized_covariance_error':max_z,
                'duplicate_leaf_counterexample':{'correct_white_variance':correct_white.tolist(),'wrong_independent_piece_variance':wrong_white.tolist(),'wrong_to_correct_ratio':(wrong_white/correct_white).tolist()},
                'conditioning':'R is fixed at the same overlap-area midpoints. Geometric interval uncertainty is retained separately and is not included in the stochastic covariance.'}
    empty=export_reference(overlaps[0]['areas'],signal,set())
    assert empty['support_mean_L_per_sr'] is None and empty['supported_area_sr']==[0,0]
    result={'identity':{'seed':1117003,'new_reference_only':True,'not_legacy_production_export_e2e':True,'target_header':header,'nside':nside,'samples_per_output':samples,'family_failure_probability':delta,'hoeffding_family_size':family,'density_global_bound_sr_per_pixel2':g_bound,
                        'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                        'overlap_cache_sha256':hashlib.sha256(cache.read_bytes()).hexdigest(),
                        'overlap_cache_provenance':provenance},
            'definition':{'mean':'integral_supported I dOmega / integral_supported dOmega [L/sr]','supported_integral':'integral_supported I dOmega [L]','missing':'No full-pixel-area fill; zero I on a supported leaf is valid data; empty support mean is undefined/null.'},
            'input_piecewise_constant_signal':signal,'signal_values_present':sorted(set(signal.values())),
            'support_leaf_sets':{name:sorted(s) for name,s in supports.items()},
            'overlap_interfaces':overlaps,'cases':cases,'covariance':covariance,'empty_support_boundary_case':empty,
            'claim_limits':{'INDEPENDENTLY_TESTED':'New reference Export consumer, finite TAN fixture, containing area propagation, conditional iid Hoeffding bounds, and fixed-R Gaussian covariance simulation.',
                            'ASSUMPTION':'True input field constant inside each real HEALPix leaf; compatible response/unit; C is a chosen valid input covariance, no unknown subleaf sky structure.',
                            'REQUIRES_REAL_DATA':'Actual PSF and transfer response, small-scale sky variation and astrometric uncertainty, observed covariance calibration, and legacy or future production Export execution.'}}
    out=base/'evidence/export_independent.json';out.write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(json.dumps({'output':str(out),'leaves':len(leaves),'checks_pass':True,'covariance_max_z':max_z},ensure_ascii=False))


if __name__=='__main__':main()
