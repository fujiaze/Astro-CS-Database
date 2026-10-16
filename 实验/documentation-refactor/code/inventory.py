"""Inventory baseline technical texts from Git, retaining blob identities."""
import csv
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
UNIT = ROOT / '实验/documentation-refactor'

REWRITTEN = {
    'AGENTS.md', 'README.md', 'docs/README.md', 'docs/ACSD_DESIGN.md',
    'docs/science/README.md', 'docs/engineering/README.md',
    'docs/science/normalize/NORMALIZE_SCIENCE.md',
    'docs/science/integration/INTEGRATION.md',
    'docs/science/integration/REJECTION.md',
    'docs/science/photometry/references/PHOTOMETRY_RESEARCH_PACK.md',
    'docs/engineering/api/PUBLIC_API.md',
    'docs/engineering/contracts/RUNTIME.md',
    'docs/engineering/contracts/OWNERSHIP_LIFETIME.md',
    'docs/detail/infrastructure/runtime.md',
    'docs/engineering/governance/DOCUMENT_GOVERNANCE.md',
    'docs/detail/registry/README.md', 'docs/detail/normalize/README.md',
    'docs/detail/mosaic/README.md', 'docs/detail/export/README.md',
    'docs/engineering/governance/README.md',
    'docs/engineering/contracts/LOG_AND_ERROR.md',
    'docs/engineering/standards/ERROR_MODEL.md',
    'docs/detail/infrastructure/log_and_error_system.md',
    'docs/detail/infrastructure/observability.md',
    'docs/engineering/resources/PERFORMANCE_MODEL.md',
    'docs/engineering/resources/observability/RESOURCE_MONITORING.md',
}
REVIEWED = {
    'docs/science/unified/UNIFIED_SCIENCE_MODEL.md',
    'docs/science/unified/DATA_SEMANTICS.md', 'docs/science/unified/SCIENCE_SCOPE.md',
    'docs/science/unified/README.md', 'docs/science/algorithms/README.md',
    'docs/science/calibration/CALIBRATION.md', 'docs/science/calibration/CCD_DEFECT.md',
    'docs/science/detection/ASTROMETRY.md', 'docs/science/detection/STAR_DETECTION.md',
    'docs/science/psf/PSF.md', 'docs/science/photometry/PHOTOMETRY.md',
    'docs/science/drizzle/DRIZZLE.md', 'docs/science/noise_snr/NOISE_SNR.md',
    'docs/science/integration/REJECTION.md', 'docs/science/integration/INTEGRATION.md',
    'docs/science/sky/UPM.md', 'docs/science/projection/HIPS_TO_FITS.md',
    'docs/science/resample/RESAMPLE.md', 'docs/science/formats/HIPS_TILE_FORMAT.md',
    'docs/GLOSSARY.md', 'docs/detail/00_INDEX.md', 'docs/detail/README.md',
    'docs/detail/normalize/pipeline.md', 'docs/detail/mosaic/pipeline.md',
    'docs/detail/export/pipeline.md', 'docs/engineering/architecture/ARCHITECTURE.md',
    'docs/engineering/architecture/NORMALIZE_PIPELINE.md',
    'docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md',
    'docs/engineering/governance/NORMALIZE_CONVERGENCE.md',
    'docs/engineering/governance/UNRESOLVED.md',
    'docs/science/photometry/references/PHOTOMETRY_RESEARCH_PACK.md',
    'docs/science/photometry/references/README.md',
    'docs/science/algorithms/normalize/CALIBRATION_ALGORITHMS.md',
    'docs/engineering/architecture/DEV_MODE.md',
    'docs/engineering/standards/DIAGNOSTIC_PROBES.md',
    'docs/engineering/resources/observability/STRUCTURED_LOGGING.md',
    'docs/engineering/resources/observability/RUN_GRAPH.md',
}
EXTRACTED = {
    'docs/science/calibration/CCD_DEFECT.md': 'CCD_DEFECT_OBSERVATIONS.md',
    'docs/science/detection/ASTROMETRY.md': 'ASTROMETRY_BUDGET_RECORDS.md',
    'docs/science/detection/STAR_DETECTION.md': 'DETECTION_OBSERVATIONS.md',
    'docs/science/psf/PSF.md': 'PSF_CALCULATION_EXAMPLE.md',
}


def main():
    revision = sys.argv[1] if len(sys.argv) > 1 else '63eccbb61'
    commit = subprocess.check_output(['git', 'rev-parse', revision], cwd=ROOT, text=True).strip()
    raw = subprocess.check_output(['git', '-c', 'core.quotepath=false', 'ls-tree', '-rlz', commit], cwd=ROOT)
    mapping = json.loads((UNIT / 'docs/PATH_MIGRATIONS.json').read_text(encoding='utf-8'))
    rows = []
    for record in raw.decode('utf-8').split('\0'):
        if not record:
            continue
        meta, name = record.split('\t', 1)
        mode, kind, blob, size = meta.split()
        if kind != 'blob':
            continue
        suffix = Path(name).suffix.lower()
        if suffix not in {'.md', '.rst', '.txt', '.yaml', '.yml', '.bib', '.log'} and not (suffix == '.json' and ('schema' in name or '/schemas/' in name)):
            continue
        if name in mapping:
            dest, anchor = mapping[name]
            action = '并入同主题正文；原文件退役'
            reading = '已读模块/规范；接口按已查生产段抽查，未做全函数运行核验'
            if anchor:
                dest += '#' + anchor
        elif name in REWRITTEN:
            dest, action, reading = name, '重写', '已读；按本轮授权整理，科学重推导未执行'
        elif name in EXTRACTED:
            dest = name + ' ; 实验/documentation-refactor/docs/' + EXTRACTED[name]
            action, reading = '保留科学正文；观察/复算附录归研究记录', '已读；未重跑数值'
        elif name in REVIEWED:
            dest, action, reading = name, '保留专题内容；必要导航同步', '已读正文；未完成逐引文/逐函数外部核验'
        elif name.startswith(('实验/', 'artifacts/')):
            dest, action, reading = name, '保留为研究证据，原文不动', '已枚举；尚未逐篇精读'
        elif name == 'docs/DOCUMENT_INDEX.yaml':
            dest, action, reading = name, '更新轻量导航', '结构解析与路径核对；不评价科学'
        else:
            dest, action, reading = name, '保留当前主题归属；逐篇重写尚未完成', '已枚举；局部查阅或尚未精读'
        rows.append(dict(path=name, baseline_commit=commit, git_blob=blob, bytes=int(size),
                         disposition=action, destination=dest, review_scope=reading))
    target = UNIT / 'docs/ORIGINAL_FILES.tsv'
    with target.open('w', encoding='utf-8', newline='') as out:
        writer = csv.DictWriter(out, fieldnames=list(rows[0]), delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    summary = dict(baseline_commit=commit, technical_texts=len(rows),
                   original_docs=sum(r['path'].startswith('docs/') for r in rows),
                   original_experiment_texts=sum(r['path'].startswith('实验/') for r in rows),
                   original_files_rewritten=sum(r['path'] in REWRITTEN for r in rows),
                   consolidated_sources=len(mapping),
                   extracted_appendices=len(EXTRACTED),
                   science_questions=len(__import__('re').findall(r'^### Q\d+\b', (UNIT / 'docs/SCIENTIFIC_QUESTIONS.md').read_text(encoding='utf-8'), __import__('re').M)),
                   full_repository_substantive_review_complete=False)
    (UNIT / 'docs/INVENTORY_SUMMARY.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=True, indent=2))


if __name__ == '__main__':
    main()
