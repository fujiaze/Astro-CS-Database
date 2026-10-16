"""Separate reviewed historical observations from maintained science text."""
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from restructure import ROOT, translate

SOURCES = {
    'docs/science/calibration/CCD_DEFECT.md': (
        '## 附录：未配套复现载体的观察记录', 'CCD_DEFECT_OBSERVATIONS.md'),
    'docs/science/detection/ASTROMETRY.md': (
        '## 附录：数值预算记录的边界', 'ASTROMETRY_BUDGET_RECORDS.md'),
    'docs/science/detection/STAR_DETECTION.md': (
        '## 附录：诊断记录的复核边界', 'DETECTION_OBSERVATIONS.md'),
    'docs/science/psf/PSF.md': (
        '## 附录：闭式与有限样本复算', 'PSF_CALCULATION_EXAMPLE.md'),
}


def main():
    for source, (heading, name) in SOURCES.items():
        path = ROOT / source
        body = path.read_text(encoding='utf-8')
        if heading not in body:
            raise RuntimeError(f'Expected section missing: {source}')
        main, record = body.split(heading, 1)
        dest = '实验/documentation-refactor/docs/' + name
        if (ROOT / dest).exists():
            raise RuntimeError(f'Refusing to overwrite record: {dest}')
        blob = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD:' + source], cwd=ROOT, text=True).strip()
        prefix = (f'# {heading.removeprefix("## 附录：")}\n\n'
                  f'来源：`{source}`，整理前 Git blob `{blob}`。'
                  '以下原文保留观察、假设和复核限制；没有重新运行，'
                  '不作为当前生产读数或正式门限。引用编号按来源文件参考表解释。\n\n')
        (ROOT / dest).write_text(prefix + translate(record.strip(), source, dest) + '\n',
                                 encoding='utf-8', newline='\n')
        back = translate(f'[记录]({name})', dest, source)
        path.write_text(main.rstrip() + '\n\n过程记录与复算示例见' + back +
                        '；正文定义不以未配套复现载体的历史读数作为验证依据。\n',
                        encoding='utf-8', newline='\n')
        if source.endswith('/PSF.md'):
            match = re.search("python3 - <<'PY'\n(.*?)\nPY\n", record, re.S)
            if not match:
                raise RuntimeError('PSF example body not found')
            code = ROOT / '实验/documentation-refactor/code/psf_calculation_example.py'
            code.write_text('# Transcribed example; NOT RUN during documentation refactor.\n' +
                            match[1] + '\n', encoding='utf-8', newline='\n')
    print(f'Separated {len(SOURCES)} reviewed appendices without deleting their records.')


if __name__ == '__main__':
    main()
