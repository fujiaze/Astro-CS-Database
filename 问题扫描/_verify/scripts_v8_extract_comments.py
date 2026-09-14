
import re, json, collections

path = '问题扫描/_verify/_v8_diff_percommit.txt'
lines = open(path, encoding='utf-8', errors='replace').read().split('\n')

commit = None
cur_file = None
newln = 0
recs = []
for ln in lines:
    if ln.startswith('COMMITSEP '):
        parts = ln.split(' ', 3)
        commit = (parts[1], parts[2] if len(parts) > 2 else '')
        cur_file = None
        continue
    if ln.startswith('diff --git'):
        cur_file = None
        continue
    if ln.startswith('+++ b/'):
        cur_file = ln[6:].strip()
        continue
    if ln.startswith('--- '):
        continue
    m = re.match(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@', ln)
    if m:
        newln = int(m.group(1))
        continue
    if ln.startswith('+'):
        recs.append({'c': commit[0], 'subj': commit[1], 'f': cur_file, 'n': newln, 't': ln[1:]})
        newln += 1
    elif ln.startswith('-') or ln.startswith('\\'):
        pass
    else:
        newln += 1

CODE_EXT = ('.c', '.h', '.cpp', '.hpp', '.cc', '.cxx', '.py')
DOC_EXT = ('.md', '.rst', '.txt', '.yaml', '.yml', '.cmake', '.in')

def is_comment(rec):
    f = rec['f'] or ''
    t = rec['t']
    s = t.strip()
    if f.endswith(CODE_EXT):
        if s.startswith('//') or s.startswith('/*') or s.startswith('*') or s.startswith('///') or s.startswith('#'):
            return 'cmt-line'
        # trailing comment after code
        m = re.search(r'(//.*)$', t)
        if m and '"' not in m.group(1)[:0]:
            # crude: skip when // is inside a string literal
            pre = t[:m.start()]
            if pre.count('"') % 2 == 0:
                return 'cmt-trailing'
        m2 = re.search(r'(/\*.*?\*/)', t)
        if m2:
            return 'cmt-trailing'
        return None
    if f.endswith(DOC_EXT):
        if not s:
            return None
        return 'doc-line'
    if f.endswith(('.json', '.csv')):
        return 'data-line'
    return None

out = []
for r in recs:
    k = is_comment(r)
    if k:
        r2 = dict(r); r2['kind'] = k
        out.append(r2)

json.dump(out, open('问题扫描/_verify/_v8_comment_lines.json', 'w', encoding='utf-8'), ensure_ascii=False)
print('added lines total:', len(recs))
print('comment/doc candidates:', len(out))
byfile = collections.Counter(o['f'] for o in out)
print('files with candidates:', len(byfile))
for f, c in byfile.most_common(45):
    print('%5d  %s' % (c, f))
