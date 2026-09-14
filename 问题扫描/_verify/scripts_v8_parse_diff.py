
import re, json, collections

path = '问题扫描/_verify/_v8_diff.txt'
lines = open(path, encoding='utf-8', errors='replace').read().split('\n')

cur_file = None
records = []
newln = 0
for ln in lines:
    if ln.startswith('diff --git'):
        cur_file = None
        continue
    if ln.startswith('+++ b/'):
        cur_file = ln[6:].strip()
        continue
    m = re.match(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@', ln)
    if m:
        newln = int(m.group(1))
        continue
    if ln.startswith('+') and not ln.startswith('+++'):
        records.append((cur_file, newln, ln[1:]))
        newln += 1
    elif ln.startswith('-') or ln.startswith('---') or ln.startswith('\\'):
        pass
    else:
        newln += 1

print('total added lines:', len(records))
files = collections.Counter(r[0] for r in records)
print('files touched:', len(files))
for f, c in files.most_common(40):
    print('%6d  %s' % (c, f))
json.dump([{'f': f, 'n': n, 't': t} for f, n, t in records],
          open('问题扫描/_verify/_v8_added.json', 'w', encoding='utf-8'), ensure_ascii=False)
