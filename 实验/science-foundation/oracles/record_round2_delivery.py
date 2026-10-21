"""Record byte identities and actual numerical evidence for #111 round two.

This is an artifact inventory, not CI, a product gate or scientific acceptance.
Historical failures are retained under their original identities.
"""
import ast
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess


def main():
    base=Path(__file__).resolve().parents[1]
    root=base.parents[1]
    destination=base/'evidence/round2_manifest.json'
    docs=sorted((root/'docs/science/foundation').glob('*.md'))
    files=[p for p in base.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p!=destination]
    files+=docs+[root/'docs/DOCUMENT_INDEX.yaml',root/'docs/science/README.md']
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    def load(name):return json.loads((base/'evidence'/name).read_text(encoding='utf-8-sig'))
    # A source may intentionally have several historical byte snapshots.
    identities={}
    for p in files:identities.setdefault(sha(p),[]).append(p.relative_to(root).as_posix())
    final_names=['round2_performance_final_kernel.json','round2_science_budget.json',
        'round2_wcs_contract.json','round2_sliver.json','round2_original_sliver_refined.json',
        'round2_affine_independent_review_final.json','round2_affine_sip_review_final.json',
        'round2_high_order_focus_review.json','round2_cd_domain_review.json',
        'round2_numeric_admission_review.json','round2_focused_budget.json',
        'round2_cpp_external_membership_final.json','round2_cpp_science_bridge_final.json',
        'round2_mp_affine_fallback.json','round2_mp_fallback_independent_review.json']
    missing=[];source_checks={}
    def scan(value,location):
        if isinstance(value,dict):
            for k,v in value.items():
                # Executables live only in temporary build dirs; their SHA is
                # an execution identity, not a promise to ship a binary.
                if k in ('executable_sha256',):continue
                if isinstance(v,str) and re.fullmatch('[0-9a-fA-F]{64}',v):
                    if ('sha256' in k or location.endswith('source_sha256') or
                        location.endswith('input_sha256')):
                        key=location+'.'+k
                        source_checks[key]=identities.get(v.lower(),[])
                        if not source_checks[key]:missing.append(key)
                else:scan(v,location+'.'+k)
        elif isinstance(value,list):
            for i,v in enumerate(value):scan(v,location+f'[{i}]')
    for name in final_names:scan(load(name),name)
    for p in files:
        if p.suffix=='.py':ast.parse(p.read_text(encoding='utf-8-sig'))
        elif p.suffix=='.json':json.loads(p.read_text(encoding='utf-8-sig'),parse_constant=lambda s:(_ for _ in ()).throw(ValueError(s)))
    broken=[]
    for p in docs:
        for target in re.findall(r'\]\(([^)]+)\)',p.read_text(encoding='utf-8')):
            if '://' not in target and not target.startswith('#') and not (p.parent/target.split('#')[0]).exists():broken.append([p.name,target])
    baseline='9f0c8bcebd41ccfd74bcda32831ac8eb7ce0d9d0'
    production=subprocess.check_output(['git','diff',baseline,'--name-only','--','lib'],cwd=root,text=True).splitlines()
    perf=load('round2_performance_final_kernel.json')
    cpp=base/'oracles/round2_affine.cpp'
    current_cpp_hash=sha(cpp)
    assert perf['build']['source_sha256']==current_cpp_hash
    assert all(0<=int(ip)<12*c['request']['nside']**2 for c in perf['cases']
               for ip in c['measurement']['first']['areas'])
    mp=load('round2_mp_affine_fallback.json')
    assert all(s['status']=='science_bounded' for c in mp['cases'] for s in c['attempts'][-1]['science'])
    old_cross=load('cross_geometry.json')
    old_hashes={n:sha(base/'oracles'/n)==h for n,h in old_cross['source_sha256'].items()}
    assert all(old_hashes.values()),old_hashes
    if missing or broken or production:raise ValueError(dict(missing_identity=missing,broken_links=broken,production_changes=production))
    result={'research_date_local':'2026-10-21','baseline_commit':baseline,
        'status':'Candidate scientific specification, awaiting external independent acceptance',
        'environment':{'python':platform.python_version(),'platform':platform.platform()},
        'files':{p.relative_to(root).as_posix():{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(files)},
        'final_evidence':final_names,'identity_resolution':source_checks,
        'checks':{'five_candidate_specs':len(docs)==5,'production_source_changes':production,
                  'python_syntax_json_finite_and_local_doc_links':True,'final_cpp_sha256':current_cpp_hash,
                  'old_A_B_cross_sources_unchanged':old_hashes,'final_candidate_ids_in_range':True,
                  'high_precision_signed_flux_science_targets_met':True},
        'historical_qualifications':{
          'round2_performance_initial.json':'Pre radius/spill repair: withdraw enclosure qualification; source snapshot retained.',
          'round2_performance_final.json':'Intermediate kernel before impossible chart-cell prune; invalid virtual candidate label in wide-field analytic examples; not final interface evidence.',
          'round2_performance_certified_cells_pre_admission.json':'Valid tested-domain kernel before explicit CD/numeric admission and work guard; exact source snapshot retained.',
          'round2_performance_certified_cells.json':'Byte-identical convenience input alias of final_kernel for independent membership/science consumers; prior content retained under pre_admission suffix.',
          'round2_cd_domain_underbound.json':'Falsified determinant absolute-value bound on a sign-unresolved near-singular CD; failure evidence only.',
          'nearest_display_archives':'Exact rational fields retain their meaning; old nearest-rounded display fields must not be consumed as containing intervals.',
          'round2_mp_affine_rational_ledger_partial.json':'Interrupted SIP attempt after denominator growth; completed rows only, not a complete successful run.',
          'round2_affine_analytic_refined.json':'Quarter-face met global proxy after added budget; complete-face remained exhausted at 128001 nodes. Does not claim science convergence.',
          'round2_high_order_focus_header_serialization_failure.json':'Different exact CD after FITS card serialization; not a same-model solver counterexample.',
          'first_round_manifest':'Original delivery identity only; original document hashes are historical, not the updated candidate documents.'},
        'bound_scope':'Conditional on specified interval primitives and compiler/library semantics; exact math and finite independent runs are not formal code verification or HST/M42 physical validation.'}
    destination.write_text(json.dumps(result,indent=2,ensure_ascii=False,allow_nan=False)+'\n',encoding='utf-8')
    print(json.dumps({'files':len(files),'final_evidence':len(final_names),'identity_fields':len(source_checks),
                      'all_checks_recorded':True,'cpp_sha256':current_cpp_hash},ensure_ascii=False))


if __name__=='__main__':main()
