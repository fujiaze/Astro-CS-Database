"""Design integrity only. This is not science implementation validation."""
from pathlib import Path
import csv
import json
import re
import sys
from urllib.parse import unquote
import jsonschema
from referencing import Registry, Resource
import subprocess
import hashlib
from math import prod

ROOT = Path(__file__).resolve().parents[1]
errors = []
warnings = []

def require(condition, message):
    if not condition:
        errors.append(message)

tracked_markdown = subprocess.run(['git', 'ls-files', '--', '*.md'], cwd=ROOT, capture_output=True,
                             text=True, check=False).stdout.split()
for relative in tracked_markdown:
    path = ROOT / relative
    if any(part in {'.git', 'build', '.work'} for part in path.relative_to(ROOT).parts):
        continue
    # Historical license provenance is archival prose, not a new local dependency.
    if 'historical-notices' in path.parts:
        continue
    text = path.read_text(encoding='utf-8')
    for target in re.findall(r'(?<![!A-Za-z0-9_])\[[^\]]+\]\(([^\s)]+)(?:\s+"[^"\n]*")?\)', text):
        if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:', target) or target.startswith('#'):
            continue
        target = unquote(target.split('#')[0]).strip('<>')
        require((path.parent / target).exists(), f'{path.relative_to(ROOT)}: missing {target}')

for path in (ROOT / 'schemas').glob('*.json'):
    jsonschema.Draft202012Validator.check_schema(json.loads(path.read_text(encoding='utf-8')))
schema = json.loads((ROOT / 'schemas/contract.schema.json').read_text(encoding='utf-8'))
science_schema = json.loads((ROOT / 'schemas/science-input.schema.json').read_text(encoding='utf-8'))
registry = Registry().with_resource(science_schema['$id'], Resource.from_contents(science_schema))
subprocess.run([sys.executable, str(ROOT / 'tools/generate_contract_schema.py'), '--check'], check=True)
for path in (ROOT / 'examples').glob('*.request.json'):
    request = json.loads(path.read_text(encoding='utf-8'))
    for error in jsonschema.Draft202012Validator(schema, registry=registry).iter_errors(request):
        errors.append(f'{path.name}: {error.message}')
    objects = request['input_objects']
    blocks = {block['id']: block for block in request.get('blocks', [])}
    def walk(value):
        if isinstance(value, dict):
            if '$ref' in value:
                require(value['$ref'] in objects, f'{path.name}: unresolved object {value["$ref"]}')
            if value.get('type') == 'Array':
                array = value.get('data', value)
                index = array['index_id']
                require(index in objects and objects[index].get('type') == 'Index', f'{path.name}: unresolved index {index}')
                if 'values' in array:
                    require(prod(array['shape']) == len(array['values']), f'{path.name}: array shape mismatch')
                for block in array.get('block_ids', []):
                    require(block in blocks, f'{path.name}: unresolved block {block}')
            for child in value.values():
                walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)
    walk(request)
    for descriptor in blocks.values():
        require(descriptor['index_id'] in objects, f'{path.name}: block has missing index')
        file = (path.parent / request.get('input_root', '.') / descriptor['path']).resolve()
        require(file.is_file(), f'{path.name}: missing external block')
        if file.is_file() and descriptor['codec'] == 'raw':
            raw = file.read_bytes()
            require(len(raw) == descriptor['raw_bytes'] == descriptor['stored_bytes'], f'{path.name}: block byte mismatch')
            require('sha256:' + hashlib.sha256(raw).hexdigest() == descriptor['raw_sha256'], f'{path.name}: block hash mismatch')

header = (ROOT / 'include/acsd/model.hpp').read_text(encoding='utf-8')
require('Result<NormalizeResult> normalize(' in header, 'missing public normalize workflow')
require('Result<ProductLocation> mosaic(' in header, 'missing public mosaic workflow')
require('Result<ProductLocation> export_image(' in header, 'missing public export workflow')
require(len(list((ROOT / 'docs/engineering/rebuild').glob('*.md'))) == 6, 'competing engineering owner')
with (ROOT / 'docs/audit/removed-files.csv').open(encoding='utf-8', newline='') as stream:
    rows = list(csv.DictReader(stream))
require(len(rows) == 4295, 'cleanup count drift')
require(len({row['path'] for row in rows}) == len(rows), 'duplicate deletion entry')
for row in rows:
    require(bool(re.fullmatch('[0-9a-f]{40}', row['baseline_git_blob'])), 'invalid baseline blob')
for folder in ['lib', 'eng', 'run', 'config', 'testdata']:
    # The rebuild tree must not carry the removed system as a dependency.  The
    # guard applies to the tracked tree; untracked leftovers of the historical
    # checkout are reported as a warning because they are not part of any
    # revision and must not be deleted by this project.
    tracked = subprocess.run(['git', 'ls-files', '--', folder], cwd=ROOT, capture_output=True,
                             text=True, check=False).stdout.split()
    require(not tracked, f'old dependency: {folder}')
    root = ROOT / folder
    if root.exists() and any(p.is_file() for p in root.rglob('*')):
        warnings.append(f'untracked legacy leftovers present in {folder} (not part of the tree)')

# Process coverage is a build product check: the whitelist in
# docs/engineering/rebuild/processes/README.md is compared with the frozen
# registry manifest.  A tree without a built manifest is not silently accepted;
# it is reported and the check is skipped so document-only builds still work.
process_manifest = ROOT / 'build' / 'process-manifest.json'
if process_manifest.is_file():
    coverage = subprocess.run(
        [sys.executable, str(ROOT / 'tools/check_process_coverage.py'),
         '--manifest', str(process_manifest), '--source-root', str(ROOT)],
        cwd=ROOT, capture_output=True, text=True, check=False)
    if coverage.returncode != 0:
        errors.append('process coverage:\n' +
                      (coverage.stdout or coverage.stderr).strip())
else:
    warnings.append('process coverage not checked: build/process-manifest.json is absent (build acsd-runtime-manifest)')

if warnings:
    print('\n'.join(warnings))
if errors:
    print('\n'.join(errors))
    sys.exit(1)
print(f'Design integrity PASS: six owners, schemas, examples, links, {len(rows)} deletion records. No science execution.')
