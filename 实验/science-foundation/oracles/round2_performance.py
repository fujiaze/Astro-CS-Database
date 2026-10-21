"""Compile and measure the standalone affine-strip prototype; retain all failures.

Run: python 实验/science-foundation/oracles/round2_performance.py --output ...json
No production source, executable or FITS data is changed.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import time

BASE = Path(__file__).resolve().parents[1]
SOURCE = Path(__file__).with_name('round2_affine.cpp')


def fixture(ra=37.73, dec=18.43, scale_arcsec=1.7, strength=0):
    s = scale_arcsec / 3600
    h = dict(CTYPE1='RA---TAN', CTYPE2='DEC--TAN', CUNIT1='deg', CUNIT2='deg',
             RADESYS='ICRS', LONPOLE=180., CRVAL1=ra, CRVAL2=dec,
             CRPIX1=1.173, CRPIX2=.819, CD1_1=-s, CD1_2=.137*s,
             CD2_1=.083*s, CD2_2=1.113*s)
    if strength:
        h.update(CTYPE1='RA---TAN-SIP', CTYPE2='DEC--TAN-SIP', A_ORDER=3, B_ORDER=3,
                 A_2_0=strength, A_1_1=-.7*strength, A_0_3=.05*strength,
                 B_0_2=-.8*strength, B_2_1=.07*strength)
    return h


def encode(case):
    h = case['header']
    terms = [(axis, int(k[2]), int(k[4]), v) for axis, p in enumerate(('A_', 'B_'))
             for k, v in h.items() if k.startswith(p) and len(k) == 5 and k[2].isdigit()]
    values = [case['name'], case['nside'], case['pixfrac'], case['tolerance_sr'],
              case['max_nodes'], case['side'], case['cache'], case['threads'],
              *[h[k] for k in ('CRVAL1','CRVAL2','CRPIX1','CRPIX2','CD1_1','CD1_2','CD2_1','CD2_2')], len(terms)]
    return ' '.join(map(str, values)) + '\n' + '\n'.join(' '.join(map(str,t)) for t in terms) + '\n'


def cases():
    out = []
    def add(name, h, N, frac=1., rel=1e-4, side=1, cache=0, threads=1, max_nodes=1601):
        area = abs(h['CD1_1']*h['CD2_2']-h['CD1_2']*h['CD2_1'])*(math.pi/180)**2*frac**2
        out.append(dict(name=name, header=h, nside=N, pixfrac=frac,
                        tolerance_sr=area*rel, relative_width_proxy=rel,
                        max_nodes=max_nodes, side=side, cache=cache, threads=threads))
    for label, h, n, frac in [
        ('fine_subleaf', fixture(scale_arcsec=.19), 262144, .63),
        ('normal_tan', fixture(), 262144, 1.),
        ('weak_sip', fixture(strength=2e-5), 262144, .81),
        ('strong_sip', fixture(strength=.018), 262144, 1.),
        ('coarse_many_leaf', fixture(scale_arcsec=18.3), 65536, 1.),
        ('low_order', fixture(143.18,-23.71,413.,.001), 512, .57),
        ('ra_wrap', fixture(359.99987,12.71,2.1,1e-4), 131072, 1.),
        ('face_seam', fixture(45.00009,.00007,2.7,1e-4), 65536, 1.),
        ('transition', fixture(77.16,math.degrees(math.asin(2/3)),3.1,1e-4), 65536, .73),
    ]:
        add(label, h,n,frac,side=4)
    for label, dec in [('north_cross',89.99983),('south_cross',-89.99979)]:
        add(label,fixture(141.17,dec,2.3,3e-4),262144,.88,rel=1e-3,max_nodes=1001)
    for rel in (1e-2,1e-4,1e-6,1e-9):
        add(f'tolerance_{rel:g}',fixture(67.771,-31.22,1.9,.003),262144,.83,rel=rel,max_nodes=3201)
    for cache, threads in ((0,1),(1,1),(0,2),(1,2)):
        add(f'batch_cache{cache}_threads{threads}',fixture(112.31,23.19,.21,1e-6),65536,.72,
            rel=1e-3,side=16,cache=cache,threads=threads)
    # Analytic TAN rectangle: centered, axis aligned, no SIP.
    h=fixture(21.317,7.413,2*3600)
    h.update(CRPIX1=1.,CRPIX2=1.,CD1_2=0.,CD2_1=0.,CD1_1=2.,CD2_2=2.)
    add('analytic_rectangle',h,16,rel=1e-4,max_nodes=8001)
    h=fixture(0.,0.,1.)
    h.update(CRPIX1=.5,CRPIX2=.5,CD1_1=math.degrees(1.1),CD2_2=math.degrees(1.1),CD1_2=0.,CD2_1=0.)
    add('analytic_face_quarter',h,1,rel=2e-4,max_nodes=8001)
    h=h.copy();h.update(CRPIX1=1.,CRPIX2=1.,CD1_1=math.degrees(2.2),CD2_2=math.degrees(2.2))
    add('analytic_complete_face',h,1,rel=2e-4,max_nodes=8001)
    return out


def build(directory):
    compiler='C:/msys64/mingw64/bin/g++.exe'
    env=dict(os.environ);env['PATH']=str(Path(compiler).parent)+os.pathsep+env.get('PATH','')
    exe=Path(directory)/'round2_affine.exe'
    cmd=[compiler,'-std=c++17','-O2','-fno-fast-math','-ffp-contract=off','-fopenmp',str(SOURCE),
         '-lmpfr','-lgmpxx','-lgmp','-lpsapi','-o',str(exe)]
    subprocess.run(cmd,check=True,env=env,capture_output=True,text=True)
    return exe,env,dict(command=cmd,compiler=subprocess.check_output([compiler,'--version'],env=env,text=True).splitlines()[0],
                        source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())


def run(output):
    data=dict(environment=dict(python=platform.python_version(),platform=platform.platform(),
                               cpu=platform.processor(),logical_cpus=os.cpu_count()),cases=[],
              interpretation='Measured scalar MPFR/GMP+OpenMP standalone kernel. Relative width sweep is a cost study, not a claim of science tolerance. All intervals conditional on compiler IEEE basic arithmetic and MPFR/GMP correctness.')
    with tempfile.TemporaryDirectory(prefix='acsd-round2-') as temp:
        exe,env,identity=build(temp);data['build']=identity
        for case in cases():
            start=time.perf_counter()
            completed=subprocess.run([str(exe)],input=encode(case),text=True,capture_output=True,env=env,timeout=180)
            record=dict(request=case,wall_process_seconds=time.perf_counter()-start,returncode=completed.returncode)
            if completed.returncode or not completed.stdout.strip():
                record.update(stderr=completed.stderr,stdout=completed.stdout,status='execution_failed')
            else:
                record['measurement']=json.loads(completed.stdout)
            if case['name']=='analytic_rectangle' and 'measurement' in record:
                a=math.pi/180;truth=4*math.atan(a*a/math.sqrt(1+2*a*a))
                bounds=record['measurement']['first']['areas'].values()
                low=sum(x[0] for x in bounds);high=sum(x[1] for x in bounds)+record['measurement']['first']['spill_sr']
                record['analytic']={'truth_sr':truth,'sum_bounds_sr':[low,high],'contains_truth':low<=truth<=high}
            if case['name'] in ('analytic_face_quarter','analytic_complete_face') and 'measurement' in record:
                truth=math.pi/(12 if case['name'].endswith('quarter') else 3)
                lo,hi=record['measurement']['first']['areas'].get('4',[0,0])
                hi+=record['measurement']['first']['spill_sr']
                record['analytic']={'truth_leaf':4,'truth_sr':truth,'bounds_sr':[lo,hi],'contains_truth':lo<=truth<=hi}
            data['cases'].append(record)
            output.write_text(json.dumps(data,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
            print(case['name'],record.get('measurement',record.get('status')),flush=True)
    return data


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args();run(args.output)
