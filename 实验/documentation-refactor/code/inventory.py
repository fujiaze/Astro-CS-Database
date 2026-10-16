"""Inventory baseline technical texts from Git, retaining blob identities."""
import csv
import json
import re
from pathlib import Path
import subprocess
import sys
from rewrite_directory_guides import GUIDES
from rewrite_standards import DOCUMENTS as STANDARD_DOCUMENTS
from rewrite_cpu import DOCUMENTS as CPU_DOCUMENTS
from rewrite_gui import DOCUMENTS as GUI_DOCUMENTS
from rewrite_testing import DOCUMENTS as TESTING_DOCUMENTS
from rewrite_storage import DOCUMENTS as STORAGE_DOCUMENTS
from rewrite_support import DOCUMENTS as SUPPORT_DOCUMENTS
from rewrite_mosaic_reducers import DOCUMENTS as MOSAIC_REDUCER_DOCUMENTS
from rewrite_detection import DOCUMENTS as DETECTION_DOCUMENTS
from rewrite_astrometry_photometry import DOCUMENTS as ASTROMETRY_PHOTOMETRY_DOCUMENTS
from rewrite_cosmetic import DOCUMENTS as COSMETIC_DOCUMENTS

ROOT = Path(__file__).resolve().parents[3]
UNIT = ROOT / '实验/documentation-refactor'

# Human semantic comparisons against the fixed baseline, recorded in the
# existing inventory. These are not inferred from length or keyword checks.
SEMANTIC_REVIEWS = {
    'docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md':
        '完整319行基线与现文语义对照：F1–F7模型/量纲、完整Akima/Simpson/固定尺度IRLS伪代码、误差/复杂度及参考回填science；7个完整C签名、C++装配全字段/默认/所有权/异常及孔径路径入detail/normalize/modules/photometry.md；旧b_n/效率/中心强断言留Q33；静态核生产并未运行',
    'docs/science/algorithms/normalize/PLATESOLVE.md':
        '完整基线/现文语义对照：选星4a/位次/割线、三角投票、TRANS裁剪/重匹配、SIP推导与逆网格、误差/参考保留正式science；完整14 API及所有字段/寿命补入detail/normalize/modules/platesolve.md；纠正尺度/KD复杂度/旧失败状态，未运行解算',
    'docs/detail/normalize/modules/star_detection.md':
        '完整基线/现文语义对照：O1–O16全部恢复正式detail；六签名、13参数、列身份、初值/LM重试、门、异常/寿命/复杂度及连贯例；纠正旧参数化/饱和中心/十数组；未运行检测测试，论文边界未逐式外核',
    'docs/science/algorithms/normalize/STAR_DETECTION_ALGORITHMS.md':
        '完整基线/现文语义对照：观测模型、差分与平滑推导、Jacobian/TR-LM/通量及误差补入science，逐算子/API归detail；历史召回/增益留研究记录，参考版本/许可与随机分配差异已核作者代码；未执行数值验证',
}

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
    'docs/engineering/build/BUILD_GRAPH.md',
    'docs/engineering/build/BUILD_NODES.md',
    'docs/engineering/build/RELEASE.md',
    'docs/engineering/standards/DEPENDENCY.md',
    'docs/engineering/data/ARTIFACTS.md',
    'docs/engineering/data/ARTIFACT_STORE.md',
    'docs/engineering/data/PROVENANCE.md',
    'docs/engineering/contracts/ATOMIC_PUBLISH.md',
    'docs/science/algorithms/export/HEALPIX_MAPPING.md',
    'docs/science/algorithms/normalize/STAR_PSF_ALGORITHMS.md',
    'docs/detail/infrastructure/gaia_xpsd_client.md',
    'docs/engineering/contracts/SCHEDULER.md',
    'docs/engineering/contracts/ASYNC_IO.md',
    'docs/engineering/contracts/FITS_IO.md',
    'docs/engineering/contracts/HIPS_IO.md',
    'docs/detail/infrastructure/aio.md',
    'docs/engineering/contracts/MANIFEST_VERIFY.md',
    'docs/detail/infrastructure/cli.md',
}
REWRITTEN.update(GUIDES)
REWRITTEN.update(STANDARD_DOCUMENTS)
REWRITTEN.update(CPU_DOCUMENTS)
REWRITTEN.update(GUI_DOCUMENTS)
REWRITTEN.update(TESTING_DOCUMENTS)
REWRITTEN.update(STORAGE_DOCUMENTS)
REWRITTEN.update(SUPPORT_DOCUMENTS)
REWRITTEN.update(MOSAIC_REDUCER_DOCUMENTS)
REWRITTEN.update(DETECTION_DOCUMENTS)
REWRITTEN.update(ASTROMETRY_PHOTOMETRY_DOCUMENTS)
REWRITTEN.update(COSMETIC_DOCUMENTS)
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
    'docs/engineering/UNIFIED_OBJECTS.md',
    'docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md',
    'docs/engineering/data/README.md',
    'docs/engineering/contracts/MANIFEST_VERIFY.md',
    'docs/engineering/api/abi/ABI.md',
    'docs/science/algorithms/common/GAIA_QUERY.md',
    'docs/science/algorithms/common/NOISE_ESTIMATION.md',
    'docs/science/algorithms/common/GATES_AND_TOLERANCES.md',
    'docs/engineering/architecture/README.md',
    'docs/engineering/architecture/MODULE_MAP.md',
    'docs/engineering/standards/NUMERIC.md',
    'docs/engineering/contracts/PIPELINE_BLOCK.md',
    'docs/detail/common/unified_objects.md',
    'docs/detail/common/unified_model.md',
    'docs/engineering/contracts/CONFIG.md',
    'docs/engineering/contracts/CLI_PROTOCOL.md',
    'docs/engineering/contracts/NORMALIZE_DATA.md',
    'docs/detail/normalize/contracts.md',
    'docs/engineering/architecture/DATA_FLOW.md',
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
        if name in SEMANTIC_REVIEWS:
            reading = SEMANTIC_REVIEWS[name]
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
                   science_questions=len(re.findall(r'^### Q\d+\b', (UNIT / 'docs/SCIENTIFIC_QUESTIONS.md').read_text(encoding='utf-8'), re.M)),
                   full_repository_substantive_review_complete=False)
    (UNIT / 'docs/INVENTORY_SUMMARY.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=True, indent=2))


if __name__ == '__main__':
    main()
