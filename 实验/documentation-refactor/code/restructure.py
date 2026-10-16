"""Reproducible, content-preserving consolidation of the reviewed registry pages.

Run once against the pre-refactor tree. Scientific definitions and source code
are not changed. Markdown paths are rebased before sections are incorporated.
"""
from pathlib import Path
import json
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
LINK = re.compile(r'(?<!!)\[([^\]\n]+)\]\(([^)\n]+)\)')


def tracked():
    return subprocess.check_output(
        ['git', '-c', 'core.quotepath=false', 'ls-files', '-z'], cwd=ROOT
    ).decode('utf-8').split('\0')[:-1]


def relative(target, parent):
    return os.path.relpath(target, parent).replace('\\', '/')


groups = {
    'normalize': ['calibration', 'cosmetic', 'wcs-platesolve',
                  'star-detection', 'star-psf', 'photometry', 'noise-snr',
                  'drizzle', 'writer', 'session'],
    'mosaic': ['coverage', 'sample', 'upm-fit', 'upm-apply', 'reject',
               'integrate', 'write', 'session', 'resample'],
    'export': ['properties', 'wcs', 'resample2', 'writer', 'verify'],
}
phase = {'normalize': 1, 'mosaic': 2, 'export': 3}
titles = {'normalize': 'Normalize 模块与接口', 'mosaic': 'Mosaic 模块与接口',
          'export': 'Export 模块与接口'}
mapping = {}
for stage, names in groups.items():
    for name in names:
        mapping[f'docs/detail/registry/acsd.phase{phase[stage]}.{name}.md'] = (
            f'docs/detail/{stage}/modules.md', name)
mapping['docs/detail/registry/acsd.phase1.hips-writer.md'] = (
    'docs/detail/infrastructure/hips_writer.md', '')
for name in ['DUAL_LINE', 'TRACEABILITY']:
    mapping[f'docs/engineering/governance/{name}.md'] = (
        'docs/engineering/governance/DOCUMENT_GOVERNANCE.md', '')
mapping['docs/engineering/standards/DOCUMENTATION.md'] = (
    'docs/engineering/governance/DOCUMENT_GOVERNANCE.md', '')


def translate(body, old, new):
    def replace(match):
        label, url = match.groups()
        if re.match(r'\w+://|mailto:|app:', url):
            return match.group(0)
        path, sep, fragment = url.partition('#')
        if not path:
            return match.group(0)
        dest = (ROOT / old).parent.joinpath(path).resolve()
        try:
            key = dest.relative_to(ROOT).as_posix()
        except ValueError:
            return match.group(0)
        if key in mapping:
            dest_path, anchor = mapping[key]
            dest = ROOT / dest_path
            # Old subheadings no longer uniquely name the consolidated section.
            fragment = anchor
        rebased = relative(dest, (ROOT / new).parent)
        return f'[{label}]({rebased}' + (f'#{fragment}' if fragment else '') + ')'
    body = LINK.sub(replace, body)
    # Root-relative code spans and plain navigation are also common in this tree.
    for old_path, (new_path, _) in mapping.items():
        body = body.replace(old_path, new_path)
    return body


def run():
    paths = tracked()
    # Explicit sources, not directory enumeration: no untracked files are moved.
    sources = {p: (ROOT / p).read_text(encoding='utf-8') for p in mapping}
    for stage, names in groups.items():
        dest = f'docs/detail/{stage}/modules.md'
        body = f'# {titles[stage]}\n\n'
        body += ('本页按生产依赖组织节点接口，依据[阶段管线](pipeline.md)和所引科学、工程合同。'
                 '每节说明输入输出、公开头、所有权、错误及实现边界；'
                 '库能力、局部会话与 CLI 生产节点分别标识。\n\n')
        body += '| 模块 | 调用职责 |\n|---|---|\n'
        for name in names:
            src = f'docs/detail/registry/acsd.phase{phase[stage]}.{name}.md'
            title = sources[src].splitlines()[0].removeprefix('# ')
            body += f'| [{name}](#{name}) | {title} |\n'
        for name in names:
            src = f'docs/detail/registry/acsd.phase{phase[stage]}.{name}.md'
            text = translate(sources[src], src, dest).splitlines()
            title = text.pop(0).removeprefix('# ')
            text = [('#' + line if re.match(r'^#{2,5} ', line) else line)
                    for line in text]
            body += f'\n## {name}\n\n**{title}。**\n' + '\n'.join(text).strip() + '\n'
        (ROOT / dest).write_text(body, encoding='utf-8', newline='\n')
    old = 'docs/detail/registry/acsd.phase1.hips-writer.md'
    new, _ = mapping[old]
    (ROOT / new).write_text(translate(sources[old], old, new), encoding='utf-8')
    for p in paths:
        if p in mapping or not p.endswith(('.md', '.rst', '.txt', '.yaml', '.yml')):
            continue
        target = ROOT / p
        if not target.is_file():
            continue
        # Historical experiment records retain their text and original links.
        if p.startswith(('实验/', 'artifacts/')):
            continue
        try:
            body = target.read_text(encoding='utf-8-sig')
        except UnicodeDecodeError:
            print(f'Skipped non-UTF8 text: {p}')
            continue
        updated = translate(body, p, p)
        if body != updated:
            target.write_text(updated, encoding='utf-8', newline='\n')
    for p in mapping:
        (ROOT / p).unlink()
    out = ROOT / '实验/documentation-refactor/docs/PATH_MIGRATIONS.json'
    out.write_text(json.dumps(mapping, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Consolidated {len(mapping)} pages; retained historical experiment text.')


if __name__ == '__main__':
    run()
