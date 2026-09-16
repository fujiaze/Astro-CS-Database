
import re, collections
src = open('lib/core/src/module_adapters.cpp', encoding='utf-8', errors='ignore').read()
lines = src.split('\n')
# 1) 找出所有 "Json <var>{{ ... }};" 或 "<var>[\"key\"] =" 形式的写侧键集（按变量名聚合）
writers = collections.defaultdict(set)
for m in re.finditer(r'\bJson\s+(\w+)\s*[\{(]', src):
    var = m.group(1)
    start = m.start()
    # 取到匹配的花括号结束（简化：取后 4000 字符内平衡）
    depth = 0; i = src.find('{', start); j = i
    while j < len(src):
        if src[j] == '{': depth += 1
        elif src[j] == '}':
            depth -= 1
            if depth == 0: break
        j += 1
    block = src[i:j+1]
    for k in re.findall(r'\{"([A-Za-z0-9_]+)"', block):
        writers[var].add(k)
for m in re.finditer(r'\(\*(\w+)\)\["([A-Za-z0-9_]+)"\]\s*=', src):
    writers[m.group(1)].add(m.group(2))
# 2) 读侧: <var>.value("k" / <var>->value("k" / <var>.contains("k" / <var>["k"]
readers = collections.defaultdict(set)
for m in re.finditer(r'\b(\w+)(\.|->)(?:value|contains|at)\(\s*"([A-Za-z0-9_]+)"', src):
    readers[m.group(1)].add(m.group(3))
for m in re.finditer(r'\b(\w+)\[\s*"([A-Za-z0-9_]+)"\s*\]', src):
    readers[m.group(1)].add(m.group(2))
print('=== 读侧键不在同文件任何 Json 写侧键集里 (按变量) ===')
for var, rks in sorted(readers.items()):
    allw = set()
    for s in writers.values(): allw |= s
    miss = sorted(k for k in rks if k not in allw)
    if miss:
        print('  %-14s reads-not-written-anywhere: %s' % (var, ', '.join(miss)))
print()
print('=== writer 变量数 %d / reader 变量数 %d ===' % (len(writers), len(readers)))
print('=== 同名变量既写又读且读>写的对 ===')
for var in sorted(set(writers) & set(readers)):
    d = sorted(readers[var] - writers[var])
    if d:
        print('  %-14s %s' % (var, ', '.join(d)))
