
import json,re
cfg=json.load(open('ci/checks.json',encoding='utf-8'))
checks=cfg['checks']
print('### 全部 121 项：id | profiles | platform | waivable | 命令摘要')
for c in checks:
    cmd=c.get('command',[])
    s=' '.join(cmd) if isinstance(cmd,list) else str(cmd)
    s=re.sub(r'\s+',' ',s)
    print(f"{c.get('id'):38s} {','.join(c.get('profiles',[])):28s} {c.get('platform'):7s} w={str(c.get('waivable')):5s} {s[:110]}")
