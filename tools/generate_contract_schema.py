"""Export the restricted declaration grammar of contracts.hpp to input JSON Schema.

No science is implemented. The header is the only field/signature owner.
Unknown C++ types fail generation instead of silently permitting arbitrary JSON.
"""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
text = (ROOT / 'include/acsd/contracts.hpp').read_text(encoding='utf-8')
text = re.sub(r'//[^\n]*', '', text)
definitions = {}
structs = {}
enums = {}
aliases = dict(re.findall(r'using\s+(\w+)\s*=\s*([^;]+);', text))

def closing(source, start):
    depth = 1
    for i in range(start + 1, len(source)):
        depth += (source[i] == '{') - (source[i] == '}')
        if not depth:
            return i
    raise ValueError('unclosed declaration')

for match in re.finditer(r'\bstruct\s+(\w+)(?:\s*:\s*(\w+))?\s*\{', text):
    name, base = match.group(1, 2)
    if name == 'Result':
        continue
    start = match.end() - 1
    structs[name] = (base, text[start + 1:closing(text, start)])
for match in re.finditer(r'\benum\s+class\s+(\w+)\s*\{([^}]+)\}', text):
    name = match.group(1)
    # Nested enums are qualified by their containing struct.
    owners = [key for key, (_, body) in structs.items() if match.group(0) in body]
    if owners:
        name = owners[0] + '::' + name
    enums[name] = [s.strip().split('=')[0].strip() for s in match.group(2).split(',') if s.strip()]

def split(source, separator=','):
    result, start, depth = [], 0, 0
    for i, char in enumerate(source):
        depth += (char in '<{[') - (char in '>}]')
        if char == separator and depth == 0:
            result.append(source[start:i].strip())
            start = i + 1
    result.append(source[start:].strip())
    return [part for part in result if part]

def type_schema(cpp, owner=''):
    cpp = re.sub(r'\bconst\s+', '', cpp).strip().rstrip('&').strip()
    if cpp in aliases and cpp not in {'Id', 'U32', 'U64'}:
        return type_schema(aliases[cpp], owner)
    if cpp in {'Id', 'std::string'}:
        return {'type': 'string'}
    if cpp in {'double', 'float'}:
        return {'anyOf': [{'type': 'number'}, {'type': 'object', 'additionalProperties': False,
            'required': ['f64'], 'properties': {'f64': {'type': 'string', 'pattern': '^[0-9a-f]{16}$'}}}]}
    if cpp == 'bool':
        return {'type': 'boolean'}
    if cpp in {'U64', 'U32', 'std::uint64_t', 'std::uint32_t', 'std::uint16_t', 'std::uint8_t', 'std::byte'}:
        bits = {'U64': 64, 'U32': 32, 'std::byte': 8}.get(cpp)
        if bits is None:
            bits = int(re.search(r'uint(\d+)', cpp).group(1))
        return {'type': 'integer', 'minimum': 0, 'maximum': 2**bits - 1}
    if cpp in {'std::int32_t', 'std::int64_t'}:
        bits = int(re.search(r'int(\d+)', cpp).group(1))
        return {'type': 'integer', 'minimum': -(2**(bits-1)), 'maximum': 2**(bits-1)-1}
    generic = re.fullmatch(r'(?:std::)?(\w+)\s*<(.*)>', cpp)
    if generic:
        kind, inner = generic.groups()
        args = split(inner)
        if kind == 'Ref':
            return {'$ref': '#/$defs/' + args[0] + 'Ref'}
        if kind in {'vector', 'span'}:
            return {'type': 'array', 'items': type_schema(args[0], owner)}
        if kind == 'array':
            return {'type': 'array', 'items': type_schema(args[0], owner), 'minItems': int(args[1]), 'maxItems': int(args[1])}
        if kind == 'optional':
            return {'anyOf': [type_schema(inner, owner), {'type': 'null'}]}
        if kind == 'pair':
            return {'type': 'array', 'prefixItems': [type_schema(a, owner) for a in args], 'minItems': 2, 'maxItems': 2}
        if kind == 'variant':
            return {'oneOf': [{'type': 'object', 'additionalProperties': False,
                'required': ['alternative', 'value'], 'properties': {'alternative': {'const': a}, 'value': type_schema(a, owner)}} for a in args]}
    if owner + '::' + cpp in enums:
        cpp = owner + '::' + cpp
    if cpp in enums:
        return {'enum': enums[cpp]}
    if cpp in structs:
        return {'$ref': '#/$defs/' + cpp}
    raise ValueError(f'Unsupported declaration type: {owner}: {cpp}')

