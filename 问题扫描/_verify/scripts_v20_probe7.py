
txt=open('cli/commands.cpp',encoding='utf-8',errors='ignore').read().split('\n')
for tag,s in [('108','is_json_output'),('130','events-jsonl'),('1517','"provider"'),('1765','"module"'),('1861','run-manifest'),('1517b','provider_arg'),('out_opt','"output"'),('cfg_opt','"config"')]:
    print('%-8s %s' % (tag, [i+1 for i,l in enumerate(txt) if s in l][:6]))
print('--- 1515-1520 ---')
print('\n'.join('%d: %s' % (i+1, txt[i].strip()[:90]) for i in range(1512,1520)))
print('--- 1760-1770 ---')
print('\n'.join('%d: %s' % (i+1, txt[i].strip()[:90]) for i in range(1759,1768)))
print('--- 1858-1866 ---')
print('\n'.join('%d: %s' % (i+1, txt[i].strip()[:90]) for i in range(1857,1866)))
