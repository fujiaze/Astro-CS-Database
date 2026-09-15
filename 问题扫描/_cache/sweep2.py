import subprocess, re, collections
out = subprocess.run(['git','grep','-n','-E', r'(write_text\(|open\([^)]*[\x27\x22]w|json\.dump|csv\.writer|DictWriter|writerow)', '--','tools/','ci/steps','runtime/','scripts/'], capture_output=True, text=True).stdout
pat = re.compile(r'[\x27\x22]([\w./\-]+\.(?:json|csv|jsonl|xml|svg|html|md))[\x27\x22]')
writers = collections.defaultdict(set)
n_lines=0
for line in out.splitlines():
    if ':' not in line: continue
    n_lines+=1
    f = line.split(':',1)[0]
    for m in pat.finditer(line):
        writers[m.group(1)].add(f)
print('writer lines:', n_lines, 'names:', len(writers))
for n in sorted(writers):
    res = subprocess.run(['git','grep','-l','-e',n,'--','*.py','*.cpp','*.h','*.sh','*.ps1','*.yml','*.yaml','*.cmake','*.txt','*.json',':(exclude)问题扫描',':(exclude)设计大纲'], capture_output=True, text=True).stdout.splitlines()
    readers=[x for x in res if x not in writers[n]]
    if not readers:
        print('ZERO-READER:', n, '<-', sorted(writers[n])[:4])