nonwire = {'Context', 'Storage', 'Resources', 'Cancellation', 'Lease', 'ArrayView', 'Reservation'}
for name, (base, body) in structs.items():
    if name in nonwire or name.endswith('Provider'):
        continue
    properties, required = {}, []
    if base == 'Model':
        properties.update(identity={'$ref': '#/$defs/Identity'}, assessment={'$ref': '#/$defs/Assessment'})
    for statement in split(body, ';'):
        statement = statement.strip()
        if not statement or '(' in statement or statement.startswith('virtual'):
            continue
        nested = re.fullmatch(r'enum\s+class\s+(\w+)\s*\{([^}]+)\}\s*(\w+)', statement, re.S)
        if nested:
            field = nested.group(3)
            properties[field] = {'enum': enums[name + '::' + nested.group(1)]}
            required.append(field)
            continue
        depth, cut = 0, None
        for i, char in enumerate(statement):
            depth += (char == '<') - (char == '>')
            if char.isspace() and depth == 0:
                cut = i
                break
        if cut is None:
            raise ValueError(f'Unparsed field {name}: {statement}')
        cpp, fields = statement[:cut], statement[cut:].strip()
        if cpp in nonwire:
            continue
        for field in split(fields):
            if not re.fullmatch(r'\w+', field):
                raise ValueError(f'Unparsed declarator {name}: {field}')
            properties[field] = type_schema(cpp, name)
            required.append(field)
    definitions[name] = {'type': 'object', 'additionalProperties': False, 'properties': properties, 'required': required}

# Ref is nullable at schema level; consumer-specific non-null invariants remain mandatory.
for name in structs:
    if name in nonwire:
        continue
    definitions[name + 'Ref'] = {'anyOf': [{'type': 'null'},
        {'type': 'object', 'required': ['$ref'], 'additionalProperties': False, 'properties': {'$ref': {'type': 'string', 'minLength': 1}}},
        {'type': 'object', 'required': ['type', 'data'], 'additionalProperties': False,
         'properties': {'type': {'const': name}, 'data': {'$ref': '#/$defs/' + name}}}]}

definitions['ArrayRef']['anyOf'].append({'type': 'object', 'additionalProperties': False,
    'required': ['type', 'dtype', 'shape', 'unit', 'index_id', 'values'],
    'properties': {'type': {'const': 'Array'}, 'dtype': type_schema('DType'),
        'shape': type_schema('std::vector<U64>'), 'unit': type_schema('Unit'),
        'index_id': {'type': 'string'}, 'values': {'type': 'array', 'maxItems': 65536,
            'items': {'anyOf': [{'type': 'number'}, type_schema('Dyadic')]}}}})
definitions['LinearActionRef']['anyOf'].append({'type': 'object', 'additionalProperties': False,
    'required': ['type', 'recipe', 'children', 'arrays'], 'properties': {
        'type': {'const': 'LinearAction'}, 'recipe': type_schema('ActionRecipe'),
        'children': type_schema('std::vector<Ref<LinearAction>>'), 'arrays': type_schema('std::vector<ArrayRef>')}})
# An abstract action cannot be constructed from its base fields alone.
definitions['LinearActionRef']['anyOf'].pop(2)
provider_fields = {
    'FootprintProvider': {'wcs': type_schema('Ref<WcsModel>')},
    'FramesProvider': {'descriptions': type_schema('std::vector<Ref<FrameDescription>>'), 'signals': type_schema('std::vector<ArrayRef>'), 'dq': type_schema('std::vector<ArrayRef>')},
    'CatalogueProvider': {'index': type_schema('Ref<CatalogueIndex>')},
    'ProductProvider': {'location': type_schema('ProductLocation')},
    'RefinableGeometryProvider': {'location': type_schema('ProductLocation'), 'snapshot': {'type': 'string'}}}
for name, properties in provider_fields.items():
    definitions[name] = {'type': 'object', 'additionalProperties': False, 'properties': properties, 'required': list(properties)}
