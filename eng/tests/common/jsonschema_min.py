#!/usr/bin/env python3
"""JSON Schema (2020-12) 关键字子集校验器 —— SCHEMA-INTEGRATE-001/W6 自带，零第三方依赖。

覆盖本任务 schema 使用的关键字：$ref/$defs, type, const, enum, required, properties,
additionalProperties, propertyNames, items, minItems/maxItems/uniqueItems, minProperties,
minLength/maxLength/pattern, minimum/maximum/exclusiveMinimum/exclusiveMaximum,
allOf/anyOf/oneOf/not, if/then/else。$ref 的兄弟关键字按 2020-12 语义照常求值。

`eng/tests/contracts/product_family/test_field_constraints_integration.py` 在 jsonschema 可用时用官方
Draft202012Validator 做差分对照；CI（unittest，无第三方依赖）用本实现。
"""
import re

_NUM = (int, float)
TYPES = {
    "object": dict, "array": list, "string": str, "boolean": bool,
    "null": type(None), "number": _NUM, "integer": int,
}


def _is_type(v, t):
    if t == "boolean":
        return isinstance(v, bool)
    if t == "integer":
        return isinstance(v, int) and not isinstance(v, bool)
    if t == "number":
        return isinstance(v, _NUM) and not isinstance(v, bool)
    if t == "null":
        return v is None
    if t == "object":
        return isinstance(v, dict)
    if t == "array":
        return isinstance(v, list)
    if t == "string":
        return isinstance(v, str)
    return False


def jeq(a, b):
    """JSON 语义相等：数值 1 == 1.0，布尔不与数值混同。"""
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, bool) and isinstance(b, bool) and a == b
    if isinstance(a, _NUM) and isinstance(b, _NUM):
        return a == b
    if type(a) is not type(b):
        return False
    return a == b


def resolve_ref(root, ref):
    if not ref.startswith("#"):
        raise ValueError("only local refs supported: " + ref)
    node = root
    for part in ref[2:].split("/") if len(ref) > 2 else []:
        part = part.replace("~1", "/").replace("~0", "~")
        node = node[part]
    return node


def validate(instance, schema, root=None, path=()):
    """返回 [(path_tuple, message)]；空列表 = 通过。"""
    if root is None:
        root = schema
    errors = []
    if schema is True or schema == {}:
        return errors
    if schema is False:
        return [(tuple(path), "false schema")]

    if "$ref" in schema:
        errors += validate(instance, resolve_ref(root, schema["$ref"]), root, path)

    if "type" in schema:
        t = schema["type"]
        types = t if isinstance(t, list) else [t]
        if not any(_is_type(instance, x) for x in types):
            errors.append((tuple(path), "type: expected %r" % (t,)))

    if "const" in schema and not jeq(instance, schema["const"]):
        errors.append((tuple(path), "const: expected %r" % (schema["const"],)))

    if "enum" in schema and not any(jeq(instance, e) for e in schema["enum"]):
        errors.append((tuple(path), "enum: %r not in %r" % (instance, schema["enum"])))

    if "allOf" in schema:
        for sub in schema["allOf"]:
            errors += validate(instance, sub, root, path)
    if "anyOf" in schema:
        if not any(not validate(instance, sub, root, path) for sub in schema["anyOf"]):
            errors.append((tuple(path), "anyOf: no branch matched"))
    if "oneOf" in schema:
        matched = sum(1 for sub in schema["oneOf"] if not validate(instance, sub, root, path))
        if matched != 1:
            errors.append((tuple(path), "oneOf: %d branches matched" % matched))
    if "not" in schema:
        if not validate(instance, schema["not"], root, path):
            errors.append((tuple(path), "not: subschema matched but must not"))

    if "if" in schema:
        if not validate(instance, schema["if"], root, path):
            if "then" in schema:
                errors += validate(instance, schema["then"], root, path)
        elif "else" in schema:
            errors += validate(instance, schema["else"], root, path)

    if isinstance(instance, dict):
        props = schema.get("properties", {})
        for key in schema.get("required", []):
            if key not in instance:
                errors.append((tuple(path), "required: missing %r" % (key,)))
        if "minProperties" in schema and len(instance) < schema["minProperties"]:
            errors.append((tuple(path), "minProperties"))
        if "propertyNames" in schema:
            for key in instance:
                errors += validate(key, schema["propertyNames"], root, tuple(path) + (key,))
        add = schema.get("additionalProperties", True)
        for key, val in instance.items():
            if key in props:
                errors += validate(val, props[key], root, tuple(path) + (key,))
            else:
                if add is False:
                    errors.append((tuple(path) + (key,), "additionalProperties: %r unexpected" % (key,)))
                elif isinstance(add, dict):
                    errors += validate(val, add, root, tuple(path) + (key,))
        for key, subs in schema.get("patternProperties", {}).items():
            for k2, v2 in instance.items():
                if re.search(key, k2):
                    errors += validate(v2, subs, root, tuple(path) + (k2,))

    if isinstance(instance, list):
        if "minItems" in schema and len(instance) < schema["minItems"]:
            errors.append((tuple(path), "minItems"))
        if "maxItems" in schema and len(instance) > schema["maxItems"]:
            errors.append((tuple(path), "maxItems"))
        if schema.get("uniqueItems"):
            seen = {}
            for i, item in enumerate(instance):
                key = repr(item)
                if key in seen:
                    errors.append((tuple(path) + (i,), "uniqueItems: duplicate of index %d" % seen[key]))
                seen[key] = i
        items = schema.get("items")
        if isinstance(items, dict):
            for i, item in enumerate(instance):
                errors += validate(item, items, root, tuple(path) + (i,))
        elif isinstance(items, list):
            for i, item in enumerate(instance):
                if i < len(items):
                    errors += validate(item, items[i], root, tuple(path) + (i,))

    if isinstance(instance, str):
        if "minLength" in schema and len(instance) < schema["minLength"]:
            errors.append((tuple(path), "minLength"))
        if "maxLength" in schema and len(instance) > schema["maxLength"]:
            errors.append((tuple(path), "maxLength"))
        if "pattern" in schema and not re.search(schema["pattern"], instance):
            errors.append((tuple(path), "pattern: %r !~ %r" % (instance, schema["pattern"])))

    if isinstance(instance, _NUM) and not isinstance(instance, bool):
        if "minimum" in schema and instance < schema["minimum"]:
            errors.append((tuple(path), "minimum"))
        if "maximum" in schema and instance > schema["maximum"]:
            errors.append((tuple(path), "maximum"))
        if "exclusiveMinimum" in schema and instance <= schema["exclusiveMinimum"]:
            errors.append((tuple(path), "exclusiveMinimum"))
        if "exclusiveMaximum" in schema and instance >= schema["exclusiveMaximum"]:
            errors.append((tuple(path), "exclusiveMaximum"))

    return errors


def is_valid(instance, schema):
    return not validate(instance, schema)
