
lines=open('tools/monitoring/run_monitored.py',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(705,760)))
