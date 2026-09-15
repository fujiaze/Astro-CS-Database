
import subprocess, re, collections
out=subprocess.run(['git','grep','-n','-E',r'(write_text\(|open\([^)]*["' + r'\'"' + r']w|json\.dump|csv\.writer|DictWriter)', '--', 'tools/', 'ci/steps', 'runtime/', 'scripts/'],capture_output=True,text=True)
pat=re.compile(r'["\']([\w./-]+\.(?:json|csv|jsonl|xml|svg|html|md))["\']')
writers=collections.defaultdict(set)
lines=[l for l in out.splitlines() if ':' in l]
print('writer lines:', len(lines))
for line in lines:
    f=line.split(':',1)[0]
    for m in pat.finditer(line):
        writers[m.group(1)].add(f)
names=sorted(writers)
print(len(names),'candidate written artifact names')
for n in names:
    res=subprocess.run(['git','grep','-l','-e',n,'--',':(exclude)tools',':(exclude)问题扫描',':(exclude)设计大纲',':(exclude)evidence',':(exclude)engineering',':(exclude)reports',':(exclude)artifacts'],capture_output=True,text=True)
    readers=[x for x in res.stdout.splitlines() if x not in writers.get(n,set())]
    if not readers:
        print('ZERO-READER:', n, ' <-', sorted(writers[n])[:4])