definitions['ModelRef']['anyOf'][2] = {'anyOf': [
    {'type': 'object', 'additionalProperties': False, 'required': ['type', 'data'],
     'properties': {'type': {'const': name}, 'data': {'$ref': '#/$defs/' + name}}}
    for name, (base, _) in structs.items() if base == 'Model']}

operations = []
signatures = text[text.index('// All functions synchronous.'):] if '// All functions synchronous.' in text else text[text.index('Result<Ref<WcsModel>> make_wcs'):]
for match in re.finditer(r'(?:Result<[^;\n]+?>|FitsPixel1|PixelCenter0|PixelEdge0)\s+(\w+)\(([^;]+?)\);', signatures):
    operation, parameters = match.groups()
    args, request = {}, {'type': 'object', 'maxProperties': 0}
    for parameter in split(parameters):
        cpp, name = parameter.rsplit(' ', 1)
        if name == 'context':
            continue
        spec = type_schema(cpp)
        if name == 'point' and cpp in {'PixelCenter0', 'PixelEdge0', 'FitsPixel1'}:
            spec = {'type': 'object', 'additionalProperties': False, 'required': ['type', 'data'],
                    'properties': {'type': {'const': cpp}, 'data': spec}}
        if name == 'request':
            request = spec
        else:
            args[name] = spec
    operations.append({'type': 'object', 'properties': {'operation': {'const': operation},
        'arguments': {'type': 'object', 'additionalProperties': False, 'properties': args, 'required': list(args)}, 'request': request}})

# Provider dispatch has the same typed requests; it never exposes private helpers.
methods = {
 'catalogue.describe': ('CatalogueProvider', {}, None),
 'catalogue.query': ('CatalogueProvider', {}, 'CatalogueQuery'),
 'catalogue.query_quads': ('CatalogueProvider', {}, 'QuadQuery'),
 'catalogue.lookup': ('CatalogueProvider', {'source_ids': 'std::vector<U64>'}, None),
 'frames.list_frames': ('FramesProvider', {}, None),
 'frames.describe_frame': ('FramesProvider', {'frame': 'Id'}, None),
 'frames.read_leaf_block': ('FramesProvider', {}, 'FrameReadRequest'),
 'frames.read_native_dependencies': ('FramesProvider', {'sources': 'std::vector<SourceIdentity>'}, None),
 'frames.open_action': ('FramesProvider', {'action_id': 'Id'}, None),
 'frames.query_support': ('FramesProvider', {'frame': 'Id', 'index': 'Ref<Index>'}, None),
 'frames.open_refinable_geometry': ('FramesProvider', {'snapshot': 'Id'}, None),
 'footprint.forward_interval': ('FootprintProvider', {'domain': 'Box'}, None),
 'footprint.derivative_interval': ('FootprintProvider', {'domain': 'Box'}, None),
 'footprint.boundary_interval': ('FootprintProvider', {'curve': 'CurvedEdge', 'parameter': 'Interval'}, None),
 'footprint.measure': ('FootprintProvider', {'source': 'Ref<DetectorFootprint>'}, 'GeometryBudget'),
 'refinable.open': ('RefinableGeometryProvider', {'snapshot': 'Id'}, None),
 'refinable.refine': ('RefinableGeometryProvider', {'base': 'Ref<GeometrySnapshot>'}, 'GeometryBudget')}
for operation, (provider, args, request) in methods.items():
    properties = {'provider': type_schema('Ref<' + provider + '>')}
    properties.update({name: type_schema(cpp) for name, cpp in args.items()})
    operations.append({'type': 'object', 'properties': {'operation': {'const': operation},
        'arguments': {'type': 'object', 'additionalProperties': False, 'properties': properties, 'required': list(properties)},
        'request': type_schema(request) if request else {'type': 'object', 'maxProperties': 0}}})
for operation, args in {
    'storage.copy': {'dtype': 'DType', 'bytes': 'std::vector<std::byte>', 'shape': 'std::vector<U64>', 'unit': 'Unit', 'index': 'Id'},
    'storage.read': {'array': 'ArrayRef', 'range': 'BlockRequest'},
    'storage.open': {'root': 'std::string', 'description': 'Array', 'blocks': 'std::vector<BlockDescriptor>'},
    'product.read_array': {'provider': 'Ref<ProductProvider>', 'array': 'ArrayRef', 'range': 'BlockRequest'}}.items():
    properties = {name: type_schema(cpp) for name, cpp in args.items()}
    operations.append({'type': 'object', 'properties': {'operation': {'const': operation},
        'arguments': {'type': 'object', 'additionalProperties': False, 'properties': properties, 'required': list(properties)},
        'request': {'type': 'object', 'maxProperties': 0}}})

