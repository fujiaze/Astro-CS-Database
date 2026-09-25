import io, os, re, sys
sys.stdout.reconfigure(encoding='utf-8')
OUT = r"独立审计/复算件/aud102"
FIELDS = ['batch', 'path', 'lines', 'title', 'role', 'topic', 'up', 'down', 'auth', 'dup',
          'meta', 'viol', 'dangle', 'conflict', 'disp']
rec = {}
for ln in io.open(os.path.join(OUT, 'records3.tsv'), encoding='utf-8'):
    p = ln.rstrip('\n').split('\t')
    p += [''] * (len(FIELDS) - len(p))
    r = dict(zip(FIELDS, p))
    path = r['path']
    if path.startswith('tasks/'):
        path = '工程控制/RELEASE-05/' + path
    sc = lambda x: len(x.get('disp', '')) + len(x.get('dup', '')) + len(x.get('conflict', ''))
    if path not in rec or sc(r) > sc(rec[path]):
        rec[path] = r
CONTENT = re.compile(r'^(掉)?\s*(?:[0-9]+\s*[–\-~至到]\s*[0-9]+|[0-9]+|[一二三四五六七八九十]+)\s*(?:[-–—、和及与]|\s)*'
                     r'(?:行|段|节|表|列|字段|注释|声明|锚|编号|句|内容|清单|块|字)|^掉?\s*(?:本|该)?(?:行|段|节|表|列|字段|注释|声明|锚|编号|句|两行|内容|清单|块|末句|其中)')

def acts(d, strict):
    a = []
    for k, c in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                 ('迁往', 'MOVE'), ('迁移', 'MOVE'), ('迁入', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                 ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        for m in re.finditer(k, d or ''):
            tail = (d or '')[m.end():m.end() + 14].lstrip('（( ')
            if strict and c in ('DEL', 'MERGE', 'MOVE', 'SINK', 'SPLIT') and CONTENT.match(tail):
                if 'EDIT' not in a:
                    a.append('EDIT')
                continue
            if c not in a:
                a.append(c)
    return a

for name, strict in (('宽松(关键词直取)', False), ('严格(区分内容级)', True)):
    cnt = {}
    for p, r in rec.items():
        A = acts(re.sub(r'\s+', ' ', r.get('disp', '')), strict)
        for c in A:
            cnt[c] = cnt.get(c, 0) + 1
    print(name, {k: cnt.get(k, 0) for k in ('DEL', 'MERGE', 'MOVE', 'SPLIT', 'SINK', 'EDIT', 'ESC', 'KEEP')})
    f = sum(1 for p, r in rec.items() if {'MERGE', 'DEL', 'MOVE', 'SPLIT', 'SINK'} & set(acts(re.sub(r'\s+', ' ', r.get('disp', '')), strict)))
    print('   文件级动作对象数:', f)
