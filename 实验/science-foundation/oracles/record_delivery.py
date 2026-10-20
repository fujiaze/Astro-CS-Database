"""记录研究交付的文件身份与实际检查；不是CI或科学验收门禁。"""
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    base=Path(__file__).resolve().parents[1]
    root=base.parents[1]
    docs=list((root/'docs/science/foundation').glob('*.md'))
    manifest_path=base/'evidence/manifest.json'
    files=[p for p in base.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p!=manifest_path]
    files+=docs+[root/'docs/science/README.md',root/'docs/DOCUMENT_INDEX.yaml']
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    load=lambda name:json.loads((base/'evidence'/name).read_text(encoding='utf-8-sig'))
    cross=load('cross_geometry.json')
    initial=load('detector_independent.json')
    refined=load('detector_interval_refinements.json')
    final_cases={c['name']:c for c in initial['cases']}
    final_cases.update({c['name']:c['case'] for c in refined['refinements']})
    check_hash={key:sha(base/'oracles'/key)==value for key,value in cross['source_sha256'].items()}
    for name in ('coordinates','statistics'):
        evidence=load(name+'_independent.json')
        check_hash[name+'_independent.py']=sha(base/'oracles'/(name+'_independent.py'))==evidence['identity']['script_sha256']
    check_hash['detector_initial_loaded_snapshot']=sha(base/'evidence/source_snapshots/detector_interval_pre_spill_guard.py')==initial['source_sha256']
    for p in files:
        if p.suffix=='.py':
            ast.parse(p.read_text(encoding='utf-8-sig'))
        elif p.suffix=='.json':
            json.loads(p.read_text(encoding='utf-8-sig'),parse_constant=lambda s:(_ for _ in ()).throw(ValueError(s)))
    source_changes=subprocess.check_output(['git','diff','HEAD','--name-only','--','lib'],cwd=root,text=True).splitlines()
    info={'research_date_local':'2026-10-20','baseline_production_sha':'e7d273ed78b7d118cf8344110b97224af7378b2a',
          'claim':'Candidate scientific foundation and synthetic/analytic evidence; not formal independent acceptance or real HST/M42 validation.',
          'files':{p.relative_to(root).as_posix():{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(files)},
          'review':{'candidate_document_count':len(docs),'source_hash_checks':check_hash,
                    'python_syntax_and_json_finite_literals':True,'production_source_changes':source_changes,
                    'cross_geometry_cases':len(cross['cases']),
                    'cross_geometry_converged_and_joint_intervals':all(c['chart']['status']=='bounded' and c['detector']['status']=='tolerance_met' and c['all_leaf_intervals_intersect'] and c['total_intervals_intersect'] for c in cross['cases']),
                    'detector_cases_after_refinement':{n:c['enclosure']['status'] for n,c in sorted(final_cases.items())}},
          'withdrawn_area_enclosure_evidence':[
              'detector_independent_cone_invalid_budget26001.json','detector_independent_natural_interval_budget5001.json',
              'detector_independent_midpoint_budget14001.json','detector_independent_midpoint_pilot.json',
              'detector_independent_high_order_budget52001.json','detector_per_leaf_budget_audit.json'],
          'failure_identity_notes':{
              'external_cone':'Legacy cone-only area bounds were falsified by a concrete missing-leaf counterexample; retain as failure evidence only.',
              'parallel_loo':'Parallel rejection-all LOO failed coverage; default reference rejects at most one maximum residual.',
              'chart_mass':'Historical floating mass and initialization-budget failures were corrected and independently rechecked.',
              'export_budget':'Tighter initial area budget failed; later explicitly declared coarser bounds propagate into Export, not a claim to initial precision.',
              'hips_seam':'Initial HPX header crossed face6 longitude interruption; projection center was corrected, array indices unchanged.'}}
    manifest_path.write_text(json.dumps(info,indent=2,ensure_ascii=False,allow_nan=False)+'\n',encoding='utf-8')
    broken=[]
    for p in docs:
        for target in re.findall(r'\]\(([^)]+)\)',p.read_text(encoding='utf-8')):
            if '://' not in target and not target.startswith('#') and not (p.parent/target.split('#')[0]).exists():
                broken.append((p.name,target))
    if broken:
        raise ValueError(broken)
    print(json.dumps({'manifest':str(manifest_path),'files':len(files),'documents':len(docs),
                      'hash_checks':check_hash,'cross_all_converged':info['review']['cross_geometry_converged_and_joint_intervals'],
                      'detector_all_converged':all(v=='tolerance_met' for v in info['review']['detector_cases_after_refinement'].values()),
                      'broken_local_document_links':broken,'production_source_changes':source_changes},ensure_ascii=False))


if __name__=='__main__':
    main()