schema = {'$schema': 'https://json-schema.org/draft/2020-12/schema',
    '$id': 'https://acsd.invalid/schema/science-input/2',
    'description': 'Generated from include/acsd/contracts.hpp. Input shape only; science invariants still require validation.',
    '$defs': definitions, 'oneOf': operations,
    'properties': {'input_objects': {'type': 'object', 'additionalProperties': {'anyOf': [
        {'$ref': '#/$defs/' + name + 'Ref'} for name in structs if name not in nonwire]}}}}
for key, value in definitions.items():
    # Every reference must resolve even in response-only model definitions.
    for target in re.findall(r'"\$ref": "#\/\$defs\/([^"/]+)"', json.dumps(value)):
        if target not in definitions:
            raise ValueError(f'Unresolved schema reference {key}: {target}')
output = json.dumps(schema, ensure_ascii=False, indent=2) + '\n'
path = ROOT / 'schemas/science-input.schema.json'
if '--check' in sys.argv:
    if not path.exists() or path.read_text(encoding='utf-8') != output:
        raise SystemExit('Generated scientific schema is stale; run tools/generate_contract_schema.py')
else:
    path.write_text(output, encoding='utf-8')
def external_refs(value):
    if isinstance(value, dict):
        return {key: (schema['$id'] + item if key == '$ref' and item.startswith('#/') else external_refs(item)) for key, item in value.items()}
    if isinstance(value, list):
        return [external_refs(item) for item in value]
    return value

cli_variants = []
for command, typename, excluded in [
    ('normalize', 'NormalizeRequest', {'frames', 'outputs'}),
    ('mosaic', 'MosaicRequest', {'inputs', 'output'}),
    ('export', 'ExportWorkflowRequest', {'input', 'output'})]:
    payload = definitions[typename]
    fields = {key: external_refs(value) for key, value in payload['properties'].items() if key not in excluded}
    properties = {'command': {'const': command}, 'request': {'type': 'object', 'additionalProperties': False,
        'properties': fields, 'required': [key for key in payload['required'] if key not in excluded]}}
    if command == 'normalize':
        properties['frame_options'] = {'type': 'array', 'items': {'type': 'object', 'additionalProperties': False,
            'required': ['frame', 'supplied_wcs', 'regroup'], 'properties': {
                'frame': {'type': 'string'}, 'supplied_wcs': external_refs(type_schema('Ref<WcsModel>')),
                'regroup': external_refs(type_schema('std::optional<RegroupRequest>'))}}}
    cli_variants.append({'type': 'object', 'additionalProperties': False,
        'properties': properties, 'required': ['command', 'request']})
cli_schema = {'$schema': schema['$schema'], '$id': 'https://acsd.invalid/schema/cli-science/2', 'oneOf': cli_variants}
cli_path = ROOT / 'schemas/cli-science.schema.json'
cli_output = json.dumps(cli_schema, ensure_ascii=False, indent=2) + '\n'
if '--check' in sys.argv:
    if not cli_path.exists() or cli_path.read_text(encoding='utf-8') != cli_output:
        raise SystemExit('Generated CLI science schema is stale')
else:
    cli_path.write_text(cli_output, encoding='utf-8')
manifest_schema = {'$schema': schema['$schema'], '$id': 'https://acsd.invalid/schema/product-manifest/2',
    '$ref': schema['$id'] + '#/$defs/ProductManifest'}
manifest_path = ROOT / 'schemas/manifest.schema.json'
manifest_output = json.dumps(manifest_schema, indent=2) + '\n'
if '--check' in sys.argv:
    if not manifest_path.exists() or manifest_path.read_text(encoding='utf-8') != manifest_output:
        raise SystemExit('Generated manifest schema is stale')
else:
    manifest_path.write_text(manifest_output, encoding='utf-8')
print(f'Public shape schema: {len(definitions)} definitions, {len(operations)} operation variants')
