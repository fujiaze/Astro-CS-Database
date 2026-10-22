"""Design integrity only. This is not science implementation validation."""
from pathlib import Path
import csv
import json
import re
import sys
from urllib.parse import unquote
import jsonschema

ROOT = Path(__file__).resolve().parents[1]
errors = []

def require(condition, message):
    if not condition:
        errors.append(message)

for path in ROOT.rglob('*.md'):
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
for path in (ROOT / 'examples').glob('*.request.json'):
    request = json.loads(path.read_text(encoding='utf-8'))
    for error in jsonschema.Draft202012Validator(schema).iter_errors(request):
        errors.append(f'{path.name}: {error.message}')

header = (ROOT / 'include/acsd/contracts.hpp').read_text(encoding='utf-8')
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
    # Removed tracked paths may leave empty local directories. They must have no files.
    root = ROOT / folder
    require(not root.exists() or not any(p.is_file() for p in root.rglob('*')), f'old dependency: {folder}')

if errors:
    print('\n'.join(errors))
    sys.exit(1)
print(f'Design integrity PASS: six owners, schemas, examples, links, {len(rows)} deletion records. No science execution.')
