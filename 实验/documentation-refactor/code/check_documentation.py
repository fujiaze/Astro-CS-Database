"""Check navigation, retained module content, and metadata-only migrations.

No science programs are executed. Baseline links in historical records and
pre-existing broken links are reported separately, never counted as repaired.
"""
from pathlib import Path
import json
import os
import re
import subprocess
import sys
import urllib.parse
import yaml

ROOT = Path(__file__).resolve().parents[3]
UNIT = ROOT / '实验/documentation-refactor'
BASELINE = '63eccbb6133aef56389641d43c55df6e243819c5'
LINK = re.compile(r'(?<!!)\[[^\]\n]+\]\(([^)\n]+)\)')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


class UniqueLoader(yaml.SafeLoader):
    pass


def unique_mapping(loader, node, deep=False):
    result = {}
    for k, v in node.value:
        key = loader.construct_object(k, deep=deep)
        if key in result:
            raise ValueError(f'Duplicate YAML key: {key}')
        result[key] = loader.construct_object(v, deep=deep)
    return result


UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)


def body_without_code(body):
    body = re.sub(r'^```.*?^```[^\n]*$', '', body, flags=re.M | re.S)
    body = re.sub(r'`[^`\n]*`', '', body)
    body = re.sub(r'\\\[.*?\\\]', '', body, flags=re.S)
    return re.sub(r'\\\(.*?\\\)', '', body, flags=re.S)


def broken_links(path, body, exists):
    found = []
    for url in LINK.findall(body_without_code(body)):
        if re.match(r'[a-zA-Z][a-zA-Z0-9+.-]*:', url) or url.startswith('#'):
            continue
        target = urllib.parse.unquote(url.partition('#')[0].partition('?')[0]).strip('<>')
        if not target:
            continue
        normalized = os.path.normpath(os.path.join(os.path.dirname(path), target)).replace('\\', '/')
        if not exists(normalized):
            found.append((path, normalized))
    return found


def canonical(body, mapping):
    # Compare retained prose, formulas, code spans and table contents. Only the
    # heading depth and migrated URL spellings are allowed to differ.
    for old, (new, _) in mapping.items():
        body = body.replace(old, new)
    body = LINK.sub(lambda m: m.group(0).rsplit('](', 1)[0] + '](LINK)', body)
    body = re.sub(r'^#{1,6}\s+', '', body, flags=re.M)
    return re.sub(r'\s+', ' ', body).strip()


def compare_json(a, b, path=''):
    changed = []
    if type(a) is not type(b):
        raise AssertionError(f'JSON type changed at {path}')
    if isinstance(a, dict):
        assert a.keys() == b.keys(), f'JSON keys changed at {path}'
        for key in a:
            changed += compare_json(a[key], b[key], path + '/' + key)
    elif isinstance(a, list):
        assert len(a) == len(b), f'JSON array changed at {path}'
        for i, (left, right) in enumerate(zip(a, b)):
            changed += compare_json(left, right, path + '/' + str(i))
    elif a != b:
        assert isinstance(a, str), f'Non-string JSON value changed at {path}'
        changed.append(path)
    return changed


def main():
    original = git('-c', 'core.quotepath=false', 'ls-tree', '-r', '--name-only', '-z', BASELINE).decode().split('\0')[:-1]
    original_set = set(original)
    mapping = json.loads((UNIT / 'docs/PATH_MIGRATIONS.json').read_text(encoding='utf-8'))
    api_source = git('show', BASELINE + ':docs/engineering/api/PUBLIC_API.md')
    assert (UNIT / 'docs/PUBLIC_API_SOURCE.txt').read_bytes() == api_source, 'API source archive differs'
    current_docs = {p for p in original if p.startswith('docs/') and (ROOT / p).is_file()}
    current_docs.update(dest for dest, _ in mapping.values() if dest.startswith('docs/'))
    index = yaml.load((ROOT / 'docs/DOCUMENT_INDEX.yaml').read_text(encoding='utf-8'), Loader=UniqueLoader)
    paths = [row['path'] for row in index['doc_index']['active']]
    assert len(paths) == len(set(paths)), 'Duplicate indexed path'
    assert set(paths) == current_docs, ('Index difference', sorted(set(paths) ^ current_docs))
    for row in index['doc_index']['active']:
        for upstream in re.findall(r'docs/[^；;\s]+\.(?:md|yaml)', row['upstream']):
            assert (ROOT / upstream).is_file(), f'Missing upstream: {upstream}'
    retained = []
    for source, (dest, anchor) in mapping.items():
        if not source.startswith('docs/detail/registry/acsd.'):
            continue
        text = git('show', BASELINE + ':' + source).decode('utf-8')
        old_body = '\n'.join(text.splitlines()[1:])
        result = (ROOT / dest).read_text(encoding='utf-8')
        assert canonical(old_body, mapping) in canonical(result, mapping), f'Content lost: {source}'
        if anchor:
            assert f'## {anchor}\n' in result, f'Missing section: {source}'
        retained.append(source)
    # Baseline uses the complete tracked tree; current scan covers maintained
    # docs and root entrypoints, plus new research records. Old experiments keep
    # their original links and are explicitly outside this repaired link set.
    baseline_bad = set()
    research = {p.relative_to(ROOT).as_posix() for p in UNIT.rglob('*.md')}
    scan = sorted(current_docs | {'README.md', 'AGENTS.md'} | research)
    for path in original:
        if path not in scan or not path.endswith('.md'):
            continue
        text = git('show', BASELINE + ':' + path).decode('utf-8-sig')
        baseline_bad.update(broken_links(path, text, lambda p: p in original_set or any(x.startswith(p.rstrip('/') + '/') for x in original)))
    current_bad = set()
    for path in scan:
        if path.endswith('.md'):
            current_bad.update(broken_links(path, (ROOT / path).read_text(encoding='utf-8-sig'), lambda p: (ROOT / p).exists()))
    new_bad = current_bad - baseline_bad
    assert not new_bad, f'New broken links: {sorted(new_bad)}'
    metadata = json.loads((UNIT / 'docs/METADATA_PATH_UPDATES.json').read_text(encoding='utf-8'))
    json_changes = {}
    for path in metadata:
        before = json.loads(git('show', BASELINE + ':' + path).decode('utf-8-sig'))
        after = json.loads((ROOT / path).read_text(encoding='utf-8-sig'))
        json_changes[path] = compare_json(before, after)
    report = dict(indexed_docs=len(paths), retained_module_pages=len(retained),
                  new_broken_markdown_links=sorted(new_bad),
                  remaining_preexisting_broken_links=sorted(current_bad),
                  metadata_string_changes=json_changes,
                  scan_scope='Current docs + root README/AGENTS + new refactor unit Markdown; historical experiments excluded',
                  scientific_execution='NOT_RUN')
    (UNIT / 'docs/CHECK_RESULTS.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k not in ['metadata_string_changes', 'remaining_preexisting_broken_links']}, ensure_ascii=True, indent=2))
    print('Remaining baseline broken links:', len(current_bad))
    print('Metadata JSON objects:', len(metadata), '(keys, numbers, enums, and array lengths unchanged)')


if __name__ == '__main__':
    main()
