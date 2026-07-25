#!/usr/bin/env python3
"""Fix unescaped double quotes inside JSON string values."""
import json

path = "documentation_conflict_register.json"
text = open(path, encoding="utf-8").read()

result = []
i = 0
in_string = False
while i < len(text):
    c = text[i]
    if not in_string:
        if c == '"':
            in_string = True
            result.append(c)
        else:
            result.append(c)
        i += 1
    else:
        # Inside a string
        if c == '\\':
            # Escaped char, copy both
            result.append(c)
            if i + 1 < len(text):
                result.append(text[i + 1])
                i += 2
            else:
                i += 1
        elif c == '"':
            # Check if this is the end of the string or an internal unescaped quote
            # Look ahead for structural chars (skipping whitespace)
            j = i + 1
            while j < len(text) and text[j] in ' \t\r\n':
                j += 1
            if j < len(text) and text[j] in ',}]:':
                # End of string
                in_string = False
                result.append(c)
                i += 1
            else:
                # Internal unescaped quote - escape it
                result.append('\\"')
                i += 1
        else:
            result.append(c)
            i += 1

fixed_text = ''.join(result)
try:
    data = json.loads(fixed_text)
    json.dump(data, open(path, 'w', encoding='utf-8'), ensure_ascii=False, indent=2)
    print('FIXED: total_conflicts =', data['total_conflicts'], ', items =', len(data['conflicts']))
except json.JSONDecodeError as e:
    print('Still broken at pos', e.pos, '(line', e.lineno, 'col', e.colno, ')')
    pos = e.pos
    print('Context:', repr(fixed_text[max(0, pos - 80):pos + 80]))
