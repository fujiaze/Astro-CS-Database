
txt=open('cli/commands.cpp',encoding='utf-8',errors='ignore').read().split('\n')
def f(s): return [i+1 for i,l in enumerate(txt) if s in l][:7]
for s in ['"--json"','"--events-jsonl"','"--output"','"--run-manifest"','"--provider"','"--module"','"--quick"','"--full"','"--group"','"--config"']:
    print('%-20s %s' % (s, f(s)))
p=open('cli/parser.cpp',encoding='utf-8',errors='ignore').read().split('\n')
def g(s): return [i+1 for i,l in enumerate(p) if s in l][:7]
print('--- parser ---')
for s in ['"--json"','"--quick"','"--group"','kGroups','"run"']:
    print('%-14s %s' % (s, g(s)))
